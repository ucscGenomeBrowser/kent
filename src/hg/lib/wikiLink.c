/* wikiLink - originally used to interoperate with a wiki site (share user identities). 
 * With the Wiki Track removed these days, this file contains code related to user
 * authentication.
 * */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

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

char *getUserName()
{
return (loginSystemEnabled() || wikiLinkEnabled()) ? wikiLinkUserName() : NULL;
}


char *wikiLinkUserId()
/* Return the user ID specified in cookies from the browser. Does not check if user is logged in.
 * To make sure that the ID is valid, call this only after you have checked with wikiLinkUserName() that the user is logged in. */
{
    return findCookieData(wikiLinkLoggedInCookie());
}

char *wikiLinkEncodeReturnUrl(char *hgsid, char *cgiName, char* urlSuffix)
/* Return a CGI-encoded URL with hgsid to a CGI.  Free when done. */
{
char retBuf[1024];
safef(retBuf, sizeof(retBuf), "%s%s?hgsid=%s%s",
      hLocalHostCgiBinUrl(), cgiName, hgsid, urlSuffix);
return cgiEncode(retBuf);
}


static char *encodedHgSessionReturnUrl(char *hgsid)
/* Return a CGI-encoded hgSession URL with hgsid.  Free when done. */
{
return wikiLinkEncodeReturnUrl(hgsid, "hgSession", "");
}


/* Longest return URL we will build.  hgLogin copies the decoded returnto into a 2 kB buffer
 * and aborts if it does not fit, and cgi-encoding can nearly triple the length on the way
 * there, so a page with a very long query string gives up the query string, not the trip
 * back. */
#define RETURN_URL_MAX 1000

static boolean returnUrlSchemeIsSafe(char *returnUrl)
/* Return TRUE unless returnUrl carries a scheme other than http or https.  The scheme is the
 * text before the first colon, and only when that colon comes before any slash, question mark
 * or hash; a colon after one of those belongs to the path or the query, so the URL is relative.
 * This is what keeps a javascript: or data: URL out of the href hgLogin writes. */
{
char *colon = strchr(returnUrl, ':');
if (colon == NULL)
    return TRUE;
char *pathStart = strpbrk(returnUrl, "/?#");
if (pathStart != NULL && pathStart < colon)
    return TRUE;
int schemeLen = colon - returnUrl;
return (schemeLen == 4 && startsWithNoCase("http", returnUrl))
    || (schemeLen == 5 && startsWithNoCase("https", returnUrl));
}

static boolean returnUrlIsWellFormed(char *returnUrl)
/* Return TRUE if returnUrl looks like a URL hgLogin can write into its page.  Every CGI
 * parameter becomes a cart variable, so returnto holds whatever the visitor's URL said, and it
 * is printed into an href attribute and into a javascript location assignment.  A quote, an
 * angle bracket, a backslash or a control character would end the attribute or the string
 * literal and reflect script onto the page.  A real URL percent-encodes all of those, so
 * refusing them turns away nothing legitimate. */
{
char *c;
for (c = returnUrl; *c != 0; c++)
    {
    unsigned char uc = (unsigned char)*c;
    if (uc < ' ' || uc == 127 || strchr("\"'<>\\`", *c) != NULL)
        return FALSE;
    }
return returnUrlSchemeIsSafe(returnUrl);
}

static boolean returnUrlHostIsApproved(char *returnUrl)
/* Return TRUE if returnUrl starts with the login host or one of the hosts listed in
 * login.approvedReturn.  The setting is optional; where it is absent no host is checked. */
{
char *approved = cfgOptionDefault(CFG_APPROVED_HOSTS, NULL);
if (approved == NULL)
    return TRUE;
struct slName *approvedHosts = slNameListFromComma(approved);
slAddHead(&approvedHosts, slNameNew(hLoginHostCgiBinUrl()));
struct slName *host;
for (host = approvedHosts; host != NULL; host = host->next)
    if (startsWith(host->name, returnUrl))
        return TRUE;
return FALSE;
}

boolean loginReturnUrlIsAcceptable(char *returnUrl)
/* Return TRUE if hgLogin will accept returnUrl as its returnto: an http or https URL with no
 * character that could break out of the page hgLogin prints it into, on an approved host.
 * hgLogin checks this on the way in; callers that build a returnto check it on the way out,
 * so that a URL hgLogin would refuse becomes a plain login link instead of an error page. */
{
return returnUrl != NULL && returnUrlIsWellFormed(returnUrl)
    && returnUrlHostIsApproved(returnUrl);
}

char *wikiLinkEncodePageReturnUrl(char *url)
/* Return url CGI-encoded for use as a returnto, or NULL if hgLogin would refuse it.
 * Free when done. */
{
if (!loginReturnUrlIsAcceptable(url) || strlen(url) > RETURN_URL_MAX)
    return NULL;
return cgiEncode(url);
}

static char *currentPageUrl(char *cgiName, char *hgsid, char *queryString)
/* Return the absolute URL of the CGI we are running now, with the given query string, or with
 * just hgsid when queryString is NULL.  Free when done. */
{
struct dyString *dy = dyStringNew(256);
dyStringPrintf(dy, "%s%s", hLocalHostCgiBinUrl(), cgiName);
if (isNotEmpty(queryString))
    {
    dyStringPrintf(dy, "?%s", queryString);
    // The cart is what carries the rest of the page state, so make sure we come back to it
    boolean hasHgsid = (startsWith("hgsid=", queryString) ||
                        stringIn("&hgsid=", queryString) != NULL);
    if (isNotEmpty(hgsid) && !hasHgsid)
        dyStringPrintf(dy, "&hgsid=%s", hgsid);
    }
else if (isNotEmpty(hgsid))
    dyStringPrintf(dy, "?hgsid=%s", hgsid);
return dyStringCannibalize(&dy);
}

char *wikiLinkEncodeCurrentPageReturnUrl(char *hgsid)
/* Return a CGI-encoded URL for the page we are on right now, to hand to hgLogin as its
 * returnto, so login and logout come back here instead of dropping the visitor on hgSession.
 * Returns NULL when there is no page worth returning to, and the caller should then fall back
 * to its own default.  Free when done. */
{
char *scriptName = cgiScriptName();
if (isEmpty(scriptName))
    return NULL;
char *lastSlash = strrchr(scriptName, '/');
char *cgiName = (lastSlash == NULL) ? scriptName : lastSlash + 1;
if (isEmpty(cgiName))
    return NULL;
// hgLogin is where the link points, so returning to it would only loop.  hgRenderTracks just
// draws an image for another website, it is not a page anyone is sitting on.
if (sameString(cgiName, "hgLogin") || sameString(cgiName, "hgRenderTracks"))
    return NULL;

/* The query string is what makes a page like hgTrackUi or hgc work at all, since their track
 * and item parameters are not all kept in the cart.  Coming back to a URL the visitor clicked
 * themselves does no more than their reload button would, as long as it was a GET; a POST
 * cannot be replayed from a URL anyway.  hgTracks is the exception: everything it needs is in
 * the cart, and its query string can hold a one-shot zoom or drag that we do not want to
 * repeat. */
char *queryString = getenv("QUERY_STRING");
char *method = cgiRequestMethod();
if (sameString(cgiName, "hgTracks") || (method != NULL && differentWord(method, "GET")))
    queryString = NULL;

char *url = currentPageUrl(cgiName, hgsid, queryString);
char *encoded = wikiLinkEncodePageReturnUrl(url);
if (encoded == NULL && queryString != NULL)
    {
    // A stray quote or an over-long query string costs the query string, not the page
    freez(&url);
    url = currentPageUrl(cgiName, hgsid, NULL);
    encoded = wikiLinkEncodePageReturnUrl(url);
    }
freez(&url);
return encoded;
}


//#*** TODO: replace all of the non-mediawiki "returnto"s here and in hgLogin.c with a #define


char *wikiLinkUserLoginUrlReturning(char *hgsid, char *returnUrl)
/* Return the URL for the wiki user login page. */
{
struct dyString *dy = dyStringNew(256);
if (loginSystemEnabled())
    {
    dyStringPrintf(dy, "%s?hgLogin.do.displayLoginPage=1&returnto=%s", loginUrl(), returnUrl);
    } 
else 
    {
    if (! wikiLinkEnabled())
        errAbort("wikiLinkUserLoginUrl called when wiki is not enabled (specified "
            "in hg.conf).");
    // The following line of code is not used at UCSC anymore since 2014
    dyStringPrintf(dy, "http://%s/index.php?title=Special:UserloginUCSC&returnto=%s",
        wikiLinkHost(), returnUrl);
    }   
return dyStringCannibalize(&dy);
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
struct dyString *dy = dyStringNew(256);
if (loginSystemEnabled())
    {
    dyStringPrintf(dy, "%s?hgLogin.do.displayLogout=1&returnto=%s", loginUrl(), returnUrl);
    } 
else
    {
    if (! wikiLinkEnabled())
        errAbort("wikiLinkUserLogoutUrl called when wiki is not enable (specified "
            "in hg.conf).");
    dyStringPrintf(dy, "http://%s/index.php?title=Special:UserlogoutUCSC&returnto=%s",
         wikiLinkHost(), returnUrl);
    }
return dyStringCannibalize(&dy);
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
struct dyString *dy = dyStringNew(256);
char *retEnc = encodedHgSessionReturnUrl(hgsid);

if (loginSystemEnabled())
    {
    dyStringPrintf(dy, "%s?hgLogin.do.signupPage=1&returnto=%s", loginUrl(), retEnc);
    }
else
    {
    if (! wikiLinkEnabled())
        errAbort("wikiLinkUserLogoutUrl called when wiki is not enable (specified "
            "in hg.conf).");
    dyStringPrintf(dy, "http://%s/index.php?title=Special:UserlogoutUCSC&returnto=%s",
         wikiLinkHost(), retEnc);
    }
freez(&retEnc);
return dyStringCannibalize(&dy);
}

char *wikiLinkChangePasswordUrlReturning(char *hgsid, char *returnUrl)
/* Return the URL for the user change password page. */
{
struct dyString *dy = dyStringNew(256);
if (loginSystemEnabled())
    {
    dyStringPrintf(dy, "%s?hgLogin.do.changePasswordPage=1&returnto=%s", loginUrl(), returnUrl);
    }
else
    {
    if (! wikiLinkEnabled())
        errAbort("wikiLinkUserLogoutUrl called when wiki is not enable (specified "
            "in hg.conf).");
    dyStringPrintf(dy, "http://%s/index.php?title=Special:UserlogoutUCSC&returnto=%s",
         wikiLinkHost(), returnUrl);
    }
return dyStringCannibalize(&dy);
}

char *wikiLinkChangePasswordUrl(char *hgsid)
/* Return the URL for the user change password page, returning to hgSession. */
{
char *retEnc = encodedHgSessionReturnUrl(hgsid);
char *result = wikiLinkChangePasswordUrlReturning(hgsid, retEnc);
freez(&retEnc);
return result;
}

char *wikiLinkChangeEmailUrlReturning(char *hgsid, char *returnUrl)
/* Return the URL for the user change email page, or NULL if unavailable.  Supported only by
 * the hgLogin login system, and only when the login.emailLink feature is enabled in hg.conf
 * (the same switch that controls the passwordless email-link sign-in). */
{
if (!loginSystemEnabled())
    return NULL;
if (!cfgOptionBooleanDefault(CFG_LOGIN_EMAIL_LINK, FALSE))
    return NULL;
struct dyString *dy = dyStringNew(256);
dyStringPrintf(dy, "%s?hgLogin.do.changeEmailPage=1&returnto=%s", loginUrl(), returnUrl);
return dyStringCannibalize(&dy);
}

char *wikiLinkChangeEmailUrl(char *hgsid)
/* Return the URL for the user change email page, returning to hgSession, or NULL if
 * unavailable. */
{
char *retEnc = encodedHgSessionReturnUrl(hgsid);
char *result = wikiLinkChangeEmailUrlReturning(hgsid, retEnc);
freez(&retEnc);
return result;
}

char *wikiLinkChangeRecovEmailUrlReturning(char *hgsid, char *returnUrl)
/* Return the URL for the page where a user sets or changes their recovery email address, or
 * NULL if unavailable.  Supported only by the hgLogin login system, and only when the admin
 * has turned it on with login.recovEmailChange in hg.conf.  hgLogin checks the rest of what
 * the feature needs (a cookie salt to sign the confirmation link, working outbound mail, and
 * the recovEmailVerified column) and sends the user back to the login page if any is missing. */
{
if (!loginSystemEnabled())
    return NULL;
if (!cfgOptionBooleanDefault(CFG_LOGIN_RECOV_EMAIL_CHANGE, FALSE))
    return NULL;
struct dyString *dy = dyStringNew(256);
dyStringPrintf(dy, "%s?hgLogin.do.changeRecovEmailPage=1&returnto=%s", loginUrl(), returnUrl);
return dyStringCannibalize(&dy);
}

char *wikiLinkChangeRecovEmailUrl(char *hgsid)
/* Return the URL for the recovery email page, returning to hgSession, or NULL if
 * unavailable. */
{
char *retEnc = encodedHgSessionReturnUrl(hgsid);
char *result = wikiLinkChangeRecovEmailUrlReturning(hgsid, retEnc);
freez(&retEnc);
return result;
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
