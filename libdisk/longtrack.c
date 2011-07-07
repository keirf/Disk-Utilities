/******************************************************************************
 * disk/longtrack.c
 * 
 * Detect various custom long protection tracks.
 * 
 * Written in 2011 by Keir Fraser
 * 
 * Type 0: Amnios, Archipelagos
 *  u16 0x4454
 *  u8 0x33 (encoded in-place, 1000+ times, to track gap)
 *  Track is checked to be >= 107200 bits long
 * 
 * Type 1: Lotus I/II
 *  u16 0x4124,0x4124
 *  Rest of track is (MFM-encoded) zeroes
 *  Track is checked to be >= 102400 bits long
 * 
 * TRKTYP_longtrack data layout:
 *  u16 type
 */

#include <libdisk/util.h>
#include "private.h"

#include <arpa/inet.h>

static void *longtrack_write_mfm(
    struct disk *d, unsigned int tracknr, struct stream *s)
{
    struct track_info *ti = &d->di->track[tracknr];
    uint16_t *dat = memalloc(2);
    unsigned int i;

    while ( stream_next_bit(s) != -1 )
    {
        if ( s->word == 0x4454a525 )
        {
            /* Check for a long sequence of encoded 0x333333.... */
            for ( i = 0; i < 500; i++ )
            {
                stream_next_bits(s, 32);
                if ( copylock_decode_word(s->word) != 0x3333 )
                    break;
            }
            if ( i != 500 )
                continue;
            /* Validated the sequence: we're done */
            ti->data_bitoff = s->index_offset - 31;
            ti->total_bits = 110000; /* long enough */
            *dat = 0;
            goto found;
        }

        if ( s->word == 0x41244124 )
        {
#if 0
            /* Check for a long sequence of encoded 0x00000.... */
            for ( i = 0; i < 500; i++ )
            {
                stream_next_bits(s, 32);
                if ( copylock_decode_word(s->word) != 0 )
                    break;
            }
            if ( i != 500 )
                continue;
#endif
            /* Validated the sequence: we're done */
            ti->data_bitoff = s->index_offset - 31;
            ti->total_bits = 105500; /* long enough */
            *dat = 1;
            goto found;
        }
    }

    return NULL;

found:
    *dat = htons(*dat);
    ti->len = 2;
    return dat;
}

static void longtrack_read_mfm(
    struct disk *d, unsigned int tracknr, struct track_buffer *tbuf)
{
    struct track_info *ti = &d->di->track[tracknr];
    uint16_t *dat = (uint16_t *)ti->dat;
    unsigned int i;

    switch ( ntohs(*dat) )
    {
    case 0:
        tbuf_bits(tbuf, SPEED_AVG, TB_raw, 16, 0x4454);
        for ( i = 0; i < 6000; i++ )
            tbuf_bits(tbuf, SPEED_AVG, TB_all, 8, 0x33);
        break;
    case 1:
        tbuf_bits(tbuf, SPEED_AVG, TB_raw, 32, 0x41244124);
        for ( i = 0; i < 6000; i++ )
            tbuf_bits(tbuf, SPEED_AVG, TB_all, 8, 0);
        break;
    }
}

struct track_handler longtrack_handler = {
    .write_mfm = longtrack_write_mfm,
    .read_mfm = longtrack_read_mfm
};