#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "base64.h"

void test(const char *string) {
    char *encoded, *decoded;
    int enc_len, dec_len;
    encoded = base64_encode((unsigned char*)string, strlen(string), &enc_len); /* don't care of memory leaks, it's just a test */
    decoded = (char*)base64_decode(encoded, enc_len, &dec_len);
    if (dec_len != strlen(string)) {
        printf("length mismatch\n");
    } else if (memcmp(string, decoded, dec_len)) {
        printf("data mismatch\n");
    } else {
        printf("test ok for %s\n", string);
    }
}

int main(int argc, char *argv[]) {
    test("0123456789");
    test("abcdefghijk");
    test("lasndlkjedli2j3lknwq.d");
    return 0;
}
