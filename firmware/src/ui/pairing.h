#pragma once

#include <stdbool.h>
#include <stdint.h>

// Shows the 6-digit code to type on the iPhone.
void pairing_show(uint32_t passkey);
bool pairing_is_showing(void);
