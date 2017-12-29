#!/bin/sh
export A=`mktemp`
export ASRC="`dirname $0`"/../images/iphone.jpg
cp --  "$ASRC" "$A"
export B=`mktemp`
export BSRC="`dirname $0`"/../images/trailingrst2.jpg
cp --  "$BSRC" "$B"
export C=`mktemp`
export CSRC="`dirname $0`"/../images/androidtrail.jpg
cp -- "$CSRC" "$C"
export D=`mktemp`
export DSRC="`dirname $0`"/../images/androidcrop.jpg
cp -- "$DSRC" "$D"
export E=`mktemp`
export ESRC="`dirname $0`"/../images/narrowrst.jpg
cp -- "$ESRC" "$E"
export F=`mktemp`
export FSRC="`dirname $0`"/../images/colorswap.jpg
cp -- "$FSRC" "$F"
for i in "$A" "$B" "$C" "$D" "$E" "$F"; do
    ./lepton -brotliheader -rejectprogressive "$i" "$i.lep" || exit 1;
done
export tmp=`mktemp`
export tmp2=`mktemp`
for i in "$A" "$D" "$E"; do
  for j in "$B" "$C"; do
     export ilep="$i.lep"
     export jlep="$j.lep"
       
     if stat "$ilep" > /dev/null && stat "$jlep" > /dev/null; then
       echo "starting $i $j"
       cat "$ilep" "$jlep" | ./lepton - > "$tmp"
       cat "$i" "$j" > "$tmp2"
       diff "$tmp" "$tmp2" || exit 1
       echo "done $i $j"
     fi
  done
done

for i in "$F"  "$B"; do
  for j in "$C" "$D"; do
    for k in "$E" "$B"; do
     export ilep="$i.lep"
     export jlep="$j.lep"
     export klep="$k.lep"
     if stat "$ilep" > /dev/null && stat "$jlep" > /dev/null && stat "$klep" > /dev/null ; then
       echo "starting $i $j $k"
       cat "$ilep" "$jlep" "$klep" | ./lepton - > "$tmp"
       cat "$i" "$j" "$k" > "$tmp2"
       diff "$tmp" "$tmp2" || exit 1
       echo "done $i $j $k"
     fi
    done
  done
done
rm -f -- "$tmp"
rm -f -- "$tmp2"
rm -f -- "$A"
rm -f -- "$A.lep"
rm -f -- "$B"
rm -f -- "$B.lep"
rm -f -- "$C"
rm -f -- "$C.lep"
rm -f -- "$D"
rm -f -- "$D.lep"
rm -f -- "$E"
rm -f -- "$E.lep"
rm -f -- "$F"
rm -f -- "$F.lep"
echo SUCCESS
