#include "common.h"
#include "options.h"
#include "linefile.h"
#include "sample.h"

static char const rcsid[] = "$Id: groupSamples.c,v 1.8 2008/09/03 19:18:12 markd Exp $";

void usage()
{
errAbort(
    "groupSamples - Group samples together into one sample.\n"
    "Samples must be sorted by chromosome position (you can\n"
    "use bedSort first if they are not).\n"
    "usage:\n"
    "   groupSamples <grouping size - int> <input file> <output file>\n"
    );
}

void addSampleToCurrent(struct sample *target, struct sample *samp, int grouping)
{
int i;
int base = target->sampleCount;
for(i=0;i<samp->sampleCount; i++)
    {
    assert(base+i < grouping);
    target->score += samp->score;
    target->samplePosition[base + i] = samp->samplePosition[i] + samp->chromStart - target->chromStart;
    target->sampleHeight[base + i] = samp->sampleHeight[i];
    target->sampleCount++;
    }
}

int sampleCoordCmp(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct sample *a = *((struct sample **)va);
const struct sample *b = *((struct sample **)vb);
int diff;
diff = strcmp(a->chrom, b->chrom);
if(diff == 0)
    diff = a->chromStart - b->chromStart;
return diff;
}

struct sample *sampleNext(struct lineFile *lf)
/* Return next sample in file, or NULL at EOF. */
{
char *row[9];
if (!lineFileRow(lf, row))
    return NULL;
return sampleLoad(row);
}

void groupByPosition(int grouping , struct lineFile *lf, FILE *out)
/* Group together samples into a larger bundle. */
{
struct sample *samp = NULL, *currSamp = NULL;
int count = 0;
while ((samp = sampleNext(lf)) != NULL)
    {
    int lastStart = samp->chromStart;
    AllocVar(currSamp);
    currSamp->chrom = cloneString(samp->chrom);
    currSamp->chromStart = samp->chromStart;
    currSamp->name = cloneString(samp->name);
    snprintf(currSamp->strand, sizeof(currSamp->strand), "%s", samp->strand);
    AllocArray(currSamp->samplePosition, grouping);
    AllocArray(currSamp->sampleHeight, grouping);
    count = 0;
    while(samp != NULL && count < grouping && sameString(samp->chrom,currSamp->chrom))
	{
	if (samp->chromStart < lastStart)
	    errAbort("%s is not sorted line %d", lf->fileName, lf->lineIx);
	lastStart = samp->chromStart;
	if(sameString(currSamp->name, "Empty") && differentString(samp->name, "Empty"))
	    {
	    freez(&currSamp->name);
	    currSamp->name = cloneString(samp->name);
	    }
	addSampleToCurrent(currSamp, samp, grouping);
	count += samp->sampleCount;
	sampleFree(&samp);
	samp = sampleNext(lf);
	}
    if(count != 0)
	currSamp->score = currSamp->score / count;
    currSamp->sampleCount = count;
    currSamp->chromEnd = currSamp->chromStart + currSamp->samplePosition[count -1];
    sampleTabOut(currSamp, out);
    sampleFree(&currSamp);
    }
}


void groupSamples(int grouping, char  *input, char *output)
/* Pack together samples. */
{
FILE *out = mustOpen(output, "w");
struct lineFile *lf = lineFileOpen(input, TRUE);
groupByPosition(grouping, lf, out);
carefulClose(&out);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int grouping = 0;
optionHash(&argc, argv);
if(argc != 4)
    usage();
grouping = atoi(argv[1]);
groupSamples(grouping, argv[2], argv[3]);
return 0;
}
