#ifndef INCL_PHYSFSEXT_GLOBBING_H
#define INCL_PHYSFSEXT_GLOBBING_H

/** \file globbing.h */

#include "physfs.h"

/**
 * \mainpage PhysicsFS globbing
 *
 * This is an extension to PhysicsFS to let you search for files with basic
 *  wildcard matching, regardless of what sort of filesystem or archive they
 *  reside in. It does this by enumerating directories as needed and manually
 *  locating matching entries.
 *
 * Usage: Set up PhysicsFS as you normally would, then use
 *  PHYSFSEXT_enumerateFilesWildcard() when enumerating files. This is just
 *  like PHYSFS_enumerateFiles(), but it returns a subset that matches your
 *  wildcard pattern. You must call PHYSFSEXT_freeEnumeration() on the results,
 *  just PHYSFS_enumerateFiles() would do with PHYSFS_freeList().
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
 *  Please see LICENSE.txt in the source's "docs" directory.
 *
 *  \author Ryan C. Gordon.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \fn char **PHYSFS_enumerateFilesWildcard(const char *dir, const char *wildcard, int caseSensitive)
 * \brief Get a file listing of a search path's directory, filtered with a wildcard pattern.
 *
 * Matching directories are interpolated. That is, if "C:\mydir" is in the
 *  search path and contains a directory "savegames" that contains "x.sav",
 *  "y.Sav", and "z.txt", and there is also a "C:\userdir" in the search path
 *  that has a "savegames" subdirectory with "w.sav", then the following code:
 *
 * \code
 * char **rc = PHYSFS_enumerateFilesWildcard("savegames", "*.sav", 0);
 * char **i;
 *
 * for (i = rc; *i != NULL; i++)
 *     printf(" * We've got [%s].\n", *i);
 *
 * PHYSFS_freeList(rc);
 * \endcode
 *
 *  ...will print:
 *
 * \verbatim
 * We've got [x.sav].
 * We've got [y.Sav].
 * We've got [w.sav].\endverbatim
 *
 * Feel free to sort the list however you like. We only promise there will
 *  be no duplicates, but not what order the final list will come back in.
 *
 * Wildcard strings can use the '*' and '?' characters, currently.
 * Matches can be case-insensitive if you pass a zero for argument 3.
 *
 * Don't forget to call PHYSFSEXT_freeEnumerator() with the return value from
 *  this function when you are done with it. As we use PhysicsFS's allocator
 *  for this list, you must free it before calling PHYSFS_deinit().
 *  Do not use PHYSFS_freeList() on the returned value!
 *
 *    \param dir directory in platform-independent notation to enumerate.
 *    \param wildcard Wildcard pattern to use for filtering.
 *    \param caseSensitive Zero for case-insensitive matching,
 *                         non-zero for case-sensitive.
 *   \return Null-terminated array of null-terminated strings.
 *
 * \sa PHYSFSEXT_freeEnumeration
 */
PHYSFS_DECL char **PHYSFSEXT_enumerateFilesWildcard(const char *dir,
                                                   const char *wildcard,
                                                   int caseSensitive);

/**
 * \fn void PHYSFSEXT_freeEnumeration(char **list)
 * \brief Free data returned by PHYSFSEXT_enumerateFilesWildcard
 *
 * Conceptually, this works like PHYSFS_freeList(), but is used with data
 *  returned by PHYSFSEXT_enumerateFilesWildcard() only. Be sure to call this
 *  on any returned data from that function before
 *
 *    \param list Pointer previously returned by
 *                PHYSFSEXT_enumerateFilesWildcard(). It is safe to pass a
 *                NULL here.
 *
 * \sa PHYSFSEXT_enumerateFilesWildcard
 */
PHYSFS_DECL void PHYSFSEXT_freeEnumeration(char **list);


/**
 * \fn void PHYSFSEXT_enumerateFilesCallbackWildcard(const char *dir, const char *wildcard, int caseSensitive, PHYSFS_EnumFilesCallback c, void *d);
 * \brief Get a file listing of a search path's directory, filtered with a wildcard pattern, using an application-defined callback.
 *
 * This function is equivalent to PHYSFSEXT_enumerateFilesWildcard(). It
 *  reports file listings, filtered by a wildcard pattern.
 *
 * Unlike PHYSFS_enumerateFiles(), this function does not return an array.
 *  Rather, it calls a function specified by the application once per
 *  element of the search path:
 *
 * \code
 *
 * static void printDir(void *data, const char *origdir, const char *fname)
 * {
 *     printf(" * We've got [%s] in [%s].\n", fname, origdir);
 * }
 *
 * // ...
 * PHYSFS_enumerateFilesCallbackWildcard("savegames","*.sav",0,printDir,NULL);
 * \endcode
 *
 * Items sent to the callback are not guaranteed to be in any order whatsoever.
 *  There is no sorting done at this level, and if you need that, you should
 *  probably use PHYSFS_enumerateFilesWildcard() instead, which guarantees
 *  alphabetical sorting. This form reports whatever is discovered in each
 *  archive before moving on to the next. Even within one archive, we can't
 *  guarantee what order it will discover data. <em>Any sorting you find in
 *  these callbacks is just pure luck. Do not rely on it.</em> As this walks
 *  the entire list of archives, you may receive duplicate filenames.
 *
 * Wildcard strings can use the '*' and '?' characters, currently.
 * Matches can be case-insensitive if you pass a zero for argument 3.
 *
 *    \param dir Directory, in platform-independent notation, to enumerate.
 *    \param wildcard Wildcard pattern to use for filtering.
 *    \param caseSensitive Zero for case-insensitive matching,
 *                         non-zero for case-sensitive.
 *    \param c Callback function to notify about search path elements.
 *    \param d Application-defined data passed to callback. Can be NULL.
 *
 * \sa PHYSFS_EnumFilesCallback
 * \sa PHYSFS_enumerateFiles
 */
PHYSFS_DECL void PHYSFSEXT_enumerateFilesCallbackWildcard(const char *dir,
                                              const char *wildcard,
                                              int caseSensitive,
                                              PHYSFS_EnumFilesCallback c,
                                              void *d);

#ifdef __cplusplus
}
#endif

#endif  /* include-once blocker. */

/* end of globbing.h ... */

