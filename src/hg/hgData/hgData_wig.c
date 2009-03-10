/* hgData - simple RESTful interface to genome data. */
#include "common.h"
#include "hgData.h"
#include "wiggle.h"

static char const rcsid[] = "$Id: hgData_wig.c,v 1.1.2.1 2009/03/10 07:32:41 mikep Exp $";

static struct wiggleDataStream *wigOutRegion(char *genome, char *track, char *chrom, int start, int end, int maxOut, 
    int operations, int *count)
// operations: wigFetchNoOp || wigFetchStats || wigFetchRawStats || wigFetchBed || wigFetchDataArray || wigFetchAscii
//     doAscii = operations & wigFetchAscii;
//     doDataArray = operations & wigFetchDataArray;
//     doBed = operations & wigFetchBed;
//     doRawStats = operations & wigFetchRawStats;
//     doStats = (operations & wigFetchStats) || doRawStats;
//     doNoOp = operations & wigFetchNoOp;
{
int n;
boolean hasBin = FALSE;
/*struct bed *intersectBedList = NULL;
char *table2 = NULL;*/
char splitTableOrFileName[256];
struct wiggleDataStream *wds = wiggleDataStreamNew();
wds->setMaxOutput(wds, maxOut);
wds->setChromConstraint(wds, chrom);
wds->setPositionConstraint(wds, start, end);
if (!hFindSplitTable(genome, chrom, track, splitTableOrFileName, &hasBin))
    ERR_TRACK_NOT_FOUND(track, genome);
n = wds->getData(wds, genome, splitTableOrFileName, operations);
if (count)
    *count = n;
return wds;
}

//struct asciiDatum
/* a single instance of a wiggle data value (trying float here to save space */
//    unsigned chromStart;    /* Start position in chromosome, 0 relative */
//    float value;

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

//struct wigAsciiData
/* linked list of wiggle data in ascii form */
//    struct wigAsciiData *next;  /*      next in singly linked list      */
//    char *chrom;                /*      chrom name for this set of data */
//    unsigned span;              /*      span for this set of data       */
//    unsigned count;             /*      number of values in this block */
//    double dataRange;           /*      for resolution calculation */
//    struct asciiDatum *data;    /*      individual data items here */

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

void printWig(char *genome, char *track, char *chrom, int chromSize, int start, int end, char *strand)
// print wig records which intersect this start-end range
{
time_t modified = 0;
int count;
// options: wigFetchStats || wigFetchRawStats || wigFetchBed || wigFetchDataArray || wigFetchAscii
struct wiggleDataStream *wds = wigOutRegion(genome, track, chrom, start, end, MAX_WIG_LINES, wigFetchDataArray|wigFetchAscii|wigFetchBed, &count);
// ascii: asciiData->count
// bed:   slCount(wds->bed)
struct json_object *w = jsonWigAscii(count, wds, wds->ascii);
wiggleDataStreamFree(&wds);
okSendHeader(modified, TRACK_EXPIRES);
printf(json_object_to_json_string(w));
json_object_put(w);
}

