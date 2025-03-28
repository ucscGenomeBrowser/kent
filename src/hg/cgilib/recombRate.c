/* recombRate.c was originally generated by the autoSql program, which also 
 * generated recombRate.h and recombRate.sql.  This module links the database and
 * the RAM representation of objects. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "linefile.h"
#include "dystring.h"
#include "jksql.h"
#include "recombRate.h"


void recombRateStaticLoad(char **row, struct recombRate *ret)
/* Load a row from recombRate table into ret.  The contents of ret will
 * be replaced at the next call to this function. */
{

ret->chrom = row[0];
ret->chromStart = sqlUnsigned(row[1]);
ret->chromEnd = sqlUnsigned(row[2]);
ret->name = row[3];
ret->decodeAvg = atof(row[4]);
ret->decodeFemale = atof(row[5]);
ret->decodeMale = atof(row[6]);
ret->marshfieldAvg = atof(row[7]);
ret->marshfieldFemale = atof(row[8]);
ret->marshfieldMale = atof(row[9]);
ret->genethonAvg = atof(row[10]);
ret->genethonFemale = atof(row[11]);
ret->genethonMale = atof(row[12]);
}

struct recombRate *recombRateLoad(char **row)
/* Load a recombRate from row fetched with select * from recombRate
 * from database.  Dispose of this with recombRateFree(). */
{
struct recombRate *ret;

AllocVar(ret);
ret->chrom = cloneString(row[0]);
ret->chromStart = sqlUnsigned(row[1]);
ret->chromEnd = sqlUnsigned(row[2]);
ret->name = cloneString(row[3]);
ret->decodeAvg = atof(row[4]);
ret->decodeFemale = atof(row[5]);
ret->decodeMale = atof(row[6]);
ret->marshfieldAvg = atof(row[7]);
ret->marshfieldFemale = atof(row[8]);
ret->marshfieldMale = atof(row[9]);
ret->genethonAvg = atof(row[10]);
ret->genethonFemale = atof(row[11]);
ret->genethonMale = atof(row[12]);
return ret;
}

struct recombRate *recombRateLoadAll(char *fileName) 
/* Load all recombRate from a tab-separated file.
 * Dispose of this with recombRateFreeList(). */
{
struct recombRate *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[13];

while (lineFileRow(lf, row))
    {
    el = recombRateLoad(row);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

struct recombRate *recombRateLoadWhere(struct sqlConnection *conn, char *table, char *where)
/* Load all recombRate from table that satisfy where clause. The
 * where clause may be NULL in which case whole table is loaded
 * Dispose of this with recombRateFreeList(). */
{
struct recombRate *list = NULL, *el;
struct dyString *query = dyStringNew(256);
struct sqlResult *sr;
char **row;

sqlDyStringPrintf(query, "select * from %s", table);
if (where != NULL)
    sqlDyStringPrintf(query, " where %-s", where);
sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = recombRateLoad(row);
    slAddHead(&list, el);
    }
slReverse(&list);
sqlFreeResult(&sr);
dyStringFree(&query);
return list;
}

struct recombRate *recombRateCommaIn(char **pS, struct recombRate *ret)
/* Create a recombRate out of a comma separated string. 
 * This will fill in ret if non-null, otherwise will
 * return a new recombRate */
{
char *s = *pS;

if (ret == NULL)
    AllocVar(ret);
ret->chrom = sqlStringComma(&s);
ret->chromStart = sqlUnsignedComma(&s);
ret->chromEnd = sqlUnsignedComma(&s);
ret->name = sqlStringComma(&s);
ret->decodeAvg = sqlFloatComma(&s);
ret->decodeFemale = sqlFloatComma(&s);
ret->decodeMale = sqlFloatComma(&s);
ret->marshfieldAvg = sqlFloatComma(&s);
ret->marshfieldFemale = sqlFloatComma(&s);
ret->marshfieldMale = sqlFloatComma(&s);
ret->genethonAvg = sqlFloatComma(&s);
ret->genethonFemale = sqlFloatComma(&s);
ret->genethonMale = sqlFloatComma(&s);
*pS = s;
return ret;
}

void recombRateFree(struct recombRate **pEl)
/* Free a single dynamically allocated recombRate such as created
 * with recombRateLoad(). */
{
struct recombRate *el;

if ((el = *pEl) == NULL) return;
freeMem(el->chrom);
freeMem(el->name);
freez(pEl);
}

void recombRateFreeList(struct recombRate **pList)
/* Free a list of dynamically allocated recombRate's */
{
struct recombRate *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    recombRateFree(&el);
    }
*pList = NULL;
}

void recombRateOutput(struct recombRate *el, FILE *f, char sep, char lastSep) 
/* Print out recombRate.  Separate fields with sep. Follow last field with lastSep. */
{
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->chrom);
if (sep == ',') fputc('"',f);
fputc(sep,f);
fprintf(f, "%u", el->chromStart);
fputc(sep,f);
fprintf(f, "%u", el->chromEnd);
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->name);
if (sep == ',') fputc('"',f);
fputc(sep,f);
fprintf(f, "%f", el->decodeAvg);
fputc(sep,f);
fprintf(f, "%f", el->decodeFemale);
fputc(sep,f);
fprintf(f, "%f", el->decodeMale);
fputc(sep,f);
fprintf(f, "%f", el->marshfieldAvg);
fputc(sep,f);
fprintf(f, "%f", el->marshfieldFemale);
fputc(sep,f);
fprintf(f, "%f", el->marshfieldMale);
fputc(sep,f);
fprintf(f, "%f", el->genethonAvg);
fputc(sep,f);
fprintf(f, "%f", el->genethonFemale);
fputc(sep,f);
fprintf(f, "%f", el->genethonMale);
fputc(lastSep,f);
}

