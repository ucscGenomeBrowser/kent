/* hcatParseParth - Parse through Parth Shah's Project Tracker json and turn it into fodder for sqlUpdateRelated.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "dystring.h"
#include "obscure.h"
#include "portable.h"
#include "jsonParse.h"
#include "csv.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hcatParseParth - Parse through Parth Shah's Project Tracker json and turn it into fodder for \n"
  "sqlUpdateRelated - that is a directory full of tab-separated files that'll update the SQL\n"
  "tables in a relationally correct way.\n"
  "usage:\n"
  "   hcatParseParth inFile.json outDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

char *lookupSpecies(char *sciName)
/* Some day we may query a decent database, for now
 * just have some of the most common */
{
if (sameWord(sciName, "Homo sapiens")) return "human";
if (sameWord(sciName, "Mus musculus")) return "mouse";
if (sameWord(sciName, "Rattus norvegicus")) return "rat";
if (sameWord(sciName, "Danio rerio")) return "zebrafish";
if (sameWord(sciName, "Drosophila melanogaster")) return "fly";
if (sameWord(sciName, "Caenorhabditis elegans")) return "worm";
if (sameWord(sciName, "Saccharomyces cerevisiae")) return "yeast";
errAbort("Unknown species %s in lookup species", sciName);
return NULL;
}

char *sciNameRefsToSpecies(struct slRef *refList, struct dyString *scratch)
/* Convert a comma separated list of taxons in inVal to a
 * comma separated species list, using scratch. */
{
struct dyString *result = dyStringNew(64);
struct slRef *ref;
for (ref = refList; ref != NULL; ref = ref->next)
    {
    char *sciName = jsonStringVal(ref->val, "species");
    csvEscapeAndAppend(result, lookupSpecies(sciName));
    }
return dyStringCannibalize(&result);
}

char *lookupAssayType(char *ingestSym)
/* Convert ingest's representation of library_construction to something we put
 * in assayTech. */
{
if (sameWord(ingestSym, "10x v2 sequencing")
    || sameWord(ingestSym, "10x 3' v2 sequencing")
    || sameWord(ingestSym, "10x 5' v2 sequencing"))	// this one Paris's typo most likely
	return "10X Chromium V2";
if (sameWord(ingestSym, "smart-seq2")
    || sameWord(ingestSym, "smart-seq")
    || sameWord(ingestSym, "full length single cell rna sequencing") // Quake lab set I seen before
    || sameWord(ingestSym, "smart-seq2 paired-end"))
	return "smart-seq2 PE";
if (sameWord(ingestSym, "cite-seq")) return "cite-seq";
if (sameWord(ingestSym, "indrop")) return "inDrop";
if (sameWord(ingestSym, "mars-seq")) return "mars-seq";
if (sameWord(ingestSym, "drop-seq")) return "drop-seq";
if (sameWord(ingestSym, "dronc-seq")) return "dronc-seq";
if (sameWord(ingestSym, "10x 3' v1 sequencing")) return "10X Chromium V1";
if (sameWord(ingestSym, "10x 3' v3 sequencing")) return "10X Chromium V3";
// if (sameWord(ingestSym, "xyz")) return "xyz";
errAbort("Unknown ingest-info.library_construction_method %s", ingestSym);
return NULL;
}

char *ingestConstructionRefsToAssayTech(struct slRef *refList, struct dyString *scratch)
/* Convert a comma separated list of taxons in inVal to a
 * comma separated species list, using scratch. */
{
struct dyString *result = dyStringNew(64);
struct slRef *ref;
for (ref = refList; ref != NULL; ref = ref->next)
    {
    char *assayType = jsonStringVal(ref->val, "library_construction_methods");
    csvEscapeAndAppend(result, lookupAssayType(assayType));
    }
return dyStringCannibalize(&result);
}

boolean isComplete(char *s)
/* Return TRUE if jel is a string with a value COMPLETE */
{
if (sameWord("COMPLETE", s))
    return TRUE;
else if (sameWord("INCOMPLETE", s))
    return FALSE;
else
    errAbort("Unexpected value %s in isComplete", s);
return FALSE;
}

char *despaceAfterComma(char *tripleName)
/* We return a possibly reduced version of input */
{
char *in = tripleName;
char *out = in;
char c;
while ((c = *in++) != 0)
    {
    if (c == ',' && *in == ' ')
       in += 1;
    *out++ = c;
    }
*out=0;
return tripleName;
}

char *nextContrib(char **pList)
/* pList points to something looking like First1,Mid1,Last1; First2,Mid2,Last2;
 * We return the next name triple and and advance list pointer past it.
 * The parsing is destructive. */
{
char *start = skipLeadingSpaces(*pList);
if (isEmpty(start))
    return NULL;

/* Find end and update pointer. */
char *end = strchr(start, ';');
if (end != NULL)
    *end++ = 0;
*pList = end;

/* Return trimmed version of whats between start and end */
start = trimSpaces(start);
despaceAfterComma(start);
return start;
}

void outputContributors(FILE *f, char *semiList, char *type, struct dyString *csvOut, 
    struct dyString *scratch)
/* Parse throughs semicolon separated list of contributors of given
 * type and output to tab sep file */
{
char *dupe = cloneString(semiList); // because of destructive parsing
char *p = dupe;
char *contrib;
while ((contrib = nextContrib(&p)) != NULL)
     {
     if (!sameWord("n/a", contrib))
	 {
	 fprintf(f, "%s\t%s\n", type, csvEscapeToDyString(scratch, contrib));
	 csvEscapeAndAppend(csvOut, contrib);
	 }
     }
freeMem(dupe);
}

void hcatParseParth(char *inFile, char *outDir)
/* hcatParseParth - Parse through Parth Shah's Project Tracker json and turn it into fodder 
 * for sqlUpdateRelated.. */
{
// These two are frankly unneeded speed optimizations for our purposes, but heck what
// do you expect from the C code base?  It was built to be fast more than easy.
struct lm *lm = lmInit(0);
struct dyString *scratch = dyStringNew(0);
struct dyString *contribCsv = dyStringNew(0);

// Here's stuff just to read in the input file in a big string and then parsed
// out into jsonElements
char *inAsString = NULL;
size_t inStringSize = 0;
readInGulp(inFile, &inAsString, &inStringSize);
verbose(1, "Read %lld bytes from %s\n", (long long)inStringSize, inFile);
struct jsonElement *rootEl = jsonParseLm(inAsString, lm);

/* Check it is the type we expect - a list, and set up a bunch of stuff to help deduplicate
 * things. */
if (rootEl->type != jsonList)
    errAbort("Expecting first object in %s to be a [] but it's not", inFile);
struct hash *uniqHash = hashNew(0), *uniqTitleHash = hashNew(0), *uniqShortNameHash = hashNew(0);

/* Make output directories and files. */
makeDirsOnPath(outDir);
char contribPath[PATH_LEN];
safef(contribPath, sizeof(contribPath), "%s/hcat_contributor.tsv",  outDir);
FILE *fContrib = mustOpen(contribPath, "w");
char projectPath[PATH_LEN];
safef(projectPath, sizeof(projectPath), "%s/hcat_project.tsv",  outDir);
FILE *fProject = mustOpen(projectPath, "w");

/* Write out file headers */
fprintf(fContrib, "#@type_id@hcat_contributortype@short_name@id	?name\n");
fprintf(fProject, "#uuid\t?short_name\ttitle\t@@species@id@hcat_project_species@project_id@species_id@hcat_species@common_name@id\t@@assay_tech@id@hcat_project_assay_tech@project_id@assaytech_id@hcat_assaytech@short_name@id\t@@contributors@id@hcat_project_contributors@project_id@contributor_id@hcat_contributor@name@id\n");

/* Main loop - once through for each project (or in some cases project fragment */
struct slRef *projectRef;
for (projectRef = rootEl->val.jeList; projectRef != NULL; projectRef = projectRef->next)
    {
    /* Each project is an object/hash/dictionary depending on your fave language.
     * Here we get that level object into a C hash, and extract the project_uuid */
    struct hash *projectHash = jsonObjectVal(projectRef->val, "project");
    struct jsonElement *uuidEl = hashMustFindVal(projectHash, "project_uuid");
    char *projectUuid = uuidEl->val.jeString;

    /* Get the ingest-info subobject and make sure it's complete. */
    struct jsonElement *ingestEl = hashFindVal(projectHash, "ingest-info");
    if (ingestEl == NULL)
        errAbort("Can't find ingest-info for project_uuid %s", projectUuid);
    char *primaryState = jsonStringField(ingestEl, "primary_state");
    if (!isComplete(primaryState))
         continue;

    /* God help us even among the completes there are multiple projects associated
     * with the same thing.  So far project_short_name is unique.  We'll just take
     * the first (complete) one and warn about the rest.  Some of the dupes have the
     * same uuid, some different.  Yes, it's a little messy this input . */
    char *shortName = jsonStringField(ingestEl, "project_short_name");
    if (hashLookup(uniqShortNameHash, shortName))
        {
	verbose(2, "Skipping duplicate project named '%s'\n", shortName);
	continue;
	}
    hashAdd(uniqShortNameHash, shortName, NULL);

    /* grab more string fields we like from ingest-info. */
    char *title = jsonStringField(ingestEl, "project_title");
    char *wrangler = jsonStringField(ingestEl, "data_curator");
    char *contributors = jsonStringField(ingestEl, "primary_investigator");

    /* Get species list, maybe.... */
    struct jsonElement *speciesEl = jsonMustFindNamedField(ingestEl, "ingest-info", "species");
    struct slRef *speciesRefList = jsonListVal(speciesEl, "species");
    char *species = sciNameRefsToSpecies(speciesRefList, scratch);

    /* Get assay techs maybe */
    struct jsonElement *constructEl = jsonMustFindNamedField(ingestEl, "ingest-info", 
						"library_construction_methods");
    struct slRef *constructList = jsonListVal(constructEl, "library_construction_methods");
    char *techs = ingestConstructionRefsToAssayTech(constructList, scratch);

    /* Still more error checking */
    hashAddUnique(uniqHash, projectUuid, NULL);
    hashAddUnique(uniqTitleHash, title, NULL);

    /* Update contributors table */
    dyStringClear(contribCsv);
    outputContributors(fContrib, contributors, "contributor", contribCsv, scratch);
    outputContributors(fContrib, wrangler, "wrangler", contribCsv, scratch);

    fprintf(fProject, "%s\t%s\t%s\t", projectUuid, shortName, title);
    fprintf(fProject, "%s\t%s\t%s\n", species, techs, contribCsv->string);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
hcatParseParth(argv[1], argv[2]);
return 0;
}
