CFLAGS ?= -O2 -Wall
PKGS = gio-2.0 glib-2.0

flashlight-sysfs: src/flashlight-sysfs.c
	$(CC) $(CFLAGS) -o $@ $< $(shell pkg-config --cflags --libs $(PKGS))

install: flashlight-sysfs
	install -D -m 0755 flashlight-sysfs $(DESTDIR)/usr/bin/flashlight-sysfs
	install -D -m 0644 flashlight-sysfs.conf $(DESTDIR)/etc/flashlight-sysfs.conf
	install -D -m 0644 org.sailfish.flashlight.provider.service $(DESTDIR)/usr/share/dbus-1/services/org.sailfish.flashlight.provider.service
	install -D -m 0644 flashlight-sysfs-permissions.service $(DESTDIR)/usr/lib/systemd/system/flashlight-sysfs-permissions.service
	install -d $(DESTDIR)/usr/lib/systemd/system/multi-user.target.wants
	ln -sf ../flashlight-sysfs-permissions.service $(DESTDIR)/usr/lib/systemd/system/multi-user.target.wants/flashlight-sysfs-permissions.service

clean:
	rm -f flashlight-sysfs
