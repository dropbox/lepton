cat "`dirname $0`"/../images/hq.jpg | ./lepton -startbyte=20M -memory=450M - | ./lepton - | ( md5sum || md5 ) | grep -l 3b2cab94105d572dfd465692fd1094b6 && echo PASS
