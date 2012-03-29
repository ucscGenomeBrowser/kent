/* hgLoginLink - interoperate with a hgLogin site (share user identities). */

#include "common.h"
#include "hash.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "hgConfig.h"
#include "hui.h"
#include "cart.h"
#include "web.h"
#include "hgLoginLink.h"


char *hgLoginLinkHost()
/* Return the hgLogin host specified in hg.conf, or NULL.  Allocd here. */
{
return cloneString(cfgOption(CFG_HGLOGIN_HOST));
}

boolean hgLoginLinkEnabled()
/* Return TRUE if all hgLogin.* parameters are defined in hg.conf . */
{
return ((cfgOption(CFG_HGLOGIN_HOST) != NULL) &&
	(cfgOption(CFG_HGLOGIN_USER_NAME_COOKIE) != NULL) &&
	(cfgOption(CFG_HGLOGIN_LOGGED_IN_COOKIE) != NULL));
}

static char *hgLoginLinkUserNameCookie()
/* Return the cookie name specified in hg.conf as the hgLogin user name cookie. */
{
return cfgOption(CFG_HGLOGIN_USER_NAME_COOKIE);
}

static char *hgLoginLinkLoggedInCookie()
/* Return the cookie name specified in hg.conf as the hgLogin logged-in cookie. */
{
return cfgOption(CFG_HGLOGIN_LOGGED_IN_COOKIE);
}

char *hgLoginLinkUserName()
/* Return the user name specified in cookies from the browser, or NULL if 
 * the user doesn't appear to be logged in. */
{
if (hgLoginLinkEnabled())
    {
    char *hgLoginUserName = findCookieData(hgLoginLinkUserNameCookie());
    char *hgLoginLoggedIn = findCookieData(hgLoginLinkLoggedInCookie());

    if (isNotEmpty(hgLoginLoggedIn) && isNotEmpty(hgLoginUserName))
	{
	return cloneString(hgLoginUserName);
	}
    }
else
    errAbort("hgLoginLinkUserName called when hgLogin is not enabled (specified "
	     "in hg.conf).");
return NULL;
}

static char *encodedHgSessionReturnUrl(int hgsid)
/* Return a CGI-encoded hgSession URL with hgsid.  Free when done. */
{
char retBuf[1024];
safef(retBuf, sizeof(retBuf), "http://%s/cgi-bin/hgSession?hgsid=%d",
      cgiServerNamePort(), hgsid);
return cgiEncode(retBuf);
}

char *hgLoginLinkUserLoginUrl(int hgsid)
/* Return the URL for the hgLogin user hgLogin page. */
{
char buf[2048];
char *retEnc = encodedHgSessionReturnUrl(hgsid);
if (! hgLoginLinkEnabled())
    errAbort("hgLoginLinkUserLoginUrl called when hgLogin is not enabled (specified "
	     "in hg.conf).");
safef(buf, sizeof(buf),
      /* "http://%s/index.php?title=Special:UserhgLoginUCSC&returnto=%s",
 * */
      "http://%s/cgi-bin/hgLogin?hgLogin.do.displayLoginPage=1&returnto=%s",
      hgLoginLinkHost(), retEnc);
freez(&retEnc);
/* DEBUG: */ printf("<BR>DEBUG in hgLoginLink Z: cloneString(buf) is%s<BR>",cloneString(buf));

return(cloneString(buf));
}

char *hgLoginLinkUserLogoutUrl(int hgsid)
/* Return the URL for the hgLogin user logout page. */
{
char buf[2048];
char *retEnc = encodedHgSessionReturnUrl(hgsid);
if (! hgLoginLinkEnabled())
    errAbort("hgLoginLinkUserLogoutUrl called when hgLogin is not enable (specified "
	     "in hg.conf).");
safef(buf, sizeof(buf),
   /*   "http://%s/index.php?title=Special:UserlogoutUCSC&returnto=%s",
 *   */
      "http://%s/cgi-bin/hgLogin?hgLogin.do.displayLogout=1&returnto=%s",
      hgLoginLinkHost(), retEnc);
freez(&retEnc);
return(cloneString(buf));
}
