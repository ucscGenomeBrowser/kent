/* makeTrackDbTagVals - Create tagVals.tab file to limit values allowed in a tag.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "ra.h"
#include "obscure.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "makeTrackDbTagVals - Create tagVals.tab file to limit values allowed in a tag.\n"
  "usage:\n"
  "   makeTrackDbTagVals in.ra tagVals.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct tagUse
/* Information on uses of tag. */
    {
    struct tagUse *next;
    char *name;			/* Tag name. */
    int count;			/* Number of times it occurs. */
    struct hash *vals;		/* Hash of all values. */
    };

int tagUseCmp(const void *va, const void *vb)
/* Compare two tagUse by name. */
{
const struct tagUse *a = *((struct tagUse **)va);
const struct tagUse *b = *((struct tagUse **)vb);
return strcmp(a->name, b->name);
}

void makeTrackDbTagVals(char *inputRa, char *outputTab)
/* makeTrackDbTagVals - Create tagVals.tab file to limit values allowed in a tag.. */
{
struct tagUse *use, *useList = NULL;
struct hash *tagHash = hashNew(0);
struct lineFile *lf = lineFileOpen(inputRa, TRUE);
while (raSkipLeadingEmptyLines(lf, NULL))
    {
    char *tag, *val;
    while (raNextTagVal(lf, &tag, &val, NULL))
        {
	struct tagUse *use = hashFindVal(tagHash, tag);
	if (use == NULL)
	    {
	    AllocVar(use);
	    use->vals = hashNew(0);
	    hashAddSaveName(tagHash, tag, use, &use->name);
	    slAddHead(&useList, use);
	    }
	use->count += 1;
	hashStore(use->vals, val);
	}
    }
lineFileClose(&lf);

FILE *f = mustOpen(outputTab, "w");
slSort(&useList, tagUseCmp);
for (use = useList; use != NULL; use = use->next)
    {
    int valCount = use->vals->elCount;
    fprintf(f, "%s", use->name);
    if (use->count > 5 && valCount < 10 && valCount*3 <= use->count)
         {
	 struct hashEl *hel, *helList = hashElListHash(use->vals);
	 for (hel = helList; hel != NULL; hel = hel->next)
	     {
	     char *s = hel->name;
	     if (hasWhiteSpace(s))
	         {
		 char *q = makeQuotedString(s, '"');
		 fprintf(f, " %s", q);
		 freeMem(q);
		 }
	     else
	         fprintf(f, " %s", s);
	     }
	 fprintf(f, "\n");
	 slFreeList(&helList);
	 }
    else
	 fprintf(f, " *\n");
        
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
makeTrackDbTagVals(argv[1], argv[2]);
return 0;
}
