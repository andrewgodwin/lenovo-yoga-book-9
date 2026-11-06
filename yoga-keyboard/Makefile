yoga-keyboard: yoga-keyboard.c
	gcc yoga-keyboard.c -o yoga-keyboard

.PHONY: install
install:
	install -m 755 -t /usr/local/bin yoga-keyboard
	cp 70-yoga-keyboard.rules /etc/udev/rules.d/
	udevadm control --reload
	cp yoga-keyboard.service /etc/systemd/system/
	systemctl daemon-reload
	systemctl enable yoga-keyboard.service
	systemctl restart yoga-keyboard.service
