import storage, usb_cdc
import board, digitalio

startupkey = digitalio.DigitalInOut(board.GP0)
startupkey.direction = digitalio.Direction.INPUT
startupkey.pull = digitalio.Pull.UP


# If the top left button isn't held down on startup, disable
# mass storage, USB HID, and CDC (serial) modes
if startupkey.value:
    #storage.disable_usb_drive() # disabled for debugging
    #usb_midi.disable()
    usb_cdc.disable()
    usb_hid.disable()

