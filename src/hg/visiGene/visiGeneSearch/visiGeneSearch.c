/* visiGeneSearch - Test out search routines for VisiGene. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "obscure.h"
#include "rbTree.h"
#include "jksql.h"
#include "visiGene.h"

static char const rcsid[] = "$Id: visiGeneSearch.c,v 1.4 2005/10/07 05:50:59 kent Exp $";

char *database = "visiGene";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "visiGeneSearch - Test out search routines for VisiGene\n"
  "usage:\n"
  "   visiGeneSearch searchTerm\n"
  "options:\n"
  "   -db=XXX set another database (default visiGene)\n"
  );
}

static struct optionSpec options[] = {
   {"db", OPTION_STRING},
   {NULL, 0},
};

struct visiMatch
/* Info on a score of an image in free format search. */
    {
    struct visiMatch *next;
    int imageId;	/* Image ID associated with search. */
    double weight;	/* The higher the weight the better the match */
    };

int visiMatchCmpImageId(void *va, void *vb)
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


struct visiSearcher
/* This collects together information for free format searches. */
    {
    struct visiSearcher *next;		/* Next search */
    struct visiMatch *matchList;	/* List of matching images. */
    struct rbTree *tree;		/* Tree for near random access. */
    };

struct visiSearcher *visiSearcherNew()
/* Create a new, empty search structure. */
{
struct visiSearcher *searcher;
AllocVar(searcher);
searcher->tree = rbTreeNew(visiMatchCmpImageId);
return searcher;
}

void visiSearcherFree(struct visiSearcher **pSearcher)
/* Free up memory associated with *pSearcher */
{
struct visiSearcher *searcher = *pSearcher;
if (searcher != NULL)
    {
    slFreeList(&searcher->matchList);
    rbTreeFree(&searcher->tree);
    freez(pSearcher);
    }
}

struct visiMatch *visiSearcherAdd(struct visiSearcher *searcher,
	int imageId, double weight)
/* Add given weight to match involving imageId,  creating
 * a fresh match if necessary for imageId. */
{
struct visiMatch key, *match;
key.imageId = imageId;
match = rbTreeFind(searcher->tree, &key);
if (match == NULL)
    {
    AllocVar(match);
    match->imageId = imageId;
    slAddHead(&searcher->matchList, match);
    rbTreeAdd(searcher->tree, match);
    }
match->weight += weight;
return match;
}

struct visiMatch *visiSearcherSortResults(struct visiSearcher *searcher)
/* Get sorted list of match results from searcher. 
 * You don't own the list returned though.  It will evaporate
 * with visiSearcherFree. */
{
slSort(&searcher->matchList, visiMatchCmpWeight);
return searcher->matchList;
}

char *stripSpacesEtc(char *s)
/* Return a copy of s with spaces, periods, and dashes removed */
{
char *d = cloneString(s);
stripChar(d, ' ');
stripChar(d, '.');
stripChar(d, '-');
touppers(d);
return d;
}

int countWordsUsedInName(char *name, struct slName *wordList)
/* The name here is last name followed by initials with a 
 * period after each initial and no space between initials.
 * The last name may contain multiple words.  Some complex
 * examples are "van den Eijnden-van Raaij A.J." and
 * "Van De Water T."   A typical example "Kwong W.H."
 * The word list is just space separated words.  We'd
 * Want the last example to be matched by "Kwong" "W.H."
 * as well as "Kwong WH" as well as "Kwong W H". */
{
int count = 0;
struct slName *word;
char *stripped = stripSpacesEtc(name);
char *pt = stripped;

for (word = wordList; word != NULL; word = word->next)
    {
    char *w = stripSpacesEtc(word->name);
    if (startsWith(w, pt))
        {
	count += 1;
	pt += strlen(w);
	freeMem(w);
	}
    else
        {
	freeMem(w);
	break;
	}
    }
freeMem(stripped);
return count;
}


static void addImagesMatchingName(struct visiSearcher *searcher,
	struct sqlConnection *conn, struct dyString *dy, char *contributor)
/* Add images that are contributed by given contributor to
 * searcher with a weight of one.  Use dy for scratch space for
 * the query. */
{
int contributorId;
struct sqlResult *sr;
char **row;

dyStringClear(dy);
dyStringPrintf(dy, 
   "select image.id from "
   "contributor,submissionContributor,imageFile,image "
   "where contributor.name = \"%s\" "
   "and contributor.id = submissionContributor.contributor "
   "and submissionContributor.submissionSet = imageFile.submissionSet "
   "and imageFile.id = image.imageFile"
   , contributor);
sr = sqlGetResult(conn, dy->string);
while ((row = sqlNextRow(sr)) != NULL)
   visiSearcherAdd(searcher, sqlUnsigned(row[0]), 1.0);
sqlFreeResult(&sr);
}

void visiGeneMatchContributor(struct visiSearcher *searcher, 
	struct sqlConnection *conn, struct slName *wordList)
/* Return ids of images that were contributed by all people in list. 
 * We want the behavior to be such that given a list of last names,
 * say "Smith Mahoney" it will return only those that match both 
 * names.  We also want it so that if you include the initials
 * after the last name either with or without periods that will
 * just return those matching the last name and initials.  For
 * instance "Smith JJ" or "Smith J.J." or "Smith J. J." all would
 * match a particular John Jacob Smith, but not Francis K. Smith.
 * Making this a little more interesting is a case like
 * "smith li" which could either be two last names, or a last
 * name followed by initials.  We would want to match both
 * cases.  Finally, making it even more interesting, is the
 * case where the last name is compound, like "Van Koppen" or
 * "de la Cruz" and the like.  Also don't forget the apostrophe
 * containing names like O'Shea. */
{
struct slName *word;
struct dyString *query = dyStringNew(0);
struct sqlResult *sr;
char **row;

for (word = wordList; word != NULL;  )
    {
    struct slName *nameList, *name;
    int maxWordsUsed = 0;
    dyStringClear(query);
    dyStringAppend(query, "select name from contributor where name like \"");
    dyStringAppend(query, word->name);
    dyStringAppend(query, " %\"");
    nameList = sqlQuickList(conn, query->string);
    if (nameList != NULL)
	{
	for (name = nameList; name != NULL; name = name->next)
	    {
	    int wordsUsed = countWordsUsedInName(name->name, word);
	    if (wordsUsed > maxWordsUsed)
		maxWordsUsed = wordsUsed;
	    }
	for (name = nameList; name != NULL; name = name->next)
	    {
	    if (countWordsUsedInName(name->name, word) == maxWordsUsed)
		addImagesMatchingName(searcher, conn, query, name->name);
	    }
	while (--maxWordsUsed >= 0)
	    word = word->next;
	}
    else
        word = word->next;
    slFreeList(&nameList);
    }
dyStringFree(&query);
}

void visiGeneMatchGene(struct visiSearcher *searcher, 
	struct sqlConnection *conn, struct slName *wordList)
/* Return ids of images that were contributed by all people in list. */
{
struct slName *word;
for (word = wordList; word != NULL; word = word->next)
    {
    char *sqlPat = sqlLikeFromWild(word->name);
    struct slInt *imageList, *image;
    if (sqlWildcardIn(sqlPat))
	 imageList = visiGeneSelectNamed(conn, sqlPat, vgsLike);
    else
	 imageList = visiGeneSelectNamed(conn, sqlPat, vgsExact);
    for (image = imageList; image != NULL; image = image->next)
	visiSearcherAdd(searcher, image->val, 1.0);
    slFreeList(&imageList);
    freez(&sqlPat);
    }
}

void visiGeneSearch(char *searchString)
/* visiGeneSearch - Test out search routines for VisiGene. */
{
struct sqlConnection *conn = sqlConnect(database);
struct visiMatch *matchList, *match;
struct visiSearcher *searcher = visiSearcherNew();
struct slName *wordList = stringToSlNames(searchString);
printf("Searching %s for \"%s\"\n", database, searchString);
visiGeneMatchContributor(searcher, conn, wordList);
visiGeneMatchGene(searcher, conn, wordList);
matchList = visiSearcherSortResults(searcher);
for (match = matchList; match != NULL; match = match->next)
    {
    printf("%f %d\n", match->weight, match->imageId);
    }
slFreeList(&wordList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
database = optionVal("db", database);
if (argc != 2)
    usage();
visiGeneSearch(argv[1]);
return 0;
}
