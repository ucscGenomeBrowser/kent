/* visiSearch - free form search of visiGene database. */

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


char titleMessage[1024] = "";

struct visiMatch *visiMatchNew(int imageId, int wordCount)
/* Create a new visiMatch structure, as yet with no weight. */
{
struct visiMatch *match;
AllocVar(match);
match->imageId = imageId;
match->wordBits = bitAlloc(wordCount);
return match;
}

void visiMatchFree(struct visiMatch **pMatch)
/* Free up memory associated with visiMatch */
{
struct visiMatch *match = *pMatch;
if (match != NULL)
    {
    bitFree(&match->wordBits);
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


struct visiSearcher
/* This collects together information for free format searches. */
    {
    struct visiSearcher *next;		/* Next search */
    struct visiMatch *matchList;	/* List of matching images. */
    struct rbTree *tree;		/* Tree for near random access. */
    int wordCount;			/* Number of words in search. */
    };

static struct visiSearcher *visiSearcherNew(int wordCount)
/* Create a new, empty search structure. */
{
struct visiSearcher *searcher;
AllocVar(searcher);
searcher->tree = rbTreeNew(visiMatchCmpImageId);
searcher->wordCount = wordCount;
return searcher;
}

static void visiSearcherFree(struct visiSearcher **pSearcher)
/* Free up memory associated with *pSearcher */
{
struct visiSearcher *searcher = *pSearcher;
if (searcher != NULL)
    {
    visiMatchFreeList(&searcher->matchList);
    rbTreeFree(&searcher->tree);
    freez(pSearcher);
    }
}

static struct visiMatch *visiSearcherAdd(struct visiSearcher *searcher,
	int imageId, double weight, int startWord, int wordCount)
/* Add given weight to match involving imageId,  creating
 * a fresh match if necessary for imageId. */
{
struct visiMatch key, *match;
key.imageId = imageId;
match = rbTreeFind(searcher->tree, &key);
if (match == NULL)
    {
    match = visiMatchNew(imageId, searcher->wordCount);
    slAddHead(&searcher->matchList, match);
    rbTreeAdd(searcher->tree, match);
    }
match->weight += weight;
assert(startWord + wordCount <= searcher->wordCount);
bitSetRange(match->wordBits, startWord, wordCount);
return match;
}

static void visiSearcherWeedResults(struct visiSearcher *searcher,
	struct sqlConnection *conn)
/* Get rid of images that are just partial matches, and also
 * images that are private.  This leaks a little memory - the
 * matches that are weeded out.*/
{
struct visiMatch *newList = NULL, *match, *next, key;
int wordCount = searcher->wordCount;
struct dyString *query = dyStringNew(0);
struct sqlResult *sr;
char **row;
int passCount = 0;

/* Construct query to fetch all non-private imageId's in matchList. */
sqlDyStringPrintf(query, "select image.id from image,submissionSet "
                      "where submissionSet.privateUser = 0 "
		      "and submissionSet.id = image.submissionSet "
		      "and image.id in (");
for (match = searcher->matchList; match != NULL; match = next)
    {
    next = match->next;
    if (bitCountRange(match->wordBits, 0, wordCount) == wordCount)
	{
	if (passCount != 0)
	    sqlDyStringPrintf(query, ",");
	sqlDyStringPrintf(query, "%d", match->imageId);
	++passCount;
	}
    }
sqlDyStringPrintf(query, ")");

/* Execute query, and put corresponding images on newList. */
if (passCount > 0)
    {
    sr = sqlGetResult(conn, query->string);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	key.imageId = sqlUnsigned(row[0]);
	match = rbTreeFind(searcher->tree, &key);
	if (match == NULL)
	    internalErr();
	slAddHead(&newList, match);
	}
    slReverse(&newList);
    }
searcher->matchList = newList;
dyStringFree(&query);
}

static struct visiMatch *visiSearcherSortResults(struct visiSearcher *searcher,
	struct sqlConnection *conn)
/* Get sorted list of match results from searcher. 
 * You don't own the list returned though.  It will evaporate
 * with visiSearcherFree. */
{
visiSearcherWeedResults(searcher, conn);
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
	struct sqlConnection *conn, struct dyString *dy, char *exactMatch,
	int startWord, int wordCount);
/* Typedef of function to add things that are exact matches of
 * some type to searcher.  This is broken out separately
 * so that the visiGeneMatchMultiWord function below can be
 * used in many contexts. */

static void visiGeneMatchMultiWord(struct visiSearcher *searcher, 
	struct sqlConnection *conn, struct slName *wordList,
	char *table, char *field, AdderFunc adder)
/* This helps cope with matches that may involve more than
 * one word.   It will preferentially match as many words
 * as possible, and if there is a multiple-word match it
 * will take that over a single-word match. */
{
struct slName *word;
struct dyString *query = dyStringNew(0);
int wordIx;

for (word = wordList, wordIx=0; word != NULL;  ++wordIx)
    {
    struct slName *nameList = NULL, *name;
    int maxWordsUsed = 0;

    if (strlen(word->name) >= 3) /* Logic could be expensive on small words */
	{
	dyStringClear(query);
	sqlDyStringPrintf(query, "select %s from %s where %s like '%s%%'",
	    field, table, field, word->name);
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
	}
    if (maxWordsUsed > 0)
	{
	for (name = nameList; name != NULL; name = name->next)
	    {
	    if (countWordsUsedInPhrase(name->name, word) == maxWordsUsed)
		(*adder)(searcher, conn, query, name->name, 
			wordIx, maxWordsUsed);
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
    struct sqlConnection *conn, char *query, struct hash *uniqHash,
    char *searchTerm, int startWord, int wordCount)
/* Add images that match query */
{
struct sqlResult *sr;
char **row;
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    boolean skip = FALSE;
    if (uniqHash)
	{
	char uniqString[512];
	safef(uniqString, sizeof(uniqString), "%s#%s", row[0], searchTerm);
	if (hashLookup(uniqHash, uniqString))
	    skip = TRUE;
	else
	    hashAdd(uniqHash, uniqString, NULL);
	}
   if (!skip)
       visiSearcherAdd(searcher, sqlUnsigned(row[0]), 1.0, 
       	startWord, wordCount);
   }
sqlFreeResult(&sr);
}

static void addImagesMatchingName(struct visiSearcher *searcher,
	struct sqlConnection *conn, struct dyString *dy, char *contributor,
	int startWord, int wordCount)
/* Add images that are contributed by given contributor to
 * searcher with a weight of one.  Use dy for scratch space for
 * the query. */
{
dyStringClear(dy);
sqlDyStringPrintf(dy, 
   "select image.id from "
   "contributor,submissionContributor,imageFile,image "
   "where contributor.name = '%s' "
   "and contributor.id = submissionContributor.contributor "
   "and submissionContributor.submissionSet = imageFile.submissionSet "
   "and imageFile.id = image.imageFile"
   , contributor);
addImagesMatchingQuery(searcher, conn, dy->string, NULL, NULL, 
	startWord, wordCount);
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
int wordIx;

for (word = wordList, wordIx=0; word != NULL;  wordIx++)
    {
    struct slName *nameList, *name;
    int maxWordsUsed = 0;
    dyStringClear(query);
    sqlDyStringPrintf(query, "select name from contributor where name like '%s %%'", word->name);
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
	        addImagesMatchingName(searcher, conn, query, name->name, 
			wordIx, maxWordsUsed);
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
	struct slInt *imageList, int startWord, int wordCount)
/* Add images in list to searcher with weight one.
 * Then free imageList */
{
struct slInt *image;
for (image = imageList; image != NULL; image = image->next)
    visiSearcherAdd(searcher, image->val, 1.0, startWord, wordCount);
slFreeList(&imageList);
}

static void visiGeneMatchGene(struct visiSearcher *searcher, 
	struct sqlConnection *conn, struct slName *wordList)
/* Add images matching genes in wordList to searcher.
 * The wordList can include wildcards. */
{
struct slName *word;
int wordIx;
for (word = wordList, wordIx=0; word != NULL; word = word->next, ++wordIx)
    {
    char *sqlPat = sqlLikeFromWild(word->name);
    struct slInt *imageList;
    if (sqlWildcardIn(sqlPat))
	 imageList = visiGeneSelectNamed(conn, sqlPat, vgsLike);
    else
	 imageList = visiGeneSelectNamed(conn, sqlPat, vgsExact);
    addImageListAndFree(searcher, imageList, wordIx, 1);
    freez(&sqlPat);
    }
}

static void visiGeneMatchAccession(struct visiSearcher *searcher, 
	struct sqlConnection *conn, struct slName *wordList)
/* Add images matching RefSeq, LocusLink, GenBank, or UniProt
 * accessions to searcher. */
{
struct slName *word;
int wordIx;
for (word = wordList, wordIx=0; word != NULL; word = word->next, ++wordIx)
    {
    addImageListAndFree(searcher, visiGeneSelectRefSeq(conn, word->name),
    	wordIx, 1);
    addImageListAndFree(searcher, visiGeneSelectLocusLink(conn, word->name),
    	wordIx, 1);
    addImageListAndFree(searcher, visiGeneSelectGenbank(conn, word->name),
    	wordIx, 1);
    addImageListAndFree(searcher, visiGeneSelectUniProt(conn, word->name),
    	wordIx, 1);
    }
}

static void visiGeneMatchSubmitId(struct visiSearcher *searcher, 
	struct sqlConnection *conn, struct slName *wordList)
/* Add images matching submitId  accessions to searcher. */
{
struct slName *word;
int wordIx;
for (word = wordList, wordIx=0; word != NULL; word = word->next, ++wordIx)
    {
    char query[512];
    sqlSafef(query, sizeof(query),
    	"select image.id from image,imageFile "
	"where imageFile.submitId = '%s' "
	"and imageFile.id = image.imageFile", word->name);
    addImageListAndFree(searcher, sqlQuickNumList(conn, query),
	wordIx, 1);
    }
}

static void addImagesMatchingBodyPart(struct visiSearcher *searcher,
	struct sqlConnection *conn, struct dyString *dy, char *bodyPart,
	int startWord, int wordCount)
/* Add images that are contributed by given contributor to
 * searcher with a weight of one.  Use dy for scratch space for
 * the query. */
{
struct hash *uniqHash = newHash(0);

dyStringClear(dy);
sqlDyStringPrintf(dy, 
   "select imageProbe.image from "
   "bodyPart,expressionLevel,imageProbe "
   "where bodyPart.name = '%s' "
   "and bodyPart.id = expressionLevel.bodyPart "
   "and expressionLevel.imageProbe = imageProbe.id "
   "and expressionLevel.level > 0"
   , bodyPart);
addImagesMatchingQuery(searcher, conn, dy->string, uniqHash, bodyPart,
	startWord, wordCount);

dyStringClear(dy);
sqlDyStringPrintf(dy,
    "select image.id from bodyPart,specimen,image "
    "where bodyPart.name = '%s' "
    "and bodyPart.id = specimen.bodyPart "
    "and specimen.id = image.specimen", bodyPart);

addImagesMatchingQuery(searcher, conn, dy->string, uniqHash, bodyPart,
	startWord, wordCount);
hashFree(&uniqHash);
}

static void visiGeneMatchBodyPart(struct visiSearcher *searcher,
	struct sqlConnection *conn, struct slName *wordList)
/* Add images matching bodyPart to searcher.
 * This is a little complicated by some body parts containing
 * multiple words, like "choroid plexus". */
{
visiGeneMatchMultiWord(searcher, conn, wordList, 
    "bodyPart", "name",
    addImagesMatchingBodyPart);
}

static void visiGeneMatchSex(struct visiSearcher *searcher,
	struct sqlConnection *conn, struct slName *wordList)
/* Add images matching bodyPart to searcher.
 * This is a little complicated by some body parts containing
 * multiple words, like "choroid plexus". */
{
struct dyString *query = dyStringNew(0);
struct slName *word;
int wordIx;
for (word = wordList, wordIx=0; word != NULL; word = word->next, ++wordIx)
    {
    dyStringClear(query);
    sqlDyStringPrintf(query, "select image.id from sex,specimen,image ");
    sqlDyStringPrintf(query, "where sex.name = '%s' ",  word->name);
    sqlDyStringPrintf(query, "and sex.id = specimen.sex ");
    sqlDyStringPrintf(query, "and specimen.id = image.specimen");
    addImagesMatchingQuery(searcher, conn, query->string, NULL, NULL,
    	wordIx, 1);
    } 
}

void addImagesMatchingStage(struct visiSearcher *searcher,
	struct sqlConnection *conn, int schemeId, int taxon,
	char *minAge, int wordIx, int wordCount)
/* Given a developmental stage scheme (schemeId) and a specific
 * stage, return all images that match stage */
{
struct dyString *dy = dyStringNew(0);
char *maxAge;
// note in the code below minAge and maxAge are strings
//  but they should contain float values.  Putting single-quotes
//  around them and escaping their contents is something that will 
//  protect against sql injection.
dyStringClear(dy);
sqlDyStringPrintf(dy, 
    "select age from lifeStage where lifeStageScheme = %d ", 
    schemeId);
sqlDyStringPrintf(dy,
    "and age > '%s' order by age", minAge);
maxAge = sqlQuickString(conn, dy->string);

dyStringClear(dy);
sqlDyStringPrintf(dy, "select image.id from specimen,image ");
sqlDyStringPrintf(dy, "where specimen.age >= '%s' ", minAge);
if (maxAge != NULL)
    sqlDyStringPrintf(dy, "and specimen.age < '%s' ", maxAge);
sqlDyStringPrintf(dy, "and specimen.taxon = %d ", taxon);
sqlDyStringPrintf(dy, "and specimen.id = image.specimen");
addImagesMatchingQuery(searcher, conn, dy->string, NULL, NULL,
	wordIx, wordCount);

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
int wordIx;

for (word = wordList, wordIx=0; word != NULL && word->next != NULL; 
	word = word->next, ++wordIx)
    {
    int schemeId = 0,taxon=0;
    dyStringClear(dy);
    sqlDyStringPrintf(dy, 
        "select id,taxon from lifeStageScheme where name = '%s'", word->name);
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
	sqlDyStringPrintf(dy, "select age from lifeStage where name = '%s' ", 
		specificStage);
	sqlDyStringPrintf(dy, "and lifeStageScheme = %d\n", schemeId);
	minAge = sqlQuickString(conn, dy->string);
	if (minAge != NULL)
	    addImagesMatchingStage(searcher, conn, schemeId, taxon, minAge,
	    	wordIx, 2);
	freez(&minAge);
	}
    }
dyStringFree(&dy);
}

static void addImagesMatchingYears(struct visiSearcher *searcher,
	struct sqlConnection *conn, int minYear, int maxYear, int wordIx)
/* Fold in images that are published between given years */
{
struct dyString *dy = dyStringNew(0);
sqlDyStringPrintf(dy,
    "select image.id from submissionSet,imageFile,image "
    "where submissionSet.year >= %d and submissionSet.year <= %d "
    "and submissionSet.id = imageFile.submissionSet "
    "and imageFile.id = image.imageFile"
    , minYear, maxYear);
addImagesMatchingQuery(searcher, conn, dy->string, NULL, NULL,
	wordIx, 1);
dyStringFree(&dy);
}

static void visiGeneMatchYear(struct visiSearcher *searcher, 
	struct sqlConnection *conn, struct slName *wordList)
/* Fold in matches to a year. */
{
struct slName *word;
char *now = sqlQuickString(conn, NOSQLINJ "select now()");
int currentYear = atoi(now);
int maxYear = currentYear+1;	/* May be slightly ahead of publication */
int minYear = 1988;	/* Oldest record in Jackson Labs database. */
int wordIx;
for (word = wordList, wordIx=0; word != NULL; word = word->next, ++wordIx)
    {
    char *name = word->name;
    if (strlen(name) == 4 && isdigit(name[0]))
        {
	int year = atoi(name);
	if (year >= minYear && year <= maxYear)
	    addImagesMatchingYears(searcher, conn, year, year, wordIx);
	}
    }
}

static void visiGeneMatchProbeId(struct visiSearcher *searcher, 
	struct sqlConnection *conn, struct slName *wordList)
/* Fold in matches to a probe ID. */
{
struct slName *word;
int wordIx;
for (word = wordList, wordIx=0; word != NULL; word = word->next, ++wordIx)
    {
    char *name = word->name;
    char *pat = "vgPrb_";
    if (startsWith(pat,  name))
        {
	struct sqlResult *sr;
	char **row, query[256];
	int probeId = atoi(name + strlen(pat));
	sqlSafef(query, sizeof(query),
	    "select ip.image from imageProbe ip, vgPrbMap m, vgPrb e"
	    " where ip.probe = m.probe and m.vgPrb = e.id"
	    " and e.id=%d", probeId);
	sr = sqlGetResult(conn, query);
	while ((row = sqlNextRow(sr)) != NULL)
	    visiSearcherAdd(searcher, sqlUnsigned(row[0]),  1.0, wordIx, 1);
	sqlFreeResult(&sr);
	}
    }
}

static void addImagesMatchingBinomial(struct visiSearcher *searcher,
	struct sqlConnection *conn, struct dyString *dy, char *binomial,
	int startWord, int wordCount)
/* Add images that match binomial name. */
{
dyStringClear(dy);
sqlDyStringPrintf(dy, 
   "select distinct image.id from "
   "image,specimen,uniProt.taxon "
   "where uniProt.taxon.binomial = '%s' "
   "and specimen.taxon = uniProt.taxon.id "
   "and image.specimen = specimen.id"
   , binomial);
addImagesMatchingQuery(searcher, conn, dy->string, NULL, binomial,
	startWord, wordCount);
}

static void addImagesMatchingCommonName(struct visiSearcher *searcher,
	struct sqlConnection *conn, struct dyString *dy, char *commonName,
	int startWord, int wordCount)
/* Add images that match common name. */
{
dyStringClear(dy);
sqlDyStringPrintf(dy, 
   "select distinct image.id from "
   "image,specimen,uniProt.commonName "
   "where uniProt.commonName.val = '%s' "
   "and specimen.taxon = uniProt.commonName.taxon "
   "and image.specimen = specimen.id"
   , commonName);
addImagesMatchingQuery(searcher, conn, dy->string, NULL, commonName,
	startWord, wordCount);
}


static void visiGeneMatchOrganism(struct visiSearcher *searcher, 
	struct sqlConnection *conn, struct slName *wordList)
/* Fold in matches to organism. */
{
visiGeneMatchMultiWord(searcher, conn, wordList, "uniProt.taxon", "binomial",
    addImagesMatchingBinomial);
visiGeneMatchMultiWord(searcher, conn, wordList, "uniProt.commonName", "val",
    addImagesMatchingCommonName);
}

static void visiGeneMatchDescription(struct visiSearcher *searcher, 
	struct sqlConnection *conn, struct slName *wordList)
/* Fold in matches to description - using full text index. */
{
struct trix *trix = NULL;
struct slName *word;
int wordIx;
if (!fileExists("visiGeneData/visiGene.ix"))
    {
    safef(titleMessage,sizeof(titleMessage),
    	" - visiGene.ix not found, run vgGetText");
    fprintf(stderr,"%s\n",titleMessage);
    return;
    }
trix = trixOpen("visiGeneData/visiGene.ix");

for (word=wordList, wordIx=0; word != NULL; word = word->next, ++wordIx)
    {
    char *s = cloneString(word->name);
    struct trixSearchResult *tsr, *tsrList;
    tolowers(s);
    tsrList = trixSearch(trix, 1, &s, tsmExact);
    for (tsr = tsrList; tsr != NULL; tsr = tsr->next)
        visiSearcherAdd(searcher, sqlUnsigned(tsr->itemId), 1.0, wordIx, 1);
    trixSearchResultFreeList(&tsrList);
    freez(&s);
    }

trixClose(&trix);
}

struct visiMatch *visiSearch(struct sqlConnection *conn, char *searchString)
/* visiSearch - return list of images that match searchString sorted
 * by how well they match. This will search most fields in the
 * database. */
{
struct visiMatch *matchList;
struct slName *wordList = stringToSlNames(searchString);
int wordCount = slCount(wordList);
struct visiSearcher *searcher = visiSearcherNew(wordCount);
visiGeneMatchContributor(searcher, conn, wordList);
visiGeneMatchYear(searcher, conn, wordList);
visiGeneMatchProbeId(searcher, conn, wordList);
visiGeneMatchGene(searcher, conn, wordList);
visiGeneMatchAccession(searcher, conn, wordList);
visiGeneMatchBodyPart(searcher, conn, wordList);
visiGeneMatchSex(searcher, conn, wordList);
visiGeneMatchStage(searcher, conn, wordList);
visiGeneMatchSubmitId(searcher, conn, wordList);
visiGeneMatchOrganism(searcher, conn, wordList);
visiGeneMatchDescription(searcher, conn, wordList);
matchList = visiSearcherSortResults(searcher, conn);
searcher->matchList = NULL; /* Transferring memory ownership to return val. */
visiSearcherFree(&searcher);
slFreeList(&wordList);
return matchList;
}

