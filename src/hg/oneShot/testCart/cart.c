#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "cartDb.h"
#include "htmshell.h"
#include "errabort.h"
#include "cart.h"


static char *sessionVar = "hgsid";	/* Name of cgi variable session is stored in. */
static char *selfVar = "hgself";	/* Name of cgi variable script name is stored in. */
static boolean savedSession = FALSE;	/* TRUE if they've called cartSaveSession. */

static void hashUpdateDynamicVal(struct hash *hash, char *name, void *val)
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


struct cartDb *cartDbLoadFromId(struct sqlConnection *conn, char *table, int id)
/* Load up cartDb entry for particular ID.  Returns NULL if no such id. */
{
if (id == 0)
   return NULL;
else
   {
   char where[64];
   sqlSafeFrag(where, "id = %u", id);
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
    sqlSafef(query, sizeof query, "INSERT %s VALUES(0,\"\",0,now(),now(),0)", table);
    sqlUpdate(conn, query);
    id = sqlLastAutoId(conn);
    if ((cdb = cartDbLoadFromId(conn,table,id)) == NULL)
        errAbort("Couldn't get cartDb right after loading.  MySQL problem??");
    }
return cdb;
}

void cartExclude(struct cart *cart, char *var)
/* Exclude var from persistent storage. */
{
hashAdd(cart->exclude, var, NULL);
}

struct cart *cartNew(unsigned int userId, unsigned int sessionId, char **exclude)
/* Load up cart from user & session id's.  Exclude is a null-terminated list of
 * strings to not include */
{
struct cgiVar *cv;
struct cart *cart;
struct sqlConnection *conn = sqlConnect("hgcentral");
char *ex;

AllocVar(cart);
cart->hash = newHash(8);
cart->exclude = newHash(7);
cart->userId = userId;
cart->sessionId = sessionId;
cart->userInfo = loadDbOverHash(conn, "userDb", userId, cart->hash);
cart->sessionInfo = loadDbOverHash(conn, "sessionDb", sessionId, cart->hash);
for (cv = cgiVarList(); cv != NULL; cv = cv->next)
    cartSetString(cart, cv->name, cv->val);
if (exclude != NULL)
    {
    while (ex = *exclude++)
	cartExclude(cart, ex);
    }
sqlDisconnect(&conn);
return cart;
}

static void updateOne(struct sqlConnection *conn,
	char *table, struct cartDb *cdb, char *contents, int contentSize)
/* Update cdb in database. */
{
struct dyString *dy = newDyString(4096);
sqlDyStringPrintf(dy, "UPDATE %s SET contents='", table);
sqlDyAppendEscaped(dy, contents);
sqlDyStringPrintf(dy, "',lastUse=now(),useCount=%d ", cdb->useCount+1);
sqlDyStringPrintf(dy, " where id=%u", cdb->id);
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
    if (!hashLookup(cart->exclude, el->name))
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
    }

/* Make up update statement. */
updateOne(conn, "userDb", cart->userInfo, encoded->string, encoded->stringSize);
updateOne(conn, "sessionDb", cart->sessionInfo, encoded->string, encoded->stringSize);

/* Cleanup */
sqlDisconnect(&conn);
hashElFreeList(&elList);
dyStringFree(&encoded);
}

void cartCheckout(struct cart **pCart)
/* Free up cart and save it to database. */
{
struct cart *cart = *pCart;
if (cart != NULL)
    {
    saveState(cart);
    cartDbFree(&cart->userInfo);
    cartDbFree(&cart->sessionInfo);
    freeHash(&cart->hash);
    freeHash(&cart->exclude);
    freez(pCart);
    }
}

void cartRemove(struct cart *cart, char *var)
/* Remove variable from cart. */
{
struct hashEl *hel = hashLookup(cart->hash, var);
if (hel != NULL)
    {
    freez(&hel->val);
    hashRemove(cart->hash, var);
    }
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
/* Return variable value if it exists or usual if not. */
{
char *s = cartOptionalString(cart, var);
if (s == NULL)
    return usual;
return s;
}

void cartSetString(struct cart *cart, char *var, char *val)
/* Set string valued cart variable. */
{
hashUpdateDynamicVal(cart->hash, var, cloneString(val));
}


int cartInt(struct cart *cart, char *var)
/* Return int valued variable. */
{
char *s = cartString(cart, var);
return atoi(s);
}

int cartUsualInt(struct cart *cart, char *var, int usual)
/* Return variable value if it exists or usual if not. */
{
char *s = cartOptionalString(cart, var);
if (s == NULL)
    return usual;
return atoi(s);
}

void cartSetInt(struct cart *cart, char *var, int val)
/* Set integer value. */
{
char buf[32];
sprintf(buf, "%d", val);
cartSetString(cart, var, buf);
}

double cartDouble(struct cart *cart, char *var)
/* Return double valued variable. */
{
char *s = cartString(cart, var);
return atof(s);
}

double cartUsualDouble(struct cart *cart, char *var, double usual)
/* Return variable value if it exists or usual if not. */
{
char *s = cartOptionalString(cart, var);
if (s == NULL)
    return usual;
return atof(s);
}

void cartSetDouble(struct cart *cart, char *var, double val)
/* Set double value. */
{
char buf[32];
sprintf(buf, "%f", val);
cartSetString(cart, var, buf);
}

static char *boolName(char *name)
/* Return name with "bool" prepended. */
{
static char buf[128];
sprintf(buf, "bool.%s", name);
return buf;
}

static boolean cartBoo(struct cart *cart, char *var)
/* Retrieve cart boolean.   Since CGI booleans simply
 * don't exist when they're false - which messes up
 * cart persistence,  we have to jump through some
 * hoops.   We prepend 'bool.' to the cart representation
 * of the variable to separate it from the cgi
 * representation that may or may not be transiently
 * in the cart. */
{
var = boolName(var);
return cartInt(cart, var);
}

static boolean cartUsualBoo(struct cart *cart, char *var, boolean usual)
/* Return variable value if it exists or usual if not. */
{
var = boolName(var);
return cartUsualInt(cart, var, usual);
}

boolean cartCgiBoolean(struct cart *cart, char *var)
/* Return boolean variable from CGI.  Remove it from cart.
 * CGI booleans alas do not store cleanly in cart. */
{
boolean val;
cartRemove(cart, var);
val = cgiBoolean(var);
cartSetBoolean(cart, var, val);
return val;
}

boolean cartBoolean(struct cart *cart, char *var, char *selfVal)
/* Retrieve cart boolean.   Since CGI booleans simply
 * don't exist when they're false - which messes up
 * cart persistence,  we have to jump through some
 * hoops.   If we are calling self, then we assume that
 * the booleans are in cgi-variables,  otherwise we
 * look in the cart for them. */
{
char *s = cgiOptionalString(selfVar);
if (s != NULL && sameString(s, selfVal))
    return cartCgiBoolean(cart, var);
else
    return cartBoo(cart, var);
}

boolean cartUsualBoolean(struct cart *cart, char *var, boolean usual, char *selfVal)
/* Return variable value if it exists or usual if not. 
 * See cartBoolean for explanation of selfVal. */
{
char *s = cgiOptionalString(selfVar);
if (s != NULL && sameString(s, selfVal))
    return cartCgiBoolean(cart, var);
else
    return cartUsualBoo(cart, var, usual);
}

void cartSetBoolean(struct cart *cart, char *var, boolean val)
/* Set boolean value. */
{
var = boolName(var);
cartSetInt(cart,var,val);
}

void cartSaveSession(struct cart *cart, char *selfName)
/* Save session in a hidden variable. This needs to be called
 * somewhere inside of form or bad things will happen. */
{
char buf[64];
sprintf(buf, "%u", cart->sessionInfo->id);
cgiMakeHiddenVar(sessionVar, buf);
cgiMakeHiddenVar(selfVar, selfName);
savedSession = TRUE;
}

static void cartDumpItem(struct hashEl *hel)
/* Dump one item in cart hash */
{
printf("%s %s\n", hel->name, (char*)(hel->val));
}

void cartDump(struct cart *cart)
/* Dump contents of cart. */
{
hashTraverseEls(cart->hash, cartDumpItem);
}

static char *cookieDate()
/* Return date string for cookie format.   We'll have to
 * revisit this in 35 years.... */
{
return "Thu, 31-Dec-2037 23:59:59 GMT";
}

void cartHtmlShell(char *title, void (*doMiddle)(struct cart *cart), char *cookieName, char **exclude)
/* Load cart from cookie and session cgi variable.  Write web-page preamble, call doMiddle
 * with cart, and write end of web-page.   Exclude may be NULL.  If it exists it's a
 * comma-separated list of variables that you don't want to save in the cart between
 * invocations of the cgi-script. */
{
int status;
char *hguidString = findCookieData("hguid");
int hguid = (hguidString == NULL ? 0 : atoi(hguidString));
int hgsid = cgiUsualInt("hgsid", 0);
struct cart *cart = cartNew(hguid, hgsid, exclude);

cartExclude(cart, sessionVar);
cartExclude(cart, selfVar);

printf("Set-Cookie: %s=%u; path=/; domain=.ucsc.edu; expires=%s\n",
	cookieName, cart->userInfo->id, cookieDate());
puts("Content-Type:text/html");
puts("\n");

htmStart(stdout, title);

/* Set up error recovery (for out of memory and the like)
 * so that we finish web page regardless of problems. */
pushAbortHandler(htmlAbort);
pushWarnHandler(htmlVaWarn);
status = setjmp(htmlRecover);

/* Do your main thing. */
if (status == 0)
    {
    doMiddle(cart);
    if (!savedSession)
	errAbort("Program error - need to call saveSession inside form");
    cartCheckout(&cart);
    }

popWarnHandler();
popAbortHandler();
htmlEnd();
}

