/** \file globbing.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "globbing.h"

/**
 * Please see globbing.h for details.
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
 *  Please see the file LICENSE.txt in the source's root directory.
 *
 *  \author Ryan C. Gordon.
 */


static int matchesPattern(const char *fname, const char *wildcard,
                          int caseSensitive)
{
    char x, y;
    const char *fnameptr = fname;
    const char *wildptr = wildcard;

    while ((*wildptr) && (*fnameptr))
    {
        y = *wildptr;
        if (y == '*')
        {
            do
            {
                wildptr++;  /* skip multiple '*' in a row... */
            } while (*wildptr == '*');

            y = (caseSensitive) ? *wildptr : (char) tolower(*wildptr);

            while (1)
            {
                x = (caseSensitive) ? *fnameptr : (char) tolower(*fnameptr);
                if ((!x) || (x == y))
                    break;
                else
                    fnameptr++;
            } /* while */
        } /* if */

        else if (y == '?')
        {
            wildptr++;
            fnameptr++;
        } /* else if */

        else
        {
            if (caseSensitive)
                x = *fnameptr;
            else
            {
                x = tolower(*fnameptr);
                y = tolower(y);
            } /* if */

            wildptr++;
            fnameptr++;

            if (x != y)
                return 0;
        } /* else */
    } /* while */

    while (*wildptr == '*')
        wildptr++;

    return (*fnameptr == *wildptr);
} /* matchesPattern */

typedef struct
{
    const PHYSFS_Allocator *allocator;
    const char *wildcard;
    int caseSensitive;
    PHYSFS_EnumFilesCallback callback;
    void *origData;
} WildcardCallbackData;


/*
 * This callback sits between the enumerator and the enduser callback,
 *  filtering out files that don't match the wildcard pattern.
 */
static void wildcardCallback(void *_d, const char *origdir, const char *fname)
{
    const WildcardCallbackData *data = (const WildcardCallbackData *) _d;
    if (matchesPattern(fname, data->wildcard, data->caseSensitive))
        data->callback(data->origData, origdir, fname);
} /* wildcardCallback */


void PHYSFSEXT_enumerateFilesCallbackWildcard(const char *dir,
                                              const char *wildcard,
                                              int caseSensitive,
                                              PHYSFS_EnumFilesCallback c,
                                              void *d)
{
    WildcardCallbackData data;
    data.allocator = PHYSFS_getAllocator();
    data.wildcard = wildcard;
    data.caseSensitive = caseSensitive;
    data.callback = c;
    data.origData = d;
    PHYSFS_enumerateFilesCallback(dir, wildcardCallback, &data);
} /* PHYSFSEXT_enumerateFilesCallbackWildcard */


void PHYSFSEXT_freeEnumeration(char **list)
{
    const PHYSFS_Allocator *allocator = PHYSFS_getAllocator();
    int i;
    if (list != NULL)
    {
        for (i = 0; list[i] != NULL; i++)
            allocator->Free(list[i]);
        allocator->Free(list);
    } /* if */
} /* PHYSFSEXT_freeEnumeration */


char **PHYSFSEXT_enumerateFilesWildcard(const char *dir, const char *wildcard,
                                        int caseSensitive)
{
    const PHYSFS_Allocator *allocator = PHYSFS_getAllocator();
    char **list = PHYSFS_enumerateFiles(dir);
    char **retval = NULL;
    int totalmatches = 0;
    int matches = 0;
    char **i;

    for (i = list; *i != NULL; i++)
    {
        #if 0
        printf("matchesPattern: '%s' vs '%s' (%s) ... %s\n", *i, wildcard,
               caseSensitive ? "case" : "nocase",
               matchesPattern(*i, wildcard, caseSensitive) ? "true" : "false");
        #endif
        if (matchesPattern(*i, wildcard, caseSensitive))
            totalmatches++;
    } /* for */

    retval = (char **) allocator->Malloc(sizeof (char *) * (totalmatches+1));
    if (retval != NULL)
    {
        for (i = list; ((matches < totalmatches) && (*i != NULL)); i++)
        {
            if (matchesPattern(*i, wildcard, caseSensitive))
            {
                retval[matches] = (char *) allocator->Malloc(strlen(*i) + 1);
                if (retval[matches] == NULL)
                {
                    while (matches--)
                        allocator->Free(retval[matches]);
                    allocator->Free(retval);
                    retval = NULL;
                    break;
                } /* if */
                strcpy(retval[matches], *i);
                matches++;
            } /* if */
        } /* for */

        if (retval != NULL)
        {
            assert(totalmatches == matches);
            retval[matches] = NULL;
        } /* if */
    } /* if */

    PHYSFS_freeList(list);
    return retval;
} /* PHYSFSEXT_enumerateFilesWildcard */


#ifdef TEST_PHYSFSEXT_ENUMERATEFILESWILDCARD
int main(int argc, char **argv)
{
    int rc;
    char **flist;
    char **i;

    if (argc != 3)
    {
        printf("USAGE: %s <pattern> <caseSen>\n"
               "   where <caseSen> is 1 or 0.\n", argv[0]);
        return 1;
    } /* if */

    if (!PHYSFS_init(argv[0]))
    {
        fprintf(stderr, "PHYSFS_init(): %s\n", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
        return 1;
    } /* if */

    if (!PHYSFS_addToSearchPath(".", 1))
    {
        fprintf(stderr, "PHYSFS_addToSearchPath(): %s\n", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
        PHYSFS_deinit();
        return 1;
    } /* if */

    flist = PHYSFSEXT_enumerateFilesWildcard("/", argv[1], atoi(argv[2]));
    rc = 0;
    for (i = flist; *i; i++)
    {
        printf("%s\n", *i);
        rc++;
    } /* for */
    printf("\n  total %d files.\n\n", rc);

    PHYSFSEXT_freeEnumeration(flist);
    PHYSFS_deinit();

    return 0;
} /* main */
#endif

/* end of globbing.c ... */

