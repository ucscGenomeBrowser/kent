/* fullTextExperiment - VisiGene search using full text.
 * This ends up not working so well - it's not specific
 * enough.  I'm checking it in in case want to try another
 * pass at it later. */

#include "common.h" 
#include "linefile.h" 
#include "hash.h" 
#include "options.h" 
#include "dystring.h"
#include "bits.h"
#include "obscure.h"
#include "rbTree.h"
#include "jksql.h"
#include "visiGene.h"
#include "visiSearch.h"
#include "trix.h"

void visiMatchFree(struct visiMatch **pMatch)
/* Free up memory associated with visiMatch */
{
struct visiMatch *match = *pMatch;
if (match != NULL)
    {
#ifdef OLD
    bitFree(&match->wordBits);
#endif /* OLD */
    freez(pMatch);
    }
}

void visiMatchFreeList(struct visiMatch **pList)
/* Free up memory associated with list of visiMatch */
{
struct visiMatch *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    visiMatchFree(&el);
    }
*pList = NULL;
}

static int visiMatchCmpImageId(void *va, void *vb)
/* rbTree comparison function to tree on imageId. */
{
struct visiMatch *a = va, *b = vb;
return a->imageId - b->imageId;
}

int visiMatchCmpWeight(const void *va, const void *vb)
/* Compare to sort based on match. */
{
const struct visiMatch *a = *((struct visiMatch **)va);
const struct visiMatch *b = *((struct visiMatch **)vb);
double dif = b->weight - a->weight;
if (dif > 0)
   return 1;
else if (dif < 0)
   return -1;
else
   return 0;
}

static struct visiMatch *matchGeneName(struct sqlConnection *conn, char *symbol)
/* Return matching list if it's a gene symbol. */
{
struct visiMatch *matchList = NULL, *match;
struct dyString *dy = dyStringNew(0);
struct sqlResult *sr;
char query[256], **row;
dyStringAppend(dy, "select imageProbe.image from gene,probe,imageProbe where ");
dyStringPrintf(dy, "gene.name=\"%s\" ", symbol);
dyStringAppend(dy, "and gene.id = probe.gene ");
dyStringAppend(dy, "and probe.id = imageProbe.probe ");
uglyf("%s<BR>\n", dy->string);
sr = sqlGetResult(conn, dy->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(match);
    match->imageId = sqlUnsigned(row[0]);
    slAddHead(&matchList, match);
    }
sqlFreeResult(&sr);
slReverse(&matchList);
return matchList;
}

struct visiMatch *visiSearch(struct sqlConnection *conn, char *searchString)
/* visiSearch - return list of images that match searchString sorted
 * by how well they match. This will search most fields in the
 * database. */
{
char *dupe = cloneString(searchString);
struct trix *trix = trixOpen("/gbdb/visiGene/visiGene.ix");
struct trixSearchResult *tsrList, *tsr;
struct visiMatch *matchList = NULL, *match;
int wordCount = chopByWhite(dupe, NULL, 0);
char **words;

AllocArray(words, wordCount);
chopByWhite(dupe, words, wordCount);
if (wordCount != 1 || (matchList = matchGeneName(conn, words[0])) == NULL)
    {
    tsrList = trixSearch(trix, wordCount, words);
    for (tsr = tsrList; tsr != NULL; tsr = tsr->next)
	{
	AllocVar(match);
	match->imageId = sqlUnsigned(tsr->itemId);
	slAddHead(&matchList, match);
	}
    trixSearchResultFreeList(&tsrList);
    trixClose(&trix);
    slReverse(&matchList);
    }
freeMem(dupe);
return matchList;
}

