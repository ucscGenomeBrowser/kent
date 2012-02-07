/* hmmpfamParse - Parse hmmpfam files.. */

#include "common.h"
#include "linefile.h"
#include "errabort.h"
#include "spacedColumn.h"
#include "hmmPfamParse.h"


void hpfModelFree(struct hpfModel **pMod)
/* Free memory associated with hpfModel */
{
struct hpfModel *mod = *pMod;
if (mod != NULL)
    {
    freeMem(mod->name);
    freeMem(mod->description);
    slFreeList(&mod->domainList);
    freez(pMod);
    }
}

void hpfModelFreeList(struct hpfModel **pList)
/* Free a list of dynamically allocated hpfModel's */
{
struct hpfModel *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hpfModelFree(&el);
    }
*pList = NULL;
}


void hpfResultFree(struct hpfResult **pHr)
/* Free memory associated with hpfResult */
{
struct hpfResult *hr = *pHr;
if (hr != NULL)
    {
    freeMem(hr->name);
    hpfModelFreeList(&hr->modelList);
    freez(pHr);
    }
}

void hpfResultFreeList(struct hpfResult **pList)
/* Free a list of dynamically allocated hpfResult's */
{
struct hpfResult *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hpfResultFree(&el);
    }
*pList = NULL;
}

void parseErr(struct lineFile *lf, char *format, ...)
/* Print out a parse error message. */
{
va_list args;
va_start(args, format);
vaWarn(format, args);
va_end(args);
errAbort("line %d of %s", lf->lineIx, lf->fileName);
}

char *needLineStartingWith(struct lineFile *lf, char *start, int maxCount)
/* Get next line that starts as so */
{
char *line = lineFileSkipToLineStartingWith(lf, start, maxCount);
if (line == NULL)
     parseErr(lf, "Missing line starting with \"%s\"", start);
return line;
}

void spacedColumnFatten(struct spacedColumn *colList)
/* Make columns extend all the way to the next column. */
{
struct spacedColumn *col, *nextCol;
for (col = colList; col != NULL; col = nextCol)
    {
    nextCol = col->next;
    if (nextCol == NULL)
        break;
    col->size = nextCol->start - col->start - 1;
    }
}

struct hpfModel *hpfFindResultInModel(struct hpfResult *hr, char *modName)
/* Look for named result in model. */
{
struct hpfModel *mod;
for (mod = hr->modelList; mod != NULL; mod = mod->next)
    if (sameString(mod->name, modName))
	break;
return mod;
}

struct hpfResult *hpfNext(struct lineFile *lf)
/* Parse out next record in hmmpfam result file. */
{
/* Seek to first line that starts with "Query sequence:" and parse name out of it. */
char *queryPat = "Query sequence: ";
char *line = lineFileSkipToLineStartingWith(lf, queryPat, 100);
if (line == NULL)
    return NULL;
line += strlen(queryPat);
char *query = cloneString(nextWord(&line));
if (query == NULL)
    parseErr(lf, "Missing sequence name");

/* Seek to start of model list, figuring out width of fields we need in the process. */
needLineStartingWith(lf, "Scores for sequence family", 10);
needLineStartingWith(lf, "Model ", 2);
char *template = needLineStartingWith(lf, "----", 1);
struct spacedColumn *colList = spacedColumnFromSample(template);
spacedColumnFatten(colList);
int colCount = slCount(colList);
if (colCount < 5)
    parseErr(lf, "Expecting at least 5 columns");

/* Parse out all the models. */
struct hpfResult *hr;
AllocVar(hr);
hr->name = query;
for (;;)
    {
    lineFileNeedNext(lf, &line, NULL);
    line = skipLeadingSpaces(line);
    if (line[0] == 0)
        break;
    if (startsWith("[no hits above thresholds]", line))
        break;
    char *row[colCount];
    if (!spacedColumnParseLine(colList, line, row))
        parseErr(lf, "short line");
    struct hpfModel *mod;
    AllocVar(mod);
    mod->name = cloneString(row[0]);
    mod->description = cloneString(row[1]);
    mod->score = lineFileNeedDouble(lf, row, 2);
    mod->eVal = lineFileNeedDouble(lf, row, 3);
    slAddTail(&hr->modelList, mod);
    }
slFreeList(&colList);

/* Skip over to the section on domains, figuriong out column widths while we're at it. */
needLineStartingWith(lf, "Parsed for domains:", 10);
needLineStartingWith(lf, "Model ", 2);
template = needLineStartingWith(lf, "----", 1);
colList = spacedColumnFromSample(template);
colCount = slCount(colList);
if (colCount < 8)
    parseErr(lf, "Expecting at least 8 columns.");
struct spacedColumn *col2 = colList->next;
colList->size = col2->start - 1;

/* Parse out all the domains. */
for (;;)
    {
    lineFileNeedNext(lf, &line, NULL);
    line = skipLeadingSpaces(line);
    if (line[0] == 0)
        break;
    if (startsWith("[no hits above thresholds]", line))
        break;
    char *row[colCount];
    if (!spacedColumnParseLine(colList, line, row))
        parseErr(lf, "short line");
    struct hpfModel *mod = hpfFindResultInModel(hr, row[0]);
    if (mod == NULL)
        parseErr(lf, "Model %s in domain section but not model section", row[0]);
    struct hpfDomain *dom;
    AllocVar(dom);
    dom->qStart = lineFileNeedNum(lf, row, 2) - 1;
    dom->qEnd = lineFileNeedNum(lf, row, 3);
    dom->hmmStart = lineFileNeedNum(lf, row, 4) - 1;
    dom->hmmEnd = lineFileNeedNum(lf, row, 5);
    dom->score = lineFileNeedDouble(lf, row, 6);
    dom->eVal = lineFileNeedDouble(lf, row, 7);
    slAddTail(&mod->domainList, dom);
    }
slFreeList(&colList);
if (!lineFileSkipToLineStartingWith(lf, "//", 10000000))
    parseErr(lf, "Expecting //");
return hr;
}

