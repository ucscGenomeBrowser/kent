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

void cartFree(struct cart **pCart);
/* Free up cart and save it to database. */

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
/* Retrieve cart boolean.   Since CGI booleans simply
 * don't exist when they're false - which messes up
 * cart persistence,  we have to jump through some
 * hoops.   We prepend 'bool.' to the cart representation
 * of the variable to separate it from the cgi
 * representation that may or may not be transiently
 * in the cart.
 *
 * You may need to either call this function or
 * cartCgiBoolean depending on the context. 
 * This routine will ignore cgi boolean variables
 * entirely unless they are first retrieved with
 * cartCgiBoolean(). */

boolean cartUsualBoolean(struct cart *cart, char *var, boolean usual);
/* Return variable value if it exists or usual if not. */

void cartSetBoolean(struct cart *cart, char *var, boolean val);
/* Set boolean value. */

boolean cartCgiBoolean(struct cart *cart, char *var);
/* Return boolean variable from CGI.  Remove it from cart.
 * CGI booleans alas do not store cleanly in cart. */

void cartSaveSession(struct cart *cart);
/* Save session in a hidden variable. This needs to be called
 * somewhere inside of form.  If you don't and the user has
 * more than one browser window up, it will get messy. */

void cartDump(struct cart *cart);
/* Dump contents of cart. */

void cartHtmlShell(char *title, void (*doMiddle)(struct cart *cart), char *cookieName, char **exclude);
/* Load cart from cookie and session cgi variable.  Write web-page preamble, call doMiddle
 * with cart, and write end of web-page.   Exclude may be NULL.  If it exists it's a
 * comma-separated list of variables that you don't want to save in the cart between
 * invocations of the cgi-script. */

#endif /* CART_H */

