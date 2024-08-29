/*
 * CSM support routines for PhysicsFS.
 *
 * This driver handles Chasm: The Rift engine archives ("CSM.BINs"). 
 * This format (but not this driver) was developed by Action Forms Ltd.
 * and published by Megamedia for use with the Chasm: The Rift engine.
 * The specs of the format are from http://github.com/Panzerschrek/Chasm-Reverse
 * The format of the archive: (from the specs)
 *
 *  A CSM file has three parts:
 *  (1) a 6 byte header
 *  (2) a TOC that contains the names, offsets, and
 *      sizes of all the entries in the CSM.BIN
 *
 *  The header consists of three four-byte parts:
 *    (a) an ASCII string which must be "CSid"
 *    (b) a uint16 which is the number of TOC entries in the CSM.BIN
 *
 *  The TOC has one -byte entry for every lump. Each entry consists
 *  of three parts:
 *
 *    (a) a uint8, the length of the filename
 *    (b) an 12-byte ASCII string, the name of the entry, padded with zeros.
 *    (c) a uint32, the size of the entry in bytes
 *    (d) a uint32, the file offset to the start of the entry
 * 
 *
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 * This file written by Jon Daniel, based on the WAD archiver by
 *  Travis Wells.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#if PHYSFS_SUPPORTS_CSM

static int csmLoadEntries(PHYSFS_Io *io, const PHYSFS_uint16 count, void *arc)
{
    PHYSFS_uint16 i;
    for (i = 0; i < count; i++)
    {
    	PHYSFS_uint8 fn_len;
	char name[12];
        PHYSFS_uint32 size;
        PHYSFS_uint32 pos;

        BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &fn_len, 1), 0);
        BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, name, 12), 0);
        BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &size, 4), 0);
        BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &pos, 4), 0);

	if(fn_len > 12) fn_len = 12;
        name[fn_len] = '\0'; /* name might not be null-terminated in file. */
        size = PHYSFS_swapULE32(size);
        pos = PHYSFS_swapULE32(pos);
        BAIL_IF_ERRPASS(!UNPK_addEntry(arc, name, 0, -1, -1, pos, size), 0);
    } /* for */

    return 1;
} /* csmLoadEntries */


static void *CSM_openArchive(PHYSFS_Io *io, const char *name,
                             int forWriting, int *claimed)
{
    PHYSFS_uint8 buf[4];
    PHYSFS_uint16 count;
    void *unpkarc;

    assert(io != NULL);  /* shouldn't ever happen. */

    BAIL_IF(forWriting, PHYSFS_ERR_READ_ONLY, NULL);
    BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, buf, sizeof (buf)), NULL);
    if (memcmp(buf, "CSid", 4) != 0)
        BAIL(PHYSFS_ERR_UNSUPPORTED, NULL);

    *claimed = 1;

    BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &count, sizeof (count)), NULL);
    count = PHYSFS_swapULE16(count);


    unpkarc = UNPK_openArchive(io, 0, 1);
    BAIL_IF_ERRPASS(!unpkarc, NULL);

    if (!csmLoadEntries(io, count, unpkarc))
    {
        UNPK_abandonArchive(unpkarc);
        return NULL;
    } /* if */

    return unpkarc;
} /* CSM_openArchive */


const PHYSFS_Archiver __PHYSFS_Archiver_CSM =
{
    CURRENT_PHYSFS_ARCHIVER_API_VERSION,
    {
        "BIN",
        "Chasm: The Rift engine format",
	"Jon Daniel <jondaniel879@gmail.com>",
        "http://www.github.com/Panzerschrek/Chasm-Reverse",
        0,  /* supportsSymlinks */
    },
    CSM_openArchive,
    UNPK_enumerate,
    UNPK_openRead,
    UNPK_openWrite,
    UNPK_openAppend,
    UNPK_remove,
    UNPK_mkdir,
    UNPK_stat,
    UNPK_closeArchive
};

#endif  /* defined PHYSFS_SUPPORTS_CSM */

/* end of physfs_archiver_CSM.c ... */

