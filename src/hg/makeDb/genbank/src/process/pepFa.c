/* copying the peptide fa and save info to store in ra */
#include "common.h"
#include "pepFa.h"
#include "hash.h"
#include "linefile.h"
#include "errabort.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "gbFa.h"
#include "localmem.h"
#include "keys.h"
#include "gbFileOps.h"

static char const rcsid[] = "$Id: pepFa.c,v 1.1 2003/06/03 01:27:48 markd Exp $";

struct pepFaInfo
/* object used to hold info abourt a peptide seq */
{
    char* acc;         /* peptide accession */
    unsigned size;     /* sequecne size */
    off_t faOff;       /* offset in file */
    unsigned faSize;   /* size of record, including header */
};
static struct hash *pepHash = NULL;

/* buffers for kvt value */
static char sizeBuf[32];
static char faOffBuf[64];
static char faSizeBuf[32];


static char *unburyAcc(struct gbFa* fa)
/* Return accession given a long style NCBI name.  Check to see if
 * the accession is NP_* and references an NM_* gene, like:
 *   >gi|6382060|ref|NP_005149.2| (NM_005158) v-abl Abelson murine ..
 * Returns null on sequences that should  be skipped.
 * Cannibalizes the longNcbiName. */
{
char *parts[4];
int partCount;

partCount = chopByChar(fa->id, '|', parts, ArraySize(parts));
if (partCount < 4) 
    errAbort("Badly formed longNcbiName: %s:%s\n", fa->fileName,
             fa->recLineNum);
if (!(startsWith("NP_", parts[3]) && startsWith("(NM_", fa->comment)))
    return NULL;
return parts[3];
}

static void saveInfo(struct hash* pepHash, char *acc, unsigned size,
                     off_t faOff, unsigned faSize)
/* save information about a peptide seq */
{
struct pepFaInfo* info;
struct hashEl* hel;
lmAllocVar(pepHash->lm, info);

hel = hashAdd(pepHash, acc, info);
info->acc = hel->name;
info->size = size;
info->faOff = faOff;
info->faSize = faSize;
}

static boolean procSeq(struct gbFa *inPepFa, struct gbFa *outPepFa)
/* process a sequence in a fasta file. */
{
char *pepAcc;

if (!gbFaReadNext(inPepFa))
    return FALSE;
/* if this a protein acc and has a gene reference, write with protein acc */
pepAcc = unburyAcc(inPepFa);
if (pepAcc != NULL)
    {
    gbFaWriteFromFa(outPepFa, inPepFa, pepAcc);
    saveInfo(pepHash, pepAcc, strlen(gbFaGetSeq(inPepFa)), outPepFa->recOff,
             (outPepFa->off-outPepFa->recOff));
    }
return TRUE;
}

void pepFaProcess(char* inPepFile, char* outPepFile)
/* process the peptide fa, build hash of seqs to fill in ra, copying only the
 * NP_* sequence with references to NM_*.
 */
{
struct gbFa* inPepFa = gbFaOpen(inPepFile, "r");
struct gbFa* outPepFa = gbFaOpen(outPepFile, "w");

if (pepHash == NULL)
    pepHash = hashNew(14);

while(procSeq(inPepFa, outPepFa))
    continue;
gbFaClose(&outPepFa);
gbFaClose(&inPepFa);
}

void pepFaGetInfo(char* pepAcc, struct kvt* kvt)
/* Add peptide info to the kvt for the accession, if it exists */
{
struct pepFaInfo* info;
if ((pepHash != NULL) && ((info = hashFindVal(pepHash, pepAcc)) != NULL))
    {
    safef(sizeBuf, sizeof(sizeBuf), "%u", info->size);
    kvtAdd(kvt, "prs", sizeBuf);
    safef(faOffBuf, sizeof(faOffBuf), "%lld", info->faOff);
    kvtAdd(kvt, "pfo", faOffBuf);
    safef(faSizeBuf, sizeof(faSizeBuf), "%u", info->faSize);
    kvtAdd(kvt, "pfs", faSizeBuf);
    }
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

