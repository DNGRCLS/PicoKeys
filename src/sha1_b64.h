#pragma once
#include <stdint.h>
#include <stddef.h>

void sha1(const uint8_t *data, size_t len, uint8_t out[20]);
size_t base64_encode(const uint8_t *in, size_t len, char *out);  // out needs 4*ceil(len/3)+1
