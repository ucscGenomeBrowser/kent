/* qacAddGapIdx - add gap data and index qac. */

/* Copyright (C) 2009 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "qaSeq.h"
#include "agp.h"
#include "agpGap.h"

#define FAKE_GAP_QUAL 100

void usage()
/* Explain usage and exit. */
{
errAbort(
  "qacAddGapIdx - Add gap data and index qac.\n"
  "usage:\n"
  "   qacAddGapIdx in.agp in.qac out.qac out.qdx\n"
  );
}

void qacAddGapIdx(char *inAgp, char *inQac, char *outQac, char *outQdx)
/* qacAddGapIdx - add gap data and index qac. */
{
struct hash *agpLists = agpLoadAll(inAgp);
boolean isSwapped;
FILE *in_qac_f = qacOpenVerify(inQac, &isSwapped);
struct qaSeq *qa, *qaList = NULL;
int total_seqs = 0;
struct hashEl *hel;
struct agp *agp;
struct agpGap *gap;
FILE *qacf = mustOpen(outQac, "wb");
FILE *qdxf = mustOpen(outQdx, "wb");
off_t offset;

/* Add gap data to the qac */
while ((qa = qacReadNext(in_qac_f, isSwapped)) != NULL)
	{
	total_seqs++;

	if ((hel = hashLookup(agpLists, qa->name)) != NULL)
		{
		for (agp = (struct agp *) hel->val; agp != NULL; agp = agp->next)
			{
			if (! agp->isFrag)
				{
				gap = (struct agpGap *) (agp->entry);
				if (qa->size - gap->chromStart + 1 >= gap->size)
					{
					memset(qa->qa + gap->chromStart - 1, FAKE_GAP_QUAL, gap->size);
					}
				else
					{
					errAbort("%s gap too big: qa.size %d - chromStart = %d size = %d\n", gap->chrom, qa->size, gap->chromStart, gap->size);
					}
				}
			}
		}

	slAddHead(&qaList, qa);
	}

/* Write out new qac and index */
qacWriteHead(qacf);
fprintf(qdxf, "%d\n", total_seqs);
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
agpHashFree(&agpLists);
qaSeqFreeList(&qaList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 5)
    usage();
qacAddGapIdx(argv[1], argv[2], argv[3], argv[4]);
return EXIT_SUCCESS;
}
