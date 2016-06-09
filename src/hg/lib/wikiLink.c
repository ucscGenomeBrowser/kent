/* wikiLink - interoperate with a wiki site (share user identities). */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "hgConfig.h"
#include "hui.h"
#include "cart.h"
#include "web.h"
#include "wikiLink.h"

// Flag to indicate that loginSystemValidateCookies has been called:
static boolean alreadyAuthenticated = FALSE;
// If centralDb has table gbMemberToken, then loginSystemValidateCookies will set this
// to a random token that validates the user; otherwise if the cookie has the same
// value as gbMembers.idx, this is set to that ID; otherwise it stays 0 and the user
// is not logged in.
static uint authToken = 0;

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

static uint getCookieToken()
/* If the cookie holding the login token exists, return its uint value, else 0. */
{
char *cookieTokenStr = findCookieData(wikiLinkLoggedInCookie());
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
/* Return a random nonzero positive integer. */
{
srand(clock1000());
uint token = (uint)rand();
if (token == 0)
    // highly unlikely - try again.
    token = (uint)rand();
return token;
}

uint loginSystemLoginUser(char *userName)
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
return authToken;
}

static char *loginCookieDate()
/* For now, don't expire (before we retire :)  Consider changing this to 6 months in the
 * future or something like that, maybe under hg.conf control (for CIRM vs GB?). */
{
return "Thu, 31-Dec-2037 23:59:59 GMT";
}

static char *expiredCookieDate()
/* Return a date that passed long ago. */
{
return "Thu, 01-Jan-1970 00:00:00 GMT";
}

char *makeAuthCookieString()
/* Return a cookie string that sets cookie to authToken if token is valid and
 * deletes/invalidates both cookies if not. */
{
struct dyString *dy = dyStringCreate("%s=", wikiLinkLoggedInCookie());
if (authToken)
    // Validated; send new token in cookie
    dyStringPrintf(dy, "%u; path=/; domain=%s; expires=%s\r\n",
                   authToken, cgiServerName(), loginCookieDate());
else
    {
    // Remove both cookies
    dyStringPrintf(dy, "; path=/; domain=%s; expires=%s\r\n",
                   cgiServerName(), expiredCookieDate());
    dyStringPrintf(dy, "%s=; path=/; domain=%s; expires=%s\r\n",
                   wikiLinkUserNameCookie(),
                   cgiServerName(), expiredCookieDate());
    }
return dyStringCannibalize(&dy);
}

char *loginSystemValidateCookies()
/* Return a cookie string or NULL.  If login cookies are present and valid, but the current
 * token has aged out, the returned cookie string sets a cookie to a new token value.
 * If login cookies are present but invalid, the cookie string deletes/expires the cookies.
 * Otherwise returns NULL. */
{
if (alreadyAuthenticated)
    return makeAuthCookieString();
alreadyAuthenticated = TRUE;
authToken = 0;
char *userName = findCookieData(wikiLinkUserNameCookie());
uint cookieToken = getCookieToken();
if (userName && cookieToken)
    {
    struct sqlConnection *conn = hConnectCentral();
    uint memberIdx = getMemberIdx(conn, userName);
    char *cookieString = NULL;
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
                cookieString = makeAuthCookieString();
                }
            else
                // Keep using this token, no change to cookie
                authToken = cookieToken;
            }
        else
            // Invalid token; delete both cookies
            cookieString = makeAuthCookieString();
        }
    else if (cookieToken == memberIdx)
        // centralDb does not have gbMemberToken table -- fall back on gbMembers.idx
        authToken = cookieToken;
    hDisconnectCentral(&conn);
    return cookieString;
    }
return NULL;
}

char *wikiLinkHost()
/* Return the wiki host specified in hg.conf, or NULL.  Allocd here. 
 * Returns hostname from http request if hg.conf entry is HTTPHOST.
 * */
{
char *wikiHost = cfgOption(CFG_WIKI_HOST);
if ((wikiHost!=NULL) && sameString(wikiHost, "HTTPHOST"))
    wikiHost = hHttpHost();
return cloneString(wikiHost);
}

boolean wikiLinkEnabled()
/* Return TRUE if all wiki.* parameters are defined in hg.conf . */
{
return ((cfgOption(CFG_WIKI_HOST) != NULL) &&
	(cfgOption(CFG_WIKI_USER_NAME_COOKIE) != NULL) &&
	(cfgOption(CFG_WIKI_LOGGED_IN_COOKIE) != NULL));
}

char *wikiLinkUserName()
/* Return the user name specified in cookies from the browser, or NULL if 
 * the user doesn't appear to be logged in. */
{
if (wikiLinkEnabled())
    {
    char *wikiUserName = findCookieData(wikiLinkUserNameCookie());
    char *wikiLoggedIn = findCookieData(wikiLinkLoggedInCookie());

    if (isNotEmpty(wikiLoggedIn) && isNotEmpty(wikiUserName))
	{
        if (loginSystemEnabled())
            {
            if (! alreadyAuthenticated)
                errAbort("wikiLinkUserName: loginSystemValidateCookies must be called first.");
            if (authToken == 0)
                return NULL;
            }
        else
            {
            // Wiki only: fall back on checking ID cookie vs. gbMembers.idx
            uint cookieId = getCookieToken();
            struct sqlConnection *conn = hConnectCentral();
            uint memberIdx = getMemberIdx(conn, wikiUserName);
            hDisconnectCentral(&conn);
            if (cookieId != memberIdx)
                return NULL;
            }
	return cloneString(wikiUserName);
	}
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
    if (! wikiLinkEnabled())
        errAbort("wikiLinkUserLoginUrl called when login system is not enabled "
           "(specified in hg.conf).");
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
    if (! wikiLinkEnabled())
        errAbort("wikiLinkUserLogoutUrl called when login system is not enabled "
            "(specified in hg.conf).");
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
    if (! wikiLinkEnabled())
        errAbort("wikiLinkUserSignupUrl called when login system is not enabled "
            "(specified in hg.conf).");
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
    if (! wikiLinkEnabled())
        errAbort("wikiLinkChangePasswordUrl called when login system is not enabled "
            "(specified in hg.conf).");
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

