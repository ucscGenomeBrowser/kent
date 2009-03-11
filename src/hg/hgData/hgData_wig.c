/* hgData - simple RESTful interface to genome data. */
#include "common.h"
#include "hgData.h"

static char const rcsid[] = "$Id: hgData_wig.c,v 1.1.2.2 2009/03/11 09:38:17 mikep Exp $";


static struct json_object *jsonAddWigAsciiData(struct json_object *o, int count, struct asciiDatum *data)
{
int i;
struct json_object *a = json_object_new_array();
if (count>0)
    json_object_object_add(o, "chrom_start", json_object_new_int(data[0].chromStart));
json_object_object_add(o, "ascii_data", a);
for ( i = 0; i < count; ++i)
    {
/*    struct json_object *d = json_object_new_array();
    json_object_array_add(a, d);*/
    // add one asciiDatum: (chromStart,value) pair
    json_object_array_add(a, json_object_new_double(data[i].value));
    }
return o;
}

static struct json_object *jsonWigAscii(int count, struct wiggleDataStream *wds, struct wigAsciiData *ascii)
{
struct json_object *w = json_object_new_object();
struct json_object *a = json_object_new_array();
json_object_object_add(w, "count", json_object_new_int(count));

json_object_object_add(w, "ascii_count", json_object_new_int(slCount(wds->ascii)));
json_object_object_add(w, "bed_count", json_object_new_int(slCount(wds->bed)));
json_object_object_add(w, "stats_count", json_object_new_int(slCount(wds->stats)));
json_object_object_add(w, "array_count", json_object_new_int(slCount(wds->array)));
json_object_object_add(w, "rowsRead", json_object_new_int(wds->rowsRead));
json_object_object_add(w, "validPoints", json_object_new_int(wds->validPoints));
json_object_object_add(w, "noDataPoints", json_object_new_int(wds->noDataPoints));
json_object_object_add(w, "bytesRead", json_object_new_int(wds->bytesRead));
json_object_object_add(w, "bytesSkipped", json_object_new_int(wds->bytesSkipped));
json_object_object_add(w, "valuesMatched", json_object_new_int(wds->valuesMatched));
json_object_object_add(w, "totalBedElements", json_object_new_int(wds->totalBedElements));
json_object_object_add(w, "db", wds->db ? json_object_new_string(wds->db) : NULL);
json_object_object_add(w, "tblName", wds->tblName ? json_object_new_string(wds->tblName) : NULL);
json_object_object_add(w, "wibFile", wds->wibFile ? json_object_new_string(wds->wibFile) : NULL);
json_object_object_add(w, "ascii", wds->ascii ? json_object_new_int((int)((void *)wds->ascii-NULL)) : NULL);
json_object_object_add(w, "bed", wds->bed ? json_object_new_int((int)((void *)wds->bed-NULL)) : NULL);
json_object_object_add(w, "stats", wds->stats ? json_object_new_int((int)((void *)wds->stats-NULL)) : NULL);
json_object_object_add(w, "array", wds->array ? json_object_new_int((int)((void *)wds->array-NULL)) : NULL);

json_object_object_add(w, "data", a);
for ( ; ascii ; ascii = ascii->next)
    {
    struct json_object *d = json_object_new_object();
    json_object_array_add(a, d);
    json_object_object_add(d, "chrom", json_object_new_string(ascii->chrom));
    json_object_object_add(d, "span", json_object_new_int(ascii->span));
    json_object_object_add(d, "count", json_object_new_int(ascii->count));
    json_object_object_add(d, "data_range", json_object_new_double(ascii->dataRange));
    jsonAddWigAsciiData(d, ascii->count, ascii->data);
    }
return w;
}
//     /*  data return structures  */
//     struct wigAsciiData *ascii; /*      list of wiggle data values */
//     struct bed *bed;            /*      data in bed format      */
//     struct wiggleStats *stats;  /*      list of wiggle stats    */
//     struct wiggleArray *array;  /*      one big in-memory array of data */


void printWigCount(char *genome, char *track, char *chrom, int chromSize, int start, int end, char *strand)
// print count of wig records which intersect this start-end range
{
int count;
struct wiggleDataStream *wds = wigOutRegion(genome, track, chrom, start, end, MAX_WIG_LINES, wigFetchNoOp, &count);
wiggleDataStreamFree(&wds);
}

void printWig(char *genome, char *track, char *chrom, int chromSize, int start, int end, char *strand, int count, struct wiggleDataStream *wds)
// print wig records which intersect this start-end range
{
time_t modified = 0;
struct json_object *w = jsonWigAscii(count, wds, wds->ascii);
logTime("%s: built json struct", __func__);
okSendHeader(modified, TRACK_EXPIRES);
printf(json_object_to_json_string(w));
json_object_put(w);
logTime("%s(%s,%s,%s,chromSize=%d,%d,%d,%s) complete.", __func__,
    genome, track, chrom, chromSize, start, end, strand);
}

