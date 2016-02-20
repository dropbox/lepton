#!/bin/sh
#cat images/hq.jpg |head -c 8388608 |tail -c 4194304 > hqslice.jpg
cat "`dirname $0`"/../images/hq.jpg | ./lepton -startbyte=4M -trunc=8M -memory=450M - | ./lepton - | ( md5sum || md5 ) | grep -l adc74975106672ed38e3041757f18aa1 && echo PASS


