/******************************************************************************
 * libdisk/container_adf.c
 * 
 * Read/write ADF images.
 * 
 * Written in 2011 by Keir Fraser
 */

#include <libdisk/util.h>
#include "private.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

static void adf_init_track(struct track_info *ti)
{
    const struct track_handler *thnd = handlers[TRKTYP_amigados];
    unsigned int i;

    init_track_info_from_handler_info(ti, thnd);
    ti->flags = 0;
    ti->valid_sectors = 0;
    ti->dat = memalloc(ti->len);
    ti->data_bitoff = 1024;
    ti->total_bits = DEFAULT_BITS_PER_TRACK;

    for ( i = 0; i < ti->len/4; i++ )
        memcpy(ti->dat+i*4, "NDOS", 4);
}

static void adf_init(struct disk *d)
{
    struct disk_info *di;
    unsigned int i;

    d->di = di = memalloc(sizeof(*di));
    di->nr_tracks = 160;
    di->flags = 0;
    di->track = memalloc(di->nr_tracks * sizeof(struct track_info));

    for ( i = 0; i < di->nr_tracks; i++ )
        adf_init_track(&di->track[i]);
}

static int adf_open(struct disk *d, bool_t quiet)
{
    struct track_info *ti;
    struct disk_info *di;
    unsigned int i, j, k, valid_sectors;
    off_t sz;

    sz = lseek(d->fd, 0, SEEK_END);
    if ( sz != 160*512*11 )
    {
        if ( !quiet )
            warnx("ADF file bad size: %lu bytes", (unsigned long)sz);
        return 0;
    }
    lseek(d->fd, 0, SEEK_SET);

    adf_init(d);
    di = d->di;

    for ( i = 0; i < di->nr_tracks; i++ )
    {
        ti = &di->track[i];
        read_exact(d->fd, ti->dat, ti->len);
        ti->valid_sectors = 0;
        for ( j = 0; j < ti->nr_sectors; j++ )
        {
            unsigned char *p = ti->dat + j*ti->bytes_per_sector;
            for ( k = 0; k < ti->bytes_per_sector/4; k++ )
                if ( memcmp(p+k*4, "NDOS", 4) )
                    break;
            if ( k != ti->bytes_per_sector/4 )
                ti->valid_sectors |= 1u << j;
        }
    }

    return 1;
}

static void adf_close(struct disk *d)
{
    struct disk_info *di = d->di;
    unsigned int i;

    lseek(d->fd, 0, SEEK_SET);
    ftruncate(d->fd, 0);

    for ( i = 0; i < di->nr_tracks; i++ )
        write_exact(d->fd, di->track[i].dat, 11*512);
}

static void adf_write_mfm(
    struct disk *d, unsigned int tracknr, struct stream *s)
{
    const struct track_handler *thnd = handlers[TRKTYP_amigados];
    struct disk_info *di = d->di;
    struct track_info *ti = &di->track[tracknr];
    unsigned int i;

    memfree(ti->dat);

    stream_reset(s, tracknr);
    stream_next_index(s);
    ti->dat = thnd->write_mfm(tracknr, ti, s);    

    if ( ti->dat == NULL )
    {
        adf_init_track(ti);
    }
    else if ( ti->type == TRKTYP_amigados_labelled )
    {
        init_track_info_from_handler_info(ti, thnd);
        for ( i = 0; i < ti->nr_sectors; i++ )
            memmove(ti->dat + i * 512,
                    ti->dat + i * (512 + 16) + 16,
                    512);
    }
}

struct container container_adf = {
    .init = adf_init,
    .open = adf_open,
    .close = adf_close,
    .write_mfm = adf_write_mfm
};