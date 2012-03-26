/* hgLoginLink - interoperate with a hgLogin site (share user identities). */

#ifndef HGLOGINLINK_H
#define HGLOGINLINK_H

/* hg.conf hgLogin parameters -- hgLoginLink is disabled if any are undefined. */
#define CFG_HGLOGIN_HOST "hgLogin.host"
#define CFG_HGLOGIN_USER_NAME_COOKIE "hgLogin.userNameCookie"
#define CFG_HGLOGIN_LOGGED_IN_COOKIE "hgLogin.loggedInCookie"
#define CFG_HGLOGIN_SESSION_COOKIE "hgLogin.sessionCookie"

char *hgLoginLinkHost();
/* Return the hgLogin host specified in hg.conf, or NULL.  Allocd here. */

boolean hgLoginLinkEnabled();
/* Return TRUE if all hgLogin.* parameters are defined in hg.conf . */

char *hgLoginLinkUserName();
/* Return the user name specified in cookies from the browser, or NULL if 
 * the user doesn't appear to be logged in. */

char *hgLoginLinkUserLoginUrl(int hgsid);
/* Return the URL for the hgLogin user hgLogin page. */

char *hgLoginLinkUserLogoutUrl(int hgsid);
/* Return the URL for the hgLogin user logout page. */

#endif /* HGLOGINLINK_H */
