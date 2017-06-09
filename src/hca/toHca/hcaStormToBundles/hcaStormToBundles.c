/* hcaStormToBundles - Convert a HCA formatted tagStorm to a directory full of bundles.. */
#include <uuid/uuid.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagStorm.h"
#include "jsonParse.h"
#include "portable.h"
#include "csv.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hcaStormToBundles - Convert a HCA formatted tagStorm to a directory full of bundles.\n"
  "usage:\n"
  "   hcaStormToBundles in.tags /path/to/data/files outDir\n"
  "options:\n"
  "   -xxx=XXX\n"
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

char *makeUuidString(char buf[37])
/* Generate a random uuid and put it in the usual hex-plus-dashes form */
{
/* Generate 16 bytes of random sequence with uuid generator */
unsigned char uuid[16];
uuid_generate(uuid);
uuid_unparse_lower(uuid, buf);
return buf;
}

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

void writeFilesArray(FILE *f, struct tagStanza *stanza, char *csvList)
/* Write out an array of file objects base on file names in csvList */
{
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
    char *format = NULL;
    boolean isRead = FALSE;
    if (endsWith(name, ".fastq.gz") || endsWith(name, ".fq.gz"))
	 {
         format = ".fastq.gz";
	 isRead = TRUE;
	 }
    else if (endsWith(name, ".bam"))
         format = ".bam";
    else if (endsWith(name, ".cram"))
         format = ".cram";
    if (format != NULL)
        {
	fprintf(f, ",\"%s\":", "format");
	writeJsonVal(f, format);
	}

    /* If it's a read format try to classify it to a type */
    if (isRead)
        {
	char *pairedEnds = tagMustFindVal(stanza, "assay.seq.paired_ends");
	char *type = NULL;
	if (sameString(pairedEnds, "no"))
	    type = "reads";
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
	}
    fputc('}', f);
    }
fputc(']', f);
slFreeList(&list);
}

void rWriteJson(FILE *f, struct tagStanza *stanza, struct subObj *obj)
/* Write out json object recursively */
{
char uuidBuf[37];
char *objName = obj->name;
fprintf(f, "{");
if (sameString(objName, "sample"))
     {
     fprintf(f, "\"uuid\":\"%s\",",  makeUuidString(uuidBuf));
     }
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

void makeBundleJson(char *dir, struct tagStanza *stanza, struct subObj *topLevelList)
/* Write out bundle json file for stanza into dir */
{
char jsonFileName[PATH_LEN];
struct subObj *topEl;
for (topEl = topLevelList; topEl != NULL; topEl = topEl->next)
    {
    safef(jsonFileName, sizeof(jsonFileName), "%s/%s.json", dir, topEl->name);
    writeTopJson(jsonFileName, stanza, topEl);
    }
}

void hcaStormToBundles(char *inTags, char *dataFileDir, char *outDir)
/* hcaStormToBundles - Convert a HCA formatted tagStorm to a directory full of bundles.. */
{
/* Check that have full path name for dataFileDir */
if (dataFileDir[0] != '/')
    errAbort("data file directory must be an absolute path starting with /");

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
    verbose(2, "topEl %s\n", topEl->name);
    struct subObj *obj = makeSubObj(allFields, topEl->name, topEl->name);
    slAddHead(&objList, obj);
    }
verbose(1, "Made %d top level objects\n", slCount(objList));

/* Figure out all files in dataFileDir and make a hash keyed by name
 * with full path as values */
struct slName *dataFileList = pathsInDirAndSubdirs(dataFileDir, "*");
verbose(1, "Got %d files in %s\n", slCount(dataFileList), dataFileDir);
struct slName *file;
struct hash *fileHash = hashNew(0);
struct dyString *csvScratch = dyStringNew(0);
for (file = dataFileList; file != NULL; file = file->next)
    {
    char name[FILENAME_LEN], extension[FILEEXT_LEN];
    char fileName[PATH_LEN];
    splitPath(file->name, NULL, name, extension);
    safef(fileName, sizeof(fileName), "%s%s", name, extension);
    hashAdd(fileHash, fileName, file->name);
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
    char *fileName, *csvPos = fileCsv;
    while ((fileName = csvParseNext(&csvPos, csvScratch)) != NULL)
        {
	/* Figure out destination path for symbolic link */
	char destPath[PATH_LEN];
	safef(destPath, sizeof(destPath), "%s/%s", bundleDir, fileName);

	/* Figure out source path using fileHash.  We'll check that the
	 * file name is unique within the hash to avoid ambiguity. */
        struct hashEl *hel = hashLookup(fileHash, fileName);
	if (hel == NULL)
	    errAbort("%s is not found in %s", fileName, dataFileDir);
	if (hashLookupNext(hel))
	    errAbort("%s occurs in multiple subdirectories of %s, can't figure out which to use",
		fileName, dataFileDir);
	char *sourcePath = hel->val;

	/* And make the link */
	makeSymLink(sourcePath, destPath);
	}

    makeBundleJson(bundleDir, stanza, objList);
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
