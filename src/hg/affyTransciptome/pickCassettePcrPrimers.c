/* pickCassettePcrPrimers - Takes a bedFile with three exons and for each bed calls primer3 to pick primers that will detect the inclusion or exclusion of the exon.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "hdb.h"
#include "dnautil.h"
#include "bed.h"


struct cassetteSeq 
/* Info necessary to make pcr primers for a cassette exon. */
{
    struct cassetteSeq *next;   /* Next in list. */
    struct bed *bed;            /* Bed that this sequence represents. */
    struct dnaSeq *seq;         /* Genomic sequence of exon concatenated together. 5'-> 3' coding*/
    int targetStart;            /* Start of cassette exon in sequence. */
    int targetEnd;              /* End of cassette exon in sequence. */
    char strand[3];             /* Strand associated with cassette exon. */
    char *name;                 /* Name of cassetteSeq. */
    char *rightPrimer;          /* Sequence of right hand primer. */
    char *leftPrimer;           /* Sequence of left hand primer. */
    int rpStart;                /* Start of right hand primer in sequence. */
    int lpStart;                /* Start of left hand primer in sequence. */
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pickCassettePcrPrimers - Takes a bedFile with three exons and for each\n"
  "bed calls primer3 to pick primers that will detect the inclusion or\n"
  "exclusion of the exon.\n"
  "usage:\n"
  "   pickCassettePcrPrimers <genomeDb: hg12,hg13,etc> <bedFile> <primerOutput> <primerTrack>\n"
  "options:\n"
  "   -primer3=/path/to/primer3/primer3executable -target=targetExonNum\n"
  );
}


int countBedSize(struct bed *bed)
/* Count the number of bases in a bed. */
{
int i=0,count=0;
for(i=0;i<bed->blockCount; i++)
    count+=bed->blockSizes[i];
return count;
}

struct cassetteSeq *cassetteSeqFromBed(struct bed *bed, int targetExon)
/* Consruct a cassetteSeq from a bed using the targetExon. */
{
struct cassetteSeq *cseq = NULL;
int i=0;
char buff[1024];
int bedSize=0;
int seqSize=0;
int targetStart=0;
/* Make sure the target exon is valid. */
if(targetExon >= bed->blockCount) 
    errAbort("pickCassettePcrPrimers::cassetteSeqFromBed() - Got request"
	     "for %d targetExon, but only %d exons present in %s\n", 
	     targetExon, bed->blockCount, bed->name);

AllocVar(cseq);
AllocVar(cseq->seq);
snprintf(buff, sizeof(buff), "%s:%d-%d_%s", bed->chrom, bed->chromStart, bed->chromEnd, bed->name);
cseq->name = cloneString(buff);
bedSize = countBedSize(bed) + 1;
cseq->seq->dna = needMem(sizeof(char)*bedSize);
cseq->bed = cloneBed(bed);
for(i=0; i<bed->blockCount; i++)
    {
    struct dnaSeq *seq = NULL;
    int chromStart =  bed->chromStarts[i] + bed->chromStart;
    int chromEnd = bed->blockSizes[i] + chromStart;
    seq = hChromSeq(bed->chrom, chromStart, chromEnd);
    sprintf(cseq->seq->dna+seqSize, "%s", seq->dna);
    if(targetExon == i)
	targetStart = seqSize;
    seqSize += bed->blockSizes[i];
    dnaSeqFree(&seq);
    }
cseq->seq->size = seqSize;
if(sameString(bed->strand, "-")) 
    {
    reverseComplement(cseq->seq->dna, cseq->seq->size);
    targetStart = cseq->seq->size - targetStart - bed->blockSizes[targetExon];
    }
cseq->targetStart = targetStart;
cseq->targetEnd = targetStart + bed->blockSizes[targetExon];
snprintf(cseq->strand, sizeof(cseq->strand), "%s", bed->strand);
return cseq;
}    


struct bed *bedLoadAll(char *fileName)
/* Load all bed's in file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct bed *bedList = NULL, *bed;
char *row[12];
while(lineFileRow(lf,row))
    {
    bed = bedLoad12(row);
    slAddHead(&bedList, bed);
    }
slReverse(&bedList);
lineFileClose(&lf);
return bedList;
}

void writeBoulderIoFormat(FILE *out, struct cassetteSeq *cseq)
/* Write out a boulderIO format record for pcr primer picking. */
{
int targetSize = cseq->targetEnd-cseq->targetStart;
fprintf(out, "PRIMER_SEQUENCE_ID=%s\n",cseq->name);
fprintf(out, "SEQUENCE=%s\n", cseq->seq->dna);
fprintf(out, "TARGET=%d,%d\n", cseq->targetStart, targetSize);
fprintf(out, "PRIMER_PRODUCT_SIZE_RANGE=%d-%d\n", targetSize + 75, targetSize+250);
fprintf(out, "=");
}

void fillInBioHash(char *fileName, struct hash *bioHash)
/* Fill in the bioHash with key/value pairs from file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line = NULL;
int regSize = 0;
while(lineFileNextReal(lf, &line)) 
    {
    char *key = NULL;
    char *val = NULL;
    char *mark = NULL;
    mark = strchr(line, '=');
    if(mark == NULL) // Error: not in boulder IO format.
	errAbort("pickCassettePcrPrimers::fillInBioHash() - ",
		 "Couldn't find '=' in line %s. File %s doesn't appear to be in boulderIO format.",
		 line, fileName);
    if(mark == line) // First character is '=' means end of record.
	break;
    key = line;
    val = mark+1;
    *mark = '\0';
    hashAddUnique(bioHash, key, cloneString(val));
    }
lineFileClose(&lf);
}
    
		 
void parseBoulderIo(char *fileName, struct cassetteSeq *cseq) 
/* Open fileName and parse the information to fill in cseq */
{
struct hash *bioHash = newHash(10);
char *tmp = NULL;
fillInBioHash(fileName, bioHash);
cseq->leftPrimer = cloneString(hashFindVal(bioHash, "PRIMER_LEFT_SEQUENCE"));
cseq->rightPrimer = cloneString(hashFindVal(bioHash, "PRIMER_RIGHT_SEQUENCE"));
tmp = hashFindVal(bioHash, "PRIMER_LEFT");
if(tmp != NULL)
    cseq->lpStart = atoi(tmp);
tmp = hashFindVal(bioHash, "PRIMER_RIGHT");
if(tmp != NULL)
    cseq->rpStart = atoi(tmp);
freeHashAndVals(&bioHash);
}

void outputFasta(struct cassetteSeq *cseq, FILE *primerFa)
/* Output a fasta record for each primer. */
{
if(cseq->leftPrimer != NULL && cseq->rightPrimer != NULL)
    {
    fprintf(primerFa, ">%s_leftPrimer\n%s\n", cseq->name, cseq->leftPrimer);
    fprintf(primerFa, ">%s_rightPrimer\n%s\n", cseq->name, cseq->rightPrimer);
    }
}


int calcGenomePos( struct bed *bed, char *sequence, struct dnaSeq *seq)
/* Figure out where a probe is in genomic space, given its position in
   transcript. */
{
char *mark = NULL;
int transPos = 0;   // Transcript location.
int genomePos= 0;   // Genome location
int i=0;
mark = strstr(seq->dna, sequence);
if(mark == NULL) 
    {
    reverseComplement(sequence, strlen(sequence));
    mark = strstr(seq->dna, sequence);
    if(mark == NULL) 
	errAbort("pickCassettePcrPrimers:;calcGenomePos() - ",
		 "Can't find %s in %s for bed %s\n", sequence, seq->dna, bed->name);
    reverseComplement(sequence, strlen(sequence));
    }
transPos = mark - seq->dna;

/* Check for reverse strand. */
if(sameString(bed->strand, "-"))
    transPos = seq->size - transPos - strlen(sequence);

/* This bit gets a little clever. GenomePos is used as a
   temporary variable to keep track of how much of the transcript
   sequence we've seen. If we find the right exon that the sequence is
   in then we use that to calculate the genome position from the chromStarts
   for that exon. Otherwise we just add the length of that exon to the
   current transcript length stored in genomePos. */
for(i=0; i<bed->blockCount; i++)
    {
    if(genomePos + bed->blockSizes[i] > transPos)
	{
	genomePos = bed->chromStarts[i] + (transPos - genomePos);
	break;
	}
    else
	genomePos += bed->blockSizes[i];
    }
genomePos += bed->chromStart;
return genomePos;
}

void checkBedMatchesSeqs(struct cassetteSeq *cseq, struct bed *bed)
/* Pull the bed sequences out of the database and make sure that they
   match the primer sequences in cseq. Second sequence in bed is always
   reverse complemented, matching sequences flip depeneding on which
   strand we're on:

   If gene is on '+' strand
   ++++++++++++++++++++++++++++++++++++++++++++->
   <--------------------------------------------
   
   llllllllll->                    <-rrrrrrrrrr (rev-comp)
   
   If gene is on '-' strand
   ++++++++++++++++++++++++++++++++++++++++++++->
   <--------------------------------------------
   
   rrrrrrrrrr->                    <-llllllllll (rev-comp)
*/
{
struct dnaSeq *firstSeq = NULL;
struct dnaSeq *secSeq = NULL;
boolean goodFlag = TRUE;
firstSeq = hChromSeq(bed->chrom, bed->chromStart, bed->chromStart+bed->blockSizes[0]);
secSeq = hChromSeq(bed->chrom, bed->chromStart+bed->chromStarts[1], bed->chromStart + bed->chromStarts[1] + bed->blockSizes[1]);
if(sameString(bed->strand,"+")) 
    {
    reverseComplement(secSeq->dna, secSeq->size);
    if(differentString(firstSeq->dna, cseq->leftPrimer))
	goodFlag = FALSE;
    if(differentString(secSeq->dna, cseq->rightPrimer))
	goodFlag = FALSE;
    reverseComplement(secSeq->dna, secSeq->size);
    }
else
    {
    reverseComplement(secSeq->dna, secSeq->size);
    if(differentString(firstSeq->dna, cseq->rightPrimer))
	goodFlag = FALSE;
    if(differentString(secSeq->dna, cseq->leftPrimer))
	goodFlag = FALSE;
    reverseComplement(secSeq->dna, secSeq->size);
    }
if(goodFlag == FALSE) 
    {
    char *rpRev = cloneString(cseq->rightPrimer);
    char *lpRev = cloneString(cseq->leftPrimer);
    char *firstBlock = cloneString(firstSeq->dna);
    char *secBlock = cloneString(secSeq->dna);
    reverseComplement(rpRev, strlen(rpRev));
    reverseComplement(lpRev, strlen(lpRev));
    reverseComplement(firstBlock, strlen(firstBlock));
    reverseComplement(secBlock, strlen(secBlock));
    warn("Problem for bed; %s on strand %s", bed->name, bed->strand);
    warn("leftPrimer:\t%s\t%s", cseq->leftPrimer, lpRev);
    warn("rightPrimer:\t%s\t%s", cseq->rightPrimer, rpRev);
    warn("firstBlock:\t%s\t%s", firstSeq->dna, firstBlock);
    warn("secBlock:\t%s\t%s", secSeq->dna, secBlock);
    freez(&rpRev);
    freez(&lpRev);
    freez(&firstBlock);
    freez(&secBlock);
    }
dnaSeqFree(&firstSeq);
dnaSeqFree(&secSeq);
}

void outputBed(struct cassetteSeq *cseq, FILE *primerBed) 
/* Output a bed linked features track to see where primers are. */
{
struct bed *bed = NULL;
struct bed *cbed = cseq->bed;
int leftStart=0, rightStart =0;
bed = cloneBed(cbed);

if(cseq->leftPrimer == NULL || cseq->rightPrimer == NULL)
    return;
leftStart = calcGenomePos(cbed, cseq->leftPrimer, cseq->seq);
rightStart = calcGenomePos(cbed, cseq->rightPrimer, cseq->seq);
if(sameString(bed->strand, "+")) 
    {
    bed->chromStart = bed->thickStart = leftStart;
    bed->chromStarts[0] = 0;
    bed->blockSizes[0] = strlen(cseq->leftPrimer);
    bed->chromStarts[1] = rightStart - leftStart;
    bed->blockSizes[1] = strlen(cseq->leftPrimer);
    bed->chromEnd = bed->thickEnd = bed->chromStarts[1] + bed->chromStart + bed->blockSizes[1];
    }
else
    {
    bed->chromStart = bed->thickStart = rightStart;
    bed->chromStarts[0] = 0;
    bed->blockSizes[0] = strlen(cseq->rightPrimer);
    bed->chromStarts[1] = leftStart - rightStart;
    bed->blockSizes[1] = strlen(cseq->rightPrimer);
    bed->chromEnd = bed->thickEnd = bed->chromStarts[1] + bed->chromStart + bed->blockSizes[1];
    }
bed->blockCount = 2;
checkBedMatchesSeqs(cseq, bed);
bedTabOutN(bed, 12, primerBed);
bedFree(&bed);
}

void callPrimer3(struct cassetteSeq *cseq, FILE *primerFa, FILE *primerBed)
/* Write out a boulder IO format file and call primer3 on it. */
{
char *primer3 = optionVal("primer3","/projects/compbio/usr/sugnet/local/src/primer3_0_9_test/src/primer3_core");
FILE *tmpFile = NULL;
char command[2048];
char inFile[512];
char outFile[512];
int retVal = 0;
snprintf(inFile, sizeof(inFile), "%s.tmp", cseq->name);
snprintf(outFile, sizeof(outFile), "%s.primer", cseq->name);
tmpFile = mustOpen(inFile, "w");
writeBoulderIoFormat(tmpFile, cseq);
carefulClose(&tmpFile);
snprintf(command, sizeof(command), "%s < %s > %s", primer3, inFile, outFile);
retVal = system(command);
if(retVal != 0)
    warn("pickCassettePcrPrimers::callPrimer3() - Primer3 call failed for bed %s", cseq->name);
else
    parseBoulderIo(outFile, cseq);
outputFasta(cseq, primerFa);
outputBed(cseq, primerBed);
}

void cassetteSeqFree(struct cassetteSeq **pCseq)
/* Free a cassetteSeq */
{
struct cassetteSeq *cseq = *pCseq;
dnaSeqFree(&cseq->seq);
bedFree(&cseq->bed);
freez(&cseq->name);
freez(&cseq->rightPrimer);
freez(&cseq->leftPrimer);
freez(&cseq);
pCseq = NULL;
}

void pickCassettePcrPrimers(char *db, char *bedFileName, char *primerFaName, char *primerBedName)
/* pickCassettePcrPrimers - Takes a bedFile with three exons and for each bed calls primer3 to pick primers that will detect the inclusion or exclusion of the exon.. */
{
struct bed *bed=NULL, *bedList = NULL;
FILE *primerFa = NULL;
FILE *primerBed = NULL;
struct cassetteSeq *cseq = NULL;
int targetExon = optionInt("targetExon", 1);
hSetDb(db);
bed = bedList = bedLoadAll(bedFileName);

primerFa = mustOpen(primerFaName, "w");
primerBed = mustOpen(primerBedName, "w");
for(bed=bedList; bed != NULL; bed = bed->next)
    {
    cseq = cassetteSeqFromBed(bed, targetExon);
    callPrimer3(cseq, primerFa, primerBed);
    cassetteSeqFree(&cseq);
    }
bedFreeList(&bedList);
carefulClose(&primerFa);
carefulClose(&primerBed);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
dnaUtilOpen();
pickCassettePcrPrimers(argv[1], argv[2], argv[3], argv[4]);
return 0;
}

