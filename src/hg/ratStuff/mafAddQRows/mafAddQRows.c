/* mafAddQRows - Add qualiy data to a maf.                                   */
/*                                                                           */
/* For each species with quality data we want to add to the maf, the quality */
/* data is stored in an external qac file which contains quality data for    */
/* each chromosome.  We additionally build an index (qdx file) which allows  */
/* us to seek directly to quality data for a particular chromosome in the    */
/* qac file.                                                                 */
/*                                                                           */
/* We build a hash containing data for each species with quality data.       */
/* The hash contains the name of the species, an open file descriptor to     */
/* the appropriate qac file, a hash mapping chromosome to file offset in     */
/* the qac file, and a qaSeq structure for holding quality data.  The qaSeq  */
/* structure acts as a one element cache, holding quality data for the most  */
/* recently requested chromosome.                                            */
/*                                                                           */
/* As we are generally interested in bases with low quality, the quality     */
/* data in the maf is a compressed version of the actual quality data.       */
/* The quality data in the maf is:                                           */
/*                                                                           */
/*     min( floor(actualy quality value/5), 9)                               */
/*                                                                           */
/* This allows us to show more of the low-quality values.  The relationship  */
/* between quality characters in the maf and the actualy quality value are   */
/* summarized in the following table:                                        */
/*                                                                           */
/*     .: In Gap Q == FAKE_GAP_QUAL                                          */
/*     0: 0 <= Q < 5 || Q == 98                                              */
/*     1: 5 <= Q < 10                                                        */
/*     2: 10 <= Q < 15                                                       */
/*     3: 15 <= Q < 20                                                       */
/*     4: 20 <= Q < 25                                                       */
/*     5: 25 <= Q < 30                                                       */
/*     6: 30 <= Q < 35                                                       */
/*     7: 35 <= Q < 40                                                       */
/*     8: 40 <= Q < 45                                                       */
/*     9: 45 <= Q < 98                                                       */
/*     F: Q == 99                                                            */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "qaSeq.h"
#include "maf.h"
#include "math.h"

/* fake quality value use to indicate gaps */
#define FAKE_GAP_QUAL 100

struct indexEntry
/* map chromosome name to file offset in a qac file */
    {
    char *name;         /* name of the chrom */
    off_t offset;       /* offset in the qac file */
    };

struct qData
/* metadata used to locate quality data for a given species,chrom */
    {
    char *name;         /* species name */
    FILE *fd;           /* file descriptor of qac file */
    boolean isSwapped;  /* is the qac file swapped? */
    struct hash *index; /* hash mapping chrom to indexEntry and hence offset in qac file */
    struct qaSeq *qa;   /* cache to avoid reloading quality data */
    };

static void usage()
/* Explain usage and exit. */
{
errAbort(
    "mafAddQRows - Add quality data to a maf\n"
    "usage:\n"
    "   mafAddQRows in.maf out.maf <list of species>\n"
    "   assumes species.qac, species.qdx exist in the current directory\n"
);
}

static struct hash *loadQacIndex(char *species)
/* Buid a hash mapping chrom to file offset in a qac file for the */
/* argument species.                                              */
{
char buffer[1024];
FILE *qdx;
int seqCount;
struct hash *qacIndex;
char seq_name[1024];
off_t offset;
struct hashEl *hel;
struct indexEntry *ie;

safef(buffer, sizeof(buffer), "%s.qdx", species);
qdx = mustOpen(buffer, "rb");

/* get the count of sequences in the qac file */
if (fgets(buffer, sizeof(buffer), qdx) == NULL)
    errnoAbort("Error reading %s quality index\n", species);
if (sscanf(buffer, "%d", &seqCount) != 1)
    errAbort("Can't get sequence count from %s quality index\n", species);

/* build an populate a hash mapping chrom to file offset */
qacIndex = newHash(digitsBaseTwo((unsigned long) seqCount));
while (fgets(buffer, sizeof(buffer), qdx) != NULL)
    {
    if (sscanf(buffer, "%s %ld", seq_name, &offset) != 2)
        errAbort("Can't parse %s quality index line:\n%s\n", species, buffer);

    if ((hel = hashLookup(qacIndex, seq_name)) == NULL)
        {
        AllocVar(ie);
        ie->name = cloneString(seq_name);
        ie->offset = offset;
        hashAdd(qacIndex, ie->name, ie);
        seqCount--;
        }
    }
if (ferror(qdx))
    errnoAbort("Error reading %s quality index\n", species);

/* sanity check */
if (seqCount != 0)
    errAbort("%s quality index is inconsistent\n", species);

carefulClose(&qdx);
return qacIndex;
}

static struct qData *qDataLoad(char *species)
/* Populate a qData struct for the named species. */
{
char buffer[1024];
struct qData *qd;

AllocVar(qd);
qd->name = cloneString(species);

safef(buffer, sizeof(buffer), "%s.qac", species);
qd->fd = qacOpenVerify(buffer, &(qd->isSwapped));

qd->index = loadQacIndex(species);
qd->qa = NULL;

return qd;
}

static struct hash *buildSpeciesHash(int argc, char *argv[])
/* Build a hash keyed on species name.  Each species we are adding "q" */
/* lines for will have an entry in this hash.  The hash contains:      */
/*   1) species name [hash key]                                        */
/*   2) file descriptor for qac file (will be opened)                  */
/*   3) flag telling us if the qac file above needs to be swapped      */
/*   4) hash mapping chrom name to an indexEntry                       */
/*   5) a small 1-element cache for holding quality data               */
{
struct hash *speciesHash;
struct hashEl *hel;
struct qData *qd;
int i;

speciesHash = newHash(digitsBaseTwo((unsigned long) argc - 3));

for (i = 3; i < argc; i++)
    {
    /* skip over duplicate species given on the command line */
    if ((hel = hashLookup(speciesHash, argv[i])) == NULL)
        {
        qd = qDataLoad(argv[i]);
        hashAdd(speciesHash, qd->name, qd);
        }
    }

return speciesHash;
}

static struct qData *loadQualityData (struct mafComp *mc, struct hash *speciesHash)
/* return qData if found or NULL if not */
{
char buffer[1024];
char *species, *chrom;
struct hashEl *hel;
struct qData *qd;
struct indexEntry *ie;

/* separate species and chrom */
strncpy(buffer, mc->src, sizeof(buffer));
species = chrom = buffer;
if ((chrom = strchr(buffer, '.')) == NULL)
    errAbort("can't find chrom for %s\n", buffer);
*chrom++ = '\0';

/* return if we don't have quality data for this species */
if ((hel = hashLookup(speciesHash, species)) == NULL)
    return(NULL);

/* load the quality data if we need to */
qd = (struct qData *) hel->val;
if (qd->qa == NULL || ! sameString(qd->qa->name, chrom))
    {
    if ((hel = hashLookup(qd->index, chrom)) == NULL)
        return(NULL);
    ie = (struct indexEntry *) hel->val;
    if (fseeko(qd->fd, ie->offset, SEEK_SET) == (off_t) -1)
        errnoAbort("fseeko() failed\n");
    if (qd->qa != NULL)
        qaSeqFree(&(qd->qa));
    qd->qa = qacReadNext(qd->fd, qd->isSwapped);
    }

return qd;
}

static char convertToQChar(UBYTE *q)
/* convert a q value into the appropriate character */
{
int val;

switch (*q)
    {
    case FAKE_GAP_QUAL:
        return('.');
        break;
    case 99:
        return('F');
        break;
    case 98:
        return('0');
        break;
    default:
        val = (int) floor((double) *q / 5);
        val = (val < 9) ? val : 9;
    }

return '0' + val;
}

void mafAddQRows(char *inMaf, char *outMaf, struct hash *speciesHash)
/* mafAddQRows - Add quality data to a maf. */
{
struct mafFile *mf;
struct mafAli *ma;
struct mafComp *mc;
struct qData *qd;
char *quality, *qp;
FILE *outf = mustOpen(outMaf, "w");
UBYTE *q;
int inc = 1;

mf = mafOpen(inMaf);
mafWriteStart(outf, mf->scoring);

while ((ma = mafNext(mf)) != NULL)
    {
    for (mc = ma->components; mc != NULL; mc = mc->next)
        {
        /* if this is not an "e" row, and we have quality data for this species */
        if (mc->size != 0 && (qd = loadQualityData(mc, speciesHash)) != NULL)
            {
            quality = cloneString(mc->text);
            qp = mc->quality = quality;

            /* set a pointer to the beginning of the quality data in memory */
            if (mc->strand == '-')
                {
                q = qd->qa->qa + qd->qa->size - 1 - mc->start;
                inc = -1;
                }
            else
                {
                q = qd->qa->qa + mc->start;
                inc = +1;
                }

            /* replace non-gap characters with the corresponding quality value */
            while (*qp != '\0')
                {
                if (*qp == '-')
                    {
                    qp++;
                    continue;
                    }
                *qp++ = convertToQChar(q);
                q += inc;
                }
            }
        }

    mafWrite(outf, ma);
    mafAliFree(&ma);
    }

carefulClose(&outf);
}

int main(int argc, char *argv[])
{
struct hash *speciesHash;

if (argc < 4)
    usage();

speciesHash = buildSpeciesHash(argc, argv);
mafAddQRows(argv[1], argv[2], speciesHash);

return EXIT_SUCCESS;
}
