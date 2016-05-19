/* wikiLink - interoperate with a wiki site (share user identities). */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "hgConfig.h"
#include "hui.h"
#include "web.h"
#include "wikiLink.h"

// Flag to indicate that loginValidateCookies has been called:
static boolean alreadyAuthenticated = FALSE;
// If centralDb has table gbMemberToken, then loginValidateCookies will set this
// to a random token that validates the user; otherwise if the cookie has the same
// value as gbMembers.idx, this is set to that ID; otherwise it stays 0 and the user
// is not logged in.
static uint authToken = 0;
// If we need to change some cookies, store cookie strings here in case loginValidateCookies
// is called multiple times (e.g. validate before cookie-writing, then later write cookies)
static struct slName *cookieStrings = NULL;

// If a random token in gbMemberToken is more than this many seconds old, make a new
// random token and delete the old:
#define TOKEN_LIFESPAN 300

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

static char *loginTokenCookie()
/* Return the name of the login system random token cookie.  Do not free result. */
{
static char defaultCookie[512];
char *cookie = cfgOption(CFG_LOGIN_TOKEN_COOKIE);
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

static uint getCookieToken(
                           boolean *retReplaceOld                 // TODO: remove in July 2016
                           )
/* If the cookie holding the login token exists, return its uint value, else 0. */
{
char *cookieTokenStr = findCookieData(loginTokenCookie());
if (isEmpty(cookieTokenStr) && wikiLinkEnabled())                 // TODO: remove in July 2016
    {                                                             // TODO: remove in July 2016
    cookieTokenStr = findCookieData(wikiLinkLoggedInCookie());    // TODO: remove in July 2016
    if (retReplaceOld && isNotEmpty(cookieTokenStr))              // TODO: remove in July 2016
        *retReplaceOld = TRUE;                                    // TODO: remove in July 2016
    }                                                             // TODO: remove in July 2016
return cookieTokenStr ? (uint)atoll(cookieTokenStr) : 0;
}

static uint getMemberIdx(struct sqlConnection *conn, char *userName)
/* Return userName's idx value in gbMembers.  Return 0 if not found. */
{
char query[512];
sqlSafef(query, sizeof(query), "select idx from gbMembers where userName='%s'", userName);
return (uint)sqlQuickLongLong(conn, query);
}

static boolean haveTokenTable(struct sqlConnection *conn)
/* Return true if centralDb has table gbMemberToken. */
{
return sqlTableExists(conn, "gbMemberToken");
}

static boolean isValidToken(struct sqlConnection *conn, uint token, char *userName,
                            boolean *retMakeNewToken)
/* Return TRUE if gbMemberToken has an entry that maps token to userName.
 * If retMakeNewToken is non-NULL, set it to TRUE if the token is older than TOKEN_LIFESPAN. */
{
boolean isValid = FALSE;
char query[512];
sqlSafef(query, sizeof(query), "select userName, createTime from gbMemberToken where token = %u",
         token);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
if ((row = sqlNextRow(sr)) != NULL)
    {
    char *userForToken = cloneString(row[0]);
    if (retMakeNewToken != NULL)
        {
        long createTime = sqlDateToUnixTime(row[1]);
        *retMakeNewToken = (time(NULL) - createTime > TOKEN_LIFESPAN);
        }
    isValid = sameString(userForToken, userName);
    }
sqlFreeResult(&sr);
return isValid;
}

static void deleteToken(struct sqlConnection *conn, uint token)
/* Remove token's entry from gbMemberToken. */
{
char query[512];
sqlSafef(query, sizeof(query), "delete from gbMemberToken where token = %u", token);
sqlUpdate(conn, query);
}

static void insertToken(struct sqlConnection *conn, uint token, char *userName)
/* Add a new entry to gbMemberToken mapping token to userName. */
{
char query[512];
sqlSafef(query, sizeof(query), "insert into gbMemberToken values (%u, '%s', now())",
         token, userName);
sqlUpdate(conn, query);
}

static uint newToken()
/* Return a random nonnegative integer.  In the extremely unlikely event that it is 0,
 * the user will have to log in again. */
{
uint token = 0;
// open random system device for read-only access.
FILE *f = mustOpen("/dev/urandom", "r");
mustRead(f, &token, 4);
carefulClose(&f);
return token;
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

static char *loginCookieDate()
/* For now, don't expire (before we retire :)  Consider changing this to 6 months in the
 * future or something like that, maybe under hg.conf control (for CIRM vs GB?). */
{
return "Thu, 31-Dec-2037 23:59:59 GMT";
}

#define EXPIRED_COOKIE_DATE "Thu, 01-Jan-1970 00:00:00 GMT"

struct slName *invalidateCookieString(char *cookieName)
/* Return a cookie string that deletes/invalidates the cookie. */
{
char *domain = getCookieDomainString();
char cookieString[2048];
safef(cookieString, sizeof(cookieString), "%s=;%s path=/; expires="EXPIRED_COOKIE_DATE,
      cookieName, domain);
return slNameNew(cookieString);
}

struct slName *loginUserNameCookieString(char *userName)
/* Return a cookie string that sets userName cookie to userName if non-empty and
 * deletes/invalidates the cookie if empty. */
{
char *cookie = loginUserNameCookie();
if (isNotEmpty(userName))
    {
    // Send userName in cookie
    char *domain = getCookieDomainString();
    char cookieString[2048];
    safef(cookieString, sizeof(cookieString), "%s=%s;%s path=/; expires=%s",
          cookie, userName, domain, loginCookieDate());
    return slNameNew(cookieString);
    }
else
    return invalidateCookieString(cookie);
}

struct slName *loginTokenCookieString(uint token)
/* Return a cookie string that sets token cookie to token if token is valid and
 * deletes/invalidates the cookie if not. */
{
char *cookie = loginTokenCookie();
if (token)
    {
    // Validated; send new token in cookie
    char *domain = getCookieDomainString();
    char cookieString[2048];
    safef(cookieString, sizeof(cookieString), "%s=%u;%s path=/; expires=%s",
          cookie, token, domain, loginCookieDate());
    return slNameNew(cookieString);
    }
else
    return invalidateCookieString(cookie);
}

struct slName *loginLoginUser(char *userName)
/* Return a nonzero token which caller must set as the value of CFG_WIKI_LOGGED_IN_COOKIE.
 * Call this when userName's password has been validated. */
{
struct sqlConnection *conn = hConnectCentral();
alreadyAuthenticated = TRUE;
if (haveTokenTable(conn))
    {
    authToken = newToken();
    insertToken(conn, authToken, userName);
    }
else
    // Fall back on gbMembers.idx
    authToken = getMemberIdx(conn, userName);
hDisconnectCentral(&conn);
slAddHead(&cookieStrings, loginTokenCookieString(authToken));
slAddHead(&cookieStrings, loginUserNameCookieString(userName));
return cookieStrings;
}

struct slName *loginLogoutUser()
/* If the gbMemberToken table exists, delete the user's random token. */
{
struct sqlConnection *conn = hConnectCentral();
if (haveTokenTable(conn))
    deleteToken(conn, getCookieToken(NULL));
slAddHead(&cookieStrings, loginTokenCookieString(0));
slAddHead(&cookieStrings, loginUserNameCookieString(NULL));
hDisconnectCentral(&conn);
return cookieStrings;
}

struct slName *loginValidateCookies()
/* Return possibly empty list of cookie strings for the caller to set.
 * If login cookies are present and valid, but the current token has aged out,
 * the returned cookie string sets the token cookie to a new token value.
 * If login cookies are present but invalid, the result deletes/expires the cookies.
 * Otherwise returns NULL (no change to cookies). */
{
if (alreadyAuthenticated)
    return cookieStrings;
alreadyAuthenticated = TRUE;
authToken = 0;
char *userName = findCookieData(loginUserNameCookie());
boolean replaceOldCookies = FALSE;                            // TODO: remove in July 2016
if (isEmpty(userName) && wikiLinkEnabled())                   // TODO: remove in July 2016
    {                                                         // TODO: remove in July 2016
    userName = findCookieData(wikiLinkUserNameCookie());      // TODO: remove in July 2016
    if (isNotEmpty(userName))                                 // TODO: remove in July 2016
        replaceOldCookies = TRUE;                             // TODO: remove in July 2016
    }                                                         // TODO: remove in July 2016
boolean deleteCookies = FALSE;                                // TODO: remove in July 2016
uint cookieToken = getCookieToken(&replaceOldCookies);
if (userName && cookieToken)
    {
    struct sqlConnection *conn = hConnectCentral();
    uint memberIdx = getMemberIdx(conn, userName);
    if (haveTokenTable(conn))
        {
        // Look up cookieToken and userName in gbMemberToken
        boolean makeNewToken = FALSE;
        boolean tokenIsValid = isValidToken(conn, cookieToken, userName, &makeNewToken);
        if (tokenIsValid
            // Also accept gbMembers.idx to smooth the transition; TODO: remove in July 2016
            || (cookieToken == memberIdx)) // TODO: remove in July 2016
            {
            if (makeNewToken
                || ! tokenIsValid) // TODO: remove in July 2016
                {
                // Delete the old token, create and store a new token, and make a cookie string
                // with the new token.
                deleteToken(conn, cookieToken);
                authToken = newToken();
                insertToken(conn, authToken, userName);
                slAddHead(&cookieStrings, loginTokenCookieString(authToken));
                }
            else
                // Keep using this token, no change to cookie
                authToken = cookieToken;
            }
        else
            {
            // Invalid token; delete cookies
            deleteCookies = TRUE;                                      // TODO: remove in July 2016
            slAddHead(&cookieStrings, loginTokenCookieString(0));
            slAddHead(&cookieStrings, loginUserNameCookieString(NULL));
            }
        }
    else if (cookieToken == memberIdx)
        // centralDb does not have gbMemberToken table -- fall back on gbMembers.idx
        authToken = cookieToken;
    hDisconnectCentral(&conn);
    }
// Delete the cookies that we used to use and make sure the new cookies are set. remove in July '16
if (replaceOldCookies)                                                 // TODO: remove in July 2016
    {                                                                  // TODO: remove in July 2016
    if (cookieStrings == NULL)                                         // TODO: remove in July 2016
        slAddHead(&cookieStrings, loginTokenCookieString(authToken));            // TODO: remove in July 2016
    if (! deleteCookies)                                               // TODO: remove in July 2016
        slAddHead(&cookieStrings, loginUserNameCookieString(userName));          // TODO: remove in July 2016
    slAddHead(&cookieStrings, invalidateCookieString(wikiLinkLoggedInCookie())); // TODO: remove in July 2016
    slAddHead(&cookieStrings, invalidateCookieString(wikiLinkUserNameCookie())); // TODO: remove in July 2016
    }                                                                  // TODO: remove in July 2016
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
    char *userName = findCookieData(loginUserNameCookie());
    if (isEmpty(userName) && wikiLinkEnabled())                   // TODO: remove in July 2016
        userName = findCookieData(wikiLinkUserNameCookie());      // TODO: remove in July 2016
    if (authToken)
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

