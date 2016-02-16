#!/bin/sh
export INPUT_TO_TEST=`dirname $0`/../../images/androidprogressive.jpg
if [ $# -eq 0 ]; then
    echo "Using default file $INPUT_TO_TEST"
else
    export INPUT_TO_TEST=$1
fi
export LEPTON_COMPRESSION_MODEL_OUT="`mktemp /tmp/temp.XXXXXX`"
export TEST_MODEL="`mktemp /tmp/temp.XXXXXX`"
export COMPRESSED_LEPTON="`mktemp /tmp/temp.XXXXXX`"
export ORIGINAL="`mktemp /tmp/temp.XXXXXX`"
if [ $# -lt 2 ]; then
    ./lepton -allowprogressive - < "$INPUT_TO_TEST" > "$COMPRESSED_LEPTON"
    cp "$LEPTON_COMPRESSION_MODEL_OUT" "$TEST_MODEL"
else
    for test_item in "$@"; do
        if [ "$test_item" != "$INPUT_TO_TEST" ]; then
            ./lepton -allowprogressive - < "$test_item" > "$COMPRESSED_LEPTON"
            cp "$LEPTON_COMPRESSION_MODEL_OUT" "$TEST_MODEL"
            export LEPTON_COMPRESSION_MODEL="$TEST_MODEL"
        else
            echo "Ignoring $test_item when training model"
        fi
    done
fi
LEPTON_COMPRESSION_MODEL="$TEST_MODEL" ./lepton -decode -allowprogressive - < "$INPUT_TO_TEST" > "$COMPRESSED_LEPTON"
LEPTON_COMPRESSION_MODEL="$TEST_MODEL" ./lepton -recode -allowprogressive - < "$COMPRESSED_LEPTON" > "$ORIGINAL"
md5sum "$ORIGINAL" "$INPUT_TO_TEST" 2> /dev/null || md5 "$ORIGINAL" "$INPUT_TO_TEST"
if diff -q "$ORIGINAL" "$INPUT_TO_TEST" ; then
    rm -- "$LEPTON_COMPRESSION_MODEL_OUT"
    rm -- "$TEST_MODEL"
    rm -- "$COMPRESSED_LEPTON"
    rm -- "$ORIGINAL"
    unset LEPTON_COMPRESSION_MODEL_OUT
    unset TEST_MODEL
    unset COMPRESSED_LEPTON
    unset ORIGINAL
    exit 0
fi
echo compression_model "$LEPTON_COMPRESSION_MODEL_OUT"
echo test_model "$TEST_MODEL"
echo compressed_lepton "$COMPRESSED_LEPTON"
echo roundtrip "$ORIGINAL"
unset LEPTON_COMPRESSION_MODEL_OUT
unset TEST_MODEL
unset COMPRESSED_LEPTON
unset ORIGINAL
exit 1
