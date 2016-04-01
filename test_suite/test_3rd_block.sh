#cat images/hq.jpg |head -c 12582912 |tail -c 4194304 > hqslice3.jpg
#cat images/hq.jpg |head -c 16777216 |tail -c 4194304 > hqslice4.jpg
#cat images/hq.jpg |head -c 20971520 | tail -c 4194304 > hqslice5.jpg
#cat images/hq.jpg |tail -c 2544824 > hqslice6.jpg
#53f01aa2a3be79c11c3b5799d93b144f  hqslice3.jpg
#7c8a60b012398615d049073bcaf537cb  hqslice4.jpg
#061931a36f6827f96747d1e6db622544  hqslice5.jpg
#3e8a3e1b50ac5fc29acdc4bc70664ac6  hqslice6.jpg
#50065babb38f15f7b1e933939576d4a5  hqslice.jpg

cat "`dirname $0`"/../images/hq.jpg | ./lepton -startbyte=8M -trunc=12M -memory=450M - | ./lepton - | ( md5sum || md5 ) | grep -l 03161bbb7e98554882b27617cc973f6b  && cat "`dirname $0`"/../images/hq.jpg | ./lepton -startbyte=12M -trunc=16M -memory=450M - | ./lepton - | ( md5sum || md5 ) | grep -l 78c216019e2422f1e3aa6b68a20f5bf7 && cat "`dirname $0`"/../images/hq.jpg | ./lepton -startbyte=16M -trunc=20M -memory=450M - | ./lepton - | ( md5sum || md5 ) | grep -l df4ab8bbe8e387038c6ba30902c2a140 && echo PASS
