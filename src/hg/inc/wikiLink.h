/* wikiLink - interoperate with a wiki site (share user identities). */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef WIKILINK_H
#define WIKILINK_H

/* hg.conf wiki parameters -- logins are disabled if any are undefined. */
#define CFG_WIKI_HOST "wiki.host"
#define CFG_WIKI_USER_NAME_COOKIE "wiki.userNameCookie"
#define CFG_WIKI_LOGGED_IN_COOKIE "wiki.loggedInCookie"
#define CFG_WIKI_SESSION_COOKIE "wiki.sessionCookie"

/* hg.conf login system parameter -- using non-wiki login system (hgLogin) if defined */
#define CFG_LOGIN_SYSTEM_NAME "login.systemName"
#define CFG_LOGIN_USE_HTTPS "login.https"
#define CFG_LOGIN_COOKIE_SALT "login.cookieSalt"
#define CFG_LOGIN_ACCEPT_ANY_ID "login.acceptAnyId"
#define CFG_LOGIN_ACCEPT_IDX "login.acceptIdx"
#define CFG_LOGIN_BASICAUTH "login.basicAuth"
#define CFG_LOGIN_RELATIVE "login.relativeLink"
/* Enables passwordless email-link sign-in and the "change email" option (default off). */
#define CFG_LOGIN_EMAIL_LINK "login.emailLink"
/* Comma-separated list of hosts that hgLogin will return a visitor to after login or logout. */
#define CFG_APPROVED_HOSTS "login.approvedReturn"
#define CFG_LOGIN_RECOV_EMAIL_CHANGE "login.recovEmailChange"

/* hg.conf central db parameters */
#define CFG_CENTRAL_DOMAIN "central.domain"
#define CFG_CENTRAL_COOKIE "central.cookie"

char *loginSystemName();
/* Return the wiki host specified in hg.conf, or NULL.  Allocd here. */

boolean loginSystemEnabled();
/* Return TRUE if login.systemName  parameter is defined in hg.conf . */

boolean loginUseHttps();
/* Return TRUE unless https is disabled in hg.conf. */

boolean loginUseBasicAuth();
/* Return TRUE if login.basicAuth is on in hg.conf . */

struct slName *loginLoginUser(char *userName, uint idx);
/* Return cookie strings to set for user so we'll recognize that user is logged in.
 * Call this after validating userName's password. */

struct slName *loginLogoutUser();
/* Return cookie strings to set (deleting the login cookies). */

struct slName *loginValidateCookies();
/* Return possibly empty list of cookie strings for the caller to set.
 * If login cookies are obsolete but (formerly) valid, the results sets updated cookies.
 * If login cookies are present but invalid, the result deletes/expires the cookies.
 * Otherwise returns NULL (no change to cookies). */

char *wikiLinkHost();
/* Return the wiki host specified in hg.conf, or NULL.  Allocd here. */

boolean wikiLinkEnabled();
/* Return TRUE if all wiki.* parameters are defined in hg.conf . */

char *wikiLinkUserName();
/* Return the user name specified in cookies from the browser, or NULL if 
 * the user doesn't appear to be logged in. */

char *getUserName();

char *wikiLinkUserId();
/* Return the user ID specified in cookies from the browser. Does not check if user is logged in.
 * To make sure that the ID is valid, call this only after you have checked with wikiLinkUserName() that the user is logged in. */

char *wikiLinkUserLoginUrl(char *hgsid);
/* Return the URL for the wiki user login page. */

char *wikiLinkUserLoginUrlReturning(char *hgsid, char *returnUrl);
/* Return the URL for the wiki user login page. */

char *wikiLinkEncodeReturnUrl(char *hgsid, char *cgiName, char* urlSuffix);
/* Return a CGI-encoded URL with hgsid to a CGI.  Free when done. */

boolean loginReturnUrlIsAcceptable(char *returnUrl);
/* Return TRUE if hgLogin will accept returnUrl as its returnto: an http or https URL with no
 * character that could break out of the page hgLogin prints it into, on an approved host.
 * hgLogin checks this on the way in; callers that build a returnto check it on the way out,
 * so that a URL hgLogin would refuse becomes a plain login link instead of an error page. */

char *wikiLinkEncodePageReturnUrl(char *url);
/* Return url CGI-encoded for use as a returnto, or NULL if hgLogin would refuse it.
 * Free when done. */

char *wikiLinkEncodeCurrentPageReturnUrl(char *hgsid);
/* Return a CGI-encoded URL for the page we are on right now, to hand to hgLogin as its
 * returnto, so login and logout come back here instead of dropping the visitor on hgSession.
 * Returns NULL when there is no page worth returning to, and the caller should then fall back
 * to its own default.  Free when done. */

char *wikiLinkUserLogoutUrl(char *hgsid);
/* Return the URL for the wiki user logout page. */

char *wikiLinkUserLogoutUrlReturning(char *hgsid, char *returnUrl);
/* Return the URL for the wiki user logout page. */

char *wikiLinkUserSignupUrl(char *hgsid);
/* Return the URL for the user signup  page. */

char *wikiLinkChangePasswordUrl(char *hgsid);
/* Return the URL for the user change password page, returning to hgSession. */

char *wikiLinkChangePasswordUrlReturning(char *hgsid, char *returnUrl);
/* Return the URL for the user change password page. */

char *wikiLinkChangeEmailUrl(char *hgsid);
/* Return the URL for the user change email page, returning to hgSession, or NULL if
 * unavailable. */

char *wikiLinkChangeEmailUrlReturning(char *hgsid, char *returnUrl);
/* Return the URL for the user change email page, or NULL if unavailable. */

char *wikiLinkChangeRecovEmailUrl(char *hgsid);
/* Return the URL for the user recovery email page, or NULL if unavailable. */

char *wikiLinkChangeRecovEmailUrlReturning(char *hgsid, char *returnUrl);
/* Return the URL for the user recovery email page, coming back to returnUrl. */

char *wikiServerAndCgiDir();
/* return the current full absolute URL up to the CGI name, like
 * http://genome.ucsc.edu/cgi-bin/. If login.relativeLink=on is
 * set, return only /cgi-bin/. Takes care of of non-root location of cgi-bin
 * and https. Result has to be free'd. */

void wikiFixLogoutLinkWithJs();
/* HTTP Basic Auth requires a strange hack to logout. This code prints a script 
 * that fixes an html link with id=logoutLink */

#endif /* WIKILINK_H */
