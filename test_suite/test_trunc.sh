#!/bin/sh
export INPUT_TO_TEST=`dirname $0`/../../images/iphone.jpg
if [ $# -eq 0 ]; then
    echo "Using default file $INPUT_TO_TEST"
else
    export INPUT_TO_TEST=$1
fi
for trunc in 58000 65536 128001 256257 1000000 10241023 ; do
    export COMPRESSED_LEPTON="`mktemp /tmp/temp.XXXXXX`"
    export FTRUNC="`mktemp /tmp/temp.XXXXXX`"
    export ORIGINAL="`mktemp /tmp/temp.XXXXXX`"
    head -c $trunc "$INPUT_TO_TEST" > "$FTRUNC"
    ./lepton -trunc=$trunc - < "$INPUT_TO_TEST" > "$COMPRESSED_LEPTON"
    ./lepton -recode - < "$COMPRESSED_LEPTON" > "$ORIGINAL"
    md5sum "$ORIGINAL" "$FTRUNC" 2> /dev/null || md5 "$ORIGINAL" "$FTRUNC"
    if diff -q "$ORIGINAL" "$FTRUNC" ; then
        rm -- "$FTRUNC"
        rm -- "$COMPRESSED_LEPTON"
        rm -- "$ORIGINAL"
        unset FTRUNC
        unset COMPRESSED_LEPTON
        unset ORIGINAL
    else
        echo truncated file "$FTRUNC"
        echo compressed_lepton "$COMPRESSED_LEPTON"
        echo roundtrip "$ORIGINAL"
        unset FTRUNC
        unset COMPRESSED_LEPTON
        unset ORIGINAL
        exit 1
    fi
done

