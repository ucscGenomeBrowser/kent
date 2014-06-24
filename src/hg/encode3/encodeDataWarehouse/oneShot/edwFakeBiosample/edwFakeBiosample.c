/* edwFakeBiosample - Fake up biosample table from meld of encode3 and encode2 sources.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "obscure.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "htmlPage.h"
#include "jsonParse.h"

char *jsonOutFile = NULL;
char *cacheName = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwFakeBiosample - Fake up biosample table from meld of encode3 and encode2 sources.\n"
  "usage:\n"
  "   edwFakeBiosample cellTypeSlim.tab edwBiosample.tab url userId password\n"
  "Where cellTypeSlim.tab is the result of running the command on hgwdev\n"
  "    hgsql -e 'select term,sex,organism from cellType' -N encode2Meta\n"
  "and also lives in the source tree.  The output is intended for the edwBiosample table.\n"
  "The url is something like www.encodedcc.org/biosamples/?format=json&limit=all\n"
  "but with user name and password sent in the https fashion.\n"
  "options:\n"
  "   -cache=cacheName - get JSON list from cache rather than from database where possible\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"cache", OPTION_STRING},
   {NULL, 0},
};

char *getTextViaHttps(char *url, char *userId, char *password)
/* getJsonViaHttps - Fetch text from url that is https password protected.  This
 * will return a NULL rather than aborting if URL not found. */
{
char fullUrl[1024];
safef(fullUrl, sizeof(fullUrl), "https://%s:%s@%s", userId, password, url);
struct htmlPage *page = htmlPageGet(fullUrl);
uglyf("getTextViaHttps %s page=%p\n", fullUrl, page);
if (page == NULL)
    return NULL;
char *text = NULL;
struct htmlStatus *st = page->status;
if (st->status == 200)
    text = cloneString(page->htmlText);
htmlPageFree(&page);
return text;
}

char *getCachedText(char *name, char *url, char *userId, char *password)
/* Get file from cache if possible,   if not from URL, and then save to cache */
{
if (cacheName != NULL)
    {
    char fileName[PATH_LEN];
    safef(fileName, PATH_LEN, "%s/%s.json", cacheName, name);
    if (fileExists(fileName))
        {
	char *jsonText;
	readInGulp(fileName, &jsonText, NULL);
	return jsonText;
	}
    else
        {
	char *jsonText = getTextViaHttps(url, userId, password);
        writeGulp(fileName, jsonText, strlen(jsonText));
	return jsonText;
	}
    }
else
    return getTextViaHttps(url, userId, password);
}

int stanfordSexed = 0;

static char *abbreviateSex(char *in)
/* Return M, F, or U */
{
if (sameWord(in, "male"))
    return "M";
else if (sameWord(in, "female"))
    return "F";
else if (sameWord(in, "unknown"))
    return "U";
else if (sameWord(in, "mixed"))
    return "B";
else
    errAbort("Unrecognized sex %s", in);
return NULL;
}

void saveStanfordBiosample(char *userId, char *password, char *term, char *acc, FILE *f)
/* Try and figure out a line for out tab separated output by asking stanford. */
{
char url[1024];
safef(url, sizeof(url), "www.encodedcc.org/biosamples/%s/?format=json", acc);
char *text = getCachedText(acc, url, userId, password);
struct jsonElement *json = jsonParse(text);
// Fish out biosample_term_id - typically something like "UBERON:002107"
struct jsonElement *donor = jsonFindNamedField(json, url, "donor");
if (donor != NULL)
    {
    char *sex = jsonOptionalStringField(donor, "sex", "male");
    struct jsonElement *organism = jsonFindNamedField(json, "donor", "organism");
    if (organism != NULL)
        {
	char *taxon = jsonStringField(organism, "taxon_id");
	fprintf(f, "0\t%s\t%s\t%s\n",  term, taxon, abbreviateSex(sex));
	++stanfordSexed;
	}
    }
freez(&text);
}

void edwFakeBiosample(char *input, char *url, char *userId, char *password, char *output)
/* edwFakeBiosample - Fake up biosample table from meld of encode3 and encode2 sources.. */
{
struct hash *biosampleHash = hashNew(0);
/* Process UCSC part from file */
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(output, "w");
char *row[3];
while (lineFileRow(lf, row))
    {
    char *term = row[0];
    char *sex = row[1];
    char *organism = row[2];
    unsigned taxon = 0;
    if (sameWord(organism, "human")) taxon = 9606;
    else if (sameWord(organism, "mouse")) taxon = 10090;
    else errAbort("Unrecognized organism %s", organism);
    fprintf(f, "0\t%s\t%u\t%s\n",  term, taxon, sex);
    hashAdd(biosampleHash, term, cloneString(sex));
    }
lineFileClose(&lf);
verbose(1, "Read %d from %s\n", biosampleHash->elCount, input);

/* Process Stanford biosample list into hash */
struct hash *stanfordAccHash = hashNew(0);  // Keyed by term, value is ENCBS #
char *jsonText = getCachedText("biosamples", url, userId, password);
if (jsonText == NULL)
     errAbort("Couldn't get text response from %s\nUser %s, password %s\n", url, userId, password);
struct jsonElement *jsonRoot = jsonParse(jsonText);
    
char *bioListName = "@graph";
struct jsonElement *jsonBioList = jsonMustFindNamedField(jsonRoot, "", bioListName);
struct slRef *ref, *refList = jsonListVal(jsonBioList, bioListName);
for (ref = refList; ref != NULL; ref = ref->next)
    {
    struct jsonElement *el = ref->val;
    char *acc = jsonStringField(el, "accession");
    char *term = jsonStringField(el, "biosample_term_name");
    if (term != NULL && acc != NULL)
        hashAdd(stanfordAccHash, term, cloneString(acc));
    }
verbose(1, "Got %d biotypes with term and accession\n", stanfordAccHash->elCount);

/* Look for experiments that we don't have biosample info on */
struct sqlConnection *conn = edwConnect();
char query[512];
safef(query, sizeof(query), "select distinct biosample from edwExperiment");
struct slName *expBio, *expBioList = sqlQuickList(conn, query);
int ucscCount = 0, stanfordCount = 0, missCount = 0;
for (expBio = expBioList; expBio != NULL; expBio = expBio->next)
    {
    if (hashLookup(biosampleHash, expBio->name) == NULL)
        {
	char *acc = hashFindVal(stanfordAccHash, expBio->name);
	if (acc != NULL)
	    {
	    ++stanfordCount;
	    verbose(2, "Looking up %s at Stanford\n", expBio->name);
	    saveStanfordBiosample(userId, password, expBio->name, acc, f);
	    }
	else
	    {
	    ++missCount;
	    }
	}
    else
        ++ucscCount;
    }
verbose(1, "ucscCount %d, stanfordCount %d, miss %d\n", ucscCount, stanfordCount, missCount);
verbose(1, "stanfordSexed %d\n", stanfordSexed);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
cacheName = optionVal("cache", cacheName);
if (cacheName)
    makeDirsOnPath(cacheName);
edwFakeBiosample(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
