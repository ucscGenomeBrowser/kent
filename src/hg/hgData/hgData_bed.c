/* hgData_bed - functions dealing with BED data. */
#include "common.h"
#include "hgData.h"
#include "sqlList.h"
#include "bed.h"
#include "hdb.h"

static char const rcsid[] = "$Id: hgData_bed.c,v 1.1.2.7 2009/02/03 10:36:57 mikep Exp $";


void printBedAsAnnoj(struct bed *b, struct hTableInfo *hti)
// print out rows of bed data formatted as AnnoJ flat model
{
struct bed *t;
int id = 0, exon = 0;
printf("{ success: true,\nmessage: \"Found %d models\",\ndata: ", slCount(b));
for (t = b ; t ; t = t->next)
    {
    char nameBuf[256];
    ++id;
    // name <- bed name, or 'id.N', can be duplicated?
    // strand <- '.' if strand unknown
    // class <- table name
    // start <- zero-based
    //   nested id <- can be duplicated between top-level ids?
    //   start <- zero-based
    //   if no exons - should we have 1 block here, or none?
    nameBuf[0] = 0;
    if (hti->endField[0] != 0)
        snprintf(nameBuf, sizeof(nameBuf), "\"%s\"", t->name);
    else
        snprintf(nameBuf, sizeof(nameBuf), "\"id.%d\"", id);
    // print line for parent
    printf("%c[null,%s,\"%c\",\"%s\",%d,%d]\n", (t==b ? '[' : ','), nameBuf, (hti->strandField[0] == 0 ? '.' : t->strand[0]),
        hti->rootName, t->chromStart, t->chromEnd-t->chromStart);
    // print line for each exon
    if (hti->countField[0] != 0) // there are exons
        {
        if (hti->startsField[0] == 0 || hti->endsSizesField[0] == 0)
            errAbort("blocks but no starts/ends");
        for (exon = 0 ; exon < t->blockCount ; ++exon)
            {
            printf(",[%s,\"e.%d\",\"%c\",\"%s\",%d,%d]\n", nameBuf, exon+1, (hti->strandField[0] == 0 ? '.' : t->strand[0]), "EXON", t->chromStart+t->chromStarts[exon], t->blockSizes[exon]);
            }
        }
    }
printf("]\n}\n");
}


void printBedAsAnnojNested(struct bed *b, struct hTableInfo *hti)
// print out rows of bed data formatted as AnnoJ nested model
{
struct bed *t;
int id = 0, exon = 0;
printf("{ success: true,\nmessage: \"Found %d models\",\ndata: \n", slCount(b));
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
    if (hti->endField[0] != 0)
	printf("%c[\"%s\"",    (t==b ? '[' : ','), t->name);
    else
	printf("%c[\"id.%d\"", (t==b ? '[' : ','), id);
    printf(",\"%c\",\"%s\",%d,%d,[\n", (hti->strandField[0] == 0 ? '.' : t->strand[0]), 
	hti->rootName, t->chromStart, t->chromEnd-t->chromStart); 
    if (0 && hti->countField[0] != 0) // there are exons
	{
	if (hti->startsField[0] == 0 || hti->endsSizesField[0] == 0)
	    errAbort("blocks but no starts/ends");
    	for (exon = 0 ; exon < t->blockCount ; ++exon)
    	    {
	    printf("[\"e.%d\",\"%s\",%d,%d]%c\n", exon+1, "EXON", t->chromStart+t->chromStarts[exon], t->blockSizes[exon], 
		(exon < t->blockCount-1 ? ',' : ' '));
	    }
	}
    printf("]]\n");
    }
printf("]\n}\n");
}

static struct json_object *jsonBedCount(char *genome, char *track, char *chrom, int start, int end, struct hTableInfo *hti, int n)
{
struct json_object *b = json_object_new_object();
struct json_object *props = json_object_new_object();
json_object_object_add(b, "properties", props);
json_object_object_add(b, "genome", json_object_new_string(genome));
json_object_object_add(b, "track", json_object_new_string(track));
json_object_object_add(b, "chrom", json_object_new_string(chrom));
json_object_object_add(b, "start", json_object_new_int(start));
json_object_object_add(b, "end", json_object_new_int(end));
json_object_object_add(b, "row_count", json_object_new_int(n));
if (hti)
    {
    json_object_object_add(props, "has_CDS", json_object_new_boolean(hti->hasCDS));
    json_object_object_add(props, "has_blocks", json_object_new_boolean(hti->hasBlocks));
    json_object_object_add(props, "type", json_object_new_string(hti->type));
    }
return b;
}

static struct json_object *jsonBed(struct hTableInfo *hti, struct bed *b)
// print out rows of bed data, each row as a list of columns
{
struct json_object *data = json_object_new_array();
struct json_object *arr;
struct bed *t;
int i;
for (t = b ; t ; t = t->next)
    {
    struct json_object *r = json_object_new_object();
    json_object_array_add(data, r); // add this row to the array
    if (hti->startField[0] != 0)
	json_object_object_add(r, "s", json_object_new_int(t->chromStart));
    if (hti->endField[0] != 0)
	json_object_object_add(r, "e", json_object_new_int(t->chromEnd));
    if (hti->nameField[0] != 0)
	json_object_object_add(r, "name", json_object_new_string(t->name));
    if (hti->scoreField[0] != 0)
	json_object_object_add(r, "score", json_object_new_int(t->score));
    if (hti->strandField[0] != 0)
	json_object_object_add(r, "strand", json_object_new_string(t->strand));
    if (hti->cdsStartField[0] != 0)
	json_object_object_add(r, "thickS", json_object_new_int(t->thickStart));
    if (hti->cdsEndField[0] != 0)
	json_object_object_add(r, "thickE", json_object_new_int(t->thickEnd));
    if (hti->countField[0] != 0)
	json_object_object_add(r, "blocks", json_object_new_int(t->blockCount));
    if (hti->startsField[0] != 0)
	{
	arr = json_object_new_array();
	json_object_object_add(r, "starts", arr);
	for (i=0 ; i< t->blockCount; ++i)
	    json_object_array_add(arr, json_object_new_int(t->chromStarts[i]));
	}
    if (hti->endsSizesField[0] != 0)
	{
	arr = json_object_new_array();
	json_object_object_add(r, "sizes", arr);
	for (i=0 ; i< t->blockCount; ++i)
	    json_object_array_add(arr, json_object_new_int(t->blockSizes[i]));
	}
    }
return data;
}

void printBedCount(char *genome, char *track, char *chrom, int start, int end, struct hTableInfo *hti, int n)
{
struct json_object *d = json_object_new_object();
json_object_object_add(d, "track", jsonBedCount(genome, track, chrom, start, end, hti, n));
printf(json_object_to_json_string(d));
json_object_put(d);
}

void printBed(char *genome, char *track, char *type, char *chrom, int start, int end, struct hTableInfo *hti, int n, struct bed *b)
{
struct json_object *d = json_object_new_object();
json_object_object_add(d, "track", jsonBedCount(genome, track, chrom, start, end, hti, n));
json_object_object_add(d, "data", jsonBed(hti, b));
printf(json_object_to_json_string(d));
json_object_put(d);
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
