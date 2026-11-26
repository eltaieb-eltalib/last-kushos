/* kushagent - portable protocol helpers. See protocol.h. */
#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sb_init(sbuf *b) { b->data = NULL; b->len = 0; b->cap = 0; }

void sb_free(sbuf *b) { free(b->data); sb_init(b); }

void sb_appendn(sbuf *b, const char *s, size_t n)
{
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 256;
        char *p;
        while (cap < b->len + n + 1) cap *= 2;
        p = (char *)realloc(b->data, cap);
        if (!p) return;
        b->data = p;
        b->cap = cap;
    }
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = 0;
}

void sb_append(sbuf *b, const char *s) { sb_appendn(b, s, strlen(s)); }

void sb_appendf_long(sbuf *b, long v)
{
    char tmp[32];
    sprintf(tmp, "%ld", v);
    sb_append(b, tmp);
}

void sb_append_json_string(sbuf *b, const char *s)
{
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    char esc[8];
    sb_append(b, "\"");
    for (; *p; p++) {
        switch (*p) {
        case '"': sb_append(b, "\\\""); break;
        case '\\': sb_append(b, "\\\\"); break;
        case '\n': sb_append(b, "\\n"); break;
        case '\r': sb_append(b, "\\r"); break;
        case '\t': sb_append(b, "\\t"); break;
        default:
            if (*p < 0x20) {
                sprintf(esc, "\\u%04x", *p);
                sb_append(b, esc);
            } else {
                sb_appendn(b, (const char *)p, 1);
            }
        }
    }
    sb_append(b, "\"");
}

static const char *find_value(const char *json, const char *key)
{
    size_t klen = strlen(key);
    const char *p = json;
    while ((p = strchr(p, '"')) != NULL) {
        if (strncmp(p + 1, key, klen) == 0 && p[1 + klen] == '"') {
            const char *q = p + 2 + klen;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == ':') {
                q++;
                while (*q == ' ' || *q == '\t') q++;
                return q;
            }
        }
        /* skip over this string token */
        p++;
        while (*p && *p != '"') {
            if (*p == '\\' && p[1]) p++;
            p++;
        }
        if (*p) p++;
    }
    return NULL;
}

int json_get_long(const char *json, const char *key, long *out)
{
    const char *v = find_value(json, key);
    char *end;
    if (!v) return 0;
    *out = strtol(v, &end, 10);
    return end != v;
}

static void put_utf8(sbuf *b, unsigned long cp)
{
    char u[4];
    if (cp < 0x80) { u[0] = (char)cp; sb_appendn(b, u, 1); }
    else if (cp < 0x800) { u[0] = (char)(0xC0 | (cp >> 6)); u[1] = (char)(0x80 | (cp & 0x3F)); sb_appendn(b, u, 2); }
    else if (cp < 0x10000) {
        u[0] = (char)(0xE0 | (cp >> 12)); u[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); u[2] = (char)(0x80 | (cp & 0x3F));
        sb_appendn(b, u, 3);
    } else {
        u[0] = (char)(0xF0 | (cp >> 18)); u[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        u[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); u[3] = (char)(0x80 | (cp & 0x3F));
        sb_appendn(b, u, 4);
    }
}

static int hex4(const char *p, unsigned long *out)
{
    int i;
    unsigned long v = 0;
    for (i = 0; i < 4; i++) {
        char c = p[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned long)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned long)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned long)(c - 'A' + 10);
        else return 0;
    }
    *out = v;
    return 1;
}

char *json_get_string(const char *json, const char *key)
{
    const char *p = find_value(json, key);
    sbuf b;
    if (!p || *p != '"') return NULL;
    sb_init(&b);
    sb_append(&b, "");
    for (p++; *p && *p != '"'; p++) {
        if (*p != '\\') { sb_appendn(&b, p, 1); continue; }
        p++;
        switch (*p) {
        case 'n': sb_append(&b, "\n"); break;
        case 'r': sb_append(&b, "\r"); break;
        case 't': sb_append(&b, "\t"); break;
        case 'b': sb_append(&b, "\b"); break;
        case 'f': sb_append(&b, "\f"); break;
        case 'u': {
            unsigned long cp, lo;
            if (!hex4(p + 1, &cp)) { sb_free(&b); return NULL; }
            p += 4;
            if (cp >= 0xD800 && cp <= 0xDBFF && p[1] == '\\' && p[2] == 'u' && hex4(p + 3, &lo)
                && lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                p += 6;
            }
            put_utf8(&b, cp);
            break;
        }
        case 0: sb_free(&b); return NULL;
        default: sb_appendn(&b, p, 1); /* \" \\ \/ */
        }
    }
    if (*p != '"') { sb_free(&b); return NULL; }
    return b.data;
}

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char *b64_encode(const unsigned char *d, size_t n)
{
    size_t i, o = 0;
    char *out = (char *)malloc(4 * ((n + 2) / 3) + 1);
    if (!out) return NULL;
    for (i = 0; i + 2 < n; i += 3) {
        out[o++] = B64[d[i] >> 2];
        out[o++] = B64[((d[i] & 3) << 4) | (d[i + 1] >> 4)];
        out[o++] = B64[((d[i + 1] & 15) << 2) | (d[i + 2] >> 6)];
        out[o++] = B64[d[i + 2] & 63];
    }
    if (i < n) {
        out[o++] = B64[d[i] >> 2];
        if (i + 1 < n) {
            out[o++] = B64[((d[i] & 3) << 4) | (d[i + 1] >> 4)];
            out[o++] = B64[(d[i + 1] & 15) << 2];
        } else {
            out[o++] = B64[(d[i] & 3) << 4];
            out[o++] = '=';
        }
        out[o++] = '=';
    }
    out[o] = 0;
    return out;
}

static int b64v(char c)
{
    const char *p = strchr(B64, c);
    return (c && p) ? (int)(p - B64) : -1;
}

unsigned char *b64_decode(const char *s, size_t *out_len)
{
    size_t n = strlen(s), o = 0;
    unsigned char *out = (unsigned char *)malloc(n / 4 * 3 + 3);
    int buf = 0, bits = 0;
    if (!out) return NULL;
    for (; *s; s++) {
        int v;
        if (*s == '=') break;
        v = b64v(*s);
        if (v < 0) continue; /* skip whitespace */
        buf = (buf << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out[o++] = (unsigned char)((buf >> bits) & 0xFF);
        }
    }
    *out_len = o;
    return out;
}
