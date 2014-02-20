# App info
APPNAME = bluelog
VERSION = 1.1.3-dev

# Determines which CSS theme to use as default
# Options: bluelog.css digifail.css, backtrack.css, pwnplug.css, openwrt.css, lcars.css
DEFAULT_CSS = bluelog.css

# Debug Targets
# Options: -DOPENWRT, -DPWNPLUG, -DPWNPAD
#TARGET = -DPWNPAD

# Compiler and options
CC = gcc
CFLAGS += -Wall -O2 $(TARGET)

# Libraries to link
LIBS = -lbluetooth -lm

# Files
DOCS = ChangeLog COPYING README README.LIVE

# Livelog.cgi prefix
CGIPRE = www/cgi-bin/

# OUI script
OUISCRIPT = scripts/gen_oui.sh

# Targets
# Build Bluelog
bluelog: bluelog.c
	$(CC) $(CFLAGS) bluelog.c $(LIBS) -o $(APPNAME)

# Build CGI module
livelog: livelog.c
	$(CC) $(CFLAGS) livelog.c -o $(CGIPRE)livelog.cgi

# Download OUI file
ouifile:
	$(OUISCRIPT) check

# Build tarball
release: clean
	tar --exclude='.*' -C ../ -czvf /tmp/$(APPNAME)-$(VERSION).tar.gz $(APPNAME)-$(VERSION)

# Clean for dist
clean:
	rm -rf $(APPNAME) $(CGIPRE)livelog.cgi *.o *.txt *.log *.gz *.cgi

# Install to system
install: bluelog livelog ouifile
	mkdir -p $(DESTDIR)/usr/bin/
	mkdir -p $(DESTDIR)/etc/$(APPNAME)/	
	mkdir -p $(DESTDIR)/usr/share/doc/$(APPNAME)-$(VERSION)/
	mkdir -p $(DESTDIR)/usr/share/man/man1
	mkdir -p $(DESTDIR)/usr/share/$(APPNAME)/
	cp $(APPNAME) $(DESTDIR)/usr/bin/
	cp bluelog.conf $(DESTDIR)/etc/$(APPNAME)/
	cp -a $(DOCS) $(DESTDIR)/usr/share/doc/$(APPNAME)-$(VERSION)/
	gzip -c $(APPNAME).1 >> $(APPNAME).1.gz
	cp $(APPNAME).1.gz $(DESTDIR)/usr/share/man/man1/
	cp oui.txt $(DESTDIR)/etc/$(APPNAME)/
	cp -a --no-preserve=ownership www/* $(DESTDIR)/usr/share/$(APPNAME)/
	cd $(DESTDIR)/usr/share/$(APPNAME)/ ; ln -sf $(DEFAULT_CSS) style.css

# Install without Bluelog Live or OUI
standalone:
	$(CC) $(CFLAGS) -DNOLIVE -DNOOUI bluelog.c $(LIBS) -o $(APPNAME)
	mkdir -p $(DESTDIR)/usr/bin/
	mkdir -p $(DESTDIR)/usr/share/doc/$(APPNAME)-$(VERSION)/
	mkdir -p $(DESTDIR)/usr/share/man/man1
	mkdir -p $(DESTDIR)/usr/share/$(APPNAME)/
	cp $(APPNAME) $(DESTDIR)/usr/bin/
	cp -a $(DOCS) $(DESTDIR)/usr/share/doc/$(APPNAME)-$(VERSION)/
	gzip -c $(APPNAME).1 >> $(APPNAME).1.gz
	cp $(APPNAME).1.gz $(DESTDIR)/usr/share/man/man1/

# Build for Pwn Plug
pwnplug: removeold
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

# Build for Pwn Pad
pwnpad: removeold ouifile
	$(CC) $(CFLAGS) -DPWNPAD bluelog.c $(LIBS) -o $(APPNAME)
	mkdir -p $(DESTDIR)/usr/bin/
	mkdir -p $(DESTDIR)/usr/share/$(APPNAME)/
	cp oui.txt $(DESTDIR)/usr/share/$(APPNAME)/
	cp $(APPNAME) $(DESTDIR)/usr/bin/

# Upgrade from previous source install
upgrade: removeold install

# Remove current version from system
uninstall:
	rm -rf $(DESTDIR)/usr/share/doc/$(APPNAME)-$(VERSION)/
	rm -f $(DESTDIR)/usr/share/man/man1/$(APPNAME).1.gz
	rm -rf $(DESTDIR)/usr/share/$(APPNAME)/
	rm -rf $(DESTDIR)/etc/$(APPNAME)/
	rm -f $(DESTDIR)/usr/bin/$(APPNAME)

# Remove older versions
removeold:
	rm -rf $(DESTDIR)/usr/share/doc/$(APPNAME)*
	rm -f $(DESTDIR)/usr/share/man/man1/$(APPNAME).1.gz
	rm -rf $(DESTDIR)/usr/share/$(APPNAME)*
	rm -rf $(DESTDIR)/var/lib/$(APPNAME)*
	rm -f $(DESTDIR)/usr/bin/$(APPNAME)
	rm -rf $(DESTDIR)/etc/$(APPNAME)
