/* gsBig - Run Genscan on big input and produce GTF files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "portable.h"
#include "fa.h"
#include "dystring.h"
#include "bits.h"

char *exePath = "/projects/compbio/bin/genscan-linux/genscan";
char *parPath = "/projects/compbio/bin/genscan-linux/HumanIso.smat";

int winSize = 1200000;	/* Size of window to pass to genscan. */
int stepSize;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gsBig - Run Genscan on big input and produce GTF files and other parsed output\n"
  "usage:\n"
  "   gsBig file.fa output.gtf\n"
  "options:\n"
  "   -subopt=output.bed - Produce suboptimal exons.\n"
  "   -trans=output.fa - where to put translated proteins.\n"
  "   -prerun=input.genscan - Assume genscan run already with this output.\n"
  "   -window=size    Set window to pass to genscan specific size (default 1200000)\n"
  "                   You want ~400 bytes memory for each base in window."
  );
}

struct genScanGene
/* Info about a genscan gene. */
    {
    struct genScanGene *next;	/* Next in list. */
    int id;			/* Gene id. */
    char strand;		/* Strand. */
    int start, end;		/* Start/end in genome. */
    struct genScanFeature *featureList;	/* List of exons and other features. */
    AA *translation;		/* Translated sequence. */
    };

struct genScanFeature
/* INfo about a genscan exon. */
    {
    struct genScanFeature *next;	/* Next in list. */
    char *name;			/* Name - format is 1.01, 1.02, etc. */
    int geneId;			/* Gene number. */
    int featId;			/* Feature (exon) id */
    char strand;		/* Strand (+ or -) */
    char *type;			/* Type. */
    int start;			/* Start in genome. */
    int end;			/* End in genome. */
    int frame;			/* Frame: 0,1,2 */
    int phase;			/* Phase 0, 1, or 2. */
    int iac;			/* ?? */
    int dot;			/* ?? */
    int codRg;			/* ?? */
    double p;			/* Probability. */
    double tScore;		/* ?? */
    };

struct genScanFeature *parseGenscanLine(struct lineFile *lf, char *line)
/* Parse a single line. */
{
char *words[16], *parts[3];
int wordCount, partCount;
char *type;
struct genScanFeature *gsf;
boolean isLong = FALSE;
int size;

wordCount = chopLine(line, words);
if (wordCount < 2)
    errAbort("Expecting at least 2 words line %d of %s", lf->lineIx, lf->fileName);
type = words[1];
if (sameString(type, "PlyA") || sameString(type, "Prom"))
    {
    lineFileExpectWords(lf, 7, wordCount);
    }
else if (sameString(type, "Init") || sameString(type, "Intr") || sameString(type, "Term") || sameString(type, "Sngl"))
    {
    lineFileExpectWords(lf, 13, wordCount);
    isLong = TRUE;
    }
else
    {
    errAbort("Unrecognized type %s line %d of %s", type, lf->lineIx, lf->fileName);
    }
AllocVar(gsf);
gsf->name = cloneString(words[0]);
partCount = chopString(words[0], ".", parts, ArraySize(parts));
if (partCount != 2 || (parts[0][0] != 'S' && !isdigit(parts[0][0])) || !isdigit(parts[1][0]))
    errAbort("Expecting N.NN field 1 line %d of %s", lf->lineIx, lf->fileName);
gsf->geneId = atoi(parts[0]);
gsf->featId = atoi(parts[1]);
gsf->type = cloneString(type);
gsf->strand = words[2][0];
if (gsf->strand == '-')
    {
    gsf->start = lineFileNeedNum(lf, words, 4) - 1;
    gsf->end = lineFileNeedNum(lf, words, 3);
    }
else
    {
    gsf->start = lineFileNeedNum(lf, words, 3) - 1;
    gsf->end = lineFileNeedNum(lf, words, 4);
    }
size = lineFileNeedNum(lf, words, 5);
if (size != gsf->end - gsf->start)
    errAbort("Len doesn't match Begin to End line %d of %s", lf->lineIx, lf->fileName);

if (isLong)
    {
    gsf->frame = lineFileNeedNum(lf, words, 6);
    gsf->phase = lineFileNeedNum(lf, words, 7);
    gsf->iac = lineFileNeedNum(lf, words, 8);
    gsf->dot = lineFileNeedNum(lf, words, 9);
    gsf->codRg = lineFileNeedNum(lf, words, 10);
    gsf->p = atof(words[11]);
    gsf->tScore = atof(words[12]);
    }
else
    gsf->tScore = atof(words[6]);
return gsf;
}

struct segment
/* A segment of the genome. */
    {
    struct segment *next;
    int start,end;	/* Start/end position. */
    struct genScanGene *geneList;	/* List of genes. */
    struct genScanFeature *suboptList;	/* Suboptimal exons. */
    };

char *skipTo(struct lineFile *lf, char *lineStart)
/* Skip to a line that starts with lineStart.  Return this line
 * or NULL if none such. */
{
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    if (startsWith(lineStart, line))
        return line;
    }
return NULL;
}

char *mustSkipTo(struct lineFile *lf, char *lineStart)
/* Skip to line that starts with lineStart or die. */
{
char *line = skipTo(lf, lineStart);
if (line == NULL)
   errAbort("Couldn't find a line that starts with '%s' in %s", lineStart, lf->fileName);
return line;
}

struct genScanGene *bundleGenes(struct genScanFeature *gsfList)
/* Bundle together features into genes.   Cannibalizes gsfList. */
{
struct genScanFeature *gsf, *nextGsf;
struct genScanGene *geneList = NULL, *gene = NULL;

for (gsf = gsfList; gsf != NULL; gsf = nextGsf)
    {
    nextGsf = gsf->next;
    if (gene == NULL || gene->id != gsf->geneId)
        {
	AllocVar(gene);
	slAddHead(&geneList, gene);
	gene->id = gsf->geneId;
	gene->strand = gsf->strand;
	gene->start = gsf->start;
	gene->end = gsf->end;
	}
    slAddTail(&gene->featureList, gsf);
    if (gsf->start < gene->start) gene->start = gsf->start;
    if (gsf->end > gene->end) gene->end = gsf->end;
    }
slReverse(&geneList);
return geneList;
}

struct segment *parseSegment(char *fileName, int start, int end, char *retSeqName)
/* Read in a genscan file into segment. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct segment *seg;
char *line;
struct genScanFeature *gsfList = NULL, *gsf;
struct genScanGene *gsg;
char *words[2];

if (!lineFileNext(lf, &line, NULL))
    errAbort("%s is empty", fileName);
if (!startsWith("GENSCAN ", line))
    errAbort("%s is not a GENSCAN output file");
if (retSeqName != NULL)
    {
    line = mustSkipTo(lf, "Sequence");
    if (chopLine(line, words) < 2)
        errAbort("Expecting sequence name line %d of %s", lf->lineIx, lf->fileName);
    strcpy(retSeqName, words[1]);
    }

mustSkipTo(lf, "Predicted genes/exons");
mustSkipTo(lf, "Gn.Ex");
mustSkipTo(lf, "-----");
AllocVar(seg);
seg->start = start;
seg->end = end;

for (;;)
    {
    if (!lineFileNext(lf, &line, NULL))
        break;
    line = skipLeadingSpaces(line);
    if (line == NULL || line[0] == 0)
        continue;
    if (!isdigit(line[0]))
	{
	lineFileReuse(lf);
        break;
	}
    gsf = parseGenscanLine(lf, line);
    slAddHead(&gsfList, gsf);
    }
slReverse(&gsfList);
printf("Got %d exons\n", slCount(gsfList));
seg->geneList = bundleGenes(gsfList);
gsfList = NULL;
printf("Got %d genes\n", slCount(seg->geneList));

if (!lineFileNext(lf, &line, NULL))
    errAbort("Unexpected end of file in %s", lf->fileName);
if (startsWith("Suboptimal exons", line))
    {
    mustSkipTo(lf, "-----");
    for (;;)
	{
	if (!lineFileNext(lf, &line, NULL))
	    break;
	line = skipLeadingSpaces(line);
	if (line == NULL || line[0] == 0)
	    continue;
	if (!startsWith("S.", line))
	    break;
	gsf = parseGenscanLine(lf, line);
	slAddHead(&gsfList, gsf);
	}
    slReverse(&gsfList);
    seg->suboptList = gsfList;
    printf("Got %d suboptimal exons\n", slCount(seg->suboptList));
    }
lineFileReuse(lf);

mustSkipTo(lf, "Predicted peptide sequence");
if ((line = skipTo(lf, ">")) != NULL)
    {
    lineFileReuse(lf);
    for (gsg = seg->geneList; gsg != NULL; gsg = gsg->next)
        {
	aaSeq seq;
	if (!faPepSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
	    errAbort("Not enough predicted peptides in %s\n", lf->fileName);
	gsg->translation = cloneString(seq.dna);
	}
    }

lineFileClose(&lf);
return seg;
}

void cdsOut(FILE *f, struct genScanFeature *gsf, char *geneName, char *seqName)
/* Output a CDS type GTF feature to f. */
{
fprintf(f, "%s\tgenscan\texon\t%d\t%d\t", seqName, gsf->start+1, gsf->end);
fprintf(f, ".\t%c\t%d\t", gsf->strand, round(gsf->p * 1000));
fprintf(f, "gene_id \"%s\"; transcript_id \"%s\"; exon_number \"%d\"; exon_id \"%s.%d\";\n",
    geneName, geneName, gsf->featId, geneName, gsf->featId);
}

void writeSeg(char *seqName, struct segment *seg, FILE *gtf, FILE *sub, FILE *trans)
/* Write out gtf and bed files. */
{
struct genScanGene *gene;
struct genScanFeature *gsf;

for (gene = seg->geneList; gene != NULL; gene = gene->next)
    {
    char geneName[128];
    sprintf(geneName, "%s.%d", seqName, gene->id);
    for (gsf = gene->featureList; gsf != NULL; gsf = gsf->next)
        {
	if (sameString("Init", gsf->type))
	    {
	    cdsOut(gtf, gsf, geneName, seqName);
	    }
	else if (sameString("Intr", gsf->type))
	    {
	    cdsOut(gtf, gsf, geneName, seqName);
	    }
	else if (sameString("Term", gsf->type))
	    {
	    cdsOut(gtf, gsf, geneName, seqName);
	    }
	else if (sameString("Sngl", gsf->type))
	    {
	    cdsOut(gtf, gsf, geneName, seqName);
	    }
	}
    if (trans != NULL)
        {
	faWriteNext(trans, geneName, gene->translation, strlen(gene->translation));
	}
    }

if (sub != NULL)
    {
    for (gsf = seg->suboptList; gsf != NULL; gsf = gsf->next)
        {
	fprintf(sub, "%s\t%d\t%d\t%s.%d\t%d\t%c\n",
	    seqName, gsf->start, gsf->end, seqName, gsf->featId,
	    round(1000*gsf->p), gsf->strand);
	}
    }
}

void offsetGenesInSeg(struct segment *seg)
/* Offset all genes in seg by seg->offset. */
{
struct genScanGene *gene;
struct genScanFeature *gsf;
int offset = seg->start;

for (gene = seg->geneList; gene != NULL; gene = gene->next)
    {
    gene->start += offset;
    gene->end += offset;
    for (gsf = gene->featureList; gsf != NULL; gsf = gsf->next)
	{
        gsf->start += offset;
	gsf->end += offset;
	}
    }
for (gsf = seg->suboptList; gsf != NULL; gsf = gsf->next)
    {
    gsf->start += offset;
    gsf->end += offset;
    }
}

void setOverlapBits(int s, int e, int overlapStart, int overlapEnd, Bits *bits)
/* Set part of bits corresponding to where s-e overlaps with overlapStart-End */
{
s = max(s, overlapStart);
e = min(e, overlapEnd);
if (s < e)
    bitSetRange(bits, s - overlapStart, e - s);
}

void featuresToBits(struct genScanFeature *gsfList, Bits *bits, int overlapStart, int overlapEnd)
/* Write ranges covered by features to bitfield. */
{
struct genScanFeature *gsf;
for (gsf = gsfList; gsf != NULL; gsf = gsf->next)
    setOverlapBits(gsf->start, gsf->end, overlapStart, overlapEnd, bits);
}

void genesToBits(struct genScanGene *geneList, Bits *bits, int overlapStart, int overlapEnd, boolean splitGene)
/* Write ranges covered by gene to bitfield. */
{
struct genScanGene *gene;
int s, e, size;

for (gene = geneList; gene != NULL; gene = gene->next)
    {
    if (splitGene)
	featuresToBits(gene->featureList, bits, overlapStart, overlapEnd);
    else
	setOverlapBits(gene->start, gene->end, overlapStart, overlapEnd, bits);
    }
}

int findCrossover(Bits *bits, int overlapStart, int overlapEnd)
/* Search from middle of overlap until find a clear spot. 
 * Return -1 if no such spot*/
{
int i, offset;
int size = overlapEnd - overlapStart;
int halfSize = size/2;
int halfPlus = halfSize+1;

for (i=0; i<=halfPlus; ++i)
    {
    offset = halfSize + i;
    if (offset < size)
        if (!bitReadOne(bits, offset))
	    return offset+overlapStart;
    offset = halfSize - i;
    if (offset >= 0)
        if (!bitReadOne(bits, offset))
	    return offset+overlapStart;
    }
return -1;
}

void calcGeneBounds(struct genScanGene *gene)
/* Calculate start/end of gene from start/end of features. */
{
struct genScanFeature *gsf;
gene->start = BIGNUM;
gene->end = -BIGNUM;
for (gsf = gene->featureList; gsf != NULL; gsf = gsf->next)
    {
    if (gene->start > gsf->start) gene->start = gsf->start;
    if (gene->end < gsf->end) gene->end = gsf->end;
    }
}

struct genScanFeature *removeFeaturesOutside(int start, int end, struct genScanFeature *gsfList)
/* Return list of features only including those between start and end.  Trim
 * features if necessary if they partially overlap. */
{
struct genScanFeature *gsf, *nextGsf, *newList = NULL;
int s, e;
for (gsf = gsfList; gsf != NULL; gsf = nextGsf)
    {
    nextGsf = gsf->next;
    s = max(gsf->start, start);
    e = min(gsf->end, end);
    if (s < e)
        {
	gsf->start = s;
	gsf->end = e;
	slAddHead(&newList, gsf);
	}
    }
slReverse(&newList);
return newList;
}

void removeOutside(int start, int end, struct segment *seg)
/* Remove parts of seg outside of range start-end. */
{
struct genScanGene *gene, *nextGene, *geneList = NULL;

for (gene = seg->geneList; gene != NULL; gene = nextGene)
    {
    nextGene = gene->next;
    if (rangeIntersection(start, end, gene->start, gene->end))
        {
	slAddHead(&geneList, gene);
	gene->featureList = removeFeaturesOutside(start, end, gene->featureList);
	calcGeneBounds(gene);
	}
    }
slReverse(&geneList);
seg->geneList = geneList;
seg->suboptList = removeFeaturesOutside(start, end, seg->suboptList);
}

boolean cutBetween(struct segment *a, struct segment *b, int overlapStart, int overlapEnd, 
	int overlapSize, boolean splitGene, int crossover)
/* Try and cut out redundant parts where a and b overlap.   
 * Don't cut a gene unless splitGene is true.   Don't
 * cut a feature unless crossover point is specified (> 0) */
{
if (crossover < 0)
    {
    Bits *bits = bitAlloc(overlapSize);
    genesToBits(a->geneList, bits, overlapStart, overlapEnd, splitGene);
    genesToBits(b->geneList, bits, overlapStart, overlapEnd, splitGene);
    featuresToBits(a->suboptList, bits, overlapStart, overlapEnd);
    featuresToBits(b->suboptList, bits, overlapStart, overlapEnd);
    crossover = findCrossover(bits, overlapStart, overlapEnd);
    bitFree(&bits);
    }
if (crossover >= 0)
    {
    removeOutside(0, crossover, a);
    removeOutside(crossover, BIGNUM, b);
    return TRUE;
    }
else
    return FALSE;
}

void mergeTwo(struct segment *a, struct segment *b)
/* Figure out where to merge between a and b. */
{
int overlapStart = b->start;
int overlapEnd = a->end;
int overlapSize = overlapEnd - overlapStart;

if (overlapSize <= 0)
    errAbort("No overlap between a and b in mergeTwo");
if (!cutBetween(a, b, overlapStart, overlapEnd, overlapSize, FALSE, -1))
    if (!cutBetween(a, b, overlapStart, overlapEnd, overlapSize, TRUE, -1))
        cutBetween(a, b, overlapStart, overlapEnd, overlapSize, TRUE, (overlapStart+overlapEnd)/2);
}

struct segment *mergeSegs(struct segment *segList)
/* Return a segment that is merged from all segments in list. */
{
struct segment *seg, *lastSeg = NULL, *merged;
int geneId = 0, featureId = 0;
for (seg = segList; seg != NULL; seg = seg->next)
    {
    offsetGenesInSeg(seg);
    if (lastSeg != NULL)
	mergeTwo(lastSeg, seg);
    lastSeg = seg;
    }
AllocVar(merged);
for (seg = segList; seg != NULL; seg = seg->next)
    {
    struct genScanGene *gene, *nextGene;
    struct genScanFeature *gsf, *nextGsf;
    for (gene = seg->geneList; gene != NULL; gene = nextGene)
        {
	nextGene = gene->next;
	gene->id = ++geneId;
	slAddHead(&merged->geneList, gene);
	for (gsf = gene->featureList; gsf != NULL; gsf = gsf->next)
	    gsf->geneId = geneId;
	}
    for (gsf = seg->suboptList; gsf != NULL; gsf = nextGsf)
        {
	nextGsf = gsf->next;
	gsf->featId = ++featureId;
	slAddHead(&merged->suboptList, gsf);
	}
    if (merged->end < seg->end) seg->end = merged->end;
    }
slReverse(&merged->geneList);
slReverse(&merged->suboptList);
return merged;
}

void gsBig(char *faName, char *gtfName, char *suboptName, char *transName)
/* gsBig - Run Genscan on big input and produce GTF files. */
{
struct dnaSeq seq;
struct lineFile *lf = lineFileOpen(faName, TRUE);
struct tempName tn1, tn2;
char *tempFa, *tempGs;
struct dyString *dy = newDyString(1024);
FILE *gtfFile = mustOpen(gtfName, "w");
FILE *subFile = NULL;
FILE *transFile = NULL;

if (suboptName != NULL)
    subFile = mustOpen(suboptName, "w");
if (transName != NULL)
    transFile = mustOpen(transName, "w");

if (cgiVarExists("prerun"))
    {
    char *preFileName = cgiString("prerun");
    char seqName[128];
    struct segment *seg = parseSegment(preFileName, 0, 100000000, seqName);
    writeSeg(seqName, seg, gtfFile, subFile, transFile);
    }
else
    {
    makeTempName(&tn1, "temp", ".fa");
    makeTempName(&tn2, "temp", ".genscan");
    tempFa = tn1.forCgi;
    tempGs = tn2.forCgi;
    while (faSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
	{
	int offset, sizeOne;
	struct segment *segList = NULL, *seg;
	char *seqName = cloneString(seq.name);

	for (offset = 0; offset < seq.size; offset += stepSize)
	    {
	    sizeOne = seq.size - offset;
	    if (sizeOne > winSize) sizeOne = winSize;
	    faWrite(tempFa, "split", seq.dna + offset, sizeOne); 
	    dyStringClear(dy);
	    dyStringPrintf(dy, "%s %s %s", exePath, parPath, tempFa);
	    if (suboptName != NULL)
	       dyStringPrintf(dy, " -subopt");
	    dyStringPrintf(dy, " > %s", tempGs);
	    system(dy->string);
	    seg = parseSegment(tempGs, offset, offset+sizeOne, NULL);
	    slAddHead(&segList, seg);
	    }
	slReverse(&segList);
	seg = mergeSegs(segList);
	writeSeg(seqName, seg, gtfFile, subFile, transFile);
	freez(&seqName);
	}
    remove(tempFa);
    remove(tempGs);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
winSize = cgiUsualInt("window", winSize);
stepSize = 2*winSize/3;
gsBig(argv[1], argv[2], cgiOptionalString("subopt"), cgiOptionalString("trans"));
return 0;
}
