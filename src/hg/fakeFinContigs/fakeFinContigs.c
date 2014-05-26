/* fakeFinContigs - Fake up contigs for a finished chromosome. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "agpFrag.h"
#include "fa.h"
#include "portable.h"
#include "hCommon.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "fakeFinContigs - Fake up contigs for a finished chromosome\n"
  "usage:\n"
  "   fakeFinContigs fin.agp fin.fa finDir rootName finFaDir ooVer\n"
  "This will scan fin.agp for gaps, and create contigs in finDir\n"
  "for each section between gaps\n"
  "Example:\n"
  "   fakeFinContigs chr20.agp chr20.fa . ctg20fin ~/gs/fin/fa 101\n"
  );
}

struct contig
/* A bunch of sequence without a gap. */
    {
    struct contig *next;	/* Next in list. */
    char name[16];		/* Contig name. */
    int startOffset;		/* Start offset in chromosome. */
    int endOffset;		/* End offset in chromosome. */
    struct agpFrag *agpList;	/* List of fragments that make it up. */
    };

void fakeFinContigs(char *agpName, char *faName, char *finDir, char *rootName, char *finFaDir, char *ooVer)
/* fakeFinContigs - Fake up contigs for a finished chromosome. */
{
struct contig *contigList = NULL, *contig = NULL;
struct agpFrag *agp;
struct lineFile *lf = lineFileOpen(agpName, TRUE);
char *line, *words[16];
int lineSize, wordCount;
int contigIx = 0;
char liftDir[512], contigDir[512], path[512];
char chrom[128];
FILE *f;
struct dnaSeq *seq;
int fragIx;

/* Build up contig list by scanning agp file. */
printf("Reading %s\n", lf->fileName);
while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#' || line[0] == 0)
        continue;
    wordCount = chopLine(line, words);
    if (wordCount < 5)
        errAbort("Expecting at least 5 words line %d of %s", lf->lineIx, lf->fileName);
    if (words[4][0] == 'N' || words[4][0] == 'U')
	{
        contig = NULL;
        continue;
	}
    lineFileExpectWords(lf, 9, wordCount);
    agp = agpFragLoad(words);
    // file is 1-based but agpFragLoad() now assumes 0-based:
    agp->chromStart -= 1;
    agp->fragStart  -= 1;
    if (contig == NULL)
	{
        AllocVar(contig);
	sprintf(contig->name, "%s%d", rootName, ++contigIx);
	contig->startOffset = agp->chromStart;
	slAddHead(&contigList, contig);
	}
    else 
        {
	if (contig->agpList != NULL && contig->agpList->chromEnd != agp->chromStart)
	    errAbort("Start doesn't match previous end line %d of %s", 
	    	lf->lineIx, lf->fileName);
	}
    if (agp->chromEnd - agp->chromStart != agp->fragEnd - agp->fragStart)
        errAbort("Chrom and frag size mismatch line %d of %s", lf->lineIx, lf->fileName);
    slAddHead(&contig->agpList, agp);
    contig->endOffset = agp->chromEnd;
    }
slReverse(&contigList);
for (contig = contigList; contig != NULL; contig = contig->next)
    slReverse(&contig->agpList);
lineFileClose(&lf);

/* Load up chromosome sequence and make sure it is in one piece. */
printf("Reading %s\n", faName);
seq = faReadAllDna(faName);
if (slCount(seq) != 1)
    errAbort("Got %d sequences in %s, can only handle one.", slCount(seq), faName);

/* Fix up agp coordinates. Make a directory for each contig.  Fill it with 
 * .fa .agp barge.NN files for that contig. */
printf("Writing contig dirs\n");
for (contig = contigList; contig != NULL; contig = contig->next)
    {
    /* Make Contig dir. */
    sprintf(contigDir, "%s/%s", finDir, contig->name);
    makeDir(contigDir);

    /* Make contig.agp file. */
    sprintf(path, "%s/%s.agp", contigDir, contig->name);
    f = mustOpen(path, "w");
    fragIx = 0;
    for (agp = contig->agpList; agp != NULL; agp = agp->next)
	{
	char buf[128];
	sprintf(buf, "%s/%s", skipChr(agp->chrom), contig->name);
	freez(&agp->chrom);
	agp->chrom = cloneString(buf);
	agp->chromStart -= contig->startOffset;
	agp->chromEnd -= contig->startOffset;
	agp->ix = ++fragIx;
	agpFragTabOut(agp, f);
	}
    carefulClose(&f);

    /* Make ooGreedy.NN.gl file */
    sprintf(path, "%s/%s.%s.gl", contigDir, "ooGreedy", ooVer);
    f = mustOpen(path, "w");
    for (agp = contig->agpList; agp != NULL; agp = agp->next)
        {
	if (agp->type[0] != 'N' && agp->type[0] != 'U')
	    {
	    fprintf(f, "%s_1\t%d\t%d\t%s\n",  agp->frag, 
	    	agp->chromStart, 
		agp->chromEnd,
	        agp->strand);
	    }
	}
    carefulClose(&f);

    /* Make contig.fa file. */
    sprintf(path, "%s/%s.fa", contigDir, contig->name);
    faWrite(path, contig->name, seq->dna + contig->startOffset, 
    	contig->endOffset - contig->startOffset);

    /* Make contig/barge file. */
    sprintf(path, "%s/barge.%s", contigDir, ooVer);
    f = mustOpen(path, "w");
    fprintf(f, "Barge (Connected Clone) File ooGreedy Version %s\n", ooVer);
    fprintf(f, "\n");
    fprintf(f, "start  accession  size overlap maxClone maxOverlap\n");
    fprintf(f, "------------------------------------------------------------\n");
    for (agp = contig->agpList; agp != NULL; agp = agp->next)
        {
	char clone[128];
	strcpy(clone, agp->frag);
	chopSuffix(clone);
	
	fprintf(f, "%d\t%s\t%d\t100\tn/a\t0\n", agp->chromStart, 
		clone, agp->chromEnd);
	}
    carefulClose(&f);

    /* Make contig/gold file. */
    sprintf(path, "%s/gold.%s", contigDir, ooVer);
    f = mustOpen(path, "w");
    fragIx = 0;
    for (agp = contig->agpList; agp != NULL; agp = agp->next)
        {
	char fragName[128];
	struct agpFrag frag = *agp;
	sprintf(fragName, "%s_1", agp->frag);
	frag.frag = fragName;
	frag.type[0] = '0';
	agpFragTabOut(&frag, f);
	}
    carefulClose(&f);
    }

/* Create lift subdirectory. */
printf("Creating lift files\n");
sprintf(liftDir, "%s/lift", finDir);
makeDir(liftDir);

/* Create lift/oOut.lst file (just a list of contigs). */
sprintf(path, "%s/oOut.lst", liftDir);
f = mustOpen(path, "w");
for (contig = contigList; contig != NULL; contig = contig->next)
    fprintf(f, "%s/%s.fa.out\n", contig->name, contig->name);
carefulClose(&f);

/* Create lift/ordered.lst file (just a list of contigs). */
sprintf(path, "%s/ordered.lst", liftDir);
f = mustOpen(path, "w");
for (contig = contigList; contig != NULL; contig = contig->next)
    fprintf(f, "%s\n", contig->name);
carefulClose(&f);

/* Create lift/ordered.lft file. */
sprintf(path, "%s/ordered.lft", liftDir);
f = mustOpen(path, "w");
splitPath(faName, NULL, chrom, NULL);
for (contig = contigList; contig != NULL; contig = contig->next)
    fprintf(f, "%d\t%s/%s\t%d\t%s\t%d\n", 
	contig->startOffset, skipChr(chrom), contig->name,  
	contig->endOffset - contig->startOffset,
	chrom, seq->size);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 7)
    usage();
fakeFinContigs(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}
