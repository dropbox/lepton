#!/bin/sh
cat "`dirname $0`"/../images/iphone16.lep | ./lepton -singlethread - | ( md5sum || md5 ) | grep -l 8ea9fcf1b2c24877aa838dd6ac1df413 && cat "`dirname $0`"/../images/iphone16.lep | ./lepton - | ( md5sum || md5 ) | grep -l 8ea9fcf1b2c24877aa838dd6ac1df413 && echo PASS
