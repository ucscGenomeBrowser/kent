/* hic.c contains a few helpful wrapper functions for managing Hi-C data. */

#include "common.h"
#include "linefile.h"
#include "dystring.h"
#include "jksql.h"
#include "hic.h"
#include "hdb.h"
#include "trackHub.h"
#include "Cstraw.h"
#include "hash.h"
#include "chromAlias.h"

char *hicLoadHeader(char *filename, struct hicMeta **header)
/* Create a hicMeta structure for the supplied Hi-C file.  If
 * the return value is non-NULL, it points to a string containing
 * an error message that explains why the retrieval failed. */
{
char *genome;
char **chromosomes, **bpResolutions;
int nChroms, nBpRes;

char *errMsg = CstrawHeader(filename, &genome, &chromosomes, &nChroms, &bpResolutions, &nBpRes, NULL, NULL);
if (errMsg != NULL)
    return errMsg;

struct hicMeta *newMeta = NULL;
AllocVar(newMeta);
newMeta->assembly = genome;
newMeta->nRes = nBpRes;
newMeta->resolutions = bpResolutions;
newMeta->nChroms = nChroms;
newMeta->chromNames = chromosomes;
newMeta->ucscToAlias = NULL;

if (!trackHubDatabase(genome))
    {
    struct hash *aliasToUcsc = chromAliasMakeLookupTable(newMeta->assembly);
    if (aliasToUcsc != NULL)
        {
        struct hash *ucscToAlias = newHash(0);
        int i;
        for (i=0; i<nChroms; i++)
            {
            struct chromAlias *cA = hashFindVal(aliasToUcsc, chromosomes[i]);
            if (cA != NULL)
                {
                hashAdd(ucscToAlias, cA->chrom, cloneString(chromosomes[i]));
                }
            }
        newMeta->ucscToAlias = ucscToAlias;
        hashFree(&aliasToUcsc);
        }
    }
*header = newMeta;
return NULL;
}
