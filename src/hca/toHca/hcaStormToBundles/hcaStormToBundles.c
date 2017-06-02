/* hcaStormToBundles - Convert a HCA formatted tagStorm to a directory full of bundles.. */
#include <uuid/uuid.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagStorm.h"
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

struct tagStanzaRef
/* A reference to a stanza in a tag storm */
    {
    struct tagStanzaRef *next;	/* Next in list */
    struct tagStanza *stanza;   /* The stanza */
    };

static void rListLeaves(struct tagStanza *list, struct tagStanzaRef **pList)
/* Recursively add leaf stanzas to *pList */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    if (stanza->children)
        rListLeaves(stanza->children, pList);
    else
        {
	struct tagStanzaRef *ref;
	AllocVar(ref);
	ref->stanza = stanza;
	slAddHead(pList, ref);
	}
    }
}

void rPathsInDirAndSubdirs(char *dir, char *wildcard, struct slName **pList)
/* Recursively add directory contents that match wildcard (* for all) to list */
{
struct fileInfo *fi, *fiList = listDirX(dir, wildcard, TRUE);
for (fi = fiList; fi != NULL; fi = fi->next)
   {
   if (fi->isDir)
       rPathsInDirAndSubdirs(fi->name, wildcard, pList);
   else
       slNameAddHead(pList, fi->name);
   }
slFreeList(&fiList);
}

struct slName *pathsInDirAndSubdirs(char *dir, char *wildcard)
/* Return list of all non-directory files in dir and it's
 * subdirs.  Returns full path to files. */
{
struct slName *list = NULL;
rPathsInDirAndSubdirs(dir, wildcard, &list);
slReverse(&list);
return list;
}


struct tagStanzaRef *tagStormListLeaves(struct tagStorm *tagStorm)
/* Return list of references to all stanzas in tagStorm.  Free this
 * result with slFreeList. */
{
struct tagStanzaRef *list = NULL;
rListLeaves(tagStorm->forest, &list);
slReverse(&list);
return list;
}

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
fputc(quote, f);
while ((c = *s++) != 0)
    {
    if (c == quote || c == esc)
        fputc(esc, f);
    fputc(c, f);
    }
fputc(quote, f);
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
    if (!firstOut)
        fputc(',', f);
    char *fieldName = field->name;
    if (field->children != NULL)
	 {
	 fprintf(f, "\"%s\":", fieldName);
	 rWriteJson(f, stanza, field);
	 firstOut = FALSE;
	 }
    else
	{
	char *val = tagFindVal(stanza, field->fullName);
	if (val != NULL)
	    {
	    fprintf(f, "\"%s\":", fieldName);
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
	    firstOut = FALSE;
	    }
	}
    }
fprintf(f, "}");
}

void writeTopJson(char *fileName, struct tagStanza *stanza, struct subObj *top)
/* Write one json file using the parts of stanza referenced in subObj */
{
uglyf("Writing %s\n", fileName);
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
    uglyf("topEl %s\n", topEl->name);
    struct subObj *obj = makeSubObj(allFields, topEl->name, topEl->name);
    slAddHead(&objList, obj);
    }
verbose(1, "Made %d objects\n", slCount(objList));

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
