/* gsBig - Run Genscan on big input and produce GTF files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "portable.h"
#include "fa.h"
#include "dystring.h"

char *exePath = "/projects/compbio/bin/genscan-linux/genscan";
char *parPath = "/projects/compbio/bin/genscan-linux/HumanIso.smat";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gsBig - Run Genscan on big input and produce GTF files\n"
  "usage:\n"
  "   gsBig file.fa output.gtf\n"
  "options:\n"
  "   -subOpt=output.bed\n"
  "   -trans=output.fa\n"
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
else if (sameString(type, "Init") || sameString(type, "Intr") || sameString(type, "Term"))
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
if (partCount != 2 || !isdigit(parts[0][0]) || !isdigit(parts[1][0]))
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

struct segment *parseSegment(char *fileName, int start, int end)
/* Read in a genscan file into segment. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct segment *seg;
char *line;
struct genScanFeature *gsfList = NULL, *gsf;
struct genScanGene *gsg;

if (!lineFileNext(lf, &line, NULL))
    errAbort("%s is empty", fileName);
if (!startsWith("GENSCAN ", line))
    errAbort("%s is not a GENSCAN output file");
mustSkipTo(lf, "Predicted genes/exons");
mustSkipTo(lf, "Gn.Ex");
mustSkipTo(lf, "-----");
AllocVar(seg);

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
uglyf("Got %d exons\n", slCount(gsfList));
seg->geneList = bundleGenes(gsfList);
gsfList = NULL;
uglyf("Got %d genes\n", slCount(seg->geneList));

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
    uglyf("Got %d suboptimal exons\n", slCount(seg->suboptList));
    }
else
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
    uglyf("Got %d translations\n", slCount(seg->geneList));
    }

lineFileClose(&lf);
return seg;
}

void gsBig(char *faName, char *gtfName, char *subOptName)
/* gsBig - Run Genscan on big input and produce GTF files. */
{
struct dnaSeq seq;
struct lineFile *lf = lineFileOpen(faName, TRUE);
struct tempName tn1, tn2;
char *tempFa, *tempGs;
struct dyString *dy = newDyString(1024);

makeTempName(&tn1, "temp", ".fa");
makeTempName(&tn2, "temp", ".genscan");
tempFa = tn1.forCgi;
tempGs = tn2.forCgi;

while (faSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
    {
    int offset, sizeOne;
    int maxSize = 1500000;
    int stepSize = 1000000;
    struct segment *segmentList = NULL, *seg;

    for (offset = 0; offset < seq.size; offset += stepSize)
        {
	sizeOne = seq.size - offset;
	if (sizeOne > maxSize) sizeOne = maxSize;
	faWrite(tempFa, "split", seq.dna + offset, sizeOne); 
	dyStringClear(dy);
	dyStringPrintf(dy, "%s %s %s", exePath, parPath, tempFa);
	if (subOptName != NULL)
	   dyStringPrintf(dy, " -subopt");
	dyStringPrintf(dy, " > %s", tempGs);
	system(dy->string);
	seg = parseSegment(tempGs, offset, offset+sizeOne);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
gsBig(argv[1], argv[2], cgiOptionalString("subOpt"));
return 0;
}
