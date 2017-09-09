/* hcaStormToBundles - Convert a HCA formatted tagStorm to a directory full of bundles.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagStorm.h"
#include "jsonParse.h"
#include "tagToJson.h"
#include "portable.h"
#include "csv.h"
#include "uuid.h"
#include "tagSchema.h"

boolean gUrls;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hcaStormToBundles - Convert a HCA formatted tagStorm to a directory full of bundles.\n"
  "usage:\n"
  "   hcaStormToBundles in.tags http://path/to/data/files schema outDir\n"
  "If the /path/to/data/files is 'urls' then it will just leave the assay.seq.files[]\n"
  "array as is.   The schema file is something in the format made by tagStormInfo -schema.\n"
  "options:\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};


void writeJsonVal(FILE *f, char *s, boolean isNum)
/* Write out s surrounded by quotes, and doing escaping of any internal quotes or \ */
{
if (isNum)
    fputs(s, f);
else
    {
    char quote = '"', esc = '\\', c;
    fputc(quote, f);
    while ((c = *s++) != 0)
	{
	if (c == quote || c == esc)
	    fputc(esc, f);
	fputc(c, f);
	}
    fputc(quote, f);
    }
}

void writeJsonTag(FILE *f, char *name, boolean *pFirstOut)
/* Write out quoted field name followed by colon.  If not first
 * time through then write comma */
{
boolean firstOut = *pFirstOut;
if (firstOut)
    *pFirstOut =  !firstOut;
else
    fputc(',', f);
fprintf(f, "\"%s\":", name);
}

int endFromFileName(char *fileName)
/* Try and figure out end from file name */
{
if (stringIn("_R1_", fileName))
    return 1;
else if (stringIn("_R2_", fileName))
    return 2;
else if (stringIn("_1.", fileName))
    return 1;
else if (stringIn("_2.", fileName))
    return 2;
errAbort("Couldn't deduce paired end from file name %s", fileName);
return 0;
}

int laneFromFileName(char *fileName)
/* Deduce lane from file name.  If there is something of form _L123_ in the midst of
 * file name we'll call it lane.  Otherwise return 0 (they start counting at 1) */
{
char *pat = "_L";
int patLen = strlen(pat);
char *s = fileName;
while ((s = stringIn(pat, s)) != NULL)
    {
    s += patLen;
    char *e = strchr(s, '_');
    if (e == NULL)
        break;
    int midSize = e - s;
    if (midSize == 3)
        {
	char midBuf[midSize+1];
	memcpy(midBuf, s, midSize);
	midBuf[midSize] = 0;
	if (isAllDigits(midBuf))
	    return atoi(midBuf);
	}
    }
return 0;
}

char *guessFormatFromName(char *name)
/* Guess format from file extension */
{
if (endsWith(name, ".fastq.gz") || endsWith(name, ".fq.gz"))
     return ".fastq.gz";
else if (endsWith(name, ".bam"))
     return ".bam";
else if (endsWith(name, ".cram"))
     return ".cram";
else
     return NULL;
}

struct laneFiles
/* The files associated with a lane. */
    {
    struct laneFiles *next;
    int laneIx;
    struct slName *fileList;
    };


int laneFilesCmp(const void *va, const void *vb)
/* Compare to sort based on laneIx (ascending). */
{
const struct laneFiles *a = *((struct laneFiles **)va);
const struct laneFiles *b = *((struct laneFiles **)vb);
return a->laneIx - b->laneIx;
}

struct laneFiles *laneFilesFind(struct laneFiles *list, int laneIx)
/* Return lane on list of given ix,  or NULL if no such lane. */
{
struct laneFiles *lane;
for (lane = list; lane != NULL; lane = lane->next)
    if (lane->laneIx == laneIx)
        return lane;
return NULL;
}
    
void writeLaneArray(FILE *f, struct tagStanza *stanza, char *csvList)
/* Write out an array of file objects base on file names in csvList */
{
struct slName *list = csvParse(csvList), *file;

/* Figure out number of files per lanes.  We'll take the lane number for the
 * file names if available, but if not we'll assume list is sorted and will
 * put the appropriate number of files in each lane. */
int laneCounter = 1;
int filesPerLane = 1;
int curFileInLane = 0;
char *pairedEnds = tagMustFindVal(stanza, "assay.seq.paired_ends");
if (!sameString(pairedEnds, "no"))
    filesPerLane = 2;

/* First pass, make a list of lanes */
struct laneFiles *laneList = NULL, *lane;
for (file = list; file != NULL; file = file->next)
    {
    /* Figure out laneIx, from file name if possible, otherwise by counting */
    char *fileName = file->name;
    int laneIx = laneFromFileName(fileName);
    if (laneIx == 0)
	laneIx = laneCounter;
    /* Update laneCounter */
    if (++curFileInLane >= filesPerLane)
	{
	++laneCounter;
	curFileInLane = 0;
	}

    /* Find lane in laneList, make new lane if it's not there. */
    lane = laneFilesFind(laneList, laneIx);
    if (lane == NULL)
        {
	AllocVar(lane);
	lane->laneIx = laneIx;
	slAddHead(&laneList, lane);
	}
    slNameAddTail(&lane->fileList, fileName);
    }
slReverse(&laneList);
slSort(&laneList, laneFilesCmp);

/* Now make a lane array and go through lane list */
boolean firstOut = TRUE;
fputc('[', f);
for (lane = laneList; lane != NULL; lane = lane->next)
    {
    /* Write comma between lane objects */
    if (firstOut)
	firstOut = FALSE;
    else
	fputc(',', f);

    /* Write lane object starting with lane index*/
    fputc('{', f);
    fprintf(f, "\"%s\": %d", "number", lane->laneIx);

    /* Rest of lane fields are based on files we contain */
    for (file = lane->fileList; file != NULL; file = file->next)
	{
	/* Calculate type */
	char *fileName = file->name;

	char *type = NULL;
	if (sameString(pairedEnds, "no"))
	    {
	    type = "reads";
	    }
	else if (sameString(pairedEnds, "yes"))
	    {
	    int end = endFromFileName(fileName);
	    if (end == 1)
		type = "read1";
	    else
		type = "read2";
	    }
	else if (sameString(pairedEnds, "index1_reads2"))
	    {
	    int end = endFromFileName(fileName);
	    if (end == 1)
		type = "index";
	    else
		type = "reads";
	    }
	else
	    errAbort("Unrecognized paired_ends %s", pairedEnds);

	fprintf(f, ",\"%s\":", type);
	writeJsonVal(f, fileName, FALSE);
	}
    fputc('}', f);
    }

fputc(']', f);
slFreeList(&list);
}

boolean allDigitNames(struct ttjSubObj *list)
/* Return TRUE if all names on list are composed of positive integers */
{
struct ttjSubObj *obj;
for (obj = list; obj != NULL; obj = obj->next)
    if (!isAllDigits(obj->name))
        return FALSE;
return TRUE;
}

boolean objNeedsCore(char *module)
/* Return TRUE if module of given name needs core defined */
{
static char *needCore[] = {"project", "sample", "assay", "cell_line", "contact",
    "death", "donor", "enrichment", "imaging", 
    "preservation", "protocol", "publication", "rna", "seq", "single_cell", 
    "well"};
return (stringArrayIx(module, needCore, ArraySize(needCore)) >= 0);
}

void printCore(FILE *f, char *type, boolean *pFirstOut)
/* Write up core module for object. */
{
writeJsonTag(f, "core", pFirstOut);
fprintf(f, "{");
boolean firstOut = TRUE;
writeJsonTag(f, "type", &firstOut);
writeJsonVal(f, type, FALSE);
writeJsonTag(f, "schema_version", &firstOut);
writeJsonVal(f, "3.0.0", FALSE);
writeJsonTag(f, "schema_url", &firstOut);
char schema_url[PATH_LEN];
safef(schema_url, sizeof(schema_url), "http://hgwdev.soe.ucsc.edu/~kent/hca/schema/%s.json",
    type);
writeJsonVal(f, schema_url, FALSE);
fprintf(f, "}");
}

void rWriteJson(FILE *f, struct tagStorm *storm, struct tagStanza *stanza, 
    struct ttjSubObj *obj, struct ttjSubObj *labeledObj, struct hash *schemaHash,
    struct dyString *scratch)
/* Write out json object recursively */
{
boolean isArray = allDigitNames(obj->children);
struct ttjSubObj *field; 
if (isArray)
    {
    fprintf(f, "["); 
    for (field = obj->children; field != NULL; field = field->next)
        {
	if (field != obj->children)
	   fprintf(f, ",");
	rWriteJson(f, storm, stanza, field, labeledObj, schemaHash, scratch);
	}
    fprintf(f, "]");
    }
else
    { 
    fprintf(f, "{"); 
    boolean firstOut = TRUE;

    /* Figure out if we need to attach a core object and do so.  The figuring bit is
     * frankly clunky. */
    char *objType = labeledObj->name;
    if (sameString(objType, "submitter") || sameString(objType, "contributors"))
         objType = "contact";
    else if (sameString(objType, "publications"))
         objType = "publication";
    else if (sameString(objType, "protocol"))  // protocol is actually just protocol_id
         objType = "string";
    else if (sameString(objType, "protocols")) // but protocols array is protocol
         objType = "protocol";
    if (objNeedsCore(objType))
        printCore(f, objType, &firstOut);


    for (field = obj->children; field != NULL; field = field->next)
	{
	char *fieldName = field->name;
	if (field->children != NULL)
	     {
	     writeJsonTag(f, fieldName, &firstOut);
	     rWriteJson(f, storm, stanza, field, field, schemaHash, scratch);
	     }
	else
	    {
	    char *val = tagFindVal(stanza, field->fullName);
	    if (val != NULL)
		{
		boolean isNum = FALSE;
		char *schemaName = tagSchemaFigureArrayName(field->fullName, scratch);
		struct tagSchema *schema = hashFindVal(schemaHash, schemaName);
		if (schema != NULL)
		   isNum = (schema->type == '#' || schema->type == '%');
		if (sameString(fieldName, "files"))
		    {
		    writeJsonTag(f, "lanes", &firstOut);
		    writeLaneArray(f, stanza, val);
		    }
		else
		    {
		    boolean isArray = FALSE;
		    writeJsonTag(f, fieldName, &firstOut);
		    if (schema != NULL)
			isArray = schema->isArray;
		    struct slName *list = csvParse(val);
		    if (isArray)
			fputc('[', f);
		    else
			{
			if (list->next != NULL)  // more than one element
			   errAbort("Multiple vals for scalar tag %s in stanza starting line %d of %s",
				field->fullName, stanza->startLineIx, storm->fileName);
			}
		    struct slName *el;
		    for (el = list; el != NULL; el = el->next)
			{
			writeJsonVal(f, el->name, isNum);
			if (el->next != NULL)
			    fputc(',', f);
			}
		    if (isArray)
			fputc(']', f);
		    slFreeList(&list);
		    }
		}
	    }
	}
    fprintf(f, "}");
    }
}

void writeTopJson(char *fileName, struct tagStorm *storm, struct tagStanza *stanza, 
    struct ttjSubObj *top, struct hash *schemaHash)
/* Write one json file using the parts of stanza referenced in ttjSubObj */
{
verbose(2, "Writing %s\n", fileName);
FILE *f = mustOpen(fileName, "w");
struct dyString *scratch = dyStringNew(0);
rWriteJson(f, storm, stanza, top, top, schemaHash, scratch);
fprintf(f, "\n");
carefulClose(&f);
}

struct slName *tagFindValList(struct tagStanza *stanza, char *tag)
/* Read in tag as a list. Do a slFreeList on this when done.
 * Returns NULL if no value */
{
char *val = tagFindVal(stanza, tag);
return csvParse(val);
}

struct slName *tagMustFindValList(struct tagStanza *stanza, char *tag)
/* Find tag or die trying, and return it as parsed out list */
{
char *val = tagMustFindVal(stanza, tag);
return csvParse(val);
}

void writeManifest(char *fileName, struct tagStanza *stanza, char *dataDir)
/* Write out manifest file */
{
verbose(2, "Writing manifest %s\n", fileName);
/* Start up a json file */
FILE *f = mustOpen(fileName, "w");
boolean firstOut = TRUE;
fprintf(f, "{");

/* Write dir and version tags */
writeJsonTag(f, "dir", &firstOut);
writeJsonVal(f, dataDir, FALSE);
writeJsonTag(f, "version", &firstOut);
writeJsonVal(f, "1", TRUE);

/* Write out files array */
writeJsonTag(f, "files", &firstOut);
fputc('[', f);
boolean firstFile = TRUE;
struct slName *file, *list = tagMustFindValList(stanza, "assay.seq.files");
for (file = list; file != NULL; file = file->next)
    {
    if (firstFile)
        firstFile = FALSE;
    else
        fputc(',', f);
    boolean firstField = TRUE;
    fputc('{', f);
    writeJsonTag(f, "name", &firstField);
    if (gUrls)
        {
	char fileName[FILENAME_LEN], ext[FILEEXT_LEN], path[PATH_LEN];
	splitPath(file->name, NULL, fileName, ext);
	safef(path, sizeof(path), "%s%s", fileName, ext);
	writeJsonVal(f, path, FALSE);
	}
    else
	writeJsonVal(f, file->name, FALSE);
    char *format = guessFormatFromName(file->name);
    if (format != NULL)
        {
	writeJsonTag(f, "format", &firstField);
	writeJsonVal(f, format, FALSE);
	}
    fputc('}', f);
    }
slFreeList(&list);
fputc(']', f);


/* Close up and go home */
fprintf(f, "}");
carefulClose(&f);
}

void makeBundleJson(struct tagStorm *storm, char *dir, struct tagStanza *stanza, 
    struct ttjSubObj *topLevelList, char *dataDir, struct hash *schemaHash)
/* Write out bundle json file for stanza into dir */
{
verbose(2, "makeBundleJson on %s\n", dir);
char jsonFileName[PATH_LEN];
struct ttjSubObj *topEl;
for (topEl = topLevelList; topEl != NULL; topEl = topEl->next)
    {
    safef(jsonFileName, sizeof(jsonFileName), "%s/%s.json", dir, topEl->name);
    writeTopJson(jsonFileName, storm, stanza, topEl, schemaHash);
    }
safef(jsonFileName, sizeof(jsonFileName), "%s/manifest.json", dir);
writeManifest(jsonFileName, stanza, dataDir);
}

void dupeValToNewTag(struct tagStorm *storm, struct tagStanza *stanzaList, 
    char *oldTag, char *newTag)
/* Everywhere see oldTag also make newTag with same value */
{
struct tagStanza *stanza;
for (stanza = stanzaList; stanza != NULL; stanza = stanza->next)
    {
    char *val = tagFindLocalVal(stanza, oldTag);
    if (val != NULL)
        tagStanzaAppend(storm, stanza, newTag, val);
    if (stanza->children)
        dupeValToNewTag(storm, stanza->children, oldTag, newTag);
    }
}

struct missingContext
/* Keep track of statis while adding missing stuff */
    {
    char *idTag;	// Tag we use as ID
    char *uuidTag;	// Tag we use as UUID
    boolean dupeOk;	// If true we allow dupes
    struct hash *idToUuid;  // Hash to convert id to UUID
    int stanzaCount;	// Number of stanzas examined
    int idFound;  // Number of times idTag found
    int uuidFound; // Number of times existing uuidTag found
    int uuidMade;   // Number of times UUID made
    };

void addToStanza(struct tagStorm *storm, struct tagStanza *stanza, void *context)
/* Check up on one stanza, updating stats and if need be making a uuid */
{
struct missingContext *mc = context;
mc->stanzaCount += 1;
char *id = tagFindLocalVal(stanza, mc->idTag);
if (id != NULL)
    mc->idFound += 1;
char *uuid = tagFindLocalVal(stanza, mc->uuidTag);
if (uuid != NULL)
    mc->uuidFound += 1;
if (id != NULL && uuid == NULL)
    {
    uuid = hashFindVal(mc->idToUuid, id);
    if (uuid == NULL)
        {
	uuid = needMem(UUID_STRING_SIZE);
	makeUuidString(uuid);
	mc->uuidMade += 1;
	hashAdd(mc->idToUuid, id, uuid);
	tagStanzaAppend(storm, stanza, mc->uuidTag, uuid);
	}
    else
	{
	if (!mc->dupeOk)
	   errAbort("The %s %s is duplicated in %s", mc->idTag, id, storm->fileName);
	}
    }
}

void addMissingUuids(struct tagStorm *storm, char *idTag, char *uuidTag, boolean dupeOk)
/* Add UUIDs to stanzas in storm that have given idTab but no uuidTag.  If dupe is ok
 * then it's ok to see the same value for the ID tag more than once. */
{
verbose(2, "addMissingUuids() adding %s where missing and there is a %s tag\n", uuidTag, idTag);
struct missingContext context;
ZeroVar(&context);
context.idTag = idTag;
context.uuidTag = uuidTag;
context.dupeOk = dupeOk;
context.idToUuid = hashNew(0);
tagStormTraverse(storm, storm->forest, &context, addToStanza);
verbose(2, "stanzaCount %d, idFound %d, uuidFound %d, uuidMade %d\n", 
    context.stanzaCount, context.idFound, context.uuidFound, context.uuidMade);
if (context.uuidMade > 0)
    verbose(1, "%s made %d times based on %s\n", uuidTag, context.uuidMade, idTag);
}

void hcaStormToBundles(char *inTags, char *dataUrl, char *schemaFile, char *outDir)
/* hcaStormToBundles - Convert a HCA formatted tagStorm to a directory full of bundles.. */
{
/* Check that have full path name for dataFileDir */
if (sameString("urls", dataUrl))
   gUrls = TRUE;
else if (!stringIn("://", dataUrl))
    errAbort("data file directory must be a url.");

/* Load up schema and put it in hash */
struct tagSchema *schemaList = tagSchemaFromFile(schemaFile);
struct hash *schemaHash = tagSchemaHash(schemaList);

/* Load up tagStorm get leaf list */
struct tagStorm *storm = tagStormFromFile(inTags);
struct tagStanzaRef *refList = tagStormListLeaves(storm);
verbose(1, "Got %d leaf nodes in %s\n", slCount(refList), inTags);

/* Add in assay.sample_id as just a dupe of sample.id */
dupeValToNewTag(storm, storm->forest, "sample.id", "assay.sample_id");
dupeValToNewTag(storm, storm->forest, "project.id", "sample.project_id");
addMissingUuids(storm, "assay.seq.ena_experiment", "assay.id", FALSE);
addMissingUuids(storm, "assay.seq.sra_experiment", "assay.id", FALSE);


/* Do some figuring based on all fields available of what objects to make */
struct slName *allFields = tagStormFieldList(storm);
verbose(1, "Got %d fields in %s\n", slCount(allFields), inTags);
struct slName *topLevelList = ttjUniqToDotList(allFields, NULL, 0);
verbose(1, "Got %d top level objects\n", slCount(topLevelList));

/* Make list of objects */
struct slName *topEl;
struct ttjSubObj *objList = NULL;
for (topEl = topLevelList; topEl != NULL; topEl = topEl->next)
    {
    verbose(1, "  %s\n", topEl->name);
    struct ttjSubObj *obj = ttjMakeSubObj(allFields, topEl->name, topEl->name);
    slAddHead(&objList, obj);
    }

/* Loop through stanzas making bundles */
struct tagStanzaRef *ref;
int bundleIx = 0;
makeDirsOnPath(outDir);
for (ref = refList; ref != NULL; ref = ref->next)
    {
    /* Fetch stanza and comma-separated list of files. */
    struct tagStanza *stanza = ref->stanza;
    char *fileCsv = tagFindVal(stanza, "assay.seq.files");
    if (fileCsv == NULL)
        errAbort("Stanza without a files tag. Stanza starts line %d of %s",  
		stanza->startLineIx, inTags); 

    /* Make subdirectory for bundle */
    ++bundleIx;
    char bundleDir[PATH_LEN];
    safef(bundleDir, sizeof(bundleDir), "%s/bundle%d", outDir, bundleIx);
    makeDir(bundleDir);

    /* Make symbolic link of all files */
    char localUrl[PATH_LEN*2];
    if (gUrls)
        {
	struct slName *fileList = tagMustFindValList(stanza, "assay.seq.files");
	splitPath(fileList->name, localUrl, NULL, NULL);
	dataUrl = localUrl;
	slFreeList(&fileList);
	}

    makeBundleJson(storm, bundleDir, stanza, objList, dataUrl, schemaHash);
    }
verbose(1, "wrote json files into %s/bundle* dirs\n", outDir);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
hcaStormToBundles(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
