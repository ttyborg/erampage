#!/bin/bash

echo =====
echo please connect to internet and press Enter to upgrade cdevtools
echo =====
read
echo src/gz python http://feeds.angstrom-distribution.org/feeds/unstable/ipk/glibc/armv7a/base > $CDEVROOT/etc/opkg/angstrom-feed.conf
opkg -o $CDEVROOT update
opkg -o $CDEVROOT install gdk-pixbuf-loader-png
opkg -o $CDEVROOT install gdk-pixbuf-loader-bmp
echo =====
echo now you should be able to build eRampage by simply typing make
echo =====
