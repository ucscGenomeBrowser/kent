/* wikiLink - interoperate with a wiki site (share user identities). */

#ifndef WIKILINK_H
#define WIKILINK_H

/* hg.conf wiki parameters -- wikiLink is disabled if any are undefined. */
#define CFG_WIKI_HOST "wiki.host"
#define CFG_WIKI_USER_NAME_COOKIE "wiki.userNameCookie"
#define CFG_WIKI_LOGGED_IN_COOKIE "wiki.loggedInCookie"

char *wikiLinkHost();
/* Return the wiki host specified in hg.conf, or NULL.  Allocd here. */

boolean wikiLinkEnabled();
/* Return TRUE if all wiki.* parameters are defined in hg.conf . */

char *wikiLinkUserName();
/* Return the user name specified in cookies from the browser, or NULL if 
 * the user doesn't appear to be logged in. */

char *wikiLinkUserLoginUrl();
/* Return the URL for the wiki user login page. */

char *wikiLinkUserLogoutUrl();
/* Return the URL for the wiki user logout page. */

#endif /* WIKILINK_H */
