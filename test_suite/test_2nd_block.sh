#!/bin/sh
#cat images/hq.jpg |head -c 8388608 |tail -c 4194304 > hqslice.jpg

cat "`dirname $0`"/../images/hq.jpg | ./lepton -startbyte=4M -trunc=8M -memory=450M - | ./lepton - | sha256sum | grep -l dd1f949f3e7687d48a195332234c3b146f2f4f25c631b7d825ee791331190be8 && echo PASS