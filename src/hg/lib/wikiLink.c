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

char *wikiLinkHost()
/* Return the wiki host specified in hg.conf, or NULL.  Allocd here. 
 * Returns hostname from http request if hg.conf entry is HTTPHOSTNAME.
 * */
{
char *wikiHost = cfgOption(CFG_WIKI_HOST);
if ((wikiHost!=NULL) && sameString(wikiHost, "HTTPHOST"))
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
safef(buf, sizeof(buf), "http%s://%s/cgi-bin/hgLogin",
      loginUseHttps() ? "s" : "", wikiLinkHost());
return cloneString(buf);
}

boolean wikiLinkEnabled()
/* Return TRUE if all wiki.* parameters are defined in hg.conf . */
{
return ((cfgOption(CFG_WIKI_HOST) != NULL) &&
	(cfgOption(CFG_WIKI_USER_NAME_COOKIE) != NULL) &&
	(cfgOption(CFG_WIKI_LOGGED_IN_COOKIE) != NULL));
}

static char *wikiLinkUserNameCookie()
/* Return the cookie name specified in hg.conf as the wiki user name cookie. */
{
return cfgOption(CFG_WIKI_USER_NAME_COOKIE);
}

static char *wikiLinkLoggedInCookie()
/* Return the cookie name specified in hg.conf as the wiki logged-in cookie. */
{
return cfgOption(CFG_WIKI_LOGGED_IN_COOKIE);
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
        "%s?hgLogin.do.displayLoginPage=1&returnto=%s",
        loginUrl(), returnUrl);
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
    if (! wikiLinkEnabled())
        errAbort("wikiLinkUserSignupUrl called when login system is not enabled "
            "(specified in hg.conf).");
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
    if (! wikiLinkEnabled())
        errAbort("wikiLinkChangePasswordUrl called when login system is not enabled "
            "(specified in hg.conf).");
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

