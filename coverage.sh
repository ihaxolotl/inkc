#!/bin/sh
# coverage.sh - Generate coverage information from the fuzzer corpus.

# Directories
BUILD_ROOT=build
PROFILE_ROOT=$BUILD_ROOT/coverage
CRASH_ROOT=${CRASH_ROOT:-"$PROFILE_ROOT/crashes"}
CORPUS_ROOT=${CORPUS_ROOT:-"$PROFILE_ROOT/corpus"}
TEST_ROOT=${TEST_ROOT:-"testing"}

make coverage 2>/dev/null
mkdir -p $CRASH_ROOT $CORPUS_ROOT
python3 ./lit.site.py --per-test-coverage $TEST_ROOT

# Test against the fuzzing corpus
LLVM_PROFILE_FILE="$PROFILE_ROOT/fuzzing-coverage.profraw" \
    $BUILD_ROOT/libink-coverage $CORPUS_ROOT/*

# Test against the runtime tests
LLVM_PROFILE_FILE="$PROFILE_ROOT/testing-coverage.profraw" \
    $BUILD_ROOT/libink-testing 2>/dev/null

find $PROFILE_ROOT -name "*.profraw" > $PROFILE_ROOT/profraw-files.txt

llvm-profdata merge -sparse \
    --input-files=$PROFILE_ROOT/profraw-files.txt \
    -o $PROFILE_ROOT/coverage.profdata

llvm-cov show $BUILD_ROOT/libink-coverage \
    -instr-profile=$PROFILE_ROOT/coverage.profdata \
    --ignore-filename-regex='(fuzzing|testing)[/\\].*' \
    --format=html \
    --output-dir=$PROFILE_ROOT/html
