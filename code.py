import board, analogio, usb_midi, adafruit_midi
from time import sleep
faders = [analogio.AnalogIn(board.A0),analogio.AnalogIn(board.A1),analogio.AnalogIn(board.A2)]

from adafruit_midi.control_change import ControlChange

sleep(1) # avoid a race condition on some systems

## CONFIG SECTION

# Each fader can send messages on its own channel. Set them here.
# (Remember, most DAWs count MIDI channels starting from 1. Here we start from 0.)
faderMIDIchannels = [0, 0, 0]
# Configure which CC number each fader should be assigned to
faderCCnumbers = [14, 15, 33]
# Selectively enable/disable faders (disabled faders will be ignored)
faderEnabled = [0, 1, 0]
# Set the number of ADC samples from which to get a smoothed value.
# Greater values may reduce jitter.
smoothingSamples = 44.0
# Increasing may reduce jitter, but will also reduce responsiveness/accuracy
newValueThreshold = 1

## CONFIG SECTION END

# Don't touch these
midi = [adafruit_midi.MIDI(midi_out=usb_midi.ports[1], out_channel=faderMIDIchannels[0]),adafruit_midi.MIDI(midi_out=usb_midi.ports[1], out_channel=faderMIDIchannels[1]),adafruit_midi.MIDI(midi_out=usb_midi.ports[1], out_channel=faderMIDIchannels[2])]
faderPrevOutput = [0, 0, 0]
faderADCValue = [0.0, 0.0, 0.0]

while True:

    for f in range(0,3):
        if faderEnabled[f] > 0:

            # Average n reads to reduce jitter
            faderaverage = 0
            for i in range(0, int(smoothingSamples)):

                faderaverage += faders[f].value

            faderADCValue[f] = faderaverage / smoothingSamples

            # Convert to MIDI CC range 0-127
            CCoutput = int(faderADCValue[f]/65535*127)

            # If the new value is different to the old one by the threshold amount or greater,
            # send a CC message.
            if abs(CCoutput - faderPrevOutput[f]) >= newValueThreshold:
                print (CCoutput)
                faderPrevOutput[f] = CCoutput
                midi[f].send(ControlChange(faderCCnumbers[f], CCoutput))


    #sleep(0.02)

