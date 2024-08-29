/*
 * Playdate support routines for PhysicsFS.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_platforms.h"

#ifdef PHYSFS_PLATFORM_PLAYDATE

#include "pd_api.h"

#include "physfs_internal.h"

static PlaydateAPI *playdate = NULL;

/* Playdate interpolates a separate write dir on top of a read-only
   base dir, so put the "user" dir somewhere deeper in the tree
   so they don't overlap, so PhysicsFS can manage this like apps
   expect. */
#define WRITABLE_DIRNAME ".$PHYSFSWRITE$"


static void *playdateAllocatorMalloc(PHYSFS_uint64 s);
static void *playdateAllocatorRealloc(void *ptr, PHYSFS_uint64 s);
static void playdateAllocatorFree(void *ptr);

int __PHYSFS_platformInit(const char *argv0)
{
    /* as a cheat, we expect argv0 to be a PlaydateAPI* on Playdate. */
    playdate = (PlaydateAPI *) argv0;

    allocator.Init = NULL;
    allocator.Deinit = NULL;
    allocator.Malloc = playdateAllocatorMalloc;
    allocator.Realloc = playdateAllocatorRealloc;
    allocator.Free = playdateAllocatorFree;

    return 1;  /* ready to go! */
}

void __PHYSFS_platformDeinit(void)
{
    playdate = NULL;
}

void __PHYSFS_platformDetectAvailableCDs(PHYSFS_StringCallback cb, void *data)
{
    /* obviously no CD-ROM drives on Playdate */
}

char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
    return __PHYSFS_strdup("/");
}

char *__PHYSFS_platformCalcUserDir(void)
{
    return __PHYSFS_strdup("/" WRITABLE_DIRNAME "/");
}

char *__PHYSFS_platformCalcPrefDir(const char *org, const char *app)
{
    const size_t slen = strlen(org) + strlen(app) + strlen(WRITABLE_DIRNAME) + 5;
    char *retval = (char *) allocator.Malloc(slen);
    BAIL_IF(!retval, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    snprintf(retval, slen, "/" WRITABLE_DIRNAME "/%s/%s/", org, app);
    return retval;
}

typedef struct listfiles_callback_data
{
    PHYSFS_EnumerateCallback callback;
    const char *origdir;
    void *callbackdata;
    PHYSFS_EnumerateCallbackResult retval;
} listfiles_callback_data;

static void listfiles_callback(const char *path, void *userdata)
{
    listfiles_callback_data *data = (listfiles_callback_data *) userdata;
    if (data->retval == PHYSFS_ENUM_OK) {
        const char *od = data->origdir;
        if (path[0] == '.') {  /* ignore "." and ".." */
            if ((path[1] == '\0') || ((path[1] == '.') && (path[2] == '\0'))) {
                return;
            }
        }
        if ((*od == '\0') || ((od[0] == '/') && (od[1] == '\0'))) {
            /* don't list our separated write dir when enumerating. */
            if (strcmp(path, WRITABLE_DIRNAME) == 0) {
                return;
            }
        }

        data->retval = data->callback(data->callbackdata, data->origdir, path);
    }
}

PHYSFS_EnumerateCallbackResult __PHYSFS_platformEnumerate(const char *dirname,
                               PHYSFS_EnumerateCallback callback,
                               const char *origdir, void *callbackdata)
{                                        
    listfiles_callback_data data;
    data.callback = callback;
    data.origdir = origdir;
    data.callbackdata = callbackdata;
    data.retval = PHYSFS_ENUM_OK;
    if (playdate->file->listfiles(dirname, listfiles_callback, &data, 1) == -1) {
        data.retval = PHYSFS_ENUM_ERROR;
        PHYSFS_setErrorCode(PHYSFS_ERR_OS_ERROR);
    } else if (data.retval == PHYSFS_ENUM_ERROR) {
        PHYSFS_setErrorCode(PHYSFS_ERR_APP_CALLBACK);
    }

    return data.retval;
}

int __PHYSFS_platformMkDir(const char *filename)
{
    BAIL_IF(playdate->file->mkdir(filename) == -1, PHYSFS_ERR_OS_ERROR, 0);
    return 1;
}

void *__PHYSFS_platformOpenRead(const char *filename)
{
    SDFile *sdf = playdate->file->open(filename, kFileRead|kFileReadData);
    BAIL_IF(!sdf, PHYSFS_ERR_OS_ERROR, NULL);
    return sdf;
}

void *__PHYSFS_platformOpenWrite(const char *filename)
{
    SDFile *sdf = playdate->file->open(filename, kFileWrite);
    BAIL_IF(!sdf, PHYSFS_ERR_OS_ERROR, NULL);
    return sdf;
}

void *__PHYSFS_platformOpenAppend(const char *filename)
{
    SDFile *sdf = playdate->file->open(filename, kFileAppend);
    BAIL_IF(!sdf, PHYSFS_ERR_OS_ERROR, NULL);
    return sdf;
}

PHYSFS_sint64 __PHYSFS_platformRead(void *opaque, void *buf, PHYSFS_uint64 len)
{
    int rc;
    BAIL_IF(!__PHYSFS_ui64FitsAddressSpace(len),PHYSFS_ERR_INVALID_ARGUMENT,-1);
    rc = playdate->file->read((SDFile *) opaque, buf, (unsigned int) len);
    BAIL_IF(rc == -1, PHYSFS_ERR_OS_ERROR, -1);
    return (PHYSFS_sint64) rc;
}

PHYSFS_sint64 __PHYSFS_platformWrite(void *opaque, const void *buf,
                                     PHYSFS_uint64 len)
{
    int rc;
    BAIL_IF(!__PHYSFS_ui64FitsAddressSpace(len),PHYSFS_ERR_INVALID_ARGUMENT,-1);
    rc = playdate->file->write((SDFile *) opaque, buf, (unsigned int) len);
    BAIL_IF(rc == -1, PHYSFS_ERR_OS_ERROR, -1);
    return (PHYSFS_sint64) rc;
}

int __PHYSFS_platformSeek(void *opaque, PHYSFS_uint64 pos)
{
    const int ipos = (int) pos;
    int rc;

    /* hooray for 32-bit filesystem limits!  :) */
    BAIL_IF(((PHYSFS_uint64) ipos) != pos, PHYSFS_ERR_INVALID_ARGUMENT, 0);

    rc = playdate->file->seek((SDFile *) opaque, ipos, SEEK_SET);
    BAIL_IF(rc == -1, PHYSFS_ERR_OS_ERROR, 0);
    return 1;
}

PHYSFS_sint64 __PHYSFS_platformTell(void *opaque)
{
    const int pos = playdate->file->tell((SDFile *) opaque);
    BAIL_IF(pos == -1, PHYSFS_ERR_OS_ERROR, -1);
    return (PHYSFS_sint64) pos;
}

PHYSFS_sint64 __PHYSFS_platformFileLength(void *opaque)
{
    SDFile *sdf = (SDFile *) opaque;
    const int pos = playdate->file->tell(sdf);
    int retval;

    BAIL_IF(pos == -1, PHYSFS_ERR_OS_ERROR, -1);
    BAIL_IF(playdate->file->seek(sdf, 0, SEEK_END) == -1, PHYSFS_ERR_OS_ERROR, -1);
    retval = playdate->file->tell(sdf);
    BAIL_IF(playdate->file->seek(sdf, pos, SEEK_CUR) == -1, PHYSFS_ERR_OS_ERROR, -1);
    BAIL_IF(retval == -1, PHYSFS_ERR_OS_ERROR, -1);
    return (PHYSFS_sint64) retval;
}

int __PHYSFS_platformFlush(void *opaque)
{
    BAIL_IF(playdate->file->flush((SDFile *) opaque) == -1, PHYSFS_ERR_OS_ERROR, 0);
    return 1;
}

void __PHYSFS_platformClose(void *opaque)
{
    playdate->file->close((SDFile *) opaque);  /* ignore errors. You should have flushed! */
}

int __PHYSFS_platformDelete(const char *path)
{
    BAIL_IF(playdate->file->unlink(path, 0) == -1, PHYSFS_ERR_OS_ERROR, 0);
    return 1;
}


/* Convert to a format PhysicsFS can grok... */
static PHYSFS_sint64 playdateTimeToUnixTime(FileStat *statbuf)
{
    return 10;  /* !!! FIXME: calculate the time in seconds, adjust for leap years and the Unix epoch. */
}

int __PHYSFS_platformStat(const char *filename, PHYSFS_Stat *st, const int follow)
{
    FileStat statbuf;
    const int rc = playdate->file->stat(filename, &statbuf);
    BAIL_IF(rc == -1, PHYSFS_ERR_OS_ERROR, 0);

    st->filesize = (PHYSFS_sint64) statbuf.size;
	st->modtime = st->createtime = playdateTimeToUnixTime(&statbuf);
	st->accesstime = -1;
    st->filetype = statbuf.isdir ? PHYSFS_FILETYPE_DIRECTORY : PHYSFS_FILETYPE_REGULAR;
    st->readonly = (strncmp(filename, "/" WRITABLE_DIRNAME, strlen("/" WRITABLE_DIRNAME)) == 0) ? 1 : 0;

    return 1;
}

void *__PHYSFS_platformGetThreadID(void)
{
    return (void *) (size_t) 0x1;  /* !!! FIXME: does Playdate have threads? */
}

void *__PHYSFS_platformCreateMutex(void)
{
    return (void *) (size_t) 0x1;  /* !!! FIXME: does Playdate have threads? */
}

void __PHYSFS_platformDestroyMutex(void *mutex)
{
}

int __PHYSFS_platformGrabMutex(void *mutex)
{
    return 1;
}

void __PHYSFS_platformReleaseMutex(void *mutex)
{
}


#undef realloc

static void *playdateAllocatorMalloc(PHYSFS_uint64 s)
{
    BAIL_IF(!__PHYSFS_ui64FitsAddressSpace(s), PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    return playdate->system->realloc(NULL, (size_t) s);
}

static void *playdateAllocatorRealloc(void *ptr, PHYSFS_uint64 s)
{
    BAIL_IF(!__PHYSFS_ui64FitsAddressSpace(s), PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    return playdate->system->realloc(ptr, (size_t) s);
}

static void playdateAllocatorFree(void *ptr)
{
    playdate->system->realloc(ptr, 0);
}


#endif  /* PHYSFS_PLATFORM_PLAYDATE */

/* end of physfs_platform_playdate.c ... */
