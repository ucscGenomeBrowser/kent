/* cdwWebBrowse - Browse CIRM data warehouse.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "cheapcgi.h"
#include "sqlSanity.h"
#include "trix.h"
#include "htmshell.h"
#include "fieldedTable.h"
#include "portable.h"
#include "paraFetch.h"
#include "tagStorm.h"
#include "rql.h"
#include "intValTree.h"
#include "cart.h"
#include "cartDb.h"
#include "jksql.h"
#include "cdw.h"
#include "cdwLib.h"
#include "cdwValid.h"
#include "hui.h"
#include "hgConfig.h"
#include "hgColors.h"
#include "rainbow.h"
#include "web.h"
#include "tablesTables.h"
#include "jsHelper.h"
#include "wikiLink.h"
#include "cdwFlowCharts.h"
#include "cdwStep.h"
#include "facetField.h"

/* Global vars */
struct cart *cart;	// User variables saved from click to click
struct hash *oldVars;	// Previous cart, before current round of CGI vars folded in
struct cdwUser *user;	// Our logged in user if any
static char *accessibleFilesToken = NULL;  // Token for file access if any
boolean isPublicSite = FALSE;


char *excludeVars[] = {"cdwCommand", "submit", "DownloadFormat", NULL};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwWebBrowse is a cgi script not meant to be run from command line.\n"
  );
}

void printHash(char *label, struct hash *hash)
/* Print out keys in hash alphabetically. */
{
struct hashEl *list, *el;
list = hashElListHash(hash);
slSort(&list, hashElCmp);
printf("%s:\n", label);
for (el = list; el != NULL; el = el->next)
    printf("    %s\n", el->name);
hashElFreeList(&list);
}

// fields/columns of the browse file table
char *fileTableFields = NULL;
char *visibleFacetFields = NULL;
#define FILEFIELDS "file_name,file_size,ucsc_db"
#define FILEFACETFIELDS "lab,data_set_id,species,assay,format,output,organ_anatomical_name,biosample_cell_type,read_size,sample_label"

struct dyString *printPopularTags(struct hash *hash, int maxSize)
/* Get all hash elements, sorted by count, and print all the ones that fit */
{
maxSize -= 3;  // Leave room for ...
struct dyString *dy = dyStringNew(0);

struct hashEl *hel, *helList = hashElListHash(hash);
slSort(&helList, hashElCmpIntValDesc);
for (hel = helList; hel != NULL; hel = hel->next)
    {
    int oldSize = dy->stringSize;
    if (oldSize != 0)
        dyStringAppend(dy, ", ");
    dyStringPrintf(dy, "%s (%d)", hel->name, ptToInt(hel->val));
    if (dy->stringSize >= maxSize)
        {
	dy->string[oldSize] = 0;
	dy->stringSize = oldSize;
	dyStringAppend(dy, "...");
	break;
	}
    }
hashElFreeList(&helList);
return dy;
}

static long long sumCounts(struct hash *hash)
/* Figuring hash is integer valued, return sum of all vals in hash */
{
long long total = 0;
struct hashEl *hel, *helList = hashElListHash(hash);
for (hel = helList; hel != NULL; hel = hel->next)
    {
    int val = ptToInt(hel->val);
    total += val;
    }
hashElFreeList(&helList);
return total;
}

int labCount(struct tagStorm *tags)
/* Return number of different labs in tags */
{
struct hash *hash = tagStormCountTagVals(tags, "lab", "accession");
int count = hash->elCount;
hashFree(&hash);
return count;
}


void wrapFileName(struct fieldedTable *table, struct fieldedRow *row, 
    char *field, char *val, char *shortVal, void *context)
/* Write out wrapper that links us to metadata display */
{
printf("<A HREF=\"../cgi-bin/cdwWebBrowse?cdwCommand=oneFile&cdwFileTag=%s&cdwFileVal=%s&%s\">",
    field, val, cartSidUrlString(cart));
printf("%s</A>", shortVal);
}


void wrapTagField(struct fieldedTable *table, struct fieldedRow *row, 
    char *field, char *val, char *shortVal, void *context)
/* Write out wrapper that links us to something nice */
{
printf("<A HREF=\"../cgi-bin/cdwWebBrowse?cdwCommand=oneTag&cdwTagName=%s&%s\">",
    val, cartSidUrlString(cart));
printf("%s</A>", shortVal);
}

void wrapTagValueInFiles(struct fieldedTable *table, struct fieldedRow *row, 
    char *field, char *val, char *shortVal, void *context)
/* Write out wrapper that links us to something nice */
{
printf("<A HREF=\"../cgi-bin/cdwWebBrowse?cdwCommand=browseFiles&%s&",
    cartSidUrlString(cart));
char query[2*PATH_LEN];
safef(query, sizeof(query), "%s = '%s'", field, val);
char *escapedQuery = cgiEncode(query);
printf("%s=%s&cdwBrowseFiles_page=1", "cdwFile_filter", escapedQuery);
freez(&escapedQuery);
printf("\">%s</A>", shortVal);
}

static char *mustFindFieldInRow(char *field, struct slName *fieldList, char **row)
/* Assuming field is in list, which is ordered same as row, return row cell
 * corrsepondint to field */
{
int fieldIx = 0;
struct slName *el;
for (el = fieldList; el != NULL; el = el->next)
    {
    if (sameString(el->name, field))
        {
	return row[fieldIx];
	}
    ++fieldIx;
    }
errAbort("Couldn't find field %s in row", field);
return NULL;
}

char *tagDescription(char *tag)
/* Return tag description given tag name. */
{
char *unparsed[] = {
#include "tagDescriptions.h"
    };
int unparsedCount = ArraySize(unparsed);

int i;
for (i=0; i<unparsedCount; ++i)
    {
    char *s = unparsed[i];
    if (startsWithWord(tag, s))
        {
	int tagLen = strlen(tag);
	s = skipLeadingSpaces(s + tagLen);
	return s;
	}
    }
return NULL;
}

void doOneTag(struct sqlConnection *conn)
/* Put up information on one tag */
{
/* Get all tags on user accessible files */
struct tagStorm *tags = cdwUserTagStorm(conn, user);

/* Look up which tag we're working on from cart, and make summary info hash and report stats */
char *tag = cartString(cart, "cdwTagName");

/* Print out tag description */
char *description = tagDescription(tag);
if (description != NULL)
    printf("<B>%s tag description</B>: %s<BR>\n", tag, description);

/* Print out some summary stats */
struct hash *hash = tagStormCountTagVals(tags, tag, "accession");
printf("The <B>%s</B> tag has %d distinct values and is used on %lld files. ", 
    tag, hash->elCount, sumCounts(hash));

/* Initially sort from most popular to least popular */
struct hashEl *hel, *helList = hashElListHash(hash);
slSort(&helList, hashElCmpIntValDesc);

/* Create fielded table containing tag values */
char *labels[] = {"files", tag};
int fieldCount = ArraySize(labels);
struct fieldedTable *table = fieldedTableNew("Tag Values", labels, fieldCount);
for (hel = helList; hel != NULL; hel = hel->next)
    {
    char numBuf[16];
    safef(numBuf, sizeof(numBuf), "%d", ptToInt(hel->val));
    char *row[2] = {numBuf, hel->name};
    fieldedTableAdd(table, row, fieldCount, 0);
    }

/* Draw sortable table */
char returnUrl[PATH_LEN*2];
safef(returnUrl, sizeof(returnUrl), "../cgi-bin/cdwWebBrowse?cdwCommand=oneTag&cdwTagName=%s&%s",
    tag, cartSidUrlString(cart) );
struct hash *outputWrappers = hashNew(0);
hashAdd(outputWrappers, tag, wrapTagValueInFiles);
webSortableFieldedTable(cart, table, returnUrl, "cdwOneTag", 0, outputWrappers, NULL);
fieldedTableFree(&table);
}

void generateTableRow(struct slName *list,char **row, char *idTag , char *idVal) 
{
struct slName *el; 
static char *outputFields[] = {"tag", "value"};
struct fieldedTable *table = fieldedTableNew("File Tags", outputFields,ArraySize(outputFields));
int fieldIx = 0;
for (el = list; el != NULL; el = el->next)
    {
    char *outRow[2];
    char *val = row[fieldIx];
    if (val != NULL)
	{
	outRow[0] = el->name;
	outRow[1] = row[fieldIx];

	// add a link to the accession row
	if (sameWord(el->name, "accession"))
	    {
	    char link[1024];
	    safef(link, sizeof(link), "%s <a href='cdwGetFile?acc=%s'>download</a>", outRow[1], outRow[1]);
	    outRow[1] = cloneString(link);
	    }
	fieldedTableAdd(table, outRow, 2, fieldIx);
	}
    ++fieldIx;
    }
char returnUrl[PATH_LEN*2];
safef(returnUrl, sizeof(returnUrl), 
    "../cgi-bin/cdwWebBrowse?cdwCommand=oneFile&cdwFileTag=%s&cdwFileVal=%s&%s",
    idTag, idVal, cartSidUrlString(cart) );
struct hash *outputWrappers = hashNew(0);
hashAdd(outputWrappers, "tag", wrapTagField);
webSortableFieldedTable(cart, table, returnUrl, "cdwOneFile", 0, outputWrappers, NULL);
fieldedTableFree(&table);
}

char *getCdwSetting(char *setting, char *deflt)
/* Get string cdw.<setting> */
{
char cdwSetting[1024];
safef(cdwSetting, sizeof cdwSetting, "cdw.%s", setting);
return cfgOptionDefault(cdwSetting, deflt);
} 

char *getCdwTableSetting(char *setting)
/* Get string cdw.<setting>
 * Allows us to use non-default settings for tables */
{
return getCdwSetting(setting, setting); // table name is its own default
} 

void doFileFlowchart(struct sqlConnection *conn)
/* Put up a page with info on one file */
{
char *idTag = cartUsualString(cart, "cdwFileTag", "accession");
char *idVal = cartString(cart, "cdwFileVal");
char query[512];
sqlSafef(query, sizeof(query), "select * from %s where %s='%s'", getCdwTableSetting("cdwFileTags"), idTag, idVal);
struct sqlResult *sr = sqlGetResult(conn, query);
struct slName  *list = sqlResultFieldList(sr);
char **row;
struct dyString *dy = dyStringNew(1024); 
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *fileId = mustFindFieldInRow("file_id", list, row);
    printf("Click on a box in the flow chart to navigate to that file."); 
    dy = makeCdwFlowchart(sqlSigned(fileId), cart);
    printf("<a href='cdwWebBrowse?cdwCommand=oneFile");
    printf("&%s", cartSidUrlString(cart));
    printf("&cdwFileTag=%s", idTag);
    printf("&cdwFileVal=%s", idVal);
    printf("'>Remove flow chart</a>"); 
    generateTableRow(list, row, idTag, idVal); 
    }
jsInline(dy->string);
dyStringFree(&dy); 
sqlFreeResult(&sr);
}

void doOneFile(struct sqlConnection *conn)
/* Put up a page with info on one file */
{
struct sqlConnection *conn2 = cdwConnect();
char *idTag = cartUsualString(cart, "cdwFileTag", "accession");
char *idVal = cartString(cart, "cdwFileVal");
char query[512];
sqlSafef(query, sizeof(query), "select * from %s where %s='%s'", getCdwTableSetting("cdwFileTags"), idTag, idVal);
struct sqlResult *sr = sqlGetResult(conn, query);
struct slName *list = sqlResultFieldList(sr);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *fileName = mustFindFieldInRow("file_name", list, row);
    char *fileSize = mustFindFieldInRow("file_size", list, row);
    char *format = mustFindFieldInRow("format", list, row);
    char *fileId = mustFindFieldInRow("file_id", list, row);
    long long size = sqlLongLongInList(&fileSize);
    printf("Tags associated with %s a %s format file of size ", fileName, format);
    printLongWithCommas(stdout, size);
     
    printf("<BR>\n");
    /* Figure out number of file inputs and outputs, and put up link to flowcharts */
    sqlSafef(query, sizeof(query), "select * from cdwStepIn where fileId = %s", fileId);
    struct cdwStepIn *cSI = cdwStepInLoadByQuery(conn2, query), *stepIter;
    int outFiles = 0, stepRunId;  
    for (stepIter = cSI; stepIter != NULL; stepIter=stepIter->next)
	{
	sqlSafef(query, sizeof(query), "select count(*) from cdwStepOut where stepRunId = %i", stepIter->stepRunId);
	outFiles += sqlQuickNum(conn2, query);
	}

    sqlSafef(query, sizeof(query), "select stepRunId from cdwStepOut where fileId = %s", fileId);
    stepRunId = sqlQuickNum(conn2, query);
    sqlSafef(query, sizeof(query), "select count(*) from cdwStepIn where stepRunId = %i", stepRunId);
    int inFiles = sqlQuickNum(conn2, query);
    if (inFiles > 0 || outFiles > 0)
	{
	printf("File relationships: %d inputs, %d outputs ", inFiles, outFiles);
	printf("<a href='cdwWebBrowse?cdwCommand=doFileFlowchart");
	printf("&%s", cartSidUrlString(cart));
	printf("&cdwFileTag=%s", idTag);
	printf("&cdwFileVal=%s", idVal);
	printf("'>flow chart</a>"); 
	} 
    generateTableRow(list, row, idTag, idVal); 
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn2);
}

struct dyString *customTextForFile(struct sqlConnection *conn, struct cdwTrackViz *viz)
/* Create custom track text */
{
struct dyString *dy = dyStringNew(0);
dyStringPrintf(dy, "track name=\"%s\" ", viz->shortLabel);
dyStringPrintf(dy, "description=\"%s\" ", viz->longLabel);
//char *host = hHttpHost();
dyStringPrintf(dy, "bigDataUrl=http://localhost/cgi-bin/cdwGetFile?acc=%s", viz->shortLabel);
if (accessibleFilesToken != NULL)
    dyStringPrintf(dy, "&token=%s", accessibleFilesToken);
dyStringPrintf(dy, " ");
    
char *indexExt = NULL;
if (sameWord(viz->type, "bam"))
    indexExt = ".bai";
else if (sameWord(viz->type, "vcfTabix"))
    indexExt = ".tbi";

if (indexExt != NULL)
    {
    dyStringPrintf(dy, "bigDataIndex=http://localhost/cgi-bin/cdwGetFile?addExt=%s&acc=%s", indexExt, viz->shortLabel);
    if (accessibleFilesToken != NULL)
        dyStringPrintf(dy, "&token=%s", accessibleFilesToken);
    }

dyStringPrintf(dy, " type=%s", viz->type);
return dy;
}

struct cdwTrackViz *cdwTrackVizFromFileId(struct sqlConnection *conn, long long fileId)
/* Return cdwTrackViz if any associated with file ID */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from cdwTrackViz where fileId=%lld", fileId);
return cdwTrackVizLoadByQuery(conn, query);
}


void wrapFileVis(struct sqlConnection *conn, char *acc, char *unwrapped)
/* Wrap hyperlink link to file around unwrapped text.  Link goes to file in vf. */
{
char *host = hHttpHost();
printf("<A HREF=\"");
printf("http://%s/cgi-bin/cdwGetFile?acc=%s", host, acc);
if (accessibleFilesToken != NULL)
    printf("&token=%s", accessibleFilesToken);
printf("\">");
printf("%s</A>", unwrapped);
}

boolean wrapTrackVis(struct sqlConnection *conn, struct cdwValidFile *vf, char *unwrapped)
/* Attempt to wrap genome browser link around unwrapped text.  Link goes to file in vf. */
{
if (vf == NULL)
    return FALSE;
struct cdwTrackViz *viz = cdwTrackVizFromFileId(conn, vf->fileId);
if (viz == NULL)
    return FALSE;
struct dyString *track = customTextForFile(conn, viz);
char *encoded = cgiEncode(track->string);
printf("<A TARGET=_BLANK HREF=\"../cgi-bin/hgTracks");
printf("?%s", cartSidUrlString(cart));
printf("&db=%s", vf->ucscDb);
printf("&hgt.customText=");
printf("%s", encoded);
printf("\">");	       // Finish HREF quote and A tag
printf("%s</A>", unwrapped);
freez(&encoded);
dyStringFree(&track);
return TRUE;
}


void wrapTrackNearFileName(struct fieldedTable *table, struct fieldedRow *row, char *tag, char *val,
    char *shortVal, void *context)
/* Construct wrapper to UCSC if row actually is a track */
{
struct sqlConnection *conn = context;
int fileNameIx = stringArrayIx("file_name", table->fields, table->fieldCount);
boolean printed = FALSE;
if (fileNameIx >= 0)
    {
    char *fileName = row->row[fileNameIx];
    char acc[FILENAME_LEN];
    safef(acc, sizeof(acc), "%s", fileName);
    char *dot = strchr(acc, '.');
    if (dot != NULL)
        *dot = 0;
    struct cdwValidFile *vf = cdwValidFileFromLicensePlate(conn, acc);
    struct cdwFile *ef = cdwFileFromId(conn, vf->fileId);
    if (cdwCheckAccess(conn, ef, user, cdwAccessRead)) printed = wrapTrackVis(conn, vf, shortVal);
    }
if (!printed)
    printf("%s", shortVal);
}

void wrapTrackNearAccession(struct fieldedTable *table, struct fieldedRow *row, 
    char *tag, char *val, char *shortVal, void *context)
/* Construct wrapper that can link to Genome Browser if any field in table
 * is an accession */
{
struct sqlConnection *conn = context;
int accIx = stringArrayIx("accession", table->fields, table->fieldCount);
boolean printed = FALSE;
if (accIx >= 0)
    {
    char *acc = row->row[accIx];
    if (acc != NULL)
	{
	struct cdwValidFile *vf = cdwValidFileFromLicensePlate(conn, acc);
	struct cdwFile *ef = cdwFileFromId(conn, vf->fileId);
	if (cdwCheckAccess(conn, ef, user, cdwAccessRead))
	    printed = wrapTrackVis(conn, vf, shortVal);
	}
    }
if (!printed)
    printf("%s", shortVal);
}

boolean isWebBrowsableFormat(char *format)
/* Return TRUE if it's one of the web-browseable formats */
{
char *formats[] = {"html", "jpg", "pdf", "png", "text", };
return stringArrayIx(format, formats, ArraySize(formats)) >= 0;
}

void wrapFormat(struct fieldedTable *table, struct fieldedRow *row, 
    char *field, char *val, char *shortVal, void *context)
/* Write out wrapper that links us to something nice */
{
struct sqlConnection *conn = context;
char *format = val;
if (isWebBrowsableFormat(format))
     {
     /* Get file name out of table */
     int fileNameIx = stringArrayIx("file_name", table->fields, table->fieldCount);
     if (fileNameIx < 0)
        errAbort("Expecting a file_name in this table");
     char *fileName = row->row[fileNameIx];

     /* Convert file to accession by chopping off at first dot */
     char *acc = cloneString(fileName);
     char *dot = strchr(acc, '.');
     if (dot != NULL)
         *dot = 0;

     struct cdwValidFile *vf = cdwValidFileFromLicensePlate(conn, acc);
     struct cdwFile *ef = cdwFileFromId(conn, vf->fileId);
     if (cdwCheckAccess(conn, ef, user, cdwAccessRead))
	   wrapFileVis(conn, acc, shortVal);
     freez(&acc);
     }
else
     printf("%s", format);
}

void wrapMetaNearAccession(struct fieldedTable *table, struct fieldedRow *row, 
    char *field, char *val, char *shortVal, void *context)
/* Write out wrapper on a column that looks for accession in same table and uses
 * that to link us to oneFile display. */
{
struct sqlConnection *conn = context;
int accIx = stringArrayIx("accession", table->fields, table->fieldCount);
boolean wrapped = FALSE;
if (accIx >= 0)
    {
    char *acc = row->row[accIx];
    if (acc != NULL)
        {
	struct cdwValidFile *vf = cdwValidFileFromLicensePlate(conn, acc);
	if (vf != NULL)
	    {
	    wrapped = TRUE;
	    printf("<A HREF=\"../cgi-bin/cdwWebBrowse?cdwCommand=oneFile&cdwFileTag=accession&cdwFileVal=%s&%s\">",
		acc, cartSidUrlString(cart));
	    printf("%s</A>", shortVal);
	    }
	}
    }

if (!wrapped)
    printf("%s", shortVal);
}

void wrapExternalUrl(struct fieldedTable *table, struct fieldedRow *row, 
    char *field, char *val, char *shortVal, void *context)
/* Attempt to wrap genome browser link around unwrapped text.  Link goes to file in vf. */
{
printf("<A HREF=\"%s\" target=\"_blank\">%s</A>", val, shortVal);
}

static void rSumLocalMatching(struct tagStanza *list, char *field, int *pSum)
/* Recurse through tree adding matches to *pSum */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    if (tagFindLocalVal(stanza, field))
        *pSum += 1;
    if (stanza->children != NULL)
         rSumLocalMatching(stanza->children, field, pSum);
    }
}

int tagStormCountStanzasWithLocal(struct tagStorm *tags, char *localField)
/* Return count of all stanzas that include locally a given field */
{
int sum = 0;
rSumLocalMatching(tags->forest, localField, &sum);
return sum;
}

int hashElCmpIntValDescNameAsc(const void *va, const void *vb)
/* Compare two hashEl from a hashInt type hash, with highest integer values
 * comingFirst. */
{
struct hashEl *a = *((struct hashEl **)va);
struct hashEl *b = *((struct hashEl **)vb);
int diff = b->val - a->val;
if (diff == 0)
    diff = strcmp(b->name, a->name);
return diff;
}

struct suggestBuilder
/* A structure to help build a list of suggestions for each file */
    {
    struct suggestBuilder *next;
    char *name;		/* Field name */
    struct hash *hash;  /* Keyed by field values, values are # of times seen */
    };

struct hash *accessibleSuggestHash(struct sqlConnection *conn, char *fields, 
    struct cdwFile *efList)
/* Create hash keyed by field name and with values the distinct values of this
 * field.  Only do this on fields where it looks like suggest would be useful. */
{
struct hash *suggestHash = hashNew(0);
int totalFiles = slCount(efList);

/* Make up list of helper structures */
struct slName *name, *nameList = slNameListFromComma(fields);
struct suggestBuilder *field, *fieldList = NULL;
for (name = nameList; name != NULL; name = name->next)
    {
    AllocVar(field);
    field->name = name->name;
    field->hash = hashNew(0);
    slAddHead(&fieldList, field);
    }
slReverse(&fieldList);

/* Build up sql query to fetch all our fields */
struct dyString *query = dyStringNew(0);
sqlDyStringPrintf(query, "select ");
for (field = fieldList; field != NULL; field = field->next)
    {
    if (field != fieldList) // not first one
	sqlDyStringPrintf(query, ",");
    sqlDyStringPrintf(query, "%s", field->name);
    }

/* Put where on it to limit it to accessible files */
sqlDyStringPrintf(query, " from %s where file_id in (", getCdwTableSetting("cdwFileTags"));

struct cdwFile *ef;
for (ef = efList; ef != NULL; ef = ef->next)
    {
    if (ef != efList) // not first one
	sqlDyStringPrintf(query, ",");
    sqlDyStringPrintf(query, "%u", ef->id);
    }
sqlDyStringPrintf(query, ")");

struct sqlResult *sr = sqlGetResult(conn, query->string);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    int fieldIx = 0;
    for (field = fieldList; field != NULL; field = field->next, ++fieldIx)
        {
	char *val = row[fieldIx];
	if (val != NULL)
	    hashIncInt(field->hash, val);
	}
    }
sqlFreeResult(&sr);

/* Loop through fields making suggestion hash entries where appropriate */
for (field = fieldList; field != NULL; field = field->next)
    {
    struct hash *valHash = field->hash;
    if (valHash->elCount < 20 || valHash->elCount < totalFiles/3)
        {
	struct hashEl *hel, *helList = hashElListHash(valHash);
	slSort(&helList, hashElCmpIntValDescNameAsc);
	struct slName *valList = NULL;
	int limit = 200;
	for (hel = helList ; hel != NULL; hel = hel->next)
	    {
	    if (--limit < 0)
	        break;
	    slNameAddHead(&valList, hel->name);
	    }
	slReverse(&valList);
	hashAdd(suggestHash, field->name, valList);
	}
    hashFree(&field->hash);
    }
slFreeList(&fieldList);
slFreeList(&nameList);
dyStringFree(&query);
return suggestHash;
}


static struct hash *sqlHashFields(struct sqlConnection *conn, char *table)
/* Return a hash containing all fields in table */
{
struct slName *field, *fieldList = sqlListFields(conn, table);
struct hash *hash = hashNew(7);
for (field = fieldList; field != NULL; field = field->next)
    hashAdd(hash, field->name, NULL);
slFreeList(&fieldList);
return hash;
}

static char *filterFieldsToJustThoseInTable(struct sqlConnection *conn, char *fields, char *table)
/* Return subset of all fields just containing those that exist in table */
{
struct hash *hash = sqlHashFields(conn, table);
struct slName *name, *nameList = slNameListFromComma(fields);
struct dyString *dy = dyStringNew(strlen(fields));
char *separator = "";
for (name = nameList; name != NULL; name = name->next)
    {
    char *s = name->name;
    if (hashLookup(hash, s))
	{
        dyStringPrintf(dy, "%s%s", separator, s);
	separator = ",";
	}
    }
hashFree(&hash);
slFreeList(&nameList);
return dyStringCannibalize(&dy);
}

void searchFilesWithAccess(struct sqlConnection *conn, char *searchString, char *allFields, 
    char* initialWhere, struct cdwFile **retList, struct dyString **retWhere, char **retFields,
    boolean securityColumnsInTable)
{
/* Get list of files that we are authorized to see and that match searchString in the trix file
 * Returns: retList of matching files, retWhere with sql where expression for these files, retFields
 * If nothing to see, retList is NULL
 * */
char *fields = filterFieldsToJustThoseInTable(conn, allFields, getCdwTableSetting("cdwFileTags"));

struct cdwFile *efList = NULL;
if (!securityColumnsInTable)
    efList = cdwAccessibleFileList(conn, user);

struct cdwFile *ef;

if (!securityColumnsInTable && !efList)
    {
    *retList = NULL;
    return;
    }

struct rbTree *searchPassTree = NULL;
if (!isEmpty(searchString))
    {
    searchPassTree = intValTreeNew(0);
    char *lowered = cloneString(searchString);
    tolowers(lowered);
    char *words[128];
    int wordCount = chopLine(lowered, words);
    char *trixPath = "/gbdb/cdw/cdw.ix";
    struct trix *trix = trixOpen(trixPath);
    struct trixSearchResult *tsr, *tsrList = trixSearch(trix, wordCount, words, tsmExpand);
    for (tsr = tsrList; tsr != NULL; tsr = tsr->next)
        {
	if (securityColumnsInTable) // creates a list with all found items file ids on it.
	    {
	    AllocVar(ef);
	    ef->id = sqlUnsigned(tsr->itemId);
	    slAddHead(&efList, ef);
	    }
	else
	    {
	    intValTreeAdd(searchPassTree, sqlUnsigned(tsr->itemId), tsr);
	    }
	}
    if (securityColumnsInTable)
	slReverse(&efList);
    }


/* Loop through all files constructing a SQL where clause that restricts us
 * to just the ones that we're authorized to hit, and that also pass initial where clause
 * if any. */
struct dyString *where = dyStringNew(0);
if (!isEmpty(initialWhere))
    sqlDyStringPrintfFrag(where, "(%-s)", initialWhere); // trust
if (securityColumnsInTable)
    {
    if (user)
	{
	// get all groupIds belonging to this user
	char query[256];
	if (!user->isAdmin)
	    {
	    sqlSafef(query, sizeof(query), 
		"select groupId from cdwGroupUser "
		" where cdwGroupUser.userId = %d", user->id);
	    struct sqlResult *sr = sqlGetResult(conn, query);
	    char **row;
	    if (!isEmpty(where->string))
		sqlDyStringPrintf(where, " and ");
	    sqlDyStringPrintfFrag(where, "(FIND_IN_SET('0', groupIds)");	 // initial 0 makes code smaller
	    while ((row = sqlNextRow(sr)) != NULL)
		{
		int groupId = sqlUnsigned(row[0]);
		sqlDyStringPrintf(where, " or FIND_IN_SET('%u', groupIds)", groupId);
		}
	    sqlFreeResult(&sr);
	    sqlDyStringPrintf(where, ")");
	    }
	}
    else
	{
	if (!isEmpty(where->string))
	    sqlDyStringPrintf(where, " and ");
	sqlDyStringPrintfFrag(where, "allAccess > 0");
	}
    }

if (efList 
    || (securityColumnsInTable && (!isEmpty(searchString)))) // have search terms but nothing was found
    {
    if (!isEmpty(where->string))
	sqlDyStringPrintf(where, " and ");
    sqlDyStringPrintfFrag(where, "file_id in (0");	 // initial 0 never found, just makes code smaller
    for (ef = efList; ef != NULL; ef = ef->next)
	{
	if (searchPassTree == NULL || securityColumnsInTable || intValTreeFind(searchPassTree, ef->id) != NULL)
	    {
	    sqlDyStringPrintf(where, ",%u", ef->id);
	    }
	}
    sqlDyStringPrintf(where, ")");
    }

rbTreeFree(&searchPassTree);

// return three variables
*retWhere  = where;
*retList   = efList;
*retFields = fields;
}

struct cdwFile* findDownloadableFiles(struct sqlConnection *conn, struct cart *cart,
    char* initialWhere, char *searchString)
/* return list of files that we are allowed to see and that match current filters */
{
// get query of files that match and where we have access
struct cdwFile *efList = NULL;
struct dyString *accWhere;
char *fields;
searchFilesWithAccess(conn, searchString, fileTableFields, initialWhere, &efList, &accWhere, &fields, FALSE);

// reduce query to those that match our filters
struct dyString *dummy;
struct dyString *filteredWhere;
char *table = isEmpty(initialWhere) ? getCdwTableSetting("cdwFileFacets") : getCdwTableSetting("cdwFileTags");

webTableBuildQuery(cart, table, accWhere->string, "cdwBrowseFiles", fileTableFields, FALSE, &dummy, &filteredWhere);

// Selected Facet Values Filtering
char *selectedFacetValues=cartUsualString(cart, "cdwSelectedFieldValues", "");
struct facetField *selectedList = deLinearizeFacetValString(selectedFacetValues);
struct facetField *sff = NULL;
struct dyString *facetedWhere = dyStringNew(1024);
for (sff = selectedList; sff; sff=sff->next)
    {
    if (slCount(sff->valList)>0)
	{
	sqlDyStringPrintfFrag(facetedWhere, " and ");  // use Frag to prevent NOSQLINJ tag
	sqlDyStringPrintf(facetedWhere, "%s in (", sff->fieldName);
	struct facetVal *el;
	for (el=sff->valList; el; el=el->next)
	    {
	    sqlDyStringPrintf(facetedWhere, "'%s'", el->val);
	    if (el->next)
		sqlDyStringPrintf(facetedWhere, ",");
	    }
	sqlDyStringPrintfFrag(facetedWhere, ")");
	}
    }

// get their fileIds
struct dyString *tagQuery = sqlDyStringCreate("SELECT file_id from %s %-s", table, filteredWhere->string); // trust
dyStringPrintf(tagQuery,  "%-s", facetedWhere->string); // trust because it was created safely
struct slName *fileIds = sqlQuickList(conn, tagQuery->string);

// retrieve the cdwFiles objects for these
struct dyString *fileQuery = sqlDyStringCreate("SELECT * FROM cdwFile WHERE id IN (");
sqlDyStringPrintValuesList(fileQuery, fileIds);
sqlDyStringPrintf(fileQuery, ")");
return cdwFileLoadByQuery(conn, fileQuery->string);
}

static void continueSearchVars()
/* print out hidden forms variables for the current search */
{
cgiContinueHiddenVar("cdwFileSearch");
char *fieldNames[128];
char *tempFileTableFields = cloneString(fileTableFields); // cannot modify string literals
int fieldCount = chopString(tempFileTableFields, ",", fieldNames, ArraySize(fieldNames));
int i;
for (i = 0; i<fieldCount; i++)
    {
    char varName[1024];
    safef(varName, sizeof(varName), "cdwBrowseFiles_f_%s", fieldNames[i]);
    cgiContinueHiddenVar(varName);
    }
}

void makeDownloadAllButtonForm() 
/* The "download all" button cannot be a form at this place, nested forms are
 * not allowed in html. So create a link instead. */
{
printf("<A HREF=\"cdwWebBrowse?hgsid=%s&cdwCommand=downloadFiles", cartSessionId(cart));

/* old way 
char *fieldNames[128];
char *tempFileTableFields = cloneString(fileTableFields); // cannot modify string literals
int fieldCount = chopString(tempFileTableFields, ",", fieldNames, ArraySize(fieldNames));
int i;
for (i = 0; i<fieldCount; i++)
    {
    char varName[1024];
    safef(varName, sizeof(varName), "cdwBrowseFiles_f_%s", fieldNames[i]);
    printf("&%s=%s", varName, cartCgiUsualString(cart, varName, ""));
    }
*/

printf("&cdwFileSearch=%s", cartCgiUsualString(cart, "cdwFileSearch", ""));
printf("&cdwFile_filter=%s", cartCgiUsualString(cart, "cdwFile_filter", ""));
printf("\">Download All</A>");
}

char *createTokenForUser()
/* create a random token and add it to the cdwDownloadToken table with the current username.
 * Returns token, should be freed.*/
{
struct sqlConnection *conn = hConnectCentral(); // r/w access -> has to be in hgcentral
char query[4096]; 
if (!sqlTableExists(conn, "cdwDownloadToken"))
     {
     sqlSafef(query, sizeof(query),
         "CREATE TABLE cdwDownloadToken (token varchar(255) NOT NULL PRIMARY KEY, "
	 "userId int NOT NULL, createTime datetime DEFAULT NOW())");
     sqlUpdate(conn, query);
     }
char *token = makeRandomKey(80);
sqlSafef(query, sizeof(query), "INSERT INTO cdwDownloadToken (token, userId) VALUES ('%s', %d)", token, user->id);
sqlUpdate(conn, query);
hDisconnectCentral(&conn);
return token;
}

void accessibleFilesTable(struct cart *cart, struct sqlConnection *conn, 
    char *searchString, char *allFields, char *fromTable, char *initialWhere,  
    char *returnUrl, char *varPrefix, int maxFieldWidth, 
    struct hash *tagOutWrappers, void *wrapperContext,
    boolean withFilters, char *itemPlural, int pageSize,
    char *visibleFacetList, boolean securityColumnsInTable)
{
struct cdwFile *efList = NULL;
struct dyString *where;
char *fields;

searchFilesWithAccess(conn, searchString, allFields, initialWhere, &efList, &where, &fields, securityColumnsInTable);

if (!securityColumnsInTable && !efList)
    {
    if (user != NULL && user->isAdmin)
	printf("<BR>The file database is empty.");
    else
	printf("<BR>Unfortunately there are no %s you are authorized to see.", itemPlural);
    return;
    }

if (user!=NULL)
    accessibleFilesToken = createTokenForUser();

/* Let the sql system handle the rest.  Might be one long 'in' clause.... */
struct hash *suggestHash = NULL;
if (!securityColumnsInTable)
    suggestHash = accessibleSuggestHash(conn, fields, efList);

webFilteredSqlTable(cart, conn, fields, fromTable, where->string, returnUrl, varPrefix, maxFieldWidth,
    tagOutWrappers, wrapperContext, withFilters, itemPlural, pageSize, suggestHash, visibleFacetList, makeDownloadAllButtonForm);

/* Clean up and go home. */
cdwFileFreeList(&efList);
dyStringFree(&where);
}

char *showSearchControl(char *varName, char *itemPlural)
/* Put up the search control text and stuff. Returns current search string. */
{
char *varVal = cartUsualString(cart, varName, "");
printf("Search <input name=\"%s\" type=\"text\" id=\"%s\" value=\"%s\" size=60>", 
    varName, varName, varVal);
printf("&nbsp;");
printf("<img src=\"../images/magnify.png\">\n");
jsInlineF(
    "$(function () {\n"
    "  $('#%s').watermark(\"type in words or starts of words to find specific %s\");\n" 
    "  $('form').delegate('#%s','change keyup paste',function(e){\n"
    "    $('[name=cdwBrowseFiles_page]').val('1');\n"
    "  });\n"
    "});\n",
    varName, itemPlural, varName);
return varVal;
}


void setCdwUser(struct sqlConnection *conn)
/* set the global variable 'user' based on the current cookie */
{
char *userName = wikiLinkUserName();

// for debugging, accept the userName on the cgiSpoof command line
// instead of a cookie
if (!cgiIsOnWeb() && userName == NULL)
    userName = cgiOptionalString("userName");

if (userName != NULL)
    user = cdwUserFromUserName(conn, userName);
}

void doDownloadUrls()
/* serve textfile with file URLs and ask user's internet browser to save it to disk */
{
struct sqlConnection *conn = sqlConnect(cdwDatabase);
setCdwUser(conn);

// on non-public cirm (and development vhosts) users must be logged in to use download tokens. 
if (user==NULL && !isPublicSite)
    {
    // this should never happen through normal UI use
    puts("Content-type: text/html\n\n");
    puts("Error: user is not logged in");
    return;
    }

// on public cirm we have users not logged in. user=NULL and download tokens are not required
char *token = NULL;
if (!isPublicSite)
    {
    token = createTokenForUser();
    }

// if we recreate the submission dir structure, we need to create a shell script
boolean createSubdirs = FALSE;
if (sameOk(cgiOptionalString("cdwDownloadName"), "subAndDir"))
    createSubdirs = TRUE;

cart = cartAndCookieWithHtml(hUserCookie(), excludeVars, oldVars, FALSE);

if (createSubdirs)
    puts("Content-disposition: attachment; filename=downloadCirm.sh\n");
else
    puts("Content-disposition: attachment; filename=fileUrls.txt\n");

char *searchString = cartUsualString(cart, "cdwFileSearch", "");
char *initialWhere = cartUsualString(cart, "cdwFile_filter", "");

struct cdwFile *efList = findDownloadableFiles(conn, cart, initialWhere, searchString);

char *host = hHttpHost();

// user may want to download with original submitted filename, not with format <accession>.<submittedExtension>
char *optArg = "";
if (sameOk(cgiOptionalString("cdwDownloadName"), "sub"))
    optArg = "&useSubmitFname=1";

struct cdwFile *ef;
for (ef = efList; ef != NULL; ef = ef->next)
    {
    struct cdwValidFile *vf = cdwValidFileFromFileId(conn, ef->id);

    if (createSubdirs)
        {
        struct cdwFile *cf = cdwFileFromId(conn, vf->fileId);
        // if we have an absolute pathname in our DB, strip the leading '/'
        // so if someone runs the script as root, it will not start to write
        // files in strange directories
        char* submitFname = cf->submitFileName;
        if ( (submitFname!=NULL) && (!isEmpty(submitFname)) && (*submitFname=='/') )
            submitFname += 1;

        printf("curl ");
	if (!isPublicSite)
	    printf("--netrc-file cirm_credentials ");
        printf("'https://%s/cgi-bin/cdwGetFile?acc=%s",
            host, vf->licensePlate); 
	if (!isPublicSite)
	    printf("&token=%s", token);
	printf("' --create-dirs -o %s\n", 
	    submitFname);
        }
    else
	{
        printf("https://%s/cgi-bin/cdwGetFile?acc=%s", host, vf->licensePlate);
	if (!isPublicSite)
	    printf("&token=%s", token);
	printf("%s\n", optArg);
	}
    }
}

void doDownloadFileConfirmation(struct sqlConnection *conn)
/* show overview page of download files */
{
if (user==NULL && !isPublicSite)
    {
    printf("Sorry, you have to log in before you can download files.");
    return;
    }

printf("<FORM ACTION=\"../cgi-bin/cdwWebBrowse\" METHOD=GET>\n");
cartSaveSession(cart);
cgiMakeHiddenVar("cdwCommand", "downloadUrls");

continueSearchVars();

char *searchString = cartUsualString(cart, "cdwFileSearch", "");
char *initialWhere = cartUsualString(cart, "cdwFile_filter", "");

struct cdwFile *efList = findDownloadableFiles(conn, cart, initialWhere, searchString);

// get total size
struct cdwFile *ef;
long long size = 0;
for (ef = efList; ef != NULL; ef = ef->next)
    size += ef->size;
int fCount = slCount(efList);

char sizeStr[4096];
sprintWithGreekByte(sizeStr, sizeof(sizeStr), size);

printf("<h4>Data Download Options</h4>\n");
printf("<b>Number of files:</b> %d<br>\n", fCount);
printf("<b>Total size:</b> %s<p>\n", sizeStr);

puts("<input class='urlListButton' type=radio name='cdwDownloadName' VALUE='acc' checked>\n");
//cgiMakeRadioButton("cdwDownloadName", "acc", TRUE);
puts("Name files by accession, one single directory<br>");
//cgiMakeRadioButton("cdwDownloadName", "sub", FALSE);
puts("<input class='urlListButton' type=radio name='cdwDownloadName' VALUE='sub'>\n");
puts("Name files as submitted, one single directory<br>");
//cgiMakeRadioButton("cdwDownloadName", "subAndDir", FALSE);
puts("<input class='scriptButton' type=radio name='cdwDownloadName' VALUE='subAndDir'>\n");
puts("Name files as submitted and put into subdirectories<p>");

cgiMakeSubmitButton();
printf("</FORM>\n");

jsInline 
    (
    "$('.scriptButton').change( function() {$('#urlListDoc').hide(); $('#scriptDoc').show()} );\n"
    "$('.urlListButton').change( function() {$('#urlListDoc').show(); $('#scriptDoc').hide()} );\n"
    );
puts("<div id='urlListDoc'>\n");
puts("When you click 'submit', a text file with the URLs of the files will get downloaded.\n");
puts("The URLs are valid for one week.<p>\n");
puts("To download the files:\n");
puts("<ul>\n");
puts("<li>With Firefox and <a href=\"https://addons.mozilla.org/en-US/firefox/addon/downthemall/\">DownThemAll</a>: Click Tools - DownThemAll! - Manager. Right click - Advanced - Import from file. Right-click - Select All. Right-click - Toogle All\n");
puts("<li>With Chrome and <a href=\"https://chrome.google.com/webstore/detail/tab-save/lkngoeaeclaebmpkgapchgjdbaekacki\">TabToSave</a>: Click the T/S icon next to the URL bar, click the edit button at the bottom of the screen and paste the file contents\n");

if (isPublicSite)
    {
    puts("<li>OSX/Linux: With curl and a single thread: <tt>xargs -n1 curl -JO < fileUrls.txt</tt>\n");
    puts("<li>Linux: With wget and a single thread: <tt>wget --content-disposition -i fileUrls.txt</tt>\n");
    puts("<li>With wget and 4 threads: <tt>xargs -n 1 -P 4 wget --content-disposition -q < fileUrls.txt</tt>\n");
    puts("<li>With aria2c, 16 threads and two threads per file: <tt>aria2c -x 16 -s 2 -i fileUrls.txt</tt>\n");
    }
else 
    {
    puts("<li>OSX/Linux: With curl and a single thread: <tt>xargs -n1 curl -JO --user YOUREMAIL:YOURPASS < fileUrls.txt</tt>\n");
    puts("<li>Linux: With wget and a single thread: <tt>wget --content-disposition -i fileUrls.txt --user YOUREMAIL --password YOURPASS</tt>\n");
    puts("<li>With wget and 4 threads: <tt>xargs -n 1 -P 4 wget --content-disposition -q --user YOUREMAIL --password YOURPASS < fileUrls.txt</tt>\n");
    puts("<li>With aria2c, 16 threads and two threads per file: <tt>aria2c --http-user YOUREMAIL --http-password YOURPASS -x 16 -s 2 -i fileUrls.txt</tt>\n");
    }

puts("</ul>\n");
puts("</div>\n");

puts("<div id='scriptDoc' style='display:none'>\n");
puts("When you click 'submit', a shell script that runs curl will get downloaded.\n");
puts("The URLs are valid for one week.<p>\n");
if (!isPublicSite)
    {
    puts("Before you start the download, create a file called cirm_credentials with your username and password like this:<br>\n");
    puts("<tt>echo 'default login <i>user@university.edu</i> password <i>mypassword</i>' > cirm_credentials</tt><p>\n");
    }
puts("Then, to download the files:\n");
puts("<ul>\n");
puts("<li>Linux/OSX: With curl and a single thread: <tt>sh downloadCirm.sh</tt>\n");
puts("<li>Linux/OSX: With curl and four threads: <tt>parallel -j4 :::: downloadCirm.sh</tt>\n");
puts("</ul>\n");
puts("<div>\n");
cdwFileFreeList(&efList);
}

void doBrowseFiles(struct sqlConnection *conn)
/* Print list of files */
{
printf("<FORM ACTION=\"../cgi-bin/cdwWebBrowse\" METHOD=GET>\n");
cartSaveSession(cart);
cgiMakeHiddenVar("cdwCommand", "browseFiles");

cgiMakeHiddenVar("clearSearch", "0");
char *clearSearch = cartOptionalString(cart, "clearSearch");
if (clearSearch && sameString(clearSearch,"1"))
    {
    cartSetString(cart, "cdwFile_filter", "");  // reset file filter to empty string
    }

// DEBUG REMOVE
//char *varName = "cdwSelectedFieldValues";
//char *varVal = cartUsualString(cart, varName, "");
//warn("varName=[%s] varVal=[%s]", varName, varVal); // DEBUG REMOVE

//warn("getCdwTableSetting(cdwFileFacets)=%s", getCdwTableSetting("cdwFileFacets")); // DEBUG REMOVE

char *selOp = cartOptionalString(cart, "browseFiles_facet_op");
if (selOp)
    {
    char *selFieldName = cartOptionalString(cart, "browseFiles_facet_fieldName");
    char *selFieldVal = cartOptionalString(cart, "browseFiles_facet_fieldVal");
    if (selFieldName && selFieldVal)
	{
	char *selectedFacetValues=cartUsualString(cart, "cdwSelectedFieldValues", "");
	//warn("selectedFacetValues=[%s] selFieldName=%s selFieldVal=%s selOp=%s", 
	    //selectedFacetValues, selFieldName, selFieldVal, selOp); // DEBUG REMOVE
	struct facetField *selList = deLinearizeFacetValString(selectedFacetValues);
	selectedListFacetValUpdate(&selList, selFieldName, selFieldVal, selOp);
	char *newSelectedFacetValues = linearizeFacetVals(selList);
	//warn("newSelectedFacetValues=[%s]", newSelectedFacetValues); // DEBUG REMOVE
	cartSetString(cart, "cdwSelectedFieldValues", newSelectedFacetValues);
	cartRemove(cart, "browseFiles_facet_op");
	cartRemove(cart, "browseFiles_facet_fieldName");
	cartRemove(cart, "browseFiles_facet_fieldVal");
	}
    }

// DEBUG REMOVE
//varVal = cartUsualString(cart, varName, "");  // get a fresh value
//htmlPrintf("Selected Fields String <input name='%s|attr|' type='text' id='%s|attr|' value='%s|attr|' size=60><br>\n", 
//    varName, varName, varVal);

printf("<B>Files</B> - search, filter, and sort files. ");
printf("Click on file's name to see full metadata.");
printf(" Links in ucsc_db go to the Genome Browser. <BR>\n");
char *searchString = showSearchControl("cdwFileSearch", "files");

/* Put up big filtered table of files */
char returnUrl[PATH_LEN*2];
safef(returnUrl, sizeof(returnUrl), "../cgi-bin/cdwWebBrowse?cdwCommand=browseFiles&%s",
    cartSidUrlString(cart) );
char *where = cartUsualString(cart, "cdwFile_filter", "");
if (!isEmpty(where))
    {
    printf("<BR>Restricting files to where %s. ", where);
    }
struct hash *wrappers = hashNew(0);
hashAdd(wrappers, "file_name", wrapFileName);
hashAdd(wrappers, "ucsc_db", wrapTrackNearFileName);
hashAdd(wrappers, "format", wrapFormat);

accessibleFilesTable(cart, conn, searchString,
  fileTableFields,
  isEmpty(where) ? getCdwTableSetting("cdwFileFacets") : getCdwTableSetting("cdwFileTags"),
  where, 
  returnUrl, "cdwBrowseFiles",
  18, wrappers, conn, FALSE, "files", 100, visibleFacetFields, TRUE);
printf("</FORM>\n");
}

char *getBrowseTracksTables()
{
char tables[1024];
safef(tables, sizeof tables, "%s,cdwTrackViz", getCdwTableSetting("cdwFileTags"));
return cloneString(tables);
}

void doBrowseTracks(struct sqlConnection *conn)
/* Print list of files */
{
printf("<FORM ACTION=\"../cgi-bin/cdwWebBrowse\" METHOD=GET>\n");
cartSaveSession(cart);
cgiMakeHiddenVar("cdwCommand", "browseTracks");
cgiMakeHiddenVar("clearSearch", "0");

printf("<B>Tracks</B> - Click on ucsc_db to open Genome Browser. ");
printf("The accession link shows more metadata.<BR>");
char returnUrl[PATH_LEN*2];
safef(returnUrl, sizeof(returnUrl), "../cgi-bin/cdwWebBrowse?cdwCommand=browseTracks&%s",
    cartSidUrlString(cart) );
char *where = "fileId=file_id and format in ('bam','bigBed', 'bigWig', 'vcf', 'narrowPeak', 'broadPeak')";
struct hash *wrappers = hashNew(0);
hashAdd(wrappers, "accession", wrapMetaNearAccession);
hashAdd(wrappers, "ucsc_db", wrapTrackNearAccession);
char *searchString = showSearchControl("cdwTrackSearch", "tracks");
accessibleFilesTable(cart, conn, searchString,
    "ucsc_db,chrom,accession,format,file_size,lab,assay,data_set_id,output,"
    "enriched_in,sample_label,submit_file_name",
    getBrowseTracksTables(), where, 
    returnUrl, "cdw_track_filter", 
    22, wrappers, conn, TRUE, "tracks", 100, NULL, FALSE);
printf("</FORM>\n");
}

struct hash* loadDatasetDescs(struct sqlConnection *conn)
/* Load cdwDataset table and return hash with name -> cdwDataset */
{
char query[256];
sqlSafef(query, sizeof query, "SELECT * FROM cdwDataset");
struct sqlResult *sr = sqlGetResult(conn, query);
struct hash *descs = hashNew(7);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct cdwDataset *dataset = cdwDatasetLoad(row);
    hashAdd(descs, dataset->name, dataset);
    }
sqlFreeResult(&sr);
return descs;
}

static boolean cdwCheckAccessFromFileId(struct sqlConnection *conn, int fileId, struct cdwUser *user, int accessType)
/* Determine if user can access file given a file ID */
{
struct cdwFile *ef = cdwFileFromId(conn, fileId);
boolean ok = cdwCheckAccess(conn, ef, user, accessType);
cdwFileFree(&ef);
return ok;
}

static boolean cdwCheckFileAccess(struct sqlConnection *conn, int fileId, struct cdwUser *user)
/* Determine if the user can see the specified file. Return True if they can and False otherwise. */ 
{
return cdwCheckAccessFromFileId(conn, fileId, user, cdwAccessRead);
}

void doServeTagStorm(struct sqlConnection *conn, char *dataSet)
/* Put up meta data tree associated with data set. */
{
char metaFileName[PATH_LEN];
safef(metaFileName, sizeof(metaFileName), "%s/%s", dataSet, "meta.txt");
printf(", <A HREF=\"cdwServeTagStorm?format=text&cdwDataSet=%s&%s\"",
	dataSet, cartSidUrlString(cart)); 
printf(">text</A>");
printf(", <A HREF=\"cdwServeTagStorm?format=tsv&cdwDataSet=%s&%s\"",
	dataSet, cartSidUrlString(cart)); 
printf(">tsv</A>");
printf(", <A HREF=\"cdwServeTagStorm?format=csv&cdwDataSet=%s&%s\"",
	dataSet, cartSidUrlString(cart)); 
printf(">csv</A>)"); 
}

void doBrowseDatasets(struct sqlConnection *conn)
/* Show datasets and links to dataset summary pages. */
{
printf("<UL>\n");
char query[PATH_LEN]; 
sqlSafef(query, sizeof(query), "SELECT * FROM cdwDataset ORDER BY label "); 
struct cdwDataset *dataset, *datasetList = cdwDatasetLoadByQuery(conn, query);
// Go through the cdwDataset table and generate an entry for each dataset. 
for (dataset = datasetList; dataset != NULL; dataset = dataset->next)
    {
    char *label = dataset->label;
    char *desc = dataset->description;
    char *datasetId = dataset->name;

    // Check if we have a dataset summary page in the CDW and store its ID. The file must have passed validation.  
    char summFname[8000];
    safef(summFname, sizeof(summFname), "%s/summary/index.html", datasetId);
    int fileId = cdwFileIdFromPathSuffix(conn, summFname);

    // If the ID exists and the user has access print a link to the page.  
    boolean haveAccess = ((fileId > 0) && cdwCheckFileAccess(conn, fileId, user));
    if (haveAccess)
	{
	printf("<LI><B><A href=\"cdwGetFile/%s/summary/index.html\">%s (%s)</A></B><BR>\n", 
	    datasetId, label, datasetId);
	// Print out file count and descriptions. 
	sqlSafef(query, sizeof(query), 
	    "select count(*) from %s where data_set_id='%s'", getCdwTableSetting("cdwFileTags"), datasetId);  
	long long fileCount = sqlQuickLongLong(conn, query);
	printf("%s (<A HREF=\"cdwWebBrowse?cdwCommand=browseFiles&cdwBrowseFiles_f_data_set_id=%s&%s\"",
		desc, datasetId, cartSidUrlString(cart)); 
	printf(">%lld files</A>)",fileCount);
	printf(" (metadata: <A HREF=\"cdwWebBrowse?cdwCommand=dataSetMetaTree&cdwDataSet=%s&%s\"",
		datasetId, cartSidUrlString(cart)); 
	printf(">html</A>");
	doServeTagStorm(conn, datasetId);

	}
    else // Otherwise print a label and description. 
	{
	printf("<LI><B>%s (%s)</B><BR>\n", label, datasetId);
	printf("%s\n", desc);
	}
    printf("</LI>\n");
    }
cdwDatasetFree(&datasetList);
}


void doDataSetMetaTree(struct sqlConnection *conn)
/* Put up meta data tree associated with data set. */
{
char *dataSet = cartString(cart, "cdwDataSet");
char metaFileName[PATH_LEN];
safef(metaFileName, sizeof(metaFileName), "%s/%s", dataSet, "meta.txt");
int fileId = cdwFileIdFromPathSuffix(conn, metaFileName);
if (!cdwCheckFileAccess(conn, fileId, user))
   errAbort("Unauthorized access to %s", metaFileName);
printf("<H2>Metadata tree for %s</H2>\n", dataSet);
char *path = cdwPathForFileId(conn, fileId);
char command[3*PATH_LEN];
fflush(stdout);
safef(command, sizeof(command), "./tagStormToHtml -embed %s -nonce=%s stdout", path, getNonce());
mustSystem(command);
}

void doBrowseFormat(struct sqlConnection *conn)
/* Browse through available formats */
{
static char *labels[] = {"count", "format", "description"};
int fieldCount = ArraySize(labels);
char *row[fieldCount];
struct fieldedTable *table = fieldedTableNew("Data formats", labels, fieldCount);
struct slPair *format, *formatList = cdwFormatList();

for (format = formatList; format != NULL; format = format->next)
    {
    char countString[16];
    char query[256];
    sqlSafef(query, sizeof(query), "select count(*) from %s where format='%s'", 
	getCdwTableSetting("cdwFileTags"), format->name);
    sqlQuickQuery(conn, query, countString, sizeof(countString));
    row[0] = countString;
    row[1] = format->name;
    row[2] = format->val;
    fieldedTableAdd(table, row, fieldCount, 0);
    }
char returnUrl[PATH_LEN*2];
safef(returnUrl, sizeof(returnUrl), "../cgi-bin/cdwWebBrowse?cdwCommand=browseFormats&%s",
    cartSidUrlString(cart) );
webSortableFieldedTable(cart, table, returnUrl, "cdwFormats", 0, NULL, NULL);
fieldedTableFree(&table);
}

void doBrowseLabs(struct sqlConnection *conn)
/* Put up information on labs */
{
static char *labels[] = {"name", "files", "PI", "institution", "web page"};
int fieldCount = ArraySize(labels);
char *row[fieldCount];
struct fieldedTable *table = fieldedTableNew("Data contributing labs", labels, fieldCount);

printf("Here is a table of labs that have contributed data<BR>\n");
char query[256];
sqlSafef(query, sizeof(query), "select * from cdwLab");
struct cdwLab *lab, *labList = cdwLabLoadByQuery(conn, query);
int i = 0;
for (lab = labList; lab != NULL; lab = lab->next)
    {
    row[0] = lab->name;
    char countString[16];
    sqlSafef(query, sizeof(query), "select count(*) from %s where lab='%s'", 
	getCdwTableSetting("cdwFileTags"), lab->name);
    sqlQuickQuery(conn, query, countString, sizeof(countString));
    row[1] = countString;
    row[2] = lab->pi;
    row[3] = lab->institution;
    row[4] = lab->url;
    fieldedTableAdd(table, row, fieldCount, ++i);
    }
char returnUrl[PATH_LEN*2];
safef(returnUrl, sizeof(returnUrl), "../cgi-bin/cdwWebBrowse?cdwCommand=browseLabs&%s",
    cartSidUrlString(cart) );
struct hash *outputWrappers = hashNew(0);
hashAdd(outputWrappers, "web page", wrapExternalUrl);
webSortableFieldedTable(cart, table, returnUrl, "cdwLab", 0, outputWrappers, NULL);
fieldedTableFree(&table);
}



void doAnalysisQuery(struct sqlConnection *conn)
/* Print up query page */
{
/* Do stuff that keeps us here after a routine submit */
printf("Enter a SQL-like query below. ");
printf("In the box after 'select' you can put in a list of tag names including wildcards. ");
printf("In the box after 'where' you can put in filters <BR> based on boolean operations between ");
printf("fields and constants. Select one of the four formats and press view to see the matching data on ");
printf("this page. <BR> The limit can be set lower to increase the query speed. <BR><BR>");
printf("<FORM class = \"inlineBlock\" ACTION=\"../cgi-bin/cdwWebBrowse\" METHOD=GET>\n");
cartSaveSession(cart);
cgiMakeHiddenVar("cdwCommand", "analysisQuery");

/* Get values from text inputs and make up an RQL query string out of fields, where, limit
 * clauses */
/* Fields clause */
char *fieldsVar = "cdwQueryFields";
char *fields = cartUsualString(cart, fieldsVar, "*");
struct dyString *rqlQuery = dyStringCreate("select %s from files ", fields);

/* Where clause */
char *whereVar = "cdwQueryWhere";
char *where = cartUsualString(cart, whereVar, "");
dyStringPrintf(rqlQuery, "where accession");
if (!isEmpty(where))
    dyStringPrintf(rqlQuery, " and (%s)", where);

/* Limit clause */
char *limitVar = "cdwQueryLimit";
int limit = cartUsualInt(cart, limitVar, 10);

struct tagStorm *tags = cdwUserTagStorm(conn, user);
struct slRef *matchList = tagStanzasMatchingQuery(tags, rqlQuery->string);
int matchCount = slCount(matchList);
/** Write out select [  ] from files whre [  ] limit [ ] <submit> **/
printf("select ");
cgiMakeTextVar(fieldsVar, fields, 40);
printf("from files ");
printf("where ");
cgiMakeTextVar(whereVar, where, 40);
printf(" limit ");
cgiMakeIntVar(limitVar, limit, 7);
char *menu[3];
menu[0] = "ra";
menu[1] = "tsv";
menu[2] = "csv";

char *formatVar = "cdwQueryFormat";
char *format = cartUsualString(cart, formatVar, menu[0]);
printf("  "); 
cgiMakeDropList(formatVar, menu, ArraySize(menu), format);
printf("  "); 
cgiMakeButton("View", "View"); 
printf("</FORM>\n\n");


printf("<BR>Clicking the download button will take you to a new page with all of the matching data in "); 
printf("the selected format.<BR>");
printf("<FORM class=\"inlineBlock\" ACTION=\"../cgi-bin/cdwGetMetadataAsFile\" METHOD=GET>\n");
cartSaveSession(cart);
cgiMakeHiddenVar("cdwCommand", "analysisQuery");
cgiMakeHiddenVar("Download format", format); 
cgiMakeButton(format, "Download"); 
printf("</FORM>\n\n");

printf("<PRE><TT>");
printLongWithCommas(stdout, matchCount);
printf(" files match\n\n");
cdwPrintMatchingStanzas(rqlQuery->string, limit, tags, format);
printf("</TT></PRE>\n");
}

void doAnalysisJointPages(struct sqlConnection *conn)
/* Show datasets and links to dataset summary pages. */
{
printf("<UL>\n");
char query[PATH_LEN]; 
sqlSafef(query, sizeof(query), "SELECT * FROM cdwJointDataset ORDER BY label ");
struct cdwJointDataset *jointDataset, *datasetList = cdwJointDatasetLoadByQuery(conn, query);
// Go through the cdwDataset table and generate an entry for each dataset. 
for (jointDataset = datasetList; jointDataset != NULL; jointDataset = jointDataset->next)
    {
    char *label = jointDataset->label;
    char *desc = jointDataset->description;
    char *datasetId = jointDataset->name;

    // Check if we have a dataset summary page in the CDW and store its ID. The file must have passed validation.  
    char summFname[8000];
    safef(summFname, sizeof(summFname), "%s/summary/index.html", datasetId);
    int fileId = cdwFileIdFromPathSuffix(conn, summFname);
    // If the ID exists and the user has access print a link to the page.  
    boolean haveAccess = ((fileId > 0) && cdwCheckFileAccess(conn, fileId, user));
    if (haveAccess)
	{
	printf("<LI><B><A href=\"cdwGetFile/%s/summary/index.html\">%s (%s)</A></B><BR>\n", 
	    datasetId, label, datasetId);
	// Print out file count and descriptions. 
	sqlSafef(query, sizeof(query), 
	    "select count(*) from %s where data_set_id='%s'", 
		getCdwTableSetting("cdwFileTags"), datasetId);  
	//long long fileCount = sqlQuickLongLong(conn, query);
	printf("%s", desc); 
	printf(" (metadata: <A HREF=\"cdwWebBrowse?cdwCommand=dataSetMetaTree&cdwDataSet=%s&%s\"",
		datasetId, cartSidUrlString(cart)); 
	printf(">html</A>");
	doServeTagStorm(conn, datasetId);
	    
	struct slName *dataset, *dataSetNames=charSepToSlNames(jointDataset->childrenNames, *",");
	printf("("); 
	int totFileCount = 0; 
	// Go through each sub dataset, print a link into the search files and count total files.  
	for (dataset = dataSetNames; dataset != NULL; dataset=dataset->next)
	    {
	    sqlSafef(query, sizeof(query), "select count(*) from %s where data_set_id='%s'", 
		getCdwTableSetting("cdwFileTags"), dataset->name); 
	    totFileCount += sqlQuickNum(conn, query); 
	    printf("<A HREF=\"cdwWebBrowse?cdwCommand=browseFiles&cdwBrowseFiles_f_data_set_id=%s&%s\">%s",
			    dataset->name, cartSidUrlString(cart), dataset->name);
	    if (dataset->next != NULL) printf(",");
	    
	    printf("</A>\n"); 
	}
    printf(") (Total files: %i)",totFileCount); 
    printf("</LI>\n");

	}
    else // Otherwise print a label and description. 
	{
	printf("<LI><B>%s (%s)</B><BR>\n", label, datasetId);
	printf("%s\n", desc);
	}
    printf("</LI>\n");
    }
cdwJointDatasetFree(&datasetList);
}

void facetSummaryRow(struct fieldedTable *table, struct facetField *ff)
/* Print out row in a high level tag counting table */
{
struct facetVal *fv;
int total = 0; // Col 4, 'files'
char valValueString[PATH_LEN], valCountString[32]; // Col 3, 'popular values (files)...' and col 2, 'vals' 
valValueString[0]= '\0'; 
strcat(valValueString, " "); 
safef(valCountString, sizeof(valCountString), "%d", slCount(ff->valList)); 
bool hairpin = TRUE; 
for (fv = ff->valList; fv != NULL; fv = fv->next)
    {
    total += fv->useCount;
    if (hairpin == FALSE) continue; 

    char temp[4*1024]; 
    // Make a new name value pair which may get added to the existing string. 
    safef(temp, sizeof(temp),"%s (%d)", fv->val, fv->useCount); 
    int newStringLen = ((int) strlen(valValueString))+((int) strlen(temp));
    if (newStringLen >= 107) // Check if the new string is acceptable size. 
	{
	// The hairpin is set to false once the full line is made, this stops the line from growing. 
	if (hairpin) 
	    //Remove the comma and append '...'.
	    {
	    valValueString[strlen(valValueString)-2] = '\0';
	    strcat(valValueString, "..."); 
	    hairpin = FALSE;
	    }
	}
    else{ // Append the name:value pair to the string.  
	strcat(valValueString, temp); 
	if (fv->next != NULL)
	    strcat(valValueString, ", "); 
	}
    }
char trueTotal[PATH_LEN]; 
safef(trueTotal, sizeof(trueTotal), "%d", total); 

/* Add data to fielded table */
char *row[4];
row[0] = cloneString(ff->fieldName);
row[1] = cloneString(valCountString);
row[2] = cloneString(valValueString);
row[3] = cloneString(trueTotal);
fieldedTableAdd(table, row, ArraySize(row), 0);
}

void tagSummaryRow(struct fieldedTable *table, struct sqlConnection *conn, char *tag)
/* Print out row in a high level tag counting table */
{
// These will become the values for our columns. 
int total = 0; // Col 4, 'files'
char valValueString[PATH_LEN], valCountString[32]; // Col 3, 'popular values (files)...' and col 2, 'vals' 

// Get the necessary data via SQL and load into a slPair. 
char query[PATH_LEN];
sqlSafef(query, sizeof(query),"select %s, count(*) as count from %s where %s is not NULL group by %s order by count desc",
  getCdwTableSetting("cdwFileTags"), tag, tag, tag); 
struct slPair *iter = NULL, *pairList = sqlQuickPairList(conn, query);

safef(valCountString, sizeof(valCountString), "%d", slCount(&pairList)-1); 
bool hairpin = TRUE; 
valValueString[0]= '\0'; 
strcat(valValueString, " "); 
//Go through the pair list and generate the 'popular values (files)...' column and the 'files' column
for (iter = pairList; iter != NULL; iter = iter->next)
    {
    total += sqlSigned( ((char*) iter->val)); // Calculate the total of the values for files column.
    if (hairpin == FALSE) continue; 

    char temp[4*1024]; 
    // Make a new name value pair which may get added to the existing string. 
    safef(temp, sizeof(temp),"%s (%s)", iter->name, (char *)iter->val); 
    int newStringLen = ((int) strlen(valValueString))+((int) strlen(temp));
    if (newStringLen >= 107) // Check if the new string is acceptable size. 
	{
	// The hairpin is set to false once the full line is made, this stops the line from growing. 
	if (hairpin) 
	    //Remove the comma and append '...'.
	    {
	    valValueString[strlen(valValueString)-2] = '\0';
	    strcat(valValueString, "..."); 
	    hairpin = FALSE;
	    }
	}
    else{ // Append the name:value pair to the string.  
	strcat(valValueString, temp); 
	if (iter->next != NULL)
	    strcat(valValueString, ", "); 
	}
    }
char trueTotal[PATH_LEN]; 
safef(trueTotal, sizeof(trueTotal), "%d", total); 

/* Add data to fielded table */
char *row[4];
row[0] = cloneString(tag);
row[1] = cloneString(valCountString);
row[2] = cloneString(valValueString);
row[3] = cloneString(trueTotal);
fieldedTableAdd(table, row, ArraySize(row), 0);
}

struct tagCount
/* A tag name and how many things are associated with it. */
    {
    struct tagCount *next;
    char *tag;
    long count;	
    };

struct tagCount *tagCountNew(char *tag, int count)
/* Return fresh new tag */
{
struct tagCount *tc;
AllocVar(tc);
tc->tag = cloneString(tag);
tc->count = count;
return tc;
}

void drawPrettyPieGraph(struct facetVal *fvList, char *id, char *title, char *subtitle)
/* Draw a pretty pie graph using D3. Import D3 and D3pie before use. */
{
// Some D3 administrative stuff, the title, subtitle, sizing etc. 
struct dyString *dy = dyStringNew(1024);

dyStringPrintf(dy,"var pie = new d3pie(\"%s\", {\n\"header\": {", id);
if (title != NULL)
    {
    dyStringPrintf(dy,"\"title\": { \"text\": \"%s\",", title);
    dyStringPrintf(dy,"\"fontSize\": 16,");
    dyStringPrintf(dy,"\"font\": \"open sans\",},");
    }
if (subtitle != NULL)
    {
    dyStringPrintf(dy,"\"subtitle\": { \"text\": \"%s\",", subtitle);
    dyStringPrintf(dy,"\"color\": \"#999999\",");
    dyStringPrintf(dy,"\"fontSize\": 10,");
    dyStringPrintf(dy,"\"font\": \"open sans\",},");
    }
dyStringPrintf(dy,"\"titleSubtitlePadding\":9 },\n");
dyStringPrintf(dy,"\"footer\": {\"color\": \"#999999\",");
dyStringPrintf(dy,"\"fontSize\": 10,");
dyStringPrintf(dy,"\"font\": \"open sans\",");
dyStringPrintf(dy,"\"location\": \"bottom-left\",},\n");
dyStringPrintf(dy,"\"size\": { \"canvasWidth\": 270, \"canvasHeight\": 220},\n");
dyStringPrintf(dy,"\"data\": { \"sortOrder\": \"value-desc\", \"content\": [\n");
struct facetVal *fv = NULL;
float colorOffset = 1;
int totalFields =  slCount(fvList);
for (fv=fvList; fv!=NULL; fv=fv->next)
    {
    float currentColor = colorOffset/totalFields;
    struct rgbColor color = saturatedRainbowAtPos(currentColor);
    dyStringPrintf(dy,"\t{\"label\": \"%s\",\n\t\"value\": %d,\n\t\"color\": \"rgb(%i,%i,%i)\"}", 
	fv->val, fv->useCount, color.r, color.b, color.g);
    if (fv->next!=NULL) 
	dyStringPrintf(dy,",\n");
    ++colorOffset;
    }
dyStringPrintf(dy,"]},\n\"labels\": {");
dyStringPrintf(dy,"\"outer\":{\"pieDistance\":20},");
dyStringPrintf(dy,"\"inner\":{\"hideWhenLessThanPercentage\":5},");
dyStringPrintf(dy,"\"mainLabel\":{\"fontSize\":11},");
dyStringPrintf(dy,"\"percentage\":{\"color\":\"#ffffff\", \"decimalPlaces\": 0},");
dyStringPrintf(dy,"\"value\":{\"color\":\"#adadad\", \"fontSize\":11},");
dyStringPrintf(dy,"\"lines\":{\"enabled\":true},},\n");
dyStringPrintf(dy,"\"effects\":{\"pullOutSegmentOnClick\":{");
dyStringPrintf(dy,"\"effect\": \"linear\",");
dyStringPrintf(dy,"\"speed\": 400,");
dyStringPrintf(dy,"\"size\": 8}},\n");
dyStringPrintf(dy,"\"misc\":{\"gradient\":{");
dyStringPrintf(dy,"\"enabled\": true,");
dyStringPrintf(dy,"\"percentage\": 100}}});");
jsInline(dy->string);
dyStringFree(&dy);
}

#ifdef OLD
struct tagCount *tagCountListFromQuery(struct sqlConnection *conn, char *query)
/* Return a tagCount list from a query that had tag/count values */
{
struct tagCount *list = NULL, *tc;
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    tc = tagCountNew(row[0], sqlUnsigned(row[1]));
    slAddHead(&list, tc);
    }
sqlFreeResult(&sr);
slReverse(&list);
return list;
}
#endif /* OLD */

void pieOnFacet(struct facetField *ff, char *divId)
/* Write a pie chart base on the values of given tag in storm */
{
struct facetVal *majorTags = facetValMajorPlusOther(ff->valList, 0.01);
drawPrettyPieGraph(majorTags, divId, ff->fieldName, NULL);
}


char *tagPopularityFields[] = { "tag name", "vals", "popular values (files)...", "files",};

void doHome(struct sqlConnection *conn)
/* Put up home/summary page */
{
printf("<table><tr><td>");
printf("<img src=\"../images/freeStemCell.jpg\" width=%d height=%d>\n", 200, 275);
printf("</td><td>");

/* Print sentence with summary of bytes, files, and labs */
char query[256];
printf("The CIRM Stem Cell Hub contains ");
sqlSafef(query, sizeof(query),
    "select sum(size) from cdwFile,cdwValidFile where cdwFile.id=cdwValidFile.id "
    " and (errorMessage = '' or errorMessage is null)"
    );
long long totalBytes = sqlQuickLongLong(conn, query);
printWithGreekByte(stdout, totalBytes);
printf(" of data in ");
#ifdef OLD
sqlSafef(query, sizeof(query),
    "select count(*) from cdwFile,cdwValidFile where cdwFile.id=cdwValidFile.fileId "
    " and (errorMessage = '' or errorMessage is null)"
    );
#endif /* OLD */

/* Using a query that is faster than the table join but gives the same result 
 * (0.2 sec vs. 0.8 sec) */
sqlSafef(query, sizeof(query), 
    "select count(*) from cdwFile where (errorMessage = '' || errorMessage is null)"
    " and cdwFileName like '%%/%s%%'",  cdwLicensePlateHead(conn) );

long long fileCount = sqlQuickLongLong(conn, query);
printLongWithCommas(stdout, fileCount);
printf(" files");
sqlSafef(query, sizeof(query),
    "select count(*) from cdwLab "
    );
long long labCount = sqlQuickLongLong(conn, query);  
printf(" from %llu labs.<BR>\n", labCount); 

printf("Try using the browse menu on files or tracks. ");
printf("The query link allows simple SQL-like queries of the metadata.");
printf("<BR>\n");


/* Print out some pie charts on important fields */
static char *pieTags[] = 
    {"lab", "format", "assay", };
struct facetField *pieFacetList = facetFieldsFromSqlTable(conn, getCdwTableSetting("cdwFileFacets"), 
						    pieTags, ArraySize(pieTags), "N/A", NULL, NULL, NULL);
struct facetField *ff;
int i;
printf("<TABLE style=\"display:inline\"><TR>\n");
for (i=0, ff = pieFacetList; i<ArraySize(pieTags); ++i, ff = ff->next)
    {
    char pieDivId[64];
    safef(pieDivId, sizeof(pieDivId), "pie_%d", i);
    printf("<TD id=\"%s\"><TD>", pieDivId);
    pieOnFacet(ff, pieDivId); 
    }
printf("</TR></TABLE>\n");
printf("<CENTER><I>charts are based on proportion of files in each category</I></CENTER>\n");
printf("</td></tr></table>\n");

/* Print out high level tags table */
static char *highLevelTags[] = 
    {"data_set_id", "lab", "assay", "format", "read_size",
    "sample_label", "species"};
struct facetField *highFacetList = facetFieldsFromSqlTable(conn, getCdwTableSetting("cdwFileFacets"), 
						highLevelTags, ArraySize(highLevelTags), NULL, NULL, NULL, NULL);

struct fieldedTable *table = fieldedTableNew("Important tags", tagPopularityFields, 
    ArraySize(tagPopularityFields));
for (ff = highFacetList; ff != NULL; ff = ff->next)
    facetSummaryRow(table, ff);

char returnUrl[PATH_LEN*2];
safef(returnUrl, sizeof(returnUrl), "../cgi-bin/cdwWebBrowse?%s", cartSidUrlString(cart) );
webSortableFieldedTable(cart, table, returnUrl, "cdwHome", 0, NULL, NULL);

printf("This table is a summary of important metadata tags and number of files they ");
printf("are attached to. Use browse tags menu to see all tags.");

printf("<BR>\n\n\n\n\n\n");
}

void doBrowseTags(struct sqlConnection *conn)
/* Put up browse tags page */
{
struct tagStorm *tags = cdwUserTagStorm(conn, user);
struct slName *tag, *tagList = tagStormFieldList(tags);
slSort(&tagList, slNameCmp);
printf("This is a list of all tags and their most popular values.");
struct fieldedTable *table = fieldedTableNew("Important tags", tagPopularityFields, 
    ArraySize(tagPopularityFields));
for (tag = tagList; tag != NULL; tag = tag->next)
    tagSummaryRow(table, conn, tag->name);
char returnUrl[PATH_LEN*2];
safef(returnUrl, sizeof(returnUrl), "../cgi-bin/cdwWebBrowse?cdwCommand=browseTags&%s",
    cartSidUrlString(cart) );
struct hash *outputWrappers = hashNew(0);
    hashAdd(outputWrappers, "tag name", wrapTagField);
webSortableFieldedTable(cart, table, returnUrl, "cdwBrowseTags", 0, outputWrappers, NULL);
tagStormFree(&tags);
}

void doHelp(struct sqlConnection *conn)
/* Put up help page */
{
puts(
#include "cdwHelp.h"
);
}

void doTestPage(struct sqlConnection *conn)
/* Put up test page */
{
printf("testing 1 2 3...<BR>\n");
static char *fields[] = 
    {"data_set_id", "lab", "assay", "format", "read_size", "species", "organ"};
uglyTime(NULL);
struct facetField *fieldList = facetFieldsFromSqlTable(conn, getCdwTableSetting("cdwFileTags"), 
						fields, ArraySize(fields), NULL, NULL, NULL, NULL);
uglyTime("listing facets");
printf("got info on %d fields<BR>\n", slCount(fieldList));
struct facetField *field;
for (field = fieldList; field != NULL; field = field->next)
    {
    printf("<div>\n");
    printf("<b>%s</b> has %d values\n", field->fieldName, slCount(field->valList));
    printf("</div>\n");
    }
}

void dispatch(struct sqlConnection *conn)
/* Dispatch page after to routine depending on cdwCommand variable */
{
char *command = cartOptionalString(cart, "cdwCommand");
if (command == NULL)
    {
    doHome(conn);
    }
else if (sameString(command, "home"))
    {
    doHome(conn);
    }
else if (sameString(command, "analysisQuery"))
    {
    doAnalysisQuery(conn);
    }
else if (sameString(command, "analysisJointPages"))
    {
    doAnalysisJointPages(conn);
    }
else if (sameString(command, "downloadFiles"))
    {
    doDownloadFileConfirmation(conn);
    }
else if (sameString(command, "browseFiles"))
    {
    doBrowseFiles(conn);
    }
else if (sameString(command, "doFileFlowchart"))
    {
    doFileFlowchart(conn);
    }
else if (sameString(command, "browseTracks"))
    {
    doBrowseTracks(conn);
    }
else if (sameString(command, "browseTags"))
    {
    doBrowseTags(conn);
    }
else if (sameString(command, "browseLabs"))
    {
    doBrowseLabs(conn);
    }
else if (sameString(command, "browseDataSets"))
    {
    doBrowseDatasets(conn);
    }
else if (sameString(command, "browseFormats"))
    {
    doBrowseFormat(conn);
    }
else if (sameString(command, "oneFile"))
    {
    doOneFile(conn);
    }
else if (sameString(command, "oneTag"))
    {
    doOneTag(conn);
    }
else if (sameString(command, "help"))
    {
    doHelp(conn);
    }
else if (sameString(command, "dataSetMetaTree"))
    {
    doDataSetMetaTree(conn);
    }
else if (sameString(command, "test"))
    {
    doTestPage(conn);
    }
else
    {
    printf("unrecognized command %s<BR>\n", command);
    }
}

void doMiddle()
/* Menu bar has been drawn.  We are in the middle of first section. */
{
struct sqlConnection *conn = sqlConnect(cdwDatabase);
setCdwUser(conn);
dispatch(conn);
sqlDisconnect(&conn);
}

static void doSendMenubar()
/* print http header and menu bar string */
{
oldVars = hashNew(0);
cart = cartAndCookieWithHtml(hUserCookie(), excludeVars, oldVars, TRUE);
//cartEmptyShell(localWebWrap, hUserCookie(), excludeVars, oldVars);
//puts("Content-Type: text/html\n\n");
char* mb = cdwLocalMenuBar(cart, TRUE);
puts(mb);
}

void localWebStartWrapper(char *titleString)
/* Output a HTML header with the given title.  Start table layout.  Draw menu bar. */
{
/* Do html header. We do this a little differently than web.c routines, mostly
 * in that we are strict rather than transitional HTML 4.01 */
    {
    puts("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" "
             "\"http://www.w3.org/TR/html4/strict.dtd\">");
    puts("<HTML><HEAD>\n");
    puts(getCspMetaHeader());
    webPragmasEtc();
    printf("<TITLE>%s</TITLE>\n", titleString);
    webIncludeResourceFile("HGStyle.css");
    webIncludeResourceFile("jquery-ui.css");
    webIncludeResourceFile("nice_menu.css");
    webIncludeResourceFile("jquery.ui.colorPicker.css");
    jsIncludeFile("jquery.js", NULL);
    jsIncludeFile("jquery.plugins.js", NULL);
    jsIncludeFile("jquery-ui.js", NULL);
    jsIncludeFile("jquery.watermark.js", NULL);
    jsIncludeFile("jquery.ui.colorPicker.js", NULL);
    jsIncludeFile("ajax.js", NULL);
    jsIncludeFile("d3pie.min.js", NULL);
    jsIncludeFile("dagre-d3.js", NULL);
    printf("<script src=\"//cdnjs.cloudflare.com/ajax/libs/d3/3.4.4/d3.min.js\"></script>");
    printf("<script src=\"//cdnjs.cloudflare.com/ajax/libs/bowser/1.6.1/bowser.min.js\"></script>");
    printf("</HEAD>\n");
    printBodyTag(stdout);
    }

webStartSectionTables();    // Start table layout code
puts(cdwLocalMenuBar(cart, FALSE));	    // Menu bar after tables open but before first section
webFirstSection(titleString);	// Open first section
webPushErrHandlers();	    // Now can do improved error handler
}


void localWebWrap(struct cart *theCart)
/* We got the http stuff handled, and a cart.  Now wrap a web page around it. */
{
cart = theCart;
localWebStartWrapper("CIRM Stem Cell Hub Data Browser V0.58");
pushWarnHandler(htmlVaWarn);
doMiddle();
webEndSectionTables();
jsInlineFinish(); 
printf("</BODY></HTML>\n");
}

void initFields()
/* initialize fields */
{
visibleFacetFields = getCdwSetting("cdwFacetFields", FILEFACETFIELDS);
char temp[1024];
safef(temp, sizeof temp, "%s,%s", FILEFIELDS, visibleFacetFields);
fileTableFields = cloneString(temp);

// filter fields against fields in current facet table
struct sqlConnection *conn = sqlConnect(cdwDatabase);
struct slName *fNames = sqlFieldNames(conn, getCdwTableSetting("cdwFileFacets"));
sqlDisconnect(&conn);

struct dyString *dy = newDyString(128);
char *fieldNames[128];
char *tempFileTableFields = cloneString(fileTableFields);
int fieldCount = chopString(tempFileTableFields, ",", fieldNames, ArraySize(fieldNames));
int i;
for (i = 0; i<fieldCount; i++)
    {
    if (slNameInList(fNames, fieldNames[i]))
	{
	if (dy->stringSize > 0)
	    dyStringAppendC(dy, ',');
	dyStringAppend(dy, fieldNames[i]);
	}
    }
freeMem(fileTableFields);
fileTableFields = dyStringCannibalize(&dy);

}

int main(int argc, char *argv[])
/* Process command line. */
{
long enteredMainTime = clock1000();
boolean isFromWeb = cgiIsOnWeb();
if (!isFromWeb && !cgiSpoof(&argc, argv))
    usage();
dnaUtilOpen();
oldVars = hashNew(0);
char *cdwCmd = cgiOptionalString("cdwCommand");
isPublicSite = cfgOptionBooleanDefault("cdw.siteIsPublic", FALSE);

initFields();

if (sameOk(cdwCmd, "downloadUrls"))
    doDownloadUrls();
else if (sameOk(cdwCmd, "menubar"))
    doSendMenubar();
else
    cartEmptyShell(localWebWrap, hUserCookie(), excludeVars, oldVars);
cgiExitTime("cdwWebBrowse", enteredMainTime);
return 0;
}

