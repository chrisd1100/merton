#include "lib.h"

uint32_t crypto_crc32(void *data, size_t size)
{
	uint32_t crc = 0;
	uint32_t table[0x100];

	for (uint32_t x = 0; x < 0x100; x++) {
		uint32_t r = x;

		for (uint8_t y = 0; y < 8; y++)
			r = (r & 1 ? 0 : 0xEDB88320) ^ r >> 1;

		table[x] = r ^ 0xFF000000;
	}

	for (size_t x = 0; x < size; x++)
		crc = table[(uint8_t) crc ^ ((uint8_t *) data)[x]] ^ crc >> 8;

	return crc;
}
