for i in ./images/*.jpg; do
  # echo $i
  echo "START: $i"
  ./target/debug/lepton $i /tmp/test.lep
  echo "FINISH: $i"
done
rm /tmp/test.lep
