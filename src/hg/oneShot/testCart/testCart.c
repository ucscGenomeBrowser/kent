/* testCart - Test cart routines.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "jksql.h"
#include "cartDb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "testCart - Test cart routines.\n"
  "usage:\n"
  "   testCart userId sessionId\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void hashUpdateDynamicVal(struct hash *hash, char *name, void *val)
/* Val is a dynamically allocated (freeMem-able) entity to put
 * in hash.  Override existing hash item with that name if any.
 * Otherwise make new hash item. */
{
struct hashEl *hel = hashLookup(hash, name);
if (hel == NULL)
    hashAdd(hash, name, val);
else
    {
    freeMem(hel->val);
    hel->val = val;
    }
}

static void cartParseOverHash(char *input, struct hash *hash)
/* Parse cgi-style input into a hash table.  This will
 * replace existing members of hash that have same name. */
{
char *namePt, *dataPt, *nextNamePt;
namePt = input;
while (namePt != NULL && namePt[0] != 0)
    {
    dataPt = strchr(namePt, '=');
    if (dataPt == NULL)
	errAbort("Mangled input string %s", namePt);
    *dataPt++ = 0;
    nextNamePt = strchr(dataPt, '&');
    if (nextNamePt == NULL)
	nextNamePt = strchr(dataPt, ';');	/* Accomodate DAS. */
    if (nextNamePt != NULL)
         *nextNamePt++ = 0;
    cgiDecode(dataPt,dataPt,strlen(dataPt));
    hashUpdateDynamicVal(hash, namePt, cloneString(dataPt));
    namePt = nextNamePt;
    }
}


struct cart
/* A cart of settings that persist. */
   {
   struct cart *next;	/* Next in list. */
   unsigned int userId;	/* User ID in database. */
   unsigned int sessionId;	/* Session ID in database. */
   struct hash *hash;	/* String valued hash. */
   struct cartDb *userInfo;	/* Info on user. */
   struct cartDb *sessionInfo;	/* Info on session. */
   };

void cartSetString(struct cart *cart, char *var, char *val)
/* Set string valued cart variable. */
{
hashUpdateDynamicVal(cart->hash, var, cloneString(val));
}

struct cartDb *cartDbLoadFromId(struct sqlConnection *conn, char *table, int id)
/* Load up cartDb entry for particular ID.  Returns NULL if no such id. */
{
if (id == 0)
   return NULL;
else
   {
   char where[64];
   sprintf(where, "id = %u", id);
   return  cartDbLoadWhere(conn, table, where);
   }
}

struct cartDb *loadDbOverHash(struct sqlConnection *conn, char *table, int id, struct hash *hash)
/* Load bits from database and save in hash. */
{
struct cartDb *cdb;
char **row;
struct sqlResult *sr = NULL;
char query[256];

if ((cdb = cartDbLoadFromId(conn, table, id)) != NULL)
    {
    cartParseOverHash(cdb->contents, hash);
    }
else
    {
    sprintf(query, "INSERT %s VALUES(0,\"\",0,now(),now(),0)", table);
    sqlUpdate(conn, query);
    id = sqlLastAutoId(conn);
    if ((cdb = cartDbLoadFromId(conn,table,id)) == NULL)
        errAbort("Couldn't get cartDb right after loading.  MySQL problem??");
    }
return cdb;
}

struct cart *cartNew(unsigned int userId, unsigned int sessionId)
/* Load up cart from user & session id's. */
{
struct cgiVar *cv;
struct cart *cart;
struct sqlConnection *conn = sqlConnect("hgcentral");

AllocVar(cart);
cart->hash = newHash(8);
cart->userId = userId;
cart->sessionId = sessionId;
cart->userInfo = loadDbOverHash(conn, "userDb", userId, cart->hash);
cart->sessionInfo = loadDbOverHash(conn, "sessionDb", sessionId, cart->hash);
for (cv = cgiVarList(); cv != NULL; cv = cv->next)
    cartSetString(cart, cv->name, cv->val);
sqlDisconnect(&conn);
return cart;
}

static void updateOne(struct sqlConnection *conn,
	char *table, struct cartDb *cdb, char *contents, int contentSize)
/* Update cdb in database. */
{
struct dyString *dy = newDyString(4096);

dyStringPrintf(dy, "UPDATE %s SET contents='", table);
dyStringAppendN(dy, contents, contentSize);
dyStringPrintf(dy, "',lastUse=now(),useCount=%d ", cdb->useCount+1);
dyStringPrintf(dy, " where id=%u", cdb->id);
sqlUpdate(conn, dy->string);
dyStringFree(&dy);
}

static void saveState(struct cart *cart)
/* Save out state to permanent storage in both user and session db. */
{
struct sqlConnection *conn = sqlConnect("hgcentral");
struct dyString *encoded = newDyString(4096);
struct hashEl *el, *elList = hashElListHash(cart->hash);
boolean firstTime = TRUE;
char *s = NULL;

/* Make up encoded string holding all variables. */
for (el = elList; el != NULL; el = el->next)
    {
    if (firstTime)
        firstTime = FALSE;
    else
        dyStringAppendC(encoded, '&');
    dyStringAppend(encoded, el->name);
    dyStringAppendC(encoded, '=');
    s = cgiEncode(el->val);
    dyStringAppend(encoded, s);
    freez(&s);
    }

/* Make up update statement. */
updateOne(conn, "userDb", cart->userInfo, encoded->string, encoded->stringSize);
updateOne(conn, "sessionDb", cart->sessionInfo, encoded->string, encoded->stringSize);

/* Cleanup */
sqlDisconnect(&conn);
hashElFreeList(&elList);
dyStringFree(&encoded);
}

void cartFree(struct cart **pCart)
/* Free up cart and save it to database. */
{
struct cart *cart = *pCart;
if (cart != NULL)
    {
    saveState(cart);
    cartDbFree(&cart->userInfo);
    cartDbFree(&cart->sessionInfo);
    freeHash(&cart->hash);
    freez(pCart);
    }
}

void cartDumpItem(struct hashEl *hel)
/* Dump one item in cart hash */
{
printf("%s %s\n", hel->name, (char*)(hel->val));
}

void cartDump(struct cart *cart)
/* Dump contents of cart. */
{
hashTraverseEls(cart->hash, cartDumpItem);
}

char *cartString(struct cart *cart, char *var)
/* Return string valued cart variable. */
{
return hashMustFindVal(cart->hash, var);
}

char *cartOptionalString(struct cart *cart, char *var)
/* Return string valued cart variable or NULL if it doesn't exist. */
{
return hashFindVal(cart->hash, var);
}

char *cartUsualString(struct cart *cart, char *var, char *usual)
/* Return variable value, or 'usual' if it doesn't exist. */
{
char *s = cartOptionalString(cart, var);
if (s == NULL)
    s = usual;
return s;
}

void cartSaveSession(struct cart *cart)
/* Save session in a hidden variable. This needs to be called
 * somewhere inside of form. */
{
char buf[64];
sprintf(buf, "%u", cart->sessionInfo->id);
cgiMakeHiddenVar("hgsid", buf);
}

void doMiddle(struct cart *cart)
/* Print out middle parts. */
{
char *old;

cartSetString(cart, "killroy", "was here");
cartSetString(cart, "lost", "G*d d*ng it!");
printf("<FORM ACTION=\"../cgi-bin/testCart\">\n");
printf("<H3>Just a Test</H3>\n");
printf("<B>Filter:</B> ");
old = cartUsualString(cart, "filter", "red");
cgiMakeRadioButton("filter", "red", sameString(old, "red"));
cgiMakeRadioButton("filter", "green", sameString(old, "green"));
cgiMakeRadioButton("filter", "blue", sameString(old, "blue"));
cgiMakeButton("submit", "Submit");
printf("</FORM>");
printf("<TT><PRE>");
cartDump(cart);
}

char *cookieDate(char *relative)
/* Return date string for cookie format. */
{
return "Thu, 31-Dec-2037 23:59:59 GMT";
}

void cartHtmShell(char *title, void (*doMiddle)(struct cart *cart), char *cookieName)
/* Load cart from cookie and session cgi variable. */
{
char *hguidString = findCookieData("hguid");
int hguid = (hguidString == NULL ? 0 : atoi(hguidString));
int hgsid = cgiUsualInt("hgsid", 0);
struct cart *cart = cartNew(hguid, hgsid);

printf("Set-Cookie: %s=%u; path=/; domain=.ucsc.edu; expires=%s\n",
	cookieName, cart->userInfo->id, cookieDate("+2y"));
puts("Content-Type:text/html");
puts("\n");

htmStart(stdout, title);
doMiddle(cart);
cartFree(&cart);
htmlEnd();
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
cartHtmShell("testCart", doMiddle, "hguid");
return 0;
}
