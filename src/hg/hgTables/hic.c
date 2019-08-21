/* hic - stuff to handle Hi-C stuff in table browser. */

/* Copyright (C) 2019 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "localmem.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "cart.h"
#include "web.h"
#include "bed.h"
#include "hdb.h"
#include "trackDb.h"
#include "obscure.h"
#include "hmmstats.h"
#include "correlate.h"
#include "asParse.h"
#include "bbiFile.h"
#include "bigBed.h"
#include "hubConnect.h"
#include "trackHub.h"
#include "hgTables.h"
#include "asFilter.h"
#include "xmlEscape.h"
#include "hic.h"
#include "interact.h"
#include "hgConfig.h"
#include "chromAlias.h"
#include "hicUi.h"

boolean isHicTable(char *table)
/* Return TRUE if table corresponds to a hic file. */
{
if (isHubTrack(table))
    {
    struct trackDb *tdb = hashFindVal(fullTableToTdbHash, table);
    return sameWord("hic", tdb->type);
    }
else
    return trackIsType(database, table, curTrack, "hic", ctLookupName);
}

char *hicFileName(char *table, struct sqlConnection *conn)
/* Return file name associated with hic.  This handles differences whether it's
 * a custom or built-in track.  Do a freeMem on returned string when done. */
{
char *fileName = bigFileNameFromCtOrHub(table, conn);
// NB: Not planning to do hic tracks via filenames in tables.  If this changes,
// add a check and a call to hicFileNameFromTable(), which will have to be written.
return fileName;
}

struct hTableInfo *hicToHti(char *table)
/* Get standard fields of hic into hti structure. */
{
struct hTableInfo *hti;
AllocVar(hti);
hti->rootName = cloneString(table);
hti->isPos= TRUE;
safecpy(hti->chromField, sizeof(hti->chromField), "chrom");
safecpy(hti->startField, sizeof(hti->startField), "chromStart");
safecpy(hti->endField, sizeof(hti->endField), "chromEnd");
safecpy(hti->scoreField, sizeof(hti->scoreField), "value");
hti->type = cloneString("hic");
return hti;
}

struct slName *hicGetFields()
/* Get fields of hic as simple name list.  We represent hic with an interact structure, so
 * this is really just an interact as object. */
{
struct asObject *as = interactAsObj();
return asColNames(as);
}

struct sqlFieldType *hicListFieldsAndTypes()
/* Get fields of hic as list of sqlFieldType (again, this is really just the list of interact fields. */
{
struct asObject *as = interactAsObj();
return sqlFieldTypesFromAs(as);
}

#define HIC_NUM_BUF_SIZE 4096

void hicRecordToRow(struct interact *sample, char *numBuf, char *row[INTERACT_NUM_COLS])
/* Convert samAlignment data structure to an array of strings, using numBuf to store
 * ascii versions of numbers temporarily */
{
char *numPt = numBuf;
char *numBufEnd = numBuf + HIC_NUM_BUF_SIZE;
row[0] = sample->chrom;
row[1] = numPt; numPt += sprintf(numPt, "%u", sample->chromStart); numPt += 1;
row[2] = numPt; numPt += sprintf(numPt, "%u", sample->chromEnd); numPt += 1;
row[3] = sample->name;
row[4] = numPt; numPt += sprintf(numPt, "%u", sample->score); numPt += 1;
row[5] = numPt; numPt += sprintf(numPt, "%f", sample->value); numPt += 1;
row[6] = sample->exp;
row[7] = numPt; numPt += sprintf(numPt, "%u", sample->color); numPt += 1;
row[8] = sample->sourceChrom;
row[9] = numPt; numPt += sprintf(numPt, "%u", sample->sourceStart); numPt += 1;
row[10] = numPt; numPt += sprintf(numPt, "%u", sample->sourceEnd); numPt += 1;
row[11] = sample->sourceName;
row[12] = sample->sourceStrand;
row[13] = sample->targetChrom;
row[14] = numPt; numPt += sprintf(numPt, "%u", sample->targetStart); numPt += 1;
row[15] = numPt; numPt += sprintf(numPt, "%u", sample->targetEnd); numPt += 1;
row[16] = sample->targetName;
row[17] = sample->targetStrand;
assert(numPt < numBufEnd);
}

int hicCompare(const void *elem1, const void *elem2)
/* Simple comparison function for sorting hic data in interact structures,
 * since it is not always returned in sorted order. */
{
const struct interact *first = *((struct interact **)elem1);
const struct interact *second = *((struct interact **)elem2);
int dif = strcmp(first->chrom, second->chrom);
if (dif == 0)
    dif = first->chromStart - second->chromStart;
return dif;
}

void hicTabOut(char *db, char *table, struct sqlConnection *conn, char *fields, FILE *f)
/* Print out selected fields from hic.  If fields is NULL, then print out all fields. */
{
if (f == NULL)
    f = stdout;

/* Convert comma separated list of fields to array. */
int fieldCount = chopByChar(fields, ',', NULL, 0);
char **fieldArray;
AllocArray(fieldArray, fieldCount);
chopByChar(fields, ',', fieldArray, fieldCount);

/* Get list of all fields in hic and turn it into a hash of column indexes keyed by
 * column name. */
struct hash *fieldHash = hashNew(0);
struct slName *field, *fieldList = hicGetFields();
int i;
for (field = fieldList, i=0; field != NULL; field = field->next, ++i)
    {
    hashAddInt(fieldHash, field->name, i);
    }

/* Create an array of column indexes corresponding to the selected field list. */
int *columnArray;
AllocArray(columnArray, fieldCount);
for (i=0; i<fieldCount; ++i)
    {
    columnArray[i] = hashIntVal(fieldHash, fieldArray[i]);
    }

/* Output row of labels */
fprintf(f, "#%s", fieldArray[0]);
for (i=1; i<fieldCount; ++i)
    fprintf(f, "\t%s", fieldArray[i]);
fprintf(f, "\n");

struct asObject *as = interactAsObj();
struct asFilter *filter = NULL;

if (anyFilter())
    {
    filter = asFilterFromCart(cart, db, table, as);
    if (filter)
        {
	fprintf(f, "# Filtering on %d columns\n", slCount(filter->columnList));
	}
    }

/* Loop through outputting each region */
struct region *region, *regionList = getRegions();

int maxOut = bigFileMaxOutput();

char *fileName = hicFileName(table, conn);
struct hicMeta *fileInfo;
char *errMsg = hicLoadHeader(fileName, &fileInfo, database);
if (errMsg != NULL)
    errAbort("%s", errMsg);

char *norm = hicUiFetchNormalization(cart, table, fileInfo);

for (region = regionList; region != NULL && (maxOut > 0); region = region->next)
    {
    struct interact *results = NULL, *result = NULL;
    int res = hicUiFetchResolutionAsInt(cart, table, fileInfo, region->end-region->start);
    char *errMsg = hicLoadData(fileInfo, res, norm, region->chrom, region->start, region->end,
            region->chrom, region->start, region->end, &results);
    if (errMsg != NULL)
        warn("%s", errMsg);
    slSort(&results, &hicCompare);
    char *row[INTERACT_NUM_COLS];
    char numBuf[HIC_NUM_BUF_SIZE];
    for (result = results; result != NULL && (maxOut > 0); result = result->next)
        {
	hicRecordToRow(result, numBuf, row);
	if (asFilterOnRow(filter, row))
	    {
	    int i;
	    fprintf(f, "%s", row[columnArray[0]]);
	    for (i=1; i<fieldCount; ++i)
		fprintf(f, "\t%s", row[columnArray[i]]);
	    fprintf(f, "\n");
	    maxOut --;
	    }
	}
    }
freeMem(fileName);

if (maxOut == 0)
    errAbort("Reached output limit of %d data values, please make region smaller,\n\tor set a higher output line limit with the filter settings.", bigFileMaxOutput());
/* Clean up and exit. */
hashFree(&fieldHash);
freeMem(fieldArray);
freeMem(columnArray);
}

static void addFilteredBedsOnRegion(char *fileName, struct region *region,
	char *table, struct asFilter *filter, struct lm *bedLm, struct bed **pBedList,
	struct hash *idHash, int *pMaxOut)
/* Add relevant beds in reverse order to pBedList */
{
struct hicMeta *fileInfo = NULL;
char *errMsg = hicLoadHeader(fileName, &fileInfo, database);

if (errMsg != NULL)
    {
    warn("%s", errMsg);
    }
else
    {
    int res = hicUiFetchResolutionAsInt(cart, table, fileInfo, region->end-region->start);
    char *norm = hicUiFetchNormalization(cart, table, fileInfo);

    struct interact *results = NULL, *result = NULL;
    errMsg = hicLoadData(fileInfo, res, norm, region->chrom, region->start, region->end,
            region->chrom, region->start, region->end, &results);
    if (errMsg != NULL)
        warn("%s", errMsg);
    slSort(&results, &hicCompare);
    char *row[INTERACT_NUM_COLS];
    char numBuf[HIC_NUM_BUF_SIZE];
    for (result=results; result != NULL; result = result->next)
        {
        hicRecordToRow(result, numBuf, row);
        if (asFilterOnRow(filter, row))
            {
            struct bed *bed;
            lmAllocVar(bedLm, bed);
            bed->chrom = lmCloneString(bedLm, result->chrom);
            bed->chromStart = result->chromStart;
            bed->chromEnd = result->chromEnd;
            bed->name = NULL;
            slAddHead(pBedList, bed);
            }
        (*pMaxOut)--;
        if (*pMaxOut <= 0)
            break;
        }
    }
}

struct bed *hicGetFilteredBedsOnRegions(struct sqlConnection *conn,
	char *db, char *table, struct region *regionList, struct lm *lm,
	int *retFieldCount)
/* Get list of beds from HIC, in all regions, that pass filtering. */
{
int maxOut = bigFileMaxOutput();
/* Figure out hic file name get column info and filter. */
struct asObject *as = interactAsObj();
struct asFilter *filter = asFilterFromCart(cart, db, table, as);
struct hash *idHash = identifierHash(db, table);

/* Get beds a region at a time. */
struct bed *bedList = NULL;
struct region *region;
for (region = regionList; region != NULL; region = region->next)
    {
    char *fileName = hicFileName(table, conn);
    addFilteredBedsOnRegion(fileName, region, table, filter, lm, &bedList, idHash, &maxOut);
    freeMem(fileName);
    if (maxOut <= 0)
	{
	errAbort("Reached output limit of %d data values, please make region smaller,\n"
	     "\tor set a higher output line limit with the filter settings.", bigFileMaxOutput());
	}
    }
slReverse(&bedList);
return bedList;
}


void showSchemaHic(char *table, struct trackDb *tdb)
/* Show schema on hic. */
{
struct sqlConnection *conn = NULL;
if (!trackHubDatabase(database))
    conn = hAllocConn(database);
char *fileName = hicFileName(table, conn);

struct asObject *as = interactAsObj();
hPrintf("<B>Database:</B> %s", database);
hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<B>Primary Table:</B> %s<br>", table);
hPrintf("<B>HI-C File:</B> %s", fileName);
hPrintf("<BR>\n");
hPrintf("<B>Format description:</B> %s<BR>", as->comment);
hPrintf("See the <A HREF=\"%s\" target=_blank>Interact Format Specification</A> for more details on "
        "the interact format, which is available for output here.  For details on the .hic format by "
        "the Aiden Lab, please see <a href=\"%s\" target=_blank>this page</a>.<BR>\n",
	"../goldenPath/help/interact.html", "https://www.aidenlab.org/documentation.html");

/* Put up table that describes fields. */
hTableStart();
hPrintf("<TR><TH>field</TH>");
hPrintf("<TH>description</TH> ");
puts("</TR>\n");
struct asColumn *col;
int colCount = 0;
for (col = as->columnList; col != NULL; col = col->next)
    {
    hPrintf("<TR><TD><TT>%s</TT></TD>", col->name);
    hPrintf("<TD>%s</TD></TR>", col->comment);
    ++colCount;
    }
hTableEnd();

/* Put up another section with sample rows. */
webNewSection("Sample Rows");
hTableStart();

/* Print field names as column headers for example */
hPrintf("<TR>");
int colIx = 0;
for (col = as->columnList; col != NULL; col = col->next)
    {
    hPrintf("<TH>%s</TH>", col->name);
    ++colIx;
    }
hPrintf("</TR>\n");

struct hicMeta *fileInfo = NULL;
char *errMsg = hicLoadHeader(fileName, &fileInfo, database);

if (errMsg != NULL)
    {
    warn("%s", errMsg);
    }
else
    {
    /* Print sample lines. */
    struct hash *chromAliasHash = chromAliasMakeLookupTable(database);
    char *chrName = fileInfo->chromNames[1]; // Skip 0, which is "All"
    struct chromAlias *a = hashFindVal(chromAliasHash, chrName);
    char *ucscChrName = NULL;
    if (a != NULL)
        ucscChrName = a->chrom;
    else
        ucscChrName = chrName;

    int maxRes = atoi(fileInfo->resolutions[0]);

    struct interact *sampleList = NULL, *sample = NULL;
    errMsg = hicLoadData(fileInfo, maxRes, "NONE", ucscChrName, 0, hChromSize(database, ucscChrName),
            ucscChrName, 0, hChromSize(database, ucscChrName), &sampleList);
    if (errMsg != NULL)
        warn("%s", errMsg);
    slSort(&sampleList, &hicCompare);
    char *row[INTERACT_NUM_COLS];
    char numBuf[HIC_NUM_BUF_SIZE];
    int count = 0;
    for (sample=sampleList; sample != NULL && count < 10; sample = sample->next, count++)
        {
        hicRecordToRow(sample, numBuf, row);
        hPrintf("<TR>");
        for (colIx=0; colIx<colCount; ++colIx)
            {
            hPrintf("<TD>");
            xmlEscapeStringToFile(row[colIx], stdout);
            hPrintf("</TD>");
            }
        hPrintf("</TR>\n");
        }
    }
hTableEnd();
printTrackHtml(tdb);

/* Clean up and go home. */
freeMem(fileName);
hFreeConn(&conn);
}

