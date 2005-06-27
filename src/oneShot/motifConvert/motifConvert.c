/* motifConvert - Convert motif into a new format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "motifConvert - Convert motif into a new format\n"
  "usage:\n"
  "   motifConvert inMotifFastaLike outMotifImprobizer\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
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

void motifConvert(char *inFile, char *outFile)
/* motifConvert - Convert motif into a new format. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
struct motif *motif;
while ((motif = motifRead(lf)) != NULL)
    improbizerMotifWrite(motif, f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
motifConvert(argv[1], argv[2]);
return 0;
}
