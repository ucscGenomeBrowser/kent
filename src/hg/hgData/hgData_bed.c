/* hgData_bed - functions dealing with BED data. */
#include "common.h"
#include "hgData.h"
#include "sqlList.h"
#include "bed.h"
#include "hdb.h"

static char const rcsid[] = "$Id: hgData_bed.c,v 1.1.2.5 2009/01/13 15:48:53 mikep Exp $";

void printBedAsAnnoj(struct bed *b, struct hTableInfo *hti)
// print out rows of bed data formatted as AnnoJ nested model
{
struct bed *t;
int id = 0, exon = 0;
printf("{ success: true,\nmessage: 'Found %d models',\ndata: [\n", slCount(b));
for (t = b ; t ; t = t->next)
    {
    ++id;
    // name <- bed name, or 'id.N', can be duplicated?
    // strand <- '.' if strand unknown 
    // class <- table name
    // start <- zero-based
    //   nested id <- can be duplicated between top-level ids?
    //   start <- zero-based
    //   if no exons - should we have 1 block here, or none?
    //   
    printf("[");
    if (hti->endField[0] != 0)
	printf("'%s'", t->name);
    else
	printf("'id.%d'", id);
    printf(",'%c','%s',%d,%d,[\n", (hti->strandField[0] == 0 ? '.' : t->strand[0]), 
	hti->rootName, t->chromStart, t->chromEnd-t->chromStart); 
    if (hti->countField[0] != 0) // there are exons
	{
	if (hti->startsField[0] == 0 || hti->endsSizesField[0] == 0)
	    errAbort("blocks but no starts/ends");
    	for (exon = 0 ; exon < t->blockCount ; ++exon)
    	    {
	    printf("['e.%d','%s',%d,%d]%c\n", exon+1, "EXON", t->chromStarts[exon], t->blockSizes[exon], 
		(exon < t->blockCount-1 ? ',' : ' '));
	    }
	}
    printf("]\n]%c\n", (t->next ? ',' : ' '));
    }
printf("]\n}\n");
}

void printBed(int n, struct bed *b, char *db, char *track, char *type, char *chrom, int start, int end, struct hTableInfo *hti)
// print out rows of bed data, each row as a list of columns
{
struct bed *t;
printf("{ \"db\" : \"%s\",\n\"track\" : \"%s\",\n\"tableName\" : \"%s\",\n\"chrom\" : \"%s\",\n\"start\" : %d,\n\"end\" : %d,\n\"count\" : %d,\n\"hasCDS\" : %s,\n\"hasBlocks\" : %s,\n\"type\" : \"%s\",\n",
    db, track, hti->rootName, chrom, start, end, n,
    (hti->hasCDS ? "true" : "false"), (hti->hasBlocks ? "true" : "false"), type );
printf("\"bedColumns\" : [");
int c = 0; // number of columns, to decide when to print ',' between name:value pairs
if (hti->startField[0] != 0)
    printf("%c\"%s\"", c++ ? ',' : ' ', hti->startField);
if (hti->endField[0] != 0)
    printf("%c\"%s\"", c++ ? ',' : ' ', hti->endField);
if (hti->nameField[0] != 0)
    printf("%c\"%s\"", c++ ? ',' : ' ', hti->nameField);
if (hti->scoreField[0] != 0)
    printf("%c\"%s\"", c++ ? ',' : ' ', hti->scoreField);
if (hti->strandField[0] != 0)
    printf("%c\"%s\"", c++ ? ',' : ' ', hti->strandField);
if (hti->cdsStartField[0] != 0)
    printf("%c\"%s\"", c++ ? ',' : ' ', hti->cdsStartField);
if (hti->cdsEndField[0] != 0)
    printf("%c\"%s\"", c++ ? ',' : ' ', hti->cdsEndField);
if (hti->countField[0] != 0)
    printf("%c\"%s\"", c++ ? ',' : ' ', hti->countField);
if (hti->startsField[0] != 0)
    printf("%c\"%s\"", c++ ? ',' : ' ', hti->startsField);
if (hti->endsSizesField[0] != 0)
    printf("%c\"%s\"", c++ ? ',' : ' ', hti->endsSizesField);

printf("],\n\"bed\" : [\n");
for (t = b ; t ; t = t->next)
    {
    c = 0; // number of columns printed this row
    printf("[");
    if (hti->startField[0] != 0)
        printf("%c%d", c++ ? ',' : ' ', t->chromStart);
    if (hti->endField[0] != 0)
        printf("%c%d", c++ ? ',' : ' ', t->chromEnd);
    if (hti->nameField[0] != 0)
        printf("%c\"%s\"", c++ ? ',' : ' ', t->name);
    if (hti->scoreField[0] != 0)
        printf("%c%d", c++ ? ',' : ' ', t->score);
    if (hti->strandField[0] != 0)
        printf("%c\'%c\'", c++ ? ',' : ' ', t->strand[0]);
    if (hti->cdsStartField[0] != 0)
        printf("%c%d", c++ ? ',' : ' ', t->thickStart);
    if (hti->cdsEndField[0] != 0)
        printf("%c%d", c++ ? ',' : ' ', t->thickEnd);
    if (hti->countField[0] != 0)
        printf("%c%d", c++ ? ',' : ' ', t->blockCount);
    if (hti->startsField[0] != 0)
	{
	char *tmp = sqlSignedArrayToString(t->chromStarts, t->blockCount);
        printf("%c[%s]", c++ ? ',' : ' ', tmp);
	freez(&tmp);
	}
    if (hti->endsSizesField[0] != 0)
	{
	char *tmp = sqlSignedArrayToString(t->blockSizes, t->blockCount);
        printf("%c[%s]", c++ ? ',' : ' ', tmp);
	freez(&tmp);
	}
    printf("]%c\n", t->next ? ',' : ' ');
    }
printf("]\n}\n");
}

static void printBedOneColumn(struct bed *b, char *column)
// print out a column of bed records
{
struct bed *t;
if (!b)
    return;
printf("\"%s\" : [", column);
for (t = b ; t ; t = t->next)
    {
    if (sameOk("chromStart", column))
        printf("%d,", t->chromStart);
    else if (sameOk("chromEnd", column))
        printf("%d,", t->chromEnd);
    else
        printf("\"%s\",", sameOk(column, "chrom") ? t->chrom : "") ;
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
