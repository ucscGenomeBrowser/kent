#include "common.h"
#include "localmem.h"
#include "hash.h"
#include "linefile.h"
#include "obscure.h"
#include "ra.h"
#include "dystring.h"
#include "tdbRecord.h"

static char const rcsid[] = "$Id: tdbRecord.c,v 1.4 2009/12/06 20:11:22 kent Exp $";

static struct tdbFilePos *tdbFilePosNew(struct lm *lm, 
    char *fileName, 		/* File name. */
    int startLineIx,		/* File start line. */
    int endLineIx,		/* File end line. */
    char *text)			/* Text of stanza. */
/* Create new tdbFilePos record. */
{
struct tdbFilePos *fp;
lmAllocVar(lm, fp);
fp->fileName = lmCloneString(lm, fileName);
fp->startLineIx = startLineIx;
fp->endLineIx = endLineIx;
fp->text = lmCloneString(lm, text);
return fp;
}

struct tdbField *tdbRecordField(struct tdbRecord *record, char *fieldName)
/* Return named field if it exists, otherwise NULL */
{
struct tdbField *field;
for (field = record->fieldList; field != NULL; field = field->next)
    {
    if (sameString(field->name, fieldName))
        return field;
    }
return NULL;
}

struct tdbField *tdbFieldNew(char *name, char *val, struct lm *lm)
/* Return new tdbField. */
{
struct tdbField *field;
lmAllocVar(lm, field);
field->name = lmCloneString(lm, name);
val = emptyForNull(skipLeadingSpaces(val));
field->val = lmCloneString(lm, val);
return field;
}

static struct slPair *trackDbParseCompositeSettingsByView(char *input)
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
    cnt = chopByChar(settings,',',words,ArraySize(settings));
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

struct tdbRecord *tdbRecordReadOne(struct lineFile *lf, char *key, struct lm *lm)
/* Read next record from file. Returns NULL at end of file. */
{
struct tdbField *fieldList = NULL;
char *keyVal = NULL;
boolean override = FALSE;
struct slPair *settingsByView = NULL;
struct hash *subGroups = NULL;
char *view = NULL;
struct hash *viewHash = NULL;
int startLineIx = lf->lineIx;
struct dyString *dy = dyStringNew(0);

if (!raSkipLeadingEmptyLines(lf,dy))
    return NULL;

char *tag, *val;
while (raNextTagVal(lf, &tag, &val,dy))
    {
    struct tdbField *field = tdbFieldNew(tag, val, lm);
    if (sameString(field->name, key))
	{
	keyVal = lmCloneFirstWord(lm, field->val);
	if (endsWith(field->val, "override") && !sameString("override", field->val))
	    {
	    override = TRUE;
	    field->val = lmCloneString(lm, keyVal);
	    }
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

struct tdbRecord *record;
lmAllocVar(lm, record);
record->fieldList = fieldList;
record->key = keyVal;
record->override = override;
record->settingsByView = settingsByView;
record->subGroups = subGroups;
record->viewHash = viewHash;
record->view = view;
record->posList = tdbFilePosNew(lm, lf->fileName, startLineIx, lf->lineIx, dy->string);
dyStringFree(&dy);
return record;
}

