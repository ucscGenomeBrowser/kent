/* Copyright (C) 2017 The Regents of the University of California
 * See README in this or parent directory for licensing information. */

/* handlerList - a list of handler functions for translating identifiers
 * from different external databases to browser positions.  Used by
 * linkInHandlers.c. */

#ifndef HANDLERLIST_H
#define HANDLERLIST_H

#ifndef JKSQL_H
#include "jksql.h"
#endif

struct linkInResult
    {
    struct linkInResult *next;
    char *db;
    char *position;
    char *trackToView; // optional track name to turn on
    };

struct linkInResult *uniProtHandler(struct sqlConnection *conn, char *id);
/* Handler for translating UniProtKB identifiers to browser positions */

#endif /* HANDLERLIST_H */
