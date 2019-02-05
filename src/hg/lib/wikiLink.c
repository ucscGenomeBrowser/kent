/* wikiLink - originally used to interoperate with a wiki site (share user identities). 
 * With the Wiki Track removed these days, this file contains code related to user
 * authentication.
 * */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "hgConfig.h"
#include "hui.h"
#include "md5.h"
#include "web.h"
#include "wikiLink.h"
#include "base64.h"

// Flag to indicate that loginValidateCookies has been called:
static boolean alreadyAuthenticated = FALSE;
// Set by loginValidateCookies, used by wikiLinkUserName
static boolean authenticated = FALSE;
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
return (cfgOption(CFG_LOGIN_SYSTEM_NAME) != NULL);
}

boolean loginUseBasicAuth()
/* Return TRUE if login.basicAuth is on in hg.conf . */
{
return (cfgOptionBooleanDefault(CFG_LOGIN_BASICAUTH, FALSE));
}

boolean wikiLinkEnabled()
/* Return TRUE if all wiki.* parameters are defined in hg.conf . */
{
return ((cfgOption(CFG_WIKI_HOST) != NULL) &&
	(cfgOption(CFG_WIKI_USER_NAME_COOKIE) != NULL) &&
	(cfgOption(CFG_WIKI_LOGGED_IN_COOKIE) != NULL));
}

static char *wikiLinkLoggedInCookie()
/* Return the cookie name specified in hg.conf as the wiki logged-in cookie, or a default.
 * Do not free result. */
{
return cfgOptionDefault(CFG_WIKI_LOGGED_IN_COOKIE, "hgLoginIdKey");
}

static char *wikiLinkUserNameCookie()
/* Return the cookie name specified in hg.conf as the wiki user name cookie, or a default.
 * Do not free result.. */
{
return cfgOptionDefault(CFG_WIKI_USER_NAME_COOKIE, "hgLoginUserName");
}

static char *getLoginCookieSalt()
/* Return the secret salt that we hash with userName to verify cookie key, NULL if undefined. */
{
return cfgOption(CFG_LOGIN_COOKIE_SALT);
}

static uint getCookieIdxOrKey(char **retKey)
/* The LoggedIn cookie value may be NULL, a number <idx>, or a long string <key>.
 * If value is NULL/empty, return 0 and set *retKey to NULL;
 * If value is just a number, return the number and set *retKey to NULL.
 * Otherwise return 0 and set *retKey to the cookie value. */
{
uint idx = 0;
char *key = NULL;
char *cookieIdKeyStr = findCookieData(wikiLinkLoggedInCookie());
if (isNotEmpty(cookieIdKeyStr))
    {
    if (isAllDigits(cookieIdKeyStr))
        idx = (uint)atoll(cookieIdKeyStr);
    else
        key = cloneString(cookieIdKeyStr);
    }
if (retKey)
    *retKey = key;
return idx;
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

static struct slName *wikiLinkUserNameCookieString(char *userName)
/* Return a cookie string that sets userName cookie to userName if non-empty and
 * deletes/invalidates the cookie if empty/NULL. */
{
return newCookieString(wikiLinkUserNameCookie(), cgiEncodeFull(userName));
}

static struct slName *wikiLinkLoggedInCookieString(uint idx, char *key)
/* Return a cookie string that sets ID cookie to key if key is non-empty, otherwise idx if > 0,
 * and deletes/invalidates the cookie if key is empty and idx is 0. */
{
char newVal[1024];
if (isNotEmpty(key))
    safef(newVal, sizeof(newVal), "%s", key);
else if (idx > 0)
    safef(newVal, sizeof(newVal), "%u", idx);
else
    newVal[0] = '\0';
return newCookieString(wikiLinkLoggedInCookie(), isNotEmpty(newVal) ? newVal : NULL);
}

static char *makeUserKey(char *userName, char *salt)
/* Add salt to userName and hash. */
{
char *userMd5 = md5HexForString(userName);
char saltedBuf[1024];
safef(saltedBuf, sizeof(saltedBuf), "%s-%s", salt, userMd5);
char *key = md5HexForString(saltedBuf);
freeMem(userMd5);
return key;
}

struct slName *loginLoginUser(char *userName, uint idx)
/* Return cookie strings to set for user so we'll recognize that user is logged in.
 * Call this after validating userName's password. */
{
alreadyAuthenticated = TRUE;
authenticated = TRUE;
cookieStrings = NULL;
char *key = NULL;
char *cookieSalt = getLoginCookieSalt();
if (isNotEmpty(cookieSalt))
    key = makeUserKey(userName, cookieSalt);
slAddHead(&cookieStrings, wikiLinkLoggedInCookieString(idx, key));
slAddHead(&cookieStrings, wikiLinkUserNameCookieString(userName));
return cookieStrings;
}

struct slName *loginLogoutUser()
/* Return cookie strings to set (deleting the login cookies). */
{
alreadyAuthenticated = TRUE;
authenticated = FALSE;
cookieStrings = NULL;
slAddHead(&cookieStrings, wikiLinkLoggedInCookieString(0, NULL));
slAddHead(&cookieStrings, wikiLinkUserNameCookieString(NULL));
return cookieStrings;
}

static char *getLoginUserName()
/* Get the (CGI-decoded) value of the login userName cookie. */
{
char *userName = cloneString(findCookieData(wikiLinkUserNameCookie()));
if (isNotEmpty(userName))
    cgiDecodeFull(userName, userName, strlen(userName));
return userName;
}

static boolean loginIsRemoteClient()
/* Return TRUE if wikiHost is non-empty and not the same as this host. */
{
char *wikiHost = cfgOption(CFG_WIKI_HOST);
return (isNotEmpty(wikiHost) &&
        differentString(wikiHost, "HTTPHOST") &&
        differentString(wikiHost, hHttpHost()));
}

static boolean idxIsValid(char *userName, uint idx)
/* If login is local, return TRUE if idx is the same as hgcentral.gbMembers.idx for userName.
 * If remote, just return TRUE. */
{
if (loginIsRemoteClient())
    return TRUE;
// Look up idx for userName in gbMembers and compare to idx
struct sqlConnection *conn = hConnectCentral();
char query[512];
sqlSafef(query, sizeof(query), "select idx from gbMembers where userName='%s'", userName);
uint memberIdx = (uint)sqlQuickLongLong(conn, query);
hDisconnectCentral(&conn);
return (idx == memberIdx);
}

static void sendNewCookies(char *userName, char *cookieSalt)
/* Compute key from userName and cookieSalt, and add a cookie string with the new key. */
{
char *newKey = makeUserKey(userName, cookieSalt);
slAddHead(&cookieStrings, wikiLinkLoggedInCookieString(0, newKey));
slAddHead(&cookieStrings, wikiLinkUserNameCookieString(userName));
}

struct slName *loginValidateCookies()
/* Return possibly empty list of cookie strings for the caller to set.
 * If login cookies are obsolete but (formerly) valid, the results sets updated cookies.
 * If login cookies are present but invalid, the result deletes/expires the cookies.
 * Otherwise returns NULL (no change to cookies). */
{
alreadyAuthenticated = TRUE;
authenticated = FALSE;
char *userName = getLoginUserName();
char *cookieKey = NULL;
uint cookieIdx = getCookieIdxOrKey(&cookieKey);
char *cookieSalt = getLoginCookieSalt();
if (userName && (cookieIdx > 0 || isNotEmpty(cookieKey)))
    {
    if (isNotEmpty(cookieSalt))
        {
        if (cookieKey && sameString(makeUserKey(userName, cookieSalt), cookieKey))
            {
            authenticated = TRUE;
            }
        else if (cfgOptionBooleanDefault(CFG_LOGIN_ACCEPT_ANY_ID, FALSE))
            {
            // Don't perform any checks on the incoming cookie.
            authenticated = TRUE;
            // Replace with improved cookie, in preparation for when better security is enabled.
            sendNewCookies(userName, cookieSalt);
            }
        else if (cfgOptionBooleanDefault(CFG_LOGIN_ACCEPT_IDX, FALSE) &&
                 idxIsValid(userName, cookieIdx))
            {
            // Compare cookieIdx vs. gbMembers.idx (if login is local) -- a little more secure
            // than before, but might cause some trouble if a userName has different idx values
            // on different systems (e.g. RR vs genome-preview/genome-text).
            authenticated = TRUE;
            // Replace with improved cookie, in preparation for when better security is enabled.
            sendNewCookies(userName, cookieSalt);
            }
        }
    else
        {
        // hg.conf doesn't specify login.cookieSalt -- no checking.
        authenticated = TRUE;
        }
    if (!authenticated)
        {
        // Invalid key; delete cookies
        slAddHead(&cookieStrings, wikiLinkLoggedInCookieString(0, NULL));
        slAddHead(&cookieStrings, wikiLinkUserNameCookieString(NULL));
        }
    }
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

boolean loginUseHttps()
/* Return TRUE unless https is disabled in hg.conf. */
{
return cfgOptionBooleanDefault(CFG_LOGIN_USE_HTTPS, TRUE);
}

static char *loginUrl()
/* Return the URL for the login host. */
{
char buf[2048];
safef(buf, sizeof(buf), "%shgLogin", hLoginHostCgiBinUrl());
return cloneString(buf);
}

char* getHttpBasicToken()
/* Return HTTP Basic Auth Token or NULL. Result has to be freed. */
{
char *auth = getenv("HTTP_AUTHORIZATION");
// e.g. "Basic bwF4OmQxUglhanM="
if (auth==NULL)
    return NULL;
char *token = cloneNotFirstWord(auth);
if (isEmpty(token))
    {
    fprintf(stderr, "wikiLinkc.: Illegal format of HTTP Authorization Header?");
    return NULL;
    }
return token;
}

void printTokenErrorAndExit() 
/* output an error message if http basic token is missing */
{
    printf("Internal error: this server has HTTP Basic Authentication enabled in cgi-bin/hg.conf:%s.<br>", CFG_LOGIN_BASICAUTH);
    puts("The Genome Browser cannot find an 'Authorization' header to the Genome Browser.<br>");
    puts("This website should only be reachable through a https connection that requires username and password.<p>");
    puts("If you have reached this website in a way that does not require a password, please contact your adminstrator.<p><p>");
    puts("If this was the case, for the administrator: ");
    puts("Make sure that HTTP Basic Authentication is activated for the cgi-bin directory in the Apache Configuration. <p>");
    puts("If it is and you are logged in, check that the CGI-BIN directory in Apache has these settings activated:<br>");
    puts("<li>CGIPassAuth on' (Apache 2.4) <br>");
    puts("<li>'SetEnvIf Authorization .+ HTTP_AUTHORIZATION=$0' (Apache 2.2).<br>");
    puts("These settings tell Apache to forward credentials to CGIs. Do not forget to restart Apache after the changes.<p>");
    exit(0);
}

boolean isValidUsername(char *s)
/* Return TRUE if s is a valid username: only contains alpha chars, @, _ or - */
{
char c = *s;
while ((c = *s++) != 0)
    {
    if (!(isalnum(c) || (c == '_') || (c=='@') || (c=='-')))
	return FALSE;
    }
return TRUE;
}

char *basicAuthUser(char *token)
/* get the HTTP Header 'Authorization', which is just the b64 encoded username:password,
 * and return the username. Result has to be freed. */
{

// username:password is b64 encrypted 
char *tokenPlain = base64Decode(token, 0);

// plain text is in format username:password
char *words[2];
int wordCount = chopString(tokenPlain, ":", words, ArraySize(words));
if (wordCount!=2)
    errAbort("wikiLink/basicAuthUser: got illegal basic auth token");
char *user = words[0];

return user;
}

char *wikiLinkUserName()
/* Return the user name specified in cookies from the browser, or NULL if 
 * the user doesn't appear to be logged in. */
{
if (loginUseBasicAuth())
    {
    char *token = getHttpBasicToken();
    //XX The following should be uncommented for security reasons
    //if (!token) 
        //printTokenErrorAndExit();
    // May 2017: Allowing normal login even when HTTP Basic is enabled. This may be insecure. 
    // Keeping it insecure pending Jim's/Clay's approval, for backwards compatibility.
    if (token) 
        return basicAuthUser(token);
    }

if (loginSystemEnabled())
    {
    if (! alreadyAuthenticated)
        loginValidateCookies();
    if (authenticated)
        return cloneString(getLoginUserName());
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
safef(retBuf, sizeof(retBuf), "%shgSession?hgsid=%s",
      hLocalHostCgiBinUrl(), hgsid);
return cgiEncode(retBuf);
}


//#*** TODO: replace all of the non-mediawiki "returnto"s here and in hgLogin.c with a #define


char *wikiLinkUserLoginUrlReturning(char *hgsid, char *returnUrl)
/* Return the URL for the wiki user login page. */
{
char buf[2048];
if (loginSystemEnabled())
    {
    safef(buf, sizeof(buf),
        "%s?hgLogin.do.displayLoginPage=1&returnto=%s",
        loginUrl(), returnUrl);
    } 
else 
    {
    if (! wikiLinkEnabled())
        errAbort("wikiLinkUserLoginUrl called when wiki is not enabled (specified "
            "in hg.conf).");
    // The following line of code is not used at UCSC anymore since 2014
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
        "%s?hgLogin.do.displayLogout=1&returnto=%s",
        loginUrl(), returnUrl);
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
        "%s?hgLogin.do.signupPage=1&returnto=%s",
        loginUrl(), retEnc);
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
        "%s?hgLogin.do.changePasswordPage=1&returnto=%s",
        loginUrl(), retEnc);
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

void wikiFixLogoutLinkWithJs()
/* HTTP Basic Auth requires a strange hack to logout. This code prints a script 
 * that fixes an html link with id=logoutLink */
{
struct dyString *dy = dyStringNew(4096);
// logoutJs.h is a stringified .js file
#include "logoutJs.h"
dyStringAppend(dy, cdwLogoutJs);
dyStringPrintf(dy, "$('#logoutLink').click( function() { logout('/', 'http://cirm.ucsc.edu'); return false; });\n");
jsInline(dy->string);
dyStringFree(&dy);
printf("<script src='//cdnjs.cloudflare.com/ajax/libs/bowser/1.6.1/bowser.min.js'></script>");
}
