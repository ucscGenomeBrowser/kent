/* gtfTab - convert from .gtf to autoSql genePred
 * tab separated/comma separated format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "genePred.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
"gtfTab - convert from .gtf to autoSql genePred\n"
"tab separated/comma separated format.\n"
"usage:\n"
"   gtfTab in.gtf out.tab");
}

struct exonInfo
/* Exon begin/end. */
    {
    struct exonInfo *next;
    int start, end;	/* Position in chromosome. */
    int exonNumber;	/* Index of exon. */
    };

struct txInfo
/* Info about one gene. */
    {
    struct txInfo *next;	/* Next in list. */
    char *chrom;		/* Chromosome location. */
    char strand[2];		/* Strand. */
    char *geneId;		/* ID of gene. */
    char *transcriptId;		/* ID of transcript. */
    int cdsStart, cdsEnd;	/* CDS bounds. */
    int txStart, txEnd;		/* Transcript bounds. */
    struct exonInfo *exonList;	/* List of exons. */
    };

int cmpExonInfo(const void *va, const void *vb)
/* Compare to sort based on start. */
{
const struct exonInfo *a = *((struct exonInfo **)va);
const struct exonInfo *b = *((struct exonInfo **)vb);
return a->start - b->start;
}

int cmpTxInfo(const void *va, const void *vb)
/* Compare to sort based on start. */
{
const struct txInfo *a = *((struct txInfo **)va);
const struct txInfo *b = *((struct txInfo **)vb);
return a->txStart - b->txStart;
}

char *trimQuotes(char *s)
/* Trim quotes if any from string. */
{
if (s[0] == '"')
    {
    char *e;
    s += 1;
    e = strchr(s, '"');
    if (e != NULL)
	*e = 0;
    }
return s;
}

void parseIds(char *text, struct lineFile *lf, char **retGeneId, 
	char **retTranscriptId, int *retExonNumber)
/* Parse out pieces of something that looks like: 
   gene_id "ctg12612-000000.0"; transcript_id "ctg12612-000000.0.0"; exon_number 8
   The lf is just for error reporting.
 */
{
char *s, *e;
char *name, *val;
char *geneId = NULL, *transcriptId = NULL, *exonNumber = NULL;
for (s = text; s != NULL; s = e)
    {
    e = strchr(s, ';');
    if (e != NULL)
	{
	*e++ = 0;
	e = skipLeadingSpaces(e);
	}
    if ((name = nextWord(&s)) == NULL)
	continue;	/* empty. */
    if ((val = nextWord(&s)) == NULL)
	errAbort("Expecting name/value id pair line %d of %s", lf->lineIx, lf->fileName);
    val = trimQuotes(val);
    if (sameString(name, "gene_id"))
	geneId = val;
    else if (sameString(name, "transcript_id"))
	transcriptId = val;
    else if (sameString(name, "exon_number"))
	exonNumber = val;
    }
if (geneId)
    *retGeneId = geneId;
else
    errAbort("Expecting gene_id line %d of %s", lf->lineIx, lf->fileName);

if (transcriptId)
    *retTranscriptId = transcriptId;
else
    errAbort("Expecting transcriptId line %d of %s", lf->lineIx, lf->fileName);
if (exonNumber)
    *retExonNumber = atoi(exonNumber);
else
    *retExonNumber = 0;
}

void gtfTab(char *inName, char *outName)
/* gtfTab - convert from .gtf to autoSql genePred
 * tab separated/comma separated format. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
int lineSize, wordCount, varCount;
char *line, *words[16];
FILE *f = mustOpen(outName, "w");
struct hash *txHash = newHash(0);
struct hash *chromHash = newHash(8);
struct txInfo *tiList = NULL, *ti;
struct exonInfo *ei;
char chromName[64];
char *s;
struct hashEl *hel;
char *geneId, *transcriptId;
int exonNumber;
int start, end;
bool isExon, isCoding;
int i;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#')
	continue;
    wordCount = chopTabs(line, words);
    if (wordCount != 9 && wordCount != 8)
	{
	errAbort("Expecting 9 tab separated fields line %d of %s", 
		lf->lineIx, lf->fileName);
	}
    if (wordCount == 8)
	{
	warn("Skipping line %d of %s", lf->lineIx, lf->fileName);
	continue;
	}
    parseIds(words[8], lf, &geneId, &transcriptId, &exonNumber);
    if ((hel = hashLookup(txHash, transcriptId)) == NULL)
	{
	sprintf(chromName, "chr%s", words[0]);
	AllocVar(ti);
	hel = hashAdd(txHash, transcriptId, ti);
	ti->chrom = hashStoreName(chromHash, chromName);
	ti->strand[0] = words[6][0];
	if (ti->strand[0] != '-' && ti->strand[0] != '+')
	    errAbort("Expecting strand field 7 line %d of %s", 
	    	lf->lineIx, lf->fileName);
	ti->geneId = cloneString(geneId);
	ti->transcriptId = hel->name;
	ti->cdsStart = ti->txStart = 0x3ffffff;
	ti->cdsEnd = ti->txEnd = 0;
	slAddHead(&tiList, ti);
	}
    else
	ti = hel->val;	
    start = atoi(words[3]) - 1;
    end = atoi(words[4]);
    isExon = isCoding = FALSE;
    if (sameWord(words[2], "exon"))
	{
	isExon = TRUE;
	}
    else if (sameWord(words[2], "cds"))
	{
	isExon = isCoding = TRUE;
	}
    if (isExon)
	{
	if (start < ti->txStart) ti->txStart = start;
	if (end > ti->txEnd) ti->txEnd = end;
	if (isCoding)
	    {
	    if (start < ti->cdsStart) ti->cdsStart = start;
	    if (end > ti->cdsEnd) ti->cdsEnd = end;
	    }
	else
	    {
	    AllocVar(ei);
	    ei->start = start;
	    ei->end = end;
	    ei->exonNumber = exonNumber;
	    slAddHead(&ti->exonList, ei);
	    }
	}
    }
lineFileClose(&lf);

slSort(&tiList, cmpTxInfo);
for (ti = tiList; ti != NULL; ti = ti->next)
    slSort(&ti->exonList, cmpExonInfo);

for (ti = tiList,i=1; ti != NULL; ti = ti->next,++i)
    {
    fprintf(f, "%s_%d\t%s\t%s\t%d\t%d\t%d\t%d\t%d\t",
    	ti->chrom, i, ti->chrom, ti->strand, ti->txStart, ti->txEnd,
	ti->cdsStart, ti->cdsEnd, slCount(ti->exonList));
    for (ei = ti->exonList; ei != NULL; ei = ei->next)
	fprintf(f, "%d,", ei->start);
    fprintf(f, "\t");
    for (ei = ti->exonList; ei != NULL; ei = ei->next)
	fprintf(f, "%d,", ei->end);
    fprintf(f, "\n");
    }
fclose(f);
freeHash(&txHash);
freeHash(&chromHash);
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
gtfTab(argv[1], argv[2]);
}

