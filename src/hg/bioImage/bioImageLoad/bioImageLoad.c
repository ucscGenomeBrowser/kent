/* bioImageLoad - Load data into bioImage database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "ra.h"

/* Variables you can override from command line. */
char *database = "bioImage";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bioImageLoad - Load data into bioImage database\n"
  "usage:\n"
  "   bioImageLoad setInfo.ra itemInfo.tab\n"
  "Please see bioImageLoad.doc for description of the .ra and .tab files\n"
  "Options:\n"
  "   -database=%s - Specifically set database\n"
  , database
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct hash *hashRowOffsets(char *line)
/* Given a space-delimited line, create a hash keyed by the words in 
 * line with values the position of the word (0 based) in line */
{
struct hash *hash = hashNew(0);
char *word;
int wordIx = 0;
while ((word = nextWord(&line)) != 0)
    {
    hashAdd(hash, word, intToPt(wordIx));
    wordIx += 1;
    }
return hash;
}

char *getVal(char *fieldName, struct hash *raHash, struct hash *rowHash, char **row, char *defaultVal)
/* Return value in row if possible, else in ra, else in default.  If no value and no default
 * return an error. */
{
char *val = NULL;
struct hashEl *hel = hashLookup(rowHash, fieldName);
if (hel != NULL)
    {
    int rowIx = ptToInt(hel->val);
    val = row[rowIx];
    }
else
    {
    val = hashFindVal(raHash, fieldName);
    if (val == NULL)
	{
	if (defaultVal != NULL)
	    val = defaultVal;
	else
	    errAbort("Can't find value for field %s", fieldName);
	}
    }
return val;
}

static char *requiredItemFields[] = {"fileName", "submitId"};
static char *requiredFields[] = {"fullDir", "thumbDir", "taxon", "isEmbryo", "age", "bodyPart", 
	"sliceType", "imageType", "contributer", };

void bioImageLoad(char *setRaFile, char *itemTabFile)
/* bioImageLoad - Load data into bioImage database. */
{
struct hash *raHash = raReadSingle(setRaFile);
struct hash *rowHash;
struct lineFile *lf = lineFileOpen(itemTabFile, TRUE);
char *line, *words[256];
int rowSize;

/* Read first line of tab file, and from it get all the field names. */
if (!lineFileNext(lf, &line, NULL))
    errAbort("%s appears to be empty", lf->fileName);
if (line[0] != '#')
    errAbort("First line of %s needs to start with #, and then contain field names",
    	lf->fileName);
rowHash = hashRowOffsets(line+1);
rowSize = rowHash->elCount;
if (rowSize >= ArraySize(words))
    errAbort("Too many fields in %s", lf->fileName);

/* Check that have all required fields */
    {
    char *fieldName;
    int i;

    for (i=0; i<ArraySize(requiredItemFields); ++i)
        {
	fieldName = requiredItemFields[i];
	if (!hashLookup(rowHash, fieldName))
	    errAbort("Field %s is not in %s", fieldName, itemTabFile);
	}

    for (i=0; i<ArraySize(requiredFields); ++i)
        {
	fieldName = requiredFields[i];
	if (!hashLookup(rowHash, fieldName) && !hashLookup(raHash, fieldName))
	    errAbort("Field %s is not in %s or %s", fieldName, setRaFile, itemTabFile);
	}
    }


/* Process rest of tab file. */
while (lineFileNextRowTab(lf, words, rowSize))
    {
    char *fileName = getVal("fileName", raHash, rowHash, words, NULL);
    char *submitId = getVal("submitId", raHash, rowHash, words, NULL);
    char *fullDir = getVal("fullDir", raHash, rowHash, words, NULL);
    char *thumbDir = getVal("thumbDir", raHash, rowHash, words, NULL);
    char *taxon = getVal("taxon", raHash, rowHash, words, NULL);
    char *isEmbryo = getVal("isEmbryo", raHash, rowHash, words, NULL);
    char *age = getVal("age", raHash, rowHash, words, NULL);
    char *bodyPart = getVal("bodyPart", raHash, rowHash, words, NULL);
    char *sliceType = getVal("sliceType", raHash, rowHash, words, NULL);
    char *imageType = getVal("imageType", raHash, rowHash, words, NULL);
    char *contributer = getVal("contributer", raHash, rowHash, words, NULL);
    char *sectionSet = getVal("sectionSet", raHash, rowHash, words, "");
    char *sectionIx = getVal("sectionIx", raHash, rowHash, words, "0");
    char *gene = getVal("gene", raHash, rowHash, words, "");
    char *locusLink = getVal("locusLink", raHash, rowHash, words, "");
    char *refSeq = getVal("refSeq", raHash, rowHash, words, "");
    char *genbank = getVal("genbank", raHash, rowHash, words, "");
    char *treatment = getVal("treatment", raHash, rowHash, words, "");
    char *isDefault = getVal("isDefault", raHash, rowHash, words, "0");
    char *publication = getVal("publication", raHash, rowHash, words, "");
    char *pubUrl = getVal("pubUrl", raHash, rowHash, words, "");
    char *setUrl = getVal("setUrl", raHash, rowHash, words, "");
    char *itemUrl = getVal("itemUrl", raHash, rowHash, words, "");
    // char *xzy = getVal("xzy", raHash, rowHash, words, xzy);

    uglyf("fileName: %s\n", fileName);
    uglyf("submitId: %s\n", submitId);
    uglyf("fullDir: %s\n", fullDir);
    uglyf("thumbDir: %s\n", thumbDir);
    uglyf("taxon: %s\n", taxon);
    uglyf("isEmbryo: %s\n", isEmbryo);
    uglyf("age: %s\n", age);
    uglyf("bodyPart: %s\n", bodyPart);
    uglyf("sliceType: %s\n", sliceType);
    uglyf("contributer: %s\n", contributer);
    uglyf("sectionSet: %s\n", sectionSet);
    uglyf("gene: %s\n", gene);
    uglyf("locusLink: %s\n", locusLink);
    uglyf("genbank: %s\n", genbank);
    uglyf("treatment: %s\n", treatment);
    uglyf("isDefault: %s\n", isDefault);
    uglyf("publication: %s\n", publication);
    uglyf("pubUrl: %s\n", pubUrl);
    uglyf("setUrl: %s\n", setUrl);
    uglyf("itemUrl: %s\n", itemUrl);
    uglyf("\n");
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
database = optionVal("database", database);
bioImageLoad(argv[1], argv[2]);
return 0;
}
