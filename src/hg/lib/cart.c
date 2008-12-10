#include "common.h"
#include "hCommon.h"
#include "obscure.h"
#include "linefile.h"
#include "errabort.h"
#include "hash.h"
#include "cheapcgi.h"
#include "cartDb.h"
#include "htmshell.h"
#include "hgConfig.h"
#include "cart.h"
#include "net.h"
#include "web.h"
#include "hdb.h"
#include "jksql.h"
#include "trashDir.h"
#ifndef GBROWSE
#include "customFactory.h"
#include "googleAnalytics.h"
#include "wikiLink.h"
#endif /* GBROWSE */
#include "hgMaf.h"

static char const rcsid[] = "$Id: cart.c,v 1.97 2008/12/10 19:02:53 angie Exp $";

static char *sessionVar = "hgsid";	/* Name of cgi variable session is stored in. */
static char *positionCgiName = "position";

DbConnector cartDefaultConnector = hConnectCart;
DbDisconnect cartDefaultDisconnector = hDisconnectCart;

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

void cartTrace(struct cart *cart, char *when, struct sqlConnection *conn)
/* Write some properties of the cart to stderr for debugging. */
{
if (cfgOption("cart.trace") == NULL)
    return;
struct cartDb *u = cart->userInfo, *s = cart->sessionInfo;
char *pix = hashFindVal(cart->hash, "pix");
char *textSize = hashFindVal(cart->hash, "textSize");
char *trackControls = hashFindVal(cart->hash, "trackControlsOnMain");
int uLen, sLen;
if (conn != NULL)
    {
    /* Since the content string is chopped, query for the actual length. */
    char query[1024];
    safef(query, sizeof(query), "select length(contents) from userDb "
	  "where id = %d", u->id);
    uLen = sqlQuickNum(conn, query);
    safef(query, sizeof(query), "select length(contents) from sessionDb "
	  "where id = %d", s->id);
    sLen = sqlQuickNum(conn, query);
    }
else
    {
    uLen = strlen(u->contents);
    sLen = strlen(s->contents);
    }
if (pix == NULL)
    pix = "-";
if (textSize == NULL)
    textSize = "-";
if (trackControls == NULL)
    trackControls = "-";
fprintf(stderr, "ASH: %22s: "
	"u.i=%d u.l=%d u.c=%d s.i=%d s.l=%d s.c=%d "
	"p=%s f=%s t=%s pid=%d %s\n",
	when,
	u->id, uLen, u->useCount, s->id, sLen, s->useCount,
	pix, textSize, trackControls, getpid(), cgiRemoteAddr());
if (cart->userId != 0 && u->id != cart->userId)
    fprintf(stderr, "ASH: bad userId %d --> %d!  pid=%d\n",
	    cart->userId, u->id, getpid());
if (cart->sessionId != 0 && s->id != cart->sessionId)
    fprintf(stderr, "ASH: bad sessionId %d --> %d!  pid=%d\n",
	    cart->sessionId, s->id, getpid());
}

boolean cartTablesOk(struct sqlConnection *conn)
/* Return TRUE if cart tables are accessible (otherwise, the connection
 * doesn't do us any good). */
{
if (!sqlTableOk(conn, "userDb"))
    {
    fprintf(stderr, "ASH: cartTablesOk failed on %s.userDb!  pid=%d\n",
	    sqlGetDatabase(conn), getpid());
    return FALSE;
    }
if (!sqlTableOk(conn, "sessionDb"))
    {
    fprintf(stderr, "ASH: cartTablesOk failed on %s.sessionDb!  pid=%d\n",
	    sqlGetDatabase(conn), getpid());
    return FALSE;
    }
return TRUE;
}

static void cartParseOverHash(struct cart *cart, char *contents)
/* Parse cgi-style contents into a hash table.  This will *not*
 * replace existing members of hash that have same name, so we can 
 * support multi-select form inputs (same var name can have multiple
 * values which will be in separate hashEl's). */
{
struct hash *hash = cart->hash;
char *namePt, *dataPt, *nextNamePt;
namePt = contents;
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
    hashAdd(hash, namePt, cloneString(dataPt));
    namePt = nextNamePt;
    }
}

static boolean looksCorrupted(struct cartDb *cdb)
/* Test for db corruption by checking format of firstUse field. */
{
if (cdb == NULL)
    return FALSE;
else
    {
    char *words[3];
    int wordCount = 0;
    boolean isCorr = FALSE;
    char *fu = cloneString(cdb->firstUse);
    wordCount = chopByChar(fu, '-', words, ArraySize(words));
    if (wordCount < 3)
	isCorr = TRUE;
    else
	{
	time_t theTime = time(NULL);
	struct tm *tm = localtime(&theTime);
	int year = atoi(words[0]);
	int month = atoi(words[1]);
	if ((year < 2000) || (year > (1900+tm->tm_year)) ||
	    (month < 1) || (month > 12))
	    isCorr = TRUE;
	}
    freez(&fu);
    return isCorr;
    }
}

struct cartDb *cartDbLoadFromId(struct sqlConnection *conn, char *table, int id)
/* Load up cartDb entry for particular ID.  Returns NULL if no such id. */
{
if (id == 0)
   return NULL;
else
   {
   struct cartDb *cdb = NULL;
   char where[64];
   safef(where, sizeof(where), "id = %u", id);
   cdb = cartDbLoadWhere(conn, table, where);
   if (looksCorrupted(cdb))
       {
       /* Can't use warn here -- it interrupts the HTML header, causing an
	* err500 (and nothing useful in error_log) instead of a warning. */
       fprintf(stderr,
	       "%s id=%d looks corrupted -- starting over with new %s id.\n",
	       table, id, table);
       cdb = NULL;
       }
   return cdb;
   }
}

struct cartDb *loadDb(struct sqlConnection *conn, char *table, int id, boolean *found) 
/* Load bits from database and save in hash. */
{
struct cartDb *cdb;
char query[256];
boolean result = TRUE;

cdb = cartDbLoadFromId(conn, table, id);
if (!cdb)
    {
    result = FALSE;
    safef(query, sizeof(query), "INSERT %s VALUES(0,\"\",0,now(),now(),0)",
	  table);
    sqlUpdate(conn, query);
    id = sqlLastAutoId(conn);
    if ((cdb = cartDbLoadFromId(conn,table,id)) == NULL)
        errAbort("Couldn't get cartDb for id=%d right after loading.  "
		 "MySQL problem??", id);
    }
*found = result;
return cdb;
}

void cartExclude(struct cart *cart, char *var)
/* Exclude var from persistent storage. */
{
hashAdd(cart->exclude, var, NULL);
}


void sessionTouchLastUse(struct sqlConnection *conn, char *encUserName,
			 char *encSessionName)
/* Increment namedSessionDb.useCount and update lastUse for this session. */
{
struct dyString *dy = dyStringNew(1024);
int useCount;
dyStringPrintf(dy, "SELECT useCount FROM %s "
	       "WHERE userName = '%s' AND sessionName = '%s';",
	       namedSessionTable, encUserName, encSessionName);
useCount = sqlQuickNum(conn, dy->string) + 1;
dyStringClear(dy);
dyStringPrintf(dy, "UPDATE %s SET useCount = %d, lastUse=now() "
	       "WHERE userName = '%s' AND sessionName = '%s';",
	       namedSessionTable, useCount, encUserName, encSessionName);
sqlUpdate(conn, dy->string);
dyStringFree(&dy);
}

#ifndef GBROWSE
static void cartCopyCustomTracks(struct cart *cart, struct hash *oldVars)
/* If cart contains any live custom tracks, save off a new copy of them, 
 * to prevent clashes by multiple loaders of the same session.  */
{
struct hashEl *el, *elList = hashElListHash(cart->hash);
char *db=NULL, *ignored;
getDbAndGenome(cart, &db, &ignored, oldVars);

for (el = elList; el != NULL; el = el->next)
    {
    if (startsWith(CT_FILE_VAR_PREFIX, el->name))
	{
	struct slName *browserLines = NULL;
	struct customTrack *ctList = NULL;
	char *ctFileName = (char *)(el->val);
	if (fileExists(ctFileName))
	    ctList = customFactoryParseAnyDb(db, ctFileName, TRUE, &browserLines);
	/* Save off only if the custom tracks are live -- if none are live,
	 * leave cart variables in place so hgSession can detect and inform 
	 * the user. */
	if (ctList)
	    {
	    struct customTrack *ct;
	    static struct tempName tn;
	    char *ctFileVar = el->name;
	    char *ctFileName;
	    for (ct = ctList;  ct != NULL;  ct = ct->next)
		{
		copyFileToTrash(&(ct->htmlFile), "ct", CT_PREFIX, ".html");
		copyFileToTrash(&(ct->wibFile), "ct", CT_PREFIX, ".wib");
		copyFileToTrash(&(ct->wigFile), "ct", CT_PREFIX, ".wig");
		}
	    trashDirFile(&tn, "ct", CT_PREFIX, ".bed");
	    ctFileName = tn.forCgi;
	    cartSetString(cart, ctFileVar, ctFileName);
	    customTracksSaveFile(db, ctList, ctFileName);
	    }
	}
    }
}
#endif /* GBROWSE */

static void storeInOldVars(struct cart *cart, struct hash *oldVars, char *var)
/* Store all cart hash elements for var into oldVars (if it exists). */
{
if (oldVars == NULL)
    return;
struct hashEl *hel = hashLookup(cart->hash, var);
while (hel != NULL)
    {
    hashAdd(oldVars, var, cloneString(hel->val));
    hel = hashLookupNext(hel);
    }
}

static void loadCgiOverHash(struct cart *cart, struct hash *oldVars)
/* Store CGI variables in cart. */
{
struct cgiVar *cv, *cvList = cgiVarList();
char *booShadow = cgiBooleanShadowPrefix();
int booSize = strlen(booShadow);
char *multShadow = cgiMultListShadowPrefix();
int multSize = strlen(multShadow);
struct hash *booHash = newHash(8);
struct hash *cgiHash = hashNew(11);

/* First handle boolean variables and store in cgiHash.  We store in a
 * separate hash in order to distinguish between a list-variable's old
 * values in the cart hash and new values from cgi. */
for (cv = cvList; cv != NULL; cv = cv->next)
    {
    if (startsWith(booShadow, cv->name))
        {
	char *booVar = cv->name + booSize;
	char *val = (cgiVarExists(booVar) ? "1" : "0");
	storeInOldVars(cart, oldVars, booVar);
	cartRemove(cart, booVar);
	hashAdd(cgiHash, booVar, val);
	hashAdd(booHash, booVar, NULL);
	}
    else if (startsWith(multShadow, cv->name))
	{
	/* This shadow variable enables us to detect when all inputs in
	 * the multi-select box have been deselected. */
	char *multVar = cv->name + multSize;
	if (! cgiVarExists(multVar))
	    {
	    storeInOldVars(cart, oldVars, multVar);
	    cartRemove(cart, multVar);
	    }
	}
    }

/* Handle non-boolean vars. */
for (cv = cgiVarList(); cv != NULL; cv = cv->next)
    {
    if (! (startsWith(booShadow, cv->name) || hashLookup(booHash, cv->name) ||
	   startsWith(multShadow, cv->name)) )

	{
	storeInOldVars(cart, oldVars, cv->name);
	cartRemove(cart, cv->name);
	hashAdd(cgiHash, cv->name, cv->val);
	}
    }

/* Add new settings to cart (old values of these variables have been
 * removed above). */
struct hashEl *hel = hashElListHash(cgiHash);
while (hel != NULL)
    {
    cartAddString(cart, hel->name, hel->val);
    hel = hel->next;
    }
hashFree(&cgiHash);
hashFree(&booHash);
}

static void hashEmpty(struct hash *hash)
/* Remove everything from hash. */
{
struct hashEl *hel, *helList = hashElListHash(hash);
for (hel = helList;  hel != NULL;  hel = hel->next)
    {
    freez(&(hel->val));
    hashRemove(hash, hel->name);
    }
hashElFreeList(&helList);
assert(hashNumEntries(hash) == 0);
}

#ifndef GBROWSE
void cartLoadUserSession(struct sqlConnection *conn, char *sessionOwner,
			 char *sessionName, struct cart *cart,
			 struct hash *oldVars, char *actionVar)
/* If permitted, load the contents of the given user's session, and then
 * reload the CGI settings (to support override of session settings).
 * If non-NULL, oldVars will contain values overloaded when reloading CGI.
 * If non-NULL, actionVar is a cartRemove wildcard string specifying the
 * CGI action variable that sent us here. */
{
struct sqlResult *sr = NULL;
char **row = NULL;
char *userName = wikiLinkUserName();
char *encSessionName = cgiEncodeFull(sessionName);
char *encSessionOwner = cgiEncodeFull(sessionOwner);
char query[512];

if (isEmpty(sessionOwner))
    errAbort("Please go back and enter a wiki user name for this session.");
if (isEmpty(sessionName))
    errAbort("Please go back and enter a session name to load.");

safef(query, sizeof(query), "SELECT shared, contents FROM %s "
      "WHERE userName = '%s' AND sessionName = '%s';",
      namedSessionTable, encSessionOwner, encSessionName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    boolean shared = atoi(row[0]);
    if (shared ||
	(userName && sameString(sessionOwner, userName)))
	{
	char *sessionVar = cartSessionVarName();
	unsigned hgsid = cartSessionId(cart);
	struct sqlConnection *conn2 = hConnectCentral();
	sessionTouchLastUse(conn2, encSessionOwner, encSessionName);
	cartRemoveLike(cart, "*");
	cartParseOverHash(cart, row[1]);
	cartSetInt(cart, sessionVar, hgsid);
	if (oldVars)
	    hashEmpty(oldVars);
	/* Overload settings explicitly passed in via CGI (except for the
	 * command that sent us here): */
	loadCgiOverHash(cart, oldVars);
#ifndef GBROWSE
	cartCopyCustomTracks(cart, oldVars);
#endif /* GBROWSE */
	if (isNotEmpty(actionVar))
	    cartRemove(cart, actionVar);
	hDisconnectCentral(&conn2);
	}
    else
	errAbort("Sharing has not been enabled for user %s's session %s.",
		 sessionOwner, sessionName);
    }
else
    errAbort("Could not find session %s for user %s.",
	     sessionName, sessionOwner);
sqlFreeResult(&sr);
freeMem(encSessionName);
}
#endif /* GBROWSE */

void cartLoadSettings(struct lineFile *lf, struct cart *cart,
		      struct hash *oldVars, char *actionVar)
/* Load settings (cartDump output) into current session, and then
 * reload the CGI settings (to support override of session settings).
 * If non-NULL, oldVars will contain values overloaded when reloading CGI.
 * If non-NULL, actionVar is a cartRemove wildcard string specifying the
 * CGI action variable that sent us here. */
{
char *line = NULL;
int size = 0;
char *sessionVar = cartSessionVarName();
unsigned hgsid = cartSessionId(cart);

cartRemoveLike(cart, "*");
cartSetInt(cart, sessionVar, hgsid);
while (lineFileNext(lf, &line, &size))
    {
    char *var = nextWord(&line);
    char *val = line;

    if (sameString(var, sessionVar))
	continue;
    else
	{
	if (val != NULL)
	    {
	    struct dyString *dy = dyStringSub(val, "\\n", "\n");
	    cartAddString(cart, var, dy->string);
	    dyStringFree(&dy);
	    }
	else if (var != NULL)
	    {
	    cartSetString(cart, var, "");
	    }
	} /* not hgsid */
    } /* each line */
if (oldVars)
    hashEmpty(oldVars);
/* Overload settings explicitly passed in via CGI (except for the
 * command that sent us here): */
loadCgiOverHash(cart, oldVars);
#ifndef GBROWSE
cartCopyCustomTracks(cart, oldVars);
#endif /* GBROWSE */

if (isNotEmpty(actionVar))
    cartRemove(cart, actionVar);
}

char *_cartVarDbName(char *db, char *var)
/* generate cart variable name that is local to an assembly database.
 * Only for use inside of cart.h.  WARNING: static return */
{
static char buf[PATH_LEN]; // something rather big
safef(buf, sizeof(buf), "%s_%s", var, db);
return buf;
}

static char *now()
/* Return a mysql-formatted time like "2008-05-19 15:33:34". */
{
char nowBuf[256];
time_t seconds = clock1();
struct tm *theTime = localtime(&seconds);
strftime(nowBuf, sizeof nowBuf, "%Y-%m-%d %H:%M:%S", theTime);
return cloneString(nowBuf);
}

static struct cartDb *emptyCartDb()
/* Create a new empty placeholder cartDb. */
{
struct cartDb *cdb;
AllocVar(cdb);
cdb->contents = cloneString("");
cdb->firstUse = now();
cdb->lastUse = now();
cdb->useCount = 1;
return cdb;
}

struct cart *cartNewEmpty(unsigned int userId, unsigned int sessionId, 
	char **exclude, struct hash *oldVars)
/* Create a new empty cart structure without reading from the database. */
{
struct cart *cart;
char *ex;

AllocVar(cart);
cart->hash = newHash(12);
cart->exclude = newHash(7);
cart->userId = userId;
cart->sessionId = sessionId;
cart->userInfo = emptyCartDb();
cart->sessionInfo = emptyCartDb();

loadCgiOverHash(cart, oldVars);

if (exclude != NULL)
    {
    while ((ex = *exclude++))
	cartExclude(cart, ex);
    }
return cart;
}

struct cart *cartNew(unsigned int userId, unsigned int sessionId, 
	char **exclude, struct hash *oldVars)
/* Load up cart from user & session id's.  Exclude is a null-terminated list of
 * strings to not include */
{
struct cart *cart;
struct sqlConnection *conn = cartDefaultConnector();
char *ex;
boolean userIdFound = FALSE, sessionIdFound = FALSE;

AllocVar(cart);
cart->hash = newHash(12);
cart->exclude = newHash(7);
cart->userId = userId;
cart->sessionId = sessionId;
cart->userInfo = loadDb(conn, "userDb", userId, &userIdFound);
cart->sessionInfo = loadDb(conn, "sessionDb", sessionId, &sessionIdFound);
if (sessionIdFound)
    cartParseOverHash(cart, cart->sessionInfo->contents);
else if (userIdFound)
    cartParseOverHash(cart, cart->userInfo->contents);
char when[1024];
safef(when, sizeof(when), "open %d %d", userId, sessionId);
cartTrace(cart, when, conn);

loadCgiOverHash(cart, oldVars);

#ifndef GBROWSE
/* If some CGI other than hgSession been passed hgSession loading instructions,
 * apply those to cart before we do anything else.  (If this is hgSession,
 * let it handle the settings so it can display feedback to the user.) */
if (! (cgiScriptName() && endsWith(cgiScriptName(), "hgSession")))
    {
    if (cartVarExists(cart, hgsDoOtherUser))
	{
	char *otherUser = cartString(cart, hgsOtherUserName);
	char *sessionName = cartString(cart, hgsOtherUserSessionName);
	struct sqlConnection *conn2 = hConnectCentral();
	cartLoadUserSession(conn2, otherUser, sessionName, cart,
			    oldVars, hgsDoOtherUser);
	hDisconnectCentral(&conn2);
	cartTrace(cart, "after cartLUS", conn);
	}
    else if (cartVarExists(cart, hgsDoLoadUrl))
	{
	char *url = cartString(cart, hgsLoadUrlName);
	struct lineFile *lf = netLineFileOpen(url);
	cartLoadSettings(lf, cart, oldVars, hgsDoLoadUrl);
	lineFileClose(&lf);
	cartTrace(cart, "after cartLS", conn);
	}
    }
#endif /* GBROWSE */

if (exclude != NULL)
    {
    while ((ex = *exclude++))
	cartExclude(cart, ex);
    }

cartDefaultDisconnector(&conn);
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


void cartEncodeState(struct cart *cart, struct dyString *dy)
/* Add a CGI-encoded var=val&... string of all cart variables to dy. */
{
struct hashEl *el, *elList = hashElListHash(cart->hash);
boolean firstTime = TRUE;
char *s = NULL;
for (el = elList; el != NULL; el = el->next)
    {
    if (!hashLookup(cart->exclude, el->name))
	{
	if (firstTime)
	    firstTime = FALSE;
	else
	    dyStringAppendC(dy, '&');
	dyStringAppend(dy, el->name);
	dyStringAppendC(dy, '=');
	s = cgiEncode(el->val);
	dyStringAppend(dy, s);
	freez(&s);
	}
    }
hashElFreeList(&elList);
}

static void saveState(struct cart *cart)
/* Save out state to permanent storage in both user and session db. */
{
struct sqlConnection *conn = cartDefaultConnector();
struct dyString *encoded = newDyString(4096);

/* Make up encoded string holding all variables. */
cartEncodeState(cart, encoded);

/* Make up update statement unless it looks like a robot with
 * a great bunch of variables. */
if (encoded->stringSize < 16*1024 || cart->userInfo->useCount > 0)
    {
    updateOne(conn, "userDb", cart->userInfo, encoded->string, encoded->stringSize);
    updateOne(conn, "sessionDb", cart->sessionInfo, encoded->string, encoded->stringSize);
    }
else
    {
    fprintf(stderr, "Cart stuffing bot?  Not writing %d bytes to cart on first use of %d from IP=%s\n",
    	encoded->stringSize, cart->userInfo->id, cgiRemoteAddr());
    /* Do increment the useCount so that cookie-users don't get stuck here: */
    updateOne(conn, "userDb", cart->userInfo, "", 0);
    }

/* Cleanup */
cartDefaultDisconnector(&conn);
dyStringFree(&encoded);
}

void cartCheckout(struct cart **pCart)
/* Free up cart and save it to database. */
{
struct cart *cart = *pCart;
if (cart != NULL)
    {
    saveState(cart);
    struct sqlConnection *conn = cartDefaultConnector();
    cartTrace(cart, "checkout", conn);
    cartDefaultDisconnector(&conn);
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
safef(buf, sizeof(buf), "%s=%u", cartSessionVarName(), cartSessionId(cart));
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
while (hel != NULL)
    {
    struct hashEl *nextHel = hashLookupNext(hel);
    freez(&hel->val);
    hashRemove(cart->hash, var);
    hel = nextHel;
    }
}

void cartRemoveExcept(struct cart *cart, char **except)
/* Remove variables except those in null terminated except array
 * from cart.  Except array may be NULL in which case all
 * are removed. */
{
struct hash *exceptHash = newHash(10);
struct hashEl *list = NULL, *el;
char *s;

/* Build up hash of things to exclude. */
if (except != NULL)
    {
    while ((s = *except++) != NULL)
	hashAdd(exceptHash, s, NULL);
    }

/* Get all cart variables and remove most of them. */
list = hashElListHash(cart->hash);
for (el = list; el != NULL; el = el->next)
    {
    if (!hashLookup(exceptHash, el->name))
	cartRemove(cart, el->name);
    }

/* Clean up. */
hashFree(&exceptHash);
hashElFreeList(&list);
}

void cartRemoveLike(struct cart *cart, char *wildCard)
/* Remove all variable from cart that match wildCard. */
{
struct hashEl *el, *elList = hashElListHash(cart->hash);

slSort(&elList, hashElCmp);
for (el = elList; el != NULL; el = el->next)
    {
    if (wildMatch(wildCard, el->name))
	cartRemove(cart, el->name);
    }
hashElFreeList(&el);
}

void cartRemovePrefix(struct cart *cart, char *prefix)
/* Remove variables with given prefix from cart. */
{
struct hashEl *el, *elList = hashElListHash(cart->hash);

slSort(&elList, hashElCmp);
for (el = elList; el != NULL; el = el->next)
    {
    if (startsWith(prefix, el->name))
	cartRemove(cart, el->name);
    }
hashElFreeList(&el);
}

boolean cartVarExists(struct cart *cart, char *var)
/* Return TRUE if variable is in cart. */
{
return hashFindVal(cart->hash, var) != NULL;
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

char *cartNonemptyString(struct cart *cart, char *name)
/* Return string value associated with name.  Return NULL
 * if value doesn't exist or if it is pure white space. */
{
char *val = trimSpaces(cartOptionalString(cart, name));
if (val != NULL && val[0] == 0)
    val = NULL;
return val;
}

char *cartUsualString(struct cart *cart, char *var, char *usual)
/* Return variable value if it exists or usual if not. */
{
char *s = cartOptionalString(cart, var);
if (s == NULL)
    return usual;
return s;
}

char *cartCgiUsualString(struct cart *cart, char *var, char *usual)
/* Look for var in CGI, then in cart, if not found then return usual. */
{
char *val = cgiOptionalString(var);
if (val != NULL)
    return(val);
if (cart != NULL)
    return cartUsualString(cart, var, usual);
return(usual);
}

struct slName *cartOptionalSlNameList(struct cart *cart, char *var)
/* Return slName list (possibly with multiple values for the same var) or
 * NULL if not found. */
{
struct slName *slnList = NULL;
struct hashEl *hel = hashLookup(cart->hash, var);
while (hel != NULL)
    {
    if (hel->val != NULL)
	{
	struct slName *sln = slNameNew(hel->val);
	slAddHead(&slnList, sln);
	}
    hel = hashLookupNext(hel);
    }
return slnList;
}

void cartAddString(struct cart *cart, char *var, char *val)
/* Add string valued cart variable (if called multiple times on same var,
 * will create a list -- retrieve with cartOptionalSlNameList. */
{
hashAdd(cart->hash, var, cloneString(val));
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

int cartUsualIntClipped(struct cart *cart, char *var, int usual,
	int minVal, int maxVal)
/* Return integer variable clipped to lie between minVal/maxVal */
{
int val = cartUsualInt(cart, var, usual);
if (val < minVal) val = minVal;
if (val > maxVal) val = maxVal;
return val;
}

int cartCgiUsualInt(struct cart *cart, char *var, int usual)
/* Look for var in CGI, then in cart, if not found then return usual. */
{
char *val = cgiOptionalString(var);
if (val != NULL)
    return atoi(val);
if (cart != NULL)
    return cartUsualInt(cart, var, usual);
return(usual);
}

void cartSetInt(struct cart *cart, char *var, int val)
/* Set integer value. */
{
char buf[32];
safef(buf, sizeof(buf), "%d", val);
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

double cartCgiUsualDouble(struct cart *cart, char *var, double usual)
/* Look for var in CGI, then in cart, if not found then return usual. */
{
char *val = cgiOptionalString(var);
if (val != NULL)
    return atof(val);
if (cart != NULL)
    return cartUsualDouble(cart, var, usual);
return(usual);
}

void cartSetDouble(struct cart *cart, char *var, double val)
/* Set double value. */
{
char buf[32];
safef(buf, sizeof(buf), "%f", val);
cartSetString(cart, var, buf);
}

boolean cartBoolean(struct cart *cart, char *var)
/* Retrieve cart boolean. */
{
char *s = cartString(cart, var);
if (sameString(s, "on") || atoi(s) != 0)
    return TRUE;
else
    return FALSE;
}

boolean cartUsualBoolean(struct cart *cart, char *var, boolean usual)
/* Return variable value if it exists or usual if not. */
{
return cartUsualInt(cart, var, usual);
}

boolean cartCgiUsualBoolean(struct cart *cart, char *var, boolean usual)
/* Look for var in CGI, then in cart, if not found then return usual. */
{
if (cgiBooleanDefined(var))
    return cgiBoolean(var);
if (cart != NULL)
    return cgiBoolean(var) || cartUsualBoolean(cart, var, usual);
return(usual);
}

void cartSetBoolean(struct cart *cart, char *var, boolean val)
/* Set boolean value. */
{
cartSetInt(cart,var,val);
}

void cartMakeTextVar(struct cart *cart, char *var, char *defaultVal, int charSize)
/* Make a text control filled with value from cart if it exists or
 * default value otherwise.  If charSize is zero it's calculated to fit
 * current value.  Default value may be NULL. */
{
cgiMakeTextVar(var, cartUsualString(cart, var, defaultVal), charSize);
}

void cartMakeIntVar(struct cart *cart, char *var, int defaultVal, int maxDigits)
/* Make a text control filled with integer value - from cart if available
 * otherwise default.  */
{
cgiMakeIntVar(var, cartUsualInt(cart, var, defaultVal), maxDigits);
}

void cartMakeDoubleVar(struct cart *cart, char *var, double defaultVal, int maxDigits)
/* Make a text control filled with integer value - from cart if available
 * otherwise default.  */
{
cgiMakeDoubleVar(var, cartUsualDouble(cart, var, defaultVal), maxDigits);
}

void cartMakeCheckBox(struct cart *cart, char *var, boolean defaultVal)
/* Make a check box filled with value from cart if it exists or
 * default value otherwise.  */
{
cgiMakeCheckBox(var, cartUsualBoolean(cart, var, defaultVal));
}

void cartMakeRadioButton(struct cart *cart, char *var, char *val, char *defaultVal)
/* Make a radio button that is selected if cart variable exists and matches
 * value (or value matches default val if cart var doesn't exist). */
{
boolean matches = sameString(val, cartUsualString(cart, var, defaultVal));
cgiMakeRadioButton(var, val, matches);
}

void cartSaveSession(struct cart *cart)
/* Save session in a hidden variable. This needs to be called
 * somewhere inside of form or bad things will happen when user
 * has multiple windows open. */
{
char buf[64];
safef(buf, sizeof(buf), "%u", cart->sessionInfo->id);
cgiMakeHiddenVar(sessionVar, buf);
}

static void cartDumpItem(struct hashEl *hel)
/* Dump one item in cart hash */
{
struct dyString *dy = NULL;
char *val = (char *)(hel->val);
stripChar(val, '\r');
dy = dyStringSub(val, "\n", "\\n");
printf("%s %s\n", hel->name, dy->string);
dyStringFree(&dy);
}

void cartDumpList(struct hashEl *elList)
/* Dump list of cart variables. */
{
struct hashEl *el;

if (elList == NULL)
    return;
slSort(&elList, hashElCmp);
for (el = elList; el != NULL; el = el->next)
    cartDumpItem(el);
hashElFreeList(&elList);
}

void cartDump(struct cart *cart)
/* Dump contents of cart. */
{
struct hashEl *elList = hashElListHash(cart->hash);
cartDumpList(elList);
}

void cartDumpPrefix(struct cart *cart, char *prefix)
/* Dump all cart variables with prefix */
{
struct hashEl *elList = cartFindPrefix(cart, prefix);
cartDumpList(elList);
}

void cartDumpLike(struct cart *cart, char *wildcard)
/* Dump all cart variables matching wildcard */
{
struct hashEl *elList = cartFindLike(cart, wildcard);
cartDumpList(elList);
}

char *cartFindFirstLike(struct cart *cart, char *wildCard)
/* Find name of first variable that matches wildCard in cart. 
 * Return NULL if none. */
{
struct hashEl *el, *elList = hashElListHash(cart->hash);
char *name = NULL;

for (el = elList; el != NULL; el = el->next)
    {
    if (wildMatch(wildCard, el->name))
	{
	name = el->name;
	break;
	}
    }
hashElFreeList(&el);
return name;
}

static struct hashEl *cartFindSome(struct cart *cart, char *pattern,
	boolean (*match)(char *a, char *b))
/* Return list of name/val pairs from cart where name matches 
 * pattern.  Free when done with hashElFreeList. */
{
struct hashEl *el, *next, *elList = hashElListHash(cart->hash);
struct hashEl *outList = NULL;

for (el = elList; el != NULL; el = next)
    {
    next = el->next;
    if (match(pattern, el->name))
	{
	slAddHead(&outList, el);
	}
    else
        {
	hashElFree(&el);
	}
    }
return outList;
}
	
struct hashEl *cartFindLike(struct cart *cart, char *wildCard)
/* Return list of name/val pairs from cart where name matches 
 * wildcard.  Free when done with hashElFreeList. */
{
return cartFindSome(cart, wildCard, wildMatch);
}

struct hashEl *cartFindPrefix(struct cart *cart, char *prefix)
/* Return list of name/val pairs from cart where name starts with 
 * prefix.  Free when done with hashElFreeList. */
{
return cartFindSome(cart, prefix, startsWith);
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
char *hguidString = findCookieData(cookieName);
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
safef(query, sizeof(query), "update %s set contents='' where id=%u",
      table, id);
sqlUpdate(conn, query);
}

void cartResetInDb(char *cookieName)
/* Clear cart in database. */
{
int hguid = getCookieId(cookieName);
int hgsid = getSessionId();
struct sqlConnection *conn = cartDefaultConnector();
clearDbContents(conn, "userDb", hguid);
clearDbContents(conn, "sessionDb", hgsid);
cartDefaultDisconnector(&conn);
}

void cartWriteCookie(struct cart *cart, char *cookieName)
/* Write out HTTP Set-Cookie statement for cart. */
{
printf("Set-Cookie: %s=%u; path=/; domain=%s; expires=%s\r\n",
	cookieName, cart->userInfo->id, cfgVal("central.domain"), cookieDate());
}

struct cart *cartForSession(char *cookieName, char **exclude, 
	struct hash *oldVars)
/* This gets the cart without writing any HTTP lines at all to stdout. */
{
int hguid = getCookieId(cookieName);
int hgsid = getSessionId();
struct cart *cart = cartNew(hguid, hgsid, exclude, oldVars);
cartExclude(cart, sessionVar);
if (sameOk(cfgOption("signalsHandler"), "on"))  /* most cgis call this routine */
    initSigHandlers();
return cart;
}

struct cart *cartAndCookieWithHtml(char *cookieName, char **exclude, 
	struct hash *oldVars, boolean doContentType)
/* Load cart from cookie and session cgi variable.  Write cookie 
 * and optionally content-type part HTTP preamble to web page.  Don't 
 * write any HTML though. */
{
if (doContentType)
    htmlPushEarlyHandlers();
else
    pushWarnHandler(cartEarlyWarningHandler);
struct cart *cart = cartForSession(cookieName, exclude, oldVars);
popWarnHandler();
cartWriteCookie(cart, cookieName);
if (doContentType)
    {
    puts("Content-Type:text/html");
    puts("\n");
    }
return cart;
}

struct cart *cartAndCookie(char *cookieName, char **exclude, 
	struct hash *oldVars)
/* Load cart from cookie and session cgi variable.  Write cookie and 
 * content-type part HTTP preamble to web page.  Don't write any HTML though. */
{
return cartAndCookieWithHtml(cookieName, exclude, oldVars, TRUE);
}

struct cart *cartAndCookieNoContent(char *cookieName, char **exclude, 
	struct hash *oldVars)
/* Load cart from cookie and session cgi variable. Don't write out
 * content type or any HTML. */
{
return cartAndCookieWithHtml(cookieName, exclude, oldVars, FALSE);
}

static void cartErrorCatcher(void (*doMiddle)(struct cart *cart), 
	struct cart *cart)
/* Wrap error catcher around call to do middle. */
{
int status = setjmp(htmlRecover);
pushAbortHandler(htmlAbort);
if (status == 0)
    doMiddle(cart);
popAbortHandler();
}

void cartEarlyWarningHandler(char *format, va_list args)
/* Write an error message so user can see it before page is really started. */
{
static boolean initted = FALSE;
if (!initted)
    {
    htmStart(stdout, "Early Error");
    initted = TRUE;
    }
printf("%s", htmlWarnStartPattern());
htmlVaParagraph(format,args);
printf("%s", htmlWarnEndPattern());
}

void cartWarnCatcher(void (*doMiddle)(struct cart *cart), struct cart *cart, WarnHandler warner)
/* Wrap error and warning handlers around doMiddle. */
{
pushWarnHandler(warner);
cartErrorCatcher(doMiddle, cart);
popWarnHandler();
}

static boolean inWeb = FALSE;

void cartHtmlStart(char *title)
/* Write HTML header and put in normal error handler. */
{
pushWarnHandler(htmlVaWarn);
htmStart(stdout, title);
}

void cartVaWebStart(struct cart *cart, char *db, char *format, va_list args)
/* Print out pretty wrapper around things when working
 * from cart. */
{
pushWarnHandler(htmlVaWarn);
webStartWrapper(cart, db, format, args, FALSE, FALSE);
inWeb = TRUE;
}

void cartWebStart(struct cart *cart, char *db, char *format, ...)
/* Print out pretty wrapper around things when working
 * from cart. */
{
va_list args;
va_start(args, format);
cartVaWebStart(cart, db, format, args);
va_end(args);
}

void cartWebEnd()
/* Write out HTML footer and get rid or error handler. */
{
webEnd();
popWarnHandler();
}

void cartFooter(void)
/* Write out HTML footer, possibly with googleAnalytics too */
{
#ifndef GBROWSE
googleAnalytics();	/* can't do this in htmlEnd	*/
#endif /* GBROWSE */
htmlEnd();		/* because it is in a higher library */
}

void cartHtmlEnd()
/* Write out HTML footer and get rid or error handler. */
{
if (inWeb)
    webEnd();	/*	this does googleAnalytics for a lot of CGIs	*/
else
    cartFooter();
popWarnHandler();
}

void cartEmptyShell(void (*doMiddle)(struct cart *cart), char *cookieName, 
	char **exclude, struct hash *oldVars)
/* Get cart and cookies and set up error handling, but don't start writing any
 * html yet. The doMiddleFunction has to call cartHtmlStart(title), and
 * cartHtmlEnd(), as well as writing the body of the HTML. 
 * oldVars - those in cart that are overlayed by cgi-vars are
 * put in optional hash oldVars. */
{
struct cart *cart = cartAndCookie(cookieName, exclude, oldVars);
cartWarnCatcher(doMiddle, cart, cartEarlyWarningHandler);
cartCheckout(&cart);
}

void cartHtmlShellPB(char *title, void (*doMiddle)(struct cart *cart),
        char *cookieName, char **exclude, struct hash *oldVars)
/* For Proteome Browser, Load cart from cookie and session cgi variable.  Write web-page
 * preamble, call doMiddle with cart, and write end of web-page.
 * Exclude may be NULL.  If it exists it's a comma-separated list of
 * variables that you don't want to save in the cart between
 * invocations of the cgi-script. */
{
struct cart *cart;
char *db, *org;
char titlePlus[128];
char *proteinID;
pushWarnHandler(cartEarlyWarningHandler);
cart = cartAndCookie(cookieName, exclude, oldVars);
getDbAndGenome(cart, &db, &org, oldVars);
proteinID = cartOptionalString(cart, "proteinID");
safef(titlePlus, sizeof(titlePlus), "%s protein %s - %s", org, proteinID, title);
popWarnHandler();
htmStart(stdout, titlePlus);
cartWarnCatcher(doMiddle, cart, htmlVaWarn);
cartCheckout(&cart);
cartFooter();
}

void cartHtmlShellPbGlobal(char *title, void (*doMiddle)(struct cart *cart),
        char *cookieName, char **exclude, struct hash *oldVars)
/* For Proteome Browser, Load cart from cookie and session cgi variable.  Write web-page
 * preamble, call doMiddle with cart, and write end of web-page.
 * Exclude may be NULL.  If it exists it's a comma-separated list of
 * variables that you don't want to save in the cart between
 * invocations of the cgi-script. */
/* cartHtmlShellPbGloabl differs from cartHtmlShellPB that it does not call getDbAndGenome */
{
struct cart *cart;
char titlePlus[128];
char *proteinID;
pushWarnHandler(cartEarlyWarningHandler);
cart = cartAndCookie(cookieName, exclude, oldVars);
proteinID = cartOptionalString(cart, "proteinID");
safef(titlePlus, sizeof(titlePlus), "Protein %s - %s", proteinID, title);
popWarnHandler();
htmStart(stdout, titlePlus);
cartWarnCatcher(doMiddle, cart, htmlVaWarn);
cartCheckout(&cart);
cartFooter();
}

void cartHtmlShellWithHead(char *head, char *title, void (*doMiddle)(struct cart *cart), 
	char *cookieName, char **exclude, struct hash *oldVars)
/* Load cart from cookie and session cgi variable.  Write web-page 
 * preamble including head and title, call doMiddle with cart, and write end of web-page.   
 * Exclude may be NULL.  If it exists it's a comma-separated list of 
 * variables that you don't want to save in the cart between
 * invocations of the cgi-script. */
{
struct cart *cart;
char *db, *org, *pos, *clade=NULL;
char titlePlus[128];
char extra[128];
pushWarnHandler(cartEarlyWarningHandler);
cart = cartAndCookie(cookieName, exclude, oldVars);
getDbAndGenome(cart, &db, &org, oldVars);
clade = hClade(org);
pos = cartOptionalString(cart, positionCgiName);
pos = addCommasToPos(db, stripCommas(pos));
*extra = 0;
if (pos == NULL && org != NULL) 
    safef(titlePlus,sizeof(titlePlus), "%s%s - %s",org, extra, title );
else if (pos != NULL && org == NULL)
    safef(titlePlus,sizeof(titlePlus), "%s - %s",pos, title );
else if (pos == NULL && org == NULL)
    safef(titlePlus,sizeof(titlePlus), "%s", title );
else
    safef(titlePlus,sizeof(titlePlus), "%s%s %s - %s",org, extra,pos, title );
popWarnHandler();
htmStartWithHead(stdout, head, titlePlus);
cartWarnCatcher(doMiddle, cart, htmlVaWarn);
cartCheckout(&cart);
cartFooter();
}

void cartHtmlShell(char *title, void (*doMiddle)(struct cart *cart), 
	char *cookieName, char **exclude, struct hash *oldVars)
/* Load cart from cookie and session cgi variable.  Write web-page 
 * preamble, call doMiddle with cart, and write end of web-page.   
 * Exclude may be NULL.  If it exists it's a comma-separated list of 
 * variables that you don't want to save in the cart between
 * invocations of the cgi-script. */
{
cartHtmlShellWithHead("", title, doMiddle, cookieName, exclude, oldVars);
}

void cartSetDbConnector(DbConnector connector) 
/* Set the connector that will be used by the cart to connect to the
 * database. Default connector is hConnectCart */
{
cartDefaultConnector = connector;
}

void cartSetDbDisconnector(DbDisconnect disconnector) 
/* Set the connector that will be used by the cart to disconnect from the
 * database. Default disconnector is hDisconnectCart */
{
cartDefaultDisconnector = disconnector;
}

static void saveDefaultGsidLists(char *genomeDb, struct cart *cart)
/* save the default lists of GSID subject and sequence IDs to 2 internal files under trash/ct
   for applications to use */
{
char *outName = NULL;
char *outName2= NULL;
FILE *outF, *outF2;
struct tempName tn;
struct tempName tn2;
struct sqlResult *sr=NULL, *sr2=NULL;
char **row, **row2;
char query[255], query2[255];
char *chp;
struct sqlConnection *conn, *conn2;

conn= hAllocConn(genomeDb);
conn2= hAllocConn(genomeDb);
    
trashDirFile(&tn, "ct", "gsidSubj", ".list");
outName = tn.forCgi;

trashDirFile(&tn2, "ct", "gsidSeq", ".list");
outName2 = tn2.forCgi;

outF = mustOpen(outName,"w");
outF2= mustOpen(outName2,"w");

safef(query, sizeof(query), "select distinct subjId from hgFixed.gsIdXref order by subjId");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    fprintf(outF, "%s\n", row[0]);

    safef(query2, sizeof(query2),
          "select dnaSeqId from hgFixed.gsIdXref where subjId='%s' order by dnaSeqId", row[0]);

    sr2 = sqlGetResult(conn2, query2);
    while ((row2 = sqlNextRow(sr2)) != NULL)
        {
        /* Remove "ss." from the front of the DNA sequence ID,
           so that they could be used both for DNA and protein MSA maf display */
        chp = strstr(row2[0], "ss.");
        if (chp != NULL)
            {
            fprintf(outF2, "%s\t%s\n", chp+3L, row[0]);
            }
        else
            {
            fprintf(outF2, "%s\t%s\n", row2[0], row[0]);
            }
        }
    sqlFreeResult(&sr2);
    }

sqlFreeResult(&sr);
carefulClose(&outF);
carefulClose(&outF2);
hFreeConn(&conn);
hFreeConn(&conn2);

cartSetString(cart, gsidSubjList, outName);
cartSetString(cart, gsidSeqList, outName2);
}

char *cartGetOrderFromFile(char *genomeDb, struct cart *cart, char *speciesUseFile)
/* Look in a cart variable that holds the filename that has a list of 
 * species to show in a maf file */
{
char *val;
struct dyString *orderDY = dyStringNew(256);
char *words[16];
if ((val = cartUsualString(cart, speciesUseFile, NULL)) == NULL)
    {
    if (hIsGsidServer())
	{
	saveDefaultGsidLists(genomeDb, cart);
	/* now it should be set */
	val = cartUsualString(cart, speciesUseFile, NULL);
	if (val == NULL)
    	    errAbort("can't find species list file var '%s' in cart\n",speciesUseFile);
	}
    else
	{
    	errAbort("can't find species list file var '%s' in cart\n",speciesUseFile);
	}
    }

struct lineFile *lf = lineFileOpen(val, TRUE);

if (lf == NULL)
    errAbort("can't open species list file %s",val);

while( ( lineFileChopNext(lf, words, sizeof(words)/sizeof(char *)) ))
    dyStringPrintf(orderDY, "%s ",words[0]);

return dyStringCannibalize(&orderDY);
}

char *cartGetOrderFromFileAndMsaTable(char *genomeDb, struct cart *cart, char *speciesUseFile, char *msaTable)
/* This function is used for GSID server only.
   Look in a cart variable that holds the filename that has a list of 
 * species to show in a maf file and also restrict the results by the IDs existing in an MSA table*/
{
char *val;
struct sqlResult *sr=NULL;
char query[255];
struct sqlConnection *conn;

struct dyString *orderDY = dyStringNew(256);
char *words[16];
if ((val = cartUsualString(cart, speciesUseFile, NULL)) == NULL)
    {
    if (hIsGsidServer())
	{
	saveDefaultGsidLists(genomeDb, cart);

	/* now it should be set */
	val = cartUsualString(cart, speciesUseFile, NULL);
	if (val == NULL)
    	    errAbort("can't find species list file var '%s' in cart\n",speciesUseFile);
	}
    else
	{
    	errAbort("can't find species list file var '%s' in cart\n",speciesUseFile);
	}
    }

struct lineFile *lf = lineFileOpen(val, TRUE);

if (lf == NULL)
    errAbort("can't open species list file %s",val);

if (hIsGsidServer())
    {
    conn= hAllocConn(genomeDb);
    while( ( lineFileChopNext(lf, words, sizeof(words)/sizeof(char *)) ))
    	{
	safef(query, sizeof(query), 
	      "select id from %s where id like '%s%s'", msaTable, "%",  words[0]);
	sr = sqlGetResult(conn, query);
	if (sqlNextRow(sr) != NULL)
    	    {
    	    dyStringPrintf(orderDY, "%s ",words[0]);
	    sqlFreeResult(&sr);
    	    }
  	}
    hFreeConn(&conn);
    }
else
    {
    while( ( lineFileChopNext(lf, words, sizeof(words)/sizeof(char *)) ))
    	{
    	dyStringPrintf(orderDY, "%s ",words[0]);
    	}
    }
return dyStringCannibalize(&orderDY);
}
