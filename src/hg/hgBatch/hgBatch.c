/* hgBatch - Do batch queries of genome database over web. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "web.h"
#include "cart.h"
#include "hdb.h"
#include "dbDb.h"
#include "web.h"

char *db;	/* Current database. */
char *organism;	/* Current organism. */
char *track;	/* Current track. */
char *keyField; /* Field to use as key in current track. */
struct cart *cart;	/* The user's ui state. */
struct hash *oldVars = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgBatch - Do batch queries of genome database over web\n"
  "usage:\n"
  "   hgBatch XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

char *autoSubmit = "onchange=\"document.mainForm.submit();\"";

void printTrackDropList(char *db, char *javascript)
/* Print tracks for this database */
{
struct trackDb *trackList = hTrackDb(NULL), *t;
int trackCount = slCount(trackList);
char **trackLabels, **trackNames;
int i;
char *selected = NULL;

AllocArray(trackLabels, trackCount);
AllocArray(trackNames, trackCount);
for (t = trackList, i=0; t != NULL; t = t->next, ++i)
    {
    trackLabels[i] = t->shortLabel;
    trackNames[i] = t->tableName;
    if (sameString(track, t->tableName))
        selected = t->shortLabel;
    }
if (selected == NULL)
    {
    selected = trackLabels[0];
    track = trackNames[0];
    }
cgiMakeDropListFull("hgbTrack", trackLabels, trackNames, 
	trackCount, selected, javascript);
}


void printFieldList(char *db, char *javascript)
/* Print key field list. */
{
struct slName *fieldList = NULL, *field;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char query[256];
char **row;
char tableName[256];
char defaultChrom[64];
struct hTableInfo *hti;
int i, fieldCount = 0;
char **fieldNames;
boolean keyExists = FALSE;

/* Get info on track and figure out name of a table
 * associated with it. */
hFindDefaultChrom(db, defaultChrom);
hti = hFindTableInfo(defaultChrom, track);
if (hti->isSplit)
   sprintf(tableName, "%s_%s", defaultChrom, track);
else
   sprintf(tableName, "%s", track);
if (keyField == NULL)
    keyField = hti->nameField;

/* Get all the fields from database into fieldList. */
snprintf(query, sizeof(query), "DESCRIBE %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    field = newSlName(row[0]);
    if (sameString(field->name, keyField))
        keyExists = TRUE;
    slAddHead(&fieldList, field);
    ++fieldCount;
    }
sqlFreeResult(&sr);
slReverse(&fieldList);

/* Move fields into an array. */
AllocArray(fieldNames, fieldCount);
for (field = fieldList, i=0; field != NULL; field = field->next, ++i)
    fieldNames[i] = field->name;

/* Make drop-down. */
if (!keyExists)
    keyField = hti->nameField;
cgiMakeDropListFull("hgbKeyField", fieldNames, fieldNames, 
	fieldCount, keyField, javascript);
}

void selectTrack()
/* Put up a form that lets them choose organism, assembly, track
 * and key field. */
{
char *onChangeOrg = "onchange=\"document.orgForm.org.value = document.mainForm.org.options[document.mainForm.org.selectedIndex].value; document.orgForm.submit();\"";


// puts("<FORM ACTION=\"/cgi-bin/hgBatch\" NAME=\"mainForm\" METHOD=\"POST\" ENCTYPE=\"multipart/form-data\">\n");
puts("<FORM ACTION=\"/cgi-bin/hgBatch\" NAME=\"mainForm\" METHOD=\"GET\">\n");

printf("organism: ");
printGenomeListHtml(db, onChangeOrg);
printf("assembly: ");
printAssemblyListHtmlExtra(db, autoSubmit);
printf("track: ");
printTrackDropList(db, autoSubmit);
printf("key field: ");
printFieldList(db, autoSubmit);
printf("<BR>");
printf("<CENTER>");
cgiMakeButton("hgb.pasteKeys", "Paste Keys");
printf(" ");
cgiMakeButton("hgb.uploadKeys", "Upload Keys");
printf("</CENTER>");
cartSaveSession(cart);
printf("</FORM>");

/* Make secondary form that just helps us manage Javascript
 * directed submissions. */
printf("<FORM ACTION=\"/cgi-bin/hgBatch\" METHOD=\"GET\" NAME=\"orgForm\">");
printf("<input type=\"hidden\" name=\"org\" value=\"%s\">", organism);
cartSaveSession(cart);
printf("</FORM>\n");
}

void pasteForm()
/* Put up form that lets them paste in keys. */
{
puts("Please paste in a list of keys to match.  These may include "
       "* and ? wildcard characters.");
printf("<FORM ACTION=\"/cgi-bin/hgBatch\" METHOD=\"POST\">");
puts("<TEXTAREA NAME=hgb.userKeys ROWS=10 COLS=80></TEXTAREA><BR>\n");
puts("<CENTER>");
puts(" <INPUT TYPE=SUBMIT Name=hgb.showPasteResults VALUE=\"Submit\"><P>\n");
puts("</CENTER>");
cartSaveSession(cart);
printf("</FORM>\n");
}

void uploadForm()
/* Put up upload form. */
{
puts("<FORM ACTION=\"/cgi-bin/hgBatch\" METHOD=\"POST\" ENCTYPE=\"multipart/form-data\">\n");
printf("Please enter the name of a file in your computer containing a space, tab, or ");
printf("line separated list of the key names you want to look up in the database.");
printf("Unlike in the paste option, wildcards don't work in this list.<BR>");
puts("Upload sequence: <INPUT TYPE=FILE NAME=\"hgb.userKeys\">");
puts("<INPUT TYPE=SUBMIT Name=hgb.showUploadResults VALUE=\"Submit File\"><P>\n");
cartSaveSession(cart);
puts("</FORM>\n");
}

boolean anyWildMatch(char *s, struct slName *wildList)
/* Return TRUE if s matches anything on wildList */
{
struct slName *wild;
for (wild = wildList; wild != NULL; wild = wild->next)
    {
    if (wildMatch(wild->name, s))
        return TRUE;
    }
return FALSE;
}

void findKeyIndex(char *table, boolean printHead, int *retIx, int *retCount)
/* Find index of key field and optionally print it. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct slName *fieldList = NULL, *field;
char query[256];
int keyIx = -1, fieldCount = 0;

printf("<TT><PRE>");

/* Get list of fields, calculate index of key field in row,
 * and optionally print field names. */
sprintf(query, "describe %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    field = slNameNew(row[0]);
    slAddHead(&fieldList, field);
    if (sameString(field->name, keyField))
        keyIx = fieldCount;
    ++fieldCount;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&fieldList);
if (keyIx < 0)
    errAbort("No field %s in %s", keyField, table);
if (printHead)
    {
    printf("#");
    for (field = fieldList; field != NULL; field = field->next)
        printf("%s\t", field->name);
    printf("\n");
    }
slFreeList(&fieldList);
*retIx = keyIx;
*retCount = fieldCount;
}

void showWildMatch(char *table, struct slName *wildList, boolean showFields)
/* Print parts of table who's keys match any wildcard on list. */
{
struct slName *wild;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
int keyIx, fieldCount, i;

findKeyIndex(table, showFields, &keyIx, &fieldCount);

/* Query table and print out all matching things. */
sprintf(query, "select * from %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (anyWildMatch(row[keyIx], wildList))
        {
	printf("%s", row[0]);
	for (i = 1; i<fieldCount; ++i)
	    printf("\t%s", row[i]);
	printf("\n");
	}
    }
hFreeConn(&conn);
}


void showMatch(char *table, struct hash *hash, boolean showFields)
/* Print parts of table who's keys are in hash. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
int keyIx, fieldCount, i;

findKeyIndex(table, showFields, &keyIx, &fieldCount);

/* Query table and print out all matching things. */
sprintf(query, "select * from %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (hashLookup(hash, row[keyIx]))
        {
	printf("%s", row[0]);
	for (i = 1; i<fieldCount; ++i)
	    printf("\t%s", row[i]);
	printf("\n");
	}
    }
hFreeConn(&conn);
}



void showPasteResults()
/* Show pasted results. These can include wildcards. */
{
struct hTableInfo *hti = hFindTableInfo(NULL, track);
struct slName *wildList = NULL, *wild;
char *s = cgiString("hgb.userKeys"), *word;

printf("<TT><PRE>");
while ((word = nextWord(&s)) != NULL)
     {
     wild = slNameNew(word);
     slAddHead(&wildList, wild);
     }
if (wildList == NULL)
     errAbort("Please go back and paste something");
if (hti->isSplit)
     {
     struct slName *chrom, *chromList = hAllChromNames();
     for (chrom = chromList; chrom != NULL; chrom = chrom->next)
         {
	 char tableName[256];
	 sprintf(tableName, "%s_%s", chrom->name, track);
	 if (hTableExists(tableName))
	     showWildMatch(tableName, wildList, chrom == chromList);
	 }
     }
else
     {
     showWildMatch(track, wildList, TRUE);
     }
printf("</PRE></TT>");
}

void showUploadResults()
/* Show uploaded results.  These don't include wildcards. */
{
struct hTableInfo *hti = hFindTableInfo(NULL, track);
struct hash *hash = newHash(18);
char *s = cgiString("hgb.userKeys"), *word;

printf("<TT><PRE>");
while ((word = nextWord(&s)) != NULL)
     {
     hashAdd(hash, word, NULL);
     }
if (hti->isSplit)
     {
     struct slName *chrom, *chromList = hAllChromNames();
     for (chrom = chromList; chrom != NULL; chrom = chrom->next)
         {
	 char tableName[256];
	 sprintf(tableName, "%s_%s", chrom->name, track);
	 if (hTableExists(tableName))
	     showMatch(tableName, hash, chrom == chromList);
	 }
     }
else
     {
     showMatch(track, hash, TRUE);
     }
printf("</PRE></TT>");
}


void doMiddle(struct cart *theCart)
/* Set up a few preliminaries and dispatch to a
 * particular form. */
{
cart = theCart;
getDbAndGenome(cart, &db, &organism);
hSetDb(db);
track = cartUsualString(cart, "hgbTrack", "mrna");
keyField = cartOptionalString(cart, "hgbKeyField");
if (cgiVarExists("hgb.pasteKeys"))
    pasteForm();
else if (cgiVarExists("hgb.uploadKeys"))
    uploadForm();
else if (cgiVarExists("hgb.showPasteResults"))
    showPasteResults();
else if (cgiVarExists("hgb.showUploadResults"))
    showUploadResults();
else
    {
    selectTrack();    
    }
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", 
	"hgb.userKeys", 
	"hgb.pasteKeys", "hgb.uploadKeys", "hgb.showPasteResults",
	"hgb.showUploadResults"};


int main(int argc, char *argv[])
/* Process command line. */
{
oldVars = hashNew(8);
cgiSpoof(&argc, argv);
htmlSetBackground("../images/floret.jpg");
cartHtmlShell("Batch query", doMiddle, "hguid", excludeVars, oldVars);
return 0;
}


