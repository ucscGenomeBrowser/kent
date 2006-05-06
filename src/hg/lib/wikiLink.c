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

static char const rcsid[] = "$Id: wikiLink.c,v 1.2 2006/05/06 00:01:23 angie Exp $";

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

char *wikiLinkUserLoginUrl(int hgsid)
/* Return the URL for the wiki user login page. */
{
char buf[2048];
if (! wikiLinkEnabled())
    errAbort("wikiLinkUserLoginUrl called when wiki is not enabled (specified "
	     "in hg.conf).");
safef(buf, sizeof(buf),
      "http://%s/index.php?title=Special:UserloginUCSC"
      "&returnto=http://%s/cgi-bin/hgSession?hgsid=%d",
      wikiLinkHost(), cgiServerName(), hgsid);
return(cloneString(buf));
}

char *wikiLinkUserLogoutUrl(int hgsid)
/* Return the URL for the wiki user logout page. */
{
char buf[2048];
if (! wikiLinkEnabled())
    errAbort("wikiLinkUserLogoutUrl called when wiki is not enable (specified "
	     "in hg.conf).");
safef(buf, sizeof(buf),
      "http://%s/index.php?title=Special:UserlogoutUCSC"
      "&returnto=http://%s/cgi-bin/hgSession?hgsid=%d",
      wikiLinkHost(), cgiServerName(), hgsid);
return(cloneString(buf));
}
