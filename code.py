import board, analogio, usb_midi, adafruit_midi, busio, microcontroller, supervisor
from time import sleep
from adafruit_midi.control_change import ControlChange
from adafruit_midi.system_exclusive import SystemExclusive

sleep(1) # avoid a race condition on some systems

print(f"Original CPU Frequency: {microcontroller.cpu.frequency}Hz")
microcontroller.cpu.frequency = 270_000_000

def clamp(value, min_val, max_val):
    return max(min(value, max_val), min_val)

def mapRange(value, inMin, inMax, outMin, outMax):
    return outMin + (((value - inMin) / (inMax - inMin)) * (outMax - outMin))

def save_to_nvm(data):
    if len(data) > 512:
        raise ValueError("Data too large for NVM (max 512 bytes)")
    microcontroller.nvm[0:len(data)] = data 

def read_from_nvm(length):
    data = bytes(microcontroller.nvm[:length])
    # Check if all bytes are either 0x00 or 0xFF
    if data == bytes([0x00] * length) or data == bytes([0xFF] * length):
        return None  # No valid data present
    return data


####################
## CONFIG SECTION ##
####################

# Each fader can send messages on its own channel. Set them here.
# (Remember, most DAWs count MIDI channels starting from 1. Here we start from 0.)
faderMidiChannel = [0, 0, 0]
# Configure which CC number each fader should be assigned to. Note: in 14-bit mode, every CC
# uses the given number + 32 (as per the original MIDI spec, mostly ignored by manufacturers 
# since it was written). So CC15 in 14-bit mode uses CCs 14 and 47. Spec compliant receivers 
# should identify these message pairs as a 14-bit control, or at least let you configure the
# mapping accordingly.
faderCCNumber = [11, 1, 16]
# When true, and if an ADS1115 board is found, CCs will be sent as 14bit MIDI pairs
# In 14-bit mode, fader range is 0 to 16383. In 7-bit mode, fader range is 0 to 127.
enable14bitmode = [True, True, True]
# Set the number of ADC samples from which to get a smoothed value.
smoothingSamples14bit = 2
smoothingSamples7bit = 4
# Increasing may reduce jitter, but will also reduce responsiveness/accuracy
newValueThreshold14bit = 20
newValueThreshold7bit = 1
# If true, and usb_cdc.disable() is commented out in boot.py, debug messages will be printed 
# to serial monitor
enableDebugMode = True

# 14 bytes, not counting sysex start, end and manufacturer ID
defaultConfigBytes = bytes([0,0,0,11,1,16,1,1,1,2,4,20,1,1])

########################
## CONFIG SECTION END ##
########################



#######################
## Don't touch these ##
#######################

def encode14bitToSysexBytesTuple(value):
    """
    Encodes a 14-bit number (0-16383) into two 7-bit MIDI SysEx data bytes.
    
    :param value: The 14-bit integer value to encode (0-16383).
    :return: A tuple containing two 7-bit values (LSB, MSB).
    """
    clamp(value, 0, 16383)    
    
    lsb = value & 0x7F        # Least significant 7 bits
    msb = (value >> 7) & 0x7F # Most significant 7 bits
    return (lsb, msb)

def decodeSysexBytesTupleTo14bit(lsb, msb):
    """
    Decodes two 7-bit MIDI sysex data bytes into a 14-bit number (0-16383).
    """
    clamp(lsb, 0, 127)
    clamp(msb, 0, 127)
    return (msb << 7) | lsb
    

def load_config_from_bytes(inbytes):
    """
    Loads config to current hardware state from a given bytes object - either
    from an incoming sysex message or from the non-volatile memory. If MIDI
    channels have been changed, a reload will be required (do that elsewhere
    to avoid a deathloop)
    """
    if not isinstance(inbytes, (bytes, bytearray)):
        raise TypeError("Expected bytes object, got: " + str(type(inbytes)))
    if len(inbytes) != 14:
        raise ValueError(f"Invalid config length: {len(inbytes)} (expected 14)")
    if inbytes is not None and len(inbytes) >= 14:            
            faderMidiChannel[0] = inbytes[0]
            faderMidiChannel[1] = inbytes[1]
            faderMidiChannel[2] = inbytes[2]
            faderCCNumber[0] = inbytes[3]
            faderCCNumber[1] = inbytes[4]
            faderCCNumber[2] = inbytes[5]
            enable14bitmode[0] = inbytes[6]
            enable14bitmode[1] = inbytes[7]
            enable14bitmode[2] = inbytes[8]                
            smoothingSamples14bit = inbytes[9]            
            smoothingSamples7bit = inbytes[10]            
            newValueThreshold14bit = inbytes[11]            
            newValueThreshold7bit = inbytes[12]            
            enableDebugMode = inbytes[13]
            print("loaded current config from sysex")

 # Overwrite defaults with saved values from non-volatile memory if present.
 # This grabs from the NVM and sets each runtime var.
stored_config = read_from_nvm(14)

## Uncomment this to reset the device to defaults
# save_to_nvm(defaultConfigBytes)

if stored_config is not None:
    if not isinstance(stored_config, (bytes, bytearray)):
        raise ValueError("Stored config is not a valid bytes object.")
    print("Stored config:", list(stored_config))
    load_config_from_bytes(stored_config)  
else:
    # There's nothing in the NVM so this might be the first run on this Pico. 
    # Write the defaults to NVM:
    save_to_nvm(defaultConfigBytes)


i2cFound = False

midi = [adafruit_midi.MIDI(midi_out=usb_midi.ports[1], out_channel=faderMidiChannel[0]),\
            adafruit_midi.MIDI(midi_out=usb_midi.ports[1], out_channel=faderMidiChannel[1]),\
            adafruit_midi.MIDI(midi_out=usb_midi.ports[1], out_channel=faderMidiChannel[2]),\
            adafruit_midi.MIDI(midi_in=usb_midi.ports[0], in_channel=0)]

faderPrevOutput = [0, 0, 0]
faderADCValue = [0.0, 0.0, 0.0]
global faders

messageCounter = [0, 0, 0]

try:
    if enableDebugMode: print ("Checking I2C")
    i2c = busio.I2C(board.GP17, board.GP16, frequency=1000000)
    if enableDebugMode: print ("Found I2C")
    import adafruit_ads1x15.ads1115 as ADS

    from adafruit_ads1x15.analog_in import AnalogIn
    from adafruit_ads1x15.ads1x15 import Mode

    ads = ADS.ADS1115(i2c)
    
    # Continuous mode is megafast when using one channel, but causes chaos when using multiple 
    # channels. It gives you the most recent value that the ADS1115 has started to convert, on
    # ANY pin, without waiting for it to be converted; so all pins get all values at once and
    # the data is basically useless. 
    # You might want to set up Fader3 with two faders doing 7-bit control and wired to the 
    # built-in Pico ADCs and the third wired to the ADS1115. The code might work as-is; if not,
    # it should be easy to adapt.
    #ads.mode = Mode.CONTINUOUS

    # Data rate must be one of: [8, 16, 32, 64, 128, 250, 475, 860]
    # 250 is a good sample rate for avoiding stationary jitter when using one channel, but for
    # multiple channels it needs to be as high as possible in Single mode.
    ads.data_rate = 860
    ads.gain = 1
    faders = [0, 0, 0]
    faders[0] = AnalogIn(ads, ADS.P0)        
    faders[1] = AnalogIn(ads, ADS.P1)        
    faders[2] = AnalogIn(ads, ADS.P2)
        

    i2cFound = True
except Exception as e:
    if enableDebugMode: print ("Failed: ", e)
    if enableDebugMode: print ("Fader3 will run in 7-bit MIDI mode since no ADS1115 breakout \
                                board was found.")
    enable14bitmode = False
    faders = [analogio.AnalogIn(board.A0),analogio.AnalogIn(board.A1),analogio.AnalogIn(board.A2)]


while True:
    
    for f in range(0,3):                  
        if i2cFound:                
            # Average n reads to reduce jitter
            faderaverage = 0
            for i in range(0, int(smoothingSamples14bit)):                    
                faderaverage += faders[f].value

            if(smoothingSamples14bit > 0):
                faderADCValue[f] = faderaverage / smoothingSamples14bit
            
            # The effective bit-depth of the ADS1115 is 15, since the 16th is used as 
            # the sign byte. Plus we lose some resolution in the voltage range we're 
            # measuring (I'm not 100% sure what it is). So this magic number of 25860 
            # is the average maximum observed raw value. The min observed value is 
            # anywhere between 0 and 200, depending on the velocity at which you zero 
            # the fader...

            # scale observed range max to 14bit maxD
            faderADCValue[f] = (int(faderADCValue[f]) * 16383) // 26000

            faderADCValue[f] = faderADCValue[f] & 0x3FFF # Mask to 14bit

            # Just to be safe, let's clamp the output to 14-bit MIDI range
            faderADCValue[f] = clamp(faderADCValue[f], 0, 16383)

            if enable14bitmode[f]:                

                if abs(faderADCValue[f] - faderPrevOutput[f]) >= newValueThreshold14bit:    
                    if enableDebugMode: print (f'Count: {messageCounter[f]}' + ' Fader'+str(f) + ' Chan: ' + str(faderMidiChannel[f]) \
                                                + ' 14bit: ' + str(faderADCValue[f]))
                    faderPrevOutput[f] = faderADCValue[f]
                    ccUpper = faderADCValue[f] & 0x7f
                    ccLower = (faderADCValue[f] >> 7) & 0x7f
                
                    midi[f].send(ControlChange(faderCCNumber[f]+32, ccUpper))
                    midi[f].send(ControlChange(faderCCNumber[f], ccLower))
                    messageCounter[f] += 1                        

            else: # send 7-bit messages
                fader7bitvalue = faderADCValue[f] & 0x7f

                # Just to be safe, let's clamp the output to 7-bit MIDI range
                fader7bitvalue = clamp(fader7bitvalue, 0, 127)

                if abs(fader7bitvalue - faderPrevOutput[f]) >= newValueThreshold7bit:    
                    faderPrevOutput[f] = fader7bitvalue
                    if enableDebugMode: print ('Fader' + str(f) + ' 7bit: ' +str(fader7bitvalue))

                    midi[f].send(ControlChange(faderCCNumber[f], fader7bitvalue))
                    
                
        else: # no external ADC found, so we'll use the Pico's onboard ADC

            # Average n reads to reduce jitter
            faderaverage = 0
            for i in range(0, int(smoothingSamples14bit)):
                # Zeroing the lower 4 bits mitigates noise in the upper part of Pico's ADC range.
                # 8bit resolution is adequate for our purposes.
                reading8bit = (faders[f].value >> 4) << 4
                faderaverage += reading8bit

            faderADCValue[f] = faderaverage / smoothingSamples14bit

            # Convert to MIDI CC range 0-127 if using 7-bit
            fader7bitvalue = int(faderADCValue[f]/65535 * 127)

            # Just to be safe, let's clamp the output to 7-bit MIDI range
            fader7bitvalue = clamp(fader7bitvalue, 0, 127)        

            # If the new value is different to the old one by the threshold amount or greater,
            # send a CC message.
            if abs(fader7bitvalue - faderPrevOutput[f]) >= newValueThreshold7bit:
                faderPrevOutput[f] = fader7bitvalue
                midi[f].send(ControlChange(faderCCNumber[f], fader7bitvalue))
                    
    # There are two types of sysex message we're looking for. The first starts with [0,A] and instructs
    # us to send a settings dump. Settings dumps are prefixed with [0,B]. If we get a settings dump back,
    # then we use that to set the local vars.
    msg = midi[3].receive()
    if msg is not None:
        print(msg)
        if isinstance(msg, SystemExclusive):           
            print("Received sysex message:")    
            print(f"Manufacturer ID: {msg.manufacturer_id}")
            print(list(msg.data))
            if int.from_bytes(msg.manufacturer_id, "big") == 0x7d:
                # That's us. Read the message.
                
                if len(msg.data) >= 1 and msg.data[0] == 0 and msg.data[1] == 0xa:
                    print("Dump was requested by remote machine!")

                    outmsg = SystemExclusive([0x7D], list(read_from_nvm(14)))
                    
                    midi[0].send(outmsg)
                    print(outmsg)

                if len(msg.data) >= 1 and msg.data[0] == 0 and msg.data[1] == 11:
                    print("Received a settings dump from remote machine")
                    # Strip the validation bytes
                    print(msg.data)
                    prepped = msg.data[2:]
                    print(prepped)
                    # Load the new values 
                    load_config_from_bytes(prepped)
                    # Write the new values to the NVM
                    save_to_nvm(prepped)
                    # reload code.py to properly reinitialise MIDI channels
                    supervisor.reload()
        else:
            print("non-sysex message: ", msg)
