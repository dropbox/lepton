#!/bin/sh
export A=`mktemp`
export ASRC="`dirname $0`"/../images/badzerorun.jpg
cp --  "$ASRC" "$A"
export B=`mktemp`
export BSRC="`dirname $0`"/../images/roundtripfail.jpg
cp --  "$BSRC" "$B"
export C=`mktemp`
echo -n a > "$C"
export D=`mktemp`
echo -n bc > "$D"
export E=`mktemp`
export ESRC="`dirname $0`"/../images/gold-legacy.lep
cp -- "$ESRC" "$E"
export F=`mktemp`
export FSRC="`dirname $0`"/../images/nofsync.jpg
cp -- "$FSRC" "$F"
for i in "$A" "$B" "$C" "$D" "$E" "$F"; do
    ./lepton -brotliheader -permissive -validate "$i" "$i.lep" || exit 1;
done
ls -l "$A".lep || exit 1
ls -l "$B".lep || exit 1
ls -l "$C".lep || exit 1
ls -l "$D".lep || exit 1
ls -l "$E".lep || exit 1
ls -l "$F".lep || exit 1
export tmp=`mktemp`
export tmp2=`mktemp`
./lepton "$C".lep "$tmp"
./lepton "$D".lep "$tmp2"
diff "$C" "$tmp" || exit 1
diff "$D" "$tmp2" || exit 1
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
