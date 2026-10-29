CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -pthread
LDFLAGS ?= -pthread -lgpiod -lm

DAEMON_SRCS = src/main.c src/ssd1306.c src/font.c src/telemetry.c src/button.c src/config.c
CLI_SRCS = src/cli.c
LEGACY_SRCS = legacy/argon_legacy_oled.c

all: argon-v5-oled argon-oled argon-1v5-oled-legacy

argon-v5-oled: $(DAEMON_SRCS)
	$(CC) $(CFLAGS) $(DAEMON_SRCS) $(LDFLAGS) -o $@

argon-oled: $(CLI_SRCS)
	$(CC) $(CFLAGS) $(CLI_SRCS) -o $@

argon-1v5-oled-legacy: $(LEGACY_SRCS) legacy/legacy_assets.h
	$(CC) $(CFLAGS) $(LEGACY_SRCS) -pthread -lgpiod -o $@

clean:
	rm -f argon-v5-oled argon-oled argon-1v5-oled-legacy
	rm -rf dist legacy/pkg/usr legacy/pkg/etc legacy/pkg/lib essentials/pkg/usr essentials/pkg/etc

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
	dpkg-deb --build --root-owner-group pkg dist/argon-v5-oled_1.1.0-alpha_arm64.deb
	cd dist && sha256sum *.deb > SHA256SUMS

deb-legacy: argon-1v5-oled-legacy
	mkdir -p dist
	mkdir -p legacy/pkg/usr/bin legacy/pkg/usr/share/argon-1v5-oled-legacy/oled legacy/pkg/etc/systemd/system legacy/pkg/usr/lib/systemd/system-shutdown
	cp argon-1v5-oled-legacy legacy/pkg/usr/bin/
	cp legacy/argonone-oledconfig legacy/pkg/usr/bin/
	cp legacy/oled/*.bin legacy/pkg/usr/share/argon-1v5-oled-legacy/oled/
	cp legacy/argon-1v5-oled-legacy.service legacy/pkg/etc/systemd/system/
	cp legacy/argon-1v5-oled-shutdown legacy/pkg/usr/lib/systemd/system-shutdown/
	chmod 755 legacy/pkg/usr/bin/* legacy/pkg/usr/lib/systemd/system-shutdown/* legacy/pkg/DEBIAN/postinst legacy/pkg/DEBIAN/prerm legacy/pkg/DEBIAN/postrm
	dpkg-deb --build --root-owner-group legacy/pkg dist/argon-1v5-oled-legacy_1.0.0_arm64.deb
	cd dist && sha256sum *.deb > SHA256SUMS

deb-essentials:
	mkdir -p dist
	mkdir -p essentials/pkg/usr/bin essentials/pkg/etc/sysctl.d essentials/pkg/etc/udev/rules.d
	cp essentials/argon-v5-essentials essentials/pkg/usr/bin/
	cp essentials/90-argon-v5-perf.conf essentials/pkg/etc/sysctl.d/
	cp essentials/60-argon-v5-perf.rules essentials/pkg/etc/udev/rules.d/
	chmod 755 essentials/pkg/usr/bin/* essentials/pkg/DEBIAN/postinst essentials/pkg/DEBIAN/postrm
	chmod 644 essentials/pkg/etc/sysctl.d/* essentials/pkg/etc/udev/rules.d/*
	dpkg-deb --build --root-owner-group essentials/pkg dist/argon-one-v5-essentials_1.0.0_arm64.deb
	cd dist && sha256sum *.deb > SHA256SUMS

.PHONY: all clean install deb deb-legacy deb-essentials
