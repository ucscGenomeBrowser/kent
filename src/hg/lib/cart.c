/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hCommon.h"
#include "obscure.h"
#include "linefile.h"
#include "errAbort.h"
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
#include "geoMirror.h"
#include "hubConnect.h"
#include "trackHub.h"
#include "cgiApoptosis.h"
#include "customComposite.h"
#include "regexHelper.h"
#include "windowsToAscii.h"

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

static struct dyString *hubWarnDy;

void cartHubWarn(char *format, va_list args)
/* save up hub related warnings to put out later */
{
char warning[1024];
vsnprintf(warning,sizeof(warning),format, args);
if (hubWarnDy == NULL)
    hubWarnDy = newDyString(100);
dyStringPrintf(hubWarnDy, "%s\n", warning);
}

void cartFlushHubWarnings()
/* flush the hub warning (if any) */
{
if (hubWarnDy)
    warn("%s",hubWarnDy->string);
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
    struct dyString *query = dyStringNew(1024);
    sqlDyStringPrintf(query, "select length(contents) from %s"
	  " where id = %d", userDbTable(), u->id);
    if (cartDbUseSessionKey())
	  sqlDyStringPrintf(query, " and sessionKey='%s'", u->sessionKey);
    uLen = sqlQuickNum(conn, query->string);
    dyStringClear(query);
    sqlDyStringPrintf(query, "select length(contents) from %s"
	  " where id = %d", sessionDbTable(),s->id);
    if (cartDbUseSessionKey())
	  sqlDyStringPrintf(query, " and sessionKey='%s'", s->sessionKey);
    sLen = sqlQuickNum(conn, query->string);
    dyStringFree(&query);
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
fprintf(stderr, "cartTrace: %22s: "
	"u.i=%d u.l=%d u.c=%d s.i=%d s.l=%d s.c=%d "
	"p=%s f=%s t=%s pid=%ld %s\n",
	when,
	u->id, uLen, u->useCount, s->id, sLen, s->useCount,
	pix, textSize, trackControls, (long)getpid(), cgiRemoteAddr());
char userIdKey[256];
cartDbSecureId(userIdKey, sizeof userIdKey, u);
if (cart->userId && !sameString(userIdKey, cart->userId))
    fprintf(stderr, "cartTrace: bad userId %s --> %d_%s!  pid=%ld\n",
	    cart->userId, u->id, u->sessionKey, (long)getpid());
char sessionIdKey[256];
cartDbSecureId(sessionIdKey, sizeof sessionIdKey, s);
if (cart->sessionId && !sameString(sessionIdKey, cart->sessionId))
    fprintf(stderr, "cartTrace: bad sessionId %s --> %d_%s!  pid=%ld\n",
	    cart->sessionId, s->id, s->sessionKey, (long)getpid());
}

boolean cartTablesOk(struct sqlConnection *conn)
/* Return TRUE if cart tables are accessible (otherwise, the connection
 * doesn't do us any good). */
{
if (!sqlTableExists(conn, userDbTable()))
    {
    fprintf(stderr, "cartTablesOk failed on %s.%s  pid=%ld\n",
	    sqlGetDatabase(conn), userDbTable(),  (long)getpid());
    return FALSE;
    }
if (!sqlTableExists(conn, sessionDbTable()))
    {
    fprintf(stderr, "cartTablesOk failed on %s.%s  pid=%ld\n",
	    sqlGetDatabase(conn), sessionDbTable(), (long)getpid());
    return FALSE;
    }
return TRUE;
}

void cartParseOverHash(struct cart *cart, char *contents)
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

struct cartDb *cartDbLoadFromId(struct sqlConnection *conn, char *table, char *secureId)
/* Load up cartDb entry for particular ID.  Returns NULL if no such id. */
{

if (!secureId)
    return NULL;
else
    {
    struct cartDb *cdb = NULL;
    struct dyString *where = dyStringNew(256);
    char *sessionKey = NULL;	    
    unsigned int id = cartDbParseId(secureId, &sessionKey);
    sqlDyStringPrintfFrag(where, "id = %u", id);
    if (cartDbUseSessionKey())
	{
	if (!sessionKey)
	    sessionKey = "";
	sqlDyStringPrintfFrag(where, " and sessionKey='%s'", sessionKey);
	}
    cdb = cartDbLoadWhere(conn, table, where->string);
    dyStringFree(&where);
    if (looksCorrupted(cdb))
       {
       /* Can't use warn here -- it interrupts the HTML header, causing an
	* err500 (and nothing useful in error_log) instead of a warning. */
       fprintf(stderr,
	       "%s id=%u looks corrupted -- starting over with new %s id.\n",
	       table, id, table);
       cdb = NULL;
       }
    return cdb;
    }
}

struct cartDb *loadDb(struct sqlConnection *conn, char *table, char *secureId, boolean *found)
/* Load bits from database and save in hash. */
{
struct cartDb *cdb;
boolean result = TRUE;

cdb = cartDbLoadFromId(conn, table, secureId);
if (!cdb)
    {
    result = FALSE;
    struct dyString *query = dyStringNew(256);
    sqlDyStringPrintf(query, "INSERT %s VALUES(0,'',0,now(),now(),0", table);
    char *sessionKey = "";
    if (cartDbHasSessionKey(conn, table)) 
	{
	if (cartDbUseSessionKey())
	    {
	    sessionKey = makeRandomKey(128+33); // at least 128 bits of protection, 33 for the world population size.
	    }
	sqlDyStringPrintf(query, ",'%s'", sessionKey);
	}
    sqlDyStringPrintf(query, ")");
    sqlUpdate(conn, query->string);
    dyStringFree(&query);
    unsigned int id = sqlLastAutoId(conn);
    char newSecureId[256];
    if (cartDbUseSessionKey() && !sameString(sessionKey,""))
	safef(newSecureId, sizeof newSecureId, "%u_%s", id, sessionKey);
    else
	safef(newSecureId, sizeof newSecureId, "%u", id);
    if ((cdb = cartDbLoadFromId(conn,table,newSecureId)) == NULL)
        errAbort("Couldn't get cartDb for id=%u right after loading.  "
		 "MySQL problem??", id);
    if (!sameString(sessionKey,""))
	freeMem(sessionKey);
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
sqlDyStringPrintf(dy, "SELECT useCount FROM %s "
	       "WHERE userName = '%s' AND sessionName = '%s';",
	       namedSessionTable, encUserName, encSessionName);
useCount = sqlQuickNum(conn, dy->string) + 1;
dyStringClear(dy);
sqlDyStringPrintf(dy, "UPDATE %s SET useCount = %d, lastUse=now() "
	       "WHERE userName = '%s' AND sessionName = '%s';",
	       namedSessionTable, useCount, encUserName, encSessionName);
sqlUpdate(conn, dy->string);
dyStringFree(&dy);
}

static void copyCustomComposites(struct cart *cart, struct hashEl *el)
/* Copy a set of custom composites to a new hub file. Update the 
 * relevant cart variables. */
{
struct tempName hubTn;
char *hubFileVar = el->name;
char *oldHubFileName = el->val;
trashDirDateFile(&hubTn, "hgComposite", "hub", ".txt");
char *newHubFileName = cloneString(hubTn.forCgi);

// let's make sure the hub hasn't been cleaned up
int fd = open(oldHubFileName, O_RDONLY);
if (fd < 0)
    {
    cartRemove(cart, hubFileVar);
    return;
    }

close(fd);
copyFile(oldHubFileName, newHubFileName);
cartReplaceHubVars(cart, hubFileVar, oldHubFileName, newHubFileName);
}

void cartReplaceHubVars(struct cart *cart, char *hubFileVar, char *oldHubUrl, char *newHubUrl)
/* Replace all cart variables corresponding to oldHubUrl (and/or its hub ID) with
 * equivalents for newHubUrl. */
{
if (! startsWith(customCompositeCartName, hubFileVar))
    errAbort("cartReplaceHubVars: expected hubFileVar to begin with '"customCompositeCartName"' "
             "but got '%s'", hubFileVar);
char *db = hubFileVar + strlen(customCompositeCartName "-");
char *errorMessage;
unsigned oldHubId =  hubFindOrAddUrlInStatusTable(db, cart, oldHubUrl, &errorMessage);
unsigned newHubId =  hubFindOrAddUrlInStatusTable(db, cart, newHubUrl, &errorMessage);

// need to change hgHubConnect.hub.#hubNumber# (connected hubs)
struct slPair *hv, *hubVarList = cartVarsWithPrefix(cart, hgHubConnectHubVarPrefix);
char buffer[4096];
for(hv = hubVarList; hv; hv = hv->next)
    {
    unsigned hubId = sqlUnsigned(hv->name + strlen(hgHubConnectHubVarPrefix));
    
    if (hubId == oldHubId)
        {
        cartRemove(cart, hv->name);
        safef(buffer, sizeof buffer, "%s%d", hgHubConnectHubVarPrefix, newHubId);
        cartSetString(cart, buffer, "1");
        }
    }

// need to change hub_#hubNumber#* (track visibilities)
safef(buffer, sizeof buffer, "%s%d_", hubTrackPrefix, oldHubId);
int oldNameLength = strlen(buffer);
hubVarList = cartVarsWithPrefix(cart, buffer);
for(hv = hubVarList; hv; hv = hv->next)
    {
    char *name = hv->name + oldNameLength;
    safef(buffer, sizeof buffer, "%s%d_%s", hubTrackPrefix, newHubId, name);
    cartSetString(cart, buffer, cloneString(hv->val));
    cartRemove(cart, hv->name);
    }

// need to change hgtgroup_hub_#hubNumber# (blue bar open )
// need to change expOrder_hub_#hubNumber#, simOrder_hub_#hubNumber# (sorting) -- values too

// need to change trackHubs #hubNumber#   
cartSetString(cart, hgHubConnectRemakeTrackHub, "on");
cartSetString(cart, hubFileVar, newHubUrl);
}

void cartCopyCustomComposites(struct cart *cart)
/* Find any custom composite hubs and copy them so they can be modified. */
{
struct hashEl *el, *elList = hashElListHash(cart->hash);

for (el = elList; el != NULL; el = el->next)
    {
    if (startsWith(customCompositeCartName, el->name))
        copyCustomComposites(cart, el);
    }
}

static void storeInOldVars(struct cart *cart, struct hash *oldVars, char *var)
/* Store all cart hash elements for var into oldVars (if it exists). */
{
if (oldVars == NULL)
    return;
struct hashEl *hel = hashLookup(cart->hash, var);

// NOTE: New cgi vars not in old cart cannot be distinguished from vars not newly set by cgi.
//       Solution: Add 'empty' var to old vars for cgi vars not already in cart
if (hel == NULL)
    hashAdd(oldVars, var, cloneString(CART_VAR_EMPTY));

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
if (oldVars == NULL)
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
            if (suffixOffset>0)
                {
                char *name = cloneString(el->name);
                safecpy(name+suffixOffset,strlen(POSITION_SUFFIX),IMGORD_SUFFIX);
                // We know that POSITION_SUFFIX is longer than IMGORD_SUFFIX
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

sqlSafef(query, sizeof(query), "SELECT shared, contents FROM %s "
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
	char *hgsid = cartSessionId(cart);
    char *sessionTableString = cartOptionalString(cart, hgSessionTableState);
    sessionTableString = cloneString(sessionTableString);
    char *pubSessionsTableString = cartOptionalString(cart, hgPublicSessionsTableState);
    pubSessionsTableString = cloneString(pubSessionsTableString);
	struct sqlConnection *conn2 = hConnectCentral();
	sessionTouchLastUse(conn2, encSessionOwner, encSessionName);
	cartRemoveLike(cart, "*");
	cartParseOverHash(cart, row[1]);
	cartSetString(cart, sessionVar, hgsid);
	if (sessionTableString != NULL)
	    cartSetString(cart, hgSessionTableState, sessionTableString);
	if (pubSessionsTableString != NULL)
	    cartSetString(cart, hgPublicSessionsTableState, pubSessionsTableString);
	if (oldVars)
	    hashEmpty(oldVars);
	/* Overload settings explicitly passed in via CGI (except for the
	 * command that sent us here): */
	loadCgiOverHash(cart, oldVars);
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

boolean containsNonPrintable(char *string)
/* Return TRUE if string contains non-ascii printable character(s). */
{
if (isEmpty(string))
    return FALSE;
boolean hasNonPrintable = FALSE;
int i;
for (i = 0;  string[i] != '\0';  i++)
    {
    if ((string[i] < 32 || string[i] > 126) && string[i] != '\t')
        {
        hasNonPrintable = TRUE;
        break;
        }
    }
return hasNonPrintable;
}

enum vsErrorType { vsNone=0, vsValid, vsBinary, vsWeird, vsData, vsVarLong, vsValLong };

struct validityStats
/* Watch out for incoming garbage data loaded as if it were a saved session file.
 * This helps decide whether to just bail or simply remove some garbage and alert the user. */
{
    char *weirdCharsExample;    // First "cart variable" with unexpected punct/space/etc
    char *dataExample;          // First "cart variable" that looks like custom track data
    char *valTooLongExample;    // First cart variable whose value is too long to include.
    uint validCount;            // Number of cart variables with no red flags, so *probably* ok.
    uint binaryCount;           // Number of "cart variables" with binary data
    uint weirdCharsCount;       // Number of "cart variables" with unexpected punct/space/etc
    uint dataCount;             // Number of "cart variables" that look like custom track data
    uint varTooLongCount;       // Number of "cart variables" whose length is too long.
    uint varTooLongLength;      // Longest too-long cart var found.
    uint valTooLongCount;       // Number of cart variables whose values are too long.
    uint valTooLongLength;      // Longest too-long cart var value found.
    enum vsErrorType lastType;  // The latest type of error, so we can roll back the previous call.
};

static void vsInit(struct validityStats *stats)
/* Set all counts to 0 and pointers to NULL. */
{
ZeroVar(stats);
}

static void vsFreeMembers(struct validityStats *stats)
/* Free all allocated members of stats (not stats itself, it may be a stack var). */
{
freeMem(stats->weirdCharsExample);
freeMem(stats->dataExample);
freeMem(stats->valTooLongExample);
}

static void vsGotValid(struct validityStats *stats)
/* Update validity stats after finding a cart var with no obvious red flags.  (No guarantee
 * it's actually a real cart variable, but at worst it's just bloating the cart a bit.) */
{
if (stats)
    {
    stats->validCount++;
    stats->lastType = vsValid;
    }
}

static void vsGotBinary(struct validityStats *stats)
/* Update validity stats after finding unprintable characters in "cart var". */
{
if (stats)
    {
    stats->binaryCount++;
    stats->lastType = vsBinary;
    }
}

static void vsGotWeirdChar(struct validityStats *stats, char *var)
/* Update validity stats after finding unexpected but printable characters in "cart var". */
{
if (stats)
    {
    stats->weirdCharsCount++;
    if (stats->weirdCharsExample == NULL)
        stats->weirdCharsExample = cloneString(var);
    stats->lastType = vsWeird;
    }
}

static void vsGotData(struct validityStats *stats, char *var)
/* Update validity stats after finding apparent custom track data in "cart var". */
{
if (stats)
    {
    stats->dataCount++;
    if (stats->dataExample == NULL)
        stats->dataExample = cloneString(var);
    stats->lastType = vsData;
    }
}

static void vsGotVarTooLong(struct validityStats *stats, size_t varLen)
/* Update validity stats after finding suspiciously lengthy "cart var". */
{
if (stats)
    {
    stats->varTooLongCount++;
    stats->varTooLongLength = max(stats->varTooLongLength, varLen);
    stats->lastType = vsVarLong;
    }
}

static void vsGotValTooLong(struct validityStats *stats, char *var, size_t valLen)
/* Update validity stats after finding cart var whose value is too long. */
{
if (stats)
    {
    stats->valTooLongCount++;
    stats->valTooLongLength = max (stats->valTooLongLength, valLen);
    if (stats->valTooLongExample == NULL)
        stats->valTooLongExample = cloneString(var);
    stats->lastType = vsValLong;
    }
}

static void vsUndo(struct validityStats *stats)
/* Roll back the latest increment to stats after finding an exceptional case.
   Only one level of undo is supported.  Not supported for vs{Binary,VarLong,ValLong}. */
{
if (stats)
    {
    switch (stats->lastType)
        {
        case vsNone:
            errAbort("vsUndo: nothing to undo (only one level of undo is possible)");
            break;
        case vsValid:
            stats->validCount--;
            stats->lastType = vsNone;
            break;
        case vsWeird:
            stats->weirdCharsCount--;
            if (stats->weirdCharsCount < 1)
                freez(&stats->weirdCharsExample);
            stats->lastType = vsNone;
            break;
        case vsData:
            stats->dataCount--;
            if (stats->dataCount < 1)
                freez(&stats->dataExample);
            stats->lastType = vsNone;
            break;
        case vsBinary:
        case vsVarLong:
        case vsValLong:
            errAbort("vsUndo: not supported for lastType vsBinary, vsVarLong or vsValLong (%d)",
                     stats->lastType);
            break;
        default:
            errAbort("vsUndo: invalid lastType %d", stats->lastType);
        }
    }
}

static uint vsErrorCount(struct validityStats *stats)
/* Return the sum of all error counts. */
{
return (stats->binaryCount +
        stats->weirdCharsCount +
        stats->dataCount +
        stats->varTooLongCount +
        stats->valTooLongCount);
}

#define CART_LOAD_TOO_MANY_ERRORS 100
#define CART_LOAD_ENOUGH_VALID 20
#define CART_LOAD_WAY_TOO_MANY_ERRORS 1000

static boolean vsTooManyErrors(struct validityStats *stats)
/* Return TRUE if the input seems to be completely invalid. */
{
if (stats)
    {
    uint errorSum = vsErrorCount(stats);
    uint total = errorSum + stats->validCount;
    return ((total > (CART_LOAD_TOO_MANY_ERRORS + CART_LOAD_ENOUGH_VALID) &&
             errorSum > CART_LOAD_TOO_MANY_ERRORS &&
             stats->validCount < CART_LOAD_ENOUGH_VALID) ||
            errorSum > CART_LOAD_WAY_TOO_MANY_ERRORS);
    }
return FALSE;
}

#define CART_VAR_MAX_LENGTH 1024
#define CART_VAL_MAX_LENGTH (64 * 1024)

static void vsReport(struct validityStats *stats, struct dyString *dyMessage)
/* Append summary/explanation to dyMessage.   */
{
if (stats && dyMessage)
    {
    boolean quitting = vsTooManyErrors(stats);
    char *atLeast = (quitting ? "At least " : "");
    dyStringPrintf(dyMessage, "<br>%d valid settings found.  ", stats->validCount);
    if (stats->binaryCount || stats->weirdCharsCount || stats->dataCount ||
        stats->varTooLongCount || stats->valTooLongCount)
        dyStringPrintf(dyMessage, "<b>Note: invalid settings were found and omitted.</b>  ");
    if (stats->binaryCount)
        dyStringPrintf(dyMessage, "%s%d setting names contained binary data.  ",
                       atLeast, stats->binaryCount);
    if (stats->weirdCharsCount)
        dyStringPrintf(dyMessage,
                       "%s%d setting names contained unexpected characters, for example '%s'.  ",
                       atLeast, stats->weirdCharsCount, htmlEncode(stats->weirdCharsExample));
    if (stats->dataCount)
        dyStringPrintf(dyMessage, "%s%d lines appeared to be custom track data, for example "
                       "a line begins with '%s'.  ",
                       atLeast, stats->dataCount, stats->dataExample);
    if (stats->varTooLongCount)
        dyStringPrintf(dyMessage, "%s%d setting names were too long (up to %d).  ",
                       atLeast, stats->varTooLongCount, stats->varTooLongLength);
    if (stats->valTooLongCount)
        dyStringPrintf(dyMessage, "%s%d setting values were too long (up to %d).  ",
                       atLeast, stats->valTooLongCount, stats->valTooLongLength);
    if (quitting)
        dyStringPrintf(dyMessage, "Encountered too many errors -- quitting.  ");
    }
}

// Our timestamp vars (_, hgt_) are an exception to the usual cart var naming patterns:
#define CART_VAR_TIMESTAMP "^([a-z]+)?_$"
// Legitimate cart vars look like this (but so do some not-vars, so we filter further below):
#define CART_VAR_VALID_CHARACTERS "^[A-Za-z]([A-Za-z0-9._:-]*[A-Za-z0-9]+)?$"

// These are "cart variables" that are actually custom track data:
static char *cartVarBlackList[] = { "X",
                                    "Y",
                                    "MT",
                                    "fixedStep",
                                    "variableStep",
                                   };

// Prefixes of "cart variables" from data files that have caused trouble in the past:
static char *cartVarBlackListPrefix[] = { "ENS",                // Giant Ensembl gene info dump
                                          "RRBS",               // Some other big tab-sep dump
                                          "VGXS",               // Genotypes
                                          NULL };

// More complicated patterns of custom track or genotype data:
static char *cartVarBlackListRegex[] = { "^chr[0-9XYMTUnLR]+(_[a-zA-Z0-9_]+)?$",
                                         "^(chr)?[A-Z]{2}[0-9]{5}[0-9]+",
                                         "^rs[0-9]+$",          // Genotypes
                                         "^i[0-9]{5}[0-9]*$)",  // Genotypes
                                         NULL };

static boolean isValidCartVar(char *var, struct validityStats *stats)
/* Return TRUE if var looks like a plausible cart variable name (as opposed to other stuff
 * that users try to load in as saved-to-file session data).  If var doesn't look right,
 * return FALSE and if stats is not NULL, record the problem. */
{
boolean isValid = TRUE;
size_t varLen = strlen(var);
if (containsNonPrintable(var))
    {
    vsGotBinary(stats);
    isValid = FALSE;
    }
else if (varLen > CART_VAR_MAX_LENGTH)
    {
    vsGotVarTooLong(stats, varLen);
    isValid = FALSE;
    }
else if (!regexMatch(var, CART_VAR_TIMESTAMP) &&
         !regexMatch(var, CART_VAR_VALID_CHARACTERS))
    {
    vsGotWeirdChar(stats, var);
    isValid = FALSE;
    }
else
    {
    if (stringArrayIx(var, cartVarBlackList, ArraySize(cartVarBlackList)) >= 0)
        {
        vsGotData(stats, var);
        isValid = FALSE;
        }
    else
        {
        int i;
        for (i = 0;  cartVarBlackListPrefix[i] != NULL;  i++)
            {
            if (startsWith(cartVarBlackListPrefix[i], var))
                {
                vsGotData(stats, var);
                isValid = FALSE;
                break;
                }
            }
        if (isValid)
            for (i = 0;  cartVarBlackListRegex[i] != NULL;  i++)
                {
                if (regexMatchNoCase(var, cartVarBlackListRegex[i]))
                    {
                    vsGotData(stats, var);
                    isValid = FALSE;
                    break;
                    }
                }
        }
    }
if (isValid)
    vsGotValid(stats);
return isValid;
}

static char *encodeForHgSession(char *string)
/* Allocate and return a new string with \-escaped '\\' and '\n' so that newline characters
 * don't cause bogus cart variables to appear (while truncating the original value) in files
 * downloaded from hgSession.  Convert "\r\n" and lone '\r' to '\n'. */
{
if (string == NULL)
    return NULL;
int inLen = strlen(string);
char outBuf[2*inLen + 1];
char *pIn, *pOut;
for (pIn = string, pOut = outBuf;  *pIn != '\0';  pIn++)
    {
    if (*pIn == '\\')
        {
        *pOut++ = '\\';
        *pOut++ = '\\';
        }
    else if (*pIn == '\r')
        {
        if (*(pIn+1) != '\n')
            {
            *pOut++ = '\\';
            *pOut++ = 'n';
            }
        }
    else if (*pIn == '\n')
        {
        *pOut++ = '\\';
        *pOut++ = 'n';
        }
    else
        *pOut++ = *pIn;
    }
*pOut = '\0';
return cloneString(outBuf);
}

static void decodeForHgSession(char *string)
/* Decode in place \-escaped '\\' and '\n' in string.  Note: some older files have only
 * \n escaped, not backslashes -- so watch out for those. */
{
if (string == NULL)
    return;
char *pIn, *pOut;
for (pIn = string, pOut = string;  *pIn != '\0';  pIn++, pOut++)
    {
    if (*pIn == '\\')
        {
        char *pNext = pIn + 1;
        if (*pNext == 'n')
            {
            pIn++;
            *pOut = '\n';
            }
        else if (*pNext == '\\')
            {
            pIn++;
            *pOut = '\\';
            }
        else
            // '\\' followed by anything other than '\\' or 'n' means we're reading in
            // an older file in which '\\' was not escaped; ignore.
            *pOut = *pIn;
        }
    else
        *pOut = *pIn;
    }
*pOut = '\0';
}

// DEVELOPER NOTE: If you add anything to this list, verify that the specific multiline
// input variable occurs in the middle of an alphabetically ordered cluster of variables
// with the same CGI prefix.  If so, hasMultilineCgiPrefix needs to include the prefix.
// If not then we need a new approach to detecting unencoded newlines.
static char *multilineVars[] = { "hgS_newSessionDescription",
                                 "hgta_enteredUserRegionFile",
                                 "hgta_enteredUserRegions",
                                 "hgta_pastedIdentifiers",
                                 "hgva_hgvs",
                                 "hgva_variantIds",
                                 "phyloGif_tree",
                                 "phyloPng_tree",
                                 "suggestDetails" };

static boolean isMultilineVar(char *var)
/* Return TRUE if var may contain newlines that we forgot to encode for many years. */
{
return (var && stringArrayIx(var, multilineVars, ArraySize(multilineVars)) >= 0);
}

static boolean hasMultilineCgiPrefix(char *var)
/* Return TRUE if var seems to come from the same CGI as a multiline var. */
{
boolean matches = FALSE;
if (isNotEmpty(var))
    {
    if (startsWith("hgS_", var) ||
        startsWith("hgta_", var) ||
        startsWith("hgva_", var) ||
        startsWith("phyloGif_", var) ||
        startsWith("phyloPng_", var) ||
        startsWith("suggest", var))
        matches = TRUE;
    }
return matches;
}

static void extendPrevCartVar(struct cart *cart, char *prevVar, char *var, char *val)
/* Concatenate newline and var/val onto previous variable's value. */
{
char *prevVal = cartString(cart, prevVar);
struct dyString *dy = dyStringCreate("%s\n%s", prevVal, var);
if (isNotEmpty(val))
    dyStringPrintf(dy, " %s", val);
cartSetString(cart, prevVar, dy->string);
dyStringFree(&dy);
}

static void updatePrevVar(char **pPrevVar, char *var)
/* If pPrevVar is not NULL, free the old value of *pPrevVar and set it to a clone of var. */
{
if (pPrevVar)
    {
    freeMem(*pPrevVar);
    *pPrevVar = cloneString(var);
    }
}

static boolean cartAddSettingIfValid(struct cart *cart, char *var, char *val,
                                     struct validityStats *stats, char **pPrevVar,
                                     boolean decodeVal)
/* If var and val raise no red flags then add the setting to cart and return TRUE.
 * Use *pPrevVar to detect and fix unencoded newlines.  Update *pPrevVar if setting is valid.
 * If decodeVal, then call decodeForHgSession on val (from hgSession saved settings file). */
{
char *prevVar = pPrevVar ? *pPrevVar : NULL;
boolean addToCart = TRUE;
if (! isValidCartVar(var, stats))
    {
    // This might be a sign of garbage being uploaded as a session -- or it might
    // be a case of unencoded newlines causing the appearance of bogus cart variables.
    addToCart = FALSE;
    if (isMultilineVar(prevVar) &&
        (stats->lastType == vsWeird || stats->lastType == vsData))
        {
        // It's our fault with the unencoded newlines, don't count it as invalid cart var.
        vsUndo(stats);
        extendPrevCartVar(cart, prevVar, var, val);
        }
    }
else if (isMultilineVar(prevVar))
    {
    // We need to watch out for "vars" that make it past the validity patterns
    // but are part of unencoded multiline input.
    // A bit dicey, but all of the multi-line variables that I know of occur
    // in the middle of an alphabetized cluster of vars with the same prefix.
    // DEVELOPER NOTE: check that assumption when changing multilineVars/hasMultilineCgiPrefix.
    if (! hasMultilineCgiPrefix(var))
        {
        extendPrevCartVar(cart, prevVar, var, val);
        addToCart = FALSE;
        }
    else
        {
        // Check cart var length post-extension.
        char *extendedVal = cartOptionalString(cart, prevVar);
        size_t extendedValLen = strlen(extendedVal);
        if (extendedValLen > CART_VAL_MAX_LENGTH)
            {
            vsGotValTooLong(stats, prevVar, extendedValLen);
            cartRemove(cart, prevVar);
            }
        }
    }
if (addToCart)
    {
    if (val != NULL)
        {
        size_t valLen = strlen(val);
        if (valLen > CART_VAL_MAX_LENGTH)
            {
            addToCart = FALSE;
            vsGotValTooLong(stats, var, valLen);
            }
        else
            {
            if (decodeVal)
                decodeForHgSession(val);
            cartAddString(cart, var, val);
            updatePrevVar(pPrevVar, var);
            }
        }
    else if (var != NULL)
        {
        cartSetString(cart, var, "");
        updatePrevVar(pPrevVar, var);
        }
    }
return addToCart;
}

boolean cartLoadSettingsFromUserInput(struct lineFile *lf, struct cart *cart, struct hash *oldVars,
                                      char *actionVar, struct dyString *dyMessage)
/* Verify that the user data in lf looks like valid settings (hgSession saved file;
 * like cartDump output, but values may or may not be htmlEncoded).
 * Older session files may have unencoded newlines, causing bogus variables;
 * watch out for those after pasted input variables like hgta_pastedIdentifiers.
 * Users have uploaded custom tracks, DTC genotypes, hgTracks HTML, even
 * binary data files.  Look for problematic patterns observed in the past.
 * Load settings into current session, and then reload the CGI settings
 * (to support override of session settings).
 * If non-NULL, oldVars will contain values overloaded when reloading CGI.
 * If non-NULL, actionVar is a cartRemove wildcard string specifying the
 * CGI action variable that sent us here.
 * If input contains suspect data, then add diagnostics to dyMessage.  If input
 * contains so much garbage that we shouldn't even try to load what passes the filters,
 * return FALSE. */
{
boolean isValidEnough = TRUE;
char *line = NULL;
int size = 0;
char *sessionVar = cartSessionVarName();
char *hgsid = cartSessionId(cart);
char *sessionTableString = cartOptionalString(cart, hgSessionTableState);
sessionTableString = cloneString(sessionTableString);
char *pubSessionsTableString = cartOptionalString(cart, hgPublicSessionsTableState);
pubSessionsTableString = cloneString(pubSessionsTableString);
cartRemoveLike(cart, "*");
cartSetString(cart, sessionVar, hgsid);
if (sessionTableString != NULL)
    cartSetString(cart, hgSessionTableState, sessionTableString);
if (pubSessionsTableString != NULL)
    cartSetString(cart, hgPublicSessionsTableState, pubSessionsTableString);

char *prevVar = NULL;
struct validityStats stats;
vsInit(&stats);
while (lineFileNext(lf, &line, &size))
    {
    char *var = line;
    // We actually want to keep leading spaces intact... they're a sign of var not being a real var.
    char *p = skipLeadingSpaces(var);
    if (!p)
        p = var;
    char *val = skipToSpaces(p);
    if (val)
        *val++ = '\0';
    if (isEmpty(var) || var[0] == '#')
        // Ignore blank line / comment
        continue;
    else if (sameString(var, sessionVar))
        // Ignore old sessionVar (already set above)
	continue;
    else if (! cartAddSettingIfValid(cart, var, val, &stats, &prevVar, TRUE))
        {
        if (vsTooManyErrors(&stats))
            {
            isValidEnough = FALSE;
            break;
            }
        }
    }
freeMem(prevVar);
if (stats.validCount == 0 && vsErrorCount(&stats) > 0)
    isValidEnough = FALSE;
if (isValidEnough)
    {
    if (oldVars)
        hashEmpty(oldVars);
    /* Overload settings explicitly passed in via CGI (except for the
     * command that sent us here): */
    loadCgiOverHash(cart, oldVars);
    }
if (isNotEmpty(actionVar))
    cartRemove(cart, actionVar);
vsReport(&stats, dyMessage);
vsFreeMembers(&stats);
return isValidEnough;
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
// TODO does anything need to go here for sessionKey? maybe not since id is not set here.
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

struct cart *cartFromCgiOnly(char *userId, char *sessionId,
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

static void doDisconnectHub(struct cart *cart)
{
char *id = cartOptionalString(cart, "hubId");

if (id != NULL)
    {
    char buffer[1024];
    safef(buffer, sizeof buffer, "hgHubConnect.hub.%s", id);
    cartRemove(cart, buffer);

    // now we need to remove any custom tracks that are on this hub
    safef(buffer, sizeof buffer, "ctfile_hub_%s", id);
    cartRemovePrefix(cart, buffer);
    }

cartRemove(cart, "hubId");
cartRemove(cart, hgHubDoDisconnect);
}

static void hideIfNotInCart(struct cart *cart, char *track)
/* If this track is not mentioned in the cart, set it to hide */
{
if (cartOptionalString(cart, track) == NULL)
    cartSetString(cart, track, "hide");
}

void cartHideDefaultTracks(struct cart *cart)
/* Hide all the tracks who have default visibilities in trackDb
 * that are something other than hide.  Do this only if the
 * variable CART_HAS_DEFAULT_VISIBILITY is set in the cart.  */
{
char *defaultString = cartOptionalString(cart, CART_HAS_DEFAULT_VISIBILITY);
boolean cartHasDefaults = (defaultString != NULL) && sameString(defaultString, "on");

if (!cartHasDefaults)
    return;

char *db = cartString(cart, "db");
struct trackDb *tdb = hTrackDb(db);
for(; tdb; tdb = tdb->next)
    {
    struct trackDb *parent = tdb->parent;
    if (parent && parent->isShow)
        hideIfNotInCart(cart, parent->track);
    if (tdb->visibility != tvHide)
        hideIfNotInCart(cart, tdb->track);
    }

// Don't do this again until someone sets this variable, 
// presumably on session load.
cartRemove(cart, CART_HAS_DEFAULT_VISIBILITY);
}

struct cart *cartNew(char *userId, char *sessionId,
                     char **exclude, struct hash *oldVars)
/* Load up cart from user & session id's.  Exclude is a null-terminated list of
 * strings to not include */
{
cgiApoptosisSetup();
if (cfgOptionBooleanDefault("showEarlyErrors", FALSE))
    errAbortSetDoContentType(TRUE);

if (cfgOptionBooleanDefault("suppressVeryEarlyErrors", FALSE))
    htmlSuppressErrors();
setUdcCacheDir();

struct cart *cart;
struct sqlConnection *conn = cartDefaultConnector();
char *ex;
boolean userIdFound = FALSE, sessionIdFound = FALSE;

AllocVar(cart);
cart->hash = newHash(12);
cart->exclude = newHash(7);
cart->userId = userId;
cart->sessionId = sessionId;
cart->userInfo = loadDb(conn, userDbTable(), userId, &userIdFound);
cart->sessionInfo = loadDb(conn, sessionDbTable(), sessionId, &sessionIdFound);
if (sessionIdFound)
    cartParseOverHash(cart, cart->sessionInfo->contents);
else if (userIdFound)
    cartParseOverHash(cart, cart->userInfo->contents);
char when[1024];
safef(when, sizeof(when), "open %s %s", userId, sessionId);
cartTrace(cart, when, conn);

loadCgiOverHash(cart, oldVars);

// I think this is the place to justify old and new values
cartJustify(cart, oldVars);

#ifndef GBROWSE
/* If some CGI other than hgSession been passed hgSession loading instructions,
 * apply those to cart before we do anything else.  (If this is hgSession,
 * let it handle the settings so it can display feedback to the user.) */
boolean didSessionLoad = FALSE;
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
	didSessionLoad = TRUE;
	}
    else if (cartVarExists(cart, hgsDoLoadUrl))
	{
	char *url = cartString(cart, hgsLoadUrlName);
	struct lineFile *lf = netLineFileOpen(url);
        struct dyString *dyMessage = dyStringNew(0);
	boolean ok = cartLoadSettingsFromUserInput(lf, cart, oldVars, hgsDoLoadUrl, dyMessage);
	lineFileClose(&lf);
	cartTrace(cart, "after cartLS", conn);
        if (! ok)
            {
            warn("Unable to load session file: %s", dyMessage->string);
            }
	didSessionLoad = ok;
        dyStringFree(&dyMessage);
	}
    }
#endif /* GBROWSE */

/* wire up the assembly hubs so we can operate without sql */
setUdcTimeout(cart);
if (cartVarExists(cart, hgHubDoDisconnect))
    doDisconnectHub(cart);

if (didSessionLoad)
    cartCopyCustomComposites(cart);

pushWarnHandler(cartHubWarn);
char *newDatabase = hubConnectLoadHubs(cart);
popWarnHandler();

if (newDatabase != NULL)
    {
    char *cartDb = cartOptionalString(cart, "db");

    if ((cartDb == NULL) || differentString(cartDb, newDatabase))
        {
        // this is some magic to use the defaultPosition and reset cart variables
        if (oldVars)
            {
            struct hashEl *hel;
            if ((hel = hashLookup(oldVars,"db")) != NULL)
                hel->val = "none";
            else
                hashAdd(oldVars, "db", "none");
            }
        cartSetString(cart,"db", newDatabase);
        }
    }

if (exclude != NULL)
    {
    while ((ex = *exclude++))
	cartExclude(cart, ex);
    }

cartDefaultDisconnector(&conn);

if (didSessionLoad)
    cartHideDefaultTracks(cart);
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
if (cartDbUseSessionKey())
  sqlDyStringPrintf(dy, " and sessionKey='%s'", cdb->sessionKey);
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

/* update sessionDb and userDb tables (removed check for cart stuffing bots) */
updateOne(conn, userDbTable(), cart->userInfo, encoded->string, encoded->stringSize);
updateOne(conn, sessionDbTable(), cart->sessionInfo, encoded->string, encoded->stringSize);

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

void cartSaveState(struct cart *cart)
/* Free up cart and save it to database.
 * Intended for updating cart before background CGI runs.
 * Use cartCheckout() instead. */
{
if (cart != NULL)
    {
    saveState(cart);
    }
}

char *cartSessionVarName()
/* Return name of CGI session ID variable. */
{
return sessionVar;
}

char *cartSessionId(struct cart *cart)
/* Return session id. */
{
static char buf[256];
cartDbSecureId(buf, sizeof buf, cart->sessionInfo);
return buf;
}

unsigned cartSessionRawId(struct cart *cart)
/* Return raw session id without security key. */
{
return cart->sessionInfo->id;
}

char *cartSidUrlString(struct cart *cart)
/* Return session id string as in hgsid=N . */
{
static char buf[64];
safef(buf, sizeof(buf), "%s=%s", cartSessionVarName(), cartSessionId(cart));
return buf;
}

char *cartUserId(struct cart *cart)
/* Return session id. */
{
static char buf[256];
cartDbSecureId(buf, sizeof buf, cart->userInfo);
return buf;
}

unsigned cartUserRawId(struct cart *cart)
/* Return raw user id without security key. */
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
while (cartVars != NULL)
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
while (cartVars != NULL)
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

boolean cartListVarExistsAnyLevel(struct cart *cart, struct trackDb *tdb,
                                  boolean parentLevel, char *suffix)
/* Return TRUE if a list variable for tdb->track (or tdb->parent->track,
 * or tdb->parent->parent->track, etc.) is in cart (list itself may be NULL). */
{
if (parentLevel)
    tdb = tdb->parent;
for ( ; tdb != NULL; tdb = tdb->parent)
    {
    char var[PATH_LEN];
    if (suffix[0] == '.' || suffix[0] == '_')
        safef(var, sizeof var, "%s%s%s", cgiMultListShadowPrefix(), tdb->track, suffix);
    else
        safef(var, sizeof var, "%s%s.%s", cgiMultListShadowPrefix(), tdb->track, suffix);
    char *cartSetting = hashFindVal(cart->hash, var);
    if (cartSetting != NULL)
	return TRUE;
    }
return FALSE;
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

struct hash *cartHashList(struct cart *cart, char *var)
/* Return hash with multiple values for the same var or NULL if not found. */
{
struct hashEl *hel = hashLookup(cart->hash, var);
struct hash *valHash = hashNew(0);
while (hel != NULL)
    {
    if (hel->val != NULL)
	{
        hashAdd(valHash, hel->val, NULL);
	}
    hel = hashLookupNext(hel);
    }
return valHash;
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
if (sameWord(s, "on") || atoi(s) > 0)
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
return (sameWord(s, "on") || atoi(s) > 0);
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
// Be explicit because some cartBools overloaded with negative "disabled" values
cartSetInt(cart,var,(val?1:0));
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
cgiMakeHiddenVar(sessionVar, cartSessionId(cart));
}

static void cartDumpItem(struct hashEl *hel, boolean asTable, boolean encodeAsHtml)
/* Dump one item in cart hash.  If encodeAsHtml, call htmlEncode on variable name and value;
 * otherwise, call encodeForHgSession on value. */
{
char *var = hel->name, *val = NULL;
if (encodeAsHtml)
    {
    var = htmlEncode(hel->name);
    val = htmlEncode((char *)(hel->val));
    }
else
    {
    val = encodeForHgSession((char *)(hel->val));
    }

if (asTable)
    {
    printf("<TR><TD>%s</TD><TD>", var);
    int width=(strlen(val)+1)*8;
    if (width<100)
        width = 100;
    cgiMakeTextVarWithJs(hel->name, val, width,
                                "change", "setCartVar(this.name,this.value);");
    printf("</TD></TR>\n");
    }
else
    printf("%s %s\n", var, val);
freeMem(val);
if (encodeAsHtml)
    freeMem(var);
}

static void cartDumpList(struct hashEl *elList, boolean asTable, boolean encodeAsHtml)
/* Dump list of cart variables optionally as a table with ajax update support.
 * If encodeAsHtml, call htmlEncode on variable name and value; otherwise, call
 * encodeForHgSession on value to prevent newlines from corrupting settings saved
 * to a file. */
{
struct hashEl *el;

if (elList == NULL)
    return;
slSort(&elList, hashElCmp);
if (asTable)
    printf("<table>\n");
for (el = elList; el != NULL; el = el->next)
    cartDumpItem(el, asTable, encodeAsHtml);
if (asTable)
    {
    printf("<tr><td colspan=2>&nbsp;&nbsp;<em>count: %d</em></td></tr>\n",slCount(elList));
    printf("</table>\n");
    }
hashElFreeList(&elList);
}

void cartDumpHgSession(struct cart *cart)
/* Dump contents of cart with escaped newlines for hgSession output files.
 * Cart variable "cartDumpAsTable" is ignored. */
{
struct hashEl *elList = hashElListHash(cart->hash);
cartDumpList(elList, FALSE, FALSE);
}

void cartDump(struct cart *cart)
/* Dump contents of cart. */
{
struct hashEl *elList = hashElListHash(cart->hash);
cartDumpList(elList,cartVarExists(cart,CART_DUMP_AS_TABLE), TRUE);
}

void cartDumpPrefix(struct cart *cart, char *prefix)
/* Dump all cart variables with prefix */
{
struct hashEl *elList = cartFindPrefix(cart, prefix);
cartDumpList(elList,cartVarExists(cart,CART_DUMP_AS_TABLE), TRUE);
}

void cartDumpLike(struct cart *cart, char *wildcard)
/* Dump all cart variables matching wildcard */
{
struct hashEl *elList = cartFindLike(cart, wildcard);
cartDumpList(elList,cartVarExists(cart,CART_DUMP_AS_TABLE), TRUE);
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

static char *getCookieId(char *cookieName)
/* Get id value from cookie. */
{
return findCookieData(cookieName);
}

static char *getSessionId()
/* Get session id if any from CGI. */
{
return cgiOptionalString("hgsid");
}

static void clearDbContents(struct sqlConnection *conn, char *table, char * secureId)
/* Clear out contents field of row in table that matches id. */
{
if (!secureId)
    return;
struct dyString *query = dyStringNew(256);
char *sessionKey = NULL;	    
unsigned int id = cartDbParseId(secureId, &sessionKey);
sqlDyStringPrintf(query, "update %s set contents='' where id=%u", table, id);
if (cartDbUseSessionKey())
    {
    if (!sessionKey)
	sessionKey = "";
    sqlDyStringPrintf(query, " and sessionKey='%s'", sessionKey);
    }
sqlUpdate(conn, query->string);
dyStringFree(&query);


}

void cartResetInDb(char *cookieName)
/* Clear cart in database. */
{
char *hguid = getCookieId(cookieName);
char *hgsid = getSessionId();
struct sqlConnection *conn = cartDefaultConnector();
clearDbContents(conn, userDbTable(), hguid);
clearDbContents(conn, sessionDbTable(), hgsid);
cartDefaultDisconnector(&conn);
}

void cartWriteCookie(struct cart *cart, char *cookieName)
/* Write out HTTP Set-Cookie statement for cart. */
{
char *domain = cfgVal("central.domain");
if (sameWord("HTTPHOST", domain))
    {
    // IE9 does not accept portnames in cookie domains
    char *hostWithPort = hHttpHost();
    struct netParsedUrl npu;
    netParseUrl(hostWithPort, &npu);
    if (strchr(npu.host, '.') != NULL)	// Domains without a . don't seem to be kept
	domain = cloneString(npu.host);
    else
        domain = NULL;
    }

char userIdKey[256];
cartDbSecureId(userIdKey, sizeof userIdKey, cart->userInfo);
// Some users reported blank cookie values. Do we see that here?
if (sameString(userIdKey,"")) // make sure we do not write any blank cookies.
    {
    // Be sure we do not lose this message.
    // Because the error happens so early we cannot trust that the warn and error handlers
    // are setup correctly and working.
    verbose(1, "unexpected error in cartWriteCookie: userId string is empty.");
    dumpStack( "unexpected error in cartWriteCookie: userId string is empty.");
    warn(      "unexpected error in cartWriteCookie: userId string is empty.");
    }
else
    {
    if (!isEmpty(domain))
	printf("Set-Cookie: %s=%s; path=/; domain=%s; expires=%s\r\n",
		cookieName, userIdKey, domain, cookieDate());
    else
	printf("Set-Cookie: %s=%s; path=/; expires=%s\r\n",
		cookieName, userIdKey, cookieDate());
    }
if (geoMirrorEnabled())
    {
    // This occurs after the user has manually choosen to go back to the original site; we store redirect value into a cookie so we 
    // can use it in subsequent hgGateway requests before loading the user's cart
    char *redirect = cgiOptionalString("redirect");
    if (redirect)
        {
        printf("Set-Cookie: redirect=%s; path=/; domain=%s; expires=%s\r\n", redirect, cgiServerName(), cookieDate());
        }
    }
/* Validate login cookies if login is enabled */
if (loginSystemEnabled())
    {
    struct slName *newCookies = loginValidateCookies(cart), *sl;
    for (sl = newCookies;  sl != NULL;  sl = sl->next)
        printf("Set-Cookie: %s\r\n", sl->name);
    }
}

struct cart *cartForSession(char *cookieName, char **exclude,
                            struct hash *oldVars)
/* This gets the cart without writing any HTTP lines at all to stdout. */
{
/* Most cgis call this routine */
if (sameOk(cfgOption("signalsHandler"), "on"))  /* most cgis call this routine */
    initSigHandlers(hDumpStackEnabled());
/* Proxy Settings 
 * net.c cannot see the cart, pass the value through env var */
char *httpProxy = cfgOption("httpProxy");  
if (httpProxy)
    setenv("http_proxy", httpProxy, TRUE);
char *httpsProxy = cfgOption("httpsProxy");
if (httpsProxy)
    setenv("https_proxy", httpsProxy, TRUE);
char *ftpProxy = cfgOption("ftpProxy");
if (ftpProxy)
    setenv("ftp_proxy", ftpProxy, TRUE);
char *noProxy = cfgOption("noProxy");
if (noProxy)
    setenv("no_proxy", noProxy, TRUE);
char *logProxy = cfgOption("logProxy");
if (logProxy)
    setenv("log_proxy", logProxy, TRUE);

// if ignoreCookie is on the URL, don't check for cookies
char *hguid = NULL;
if (cgiOptionalString("ignoreCookie") == NULL)
    hguid = getCookieId(cookieName);
char *hgsid = getSessionId();
struct cart *cart = cartNew(hguid, hgsid, exclude, oldVars);
cartExclude(cart, sessionVar);
return cart;
}

struct cart *cartAndCookieWithHtml(char *cookieName, char **exclude,
                                   struct hash *oldVars, boolean doContentType)
/* Load cart from cookie and session cgi variable.  Write cookie
 * and optionally content-type part HTTP preamble to web page.  Don't
 * write any HTML though. */
{
// Note: early abort works fine but early warn does not
htmlPushEarlyHandlers();
struct cart *cart = cartForSession(cookieName, exclude, oldVars);
popWarnHandler();
popAbortHandler();

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
pushAbortHandler(htmlAbort);
hDumpStackPushAbortHandler();
int status = setjmp(htmlRecover);
if (status == 0)
    {
    doMiddle(cart);
    }
hDumpStackPopAbortHandler();
popAbortHandler();
}

void cartEarlyWarningHandler(char *format, va_list args)
/* Write an error message so user can see it before page is really started. */
{
static boolean initted = FALSE;
va_list argscp;
va_copy(argscp, args);
if (!initted && !cgiOptionalString("ajax"))
    {
    htmStart(stdout, "Early Error");
    initted = TRUE;
    }
printf("%s", htmlWarnStartPattern());
htmlVaEncodeErrorText(format,args);
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
static boolean didCartHtmlStart = FALSE;

void cartHtmlStart(char *title)
/* Write HTML header and put in normal error handler. */
{
pushWarnHandler(htmlVaWarn);
htmStart(stdout, title);
didCartHtmlStart = TRUE;
}

static void cartVaWebStartMaybeHeader(struct cart *cart, char *db, boolean withHttpHeader,
                                      char *format, va_list args)
/* Print out optional Content-Type and pretty wrapper around things when working from cart. */
{
pushWarnHandler(htmlVaWarn);
webStartWrapper(cart, trackHubSkipHubName(db), format, args, withHttpHeader, FALSE);
inWeb = TRUE;
jsIncludeFile("jquery.js", NULL);
jsIncludeFile("utils.js", NULL);
jsIncludeFile("ajax.js", NULL);
}

void cartVaWebStart(struct cart *cart, char *db, char *format, va_list args)
/* Print out pretty wrapper around things when working
 * from cart. */
{
cartVaWebStartMaybeHeader(cart, db, FALSE, format, args);
}

void cartWebStart(struct cart *cart, char *db, char *format, ...)
/* Print out pretty wrapper around things when working
 * from cart. */
{
va_list args;
va_start(args, format);
cartVaWebStart(cart, db, format, args);
va_end(args);
if (isNotEmpty(db))
    {
    // Why do we put an input outside of a form on almost every page we make?
    // Tim put this in.  Talking with him it sounds like some pages might actually
    // depend on it.  Not removing it until we have a chance to test.  Best fix
    // might be to add it to cartSaveSession, though this would then no longer be
    // well named, and not all things have 'db.'  Arrr.  Probably best to remove
    // and test a bunch.
    cgiMakeHiddenVar("db", db);
    }
}

void cartWebStartHeader(struct cart *cart, char *db, char *format, ...)
/* Print out Content-type header and then pretty wrapper around things when working
 * from cart. */
{
va_list args;
va_start(args, format);
cartVaWebStartMaybeHeader(cart, db, TRUE, format, args);
va_end(args);
}

void cartWebEnd()
/* Write out HTML footer and get rid or error handler. */
{
webEnd();
popWarnHandler();
inWeb = FALSE;
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
else if (didCartHtmlStart)
    cartFooter();
else
    return;
popWarnHandler();
}

void setThemeFromCart(struct cart *cart) 
/* If 'theme' variable is set in cart: overwrite background with the one from
 * defined for this theme Also set the "styleTheme", with additional styles
 * that can overwrite the main style settings */
{
// Get theme from cart and use it to get background file from config;
// format is browser.theme.<name>=<stylesheet>[,<background>]

char *cartTheme = cartOptionalString(cart, "theme");

// XXXX which setting should take precedence? Currently browser.theme does.

char *styleFile = cfgOption("browser.style");
if(styleFile != NULL)
    {
    char buf[512];
    safef(buf, sizeof(buf), "<link rel='stylesheet' href='%s' type='text/css'>", styleFile);
    char *copy = cloneString(buf);
    htmlSetStyleTheme(copy); // for htmshell.c, used by hgTracks
    webSetStyle(copy);       // for web.c, used by hgc
    }

if(isNotEmpty(cartTheme))
    {
    char *themeKey = catTwoStrings("browser.theme.", cartTheme);
    styleFile = cfgOption(themeKey);
    freeMem(themeKey);
    if (isEmpty(styleFile))
        return;

    char * link = webTimeStampedLinkToResourceOnFirstCall(styleFile, TRUE); // resource file link wrapped in html
    if (link != NULL)
        {
        htmlSetStyleTheme(link); // for htmshell.c, used by hgTracks
        webSetStyle(link);       // for web.c, used by hgc
        }
    }
}

void cartSetLastPosition(struct cart *cart, char *position, struct hash *oldVars)
/* If position and oldVars are non-NULL, and oldVars' position is different, add it to the cart
 * as lastPosition.  This is called by cartHtmlShell{,WithHead} but not other cart openers;
 * it should be called after cartGetPosition or equivalent. */
{
if (position != NULL && oldVars != NULL)
    {
    struct hashEl *oldPos = hashLookup(oldVars, positionCgiName);
    if (oldPos != NULL && differentString(position, oldPos->val))
        cartSetString(cart, "lastPosition", oldPos->val);
    }
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
char *db, *org, *pos;
char titlePlus[2048];
pushWarnHandler(cartEarlyWarningHandler);
cart = cartAndCookie(cookieName, exclude, oldVars);
getDbAndGenome(cart, &db, &org, oldVars);
pos = cartGetPosition(cart, db, NULL);
pos = addCommasToPos(db, stripCommas(pos));
cartSetLastPosition(cart, pos, oldVars);
safef(titlePlus, sizeof(titlePlus), "%s %s %s %s", 
                    org ? trackHubSkipHubName(org) : "", db ? db : "",  pos ? pos : "", title);
popWarnHandler();
setThemeFromCart(cart);
htmStartWithHead(stdout, head, titlePlus);
cartWarnCatcher(doMiddle, cart, htmlVaWarn);
cartCheckout(&cart);
cartFooter();
}

static void cartEmptyShellMaybeContent(void (*doMiddle)(struct cart *cart), char *cookieName,
                                       char **exclude, struct hash *oldVars, boolean doContentType)
/* Get cart and cookies and set up error handling.
 * If doContentType, print out Content-type:text/html
 * but don't start writing any html yet.
 * The doMiddleFunction has to call cartHtmlStart(title), and
 * cartHtmlEnd(), as well as writing the body of the HTML.
 * oldVars - those in cart that are overlayed by cgi-vars are
 * put in optional hash oldVars. */
{
struct cart *cart = cartAndCookieWithHtml(cookieName, exclude, oldVars, doContentType);
setThemeFromCart(cart);
cartWarnCatcher(doMiddle, cart, cartEarlyWarningHandler);
cartCheckout(&cart);
}

void cartEmptyShell(void (*doMiddle)(struct cart *cart), char *cookieName,
                    char **exclude, struct hash *oldVars)
/* Get cart and cookies and set up error handling, but don't start writing any
 * html yet. The doMiddleFunction has to call cartHtmlStart(title), and
 * cartHtmlEnd(), as well as writing the body of the HTML.
 * oldVars - those in cart that are overlayed by cgi-vars are
 * put in optional hash oldVars. */
{
cartEmptyShellMaybeContent(doMiddle, cookieName, exclude, oldVars, TRUE);
}

void cartEmptyShellNoContent(void (*doMiddle)(struct cart *cart), char *cookieName,
                             char **exclude, struct hash *oldVars)
/* Get cart and cookies and set up error handling.
 * The doMiddle function must write the Content-Type header line.
 * oldVars - those in cart that are overlayed by cgi-vars are
 * put in optional hash oldVars. */
{
cartEmptyShellMaybeContent(doMiddle, cookieName, exclude, oldVars, FALSE);
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

sqlSafef(query, sizeof(query), "select distinct subjId from hgFixed.gsIdXref order by subjId");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    fprintf(outF, "%s\n", row[0]);

    sqlSafef(query2, sizeof(query2),
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
        sqlSafef(query, sizeof(query),
	      "select id from %s where id like '%%%s'", msaTable, words[0]);
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
                                      boolean parentLevel, char *suffix,char **pVariable)
/* Returns value or NULL for a cart variable from lowest level on up. Optionally
 * fills the non NULL pVariable with the actual name of the variable in the cart */
{
if (parentLevel)
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

void cartRemoveVariableClosestToHome(struct cart *cart, struct trackDb *tdb,
                                     boolean parentLevel, char *suffix)
/* Looks for then removes a cart variable from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */
{
char *var = NULL;
(void)cartLookUpVariableClosestToHome(cart,tdb,parentLevel,suffix,&var);
if (var != NULL)
    {
    cartRemove(cart,var);
    freeMem(var);
    }
}

char *cartStringClosestToHome(struct cart *cart, struct trackDb *tdb,
                              boolean parentLevel, char *suffix)
/* Returns value or Aborts for a cart string from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */
{
char *setting = cartOptionalStringClosestToHome(cart,tdb,parentLevel,suffix);
if (setting == NULL)
    errAbort("cartStringClosestToHome: '%s' not found", suffix);
return setting;
}

boolean cartVarExistsAnyLevel(struct cart *cart, struct trackDb *tdb,
                              boolean parentLevel, char *suffix)
/* Returns TRUE if variable exists anywhere, looking from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */
{
return (NULL != cartOptionalStringClosestToHome(cart,tdb,parentLevel,suffix));
}


char *cartUsualStringClosestToHome(struct cart *cart, struct trackDb *tdb,
                                   boolean parentLevel, char *suffix, char *usual)
/* Returns value or {usual} for a cart string from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */
{
char *setting = cartOptionalStringClosestToHome(cart,tdb,parentLevel,suffix);
if(setting == NULL)
    setting = usual;
return setting;
}

boolean cartBooleanClosestToHome(struct cart *cart, struct trackDb *tdb,
                                 boolean parentLevel, char *suffix)
/* Returns value or Aborts for a cart boolean ('on' or != 0) from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */
{
char *setting = cartStringClosestToHome(cart,tdb,parentLevel,suffix);
return (sameString(setting, "on") || atoi(setting) > 0);
}

boolean cartUsualBooleanClosestToHome(struct cart *cart, struct trackDb *tdb,
                                      boolean parentLevel, char *suffix,boolean usual)
/* Returns value or {usual} for a cart boolean ('on' or != 0) from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */
{
char *setting = cartOptionalStringClosestToHome(cart,tdb,parentLevel,suffix);
if (setting == NULL)
    return usual;
return (sameString(setting, "on") || atoi(setting) > 0);
}


int cartUsualIntClosestToHome(struct cart *cart, struct trackDb *tdb,
                              boolean parentLevel, char *suffix, int usual)
/* Returns value or {usual} for a cart int from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */
{
char *setting = cartOptionalStringClosestToHome(cart,tdb,parentLevel,suffix);
if (setting == NULL)
    return usual;
return atoi(setting);
}

double cartUsualDoubleClosestToHome(struct cart *cart, struct trackDb *tdb,
                                    boolean parentLevel, char *suffix, double usual)
/* Returns value or {usual} for a cart fp double from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */
{
char *setting = cartOptionalStringClosestToHome(cart,tdb,parentLevel,suffix);
if (setting == NULL)
    return usual;
return atof(setting);
}

struct slName *cartOptionalSlNameListClosestToHome(struct cart *cart, struct trackDb *tdb,
                                                   boolean parentLevel, char *suffix)
/* Return slName list (possibly with multiple values for the same var) from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */
{
char *var = NULL;
cartLookUpVariableClosestToHome(cart,tdb,parentLevel,suffix,&var);
if (var == NULL)
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
for (subTdb=tdb->subtracks;subTdb!=NULL;subTdb=subTdb->next)
    cartRemoveAllForTdbAndChildren(cart,subTdb);
cartRemoveAllForTdb(cart,tdb);
saveState(cart);
}

char *cartOrTdbString(struct cart *cart, struct trackDb *tdb, char *var, char *defaultVal)
/* Look first in cart, then in trackDb for var.  Return defaultVal if not found. */
{
char *tdbDefault = trackDbSettingClosestToHomeOrDefault(tdb, var, defaultVal);
boolean parentLevel = isNameAtParentLevel(tdb, var);
return cartUsualStringClosestToHome(cart, tdb, parentLevel, var, tdbDefault);
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

boolean cartOrTdbBoolean(struct cart *cart, struct trackDb *tdb, char *var, boolean defaultVal)
/* Look first in cart, then in trackDb for var.  Return defaultVal if not found. */
{
boolean tdbVal = defaultVal;
char *tdbSetting = trackDbSetting(tdb, var);
if (tdbSetting != NULL)
    tdbVal = trackDbSettingClosestToHomeOn(tdb, var);
return cartUsualBooleanClosestToHome(cart, tdb, isNameAtParentLevel(tdb, var), var, tdbVal);
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


boolean cartValueHasChanged(struct cart *newCart,struct hash *oldVars,char *setting,
                            boolean ignoreRemoved,boolean ignoreCreated)
/* Returns TRUE if new cart setting has changed from old cart setting */
{
char *oldValue = hashFindVal(oldVars,setting);
if (oldValue == NULL)
    return FALSE;  // All vars changed by cgi will be found in old vars

char *newValue = cartOptionalString(newCart,setting);
if (newValue == NULL)
    return (!ignoreRemoved);

if (sameString(oldValue,CART_VAR_EMPTY))
    {
    if (sameString(newValue,"hide")
    ||  sameString(newValue,"off")
    ||  sameString(newValue,"0"))   // Special cases DANGER!
        return FALSE;
    }

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
    char setting[512];
    if (vis)
        safef(setting,sizeof(setting),"%s",descendentTdb->track);
    else
        safef(setting,sizeof(setting),"%s.%s",descendentTdb->track,suffix);
    removed += cartRemoveAndCount(cart,setting);
    }
return removed;
}

static int cartRemoveOldFromTdbTree(struct cart *newCart,struct hash *oldVars,struct trackDb *tdb,
                                    char *suffix,char *parentVal,boolean skipParent)
// Removes a 'trackName.suffix' from all tdb descendents (but not parent), BUT ONLY
//   IF OLD or same as parentVal. If suffix NULL then removes 'trackName' which holds visibility
{
int removed = 0;
boolean vis = (suffix == NULL || *suffix == '\0');
struct slRef *tdbRef, *tdbRefList = trackDbListGetRefsToDescendants(skipParent?tdb->subtracks:tdb);
for (tdbRef = tdbRefList; tdbRef != NULL; tdbRef = tdbRef->next)
    {
    struct trackDb *descendentTdb = tdbRef->val;
    char setting[512];
    if (vis)
        safef(setting,sizeof(setting),"%s",descendentTdb->track);
    else
        safef(setting,sizeof(setting),"%s.%s",descendentTdb->track,suffix);
    char *newVal = cartOptionalString(newCart,setting);
    if (    newVal    != NULL
    && (   (parentVal != NULL && sameString(newVal,parentVal))
        || (FALSE == cartValueHasChanged(newCart,oldVars,setting,TRUE,FALSE))))
        removed += cartRemoveAndCount(newCart,setting);
    }
return removed;
}

static boolean cartTdbOverrideSuperTracks(struct cart *cart,struct trackDb *tdb,
                                          boolean ifJustSelected)
// When when the child of a hidden supertrack is foudn and selected, then shape the
// supertrack accordingly. Returns TRUE if any cart changes are made
{
// This is only pertinent to supertrack children just turned on
if (!tdbIsSuperTrackChild(tdb))
    return FALSE;

char setting[512];

// Must be from having just selected the track in findTracks.
// This will carry with it the "_sel" setting.
if (ifJustSelected)
    {
    safef(setting,sizeof(setting),"%s_sel",tdb->track);
    if (!cartVarExists(cart,setting))
        return FALSE;
    cartRemove(cart,setting); // Unlike composite subtracks, supertrack children keep
    }                         // the "_sel" setting only for detecting this moment

// if parent is not hidden then nothing to do
ASSERT(tdb->parent != NULL && tdbIsSuperTrack(tdb->parent));
enum trackVisibility vis = tdbVisLimitedByAncestry(cart, tdb->parent, FALSE);
if (vis != tvHide)
    return FALSE;

// Now turn all other supertrack children to hide and the supertrack to visible
struct slRef *childRef;
for (childRef = tdb->parent->children;childRef != NULL; childRef = childRef->next)
    {
    struct trackDb *child = childRef->val;
    if (child == tdb)
        continue;

    // Make sure this child hasn't also just been turned on!
    safef(setting,sizeof(setting),"%s_sel",child->track);
    if (cartVarExists(cart,setting))
        {
        cartRemove(cart,setting); // Unlike composite subtracks, supertrack children keep
        continue;                 // the "_sel" setting only for detecting this moment
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


static int cartTdbParentShapeVis(struct cart *cart,struct trackDb *parent,char *view,
                                 struct hash *subVisHash,boolean reshapeFully)
// This shapes one level of vis (view or container) based upon subtrack specific visibility.
// Returns count of tracks affected
{
ASSERT(view || (tdbIsContainer(parent) && tdbIsContainerChild(parent->subtracks)));
struct trackDb *subtrack = NULL;
char setting[512];
safef(setting,sizeof(setting),"%s",parent->track);

enum trackVisibility visMax  = tvHide;
enum trackVisibility visOrig = tdbVisLimitedByAncestry(cart, parent, FALSE);

// Should walk through children to get max new vis for this parent
for (subtrack = parent->subtracks;subtrack != NULL;subtrack = subtrack->next)
    {
    char *foundVis = hashFindVal(subVisHash, subtrack->track); // if the subtrack doesn't have
    if (foundVis != NULL)                                      // individual vis AND...
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
// If view, this should always be set, since if a single view needs
// to be promoted, the composite will go to full.
if (tdbIsCompositeView(parent))
    {
    cartSetString(cart,setting,hStringFromTv(visMax)); // Set this explicitly.
    countVisChanged++;                                 // The visOrig may be inherited!
    }

if (visMax != visOrig || reshapeFully)
    {
    if (!tdbIsCompositeView(parent)) // view vis is always shaped,
        {                            // but composite vis is conditionally shaped.
        cartSetString(cart,setting,hStringFromTv(visMax));    // Set this explicitly.
        countVisChanged++;
        if (visOrig == tvHide && tdbIsSuperTrackChild(parent))
            cartTdbOverrideSuperTracks(cart,parent,FALSE);    // deal with superTrack vis! cleanup
        }

    // Now set all subtracks that inherit vis back to visOrig
    for (subtrack = parent->subtracks;subtrack != NULL;subtrack = subtrack->next)
        {
        int fourState = subtrackFourStateChecked(subtrack,cart);
        if (!fourStateChecked(fourState))
            cartRemove(cart,subtrack->track); // Remove subtrack level vis if it isn't even checked
        else  // subtrack is checked (should include subtrack level vis)
            {
            char *visFromHash = hashFindVal(subVisHash, subtrack->track);
            if (tdbIsMultiTrack(parent))           // MultiTrack vis is ALWAYS inherited vis
                cartRemove(cart,subtrack->track);  // and non-selected should not have vis
            if (visFromHash == NULL)   // if the subtrack doesn't have individual vis AND...
                {
                if (reshapeFully || visOrig == tvHide)
                    {                                       // uncheck
                    subtrackFourStateCheckedSet(subtrack, cart,FALSE,fourStateEnabled(fourState));
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
    {
    WARN("%s visOrig:%s visMax:%s unchecked:%d  Vis changed:%d",parent->track,
         hStringFromTv(visOrig),hStringFromTv(visMax),countUnchecked,countVisChanged);
    }

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
        if (fourStateVisible(fourState))            // subtrack is checked too
            {
            WARN("Subtrack level vis %s=%s",subtrack->track,val);
            hashAdd(subVisHash,subtrack->track,val);
            safef(setting,sizeof(setting),"%s_sel",subtrack->track);
            hashAdd(subVisHash,setting,subtrack); // Add the "_sel" setting which should also exist.
            }                                     // Point it to subtrack
        }
    }
slFreeList(&tdbRefList);

if (hashNumEntries(subVisHash) == 0)
    {
    //WARN("No subtrack level vis for %s",tdbContainer->track);
    return FALSE;
    }

// Next look for any cart settings other than subtrack vis/sel
// New directive means if composite is hidden, then ignore previous and don't bother checking cart.
boolean reshapeFully = (tdbVisLimitedByAncestry(cart, tdbContainer, FALSE) == tvHide);
boolean hasViews = tdbIsCompositeView(tdbContainer->subtracks);

WARN("reshape: %s",reshapeFully?"Fully":"Incrementally");

// Now shape views and composite to match subtrack specific visibility
int count = 0;
if (hasViews)
    {
    for (tdbView = tdbContainer->subtracks;tdbView != NULL; tdbView = tdbView->next )
        {
        char *view = NULL;
        if (tdbIsView(tdbView,&view) )
            count += cartTdbParentShapeVis(cart,tdbView,view,subVisHash,reshapeFully);
        }
    if (count > 0)
        {
        // At least one view was shaped, so all views will get explicit vis.
        // This means composite must be set to full
        enum trackVisibility visOrig = tdbVisLimitedByAncestry(cart, tdbContainer, FALSE);
        cartSetString(cart,tdbContainer->track,"full");    // Now set composite to full.
        if (visOrig == tvHide && tdbIsSuperTrackChild(tdbContainer))
            cartTdbOverrideSuperTracks(cart,tdbContainer,FALSE);// deal with superTrack vis cleanup
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
    char *cartVis = cartOptionalString(newCart,tdbView->track);
    if (cartVis != NULL)  // special to get viewVis in the list
        {
        lmAllocVar(lm, oneName);
        oneName->name = lmCloneString(lm, tdbView->track);
        oneName->val = lmCloneString(lm, cartVis);
        slAddHead(&changedSettings,oneName);
        }

    // Now the non-vis settings
    safef(setting,sizeof(setting),"%s.",tdbView->track);
    struct slPair *changeViewSettings = cartVarsWithPrefixLm(newCart, setting, lm);
    changedSettings = slCat(changedSettings, changeViewSettings);
    }
if (changedSettings == NULL && !containerVisChanged)
    return anythingChanged;

// Prune list to only those which have changed
if (changedSettings != NULL)
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
        char *cartVis = NULL;
        boolean viewVisChanged = FALSE;
        if (!tdbIsView(tdbView,&view))
            break;

        struct slPair *leftOvers = NULL;
        // Walk through settings that match this view
        while ((oneName = slPopHead(&changedSettings)) != NULL)
            {
            suffix = NULL;
            if (startsWith(tdbView->track,oneName->name))
                {
                suffix = oneName->name + strlen(tdbView->track);
                if (*suffix == '.') // NOTE: standardize on '.' since its is so pervasive
                    suffix++;
                else if (isalnum(*suffix)) // viewTrackName is subset of another track!
                    suffix = NULL;         // add back to list for next round
                }

            if (suffix == NULL)
                {
                slAddHead(&leftOvers,oneName);
                continue;
                }

            if (*suffix == '\0')
                {
                viewVisChanged = TRUE;
                cartVis = oneName->val;
                }
            else  // be certain to exclude vis settings here
            if (cartRemoveOldFromTdbTree(newCart,oldVars,tdbView,suffix,oneName->val,TRUE) > 0)
                clensed++;

            //slPairFree(&oneName); // lm memory so free not needed
            }
        if (viewVisChanged)
            {
            // If just created and if vis is the same as tdb default then vis has not changed
            char *oldValue = hashFindVal(oldVars,tdbView->track);
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

void cgiExitTime(char *cgiName, long enteredMainTime)
/* single stderr print out called at end of CGI binaries to record run
 * time in apache error_log */
{
if (sameWord("yes", cfgOptionDefault("browser.cgiTime", "yes")) )
  fprintf(stderr, "CGI_TIME: %s: Overall total time: %ld millis\n",
        cgiName, clock1000() - enteredMainTime);
}

// TODO This should probably be moved to customFactory.c
// Only used by hgSession 
#include "errCatch.h"
void cartCheckForCustomTracks(struct cart *cart, struct dyString *dyMessage)
/* Scan cart for ctfile_<db> variables.  Tally up the databases that have
 * live custom tracks and those that have expired custom tracks. */
/* While we're at it, also look for saved blat results. */
{
struct hashEl *helList = cartFindPrefix(cart, CT_FILE_VAR_PREFIX);
if (helList != NULL)
    {
    struct hashEl *hel;
    boolean gotLiveCT = FALSE, gotExpiredCT = FALSE, gotErrorCT = FALSE;
    struct slName *liveDbList = NULL, *expiredDbList = NULL,  *errorDbList = NULL, *sln = NULL;
    for (hel = helList;  hel != NULL;  hel = hel->next)
	{
	char *db = hel->name + strlen(CT_FILE_VAR_PREFIX);
	boolean thisGotLiveCT = FALSE, thisGotExpiredCT = FALSE, thisGotErrorCT = FALSE;
	char errMsg[4096];
	/* If the file doesn't exist, just remove the cart variable so it
	 * doesn't get copied from session to session.  If it does exist,
	 * leave it up to customFactoryTestExistence to parse the file for
	 * possible customTrash table references, some of which may exist
	 * and some not. */
	if (!fileExists(hel->val))
	    {
	    cartRemove(cart, hel->name);
	    thisGotExpiredCT = TRUE;
	    }
	else
	    {
	    /* protect against errAbort */
	    struct errCatch *errCatch = errCatchNew();
	    if (errCatchStart(errCatch))
		{
		customFactoryTestExistence(db, hel->val, &thisGotLiveCT, &thisGotExpiredCT, NULL);
		}
	    errCatchEnd(errCatch);
	    if (errCatch->gotError)  // tends to abort if db not found or hub not attached.
		{
		thisGotErrorCT = TRUE;
		safef(errMsg, sizeof errMsg, "%s {%s}", db, errCatch->message->string);
		}
	    errCatchFree(&errCatch);
	    }
	if (thisGotLiveCT)
	    slNameAddHead(&liveDbList, db);
	if (thisGotExpiredCT)
	    slNameAddHead(&expiredDbList, db);
	if (thisGotErrorCT)
	    slNameAddHead(&errorDbList, errMsg);
	gotLiveCT |= thisGotLiveCT;
	gotExpiredCT |= thisGotExpiredCT;
	gotErrorCT |= thisGotErrorCT;
	}
    if (gotLiveCT)
	{
	slSort(&liveDbList, slNameCmp);
	dyStringPrintf(dyMessage,
		       "<P>Note: the session has at least one active custom "
		       "track (in database ");
	for (sln = liveDbList;  sln != NULL;  sln = sln->next)
	    dyStringPrintf(dyMessage, "<A HREF=\"hgCustom?%s&db=%s\">%s</A>%s",
			   cartSidUrlString(cart), sln->name,
			   sln->name, (sln->next ? sln->next->next ? ", " : " and " : ""));
	dyStringAppend(dyMessage, "; click on the database link "
		       "to manage custom tracks).  ");

	}
    if (gotExpiredCT)
	{
	slSort(&expiredDbList, slNameCmp);
	dyStringPrintf(dyMessage,
		       "<P>Note: the session has at least one expired custom "
		       "track (in database ");
	for (sln = expiredDbList;  sln != NULL;  sln = sln->next)
	    dyStringPrintf(dyMessage, "%s%s",
			   sln->name, (sln->next ? sln->next->next ? ", " : " and " : ""));
	dyStringPrintf(dyMessage,
		       "), so it may not appear as originally intended.  ");
	}
    if (gotErrorCT)
	{
	slSort(&errorDbList, slNameCmp);
	dyStringPrintf(dyMessage,
		       "<P>Note: the session has at least one custom "
		       "track with errors (in database ");
	for (sln = errorDbList;  sln != NULL;  sln = sln->next)
	    dyStringPrintf(dyMessage, "%s%s",
			   sln->name, (sln->next ? sln->next->next ? ", " : " and " : ""));
	dyStringPrintf(dyMessage,
		       "), so it may not appear as originally intended.  ");
	}
    dyStringPrintf(dyMessage,
		   "These custom tracks should not expire, however, "
		   "the UCSC Genome Browser is not a data storage service; "
		   "<b>please keep a local backup of your sessions contents "
		   "and custom track data</b>. </P>");
    slNameFreeList(&liveDbList);
    slNameFreeList(&expiredDbList);
    }
/* Check for saved blat results (quasi custom track). */
char *ss = cartOptionalString(cart, "ss");
if (isNotEmpty(ss))
    {
    char buf[1024];
    char *words[2];
    int wordCount;
    boolean exists = FALSE;
    safecpy(buf, sizeof(buf), ss);
    wordCount = chopLine(buf, words);
    if (wordCount < 2)
	exists = FALSE;
    else
	exists = fileExists(words[0]) && fileExists(words[1]);

    if (exists)
	dyStringPrintf(dyMessage,
		       "<P>Note: the session contains BLAT results.  ");
    else
	dyStringPrintf(dyMessage,
		"<P>Note: the session contains an expired reference to "
		"previously saved BLAT results, so it may not appear as "
		"originally intended.  ");
    dyStringPrintf(dyMessage,
		   "BLAT results are subject to an "
		   "<A HREF=\"../goldenPath/help/hgSessionHelp.html#CTs\" TARGET=_BLANK>"
		   "expiration policy</A>.");
    }
}

char *cartGetPosition(struct cart *cart, char *database, struct cart **pLastDbPosCart)
/* get the current position in cart as a string chr:start-end.
 * This can handle the special CGI params 'default' and 'lastDbPos'
 * Returned value has to be freed. Returns 
 * default position of assembly is no position set in cart nor as CGI var.
 * Returns NULL if no position set anywhere and no default position.
 * For virtual modes, returns the type and extraState. 
*/
{
// position=lastDbPos in URL? -> go back to the last browsed position for this db
char *position = NULL;
char *defaultPosition = hDefaultPos(database);
struct cart *lastDbPosCart = cartOfNothing();
boolean gotCart = FALSE;
char dbPosKey[256];
safef(dbPosKey, sizeof(dbPosKey), "position.%s", database);
if (sameOk(cgiOptionalString("position"), "lastDbPos"))
    {
    char *dbLocalPosContent = cartUsualString(cart, dbPosKey, NULL);
    if (dbLocalPosContent)
	{
	if (strchr(dbLocalPosContent, '='))
	    {
	    gotCart = TRUE;
	    cartParseOverHash(lastDbPosCart, cloneString(dbLocalPosContent)); // this function chews up input
	    position = cloneString(cartUsualString(lastDbPosCart, "position", NULL));
	    }
	else
	    {
	    position = dbLocalPosContent;  // old style value
	    }
	}
    else
	{
	position = defaultPosition; // no value was set
	}
    }
    
if (position == NULL)
    {
    position = windowsToAscii(cloneString(cartUsualString(cart, "position", NULL)));
    }

/* default if not set at all, as would happen if it came from a URL with no
 * position. Otherwise tell them to go back to the gateway. Also recognize
 * "default" as specifying the default position. */
if (((position == NULL) || sameString(position, "default"))
    && (defaultPosition != NULL))
    position = cloneString(defaultPosition);

if (!gotCart)
    {
    cartSetBoolean(lastDbPosCart, "virtMode", FALSE);
    cartSetString(lastDbPosCart, "virtModeType", "default");
    cartSetString(lastDbPosCart, "lastVirtModeType", "default");
    cartSetString(lastDbPosCart, "position", position);
    cartSetString(lastDbPosCart, "nonVirtPosition", position);
    cartSetString(lastDbPosCart, "lastVirtModeExtra", "");
    }

if (pLastDbPosCart)
    *pLastDbPosCart = lastDbPosCart;

return position;
}

void cartSetDbPosition(struct cart *cart, char *database, struct cart *lastDbPosCart)
/* Set the 'position.db' variable in the cart.*/
{
char dbPosKey[256];
safef(dbPosKey, sizeof dbPosKey, "position.%s", database);
struct dyString *dbPosValue = newDyString(4096);
cartEncodeState(lastDbPosCart, dbPosValue);
cartSetString(cart, dbPosKey, dbPosValue->string);
}

void cartTdbFetchMinMaxPixels(struct cart *theCart, struct trackDb *tdb,
                                int defaultMin, int defaultMax, int defaultVal,
                                int *retMin, int *retMax, int *retDefault, int *retCurrent)
/* Configure maximum track height for variable height tracks (e.g. wiggle, barchart)
 *      Initial height and limits may be defined in trackDb with the maxHeightPixels string,
 *	Or user requested limits are defined in the cart. */
{
boolean parentLevel = isNameAtParentLevel(tdb, tdb->track);
char *heightPer = NULL; /*	string from cart	*/
int minHeightPixels = defaultMin;
int maxHeightPixels = defaultMax;
int defaultHeightPixels = defaultVal;
int defaultHeight;      /*      truncated by limits     */
char defaultDefault[16];
safef(defaultDefault, sizeof defaultDefault, "%d", defaultVal);
char *tdbDefault = cloneString(
    trackDbSettingClosestToHomeOrDefault(tdb, MAXHEIGHTPIXELS, defaultDefault));

if (sameWord(defaultDefault, tdbDefault))
    {
    struct hashEl *hel;
    /*	no maxHeightPixels from trackDb, maybe it is in tdb->settings
     *	(custom tracks keep settings here)
     */
    if ((tdb->settings != (char *)NULL) &&
	(tdb->settingsHash != (struct hash *)NULL))
	{
	if ((hel = hashLookup(tdb->settingsHash, MAXHEIGHTPIXELS)) != NULL)
	    {
	    freeMem(tdbDefault);
	    tdbDefault = cloneString((char *)hel->val);
	    }
	}
    }

/*      the maxHeightPixels string can be one, two, or three words
 *      separated by :
 *      All three would be:     max:default:min
 *      When only two:          max:default
 *      When only one:          max
 *      (this works too:        min:default:max)
 *      Where min is minimum allowed, default is initial default setting
 *      and max is the maximum allowed
 *	If it isn't available, these three have already been set
 *	in their declarations above
 */
if (differentWord(defaultDefault, tdbDefault))
    {
    char *words[3];
    char *sep = ":";
    int wordCount;
    wordCount=chopString(tdbDefault,sep,words,ArraySize(words));
    switch (wordCount)
	{
	case 3:
	    minHeightPixels = atoi(words[2]);
	    defaultHeightPixels = atoi(words[1]);
	    maxHeightPixels = atoi(words[0]);

            // flip max and min if min>max
            if (maxHeightPixels < minHeightPixels)
                {
                int pixels;
                pixels = maxHeightPixels;
                maxHeightPixels = minHeightPixels;
                minHeightPixels = pixels;
                }

	    if (defaultHeightPixels > maxHeightPixels)
		defaultHeightPixels = maxHeightPixels;
	    if (minHeightPixels > defaultHeightPixels)
		minHeightPixels = defaultHeightPixels;
	    break;
	case 2:
	    defaultHeightPixels = atoi(words[1]);
	    maxHeightPixels = atoi(words[0]);
	    if (defaultHeightPixels > maxHeightPixels)
		defaultHeightPixels = maxHeightPixels;
	    if (minHeightPixels > defaultHeightPixels)
		minHeightPixels = defaultHeightPixels;
	    break;
	case 1:
	    maxHeightPixels = atoi(words[0]);
	    defaultHeightPixels = maxHeightPixels;
	    if (minHeightPixels > defaultHeightPixels)
		minHeightPixels = defaultHeightPixels;
	    break;
	default:
	    break;
	}
    }
heightPer = cartOptionalStringClosestToHome(theCart, tdb, parentLevel, HEIGHTPER);
/*      Clip the cart value to range [minHeightPixels:maxHeightPixels] */
if (heightPer) defaultHeight = min( maxHeightPixels, atoi(heightPer));
else defaultHeight = defaultHeightPixels;
defaultHeight = max(minHeightPixels, defaultHeight);

*retMin = minHeightPixels;
*retMax = maxHeightPixels;
*retDefault = defaultHeightPixels;
*retCurrent = defaultHeight;

freeMem(tdbDefault);
} 

