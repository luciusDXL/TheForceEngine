/*
 * This code provides a glue layer between PhysicsFS and Simple Directmedia
 *  Layer 3's (SDL3) SDL_IOStream and SDL_Storage i/o abstractions.
 *
 * License: this code is public domain. I make no warranty that it is useful,
 *  correct, harmless, or environmentally safe.
 *
 * This particular file may be used however you like, including copying it
 *  verbatim into a closed-source project, exploiting it commercially, and
 *  removing any trace of my name from the source (although I hope you won't
 *  do that). I welcome enhancements and corrections to this file, but I do
 *  not require you to send me patches if you make changes. This code has
 *  NO WARRANTY.
 *
 * Unless otherwise stated, the rest of PhysicsFS falls under the zlib license.
 *  Please see LICENSE.txt in the root of the source tree.
 *
 * SDL3 is zlib-licensed, like PhysicsFS. You can get SDL at
 *  https://www.libsdl.org/
 *
 *  This file was written by Ryan C. Gordon. (icculus@icculus.org).
 */

/* This works with SDL3. For SDL1 and SDL2, use physfsrwops.h */

#include "physfssdl3.h"

/* SDL_IOStream -> PhysicsFS bridge ... */

static Sint64 SDLCALL physfsiostream_size(void *userdata)
{
    return (Sint64) PHYSFS_fileLength((PHYSFS_File *) userdata);
}

static Sint64 SDLCALL physfsiostream_seek(void *userdata, Sint64 offset, int whence)
{
    PHYSFS_File *handle = (PHYSFS_File *) userdata;
    PHYSFS_sint64 pos = 0;

    if (whence == SDL_IO_SEEK_SET) {
        pos = (PHYSFS_sint64) offset;
    } else if (whence == SDL_IO_SEEK_CUR) {
        const PHYSFS_sint64 current = PHYSFS_tell(handle);
        if (current == -1) {
            return SDL_SetError("Can't find position in file: %s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
        }

        if (offset == 0) {  /* this is a "tell" call. We're done. */
            return (Sint64) current;
        }

        pos = current + ((PHYSFS_sint64) offset);
    } else if (whence == SDL_IO_SEEK_END) {
        const PHYSFS_sint64 len = PHYSFS_fileLength(handle);
        if (len == -1) {
            return SDL_SetError("Can't find end of file: %s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
        }

        pos = len + ((PHYSFS_sint64) offset);
    } else {
        return SDL_SetError("Invalid 'whence' parameter.");
    }

    if ( pos < 0 ) {
        return SDL_SetError("Attempt to seek past start of file.");
    }
    
    if (!PHYSFS_seek(handle, (PHYSFS_uint64) pos)) {
        return SDL_SetError("PhysicsFS error: %s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
    }

    return (Sint64) pos;
}

static size_t SDLCALL physfsiostream_read(void *userdata, void *ptr, size_t size, SDL_IOStatus *status)
{
    PHYSFS_File *handle = (PHYSFS_File *) userdata;
    const PHYSFS_uint64 readlen = (PHYSFS_uint64) size;
    const PHYSFS_sint64 rc = PHYSFS_readBytes(handle, ptr, readlen);
    if (rc != ((PHYSFS_sint64) readlen)) {
        if (!PHYSFS_eof(handle)) {  /* not EOF? Must be an error. */
            /* Setting an SDL error makes SDL take care of `status` for you. */
            SDL_SetError("PhysicsFS error: %s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
            return 0;
        }
    }
    return (size_t) rc;
}

static size_t SDLCALL physfsiostream_write(void *userdata, const void *ptr, size_t size, SDL_IOStatus *status)
{
    PHYSFS_File *handle = (PHYSFS_File *) userdata;
    const PHYSFS_uint64 writelen = (PHYSFS_uint64) size;
    const PHYSFS_sint64 rc = PHYSFS_writeBytes(handle, ptr, writelen);
    if (rc != ((PHYSFS_sint64) writelen)) {
        /* Setting an SDL error makes SDL take care of `status` for you. */
        SDL_SetError("PhysicsFS error: %s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
    }
    return (size_t) rc;
}

static int SDLCALL physfsiostream_close(void *userdata)
{
    if (!PHYSFS_close((PHYSFS_File *) userdata)) {
        return SDL_SetError("PhysicsFS error: %s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
    }
    return 0;
}

static SDL_IOStream *create_iostream(PHYSFS_File *handle)
{
    SDL_IOStream *retval = NULL;
    if (handle == NULL) {
        SDL_SetError("PhysicsFS error: %s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
    } else {
        SDL_IOStreamInterface iface;
        iface.size  = physfsiostream_size;
        iface.seek  = physfsiostream_seek;
        iface.read  = physfsiostream_read;
        iface.write = physfsiostream_write;
        iface.close = physfsiostream_close;
        retval = SDL_OpenIO(&iface, handle);
    }

    return retval;
}

SDL_IOStream *PHYSFSSDL3_makeIOStream(PHYSFS_File *handle)
{
    SDL_IOStream *retval = NULL;
    if (handle == NULL) {
        SDL_SetError("NULL pointer passed to PHYSFSSDL3_makeRWops().");
    } else {
        retval = create_iostream(handle);
    }
    return retval;
}

SDL_IOStream *PHYSFSSDL3_openRead(const char *fname)
{
    return create_iostream(PHYSFS_openRead(fname));
}

SDL_IOStream *PHYSFSSDL3_openWrite(const char *fname)
{
    return create_iostream(PHYSFS_openWrite(fname));
}

SDL_IOStream *PHYSFSSDL3_openAppend(const char *fname)
{
    return create_iostream(PHYSFS_openAppend(fname));
}


/* SDL_Storage -> PhysicsFS bridge ... */

static int SDLCALL physfssdl3storage_close(void *userdata)
{
    return 0;  /* this doesn't do anything, we didn't allocate anything specific to this object. */
}

static SDL_bool SDLCALL physfssdl3storage_ready(void *userdata)
{
    return SDL_TRUE;
}

/* this is a really obnoxious symbol name... */
typedef struct physfssdl3storage_enumerate_callback_data
{
    SDL_EnumerateDirectoryCallback sdlcallback;
    void *sdluserdata;
} physfssdl3storage_enumerate_callback_data;

static PHYSFS_EnumerateCallbackResult physfssdl3storage_enumerate_callback(void *userdata, const char *origdir, const char *fname)
{
    const physfssdl3storage_enumerate_callback_data *data = (physfssdl3storage_enumerate_callback_data *) userdata;
    const int rc = data->sdlcallback(data->sdluserdata, origdir, fname);
    if (rc < 0) {
        return PHYSFS_ENUM_ERROR;
    } else if (rc == 0) {
        return PHYSFS_ENUM_STOP;
    }
    return PHYSFS_ENUM_OK;
}

static int SDLCALL physfssdl3storage_enumerate(void *userdata, const char *path, SDL_EnumerateDirectoryCallback callback, void *callback_userdata)
{
    physfssdl3storage_enumerate_callback_data data;
    data.sdlcallback = callback;
    data.sdluserdata = callback_userdata;
    return PHYSFS_enumerate(path, physfssdl3storage_enumerate_callback, &data) ? 0 : -1;
}

static int SDLCALL physfssdl3storage_info(void *userdata, const char *path, SDL_PathInfo *info)
{
    PHYSFS_Stat statbuf;
    if (!PHYSFS_stat(path, &statbuf)) {
        return SDL_SetError("%s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
    }

    if (info) {
        switch (statbuf.filetype) {
            case PHYSFS_FILETYPE_REGULAR: info->type = SDL_PATHTYPE_FILE; break;
            case PHYSFS_FILETYPE_DIRECTORY: info->type = SDL_PATHTYPE_DIRECTORY; break;
            default: info->type = SDL_PATHTYPE_OTHER; break;
        }

        info->size = (Uint64) statbuf.filesize;
        info->create_time = (SDL_Time) ((statbuf.createtime < 0) ? 0 : SDL_SECONDS_TO_NS(statbuf.createtime));
        info->modify_time = (SDL_Time) ((statbuf.modtime < 0) ? 0 : SDL_SECONDS_TO_NS(statbuf.modtime));
        info->access_time = (SDL_Time) ((statbuf.accesstime < 0) ? 0 : SDL_SECONDS_TO_NS(statbuf.accesstime));
    }

    return 0;
}

static int SDLCALL physfssdl3storage_read_file(void *userdata, const char *path, void *destination, Uint64 length)
{
    PHYSFS_file *f = PHYSFS_openRead(path);
    PHYSFS_sint64 br;

    if (!f) {
        return SDL_SetError("%s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
    }

    br = PHYSFS_readBytes(f, destination, length);
    PHYSFS_close(f);

    if (br != (Sint64) length) {
        return SDL_SetError("%s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
    }
    return 0;
}


static int SDLCALL physfssdl3storage_write_file(void *userdata, const char *path, const void *source, Uint64 length)
{
    PHYSFS_file *f = PHYSFS_openWrite(path);
    PHYSFS_sint64 bw;

    if (!f) {
        return SDL_SetError("%s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
    }

    bw = PHYSFS_writeBytes(f, source, length);
    PHYSFS_close(f);

    if (bw != (Sint64) length) {
        return SDL_SetError("%s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
    }
    return 0;
}

static int SDLCALL physfssdl3storage_mkdir(void *userdata, const char *path)
{
    if (!PHYSFS_mkdir(path)) {
        return SDL_SetError("%s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
    }
    return 0;

}

static int SDLCALL physfssdl3storage_remove(void *userdata, const char *path)
{
    if (!PHYSFS_delete(path)) {
        return SDL_SetError("%s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
    }
    return 0;
}

static int SDLCALL physfssdl3storage_rename(void *userdata, const char *oldpath, const char *newpath)
{
    return SDL_Unsupported();  /* no rename operation in PhysicsFS. */
}

static Uint64 SDLCALL physfssdl3storage_space_remaining(void *userdata)
{
    return SDL_MAX_UINT64;  /* we don't track this in PhysicsFS. */
}

SDL_Storage *PHYSFSSDL3_makeStorage(void)
{
    SDL_StorageInterface iface;
    iface.close = physfssdl3storage_close;
    iface.ready = physfssdl3storage_ready;
    iface.enumerate = physfssdl3storage_enumerate;
    iface.info = physfssdl3storage_info;
    iface.read_file = physfssdl3storage_read_file;
    iface.write_file = physfssdl3storage_write_file;
    iface.mkdir = physfssdl3storage_mkdir;
    iface.remove = physfssdl3storage_remove;
    iface.rename = physfssdl3storage_rename;
    iface.space_remaining = physfssdl3storage_space_remaining;
    return SDL_OpenStorage(&iface, NULL);
}

/* end of physfssdl3.c ... */

