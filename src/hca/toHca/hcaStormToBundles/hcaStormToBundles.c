/* hcaStormToBundles - Convert a HCA formatted tagStorm to a directory full of bundles.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagStorm.h"
#include "jsonParse.h"
#include "portable.h"
#include "csv.h"

boolean gUrls;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hcaStormToBundles - Convert a HCA formatted tagStorm to a directory full of bundles.\n"
  "usage:\n"
  "   hcaStormToBundles in.tags http://path/to/data/files outDir\n"
  "If the /path/to/data/files is 'urls' then it will just leave the assay.files[]\n"
  "array as is.\n"
  "options:\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};


struct subObj
/* A subobject - may have children or not */
    {
    struct subObj *next;      // Points to next at same level
    struct subObj *children;  // Points to lower level, may be NULL.
    char *name;		      // Field name 
    char *fullName;	      // Field name with parent fields too.
    };

struct slName *uniqToDotList(struct slName *fieldList, char *prefix, int prefixLen)
/* Return list of all unique prefixes in field, that is parts up to first dot */
/* Return list of all words in fields that start with optional prefix, up to next dot */
{
struct slName *subList = NULL, *fieldEl;
for (fieldEl = fieldList; fieldEl != NULL; fieldEl = fieldEl->next)
     {
     char *field = fieldEl->name;
     if (prefixLen > 0)
         {
	 if (!startsWith(prefix, field))
	     continue;
	 field += prefixLen;
	 }
     char *dotPos = strchr(field, '.');
     if (dotPos == NULL)
	{
	if (prefixLen == 0)
	    errAbort("Field %s has no '.'", field);
	else
	    dotPos = field + strlen(field);
	}
     int prefixSize = dotPos - field;
     char prefix[prefixSize+1];
     memcpy(prefix, field, prefixSize);
     prefix[prefixSize] = 0;
     slNameStore(&subList, prefix);
     }
return subList;
}

struct subObj *makeSubObj(struct slName *fieldList, char *objName, char *prefix)
/* Make a subObj */
{
struct subObj *obj;
AllocVar(obj);
obj->name = cloneString(objName);
obj->fullName = cloneString(prefix);
verbose(3, "Making subObj %s %s\n", obj->name, obj->fullName);

/* Make a string that is prefix plus a dot */
char prefixDot[512];
safef(prefixDot, sizeof(prefixDot), "%s.", prefix);
int prefixDotLen = strlen(prefixDot);

struct slName *subList = uniqToDotList(fieldList, prefixDot, prefixDotLen);
if (subList != NULL)
     {
     struct slName *subName;
     for (subName = subList; subName != NULL; subName = subName->next)
         {
	 char newPrefix[512];
	 char *name = subName->name;
	 safef(newPrefix, sizeof(newPrefix), "%s%s", prefixDot, name);
	 struct subObj *subObj = makeSubObj(fieldList, name, newPrefix); 
	 slAddHead(&obj->children, subObj);
	 }
     slFreeList(&subList);
     }

return obj;
}

void writeJsonVal(FILE *f, char *s)
/* Write out s surrounded by quotes, and doing escaping of any internal quotes or \ */
{
char quote = '"', esc = '\\', c;
fputc('"', f);
while ((c = *s++) != 0)
    {
    if (c == quote || c == esc)
        fputc(esc, f);
    fputc(c, f);
    }
fputc('"', f);
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

void writeProtocolsArray(FILE *f, struct tagStanza *stanza, char *protocolsCsv)
/* Write up an array of protocols based on protocols and protocol_types tags in
 * stanza. */
{
char *typesCsv = tagFindVal(stanza, "sample.protocol_types");
if (typesCsv == NULL)
    errAbort("sample.protocol defined but not sample.protocol_types");

struct slName *protocolList = csvParse(protocolsCsv), *proto;
struct slName *typeList = csvParse(typesCsv), *type;
if (slCount(protocolList) != slCount(typeList))
    errAbort("Diffent sized protocols and protocol_types lists.");

boolean firstProtocol = TRUE;
fputc('[', f);
for (proto = protocolList, type = typeList; proto != NULL; proto = proto->next, type = type->next)
    {
    if (firstProtocol)
        firstProtocol = FALSE;
    else 
        fputc(',', f);
    fputc('{', f);
    boolean first = TRUE;
    writeJsonTag(f, "description", &first);
    writeJsonVal(f, proto->name);
    writeJsonTag(f, "type", &first);
    writeJsonVal(f, type->name);
    fputc('}', f);
    }
fputc(']', f);
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

void writeFilesArray(FILE *f, struct tagStanza *stanza, char *csvList)
/* Write out an array of file objects base on file names in csvList */
{
int laneIx = 1;
int filesPerLane = 1;
int curFileInLane = 0;
char *pairedEnds = tagMustFindVal(stanza, "assay.seq.paired_ends");
if (!sameString(pairedEnds, "no"))
    filesPerLane = 2;

boolean firstOut = TRUE;
struct slName *list = csvParse(csvList), *file;
fputc('[', f);
for (file = list; file != NULL; file = file->next)
    {
    /* Write comma between file objects */
    if (firstOut)
	firstOut = FALSE;
    else
	fputc(',', f);

    /* Write file object starting with file name */
    fputc('{', f);
    char *name = file->name;
    fprintf(f, "\"%s\":", "name");
    writeJsonVal(f, name);

    /* If can recognize format from suffix write format. */
    char *format = guessFormatFromName(name);
    if (format != NULL)
        {
	fprintf(f, ",\"%s\":", "format");
	writeJsonVal(f, format);
	}

    /* If it's a read format try to classify it to a type */
    boolean isRead = sameString(format, ".fastq.gz");
    if (isRead)
        {
	/* Calculate and write type */
	char *type = NULL;
	if (sameString(pairedEnds, "no"))
	    {
	    type = "reads";
	    }
	else if (sameString(pairedEnds, "yes"))
	    {
	    int end = endFromFileName(name);
	    if (end == 1)
	        type = "read1";
	    else
	        type = "read2";
	    }
	else if (sameString(pairedEnds, "index1_reads2"))
	    {
	    int end = endFromFileName(name);
	    if (end == 1)
		type = "index";
	    else
	        type = "reads";
	    }
	else
	    errAbort("Unrecognized paired_ends %s", pairedEnds);
	fprintf(f, ",\"%s\":", "type");
	writeJsonVal(f, type);

	/* Write out lane info */
	int lane = laneFromFileName(name);
	if (lane == 0)
	    lane = laneIx;
	fprintf(f, ",\"%s\": \"%d\"", "lane", lane);

	/* Update laneIx */
	if (++curFileInLane >= filesPerLane)
	    {
	    ++laneIx;
	    curFileInLane = 0;
	    }
	}
    fputc('}', f);
    }
fputc(']', f);
slFreeList(&list);
}

void rWriteJson(FILE *f, struct tagStanza *stanza, struct subObj *obj)
/* Write out json object recursively */
{
fprintf(f, "{");
struct subObj *field;
boolean firstOut = TRUE;
for (field = obj->children; field != NULL; field = field->next)
    {
    char *fieldName = field->name;
    if (field->children != NULL)
	 {
	 writeJsonTag(f, fieldName, &firstOut);
	 rWriteJson(f, stanza, field);
	 }
    else if (sameString("protocol_types", fieldName))
        {
		// do nothing, this was handles by protocols 
	}
    else
	{
	char *val = tagFindVal(stanza, field->fullName);
	if (val != NULL)
	    {
	    writeJsonTag(f, fieldName, &firstOut);
	    if (sameString(fieldName, "files"))
	        {
		writeFilesArray(f, stanza, val);
		}
	    else if (sameString(fieldName, "protocols"))
	        {
		writeProtocolsArray(f, stanza, val);
		}
	    else
		{
		if (csvNeedsParsing(val))
		    {
		    struct slName *list = csvParse(val);
		    if (list != NULL && list->next == NULL)
			writeJsonVal(f, list->name);
		    else
			{
			fputc('[', f);
			struct slName *el;
			for (el = list; el != NULL; el = el->next)
			    {
			    writeJsonVal(f, el->name);
			    if (el->next != NULL)
				fputc(',', f);
			    }
			fputc(']', f);
			}
		    slFreeList(&list);
		    }
		else
		    writeJsonVal(f, val);
		}
	    }
	}
    }
fprintf(f, "}");
}

void writeTopJson(char *fileName, struct tagStanza *stanza, struct subObj *top)
/* Write one json file using the parts of stanza referenced in subObj */
{
verbose(2, "Writing %s\n", fileName);
FILE *f = mustOpen(fileName, "w");
rWriteJson(f, stanza, top);
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
writeJsonVal(f, dataDir);
writeJsonTag(f, "version", &firstOut);
writeJsonVal(f, "1");

/* Write out files array */
writeJsonTag(f, "files", &firstOut);
fputc('[', f);
boolean firstFile = TRUE;
struct slName *file, *list = tagMustFindValList(stanza, "assay.files");
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
	writeJsonVal(f, path);
	}
    else
	writeJsonVal(f, file->name);
    char *format = guessFormatFromName(file->name);
    if (format != NULL)
        {
	writeJsonTag(f, "format", &firstField);
	writeJsonVal(f, format);
	}
    fputc('}', f);
    }
slFreeList(&list);
fputc(']', f);


/* Close up and go home */
fprintf(f, "}");
carefulClose(&f);
}

void makeBundleJson(char *dir, struct tagStanza *stanza, struct subObj *topLevelList,
    char *dataDir)
/* Write out bundle json file for stanza into dir */
{
verbose(2, "makeBundleJson on %s\n", dir);
char jsonFileName[PATH_LEN];
struct subObj *topEl;
for (topEl = topLevelList; topEl != NULL; topEl = topEl->next)
    {
    safef(jsonFileName, sizeof(jsonFileName), "%s/%s.json", dir, topEl->name);
    writeTopJson(jsonFileName, stanza, topEl);
    }
safef(jsonFileName, sizeof(jsonFileName), "%s/manifest.json", dir);
writeManifest(jsonFileName, stanza, dataDir);
}

void hcaStormToBundles(char *inTags, char *dataUrl, char *outDir)
/* hcaStormToBundles - Convert a HCA formatted tagStorm to a directory full of bundles.. */
{
/* Check that have full path name for dataFileDir */
if (sameString("urls", dataUrl))
   gUrls = TRUE;
else if (!stringIn("://", dataUrl))
    errAbort("data file directory must be a url.");

/* Load up tagStorm and get leaf list */
struct tagStorm *storm = tagStormFromFile(inTags);
struct tagStanzaRef *refList = tagStormListLeaves(storm);
verbose(1, "Got %d leaf nodes in %s\n", slCount(refList), inTags);

/* Do some figuring based on all fields available of what objects to make */
struct slName *allFields = tagStormFieldList(storm);
verbose(1, "Got %d fields in %s\n", slCount(allFields), inTags);
struct slName *topLevelList = uniqToDotList(allFields, NULL, 0);
verbose(1, "Got %d top level objects\n", slCount(topLevelList));

/* Make list of objects */
struct slName *topEl;
struct subObj *objList = NULL;
for (topEl = topLevelList; topEl != NULL; topEl = topEl->next)
    {
    verbose(1, "  %s\n", topEl->name);
    struct subObj *obj = makeSubObj(allFields, topEl->name, topEl->name);
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
    char *fileCsv = tagFindVal(stanza, "assay.files");
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
	struct slName *fileList = tagMustFindValList(stanza, "assay.files");
	splitPath(fileList->name, localUrl, NULL, NULL);
	dataUrl = localUrl;
	slFreeList(&fileList);
	}

    makeBundleJson(bundleDir, stanza, objList, dataUrl);
    }
verbose(1, "wrote json files into %s/bundle* dirs\n", outDir);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
hcaStormToBundles(argv[1], argv[2], argv[3]);
return 0;
}
