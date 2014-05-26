/* txReadRa - Read ra files from genbank and parse out relevant info into some tab-separated files.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "ra.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "txReadRa - Read ra files from genbank and parse out relevant info into some tab-separated files.\n"
  "usage:\n"
  "   txReadRa genbank.ra refseq.ra outDir\n"
  "Output files are in outDir, including\n"
  "   cds.tab - cds entries for both genbank and refSeq\n"
  "   mrnaSize.tab - size entries for both genbank and refSeq\n"
  "   exceptions.tab - Information on selenocystienes and other exceptions.\n"
  "   refSeqStatus.tab - Reviewed/Validated/Preliminary/etc. status for refSeq\n"
  "   refPepStatus.tab - Similar info for refSeq proteins\n"
  "   refToPep.tab - Maps refSeq mRNA to proteins\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

FILE *openToWrite(char *dir, char *file)
/* Return dir/file open for writing. */
{
char path[PATH_LEN];
safef(path, sizeof(path), "%s/%s", dir, file);
return mustOpen(path, "w");
}

char *requiredField(struct hash *ra, struct lineFile *lf, char *field)
/* Get field from ra, or die with approximate line number. */
{
char *val = hashFindVal(ra, field);
if (val == NULL)
    errAbort("Missing required %s field in record ending line %d of %s",
    	field, lf->lineIx, lf->fileName);
return val;
}

void outIfFound(char *acc, char *ver, struct hash *ra, char *tag, FILE *f)
/* Output tag and value if found. */
{
char *val = hashFindVal(ra, tag);
if (val != NULL)
    fprintf(f, "%s.%s\t%s\t%s\n", acc, ver, tag, val);
}

void outputExceptions(char *acc, char *ver, struct hash *ra, FILE *f)
/* Output the exceptions to file. */
{
outIfFound(acc, ver, ra, "selenocysteine", f);
outIfFound(acc, ver, ra, "translExcept", f);
outIfFound(acc, ver, ra, "exception", f);
}

void txReadRa(char *mrnaRa, char *refSeqRa, char *outDir)
/* txReadRa - Read ra files from genbank and parse out relevant info into some 
 * tab-separated files. */
{
struct lineFile *mrna = lineFileOpen(mrnaRa, TRUE);
struct lineFile *refSeq = lineFileOpen(refSeqRa, TRUE);
makeDir(outDir);
FILE *fCds = openToWrite(outDir, "cds.tab");
FILE *fStatus = openToWrite(outDir, "refSeqStatus.tab");
FILE *fSize = openToWrite(outDir, "mrnaSize.tab");
FILE *fRefToPep = openToWrite(outDir, "refToPep.tab");
FILE *fPepStatus = openToWrite(outDir, "refPepStatus.tab");
FILE *fExceptions = openToWrite(outDir, "exceptions.tab");
FILE *fAccVer = openToWrite(outDir, "accVer.tab");

struct hash *ra;
while ((ra = raNextRecord(refSeq)) != NULL)
    {
    char *acc = requiredField(ra, refSeq, "acc");
    char *rss = requiredField(ra, refSeq, "rss");
    char *siz = requiredField(ra, refSeq, "siz");
    char *ver = requiredField(ra, mrna, "ver");
    char *prt = hashFindVal(ra, "prt");
    char *cds = hashFindVal(ra, "cds");

    /* Translate rss into status. */
    char *status = NULL;
    if (sameString(rss, "rev"))
	status = "Reviewed";
    else if (sameString(rss, "pro"))
	status = "Provisional";
    else if (sameString(rss, "pre"))
	status = "Predicted";
    else if (sameString(rss, "val"))
	status = "Validated";
    else if (sameString(rss, "inf"))
	status = "Inferred";
    else
	errAbort("Unrecognized rss field %s after line %d of %s", rss, 
	    refSeq->lineIx, refSeq->fileName);

    fprintf(fStatus, "%s.%s\t%s\n", acc, ver, status);
    if (prt != NULL)
	{
	fprintf(fPepStatus, "%s\t%s\n", prt, status);
	fprintf(fRefToPep, "%s.%s\t%s\n", acc, ver, prt);
	}
    fprintf(fSize, "%s.%s\t%s\n", acc, ver, siz);
    if (cds != NULL)
	fprintf(fCds, "%s.%s\t%s\n", acc, ver, cds);
    outputExceptions(acc, ver, ra, fExceptions);
    fprintf(fAccVer, "%s\t%s.%s\n", acc, acc, ver);
    hashFree(&ra);
    }

while ((ra = raNextRecord(mrna)) != NULL)
    {
    char *acc = requiredField(ra, mrna, "acc");
    char *siz = requiredField(ra, mrna, "siz");
    char *ver = requiredField(ra, mrna, "ver");
    char *cds = hashFindVal(ra, "cds");
    fprintf(fSize, "%s.%s\t%s\n", acc, ver, siz);
    if (cds != NULL)
    	fprintf(fCds, "%s.%s\t%s\n", acc, ver, cds);
    outputExceptions(acc, ver, ra, fExceptions);
    fprintf(fAccVer, "%s\t%s.%s\n", acc, acc, ver);
    hashFree(&ra);
    }

carefulClose(&fCds);
carefulClose(&fStatus);
carefulClose(&fSize);
carefulClose(&fRefToPep);
carefulClose(&fPepStatus);
carefulClose(&fExceptions);
carefulClose(&fAccVer);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
txReadRa(argv[1], argv[2], argv[3]);
return 0;
}
