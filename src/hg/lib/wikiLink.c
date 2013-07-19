/* wikiLink - interoperate with a wiki site (share user identities). */

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
/* Return the wiki host specified in hg.conf, or NULL.  Allocd here. */
{
return cloneString(cfgOption(CFG_WIKI_HOST));
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

static char *encodedHgSessionReturnUrl(int hgsid)
/* Return a CGI-encoded hgSession URL with hgsid.  Free when done. */
{
char retBuf[1024];
safef(retBuf, sizeof(retBuf), "http%s://%s/cgi-bin/hgSession?hgsid=%d",
      cgiAppendSForHttps(), cgiServerNamePort(), hgsid);
return cgiEncode(retBuf);
}

char *wikiLinkUserLoginUrl(int hgsid)
/* Return the URL for the wiki user login page. */
{
char buf[2048];
char *retEnc = encodedHgSessionReturnUrl(hgsid);
if (loginSystemEnabled())
    {
    if (! wikiLinkEnabled())
        errAbort("wikiLinkUserLoginUrl called when login system is not enabled "
           "(specified in hg.conf).");
    safef(buf, sizeof(buf),
        "http%s://%s/cgi-bin/hgLogin?hgLogin.do.displayLoginPage=1&returnto=%s",
        cgiAppendSForHttps(), wikiLinkHost(), retEnc);
    } 
else 
    {
    if (! wikiLinkEnabled())
        errAbort("wikiLinkUserLoginUrl called when wiki is not enabled (specified "
            "in hg.conf).");
    safef(buf, sizeof(buf),
        "http://%s/index.php?title=Special:UserloginUCSC&returnto=%s",
        wikiLinkHost(), retEnc);
    }   
freez(&retEnc);
return(cloneString(buf));
}

char *wikiLinkUserLogoutUrl(int hgsid)
/* Return the URL for the wiki user logout page. */
{
char buf[2048];
char *retEnc = encodedHgSessionReturnUrl(hgsid);

if (loginSystemEnabled())
    {
    if (! wikiLinkEnabled())
        errAbort("wikiLinkUserLogoutUrl called when login system is not enabled "
            "(specified in hg.conf).");
    safef(buf, sizeof(buf),
        "http%s://%s/cgi-bin/hgLogin?hgLogin.do.displayLogout=1&returnto=%s",
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

char *wikiLinkUserSignupUrl(int hgsid)
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

char *wikiLinkChangePasswordUrl(int hgsid)
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

