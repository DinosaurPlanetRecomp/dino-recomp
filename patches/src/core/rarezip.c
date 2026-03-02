#include "patches.h"

#include "PR/ultratypes.h"

extern u8 *rarezip_inflate_input;
extern u8 *rarezip_inflate_output;

extern u32 rarezip_bit_buffer;
extern u32 rarezip_num_bits;

extern s32 rarezip_inflate_block();

RECOMP_PATCH u8 *rarezip_uncompress(u8 *compressedInput, u8 *decompressedOutput, s32 outputSize) {
    rarezip_inflate_input = compressedInput + 5;
    rarezip_inflate_output = decompressedOutput;
    rarezip_num_bits = 0;
    rarezip_bit_buffer = 0;

    //if (rarezip_inflate_input){} // fake match
    
    while (rarezip_inflate_block() != 0) {
        // @recomp: Overflow checking
        if (outputSize != -1) {
            s32 inflatedSize = rarezip_inflate_output - decompressedOutput;
            if (inflatedSize > outputSize) {
                recomp_eprintf("rarezip_uncompress(%08x,%08x,...) overflow %d/%d\n", 
                    compressedInput, decompressedOutput, inflatedSize, outputSize);
            }
        }
    }

    // if (outputSize != -1) {
    //     if (((!decompressedOutput) && (!decompressedOutput)) && (!decompressedOutput)){} // fake match
    //     if (compressedInput){} // fake match
    // }
    
    return decompressedOutput;
}
