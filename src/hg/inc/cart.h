/* cart - stuff to manage variables that persist from 
 * one invocation of a cgi script to another (variables
 * that are carted around).  */

#ifndef CART_H
#define CART_H

struct cart
/* A cart of settings that persist. */
   {
   struct cart *next;	/* Next in list. */
   unsigned int userId;	/* User ID in database. */
   unsigned int sessionId;	/* Session ID in database. */
   struct hash *hash;	/* String valued hash. */
   struct hash *exclude;	/* Null valued hash of variables not to save. */
   struct cartDb *userInfo;	/* Info on user. */
   struct cartDb *sessionInfo;	/* Info on session. */
   };

struct cart *cartNew(unsigned int userId, unsigned int sessionId, char **exclude);
/* Load up cart from user & session id's.  Exclude is a null-terminated list of
 * strings to not include */

void cartCheckout(struct cart **pCart);
/* Save cart to database and free it up. */

char *cartSessionVarName();
/* Return name of CGI session ID variable. */

unsigned int cartSessionId(struct cart *cart);
/* Return session id. */

char *cartSidUrlString(struct cart *cart);
/* Return session id string as in hgsid=N . */

unsigned int cartUserId(struct cart *cart);
/* Return session id. */

void cartRemove(struct cart *cart, char *var);
/* Remove variable from cart. */

char *cartString(struct cart *cart, char *var);
/* Return string valued cart variable. */

char *cartOptionalString(struct cart *cart, char *var);
/* Return string valued cart variable or NULL if it doesn't exist. */

char *cartUsualString(struct cart *cart, char *var, char *usual);
/* Return variable value if it exists or usual if not. */

void cartSetString(struct cart *cart, char *var, char *val);
/* Set string valued cart variable. */

int cartInt(struct cart *cart, char *var);
/* Return int valued variable. */

int cartIntExp(struct cart *cart, char *var);
/* Return integer valued expression in variable. */

int cartUsualInt(struct cart *cart, char *var, int usual);
/* Return variable value if it exists or usual if not. */

void cartSetInt(struct cart *cart, char *var, int val);
/* Set integer value. */

double cartDouble(struct cart *cart, char *var);
/* Return double valued variable. */

double cartUsualDouble(struct cart *cart, char *var, double usual);
/* Return variable value if it exists or usual if not. */

void cartSetDouble(struct cart *cart, char *var, double val);
/* Set double value. */

boolean cartBoolean(struct cart *cart, char *var);
/* Retrieve cart boolean. */

boolean cartUsualBoolean(struct cart *cart, char *var, boolean usual);
/* Return variable value if it exists or usual if not.  */

void cartSetBoolean(struct cart *cart, char *var, boolean val);
/* Set boolean value. */

boolean cartCgiBoolean(struct cart *cart, char *var);
/* Return boolean variable from CGI.  Remove it from cart.
 * CGI booleans alas do not store cleanly in cart. */

void cartSaveSession(struct cart *cart);
/* Save session in a hidden variable. This needs to be called
 * somewhere inside of form or bad things will happen. */

void cartDump(struct cart *cart);
/* Dump contents of cart. */

void cartResetInDb(char *cookieName);
/* Clear cart in database. */

void cartEmptyShell(void (*doMiddle)(struct cart *cart), char *cookieName, char **exclude);
/* Get cart and cookies and set up error handling, but don't start writing any
 * html yet. The doMiddleFunction has to call cartHtmlStart(title), and
 * cartHtmlEnd(), as well as writing the body of the HTML. */

void cartHtmlStart(char *title);
/* Write HTML header and put in normal error handler. Needed with cartEmptyShell,
 * but not cartHtmlShell. */

void cartWebStart(char *format, ...);
/* Print out pretty wrapper around things when working
 * from cart. */

void cartHtmlEnd();
/* Write out HTML footer and get rid or error handler. Needed with cartEmptyShell,
 * but not cartHtmlShell. */

void cartHtmlShell(char *title, void (*doMiddle)(struct cart *cart), char *cookieName, char **exclude);
/* Load cart from cookie and session cgi variable.  Write web-page preamble, call doMiddle
 * with cart, and write end of web-page.   Exclude may be NULL.  If it exists it's a
 * comma-separated list of variables that you don't want to save in the cart between
 * invocations of the cgi-script. */

#endif /* CART_H */

