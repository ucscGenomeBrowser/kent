/* qaAgpToQacIdx - build compressed quality file and index file. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "agp.h"
#include "agpFrag.h"
#include "agpGap.h"
#include "qaSeq.h"

#define FAKE_GAP_QUAL 100

void usage()
/* Explain usage and exit. */
{
errAbort(
  "qaAgpToQacIdx - Build compressed quality file and corresponding index file\n"
  "usage:\n"
  "   qaAgpToQacIdx in.agp in.qa out.qac out.qdx\n"
  );
}

void qaAgpToQacIdx(char *inAgp, char *inQa, char *outQac, char *outQdx)
/* qaAgpToQacIdx - build compressed quality file and index file. */
{
struct hash *agpLists = agpLoadAll(inAgp);
struct qaSeq *qa, *qaList = NULL;
struct lineFile *qalf = lineFileOpen(inQa, TRUE);
int seq_count = 0;
char *s;
char buffer[1024];
struct hashEl *hel;
struct agp *agp;
struct agpGap *gap;
FILE *qacf = mustOpen(outQac, "wb");
FILE *qdxf = mustOpen(outQdx, "wb");
off_t offset;

/* Add gap data to the qa */
while ((qa = qaReadNext(qalf)) != NULL)
	{
	seq_count++;

	/* only keep first word */
	if ((s = strchr(qa->name, ' ')) != NULL)
		{
		*s = '\0';
		}
	
	safef(buffer, sizeof(buffer), "%s", qa->name);

	/* strip off anything after the first '.' */
	/* scaffold_0.1-193456 seemed redundant,  */
	/* and doesn't match the agp file         */
	if ((s = strchr(buffer, '.')) != NULL)
		{
		*s = '\0';
		}

	/* set fake quality values for gaps */
	if ((hel = hashLookup(agpLists, buffer)) != NULL)
		{
		for (agp = (struct agp *) hel->val; agp != NULL; agp = agp->next)
			{
			if (! agp->isFrag)
				{
				gap = (struct agpGap *) (agp->entry);
				if (qa->size - gap->chromStart + 1 > gap->size)
					{
					memset(qa->qa + gap->chromStart - 1, FAKE_GAP_QUAL, gap->size);
					}
				else
					{
					errAbort("gap too big: chromStart = %d size = %d\n", gap->chromStart, gap->size);
					}
				}
			}
		}

	slAddHead(&qaList, qa);
	}

lineFileClose(&qalf);
agpHashFree(&agpLists);


/* Write our the qac and index */
qacWriteHead(qacf);
fprintf(qdxf, "%d\n", seq_count);
for (qa = qaList; qa != NULL; qa = qa->next)
	{
	if ((offset = ftello(qacf)) == (off_t) -1)
		{
		errAbort("ftello() failed\n");
		}
	fprintf(qdxf, "%s\t%ld\n", qa->name, offset);
	qacWriteNext(qacf, qa);
	}

/* Clean up after ourselves */
carefulClose(&qacf);
carefulClose(&qdxf);
qaSeqFreeList(&qaList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 5)
	usage();
qaAgpToQacIdx(argv[1], argv[2], argv[3], argv[4]);
return EXIT_SUCCESS;
}
