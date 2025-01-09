import storage, usb_cdc, usb_midi, usb_hid
import board, digitalio


startupkey = digitalio.DigitalInOut(board.GP0)
startupkey.direction = digitalio.Direction.INPUT
startupkey.pull = digitalio.Pull.UP

## WORK IN PROGRESS - this is pilfered from my Macro Pad code and I need a better startup sequence
## this time around. 

# If the top left button isn't held down on startup, disable
# mass storage, USB HID, and CDC (serial) modes
if startupkey.value:
    #storage.disable_usb_drive() # disabled for debugging
    #usb_midi.disable()
    
    #usb_cdc.disable()
    usb_hid.disable()

usb_hid.disable()
usb_midi.enable()
usb_midi.set_names(streaming_interface_name="Fader3 MIDI", audio_control_interface_name="Fader3 MIDI")
