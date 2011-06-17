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

static char const rcsid[] = "$Id: wikiLink.c,v 1.3 2006/10/09 21:08:01 angie Exp $";

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
safef(retBuf, sizeof(retBuf), "http://%s/cgi-bin/hgSession?hgsid=%d",
      cgiServerNamePort(), hgsid);
return cgiEncode(retBuf);
}

char *wikiLinkUserLoginUrl(int hgsid)
/* Return the URL for the wiki user login page. */
{
char buf[2048];
char *retEnc = encodedHgSessionReturnUrl(hgsid);
if (! wikiLinkEnabled())
    errAbort("wikiLinkUserLoginUrl called when wiki is not enabled (specified "
	     "in hg.conf).");
safef(buf, sizeof(buf),
      "http://%s/index.php?title=Special:UserloginUCSC&returnto=%s",
      wikiLinkHost(), retEnc);
freez(&retEnc);
return(cloneString(buf));
}

char *wikiLinkUserLogoutUrl(int hgsid)
/* Return the URL for the wiki user logout page. */
{
char buf[2048];
char *retEnc = encodedHgSessionReturnUrl(hgsid);
if (! wikiLinkEnabled())
    errAbort("wikiLinkUserLogoutUrl called when wiki is not enable (specified "
	     "in hg.conf).");
safef(buf, sizeof(buf),
      "http://%s/index.php?title=Special:UserlogoutUCSC&returnto=%s",
      wikiLinkHost(), retEnc);
freez(&retEnc);
return(cloneString(buf));
}
