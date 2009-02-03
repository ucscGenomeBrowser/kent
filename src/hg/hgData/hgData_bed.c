/* hgData_bed - functions dealing with BED data. */
#include "common.h"
#include "hgData.h"
#include "sqlList.h"
#include "bed.h"
#include "hdb.h"

static char const rcsid[] = "$Id: hgData_bed.c,v 1.1.2.8 2009/02/03 22:13:14 mikep Exp $";

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

static struct json_object *jsonBedAsAnnojFlat(struct hTableInfo *hti, struct bed *b)
// return json list of bed data as AnnoJ flat model
{
struct json_object *data = json_object_new_array();
struct json_object *row;
struct bed *t;
int id = 0, e;
for (t = b ; t ; t = t->next)
    {
    row = json_object_new_array();
    json_object_array_add(data, row);
    char nameBuf[1024];
    char exonBuf[1024];
    ++id;
    // name <- bed name, or 'id.N', can be duplicated?
    // strand <- '.' if strand unknown
    // class <- table name
    // start <- zero-based
    //   nested id <- can be duplicated between top-level ids?
    //   start <- zero-based
    //   if no exons - should we have 1 block here, or none?
    if (hti->endField[0] != 0)
        snprintf(nameBuf, sizeof(nameBuf), "%s", t->name);
    else
        snprintf(nameBuf, sizeof(nameBuf), "id.%d", id);
    // row for parent
    json_object_array_add(row, NULL);    // parent is null on first item
    json_object_array_add(row, json_object_new_string(nameBuf)); // name
    json_object_array_add(row, json_object_new_string(hti->strandField[0] == 0 ? "." : t->strand));
    json_object_array_add(row, json_object_new_string(hti->rootName)); // table name
    json_object_array_add(row, json_object_new_int(t->chromStart)); 
    json_object_array_add(row, json_object_new_int(t->chromEnd-t->chromStart));
    // print line for each exon
    if (hti->countField[0] != 0) // there are exons
        {
        if (hti->startsField[0] == 0 || hti->endsSizesField[0] == 0)
            errAbort("blocks but no starts/ends");
        for (e = 0 ; e < t->blockCount ; ++e)
            {
	    row = json_object_new_array();
	    json_object_array_add(data, row);
	    json_object_array_add(row, json_object_new_string(nameBuf)); // parent
	    snprintf(exonBuf, sizeof(exonBuf), "e.%d", e+1);
	    json_object_array_add(row, json_object_new_string(exonBuf)); // exon name
	    json_object_array_add(row, json_object_new_string(hti->strandField[0] == 0 ? "." : t->strand));
	    json_object_array_add(row, json_object_new_string("EXON")); // need to fix to be utr5, utr3, cds, or exon
	    json_object_array_add(row, json_object_new_int(t->chromStart + t->chromStarts[e])); 
	    json_object_array_add(row, json_object_new_int(t->blockSizes[e]));
            }
        }
    }
return data;
}

static struct json_object *jsonBedAsAnnojNested(struct hTableInfo *hti, struct bed *b)
// return json list of bed data as AnnoJ nested model
{
struct json_object *data = json_object_new_array();
struct json_object *row;
struct json_object *exonList;
struct json_object *exon;
struct bed *t;
int id = 0, e;
for (t = b ; t ; t = t->next)
    {
    // add a row to the data
    row = json_object_new_array();
    json_object_array_add(data, row);
    char nameBuf[1024];
    char exonBuf[1024];
    ++id;
    // name <- bed name, or 'id.N', can be duplicated?
    // strand <- '.' if strand unknown
    // class <- table name
    // start <- zero-based
    //   nested id <- can be duplicated between top-level ids?
    //   start <- zero-based
    //   if no exons - should we have 1 block here, or none?
    if (hti->endField[0] != 0)
        snprintf(nameBuf, sizeof(nameBuf), "%s", t->name);
    else
        snprintf(nameBuf, sizeof(nameBuf), "id.%d", id);
    // row for parent
    json_object_array_add(row, json_object_new_string(nameBuf)); // name
    json_object_array_add(row, json_object_new_string(hti->strandField[0] == 0 ? "." : t->strand));
    json_object_array_add(row, json_object_new_string(hti->rootName)); // table name
    json_object_array_add(row, json_object_new_int(t->chromStart));
    json_object_array_add(row, json_object_new_int(t->chromEnd-t->chromStart));
    // add an empty list of exons to row
    exonList = json_object_new_array();
    json_object_array_add(row, exonList);
    if (hti->countField[0] != 0) // there are exons
        {
        if (hti->startsField[0] == 0 || hti->endsSizesField[0] == 0)
            errAbort("blocks but no starts/ends");
        for (e = 0 ; e < t->blockCount ; ++e)
            {
	    // add each exon to exonList
            exon = json_object_new_array();
	    json_object_array_add(exonList, exon);
            snprintf(exonBuf, sizeof(exonBuf), "e.%d", e+1);
            json_object_array_add(exon, json_object_new_string(exonBuf)); // exon name
            json_object_array_add(exon, json_object_new_string("EXON")); // need to fix to be utr5, utr3, cds, or exon
            json_object_array_add(exon, json_object_new_int(t->chromStart + t->chromStarts[e]));
            json_object_array_add(exon, json_object_new_int(t->blockSizes[e]));
            }
        }
    }
return data;
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

void printBed(char *genome, char *track, char *type, char *chrom, int start, int end, struct hTableInfo *hti, int n, struct bed *b, char *format)
{
struct json_object *d = json_object_new_object();
json_object_object_add(d, "track", jsonBedCount(genome, track, chrom, start, end, hti, n));
if (format==NULL)
    json_object_object_add(d, "bed", jsonBed(hti, b));
else if (sameOk(format, ANNOJ_FLAT_FMT))
    json_object_object_add(d, format, jsonBedAsAnnojFlat(hti, b));
else if (sameOk(format, ANNOJ_NESTED_FMT))
    json_object_object_add(d, format, jsonBedAsAnnojNested(hti, b));
else
    ERR_BAD_FORMAT(format);
printf(json_object_to_json_string(d));
json_object_put(d);
}

