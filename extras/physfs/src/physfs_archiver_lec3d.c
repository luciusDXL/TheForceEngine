/*
 * LucasArts 1990s-era shooters container files
 *  GOB (Dark Forces 1994,  Jedi Knight 1997)
 *  LAB (Outlaws 1996)
 *  LFD (XWing, Tie Fighter, Dark Forces, probably others)
 *
 * Written by Manuel Lauss <manuel.lauss@gmail.com>
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#if PHYSFS_SUPPORTS_LECARCHIVES

/* GOB1: WAD-Style */
static int gob1LoadEntries(PHYSFS_Io *io, const PHYSFS_uint32 offset, void *arc)
{
	PHYSFS_uint32 i, entries, dofs, dlen;
	char name[13];

	if (!io->seek(io, offset))
		return 0;

	BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &entries, 4), 0);
	entries = PHYSFS_swapULE32(entries);

	for (i = 0; i < entries; i++)
	{
		BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &dofs, 4), 0);
		BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &dlen, 4), 0);
		BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, name, 13), 0);
		name[12] = '\0';

		dofs = PHYSFS_swapULE32(dofs);
		dlen = PHYSFS_swapULE32(dlen);
		BAIL_IF_ERRPASS(!UNPK_addEntry(arc, name, 0, -1, -1, dofs, dlen), 0);
	}

	return 1;
}

/* GOB2: GOB1 with 127 byte filepath hierarchy */
static int gob2LoadEntries(PHYSFS_Io *io, const PHYSFS_uint32 offset, void *arc)
{
	PHYSFS_uint32 i, entries, dofs, dlen;
	char name[128], *c;

	if (!io->seek(io, offset))
		return 0;

	BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &entries, 4), 0);
	entries = PHYSFS_swapULE32(entries);

	for (i = 0; i < entries; i++)
	{
		BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &dofs, 4), 0);
		BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &dlen, 4), 0);
		BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, name, 128), 0);
		name[127] = '\0';

		/* replace backslashes */
		c = name;
		while (*c)
		{
			if (*c == '\\')
				*c = '/';
			++c;
		}

		dofs = PHYSFS_swapULE32(dofs);
		dlen = PHYSFS_swapULE32(dlen);
		BAIL_IF_ERRPASS(!UNPK_addEntry(arc, name, 0, -1, -1, dofs, dlen), 0);
	}

	return 1;
}

static void *GOB_openArchive(PHYSFS_Io *io, const char *name,
			     int forWriting, int *claimed)
{
	PHYSFS_uint8 buf[12];
	PHYSFS_uint32 catofs = 0;
	void *unpkarc = NULL;
	int ret, gob;

	assert(io != NULL);  /* shouldn't ever happen. */

	BAIL_IF(forWriting, PHYSFS_ERR_READ_ONLY, NULL);
	BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, buf, 4), NULL);

	if (memcmp(buf, "GOB\x0a", 4) == 0)
	{
		gob = 1;
	}
	else if (memcmp(buf, "GOB\x20", 4) == 0)
	{
		/* Version */
		BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &catofs, 4), NULL);
		catofs = PHYSFS_swapULE32(catofs);
		if (catofs != 0x14)
			BAIL(PHYSFS_ERR_UNSUPPORTED, NULL);

		gob = 2;
	}
	else
	{
		BAIL(PHYSFS_ERR_UNSUPPORTED, NULL);
		gob = 0;
	}

	BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &catofs, 4), NULL);
	catofs = PHYSFS_swapULE32(catofs);

	*claimed = 1;

	unpkarc = UNPK_openArchive(io, 0, 1);
	BAIL_IF_ERRPASS(!unpkarc, NULL);

	switch (gob)
	{
	case 1: ret = gob1LoadEntries(io, catofs, unpkarc); break;
	case 2: ret = gob2LoadEntries(io, catofs, unpkarc); break;
	default: ret = 0;
	}

	if (ret == 0)
	{
		UNPK_abandonArchive(unpkarc);
		return NULL;
	}

	return unpkarc;
}

const PHYSFS_Archiver __PHYSFS_Archiver_GOB =
{
	CURRENT_PHYSFS_ARCHIVER_API_VERSION,
	{
		"GOB",
		"LucasArts GOB container",
		"Manuel Lauss <manuel.lauss@gmail.com>",
		"https://icculus.org/physfs/",
		0,  /* supportsSymlinks */
	},
	GOB_openArchive,
	UNPK_enumerate,
	UNPK_openRead,
	UNPK_openWrite,
	UNPK_openAppend,
	UNPK_remove,
	UNPK_mkdir,
	UNPK_stat,
	UNPK_closeArchive
};

/** LFD **/

static int lfdLoadEntries(PHYSFS_Io *io, const PHYSFS_uint32 skip, void *arc)
{
	char ext[5], finalname[16];
	PHYSFS_uint32 dlen, pos;
	PHYSFS_sint64 len;
	int i;

	len = io->length(io);
	if (len < 4)
	{
		PHYSFS_setErrorCode(PHYSFS_ERR_PAST_EOF);
		return 0;
	}

	pos = skip;
	while (pos < len)
	{
		if (!io->seek(io, pos))
			return 0;

		memset(finalname, 0, 16);
		BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, ext, 4), 0);
		BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, finalname, 8), 0);
		BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &dlen, 4), 0);
		dlen = PHYSFS_swapULE32(dlen);

		ext[4] = 0;
		i = strlen(finalname);
		finalname[i] = '.';
		strcpy(finalname + i + 1, ext);
		pos += 16;
		BAIL_IF_ERRPASS(!UNPK_addEntry(arc, finalname, 0, -1, -1, pos, dlen), 0);

		pos += dlen;	/* name+size entry + actual size */
	}

	return 1;
}

static void *LFD_openArchive(PHYSFS_Io *io, const char *name,
			     int forWriting, int *claimed)
{
	PHYSFS_uint8 buf[16];
	PHYSFS_uint32 catsize;
	void *unpkarc = NULL;

	assert(io != NULL);	/* shouldn't ever happen. */

	BAIL_IF(forWriting, PHYSFS_ERR_READ_ONLY, NULL);
	BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, buf, 12), NULL);

	if (memcmp(buf, "RMAPresource", 12) == 0)
	{
		/* has a content directory */
		BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &catsize, 4), NULL);
		catsize = PHYSFS_swapULE32(catsize);
		if ((catsize & 15) || (catsize == 0))
			BAIL(PHYSFS_ERR_UNSUPPORTED, NULL);

		catsize += 16;	/* include header */
	}
	else
	{
		catsize = 0;
	}

	if (!io->seek(io, 0))
		return NULL;

	*claimed = 1;

	unpkarc = UNPK_openArchive(io, 0, 1);
	BAIL_IF_ERRPASS(!unpkarc, NULL);

	if (!lfdLoadEntries(io, catsize, unpkarc))
	{
		UNPK_abandonArchive(unpkarc);
		return NULL;
	}

	return unpkarc;
}

const PHYSFS_Archiver __PHYSFS_Archiver_LFD =
{
	CURRENT_PHYSFS_ARCHIVER_API_VERSION,
	{
		"LFD",
		"LucasArts LFD container",
		"Manuel Lauss <manuel.lauss@gmail.com>",
		"https://icculus.org/physfs/",
		0,  /* supportsSymlinks */
	},
	LFD_openArchive,
	UNPK_enumerate,
	UNPK_openRead,
	UNPK_openWrite,
	UNPK_openAppend,
	UNPK_remove,
	UNPK_mkdir,
	UNPK_stat,
	UNPK_closeArchive
};

/** LAB **/

/* read characters until \0 is found */
static PHYSFS_sint32 readstring(PHYSFS_Io *io, char *dest, unsigned max)
{
	PHYSFS_uint32 bytes = 0;
	PHYSFS_sint64 ret;
	char c;

	while (bytes < max)
	{
		ret = io->read(io, &c, 1);
		if (ret == 0)
			return 0; /* file ended prematurely */
		else if (ret < 0)
			return -1;
		else if (c == 0)
			return bytes;
		*dest++ = c;
		bytes++;
	}
	return bytes;
}

static int labLoadEntries(PHYSFS_Io *io, const PHYSFS_uint32 cnt, void *arc)
{
	const PHYSFS_uint32 lab_name_table_start = 16 + (cnt * 16);
	PHYSFS_uint32 nofs, dofs, dlen, fcc;
	PHYSFS_sint32 readlen;
	PHYSFS_sint64 savepos;
	char fn[32];
	int i;

	for (i = 0; i < cnt; i++)
	{
		BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &nofs, 4), 0);
		BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &dofs, 4), 0);
		BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &dlen, 4), 0);
		BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &fcc, 4), 0);
		nofs = PHYSFS_swapULE32(nofs);
		dofs = PHYSFS_swapULE32(dofs);
		dlen = PHYSFS_swapULE32(dlen);
		fcc = PHYSFS_swapULE32(fcc);

		/* remember where we parked */
		savepos = io->tell(io);
		if (savepos < 0)
			return 0;
		/* go to the filename table entry and read it */
		if (!io->seek(io, lab_name_table_start + nofs))
			return 0;
		memset(fn, 0, 32);
		readlen = readstring(io, fn, 31);
		if (readlen > 0)
		{
			BAIL_IF_ERRPASS(!UNPK_addEntry(arc, fn, 0, -1, -1, dofs, dlen), 0);
		}
		else
		{
			if (readlen == 0)
				PHYSFS_setErrorCode(PHYSFS_ERR_PAST_EOF);

			return 0;
		}

		/* ah that's the spot */
		if (!io->seek(io, savepos))
			return 0;
	}

	return 1;
}

static void *LAB_openArchive(PHYSFS_Io *io, const char *name,
			     int forWriting, int *claimed)
{
	PHYSFS_uint8 buf[16];
	PHYSFS_uint32 catsize, entries, i;
	void *unpkarc = NULL;

	assert(io != NULL);  /* shouldn't ever happen. */

	BAIL_IF(forWriting, PHYSFS_ERR_READ_ONLY, NULL);
	BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, buf, 4), NULL);
	if (memcmp(buf, "LABN", 4) != 0)
		BAIL(PHYSFS_ERR_UNSUPPORTED, NULL);

	/* Container Format version, always 0x00010000 */
	BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &i, 4), NULL);
	i = PHYSFS_swapULE32(i);
	if (i != 0x00010000)
		BAIL(PHYSFS_ERR_UNSUPPORTED, NULL);

	BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &entries, 4), NULL);
	BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &catsize, 4), NULL);
	entries = PHYSFS_swapULE32(entries);
	catsize = PHYSFS_swapULE32(catsize);

	*claimed = 1;

	unpkarc = UNPK_openArchive(io, 0, 1);
	BAIL_IF_ERRPASS(!unpkarc, NULL);

	if (!labLoadEntries(io, entries, unpkarc))
	{
		UNPK_abandonArchive(unpkarc);
		return NULL;
	}

	return unpkarc;
}

const PHYSFS_Archiver __PHYSFS_Archiver_LAB =
{
	CURRENT_PHYSFS_ARCHIVER_API_VERSION,
	{
		"LAB",
		"LucasArts LAB container",
		"Manuel Lauss <manuel.lauss@gmail.com>",
		"https://icculus.org/physfs/",
		0,  /* supportsSymlinks */
	},
	LAB_openArchive,
	UNPK_enumerate,
	UNPK_openRead,
	UNPK_openWrite,
	UNPK_openAppend,
	UNPK_remove,
	UNPK_mkdir,
	UNPK_stat,
	UNPK_closeArchive
};

#endif
