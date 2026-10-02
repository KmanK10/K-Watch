#include "text.h"

#include <stdint.h>
#include <string.h>

static uint32_t decode_utf8(const unsigned char **p)
{
    const unsigned char *s = *p;
    uint32_t cp;
    int extra;
    if (s[0] < 0x80) {
        cp = s[0];
        extra = 0;
    } else if ((s[0] & 0xE0) == 0xC0) {
        cp = s[0] & 0x1F;
        extra = 1;
    } else if ((s[0] & 0xF0) == 0xE0) {
        cp = s[0] & 0x0F;
        extra = 2;
    } else if ((s[0] & 0xF8) == 0xF0) {
        cp = s[0] & 0x07;
        extra = 3;
    } else {
        *p = s + 1;
        return 0xFFFD;
    }
    s++;
    for (int i = 0; i < extra; i++) {
        if ((*s & 0xC0) != 0x80) {
            *p = s;
            return 0xFFFD;
        }
        cp = (cp << 6) | (*s++ & 0x3F);
    }
    *p = s;
    return cp;
}

static const char *ascii_for(uint32_t cp)
{
    switch (cp) {
    case 0x00A0: return " ";
    case 0x2018: case 0x2019: case 0x201B: case 0x2032: return "'";
    case 0x201C: case 0x201D: case 0x2033: return "\"";
    case 0x2010: case 0x2011: case 0x2012: case 0x2013: case 0x2014: return "-";
    case 0x2026: return "...";
    case 0x2022: return "*";
    default: return NULL;
    }
}

void text_to_ascii(char *dst, size_t dst_size, const char *src)
{
    size_t out = 0;
    const unsigned char *p = (const unsigned char *)src;
    while (*p && out + 1 < dst_size) {
        uint32_t cp = decode_utf8(&p);
        if (cp >= 0x20 && cp < 0x7F) {
            dst[out++] = (char)cp;
        } else if (cp == '\n') {
            dst[out++] = '\n';
        } else {
            const char *rep = ascii_for(cp);
            if (rep && out + strlen(rep) + 1 < dst_size) {
                memcpy(&dst[out], rep, strlen(rep));
                out += strlen(rep);
            }
        }
    }
    dst[out] = '\0';
}
