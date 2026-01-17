# Linux tweaks for the Lenovo Yoga Book 9 (13IMU9)

This repository contains my tweaks to get Linux running well on this machine. They might also be applicable to the Yoga Book 9i - I don't have one to test!

`sh install.sh` should install all the relevant tweaks.

## Already working on Fedora 43 / Kernel 6.17

WiFi, Bluetooth, battery control, power modes, USB devices

## Suspend/ACPI

Works sporadically out of the box, but seems rock solid with the kernel argument `pci=hpiosize=0`

## Screen Orientation

To fix it during boot and framebuffer consoles, add `video=eDP-1:panel_orientation=upside_down` to the kernel arguments.

Your compositor may or may not honor this; if it does not, just invert the top screen inside of it.

## Touchscreen

The top screen's panel is installed upside-down but its digitizer is installed the right way up. KWin (at least) thinks that the digitizer should be running the same direction as the screen and gets everything inverted, even with screen rotation.

The included HWDB tweaks file goes in `/etc/udev/hwdb.d` and inverts the touch and stylus coordinates (they are, naturally, on different scales) to make it work.

## Screen Rotation

TODO. KDE will happily auto-rotate the top screen with the accelerometer but not the bottom one.

## Speakers

TODO. I know some firmware shenanigans are likely involved.
