/* trfBig - Mask tandem repeats on a big sequence file.. */
#include "common.h"
#include "linefile.h"
#include "fa.h"
#include "nib.h"
#include "portable.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "trfBig - Mask tandem repeats on a big sequence file.\n"
  "usage:\n"
  "   trfBig inFile outFile\n"
  "This will repeatedly run trf to mask tandem repeats in infile\n"
  "and put masked results in outFile.  inFile and outFile can be .fa\n"
  "or .nib format.\n"
  "\n"
  "options:\n"
  "   -bed creates a bed file in current dir\n");
}

void writeSomeDatToBed(char *inName, FILE *out, char *chromName, int chromOffset, 
	int start, int end)
/* Read dat file and write bits of it to .bed out file adding offset as necessary. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
char *line;
int lineSize;
char *row[14];
boolean gotHead = FALSE;
int s, e, i;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (startsWith("Parameters:", line))
        {
	gotHead = TRUE;
	break;
	}
    }
if (!gotHead) errAbort("%s isn't a recognized trf .dat file\n", inName);

while(lineFileRow(lf, row))
    {
    s = atoi(row[0])-1;
    e = atoi(row[1]);
    if (s < start) s = start;
    if (e > end) e = end;
    if (s < e)
        {
	fprintf(out, "%s\t%d\t%d\ttrf", chromName, s+chromOffset, e+chromOffset);
	for (i=2; i<14; ++i)
	    fprintf(out, "\t%s", row[i]);
	fprintf(out, "\n");
	}
    }
lineFileClose(&lf);
}

void removeWild(char *pat)
/* Remove all files matching wildcard. */
{
char dir[256], fn[128], ext[64];
char wild[256];
splitPath(pat, dir, fn, ext);
sprintf(wild, "%s%s", fn, ext);
uglyf("dir '%s' fn '%s' ext '%s' pat '%s'\n", dir, fn, ext, pat);
}

void trfBig(char *input, char *output)
/* trfBig - Mask tandem repeats on a big sequence file.. */
{
int maxSize = 5000000;
int overlapSize = 10000;
int start, end, s, e;
int halfOverlapSize = overlapSize/2;
struct dnaSeq *seqList = NULL, *seq;
char *tempFile, trfRootName[512], trfTemp[512], bedFileName[512];
char command[512];
char dir[256], chrom[128], ext[64];
boolean doBed = cgiBoolean("bed");
FILE *bedFile = NULL;

if (doBed)
    {
    splitPath(output, dir, chrom, ext);
    sprintf(bedFileName, "%s%s.bed", dir, chrom);
    bedFile = mustOpen(bedFileName, "w");
    }
if (endsWith(input, ".nib") && endsWith(output, ".nib"))
    {
    int nibSize;
    FILE *in;
    struct nibStream *ns = nibStreamOpen(output);
    struct tempName tn;

    
    makeTempName(&tn, "trf", ".fa");
    tempFile = tn.forCgi;
    nibOpenVerify(input, &in, &nibSize);
    for (start = 0; start < nibSize; start = end)
        {
	end = start + maxSize;
	if (end > nibSize) end = nibSize;
	seq = nibLdPart(input, in, nibSize, start, end - start);
	faWrite(tempFile, seq->name, seq->dna, seq->size);
	freeDnaSeq(&seq);
	sprintf(command, "trf %s 2 7 7 80 10 50 500 -m %s", 
		tempFile, doBed ? "-d" : "");
	system(command);
	sprintf(trfRootName, "%s.2.7.7.80.10.50.500", tempFile);
	sprintf(trfTemp, "%s.mask", trfRootName);
	seq = faReadDna(trfTemp);
	s = (start == 0 ? 0 : halfOverlapSize);
	if (end == nibSize)
	    e = end - start;
	else
	    {
	    e = end - halfOverlapSize - start;
	    end -= overlapSize;
	    }
	nibStreamMany(ns, seq->dna + s, e-s);
	if (doBed)
	    {
	    sprintf(trfTemp, "%s.dat", trfRootName);
	    writeSomeDatToBed(trfTemp, bedFile, chrom, start, s, e);
	    }
	}
    nibStreamClose(&ns);
    sprintf(trfTemp, "%s*", tempFile);
    removeWild(trfTemp);
    }
else
    {
    errAbort("Sorry, currently can only handle nib files.");
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
trfBig(argv[1], argv[2]);
return 0;
}
