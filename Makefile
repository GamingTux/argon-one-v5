CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -pthread
LDFLAGS ?= -pthread -lgpiod -lm

DAEMON_SRCS = src/main.c src/ssd1306.c src/font.c src/telemetry.c src/button.c src/config.c
CLI_SRCS = src/cli.c

all: argon-v5-oled argon-oled

argon-v5-oled: $(DAEMON_SRCS)
	$(CC) $(CFLAGS) $(DAEMON_SRCS) $(LDFLAGS) -o $@

argon-oled: $(CLI_SRCS)
	$(CC) $(CFLAGS) $(CLI_SRCS) -o $@

clean:
	rm -f argon-v5-oled argon-oled
	rm -rf dist

install: all
	install -d $(DESTDIR)/usr/bin
	install -m 755 argon-v5-oled $(DESTDIR)/usr/bin/argon-v5-oled
	install -m 755 argon-oled $(DESTDIR)/usr/bin/argon-oled
	install -m 755 argon-oled-desktop $(DESTDIR)/usr/bin/argon-oled-desktop
	install -m 755 argon_hw_discovery.py $(DESTDIR)/usr/bin/argon-hw-discover
	install -m 755 argon_card_builder.py $(DESTDIR)/usr/bin/argon-card-builder
	install -m 755 oled_engine.py $(DESTDIR)/usr/bin/oled_engine.py
	install -d $(DESTDIR)/etc
	install -m 644 argon-v5-oled.conf $(DESTDIR)/etc/argon-v5-oled.conf
	install -d $(DESTDIR)/etc/systemd/system
	install -m 644 argon-v5-oled.service $(DESTDIR)/etc/systemd/system/argon-v5-oled.service
	install -d $(DESTDIR)/usr/share/applications
	install -m 644 pkg/usr/share/applications/argon-oled-desktop.desktop $(DESTDIR)/usr/share/applications/argon-oled-desktop.desktop

deb: all
	mkdir -p dist
	mkdir -p pkg/usr/bin pkg/etc pkg/etc/systemd/system pkg/usr/share/applications
	cp argon-v5-oled pkg/usr/bin/
	cp argon-oled pkg/usr/bin/
	cp argon-oled-desktop pkg/usr/bin/
	cp argon_hw_discovery.py pkg/usr/bin/argon-hw-discover
	cp argon_card_builder.py pkg/usr/bin/argon-card-builder
	cp oled_engine.py pkg/usr/bin/
	cp argon-v5-oled.conf pkg/etc/
	cp argon-v5-oled.service pkg/etc/systemd/system/
	chmod 755 pkg/usr/bin/*
	dpkg-deb --build --root-owner-group pkg dist/argon-v5-oled_0.1.0-alpha_arm64.deb
	cd dist && sha256sum argon-v5-oled_0.1.0-alpha_arm64.deb > SHA256SUMS

.PHONY: all clean install deb
