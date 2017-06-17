/* Copyright (C) 2017 The Regents of the University of California
 * See README in this or parent directory for licensing information. */

/* linkInHandler - supports registering and invoking handlers for linking
 * identifiers to positions via a resource.
 * To define a new handler, add it to handlerList.c and then add a new
 * handler registration line to registerLinkInHandlers().  A single
 * resource handler can be run using the name it's registered by in a call
 * to checkLinkInHandlerForResource.  All handlers can be checked in sequence
 * with checkAllLinkInHandlers(); */

#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "linkInHandlers.h"
#include "handlerList.h"

struct hash *gLinkInHandlers = NULL;


void registerLinkInHandlers()
/* Register handlers for linking in IDs from all implemented databases */
{
//registerLinkInHandler("name", handlerFunction);
registerLinkInHandler("uniProt", uniProtHandler);
}


void registerLinkInHandler(char *name, LinkInHandler handler)
/* Add a linkIn handler for a particular database name.
 * Names are case-sensitive. */
{
char *hashName = cloneString(name);
touppers(hashName);
if (gLinkInHandlers == NULL)
    gLinkInHandlers = newHash(6);
if (hashLookupUpperCase(gLinkInHandlers, hashName))
    warn("Handler duplicated for database %s", name);
else
    {
    struct linkInHandlerEntry *newEntry;
    AllocVar(newEntry);
    newEntry->next = NULL;
    newEntry->resourceName = cloneString(name);
    newEntry->handler = handler;
    hashAdd(gLinkInHandlers, hashName, newEntry);
    }
}


struct linkInResult *checkLinkInHandlerForResource(char *linkInResource, char *linkInId)
/* Attempt to locate a handler for the provided linkInResource name, and then use
 * it to locate position results for the provided linkInId identifier.  E.g.,
 * linkInResource='uniProt' and linkInId='Q6GZX4' */
{
struct linkInHandlerEntry *thisEntry = hashFindValUpperCase(gLinkInHandlers, linkInResource);
if (thisEntry == NULL)
    {
    warn("No handler found for resource %s", linkInResource);
    return NULL;
    }
else
    {
    struct sqlConnection *conn = sqlConnect(NULL);
    return thisEntry->handler(conn, linkInId);
    }
}


struct linkInResult *checkAllLinkInHandlers(char *linkInId)
/* Iterate over the list of registered linkIn handlers, checking all of them
 * for position results for the supplied linkInId identifier */
{
struct linkInResult *results = NULL;
struct hashCookie hashIterator = hashFirst(gLinkInHandlers);
struct sqlConnection *conn = sqlConnect(NULL);
struct hashEl *thisHandler = NULL;
while ((thisHandler = hashNext(&hashIterator)) != NULL)
    {
    struct linkInHandlerEntry *entry = thisHandler->val;
    struct linkInResult *moreResults = entry->handler(conn, linkInId);
    results = slCat(results, moreResults);
    }
return results;
}


char **listLinkInHandlers()
/* Return the list of registered resource handler names.
 * Leaks memory from the hashEl list used to get the list
 * of names. Last name in the list is a NULL pointer. */
{
struct hashEl *handler = hashElListHash(gLinkInHandlers);
if (handler == NULL)
    return NULL;
int elCount = slCount(handler), i;
char **results = NULL;
AllocArray(results, elCount+1);
for (i=0; i<elCount; i++)
    {
    results[i] = cloneString(handler->name);
    }
return results;
}
