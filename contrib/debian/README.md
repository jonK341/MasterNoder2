
Debian
====================
This directory contains files used to package masternoder2d/masternoder2-qt
for Debian-based Linux systems. If you compile masternoder2d/masternoder2-qt yourself, there are some useful files here.

## masternoder2: URI support ##


masternoder2-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install masternoder2-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your masternoder2-qt binary to `/usr/bin`
and the `../../share/pixmaps/masternoder2128.png` to `/usr/share/pixmaps`

masternoder2-qt.protocol (KDE)

