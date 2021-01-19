#!/bin/sh
export A=`mktemp`
export B=`mktemp`.lep

head -c 4K /dev/urandom > $B

echo "attempting default no-clobber..."
./lepton "$A" "$B"
if [ $? -ne 17 ]; then
  echo "did not receive clobber exit status of 17"
  echo "FAILING TEST"
  exit 1
fi

echo "attempting clobber..."
./lepton -clobber "$A" "$B"
BLEP_SZ=$(stat --printf="%s" "$B")
if [ $BLEP_SZ -ne 0 ]; then
  echo "should've blepped to 0 by truncating $B. Instead, got $BLEP_SZ."
  echo "FAILING TEST"
  exit 1
fi

rm $A
rm $B
echo SUCCESS
