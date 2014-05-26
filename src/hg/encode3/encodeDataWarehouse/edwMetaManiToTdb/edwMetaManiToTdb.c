/* edwMetaManiToTdb - Create a trackDb file based on input from meta file and manifest file.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "net.h"
#include "meta.h"
#include "hmmstats.h"
#include "bigBed.h"
#include "bigWig.h"
#include "bamFile.h"
#include "encode3/encode2Manifest.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwMetaManiToTdb - Create a trackDb file based on input from meta file and manifest file.\n"
  "usage:\n"
  "   edwMetaManiToTdb metaInput manifestInput list,of,vars tdbOutput\n"
  "Where metaInput is metadate in tag-storm format,  manifest is an ENCODE submission manifest,\n"
  "list,of,vars is a comma separated list of variables (tags in meta file) that users can use\n"
  "to select which experiments to view,  and tdbOutput is a trackDb.txt format file\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

char *tagVal(struct encode2Manifest *man, struct hash *metaHash, char *tag)
/* Return value associated with tag or NULL if not found. */
{
char *experiment = man->experiment;
struct meta *meta = hashFindVal(metaHash, experiment);
if (meta == NULL)
    errAbort("experiment %s is in manifest but not meta", experiment);
return metaTagVal(meta, tag);
}

struct composite
/* Info on a composite track. */
    {
    struct composite *next;
    char *name;	    /* Name is either from composite tag, or if missing a concat of other vals. */
    struct slRef *manRefList;	/* Manifest items associated with this one. */
    };

struct composite *makeCompositeList(struct encode2Manifest *manList, struct hash *metaHash)
/* Return a list of composites with everything on manList */
{
struct composite *comp, *compList = NULL;
struct hash *compHash = hashNew(0);
char compName[256];
struct encode2Manifest *man;
for (man = manList; man != NULL; man = man->next)
    {
    char *realComp = tagVal(man, metaHash, "composite");
    if (realComp != NULL)
        safef(compName, sizeof(compName), "%s", realComp);
    else
        {
	char *lab = emptyForNull(tagVal(man, metaHash, "lab"));
	char *dataType = emptyForNull(tagVal(man, metaHash, "dataType"));
	safef(compName, sizeof(compName), "comp%s%s", lab, dataType);
	}
    comp = hashFindVal(compHash, compName);
    if (comp == NULL)
        {
	AllocVar(comp);
	comp->name = cloneString(compName);
	hashAdd(compHash, compName, comp);
	slAddTail(&compList, comp);
	}
    struct slRef *manRef = slRefNew(man);
    slAddTail(&comp->manRefList, manRef);
    }
hashFree(&compHash);
return compList;
}

int trackId = 0, viewId = 0;

struct view
/* A view aka output_type */
    {
    struct view *next;
    char *name;			/* Name of view */
    char *format;	        /* Format associated with view */
    char *trackName;		/* Name when used as a track. */
    struct slRef *manRefList;	/* Files associated with view */
    };

struct view *makeViewList(struct slRef *manRefList)
/* Return a list of views with everything on manList */
{
struct hash *hash = hashNew(0);
struct view *view, *viewList = NULL;
struct slRef *ref;
for (ref = manRefList; ref != NULL; ref = ref->next)
    {
    struct encode2Manifest *man = ref->val;
    char *outputType = man->outputType;
    char *format = man->format;
    view = hashFindVal(hash,  outputType);
    if (view == NULL)
        {
	AllocVar(view);
	view->name = cloneString(outputType);
	view->format = cloneString(format);
	char trackName[64];
	safef(trackName, sizeof(trackName), "v%d", ++viewId);
	view->trackName = cloneString(trackName);
	hashAdd(hash, outputType, view);
	slAddTail(&viewList, view);
	}
    if (!sameString(format, view->format))
        errAbort("Multiple formats (%s and %s) in output_type %s", 
	    format, view->format, view->name);
    struct slRef *newRef = slRefNew(man);
    slAddTail(&view->manRefList, newRef);
    }
hashFree(&hash);
return viewList;
}

int indent = 4;

boolean viewableFormat(char *format)
/* Return TRUE if it's a format we can visualize. */
{
return sameString(format, "bigWig") || sameString(format, "bam") 
       || edwIsSupportedBigBedFormat(format);
}

struct taggedFile
/* A bunch of tags mostly */
    {
    struct taggedFile *next;
    struct metaTagVal *tagList;	/* All tags. */
    struct encode2Manifest *manifest;  /* File in manifest */
    };

struct expVal
/* A value seen in for a tag along with a list of all files with that value */
    {
    struct expVal *next;
    char *val;	    /* Value seen here*/
    struct slRef *tfList;  /* Tagged files this is found in. */
    };

struct expVal *expValFind(struct expVal *list, char *val)
/* Find val in list, return NULL if not found. */
{
struct expVal *el;
for (el = list; el != NULL; el = el->next)
    if (sameString(el->val, val))
         break;
return el;
}

struct expVar
/* An experimental var - a tag, a list of all the values it holds. */
    {
    struct expVar *next;
    char *name;	    /* Name of variable. */
    struct expVal *valList; /* All values seen */
    int useCount;   /* Number of times used */
    double priority;   /* Overall priority score - smaller is better. */
    };

int expVarCmpQuery(const void *va, const void *vb)
/* Compare score (descending) */
{
const struct expVar *a = *((struct expVar **)va);
const struct expVar *b = *((struct expVar **)vb);
double dif = a->priority - b->priority;
if (dif > 0)
    return 1;
else if (dif < 0)
    return -1;
else
    return strcmp(a->name, b->name);
}

struct expVar *expVarsFromTaggedFiles(struct taggedFile *tfList)
/* Build up a list of vars and their values and what file they are found in. */
{
struct hash *tagHash = hashNew(0);
struct expVar *varList = NULL, *var;
struct taggedFile *tf;
for (tf = tfList; tf != NULL; tf = tf->next)
    {
    struct metaTagVal *mtv;
    for (mtv = tf->tagList; mtv != NULL; mtv = mtv->next)
        {
	if (sameString(mtv->val, "n/a") || isEmpty(mtv->tag))
	    continue;
	var = hashFindVal(tagHash, mtv->tag);
	if (var == NULL)
	    {
	    AllocVar(var);
	    var->name = cloneString(mtv->tag);
	    slAddHead(&varList, var);
	    hashAdd(tagHash, var->name, var);
	    }
	struct expVal *val = expValFind(var->valList, mtv->val);
	if (val == NULL)
	    {
	    AllocVar(val);
	    val->val = cloneString(mtv->val);
	    slAddHead(&var->valList, val);
	    }
	var->useCount += 1;
	refAdd(&val->tfList, tf);
	}
    }
hashFree(&tagHash);
return varList;
}

struct taggedFile *taggedFileForComposite(struct composite *composite, struct hash *metaHash)
/* Return a taggedFile for every file in the composite. */
{
struct slRef *manRefList = composite->manRefList;
struct taggedFile *tf, *tfList = NULL;
struct slRef *ref;
for (ref = manRefList; ref != NULL; ref = ref->next)
    {
    /* Wrap up tags and manifest together, including a bonus tag or two from manifest. */
    struct encode2Manifest *man = ref->val;
    struct meta *meta = hashMustFindVal(metaHash, man->experiment);
    AllocVar(tf);
    tf->manifest = man;
    slAddHead(&tfList, tf);
    struct metaTagVal *s, *d;
    for (s = meta->tagList; s != NULL; s = s->next)
        {
	d = metaTagValNew(s->tag, s->val);
	slAddHead(&tf->tagList, d);
	}
    d = metaTagValNew("replicate", man->replicate);
    slAddHead(&tf->tagList, d);
    }
return tfList;
}

struct metaTagVal *metaTagValLookup(struct metaTagVal *list, char *tag)
/* Return metaTagVal on list with given tag name, or NULL if none exists */
{
struct metaTagVal *mtv;
for (mtv = list; mtv != NULL; mtv = mtv->next)
    {
    if (sameString(mtv->tag, tag))
	break;
    }
return mtv;
}

char *metaTagValFindVal(struct metaTagVal *list, char *tag)
/* Return val associated with tag on list, or NULL if no such tag on list */
{
struct metaTagVal *mtv = metaTagValLookup(list, tag);
if (mtv == NULL)
    return NULL;
return mtv->val;
}

struct slName *valsForVar(char *varName, struct taggedFile *tfList)
/* Return all values for given variable. */
{
struct slName *list = NULL;
struct hash *uniqHash = hashNew(7);
struct taggedFile *tf;
for (tf = tfList; tf != NULL; tf = tf->next)
    {
    char *val = metaTagValFindVal(tf->tagList, varName);
    if (val != NULL)
        {
	if (hashLookup(uniqHash, val) == NULL)
	    {
	    hashAdd(uniqHash, val, NULL);
	    slNameAddHead(&list, val);
	    }
        }
    }
hashFree(&uniqHash);
slNameSort(&list);
return list;
}

char *printBigWigViewInfo(FILE *f, char *indent, struct view *view, 
    struct composite *comp, struct taggedFile *tfList)
/* Print out info for a bigWig view, including subtracks. */
{
/* Look at all tracks in this view and calculate overall limits. */
double sumOfSums = 0, sumOfSumSquares = 0;
bits64 sumOfN = 0;
struct taggedFile *tf;
for (tf = tfList; tf != NULL; tf = tf->next)
    {
    if (sameString(tf->manifest->outputType, view->name))
	{
	char *relativeName = tf->manifest->fileName;
	char *path = relativeName;
	struct bbiFile *bbi = bigWigFileOpen(path);
	struct bbiSummaryElement sum = bbiTotalSummary(bbi);
	sumOfSums += sum.sumData;
	sumOfSumSquares += sum.sumSquares;
	sumOfN = sum.validCount;
	bigWigFileClose(&bbi);
	}
    }
double mean = sumOfSums/sumOfN;
double std = calcStdFromSums(sumOfSums, sumOfSumSquares, sumOfN);
double clipMax = mean + 6*std;

/* Output view stanza. */
char type[64];
safef(type, sizeof(type), "bigWig %g %g", 0.0, clipMax);
fprintf(f, "%stype %s\n", indent, type);
fprintf(f, "%sviewLimits 0:%g\n", indent, clipMax);
fprintf(f, "%sminLimit 0\n", indent);
fprintf(f, "%smaxLimit %g\n", indent, clipMax);
fprintf(f, "%sautoScale off\n", indent);
fprintf(f, "%smaxHeightPixels 100:32:16\n", indent);
fprintf(f, "%swindowingFunction mean+whiskers\n", indent);
return cloneString(type);
}

char *printBigBedViewInfo(FILE *f, char *indent, struct view *view, 
    struct composite *comp, struct taggedFile *tfList)
/* Print out info for a bigBed view. */
{
/* Get defined fields and total fields, and make sure they are the same for everyone. */
int defFields = 0, fields = 0;
struct taggedFile *tf, *bigBedTf = NULL;
for (tf = tfList; tf != NULL; tf = tf->next)
    {
    if (sameString(view->name, tf->manifest->outputType))
	{
	struct bbiFile *bbi = bigBedFileOpen(tf->manifest->fileName);
	if (defFields == 0)
	    {
	    fields = bbi->fieldCount;
	    defFields = bbi->definedFieldCount;
	    bigBedTf = tf;
	    }
	else
	    {
	    if (fields != bbi->fieldCount || defFields != bbi->definedFieldCount)
		errAbort("Different formats for bigBeds in %s vs %s", bigBedTf->manifest->fileName,
		    tf->manifest->fileName);
	    }
	bigBedFileClose(&bbi);
	}
    }
char type[32];
safef(type, sizeof(type), "bigBed %d%s", defFields, (fields > defFields ? " +" : ""));
fprintf(f, "%stype %s\n", indent, type); 
return cloneString(type);
}

char *printBamViewInfo(FILE *f, char *indent, struct view *view, 
    struct composite *comp, struct taggedFile *tfList)
/* Print out info for a Bam view. */
{
fprintf(f, "%stype bam\n", indent);
fprintf(f, "%sshowNames off\n", indent);
fprintf(f, "%sbamColorMode gray\n", indent);
fprintf(f, "%sindelDoubleInsert on\n", indent);
fprintf(f, "%sindelQueryInsert on\n", indent);
fprintf(f, "%smaxWindowToDraw 100000\n", indent);
return "bam";
}

char *printFormatSpecificViewInfo(FILE *f, char *indent, struct view *view, 
    struct composite *comp, struct taggedFile *tfList)
/* Print out part of a view trackDb stanza that are format specific. */
{
char *format = view->format;
if (sameString(format, "bigWig"))
    {
    return printBigWigViewInfo(f, indent, view, comp, tfList);
    }
else if (sameString(format, "bam"))
    {
    return printBamViewInfo(f, indent, view, comp, tfList);
    }
else if (edwIsSupportedBigBedFormat(format))
    {
    return printBigBedViewInfo(f, indent, view, comp, tfList);
    }
else
    {
    errAbort("Unrecognized format in printFormatSpecificViewInfo");
    return NULL;
    }
}

void printLeafTrackList(FILE *f, char *indent, struct view *view, struct composite *comp,
    struct slName *varList, struct taggedFile *tfList, char *type)
/* Print list of low level tracks under view */
{
struct taggedFile *tf;
for (tf = tfList; tf != NULL; tf = tf->next)
    {
    if (sameString(tf->manifest->outputType, view->name))
	{
	fprintf(f, "%strack t%d\n", indent, ++trackId);
	fprintf(f, "%sparent %s\n", indent, view->trackName);
	fprintf(f, "%stype %s\n", indent, type);
	fprintf(f, "%ssubGroups view=%s", indent, view->name);
	struct slName *var;
	for (var = varList; var != NULL; var = var->next)
	    {
	    char *val = metaTagValFindVal(tf->tagList, var->name);
	    if (val != NULL)
		fprintf(f, " %s=%s", var->name, val);
	    }
	fprintf(f, "\n");
	fprintf(f, "%sshortLabel", indent);
	for (var = varList; var != NULL; var = var->next)
	    {
	    char *val = metaTagValFindVal(tf->tagList, var->name);
	    if (val != NULL)
		fprintf(f, " %s", val);
	    }
	fprintf(f, "\n");
	char *lab = emptyForNull(metaTagValFindVal(tf->tagList, "lab"));
	char *dataType = emptyForNull(metaTagValFindVal(tf->tagList, "dataType"));
	fprintf(f, "%slongLabel %s %s", indent, lab, dataType);
	boolean gotOne = FALSE;
	for (var = varList; var != NULL; var = var->next)
	    {
	    char *val = metaTagValFindVal(tf->tagList, var->name);
	    if (val != NULL)
		{
		if (gotOne)
		    fprintf(f, ",");
		else
		    gotOne = TRUE;
		fprintf(f, " %s %s", var->name, val);
		}
	    }
	fprintf(f, "\n");
	fprintf(f, "%sbigDataUrl %s\n", indent, tf->manifest->fileName);
	fprintf(f, "\n");
	}
    }
}

void outputComposite(struct composite *comp, struct slName *varList, struct hash *metaHash, FILE *f)
/* Rummage through composite and try to make a trackDb stanza for it. */
{
struct encode2Manifest *firstMan = comp->manRefList->val;
struct view *view, *viewList = makeViewList(comp->manRefList);
fprintf(f, "track %s\n", comp->name);
fprintf(f, "compositeTrack on\n");
char *lab = tagVal(firstMan, metaHash, "lab");
if (lab == NULL)
    lab = "unknown";
char *dataType = tagVal(firstMan, metaHash, "dataType");
if (dataType == NULL)
    dataType = "unknown";
fprintf(f, "shortLabel %s %s\n", lab, dataType);
fprintf(f, "longLabel %s %s\n", lab, dataType);
fprintf(f, "type bed 3\n");
fprintf(f, "subGroup1 view Views");
for (view = viewList; view != NULL; view = view->next)
    {
    if (viewableFormat(view->format))
	fprintf(f, " %s=%s", view->name, view->name);
    }
fprintf(f, "\n");

struct taggedFile *tfList = taggedFileForComposite(comp, metaHash);
uglyf("%d tfList\n", slCount(tfList));

struct slName *var;
int groupId = 1;
for (var = varList; var != NULL; var = var->next)
    {
    fprintf(f, "subGroup%d %s %s", ++groupId, var->name, var->name);
    struct slName *val, *valList = valsForVar(var->name, tfList);
    for (val = valList; val != NULL; val = val->next)
        fprintf(f, " %s=%s", val->name, val->name);
    fprintf(f, "\n");
    }

fprintf(f, "dimensions");
int varIx = 0;
for (var = varList; var != NULL; var = var->next)
    {
    char c = 0;
    varIx += 1;
    if (varIx == 1)
	c = 'Y';
    else if (varIx == 2)
	c = 'X';
    else if (varIx == 3)
	c = 'Z';
    else
        errAbort("Too many dimensions in list,of,vars");
    fprintf(f, " dimension%c=%s", c, var->name);
    }
fprintf(f, "\n");

fprintf(f, "sortOrder");
for (var = varList; var != NULL; var = var->next)
    fprintf(f, " %s=+", var->name);
fprintf(f, " view=+\n");
fprintf(f, "dragAndDrop subtracks\n");
fprintf(f, "\n");

for (view = viewList; view != NULL; view = view->next)
    {
    if (viewableFormat(view->format))
	{
	fprintf(f, "    track %s\n", view->trackName);
	fprintf(f, "    parent %s\n", comp->name);
	fprintf(f, "    view %s\n", view->name);
	fprintf(f, "    shortLabel %s\n", view->name);
	fprintf(f, "    longLabel %s\n", view->name);
	fprintf(f, "    visibility dense\n");
	char *type = printFormatSpecificViewInfo(f, "    ", view, comp, tfList);
	fprintf(f, "\n");
	printLeafTrackList(f, "\t", view, comp, varList, tfList, type);
	}
    }
}

void edwMetaManiToTdb(char *metaInput, char *manifestInput, char *varCommaList, char *tdbOutput)
/* edwMetaManiToTdb - Create a trackDb file based on input from meta file and manifest file.. */
{
struct encode2Manifest *manList = encode2ManifestShortLoadAll(manifestInput);
struct meta *metaForest = metaLoadAll(metaInput, "meta", "parent", TRUE, FALSE);
struct slName *varList = slNameListFromComma(varCommaList);
struct hash *hash = metaHash(metaForest);
struct composite *comp, *compList = makeCompositeList(manList, hash);
uglyf("%d elements in manList, %d in metaForest top level, %d total, %d vars, %d composites\n", slCount(manList), slCount(metaForest), hash->elCount, slCount(varList), slCount(compList));

FILE *f = mustOpen(tdbOutput, "w");
for (comp = compList; comp != NULL; comp = comp->next)
    {
    outputComposite(comp, varList, hash, f);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
edwMetaManiToTdb(argv[1], argv[2], argv[3], argv[4]);
return 0;
}

