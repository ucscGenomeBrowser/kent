/** orthoEvaluate - Program to evaluate beds for:
    - Coding potenttial (Using Victor Solovyev's bestorf repeatedly.)
    - Overlap with native mRNAs/ESTs
    NOTE: THIS PROGRAM UNDER DEVLOPMENT. NOT FOR PRODUCTION USE!
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

static char const rcsid[] = "$Id: orthoEvaluate.c,v 1.6 2003/06/23 22:43:21 sugnet Exp $";

static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"help", OPTION_BOOLEAN},
    {"db", OPTION_STRING},
    {"bedFile", OPTION_STRING},
    {"bedOutFile", OPTION_STRING},
    {"orthoBedOut", OPTION_STRING},
    {"orthoEvalOut", OPTION_STRING},
    {"htmlFile", OPTION_STRING},
    {"htmlFrameFile", OPTION_STRING},
    {"scoreFile", OPTION_STRING},
    {"numPicks", OPTION_INT},
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
    "File to output final bed picks for track.",
    "File to output mapped orthoBeds that are used.",
    "File to output evaluation records.",
    "File for html output.",
    "[optional default: frame.html] File for html frames.",
    "[optional default: not created] Print scores for each intron. orient,support,borf,coding",
    "[optional default: 100] Number of introns to pick.",
    "[optional default: borf] bestOrf executable name.",
    "[optional default: generated] temp fasta file for bestOrf.",
    "[optional default: generated] temp output file for bestOrf."
};

struct orthoEval
/* Evaluation for a single mapped mRNA. */
{
    struct orthoEval *next; /* Next in list. */
    char *orthoBedName;     /* Name of orhtoBed. */
    struct bed *agxBed;     /* Summary of native mRNA's and intronEsts at loci. */
    struct altGraphX *agx;  /* Splicing graph at native loci. */
    struct bed *orthoBed;   /* Bed record created by mapping accession.*/
    struct borf *borf;      /* Results from Victors Solovyev's bestOrf program. */
    int basesOverlap;       /* Number of bases overlapping with transcripts at native loci. */
    char *agxBedName;       /* Name of agxBed that most overlaps, memory not owned here. */
    int numIntrons;         /* Number of introns in orthoBed. Should be orthoBed->exonCount -1. */
    int *inCodInt;         /* Are the introns surrounded in orf (as defined by borf record). */
    int *orientation;       /* Orientation of an intron, -1 = reverse, 0 = not consensus, 1 = forward. */
    int *support;           /* Number of native transcripts supporting each intron. */
    char **agxNames;        /* Names of native agx that support each intron. Memory for names not owned here. */
};

struct intronEv
/** Data about one intron. */
{
    struct intronEv *next; /* Next in list. */
    struct orthoEval *ev;  /* Parent orthoEval structure. */
    char *chrom;           /* Chromosome. */
    unsigned int           /* Exon starts and stops on chromsome. */
        e1S, e1E, 
	e2S, e2E;
    char *agxName;         /* Name of best fitting native agx record. Memory not owned here. */
    int support;           /* Number of mRNAs supporting this list. */
    int orientation;       /* Orientation of intron. */
    boolean inCodInt;         /* TRUE if in coding region, FALSE otherwise. */
    float borfScore;       /* Score for ev from borf program. */
};

static boolean doHappyDots;   /* output activity dots? */

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
	 "  not sure just yet..."
	 "NOTE: THIS PROGRAM UNDER DEVLOPMENT. NOT FOR PRODUCTION USE!\n");
}

void occassionalDot()
/* Write out a dot every 20 times this is called. */
{
static int dotMod = 100;
static int dot = 100;
if (doHappyDots && (--dot <= 0))
    {
    putc('.', stdout);
    fflush(stdout);
    dot = dotMod;
    }
}

void orthoEvalTabOut(struct orthoEval *ev, FILE *f)
/* Tab out an orthoEval record. Skipping the agxBed, and agx records. */
{
int i=0;
assert(ev);
assert(ev->orthoBedName);
assert(ev->borf);
assert(ev->agxBedName);
assert(ev->inCodInt);
assert(ev->orientation);
assert(ev->support);
assert(ev->agxNames);
fprintf(f, "%s\t", ev->orthoBedName);
bedCommaOutN(ev->orthoBed, 12, f);
fprintf(f, "\t");
borfCommaOut(ev->borf, f);
fprintf(f, "\t");
fprintf(f, "%d\t", ev->basesOverlap);
fprintf(f, "%s\t", ev->agxBedName);
fprintf(f, "%d\t", ev->numIntrons);
for(i=0; i<ev->numIntrons; i++)
    fprintf(f, "%d,", ev->inCodInt[i]);
fprintf(f, "\t");
for(i=0; i<ev->numIntrons; i++)
    fprintf(f, "%d,", ev->orientation[i]);
fprintf(f, "\t");
for(i=0; i<ev->numIntrons; i++)
    fprintf(f, "%d,", ev->support[i]);
fprintf(f, "\t");
for(i=0; i<ev->numIntrons; i++)
    fprintf(f, "%s,", ev->agxNames[i]);
fprintf(f, "\n");
}

struct orthoEval *orthoEvalLoad(char *row[])
/* Load a row of strings to an orthoEval. */
{
struct orthoEval *oe = NULL;
int sizeOne = 0;
AllocVar(oe);
oe->orthoBedName = cloneString(row[0]);
oe->orthoBed = bedCommaInN(&row[1], NULL, 12);
oe->borf = borfCommaIn(&row[2], NULL);
oe->basesOverlap = sqlUnsigned(row[3]);
oe->agxBedName = cloneString(row[4]);
oe->numIntrons = sqlUnsigned(row[5]);
sqlSignedDynamicArray(row[6], &oe->inCodInt, &sizeOne);
assert(sizeOne == oe->numIntrons);
sqlSignedDynamicArray(row[7], &oe->orientation, &sizeOne);
assert(sizeOne == oe->numIntrons);
sqlSignedDynamicArray(row[8], &oe->support, &sizeOne);
assert(sizeOne == oe->numIntrons);
sqlStringDynamicArray(row[9], &oe->agxNames, &sizeOne);
assert(sizeOne == oe->numIntrons);
return oe;
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
   overlap. Fill in the primary agxBed name with agxBed record that
   overlaps most. */
{
Bits *nativeBits = NULL;
Bits *orthoBits = NULL;
unsigned int bitSize = 0;
unsigned int minCoord = BIGNUM, maxCoord = 0;
unsigned int offSet = 0;
int i=0;
int start = 0;
int end = 0;
int maxOverlap = 0;
int currentOverlap =0;
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

/* Set the bits for each exon in the orthoBed. */
for(i=0; i<orthoBed->blockCount; i++)
    {
    start = orthoBed->chromStart + orthoBed->chromStarts[i] - minCoord;
    end = orthoBed->chromStart + orthoBed->chromStarts[i] + orthoBed->blockSizes[i] - minCoord;
    bitSetRange(orthoBits, start, end-start);
    }

/* Set the bits for each exon in each bed. */
for(bed=ev->agxBed; bed != NULL; bed = bed->next)
    {
    for(i=0; i<bed->blockCount; i++)
	{
	start = bed->chromStart + bed->chromStarts[i] - minCoord;
	end = bed->chromStart + bed->chromStarts[i] + bed->blockSizes[i] - minCoord;
	bitSetRange(nativeBits, start, end-start);
	bitAnd(nativeBits, orthoBits, bitSize);
	currentOverlap = bitCountRange(orthoBits, 0, bitSize);
	ev->basesOverlap += currentOverlap;
	if(currentOverlap >= maxOverlap)
	    {
	    freez(&ev->agxBedName);
	    ev->agxBedName = cloneString(bed->name);
	    }
	bitClear(nativeBits, bitSize);
	}
    }
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
/* Useful in gdb to see what is in an array. */
{
int i;
for(i=0;i<count; i++)
    printf("%d,", array[i]);
printf("\n");
}

int numInArray(int *array, int count, int val)
/* Return the index in the array that val is found at
   or -1 if not found. */
{
int i=0;
for(i=0; i<count; i++)
    {
    if(array[i] == val)
	return i;
    }
return -1;
}

int agxSupportForSplice(struct altGraphX *agx, struct bed *bed, int intron)
/* Check to see if there is an set of edges in agx that
   support the exon, intron, exon juction centered on the intron
   in bed. */
{
bool **em = NULL;            /* Edge matrix for splicing graph. */
int support = 0;             /* Number of mRNAs/ESTs supporting intron. */
int e1Start=0, e1End=0,      /* Coordinates of exons. */
    e2Start=0, e2End=0; 
int e1VS=-1, e1VE=-1,        /* Vertex numbers in agx graph. */
    e2VS=-1, e2VE=-1;     

/* --- Convenient shorthand for bits of agx --- */
int vC = 0;            /* Number of vertices. */
int *vPos = NULL;      /* Positions of vertices. */
unsigned char *vTypes = NULL;    /* Types of vertices. */
struct evidence *ev = NULL; /* Evidence for edges. */

/* Sanity checks. */
assert(agx);
assert(bed);
assert(intron < bed->blockCount && intron >= 0);

/* Initialization. */
vC = agx->vertexCount;
vPos = agx->vPositions;
vTypes = agx->vTypes;

/* Get chromsomal starts/ends for exons. */
e1Start = bed->chromStart + bed->chromStarts[intron];
e1End = e1Start + bed->blockSizes[intron];
e2Start = bed->chromStart + bed->chromStarts[intron+1];
e2End = e2Start + bed->blockSizes[intron+1];

/* Find them in the graph if possible. */
e1VS = numInArray(vPos, vC, e1Start);
e1VE = numInArray(vPos, vC, e1End);
e2VS = numInArray(vPos, vC, e2Start);
e2VE = numInArray(vPos, vC, e2End);

/* If we found them get the support for each edge and sum them. */
if(e1VS == -1 || e1VE == -1 || e1VS == -1 || e1VE == -1)
    return 0;
em = altGraphXCreateEdgeMatrix(agx);
if(em[e1VS][e1VE] && em[e1VE][e2VS] && em[e2VS][e2VE])
    {
    int edgeNum = -1;
    edgeNum = altGraphXGetEdgeNum(agx, e1VS, e1VE);
    ev = slElementFromIx(agx->evidence, edgeNum);
    support += ev->evCount;

    edgeNum = altGraphXGetEdgeNum(agx, e1VE, e2VS);
    ev = slElementFromIx(agx->evidence, edgeNum);
    support += ev->evCount;

    edgeNum = altGraphXGetEdgeNum(agx, e2VS, e2VE);
    ev = slElementFromIx(agx->evidence, edgeNum);
    support += ev->evCount;
    }
altGraphXFreeEdgeMatrix(&em, vC);
return support;
}

int agxSupportForEdge(struct altGraphX *agxList, struct bed *bed, int intron, 
		      int *support, char **agxNames)
/* Scroll through native splicing graphs looking for native support of
   intron in bed if support is found fill in the support array and
   name array. */
{
struct altGraphX *agx = NULL;
int ev = 0;

for(agx = agxList; agx != NULL; agx = agx->next)
    {
    ev = agxSupportForSplice(agx, bed, intron);
    if(ev > 0)
	{
	support[intron] = ev;
	agxNames[intron] = agx->name;
	return;
	}
    }
}

/* int agxSupportForEdge(struct altGraphX *agxList, int chromStart, int chromEnd) */
/* /\* Return the number of mRNA evidence for a given chromStart, chromEnd edge  */
/*    in altGraphX. *\/ */
/* { */
/* int vC = 0; //agx->vertexCount; */
/* int *vPos = NULL; //agx->vPositions; */
/* struct altGraphX *agx = NULL; */
/* int i,j,k; */
/* for(agx = agxList; agx != NULL; agx = agx->next) */
/*     { */
/*     vC = agx->vertexCount; */
/*     vPos = agx->vPositions; */
/*     for(i=0; i< vC; i++) */
/* 	{ */
/* 	if(vPos[i] == chromStart) */
/* 	    { */
/* 	    for(j=0; j<vC; j++) */
/* 		{ */
/* 		if(vPos[j] == chromEnd) */
/* 		    { */
/* 		    for(k=0; k<agx->edgeCount; k++) */
/* 			{ */
/* 			if(agx->edgeStarts[k] == i && agx->edgeEnds[k] == j) */
/* 			    { */
/* 			    struct evidence *ev = slElementFromIx(agx->evidence, k); */
/* 			    return ev->evCount; */
/* 			    } */
/* 			} */
/* 		    } */
/* 		} */
/* 	    } */
/* 	} */
/*     } */
/* return 0; */
/* } */
    

void evaluateAnIntron(struct orthoEval *ev,
		      struct bed *orthoBed, int intron, struct altGraphX *agxList, struct dnaSeq *genoSeq,
		      int *orientation, int *support, int *inCodInt, char **agxNames)
/* Fill in the orientation, support, inCodInt values for a particular intron. */
{
struct altGraphX *agx = NULL;
unsigned int chromStart = orthoBed->chromStart + orthoBed->chromStarts[intron] + orthoBed->blockSizes[intron];
unsigned int chromEnd = orthoBed->chromStart + orthoBed->chromStarts[intron+1];
agxNames[intron] = ev->orthoBedName;
agxSupportForEdge(agxList, orthoBed, intron, support, agxNames);
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
int *inCodInt = NULL;
char **agxNames = NULL;
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
AllocArray(agxNames, orthoBed->blockCount -1);
genoSeq = hChromSeq(orthoBed->chrom, orthoBed->chromStart, orthoBed->chromEnd);
agxList = loadNativeAgxForBed(orthoBed, "altGraphX");
for(i=0; i<orthoBed->blockCount -1; i++)
    evaluateAnIntron(ev, orthoBed, i, agxList, genoSeq, orientation, support, inCodInt, agxNames);
ev->agx = agxList;
ev->numIntrons = orthoBed->blockCount -1;
ev->inCodInt = inCodInt;
ev->orientation = orientation;
ev->support = support;
ev->agxNames = agxNames;
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
char *word=NULL;
char *row[13];
struct borf *borf = NULL;
int i=0;
struct dnaSeq seq;
int seqSize = 0;
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

void setThickStartStop(struct orthoEval *ev)
/* Use the borf strucutre to insert coding start/stop. */
{
int thickStart = -1, thickEnd =-1;
int baseCount = -0;
int i=0;
struct borf *borf = ev->borf;
struct bed *bed = ev->orthoBed;
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

struct orthoEval *scoreOrthoBeds(char *bedFile, char *db)
/* Score each bed in the orhtoBed file. */
{
struct bed *bedList = NULL, *bed = NULL;
struct borf *borf = NULL;
struct orthoEval *evList = NULL, *ev = NULL;
hSetDb(db);
bedList = bedLoadNAll(bedFile, 12);
warn("Scoring beds");
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    occassionalDot();
    AllocVar(ev);
    ev->orthoBedName = cloneString(bed->name);
    ev->agxBedName = cloneString(bed->name);
    ev->orthoBed = bed;
    ev->borf = borf = borfForBed(bed);
    setThickStartStop(ev);
    calcBasesOverlap(ev);
    calcIntronStats(ev);
//    borfTabOut(borf, stdout);
    slAddHead(&evList, ev);
    }
warn("\nDone.");
//bedFreeList(&bedList);
return evList;
}

struct intronEv *intronIvForEv(struct orthoEval *ev, int intron)
/* Return an intronEv record for a particular intron. */
{
struct intronEv *iv = NULL;
struct bed *bed = NULL;
assert(ev);
assert(ev->orthoBed);
assert(intron < ev->orthoBed->blockCount - 1);
bed = ev->orthoBed;
AllocVar(iv);
iv->ev = ev;
iv->chrom = bed->chrom;

iv->e1S = bed->chromStart + bed->chromStarts[intron];
iv->e1E = iv->e1S + bed->blockSizes[intron];
iv->e2S = bed->chromStart + bed->chromStarts[intron+1];
iv->e2E = iv->e2S + bed->blockSizes[intron+1];

iv->agxName = ev->agxNames[intron];
iv->support = ev->support[intron];
iv->orientation = ev->orientation[intron];
iv->inCodInt = ev->inCodInt[intron];
iv->borfScore = ev->borf->score;
return iv;
}

int scoreForIntronEv(const struct intronEv *iv)
/* Score function based on native support, orientation, and coding. */
{
static FILE *out = NULL;
int score = 0;
char *scoreFile = optionVal("scoreFile", NULL);
if(out == NULL && scoreFile != NULL)
    out = mustOpen(scoreFile, "w");
if(out != NULL)
    fprintf(out, "%d\t%d\t%f\t%d\n", iv->orientation, iv->support, iv->borfScore, iv->inCodInt);
if(iv->orientation != 0)
    score += 5;
score += iv->support *2;
score += (float) iv->borfScore/5;
if(iv->inCodInt)
    score = score +2;
return score;
}

boolean checkMgcPicks(struct intronEv *iv)
/* Try to look up an mgc pick in this area. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
int rowOffset = 0;
char *mgcTable = optionVal("mgcTable", "chuckMgcPicked");
char *mgcGenes = optionVal("mgcGenes", "chuckMgcGenes");
char *mgcAList = optionVal("mgcAList", "chuckMgcAList");
boolean foundSome = FALSE;
sr = hRangeQuery(conn, mgcTable, iv->chrom, iv->e1S, iv->e2E, NULL, &rowOffset); 
if(sqlNextRow(sr) != NULL)
    foundSome = TRUE;
sqlFreeResult(&sr);
if(foundSome == FALSE)
    {
    sr = hRangeQuery(conn, mgcGenes, iv->chrom, iv->e1S, iv->e2E, NULL, &rowOffset); 
    if(sqlNextRow(sr) != NULL)
	foundSome = TRUE;
    sqlFreeResult(&sr);
    }
if(foundSome == FALSE)
    {
    sr = hRangeQuery(conn, mgcAList, iv->chrom, iv->e1S, iv->e2E, NULL, &rowOffset); 
    if(sqlNextRow(sr) != NULL)
	foundSome = TRUE;
    sqlFreeResult(&sr);
    }

hFreeConn(&conn);
return foundSome;
}

boolean isOverlappedByRefSeq(struct intronEv *iv)
/* Return TRUE if there is a refSeq overlapping. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
int rowOffset = 0;
char *refSeqTable = "refGene";
boolean foundSome = FALSE;
sr = hRangeQuery(conn, refSeqTable, iv->chrom, iv->e1S, iv->e2E, NULL, &rowOffset); 
if(sqlNextRow(sr) != NULL)
    foundSome = TRUE;
sqlFreeResult(&sr);
hFreeConn(&conn);
return foundSome;
}

int intronEvalCmp(const void *va, const void *vb)
/* Compare to sort based score function. */
{
const struct intronEv *a = *((struct intronEv **)va);
const struct intronEv *b = *((struct intronEv **)vb);
float aScore = scoreForIntronEv(a);
float bScore = scoreForIntronEv(b);
return bScore - aScore; /* reverse to get largest values first. */
}

boolean isUniqueCoordAndAgx(struct intronEv *iv, struct hash *posHash, struct hash *agxHash)
/* Return TRUE if iv isn't in posHash and agxHash.
   Return FALSE otherwise. */
{
static char key[1024];
boolean unique = TRUE;

safef(key, sizeof(key), "%s-%d-%d-%d-%d", iv->chrom, iv->e1S, iv->e1E, iv->e2S, iv->e2E);
/* Unique location (don't pick same intron twice. */
if(hashFindVal(posHash, key) == NULL)
    hashAdd(posHash, key, iv);
else 
    unique = FALSE;

/* Unique loci, don't pick from same overall loci if possible. */
safef(key, sizeof(key), "%s", iv->agxName);
if(hashFindVal(agxHash, key) == NULL)
    hashAdd(agxHash, key, iv);
else
    unique = FALSE;

/* Definitely don't pick from same mRNA. */
safef(key, sizeof(key), "%s", iv->ev->orthoBedName);
if(hashFindVal(agxHash, key) == NULL)
    hashAdd(agxHash, key, iv);
else
    unique = FALSE;

if(unique)
    unique = !checkMgcPicks(iv);

return unique;
}

struct bed *bedForIv(struct intronEv *iv)
/* Make a bed to represent a intronEv structure. */
{
struct bed *bed = NULL;
AllocVar(bed);
bed->chrom = cloneString(iv->chrom);
bed->chromStart = bed->thickStart = iv->e1S;
bed->chromEnd = bed->thickEnd = iv->e2E;
bed->name = cloneString(iv->ev->orthoBed->name);
if(iv->orientation == 1)
    safef(bed->strand, sizeof(bed->strand), "+");
else if(iv->orientation == -1)
    safef(bed->strand, sizeof(bed->strand), "-");
else 
    safef(bed->strand, sizeof(bed->strand), "%s", iv->ev->orthoBed->strand);
bed->blockCount = 2;
AllocArray(bed->chromStarts, 2);
bed->chromStarts[0] = iv->e1S - bed->chromStart;
bed->chromStarts[1] = iv->e2S - bed->chromStart;
AllocArray(bed->blockSizes, 2);
bed->blockSizes[0] = iv->e1E - iv->e1S;
bed->blockSizes[1] = iv->e2E - iv->e2S;
bed->score = scoreForIntronEv(iv);
return bed;
}

void writeOutFrames(FILE *htmlOut, char *fileName, char *db)
{
fprintf(htmlOut, "<html><head><title>Introns and flanking exons for RACE PCR</title></head>\n"
	"<frameset cols=\"18%,82%\">\n"
	"<frame name=\"_list\" src=\"./%s\">\n"
	"<frame name=\"browser\" src=\"http://mgc.cse.ucsc.edu/cgi-bin/hgTracks?db=%s&position=chrX:151171710-151173193&"
	"hgt.customText=http://www.soe.ucsc.edu/~sugnet/tmp/mgcIntron.bed\">\n"
	"</frameset>\n"
	"</html>\n", fileName, db);
}

void orthoEvaluate(char *bedFile, char *db)
/* Create orthoEval records and find best introns
   where best is:
   1) Supported by native transcripts.
   2) Has conserved intron gt-ag.
   3) Is in coding region.
*/
{
struct orthoEval *evList = NULL, *ev = NULL;
struct intronEv *ivList = NULL, *iv = NULL;
struct hash *posHash = newHash(12), *agxHash = newHash(12);
char *htmlOutName= NULL, *bedOutName = NULL;
char *htmlFrameName=NULL, *orthoBedOutName=NULL, *orthoEvalOutName=NULL;
FILE *htmlOut=NULL, *bedOut=NULL, *htmlFrameOut=NULL;
FILE *orthoBedOut = NULL, *orthoEvalOut = NULL;
struct bed *bed = NULL;
int maxPicks = optionInt("numPicks", 100);
int i=0;
boolean isRefSeq=FALSE;
htmlOutName = optionVal("htmlFile", NULL);
bedOutName = optionVal("bedOutFile", NULL);
orthoBedOutName = optionVal("orthoBedOut", NULL);
orthoEvalOutName = optionVal("orthoEvalOut", NULL);
htmlFrameName = optionVal("htmlFrameFile", "frame.html");
if(orthoBedOutName == NULL)
    errAbort("Please specify an orthoBedOut file. Use -help for usage.");
if(orthoEvalOutName == NULL)
    errAbort("Please specify an orthoEvalOut file. Use -help for usage.");
if(htmlOutName == NULL)
    errAbort("Please specify an htmlFile. Use -help for usage.");
if(bedOutName == NULL)
    errAbort("Please specify an bedOutFile. Use -help for usage.");
evList = scoreOrthoBeds(bedFile, db);
warn("Creating intron records");
for(ev = evList; ev != NULL; ev = ev->next)
    {
    for(i=0; i<ev->numIntrons; i++)
	{
	occassionalDot();
	iv = intronIvForEv(ev, i);
	slAddHead(&ivList, iv);
	}
    }
warn("\nDone");
warn("Sorting");
slSort(&ivList, intronEvalCmp);
htmlOut = mustOpen(htmlOutName, "w");
bedOut = mustOpen(bedOutName, "w");
htmlFrameOut = mustOpen(htmlFrameName, "w");
orthoBedOut = mustOpen(orthoBedOutName, "w");
orthoEvalOut = mustOpen(orthoEvalOutName, "w");
writeOutFrames(htmlFrameOut, htmlOutName, db);
fprintf(htmlOut, "<html><body><table border=1><tr><th>Mouse Acc.</th><th>Score</th><th>RefSeq</th></tr>\n");
warn("Writing out");
for(iv = ivList; iv != NULL && maxPicks > 0; iv = iv->next)
    {
    if(isUniqueCoordAndAgx(iv, posHash, agxHash))
	{
	bed = bedForIv(iv);
	isRefSeq = isOverlappedByRefSeq(iv);
	fprintf(htmlOut, "<tr><td><a target=\"browser\" "
		"href=\"http://mgc.cse.ucsc.edu/cgi-bin/hgTracks?db=hg15&position=%s:%d-%d\"> "
		"%s </a></td><td>%d</td><td>%s</td></tr>\n", 
		bed->chrom, bed->chromStart-40, bed->chromEnd+50, bed->name, bed->score, isRefSeq ? "yes" : "no");
	bedTabOutN(bed, 12, bedOut);
	bedTabOutN(iv->ev->orthoBed, 12, orthoBedOut);
	orthoEvalTabOut(iv->ev, orthoEvalOut);
	bedFree(&bed);
	maxPicks--;
	}
    }

fprintf(htmlOut, "</table></body></html>\n");
carefulClose(&bedOut);
carefulClose(&htmlOut);
carefulClose(&htmlFrameOut);
carefulClose(&orthoBedOut);
carefulClose(&orthoEvalOut);
warn("Done.");
hashFree(&posHash);
hashFree(&agxHash);
}
	
int main(int argc, char *argv[])
/* Where it all starts. */
{
char *db = NULL;
char *bedFile = NULL;
doHappyDots = isatty(1);  /* stdout */
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
