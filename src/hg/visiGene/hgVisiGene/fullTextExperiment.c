/* fullTextExperiment - VisiGene search using full text.
 * This ends up not working so well - it's not specific
 * enough.  I'm checking it in in case want to try another
 * pass at it later. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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

static struct hash *makePrivateHash(struct sqlConnection *conn)
/* Build up hash of all submission sets that are private.
 * This is keyed by the ascii version of submissionSet.id */
{
struct hash *hash = hashNew(0);
if (sqlFieldIndex(conn, "submissionSet", "privateUser") >= 0)
    {
    struct sqlResult *sr;
    char **row;
    sr = sqlGetResult(conn, NOSQLINJ "select id from submissionSet where privateUser!=0");
    while ((row = sqlNextRow(sr)) != NULL)
        hashAdd(hash, row[0], NULL);
    sqlFreeResult(&sr);
    }
return hash;
}

static boolean isPrivate(struct sqlConnection *conn,
	struct hash *privateHash, char *imageId)
/* Return TRUE if image is associated with private submissionSet. */
{
char *src, buf[16];
char query[256];
sqlSafef(query, sizeof(query), "select submissionSet from image where id='%s'",
	imageId);
src = sqlQuickQuery(conn, query, buf, sizeof(buf));
if (src != NULL && hashLookup(privateHash, src) != NULL)
    return TRUE;
return FALSE;
}

static struct visiMatch *matchGeneName(struct sqlConnection *conn, 
	char *symbol, struct hash *privateHash)
/* Return matching list if it's a gene symbol. */
{
struct visiMatch *matchList = NULL, *match;
struct slInt *imageList, *image;
char *sqlPat = sqlLikeFromWild(symbol);
if (sqlWildcardIn(sqlPat))
    imageList = visiGeneSelectNamed(conn, sqlPat, vgsLike);
else
    imageList = visiGeneSelectNamed(conn, sqlPat, vgsExact);
for (image = imageList; image != NULL; image = image->next)
    {
    char asciiId[16];
    safef(asciiId, sizeof(asciiId), "%d", image->val);
    if (!isPrivate(conn, privateHash, asciiId))
	{
	AllocVar(match);
	match->imageId = image->val;
	slAddHead(&matchList, match);
	}
    }
slReverse(&matchList);
return matchList;
}


struct visiMatch *visiSearch(struct sqlConnection *conn, char *searchString)
/* visiSearch - return list of images that match searchString sorted
 * by how well they match. This will search most fields in the
 * database. */
{
char *dupe = cloneString(searchString);
struct trix *trix = trixOpen("visiGeneData/visiGene.ix");
struct trixSearchResult *tsrList, *tsr;
struct visiMatch *matchList = NULL, *match;
int wordCount = chopByWhite(dupe, NULL, 0);
char **words;
boolean hasWild;
struct hash *privateHash = makePrivateHash(conn);

tolowers(dupe);
hasWild = (strchr(searchString, '*') != NULL);
if (hasWild)
    stripChar(dupe, '*');
AllocArray(words, wordCount);
chopByWhite(dupe, words, wordCount);
/* if (wordCount != 1 || 
	(matchList = matchGeneName(conn, words[0],privateHash)) == NULL) */
    {
    tsrList = trixSearch(trix, wordCount, words, hasWild ? tsmExpand : tsmExact);
    for (tsr = tsrList; tsr != NULL; tsr = tsr->next)
	{
	if (!isPrivate(conn, privateHash, tsr->itemId))
	    {
	    AllocVar(match);
	    match->imageId = sqlUnsigned(tsr->itemId);
	    slAddHead(&matchList, match);
	    }
	}
    trixSearchResultFreeList(&tsrList);
    trixClose(&trix);
    slReverse(&matchList);
    }
freeMem(dupe);
return matchList;
}

