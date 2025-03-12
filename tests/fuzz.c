/*===- fuzz_target.c - standalone main() for fuzz targets. ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

extern int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size);
__attribute__((weak)) extern int LLVMFuzzerInitialize(int *argc, char ***argv);

int main(int argc, char **argv)
{
    fprintf(stderr, "StandaloneFuzzTargetMain: running %d inputs\n", argc - 1);

    if (LLVMFuzzerInitialize) {
        LLVMFuzzerInitialize(&argc, &argv);
    }
    for (int i = 1; i < argc; i++) {
        fprintf(stderr, "Running: %s\n", argv[i]);
        FILE *f = fopen(argv[i], "r");

        assert(f);

        fseek(f, 0, SEEK_END);

        size_t len = (size_t)ftell(f);

        fseek(f, 0, SEEK_SET);

        unsigned char *buf = (unsigned char *)malloc(len);
        size_t n_read = (size_t)fread(buf, 1, len, f);
        fclose(f);

        assert(n_read == len);
        LLVMFuzzerTestOneInput(buf, len);

        free(buf);
        fprintf(stderr, "Done:    %s: (%zd bytes)\n", argv[i], n_read);
    }
}
