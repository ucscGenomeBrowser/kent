/* ensFace - stuff to manage interface to Ensembl web site. */

#include "common.h"
#include "dystring.h"
#include "ensFace.h"

static char const rcsid[] = "$Id: ensFace.c,v 1.1 2003/06/18 03:26:02 kent Exp $";

struct stringPair
/* A pair of strings. */
   {
   char *a;	/* One string. */
   char *b;	/* The other string */
   };

static struct stringPair ucscEnsConvTable[] = {
    {"human", "Homo_sapiens"},
    {"mouse", "Mus_musculus"},
    {"rat", "Rattus_norvegicus"},
};

char *ensOrgName(char *ucscOrgName)
/* Convert from ucsc to Ensembl organism name */
{
int i;
for (i=0; i<ArraySize(ucscEnsConvTable); ++i)
    {
    if (sameWord(ucscEnsConvTable[i].a, ucscOrgName))
        return ucscEnsConvTable[i].b;
    }
return NULL;
}

struct dyString *ensContigViewUrl(char *ensOrg, char *chrom, int chromSize,
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
