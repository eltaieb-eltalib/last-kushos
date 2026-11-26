#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "protocol.h"

int main(void)
{
    /* What Python's json.dumps(ensure_ascii=True) sends for Arabic text and an emoji */
    const char *req = "{\"id\": 42, \"method\": \"type_unicode\", \"params\": {\"text\": \"\\u0645\\u0631\\u062d\\u0628\\u0627 \\\"x\\\"\\n\\ud83d\\ude00\", \"timeout_ms\": 1500}}";
    long id = 0, t = 0;
    char *m, *text, *enc;
    unsigned char *dec;
    size_t n;
    sbuf b;

    assert(json_get_long(req, "id", &id) && id == 42);
    assert(json_get_long(req, "timeout_ms", &t) && t == 1500);
    m = json_get_string(req, "method");
    assert(m && !strcmp(m, "type_unicode"));
    text = json_get_string(req, "text");
    assert(text && !strcmp(text, "\xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7 \"x\"\n\xf0\x9f\x98\x80"));
    assert(json_get_string(req, "missing") == NULL);
    /* key text inside a string value must not be matched */
    assert(json_get_string("{\"a\": \"\\\"id\\\": 5\", \"id\": 7}", "zz") == NULL);
    assert(json_get_long("{\"a\": \"\\\"id\\\": 5\", \"id\": 7}", "id", &id) && id == 7);

    sb_init(&b);
    sb_append_json_string(&b, "a\"b\\c\n\x01");
    assert(!strcmp(b.data, "\"a\\\"b\\\\c\\n\\u0001\""));
    sb_free(&b);

    enc = b64_encode((const unsigned char *)"hello!!", 7);
    assert(!strcmp(enc, "aGVsbG8hIQ=="));
    dec = b64_decode(enc, &n);
    assert(n == 7 && !memcmp(dec, "hello!!", 7));
    free(m); free(text); free(enc); free(dec);
    printf("protocol tests passed\n");
    return 0;
}
