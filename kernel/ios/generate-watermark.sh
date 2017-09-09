#!/bin/bash

ID=`echo "$1probably sha1 of their name + secret nonce, and then put that in the binary and also use that to decrypt some other piece of info." | /usr/bin/openssl sha1`
ID_COMMAS=`echo $ID | sed 's/\([0-9a-f][0-9a-f]\)/0x\1, /g'`
ID_INITIALIZER="{'W', '$', 'P', 'P', $ID_COMMAS 0x0}"

CRC_FILE=`mktemp -t crc`
echo "0000: ${ID}00" | xxd -r -c 256 > $CRC_FILE
ID_CRC32=`crc32 $CRC_FILE`
rm $CRC_FILE

cat > watermark.h << END_WATERMARK
#include <zlib.h>
#include <stdint.h>

static inline uint32_t watermark_static()
{
    return 0x$ID_CRC32;
}

static inline uint32_t watermark_generate()
{
    const uint8_t watermark[] = $ID_INITIALIZER;
    return crc32(0, watermark + 4, sizeof(watermark) - 4);
}

END_WATERMARK

