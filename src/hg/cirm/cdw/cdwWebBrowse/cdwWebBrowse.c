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
#include "cdwDataset.h"

/* Global vars */
struct cart *cart;	// User variables saved from click to click
struct hash *oldVars;	// Previous cart, before current round of CGI vars folded in
struct cdwUser *user;	// Our logged in user if any

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwWebBrowse is a cgi script not meant to be run from command line.\n"
  );
}

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

static int matchCount = 0;
static boolean doSelect = FALSE;

static void rMatchesToRa(struct tagStorm *tags, struct tagStanza *list, 
    struct rqlStatement *rql, struct lm *lm)
/* Recursively traverse stanzas on list outputting matching stanzas as ra. */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    if (rql->limit < 0 || rql->limit > matchCount)
	{
	if (stanza->children)
	    rMatchesToRa(tags, stanza->children, rql, lm);
	else    /* Just apply query to leaves */
	    {
	    if (cdwRqlStatementMatch(rql, stanza, lm))
		{
		++matchCount;
		if (doSelect)
		    {
		    struct slName *field;
		    for (field = rql->fieldList; field != NULL; field = field->next)
			{
			char *val = tagFindVal(stanza, field->name);
			if (val != NULL)
			    printf("%s\t%s\n", field->name, val);
			}
		    printf("\n");
		    }
		}
	    }
	}
    }
}

void showMatchingAsRa(char *rqlQuery, int limit, struct tagStorm *tags)
/* Show stanzas that match query */
{
struct dyString *dy = dyStringCreate("%s", rqlQuery);
int maxLimit = 10000;
if (limit > maxLimit)
    limit = maxLimit;
struct rqlStatement *rql = rqlStatementParseString(dy->string);

/* Get list of all tag types in tree and use it to expand wildcards in the query
 * field list. */
struct slName *allFieldList = tagStormFieldList(tags);
slSort(&allFieldList, slNameCmpCase);
rql->fieldList = wildExpandList(allFieldList, rql->fieldList, TRUE);

/* Traverse tag tree outputting when rql statement matches in select case, just
 * updateing count in count case. */
doSelect = sameWord(rql->command, "select");
if (doSelect)
    rql->limit = limit;
struct lm *lm = lmInit(0);
rMatchesToRa(tags, tags->forest, rql, lm);
if (sameWord(rql->command, "count"))
    printf("%d\n", matchCount);

}

int labCount(struct tagStorm *tags)
/* Return number of different labs in tags */
{
struct hash *hash = tagStormCountTagVals(tags, "lab");
int count = hash->elCount;
hashFree(&hash);
return count;
}


void wrapFileName(struct fieldedTable *table, struct fieldedRow *row, 
    char *field, char *val, char *shortVal, void *context)
/* Write out wrapper that links us to something nice */
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
printf("%s=%s", "cdwFile_filter", escapedQuery);
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
struct hash *hash = tagStormCountTagVals(tags, tag);
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

void doOneFile(struct sqlConnection *conn)
/* Put up a page with info on one file */
{
char *idTag = cartUsualString(cart, "cdwFileTag", "accession");
char *idVal = cartString(cart, "cdwFileVal");
char query[512];
sqlSafef(query, sizeof(query), "select * from cdwFileTags where %s='%s'", idTag, idVal);
struct sqlResult *sr = sqlGetResult(conn, query);
struct slName *el, *list = sqlResultFieldList(sr);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *fileName = mustFindFieldInRow("file_name", list, row);
    char *fileSize = mustFindFieldInRow("file_size", list, row);
    char *format = mustFindFieldInRow("format", list, row);
    long long size = atoll(fileSize);
    printf("Tags associated with %s a %s format file of size ", fileName, format);
    printLongWithCommas(stdout, size);
    printf("<BR>\n");


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
}

struct dyString *customTextForFile(struct sqlConnection *conn, struct cdwTrackViz *viz)
/* Create custom track text */
{
struct dyString *dy = dyStringNew(0);
dyStringPrintf(dy, "track name=\"%s\" ", viz->shortLabel);
dyStringPrintf(dy, "description=\"%s\" ", viz->longLabel);
char *host = hHttpHost();
dyStringPrintf(dy, "bigDataUrl=http://%s/cdw/%s type=%s", host, viz->bigDataFile, viz->type);
return dy;
}

struct cdwTrackViz *cdwTrackVizFromFileId(struct sqlConnection *conn, long long fileId)
/* Return cdwTrackViz if any associated with file ID */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from cdwTrackViz where fileId=%lld", fileId);
return cdwTrackVizLoadByQuery(conn, query);
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
printf("<A HREF=\"../cgi-bin/hgTracks");
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
    if (cdwCheckAccess(conn, ef, user, cdwAccessRead))
	printed = wrapTrackVis(conn, vf, shortVal);
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
sqlDyStringPrintf(query, "%s", "");   // Get header correct
char *separator = "select ";  // This will get printed before first one
for (field = fieldList; field != NULL; field = field->next)
    {
    dyStringPrintf(query, "%s%s", separator, field->name);
    separator = ",";
    }

/* Put where on it to limit it to accessible files */
dyStringPrintf(query, " from cdwFileTags where file_id in (");

struct cdwFile *ef;
separator = "";
for (ef = efList; ef != NULL; ef = ef->next)
    {
    dyStringPrintf(query, "%s%u", separator, ef->id);
    separator = ",";
    }
dyStringPrintf(query, ")");

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


void accessibleFilesTable(struct cart *cart, struct sqlConnection *conn, 
    char *searchString, char *fields, char *from, char *initialWhere,  
    char *returnUrl, char *varPrefix, int maxFieldWidth, 
    struct hash *tagOutWrappers, void *wrapperContext,
    boolean withFilters, char *itemPlural, int pageSize)
{
/* Do precalculation for quicker auth check */
int userId = 0;
if (user != NULL)
    userId = user->id;

/* Get list of files we are authorized to see, and return early if empty */
struct cdwFile *ef, *efList = cdwAccessibleFileList(conn, user);
if (efList == NULL)
    {
    if (user != NULL && user->isAdmin)
	printf("<BR>The file database is empty.");
    else
	printf("<BR>Unfortunately there are no %s you are authorized to see.", itemPlural);
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
    struct trixSearchResult *tsr, *tsrList = trixSearch(trix, wordCount, words, TRUE);
    for (tsr = tsrList; tsr != NULL; tsr = tsr->next)
        {
	intValTreeAdd(searchPassTree, sqlUnsigned(tsr->itemId), tsr);
	}
    }

/* Loop through all files constructing a SQL where clause that restricts us
 * to just the ones that we're authorized to hit, and that also pass initial where clause
 * if any. */
struct dyString *where = dyStringNew(0);
if (!isEmpty(initialWhere))
     dyStringPrintf(where, "(%s) and ", initialWhere);
dyStringPrintf(where, "file_id in (0");	 // initial 0 never found, just makes code smaller
int accessCount = 0;
for (ef = efList; ef != NULL; ef = ef->next)
    {
    if (searchPassTree == NULL || intValTreeFind(searchPassTree, ef->id) != NULL)
	{
	dyStringPrintf(where, ",%u", ef->id);
	++accessCount;
	}
    }
dyStringAppendC(where, ')');

/* Let the sql system handle the rest.  Might be one long 'in' clause.... */
struct hash *suggestHash = accessibleSuggestHash(conn, fields, efList);
webFilteredSqlTable(cart, conn, fields, from, where->string, returnUrl, varPrefix, maxFieldWidth,
    tagOutWrappers, wrapperContext, withFilters, itemPlural, pageSize, suggestHash);

/* Clean up and go home. */
cdwFileFreeList(&efList);
dyStringFree(&where);
rbTreeFree(&searchPassTree);
}

char *showSearchControl(char *varName, char *itemPlural)
/* Put up the search control text and stuff. Returns current search string. */
{
char *varVal = cartUsualString(cart, varName, "");
printf("Search <input name=\"%s\" type=\"text\" id=\"%s\" value=\"%s\" size=60>", 
    varName, varName, varVal);
printf("&nbsp;");
printf("<img src=\"../images/magnify.png\">\n");
printf("<script>\n");
printf("$(function () {\n");
printf("  $('#%s').watermark(\"type in words or starts of words to find specific %s\");\n", 
    varName, itemPlural);
printf("});\n");
printf("</script>\n");
return varVal;
}

void doBrowseFiles(struct sqlConnection *conn)
/* Print list of files */
{
printf("<FORM ACTION=\"../cgi-bin/cdwWebBrowse\" METHOD=GET>\n");
cartSaveSession(cart);
cgiMakeHiddenVar("cdwCommand", "browseFiles");

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
accessibleFilesTable(cart, conn, searchString,
  "file_name,file_size,ucsc_db,lab,assay,data_set_id,output,format,read_size,item_count,body_part",
  "cdwFileTags", where, 
  returnUrl, "cdwBrowseFiles",
  18, wrappers, conn, TRUE, "files", 100);
printf("</FORM>\n");
}

void doBrowseTracks(struct sqlConnection *conn)
/* Print list of files */
{
printf("<FORM ACTION=\"../cgi-bin/cdwWebBrowse\" METHOD=GET>\n");
cartSaveSession(cart);
cgiMakeHiddenVar("cdwCommand", "browseTracks");

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
    "enriched_in,body_part,submit_file_name",
    "cdwFileTags,cdwTrackViz", where, 
    returnUrl, "cdw_track_filter", 
    22, wrappers, conn, TRUE, "tracks", 100);
printf("</FORM>\n");
}

struct hash* loadDatasetDescs(struct sqlConnection *conn)
/* Load cdwDataset table and return hash with name -> cdwDataset */
{
char* query = NOSQLINJ "SELECT * FROM cdwDataset;";
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

void doBrowsePopularTags(struct sqlConnection *conn, char *tag)
/* Print list of most popular tags of type */
{
struct tagStorm *tags = cdwUserTagStorm(conn, user);
struct hash *hash = tagStormCountTagVals(tags, tag);
struct hashEl *hel, *helList = hashElListHash(hash);
slSort(&helList, hashElCmpIntValDesc);
int valIx = 0, maxValIx = 100;
printf("<UL>\n");

struct hash *descs = loadDatasetDescs(conn);

for (hel = helList; hel != NULL && ++valIx <= maxValIx; hel = hel->next)
    {
    printf("<LI>\n");
    struct cdwDataset *dataset = hashFindVal(descs, hel->name);

    char *label;
    char *desc;
    if (dataset != NULL)
        {
        label = dataset->label;
        desc = dataset->description;
        }
    else
        {
        label = hel->name;
        desc = "Missing description in table cdw.cdwDataset";
        }

    char *datasetId = hel->name;
    printf("<B><A href=\"../cdwDatasets/%s/\">%s</A></B><BR>", datasetId, label);
    printf("%s (<A HREF=\"cdwWebBrowse?cdwCommand=browseFiles&cdwBrowseFiles_f_data_set_id=%s&%s\">%d files</A>)", desc, datasetId, cartSidUrlString(cart), ptToInt(hel->val));
    printf("</LI>\n");
    cdwDatasetFree(&dataset);
    }
printf("</UL>\n");
hashFree(&descs);
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
    sqlSafef(query, sizeof(query), "select count(*) from cdwFileTags where format='%s'", 
	format->name);
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
    sqlSafef(query, sizeof(query), "select count(*) from cdwFileTags where lab='%s'", lab->name);
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

void doQuery(struct sqlConnection *conn)
/* Print up query page */
{
/* Do stuff that keeps us here after a routine submit */
printf("<FORM ACTION=\"../cgi-bin/cdwWebBrowse\" METHOD=GET>\n");
cartSaveSession(cart);
cgiMakeHiddenVar("cdwCommand", "query");

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
printf("Enter a SQL-like query below. ");
printf("In the box after 'select' you can put in a list of tag names including wildcards. ");
printf("In the box after 'where' you can put in filters based on boolean operations between ");
printf("fields and constants. <BR>");
/** Write out select [  ] from files whre [  ] limit [ ] <submit> **/
printf("select ");
cgiMakeTextVar(fieldsVar, fields, 40);
printf("from files ");
printf("where ");
cgiMakeTextVar(whereVar, where, 40);
printf(" limit ");
cgiMakeIntVar(limitVar, limit, 7);
cgiMakeSubmitButton();



printf("<PRE><TT>");
printLongWithCommas(stdout, matchCount);
printf(" files match\n\n");
showMatchingAsRa(rqlQuery->string, limit, tags);
printf("</TT></PRE>\n");
printf("</FORM>\n");
}

void tagSummaryRow(struct fieldedTable *table, struct tagStorm *tags, char *tag)
/* Print out row in a high level tag counting table */
{
/* Do analysis hash */
struct hash *hash = tagStormCountTagVals(tags, tag);

/* Convert count of distinct values to string */
int valCount = hash->elCount;
if (valCount > 0)
    {
    char valCountString[32];
    safef(valCountString, sizeof(valCountString), "%d", valCount);

    /* Convert count of files using tag to string */
    int fileCount = sumCounts(hash);
    char fileCountString[32];
    safef(fileCountString, sizeof(fileCountString), "%d", fileCount);

    struct dyString *dy = printPopularTags(hash, 110);

    /* Add data to fielded table */
    char *row[4];
    row[0] = tag;
    row[1] = valCountString;
    row[2] = dy->string;
    row[3] = fileCountString;
    fieldedTableAdd(table, row, ArraySize(row), 0);

    /* Clean up */
    dyStringFree(&dy);
    }
hashFree(&hash);
}

void drawPrettyPieGraph(struct slPair *data, char *id, char *title, char *subtitle)
/* Draw a pretty pie graph using D3. Import D3 and D3pie before use. */
{
// Some D3 administrative stuff, the title, subtitle, sizing etc. 
printf("<script>\nvar pie = new d3pie(\"%s\", {\n\"header\": {", id);
if (title != NULL)
    {
    printf("\"title\": { \"text\": \"%s\",", title);
    printf("\"fontSize\": 16,");
    printf("\"font\": \"open sans\",},");
    }
if (subtitle != NULL)
    {
    printf("\"subtitle\": { \"text\": \"%s\",", subtitle);
    printf("\"color\": \"#999999\",");
    printf("\"fontSize\": 10,");
    printf("\"font\": \"open sans\",},");
    }
printf("\"titleSubtitlePadding\":9 },\n");
printf("\"footer\": {\"color\": \"#999999\",");
printf("\"fontSize\": 10,");
printf("\"font\": \"open sans\",");
printf("\"location\": \"bottom-left\",},\n");
printf("\"size\": { \"canvasWidth\": 270, \"canvasHeight\": 220},\n");
printf("\"data\": { \"sortOrder\": \"value-desc\", \"content\": [\n");
struct slPair *start = NULL;
float colorOffset = 1;
int totalFields =  slCount(data);
for (start=data; start!=NULL; start=start->next)
    {
    float currentColor = colorOffset/totalFields;
    struct rgbColor color = saturatedRainbowAtPos(currentColor);
    char *temp = start->val;
    printf("\t{\"label\": \"%s\",\n\t\"value\": %s,\n\t\"color\": \"rgb(%i,%i,%i)\"}", 
	start->name, temp, color.r, color.b, color.g);
    if (start->next!=NULL) 
	printf(",\n");
    ++colorOffset;
    }
printf("]},\n\"labels\": {");
printf("\"outer\":{\"pieDistance\":20},");
printf("\"inner\":{\"hideWhenLessThanPercentage\":5},");
printf("\"mainLabel\":{\"fontSize\":11},");
printf("\"percentage\":{\"color\":\"#ffffff\", \"decimalPlaces\": 0},");
printf("\"value\":{\"color\":\"#adadad\", \"fontSize\":11},");
printf("\"lines\":{\"enabled\":true},},\n");
printf("\"effects\":{\"pullOutSegmentOnClick\":{");
printf("\"effect\": \"linear\",");
printf("\"speed\": 400,");
printf("\"size\": 8}},\n");
printf("\"misc\":{\"gradient\":{");
printf("\"enabled\": true,");
printf("\"percentage\": 100}}});");
printf("</script>\n");

}

void pieOnTag(struct tagStorm *tags, char *tag, char *divId)
/* Write a pie chart base on the values of given tag in storm */
{
/* Do analysis hash */
struct hash *hash = tagStormCountTagVals(tags, tag);

/* Convert count of distinct values to string */
int valCount = hash->elCount;
if (valCount > 0)
    {
    /* Get a list of values, sorted by how often they occur */
    struct hashEl *hel, *helList = hashElListHash(hash);
    slSort(&helList, hashElCmpIntValDesc);

    /* Convert hashEl to slPair the way the pie charter wants */
    struct slPair *pairList = NULL;
    for (hel = helList; hel != NULL; hel = hel->next)
        {
	char numString[32];
	safef(numString, sizeof(numString), "%d", ptToInt(hel->val));
	slPairAdd(&pairList, hel->name, cloneString(numString));
	}
    slReverse(&pairList);

    drawPrettyPieGraph(pairList, divId, tag, NULL);
    }
hashFree(&hash);
}

char *tagPopularityFields[] = { "tag name", "vals", "popular values (files)...", "files",};

void doHome(struct sqlConnection *conn)
/* Put up home/summary page */
{
struct tagStorm *tags = cdwTagStorm(conn);
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
// printLongWithCommas(stdout, totalBytes);
printWithGreekByte(stdout, totalBytes);
printf(" of data in ");
sqlSafef(query, sizeof(query),
    "select count(*) from cdwFile,cdwValidFile where cdwFile.id=cdwValidFile.fileId "
    " and (errorMessage = '' or errorMessage is null)"
    );
long long fileCount = sqlQuickLongLong(conn, query);
printLongWithCommas(stdout, fileCount);
printf(" files");
printf(" from %d labs. ", labCount(tags));
printf("You have access to ");
printLongWithCommas(stdout, cdwCountAccessible(conn, user));
printf(" files.<BR>\n");
printf("Try using the browse menu on files or tracks. ");
printf("The query link allows simple SQL-like queries of the metadata.");
printf("<BR>\n");

/* Print out some pie charts on important fields */
static char *pieTags[] = 
   // {"data_set_id"};
    {"lab", "format", "assay", };
int i;
printf("<TABLE style=\"display:inline\"><TR>\n");
for (i=0; i<ArraySize(pieTags); ++i)
    {
    char *field = pieTags[i];
    char pieDivId[64];
    safef(pieDivId, sizeof(pieDivId), "pie_%d", i);
    printf("<TD id=\"%s\"><TD>", pieDivId);
    pieOnTag(tags, field, pieDivId);
    }
printf("</TR></TABLE>\n");
printf("<CENTER><I>charts are based on proportion of files in each category</I></CENTER>\n");
printf("</td></tr></table>\n");


/* Print out high level tags table */
static char *highLevelTags[] = 
    {"data_set_id", "lab", "assay", "format", "read_size",
    "body_part", "species"};

struct fieldedTable *table = fieldedTableNew("Important tags", tagPopularityFields, 
    ArraySize(tagPopularityFields));
for (i=0; i<ArraySize(highLevelTags); ++i)
    tagSummaryRow(table, tags, highLevelTags[i]);
char returnUrl[PATH_LEN*2];
safef(returnUrl, sizeof(returnUrl), "../cgi-bin/cdwWebBrowse?%s", cartSidUrlString(cart) );
webSortableFieldedTable(cart, table, returnUrl, "cdwHome", 0, NULL, NULL);

printf("This table is a summary of important metadata tags and number of files they ");
printf("are attached to. Use browse tags menu to see all tags.");

printf("<BR>\n");
printf("<center>");
printf("</center>");

tagStormFree(&tags);
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
    tagSummaryRow(table, tags, tag->name);
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
printf("This being a prototype, there's not much help available.  Try clicking and hovering over the Browse link on the top bar to expose a menu. The trickiest part of the system is the query link.");
printf("The query link has you type in a SQL-like query. ");
printf("Try 'select * from files where accession' to get all metadata tags ");
printf("from files that have passed basic format validations. Instead of '*' you ");
printf("could use a comma separated list of tag names. ");
printf("Instead of 'accession' you could put in a boolean expression involving field names and ");
printf("constants. String constants need to be surrounded by quotes - either single or double.");
printf("<BR><BR>");
}

void doTest(struct sqlConnection *conn)
/* Test out something */
{
printf("<FORM ACTION=\"../cgi-bin/cdwWebBrowse\" METHOD=GET>\n");
cartSaveSession(cart);
cgiMakeHiddenVar("cdwCommand", "test");

char *id = "test_id_12";

/* Print out input control and some text. */
// printf("<input type=\"text\" class=\"%s\" name=\"cdw_f_%s\" id=\"f_%s\" size=12>", 
  //   "positionInput", field, field);
printf("Hello from the test page");
printf("<input type=\"text\" id=\"%s\">", id);

/* Print out javascript to wrap data picker around this */
printf("<script>");
printf("$(function () {\n");
printf("  $('#%s').datepicker();\n", id);
printf("});\n");
printf("</script>");

/* Try a menu, why not */
printf("<BR>\n");
printf("<ul id=\"menu_xyz\">\n");
printf("<li id=\"xyz_1\">xyz 1</li>\n");
printf("<li id=\"xyz_2\">xyz 2</li>\n");
printf("</ul>\n");

printf("<script>\n");
printf("$(function () {\n");
printf("$('#menu_xyz').menu({\n");
printf("  select: function(event, ui) {alert('hi');}\n");
printf("});\n");
printf("});\n");
printf("</script>\n");
#ifdef SOON
#endif /* SOON */

printf("<button id='just_a_button'>say hello button</button>\n");
printf("<script>\n");
printf("$(function () {\n");
printf("  $('#just_a_button').click(function (event) {\n");
printf("    alert(\"A hi that doesn't submit\");\n");
printf("    event.preventDefault();\n");
printf("    event.stopPropagation();\n");
printf("  });\n");
printf("});\n");
printf("</script>\n");

printf("<button>submit me</button>\n");
printf("<BR>");

char *varName = "cdw_test_foo_23";
char *val = cartUsualString(cart, varName, "");
printf("<input name=\"%s\" type=\"text\" id=\"watered\" value=\"%s\">", varName, val);
printf("<script>\n");
printf("$(function () {\n");
printf("  $('#watered').watermark(\"why hello there\");\n");
printf("});\n");
printf("</script>\n");

char *colVar = "cdw_test_col";
char *colVal = cartUsualString(cart, colVar, "xyz");
printf("<input name=\"%s\" type=\"text\" id=\"%s\" value=\"%s\">", colVar, colVar, colVal);
#ifdef SOON
printf("<script>\n");
printf("$(function () {\n");
printf("  $('#%s').colorPicker();\n", colVar);
printf("});\n");
printf("</script>\n");
#endif /* SOON */

/* Make a pie chart */
    {
    struct slPair *dataList = NULL;
    slPairAdd(&dataList, "A", "2");
    slPairAdd(&dataList, "B", "3");
    slPairAdd(&dataList, "C", "4");
    slPairAdd(&dataList, "D", "5");
    printf("<DIV id=\"pie1\">");
    drawPrettyPieGraph(dataList, "pie1", "abcd", "bigger and bigger");
    printf("</DIV>\n");
    }

printf("</FORM>");
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
else if (sameString(command, "query"))
    {
    doQuery(conn);
    }
else if (sameString(command, "browseFiles"))
    {
    doBrowseFiles(conn);
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
    doBrowsePopularTags(conn, "data_set_id");
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
else if (sameString(command, "test"))
    {
    doTest(conn);
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
char *userName = wikiLinkUserName();
if (userName != NULL)
    {
    /* Look up email vial hgCentral table */
    struct sqlConnection *cc = hConnectCentral();
    char query[512];
    sqlSafef(query, sizeof(query), "select email from gbMembers where userName='%s'", userName);
    char *email = sqlQuickString(cc, query);
    hDisconnectCentral(&cc);
    user = cdwUserFromEmail(conn, email);
    }
dispatch(conn);
sqlDisconnect(&conn);
}

struct dyString *getLoginBits()
/* Get a little HTML fragment that has login/logout bit of menu */
{
/* Construct URL to return back to this page */
char *command = cartUsualString(cart, "cdwCommand", "home");
char *sidString = cartSidUrlString(cart);
char returnUrl[PATH_LEN*2];
safef(returnUrl, sizeof(returnUrl), "http%s://%s/cgi-bin/cdwWebBrowse?cdwCommand=%s&%s",
    cgiAppendSForHttps(), cgiServerNamePort(), command, sidString );
char *encodedReturn = cgiEncode(returnUrl);

/* Write a little html into loginBits */
struct dyString *loginBits = dyStringNew(0);
dyStringAppend(loginBits, "<li id=\"query\"><a href=\"");
char *userName = wikiLinkUserName();
if (userName == NULL)
    {
    dyStringPrintf(loginBits, "../cgi-bin/hgLogin?hgLogin.do.displayLoginPage=1&returnto=%s&%s",
	    encodedReturn, sidString);
    dyStringPrintf(loginBits, "\">Login</a></li>");
    }
else
    {
    dyStringPrintf(loginBits, "../cgi-bin/hgLogin?hgLogin.do.displayLogout=1&returnto=%s&%s",
	    encodedReturn, sidString);
    dyStringPrintf(loginBits, "\">Logout %s</a></li>", userName);
    }

/* Clean up and go home */
freez(&encodedReturn);
return loginBits;
}

static char *localMenuBar()
/* Return menu bar string */
{
struct dyString *loginBits = getLoginBits();

// menu bar html is in a stringified .h file
struct dyString *dy = dyStringNew(4*1024);
dyStringPrintf(dy, 
#include "cdwNavBar.h"
       , loginBits->string);


return menuBarAddUiVars(dy->string, "/cgi-bin/cdw", cartSidUrlString(cart));
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
    printf("<script src=\"//cdnjs.cloudflare.com/ajax/libs/d3/3.4.4/d3.min.js\"></script>");
    printf("</HEAD>\n");
    printBodyTag(stdout);
    }

webStartSectionTables();    // Start table layout code
puts(localMenuBar());	    // Menu bar after tables open but before first section
webFirstSection(titleString);	// Open first section
webPushErrHandlers();	    // Now can do improved error handler
}


void localWebWrap(struct cart *theCart)
/* We got the http stuff handled, and a cart.  Now wrap a web page around it. */
{
cart = theCart;
localWebStartWrapper("CIRM Stem Cell Hub Data Browser V0.49");
pushWarnHandler(htmlVaWarn);
doMiddle();
webEndSectionTables();
printf("</BODY></HTML>\n");
}


char *excludeVars[] = {"cdwCommand", "submit", NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
boolean isFromWeb = cgiIsOnWeb();
if (!isFromWeb && !cgiSpoof(&argc, argv))
    usage();
dnaUtilOpen();
oldVars = hashNew(0);
cartEmptyShell(localWebWrap, hUserCookie(), excludeVars, oldVars);
return 0;
}

