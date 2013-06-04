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

static int visiMatchCmpImageId(void *va, void *vb)
/* rbTree comparison function to tree on imageId. */
{
struct visiMatch *a = va, *b = vb;
return a->imageId - b->imageId;
}

static int visiMatchCmpWeight(const void *va, const void *vb)
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

static struct visiSearcher *visiSearcherNew()
/* Create a new, empty search structure. */
{
struct visiSearcher *searcher;
AllocVar(searcher);
searcher->tree = rbTreeNew(visiMatchCmpImageId);
return searcher;
}

static void visiSearcherFree(struct visiSearcher **pSearcher)
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

static struct visiMatch *visiSearcherAdd(struct visiSearcher *searcher,
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

static struct visiMatch *visiSearcherSortResults(struct visiSearcher *searcher)
/* Get sorted list of match results from searcher. 
 * You don't own the list returned though.  It will evaporate
 * with visiSearcherFree. */
{
slSort(&searcher->matchList, visiMatchCmpWeight);
return searcher->matchList;
}

static char *stripSpacesEtc(char *s)
/* Return a copy of s with spaces, periods, and dashes removed */
{
char *d = cloneString(s);
stripChar(d, ' ');
stripChar(d, '.');
stripChar(d, '-');
touppers(d);
return d;
}

static int countPartsUsedInName(char *name, struct slName *wordList)
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


static int countWordsUsedInPhrase(char *phrase, struct slName *wordList)
/* Count the number of words in wordList that are used in phrase.
 * The words are in order and space-separated in phrase. */
{
int count = 0;
struct slName *word;
char *dupe = cloneString(phrase);
char *s = dupe, *w;	

for (word = wordList; word != NULL; word = word->next)
    {
    w = nextWord(&s);
    if (w == NULL)
        break;
    if (sameWord(w, word->name))
        count += 1;
    else
        break;
    }
freeMem(dupe);
return count;
}


typedef void AdderFunc(struct visiSearcher *searcher,
	struct sqlConnection *conn, struct dyString *dy, char *exactMatch);
/* Typedef of function to add things that are exact matches of
 * some type to searcher.  This is broken out separately
 * so that the visiGeneMatchMultiWord function below can be
 * used in many contexts. */

static void visiGeneMatchMultiWord(struct visiSearcher *searcher, 
	struct sqlConnection *conn, struct slName *wordList,
	char *table, AdderFunc adder)
/* This helps cope with matches that may involve more than
 * one word.   It will preferentially match as many words
 * as possible, and if there is a multiple-word match it
 * will take that over a single-word match. */
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
    sqlDyStringPrintf(query, "select name from %s where name like \"", table);
    dyStringAppend(query, word->name);
    dyStringAppend(query, "%\"");
    nameList = sqlQuickList(conn, query->string);
    if (nameList != NULL)
	{
	for (name = nameList; name != NULL; name = name->next)
	    {
	    int wordsUsed = countWordsUsedInPhrase(name->name, word);
	    if (wordsUsed > maxWordsUsed)
		maxWordsUsed = wordsUsed;
	    }
	}
    if (maxWordsUsed > 0)
	{
	for (name = nameList; name != NULL; name = name->next)
	    {
	    if (countWordsUsedInPhrase(name->name, word) == maxWordsUsed)
		(*adder)(searcher, conn, query, name->name);
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

static void addImagesMatchingQuery(struct visiSearcher *searcher,
    struct sqlConnection *conn, char *query)
/* Add images that match query */
{
struct sqlResult *sr;
char **row;
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
   visiSearcherAdd(searcher, sqlUnsigned(row[0]), 1.0);
sqlFreeResult(&sr);
}

static void addImagesMatchingName(struct visiSearcher *searcher,
	struct sqlConnection *conn, struct dyString *dy, char *contributor)
/* Add images that are contributed by given contributor to
 * searcher with a weight of one.  Use dy for scratch space for
 * the query. */
{
int contributorId;

dyStringClear(dy);
sqlDyStringPrintf(dy, 
   "select image.id from "
   "contributor,submissionContributor,imageFile,image "
   "where contributor.name = \"%s\" "
   "and contributor.id = submissionContributor.contributor "
   "and submissionContributor.submissionSet = imageFile.submissionSet "
   "and imageFile.id = image.imageFile"
   , contributor);
addImagesMatchingQuery(searcher, conn, dy->string);
}

static void visiGeneMatchContributor(struct visiSearcher *searcher, 
	struct sqlConnection *conn, struct slName *wordList)
/* Put images from contributors in wordList into searcher.
 * We want the behavior to be such that if you give it two names
 * say "Smith Mahoney" it will weigh those that match both 
 * names.  We also want it so that if you include the initials
 * after the last name either with or without periods that will
 * set those matching the last name and initials.  For
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
    sqlDyStringPrintf(query, "select name from contributor where name like \"");
    dyStringAppend(query, word->name);
    dyStringAppend(query, " %\"");
    nameList = sqlQuickList(conn, query->string);
    if (nameList != NULL)
	{
	for (name = nameList; name != NULL; name = name->next)
	    {
	    int wordsUsed = countPartsUsedInName(name->name, word);
	    if (wordsUsed > maxWordsUsed)
		maxWordsUsed = wordsUsed;
	    }
	for (name = nameList; name != NULL; name = name->next)
	    {
	    if (countPartsUsedInName(name->name, word) == maxWordsUsed)
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

static void addImageListAndFree(struct visiSearcher *searcher,
	struct slInt *imageList)
/* Add images in list to searcher with weight one.
 * Then free imageList */
{
struct slInt *image;
for (image = imageList; image != NULL; image = image->next)
    visiSearcherAdd(searcher, image->val, 1.0);
slFreeList(&imageList);
}

static void visiGeneMatchGene(struct visiSearcher *searcher, 
	struct sqlConnection *conn, struct slName *wordList)
/* Add images matching genes in wordList to searcher.
 * The wordList can include wildcards. */
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
    addImageListAndFree(searcher, imageList);
    freez(&sqlPat);
    }
}

static void visiGeneMatchAccession(struct visiSearcher *searcher, 
	struct sqlConnection *conn, struct slName *wordList)
/* Add images matching RefSeq, LocusLink, GenBank, or UniProt
 * accessions to searcher. */
{
struct slName *word;
for (word = wordList; word != NULL; word = word->next)
    {
    addImageListAndFree(searcher, visiGeneSelectRefSeq(conn, word->name));
    addImageListAndFree(searcher, visiGeneSelectLocusLink(conn, word->name));
    addImageListAndFree(searcher, visiGeneSelectGenbank(conn, word->name));
    addImageListAndFree(searcher, visiGeneSelectUniProt(conn, word->name));
    }
}

static void addImagesMatchingBodyPart(struct visiSearcher *searcher,
	struct sqlConnection *conn, struct dyString *dy, char *bodyPart)
/* Add images that are contributed by given contributor to
 * searcher with a weight of one.  Use dy for scratch space for
 * the query. */
{
dyStringClear(dy);
sqlDyStringPrintf(dy, 
   "select imageProbe.image from "
   "bodyPart,expressionLevel,imageProbe "
   "where bodyPart.name = \"%s\" "
   "and bodyPart.id = expressionLevel.bodyPart "
   "and expressionLevel.imageProbe = imageProbe.id"
   , bodyPart);
addImagesMatchingQuery(searcher, conn, dy->string);

dyStringClear(dy);
sqlDyStringPrintf(dy,
    "select image.id from bodyPart,specimen,image "
    "where bodyPart.name = \"%s\" "
    "and bodyPart.id = specimen.bodyPart "
    "and specimen.id = image.specimen");
addImagesMatchingQuery(searcher, conn, dy->string);
}

static void visiGeneMatchBodyPart(struct visiSearcher *searcher,
	struct sqlConnection *conn, struct slName *wordList)
/* Add images matching bodyPart to searcher.
 * This is a little complicated by some body parts containing
 * multiple words, like "choroid plexus". */
{
visiGeneMatchMultiWord(searcher, conn, wordList, "bodyPart",
    addImagesMatchingBodyPart);
}

void addImagesMatchingStage(struct visiSearcher *searcher,
	struct sqlConnection *conn, int schemeId, int taxon,
	char *minAge)
/* Given a developmental stage scheme (schemeId) and a specific
 * stage, return all images that match stage */
{
struct dyString *dy = dyStringNew(0);
char *maxAge;
struct sqlResult *sr;
char **row;

dyStringClear(dy);
sqlDyStringPrintf(dy, 
    "select age from lifeStage where lifeStageScheme = %d ", 
    schemeId);
dyStringPrintf(dy,
    "and age > %s order by age", minAge);
maxAge = sqlQuickString(conn, dy->string);

dyStringClear(dy);
sqlDyStringPrintf(dy, "select image.id from specimen,image ");
dyStringPrintf(dy, "where specimen.age >= %s ", minAge);
if (maxAge != NULL)
    dyStringPrintf(dy, "and specimen.age < %s ", maxAge);
dyStringPrintf(dy, "and specimen.taxon = %d ", taxon);
dyStringPrintf(dy, "and specimen.id = image.specimen");
addImagesMatchingQuery(searcher, conn, dy->string);

dyStringFree(&dy);
}


static void visiGeneMatchStage(struct visiSearcher *searcher,
	struct sqlConnection *conn, struct slName *wordList)
/* Add images matching Theiler or other developmental stage */
{
struct slName *word;
struct dyString *dy = dyStringNew(0);
struct sqlResult *sr;
char **row;
for (word = wordList; word != NULL && word->next != NULL; word = word->next)
    {
    int schemeId = 0,taxon=0;
    dyStringClear(dy);
    sqlDyStringPrintf(dy, 
        "select id,taxon from lifeStageScheme where name = \"%s\"",
    	word->name);
    sr = sqlGetResult(conn, dy->string);
    if ((row = sqlNextRow(sr)) != NULL)
        {
	schemeId = sqlUnsigned(row[0]);
	taxon = sqlUnsigned(row[1]);
	}
    sqlFreeResult(&sr);
    if (schemeId > 0)
        {
	char *specificStage = word->next->name;
	char *minAge;
	dyStringClear(dy);
	sqlDyStringPrintf(dy, "select age from lifeStage where name = \"%s\" ", 
		specificStage);
	dyStringPrintf(dy, "and lifeStageScheme = %d\n", schemeId);
	minAge = sqlQuickString(conn, dy->string);
	if (minAge != NULL)
	    addImagesMatchingStage(searcher, conn, schemeId, taxon, minAge);
	freez(&minAge);
	}
    }
dyStringFree(&dy);
}

static void addImagesMatchingYears(struct visiSearcher *searcher,
	struct sqlConnection *conn, int minYear, int maxYear)
/* Fold in images that are published between given years */
{
struct dyString *dy = dyStringNew(0);
sqlDyStringPrintf(dy,
    "select image.id from submissionSet,imageFile,image "
    "where submissionSet.year >= %d and submissionSet.year <= %d "
    "and submissionSet.id = imageFile.submissionSet "
    "and imageFile.id = image.imageFile"
    , minYear, maxYear);
addImagesMatchingQuery(searcher, conn, dy->string);
dyStringFree(&dy);
}

static void visiGeneMatchYear(struct visiSearcher *searcher, 
	struct sqlConnection *conn, struct slName *wordList)
/* Fold in matches to a year. */
{
struct slName *word;
char *now = sqlQuickString(conn, "NOSQLINJ select now()");
int currentYear = atoi(now);
int maxYear = currentYear+1;	/* May be slightly ahead of publication */
int minYear = 1988;	/* Oldest record in Jackson Labs database. */
for (word = wordList; word != NULL; word = word->next)
    {
    char *name = word->name;
    if (strlen(name) == 4 && isdigit(name[0]))
        {
	int year = atoi(name);
	if (year >= minYear && year <= maxYear)
	    addImagesMatchingYears(searcher, conn, year, year);
	}
    }
}

struct slInt *visiGeneSearch(char *searchString)
/* visiGeneSearch - Test out search routines for VisiGene. */
{
struct sqlConnection *conn = sqlConnect(database);
struct visiMatch *matchList, *match;
struct slInt *imageList = NULL, *image;
struct visiSearcher *searcher = visiSearcherNew();
struct slName *wordList = stringToSlNames(searchString);
uglyf("Searching %s for \"%s\"\n", database, searchString);
visiGeneMatchContributor(searcher, conn, wordList);
visiGeneMatchYear(searcher, conn, wordList);
visiGeneMatchGene(searcher, conn, wordList);
visiGeneMatchAccession(searcher, conn, wordList);
visiGeneMatchBodyPart(searcher, conn, wordList);
#ifdef SOON
visiGeneMatchStage(searcher, conn, wordList);
#endif /* SOON */
matchList = visiSearcherSortResults(searcher);
for (match = matchList; match != NULL; match = match->next)
    {
    uglyf("%f %d\n", match->weight, match->imageId);
    image = slIntNew(match->imageId);
    slAddHead(&imageList, image);
    }
slFreeList(&matchList);
slFreeList(&wordList);
slReverse(&imageList);
return imageList;
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
