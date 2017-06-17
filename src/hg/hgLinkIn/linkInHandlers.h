/* Copyright (C) 2017 The Regents of the University of California
 * See README in this or parent directory for licensing information. */

/* linkInHandler - supports registering and invoking handlers for linking
 * identifiers to positions via a resource.
 * To define a new handler, add it to handlerList.c and then add a new
 * handler registration line to registerLinkInHandlers().  A single
 * resource handler can be run using the name it's registered by in a call
 * to checkLinkInHandlerForResource.  All handlers can be checked in sequence
 * with checkAllLinkInHandlers(); */

#ifndef LINKINHANDLER_H
#define LINKINHANDLER_H

#ifndef JKSQL_H
#include "jksql.h"
#endif

#ifndef HANDLERLIST_H
#include "handlerList.h"
#endif

typedef struct linkInResult *(*LinkInHandler)(struct sqlConnection *conn, char *id);

struct linkInHandlerEntry
    {
    struct linkInHandlerEntry *next;
    char *resourceName;
    LinkInHandler handler;
    };


void registerLinkInHandler(char *name, LinkInHandler handler);
/* Add a linkIn handler for a particular database name.
 * Names are case-sensitive. */

void registerLinkInHandlers();
/* Register all linkIn handlers to prepare for searches */

struct linkInResult *checkLinkInHandlerForResource(char *linkInResource, char *linkInId);
/* Search a particular resource handler for an identifier */

struct linkInResult *checkAllLinkInHandlers(char *linkInId);
/* Search all registered resource handlers for an identifier */

char **listLinkInHandlers();
/* Return the list of registered resource handler names.  Last name in
 * the list is a NULL pointer. */

#endif /* LINKINHANDLER_H */
