/* mafToAnc - Find and report anchors in a maf file.  Anchors are defined */
/*   as ungapped alignments with a minimum length of 100bp.  The user     */
/*   can utilize the -minLen paramater to change the minimum ungapped     */
/*   length required to define an anchor.                                 */
#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "maf.h"
#include "anc.h"

#define MAXSPECIES  100

static char const rcsid[] = "$Id: mafToAnc.c,v 1.2 2008/02/13 00:16:43 rico Exp $";

static struct optionSpec options[] = {
	{"minLen", OPTION_INT},
	{NULL, 0},
};


void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafToAnc - Find anchors in a maf file"
  "usage:\n"
  "   mafToAnc in.maf out.anc\n"
  "options:\n"
  "   -minLen=<D> - min length of ungapped alignment to define anchor (default=100)\n"
  );
}


static int compar_comp(const void *a, const void *b)
/* Comparison function for sorting alignment blocks. */
{
struct mafComp *p, *q;
int rc;

p = (*((struct mafAli **) a))->components;
q = (*((struct mafAli **) b))->components;

if ((rc = strcmp(p->src, q->src)) != 0)
	return(rc);

if (p->start != q->start)
	return(p->start - q->start);

return(p->start + p->size - q->start - q->size);
}


static void mafSortByFirstCompPos(struct mafAli **aliList, int ali_count)
/* Sort the alignments by the position of the first component. */
{
struct mafAli **ali_array, *ma;
int i;

AllocArray(ali_array, ali_count);

for (ma = *aliList, i = 0; ma != NULL; ma = ma->next, i++)
	ali_array[i] = ma;

qsort((void *) ali_array, (size_t) ali_count, sizeof(struct mafAli *), compar_comp);

*aliList = ali_array[0];
for (i = 0; i < ali_count - 1; i++)
	ali_array[i]->next = ali_array[i + 1];
ali_array[ali_count - 1]->next = NULL;

freeMem(ali_array);
}


static void mafAliFuse(struct mafAli *ma)
/* Fuse adjacent alignment blocks. */
{
struct mafAli *ma1, *ma2;
struct mafComp *mc1, *mc2;
char *text;

ma1 = ma;
while (ma1->next != NULL)
	{
	ma2 = ma1->next;

	for (mc1 = ma1->components, mc2 = ma2->components; mc1 != NULL && mc2 != NULL; mc1 = mc1->next, mc2 = mc2->next)
		if (mc1->start + mc1->size != mc2->start || mc1->strand != mc2->strand || ! sameString(mc1->src, mc2->src))
			break;

	/* We found two alignment blocks that can be merged. */
	if (mc1 == NULL && mc2 == NULL)
		{
		/* Merge alignment blocks. */
		for (mc1 = ma1->components, mc2 = ma2->components; mc1 != NULL && mc2 != NULL; mc1 = mc1->next, mc2 = mc2->next)
			{
			mc1->size += mc2->size;
			AllocArray(text, ma1->textSize + ma2->textSize + 1);
			strcpy(text, mc1->text);
			strcat(text, mc2->text);
			freeMem(mc1->text);
			mc1->text = text;
			}
		ma1->textSize += ma2->textSize;
		ma1->next = ma2->next;
		mafAliFree(&ma2);
		}
		else
		{
		ma1 = ma2;
		}
	}
}


static struct ancBlock *buildAnchor(int len, int *compPos, struct mafAli *ma)
/* Build and anchor from the argument data. */
{
struct mafComp *mc;
struct ancBlock *ab;
struct ancComp *ac, *tail = NULL;
int compIdx;

/* Initialize the anchor block */
AllocVar(ab);
ab->prev       = NULL;
ab->next       = NULL;
ab->ancLen     = len;
ab->components = NULL;

for (mc = ma->components, compIdx = 0; mc != NULL; mc = mc->next, compIdx++)
	{
	/* Initialize a new anchor component. */
	AllocVar(ac);
	ac->next    = NULL;
	ac->src     = cloneString(mc->src);
	ac->start   = compPos[compIdx] - len;
	ac->strand  = mc->strand;
	ac->srcSize = mc->srcSize;

	/* Add the new component to the current list. */
	if (ab->components == NULL)
		ab->components = ac;
	else
		tail->next = ac;
	tail = ac;
	}

return(ab);
}

static void findAnchors(struct mafAli **aliList, char *outAnc, int minLen)
/* Find and report anchors. */
{
struct mafAli *ma, *next;
struct mafComp *mc;
int idx, len, numComps, gapCol = 0;
int compPos[MAXSPECIES];
struct ancBlock *block;
FILE *f = mustOpen(outAnc, "w");


ancWriteStart(f, minLen);

/* Find and report anchors. */
for (ma = *aliList; ma != NULL; ma = next)
	{
	next = ma->next;
	idx = len = 0;

	/* Initialize positions for each component */
	for (mc = ma->components, numComps = 0; mc != NULL; mc = mc->next, numComps++)
		compPos[numComps] = mc->start;

	while (idx < ma->textSize)
		{
		/* Check to see if there is a gap in this column. */
		gapCol = 0;
		for (mc = ma->components; mc != NULL; mc = mc->next)
			if (mc->text[idx] == '-')
				{
				gapCol = 1;
				break;
				}

		if (! gapCol)
			{
			len++;
			}
			else
			{
			/* Possibly report an anchor. */
			if (len >= minLen)
				{
				block = buildAnchor(len, compPos, ma);
				ancWrite(f, block);
				ancBlockFree(&block);
				}
			len = 0;
			}

		/* Update the positions for each component. */
		for (mc = ma->components, numComps = 0; mc != NULL; mc = mc->next, numComps++)
			if (mc->text[idx] != '-')
				compPos[numComps] += 1;

		idx++;
		}

	/* Is the final region of this alignment an anchor? */
	if ((! gapCol) && (len >= minLen))
		{
		block = buildAnchor(len, compPos, ma);
		ancWrite(f, block);
		ancBlockFree(&block);
		}

	mafAliFree(&ma);
	}

carefulClose(&f);
}


void mafToAnc(char *inMaf, char *outAnc, int minLen)
/* mafToAnc - Find anchors in a maf file. */
{
struct mafFile *mf = mafOpen(inMaf);
struct mafAli *ma, *ali_list = NULL, *ali_list_tail = NULL;
struct mafComp *prev, *curr;
int ali_count = 0;

/* Read in and count the alignment blocks from the maf. */
while ((ma = mafNext(mf)) != NULL)
	{
	/* Sanity check. */
	if (ma->components != NULL && ma->components->strand != '+')
		{
		warn("Expecting the first component to be on the + strand:");
		mafWrite(stderr, ma);
		exit(EXIT_FAILURE);
		}

	/* Strip out components that represent "e" lines. */
	prev = curr = ma->components;
	while (curr != NULL)
		{
		if ((curr->size == 0) && (curr->leftStatus))
			{
			if (curr == ma->components)
				{
				ma->components = curr->next;
				mafCompFree(&curr);
				prev = curr = ma->components;
				}
				else
				{
				prev->next = curr->next;
				mafCompFree(&curr);
				curr = prev->next;
				}
			}
			else
			{
			/* Strip out quality data. */
			if (curr->quality)
				freeMem(curr->quality);

			prev = curr;
			curr = curr->next;
			}
		}

	/* Discard alignments that only have a single component. */
	if (ma->components != NULL && ma->components->next == NULL)
		{
		mafAliFree(&ma);
		continue;
		}

	ali_count++;

	if (ali_list == NULL)
		{
		ali_list = ma;
		}
		else
		{
		ali_list_tail->next = ma;
		}
	ali_list_tail = ma;
	}
mafFileFree(&mf);

/* Sort the alignments by the position of the first component. */
mafSortByFirstCompPos(&ali_list, ali_count);

/* Combine adjacent alignment blocks that can be fused. */
mafAliFuse(ali_list);

/* Find and report anchors. */
findAnchors(&ali_list, outAnc, minLen);
}


int main(int argc, char *argv[])
/* Process command line. Search maf file for anchors. */
{
int minLen;

optionInit(&argc, argv, options);
minLen = optionInt("minLen", 100);

if (argc != 3)
    usage();

mafToAnc(argv[1], argv[2], minLen);

return(EXIT_SUCCESS);
}
