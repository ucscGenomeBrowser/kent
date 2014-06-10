/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "dnaseq.h"
#include "nib.h"
#include "linefile.h"
#include "dnautil.h"
#include "fa.h"
#include "cheapcgi.h"


struct genomePos 
{
    struct genomePos *next;     /* Next in list. */
    short chrom;                /* Number for chromosome. */
    unsigned int position;      /* Position in chrom. Always a 25mer so don't need end. */
};

struct chromInfo
{
    struct chromInfo *next;    /* Next in list. */
    char *chrom;               /* Chrom. */
    char *fileName;            /* Name of file. */
};

struct chromInfo *ciList = NULL;  /* List of information about the chromsomes, should be length 1 now. */
struct hash *bigHash = NULL;      /* Hash of all the nmers. */
double totalCount = 0;            /* Total count of all unique nmers. */
double allCount = 0;              /* Count of all nmers examined. */
struct dnaSeq *genomeSeq = NULL;  /* Genomic sequence, usually a chromsome subset. */
int genomeOffset = 0;             /* Offset in chromosome of gneomSeq. */
FILE *commonFile = NULL;          /* File to print common oligos to. */
int nmerSize = 0;                 /* Size of nmers to align. */

void usage() 
{
errAbort("Program to find perfect alignments for nmerSize mers in a portion of the genome.\n"
	 "usage:\n\t"
	 "nmerAlign chromStart chromEnd chromFile.nib query.fa outFile.tab");
}

void initChromList(int numChrom, char *chrom[]) 
/* Load all of the chromosome names into a list so that they can be
   named by the index in the list and thus require only 1 byte each. */
{
int i;
struct chromInfo *ci = NULL;
char *tmpMark = NULL;
for(i=0; i<numChrom; i++) 
    {
    AllocVar(ci);
    ci->chrom = cloneString(chrom[i]);
    chopSuffix(ci->chrom);
    tmpMark = strrchr(ci->chrom, '/'); /* look for the end of the path. */
    if(tmpMark != NULL)
	ci->chrom = tmpMark+1;
    ci->fileName = cloneString(chrom[i]);
    slAddHead(&ciList, ci);
    }
slReverse(&ciList);
}

char *chromFromIndex(int i) 
/* Translate the index of a chromosome into a name .*/
{
struct chromInfo *ci = slElementFromIx(ciList, i);
if(ci == NULL)
    errAbort("count25Mers::chromFromIndex() - Couldn't find index %d in a list that is only %d long.\n", i, slCount(ciList));
return ci->chrom;
}

int indexFromChrom(char *chrom) 
/* Get the index for a given chromosome by name.  */
{
struct chromInfo *ci = NULL;
int count =0;
for(ci = ciList; ci != NULL; ci = ci->next) 
    {
    if(sameString(ci->chrom, chrom))
	return count;
    count++;
    }
errAbort("count25Mers::indexFromChrom() - Couldn't find chrom: %s in list.\n", chrom);
}

boolean masked(struct dnaSeq *seq, unsigned int start, unsigned int end)
/* Return true if there are lower case bases or N's in sequence. */
{
unsigned int i = 0;
for(i=start; i<end; i++) 
    {
    if(seq->dna[i] == 'N' || seq->dna[i] == 'n' || islower(seq->dna[i]))
	return TRUE;
    }
return FALSE;
}

void addToHash(struct dnaSeq *seq, unsigned int start, unsigned int end, double *chromCount, int chromOffset)
/* Check to see if oligo from start to end is in hash, if so add a position
   to list, otherwise add a new hash element. */
{
char *buff = NULL;
struct genomePos *gp = NULL; 
struct genomePos *gpNew = NULL;
buff = needMem((end-start+1)*sizeof(char));

//snprintf(buff, (end-start+1), "%s", seq->dna+start);
strncpy(buff, seq->dna+start, end-start);
gp = hashFindVal(bigHash, buff);

AllocVar(gpNew);
gpNew->chrom = indexFromChrom(seq->name);
gpNew->position = start + chromOffset;
allCount++;
if(gp == NULL) 
    {
    hashAddUnique(bigHash, buff, gpNew);
    (*chromCount)++;
    totalCount++;
    }
else 
    {
    slAddTail(&gp, gpNew);
    }
freez(&buff);
}

void countChromNMers(struct dnaSeq *seq, double *chromCount, int chromOffset)
/* Count how many unique in genome so far unmasked Nmers occur on this chromsome. */
{
unsigned int i = 0;
unsigned limit = seq->size - nmerSize;
int size = 1000000;
warn("Looking for %d dots.", limit/size);
for(i=0; i < limit; i++) 
    {
    if(i%size == 0)
	{
	fprintf(stderr, ".");
	fflush(stderr);
	}
    if(!masked(seq, i, i+nmerSize))
	{
	addToHash(seq, i, i+nmerSize, chromCount, chromOffset);
	}
    }
fprintf(stderr, "\n");
fflush(stderr);
}

void changeChromName(struct dnaSeq *chrom)
/* Convert something like /here/there/chr22.nib:1-1000 to chr22 */
{
char *tmp = NULL;
tmp = strrchr(chrom->name, '.');
if(tmp != NULL)
    *tmp = '\0';
tmp = strrchr(chrom->name, '/');
if(tmp != NULL)
    {
    tmp = tmp+1;
    chrom->name = tmp;
    }
}

void reportCommonTiles(void *val)
/* If there are more than 50 places where a tile occurs call it common and report it. */
{
struct genomePos *gp = val;
int size = slCount(gp);
if(size > 20)
    {
    char *buff = needMem(26*sizeof(char));
    strncpy(buff, genomeSeq->dna + (gp->position - genomeOffset), nmerSize);
    fprintf(commonFile, "%s\t%d\n", buff, size);
    freez(&buff);
    }
}

void buildBigHash(char *chromName, int chromStart, int chromEnd, char *outFile)
{
struct dnaSeq *chrom = NULL;
double chromCount = 0;
int seqSize = 0;
FILE *f;
char buff[1024];
bigHash = newHash(20);
snprintf(buff, sizeof(buff), "%s.common", outFile);
commonFile = mustOpen(buff, "w");
warn("Reading chromosome %s", chromFromIndex(0));
nibOpenVerify(chromName, &f, &seqSize);
chrom = nibLdPartMasked(NIB_MASK_MIXED, chromName, f, seqSize, chromStart, (chromEnd-chromStart));
fclose(f);
warn("Counting");
changeChromName(chrom);
countChromNMers(chrom, &chromCount, chromStart);
warn("%f %dmers in chrom %s", chromCount, nmerSize, chromFromIndex(0));

/* Do some reporting about common tiles. */
genomeOffset = chromStart;
genomeSeq = chrom;
hashTraverseVals(bigHash, reportCommonTiles);
genomeOffset = 0;
genomeSeq = NULL;

/* Cleanup. */
dnaSeqFree(&chrom);
warn("%f unique %dmers in %f examined.", totalCount, nmerSize, allCount);
carefulClose(&commonFile);
}

void writeAlign(char *name, DNA *seq,  struct genomePos *gpList, FILE *out, char *strand)
{
struct genomePos *gp = NULL;
for(gp = gpList; gp != NULL; gp = gp->next)
    {
    fprintf(out, "%s\t%d\t%d\t%s\t%s\t%s\n", chromFromIndex(gp->chrom), gp->position, gp->position+strlen(seq), name, strand, seq);
    }
}

void alignNMers(char *fastaFile, char *outFile)
{
struct lineFile *fa = lineFileOpen(fastaFile, TRUE);
FILE *out = mustOpen(outFile, "w");
DNA *dna = NULL;
int seqSize = 0;
int aligned = 0;
int numSeq = 0;
char *seqName = NULL;
dnaUtilOpen();
while(faSpeedReadNext(fa, &dna, &seqSize, &seqName))
    {
    boolean found = FALSE;
    struct genomePos *gpList = NULL, *gp = NULL;
    numSeq++;
    toUpperN(dna, strlen(dna));
    reverseBytes(dna, seqSize);
    gpList = hashFindVal(bigHash, dna);
    if(gpList != NULL)
	{
	aligned++;
	found = TRUE;
	}
    writeAlign(seqName, dna, gpList, out, "+");
    reverseComplement(dna, seqSize);

    gpList = hashFindVal(bigHash, dna);
    if(gpList != NULL && !found) 
	aligned++;
    writeAlign(seqName, dna, gpList, out, "-");
    }
warn("%d of %d sequences aligned.", aligned, numSeq);
lineFileClose(&fa);
carefulClose(&out);
}

int main(int argc, char *argv[]) 
{
unsigned int chromStart = 0, chromEnd = 0;
cgiSpoof(&argc, argv);
nmerSize = cgiOptionalInt("nmerSize", 25);
if(argc != 6) 
    usage();
initChromList(1, argv+3); 
chromStart = atoi(argv[1]);
chromEnd = atoi(argv[2]);
buildBigHash(argv[3], chromStart, chromEnd, argv[5]);
alignNMers(argv[4], argv[5]);
return 0;
}
