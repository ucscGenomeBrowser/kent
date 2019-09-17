/* cart - stuff to manage variables that persist from
 * one invocation of a cgi script to another (variables
 * that are carted around).  */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef CART_H
#define CART_H

struct cart;         // forward definition for use in trackDb.h

#include "jksql.h"
#include "errAbort.h"
#include "dystring.h"
#include "linefile.h"
#include "trackDb.h"

// If cgi set as CART_VAR_EMPTY, then removed from cart
// If If cgi created new and oldVars are stored, then will be CART_VAR_EMPTY in old vars
#define CART_VAR_EMPTY "[]"
#define IS_CART_VAR_EMPTY(var) ((var) == NULL || sameString(var,CART_VAR_EMPTY))

typedef struct sqlConnection *(*DbConnector)();
/* funtion type used to get a connection to database */

typedef void (*DbDisconnect)(struct sqlConnection **pConn);
/* function type used to cleanup a connection from database */


struct cart
/* A cart of settings that persist. */
   {
   struct cart *next;	/* Next in list. */
   char *userId;	/* User ID in database. */
   char *sessionId;	/* Session ID in database. */
   struct hash *hash;	/* String valued hash. */
   struct hash *exclude;	/* Null valued hash of variables not to save. */
   struct cartDb *userInfo;	/* Info on user. */
   struct cartDb *sessionInfo;	/* Info on session. */
   };

INLINE char *_cartVarDbName(const char *db, const char *var)
/* generate cart variable name that is local to an assembly database.
 * Only for use inside of cart.h.  WARNING: static return */
{
static char buf[PATH_LEN]; // something rather big
safef(buf, sizeof(buf), "%s_%s", var, db);
return buf;
}

boolean cartTablesOk(struct sqlConnection *conn);
/* Return TRUE if cart tables are accessible (otherwise, the connection
 * doesn't do us any good). */

void cartParseOverHash(struct cart *cart, char *contents);
/* Parse cgi-style contents into a hash table.  This will *not*
 * replace existing members of hash that have same name, so we can
 * support multi-select form inputs (same var name can have multiple
 * values which will be in separate hashEl's). */

struct cart *cartNew(char *userId, char *sessionId,
	char **exclude, struct hash *oldVars);
/* Load up cart from user & session id's.  Exclude is a null-terminated list of
 * strings to not include. oldVars is an optional hash to put in values
 * that were just overwritten by cgi-variables. */

struct cart *cartOfNothing();
/* Create a new, empty, cart with no real connection to the database. */

struct cart *cartFromHash(struct hash *hash);
/* Create a cart from hash */

struct cart *cartFromCgiOnly(char *userId, char *sessionId,
	char **exclude, struct hash *oldVars);
/* Create a new cart that contains only CGI variables, nothing from the
 * database, and no way to write back to database either. */

void cartCheckout(struct cart **pCart);
/* Save cart to database and free it up. */

void cartSaveState(struct cart *cart);
/* Free up cart and save it to database.
 * Intended for updating cart before background CGI runs.
 * Use cartCheckout() instead. */

void cartEncodeState(struct cart *cart, struct dyString *dy);
/* Add a CGI-encoded var=val&... string of all cart variables to dy. */

char *cartSessionVarName();
/* Return name of CGI session ID variable. */

char *cartSessionId(struct cart *cart);
/* Return session id. */

unsigned cartSessionRawId(struct cart *cart);
/* Return raw session id without security key. */

unsigned cartUserRawId(struct cart *cart);
/* Return raw user id without security key. */

char *cartSidUrlString(struct cart *cart);
/* Return session id string as in hgsid=N . */

char *cartUserId(struct cart *cart);
/* Return session id. */

void cartRemove(struct cart *cart, char *var);
/* Remove variable from cart. */

void cartRemoveExcept(struct cart *cart, char **except);
/* Remove variables except those in null terminated except array
 * from cart.  Except array may be NULL in which case all
 * are removed. */

struct slPair *cartVarsLike(struct cart *cart, char *wildCard);
/* Return a slPair list of cart vars that match the wildcard */

struct slPair *cartVarsWithPrefix(struct cart *cart, char *prefix);
/* Return a slPair list of cart vars that begin with prefix */

struct slPair *cartVarsWithPrefixLm(struct cart *cart, char *prefix, struct lm *lm);
/* Return list of cart vars that begin with prefix allocated in local memory.
 * Quite a lot faster than cartVarsWithPrefix. */

void cartRemoveLike(struct cart *cart, char *wildCard);
/* Remove all variable from cart that match wildCard. */

void cartRemovePrefix(struct cart *cart, char *prefix);
/* Remove variables with given prefix from cart. */

boolean cartVarExists(struct cart *cart, char *var);
/* Return TRUE if variable is in cart. */

boolean cartListVarExists(struct cart *cart, char *var);
/* Return TRUE if a list variable is in cart (list may still be empty). */

INLINE boolean cartVarExistsDb(struct cart *cart, char *db, char *var)
/* Return TRUE if variable_$db is in cart. */
{
return cartVarExists(cart, _cartVarDbName(db, var));
}

char *cartString(struct cart *cart, char *var);
/* Return string valued cart variable. */

INLINE char *cartStringDb(struct cart *cart, char *db, char *var)
/* Return string valued cart var_$db. */
{
return cartString(cart, _cartVarDbName(db, var));
}

char *cartOptionalString(struct cart *cart, char *var);
/* Return string valued cart variable or NULL if it doesn't exist. */

INLINE char *cartOptionalStringDb(struct cart *cart, char *db, char *var)
/* Return string valued cart variable_$db or NULL if it doesn't exist. */
{
return cartOptionalString(cart, _cartVarDbName(db, var));
}

char *cartNonemptyString(struct cart *cart, char *name);
/* Return string value associated with name.  Return NULL
 * if value doesn't exist or if it is pure white space. */

INLINE char *cartNonemptyStringDb(struct cart *cart, char *db, char *name)
/* Return string value associated with name_$db.  Return NULL
 * if value doesn't exist or if it is pure white space. */
{
return cartNonemptyString(cart, _cartVarDbName(db, name));
}

char *cartUsualString(struct cart *cart, char *var, char *usual);
/* Return variable value if it exists or usual if not. */

INLINE char *cartUsualStringDb(struct cart *cart, char *db, char *var, char *usual)
/* Return var_$db value if it exists or usual if not. */
{
return cartUsualString(cart, _cartVarDbName(db, var), usual);
}

char *cartCgiUsualString(struct cart *cart, char *var, char *usual);
/* Look for var in CGI, then in cart, if not found then return usual. */

struct slName *cartOptionalSlNameList(struct cart *cart, char *var);
/* Return slName list (possibly with multiple values for the same var) or
 * NULL if not found. */

struct hash *cartHashList(struct cart *cart, char *var);
/* Return hash with multiple values for the same var or NULL if not found. */

void cartAddString(struct cart *cart, char *var, char *val);
/* Add string valued cart variable (if called multiple times on same var,
 * will create a list -- retrieve with cartOptionalSlNameList. */

void cartSetString(struct cart *cart, char *var, char *val);
/* Set string valued cart variable. */

INLINE void cartSetStringDb(struct cart *cart, char *db, char *var, char *val)
/* Set string valued cart var_$db. */
{
cartSetString(cart, _cartVarDbName(db, var), val);
}

int cartInt(struct cart *cart, char *var);
/* Return int valued variable. */

INLINE int cartIntDb(struct cart *cart, char *db, char *var)
/* Return int valued variable_$db. */
{
return cartInt(cart, _cartVarDbName(db, var));
}


int cartIntExp(struct cart *cart, char *var);
/* Return integer valued expression in variable. */

int cartUsualInt(struct cart *cart, char *var, int usual);
/* Return variable value if it exists or usual if not. */

INLINE int cartUsualIntDb(struct cart *cart, char *db, char *var, int usual)
/* Return variable_$db value if it exists or usual if not. */
{
return cartUsualInt(cart, _cartVarDbName(db, var), usual);
}

int cartUsualIntClipped(struct cart *cart, char *var, int usual,
	int minVal, int maxVal);
/* Return integer variable clipped to lie between minVal/maxVal */

int cartCgiUsualInt(struct cart *cart, char *var, int usual);
/* Look for var in CGI, then in cart, if not found then return usual. */

void cartSetInt(struct cart *cart, char *var, int val);
/* Set integer value. */

INLINE void cartSetIntDb(struct cart *cart, char *db, char *var, int val)
/* Set integer value for var_$db. */
{
cartSetInt(cart, _cartVarDbName(db, var), val);
}

double cartDouble(struct cart *cart, char *var);
/* Return double valued variable. */

INLINE double cartDoubleDb(struct cart *cart, char *db, char *var)
/* Return double valued var_$db. */
{
return cartDouble(cart, _cartVarDbName(db, var));
}

double cartUsualDouble(struct cart *cart, char *var, double usual);
/* Return variable value if it exists or usual if not. */

INLINE double cartUsualDoubleDb(struct cart *cart, char *db, char *var, double usual)
/* Return var_$db value if it exists or usual if not. */
{
return cartUsualDouble(cart, _cartVarDbName(db, var), usual);
}

double cartCgiUsualDouble(struct cart *cart, char *var, double usual);
/* Look for var in CGI, then in cart, if not found then return usual. */

void cartSetDouble(struct cart *cart, char *var, double val);
/* Set double value. */

INLINE void cartSetDoubleDb(struct cart *cart, char *db, char *var, double val)
/* Set double value for var_$db. */
{
cartSetDouble(cart, _cartVarDbName(db, var), val);
}

boolean cartBoolean(struct cart *cart, char *var);
/* Retrieve cart boolean. */

INLINE boolean cartBooleanDb(struct cart *cart, char *db, char *var)
/* Retrieve cart boolean for var_$db. */
{
return cartBoolean(cart, _cartVarDbName(db, var));
}

boolean cartUsualBoolean(struct cart *cart, char *var, boolean usual);
/* Return variable value if it exists or usual if not.  */

INLINE boolean cartUsualBooleanDb(struct cart *cart, char *db, char *var, boolean usual)
/* Return var_$db value if it exists or usual if not.  */
{
return cartUsualBoolean(cart, _cartVarDbName(db, var), usual);
}

boolean cartCgiUsualBoolean(struct cart *cart, char *var, boolean usual);
/* Look for var in CGI, then in cart, if not found then return usual. */

void cartSetBoolean(struct cart *cart, char *var, boolean val);
/* Set boolean value. */

INLINE void cartSetBooleanDb(struct cart *cart, char *db, char *var, boolean val)
/* Set boolean value for $var_db. */
{
cartSetBoolean(cart, _cartVarDbName(db, var), val);
}

void cartMakeTextVar(struct cart *cart, char *var, char *defaultVal, int charSize);
/* Make a text control filled with value from cart if it exists or
 * default value otherwise.  If charSize is zero it's calculated to fit
 * current value.  Default value may be NULL. */

void cartMakeIntVar(struct cart *cart, char *var, int defaultVal, int maxDigits);
/* Make a text control filled with integer value - from cart if available
 * otherwise default.  */

void cartMakeDoubleVar(struct cart *cart, char *var, double defaultVal, int maxDigits);
/* Make a text control filled with integer value - from cart if available
 * otherwise default.  */

void cartMakeCheckBox(struct cart *cart, char *var, boolean defaultVal);
/* Make a check box filled with value from cart if it exists or
 * default value otherwise.  */

void cartMakeRadioButton(struct cart *cart, char *var, char *val, char *defaultVal);
/* Make a radio button that is selected if cart variable exists and matches
 * value (or value matches default val if cart var doesn't exist). */

void cartSaveSession(struct cart *cart);
/* Save session in a hidden variable. This needs to be called
 * somewhere inside of form or bad things will happen. */

void cartDump(struct cart *cart);
/* Dump contents of cart with anti-XSS HTML encoding. */

void cartDumpHgSession(struct cart *cart);
/* Dump contents of cart with escaped newlines for hgSession output files.
 * Cart variable "cartDumpAsTable" is ignored. */

#define CART_DUMP_AS_TABLE "cartDumpAsTable"

void cartDumpPrefix(struct cart *cart, char *prefix);
/* Dump all cart variables with prefix */

void cartDumpLike(struct cart *cart, char *wildcard);
/* Dump all cart variables matching wildcard */

struct hashEl *cartFindPrefix(struct cart *cart, char *prefix);
/* Return list of name/val pairs from cart where name starts with
 * prefix.  Free when done with hashElFreeList. */

struct hashEl *cartFindLike(struct cart *cart, char *wildCard);
/* Return list of name/val pairs from cart where name matches
 * wildcard.  Free when done with hashElFreeList. */

char *cartFindFirstLike(struct cart *cart, char *wildCard);
/* Find name of first variable that matches wildCard in cart.
 * Return NULL if none. */

void cartResetInDb(char *cookieName);
/* Clear cart in database. */

void cartEarlyWarningHandler(char *format, va_list args);
/* Write an error message so user can see it before page is really started */

void cartWarnCatcher(void (*doMiddle)(struct cart *cart), struct cart *cart, WarnHandler warner);
/* Wrap error and warning handlers around doMiddl. */

void cartEmptyShell(void (*doMiddle)(struct cart *cart), char *cookieName,
	char **exclude, struct hash *oldVars);
/* Get cart and cookies and set up error handling, but don't start writing any
 * html yet. The doMiddleFunction has to call cartHtmlStart(title), and
 * cartHtmlEnd(), as well as writing the body of the HTML.
 * oldVars - those in cart that are overlayed by cgi-vars are
 * put in optional hash oldVars. */

void cartEmptyShellNoContent(void (*doMiddle)(struct cart *cart), char *cookieName,
                             char **exclude, struct hash *oldVars);
/* Get cart and cookies and set up error handling.
 * The doMiddle function must write the Content-Type header line.
 * oldVars - those in cart that are overlayed by cgi-vars are
 * put in optional hash oldVars. */

void cartHtmlStart(char *title);
/* Write HTML header and put in normal error handler. Needed with cartEmptyShell,
 * but not cartHtmlShell. */

void cartFooter(void);
/* Write out HTML footer, possibly with googleAnalytics too */

void cartHtmlEnd();
/* Write out HTML footer and get rid or error handler. Needed with cartEmptyShell,
 * but not cartHtmlShell. */

void cartWebStart(struct cart *theCart, char *db, char *format, ...)
/* Print out pretty wrapper around things when working
 * from cart. Balance this with cartWebEnd. */
#if defined(__GNUC__)
__attribute__((format(printf, 3, 4)))
#endif
;

void cartVaWebStart(struct cart *cart, char *db, char *format, va_list args);
/* Print out pretty wrapper around things when working
 * from cart. */

void cartWebStartHeader(struct cart *cart, char *db, char *format, ...);
/* Print out Content-type header and then pretty wrapper around things when working
 * from cart. */

void cartWebEnd();
/* End out pretty wrapper around things when working
 * from cart. */

void cartHtmlShellWithHead(char *head, char *title, void (*doMiddle)(struct cart *cart),
        char *cookieName, char **exclude, struct hash *oldVars);
/* Load cart from cookie and session cgi variable.  Write web-page
 * preamble including head and title, call doMiddle with cart, and write end of web-page.
 * Exclude may be NULL.  If it exists it's a comma-separated list of
 * variables that you don't want to save in the cart between
 * invocations of the cgi-script. */

void cartHtmlShell(char *title, void (*doMiddle)(struct cart *cart),
	char *cookieName, char **exclude, struct hash *oldVars);
/* Load cart from cookie and session cgi variable.  Write web-page preamble, call doMiddle
 * with cart, and write end of web-page.   Exclude may be NULL.  If it exists it's a
 * comma-separated list of variables that you don't want to save in the cart between
 * invocations of the cgi-script. oldVars is an optional hash that will get values
 * of things in the cart that were overwritten by cgi-variables. */

void cartWriteCookie(struct cart *cart, char *cookieName);
/* Write out HTTP Set-Cookie statement for cart. */

struct cart *cartAndCookie(char *cookieName, char **exclude,
	struct hash *oldVars);
/* Load cart from cookie and session cgi variable.  Write cookie and
 * content-type part HTTP preamble to web page.  Don't write any HTML though. */

struct cart *cartAndCookieNoContent(char *cookieName, char **exclude,
	struct hash *oldVars);
/* Load cart from cookie and session cgi variable. Don't write out
 * content type or any HTML. */

struct cart *cartAndCookieWithHtml(char *cookieName, char **exclude,
	struct hash *oldVars, boolean doContentType);
/* Load cart from cookie and session cgi variable.  Write cookie
 * and optionally content-type part HTTP preamble to web page.  Don't
 * write any HTML though. */

struct cart *cartForSession(char *cookieName, char **exclude,
	struct hash *oldVars);
/* This gets the cart without writing any HTTP lines at all to stdout. */

void cartSetDbConnector(DbConnector connector);
/* Set the connector that will be used by the cart to connect to the
 * database. Default connector is hConnectCart */

void cartSetDbDisconnector(DbDisconnect disconnector);
/* Set the connector that will be used by the cart to disconnect from the
 * database. Default disconnector is hDisconnectCart */


/* Libified constants and code that originally belonged only to hgSession
 * (now hgTracks uses them too), plus a couple for DataTables in hgSession
 * and hgPublicSessions: */
#define hgSessionPrefix "hgS_"
#define hgPublicSessionsPrefix "hgPS_"
#define dataTableStateName "DataTableState"
#define hgSessionTableState hgSessionPrefix dataTableStateName
#define hgPublicSessionsTableState hgPublicSessionsPrefix dataTableStateName

#define hgsOtherUserName hgSessionPrefix "otherUserName"
#define hgsOtherUserSessionName hgSessionPrefix "otherUserSessionName"
#define hgsDoOtherUser hgSessionPrefix "doOtherUser"

#define hgsLoadUrlName hgSessionPrefix "loadUrlName"
#define hgsDoLoadUrl hgSessionPrefix "doLoadUrl"

#define namedSessionTable "namedSessionDb"

void sessionTouchLastUse(struct sqlConnection *conn, char *encUserName,
			 char *encSessionName);
/* Increment namedSessionDb.useCount and update lastUse for this session. */

void cartLoadUserSession(struct sqlConnection *conn, char *sessionOwner,
			 char *sessionName, struct cart *cart,
			 struct hash *oldVars, char *actionVar);
/* If permitted, load the contents of the given user's session, and then
 * reload the CGI settings (to support override of session settings).
 * If non-NULL, oldVars will contain values overloaded when reloading CGI.
 * If non-NULL, actionVar is a cartRemove wildcard string specifying the
 * CGI action variable that sent us here. */

boolean cartLoadSettingsFromUserInput(struct lineFile *lf, struct cart *cart, struct hash *oldVars,
                                      char *actionVar, struct dyString *dyMessage);
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

char *cartGetOrderFromFile(char *genomeDb, struct cart *cart, char *speciesUseFile);
/* Look in a cart variable that holds the filename that has a list of
 * species to show in a maf file */

char *cartGetOrderFromFileAndMsaTable(char *genomeDb, struct cart *cart, char *speciesUseFile, char *msaTable);
/* Look in a cart variable that holds the filename that has a list of
 * species to show in a maf file */

char *cartLookUpVariableClosestToHome(struct cart *cart, struct trackDb *tdb,
                                      boolean parentLevel, char *suffix,char **pVariable);
/* Returns value or NULL for a cart variable from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix
   Optionally fills the non NULL pVariable with the actual name of the variable in the cart */
#define cartOptionalStringClosestToHome(cart,tdb,parentLevel,suffix) \
        cartLookUpVariableClosestToHome((cart),(tdb),(parentLevel),(suffix),NULL)

void cartRemoveVariableClosestToHome(struct cart *cart, struct trackDb *tdb,
                                     boolean parentLevel, char *suffix);
/* Looks for then removes a cart variable from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */

char *cartStringClosestToHome(struct cart *cart, struct trackDb *tdb,
                              boolean parentLevel, char *suffix);
/* Returns value or Aborts for a cart string from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */

boolean cartVarExistsAnyLevel(struct cart *cart, struct trackDb *tdb,
                              boolean parentLevel, char *suffix);
/* Returns TRUE if variable exists anywhere, looking from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */

boolean cartListVarExistsAnyLevel(struct cart *cart, struct trackDb *tdb,
				  boolean parentLevel, char *suffix);
/* Return TRUE if a list variable for tdb->track (or tdb->parent->track,
 * or tdb->parent->parent->track, etc.) is in cart (list itself may be NULL). */

char *cartUsualStringClosestToHome(struct cart *cart, struct trackDb *tdb, boolean parentLevel,
                                   char *suffix, char *usual);
/* Returns value or {usual} for a cart string from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */

boolean cartBooleanClosestToHome(struct cart *cart, struct trackDb *tdb,
                                 boolean parentLevel, char *suffix);
/* Returns value or Aborts for a cart boolean ('on' or != 0) from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */

boolean cartUsualBooleanClosestToHome(struct cart *cart, struct trackDb *tdb,
                                      boolean parentLevel, char *suffix,boolean usual);
/* Returns value or {usual} for a cart boolean ('on' or != 0) from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */

int cartUsualIntClosestToHome(struct cart *cart, struct trackDb *tdb,
                              boolean parentLevel, char *suffix, int usual);
/* Returns value or {usual} for a cart int from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */

double cartUsualDoubleClosestToHome(struct cart *cart, struct trackDb *tdb,
                                    boolean parentLevel, char *suffix, double usual);
/* Returns value or {usual} for a cart fp double from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */

struct slName *cartOptionalSlNameListClosestToHome(struct cart *cart, struct trackDb *tdb,
                                                   boolean parentLevel, char *suffix);
/* Return slName list (possibly with multiple values for the same var) from lowest level on up:
   subtrackName.suffix, then compositeName.view.suffix, then compositeName.suffix */

void cartRemoveAllForTdb(struct cart *cart, struct trackDb *tdb);
/* Remove all variables from cart that are associated with this tdb. */

void cartRemoveAllForTdbAndChildren(struct cart *cart, struct trackDb *tdb);
/* Remove all variables from cart that are associated
   with this tdb and it's children. */

char *cartOrTdbString(struct cart *cart, struct trackDb *tdb, char *var, char *defaultVal);
/* Look first in cart, then in trackDb for var.  Return defaultVal if not found. */

int cartOrTdbInt(struct cart *cart, struct trackDb *tdb, char *var, int defaultVal);
/* Look first in cart, then in trackDb for var.  Return defaultVal if not found. */

double cartOrTdbDouble(struct cart *cart, struct trackDb *tdb, char *var, double defaultVal);
/* Look first in cart, then in trackDb for var.  Return defaultVal if not found. */

boolean cartOrTdbBoolean(struct cart *cart, struct trackDb *tdb, char *var, boolean defaultVal);
/* Look first in cart, then in trackDb for var.  Return defaultVal if not found. */

boolean cartValueHasChanged(struct cart *newCart,struct hash *oldVars,char *setting,
                            boolean ignoreRemoved,boolean ignoreCreated);
/* Returns TRUE if new cart setting has changed from old cart setting */

int cartRemoveFromTdbTree(struct cart *cart,struct trackDb *tdb,char *suffix,boolean skipParent);
/* Removes a 'trackName.suffix' from all tdb descendents (but not parent).
   If suffix NULL then removes 'trackName' which holds visibility */

boolean cartTdbTreeReshapeIfNeeded(struct cart *cart,struct trackDb *tdbComposite);
/* When subtrack vis is set via findTracks, and composite has no cart settings,
   then fashion composite to match found */

boolean cartTdbTreeCleanupOverrides(struct trackDb *tdb,struct cart *newCart,struct hash *oldVars, struct lm *lm);
/* When composite/view settings changes, remove subtrack specific settings
   Returns TRUE if any cart vars are removed */

void cartCopyCustomComposites(struct cart *cart);
/* Find any custom composite hubs and copy them so they can be modified. */

void cartReplaceHubVars(struct cart *cart, char *hubFileVar, char *oldHubUrl, char *newHubUrl);
/* Replace all cart variables corresponding to oldHubUrl (and/or its hub ID) with
 * equivalents for newHubUrl. */

void cgiExitTime(char *cgiName, long enteredMainTime);
/* single stderr print out called at end of CGI binaries to record run
 * time in apache error_log */

void cartHubWarn(char *format, va_list args);
/* save up hub related warnings to put out later */

void cartFlushHubWarnings();
/* flush the hub errors (if any) */

void cartCheckForCustomTracks(struct cart *cart, struct dyString *dyMessage);
/* Scan cart for ctfile_<db> variables.  Tally up the databases that have
 * live custom tracks and those that have expired custom tracks. */
/* While we're at it, also look for saved blat results. */

#define CART_HAS_DEFAULT_VISIBILITY "defaultsSet"

extern void cartHideDefaultTracks(struct cart *cart);
/* Hide all the tracks who have default visibilities in trackDb
 * that are something other than hide.  Do this only if the
 * variable CART_HAS_DEFAULT_VISIBILITY is set in the cart.  */

char *cartGetPosition(struct cart *cart, char *database, struct cart **pLastDbPosCart);
/* get the current position in cart as a string chr:start-end.
 * This can handle the special CGI params 'default' and 'lastDbPos'
 * Returned value has to be freed. Returns default position of assembly 
 * if no position set in cart nor as CGI var.
 * For virtual modes, returns the type and extraState. 
*/

void cartSetDbPosition(struct cart *cart, char *database, struct cart *lastDbPosCart);
/* Set the 'position.db' variable in the cart.*/

void cartSetLastPosition(struct cart *cart, char *position, struct hash *oldVars);
/* If position and oldVars are non-NULL, and oldVars' position is different, add it to the cart
 * as lastPosition.  This is called by cartHtmlShell{,WithHead} but not other cart openers;
 * it should be called after cartGetPosition or equivalent. */

void cartTdbFetchMinMaxPixels(struct cart *theCart, struct trackDb *tdb,
                                int defaultMin, int defaultMax, int defaultDefault,
                                int *retMin, int *retMax, int *retDefault, int *retCurrent);
/* Configure maximum track height for variable height tracks (e.g. wiggle, barchart)
 *      Initial height and limits may be defined in trackDb with the maxHeightPixels string,
 *      Or user requested limits are defined in the cart. */

#endif /* CART_H */

