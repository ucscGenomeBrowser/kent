/* rsyncEdwUserTable - Update edwUser table using user information from encodedcc. */
#include "common.h"
#include "dystring.h"
#include "options.h"
#include "jksql.h"
#include "jsHelper.h"
#include "htmlPage.h"
#include "edwLib.h"
#include "encodeDataWarehouse.h"

boolean really;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "rsyncEdwUserTable - Update edwUser table using user information from encodedcc. \n"
  "usage:\n"
  "rsyncEdwUserTable url uid pwd out.tab\n"
  "where url will need to be in quotes since it contains a question mark.\n"
  "options:\n"
  "   -really - Needs to be set for anything to happen,  otherwise will just print file names.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
    {"really", OPTION_BOOLEAN},
    {NULL, 0},
};

char *getUserTable(char *url, char *uid, char *pwd)
/* Get the userTable in JSON format from encodedcc using REST API similar 
to the curl command  --
    curl --header "accept:application/json" -u "uid:pwd"
        http://submit.encodedcc.org/users/?format=json
*/
{
struct dyString *dy = newDyString(512);
/* constant strings for REST API call */
dyStringPrintf(dy, "http://%s:%s@%s", uid, pwd, url);
struct htmlPage *page = htmlPageGet(dy->string);
struct htmlStatus *st = page->status;
if (st->status != 200)
    errAbort("Failed to get page with status code: %d ", st->status);
char  *userTab=cloneString(page->htmlText);
htmlPageFree(&page);
return userTab;
} 

void maybeDoUpdate(struct sqlConnection *conn, char *query, boolean really)
/* If really is true than do sqlUpdate with query, otherwise just print
 * it out. */
{
if (really)
    sqlUpdate(conn, query);
else
    printf("Update: %s\n", query);
}

void updateEdwUserInfo(char *email, char *uuid, int isAdmin)
/* update edwUser table based on information from encodedcc's users json
 * file */
{
struct sqlConnection *conn =  edwConnectReadWrite(edwDatabase);
char query[512];
/* use email to search now, will use uuid eventually */
if (!sqlRowExists(conn, "edwUser", "email", email))
    {
    sqlSafef(query, sizeof(query),
	"insert into edwUser (email, uuid, isAdmin) values('%s', '%s', %d)", 
	email, uuid, isAdmin);
    maybeDoUpdate(conn, query, really);
    }
else
    {
    sqlSafef(query, sizeof(query),
	"update edwUser set uuid = '%s', isAdmin = '%d' where email = '%s'", 
	uuid, isAdmin, email);
    maybeDoUpdate(conn, query, really);
    }
return;
}

void processUserList(char *userTable, FILE *f)
/* Get email, uuis and groups values of each user object to update edwUser table, also wwrite them out as TSV */
{
struct jsonElement *jsonRoot = jsonParse(userTable);
char *userListName = "@graph";
struct jsonElement *jsonUserList = jsonMustFindNamedField(jsonRoot, "", userListName);
verbose(1, "Got @graph %p\n", jsonUserList);
struct slRef *ref, *refList = jsonListVal(jsonUserList, userListName);
verbose(1, "Got %d users\n", slCount(refList));
int realUserCount = 0;
for (ref = refList; ref != NULL; ref = ref->next)
    {
    struct jsonElement *el = ref->val;
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
    updateEdwUserInfo(email, uuid, isAdmin);
    fprintf(f, "%s\t%s\t%d\n", email, uuid, isAdmin);
    ++realUserCount;
    }
    verbose(1, "Processed %d users\n", realUserCount);
}

int main(int argc, char *argv[])
{
optionInit(&argc, argv, options);
really = optionExists("really");
if (really) verbose(1, "Really going to do it! \n");
if (argc != 5)
    usage();
FILE *f = mustOpen(argv[4], "w");
char *userTable=cloneString(getUserTable(argv[1], argv[2], argv[3]));
processUserList(userTable, f);
carefulClose(&f);
return 0;
}
