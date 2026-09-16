/* trackDbExpand - build subtracks from a metadata file instead of from trackDb rows.
 * See trackDbExpand.h for what the two files look like. */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "ra.h"
#include "udc.h"
#include "jsonParse.h"
#include "htmlColor.h"
#include "hdb.h"
#include "hgConfig.h"
#include "sqlNum.h"
#include "trackDb.h"
#include "trackDbExpand.h"

struct subTemplate
/* One stanza of a template file: how to make one subtrack out of one metadata row. */
    {
    struct subTemplate *next;
    char *dataType;             /* Name of the data type, last part of the track name. */
    struct slPair *settings;    /* Settings, with ${column} references not yet filled in. */
    char *whenColumn;           /* Column the condition tests, NULL when there is no condition. */
    char *whenOp;               /* Comparison the condition makes. */
    double whenValue;           /* Number the condition compares against. */
    };

static void templateTakeWhen(struct subTemplate *temp, char *fileName)
/* Move the stanza's dataTypeWhen line out of its settings and into the condition fields,
 * so that it makes a subtrack for some rows only rather than becoming a track setting. */
{
struct slPair *pair, *prev = NULL;

for (pair = temp->settings; pair != NULL; prev = pair, pair = pair->next)
    if (sameString(pair->name, "dataTypeWhen"))
        break;
if (pair == NULL)
    return;
if (prev == NULL)
    temp->settings = pair->next;
else
    prev->next = pair->next;

char *dupe = cloneString(pair->val);
char *words[4];
if (chopByWhite(dupe, words, ArraySize(words)) != 3)
    errAbort("dataTypeWhen of %s is '%s', and should be a column, a comparison and a number",
        fileName, (char *)pair->val);
temp->whenColumn = cloneString(words[0]);
temp->whenOp = cloneString(words[1]);
temp->whenValue = sqlDouble(words[2]);
freeMem(dupe);
}

static boolean templateApplies(struct subTemplate *temp, char **row, struct hash *colHash,
    char *fileName)
/* Does this stanza make a subtrack out of this row?  A stanza with no condition always does. */
{
if (temp->whenColumn == NULL)
    return TRUE;

int colIx = hashIntValDefault(colHash, temp->whenColumn, -1);
if (colIx < 0)
    errAbort("dataTypeWhen of %s tests column '%s', which the metadata file does not have",
        fileName, temp->whenColumn);
double value = sqlDouble(row[colIx]);
char *op = temp->whenOp;

if (sameString(op, ">"))
    return value > temp->whenValue;
if (sameString(op, ">="))
    return value >= temp->whenValue;
if (sameString(op, "<"))
    return value < temp->whenValue;
if (sameString(op, "<="))
    return value <= temp->whenValue;
if (sameString(op, "=="))
    return value == temp->whenValue;
if (sameString(op, "!="))
    return value != temp->whenValue;
errAbort("dataTypeWhen of %s compares with '%s', and should use one of > >= < <= == !=",
    fileName, op);
return FALSE;
}

static struct subTemplate *templatesFromFile(char *fileName)
/* Read the template file.  Each stanza starts with a dataType line. */
{
struct lineFile *lf = udcWrapShortLineFile(fileName, NULL, 1024*1024);
struct subTemplate *list = NULL;
struct slPair *stanza;

while ((stanza = raNextStanzAsPairs(lf)) != NULL)
    {
    if (differentString(stanza->name, "dataType"))
        errAbort("Stanza does not start with dataType, line %d of %s", lf->lineIx, fileName);
    struct subTemplate *temp;
    AllocVar(temp);
    temp->dataType = cloneString(stanza->val);
    temp->settings = stanza->next;
    stanza->next = NULL;
    templateTakeWhen(temp, fileName);
    slAddHead(&list, temp);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

static struct hash *colorHashFromFile(char *fileName)
/* Read a colorSettingsUrl file into a hash of column name to a hash of column value to
 * an "r,g,b" string.  This is the file the faceted UI already reads for its facet
 * swatches: an object keyed by column name, whose values are objects keyed by a value
 * of that column, whose leaves are #rrggbb codes. */
{
char *text = udcFileReadAll(fileName, NULL, 64*1024*1024, NULL);
struct hash *byColumn = hashNew(0);
struct hashCookie columns = hashFirst(jsonObjectVal(jsonParse(text), fileName));
struct hashEl *column;

while ((column = hashNext(&columns)) != NULL)
    {
    struct hash *colors = hashNew(0);
    struct hashCookie values = hashFirst(jsonObjectVal(column->val, column->name));
    struct hashEl *value;
    while ((value = hashNext(&values)) != NULL)
        {
        char *code = jsonStringVal(value->val, value->name);
        unsigned packed;
        if (!htmlColorForCode(code, &packed))
            errAbort("Color '%s' of %s in %s is not an #rrggbb code",
                code, value->name, fileName);
        int r, g, b;
        htmlColorToRGB(packed, &r, &g, &b);
        char rgb[32];
        safef(rgb, sizeof(rgb), "%d,%d,%d", r, g, b);
        hashAdd(colors, value->name, cloneString(rgb));
        }
    hashAdd(byColumn, column->name, colors);
    }
freeMem(text);
return byColumn;
}

static char *colorLookup(struct hash *colorHash, char *colName, char *value, char *lookup,
    char *fileName)
/* Return the color the palette gives this value of this column.  A value the palette
 * does not mention is black, which is what the stanzas this replaces used. */
{
if (differentString(lookup, "colorSettings"))
    errAbort("Template of %s asks for a '%s' lookup, which is not a kind of lookup there is",
        fileName, lookup);
if (colorHash == NULL)
    errAbort("Template of %s does a colorSettings lookup, but the track has no colorSettingsUrl",
        fileName);
struct hash *colors = hashFindVal(colorHash, colName);
char *color = (colors == NULL ? NULL : hashFindVal(colors, value));
return (color == NULL ? "0,0,0" : color);
}

static char *fillInRow(char *template, char **row, struct hash *colHash,
    struct hash *colorHash, char *fileName)
/* Return a copy of template with every ${column} replaced by that column of row. */
{
if (strstr(template, "${") == NULL)
    return cloneString(template);

struct dyString *dy = dyStringNew(strlen(template) + 128);
char *s = template;
for (;;)
    {
    char *open = strstr(s, "${");
    if (open == NULL)
        {
        dyStringAppend(dy, s);
        break;
        }
    dyStringAppendN(dy, s, open - s);
    char *close = strchr(open, '}');
    if (close == NULL)
        errAbort("No closing brace in template '%s' of %s", template, fileName);
    char colName[256];
    int nameSize = close - (open + 2);
    if (nameSize >= (int)sizeof(colName))
        errAbort("Column name too long in template '%s' of %s", template, fileName);
    memcpy(colName, open + 2, nameSize);
    colName[nameSize] = 0;

    /* ${column:colorSettings} puts in the color the palette gives that column's value,
     * rather than the value itself. */
    char *lookup = strchr(colName, ':');
    if (lookup != NULL)
        *lookup++ = 0;
    int colIx = hashIntValDefault(colHash, colName, -1);
    if (colIx < 0)
        errAbort("Template of %s asks for column '%s', which the metadata file does not have",
            fileName, colName);
    char *val = row[colIx];
    if (lookup != NULL)
        val = colorLookup(colorHash, colName, val, lookup, fileName);
    dyStringAppend(dy, val);
    s = close + 1;
    }
return dyStringCannibalize(&dy);
}

static struct trackDb *tdbFromTemplate(struct trackDb *parent, struct subTemplate *temp,
    char *key, char **row, struct hash *colHash, struct hash *colorHash, char *fileName,
    int priorityIx)
/* Make one subtrack of parent from one template stanza and one metadata row. */
{
struct trackDb *tdb = trackDbNew();
char name[512];

/* The faceted composite UI parses this three part name back apart, so the shape of it
 * is part of the interface rather than a local choice.  See facetedCompositeUi(). */
safef(name, sizeof(name), "%s_%s_%s", parent->track, key, temp->dataType);
tdb->track = cloneString(name);
tdb->table = cloneString(name);

/* Same size as trackDbSettingsFromString() gives a track that came out of the trackDb
 * table, so a generated subtrack costs the same in the trackDb cache as a stored one. */
tdb->settingsHash = hashNew(7);
struct dyString *dy = dyStringNew(512);
struct slPair *pair;
for (pair = temp->settings; pair != NULL; pair = pair->next)
    {
    char *val = fillInRow(pair->val, row, colHash, colorHash, fileName);
    hashAdd(tdb->settingsHash, pair->name, val);
    dyStringPrintf(dy, "%s %s\n", pair->name, val);
    }

/* Settings every generated subtrack gets, whatever the template says. */
char parentSetting[512];
safef(parentSetting, sizeof(parentSetting), "%s off", parent->track);
hashAdd(tdb->settingsHash, "parent", cloneString(parentSetting));
dyStringPrintf(dy, "parent %s\n", parentSetting);

char priority[64];
safef(priority, sizeof(priority), "%.5f", priorityIx * 0.00001);
hashAdd(tdb->settingsHash, "priority", cloneString(priority));
dyStringPrintf(dy, "priority %s\n", priority);

tdb->settings = dyStringCannibalize(&dy);
trackDbFieldsFromSettings(tdb);
return tdb;
}

static struct trackDb *expandOne(struct trackDb *parent, struct dyString *incFiles)
/* Build the subtracks of one container out of its metadata and template files. */
{
char *templateFile = hReplaceGbdb(trackDbSetting(parent, "subtrackTemplate"));
char *metaFile = hReplaceGbdb(trackDbSetting(parent, "metaDataUrl"));
char *primaryKey = trackDbSetting(parent, "primaryKey");
char *dataTypeColumn = trackDbSetting(parent, "dataTypeColumn");
char *colorFile = hReplaceGbdb(trackDbSetting(parent, "colorSettingsUrl"));

if (metaFile == NULL || primaryKey == NULL)
    errAbort("Track %s has subtrackTemplate but no metaDataUrl or no primaryKey",
        parent->track);
if (incFiles != NULL)
    {
    dyStringPrintf(incFiles, "%s\n%s\n", templateFile, metaFile);
    if (colorFile != NULL)
        dyStringPrintf(incFiles, "%s\n", colorFile);
    }

struct subTemplate *templates = templatesFromFile(templateFile);
struct hash *colorHash = (colorFile == NULL ? NULL : colorHashFromFile(colorFile));
struct lineFile *lf = udcWrapShortLineFile(metaFile, NULL, 256*1024*1024);

/* The first line names the columns.  Remember where each one is. */
char *header;
if (!lineFileNextReal(lf, &header))
    errAbort("No header line in %s", metaFile);
int colCount = countChars(header, '\t') + 1;
char **colNames;
AllocArray(colNames, colCount);
chopByChar(header, '\t', colNames, colCount);
struct hash *colHash = hashNew(0);
int ii;
for (ii = 0; ii < colCount; ii++)
    hashAddInt(colHash, colNames[ii], ii);
if (!hashLookup(colHash, primaryKey))
    errAbort("Metadata file %s has no column named %s", metaFile, primaryKey);
int keyIx = hashIntVal(colHash, primaryKey);

char **row;
AllocArray(row, colCount);
struct trackDb *list = NULL;
char *line;
int priorityIx = 0;
while (lineFileNextReal(lf, &line))
    {
    int wordCount = chopByChar(line, '\t', row, colCount);
    if (wordCount != colCount)
        errAbort("Expecting %d columns, got %d, line %d of %s",
            colCount, wordCount, lf->lineIx, metaFile);

    /* A row may say which data types it has.  Without that column it has all of them. */
    char *rowTypes = NULL;
    if (dataTypeColumn != NULL)
        rowTypes = row[hashIntVal(colHash, dataTypeColumn)];

    struct slName *typeList = NULL;
    if (rowTypes != NULL)
        typeList = slNameListFromString(rowTypes, ',');

    struct subTemplate *temp;
    for (temp = templates; temp != NULL; temp = temp->next)
        {
        if (typeList != NULL && !slNameInList(typeList, temp->dataType))
            continue;
        if (!templateApplies(temp, row, colHash, metaFile))
            continue;
        priorityIx += 1;
        slAddHead(&list, tdbFromTemplate(parent, temp, row[keyIx], row, colHash, colorHash,
            metaFile, priorityIx));
        }
    slFreeList(&typeList);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

struct trackDb *trackDbExpandSubtracks(struct trackDb *tdbList, struct dyString *incFiles)
/* Expand every track in the list that has a subtrackTemplate setting into its
 * generated subtracks.  Returns the list with the new tracks spliced in. */
{
if (!cfgOptionBooleanDefault("expandSubtracks", FALSE))
    return tdbList;

struct trackDb *tdb, *expanded = NULL;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    if (trackDbLocalSetting(tdb, "subtrackTemplate") != NULL)
        expanded = slCat(expanded, expandOne(tdb, incFiles));
    }
return slCat(tdbList, expanded);
}
