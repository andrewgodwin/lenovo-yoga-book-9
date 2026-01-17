sudo rpm-ostree kargs --append=pci=hpiosize=0 --append=video=eDP-1:panel_orientation=upside_down
sudo cp 99-lenovo-yoga-book-9.hwdb /etc/udev/hwdb.d/99-lenovo-yoga-book-9.hwdb
sudo systemd-hwdb update
