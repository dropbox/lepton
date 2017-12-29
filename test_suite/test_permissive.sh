#!/bin/sh
for i in $*; do
    ./lepton -brotliheader -permissive "$i" "${i%.jpg}.lep";
done
export tmp=`mktemp`
export tmp2=`mktemp`
for i in $*; do
  for j in $*; do
     export ilep="${i%.jpg}.lep"
     export jlep="${j%.jpg}.lep"
       
     if stat "$ilep" > /dev/null && stat "$jlep" > /dev/null; then
       echo "starting $i $j"
       cat "$ilep" "$jlep" | ./lepton - > "$tmp"
       cat "$i" "$j" > "$tmp2"
       diff "$tmp" "$tmp2" || exit 1
       echo "done $i $j"
     fi
  done
done

for i in $*; do
  for j in $*; do
    for k in $*; do
     export ilep="${i%.jpg}.lep"
     export jlep="${j%.jpg}.lep"
     export klep="${k%.jpg}.lep"
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
