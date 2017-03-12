#!/bin/sh
if cat "`dirname $0`"/../images/roundtripfail.jpg | ./lepton -verify - > /dev/null ; then
    false # if we fail even though roundtrip is on...then the test does not pass
else
    # make sure this example actually makes it through the encode (but not the decode)
    cat "`dirname $0`"/../images/roundtripfail.jpg | ./lepton -skipverify - > /dev/null
fi
