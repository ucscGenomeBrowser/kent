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

#endif /* CART_H */

