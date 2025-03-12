#!/bin/sh
# coverage.sh - Generate coverage information from the fuzzer corpus.

# Directories
BUILD_ROOT=build
CRASH_ROOT=${CRASH_ROOT:-"$BUILD_ROOT/crashes"}
CORPUS_ROOT=${CORPUS_ROOT:-"$BUILD_ROOT/corpus"}

# Artifacts
profraw_file="$BUILD_ROOT/coverage.profraw"
profdata_file="$BUILD_ROOT/coverage.profdata"
exe="$BUILD_ROOT/libink-coverage"

if [ ! -d $BUILD_ROOT ]; then
    echo "Could not collect coverage information. A build has not yet been created."
    exit 1
fi

if [ ! -f $exe ]; then
    echo "Could not collect coverage information. Coverage binary has not yet been created."
    exit 1
fi

mkdir -p $CRASH_ROOT $CORPUS_ROOT
LLVM_PROFILE_FILE="$profraw_file" $exe $CORPUS_ROOT/*
llvm-profdata merge -sparse $profraw_file -o $profdata_file
llvm-cov show $exe \
    -instr-profile=$profdata_file \
    --ignore-filename-regex='(fuzzing|testing)[/\\].*' \
    --format=html \
    --output-dir=$BUILD_ROOT/html
