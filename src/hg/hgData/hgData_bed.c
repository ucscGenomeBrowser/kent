/* hgData_bed - functions dealing with BED data. */
#include "common.h"
#include "hgData.h"
#include "sqlList.h"
#include "bed.h"
#include "hdb.h"

static char const rcsid[] = "$Id: hgData_bed.c,v 1.1.2.2 2009/01/08 17:14:12 mikep Exp $";

void printBed(struct bed *b, struct hTableInfo *hti)
// print out rows of bed data, each row as a list of columns
{
struct bed *t;
if (!b)
    return;
printf(" \"bedColumns\" : [");
if (hti->startField[0] != 0)
    printf("\"%s\",", hti->startField);
if (hti->endField[0] != 0)
    printf("\"%s\",", hti->endField);
if (hti->nameField[0] != 0)
    printf("\"%s\",", hti->nameField);
if (hti->scoreField[0] != 0)
    printf("\"%s\",", hti->scoreField);
if (hti->strandField[0] != 0)
    printf("\"%s\",", hti->strandField);
if (hti->cdsStartField[0] != 0)
    printf("\"%s\",", hti->cdsStartField);
if (hti->cdsEndField[0] != 0)
    printf("\"%s\",", hti->cdsEndField);
if (hti->countField[0] != 0)
    printf("\"%s\",", hti->countField);
if (hti->startsField[0] != 0)
    printf("\"%s\",", hti->startsField);
if (hti->endsSizesField[0] != 0)
    printf("\"%s\",", hti->endsSizesField);

printf("],\n\"bed\" : [\n");
for (t = b ; t ; t = t->next)
    {
    printf("[");
    if (hti->startField[0] != 0)
        printf("%d,", t->chromStart);
    if (hti->endField[0] != 0)
        printf("%d,", t->chromEnd);
    if (hti->nameField[0] != 0)
        printf("\"%s\",", t->name);
    if (hti->scoreField[0] != 0)
        printf("%d,", t->score);
    if (hti->strandField[0] != 0)
        printf("\'%c\',", t->strand[0]);
    if (hti->cdsStartField[0] != 0)
        printf("%d,", t->thickStart);
    if (hti->cdsEndField[0] != 0)
        printf("%d,", t->thickEnd);
    if (hti->countField[0] != 0)
        printf("%d,", t->blockCount);
    if (hti->startsField[0] != 0)
	{
	char *tmp = sqlSignedArrayToString(t->chromStarts, t->blockCount);
        printf("[%s],", tmp);
	freez(&tmp);
	}
    if (hti->endsSizesField[0] != 0)
	{
	char *tmp = sqlSignedArrayToString(t->blockSizes, t->blockCount);
        printf("[%s],", tmp);
	freez(&tmp);
	}
    printf("],\n");
    }
printf("]\n");
}

static void printBedOneColumn(struct bed *b, char *column)
// print out a column of bed records
{
struct bed *t;
if (!b)
    return;
printf(" \"%s\" : [", column);
for (t = b ; t ; t = t->next)
    {
    if (sameOk("chromStart", column))
        printf(" %d,", t->chromStart);
    else if (sameOk("chromEnd", column))
        printf(" %d,", t->chromEnd);
    else
        printf(" \"%s\",", sameOk(column, "chrom") ? t->chrom : "") ;
    }
printf("]\n");
}

void printBedByColumn(struct bed *b, struct hTableInfo *hti)
// print out a list of bed records by column
{
    printBedOneColumn(b, hti->startField);
    printBedOneColumn(b, hti->endField);
    printBedOneColumn(b, hti->nameField);
    printBedOneColumn(b, hti->scoreField);
    printBedOneColumn(b, hti->strandField);
    printBedOneColumn(b, hti->cdsStartField);
    printBedOneColumn(b, hti->cdsEndField);
    printBedOneColumn(b, hti->countField);
    printBedOneColumn(b, hti->startsField);
    printBedOneColumn(b, hti->endsSizesField);
}
