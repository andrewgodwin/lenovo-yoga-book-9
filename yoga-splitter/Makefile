yoga-splitter: yoga-splitter.c
	gcc yoga-splitter.c -o yoga-splitter

.PHONY: debug
debug: yoga-splitter.c
	gcc -DDEBUG=true yoga-splitter.c -o yoga-splitter

.PHONY: install
install:
	install -m 755 -t /usr/local/bin yoga-splitter
	cp yoga-splitter.service /etc/systemd/system/
	systemctl daemon-reload
	systemctl enable yoga-splitter.service
	systemctl start yoga-splitter.service
