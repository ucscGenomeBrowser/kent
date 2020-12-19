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
if (sameWord(ingestSym, "cel-seq2")) return "cel-seq2";
if (sameWord(ingestSym, "sci-rna-seq")) return "sci-rna-seq";
if (sameWord(ingestSym, "seq-well")) return "seq-well";
if (sameWord(ingestSym, "indrop")) return "inDrop";
if (sameWord(ingestSym, "mars-seq")) return "mars-seq";
if (sameWord(ingestSym, "drop-seq")) return "drop-seq";
if (sameWord(ingestSym, "dronc-seq")) return "dronc-seq";
if (sameWord(ingestSym, "10x 3' v1 sequencing")) return "10X Chromium V1";
if (sameWord(ingestSym, "10x 3' v3 sequencing")) return "10X Chromium V3";
if (sameWord(ingestSym, "10x v3 sequencing")) return "10X Chromium V3";
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

struct jsonElement *bypassListOfOne(struct jsonElement *el, char *name)
/* Check that element is a list with just one member, and then return that one member */
{
if (el->type != jsonList)
    errAbort("Expected %s to be a list", name);
struct slRef *list = el->val.jeList;
int listSize = slCount(list);
if (listSize != 1)
    errAbort("Expected %s to be a list with just one member", name);
return list->val;
}

void outputTracker(FILE *f, char *projectName, char *submissionId, char *uuid,
    struct hash *projectHash, int subBunCount, struct dyString *scratch)
/* Look for interesting state info from parth's tracker outside of ingest and output it */
{
/* Grab the dss-info and lots of pieces of it. It sees both primary and analysis,
 * not sure about matrix */
struct jsonElement *dssEl = bypassListOfOne(hashMustFindVal(projectHash, "dss-info"), "dss-info");
struct jsonElement *awsPrimary = jsonMustFindNamedField(dssEl,
						"dss-info", "aws_primary_bundle_count");
int awsPrimaryCount = jsonDoubleVal(awsPrimary, "aws_primary_bundle_count");
struct jsonElement *gcpPrimary = jsonMustFindNamedField(dssEl,
						"dss-info", "gcp_primary_bundle_count");
int gcpPrimaryCount = jsonDoubleVal(gcpPrimary, "gcp_primary_bundle_count");
struct jsonElement *awsAnalysis = jsonMustFindNamedField(dssEl,
						"dss-info", "aws_analysis_bundle_count");
int awsAnalysisCount = jsonDoubleVal(awsAnalysis, "aws_analysis_bundle_count");
struct jsonElement *gcpAnalysis = jsonMustFindNamedField(dssEl,
						"dss-info", "gcp_analysis_bundle_count");
int gcpAnalysisCount = jsonDoubleVal(gcpAnalysis, "gcp_analysis_bundle_count");

/* Grab just a little bit from azul-info */
struct jsonElement *azulEl = bypassListOfOne(hashMustFindVal(projectHash, "azul-info"), 
						"azul-info");
struct jsonElement *azulBundle = jsonMustFindNamedField(azulEl,
						"azul-info", "analysis_bundle_count");
int azulBundleCount = jsonDoubleVal(azulBundle, "azul-info.analysis_bundle_count");

/* Grab a little bit from analysis-info */
struct jsonElement *analysisEl = bypassListOfOne(hashMustFindVal(projectHash, "analysis-info"),
						"analysis-info");
struct jsonElement *succeededEl = jsonFindNamedField(analysisEl, 
						"analysis-info", "succeeded_workflows");
int succeededWorkflows = 0;
if (succeededEl != NULL)
    succeededWorkflows = jsonDoubleVal(succeededEl, "succeeded_workflows");

/* And get the matrix stuff! */
struct jsonElement *matrixEl = bypassListOfOne(hashMustFindVal(projectHash, "matrix-info"),
						"matrix-info");
struct jsonElement *matBundle = jsonMustFindNamedField(matrixEl, 
						"matrix-info", "analysis_bundle_count");
int matrixBundleCount = jsonDoubleVal(matBundle, "matrix-info.analysis_bundle_count");
struct jsonElement *cellCountEl = jsonMustFindNamedField(matrixEl,
						"matrix-info", "cell_count");
int cellCount = jsonDoubleVal(cellCountEl, "cell_count");

fprintf(f, "%s\t%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", 
    projectName, uuid, submissionId, 
    subBunCount, awsPrimaryCount, gcpPrimaryCount, awsAnalysisCount, gcpAnalysisCount,
    azulBundleCount, succeededWorkflows, matrixBundleCount, cellCount);
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
char postPath[PATH_LEN];
safef(postPath, sizeof(postPath), "%s/hcat_tracker.tsv",  outDir);
FILE *fTracker = mustOpen(postPath, "w");

/* Write out file headers */
fprintf(fContrib, "#@type_id@hcat_contributortype@short_name@id	?name\n");
fprintf(fProject, "#?short_name\ttitle\t@@species@id@hcat_project_species@project_id@species_id@hcat_species@common_name@id\t@@assay_tech@id@hcat_project_assay_tech@project_id@assaytech_id@hcat_assaytech@short_name@id\t@@contributors@id@hcat_project_contributors@project_id@contributor_id@hcat_contributor@name@id\tsubmit_date\n");
fprintf(fTracker, "#@project_id@hcat_project@short_name@id\t?uuid\tsubmission_id\tsubmission_bundles_exported_count\taws_primary_bundle_count\tgcp_primary_bundle_count\taws_analysis_bundle_count\tgcp_analysis_bundle_count\tazul_analysis_bundle_count\tsucceeded_workflows\tmatrix_bundle_count\tmatrix_cell_count\n");

/* Main loop - once through for each project (or in some cases project fragment */
struct slRef *projectRef;
verbose(1, "Got %d high level objects\n", slCount(rootEl->val.jeList));
for (projectRef = rootEl->val.jeList; projectRef != NULL; projectRef = projectRef->next)
    {
    /* Each project is an object/hash/dictionary depending on your fave language.
     * Here we get that level object into a C hash, and extract the project_uuid */
    struct hash *projectHash = jsonObjectVal(projectRef->val, "project");
    struct jsonElement *uuidEl = hashMustFindVal(projectHash, "project_uuid");
    char *projectUuid = uuidEl->val.jeString;

    /* Get the ingest-info subobject and make sure it's complete. */
    struct jsonElement *ingestList = hashFindVal(projectHash, "ingest-info");
    if (ingestList == NULL)
        errAbort("Can't find ingest-info for project_uuid %s", projectUuid);
    if (ingestList->type != jsonList)
        errAbort("Expecting list value for ingest-info");
    int ingestListSize = slCount(ingestList->val.jeList);
    if (ingestListSize != 1)
        verbose(1, "ingest-info[] has %d members\n", ingestListSize);

    int subBunCount = 0;
    struct slRef *ingestRef;
    char *submissionId = NULL;
    char *shortName = NULL;
    boolean gotReal = FALSE;
    for (ingestRef = ingestList->val.jeList; ingestRef != NULL; ingestRef = ingestRef->next)
	{
	struct jsonElement *ingestEl = ingestRef->val;
	char *primaryState = jsonStringField(ingestEl, "primary_state");
	if (!isComplete(primaryState))
	     continue;

	/* God help us even among the completes there are multiple projects associated
	 * with the same thing.  So far project_short_name is unique.  We'll just take
	 * the first (complete) one and warn about the rest.  Some of the dupes have the
	 * same uuid, some different.  Yes, it's a little messy this input . */
	shortName = jsonStringField(ingestEl, "project_short_name");
	if (shortName == NULL)
	    {
	    verbose(1, "Skipping project without shortName '%s'\n", shortName);
	    continue;
	    }
	// Abbreviate what is really and truly not a short name!
	if (startsWith("Single cell RNAseq characterization of cell types produced over time in an in ",
	     shortName))
	    {
	    shortName = "Single cell RNAseq characterization of cell types produced over time";
	    verbose(2, "Abbreviated shortName to %s\n", shortName);
	    }
	if (hashLookup(uniqShortNameHash, shortName))
	    {
	    verbose(2, "Skipping duplicate project named '%s'\n", shortName);
	    continue;
	    }
	hashAdd(uniqShortNameHash, shortName, NULL);

	/* Grab more string fields we like from ingest-info. */
	submissionId = jsonStringField(ingestEl, "submission_id");
	if (submissionId == NULL)
	   {
	   warn("submissionId for %s is NULL", projectUuid);
	   continue;
	   }
	char *title = jsonStringField(ingestEl, "project_title");
	char *wrangler = jsonStringField(ingestEl, "data_curator");
	char *contributors = jsonStringField(ingestEl, "primary_investigator");
	char *submissionDateTime = jsonStringField(ingestEl, "submission_date");

	/* Turn dateTime into just date */
	char *tStart = strchr(submissionDateTime, 'T');
	if (tStart == NULL)
	    errAbort("No T separator in submission_date %s", submissionDateTime);
	char *submissionDate = cloneStringZ(submissionDateTime, tStart - submissionDateTime);

	/* Get species list, maybe.... */
	struct jsonElement *speciesEl = jsonMustFindNamedField(ingestEl, "ingest-info", "species");
	struct slRef *speciesRefList = jsonListVal(speciesEl, "species");
	char *species = sciNameRefsToSpecies(speciesRefList, scratch);
	struct jsonElement *subBunCountEl = jsonMustFindNamedField(ingestEl, 
					"ingest-info", "submission_bundles_exported_count");
	subBunCount = jsonDoubleVal(subBunCountEl, "submission_bundles_exported_count");

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

	/* Update project table */
	fprintf(fProject, "%s\t%s\t", shortName, title);
	fprintf(fProject, "%s\t%s\t%s\t", species, techs, contribCsv->string);
	fprintf(fProject, "%s\n", submissionDate);
	gotReal = TRUE;

	break;	    // Still figuring out if this loop is here to stay
	}

    /* We processed the heck out of the ingest-info, and this routine is so long,
     * pass along what we parsed out that goes into the tracker table, and have it
     * deal with the azul-info, matrix-info, etc,  which are read-only to wranglers. */
    if (gotReal)
	outputTracker(fTracker, shortName, submissionId, projectUuid, projectHash, 
	    subBunCount, scratch);
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
