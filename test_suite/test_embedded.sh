#!/bin/sh
export INPUT_TO_TEST=`dirname $0`/../images/iphone.jpg
if [ $# -eq 0 ]; then
    echo "Using default file $INPUT_TO_TEST"
else
    export INPUT_TO_TEST=$1
fi
export trailer=100003
for embedding in 100001 ; do
    export COMPRESSED_LEPTON="`mktemp /tmp/temp.XXXXXX`"
    export FEMB="`mktemp /tmp/temp.XXXXXX`"
    export ORIGINAL="`mktemp /tmp/temp.XXXXXX`"
    head -c $embedding /dev/urandom > "$FEMB"
    cat "$INPUT_TO_TEST"  >> "$FEMB"
    head -c $trailer /dev/urandom >> "$FEMB"
    ./lepton -brotliheader -embedding=$embedding - < "$FEMB" > "$COMPRESSED_LEPTON"
    ./lepton -recode - < "$COMPRESSED_LEPTON" > "$ORIGINAL"
    md5sum "$ORIGINAL" "$FEMB" 2> /dev/null || md5 "$ORIGINAL" "$FEMB"
    if diff -q "$ORIGINAL" "$FEMB" ; then
        rm -- "$FEMB"
        rm -- "$COMPRESSED_LEPTON"
        rm -- "$ORIGINAL"
        unset FEMB
        unset COMPRESSED_LEPTON
        unset ORIGINAL
    else
        echo truncated file "$FEMB"
        echo compressed_lepton "$COMPRESSED_LEPTON"
        echo roundtrip "$ORIGINAL"
        unset FEMB
        unset COMPRESSED_LEPTON
        unset ORIGINAL
        exit 1
    fi
done

