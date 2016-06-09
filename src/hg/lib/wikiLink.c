/* wikiLink - interoperate with a wiki site (share user identities). */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "htmshell.h"
#include "autoUpgrade.h"
#include "cartDb.h"
#include "cheapcgi.h"
#include "hgConfig.h"
#include "hui.h"
#include "web.h"
#include "wikiLink.h"

// Flag to indicate that loginValidateCookies has been called:
static boolean alreadyAuthenticated = FALSE;
// Set by loginValidateCookies, used by wikiLinkUserName
static boolean authenticated = FALSE;
// User name from remote login cookies
static char *remoteUserName = NULL;
// If we need to change some cookies, store cookie strings here in case loginValidateCookies
// is called multiple times (e.g. validate before cookie-writing, then later write cookies)
static struct slName *cookieStrings = NULL;

char *loginSystemName()
/* Return the wiki host specified in hg.conf, or NULL.  Allocd here. */
{
return cloneString(cfgOption(CFG_LOGIN_SYSTEM_NAME));
}

boolean loginSystemEnabled()
/* Return TRUE if login.systemName  parameter is defined in hg.conf . */
{
#ifdef USE_SSL
return (cfgOption(CFG_LOGIN_SYSTEM_NAME) != NULL);
#else
return FALSE;
#endif
}

boolean wikiLinkEnabled()
/* Return TRUE if all wiki.* parameters are defined in hg.conf . */
{
return ((cfgOption(CFG_WIKI_HOST) != NULL) &&
	(cfgOption(CFG_WIKI_USER_NAME_COOKIE) != NULL) &&
	(cfgOption(CFG_WIKI_LOGGED_IN_COOKIE) != NULL));
}

static char *wikiLinkLoggedInCookie()
/* Return the cookie name specified in hg.conf as the wiki logged-in cookie. */
{
return cfgOption(CFG_WIKI_LOGGED_IN_COOKIE);
}

static char *wikiLinkUserNameCookie()
/* Return the cookie name specified in hg.conf as the wiki user name cookie. */
{
return cfgOption(CFG_WIKI_USER_NAME_COOKIE);
}

static char *loginIdKeyCookie()
/* Return the name of the login system <id>_<key> cookie.  Do not free result. */
{
static char defaultCookie[512];
char *cookie = cfgOption(CFG_LOGIN_IDKEY_COOKIE);
if (isEmpty(cookie))
    {
    char *centralDbPrefix = cfgOptionDefault(CFG_CENTRAL_COOKIE, "central");
    safef(defaultCookie, sizeof(defaultCookie), "%s.hgLoginToken", centralDbPrefix);
    cookie = defaultCookie;
    }
return cookie;
}

static char *loginUserNameCookie()
/* Return the name of the login system user name cookie.  Do not free result. */
{
static char defaultCookie[512];
char *cookie = cfgOption(CFG_LOGIN_USER_NAME_COOKIE);
if (isEmpty(cookie))
    {
    char *centralDbPrefix = cfgOptionDefault(CFG_CENTRAL_COOKIE, "central");
    safef(defaultCookie, sizeof(defaultCookie), "%s.hgLoginUserName", centralDbPrefix);
    cookie = defaultCookie;
    }
return cookie;
}

static uint getCookieIdKey(char **retKey
                         , boolean *retReplaceOld                 // TODO: remove in July 2016
                           )
/* The cookie may have either just a number <idx> or <idx>_<key>.  Return idx;
 * key defaults to NULL, and is placed in retKey if retKey is non-NULL. */
{
uint idx = 0;
char *key = NULL;
char *cookieIdKeyStr = findCookieData(loginIdKeyCookie());

// BEGIN TODO: remove in July 2016
// If login cookies are not set, but wiki cookies are, accept their values but replace them later
if (isEmpty(cookieIdKeyStr) && wikiLinkEnabled())
    {
    cookieIdKeyStr = findCookieData(wikiLinkLoggedInCookie());
    if (retReplaceOld && isNotEmpty(cookieIdKeyStr))
        *retReplaceOld = TRUE;
    }
// END TODO: remove in July 2016

if (isNotEmpty(cookieIdKeyStr))
    {
    char copy[strlen(cookieIdKeyStr)+1];
    safecpy(copy, sizeof(copy), cookieIdKeyStr);
    char *p = strchr(copy, '_');
    if (p != NULL)
        *p++ = '\0';
    idx = (uint)atoll(copy);
    key = (p) ? cloneString(p) : NULL;
    }
if (retKey)
    *retKey = key;
return idx;
}

static uint getMemberIdx(struct sqlConnection *conn, char *userName)
/* Return userName's idx value in gbMembers.  Return 0 if not found. */
{
char query[512];
sqlSafef(query, sizeof(query), "select idx from gbMembers where userName='%s'", userName);
return (uint)sqlQuickLongLong(conn, query);
}

static boolean haveKeyList(struct sqlConnection *conn)
/* Return true if gbMembers has column keyList. */
{
if (sqlColumnExists(conn, "gbMembers", "keyList"))
    return TRUE;
else
    {
    autoUpgradeTableAddColumn(conn, "gbMembers", "keyList", "longblob", FALSE, "NULL");
    return sqlColumnExists(conn, "gbMembers", "keyList");
    }
}

static boolean isValidKey(struct sqlConnection *conn, uint idx, char *userName, char *key)
/* Return TRUE if gbMembers has a row that maps idx to userName and key. */
{
boolean isValid = FALSE;
char query[512];
sqlSafef(query, sizeof(query), "select userName, keyList from gbMembers where idx = %u", idx);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
if ((row = sqlNextRow(sr)) != NULL)
    {
    if (sameString(row[0], userName))
        {
        struct slName *validKeyList = slNameListFromString(row[1], ',');
        isValid = slNameInListUseCase(validKeyList, key);
        }
    }
sqlFreeResult(&sr);
return isValid;
}

static void deleteKey(struct sqlConnection *conn, uint idx, char *key)
/* Remove key from idx row's comma-separated keyList. */
{
char query[2048];
sqlSafef(query, sizeof(query), "select keyList from gbMembers where idx = %u", idx);
char buf[1024];
char *keyListStr = sqlQuickQuery(conn, query, buf, sizeof(buf));
if (isNotEmpty(keyListStr))
    {
    struct slName *keyList = slNameListFromString(keyListStr, ',');
    struct slName *keyToDelete = slNameFind(keyList, key);
    if (keyToDelete)
        {
        slRemoveEl(&keyList, keyToDelete);
        char *newListStr = slNameListToString(keyList, ',');
        sqlSafef(query, sizeof(query), "update gbMembers set keyList='%s' where idx = %u",
                 newListStr, idx);
        sqlUpdate(conn, query);
        }
    }
}

static void insertKey(struct sqlConnection *conn, uint idx, char *key)
/* Add a new entry to gbMembers.keyList for idx. */
{
char query[2048];
sqlSafef(query, sizeof(query), "select keyList from gbMembers where idx = %u", idx);
char buf[1024];
char *keyListStr = sqlQuickQuery(conn, query, buf, sizeof(buf));
if (isNotEmpty(keyListStr))
    sqlSafef(query, sizeof(query), "update gbMembers set keyList='%s,%s' where idx = %u",
             key, keyListStr, idx);
else
    sqlSafef(query, sizeof(query), "update gbMembers set keyList='%s' where idx = %u", key, idx);
sqlUpdate(conn, query);
}

char *getCookieDomainString()
/* Get a string that will look something like " domain=.ucsc.edu;" if central.domain
 * is defined, otherwise just "".  Don't free result. */
{
static char domainString[256];
char *domain = cloneString(cfgOption(CFG_CENTRAL_DOMAIN));
if (domain != NULL && strchr(domain, '.') != NULL)
    safef(domainString, sizeof(domainString), " domain=%s;", domain);
else
    domainString[0] = '\0';
return domainString;
}

#define NO_EXPIRE_COOKIE_DATE "Thu, 31-Dec-2037 23:59:59 GMT"
#define EXPIRED_COOKIE_DATE "Thu, 01-Jan-1970 00:00:00 GMT"

struct slName *newCookieString(char *name, char *value)
/* Return a cookie string that sets cookie to value if non-empty and
 * deletes/invalidates the cookie if value is empty or NULL. */
{
char *domain = getCookieDomainString();
char cookieString[2048];
if (isNotEmpty(value))
    // Set the cookie to value
    safef(cookieString, sizeof(cookieString), "%s=%s;%s path=/; expires="NO_EXPIRE_COOKIE_DATE,
          name, value, domain);
else
    // Invalidate the cookie
    safef(cookieString, sizeof(cookieString), "%s=;%s path=/; expires="EXPIRED_COOKIE_DATE,
          name, domain);
return slNameNew(cookieString);
}

static struct slName *loginUserNameCookieString(char *userName)
/* Return a cookie string that sets userName cookie to userName if non-empty and
 * deletes/invalidates the cookie if empty/NULL. */
{
return newCookieString(loginUserNameCookie(), cgiEncodeFull(userName));
}

static struct slName *loginIdKeyCookieString(uint idx, char *key)
/* Return a cookie string that sets ID cookie to idKey if idKey is non-empty and
 * deletes/invalidates the cookie if empty/NULL. */
{
char newVal[1024];
if (isNotEmpty(key))
    safef(newVal, sizeof(newVal), "%u_%s", idx, key);
else
    safef(newVal, sizeof(newVal), "%u", idx);
return newCookieString(loginIdKeyCookie(), idx ? newVal : NULL);
}

static char *makeNewKey()
/* Return a new random key using the same number of bits that we use for
 * {userDb,sessionDb}.sessionKey */
{
// at least 128 bits of protection, 33 for the world population size.
int numBits = 128 + 33;
return cartDbMakeRandomKey(numBits);
}

struct slName *loginLoginUser(char *userName, uint idx)
/* Return cookie strings to set for user so we'll recognize that user is logged in.
 * Call this after validating userName's password. */
{
alreadyAuthenticated = TRUE;
authenticated = TRUE;
char *key = NULL;
struct sqlConnection *conn = hConnectCentral();
if (haveKeyList(conn))
    {
    key = makeNewKey();
    insertKey(conn, idx, key);
    }
hDisconnectCentral(&conn);
slAddHead(&cookieStrings, loginIdKeyCookieString(idx, key));
slAddHead(&cookieStrings, loginUserNameCookieString(userName));
return cookieStrings;
}

struct slName *loginLogoutUser()
/* Return cookie strings to set (deleting the login cookies). */
{
alreadyAuthenticated = TRUE;
authenticated = FALSE;
cookieStrings = NULL;
char *key = NULL;
uint idx = getCookieIdKey(&key, NULL);
struct sqlConnection *conn = hConnectCentral();
if (haveKeyList(conn) && key)
    deleteKey(conn, idx, key);
hDisconnectCentral(&conn);
slAddHead(&cookieStrings, loginIdKeyCookieString(0, NULL));
slAddHead(&cookieStrings, loginUserNameCookieString(NULL));
// BEGIN TODO: remove in July 2016
if (wikiLinkEnabled())
    {
    slAddHead(&cookieStrings, newCookieString(wikiLinkLoggedInCookie(), NULL));
    slAddHead(&cookieStrings, newCookieString(wikiLinkUserNameCookie(), NULL));
    }
// END TODO: remove in July 2016
return cookieStrings;
}

static char *getRemoteCookiePrefix(char *wikiHost)
/* Try to guess what cookie prefix wikiHost will use, to tide us over for release 333.
 * It's better to set hg.conf's login.tokenCookie and login.userNameCookie than to rely on this. */
{
if (sameString("genome.ucsc.edu", wikiHost))
    return "hguid";
if (startsWith("hgwbeta.", wikiHost))
    return "hguid.hgwbeta";
if (startsWith("genome-test.", wikiHost) || startsWith("hgwdev.", wikiHost))
    return "hguid.genome-test";
return NULL;
}

static char *getRemoteCookieVal(char *cfgCookieName, char *remoteCookiePrefix, char *suffix)
/* Return the value of the remote login cookie.  If the cookie name is not specified in hg.conf,
 * make a guess to tide us over for release 333.  Do not free result. */
{
char *cookie = cfgOption(cfgCookieName);
if (isNotEmpty(cookie))
    return findCookieData(cookie);
if (remoteCookiePrefix)
    {
    char defaultCookie[512];
    safef(defaultCookie, sizeof(defaultCookie), "%s.%s", remoteCookiePrefix, suffix);
    return findCookieData(defaultCookie);
    }
return NULL;
}

static void remoteHostBypass()
/* If a remote host was used for login, redirecting back to this server, then this server's
 * central database does not have the correct login token -- so we can't validate cookies.
 * Fall back on the old method of just accepting whatever cookies we get.  Unfortunately
 * we'll have to take a guess at what those cookies are, although hg.conf can override. */
{
char *wikiHost = cfgOption(CFG_WIKI_HOST);
if (isEmpty(wikiHost) || sameString(wikiHost, "HTTPHOST") || sameString(wikiHost, hHttpHost()))
    return;
alreadyAuthenticated = TRUE;
cookieStrings = NULL;
char *cookiePrefix = getRemoteCookiePrefix(wikiHost);
char *userName = getRemoteCookieVal(CFG_LOGIN_USER_NAME_COOKIE, cookiePrefix, "hgLoginUserName");
char *idKey = getRemoteCookieVal(CFG_LOGIN_IDKEY_COOKIE, cookiePrefix, "hgLoginToken");
authenticated = (isNotEmpty(userName) && isNotEmpty(idKey));
if (isNotEmpty(userName))
    {
    remoteUserName = cloneString(userName);
    cgiDecodeFull(remoteUserName, remoteUserName, strlen(remoteUserName));
    }
// BEGIN TODO: remove in July 2016
if (! authenticated && wikiLinkEnabled())
    {
    // Accept old wiki cookies for now
    char *wikiUserName = findCookieData(wikiLinkUserNameCookie());
    char *wikiLoggedIn = findCookieData(wikiLinkLoggedInCookie());
    if (isNotEmpty(wikiLoggedIn) && isNotEmpty(wikiUserName))
        {
        authenticated = TRUE;
        remoteUserName = wikiUserName;
        }
    }
// END TODO: remove in July 2016
}

static char *getLoginUserName()
/* Get the (CGI-decoded) value of the login userName cookie. */
{
char *userName = cloneString(findCookieData(loginUserNameCookie()));
if (isNotEmpty(userName))
    cgiDecodeFull(userName, userName, strlen(userName));
return userName;
}

struct slName *loginValidateCookies()
/* Return possibly empty list of cookie strings for the caller to set.
 * If login cookies are obsolete but (formerly) valid, the results sets updated cookies.
 * If login cookies are present but invalid, the result deletes/expires the cookies.
 * Otherwise returns NULL (no change to cookies). */
{
remoteHostBypass();
if (alreadyAuthenticated)
    return cookieStrings;
alreadyAuthenticated = TRUE;
authenticated = FALSE;
char *userName = getLoginUserName();

// BEGIN TODO: remove in July 2016
// If we're using values from old wiki cookies, replace the cookies.
boolean replaceOldCookies = FALSE;
if (isEmpty(userName) && wikiLinkEnabled())
    {
    userName = findCookieData(wikiLinkUserNameCookie());
    if (isNotEmpty(userName))
        replaceOldCookies = TRUE;
    }
boolean deleteCookies = FALSE;
// END TODO: remove in July 2016

char *cookieKey = NULL;
uint cookieIdx = getCookieIdKey(&cookieKey, &replaceOldCookies);
if (userName && cookieIdx)
    {
    struct sqlConnection *conn = hConnectCentral();
    if (haveKeyList(conn))
        {
        uint memberIdx = getMemberIdx(conn, userName);        // TODO: remove in July 2016
        // Check userName and cookieKey vs gbMembers' userName and keyList for cookieIdx
        boolean keyIsValid = isValidKey(conn, cookieIdx, userName, cookieKey);
        if (keyIsValid)
            authenticated = TRUE;

// BEGIN TODO: remove in July 2016
        // For the first couple months, also accept gbMembers.idx to smooth the transition.
        else if (cookieKey == NULL && cookieIdx == memberIdx)
            {
            authenticated = TRUE;
            // Create and store a new key, and make a cookie string with the new key.
            char *newKey = makeNewKey();
            insertKey(conn, cookieIdx, newKey);
            slAddHead(&cookieStrings, loginIdKeyCookieString(cookieIdx, newKey));
            }
// END TODO: remove in July 2016

        else
            {
            // Invalid key; delete cookies
            deleteCookies = TRUE;                                      // TODO: remove in July 2016
            slAddHead(&cookieStrings, loginIdKeyCookieString(0, NULL));
            slAddHead(&cookieStrings, loginUserNameCookieString(NULL));
            }
        }
    else
        {
        // gbMembers does not have keyList column -- just use gbMembers.idx
        uint memberIdx = getMemberIdx(conn, userName);
        if (cookieIdx == memberIdx)
            authenticated = TRUE;
        }
    hDisconnectCentral(&conn);
    }

// BEGIN TODO: remove in July 2016
// Delete the cookies that we used to use and make sure the new cookies are set.
if (replaceOldCookies)
    {
    slAddHead(&cookieStrings, newCookieString(wikiLinkLoggedInCookie(), NULL));
    slAddHead(&cookieStrings, newCookieString(wikiLinkUserNameCookie(), NULL));
    if (cookieStrings == NULL)
        slAddHead(&cookieStrings, loginIdKeyCookieString(cookieIdx, cookieKey));
    if (! deleteCookies)
        slAddHead(&cookieStrings, loginUserNameCookieString(userName));
    }
// END TODO: remove in July 2016

return cookieStrings;
}

char *wikiLinkHost()
/* Return the wiki host specified in hg.conf, or NULL.  Allocd here. 
 * Returns hostname from http request if hg.conf entry is HTTPHOST.
 * */
{
char *wikiHost = cfgOption(CFG_WIKI_HOST);
if (isEmpty(wikiHost) || sameString(wikiHost, "HTTPHOST"))
    wikiHost = hHttpHost();
return cloneString(wikiHost);
}

char *wikiLinkUserName()
/* Return the user name specified in cookies from the browser, or NULL if 
 * the user doesn't appear to be logged in. */
{
if (loginSystemEnabled())
    {
    if (! alreadyAuthenticated)
        errAbort("wikiLinkUserName: loginValidateCookies must be called first.");
    char *userName = getLoginUserName();
    if (isEmpty(userName) && wikiLinkEnabled())                   // TODO: remove in July 2016
        userName = findCookieData(wikiLinkUserNameCookie());      // TODO: remove in July 2016
    if (isEmpty(userName) && isNotEmpty(remoteUserName))
        userName = remoteUserName;
    if (authenticated)
        return cloneString(userName);
    }
else if (wikiLinkEnabled())
    {
    char *wikiUserName = findCookieData(wikiLinkUserNameCookie());
    char *wikiLoggedIn = findCookieData(wikiLinkLoggedInCookie());
    if (isNotEmpty(wikiLoggedIn) && isNotEmpty(wikiUserName))
        return cloneString(wikiUserName);
    }
else
    errAbort("wikiLinkUserName called when wiki is not enabled (specified "
        "in hg.conf).");
return NULL;
}

static char *encodedHgSessionReturnUrl(char *hgsid)
/* Return a CGI-encoded hgSession URL with hgsid.  Free when done. */
{
char retBuf[1024];
char *cgiDir = cgiScriptDirUrl();
safef(retBuf, sizeof(retBuf), "http%s://%s%shgSession?hgsid=%s",
      cgiAppendSForHttps(), cgiServerNamePort(), cgiDir, hgsid);
return cgiEncode(retBuf);
}



char *wikiLinkUserLoginUrlReturning(char *hgsid, char *returnUrl)
/* Return the URL for the wiki user login page. */
{
char buf[2048];
if (loginSystemEnabled())
    {
    safef(buf, sizeof(buf),
        "http%s://%s/cgi-bin/hgLogin?hgLogin.do.displayLoginPage=1&returnto=%s",
        cgiAppendSForHttps(), wikiLinkHost(), returnUrl);
    } 
else 
    {
    if (! wikiLinkEnabled())
        errAbort("wikiLinkUserLoginUrl called when wiki is not enabled (specified "
            "in hg.conf).");
    safef(buf, sizeof(buf),
        "http://%s/index.php?title=Special:UserloginUCSC&returnto=%s",
        wikiLinkHost(), returnUrl);
    }   
return(cloneString(buf));
}

char *wikiLinkUserLoginUrl(char *hgsid)
/* Return the URL for the wiki user login page with return going to hgSessions. */
{
char *retUrl = encodedHgSessionReturnUrl(hgsid);
char *result = wikiLinkUserLoginUrlReturning(hgsid, retUrl);
freez(&retUrl);
return result;
}

char *wikiLinkUserLogoutUrlReturning(char *hgsid, char *returnUrl)
/* Return the URL for the wiki user logout page. */
{
char buf[2048];
if (loginSystemEnabled())
    {
    safef(buf, sizeof(buf),
        "http%s://%s/cgi-bin/hgLogin?hgLogin.do.displayLogout=1&returnto=%s",
        cgiAppendSForHttps(), wikiLinkHost(), returnUrl);
    } 
else
    {
    if (! wikiLinkEnabled())
        errAbort("wikiLinkUserLogoutUrl called when wiki is not enable (specified "
            "in hg.conf).");
    safef(buf, sizeof(buf),
        "http://%s/index.php?title=Special:UserlogoutUCSC&returnto=%s",
         wikiLinkHost(), returnUrl);
    }
return(cloneString(buf));
}

char *wikiLinkUserLogoutUrl(char *hgsid)
/* Return the URL for the wiki user logout page that returns to hgSessions. */
{
char *retEnc = encodedHgSessionReturnUrl(hgsid);
char *result = wikiLinkUserLogoutUrlReturning(hgsid, retEnc);
freez(&retEnc);
return result;
}

char *wikiLinkUserSignupUrl(char *hgsid)
/* Return the URL for the user signup  page. */
{
char buf[2048];
char *retEnc = encodedHgSessionReturnUrl(hgsid);

if (loginSystemEnabled())
    {
    safef(buf, sizeof(buf),
        "http%s://%s/cgi-bin/hgLogin?hgLogin.do.signupPage=1&returnto=%s",
        cgiAppendSForHttps(), wikiLinkHost(), retEnc);
    }
else
    {
    if (! wikiLinkEnabled())
        errAbort("wikiLinkUserLogoutUrl called when wiki is not enable (specified "
            "in hg.conf).");
    safef(buf, sizeof(buf),
        "http://%s/index.php?title=Special:UserlogoutUCSC&returnto=%s",
         wikiLinkHost(), retEnc);
    }
freez(&retEnc);
return(cloneString(buf));
}

char *wikiLinkChangePasswordUrl(char *hgsid)
/* Return the URL for the user change password page. */
{
char buf[2048];
char *retEnc = encodedHgSessionReturnUrl(hgsid);

if (loginSystemEnabled())
    {
    safef(buf, sizeof(buf),
        "http%s://%s/cgi-bin/hgLogin?hgLogin.do.changePasswordPage=1&returnto=%s",
        cgiAppendSForHttps(), wikiLinkHost(), retEnc);
    }
else
    {
    if (! wikiLinkEnabled())
        errAbort("wikiLinkUserLogoutUrl called when wiki is not enable (specified "
            "in hg.conf).");
    safef(buf, sizeof(buf),
        "http://%s/index.php?title=Special:UserlogoutUCSC&returnto=%s",
         wikiLinkHost(), retEnc);
    }
freez(&retEnc);
return(cloneString(buf));
}

