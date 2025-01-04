import board, analogio, usb_midi, adafruit_midi, busio, microcontroller
from time import sleep
from adafruit_midi.control_change import ControlChange

sleep(1) # avoid a race condition on some systems

print(f"Original CPU Frequency: {microcontroller.cpu.frequency}Hz")
microcontroller.cpu.frequency = 270_000_000

def clamp(value, min_val, max_val):
    return max(min(value, max_val), min_val)

def mapRange(value, inMin, inMax, outMin, outMax):
    return outMin + (((value - inMin) / (inMax - inMin)) * (outMax - outMin))

####################
## CONFIG SECTION ##
####################

## TO DO - add some sysex config guff, like I did with Knobber a thousand years ago.

# Selectively enable/disable faders (disabled faders will be ignored)
faderEnabled = [True, True, True]
# Each fader can send messages on its own channel. Set them here.
# (Remember, most DAWs count MIDI channels starting from 1. Here we start from 0.)
faderMidiChannel = [0, 0, 0]
# Configure which CC number each fader should be assigned to. Note: in 14-bit mode, every CC
# uses the given number + 32 (as per the original MIDI spec, mostly ignored by manufacturers 
# since it was written). So CC15 in 14-bit mode uses CCs 14 and 47. Spec compliant receivers 
# should identify these message pairs as a 14-bit control, or at least let you configure the
# mapping accordingly.
faderCCNumber = [14, 15, 30]
# When true, and if an ADS1115 board is found, CCs will be sent as 14bit MIDI pairs
enable14bitmode = [True, True, True]
# Optionally scale the output to this range (per fader). In 7-bit mode, max values over 127 
# will be clamped. In 14-bit mode, max values over 16383 will be clamped. Swap larger and 
# smaller values to invert the output.
faderCustomRange = [[0,16383],[0, 16383],[0,16383]]
# Set the number of ADC samples from which to get a smoothed value.
smoothingSamples14bit = [2.0, 2.0, 2.0]
smoothingSamples7bit = [4.0, 4.0, 4.0]
# Increasing may reduce jitter, but will also reduce responsiveness/accuracy
newValueThreshold14bit = [20, 20, 20]
newValueThreshold7bit = [1, 1, 1]
# If true, and usb_cdc.disable() is commented out in boot.py, debug messages will be printed 
# to serial monitor
enableDebugMode = True

########################
## CONFIG SECTION END ##
########################


#######################
## Don't touch these ##
#######################
i2cFound = False
midi = [adafruit_midi.MIDI(midi_out=usb_midi.ports[1], out_channel=faderMidiChannel[0]),\
        adafruit_midi.MIDI(midi_out=usb_midi.ports[1], out_channel=faderMidiChannel[1]),\
        adafruit_midi.MIDI(midi_out=usb_midi.ports[1], out_channel=faderMidiChannel[2])]
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
    if faderEnabled[0]:
        faders[0] = AnalogIn(ads, ADS.P0)
    if faderEnabled[1]:
        faders[1] = AnalogIn(ads, ADS.P1)
    if faderEnabled[2]:
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
        if faderEnabled[f]:            

            if i2cFound:
                
                # Average n reads to reduce jitter
                faderaverage = 0
                for i in range(0, int(smoothingSamples14bit[f])):                    
                    faderaverage += faders[f].value

                faderADCValue[f] = faderaverage / smoothingSamples14bit[f]

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
                
                    # First map the value to the custom scale
                    fadercustommin = clamp(faderCustomRange[f][0], 0, 16382)
                    fadercustommax = clamp(faderCustomRange[f][1], 1, 16383)                    
                    faderADCValue[f] = int(mapRange(faderADCValue[f], 0, 16383, fadercustommin, fadercustommax))

                    if abs(faderADCValue[f] - faderPrevOutput[f]) >= newValueThreshold14bit[f]:    
                        if enableDebugMode: print (f'Count: {messageCounter[f]}' + ' Fader'+str(f) + ' 16bit: ' + str(faders[f].value) \
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

                    # First map the value to the custom scale
                    fadercustommin = clamp(faderCustomRange[f][0], 0, 126)
                    fadercustommax = clamp(faderCustomRange[f][1], 1, 127)                    
                    faderADCValue[f] = int(mapRange(faderADCValue[f], 0, 127, fadercustommin, fadercustommax))

                    if abs(fader7bitvalue - faderPrevOutput[f]) >= newValueThreshold7bit[f]:    
                        faderPrevOutput[f] = fader7bitvalue
                        if enableDebugMode: print ('Fader' + str(f) + ' 7bit: ' +str(fader7bitvalue))

                        midi[f].send(ControlChange(faderCCNumber[f], fader7bitvalue))
                        
                    
            else: # no external ADC found, so we'll use the Pico's onboard ADC

                # Average n reads to reduce jitter
                faderaverage = 0
                for i in range(0, int(smoothingSamples14bit[f])):
                    # Zeroing the lower 4 bits mitigates noise in the upper part of Pico's ADC range.
                    # 8bit resolution is adequate for our purposes.
                    reading8bit = (faders[f].value >> 4) << 4
                    faderaverage += reading8bit

                faderADCValue[f] = faderaverage / smoothingSamples14bit[f]

                # Convert to MIDI CC range 0-127 if using 7-bit
                fader7bitvalue = int(faderADCValue[f]/65535 * 127)

                # Just to be safe, let's clamp the output to 7-bit MIDI range
                fader7bitvalue = clamp(fader7bitvalue, 0, 127)
            
                # First map the value to the custom scale
                fadercustommin = clamp(faderCustomRange[f][0], 0, 126)
                fadercustommax = clamp(faderCustomRange[f][1], 1, 127)                    
                faderADCValue[f] = int(mapRange(faderADCValue[f], 0, 127, fadercustommin, fadercustommax))

                # If the new value is different to the old one by the threshold amount or greater,
                # send a CC message.
                if abs(fader7bitvalue - faderPrevOutput[f]) >= newValueThreshold7bit[f]:
                    faderPrevOutput[f] = fader7bitvalue
                    midi[f].send(ControlChange(faderCCNumber[f], fader7bitvalue))
                        
    #sleep(0.01)
