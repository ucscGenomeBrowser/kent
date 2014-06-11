/* edwRsyncEdwUser - Update edwUser table using user information from encodedcc. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "dystring.h"
#include "options.h"
#include "jksql.h"
#include "jsHelper.h"
#include "htmlPage.h"
#include "edwLib.h"
#include "encodeDataWarehouse.h"

boolean really;
/* number of users in different stage */
int prevN = 0, postN = 0, procN = 0, addedN = 0, updateN = 0;;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwRsyncEdwUser - Update edwUser table using user information from encodedcc. \n"
  "usage:\n"
  "edwRsyncEdwUser url uid pwd update.sql\n"
  "where url will need to be in quotes if it contains a question mark.\n"
  "options:\n"
  "   -really - Needs to be set for anything to happen, otherwise will just print update statement to update.sql file.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
    {"really", OPTION_BOOLEAN},
    {NULL, 0},
};

int countTable(struct sqlConnection *conn, char *tName)
/* get number of rows of the table */
{
char query[512];
sqlSafef(query, sizeof(query),
    "select count(*) from %s" , tName);
return sqlQuickNum(conn, query);
}
    
void logUpdateResult(FILE *f)
/* log the update result */
{
/* mail message for cron task when new users are added */
if ( addedN > 0)
    verbose(1, "%d user records processed.\n%d new user added to edwUser table.\nedwUser started with %d and ended with %d rows.\n%d users information updated\n",
    procN, addedN, prevN, postN, updateN);
/* write out the update statistics to the output file */
fprintf(f, "%d user records processed.\n%d new user added to edwUser table.\nedwUser table started with %d and ended with %d rows.\n%d users information updated\n",
    procN, addedN, prevN, postN, updateN);
}

char *getTextViaHttps(char *url, char *userId, char *password)
/* getTextViaHttps - Fetch text from url that is https password protected. This
 * will return a NULL rather than aborting if URL not found. */
/* TODO: Move to edwLib.c */
{
char fullUrl[1024];
safef(fullUrl, sizeof(fullUrl), "https://%s:%s@%s", userId, password, url);
struct htmlPage *page = htmlPageGet(fullUrl);
if (page == NULL)
    return NULL;
char *text = NULL;
struct htmlStatus *st = page->status;
if (st->status == 200)
    text = cloneString(page->htmlText);
htmlPageFree(&page);
return text;
}

char *getUserJson(char *sUrl, char *uuid, char *userId, char *password)
/* Get detail user information associated with this uuid */
{
char url[512];
safef(url, sizeof(url), "%s/users/%s/?format=json&datastore=database", sUrl, uuid);
return getTextViaHttps(url, userId, password);
}

char *getAllUsersJson(char *sUrl, char *userId, char *password)
/* Get the userTable in JSON format from encodedcc using REST API
 * similar to the curl command  --
    curl --header "accept:application/json" -u "uid:pwd"
        https://www.encodedcc.org/users/?format=json&limit=all
*/
{
char url[512];
safef(url, sizeof(url), "%s/users/?format=json&limit=all", sUrl);
//verbose(1, "Fetching all users from\n  %s\n", url);
return getTextViaHttps(url, userId, password);
}

void maybeDoUpdate(struct sqlConnection *conn, char *query, boolean really, FILE *f)
/* If really is true than do sqlUpdate with query, otherwise just print
 * it out. */
{
if (really)
    sqlUpdate(conn, query);
else
    fprintf(f, "%s\n", query);
}

void updateEdwUserInfo(struct sqlConnection *conn, char *email, char *uuid, int isAdmin, FILE *f)
/* update edwUser table based on information from encodedcc's users json
 * text using uuid as search key to update email and isAdmine column */
{
char query[512];
/* use uuid then email to search the user row */
if (!sqlRowExists(conn, "edwUser", "uuid", uuid))
    {
    /* Was this user created by edwCreateUser without real encodedcc uuid? */
    if (sqlRowExists(conn, "edwUser", "email", email))
	{
	/* yes, update it with real uuid from encodedcc */
        sqlSafef(query, sizeof(query),
	    "update edwUser set uuid = '%s', isAdmin = '%d' where email = '%s' and uuid = '0'",
	    uuid, isAdmin, email);
        maybeDoUpdate(conn, query, really, f);
	updateN += 1;
	}
    else
	{
	sqlSafef(query, sizeof(query),
	    "insert into edwUser (email, uuid, isAdmin) values('%s', '%s', %d)",	    email, uuid, isAdmin);
        maybeDoUpdate(conn, query, really, f);
	addedN += 1;
	}
    }
else
    {
    sqlSafef(query, sizeof(query),
	"update edwUser set email = '%s', isAdmin = '%d' where uuid = '%s'", 
	email, isAdmin, uuid);
    maybeDoUpdate(conn, query, really, f);
    updateN += 1;
    }
return;
}

struct slName *createUuidList(char *url, char *userId, char *password)
{
char *usersJsonText = getAllUsersJson(url, userId, password);
struct jsonElement *jsonRoot = jsonParse(usersJsonText);
char *userListName = "@graph";
struct jsonElement *jsonUserList = jsonMustFindNamedField(jsonRoot, "", userListName);
//verbose(1, "Got @graph %p\n", jsonUserList);
struct slName *list = NULL;
struct slRef *ref, *refList = jsonListVal(jsonUserList, userListName);
for (ref = refList; ref != NULL; ref = ref->next)
    {
    struct jsonElement *el = ref->val;
    char *uuid = jsonStringField(el, "uuid");
    if (uuid !=NULL) slNameAddHead(&list, uuid);
    }
//verbose(1, "Got %d uuid to process\n", slCount(list));
return list;
}

void edwRsyncEdwUser(char *url, char *userId, char *password, char *outTab)
/* edwRsyncEdwUser - Update edwUser table using user information from
 * Stanford  encodedcc. */ 
{
FILE *f = mustOpen(outTab, "w");
//char *database = "chinhliTest";
//struct sqlConnection *conn = sqlConnect(database);
struct sqlConnection *conn =  edwConnectReadWrite(edwDatabase);
prevN = countTable(conn, "edwUser");
struct slName *uuidList=createUuidList(url, userId, password);
//verbose(1, "uuidList created  %p\n", uuidList);
struct slName *sln;
int realUserCount = 0;
for (sln = uuidList;  sln != NULL;  sln = sln->next)
    {
    char *userJson = getUserJson(url, sln->name, userId, password);
    struct jsonElement *el = jsonParse(userJson);
    char *email = jsonStringField(el, "email");
    char *uuid = jsonStringField(el, "uuid");
    struct jsonElement *groupsArray = jsonMustFindNamedField(el, NULL, "groups");
    int isAdmin = 0;
    /* loop thru the groups array, set isAdmin to 1 for admin/wrangler group */
    if (groupsArray != NULL)
        {
        struct slRef *grpRef;
        struct slRef  *groupsList = jsonListVal(groupsArray, "groups");
        for (grpRef = groupsList; grpRef != NULL; grpRef = grpRef->next)
            {
            struct jsonElement *grp = grpRef->val;
            char *grpName = jsonStringVal(grp,"");
            if ((grpName != NULL) && (sameString(grpName, "admin") ||
                sameString(grpName, "wrangler")))
                isAdmin = 1;
            }
        }
    updateEdwUserInfo(conn, email, uuid, isAdmin, f);
    //verbose(1, "%s\t%s\t%d\n", email, uuid, isAdmin);
    ++realUserCount;
    }
procN = realUserCount;
postN = countTable(conn, "edwUser");
logUpdateResult(f);
//verbose(1, "Total of %d users processed\n", realUserCount);
carefulClose(&f);
}

int main(int argc, char *argv[])
{
optionInit(&argc, argv, options);
really = optionExists("really");
//if (really) verbose(1, "Really going to do it! \n");
if (argc != 5)
    usage();
edwRsyncEdwUser(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
