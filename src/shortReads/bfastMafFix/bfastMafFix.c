/* bfastMafFix - Fix bfast's broken MAFs.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "sqlNum.h"
#include "maf.h"

static char const rcsid[] = "$Id: bfastMafFix.c,v 1.3 2008/11/06 06:57:11 kent Exp $";

char *out = "maf";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bfastMafFix - Fix bfast's broken MAFs.\n"
  "usage:\n"
  "   bfastMafFix bfast.maf chrom.sizes fixed.maf\n"
  "options:\n"
  "   -out=type Type can be maf (default), bed, or splat.\n"
  );
}

static struct optionSpec options[] = {
   {"out", OPTION_STRING},
   {NULL, 0},
};

struct mafAli *processLineA(char *line, int lineIx, char **retContig)
/* Process a maf line that starts with an 'a' and start building a mafAli around it. */
{
struct hash *varHash = hashVarLine(line, lineIx);
*retContig = hashMustFindVal(varHash, "contig-name");
char *scoreString = hashMustFindVal(varHash, "score");
struct mafAli *maf;
AllocVar(maf);
maf->score = atof(scoreString);
return maf;
}

void addComponent(struct mafAli *maf, char **pContig, struct hash *chromSizeHash,
	struct lineFile *lf, char *line)
/* Add component to maf file. */
{
/* Chop into space delimited fields. */
char *row[7];
int fieldCount = chopByWhite(line, row, ArraySize(row));
if (fieldCount != 6)	/* Already skipped the s word. */
    errAbort("Expecting %d fields got %d line %d of %s", 
    	7, fieldCount+1, lf->lineIx, lf->fileName);

struct mafComp *mc;
AllocVar(mc);
if (maf->components == NULL)
    {
    mc->src = *pContig;
    pContig = NULL;
    if (mc->src == NULL)
        errAbort("No contig-name line %d of %s", lf->lineIx-1, lf->fileName);
    mc->srcSize = hashIntVal(chromSizeHash, mc->src);
    mc->strand = '+';
    mc->start = sqlUnsigned(row[1]);
    mc->size = sqlUnsigned(row[2]);
    mc->text = cloneString(row[5]);
    maf->textSize = strlen(mc->text);
    maf->components = mc;
    }
else
    {
    if (maf->components->next != NULL)
        errAbort("Got three s lines line %d of %s, expected just 2", lf->lineIx, lf->fileName);
    mc->src = cloneString(row[0]+1);
    mc->srcSize = sqlUnsigned(row[4]);
    mc->strand = row[3][0];
    mc->start = 0;
    mc->size = mc->srcSize;
    mc->text = cloneString(row[5]);
    if (strlen(mc->text) != maf->textSize)
       errAbort("text size mismatch between components %d vs %d line %d of %s",
       	maf->textSize, (int)strlen(mc->text), lf->lineIx, lf->fileName);
    maf->components->next = mc;
    }
}

void writeAsBed(FILE *f, struct mafAli *maf)
/* Write alignment as a bed. */
{
struct mafComp *chromMc = maf->components;
struct mafComp *readMc = chromMc->next;
fprintf(f, "%s\t%d\t%d\t%s\t%d\t%c\n",
	chromMc->src, chromMc->start, chromMc->start + chromMc->size,
	readMc->src, round(1000 * maf->score / 50), readMc->strand);
}

void writeSplatSeq(FILE *f, int textSize, char *chromText, char *readText)
/* Write out sequence in splat format*/
{
int i;
for (i=0; i<textSize; ++i)
    {
    char c = toupper(chromText[i]);
    char r = toupper(readText[i]);
    if (c == r || c == '-')
        fputc(c, f);
    else if (r == '-')
        {
	fputc('^', f);
	fputc(r, f);
	}
    else
        fputc(tolower(r), f);
    }
}

void writeAsSplat(FILE *f, struct mafAli *maf)
/* Write alignment as a splat. */
{
struct mafComp *chromMc = maf->components;
struct mafComp *readMc = chromMc->next;
fprintf(f, "%s\t%d\t%d\t", 
	chromMc->src, chromMc->start, chromMc->start + chromMc->size);
writeSplatSeq(f, maf->textSize, chromMc->text, readMc->text);
fprintf(f, "\t%d\t%c\t%s\n", round(1000 * maf->score / 50), readMc->strand, readMc->src);
}

void bfastMafFix(char *input, char *chromSizes, char *output)
/* bfastMafFix - Fix bfast's broken MAFs.. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
struct hash *chromSizeHash = hashNameIntFile(chromSizes);
FILE *f = mustOpen(output, "w");
mafWriteStart(f, "bfastFixed");
char *line;
struct mafAli *maf = NULL;
char *contig;

while (lineFileNext(lf, &line, NULL))
    {
    line = skipLeadingSpaces(line);
    char c = line[0];
    line += 2;
    switch (c)
        {
	case 0:
	   if (sameString(out, "bed"))
	      writeAsBed(f, maf);
	   else if (sameString(out, "maf"))
	      mafWrite(f, maf);
	   else if (sameString(out, "splat"))
	      writeAsSplat(f, maf);
	   else
	      errAbort("Unknown out type %s\n", out);
	   mafAliFree(&maf);
	   break;
	case 'a':
	   maf = processLineA(line, lf->lineIx, &contig);
	   break;
	case 's':
	   addComponent(maf, &contig, chromSizeHash, lf, line);
	   break;
	default:
	   break;
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
out = optionVal("out", out);
bfastMafFix(argv[1], argv[2], argv[3]);
return 0;
}
