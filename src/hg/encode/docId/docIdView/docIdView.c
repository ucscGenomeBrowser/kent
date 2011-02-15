/* docIdView - cgi to show metadata from docId table. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "htmshell.h"
#include "web.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"
#include "docId.h"
#include "mdb.h"
#include "portable.h"
#include "trashDir.h"
#include "dystring.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

/* Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars = NULL;
char *database = "encpipeline_prod";
char *docIdTable = DEFAULT_DOCID_TABLE;
//char *docIdDir = "/hive/groups/encode/dcc/pipeline/downloads/docId";
char *docIdDir = "http://hgdownload-test.cse.ucsc.edu/goldenPath/docId";
char *docIdDirBeta = "http://hgdownload-test.cse.ucsc.edu/goldenPath/betaDocId";

void addValue(struct dyString *str, char *value)
{
if ((value != NULL) && !sameString(value, "None"))
    {
    dyStringPrintf(str, "%s, ", value);
    }
}

void doStandard(struct cart *theCart)
{
cart = theCart;
cartWebStart(cart, database, "ENCODE DCC Submissions");
struct sqlConnection *conn = sqlConnect(database);
struct docIdSub *docIdSub;
char query[10 * 1024];
struct sqlResult *sr;
char **row;
struct tempName tn;
trashDirFile(&tn, "docId", "meta", ".txt");
char *tempFile = tn.forCgi;
//printf("tempFile is %s\n<BR>", tempFile);

    // <Data type> <Cell Type> <Key Metadata> <View>
printf("<table border=1><tr>");
printf("<th>date submitted</th>");
printf("<th>dataType</th>");
printf("<th>cell type</th>");
printf("<th>metadata</th>");
// printf("<th>view</th>");
printf("<th>fileType</th>");
printf("<th>file</th>");
printf("<th>lab</th>");
printf("<th>assembly</th>");
printf("<th>subId</th>");
printf("<th>val-report</th>");
printf("</tr>\n");
safef(query, sizeof query, "select * from %s", docIdTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    docIdSub = docIdSubLoad(row);
    verbose(2, "ix %d\n", docIdSub->ix);
    verbose(2, "submitDate %s\n", docIdSub->submitDate);
    verbose(2, "md5sum %s\n", docIdSub->md5sum);
    verbose(2, "valReport %s\n", docIdSub->valReport);
    verbose(2, "metaData %s\n", docIdSub->metaData);
    verbose(2, "submitPath %s\n", docIdSub->submitPath);
    verbose(2, "submitter %s\n", docIdSub->submitter);

    cgiDecode(docIdSub->metaData, docIdSub->metaData, strlen(docIdSub->metaData));
    //printf("tempFile %s\n", tempFile);
    FILE *f = mustOpen(tempFile, "w");
    fwrite(docIdSub->metaData, strlen(docIdSub->metaData), 1, f);
    fclose(f);
    //boolean validated = FALSE;
    struct mdbObj *mdbObj = mdbObjsLoadFromFormattedFile(tempFile, NULL);
    unlink(tempFile);

    // <Data type> <Cell Type> <Key Metadata> <View>
    char *docIdType = mdbObjFindValue(mdbObj, "type");
    char *docIdComposite = mdbObjFindValue(mdbObj, "composite");
    char buffer[10 * 1024];
    char *cellFromMetaData = mdbObjFindValue(mdbObj, "cell"); 

    safef(buffer, sizeof buffer, "%d", docIdSub->ix);
    if (sameString(database, "encpipeline_beta"))
        docIdDir = docIdDirBeta;

    printf("<tr>");
    printf("<td>%s</td> ",   docIdSub->submitDate);
    printf("<td>%s</td> ",   mdbObjFindValue(mdbObj, "dataType"));
    printf("<td>%s</td> ",  cellFromMetaData);
    struct dyString *str = newDyString(100);
    addValue(str,  mdbObjFindValue(mdbObj, "antibody"));
    addValue(str,  mdbObjFindValue(mdbObj, "treatment"));
    addValue(str,  mdbObjFindValue(mdbObj, "rnaExtract"));
    addValue(str,  mdbObjFindValue(mdbObj, "localization"));
    printf("<td>%s<a href=docIdView?docId=%s&db=%s&meta=\"\"> ...</a></td>", str->string,buffer, database);
    freeDyString(&str);
        
//    printf("<td>%s</td> ",   mdbObjFindValue(mdbObj, "view"));
    printf("<td>%s</td> ",   mdbObjFindValue(mdbObj, "type"));
    printf("<td><a href=%s> %s</a></td>", 
        docIdGetPath(buffer, docIdDir, docIdType, NULL) , 
        docIdDecorate(docIdComposite, cellFromMetaData, docIdSub->ix));
    char *lab = mdbObjFindValue(mdbObj, "lab");
    char *subId = mdbObjFindValue(mdbObj, "subId");
    printf("<td><a href=docIdView?docId=%s&db=%s&lab=\"%s\"> %s</a></td>",buffer, database, subId, lab);
    printf("<td>%s</td> ",   mdbObjFindValue(mdbObj, "assembly"));
    printf("<td>%s</td> ",   subId);
    printf("<td><a href=docIdView?docId=%s&db=%s&report=\"\"> report</a></td>", buffer, database);
    printf("</tr>\n");
    }

printf("</table>");
sqlFreeResult(&sr);
sqlDisconnect(&conn);
cartWebEnd();
}

void doDocIdMeta(struct cart *theCart)
{
char *docId = cartString(theCart, "docId");
cartWebStart(cart, database, "ENCODE DCC:  Metadata for docId %s",docId);
struct sqlConnection *conn = sqlConnect(database);
char query[10 * 1024];
struct sqlResult *sr;
char **row;
struct tempName tn;
trashDirFile(&tn, "docId", "meta", ".txt");
char *tempFile = tn.forCgi;
boolean beenHere = FALSE;

printf("<a href=docIdView?db=%s> Return </a><BR>", database);
safef(query, sizeof query, "select * from %s where ix=%s", docIdTable,docId);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (beenHere)
        errAbort("found more than one record with ix value %s in table %s", 
            docId, docIdTable);
    beenHere = TRUE;

    struct docIdSub *docIdSub = docIdSubLoad(row);
    cgiDecode(docIdSub->metaData, docIdSub->metaData, strlen(docIdSub->metaData));
    //printf("tempFile is %s\n<BR>", tempFile);
    FILE *f = mustOpen(tempFile, "w");
    fwrite(docIdSub->metaData, strlen(docIdSub->metaData), 1, f);
    fclose(f);
    boolean validated;
    struct mdbObj *mdbObj = mdbObjsLoadFromFormattedFile(tempFile, &validated);
    unlink(tempFile);

    if (mdbObj->next)
        errAbort("got more than one metaObj from metaData");

    for(; mdbObj; mdbObj = mdbObj->next)
        {
        printf("<BR>object: %s\n", mdbObj->obj);
        struct mdbVar *vars = mdbObj->vars;
        for(; vars; vars = vars->next)
            {
            printf("<BR>%s %s\n", vars->var, vars->val);
            }
        }
    }

cartWebEnd();
}

void doDocIdReport(struct cart *theCart)
{
char *docId = cartString(theCart, "docId");
cartWebStart(cart, database, "ENCODE DCC:  Validation report for docId %s",docId);
struct sqlConnection *conn = sqlConnect(database);
char query[10 * 1024];
struct sqlResult *sr;
char **row;
boolean beenHere = FALSE;

printf("<a href=docIdView?db=%s> Return </a><BR>", database);
safef(query, sizeof query, "select * from %s where ix=%s", docIdTable,docId);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (beenHere)
        errAbort("found more than one record with ix value %s in table %s", 
            docId, docIdTable);
    beenHere = TRUE;

    struct docIdSub *docIdSub = docIdSubLoad(row);
    cgiDecode(docIdSub->valReport, docIdSub->valReport, strlen(docIdSub->valReport));
    //printf("tempFile is %s\n<BR>", tempFile);
    printf("<pre>%s", docIdSub->valReport);
    }

cartWebEnd();
}

void doLabContacts(struct cart *theCart)
{
char *subId = cartString(theCart, "lab");
cartWebStart(cart, database, "ENCODE DCC:  Contacts for submission: %s",subId);
struct sqlConnection *conn = sqlConnect(database);
char query[10 * 1024];
struct sqlResult *sr;
char **row;

printf("<a href=docIdView?db=%s> Return </a><BR>", database);
safef(query, sizeof query, "select user_id from %s where id = %s ", "projects",subId);
char *userId = sqlQuickString(conn, query);

safef(query, sizeof query, "select name,email,pi from %s where id = '%s' ", "users",userId);
sr = sqlGetResult(conn, query);
printf("<pre>");
while ((row = sqlNextRow(sr)) != NULL)
    {
    printf("Name:  %s\nEmail: %s\nPI:    %s", row[0], row[1], row[2]);
    }

sqlFreeResult(&sr);
sqlDisconnect(&conn);
cartWebEnd();
}

void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
if (cgiVarExists("docId"))
    {
    if (cgiVarExists("meta"))
        doDocIdMeta(theCart);
    else if (cgiVarExists("report"))
        doDocIdReport(theCart);
    else if (cgiVarExists("lab"))
        doLabContacts(theCart);
    }
else 
    doStandard(theCart);
}



/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", "docId", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (cgiVarExists("db"))
    {
    database=cgiOptionalString("db");
    }

cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
