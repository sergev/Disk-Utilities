/*
 * disk/eye_of_horus.c
 * 
 * Custom format as used on Eye Of Horus by Logotron / Denton Designs.
 * 
 * Written in 2012 by Keir Fraser
 * 
 * RAW TRACK LAYOUT:
 *  u16 0x4489,0x4489 :: Sync
 *  u32 header[5][2]  :: Interleaved even/odd longs
 *  u32 header_csum[2]
 *  u32 data_csum[2]
 *  u32 data[N][2]
 * 
 * TRKTYP_eye_of_horus data layout:
 *  u8 sector_data[N]
 */

#include <libdisk/util.h>
#include "../private.h"

#include <arpa/inet.h>

static void *eye_of_horus_write_mfm(
    struct disk *d, unsigned int tracknr, struct stream *s)
{
    struct track_info *ti = &d->di->track[tracknr];

    while (stream_next_bit(s) != -1) {

        uint32_t raw[2], hdr[7], dat[0x1600/4];
        unsigned int i;
        char *block;

        if (s->word != 0x44894489)
            continue;
        ti->data_bitoff = s->index_offset - 31;

        for (i = 0; i < ARRAY_SIZE(hdr); i++) {
            if (stream_next_bytes(s, raw, 8) == -1)
                goto fail;
            mfm_decode_bytes(MFM_even_odd, 4, raw, &hdr[i]);
        }
        if ((ntohl(hdr[0]) != (0xff00000b | (tracknr << 16))) ||
            (ntohl(hdr[1]) > 0x1600) ||
            (ntohl(hdr[5]) != amigados_checksum(hdr, 5*4)))
            continue;

        ti->bytes_per_sector = ntohl(hdr[1]);
        ti->len = ti->bytes_per_sector + 3*4;

        for (i = 0; i < ti->bytes_per_sector/4; i++) {
            if (stream_next_bytes(s, raw, 8) == -1)
                goto fail;
            mfm_decode_bytes(MFM_even_odd, 4, raw, &dat[i]);
        }
        if (ntohl(hdr[6]) != amigados_checksum(dat, ti->bytes_per_sector))
            continue;

        block = memalloc(ti->len);
        memcpy(block, dat, ti->len);
        memcpy(&block[ti->bytes_per_sector], &hdr[2], 3*4);
        set_all_sectors_valid(ti);
        return block;
    }

fail:
    return NULL;
}

static void eye_of_horus_read_mfm(
    struct disk *d, unsigned int tracknr, struct track_buffer *tbuf)
{
    struct track_info *ti = &d->di->track[tracknr];
    uint32_t *dat = (uint32_t *)ti->dat, hdr[7];
    unsigned int i;

    tbuf_bits(tbuf, SPEED_AVG, MFM_raw, 32, 0x44894489);

    hdr[0] = htonl(0xff00000b | (tracknr << 16));
    hdr[1] = htonl(ti->bytes_per_sector);
    memcpy(&hdr[2], &dat[ti->bytes_per_sector/4], 3*4);
    hdr[5] = htonl(amigados_checksum(hdr, 5*4));
    hdr[6] = htonl(amigados_checksum(dat, ti->bytes_per_sector));

    for (i = 0; i < ARRAY_SIZE(hdr); i++)
        tbuf_bits(tbuf, SPEED_AVG, MFM_even_odd, 32, ntohl(hdr[i]));

    for (i = 0; i < ti->bytes_per_sector/4; i++)
        tbuf_bits(tbuf, SPEED_AVG, MFM_even_odd, 32, ntohl(dat[i]));
}

struct track_handler eye_of_horus_handler = {
    .nr_sectors = 1,
    .write_mfm = eye_of_horus_write_mfm,
    .read_mfm = eye_of_horus_read_mfm
};

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */