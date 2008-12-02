/* mafAddQRows - Add quality data to a maf.                                  */
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

static double divisor;

struct speciesList 
/* list of species for which we are adding quality data */
    {
    struct speciesList *next;
    char *name;
    char *dir;
    };

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

static struct optionSpec options[] = {
	{"divisor", OPTION_DOUBLE},
	{NULL, 0}
};


static void usage()
/* Explain usage and exit. */
{
errAbort(
    "mafAddQRows - Add quality data to a maf\n"
    "usage:\n"
    "   mafAddQRows species.lst in.maf out.maf\n"
    "where each species.lst line contains two fields\n"
    "   1) species name\n"
    "   2) directory where the .qac and .qdx files are located\n"
    "options:\n"
    "  -divisor=n is value to divide Q value by.  Default is 5.\n"
);
}


static struct slPair *buildSpeciesList(char *speciesFile)
/* read species and directory info from the species.lst file */
{
struct slPair *speciesList = NULL;
struct lineFile *lf = lineFileOpen(speciesFile, TRUE);
char *row[1];

while (lineFileNextRow(lf, row, 2))
    slPairAdd(&speciesList, row[0], (void *) cloneString(row[1]));

lineFileClose(&lf);

return speciesList;
}


static struct hash *loadQacIndex(struct slPair *species)
/* Buid a hash mapping chrom to file offset in a qac file for the */
/* argument species.                                              */
{
char buffer[1024];
struct lineFile *lf;
char *row[1];
int seqCount;
struct hash *qacIndex;
struct hashEl *hel;
struct indexEntry *ie;

safef(buffer, sizeof(buffer), "%s/%s.qdx", (char *) species->val, species->name);
lf = lineFileOpen(buffer, TRUE);

/* get the count of sequences in the qac file */
if (lineFileNextRow(lf, row, 1) == FALSE)
    errAbort("Index file %s is empty.", lf->fileName);
seqCount = lineFileNeedFullNum(lf, row, 0);

/* build and populate a hash mapping chrom to file offset */
qacIndex = newHash(digitsBaseTwo((unsigned long) seqCount));
while (lineFileNextRow(lf, row, 2))
    {
    if ((hel = hashLookup(qacIndex, row[0])) == NULL)
        {
        AllocVar(ie);
        ie->name = cloneString(row[0]);
        errno = 0;
        ie->offset = (off_t) strtol(row[1], NULL, 0);
        if (errno != 0)
            errnoAbort("Unable to convert %s to a long on line %d of %s.",
                row[1], lf->lineIx, lf->fileName);
        hashAdd(qacIndex, ie->name, ie);
        seqCount--;
        }
    else
        {
        errAbort("Duplicate sequence name %s on line %d of %s.",
            row[0], lf->lineIx, lf->fileName);
        }
    }

/* sanity check */
if (seqCount != 0)
    errAbort("%s quality index is inconsistent.", species->name);

lineFileClose(&lf);

return qacIndex;
}


static struct qData *qDataLoad(struct slPair *species)
/* Populate a qData struct for the named species. */
{
char buffer[1024];
struct qData *qd;

AllocVar(qd);
qd->name = cloneString(species->name);

safef(buffer, sizeof(buffer), "%s/%s.qac", (char *) species->val, species->name);
qd->fd = qacOpenVerify(buffer, &(qd->isSwapped));

qd->index = loadQacIndex(species);
qd->qa = NULL;

return qd;
}


static struct hash *buildSpeciesHash(struct slPair *speciesList)
/* Build a hash keyed on species name.  Each species we are adding "q" */
/* lines for will have an entry in this hash.  The hash contains:      */
/*   1) species name [hash key]                                        */
/*   2) file descriptor for qac file (will be opened)                  */
/*   3) flag telling us if the qac file above needs to be swapped      */
/*   4) hash mapping chrom name to an indexEntry                       */
/*   5) a 1-element cache for holding quality data for a chromosome    */
{
struct hash *speciesHash;
struct slPair *pair;
struct hashEl *hel;
struct qData *qd;

speciesHash = newHash(digitsBaseTwo((unsigned long) slCount(speciesList)));

for (pair = speciesList; pair != NULL; pair = pair->next)
    {
    if ((hel = hashLookup(speciesHash, pair->name)) != NULL)
        errAbort("Duplicate species %s in species list file.", pair->name);
    qd = qDataLoad(pair);
    hashAdd(speciesHash, qd->name, qd);
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
        val = (int) floor((double) *q / divisor);
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
				/* check qac, maf for sanity */
				if ((mc->srcSize - mc->start - mc->size < 0)
						|| (mc->srcSize - mc->start > qd->qa->size))
					{
					fprintf(stderr, "Range mismatch: %s qual 0-%d\n",
								mc->src, qd->qa->size);
					mafWrite(stderr, ma);
					exit(EXIT_FAILURE);
					}
                q = qd->qa->qa + qd->qa->size - 1 - mc->start;
                inc = -1;
                }
            else
                {
				/* check qac, maf for sanity */
				if ((mc->start < 0) || (mc->start + mc->size > qd->qa->size))
					{
					fprintf(stderr, "Range mismatch: %s qual 0-%d\n",
								mc->src, qd->qa->size);
					mafWrite(stderr, ma);
					exit(EXIT_FAILURE);
					}
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
struct slPair *speciesList;
struct hash *speciesHash;

optionInit(&argc, argv, options);

if (argc != 4)
    usage();

divisor = optionDouble("divisor", 5.0);

if (divisor <= 0)
    {
    errAbort("Invalid divisor: %f\n", divisor);
    }

/* load metadata about the quality data we're adding */
speciesList = buildSpeciesList(argv[1]);
speciesHash = buildSpeciesHash(speciesList);
slPairFreeValsAndList(&speciesList);

mafAddQRows(argv[2], argv[3], speciesHash);

return EXIT_SUCCESS;
}
