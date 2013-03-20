/* encode2GffDoctor - Fix up gff/gtf files from encode phase 2 a bit.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encode2GffDoctor - Fix up gff/gtf files from encode phase 2 a bit.\n"
  "usage:\n"
  "   encode2GffDoctor in.gff out.gff\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

char *replaceFieldInGffGroup(char *in, char *tag, char *newVal)
/* Update value of tag */
{
char *tagStart = stringIn(tag, in);
if (tagStart == NULL)
    internalErr();
char *valEnd = strchr(tagStart, ';');
if (valEnd == NULL)
    internalErr();
struct dyString *dy = dyStringNew(strlen(in));
dyStringAppendN(dy, in, tagStart - in);
dyStringAppend(dy, tag);
dyStringAppendC(dy, ' ');
dyStringPrintf(dy, "%s", newVal);
dyStringAppend(dy, valEnd);
return dyStringCannibalize(&dy);
}

char *subForSmaller(char *string, char *oldWay, char *newWay)
/* Return copy of string with first if any oldWay substituted with newWay. */
{
int oldSize = strlen(oldWay);
int newSize = strlen(newWay);
assert(oldSize >= newSize);
char *s = stringIn(oldWay, string);
if (s != NULL)
    {
    memcpy(s, newWay, newSize);
    if (oldSize != newSize)
        strcpy(s+newSize, s+oldSize);
    }
return string;
}

void encode2GffDoctor(char *inFile, char *outFile)
/* encode2GffDoctor - Fix up gff/gtf files from encode phase 2 a bit.. */
{
char *transcriptTag = "transcript_id ";
struct lineFile *lf = lineFileOpen(inFile, TRUE);
char *row[9];
FILE *f = mustOpen(outFile, "w");
while (lineFileRowTab(lf, row))
    {
    if (sameString(row[0], "chrMT"))
        row[0] = "chrM";
    if (sameString(row[1], "Cufflinks"))
        {
	/* Abbreviate really long transcript IDs. */
	char *trans = stringBetween(transcriptTag, ";", row[8]);
	if (trans != NULL)
	    {
	    char *s = trans;
	    char *id = nextWordRespectingQuotes(&s);
	    int maxSize = 200;
	    if (strlen(id) > maxSize)
	        {
		id[maxSize-4] = 0;
		char *e = strrchr(id, ',');
		if (e != NULL)
		    {
		    *e++ = '.';
		    *e++ = '.';
		    *e++ = '.';
		    *e++ = '"';
		    *e = 0;
		    }
		row[8] = replaceFieldInGffGroup(row[8], transcriptTag, id);
		}
	    freez(&trans);
	    }
	else
	    {
	    /* No transcript tag?  If it's one of the ones with gene_ids and transcript_ids
	     * instead of "gene_id" and "transcript_id" try to fix it by turning gene_ids to
	     * gene_id.  */
	    char *geneId = stringBetween("gene_id ", ";", row[8]);
	    if (geneId == NULL)
		{
		row[8] = subForSmaller(row[8], "gene_ids", "gene_id");
		}
	    freez(&geneId);
	    }
	}
    int i;
    fprintf(f, "%s", row[0]);
    for (i=1; i<9; ++i)
        fprintf(f, "\t%s", row[i]);
    fprintf(f, "\n");
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
encode2GffDoctor(argv[1], argv[2]);
return 0;
}
