/* code to support suggesting genes given a prefix typed by the user. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef SUGGEST_H
#define SUGGEST_H

#include "common.h"
#include "jksql.h"
#include "hdb.h"

char *connGeneSuggestTable(struct sqlConnection *conn);
// return name of gene suggest table if this connection has tables to support gene autocompletion, NULL otherwise

boolean assemblySupportsGeneSuggest(char *database);
// return true if this assembly has tables to support gene autocompletion

char *assemblyGeneSuggestTrack(char *database);
// return name of gene suggest track if this assembly has tables to support gene autocompletion, NULL otherwise
// Do NOT free returned string.


#endif /* SUGGEST_H */
