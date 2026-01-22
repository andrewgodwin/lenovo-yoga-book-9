# Add kernel args
sudo rpm-ostree kargs --append-if-missing=pci=hpiosize=0 --append-if-missing=video=eDP-1:panel_orientation=upside_down

# Add udev rule to ignore the default touch device
sudo cp udev/99-lenovo-yoga-book-9.rules /etc/udev/rules.d/

# Add HWDB quirk for top screen coordinates
sudo cp hwdb/99-lenovo-yoga-book-9.hwdb /etc/udev/hwdb.d/
sudo systemd-hwdb update

# Install and enable the touchscreen daemon
cd yoga-splitter
make
sudo make install

# Install and enable the KDE screen rotation
cd ../kde-rotation
sudo make install
