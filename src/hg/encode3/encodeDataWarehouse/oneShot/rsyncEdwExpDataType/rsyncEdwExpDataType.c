/* rsyncEdwExpDataType - Get experiment and data types from ENCODED via json, and from 
 * encode2 database via sql. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jsHelper.h"
#include "htmlPage.h"
#include "portable.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "obscure.h"
#include "encode/encodeExp.h"


/* Variables settable from command line */
char *cacheName = NULL;
boolean fresh = FALSE;
boolean noStanford = FALSE;
boolean noUcsc = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "rsyncEdwExpDataType - Get experiment and data types from ENCODED via json, and from\n"
  "encode2 database via sql\n"
  "usage:\n"
  "   rsyncEdwExpDataType url userId password out.tab\n"
  "where URL is 'submit.encodedcc.org/experiments/?format=json' with quotes most likely\n"
  "and the userId and password are programatically generated things obtained from Laurence Rowe\n"
  "at Stanford most likely.\n"
  "Options:\n"
  "   -cache=cacheName - get JSON list from cache rather than from database where possible\n"
  "   -fresh - ignore existing edwExperiment table\n"
  "   -noStanford - ignore Stanford/JSON bits\n"
  "   -noUcsc - ignore UCSC old Encode2 bits\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"cache", OPTION_STRING},
   {"fresh", OPTION_BOOLEAN},
   {"noStanford", OPTION_BOOLEAN},
   {"noUcsc", OPTION_BOOLEAN},
   {NULL, 0},
};

char *getTextViaHttps(char *url, char *userId, char *password)
/* getJsonViaHttps - Fetch text from url that is https password protected.  This
 * will return a NULL rather than aborting if URL not found. */
{
char fullUrl[1024];
safef(fullUrl, sizeof(fullUrl), "https://%s:%s@%s", userId, password, url);
struct htmlPage *page = htmlPageGet(fullUrl);
if (page == NULL)
    return NULL;
char *text = NULL;
struct htmlStatus *st = page->status;
if (st->status == 200)
    text = cloneString(page->htmlText);
htmlPageFree(&page);
return text;
}

char *fromAgreeingObjectFields(char *expName, struct slRef *replicaList, char *object, char *field)
/* Return given string valued field from replica.object.field assuming all replicas agree. */
{
char *val = NULL;
struct slRef *repRef;
for (repRef = replicaList; repRef != NULL; repRef = repRef->next)
    {
    struct jsonElement *rep = repRef->val;
    struct jsonElement *obj = jsonFindNamedField(rep, "", object);
    if (obj != NULL)
        {
	struct jsonElement *objField = jsonFindNamedField(obj, object, field);
	if (objField != NULL)
	    {
	    char *newVal = jsonStringVal(objField, field);
	    if (val == NULL)
	        val = newVal;
	    else if (!sameString(val, newVal))
	        errAbort("%s of replicas of %s disagree on %s: %s vs %s",
		    object, expName, field, val, newVal);
	    }
	}
    }
return val;
}

char *fromAgreeingLibs(char *expName, struct slRef *replicaList, char *field)
/* Return given string valued field from replica.library assuming all replicas agree. */
{
return fromAgreeingObjectFields(expName, replicaList, "library", field);
}

char *findCacheFileName(char *table, char *accession, char fileName[PATH_LEN])
/* Fill in fileName and return it if cacingis turned on. */
{
if (cacheName)
    {
    safef(fileName, PATH_LEN, "%s/%s.json", cacheName, accession);
    return fileName;
    }
return NULL;
}

char *getStanfordJson(char *table, char *accession, char *userId, char *password)
/* Get json text associated with an object */
{
char url[512];
safef(url, sizeof(url), "submit.encodedcc.org/%s%s/?format=json", table, accession);
verbose(1, "Fetching from %s\n", url);
return getTextViaHttps(url, userId, password);
}

char *getCachedJson(char *table, char *accession, char *userId, char *password)
/* Get json text associated with experiment from cache or rest query.
 * Save result in cache possibly.  May return NULL if object doesn't exist either
 * locally or remotely. */
{
char cacheFile[PATH_LEN];
if (findCacheFileName(table, accession, cacheFile))
    {
    if (fileExists(cacheFile))
        {
	char *result;
	readInGulp(cacheFile, &result, NULL);
	return result;
	}
    }
char *jsonText = getStanfordJson(table, accession, userId, password);
if (cacheName && jsonText)
    writeGulp(cacheFile, jsonText, strlen(jsonText));
return jsonText;
}

struct jsonElement *mightGetParsedJsonForId(char *table, char *accession, 
    char *userId, char *password)
/* Return parsed out experiment, handling caching */
{
char *jsonText = getCachedJson(table, accession, userId, password);
if (jsonText == NULL)
    return NULL;
struct jsonElement *el = jsonParse(jsonText);
freez(&jsonText);
return el;
}

struct jsonElement *getParsedJsonForId(char *table, char *accession, 
    char *userId, char *password)
/* Return parsed out experiment, handling caching */
{
struct jsonElement *el = mightGetParsedJsonForId(table, accession, userId, password);
if (el == NULL)
    errAbort("Couldn't get json %s %s", table, accession);
return el;
}

char *chipTarget(char *expAccession, char *userId, char *password)
/* Load JSON and find somewhere inside of it target if possible */
{
char *result = NULL;
struct jsonElement *exp = getParsedJsonForId("experiments/", expAccession, userId, password);
if (exp)
    {
    struct jsonElement *targ = jsonFindNamedField(exp, expAccession, "target");
    result = jsonOptionalStringField(targ, "name", NULL);
    }
return result;
}

char *findControl(char *expAccession, char *userId, char *password)
/* Load up JSON and find somewhere inside of it control if possible. */
{
char *result = NULL;
struct jsonElement *exp = getParsedJsonForId("experiments/", expAccession, userId, password);
if (exp)
    {
    struct jsonElement *possibles = jsonFindNamedField(exp, expAccession, "possible_controls");
    struct slRef *ref, *refList = jsonListVal(possibles, "possible_controls");
    for (ref = refList; ref != NULL; ref = ref->next)
        {
	struct jsonElement *controls = ref->val;
	char *controlExperiment = jsonOptionalStringField(controls, "accession", NULL);
	if (controlExperiment != NULL)
	    {
	    result = cloneString(controlExperiment);
	    break;
	    }
	}
    }
return result;
}

char *rnaSubtype(char *expAccession, char *userId, char *password)
/* Figure out subtype of RNA for experiment.  We do this looking at fields in replicates[].library */
{
struct jsonElement *exp = getParsedJsonForId("experiments/", expAccession, userId, password);
struct jsonElement *replicatesArray = jsonFindNamedField(exp, NULL, "replicates");
if (replicatesArray == NULL)
     return NULL;
struct slRef *refList = jsonListVal(replicatesArray, "replicates");
if (refList == NULL)
     return NULL;
char *nucTypeName = "nucleic_acid_term_name";
char *nucType = fromAgreeingLibs(expAccession, refList, nucTypeName);
if (nucType == NULL)
     errAbort("Missing %s from %s replicates library", nucTypeName, expAccession);
if (sameString("miRNA", nucType))
    return "miRNA-seq";

char *sizeRangeName = "size_range";
char *sizeRange = fromAgreeingLibs(expAccession, refList, sizeRangeName);
if (sizeRange == NULL)
     {
     warn("Missing %s from %s replicates library", sizeRangeName, expAccession);
     return "RNA-seq";
     }
if (sameString("<200", sizeRange))
    {
    return "Short RNA-seq";
    }
else if (sameString(">200", sizeRange))
    {
    return "Long RNA-seq";
    }
else
    errAbort("Unrecognized sizeRange %s in %s", sizeRange, expAccession);
return NULL;
}

struct hash *hashExpTable(struct sqlConnection *conn)
/* Load up hash filled with edwExperiment items keyed by accession */
{
struct hash *hash = hashNew(0);
struct edwExperiment *expList = edwExperimentLoadByQuery(conn, "select * from edwExperiment");
struct edwExperiment *exp;
for (exp = expList; exp != NULL; exp = exp->next)
    hashAdd(hash, exp->accession, exp);
return hash;
}

void rsyncStanfordExp(char *url, char *userId, char *password, FILE *f)
/* Get data from Stanford ENCODED via JSON */
{
struct hash *oldHash = (fresh ? hashNew(0) : hashExpTable(edwConnect()));
if (cacheName)
    makeDirsOnPath(cacheName);
struct jsonElement *jsonRoot = getParsedJsonForId("", "experiments", userId, password);
char *expListName = "@graph";
struct jsonElement *jsonExpList = jsonMustFindNamedField(jsonRoot, "", expListName);
verbose(1, "Got @graph %p\n", jsonExpList);
struct slRef *ref, *refList = jsonListVal(jsonExpList, expListName);
verbose(1, "Got %d experiments\n", slCount(refList));
int realExpCount = 0;
for (ref = refList; ref != NULL; ref = ref->next)
    {
    struct jsonElement *el = ref->val;
    char *acc = jsonStringField(el, "accession");
    char *assayType = jsonStringField(el, "assay_term_name");
    char *dataType = assayType;
    if (dataType != NULL)
        {
	char *rfa = jsonOptionalStringField(el, "award.rfa", "");
	if (!isEmpty(rfa) && sameString(rfa, "ENCODE3") && sameString(dataType, "RNA-seq"))
	    {
	    char *newDataType = rnaSubtype(acc, userId, password);
	    verbose(1, "%s -> %s\n", dataType, naForNull(newDataType));
	    dataType = newDataType;
	    }
	if (dataType != NULL)
	    {
	    struct edwExperiment *oldExp = hashFindVal(oldHash, acc);
	    char *ipTarget = "", *control = "";
	    if (sameString(dataType, "ChIP-seq") && sameString(rfa, "ENCODE3"))
	        {
		ipTarget = emptyForNull(chipTarget(acc, userId, password));
		verbose(1, "ipTarget %s\n", ipTarget);
		control = findControl(acc, userId, password);
		}
	    if (oldExp != NULL)
		{
		if (!sameString(oldExp->dataType, dataType))
		    errAbort("Change in data type for %s %s vs %s\n", 
			    acc, oldExp->dataType, dataType);
		}
	    fprintf(f, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", acc, dataType,
		jsonOptionalStringField(el, "lab.title", ""),
		jsonOptionalStringField(el, "biosample_term_name", ""),
		rfa, assayType, ipTarget, emptyForNull(control));
	    ++realExpCount;
	    }
	}
    }
verbose(1, "Got %d experiments with dataType\n", realExpCount);
}

char *fromUcscDataType(struct encodeExp *exp, struct slPair *varList)
/* Translate ChipSeq to ChIP-seq and the like. */
/* Pretty soon may be able to translate from UCSC to Stanford experiment instead. */
{
char *oldType = exp->dataType;
if (sameString(oldType, "ChipSeq")) return "ChIP-seq";
else if (sameString(oldType, "RnaSeq")) 
    {
    char *newType = "RNA-seq";	// Generic by default
    char *rnaExtract = slPairFindVal(varList, "rnaExtract");
    if (rnaExtract != NULL)
         {
	 if (startsWith("long", rnaExtract))
	     newType = "Long RNA-seq";
	 else if (startsWith("short", rnaExtract))
	     newType = "Short RNA-seq";
	 }
    return newType;
    }
else if (sameString(oldType, "DnaseSeq")) return "DNase-seq";
else return oldType;
}

char *findUcscControl(struct sqlConnection *conn, struct encodeExp *exp)
/* Try and find best control for experiment, returning it's accession, or NULL if can't find. */
{
return NULL;    // Going to figure out how to do it from Stanford first....
}

void rsyncUcscExp(FILE *f)
/* Grab encodeExp table from hgFixed and save it to tab separated file in edwExperiment format. */
{
struct sqlConnection *conn = sqlConnectRemote("genome-mysql.cse.ucsc.edu", 
		"genome", NULL, "hgFixed");
struct encodeExp *exp, *expList = encodeExpLoadByQuery(conn, "select * from encodeExp");
for (exp = expList; exp != NULL; exp = exp->next)
    {
    if (isEmpty(exp->accession))
        continue;
    struct slPair *varList = slPairListFromString(exp->expVars,FALSE);
    fprintf(f, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", exp->accession, fromUcscDataType(exp, varList),
	exp->lab, exp->cellType, "ENCODE2", exp->dataType, 
	emptyForNull(slPairFindVal(varList, "antibody")), emptyForNull(findUcscControl(conn,exp)));
    slPairFreeValsAndList(&varList);
    }
}

void rsyncEdwExpDataType(char *url, char *userId, char *password, char *outTab)
/* rsyncEdwExpDataType - Get experiment and data types from ENCODED via json.. */
{
FILE *f = mustOpen(outTab, "w");
if (!noStanford)
    {
    rsyncStanfordExp(url, userId, password, f);
    }
if (!noUcsc)
    {
    rsyncUcscExp(f);
    }
carefulClose(&f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
cacheName = optionVal("cache", cacheName);
noStanford = optionExists("noStanford");
noUcsc = optionExists("noUcsc");
rsyncEdwExpDataType(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
