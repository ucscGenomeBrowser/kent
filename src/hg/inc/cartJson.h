/* cartJson - parse and execute JSON commands to update cart and/or return cart data as JSON. */

#ifndef CARTJSON_H
#define CARTJSON_H

#include "cart.h"
#include "hash.h"
#include "jsonWrite.h"

#define CARTJSON_COMMAND "cjCmd"

struct cartJson
    // Use the cart to perform commands dispatched from handlerHash; print results with jsonWrite.
    {
    struct cart *cart;
    struct hash *handlerHash;
    struct jsonWrite *jw;
    };

struct cartJson *cartJsonNew(struct cart *cart);
/* Allocate and return a cartJson object with default handlers. */

typedef void CartJsonHandler(struct cartJson *cj, struct hash *paramHash);
/* Implementation of some command; paramHash associates parameter names with
 * jsonElement values. */

char *cartJsonOptionalParam(struct hash *paramHash, char *name);
/* Convenience function for a CartJsonHandler function: Look up name in paramHash.
 * Return the string contained in its jsonElement value, or NULL if not found. */

char *cartJsonRequiredParam(struct hash *paramHash, char *name, struct jsonWrite *jw,
                            char *context);
/* Convenience function for a CartJsonHandler function: Look up name in paramHash.
 * If found, return the string contained in its jsonElement value.
 * If not, write an error message (using context) and return NULL. */

void cartJsonRegisterHandler(struct cartJson *cj, char *command, CartJsonHandler *handler);
/* Associate handler with command; when javascript sends command, handler will be executed. */

void cartJsonExecute(struct cartJson *cj);
/* Get commands from cgi, print Content-type, execute commands, print results as JSON. */

void cartJsonFree(struct cartJson **pCj);
/* Close **pCj's contents and nullify *pCj. */

void cartJsonChangeDb(struct cartJson *cj, struct hash *paramHash);
/* Change db to new value, update cart and print JSON of new position & gene suggest track. */

void cartJsonChangeOrg(struct cartJson *cj, struct hash *paramHash);
/* Change org to new value, update cart and print JSON of new db menu, new position etc. */

void cartJsonChangeClade(struct cartJson *cj, struct hash *paramHash);
/* Change clade to new value, update cart, and print JSON of new org & db menus, new position etc */

void cartJsonGetGroupsTracksTables(struct cartJson *cj, struct hash *paramHash);
/* Print info necessary for group/track/table menus. */

#endif /* CARTJSON_H */
