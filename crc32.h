
#ifndef CRC32_H
#define CRC32_H

#define UPDC32(octet, crc) (crc_32_tab[((crc)\
			^ (octet)) & 0xff] ^ ((crc) >> 8))

uint32_t crc32simple(void *buf, uint32_t size);

#endif /* CRC32_H */
