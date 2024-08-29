/*
 * Wii/GameCube support routines for PhysicsFS.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_platforms.h"

#ifdef PHYSFS_PLATFORM_OGC

#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/param.h>
#include <time.h>
#include <limits.h>


#include <ogc/lwp.h>
#include <ogc/mutex.h>
#include <ogc/system.h>

#include "physfs_internal.h"


static PHYSFS_ErrorCode errcodeFromErrnoError(const int err)
{
    switch (err)
    {
        case 0: return PHYSFS_ERR_OK;
        case EACCES: return PHYSFS_ERR_PERMISSION;
        case EPERM: return PHYSFS_ERR_PERMISSION;
        case EDQUOT: return PHYSFS_ERR_NO_SPACE;
        case EIO: return PHYSFS_ERR_IO;
        case ELOOP: return PHYSFS_ERR_SYMLINK_LOOP;
        case EMLINK: return PHYSFS_ERR_NO_SPACE;
        case ENAMETOOLONG: return PHYSFS_ERR_BAD_FILENAME;
        case ENOENT: return PHYSFS_ERR_NOT_FOUND;
        case ENOSPC: return PHYSFS_ERR_NO_SPACE;
        case ENOTDIR: return PHYSFS_ERR_NOT_FOUND;
        case EISDIR: return PHYSFS_ERR_NOT_A_FILE;
        case EROFS: return PHYSFS_ERR_READ_ONLY;
        case ETXTBSY: return PHYSFS_ERR_BUSY;
        case EBUSY: return PHYSFS_ERR_BUSY;
        case ENOMEM: return PHYSFS_ERR_OUT_OF_MEMORY;
        case ENOTEMPTY: return PHYSFS_ERR_DIR_NOT_EMPTY;
        default: return PHYSFS_ERR_OS_ERROR;
    } /* switch */
} /* errcodeFromErrnoError */


static inline PHYSFS_ErrorCode errcodeFromErrno(void)
{
    return errcodeFromErrnoError(errno);
} /* errcodeFromErrno */


static inline char *buildSubdirPath(const char *subdir, size_t subdir_length)
{
    const char *baseDir;
    char *retval;
    size_t length;

    baseDir = PHYSFS_getBaseDir();
    length = strlen(baseDir);

    retval = allocator.Malloc(length + subdir_length);
    BAIL_IF(!retval, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    strcpy(retval, baseDir);
    strcpy(retval + length, subdir);

    return retval;
}

char *__PHYSFS_platformCalcUserDir(void)
{
    static const char subdir[] = "userdata/";

    /* We don't have users on the Wii/GameCube. Just create a userdata folder
     * in the application's directory. */
    return buildSubdirPath(subdir, sizeof(subdir));
} /* __PHYSFS_platformCalcUserDir */


PHYSFS_EnumerateCallbackResult __PHYSFS_platformEnumerate(const char *dirname,
                               PHYSFS_EnumerateCallback callback,
                               const char *origdir, void *callbackdata)
{
    DIR *dir;
    struct dirent *ent;
    PHYSFS_EnumerateCallbackResult retval = PHYSFS_ENUM_OK;

    dir = opendir(dirname);
    BAIL_IF(dir == NULL, errcodeFromErrno(), PHYSFS_ENUM_ERROR);

    while ((retval == PHYSFS_ENUM_OK) && ((ent = readdir(dir)) != NULL))
    {
        const char *name = ent->d_name;
        if (name[0] == '.')  /* ignore "." and ".." */
        {
            if ((name[1] == '\0') || ((name[1] == '.') && (name[2] == '\0')))
                continue;
        } /* if */

        retval = callback(callbackdata, origdir, name);
        if (retval == PHYSFS_ENUM_ERROR)
            PHYSFS_setErrorCode(PHYSFS_ERR_APP_CALLBACK);
    } /* while */

    closedir(dir);

    return retval;
} /* __PHYSFS_platformEnumerate */


int __PHYSFS_platformMkDir(const char *path)
{
    const int rc = mkdir(path, S_IRWXU);
    BAIL_IF(rc == -1, errcodeFromErrno(), 0);
    return 1;
} /* __PHYSFS_platformMkDir */


#if !defined(O_CLOEXEC) && defined(FD_CLOEXEC)
static inline void set_CLOEXEC(int fildes)
{
    int flags = fcntl(fildes, F_GETFD);
    if (flags != -1) {
        fcntl(fildes, F_SETFD, flags | FD_CLOEXEC);
    }
}
#endif

static void *doOpen(const char *filename, int mode)
{
    const int appending = (mode & O_APPEND);
    int fd;
    int *retval;

    errno = 0;

    /* O_APPEND doesn't actually behave as we'd like. */
    mode &= ~O_APPEND;

#ifdef O_CLOEXEC
    /* Add O_CLOEXEC if defined */
    mode |= O_CLOEXEC;
#endif

    do {
        fd = open(filename, mode, S_IRUSR | S_IWUSR);
    } while ((fd < 0) && (errno == EINTR));
    BAIL_IF(fd < 0, errcodeFromErrno(), NULL);

#if !defined(O_CLOEXEC) && defined(FD_CLOEXEC)
    set_CLOEXEC(fd);
#endif

    if (appending)
    {
        if (lseek(fd, 0, SEEK_END) < 0)
        {
            const int err = errno;
            close(fd);
            BAIL(errcodeFromErrnoError(err), NULL);
        } /* if */
    } /* if */

    retval = (int *) allocator.Malloc(sizeof (int));
    if (!retval)
    {
        close(fd);
        BAIL(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    *retval = fd;
    return ((void *) retval);
} /* doOpen */


void *__PHYSFS_platformOpenRead(const char *filename)
{
    return doOpen(filename, O_RDONLY);
} /* __PHYSFS_platformOpenRead */


void *__PHYSFS_platformOpenWrite(const char *filename)
{
    return doOpen(filename, O_WRONLY | O_CREAT | O_TRUNC);
} /* __PHYSFS_platformOpenWrite */


void *__PHYSFS_platformOpenAppend(const char *filename)
{
    return doOpen(filename, O_WRONLY | O_CREAT | O_APPEND);
} /* __PHYSFS_platformOpenAppend */


PHYSFS_sint64 __PHYSFS_platformRead(void *opaque, void *buffer,
                                    PHYSFS_uint64 len)
{
    const int fd = *((int *) opaque);
    ssize_t rc = 0;

    if (!__PHYSFS_ui64FitsAddressSpace(len))
        BAIL(PHYSFS_ERR_INVALID_ARGUMENT, -1);

    do {
        rc = read(fd, buffer, (size_t) len);
    } while ((rc == -1) && (errno == EINTR));
    BAIL_IF(rc == -1, errcodeFromErrno(), -1);
    assert(rc >= 0);
    assert(rc <= len);
    return (PHYSFS_sint64) rc;
} /* __PHYSFS_platformRead */


PHYSFS_sint64 __PHYSFS_platformWrite(void *opaque, const void *buffer,
                                     PHYSFS_uint64 len)
{
    const int fd = *((int *) opaque);
    ssize_t rc = 0;

    if (!__PHYSFS_ui64FitsAddressSpace(len))
        BAIL(PHYSFS_ERR_INVALID_ARGUMENT, -1);

    do {
        rc = write(fd, (void *) buffer, (size_t) len);
    } while ((rc == -1) && (errno == EINTR));
    BAIL_IF(rc == -1, errcodeFromErrno(), rc);
    assert(rc >= 0);
    assert(rc <= len);
    return (PHYSFS_sint64) rc;
} /* __PHYSFS_platformWrite */


int __PHYSFS_platformSeek(void *opaque, PHYSFS_uint64 pos)
{
    const int fd = *((int *) opaque);
    const off_t rc = lseek(fd, (off_t) pos, SEEK_SET);
    BAIL_IF(rc == -1, errcodeFromErrno(), 0);
    return 1;
} /* __PHYSFS_platformSeek */


PHYSFS_sint64 __PHYSFS_platformTell(void *opaque)
{
    const int fd = *((int *) opaque);
    PHYSFS_sint64 retval;
    retval = (PHYSFS_sint64) lseek(fd, 0, SEEK_CUR);
    BAIL_IF(retval == -1, errcodeFromErrno(), -1);
    return retval;
} /* __PHYSFS_platformTell */


PHYSFS_sint64 __PHYSFS_platformFileLength(void *opaque)
{
    const int fd = *((int *) opaque);
    struct stat statbuf;
    BAIL_IF(fstat(fd, &statbuf) == -1, errcodeFromErrno(), -1);
    return ((PHYSFS_sint64) statbuf.st_size);
} /* __PHYSFS_platformFileLength */


int __PHYSFS_platformFlush(void *opaque)
{
    const int fd = *((int *) opaque);
    int rc = -1;
    if ((fcntl(fd, F_GETFL) & O_ACCMODE) != O_RDONLY) {
        do {
            rc = fsync(fd);
        } while ((rc == -1) && (errno == EINTR));
        BAIL_IF(rc == -1, errcodeFromErrno(), 0);
    }
    return 1;
} /* __PHYSFS_platformFlush */


void __PHYSFS_platformClose(void *opaque)
{
    const int fd = *((int *) opaque);
    int rc = -1;
    do {
        rc = close(fd);  /* we don't check this. You should have used flush! */
    } while ((rc == -1) && (errno == EINTR));
    allocator.Free(opaque);
} /* __PHYSFS_platformClose */


int __PHYSFS_platformDelete(const char *path)
{
    BAIL_IF(remove(path) == -1, errcodeFromErrno(), 0);
    return 1;
} /* __PHYSFS_platformDelete */


int __PHYSFS_platformStat(const char *fname, PHYSFS_Stat *st, const int follow)
{
    struct stat statbuf;
    const int rc = follow ? stat(fname, &statbuf) : lstat(fname, &statbuf);
    BAIL_IF(rc == -1, errcodeFromErrno(), 0);

    if (S_ISREG(statbuf.st_mode))
    {
        st->filetype = PHYSFS_FILETYPE_REGULAR;
        st->filesize = statbuf.st_size;
    } /* if */

    else if(S_ISDIR(statbuf.st_mode))
    {
        st->filetype = PHYSFS_FILETYPE_DIRECTORY;
        st->filesize = 0;
    } /* else if */

    else if(S_ISLNK(statbuf.st_mode))
    {
        st->filetype = PHYSFS_FILETYPE_SYMLINK;
        st->filesize = 0;
    } /* else if */

    else
    {
        st->filetype = PHYSFS_FILETYPE_OTHER;
        st->filesize = statbuf.st_size;
    } /* else */

    st->modtime = statbuf.st_mtime;
    st->createtime = statbuf.st_ctime;
    st->accesstime = statbuf.st_atime;

    st->readonly = (access(fname, W_OK) == -1);
    return 1;
} /* __PHYSFS_platformStat */


void *__PHYSFS_platformGetThreadID(void)
{
    return (void *) LWP_GetSelf();
} /* __PHYSFS_platformGetThreadID */


void *__PHYSFS_platformCreateMutex(void)
{
    mutex_t m;
    LWP_MutexInit(&m, true);
    return (void *) m;
} /* __PHYSFS_platformCreateMutex */


void __PHYSFS_platformDestroyMutex(void *mutex)
{
    mutex_t m = (mutex_t) mutex;

    LWP_MutexDestroy(m);
} /* __PHYSFS_platformDestroyMutex */


int __PHYSFS_platformGrabMutex(void *mutex)
{
    mutex_t m = (mutex_t) mutex;
    return LWP_MutexLock(m) == 0 ? 1 : 0;
} /* __PHYSFS_platformGrabMutex */


void __PHYSFS_platformReleaseMutex(void *mutex)
{
    mutex_t m = (mutex_t) mutex;
    LWP_MutexUnlock(m);
} /* __PHYSFS_platformReleaseMutex */



int __PHYSFS_platformInit(const char *argv0)
{
    return 1;  /* always succeed. */
} /* __PHYSFS_platformInit */


void __PHYSFS_platformDeinit(void)
{
    /* no-op */
} /* __PHYSFS_platformDeinit */


void __PHYSFS_platformDetectAvailableCDs(PHYSFS_StringCallback cb, void *data)
{
} /* __PHYSFS_platformDetectAvailableCDs */


char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
    char *retval;
    const size_t bufsize = 128;

    retval = allocator.Malloc(bufsize);
    BAIL_IF(!retval, PHYSFS_ERR_OUT_OF_MEMORY, NULL);

    if (getcwd(retval, bufsize - 1))
    {
        /* Make sure the path is slash-terminated */
        size_t length = strlen(retval);
        if (length > 0 && retval[length - 1] != '/')
        {
            retval[length++] = '/';
            retval[length] = '\0';
        }
    }
    else
    {
        strcpy(retval, "/");
    }

    return retval;
} /* __PHYSFS_platformCalcBaseDir */


char *__PHYSFS_platformCalcPrefDir(const char *org, const char *app)
{
    static const char subdir[] = "data/";

    return buildSubdirPath(subdir, sizeof(subdir));
} /* __PHYSFS_platformCalcPrefDir */

/* end of physfs_platform_unix.c ... */

#endif  /* PHYSFS_PLATFORM_OGC */

/* end of physfs_platform_ogc.c ... */

