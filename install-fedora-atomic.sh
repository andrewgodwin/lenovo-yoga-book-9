# Add kernel args
sudo rpm-ostree kargs --append-if-missing=pci=hpiosize=0 --append-if-missing=video=eDP-1:panel_orientation=upside_down --append-if-missing=acpi_osi=! --append-if-missing=acpi_backlight=native --append-if-missing=firmware_class.path=/etc/firmware

# Add udev rule to ignore the default touch device
sudo cp udev/99-lenovo-yoga-book-9.rules /etc/udev/rules.d/

# Add HWDB quirk for top screen coordinates
sudo cp hwdb/99-lenovo-yoga-book-9.hwdb /etc/udev/hwdb.d/
sudo systemd-hwdb update

# Install the speaker amp firmware (it's not in linux-firmware)
sudo install -D -m 0644 firmware/TAS2XXX38F8.bin /etc/firmware/TAS2XXX38F8.bin

# Make the volume control scale both speaker paths, set their balance, and
# reload the amp firmware at each playback start
sudo install -D -m 0644 wireplumber/99-lenovo-yoga-book-9.conf /etc/wireplumber/wireplumber.conf.d/
amixer -c0 -q cset iface=CARD,name='Speaker Force Firmware Load' on
amixer -c0 -q cset name='Master Playback Volume' 87
amixer -c0 -q cset name='Speaker Playback Volume' 81,81
amixer -c0 -q cset name='DAC2 Playback Volume' 87,87
sudo alsactl store

# Install and enable the touchscreen daemon
cd yoga-splitter
make
sudo make install

# Install and enable the KDE screen rotation
cd ../kde-rotation
sudo make install
