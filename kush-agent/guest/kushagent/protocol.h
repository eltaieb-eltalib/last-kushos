/* kushagent - portable protocol helpers (tiny JSON field reader, string builder, base64).
 * Kept free of Windows APIs so they can be unit-tested on the build host. */
#ifndef KUSHAGENT_PROTOCOL_H
#define KUSHAGENT_PROTOCOL_H

#include <stddef.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} sbuf;

void sb_init(sbuf *b);
void sb_free(sbuf *b);
void sb_append(sbuf *b, const char *s);
void sb_appendn(sbuf *b, const char *s, size_t n);
void sb_appendf_long(sbuf *b, long v);
/* Appends a JSON string literal (with quotes) for UTF-8 input; non-ASCII stays UTF-8. */
void sb_append_json_string(sbuf *b, const char *utf8);

/* Find the first "key": <integer> in the JSON text. Returns 1 on success. */
int json_get_long(const char *json, const char *key, long *out);
/* Returns a malloc'd UTF-8 string with escapes (including \uXXXX surrogate pairs) decoded,
 * or NULL if the key is missing or not a string. */
char *json_get_string(const char *json, const char *key);

char *b64_encode(const unsigned char *data, size_t len);
unsigned char *b64_decode(const char *s, size_t *out_len);

#endif
