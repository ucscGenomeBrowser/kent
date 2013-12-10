/* rsyncEdwExpDataType - Get experiment and data types from ENCODED via json.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jsHelper.h"
#include "htmlPage.h"
#include "obscure.h"

/* Variables settable from command line */
char *jsonIn = NULL;
char *jsonOut = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "rsyncEdwExpDataType - Get experiment and data types from ENCODED via json.\n"
  "usage:\n"
  "   rsyncEdwExpDataType url userId password out.tab\n"
  "where URL is 'submit.encodedcc.org/experiments/?format=json' with quotes most likely\n"
  "and the userId and password are programatically generated things obtained from Laurence Rowe\n"
  "at Stanford most likely.\n"
  "Options:\n"
  "   -jsonIn=fileName - get JSON list from file rather than from database\n"
  "   -jsonOut=fileName - save unparsed JSON output to filename\n"
  );
}


/* Command line validation table. */
static struct optionSpec options[] = {
   {"jsonIn", OPTION_STRING},
   {"jsonOut", OPTION_STRING},
   {NULL, 0},
};

char *getJsonViaHttps(char *url, char *userId, char *password)
/* rsyncEdwExpDataType - Get experiment and data types from ENCODED via json.. */
{
char fullUrl[1024];
safef(fullUrl, sizeof(fullUrl), "https://%s:%s@%s", userId, password, url);
struct htmlPage *page = htmlPageGet(fullUrl);
struct htmlStatus *st = page->status;
if (st->status != 200)
    errAbort("Failed to get page from %s - status code: %d ", url, st->status);
char  *json=cloneString(page->htmlText);
htmlPageFree(&page);
return json;
}

char *fromAgreeingLibs(char *expName, struct slRef *replicaList, char *field)
/* Return given string valued field from replica.library assuming all replicas agree. */
{
char *val = NULL;
struct slRef *repRef;
for (repRef = replicaList; repRef != NULL; repRef = repRef->next)
    {
    struct jsonElement *rep = repRef->val;
    struct jsonElement *lib = jsonFindNamedField(rep, "", "library");
    if (lib != NULL)
        {
	struct jsonElement *libField = jsonFindNamedField(lib, "library", field);
	if (libField != NULL)
	    {
	    char *newVal = jsonStringVal(libField, field);
	    if (val == NULL)
	        val = newVal;
	    else if (!sameString(val, newVal))
	        errAbort("Libraries of replicas of %s disagree on %s: %s vs %s",
		    expName, field, val, newVal);
	    }
	}
    }
return val;
}

char *rnaSubtype(char *expAccession, char *userId, char *password)
/* Figure out subtype of RNA for experiment.  We do this looking at fields in replicates[].library */
{
char url[512];
safef(url, sizeof(url), "submit.encodedcc.org/experiments/%s/?format=json", expAccession);
char *jsonText = getJsonViaHttps(url, userId, password);
struct jsonElement *exp = jsonParse(jsonText);
freez(&jsonText);

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

void rsyncEdwExpDataType(char *url, char *userId, char *password, char *outTab)
/* rsyncEdwExpDataType - Get experiment and data types from ENCODED via json.. */
{
char *jsonText = NULL;
if (jsonIn)
    readInGulp(jsonIn, &jsonText, NULL);
else
    jsonText = getJsonViaHttps(url, userId, password);
if (jsonOut)
    writeGulp(jsonOut, jsonText, strlen(jsonText));
struct jsonElement *jsonRoot = jsonParse(jsonText);
char *expListName = "@graph";
struct jsonElement *jsonExpList = jsonMustFindNamedField(jsonRoot, "", expListName);
verbose(1, "Got @graph %p\n", jsonExpList);
struct slRef *ref, *refList = jsonListVal(jsonExpList, expListName);
verbose(1, "Got %d experiments\n", slCount(refList));
FILE *f = mustOpen(outTab, "w");
int realExpCount = 0;
for (ref = refList; ref != NULL; ref = ref->next)
    {
    struct jsonElement *el = ref->val;
    char *acc = jsonStringField(el, "accession");
    char *dataType = jsonStringField(el, "assay_term_name");
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
	    fprintf(f, "%s\t%s\t%s\t%s\t%s\n", acc, dataType,
		jsonOptionalStringField(el, "lab.title", ""),
		jsonOptionalStringField(el, "biosample_term_name", ""),
		rfa);
	    ++realExpCount;
	    }
	}
    }
verbose(1, "Got %d experiments with dataType\n", realExpCount);
carefulClose(&f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
jsonIn = optionVal("jsonIn", jsonIn);
jsonOut = optionVal("jsonOut", jsonOut);
rsyncEdwExpDataType(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
