# Linux tweaks for the Lenovo Yoga Book 9 (83FF / 13IMU9)

This repository contains my tweaks to get Linux running well on this machine. They might also be applicable to the Yoga Book 9i - I don't have one to test!

`sh install-fedora-atomic.sh` should install all the relevant tweaks if you are running Fedora Atomic/Universal Blue/Bazzite.

## Already working on Fedora 43 / Kernel 6.17

WiFi, Bluetooth, battery control, power modes, USB devices

## Suspend/ACPI

Works sporadically out of the box, but seems rock solid with the kernel arguments `pci=hpiosize=0` (not quite sure why) and `acpi_osi=!`.

`acpi_osi=!` stops Linux answering yes to the firmware's Windows version checks. The firmware only checks up to `Windows 2015`, and this makes it fall back to its non-Windows OS value (`OSYS` is 1000 rather than 2015). That changes the value it sends to the embedded controller on wake, which is probably what fixes resume, though it also changes some other firmware behaviour, so that's not proven.

## Screen Orientation

To fix it during boot and framebuffer consoles, add `video=eDP-1:panel_orientation=upside_down` to the kernel arguments.

Your compositor may or may not honor this; if it does not, just invert the top screen inside of it.

## Brightness

The `acpi_osi=!` argument above makes the kernel pick the ACPI backlight devices (`acpi_video0` and `acpi_video1`) over the native i915 ones. On kernel 7.2 those devices accept values but don't change either panel, so the top screen's brightness controls do nothing.

Add `acpi_backlight=native` to the kernel arguments to fix it. You should then have `intel_backlight` (top screen) and `card1-eDP-2-backlight` (bottom screen) in `/sys/class/backlight`, and both screens should respond to the brightness keys and KDE's slider.

## Touchscreen

The top screen's panel is installed upside-down but its digitizer is installed the right way up, and the bottom touchscreen reports touch events on a non-standard HID report code (0x38), on the same HID device.

The "cleanest" way I found to fix this, pending a `hid-multitouch` fix, is a small daemon that opens the raw HID device, decodes the touch points itself (basically acting as a poor replica of `hid-multitouch`), and then sends the touch events to two new virtual input devices, one for each screen. That code is contained in the `yoga-splitter` subdirectory.

I also included a HWDB quirks file that just fixes the coordinates for the top screen, if you don't want the bottom touchscreen to work. There is also a udev rules file that removes the default input device from the touchscreen so you don't have two.

## Screen Rotation

KDE auto-rotates only the top screen (eDP-1) with the accelerometer. The `kde-rotation` directory contains a script that monitors orientation changes and syncs the bottom screen (eDP-2) rotation to match. It's likely easiest to just add it to KDE autostart.

## Speakers

There are two sets of speakers: the Realtek ALC287's `Speaker` output handles treble, and two TI TAS2781 smart amps handle the bass. The amps need the firmware file `TAS2XXX38F8.bin`, which isn't in `linux-firmware` yet - a copy is in this repo, extracted from the Windows drivers.

The install script copies it to `/etc/firmware` and adds `firmware_class.path=/etc/firmware` to the kernel arguments, so it all works on atomic distributions (like Bazzite, where this fix was tested). The calibration/volumes are untested, so probably don't drive it too high at/above full volume.

By default the system volume only changes the treble path (`Speaker`), not the bass path (`DAC2`). The WirePlumber rule in `wireplumber/` switches to software volume so they change together, and the install script fixes the HW levels at `Speaker` 81 and `DAC2` 87, which sets the treble 4.5dB below the bass, and then `alsactl store` saves those levels across reboots.

The amps also go silent once they've power-saved, even with the firmware loaded, apparently because they lose their DSP program. Turning on the `Speaker Force Firmware Load` control fixes this by reloading it at each playback start, so the install script sets that too. The reload also resets `Speaker Analog Volume` to its default of 14, so there's no point setting that.

## Keyboard Detection

The embedded controller knows if the magnetic keyboard is on the bottom screen, and where. Linux has no driver for this, so nothing acts on it yet, but the data is there if you want to do something like reserve that part of the screen.

The state is the `BKBD` field, bits 4-5 of the byte at physical address `0xFE0B0423`: 0 is off the screen, 1 is against the hinge, and 2 is the normal typing position at the far edge. A userspace program can read it as root by mapping `/dev/mem` read-only, as Bazzite's kernel has no `ec_sys` module. This won't work with Secure Boot's kernel lockdown on.

Every change also fires a WMI event, which shows up on the `acpi_event` generic netlink family (multicast group `acpi_mc_group`) with device class `wmi`, bus ID `PNP0C14:03` and type `0xEB`. The event carries no data, so read the byte when you get it, and once at startup.

The `yoga-keyboard` daemon does all of this for you, and republishes the position as a virtual input device that doesn't need root to read.
