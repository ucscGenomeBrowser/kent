/* ensFace - stuff to manage interface to Ensembl web site. */

#include "common.h"
#include "dystring.h"
#include "ensFace.h"
#include "hCommon.h"

static char const rcsid[] = "$Id: ensFace.c,v 1.5 2005/12/17 00:51:57 hartera Exp $";

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
                            int winStart, int winEnd)
/* Return a URL that will take you to ensembl's contig view. */
/* Not using chromSize. */
{
struct dyString *dy = dyStringNew(0);
char *chrName;

if (startsWith("scaffold", chrom))
    chrName = chrom;
else
    chrName = skipChr(chrom);
dyStringPrintf(dy, 
               "http://www.ensembl.org/%s/contigview?chr=%s&start=%d&end=%d", ensOrg, chrName, winStart, winEnd);
return dy;
}

