/** orthoEvaluate - Program to evaluate beds for:
    - Coding potenttial (Using Victor Solovyev's bestorf repeatedly.)
    - Overlap with native mRNAs/ESTs
*/
#include "common.h"
#include "hdb.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"
#include "bed.h"
#include "borf.h"
#include "dnaseq.h"
#include "fa.h"
#include "portable.h"
#include "dystring.h"
#include "altGraphX.h"
#include "bits.h"
#include "dnautil.h"

static char const rcsid[] = "$Id: orthoEvaluate.c,v 1.2 2003/06/20 20:21:36 sugnet Exp $";

static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"help", OPTION_BOOLEAN},
    {"db", OPTION_STRING},
    {"bedFile", OPTION_STRING},
    {"bestOrfExe", OPTION_STRING},
    {"tmpFa", OPTION_STRING},
    {"tmpOrf", OPTION_STRING},
    {NULL, 0}
};

static char *optionDescripts[] = 
/* Description of our options for usage summary. */
{
    "Display this message.",
    "Database (i.e. hg15) that bedFile corresponds to.",
    "File with beds to be evaluated.",
    "[optional default: borf] bestOrf executable name.",
    "[optional default: generated] temp fasta file for bestOrf.",
    "[optional default: generated] temp output file for bestOrf."
};

struct orthoEval
/* Evaluation for a single mapped mRNA. */
{
    struct orthoEval *next; /* Next in list. */
    struct bed *orthoBed;  /* Bed record created by mapping accession.*/
    struct borf *borf;     /* Results from Victors Solovyev's bestOrf program. */
    struct bed *agxBed;    /* Summary of native mRNA's and intronEsts at loci. */
    struct altGraphX *agx; /* Splicing graph at native loci. */
    struct dnaSeq *seq;    /* Genomice sequence for orthoBed. */
    int basesOverlap;      /* Number of bases overlapping with transcripts at native loci. */
    int numIntrons;        /* Number of introns in orthoBed. Should be orthoBed->exonCount -1. */
    bool *inCodInt;        /* Are the introns surrounded in orf (as defined by borf record). */
    int *orientation;      /* Orientation of an intron, -1 = reverse, 0 = not consensus, 1 = forward. */
    int *support;          /* Number of native transcripts supporting each intron. */
};

void usage()
/** Print usage and quit. */
{
int i=0;
char *version = cloneString((char*)rcsid);
char *tmp = strstr(version, "orthoEvaluate.c,v ");
if(tmp != NULL)
    version = tmp + 13;
tmp = strrchr(version, 'E');
if(tmp != NULL)
    (*tmp) = '\0';
warn("orthoEvaluate - Evaluate the coding potential of a bed.\n"
     "   (version: %s)", version );
for(i=0; i<ArraySize(optionSpecs) -1; i++)
    fprintf(stderr, "  -%s -- %s\n", optionSpecs[i].name, optionDescripts[i]);
errAbort("\nusage:\n"
	 "  not sure just yet...");
}


struct bed *loadNativeAgxBedsForBed(struct bed *orthoBed, char *table)
/* Load up all the beds that span the area in bed. */
{
struct sqlConnection *conn = hAllocConn();
int rowOffset;
struct sqlResult *sr = hRangeQuery(conn, table, orthoBed->chrom, 
	orthoBed->chromStart, orthoBed->chromEnd, NULL, &rowOffset);
char **row;
struct bed *bed = NULL, *bedList = NULL;
if(!(hTableExists(table)))
    errAbort("orthoEvaluate::loadNativeAgxBedsForBed() - Please create table '%s' before accessing",
	     table);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row + rowOffset, 12);
    if(sameString(bed->strand, orthoBed->strand))
	slAddHead(&bedList, bed);
    else
	bedFree(&bed);
    }
slSort(&bedList, bedCmp);
sqlFreeResult(&sr);
hFreeConn(&conn);
return bedList;
}

void calcBasesOverlap(struct orthoEval *ev)
/* Load the agxBed for the orthoEval and get the number of bases that 
   overlap. */
{
Bits *nativeBits = NULL;
Bits *orthoBits = NULL;
unsigned int bitSize = 0;
unsigned int minCoord = BIGNUM, maxCoord = 0;
unsigned int offSet = 0;
int i=0;
int start = 0;
int end = 0;
struct bed *bed = NULL, *orthoBed = NULL; //ev->orthoBed;
assert(ev);
assert(ev->orthoBed);
orthoBed = ev->orthoBed;

/* Load beds in this range. */
ev->agxBed = loadNativeAgxBedsForBed(ev->orthoBed, "agxBed");

/* Find the minimum and maximum coordinates spans. */
for(bed = ev->agxBed; bed != NULL; bed = bed->next)
    {
    minCoord = min(bed->chromStart, minCoord);
    maxCoord = max(bed->chromEnd, maxCoord);
    }

/* If we don't have any beds return. */
if(minCoord == BIGNUM || maxCoord == 0 || ev->agxBed == NULL)
    return;

/* Finish off getting the min and max and allocate bitmaps. */
maxCoord = max(ev->orthoBed->chromEnd, maxCoord);
minCoord = min(ev->orthoBed->chromStart, minCoord);
bitSize = maxCoord - minCoord;
nativeBits = bitAlloc(bitSize);
orthoBits = bitAlloc(bitSize);

/* Set the bits for each exon in each bed. */
for(bed=ev->agxBed; bed != NULL; bed = bed->next)
    {

    for(i=0; i<bed->blockCount; i++)
	{
	start = bed->chromStart + bed->chromStarts[i] - minCoord;
	end = bed->chromStart + bed->chromStarts[i] + bed->blockSizes[i] - minCoord;
	bitSetRange(nativeBits, start, end-start);
	}
    }

/* Set the bits for each exon in the orthoBed. */
for(i=0; i<orthoBed->blockCount; i++)
    {
    start = orthoBed->chromStart + orthoBed->chromStarts[i] - minCoord;
    end = orthoBed->chromStart + orthoBed->chromStarts[i] + orthoBed->blockSizes[i] - minCoord;
    bitSetRange(orthoBits, start, end-start);
    }

bitAnd(orthoBits, nativeBits, bitSize);
ev->basesOverlap = bitCountRange(orthoBits, 0, bitSize);
bitFree(&orthoBits);
bitFree(&nativeBits);
}


struct altGraphX *loadNativeAgxForBed(struct bed *orthoBed, char *table)
/* Load up all the altGraphX's that span the area in bed. */
{
struct sqlConnection *conn = hAllocConn();
int rowOffset;
struct sqlResult *sr = hRangeQuery(conn, table, orthoBed->chrom, 
	orthoBed->chromStart, orthoBed->chromEnd, NULL, &rowOffset);
char **row;
struct altGraphX *agx = NULL, *agxList = NULL;
if(!(hTableExists(table)))
    errAbort("orthoEvaluate::loadNativeAgxForBed() - Please create table '%s' before accessing",
	     table);
while ((row = sqlNextRow(sr)) != NULL)
    {
    agx = altGraphXLoad(row + rowOffset);
    if(sameString(orthoBed->strand, agx->strand))
	slAddHead(&agxList, agx);
    else
	altGraphXFree(&agx);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return agxList;
}

void printIntArray(int *array, int count)
{
int i;
for(i=0;i<count; i++)
    printf("%d,", array[i]);
printf("\n");
}

int agxSupportForEdge(struct altGraphX *agxList, int chromStart, int chromEnd)
/* Return the number of mRNA evidence for a given chromStart, chromEnd edge 
   in altGraphX. */
{
int vC = 0; //agx->vertexCount;
int *vPos = NULL; //agx->vPositions;
struct altGraphX *agx = NULL;
int i,j,k;
for(agx = agxList; agx != NULL; agx = agx->next)
    {
    vC = agx->vertexCount;
    vPos = agx->vPositions;
    for(i=0; i< vC; i++)
	{
	if(vPos[i] == chromStart)
	    {
	    for(j=0; j<vC; j++)
		{
		if(vPos[j] == chromEnd)
		    {
		    for(k=0; k<agx->edgeCount; k++)
			{
			if(agx->edgeStarts[k] == i && agx->edgeEnds[k] == j)
			    {
			    struct evidence *ev = slElementFromIx(agx->evidence, k);
			    return ev->evCount;
			    }
			}
		    }
		}
	    }
	}
    }
return 0;
}
    

void evaluateAnIntron(struct bed *orthoBed, int intron, struct altGraphX *agxList, struct dnaSeq *genoSeq,
		      int *orientation, int *support, bool *inCodInt)
/* Fill in the orientation, support, inCodInt values for a particular intron. */
{
struct altGraphX *agx = NULL;
unsigned int chromStart = orthoBed->chromStart + orthoBed->chromStarts[intron] + orthoBed->blockSizes[intron];
unsigned int chromEnd = orthoBed->chromStart + orthoBed->chromStarts[intron+1];
support[intron] = agxSupportForEdge(agxList, chromStart, chromEnd);
orientation[intron] = intronOrientation(genoSeq->dna - orthoBed->chromStart + chromStart,
					genoSeq->dna - orthoBed->chromStart + chromEnd);
inCodInt[intron] = (chromStart >= orthoBed->thickStart && chromEnd <= orthoBed->thickEnd);
}

void calcIntronStats(struct orthoEval *ev)
/* Calculate the overlap for each intron. */
{
struct altGraphX *agx = NULL, *agxList = NULL;
int i=0, start=0, end=0;
int numIntrons = 0;
int *support = NULL;
int *orientation = NULL;
bool *inCodInt = NULL;
struct bed *orthoBed = NULL;
struct dnaSeq *genoSeq = NULL;
assert(ev);
assert(ev->orthoBed);
orthoBed = ev->orthoBed;

/* Allocate the directories. */
if(orthoBed->blockCount == 1)
    return;
AllocArray(orientation, orthoBed->blockCount -1);
AllocArray(support, orthoBed->blockCount -1);
AllocArray(inCodInt, orthoBed->blockCount -1);
genoSeq = hChromSeq(orthoBed->chrom, orthoBed->chromStart, orthoBed->chromEnd);
agxList = loadNativeAgxForBed(orthoBed, "altGraphX");
for(i=0; i<orthoBed->blockCount -1; i++)
    evaluateAnIntron(orthoBed, i, agxList, genoSeq, orientation, support, inCodInt);
ev->agx = agxList;
ev->numIntrons = orthoBed->blockCount -1;
ev->inCodInt = inCodInt;
ev->orientation = orientation;
ev->support = support;
dnaSeqFree(&genoSeq);
}


char *mustFindLine(struct lineFile *lf, char *pat)
/* Find line that starts (after skipping white space)
 * with pattern or die trying.  Skip over pat in return*/
{
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    line = skipLeadingSpaces(line);
    if (startsWith(pat, line))
	{
	line += strlen(pat);
	return skipLeadingSpaces(line);
	}
    }
errAbort("Couldn't find %s in %s\n", pat, lf->fileName);
return NULL;
}

struct borf *convertBorfOutput(char *fileName)
/* Convert bestorf output to our database format. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line = mustFindLine(lf, "BESTORF");
char *word;
char *row[13];
struct borf *borf = NULL;
int i;
struct dnaSeq seq;
int seqSize;
AllocVar(borf);
ZeroVar(&seq);

line = mustFindLine(lf, "Seq name:");
word = nextWord(&line);
borf->name = cloneString(word);                            /* Name */
line = mustFindLine(lf, "Length of sequence:");
borf->size = atoi(line);                                /* Size */
lineFileNeedNext(lf, &line, NULL);
line = skipLeadingSpaces(line);
if (startsWith("no reliable", line))
    {
    return borf;
    }
else
    {
    line = mustFindLine(lf, "G Str F");
    lineFileSkip(lf, 1);
    lineFileRow(lf, row);
    safef(borf->strand, sizeof(borf->strand),"%s", row[1]); /* Strand. */
    borf->feature = cloneString(row[3]);                    /* Feature. */
    borf->cdsStart = lineFileNeedNum(lf, row, 4) - 1;       /* cdsStart */
    borf->cdsEnd = lineFileNeedNum(lf, row, 6);             /* cdsEnd */
    borf->score = atof(row[7]);                             /* score */
    borf->orfStart = lineFileNeedNum(lf, row, 8) - 1;       /* orfStart */
    borf->orfEnd = lineFileNeedNum(lf, row, 10);            /* orfEnd */
    borf->cdsSize = atoi(row[11]);                          /* cdsSize. */
    safef(borf->frame, sizeof(borf->frame), "%s", row[12]); /* Frame */
    line = mustFindLine(lf, "Predicted protein");
    if (!faPepSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
	errAbort("Can't find peptide in %s", lf->fileName);
    borf->protein = cloneString(seq.dna);                   /* Protein. */
    }
lineFileClose(&lf);
return borf;
}

struct borf *borfForBed(struct bed *bed)
/* borfBig - Run Victor Solovyev's bestOrf on a bed. */
{
char *exe = optionVal("bestOrfExe", "/projects/compbio/bin/bestorf.linux/bestorf /projects/compbio/bin/bestorf.linux/hume.dat");
static char *tmpFa = NULL;
static char *tmpOrf = NULL;
struct borf *borf = NULL;
struct dnaSeq *seq = NULL;
struct dyString *cmd = newDyString(256);
int retVal = 0;
if(tmpFa == NULL)
    tmpFa = optionVal("tmpFa", cloneString(rTempName("/tmp", "borf", ".fa")));
if(tmpOrf == NULL)
    tmpOrf = optionVal("tmpOrf", cloneString(rTempName("/tmp", "borf", ".out")));
seq = hSeqForBed(bed);
/* AllocVar(seq); */
/* seq->dna = cloneString("aaattaaaccagagtatgtcagtggactaaaggatgaattggacattttaattgttggaggatattggggtaaaggatcacggggtggaatgatgtctcattttctgtgtgcagtagcagagaagccccctcctggtgagaagccatctgtgtttcatactctctctcgtgttgggtctggctgcaccatgaaagaactgtatgatctgggtttgaaattggccaagtattggaagccttttcatagaaaagctccaccaagcagcattttatgtggaacagagaagccagaagtatacattgaaccttgtaattctgtcattgttcagattaaagcagcagagatcgtacccagtgatatgtataaaactggctgcaccttgcgttt"); */
/* seq->name = cloneString("HALIG4"); */
/* seq->size = strlen(seq->dna); */
faWrite(tmpFa, seq->name, seq->dna, seq->size);
dyStringClear(cmd);
dyStringPrintf(cmd, "%s %s > %s", exe, tmpFa, tmpOrf);
retVal = system(cmd->string);
borf = convertBorfOutput(tmpOrf);
remove(tmpFa);
remove(tmpOrf);
dyStringFree(&cmd);
dnaSeqFree(&seq);
return borf;
}

void setThickStartStop(struct orthoEval *oe)
/* Use the borf strucutre to insert coding start/stop. */
{
int thickStart = -1, thickEnd =-1;
int baseCount = -0;
int i=0;
struct borf *borf = oe->borf;
struct bed *bed = oe->orthoBed;
assert(bed);
assert(borf);
for(i=0; i<bed->blockCount; i++)
    {
    baseCount += bed->blockSizes[i];
    if(baseCount >= borf->cdsStart && thickStart == -1)
	thickStart = bed->chromStart + bed->chromStarts[i] + bed->blockSizes[i] - (baseCount - borf->cdsStart);
    if(baseCount >= borf->cdsEnd && thickEnd == -1)
	thickEnd = bed->chromStart + bed->chromStarts[i] + bed->blockSizes[i] - (baseCount - borf->cdsEnd) ;
    }
bed->thickStart = thickStart;
bed->thickEnd = thickEnd;
}

void orthoEvaluate(char *bedFile, char *db)
/* Score each bed in the orhtoBed file. */
{
struct bed *bedList = NULL, *bed = NULL;
struct borf *borf = NULL;
struct orthoEval *oeList = NULL, *oe = NULL;
hSetDb(db);
bedList = bedLoadNAll(bedFile, 12);
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    AllocVar(oe);
    oe->orthoBed = bed;
    oe->borf = borf = borfForBed(bed);
    setThickStartStop(oe);
    calcBasesOverlap(oe);
    calcIntronStats(oe);
    borfTabOut(borf, stdout);
    }
bedFreeList(&bedList);
}

int main(int argc, char *argv[])
/* Where it all starts. */
{
char *db = NULL;
char *bedFile = NULL;
if(argc == 1)
    usage();
optionInit(&argc, argv, optionSpecs);
if(optionExists("help"))
    usage();
db = optionVal("db", NULL);
if(db == NULL)
    errAbort("Must specify db. Use -help for usage.");
bedFile = optionVal("bedFile", NULL);
if(bedFile == NULL)
    errAbort("Must specify bedFile. Use -help for usage.");
orthoEvaluate(bedFile, db);
return 0;
}
