/* motifConvert - Convert motif into a new format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaMotif.h"

boolean improbizer = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "motifConvert - Convert motif into a new format\n"
  "usage:\n"
  "   motifConvert inMotifFastaLike output\n"
  "options:\n"
  "   -improbizer\n"
  );
}

static struct optionSpec options[] = {
   {"improbizer", OPTION_BOOLEAN},
   {NULL, 0},
};

struct motifColumn
/* A single column of motif */
    {
    struct motifColumn *next;
    double val[4];		/* In a,c,g,t order. */
    };

struct motif
/* A DNA binding motif. */
    {
    struct motif *next;
    char *name;		/* Motif name. */
    struct motifColumn *columns;	/* List of columns */
    };

struct motif *motifRead(struct lineFile *lf)
/* Read in next motif from file.  Return NULL at EOF. */
{
char *line, c, *words[5];
int wordCount, i;
struct motif *motif = NULL;
struct motifColumn *col;

/* Find first line starting with > and start creating motif around it. */
for (;;)
    {
    if (!lineFileNext(lf, &line, NULL))
        return NULL;
    c = line[0];
    if (c == '>')
        {
	AllocVar(motif);
	line = trimSpaces(line+1);
	motif->name = cloneString(line);
	break;
	}
    if (isspace(c))
        errAbort("strange format line %d of %s", lf->lineIx, lf->fileName);
    }

/* Keep adding to motif until get next line that starts with '>' or EOF */
for (;;)
    {
    if (!lineFileNext(lf, &line, NULL))
	break;
    line = skipLeadingSpaces(line);
    c = line[0];
    if (c == 0)
        continue;
    if (c == '>')
        {
	lineFileReuse(lf);
	break;
	}
    wordCount = chopLine(line, words);
    lineFileExpectWords(lf, 4, wordCount);
    AllocVar(col);
    for (i=0; i<4; ++i)
        col->val[i] = atof(words[i]);
    slAddHead(&motif->columns, col);
    }
slReverse(&motif->columns);
return motif;
}

void writeRow(struct motif *motif, char label, int ix, FILE *f)
/* Write out one row of motif. */
{
struct motifColumn *col;
fprintf(f, "%c", label);
for (col = motif->columns; col != NULL; col = col->next)
    fprintf(f, " %1.2f", col->val[ix]);
fprintf(f, "\n");
}

void improbizerMotifWrite(struct motif *motif, FILE *f)
/* Write out motif in improbizer format. */
{
fprintf(f, "#%s\n", motif->name);
writeRow(motif, 'A', 0, f);
writeRow(motif, 'C', 1, f);
writeRow(motif, 'G', 2, f);
writeRow(motif, 'T', 3, f);
fprintf(f, "\n");
}

void writeAsDnaMotif(struct motif *motif, FILE *f)
/* Convert to dnaMotif and write. */
{
struct dnaMotif *dm;
int count = slCount(motif->columns);
int i;
struct motifColumn *col;
AllocVar(dm);
dm->name = cloneString(motif->name);
subChar(dm->name, ' ', '_');
dm->columnCount = count;
AllocArray(dm->aProb, count);
AllocArray(dm->cProb, count);
AllocArray(dm->gProb, count);
AllocArray(dm->tProb, count);
for (col = motif->columns, i=0; col != NULL; col = col->next, i++)
    {
    dm->aProb[i] = col->val[0];
    dm->cProb[i] = col->val[1];
    dm->gProb[i] = col->val[2];
    dm->tProb[i] = col->val[3];
    }
dnaMotifNormalize(dm);
dnaMotifTabOut(dm, f);
dnaMotifFree(&dm);
}

void motifConvert(char *inFile, char *outFile)
/* motifConvert - Convert motif into a new format. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
struct motif *motif;
while ((motif = motifRead(lf)) != NULL)
    {
    if (improbizer)
	improbizerMotifWrite(motif, f);
    else
        writeAsDnaMotif(motif, f);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
improbizer = optionExists("improbizer");
motifConvert(argv[1], argv[2]);
return 0;
}
