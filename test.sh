for i in ./images/{android,androidcrop,androidcropoptions,androidprogressive,androidtrail,grayscale,hq,iphone,iphonecity,iphonecrop,iphonecrop2,iphoneprogressive,slrcity,slrhills,slrindoor,trailingrst2}.jpg; do
  echo "START COMPRESS: $i"
  ./target/debug/lepton $i /tmp/test.lep
  echo "FINISH COMPRESS: $i"
  echo "START DECOMPRESS: $i"
  ./target/debug/lepton -d /tmp/test.lep /tmp/test.jpg
  echo "FINISH DECOMPRESS: $i"
  dif=$(diff $i /tmp/test.jpg)
  echo "DIFF: $dif"
done
rm /tmp/test.lep /tmp/test.jpg