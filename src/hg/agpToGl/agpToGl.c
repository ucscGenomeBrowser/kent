/* agpToGl - Convert AGP file to GL file.  Some fakery involved.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "agpToGl - Convert AGP file to GL file.  Some fakery involved.\n"
  "usage:\n"
  "   agpToGl source.agp dest\n"
  "options:\n"
  "   -md=seq_contig.md - get list of clones to reverse from NCBI .md file\n"
  "                       and produce output parsed into contigs\n"
  );
}

struct mdContig
/* Info on size of contig. */
    {
    struct mdContig *next;
    char *name;	/* Contig name. */
    char *chrom;	/* Chromosome name. */
    int size;   /* Contig size. */
    char strand;	/* + or - or ? */
    };

void readFromMd(char *fileName, struct hash *mdHash)
/* Make an entry in mdHash for every contig in md file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[9];
struct mdContig *mdc;

while (lineFileRow(lf, row))
   {
   char *name = row[5];
   char *chrom = row[1];
   char *s;
   int start = lineFileNeedNum(lf, row, 2) - 1;
   int end = lineFileNeedNum(lf, row, 3);
   if (sameString(name, "start") || sameString(name, "end"))
       continue;
   AllocVar(mdc);
   hashAddSaveName(mdHash, name, mdc, &mdc->name);
   if ((s = strchr(chrom, '|')) != NULL) *s = 0;
   mdc->chrom = cloneString(chrom);
   mdc->size = end - start;
   mdc->strand = row[4][0];
   }
lineFileClose(&lf);
}

struct clone
/* Info on one fragment. */
    {
    struct clone *next;
    char *name;		/* Clone name.version usually. */
    int fragCount;	/* Number of fragments. */
    };

struct agpFrag
/* Info on a fragment. */
    {
    struct agpFrag *next;
    struct clone *clone;	/* Clone this is in. */
    int id;			/* Fragment ID. */
    int start,end;		/* Half open position of fragment in contig. */
    char strand;		/* Strand */
    };

struct agpContig 
/* Info on fragments of contig. */
    {
    struct agpContig *next;
    char *name;		/* Contig name. */
    struct agpFrag *fragList;
    };

void writeFragsAsGl(char *fileName, struct agpFrag *fragList)
/* Create .gl file out of fragments. */
{
FILE *f = mustOpen(fileName, "w");
struct agpFrag *frag;

for (frag = fragList; frag != NULL; frag = frag->next)
    {
    if (strrchr(frag->clone->name,'_'))
	{
	fprintf(f, "%s %d %d %c\n", frag->clone->name,
	    frag->start, frag->end, frag->strand);
	}
    else
	{
	fprintf(f, "%s_%d %d %d %c\n", frag->clone->name, frag->id, 
	    frag->start, frag->end, frag->strand);
	}
    }
carefulClose(&f);
}

void agpToGl(char *agpIn, char *mdIn, char *glOut)
/* agpToGl - Convert AGP file to GL file.  Some fakery involved.. */
{
struct hash *mdHash = newHash(0);
struct hash *cloneHash =  newHash(0);
struct hash *contigHash = newHash(0);
struct lineFile *lf = lineFileOpen(agpIn, TRUE);
struct agpContig *contigList = NULL, *contig;
char *row[9];
int wordCount;
char *cloneName, *contigName;
struct agpFrag *frag;
struct clone *clone;
int start, end;

/* Get list of reversed clones. */
if (mdIn != NULL)
    readFromMd(mdIn, mdHash);

/* Read in contigs. */
while ((wordCount = lineFileChop(lf, row)) != 0)
    {
    if (wordCount < 5)
        errAbort("Short line %d of %s", lf->lineIx, lf->fileName);
    if (row[4][0] == 'N' || row[4][0] == 'U')
        continue;
    lineFileExpectWords(lf, 9, wordCount);
    contigName = row[0];
    start = lineFileNeedNum(lf, row, 1) - 1;
    end = lineFileNeedNum(lf, row, 2);
    cloneName = row[5];

    if ((contig = hashFindVal(contigHash, contigName)) == NULL)
        {
	AllocVar(contig);
	hashAddSaveName(contigHash, contigName, contig, &contig->name);
	slAddHead(&contigList, contig);
	}
    if ((clone = hashFindVal(cloneHash, cloneName)) == NULL)
        {
	AllocVar(clone);
	hashAddSaveName(cloneHash, cloneName, clone, &clone->name);
	}
    AllocVar(frag);
    frag->clone = clone;
    frag->id = ++clone->fragCount;
    frag->start = start;
    frag->end = end;
    frag->strand = row[8][0];
    slAddHead(&contig->fragList, frag);
    }

/* Go reverse fragment lists as needed. */
for (contig = contigList; contig != NULL; contig = contig->next)
    {
    struct mdContig *mdc;
    if ((mdc = hashFindVal(mdHash, contig->name)) != NULL && mdc->strand == '-')
        {
	int size = mdc->size;
	for (frag = contig->fragList; frag != NULL; frag = frag->next)
	    {
	    int s = frag->start, e = frag->end;
	    frag->start = size - e;
	    frag->end = size - s;
	    if (frag->strand == '-')
	        frag->strand = '+';
	    else
	        frag->strand = '-';
	    }
	}
    else
	slReverse(&contig->fragList);
    }

/* Write out */
if (mdIn == NULL)
    {
    /* Simple case single file output. */
    if (slCount(contigList) > 1)
        errAbort("Multiple contigs in agp file, sorry can't convert to gl.");
    if (contigList == NULL)
        errAbort("Empty agp file");
    writeFragsAsGl(glOut, contigList->fragList);
    }
else
    {
    char path[512];
    for (contig = contigList; contig != NULL; contig = contig->next)
        {
	struct mdContig *mdc = hashMustFindVal(mdHash, contig->name);
	sprintf(path, "%s/%s/%s/contig.gl", glOut, mdc->chrom, mdc->name);
	writeFragsAsGl(path, contig->fragList);
	}
    }

lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
agpToGl(argv[1], cgiOptionalString("md"), argv[2]);
return 0;
}
