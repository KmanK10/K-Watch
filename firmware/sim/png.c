// Minimal PNG writer (uncompressed), so screenshots open anywhere.

#include "png.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t s_crc_table[256];

static void crc_init(void)
{
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) {
            c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        }
        s_crc_table[n] = c;
    }
}

static uint32_t crc(uint32_t c, const uint8_t *buf, size_t len)
{
    c = ~c;
    for (size_t i = 0; i < len; i++) {
        c = s_crc_table[(c ^ buf[i]) & 0xff] ^ (c >> 8);
    }
    return ~c;
}

static void put_be32(uint8_t *p, uint32_t v)
{
    p[0] = v >> 24;
    p[1] = v >> 16;
    p[2] = v >> 8;
    p[3] = v;
}

static void write_chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len)
{
    uint8_t head[8];
    put_be32(head, len);
    memcpy(head + 4, type, 4);
    fwrite(head, 1, 8, f);
    if (len) {
        fwrite(data, 1, len, f);
    }
    uint32_t c = crc(0, (const uint8_t *)type, 4);
    c = crc(c, data, len);
    uint8_t tail[4];
    put_be32(tail, c);
    fwrite(tail, 1, 4, f);
}

bool png_write_rgb565(const char *path, const uint16_t *px, int w, int h)
{
    crc_init();

    size_t raw_len = (size_t)h * (1 + (size_t)w * 3);
    uint8_t *raw = malloc(raw_len);
    if (!raw) {
        return false;
    }
    uint8_t *p = raw;
    for (int y = 0; y < h; y++) {
        *p++ = 0;   // no filter
        for (int x = 0; x < w; x++) {
            uint16_t c = px[y * w + x];
            uint8_t r = (c >> 11) & 0x1f, g = (c >> 5) & 0x3f, b = c & 0x1f;
            *p++ = (r << 3) | (r >> 2);
            *p++ = (g << 2) | (g >> 4);
            *p++ = (b << 3) | (b >> 2);
        }
    }

    // zlib stream made of uncompressed ("stored") deflate blocks.
    size_t blocks = (raw_len + 65534) / 65535;
    size_t z_len = 2 + raw_len + blocks * 5 + 4;
    uint8_t *z = malloc(z_len);
    if (!z) {
        free(raw);
        return false;
    }
    uint8_t *q = z;
    *q++ = 0x78;
    *q++ = 0x01;
    uint32_t a = 1, b = 0;
    for (size_t off = 0; off < raw_len; off += 65535) {
        size_t n = raw_len - off < 65535 ? raw_len - off : 65535;
        *q++ = off + n >= raw_len ? 1 : 0;
        *q++ = n & 0xff;
        *q++ = n >> 8;
        *q++ = ~n & 0xff;
        *q++ = (~n >> 8) & 0xff;
        memcpy(q, raw + off, n);
        q += n;
        for (size_t i = 0; i < n; i++) {
            a = (a + raw[off + i]) % 65521;
            b = (b + a) % 65521;
        }
    }
    put_be32(q, (b << 16) | a);

    FILE *f = fopen(path, "wb");
    if (!f) {
        free(raw);
        free(z);
        return false;
    }
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    fwrite(sig, 1, 8, f);
    uint8_t ihdr[13];
    put_be32(ihdr, w);
    put_be32(ihdr + 4, h);
    ihdr[8] = 8;    // bits per channel
    ihdr[9] = 2;    // RGB
    ihdr[10] = ihdr[11] = ihdr[12] = 0;
    write_chunk(f, "IHDR", ihdr, sizeof(ihdr));
    write_chunk(f, "IDAT", z, (uint32_t)z_len);
    write_chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(raw);
    free(z);
    return true;
}
