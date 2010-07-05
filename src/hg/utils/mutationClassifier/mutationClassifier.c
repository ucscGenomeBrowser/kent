#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "bed.h"
#include "jksql.h"
#include "options.h"
#include "hdb.h"
#include "genePred.h"
#include "dnautil.h"
#include "assert.h"
#include "hash.h"

static int debug = 0;
static struct hash *geneHash = NULL;

#ifdef TCGA_CODES

#define SPLICE_SITE "Splice_Site_SNP"
#define MISSENSE "Missense_Mutation"
#define READ_THROUGH "Read_Through"
#define NONSENSE "Nonsense_Mutation"
#define NONSENSE_LAST_EXON "Nonsense_Mutation(Last_Exon)"
#define SYNONYMOUS "Silent"
#define IN_FRAME_DEL "In_Frame_Del"
#define IN_FRAME_INS "In_Frame_Ins"
#define FRAME_SHIFT_DEL "Frame_Shift_Del"
#define FRAME_SHIFT_INS "Frame_Shift_Ins"
#define THREE_PRIME_UTR "3'UTR"
#define FIVE_PRIME_UTR "5'UTR"
#define INTRON "Intron"

#else

#define SPLICE_SITE "spliceSite"
#define MISSENSE "missense"
#define READ_THROUGH "readThrough"
#define NONSENSE "nonsense"
#define NONSENSE_LAST_EXON "nonsenseLastExon"
#define SYNONYMOUS "synonymous"
#define IN_FRAME_DEL "inFrameDel"
#define IN_FRAME_INS "inFrameIns"
#define FRAME_SHIFT_DEL "frameShiftDel"
#define FRAME_SHIFT_INS "frameShiftIns"
#define THREE_PRIME_UTR "threePrimeUtr"
#define FIVE_PRIME_UTR "fivePrimeUtr"
#define INTRON "intron"

#endif


static struct optionSpec optionSpecs[] = {
    {"oneBased", OPTION_BOOLEAN},
    {NULL, 0}
};

boolean oneBased = FALSE;

struct bed6
/* A five field bed. */
    {
    struct bed6 *next;
    char *chrom;	/* Allocated in hash. */
    unsigned start;	/* Start (0 based) */
    unsigned end;	/* End (non-inclusive) */
    char *name;	/* Name of item */
    int score; /* Score - 0-1000 */
    char strand;
    };

struct genePredStub
{
    // stub used to allow return multiple references to the same gene prediction in a linked list;
    // this is a hack to save a lot of memory by avoiding making lots of copies of a genePred which only have
    // a different next value.

    struct genePredStub *next;
    struct genePred *genePred;
};


void usage()
/* Explain usage and exit. */
{
errAbort(
  "mutationClassifier - classify mutations \n"
  "usage:\n"
  "   mutationClassifier [options] database snp.bed\n"
  "Classifies SNPs and indels which are in coding regions of UCSC\n"
  "canononical genes as synonymous or non-synonymous.\n"
  "Prints bed4 for identified SNPs; name field contains the codon transformation.\n"
  "Standard single character amino acid codes are used; '*' == stop codon.\n"
  "output is bed4+ with classification code in the name field, and additonal\n"
  "annotations in subsequent fields.\n\n"
  "Mutations are classified with the following codes:\n"
  "%s\n"
  "%s\n"
  "%s\n"
  "%s\n"
  "%s\n"
  "%s\n"
  "%s\n"
  "%s\n"
  "%s\n"
  "%s\n"
  "%s\n"
  "\n"
  "snp.bed should be bed4 (with SNP base in the name field).\n"
  "SNPs are assumed to be on positive strand, unless snp.bed is bed6 with\n"
  "explicit strand field (score field is ignored). SNPs should be\n"
  "zero-length bed entries; all entries are assumed to be indels.\n"
  "\n"
  "options:\n"
  "   -oneBased     = Use one-based coordinates instead of zero-based.\n"
  "example:\n"
  "     mutationClassifier hg19 snp.bed\n",
  SPLICE_SITE, MISSENSE, READ_THROUGH, NONSENSE, NONSENSE_LAST_EXON, SYNONYMOUS, IN_FRAME_DEL, IN_FRAME_INS, FRAME_SHIFT_DEL, FRAME_SHIFT_INS, INTRON
  );
}

static struct bed *shallowBedCopy(struct bed *bed)
/* Make a shallow copy of given bed item (i.e. only replace the next pointer) */
{
struct bed *newBed;
if (bed == NULL)
    return NULL;
AllocVar(newBed);
memcpy(newBed, bed, sizeof(*newBed));
newBed->next = NULL;
return newBed;
}

static struct genePredStub *genePredStubCopy(struct genePred *gp)
// Make a genePredStub of genePred.
{
struct genePredStub *newGenePred;
if (gp == NULL)
    return NULL;
AllocVar(newGenePred);
newGenePred->genePred = gp;
newGenePred->next = NULL;
return newGenePred;
}

static int myBedCmp(const void *va, const void *vb)
/* slSort callback to sort based on chrom,chromStart. */
{
     const struct bed *a = *((struct bed **)va);
     const struct bed *b = *((struct bed **)vb);
     int diff = strcmp(a->chrom, b->chrom);
     if (!diff)
	  diff = a->chromStart - b->chromStart;
     return diff;
}

static int myGenePredCmp(const void *va, const void *vb)
/* slSort callback to sort based on chrom,chromStart. */
{
     const struct genePred *a = *((struct genePred **)va);
     const struct genePred *b = *((struct genePred **)vb);
     int diff = strcmp(a->chrom, b->chrom);
     if (!diff)
	  diff = a->cdsStart - b->cdsStart;
     return diff;
}

static int bedItemsOverlap(struct bed *a, struct genePred *b)
{
     return (!strcmp(a->chrom, b->chrom) &&
	     a->chromEnd > b->txStart &&
	     a->chromStart <= b->txEnd);
}

static int intersectBeds (struct bed *a, struct genePred *b,
                          struct bed **aCommon, struct genePredStub **bCommon)
{
// NOTE that because of the definition of this function, aCommon and bCommon can have 
// duplicate copies from a and b respectively.
//
// if bCommon is NULL we don't return intersected regions in b, AND we only include one copy
// from a (even if it overlaps multiple copies from b).
//
// Running time is O(nlgn) (where n = max(slCount(a), slCount(b)))
// (though b/c of the multiple matching "feature" there's a degenerate case that's O(n^2) 
// if all the items in a overlap all the items in b).

int count = 0;
slSort(&a, myBedCmp);
slSort(&b, myGenePredCmp);
// allocA and allocB point to the last allocated bed struct's
struct bed *curA;
struct bed *lastAddedA = NULL;
struct genePred *curB = b;
struct genePred *savedB = NULL;
struct genePredStub *lastAddedB = NULL;

for (curA = a; curA != NULL && curB != NULL;)
    {
    if (debug)
        {
        fprintf(stderr, "A: %s:%d-%d\n", curA->chrom, curA->chromStart, curA->chromEnd);
        fprintf(stderr, "B: %s:%d-%d\n", curB->chrom, curB->cdsStart, curB->cdsEnd);
        }
    if (bedItemsOverlap(curA, curB))
        {
        if (debug)
            {
            fprintf(stderr, "%s:%d-%d", curA->chrom, curA->chromStart, curA->chromEnd);
            }
        if (aCommon != NULL)
            {
            struct bed *tmpA = shallowBedCopy(curA);
            if (*aCommon == NULL)
                {
                *aCommon = tmpA;
                }
            else
                {
                lastAddedA->next = tmpA;
                }
            // We put newly allocated bed items at the end of the returned list so they match order in original list
            lastAddedA = tmpA;
            }
        if (bCommon != NULL)
            {
            struct genePredStub *tmpB = genePredStubCopy(curB);
            if (*bCommon == NULL)
                {
                *bCommon = tmpB;
                }
            else
                {
                lastAddedB->next = tmpB;
                }
            lastAddedB = tmpB;
            }
        if (bCommon == NULL)
            {
            // see note at beginning of function
            curA = curA->next;
            }
        else
            {
            if (savedB == NULL)
                savedB = curB;
            curB = curB->next;
            }
        count++;
        }
    else
        {
        if (savedB)
            {
            // curA has matched at least one entry in b; now rewind curB to look for potentially multiple matches 
            // within b in the next entry from a (see notes in hw1_analyze.h)
            curA = curA->next;
            curB = savedB;
            savedB = NULL;
            }
        else
            {
            int diff = strcmp(curA->chrom, curB->chrom);
            if (!diff)
                diff = curA->chromStart - curB->cdsStart;
            if (diff < 0)
                {
                curA = curA->next;
                }
            else
                {
                curB = curB->next;
                }
            }
        }
    }
return count;
}

static struct bed6 *readBedFile(char *fileName)
{
// read bed file (we handle bed4 or bed6)
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct bed6 *retVal = NULL;
int wordCount;
char *row[40];
struct bed6 *bed;

while ((wordCount = lineFileChop(lf, row)) != 0)
    {
    char *chrom = row[0];
    int start = lineFileNeedNum(lf, row, 1);
    int end = lineFileNeedNum(lf, row, 2);
    if (start > end)
        errAbort("start after end line %d of %s", lf->lineIx, lf->fileName);
    AllocVar(bed);
    bed->chrom = cloneString(chrom);
    bed->start = start;
    bed->end = end;
    if (oneBased)
	{
	bed->start -= 1;
	bed->end   -= 1;
	}
    bed->name = cloneString(row[3]);
    if (wordCount >= 5)
        bed->score = lineFileNeedNum(lf, row, 4);
    if (wordCount >= 6)
        {
        bed->strand = row[5][0];
        if (bed->strand == '-')
            // we support this so we can process dbSnp data (which has reverse strand SNPs).
            complement(bed->name, strlen(bed->name)); 
        }
    slAddHead(&retVal, bed);
    }
lineFileClose(&lf);
return retVal;
}

static void clipGenPred(char *database, struct genePred *gp)
{
// Clip exonStarts/exonEnds to cdsStart/cdsEnd and then read in the whole DNA for this gene in preparation for a SNP check.
// After this call, exonStarts/exonEnds contain only the exons used for CDS (i.e. some may be removed).
// DNA is put in name2 field; whole dna is read to make it easier to deal with AA's that cross exon junctions.
// Sequence of negative strand genes is reverse-complemented.
    int i;
    unsigned *newStarts = needMem(gp->exonCount * sizeof(unsigned));
    unsigned *newEnds = needMem(gp->exonCount * sizeof(unsigned));
    int newCount = 0;
    gp->name2 = cloneString("");
    for (i=0;i<gp->exonCount;i++)
        {
        if (gp->exonEnds[i] >= gp->cdsStart && gp->exonStarts[i] <= gp->cdsEnd)
            {
            char retNibName[HDB_MAX_PATH_STRING];
            newStarts[newCount] = max(gp->exonStarts[i], gp->cdsStart);
            newEnds[newCount] = min(gp->exonEnds[i], gp->cdsEnd);
            hNibForChrom(database, gp->chrom, retNibName);
            struct dnaSeq *dna = hFetchSeqMixed(retNibName, gp->chrom, newStarts[newCount], newEnds[newCount]);
            char *newName = needMem(strlen(gp->name2) + strlen(dna->dna) + 1);
            sprintf(newName, "%s%s", gp->name2, dna->dna);
            freeMem(gp->name2);
            gp->name2 = newName;
            newCount++;
            }
        }
    gp->exonCount = newCount;
    freeMem(gp->exonStarts);
    freeMem(gp->exonEnds);
    gp->exonStarts = newStarts;
    gp->exonEnds = newEnds;
    if (gp->strand[0] == '-')
        {
        reverseComplement(gp->name2, strlen(gp->name2));
        }
    gp->score = strlen(gp->name2);
    if (debug)
        printf("%s - %d: %s\n", gp->name2, (int) strlen(gp->name2), gp->name2);
}

static int transformPos(char *database, struct genePred *gp, unsigned pos, boolean *lastExon)
{
// transformPos chrom:chromStart coordinates to relative CDS coordinates
// returns -1 if pos is NOT within the CDS

int i, delta = 0;
boolean reverse = gp->strand[0] == '-';

if (gp->name2 == NULL)
    {
    clipGenPred(database, gp);
    }
for (i=0;i<gp->exonCount;i++)
    {
    if (pos < gp->exonStarts[i])
        {
        return -1;
        }
    else if (pos < gp->exonEnds[i])
        {
        pos = delta + pos - gp->exonStarts[i];
        if (gp->strand[0] == '-')
            pos = gp->score - pos - 1;
        // assert(pos >= 0 && pos < strlen(gp->name2));
        *lastExon = reverse ? i == 0 : (i + 1) == gp->exonCount;
        return pos;
        }
    delta += gp->exonEnds[i] - gp->exonStarts[i];
    }
return -1;
}

struct genePred *readGenes(char *database)
{
struct genePred *retVal = NULL;
char query[256];
struct sqlResult *sr;
char **row;
struct sqlConnection *conn = sqlConnect(database);
safef(query, sizeof(query), "select k.* from knownGene k, knownCanonical c where k.cdsStart != k.cdsEnd and k.name = c.transcript");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct genePred *gp = genePredLoad(row);
    gp->name2 = NULL;
    slAddHead(&retVal, gp);
    }
sqlFreeResult(&sr);

// get geneSymbol for output purposes (we use a hash b/c it doesn't fit into struct genePred
geneHash = newHash(16);
safef(query, sizeof(query), "select x.kgID, x.geneSymbol from knownCanonical k, kgXref x where k.transcript = x.kgID");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *geneSymbol = cloneString(row[1]);
    hashAdd(geneHash, row[0], (void *) geneSymbol);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return(retVal);
}

static void mutationClassifier(char *database, char *inputFile)
{
struct bed *overlapA = NULL;
struct genePredStub *overlapB = NULL;
struct bed6 *snps = readBedFile(inputFile);

verbose(2, " read %d mutations\n", slCount(snps));
struct genePred *genes = readGenes(database);
verbose(2, "Hello: %d canonical known genes\n", slCount(genes));
int count = intersectBeds((struct bed *) snps, genes, &overlapA, &overlapB);
verbose(2, "number of intersects: %d\n", count);
// reading unmasked file is much faster - why?
// sprintf(retNibName, "/hive/data/genomes/hg19/hg19.2bit");
for (;overlapA != NULL; overlapA = overlapA->next, overlapB = overlapB->next)
    {
    struct genePred *gp = overlapB->genePred;
    char *code = NULL;
    char additional[256];
    additional[0] = 0;
    // boolean reverse = !strcmp(overlapA->strand, "-");
    boolean lastExon;

    int pos = transformPos(database, gp, overlapA->chromStart, &lastExon);
    if (pos >= 0)
        {
        int len = strlen(overlapA->name);
        if (len > 1 || (overlapA->chromEnd - overlapA->chromStart))
            {
            int delta = len - (overlapA->chromEnd - overlapA->chromStart);
            if (delta % 3)
                code = FRAME_SHIFT_INS;
            else
                code = IN_FRAME_INS;
            }
        else
            {
            unsigned codonStart;
            if ((pos % 3) == 0)
                codonStart = pos;
            else if ((pos % 3) == 1)
                codonStart = pos - 1;
            else
                codonStart = pos - 2;
            char original[4];
            char new[4];
            strncpy(original, gp->name2 + codonStart, 3);
            strncpy(new, gp->name2 + codonStart, 3);
            original[3] = new[3] = 0;
            new[pos % 3] = overlapA->name[0];
            if (gp->strand[0] == '-')
                complement(new + (pos % 3), 1);
            AA originalAA = lookupCodon(original);
            AA newAA = lookupCodon(new);
            if (!originalAA)
                originalAA = '*';
            if (newAA)
                {
                code = originalAA == newAA ? SYNONYMOUS : originalAA == '*' ? READ_THROUGH : MISSENSE;
                }
            else
                {
                newAA = '*';
                code = lastExon ? NONSENSE_LAST_EXON : NONSENSE;
                }
            if (debug)
                fprintf(stderr, "original: %s:%c; new: %s:%c\n", original, originalAA, new, newAA);

#ifdef TCGA_CODES
	    int aaPos = codonStart / 3 + 1;
            safef(additional, sizeof(additional), "g.%s:%d%c>%c\tc.%d%c>%c\tc.(%d-%d)%s>%s\tp.%c%d%c", 
		  gp->chrom + 3, overlapA->chromStart + 1, original[pos % 3], new[pos % 3], 
		  pos + 1, original[pos % 3], new[pos % 3],
		  codonStart + 1, codonStart + 3, original, new,
		  originalAA, aaPos, newAA);
#else
            safef(additional, sizeof(additional), "%c>%c", originalAA, newAA);
#endif

            if (debug)
                fprintf(stderr, "mismatch at %s:%d; %d; %c => %c\n", overlapA->chrom, overlapA->chromStart, pos, originalAA, newAA);
            }
        }
    else
        {
        boolean reverse = gp->strand[0] == '-';
        if (overlapA->chromStart < gp->cdsStart)
            {
            code = reverse ? THREE_PRIME_UTR : FIVE_PRIME_UTR;
            }
        else if (overlapA->chromStart >= gp->cdsEnd)
            {
            code = reverse ? FIVE_PRIME_UTR : THREE_PRIME_UTR;
            }
        else
            {
            // In intro, so check for interuption of splice junction (special case first and last exon).
            int i;
            for (i=0;i<gp->exonCount;i++)
                {
                int start = gp->exonStarts[i] - overlapA->chromStart;
                int end   = overlapA->chromEnd - gp->exonEnds[i];
                // XXXX I still think this isn't quite right (not sure if we s/d use chromEnd or chromStart).
                if (i == 0)
                    {
                    if (end == 1 || end == 2)
                        code = SPLICE_SITE;
                    }
                else if (i == (gp->exonCount - 1))
                    {
                    if (start == 1 || start == 2)
                        code = SPLICE_SITE;
                    }
                else if ((start == 1 || start == 2) || (end == 1 || end == 2))
                    code = SPLICE_SITE;
		else
		    code = INTRON;
                }
            }
        }
    if (code)
        {
        char *geneSymbol = gp->name;
        struct hashEl *el;
        if ((el = hashLookup(geneHash, geneSymbol)))
            {
            geneSymbol = (char *) el->val;
            }
	int start = overlapA->chromStart;
	int end   = overlapA->chromEnd;
	if (oneBased)
	    {
	    start += 1;
	    end   += 1;
	    }
        printf("%s\t%d\t%d\t%s\t%s\t%s\n", 
	       overlapA->chrom, start, end, geneSymbol, code, additional);
        }
    }
}

int main(int argc, char** argv)
{
optionInit(&argc, argv, optionSpecs);
if (argc < 3)
    usage();

if (optionExists("oneBased"))
    oneBased = TRUE;

mutationClassifier(argv[1], argv[2]);
return 0;
}
