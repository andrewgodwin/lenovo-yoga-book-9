
.PHONY: install
install:
	install -m 755 -t /usr/local/bin/ yoga-kde-rotation
	cp yoga-kde-rotation.service /etc/systemd/system/
	systemctl daemon-reload
	systemctl enable yoga-kde-rotation.service
	systemctl start yoga-kde-rotation.service
