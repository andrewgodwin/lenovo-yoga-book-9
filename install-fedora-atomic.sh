# Add kernel args
sudo rpm-ostree kargs --append=pci=hpiosize=0 --append=video=eDP-1:panel_orientation=upside_down

# Add HWDB quirk for top screen coordinates
sudo cp 99-lenovo-yoga-book-9.hwdb /etc/udev/hwdb.d/99-lenovo-yoga-book-9.hwdb
sudo systemd-hwdb update

# Install and enable the touchscreen daemon
cd yoga-splitter
make
sudo make install
