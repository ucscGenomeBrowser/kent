/* ensFace - stuff to manage interface to Ensembl web site. */

#include "common.h"
#include "dystring.h"
#include "ensFace.h"

static char const rcsid[] = "$Id: ensFace.c,v 1.2 2003/08/12 01:39:52 kate Exp $";

struct stringPair
/* A pair of strings. */
   {
   char *a;	/* One string. */
   char *b;	/* The other string */
   };


/* Special case for fugu -- scientific name was changed
 * in Genbank, and Ensembl hasn't changed */

char *ensOrgName(char *scientificName)
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
{
struct dyString *dy = dyStringNew(0);
int bigStart, bigEnd, smallStart, smallEnd;
int winSize = winEnd - winStart;
bigStart = smallStart = winStart;
bigEnd = smallEnd = winEnd;

if (winSize < 1000000)
    {
    bigStart -= 500000;
    if (bigStart < 0) bigStart = 0;
    bigEnd += 500000;
    if (bigEnd > chromSize) bigEnd = chromSize;
    dyStringPrintf(dy, "http://www.ensembl.org/%s/contigview"
	   "?chr=%s&vc_start=%d&vc_end=%d&wvc_start=%d&wvc_end=%d",
	    ensOrg, chrom, bigStart, bigEnd, smallStart, smallEnd);
    }
else
    {
    dyStringPrintf(dy, "http://www.ensembl.org/%s/contigview"
	   "?chr=%s&vc_start=%d&vc_end=%d",
	    ensOrg, chrom, bigStart, bigEnd);
    }
return dy;
}

