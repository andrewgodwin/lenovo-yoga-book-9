# Linux tweaks for the Lenovo Yoga Book 9 (13IMU9)

This repository contains my tweaks to get Linux running well on this machine. They might also be applicable to the Yoga Book 9i - I don't have one to test!

`sh install-fedora-atomic.sh` should install all the relevant tweaks if you are running Fedora Atomic/Universal Blue/Bazzite.

## Already working on Fedora 43 / Kernel 6.17

WiFi, Bluetooth, battery control, power modes, USB devices

## Suspend/ACPI

Works sporadically out of the box, but seems rock solid with the kernel argument `pci=hpiosize=0`

## Screen Orientation

To fix it during boot and framebuffer consoles, add `video=eDP-1:panel_orientation=upside_down` to the kernel arguments.

Your compositor may or may not honor this; if it does not, just invert the top screen inside of it.

## Touchscreen

The top screen's panel is installed upside-down but its digitizer is installed the right way up, and the bottom touchscreen reports touch events on a non-standard HID report code (0x38), on the same HID device.

The "cleanest" way I found to fix this, pending a `hid-multitouch` fix, is a small daemon that opens the raw HID device, decodes the touch points itself (basically acting as a poor replica of `hid-multitouch`), and then sends the touch events to two new virtual input devices, one for each screen. That code is contained in the `yoga-splitter` subdirectory.

I also included a HWDB quirks file that just fixes the coordinates for the top screen, if you don't want the bottom touchscreen to work.

## Screen Rotation

TODO. KDE will happily auto-rotate the top screen with the accelerometer but not the bottom one.

## Speakers

TODO. I know some firmware shenanigans are likely involved.
