#pragma once

#include <stddef.h>

// The built-in fonts only cover ASCII. Converts common punctuation (smart quotes,
// dashes, ellipsis) to plain equivalents and drops anything else, such as emoji.
void text_to_ascii(char *dst, size_t dst_size, const char *src);
