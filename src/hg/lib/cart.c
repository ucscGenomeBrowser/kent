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
#include "jsHelper.h"
#include "trashDir.h"
#ifndef GBROWSE
#include "customFactory.h"
#include "googleAnalytics.h"
#include "wikiLink.h"
#endif /* GBROWSE */
#include "hgMaf.h"
#include "hui.h"

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
	"p=%s f=%s t=%s pid=%ld %s\n",
	when,
	u->id, uLen, u->useCount, s->id, sLen, s->useCount,
	pix, textSize, trackControls, (long)getpid(), cgiRemoteAddr());
if (cart->userId != 0 && u->id != cart->userId)
    fprintf(stderr, "ASH: bad userId %d --> %d!  pid=%ld\n",
	    cart->userId, u->id, (long)getpid());
if (cart->sessionId != 0 && s->id != cart->sessionId)
    fprintf(stderr, "ASH: bad sessionId %d --> %d!  pid=%ld\n",
	    cart->sessionId, s->id, (long)getpid());
}

boolean cartTablesOk(struct sqlConnection *conn)
/* Return TRUE if cart tables are accessible (otherwise, the connection
 * doesn't do us any good). */
{
if (!sqlTableExists(conn, "userDb"))
    {
    fprintf(stderr, "ASH: cartTablesOk failed on %s.userDb!  pid=%ld\n",
	    sqlGetDatabase(conn), (long)getpid());
    return FALSE;
    }
if (!sqlTableExists(conn, "sessionDb"))
    {
    fprintf(stderr, "ASH: cartTablesOk failed on %s.sessionDb!  pid=%ld\n",
	    sqlGetDatabase(conn), (long)getpid());
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

#define CART_DIFFS_INCLUDE_EMPTIES
#ifdef CART_DIFFS_INCLUDE_EMPTIES
// NOTE: New cgi vars not in old cart cannot be distinguished from vars not newly set by cgi.
//       Solution: Add 'empty' var to old vars for cgi vars not already in cart
if (hel == NULL)
    hashAdd(oldVars, var, cloneString(CART_VAR_EMPTY));
#endif///def CART_DIFFS_INCLUDE_EMPTIES

while (hel != NULL)
    {
    hashAdd(oldVars, var, cloneString(hel->val));
    hel = hashLookupNext(hel);
    }
}

static void cartJustify(struct cart *cart, struct hash *oldVars)
/* Handles rules for allowing some cart settings to override others. */
{
// New priority (position) settings should erase imgOrd settings
if(oldVars == NULL)
    return;
#define POSITION_SUFFIX ".priority"
#define IMGORD_SUFFIX   "_imgOrd"
struct hashEl *list = NULL, *el;
list = hashElListHash(cart->hash);
//warn("cartJustify() begins");
for (el = list; el != NULL; el = el->next)
    {
    if (endsWith(el->name,POSITION_SUFFIX))
        {
        if (cartValueHasChanged(cart,oldVars,el->name,TRUE,TRUE))
            {
            int suffixOffset = strlen(el->name) - strlen(POSITION_SUFFIX);
            if(suffixOffset>0)
                {
                char *name = cloneString(el->name);
                safecpy(name+suffixOffset,strlen(POSITION_SUFFIX),IMGORD_SUFFIX); // We know that POSITION_SUFFIX is longer than IMGORD_SUFFIX
                //warn("Removing imgOrd for %s",name);
                cartRemove(cart, name); // Removes if found
                freeMem(name);
                }
            }
        }
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
    // Support for 2 boolean CBs: checked/unchecked (1/0) and enabled/disabled:(-1/-2)
	char *val = (cgiVarExists(booVar) ? "1" : cv->val);
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
	    storeInOldVars(cart, oldVars, cv->name);
	    cartRemove(cart, multVar);
	    }
	}
    }

/* Handle non-boolean vars. */
for (cv = cgiVarList(); cv != NULL; cv = cv->next)
    {
    if (! (startsWith(booShadow, cv->name) || hashLookup(booHash, cv->name)))
	{
	storeInOldVars(cart, oldVars, cv->name);
	cartRemove(cart, cv->name);
        if (differentString(cv->val, CART_VAR_EMPTY))  // NOTE: CART_VAR_EMPTY logic not implemented for boolShad
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

struct cart *cartFromHash(struct hash *hash)
/* Create a cart from hash */
{
struct cart *cart;
AllocVar(cart);
cart->hash = hash;
cart->exclude = newHash(7);
cart->userInfo = emptyCartDb();
cart->sessionInfo = emptyCartDb();
return cart;
}

struct cart *cartOfNothing()
/* Create a new, empty, cart with no real connection to the database. */
{
return cartFromHash(newHash(0));
}

struct cart *cartFromCgiOnly(unsigned int userId, unsigned int sessionId,
	char **exclude, struct hash *oldVars)
/* Create a new cart that contains only CGI variables, nothing from the
 * database. */
{
struct cart *cart = cartOfNothing();
cart->userId = userId;
cart->sessionId = sessionId;
loadCgiOverHash(cart, oldVars);

if (exclude != NULL)
    {
    char *ex;
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

// I think this is the place to justify old and new values
cartJustify(cart, oldVars);

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

static char *cartMultShadowVar(struct cart *cart, char *var)
/* Return a pointer to the list variable shadow variable name for var.
 * Don't modify or free result. */
{
static char multShadowVar[PATH_LEN];
safef(multShadowVar, sizeof(multShadowVar), "%s%s", cgiMultListShadowPrefix(), var);
return multShadowVar;
}

static int cartRemoveAndCountNoShadow(struct cart *cart, char *var)
/* Remove variable from cart, returning count of removed vars. */
{
int removed = 0;
struct hashEl *hel = hashLookup(cart->hash, var);
while (hel != NULL)
    {
    struct hashEl *nextHel = hashLookupNext(hel);
    freez(&hel->val);
    hashRemove(cart->hash, var);
    removed++;
    hel = nextHel;
    }
return removed;
}

static int cartRemoveAndCount(struct cart *cart, char *var)
/* Remove variable from cart, returning count of removed vars. */
{
int removed = cartRemoveAndCountNoShadow(cart, var);
(void)cartRemoveAndCountNoShadow(cart, cartMultShadowVar(cart, var));
return removed;
}

void cartRemove(struct cart *cart, char *var)
/* Remove variable from cart. */
{
(void)cartRemoveAndCount(cart, var);
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

struct slPair *cartVarsLike(struct cart *cart, char *wildCard)
/* Return a slPair list of cart vars that match the wildcard */
{
struct slPair *cartVars = NULL;
struct hashEl *el, *elList = hashElListHash(cart->hash);
slSort(&elList, hashElCmp);
for (el = elList; el != NULL; el = el->next)
    {
    if (wildMatch(wildCard, el->name))
        slAddHead(&cartVars,slPairNew(el->name,el->val));
    }
hashElFreeList(&elList);
return cartVars;
}

struct slPair *cartVarsWithPrefix(struct cart *cart, char *prefix)
/* Return a slPair list of cart vars that begin with prefix */
{
struct slPair *cartVars = NULL;
struct hashEl *el, *elList = hashElListHash(cart->hash);
slSort(&elList, hashElCmp);
for (el = elList; el != NULL; el = el->next)
    {
    if (startsWith(prefix, el->name))
        slAddHead(&cartVars,slPairNew(el->name,el->val));
    }
hashElFreeList(&elList);
return cartVars;
}

struct slPair *cartVarsWithPrefixLm(struct cart *cart, char *prefix, struct lm *lm)
/* Return list of cart vars that begin with prefix allocated in local memory.
 * Quite a lot faster than cartVarsWithPrefix. */
{
struct slPair *cartVars = NULL;
struct hash *hash = cart->hash;
int hashSize = hash->size;
struct hashEl *hel;
int i;
for (i=0; i<hashSize; ++i)
    {
    for (hel = hash->table[i]; hel != NULL; hel = hel->next)
        {
	if (startsWith(prefix, hel->name))
	    {
	    struct slPair *pair;
	    lmAllocVar(lm, pair);
	    pair->name = lmCloneString(lm, hel->name);
	    pair->val = lmCloneString(lm, hel->val);
	    slAddHead(&cartVars, pair);
	    }
	}
    }
return cartVars;
}

void cartRemoveLike(struct cart *cart, char *wildCard)
/* Remove all variable from cart that match wildCard. */
{
struct slPair *cartVars = cartVarsLike(cart,wildCard);
while(cartVars != NULL)
    {
    struct slPair *cartVar = slPopHead(&cartVars);
    cartRemove(cart, cartVar->name);
    freeMem(cartVar);
    }
}

void cartRemovePrefix(struct cart *cart, char *prefix)
/* Remove variables with given prefix from cart. */
{
struct slPair *cartVars = cartVarsWithPrefix(cart,prefix);
while(cartVars != NULL)
    {
    struct slPair *cartVar = slPopHead(&cartVars);
    cartRemove(cart, cartVar->name);
    freeMem(cartVar);
    }
}

boolean cartVarExists(struct cart *cart, char *var)
/* Return TRUE if variable is in cart. */
{
return hashFindVal(cart->hash, var) != NULL;
}

boolean cartListVarExists(struct cart *cart, char *var)
/* Return TRUE if a list variable is in cart (list may still be empty). */
{
return cartVarExists(cart, cartMultShadowVar(cart, var));
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
if (sameString(s, "on") || atoi(s) > 0)
    return TRUE;
else
    return FALSE;
}

boolean cartUsualBoolean(struct cart *cart, char *var, boolean usual)
/* Return variable value if it exists or usual if not. */
{
char *s = cartOptionalString(cart, var);
if (s == NULL)
    return usual;
return (sameString(s, "on") || atoi(s) > 0);
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
cartSetInt(cart,var,(val?1:0)); // Be explicit because some cartBools overloaded with negative "disabled" values
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

static void cartDumpItem(struct hashEl *hel,boolean asTable)
/* Dump one item in cart hash */
{
char *var = htmlEncode(hel->name);
char *val = htmlEncode((char *)(hel->val));
if (asTable)
    {
    printf("<TR><TD>%s</TD><TD>", var);
    int width=(strlen(val)+1)*8;
    if(width<100)
        width = 100;
    cgiMakeTextVarWithExtraHtml(hel->name, val, width, "onchange='setCartVar(this.name,this.value);'");
    printf("</TD></TR>\n");
    }
else
    printf("%s %s\n", var, val);

freeMem(var);
freeMem(val);
}

void cartDumpList(struct hashEl *elList,boolean asTable)
/* Dump list of cart variables optionally as a table with ajax update support. */
{
struct hashEl *el;

if (elList == NULL)
    return;
slSort(&elList, hashElCmp);
if (asTable)
    printf("<table>\n");
for (el = elList; el != NULL; el = el->next)
    cartDumpItem(el,asTable);
if (asTable)
    {
    printf("<tr><td colspan=2>&nbsp;&nbsp;<em>count: %d</em></td></tr>\n",slCount(elList));
    printf("</table>\n");
    }
hashElFreeList(&elList);
}

void cartDump(struct cart *cart)
/* Dump contents of cart. */
{
struct hashEl *elList = hashElListHash(cart->hash);
cartDumpList(elList,cartVarExists(cart,CART_DUMP_AS_TABLE));
}

void cartDumpPrefix(struct cart *cart, char *prefix)
/* Dump all cart variables with prefix */
{
struct hashEl *elList = cartFindPrefix(cart, prefix);
cartDumpList(elList,cartVarExists(cart,CART_DUMP_AS_TABLE));
}

void cartDumpLike(struct cart *cart, char *wildcard)
/* Dump all cart variables matching wildcard */
{
struct hashEl *elList = cartFindLike(cart, wildcard);
cartDumpList(elList,cartVarExists(cart,CART_DUMP_AS_TABLE));
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
	boolean (*match)(const char *a, const char *b))
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
    initSigHandlers(hDumpStackEnabled());
char *httpProxy = cfgOption("httpProxy");  /* most cgis call this routine */
if (httpProxy)
    setenv("http_proxy", httpProxy, TRUE);   /* net.c cannot see the cart, pass the value through env var */
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
hDumpStackPushAbortHandler();
if (status == 0)
    doMiddle(cart);
hDumpStackPopAbortHandler();
popAbortHandler();
}

void cartEarlyWarningHandler(char *format, va_list args)
/* Write an error message so user can see it before page is really started. */
{
static boolean initted = FALSE;
va_list argscp;
va_copy(argscp, args);
if (!initted)
    {
    htmStart(stdout, "Early Error");
    initted = TRUE;
    }
printf("%s", htmlWarnStartPattern());
htmlVaParagraph(format,args);
printf("%s", htmlWarnEndPattern());

/* write warning/error message to stderr so they get logged. */
logCgiToStderr();
vfprintf(stderr, format, argscp);
va_end(argscp);
putc('\n', stderr);
fflush(stderr);
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
jsIncludeFile("jquery.js", NULL);
jsIncludeFile("utils.js", NULL);
jsIncludeFile("ajax.js", NULL);
cgiMakeHiddenVar("db", db);
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
if(pos != NULL && oldVars != NULL)
    {
    struct hashEl *oldpos = hashLookup(oldVars, positionCgiName);
    if(oldpos != NULL && differentString(pos,oldpos->val))
        cartSetString(cart,"lastPosition",oldpos->val);
    }
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

char *cartLookUpVariableClosestToHome(struct cart *cart, struct trackDb *tdb,
	boolean compositeLevel, char *suffix,char **pVariable)
/* Returns value or NULL for a cart variable from lowest level on up. Optionally
 * fills the non NULL pVariable with the actual name of the variable in the cart */
{
if (compositeLevel)
    tdb = tdb->parent;
for ( ; tdb != NULL; tdb = tdb->parent)
    {
    char buf[512];
    if (suffix[0] == '.' || suffix[0] == '_')
        safef(buf, sizeof buf, "%s%s", tdb->track,suffix);
    else
        safef(buf, sizeof buf, "%s.%s", tdb->track,suffix);
    char *cartSetting = hashFindVal(cart->hash, buf);
    if (cartSetting != NULL)
	{
	if(pVariable != NULL)
	    *pVariable = cloneString(buf);
	return cartSetting;
	}
    }
if (pVariable != NULL)
    *pVariable = NULL;
return NULL;
}

void cartRemoveVariableClosestToHome(struct cart *cart, struct trackDb *tdb, boolean compositeLevel, char *suffix)
/* Looks for then removes a cart variable from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */
{
char *var = NULL;
(void)cartLookUpVariableClosestToHome(cart,tdb,compositeLevel,suffix,&var);
if(var != NULL)
    {
    cartRemove(cart,var);
    freeMem(var);
    }
}

char *cartStringClosestToHome(struct cart *cart, struct trackDb *tdb, boolean compositeLevel, char *suffix)
/* Returns value or Aborts for a cart string from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */
{
char *setting = cartOptionalStringClosestToHome(cart,tdb,compositeLevel,suffix);
if(setting == NULL)
    errAbort("cartStringClosestToHome: '%s' not found", suffix);
return setting;
}

boolean cartVarExistsAnyLevel(struct cart *cart, struct trackDb *tdb, boolean compositeLevel, char *suffix)
/* Returns TRUE if variable exists anywhere, looking from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */
{
return (NULL != cartOptionalStringClosestToHome(cart,tdb,compositeLevel,suffix));
}


char *cartUsualStringClosestToHome(struct cart *cart, struct trackDb *tdb, boolean compositeLevel, char *suffix, char *usual)
/* Returns value or {usual} for a cart string from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */
{
char *setting = cartOptionalStringClosestToHome(cart,tdb,compositeLevel,suffix);
if(setting == NULL)
    setting = usual;
return setting;
}

boolean cartBooleanClosestToHome(struct cart *cart, struct trackDb *tdb, boolean compositeLevel, char *suffix)
/* Returns value or Aborts for a cart boolean ('on' or != 0) from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */
{
char *setting = cartStringClosestToHome(cart,tdb,compositeLevel,suffix);
return (sameString(setting, "on") || atoi(setting) > 0);
}

boolean cartUsualBooleanClosestToHome(struct cart *cart, struct trackDb *tdb, boolean compositeLevel, char *suffix,boolean usual)
/* Returns value or {usual} for a cart boolean ('on' or != 0) from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */
{
char *setting = cartOptionalStringClosestToHome(cart,tdb,compositeLevel,suffix);
if(setting == NULL)
    return usual;
return (sameString(setting, "on") || atoi(setting) > 0);
}


int cartUsualIntClosestToHome(struct cart *cart, struct trackDb *tdb, boolean compositeLevel, char *suffix, int usual)
/* Returns value or {usual} for a cart int from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */
{
char *setting = cartOptionalStringClosestToHome(cart,tdb,compositeLevel,suffix);
if (setting == NULL)
    return usual;
return atoi(setting);
}

double cartUsualDoubleClosestToHome(struct cart *cart, struct trackDb *tdb, boolean compositeLevel, char *suffix, double usual)
/* Returns value or {usual} for a cart fp double from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */
{
char *setting = cartOptionalStringClosestToHome(cart,tdb,compositeLevel,suffix);
if (setting == NULL)
    return usual;
return atof(setting);
}

struct slName *cartOptionalSlNameListClosestToHome(struct cart *cart, struct trackDb *tdb, boolean compositeLevel, char *suffix)
/* Return slName list (possibly with multiple values for the same var) from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */
{
char *var = NULL;
cartLookUpVariableClosestToHome(cart,tdb,compositeLevel,suffix,&var);
if(var == NULL)
    return NULL;

struct slName *slNames = cartOptionalSlNameList(cart,var);
freeMem(var);
return slNames;
}

void cartRemoveAllForTdb(struct cart *cart, struct trackDb *tdb)
/* Remove all variables from cart that are associated with this tdb. */
{
char setting[256];
safef(setting,sizeof(setting),"%s.",tdb->track);
cartRemovePrefix(cart,setting);
safef(setting,sizeof(setting),"%s_",tdb->track); // TODO: All should be {track}.{varName}... Fix {track}_sel
cartRemovePrefix(cart,setting);
cartRemove(cart,tdb->track);
}

void cartRemoveAllForTdbAndChildren(struct cart *cart, struct trackDb *tdb)
/* Remove all variables from cart that are associated
   with this tdb and it's children. */
{
struct trackDb *subTdb;
for(subTdb=tdb->subtracks;subTdb!=NULL;subTdb=subTdb->next)
    cartRemoveAllForTdbAndChildren(cart,subTdb);
cartRemoveAllForTdb(cart,tdb);
        saveState(cart);
}

char *cartOrTdbString(struct cart *cart, struct trackDb *tdb, char *var, char *defaultVal)
/* Look first in cart, then in trackDb for var.  Return defaultVal if not found. */
{
char *tdbDefault = trackDbSettingClosestToHomeOrDefault(tdb, var, defaultVal);
boolean compositeLevel = isNameAtCompositeLevel(tdb, var);
return cartUsualStringClosestToHome(cart, tdb, compositeLevel, var, tdbDefault);
}

int cartOrTdbInt(struct cart *cart, struct trackDb *tdb, char *var, int defaultVal)
/* Look first in cart, then in trackDb for var.  Return defaultVal if not found. */
{
char *a = cartOrTdbString(cart, tdb, var, NULL);
if (a == NULL)
    return defaultVal;
else
    return atoi(a);
}


double cartOrTdbDouble(struct cart *cart, struct trackDb *tdb, char *var, double defaultVal)
/* Look first in cart, then in trackDb for var.  Return defaultVal if not found. */
{
char *a = cartOrTdbString(cart, tdb, var, NULL);
if (a == NULL)
    return defaultVal;
else
    return atof(a);
}

// These macros allow toggling warn messages to NOOPS when no longer debugging
//#define DEBUG_WITH_WARN
#ifdef DEBUG_WITH_WARN
                                #define WARN warn
                                #define ASSERT assert
#else///ifndef DEBUG_WITH_WARN
                                #define WARN(...)
                                #define ASSERT(...)
#endif///ndef DEBUG_WITH_WARN


boolean cartValueHasChanged(struct cart *newCart,struct hash *oldVars,char *setting,boolean ignoreRemoved,boolean ignoreCreated)
/* Returns TRUE if new cart setting has changed from old cart setting */
{
char *oldValue = hashFindVal(oldVars,setting);
if (oldValue == NULL)
#ifdef CART_DIFFS_INCLUDE_EMPTIES
    return FALSE;  // All vars changed by cgi will be found in old vars
#else///ifndef CART_DIFFS_INCLUDE_EMPTIES
    return (!ignoreCreated);
#endif///ndef CART_DIFFS_INCLUDE_EMPTIES

char *newValue = cartOptionalString(newCart,setting);
if (newValue == NULL)
    return (!ignoreRemoved);

#ifdef CART_DIFFS_INCLUDE_EMPTIES
if (sameString(oldValue,CART_VAR_EMPTY))
    {
    if (sameString(newValue,"hide")
    ||  sameString(newValue,"off")
    ||  sameString(newValue,"0"))   // Special cases DANGER!
        return FALSE;
    }
#endif///def CART_DIFFS_INCLUDE_EMPTIES

return (differentString(newValue,oldValue));
}

static int cartNamesPruneChanged(struct cart *newCart,struct hash *oldVars,
                          struct slPair **cartNames,boolean ignoreRemoved,boolean unChanged)
/* Prunes a list of cartNames if the settings have changed between new and old cart.
   Returns pruned count */
{
int pruned = 0;
struct slPair *oldList = *cartNames;
struct slPair *newList = NULL;
struct slPair *oneName = NULL;
while ((oneName = slPopHead(&oldList)) != NULL)
    {
    boolean thisOneChanged = cartValueHasChanged(newCart,oldVars,oneName->name,ignoreRemoved,TRUE);
    if (unChanged != thisOneChanged)
        slAddHead(&newList,oneName);
    else
        {
        pruned++;
        }
    }
*cartNames = newList;
return pruned;
}


int cartRemoveFromTdbTree(struct cart *cart,struct trackDb *tdb,char *suffix,boolean skipParent)
/* Removes a 'trackName.suffix' from all tdb descendents (but not parent).
   If suffix NULL then removes 'trackName' which holds visibility */
{
int removed = 0;
boolean vis = (suffix == NULL || *suffix == '\0');
struct slRef *tdbRef, *tdbRefList = trackDbListGetRefsToDescendants(skipParent?tdb->subtracks:tdb);
for (tdbRef = tdbRefList; tdbRef != NULL; tdbRef = tdbRef->next)
    {
    struct trackDb *descendentTdb = tdbRef->val;
    char settingName[512];  // wgEncodeOpenChromChip.Peaks.vis
    if (vis)
        safef(settingName,sizeof(settingName),"%s",descendentTdb->track);
    else
        safef(settingName,sizeof(settingName),"%s.%s",descendentTdb->track,suffix);
    removed += cartRemoveAndCount(cart,settingName);
    }
return removed;
}

static int cartRemoveOldFromTdbTree(struct cart *newCart,struct hash *oldVars,struct trackDb *tdb,char *suffix,char *parentVal,boolean skipParent)
/* Removes a 'trackName.suffix' from all tdb descendents (but not parent), BUT ONLY IF OLD or same as parentVal.
   If suffix NULL then removes 'trackName' which holds visibility */
{
int removed = 0;
boolean vis = (suffix == NULL || *suffix == '\0');
struct slRef *tdbRef, *tdbRefList = trackDbListGetRefsToDescendants(skipParent?tdb->subtracks:tdb);
for (tdbRef = tdbRefList; tdbRef != NULL; tdbRef = tdbRef->next)
    {
    struct trackDb *descendentTdb = tdbRef->val;
    char settingName[512];  // wgEncodeOpenChromChip.Peaks.vis
    if (vis)
        safef(settingName,sizeof(settingName),"%s",descendentTdb->track);
    else
        safef(settingName,sizeof(settingName),"%s.%s",descendentTdb->track,suffix);
    char *newVal = cartOptionalString(newCart,settingName);
    if (    newVal    != NULL
    && (   (parentVal != NULL && sameString(newVal,parentVal))
        || (FALSE == cartValueHasChanged(newCart,oldVars,settingName,TRUE,FALSE))))
        removed += cartRemoveAndCount(newCart,settingName);
    }
return removed;
}

static boolean cartTdbOverrideSuperTracks(struct cart *cart,struct trackDb *tdb,boolean ifJustSelected)
/* When when the child of a hidden supertrack is foudn and selected, then shape the supertrack accordingly
   Returns TRUE if any cart changes are made */
{
// This is only pertinent to supertrack children just turned on
if (!tdbIsSuperTrackChild(tdb))
    return FALSE;

char setting[512];

// Must be from having just selected the track in findTracks.  This will carry with it the "_sel" setting.
if (ifJustSelected)
    {
    safef(setting,sizeof(setting),"%s_sel",tdb->track);
    if (!cartVarExists(cart,setting))
        return FALSE;
    cartRemove(cart,setting); // Unlike composite subtracks, supertrack children keep the "_sel" setting only for detecting this moment
    }

// if parent is not hidden then nothing to do
ASSERT(tdb->parent != NULL && tdbIsSuperTrack(tdb->parent));
enum trackVisibility vis = tdbVisLimitedByAncestry(cart, tdb->parent, FALSE);
if (vis != tvHide)
    return FALSE;

// Now turn all other supertrack children to hide and the supertrack to visible
struct slRef *childRef;
for(childRef = tdb->parent->children;childRef != NULL; childRef = childRef->next)
    {
    struct trackDb *child = childRef->val;
    if (child == tdb)
        continue;

    // Make sure this child hasn't also just been turned on!
    safef(setting,sizeof(setting),"%s_sel",child->track);
    if (cartVarExists(cart,setting))
        {
        cartRemove(cart,setting); // Unlike composite subtracks, supertrack children keep the "_sel" setting only for detecting this moment
        continue;
        }

    // hide this sibling if not already hidden
    char *cartVis = cartOptionalString(cart,child->track);
    if (cartVis != NULL)
        {
        if (child->visibility == tvHide)
            cartRemove(cart,child->track);
        else if (hTvFromString(cartVis) != tvHide)
            cartSetString(cart,child->track,"hide");
            }
    else if (child->visibility != tvHide)
        cartSetString(cart,child->track,"hide");
        }
// and finally show the parent
cartSetString(cart,tdb->parent->track,"show");
WARN("Set %s to 'show'",tdb->parent->track);
return TRUE;
}


static int cartTdbParentShapeVis(struct cart *cart,struct trackDb *parent,char *view,struct hash *subVisHash,boolean reshapeFully)
// This shapes one level of vis (view or container) based upon subtrack specific visibility.  Returns count of tracks affected
{
ASSERT(view || (tdbIsContainer(parent) && tdbIsContainerChild(parent->subtracks)));
struct trackDb *subtrack = NULL;
char setting[512];
if (view != NULL)
    safef(setting,sizeof(setting),"%s.%s.vis",parent->parent->track,view);
else
    safef(setting,sizeof(setting),"%s",parent->track);

enum trackVisibility visMax  = tvHide;
enum trackVisibility visOrig = tdbVisLimitedByAncestry(cart, parent, FALSE);

// Should walk through children to get max new vis for this parent
for(subtrack = parent->subtracks;subtrack != NULL;subtrack = subtrack->next)
    {
    char *foundVis = hashFindVal(subVisHash, subtrack->track); // if the subtrack doesn't have individual vis AND...
    if (foundVis != NULL)
        {
        enum trackVisibility visSub = hTvFromString(foundVis);
        if (tvCompare(visMax, visSub) >= 0)
            visMax = visSub;
        }
    else if (!reshapeFully && visOrig != tvHide)
        {
        int fourState = subtrackFourStateChecked(subtrack,cart);
        if (fourStateVisible(fourState)) // subtrack must be visible
            {
            enum trackVisibility visSub = tdbVisLimitedByAncestry(cart, subtrack, FALSE);
            if (tvCompare(visMax, visSub) >= 0)
                visMax = visSub;
            }
        }
    }

// Now we need to update non-subtrack specific vis/sel in cart
int countUnchecked=0;
int countVisChanged=0;
// If view, this should always be set, since if a single view needs to be promoted, the composite will go to full.
if (tdbIsCompositeView(parent))
    {
    cartSetString(cart,setting,hStringFromTv(visMax));    // Set this explicitly.  The visOrig may be inherited!
    countVisChanged++;
    }

if (visMax != visOrig || reshapeFully)
    {
    if (!tdbIsCompositeView(parent)) // view vis is always shaped, but composite vis is conditionally shaped.
        {
        cartSetString(cart,setting,hStringFromTv(visMax));    // Set this explicitly.  The visOrig may be inherited!
        countVisChanged++;
        if (visOrig == tvHide && tdbIsSuperTrackChild(parent))
            cartTdbOverrideSuperTracks(cart,parent,FALSE);      // deal with superTrack vis! cleanup
        }

    // Now set all subtracks that inherit vis back to visOrig
    for(subtrack = parent->subtracks;subtrack != NULL;subtrack = subtrack->next)
        {
        int fourState = subtrackFourStateChecked(subtrack,cart);
        if (!fourStateChecked(fourState))
            cartRemove(cart,subtrack->track);  // Remove subtrack level vis if it isn't even checked just in case
        else  // subtrack is checked (should include subtrack level vis)
            {
            char *visFromHash = hashFindVal(subVisHash, subtrack->track);
            if (tdbIsMultiTrack(parent))
                cartRemove(cart,subtrack->track);  // MultiTrack vis is ALWAYS inherited vis and non-selected should not have vis
            if (visFromHash == NULL)   // if the subtrack doesn't have individual vis AND...
                {
                if (reshapeFully || visOrig == tvHide)
                    {
                    subtrackFourStateCheckedSet(subtrack, cart,FALSE,fourStateEnabled(fourState)); // uncheck
                    cartRemove(cart,subtrack->track);  // Remove it if it exists, just in case
                    countUnchecked++;
                    }
                else if (visOrig != tvHide)
                    {
                    cartSetString(cart,subtrack->track,hStringFromTv(visOrig));
                    countVisChanged++;
                    }
                }
            else // This subtrack has explicit vis
                {
                enum trackVisibility vis = hTvFromString(visFromHash);
                if (vis == visMax)
                    {
                    cartRemove(cart,subtrack->track);  // Remove vis which should now be inherited
                    countVisChanged++;
                    }
                }
            }
        }
    }
else if (tdbIsMultiTrack(parent))
    {
    // MultiTrack vis is ALWAYS inherited vis so remove any subtrack specific vis
    struct hashCookie brownie = hashFirst(subVisHash);
    struct hashEl* cartVar = NULL;
    while ((cartVar = hashNext(&brownie)) != NULL)
        {
        if (!endsWith(cartVar->name,"_sel"))
            cartRemove(cart,cartVar->name);
        }
    }
if (countUnchecked + countVisChanged)
    WARN("%s visOrig:%s visMax:%s unchecked:%d  Vis changed:%d",parent->track,hStringFromTv(visOrig),hStringFromTv(visMax),countUnchecked,countVisChanged);

return (countUnchecked + countVisChanged);
}

boolean cartTdbTreeReshapeIfNeeded(struct cart *cart,struct trackDb *tdbContainer)
/* When subtrack vis is set via findTracks, and composite has no cart settings,
   then "shape" composite to match found */
{
if (!tdbIsContainer(tdbContainer))
    return FALSE;  // Don't do any shaping

// First look for subtrack level vis
char setting[512];
struct trackDb *subtrack = NULL;
struct trackDb *tdbView  = NULL;
struct hash *subVisHash = newHash(0);
struct slRef *tdbRef, *tdbRefList = trackDbListGetRefsToDescendantLeaves(tdbContainer->subtracks);
for (tdbRef = tdbRefList; tdbRef != NULL; tdbRef = tdbRef->next)
    {
    subtrack = tdbRef->val;
    char *val=cartOptionalString(cart,subtrack->track);
    if (val && differentString(val,"hide"))  // NOTE should we include hide?
        {
        int fourState = subtrackFourStateChecked(subtrack,cart);
        //if (cartUsualInt(cart,setting,0) != 0) // FIXME: sub level vis is not getting cleared out!!!!
        if (fourStateVisible(fourState))            // subtrack is checked too
            {
            WARN("Subtrack level vis %s=%s",subtrack->track,val);
            hashAdd(subVisHash,subtrack->track,val);
            safef(setting,sizeof(setting),"%s_sel",subtrack->track);
            hashAdd(subVisHash,setting,subtrack);     // Add the "_sel" setting which should also exist.  Point it to subtrack
            }
        }
    }
slFreeList(&tdbRefList);

if (hashNumEntries(subVisHash) == 0)
    {
    //WARN("No subtrack level vis for %s",tdbContainer->track);
    return FALSE;
    }

// Next look for any cart settings other than subtrack vis/sel
// New directive means that if composite is hidden, then ignore previous and don't bother checking cart.
boolean reshapeFully = (tdbVisLimitedByAncestry(cart, tdbContainer, FALSE) == tvHide);
boolean hasViews = tdbIsCompositeView(tdbContainer->subtracks);

WARN("reshape: %s",reshapeFully?"Fully":"Incrementally");

// Now shape views and composite to match subtrack specific visibility
int count = 0;
if (hasViews)
    {
    for(tdbView = tdbContainer->subtracks;tdbView != NULL; tdbView = tdbView->next )
        {
        char *view = NULL;
        if (tdbIsView(tdbView,&view) )
            count += cartTdbParentShapeVis(cart,tdbView,view,subVisHash,reshapeFully);
        }
    if (count > 0)
        {
        // At least on view was shaped, so all views will get explicit vis.  This means composite must be set to full
        enum trackVisibility visOrig = tdbVisLimitedByAncestry(cart, tdbContainer, FALSE);
        cartSetString(cart,tdbContainer->track,"full");    // Now set composite to full.
        if (visOrig == tvHide && tdbIsSuperTrackChild(tdbContainer))
            cartTdbOverrideSuperTracks(cart,tdbContainer,FALSE);      // deal with superTrack vis! cleanup
        }
    }
else // If no views then composite is not set to fuul but to max of subtracks
    count = cartTdbParentShapeVis(cart,tdbContainer,NULL,subVisHash,reshapeFully);

hashFree(&subVisHash);

// If reshaped, be sure to set flag to stop composite cleanup
if (count > 0)
    tdbExtrasReshapedCompositeSet(tdbContainer);

return TRUE;
}

boolean cartTdbTreeCleanupOverrides(struct trackDb *tdb,struct cart *newCart,struct hash *oldVars, struct lm *lm)
/* When container or composite/view settings changes, remove subtrack specific settings
   Returns TRUE if any cart vars are removed */
{
boolean anythingChanged = cartTdbOverrideSuperTracks(newCart,tdb,TRUE);
if (!tdbIsContainer(tdb))
    return anythingChanged;

// If composite has been reshaped then don't clean it up
if (tdbExtrasReshapedComposite(tdb))
    return anythingChanged;

// vis is a special additive case! composite or view level changes then remove subtrack vis
boolean containerVisChanged = cartValueHasChanged(newCart,oldVars,tdb->track,TRUE,FALSE);
if (containerVisChanged)
    {
    // If just created and if vis is the same as tdb default then vis has not changed
    char *cartVis = cartOptionalString(newCart,tdb->track);
    char *oldValue = hashFindVal(oldVars,tdb->track);
    if (cartVis && oldValue == NULL && hTvFromString(cartVis) == tdb->visibility)
        containerVisChanged = FALSE;
    }
struct trackDb *tdbView = NULL;
struct slPair *oneName = NULL;
char *suffix = NULL;
int clensed = 0;

// Build list of current settings for container or composite and views
char setting[512];
safef(setting,sizeof(setting),"%s.",tdb->track);
char * view = NULL;
boolean hasViews = FALSE;
struct slPair *changedSettings = cartVarsWithPrefixLm(newCart, setting, lm);
for (tdbView = tdb->subtracks;tdbView != NULL; tdbView = tdbView->next)
    {
    if (!tdbIsView(tdbView,&view))
        break;
    hasViews = TRUE;
    safef(setting,sizeof(setting),"%s.",tdbView->track);          // unfortunatly setting name could be viewTrackName.???
    //safef(setting,   sizeof(setting),"%s.%s.",tdb->track,view); // or containerName.Sig.???   HOWEVER: this are picked up by containerName prefix
    struct slPair *changeViewSettings = cartVarsWithPrefixLm(newCart, setting, lm);
    changedSettings = slCat(changedSettings, changeViewSettings);
    }
if (changedSettings == NULL && !containerVisChanged)
    return anythingChanged;

// Prune list to only those which have changed
if(changedSettings != NULL)
    {
    (void)cartNamesPruneChanged(newCart,oldVars,&changedSettings,TRUE,FALSE);
    if (changedSettings == NULL && !containerVisChanged)
        return anythingChanged;
    }

// Walk through views
if (hasViews)
    {
    for (tdbView = tdb->subtracks;tdbView != NULL; tdbView = tdbView->next)
        {
        boolean viewVisChanged = FALSE;
        if (!tdbIsView(tdbView,&view))
            break;

        safef(setting,   sizeof(setting),"%s.%s.",tdb->track,view); // unfortunatly setting name could be containerName.View.???
        char settingAlt[512];
        safef(settingAlt,sizeof(settingAlt),"%s.",tdbView->track);  // or viewTrackName.???
        struct slPair *leftOvers = NULL;
        // Walk through settings that match this view
        while ((oneName = slPopHead(&changedSettings)) != NULL)
            {
            if(startsWith(setting,oneName->name))
                suffix = oneName->name + strlen(setting);
            else if(startsWith(settingAlt,oneName->name))
                suffix = oneName->name + strlen(settingAlt);
            else
                {
                slAddHead(&leftOvers,oneName);
                continue;
                }

            if (sameString(suffix,"vis"))
                {
                viewVisChanged = TRUE;
                }
            else if (cartRemoveOldFromTdbTree(newCart,oldVars,tdbView,suffix,oneName->val,TRUE) > 0)
                    clensed++;

            }
        if (viewVisChanged)
            {
            // If just created and if vis is the same as tdb default then vis has not changed
            safef(setting,sizeof(setting),"%s.%s.vis",tdb->track,view);
            char *cartVis = cartOptionalString(newCart,setting);
            char *oldValue = hashFindVal(oldVars,setting);
            if (cartVis && oldValue == NULL && hTvFromString(cartVis) != tdbView->visibility)
                viewVisChanged = FALSE;
            }

        if  (containerVisChanged || viewVisChanged)
            { // vis is a special additive case!
            WARN("Removing subtrack vis for %s.%s",tdb->track,view);
            char *viewVis = hStringFromTv(tdbVisLimitedByAncestry(newCart, tdbView, FALSE));
            if (cartRemoveOldFromTdbTree(newCart,oldVars,tdbView,NULL,viewVis,TRUE) > 0)
                clensed++;
            }
        changedSettings = leftOvers;
        }
    }

// Now deal with anything remaining at the container level
while ((oneName = slPopHead(&changedSettings)) != NULL)
    {
    suffix = oneName->name + strlen(tdb->track) + 1;
    if (cartRemoveOldFromTdbTree(newCart,oldVars,tdb,suffix,oneName->val,TRUE) > 0)
        clensed++;
    }
if  (containerVisChanged && !hasViews)
    { // vis is a special additive case!
    char *vis = hStringFromTv(tdbVisLimitedByAncestry(newCart, tdb, FALSE));
    if (cartRemoveOldFromTdbTree(newCart,oldVars,tdb,NULL,vis,TRUE) > 0)
        clensed++;
    }

anythingChanged = (anythingChanged || (clensed > 0));
return anythingChanged;
}



