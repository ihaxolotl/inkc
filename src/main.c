#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lex.h"

enum ink_status {
    INK_E_OK,
    INK_E_OOM,
    INK_E_OS,
    INK_E_FILE,
};

static const char *INK_FILE_EXT = ".ink";
static const size_t INK_FILE_EXT_LENGTH = 4;

int unix_load_file(const char *filename, unsigned char **bytes, size_t *length)
{
    int fd;
    ssize_t nread;
    off_t buflen;
    unsigned char *buf;

    fd = open(filename, O_RDONLY);
    if (fd == -1)
        return -1;

    buflen = lseek(fd, 0, SEEK_END);
    if (buflen == -1)
        goto err_file;
    if (lseek(fd, 0, SEEK_SET) == -1)
        goto err_file;

    buf = malloc(buflen + 1);
    if (buf == NULL)
        goto err_file;

    nread = read(fd, buf, buflen);
    if (nread != buflen)
        goto err_memory;

    buf[buflen] = '\0';
    *bytes = buf;
    *length = buflen;

    close(fd);
    return 0;
err_memory:
    free(buf);
err_file:
    close(fd);
    return -1;
}

char *ink_string_copy(const char *chars, size_t length)
{
    char *buf;

    buf = malloc(length + 1);
    if (buf == NULL)
        return NULL;

    memcpy(buf, chars, length);
    buf[length] = '\0';
    return buf;
}

/**
 * Load an Ink source file.
 *
 * `source` will only be initialized upon success.
 */
int ink_source_load(const char *filename, struct ink_source *source)
{
    int rc;
    const char *ext;
    const size_t namelen = strlen(filename);

    if (namelen < INK_FILE_EXT_LENGTH)
        return -INK_E_FILE;

    ext = filename + namelen - INK_FILE_EXT_LENGTH;

    if (strncmp(ext, INK_FILE_EXT, INK_FILE_EXT_LENGTH) != 0)
        return -INK_E_FILE;

    source->filename = ink_string_copy(filename, namelen);
    if (source->filename == NULL)
        return -INK_E_OOM;

    rc = unix_load_file(filename, &source->bytes, &source->length);
    if (rc == -1) {
        free(source->filename);
        return -INK_E_OS;
    }
    return 0;
}

void ink_source_free(struct ink_source *source)
{
    free(source->bytes);
    free(source->filename);

    source->filename = NULL;
    source->bytes = NULL;
    source->length = 0;
}

void ink_token_print(const struct ink_token *token)
{
    printf("[DEBUG] %s(%u, %u)\n", ink_token_type_strz(token->type),
           token->start_offset, token->end_offset);
}

int main(int argc, char *argv[])
{
    int rc;
    const char *filename;
    struct ink_source source;
    struct ink_token token;
    struct ink_lexer lexer = {
        .source = &source,
        .start_offset = 0,
        .cursor_offset = 0,
    };

    if (argc < 2) {
        fprintf(stderr, "Not enough arguments.\n");
        return EXIT_FAILURE;
    }

    filename = argv[1];

    rc = ink_source_load(filename, &source);
    if (rc < 0) {
        switch (-rc) {
        case INK_E_OS:
            fprintf(stderr, "[ERROR] OS Error.\n");
            break;
        case INK_E_FILE:
            fprintf(stderr, "[ERROR] %s is not an ink script.\n", filename);
            break;
        default:
            fprintf(stderr, "[ERROR] Unknown error.\n");
            break;
        }
        return EXIT_FAILURE;
    }

    printf("Source file %s is %zu bytes\n", source.filename, source.length);

    do {
        ink_token_next(&lexer, &token);
        ink_token_print(&token);
    } while (token.type != INK_TT_EOF);

    ink_source_free(&source);
    return EXIT_SUCCESS;
}
