/* encode2BedPlusDoctor - Doctor up a bed file that has extra fields and a .as file.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "asParse.h"
#include "sqlNum.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encode2BedPlusDoctor - Doctor up a bed file that has extra fields and a .as file.\n"
  "usage:\n"
  "   encode2BedPlusDoctor inFile format.as outFile\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

boolean asColumnIsScalar(struct asColumn *col)
/* Return TRUE if a simple type - not an array, list, or object. */
{
if (col->isList || col->isArray)
    return FALSE;
struct asTypeInfo *ti = col->lowType;
if (ti->type == t_object || ti->type == t_simple || ti->type == t_set)
    return FALSE;
return TRUE;
}

void encode2BedPlusDoctor(char *inFile, char *asFile, char *outFile)
/* encode2BedPlusDoctor - Doctor up a bed file that has extra fields and a .as file.. */
{
struct asObject *as = asParseFile(asFile);
int colCount = slCount(as->columnList);
char *row[colCount+1];
char *line;
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");

while (lineFileNextReal(lf, &line))
    {
    /* First just do some basic sanity checks. If these fail we abort rather than fix. */
    int wordCount = chopLine(line, row);
    lineFileExpectWords(lf, colCount, wordCount);
    if (wordCount > 3)
        {
	char *name = row[3];
	if (strlen(name) > 255)
	    errAbort("Name too long (%d chars) line %d of %s\n", 
		(int)strlen(name), lf->lineIx, lf->fileName);
	}

    /* Now go through and do some relatively harmless fixup - truncating floating point
     * numbers to integers is all currently. */
    int i;
    struct asColumn *col;
    int floatFixCount = 0;
    for (i=0, col=as->columnList; col != NULL; ++i, col=col->next)
        {
	char *val = row[i];
	if (asColumnIsScalar(col))
	    {
	    enum asTypes type = col->lowType->type;
	    if (asTypesIsInt(type))
		{
		if (strchr(val, '.') || strchr(val, 'e') || strchr(val, 'E'))
		    {
		    if (++floatFixCount > 1)
			errAbort("Simple logic can only handle one float round per line");
		    double d = sqlDouble(val);
		    if (d > BIGNUM)
		       d = BIGNUM;
		    char fixBuf[16];
		    safef(fixBuf, sizeof(fixBuf), "%lld", (long long)round(d));
		    val = row[i] = fixBuf;
		    }
		else
		    {
		    long long ll = atoll(val);
		    if (ll > BIGNUM)
		        {
			char fixBuf[16];
			safef(fixBuf, sizeof(fixBuf), "%d",BIGNUM);
			val = row[i] = fixBuf;
			}
		    }
		}
	    else if (asTypesIsFloating(type))
	        {
		if (sameString(val, "."))
		    {
		    if (sameWord(as->name, "RnaElements"))
			{
			val = row[i] = "-1";
			}
		    }
		}
	    }
	}

    /* Output row. */
    fprintf(f, "%s", row[0]);
    for (i=1; i<colCount; ++i)
	fprintf(f, "\t%s", row[i]);
    fprintf(f, "\n");
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
encode2BedPlusDoctor(argv[1], argv[2], argv[3]);
return 0;
}
