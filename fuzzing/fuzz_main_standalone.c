/*===- fuzz_main_standalone.c - standalone main() for fuzz targets. ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===*/
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
__attribute__((weak)) extern int LLVMFuzzerInitialize(int *argc, char ***argv);

int main(int argc, char **argv)
{
    size_t len = 0;
    size_t nr = 0;
    uint8_t *b = NULL;
    FILE *fp = NULL;

    fprintf(stderr, "StandaloneFuzzTargetMain: running %d inputs\n", argc - 1);

    if (LLVMFuzzerInitialize) {
        LLVMFuzzerInitialize(&argc, &argv);
    }
    for (int i = 1; i < argc; i++) {
        fprintf(stderr, "Running: %s\n", argv[i]);
        fp = fopen(argv[i], "r");
        assert(fp);

        fseek(fp, 0, SEEK_END);
        len = (size_t)ftell(fp);
        fseek(fp, 0, SEEK_SET);

        b = (uint8_t *)malloc(len + 1);
        nr = (size_t)fread(b, 1, len, fp);
        fclose(fp);

        assert(nr == len);
        b[nr] = '\0';

        LLVMFuzzerTestOneInput(b, len);
        free(b);
        fprintf(stderr, "Done:    %s: (%zd bytes)\n", argv[i], nr);
    }
}
