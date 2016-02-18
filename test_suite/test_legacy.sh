#!/bin/sh
cat "`dirname $0`"/../images/gold-legacy.lep | ./lepton - | sha256sum | grep -l 79f13e3951e84b4e7bd8ff0f1315f336044f542387ac3a9c26205c5ac372ea54 && echo PASS
