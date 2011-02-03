/* trfBig - Mask tandem repeats on a big sequence file.. */
#include "common.h"
#include "linefile.h"
#include "fa.h"
#include "nib.h"
#include "portable.h"
#include "options.h"
#include "verbose.h"

static char const rcsid[] = "$Id: trfBig.c,v 1.20 2009/12/24 05:10:49 markd Exp $";

/* Variables that can be set from command line. */
char *trfExe = "trf";	/* trf executable name. */
boolean doBed = FALSE;	/* Output .bed file. */
char *tempDir = ".";	/* By default use current dir. */
int maxPeriod = 2000;    /* Maximum size of repeat. */
bool keep = FALSE;       /* Don't delete tmp files */

/* command line option specifications */
static struct optionSpec optionSpecs[] =
{
    {"bed", OPTION_BOOLEAN},
    {"bedAt", OPTION_STRING},
    {"tempDir", OPTION_STRING},
    {"trf", OPTION_STRING},
    {"maxPeriod", OPTION_INT},
    {"keep", OPTION_BOOLEAN},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "trfBig - Mask tandem repeats on a big sequence file.\n"
  "usage:\n"
  "   trfBig inFile outFile\n"
  "This will repeatedly run trf to mask tandem repeats in infile\n"
  "and put masked results in outFile.  inFile and outFile can be .fa\n"
  "or .nib format. Outfile can be .bed as well. Sequence output is hard\n"
  "masked, lowercase.\n"
  "\n"
  "   -bed creates a bed file in current dir\n"
  "   -bedAt=path.bed - create a bed file at explicit location\n"
  "   -tempDir=dir Where to put temp files.\n"
  "   -trf=trfExe explicitly specifies trf executable name\n"
  "   -maxPeriod=N  Maximum period size of repeat (default %d)\n"
  "   -keep  don't delete tmp files\n",
  maxPeriod);
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
struct fileInfo *list, *el;

splitPath(pat, dir, fn, ext);
sprintf(wild, "%s%s", fn, ext);
if (dir[0] == 0) strcpy(dir, ".");

list = listDirX(tempDir, wild, TRUE);
for (el = list; el != NULL; el = el->next)
    {
    remove(el->name);
    verbose(1, "Removed %s\n", el->name);
    }
slFreeList(&list);
}

void makeTrfRootName(char trfRootName[512], char *faFile)
/* Make root name of files trf produces from faFile. */
{
sprintf(trfRootName, "%s.2.7.7.80.10.50.%d", faFile, maxPeriod);
}

void trfSysCall(char *faFile)
/* Invoke trf program on file. */
{
// need to execute in trf directory, as tmp files go to current directory
char faBase[FILENAME_LEN], faExt[FILENAME_LEN];
splitPath(faFile, NULL, faBase, faExt);

char command[1024];
safef(command, sizeof(command), "cd %s && %s %s%s 2 7 7 80 10 50 %d -m %s", 
      tempDir, trfExe, faBase, faExt, maxPeriod, doBed ? "-d" : "");
verbose(1, "command %s\n", command);
fflush(stdout);
fflush(stderr);

/* Run the system command, expecting a return code of 1, as trf
   returns the number of successfully processed sequences. */
int status = system(command);
if (status == -1) 
    errnoAbort("error starting command: %s", command);
else if (WIFSIGNALED(status))
    errAbort("command terminated by signal %d: %s", WTERMSIG(status), command);
else if (WIFEXITED(status))
    {
    if (WEXITSTATUS(status) != 1)
        errAbort("command exited with status %d (expected 1): %s", WEXITSTATUS(status), command);
    }
else
    errAbort("unexpected exit for command: %s", command);
}

void outputWithBreaks(FILE *out, char *s, int size, int lineSize)
/* Print s of given size to file, adding line feeds every now and then. */
{
int i, oneSize;
for (i=0; i<size; i += oneSize)
    {
    oneSize = size - i;
    if (oneSize > lineSize) oneSize = lineSize;
    mustWrite(out, s+i, oneSize);
    fputc('\n', out);
    }
}

void trfBig(char *input, char *output)
/* trfBig - Mask tandem repeats on a big sequence file.. */
{
int maxSize = 5000000;
int overlapSize = 10000;
int start, end, s, e;
int halfOverlapSize = overlapSize/2;
char tempFile[512], trfRootName[512], trfTemp[512], bedFileName[512];
char dir[256], seqName[128], ext[64];
FILE *bedFile = NULL;
struct dnaSeq  *maskedSeq = NULL;

if (doBed)
    {
    if (optionExists("bedAt"))
        strcpy(bedFileName, optionVal("bedAt", NULL));
    else
	{
	splitPath(output, dir, seqName, ext);
	sprintf(bedFileName, "%s%s.bed", dir, seqName);
	}
    bedFile = mustOpen(bedFileName, "w");
    }
splitPath(input, dir, seqName, ext);
if (sameString("stdin", seqName))
    safef(tempFile, sizeof(tempFile), "%s",
	  rTempName(tempDir, seqName, ".tf"));
else
    safef(tempFile, sizeof(tempFile), "%s/%s.tf", tempDir, seqName);
if (endsWith(input, ".nib") && 
	(endsWith(output, ".nib") || sameString(output, "/dev/null")))
    {
    int nibSize;
    FILE *in;
    struct nibStream *ns = nibStreamOpen(output);
    struct dnaSeq *seq;

    nibOpenVerify(input, &in, &nibSize);
    for (start = 0; start < nibSize; start = end)
        {
	end = start + maxSize;
	if (end > nibSize) end = nibSize;
	seq = nibLdPart(input, in, nibSize, start, end - start);
	faWrite(tempFile, seq->name, seq->dna, seq->size);
	freeDnaSeq(&seq);
	trfSysCall(tempFile);
	makeTrfRootName(trfRootName, tempFile);
	sprintf(trfTemp, "%s.mask", trfRootName);
	maskedSeq = faReadDna(trfTemp);
	s = (start == 0 ? 0 : halfOverlapSize);
	if (end == nibSize)
	    e = end - start;
	else
	    {
	    e = end - halfOverlapSize - start;
	    end -= overlapSize;
	    }
	nibStreamMany(ns, maskedSeq->dna + s, e-s);
	freeDnaSeq(&maskedSeq);
	if (doBed)
	    {
	    sprintf(trfTemp, "%s.dat", trfRootName);
	    writeSomeDatToBed(trfTemp, bedFile, seqName, start, s, e);
	    }
	}
    nibStreamClose(&ns);
    }
else if (!endsWith(input, ".nib") && !endsWith(output, ".nib"))
    {
    struct lineFile *lf = lineFileOpen(input, TRUE);
    struct dnaSeq seq;
    FILE *out = mustOpen(output, "w");

    ZeroVar(&seq);
    while (faSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
        {
	fprintf(out, ">%s\n", seq.name);
	for (start = 0; start < seq.size; start = end)
	    {
	    end = start + maxSize;
	    if (end > seq.size) end = seq.size;
	    faWrite(tempFile, seq.name, seq.dna+start, end - start);
	    trfSysCall(tempFile);
	    makeTrfRootName(trfRootName, tempFile);
	    sprintf(trfTemp, "%s.mask", trfRootName);
	    maskedSeq = faReadDna(trfTemp);
	    s = (start == 0 ? 0 : halfOverlapSize);
	    if (end == seq.size)
		e = end - start;
	    else
		{
		e = end - halfOverlapSize - start;
		end -= overlapSize;
		}
	    outputWithBreaks(out, maskedSeq->dna+s, e-s, 50);
	    freeDnaSeq(&maskedSeq);
	    if (doBed)
		{
		sprintf(trfTemp, "%s.dat", trfRootName);
		writeSomeDatToBed(trfTemp, bedFile, seq.name, start, s, e);
		}
	    }
	}
    lineFileClose(&lf);
    carefulClose(&out);
    }
else
    {
    errAbort("Sorry, both input and output must be in same format.");
    }
if (!keep)
    {
    sprintf(trfTemp, "%s*", tempFile);
    removeWild(trfTemp);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();
trfExe = optionVal("trf", trfExe);
doBed = optionExists("bed") || optionExists("bedAt");
tempDir = optionVal("tempDir", tempDir);
maxPeriod = optionInt("maxPeriod", maxPeriod);
keep = optionExists("keep");
trfBig(argv[1], argv[2]);
return 0;
}
