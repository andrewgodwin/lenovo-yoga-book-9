# Yoga Keyboard Position

The Yoga Book 9's embedded controller knows where the magnetic keyboard is, but you need root to read it, which doesn't work for a sandboxed on-screen keyboard.

This small daemon republishes it as a virtual input device called "Yoga Keyboard Position", which the logged-in user can read, even from inside a Flatpak with `--device=all`.

## Installation

```
make
sudo make install
```

## Reading it

Find the device in `/dev/input/event*` by name. It has one axis, `ABS_MISC`, with this value:

* 0: Not on the screen
* 1: Against the hinge
* 2: In the normal typing position at the far edge

Use the `EVIOCGABS` ioctl to get the current value at startup, then read `input_event`s from the device to get changes.

It's an axis and not switches as logind thinks you're docked if `SW_DOCK` is on, and then lid close won't suspend. Nothing else cares about an `ABS_MISC`-only device, so it doesn't show up as a joystick or in libinput.
