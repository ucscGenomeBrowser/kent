#include "common.h"
#include "linefile.h"
#include "errabort.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "cartDb.h"
#include "htmshell.h"
#include "cart.h"
#include "web.h"

static char *sessionVar = "hgsid";	/* Name of cgi variable session is stored in. */

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

struct sqlConnection *cartSqlConnect()
/* Return sqlConnection to cart database. 
 * Free this with sqlDisconnect(). */
{
return sqlConnect("hgcentral");
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

void cartExclude(struct cart *cart, char *var)
/* Exclude var from persistent storage. */
{
hashAdd(cart->exclude, var, NULL);
}

struct cart *cartNew(unsigned int userId, unsigned int sessionId, char **exclude)
/* Load up cart from user & session id's.  Exclude is a null-terminated list of
 * strings to not include */
{
struct cgiVar *cv, *cvList = cgiVarList();
struct cart *cart;
struct sqlConnection *conn = cartSqlConnect();
char *ex;
char *booShadow = cgiBooleanShadowPrefix();
int booSize = strlen(booShadow);
struct hash *booHash = newHash(8);

AllocVar(cart);
cart->hash = newHash(8);
cart->exclude = newHash(7);
cart->userId = userId;
cart->sessionId = sessionId;
cart->userInfo = loadDbOverHash(conn, "userDb", userId, cart->hash);
cart->sessionInfo = loadDbOverHash(conn, "sessionDb", sessionId, cart->hash);

/* First handle boolean variables and store in hash. */
for (cv = cvList; cv != NULL; cv = cv->next)
    {
    if (startsWith(booShadow, cv->name))
        {
	char *booVar = cv->name + booSize;
	char *val = (cgiVarExists(booVar) ? "1" : "0");
	cartSetString(cart, booVar, val);
	hashAdd(booHash, booVar, NULL);
	}
    }

/* Handle non-boolean vars. */
for (cv = cgiVarList(); cv != NULL; cv = cv->next)
    {
    if (!startsWith(booShadow, cv->name) && !hashLookup(booHash, cv->name))
	cartSetString(cart, cv->name, cv->val);
    }

if (exclude != NULL)
    {
    while (ex = *exclude++)
	cartExclude(cart, ex);
    }

hashFree(&booHash);
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

char *cartSessionVarName()
/* Return name of CGI session ID variable. */
{
return sessionVar;
}

unsigned int cartSessionId(struct cart *cart)
/* Return session id. */
{
return cart->sessionInfo->id;
}

char *cartSidUrlString(struct cart *cart)
/* Return session id string as in hgsid=N . */
{
static char buf[64];
sprintf(buf, "%s=%u", cartSessionVarName(), cartSessionId(cart));
return buf;
}

unsigned int cartUserId(struct cart *cart)
/* Return session id. */
{
return cart->userInfo->id;
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

int cartIntExp(struct cart *cart, char *var)
/* Return integer valued expression in variable. */
{
return intExp(cartString(cart, var));
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

boolean cartBoolean(struct cart *cart, char *var)
/* Retrieve cart boolean. */
{
return cartInt(cart, var);
}

boolean cartUsualBoolean(struct cart *cart, char *var, boolean usual)
/* Return variable value if it exists or usual if not. */
{
return cartUsualInt(cart, var, usual);
}

void cartSetBoolean(struct cart *cart, char *var, boolean val)
/* Set boolean value. */
{
cartSetInt(cart,var,val);
}

void cartSaveSession(struct cart *cart)
/* Save session in a hidden variable. This needs to be called
 * somewhere inside of form or bad things will happen when user
 * has multiple windows open. */
{
char buf[64];
sprintf(buf, "%u", cart->sessionInfo->id);
cgiMakeHiddenVar(sessionVar, buf);
}

static void cartDumpItem(struct hashEl *hel)
/* Dump one item in cart hash */
{
printf("%s %s\n", hel->name, (char*)(hel->val));
}

void cartDump(struct cart *cart)
/* Dump contents of cart. */
{
struct hashEl *el, *elList = hashElListHash(cart->hash);
slSort(&elList, hashElCmp);
for (el = elList; el != NULL; el = el->next)
    cartDumpItem(el);
hashElFreeList(&el);
}

static char *cookieDate()
/* Return date string for cookie format.   We'll have to
 * revisit this in 35 years.... */
{
return "Thu, 31-Dec-2037 23:59:59 GMT";
}

static int getCookieId(char *cookieName)
/* Get id value from cookie. */
{
char *hguidString = findCookieData("hguid");
return (hguidString == NULL ? 0 : atoi(hguidString));
}

static int getSessionId()
/* Get session id if any from CGI. */
{
return cgiUsualInt("hgsid", 0);
}

static void clearDbContents(struct sqlConnection *conn, char *table, unsigned id)
/* Clear out contents field of row in table that matches id. */
{
char query[256];
if (id == 0)
   return;
sprintf(query, "update %s set contents='' where id=%u", table, id);
sqlUpdate(conn, query);
}

void cartResetInDb(char *cookieName)
/* Clear cart in database. */
{
int hguid = getCookieId(cookieName);
int hgsid = getSessionId();
struct sqlConnection *conn = cartSqlConnect();
clearDbContents(conn, "userDb", hguid);
clearDbContents(conn, "sessionDb", hgsid);
sqlDisconnect(&conn);
}

static struct cart *cartAndCookie(char *cookieName, char **exclude)
/* Load cart from cookie and session cgi variable.  Write cookie and content-type part 
 * HTTP preamble to web page.  Don't write any HTML though. */
{
/* Get the current cart from cookie and cgi session variable. */
int hguid = getCookieId(cookieName);
int hgsid = getSessionId();
struct cart *cart = cartNew(hguid, hgsid, exclude);

/* Remove some internal variables from cart permanent storage. */
cartExclude(cart, sessionVar);

/* Write out cookie for next time. */
printf("Set-Cookie: %s=%u; path=/; domain=.ucsc.edu; expires=%s\n",
	cookieName, cart->userInfo->id, cookieDate());
puts("Content-Type:text/html");
puts("\n");

return cart;
}

static void cartErrorCatcher(void (*doMiddle)(struct cart *cart), struct cart *cart)
/* Wrap error catcher around call to do middle. */
{
int status = setjmp(htmlRecover);
pushAbortHandler(htmlAbort);
if (status == 0)
    {
    doMiddle(cart);
    }
popAbortHandler();
}

static void cartEarlyWarningHandler(char *format, va_list args)
/* Write an error message so user can see it before page is really started. */
{
static boolean initted = FALSE;
if (!initted)
    {
    htmStart(stdout, "Early Error");
    initted = TRUE;
    }
htmlVaParagraph(format,args);
}

static void cartWarnCatcher(void (*doMiddle)(struct cart *cart), struct cart *cart, WarnHandler warner)
/* Wrap error and warning handlers around doMiddl. */
{
pushWarnHandler(warner);
cartErrorCatcher(doMiddle, cart);
popWarnHandler();
}

void cartHtmlStart(char *title)
/* Write HTML header and put in normal error handler. */
{
pushWarnHandler(htmlVaWarn);
htmStart(stdout, title);
}

void cartWebStart(char *format, ...)
/* Print out pretty wrapper around things when working
 * from cart. */
{
va_list args;
va_start(args, format);
pushWarnHandler(htmlVaWarn);
webStartWrapper(format, args, FALSE);
va_end(args);
}


void cartHtmlEnd()
/* Write out HTML footer and get rid or error handler. */
{
htmlEnd();
popWarnHandler();
}

void cartEmptyShell(void (*doMiddle)(struct cart *cart), char *cookieName, char **exclude)
/* Get cart and cookies and set up error handling, but don't start writing any
 * html yet. The doMiddleFunction has to call cartHtmlStart(title), and
 * cartHtmlEnd(), as well as writing the body of the HTML. */
{
struct cart *cart = cartAndCookie(cookieName, exclude);
cartWarnCatcher(doMiddle, cart, cartEarlyWarningHandler);
cartCheckout(&cart);
}

void cartHtmlShell(char *title, void (*doMiddle)(struct cart *cart), char *cookieName, char **exclude)
/* Load cart from cookie and session cgi variable.  Write web-page preamble, call doMiddle
 * with cart, and write end of web-page.   Exclude may be NULL.  If it exists it's a
 * comma-separated list of variables that you don't want to save in the cart between
 * invocations of the cgi-script. */
{
struct cart *cart = cartAndCookie(cookieName, exclude);
int status;

htmStart(stdout, title);
cartWarnCatcher(doMiddle, cart, htmlVaWarn);
cartCheckout(&cart);
htmlEnd();
}

