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
    export DOUBLE_TROUBLE="`mktemp /tmp/dt.XXXXXX`"
    export DOUBLE_ORIGINAL_Z="`mktemp /tmp/doz.XXXXXX`"
    export DOUBLE_TROUBLE_RT_MD5="`mktemp /tmp/dt.XXXXXX`"
    export DOUBLE_ORIGINAL="`mktemp /tmp/do.XXXXXX`"
    export DOUBLE_FEMB="`mktemp /tmp/dfemb.XXXXXX`"
    head -c $embedding /dev/urandom > "$FEMB"
    cat "$INPUT_TO_TEST"  >> "$FEMB"
    head -c $trailer /dev/urandom >> "$FEMB"
    ./lepton -brotliheader -embedding=$embedding - < "$FEMB" > "$COMPRESSED_LEPTON"
    ./lepton -recode - < "$COMPRESSED_LEPTON" > "$ORIGINAL"
    cat "$COMPRESSED_LEPTON" "$COMPRESSED_LEPTON" > "$DOUBLE_TROUBLE"
    ./lepton -recode - -zlib0 < "$DOUBLE_TROUBLE" > "$DOUBLE_ORIGINAL_Z"
    if zlib-flate -compress < /dev/null | zlib-flate -uncompress ; then
        zlib-flate -uncompress < "$DOUBLE_ORIGINAL_Z" > "$DOUBLE_ORIGINAL"
        cat "$FEMB" "$FEMB" > "$DOUBLE_FEMB"
        if diff -q "$DOUBLE_FEMB" "$DOUBLE_ORIGINAL"; then
            echo "SUCCESS WITH zlib0 and concat"
        else
            echo "FAILED WITH $DOUBLE_FEMB vs$DOUBLE_ORIGINAL"
            exit 1
        fi
    fi
    if diff -q "$ORIGINAL" "$FEMB" ; then
        rm -- "$FEMB"
        rm -- "$COMPRESSED_LEPTON"
        rm -- "$ORIGINAL"
        rm -- "$DOUBLE_TROUBLE"
        rm -- "$DOUBLE_ORIGINAL"
        rm -- "$DOUBLE_ORIGINAL_Z"
        rm -- "$DOUBLE_FEMB"
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

