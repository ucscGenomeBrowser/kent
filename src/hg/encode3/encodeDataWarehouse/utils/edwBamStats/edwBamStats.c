/* edwBamStats - Collect some info on a BAM file. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* version history: 
 *    1 - initial release 
 *    2 - added sampleBam option
 */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "options.h"
#include "hmmstats.h"
#include "bamFile.h"
#include "md5.h"
#include "obscure.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

int u4mSize = 4000000;	
int sampleBedSize = 250000;
int sampleBamSize = 5000000;
char *sampleBed = NULL;
char *sampleBam = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwBamStats v2 - Collect some info on a BAM file\n"
  "usage:\n"
  "   edwBamStats in.bam out.ra\n"
  "options:\n"
  "   -sampleBed=out.bed\n"
  "   -sampleBam=out.bam\n"
  "   -sampleBamSize=N Max # of items in sampleBam, default %d. Only includes mapped reads.\n"
  "   -sampleBedSize=N Max # of items in sampleBed, default %d.\n"
  "   -u4mSize=N # of uniquely mapped items used in library complexity calculation, default %d\n"
  , sampleBamSize, sampleBedSize, u4mSize
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"sampleBed", OPTION_STRING},
   {"sampleBam", OPTION_STRING},
   {"sampleBedSize", OPTION_INT},
   {"sampleBamSize", OPTION_INT},
   {"u4mSize", OPTION_INT},
   {NULL, 0},
};

struct targetPos
/* A position in a target sequence */
    {
    struct targetPos *next;
    int targetId;   /* Bam tid */
    unsigned pos;   /* Position. */
    bits16 size;    /* Read size. */
    char strand;    /* + or - */
    };

int targetPosCmp(const void *va, const void *vb)
/* Compare to sort based on query start. */
{
const struct targetPos *a = *((struct targetPos **)va);
const struct targetPos *b = *((struct targetPos **)vb);
int dif;
dif = a->targetId - b->targetId;
if (dif == 0)
    dif = a->pos - b->pos;
if (dif == 0)
    dif = a->strand - b->strand;
return dif;
}

int targetPosCmpNoStrand(const void *va, const void *vb)
/* Compare to sort based on query start ignoring strand. */
{
const struct targetPos *a = *((struct targetPos **)va);
const struct targetPos *b = *((struct targetPos **)vb);
int dif;
dif = a->targetId - b->targetId;
if (dif == 0)
    dif = a->pos - b->pos;
return dif;
}

int countUniqueFromSorted(struct targetPos *tpList)
/* Count the unique number of positions in a sorted list. */
{
int count = 1;
struct targetPos *tp = tpList, *next;
for (;;)
    {
    next = tp->next;
    if (next == NULL)
        break;
    if (next->pos != tp->pos || next->targetId != tp->targetId || next->strand != tp->strand)
        ++count;
    tp = next;
    }
return count;
}

int uint32_tCmp(const void *va, const void *vb)
/* Compare function to sort array of ints. */
{
const uint32_t *pa = va;
const uint32_t *pb = vb;
uint32_t a = *pa, b = *pb;
if (a < b)
    return -1;
else if (a > b)
    return 1;
else
    return 0;
}

typedef char *CharPt;

int charPtCmp(const void *va, const void *vb)
/* Compare function to sort array of ints. */
{
const CharPt *pa = va;
const CharPt *pb = vb;
return strcmp(*pa, *pb);
}

void splitList(struct targetPos *oldList, struct targetPos **pA, struct targetPos **pB)
/* Split old list evenly between a and b. */
{
struct targetPos *aList = NULL, *bList = NULL, *next, *tp;
boolean pingPong = FALSE;
for (tp = oldList; tp != NULL; tp = next)
    {
    next = tp->next;
    if (pingPong)
        {
	slAddHead(&aList, tp);
	pingPong = FALSE;
	}
    else
        {
	slAddHead(&bList, tp);
	pingPong = TRUE;
	}
    }
*pA = aList;
*pB = bList;
}

void subsampleMappedFromBam(char *inBam, long long inMappedCount, 
    char *sampleBam, long long outMappedCount)
/* Create a sam file that is a robustly sampled subset of the mapping portion
 * of the inBam file.  The sam can be converted to bam later.  This routine
 * already has enough to do! */
{
/* Create a byte-map that is inMappedCount long and has outMappedCount 1's randomly
 * dispersed through it. */
char *map = needLargeMem(inMappedCount);
int outSize = min(outMappedCount, inMappedCount);
uglyf("inMappedCount=%lld ouMappedCount=%lld outSize = %d\n", inMappedCount, outMappedCount, outSize);
memset(map, 1, outSize);
memset(map+outSize, 0, inMappedCount - outSize);
shuffleArrayOfChars(map, inMappedCount);

/* Open up bam file */
samfile_t *in = bamMustOpenLocal(inBam, "rb", NULL);

/* Open up sam output and write header */
bam_hdr_t *header = sam_hdr_read(in);
samfile_t *out = bamMustOpenLocal(sampleBam, "wb", header);


/* Loop through bam items, writing them to sam or not according to map. */
int mapIx = 0;
bam1_t one;
ZeroVar(&one);	// This seems to be necessary!
for (;;)
    {
    /* Read next record. */
    if (sam_read1(in, header, &one) < 0)
	break;

    /* Just consider mapped reads. */
    if ((one.core.flag & BAM_FUNMAP) == 0 && one.core.tid >= 0)
        {
	if (map[mapIx])
	    sam_write1(out, header, &one);
	++mapIx;
	}
    }

/* Clean up and go home. */
freeMem(map);
samclose(in);
samclose(out);
}


void openSamReadHeader(char *fileName, samfile_t **retSf, bam_header_t **retHead)
/* Open file and check header.  Abort with error message if a problem. */
{
samfile_t *sf = bamMustOpenLocal(fileName, "rb", NULL);
bam_hdr_t *head = sam_hdr_read(sf);
if (head == NULL)
    errAbort("Aborting ... Bad BAM header in file: %s", fileName);
*retSf = sf;
*retHead = head;
}


void statsWriteHeaderInfo(FILE *f, bam_header_t *head)
// Write to stats file, useful information discovered in bam header.
{
// Try to determine the aligner.  It would be good to not hard-code expected aligners,
// But with 0 or more @PG lines in a header, it might be hard to discover a simple answer.
if (strcasestr(head->text,"\n@PG\tID:bwa") != NULL)
    fprintf(f, "alignedBy bwa\n");
else if (strcasestr(head->text,"\n@PG\tID:TopHat") != NULL)
    fprintf(f, "alignedBy TopHat\n");
else if (strcasestr(head->text,"\n@PG\tID:STAR") != NULL)
    fprintf(f, "alignedBy STAR\n");
else if (strcasestr(head->text,"\n@PG\tID:RSEM") != NULL)
    fprintf(f, "alignedBy RSEM\n");
else
    {
    //fprintf(f, "alignedBy unknown\n");
    if (strcasestr(head->text,"\n@PG\tID:BEDTools_bedToBam") != NULL)
        fprintf(f, "createdBy BEDTools_bedToBam\n");
    }

// See if any CO lines can be discovered (e.g. "@CO     REFID:ENCFF001RGS")
#define CO_PREFIX "\n@CO\t"
char *p = head->text;
for (;;)
    {
    // Find "CO" at beginning of line
    if ((p = stringIn(CO_PREFIX,p)) == NULL)
        break;
    p += strlen(CO_PREFIX);
    char *coPair = cloneNextWordByDelimiter(&p,' ');
    p -= 1; // cloneNextWord skips past delimiter, which we still need
    if (coPair == NULL || *coPair == '\0')
        continue;

    // Only support expected CO lines at this time
    char *words[2];
    if (chopString(coPair, ":", words, ArraySize(words)) != 2)
        continue;  // mangled line so skip it
    if (sameString(words[0], "REFID") || sameString(words[0], "ANNID")
    ||  sameString(words[0], "LIBID") || sameString(words[0], "SPIKEID"))
        {
        // Overkill: camelCase theId
        strLower(words[0]);
        words[0][strlen(words[0])-2] = 'I';
        }
    else
        continue;

    // print RA style "var val ..." pair
    fprintf(f, "%s %s",words[0],words[1]);
    if (*p != '\n')  // Perhaps there is more to the line?
        {
        char *restOf = cloneNextWordByDelimiter(&p,'\n');
        p -= 1; // cloneNextWord skips past delimiter, which we still need
        fprintf(f, " %s",restOf);
        //freeMem(restOf); // spill
        }
    //freeMem(coPair); // spill
    fputc('\n', f);
    }
}


void edwBamStats(char *inBam, char *outRa)
/* edwBamStats - Collect some info on a BAM file. */
{
/* Statistics we'll gather. */
long long mappedCount = 0, uniqueMappedCount = 0;
long long maxReadBases=0, minReadBases=0, readCount=0, sumReadBases=0;
double sumSquaredReadBases = 0.0;
boolean sortedByChrom = TRUE, isPaired = FALSE;


/* List of positions. */
struct lm *lm = lmInit(0);
struct targetPos *tp, *tpList = NULL;

/* Some stuff to keep memory use down for very big BAM files,  lets us downsample. */
int skipRatio = 1;
struct targetPos *freeList = NULL;
int skipPos = 1;
int doubleSize = 2*u4mSize;
int listSize = 0;

/* Open file and get header for it. */
samfile_t *sf = NULL;
bam_header_t *head = NULL;
openSamReadHeader(inBam, &sf, &head);

/* Start with some processing on the headers.  Get sorted versions of them. */
uint32_t *sortedSizes = CloneArray(head->target_len, head->n_targets);
qsort(sortedSizes, head->n_targets, sizeof(sortedSizes[0]), uint32_tCmp);
char **sortedNames = CloneArray(head->target_name, head->n_targets);
qsort(sortedNames, head->n_targets, sizeof(sortedNames[0]), charPtCmp);

/* Sum up some target sizes. */
long long targetBaseCount = 0;   /* Total size of all bases in target seq */
int i;
for (i=0; i<head->n_targets; ++i)
    targetBaseCount  += head->target_len[i];

bam1_t one;
ZeroVar(&one);	// This seems to be necessary!
for (;;)
    {
    /* Read next record. */
    if (sam_read1(sf, head, &one) < 0)
	break;

    /* Gather read count and length statistics. */
    long long seqSize = one.core.l_qseq;
    if (readCount == 0)
        {
	maxReadBases = minReadBases = seqSize;
	}
    else
	{
	if (maxReadBases < seqSize)
	    maxReadBases = seqSize;
	if (minReadBases > seqSize)
	    minReadBases = seqSize;
	}
    sumReadBases += seqSize;
    sumSquaredReadBases += (seqSize * seqSize);
    ++readCount;

    /* Gather position for uniquely mapped reads. */
    if ((one.core.flag & BAM_FUNMAP) == 0 && one.core.tid >= 0)
        {
	++mappedCount;
	if (one.core.qual > edwMinMapQual)
	    {
	    ++uniqueMappedCount;
	    if (--skipPos == 0)
		{
		skipPos = skipRatio;
		if (freeList != NULL)
		    tp = slPopHead(&freeList);
		else
		    lmAllocVar(lm, tp);
		tp->targetId = one.core.tid;
		tp->pos = one.core.pos;
		tp->size = one.core.l_qseq;
		tp->strand = ((one.core.flag & BAM_FREVERSE) ? '-' : '+');
		if (tpList != NULL && targetPosCmpNoStrand(&tpList, &tp) > 0)
		    sortedByChrom = FALSE; 
		slAddHead(&tpList, tp);
		++listSize;
		if (listSize >= doubleSize)
		    {
		    splitList(tpList, &tpList, &freeList);
		    listSize /= 2;
		    skipRatio *= 2;
		    verbose(2, "tpList %d, freeList %d, listSize %d, skipRatio %d, readCount %lld\n", 
			slCount(tpList), slCount(freeList), listSize, skipRatio, readCount);
		    }
		}
	    }
	}
    if (one.core.flag & BAM_FPAIRED)
        isPaired = TRUE;
    }
verbose(1, "Scanned %lld reads in %s\n", readCount, inBam);


FILE *f = mustOpen(outRa, "w");
statsWriteHeaderInfo(f,head);
fprintf(f, "isPaired %d\n", isPaired);
fprintf(f, "isSortedByTarget %d\n", sortedByChrom);
fprintf(f, "readCount %lld\n", readCount);
fprintf(f, "readBaseCount %lld\n", sumReadBases);
fprintf(f, "mappedCount %lld\n", mappedCount);
fprintf(f, "uniqueMappedCount %lld\n", uniqueMappedCount);
fprintf(f, "readSizeMean %g\n", (double)sumReadBases/readCount);
if (minReadBases != maxReadBases)
    fprintf(f, "readSizeStd %g\n", calcStdFromSums(sumReadBases, sumSquaredReadBases, readCount));
else
    fprintf(f, "readSizeStd 0\n");
fprintf(f, "readSizeMin %lld\n", minReadBases);
fprintf(f, "readSizeMax %lld\n", maxReadBases);
tpList = slListRandomSample(tpList, u4mSize);
slSort(&tpList, targetPosCmp);
int m4ReadCount = slCount(tpList);
fprintf(f, "u4mReadCount %d\n", m4ReadCount);
int m4UniquePos = countUniqueFromSorted(tpList);
fprintf(f, "u4mUniquePos %d\n", m4UniquePos);
double m4UniqueRatio = (double)m4UniquePos/m4ReadCount;
fprintf(f, "u4mUniqueRatio %g\n", m4UniqueRatio);
verbose(1, "u4mUniqueRatio %g\n", m4UniqueRatio);

fprintf(f, "targetSeqCount %d\n", (int) head->n_targets);
fprintf(f, "targetBaseCount %lld\n", targetBaseCount);

/* Deal with bed output if any */
if (sampleBed != NULL)
    {
    tpList = slListRandomSample(tpList, sampleBedSize);
    slSort(&tpList, targetPosCmp);
    FILE *bf = mustOpen(sampleBed, "w");
    for (tp = tpList; tp != NULL; tp = tp->next)
        {
        char *chrom = head->target_name[tp->targetId];
	fprintf(bf, "%s\t%u\t%u\t.\t0\t%c\n", 
	    chrom, tp->pos, tp->pos + tp->size, tp->strand);
	}
    carefulClose(&bf);
    }

/* Deal with sam output if any */
if (sampleBam != NULL)
    {
    subsampleMappedFromBam(inBam, mappedCount, sampleBam, sampleBamSize);
    }

/* Clean up */
samclose(sf);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
sampleBed = optionVal("sampleBed", sampleBed);
sampleBam = optionVal("sampleBam", sampleBam);
sampleBedSize = optionInt("sampleBedSize", sampleBedSize);
sampleBamSize = optionInt("sampleBamSize", sampleBamSize);
u4mSize = optionInt("u4mSize", u4mSize);
edwBamStats(argv[1], argv[2]);
return 0;
}
