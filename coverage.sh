#!/bin/sh
# coverage.sh - Generate coverage information from the fuzzer corpus.

# Directories
BUILD_ROOT=build
PROFILE_ROOT=$BUILD_ROOT/coverage
CRASH_ROOT=${CRASH_ROOT:-"$PROFILE_ROOT/crashes"}
CORPUS_ROOT=${CORPUS_ROOT:-"$PROFILE_ROOT/corpus"}
TEST_ROOT=${TEST_ROOT:-"testing"}
exe="$BUILD_ROOT/libink-coverage"

if [ ! -d $PROFILE_ROOT ]; then
    echo "Could not collect coverage information. Coverage build has not yet been created."
    exit 1
fi

if [ ! -f $exe ]; then
    echo "Could not collect coverage information. Coverage binary has not yet been created."
    exit 1
fi

mkdir -p $CRASH_ROOT $CORPUS_ROOT
LLVM_PROFILE_FILE="$PROFILE_ROOT/fuzzer-coverage.profraw" $exe $CORPUS_ROOT/*
python3 ./lit.site.py --per-test-coverage $TEST_ROOT
find $PROFILE_ROOT -name "*.profraw" > $PROFILE_ROOT/profraw-files.txt

llvm-profdata merge -sparse \
    --input-files=$PROFILE_ROOT/profraw-files.txt \
    -o $PROFILE_ROOT/coverage.profdata

llvm-cov show $exe \
    -instr-profile=$PROFILE_ROOT/coverage.profdata \
    --ignore-filename-regex='(fuzzing|testing)[/\\].*' \
    --format=html \
    --output-dir=$PROFILE_ROOT/html
