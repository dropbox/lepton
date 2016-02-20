#!/bin/sh
#if which md5sum; then
cat "`dirname $0`"/../images/gold-legacy.lep | ./lepton - | ( md5sum || md5 ) | grep -l 9ffbfc24d1157d0b1ed7a9b53bef4c23 && echo PASS
#else

#cat "`dirname $0`"/../images/gold-legacy.lep | ./lepton - | md5 | grep -l 9ffbfc24d1157d0b1ed7a9b53bef4c23 && echo PASS
#fi
