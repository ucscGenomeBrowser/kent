/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "localmem.h"
#include "hash.h"
#include "linefile.h"
#include "obscure.h"
#include "raRecord.h"


struct raFilePos *raFilePosNew(struct lm *lm, char *fileName, int lineIx)
/* Create new raFilePos record. */
{
struct raFilePos *fp;
lmAllocVar(lm, fp);
fp->fileName = fileName;
fp->lineIx = lineIx;
return fp;
}

struct raField *raRecordField(struct raRecord *ra, char *fieldName)
/* Return named field if it exists, otherwise NULL */
{
struct raField *field;
for (field = ra->fieldList; field != NULL; field = field->next)
    {
    if (sameString(field->name, fieldName))
        return field;
    }
return NULL;
}

struct raField *raFieldNew(char *name, char *val, struct lm *lm)
/* Return new raField. */
{
struct raField *field;
lmAllocVar(lm, field);
field->name = lmCloneString(lm, name);
val = emptyForNull(skipLeadingSpaces(val));
field->val = lmCloneString(lm, val);
return field;
}

struct raField *raFieldFromLine(char *line, struct lm *lm)
/* Parse out line and convert it to a raField.  Will return NULL on empty lines. 
 * Will insert some zeroes into the input line as well. */
{
char *word = nextWord(&line);
if (word == NULL)
    return NULL;
trimSpaces(line);
return raFieldNew(word, line, lm);
}

struct slPair *trackDbParseCompositeSettingsByView(char *input)
/* Parse something that looks like:
 * wigView:windowingFunction=mean+whiskers,viewLimits=0:1000 bedView:minScore=200  
 * Into
 *    slPair "wigView", 
 * 	 { slPair "windowingFunction", "mean+whiskers" }
 *	 { slPair "viewLimits", "0:1000" }
 *    slPair "bedView",
 * 	 { slPair "minScore", "200" } */
{
char *inBuf = cloneString(input);
struct slPair *viewList = NULL, *view;
char *words[32];
int cnt,ix;
cnt = chopLine(inBuf, words);
for(ix=0;ix<cnt;ix++)
    {
    char *viewName = words[ix];
    char *settings = strchr(viewName, ':');
    struct slPair *el, *list = NULL;
    if (settings == NULL)
        errAbort("Missing colon in settingsByView on %s", viewName);
    *settings++ = 0;
    char *words[32];
    int cnt,ix;
    cnt = chopByChar(settings,',',words,ArraySize(words));
    for (ix=0; ix<cnt; ix++)
        {
	char *name = words[ix];
	char *val = strchr(name, '=');
	if (val == NULL)
	    errAbort("Missing equals in settingsByView on %s", name);
	*val++ = 0;
	AllocVar(el);
	el->name = cloneString(name);
	el->val = cloneString(val);
	slAddHead(&list,el);
	}
    slReverse(&list);
    AllocVar(view);
    view->name = cloneString(viewName);
    view->val = list;
    slAddHead(&viewList, view);
    }
slReverse(&viewList);
freeMem(inBuf);
return viewList;
}

struct raRecord *raRecordReadOne(struct lineFile *lf, char *key, struct lm *lm)
/* Read next record from file. Returns NULL at end of file. */
{
struct raField *field, *fieldList = NULL;
char *line;
char *keyVal = NULL;
boolean override = FALSE;
struct slPair *settingsByView = NULL;
struct hash *subGroups = NULL;
char *view = NULL;
struct hash *viewHash = NULL;

/* Skip over blank initial lines. */
for (;;)
    {
    if (!lineFileNext(lf, &line, NULL))
        return NULL;
    line = skipLeadingSpaces(line);
    if (line != NULL && (line[0] != 0 && line[0] != '#'))
         {
	 lineFileReuse(lf);
	 break;
	 }
    }

/* Keep going until get a blank line. */
for (;;)
    {
    if (!lineFileNext(lf, &line, NULL))
        break;
    line = skipLeadingSpaces(line);
    if (line[0] == '#')
        continue;
    field = raFieldFromLine(line, lm);
    if (field == NULL)
        break;
    if (sameString(field->name, key))
	{
	keyVal = lmCloneFirstWord(lm, field->val);
	if (endsWith(field->val, "override") && !sameString("override", field->val))
	    override = TRUE;
	}
    else if (sameString(field->name, "settingsByView"))
        {
	settingsByView = trackDbParseCompositeSettingsByView(field->val);
	}
    else if (sameString(field->name, "subGroups"))
        {
	subGroups = hashVarLine(field->val, lf->lineIx);
	view = hashFindVal(subGroups, "view");
	}
    else if (startsWith("subGroup", field->name) && isdigit(field->name[8]))
        {
	if (startsWithWord("view", field->val))
	    {
	    char *buf = cloneString(field->val);
	    char *line = buf;
	    nextWord(&line);	/* Skip over view. */
	    nextWord(&line);	/* Skip over view name. */
	    viewHash = hashThisEqThatLine(line, lf->lineIx, FALSE);
	    freeMem(buf);
	    }
	}
    slAddHead(&fieldList, field);
    }

if (fieldList == NULL)
    return NULL;
slReverse(&fieldList);
struct raRecord *record;
lmAllocVar(lm, record);
record->fieldList = fieldList;
record->key = keyVal;
record->override = override;
record->settingsByView = settingsByView;
record->subGroups = subGroups;
record->viewHash = viewHash;
record->view = view;
return record;
}

