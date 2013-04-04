# App info
APPNAME = bluelog
VERSION = 1.0.5

# Bluelog-specific, select which CSS theme to use as default
# Options: digifail.css, backtrack.css, pwnplug.css, openwrt.css
DEFAULT_CSS = digifail.css

# Debug, build as if on OpenWRT
#TARGET = -DOPENWRT
# Pwn Plug
#TARGET = -DPWNPLUG
# Raspberry-pi
#TARGET = -DRPI -march=armv6 -mfpu=vfp -mfloat-abi=hard

# Compiler and options
CC = gcc
CFLAGS += -Wall -O2 $(TARGET)

# Libraries to link
LIBS = -lbluetooth -lsqlite3

# Files
DOCS = ChangeLog COPYING README README.LIVE README.BAKTRK FUTURE

INSTALL = install -o root
INSTALL_PROGRAM = $(INSTALL) -D
INSTALL_DATA = $(INSTALL) -D -m0644 -g root
INSTALL_DIR = $(INSTALL) -d -m0755 -g root
SCRONTABS = /etc/cron.d


# Livelog.cgi prefix
CGIPRE = www/cgi-bin/

# Targets
# Build Bluelog
bluelog: bluelog.c
	$(CC) $(CFLAGS) bluelog.c $(LIBS) -o $(APPNAME)

# Build CGI module
livelog: livelog.c
	$(CC) $(CFLAGS) livelog.c -o $(CGIPRE)livelog.cgi

# Build tarball
release: clean
	tar --exclude='.*' -C ../ -czvf /tmp/$(APPNAME)-$(VERSION).tar.gz $(APPNAME)-$(VERSION)

# Clean for dist
clean:
	rm -rf $(APPNAME) $(CGIPRE)livelog.cgi *.o *.txt *.log *.gz *.cgi

# Install to system
install: bluelog livelog
	mkdir -p $(DESTDIR)/usr/bin/
	mkdir -p $(DESTDIR)/usr/share/doc/$(APPNAME)-$(VERSION)/
	mkdir -p $(DESTDIR)/usr/share/man/man1
	mkdir -p $(DESTDIR)/usr/share/$(APPNAME)/
	cp $(APPNAME) $(DESTDIR)/usr/bin/
	cp -a $(DOCS) $(DESTDIR)/usr/share/doc/$(APPNAME)-$(VERSION)/
	gzip -c $(APPNAME).1 >> $(APPNAME).1.gz
	cp $(APPNAME).1.gz $(DESTDIR)/usr/share/man/man1/
	cp -a --no-preserve=ownership www/* $(DESTDIR)/usr/share/$(APPNAME)/
	cd $(DESTDIR)/usr/share/$(APPNAME)/ ; ln -sf $(DEFAULT_CSS) style.css
	$(INSTALL_DATA) bluelog.crontab $(DESTDIR)$(SCRONTABS)/bluelog
	cp bluelog.debian-init /etc/init.d/bluelog
	chmod 755 /etc/init.d/bluelog
	update-rc.d bluelog defaults

# Install without Bluelog Live
nolive: bluelog
	mkdir -p $(DESTDIR)/usr/bin/
	mkdir -p $(DESTDIR)/usr/share/doc/$(APPNAME)-$(VERSION)/
	mkdir -p $(DESTDIR)/usr/share/man/man1
	mkdir -p $(DESTDIR)/usr/share/$(APPNAME)/
	cp $(APPNAME) $(DESTDIR)/usr/bin/
	cp -a $(DOCS) $(DESTDIR)/usr/share/doc/$(APPNAME)-$(VERSION)/
	gzip -c $(APPNAME).1 >> $(APPNAME).1.gz
	cp $(APPNAME).1.gz $(DESTDIR)/usr/share/man/man1/

# Build for Pwn Plug
pwnplug:
	$(CC) $(CFLAGS) -DPWNPLUG bluelog.c $(LIBS) -o $(APPNAME)
	$(CC) $(CFLAGS) -DPWNPLUG livelog.c -o $(CGIPRE)livelog.cgi
	mkdir -p $(DESTDIR)/usr/bin/
	mkdir -p $(DESTDIR)/var/www/$(APPNAME)/images
	cp $(APPNAME) $(DESTDIR)/usr/bin/
	cp --no-preserve=ownership www/*.html $(DESTDIR)/var/www/$(APPNAME)/
	cp --no-preserve=ownership -a www/cgi-bin $(DESTDIR)/var/www/
	cp --no-preserve=ownership www/pwnplug.css $(DESTDIR)/var/www/$(APPNAME)/style.css
	cp --no-preserve=ownership www/images/favicon.png $(DESTDIR)/var/www/$(APPNAME)/images/
	cp --no-preserve=ownership www/images/email.png $(DESTDIR)/var/www/$(APPNAME)/images/
	cp --no-preserve=ownership www/images/qrcontact.png $(DESTDIR)/var/www/$(APPNAME)/images/
	cp --no-preserve=ownership www/images/pwnplug_logo.png $(DESTDIR)/var/www/$(APPNAME)/images/

# Upgrade from previous source install
upgrade: removeold install

# Remove current version from system
uninstall:
	rm -rf $(DESTDIR)/usr/share/doc/$(APPNAME)-$(VERSION)/
	rm -f $(DESTDIR)/usr/share/man/man1/$(APPNAME).1.gz
	rm -rf $(DESTDIR)/usr/share/$(APPNAME)/
	rm -f $(DESTDIR)/usr/bin/$(APPNAME)

# Remove older versions
removeold:
	rm -rf $(DESTDIR)/usr/share/doc/$(APPNAME)*
	rm -f $(DESTDIR)/usr/share/man/man1/$(APPNAME).1.gz
	rm -rf $(DESTDIR)/usr/share/$(APPNAME)*
	rm -rf $(DESTDIR)/var/lib/$(APPNAME)*
	rm -f $(DESTDIR)/usr/bin/$(APPNAME)
