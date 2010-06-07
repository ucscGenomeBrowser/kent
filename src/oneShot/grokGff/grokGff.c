/* GrokGff - analyse gff files and do some basic stats. */
#include "common.h"
#include "gff.h"

struct gffGene
/* A list of transcripts. */
    {
    struct gffGene *next;
    char *seq;
    char strand;
    int start, end;
    struct gffGroup *groupList;
    };

void usage()
/* Display usage instructions and exit. */
{
errAbort(
   "grokGff - analyse GFF files\n"
   "usage:\n"
   "   grokGff file(s).gff");
}

struct gffGene *overlaps(struct gffGroup *q, struct gffGene *geneList)
/* Return gene that overlaps q. */
{
struct gffGene *g;

for (g = geneList; g != NULL; g = g->next)
    {
    if (g->strand == q->strand && rangeIntersection(g->start, g->end, q->start, q->end) > 0
    	&& sameString(q->seq, g->seq))
	{
	return g;
	}
    }
return NULL;
}

boolean isGeneGroup(struct gffGroup *g)
/* Return TRUE if group has exon or CDS. */
{
struct gffLine *gl;
for (gl = g->lineList; gl != NULL; gl = gl->next)
    {
    if (sameWord(gl->feature, "exon") || sameWord(gl->feature, "CDS"))
	return TRUE;
    }
return FALSE;
}

int groupCount = 0;
int geneCount = 0;
int transcriptCount = 0;
struct gffGene *geneList = NULL;

void gatherStats(struct gffFile *gff)
/* Figure out some stats on gff */
{
struct gffGroup *g, *next;
struct gffGene *gene;

for (g = gff->groupList; g != NULL; g = next)
    {
    next = g->next;
    ++groupCount;
    if (isGeneGroup(g))
	{
	++transcriptCount;
	if ((gene = overlaps(g, geneList)) == NULL)
	    {
	    ++geneCount;
	    AllocVar(gene);
	    gene->seq = g->seq;
	    gene->strand = g->strand;
	    gene->start = g->start;
	    gene->end = g->end;
	    slAddHead(&geneList, gene);
	    }
	else
	    {
	    gene->start = min(gene->start, g->start);
	    gene->end = max(gene->end, g->end);
	    }
	slAddHead(&gene->groupList, g);
	}
    }
}

void printStats()
{
int multiCount = 0;
struct gffGene *gene;
for (gene = geneList; gene != NULL; gene = gene->next)
    {
    if (slCount(gene->groupList) > 1)
	++multiCount;
    }
printf("%d groups %d transcripts %d genes %d genes with multiple transcripts\n",
	groupCount, transcriptCount, geneCount, multiCount);
}

int gffGeneCmp(const void *va, const void *vb)
/* Compare two gffGene. */
{
const struct gffGene *a = *((struct gffGene **)va);
const struct gffGene *b = *((struct gffGene **)vb);
return a->start - b->start;
}

void grokGff(char *files[], int  fileCount)
/* Do summary stats on gff files. */
{
int i;
struct gffFile *gff = gffFileNew("all");
struct gffGene *gene;

for (i=0; i<fileCount; ++i)
    {
    char *fileName = files[i];
    int lineCount, groupCount;
    int offset = 0;
    char *s;
    
    if ((s = strstr(fileName, "22_")) != NULL)
	{
	offset = 500000 * atoi(s+3);
	uglyf("%s offset %d\n", s, offset);
	}
    printf("Reading %s\n", fileName);
    gffFileAdd(gff, fileName, offset);
    }
printf("Grouping\n");
gffGroupLines(gff);
gatherStats(gff);
printStats();

slSort(&geneList, gffGeneCmp);
for (gene = geneList; gene != NULL; gene = gene->next)
    {
    struct gffGroup *group;
    for (group = gene->groupList; group != NULL; group = group->next)
	{
	printf("%s %d %d %c\n", group->name, group->start, group->end, group->strand);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 2)
    usage();
grokGff(argv+1, argc-1);
}

