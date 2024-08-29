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

#ifndef _INCLUDE_PHYSFSSDL3_H_
#define _INCLUDE_PHYSFSSDL3_H_

#include "physfs.h"
#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

/* You can either just get SDL_IOStreams directly from a PhysicsFS path,
   or make an SDL_Storage that bridges to the PhysicsFS VFS. */


/**
 * Open a platform-independent filename for reading, and make it accessible
 *  via an SDL_IOStream. The file will be closed in PhysicsFS when the
 *  RWops is closed. PhysicsFS should be configured to your liking before
 *  opening files through this method.
 *
 *   @param filename File to open in platform-independent notation.
 *  @return A valid SDL_IOStream on success, NULL on error. Specifics
 *           of the error can be gleaned from SDL_GetError().
 */
PHYSFS_DECL SDL_IOStream *PHYSFSSDL3_openRead(const char *fname);

/**
 * Open a platform-independent filename for writing, and make it accessible
 *  via an SDL_IOStream. The file will be closed in PhysicsFS when the
 *  RWops is closed. PhysicsFS should be configured to your liking before
 *  opening files through this method.
 *
 *   @param filename File to open in platform-independent notation.
 *  @return A valid SDL_IOStream on success, NULL on error. Specifics
 *           of the error can be gleaned from SDL_GetError().
 */
PHYSFS_DECL SDL_IOStream *PHYSFSSDL3_openWrite(const char *fname);

/**
 * Open a platform-independent filename for appending, and make it accessible
 *  via an SDL_IOStream. The file will be closed in PhysicsFS when the
 *  RWops is closed. PhysicsFS should be configured to your liking before
 *  opening files through this method.
 *
 *   @param filename File to open in platform-independent notation.
 *  @return A valid SDL_IOStream on success, NULL on error. Specifics
 *           of the error can be gleaned from SDL_GetError().
 */
PHYSFS_DECL SDL_IOStream *PHYSFSSDL3_openAppend(const char *fname);

/**
 * Make a SDL_IOStream from an existing PhysicsFS file handle. You should
 *  dispose of any references to the handle after successful creation of
 *  the RWops. The actual PhysicsFS handle will be destroyed when the
 *  RWops is closed.
 *
 *   @param handle a valid PhysicsFS file handle.
 *  @return A valid SDL_IOStream on success, NULL on error. Specifics
 *           of the error can be gleaned from SDL_GetError().
 */
PHYSFS_DECL SDL_IOStream *PHYSFSSDL3_makeIOStream(PHYSFS_File *handle);

/**
 * Expose the PhysicsFS VFS through an SDL_Storage interface.
 * This just makes the global interpolated file tree available
 * through SDL3's interface. You can still manipulate it outside
 * of SDL_Storage by calling PhysicsFS APIs directly.
 *
 *  @return A valid SDL_Storage on success, NULL on error. Specifics
 *           of the error can be gleaned from SDL_GetError().
 */
PHYSFS_DECL SDL_Storage *PHYSFSSDL3_makeStorage(void);

#ifdef __cplusplus
}
#endif

#endif /* include-once blocker */

/* end of physfssdl3.h ... */

