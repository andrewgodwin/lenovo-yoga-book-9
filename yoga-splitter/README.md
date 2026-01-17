# Yoga Multitouch Splitter

The Yoga Book 9 has a HID touchscreen device that emits events for the two separate panels as two report IDs; the top screen on 0x30, and the bottom screen on 0x38.

The `hid-multitouch` driver, at least as of when I write this, does not understand the 0x38 reports and just bins them, only interpreting the 0x30 reports.

This small daemon reads the HID device directly and interprets both sets of reports, sending them to two new virtual input devices, which can then be mapped to the right screens in your DE.

(It also flips the coordinates of the top screen as the digitizer is installed in the opposite direction to the underlying display)


## Installation

```
make
sudo make install
```

Then go into your desktop environment and set "Yoga Top Multitouch" to be mapped to the top screen, "Yoga Bottom Multitouch" to be mapped to the bottom screen, and then disable any input from any INGENIC devices (my touchscreen appears as "INGENEIC Gadget Serial and keyboard"). In KDE this is all in System Settings - Touchscreen; I have no idea where it might be in other DEs.

## Issues

There's some funky behaviour with the slots (fingers) in the underlying device; if there is only one contact point, it will always shove that contact point into the first slot, even if it was a later finger before. It's possible this is for legacy compatibility with systems that just read the first slot for non-multitouch input; either way, it means contact_count = 1 is special cased.

I also used to have some occasional "lingering touches" as I was writing this, so there is a timeout loop that lifts a finger if it's not been seen for more than 500ms (in my experience, even holding a finger still emits a ton of reports). I suspect I have now fixed the cause of this with the contact_count special case, but I am leaving it in for now just in case.
