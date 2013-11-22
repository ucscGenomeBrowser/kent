/* test - test out something. */
#include "common.h"
#include "hPrint.h"
#include "jksql.h"
#include "dystring.h"
#include "jsHelper.h"
#include "htmlPage.h"
#include "edwLib.h"
#include "encodeDataWarehouse.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "rsyncEdwUser - Update edwUser table using JSON user table from encodedcc. \n"
  "usage:\n"
  "rsyncEdwUser url uid pwd\n"
  "where url will need to be in quotes since it contains a question mark.\n"
  );
}

char *getUserTable(char *url, char *uid, char *pwd)
/* Get the userTable in JSON format from encodedcc using REST API similar 
to the curl command  --
    curl --header "accept:application/json" -u "uid:pwd"
        http://submit.encodedcc.org/users/?format=jason
*/
{
struct dyString *dy = newDyString(512);
/* constant strings for REST API call */
dyStringPrintf(dy, "https://%s:%s@%s", uid, pwd, url);
struct htmlPage *page = htmlPageGet(dy->string);
struct htmlStatus *st = page->status;
if (st->status != 200)
    errAbort("Failed to get page with status code: %d ", st->status);
char  *userTab=cloneString(page->htmlText);
htmlPageFree(&page);
return userTab;
} 

struct slName *createUserEmailList(char *str)
/* Create the user email list from users JSON file */
{
struct jsonElement *userJson = jsonParse(str);
char jName[128]="email";
struct slName *list = jsonFindNameUniq(userJson, jName);;
slUniqify(&list, slNameCmp, slNameFree);
return list;
}

void updateEdwUser(struct slName *emailList)
{
struct sqlConnection *conn =  edwConnectReadWrite(edwDatabase);
struct slName *sln;
char query[512];
for (sln = emailList;  sln != NULL;  sln = sln->next)
    {
    if (!sqlRowExists(conn, "edwUser", "email", sln->name))
        {
        sqlSafef(query, sizeof(query),
            "insert into edwUser (email) values('%s')", sln->name);
        sqlUpdate(conn, query);
        hPrintf("email %s added\n", sln->name);
        }
    else
        hPrintf("email %s exists\n", sln->name);
    }
return;
}

int main(int argc, char *argv[])
{
if (argc != 4)
    usage();
char *userTable=cloneString(getUserTable(argv[1], argv[2], argv[3]));
struct slName *userList = createUserEmailList(userTable);
updateEdwUser(userList);
return 0;
}
