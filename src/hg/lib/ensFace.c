/* ensFace - stuff to manage interface to Ensembl web site. */

#include "common.h"
#include "dystring.h"
#include "ensFace.h"
#include "hCommon.h"

static char const rcsid[] = "$Id: ensFace.c,v 1.7 2008/04/17 20:38:15 hiram Exp $";

struct stringPair
/* A pair of strings. */
   {
   char *a;	/* One string. */
   char *b;	/* The other string */
   };


char *ensOrgNameFromScientificName(char *scientificName)
/* Convert from ucsc to Ensembl organism name.
 * This is scientific name, with underscore replacing spaces
 * Caller must free returned string */
{
    char *p;
    char *res;
    if (scientificName == NULL) 
        return NULL;
    if (sameWord(scientificName, "Takifugu rubripes"))
        {
        /* special case for fugu, whose scientific name
         * has been changed to Takifugu, but not at ensembl */
        return "Fugu_rubripes";
        }
    if (sameWord(scientificName, "Pongo pygmaeus abelii"))
        {
        /* special case for Orangutan, different form of the same
         * scientific name */
        return "Pongo_pygmaeus";
        }
    /* replace spaces with underscores, assume max two spaces
     * (species and sub-species).  */
    res = cloneString(scientificName);
    if ((p = index(res, ' ')) != NULL)
        *p = '_';
    if ((p = rindex(res, ' ')) != NULL)
        *p = '_';
    return res;
}

struct dyString *ensContigViewUrl(
                            char *ensOrg, char *chrom, int chromSize,
                            int winStart, int winEnd, char *archive)
/* Return a URL that will take you to ensembl's contig view. */
/* Not using chromSize.  archive is possibly a date reference */
{
struct dyString *dy = dyStringNew(0);
char *chrName;

if (startsWith("scaffold", chrom))
    chrName = chrom;
else
    chrName = skipChr(chrom);
if (archive)
    dyStringPrintf(dy, 
	   "http://%s.archive.ensembl.org/%s/contigview?chr=%s&start=%d&end=%d",
		    archive, ensOrg, chrName, winStart, winEnd);
else
    dyStringPrintf(dy, 
               "http://www.ensembl.org/%s/contigview?chr=%s&start=%d&end=%d", ensOrg, chrName, winStart, winEnd);
return dy;
}

