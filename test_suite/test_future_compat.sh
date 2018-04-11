#!/bin/sh
cat "`dirname $0`"/../images/narrowrst.lep | ./lepton - | ( md5sum || md5 ) | grep -l 07e9021d35114bd69f44f5bc1c3788e3 && echo PASS
