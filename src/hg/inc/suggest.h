/* code to support suggesting genes given a prefix typed by the user. */

#ifndef SUGGEST_H
#define SUGGEST_H

#include "common.h"
#include "jksql.h"
#include "hdb.h"

boolean connSupportsGeneSuggest(struct sqlConnection *conn);
// return true if this connection has tables to support gene autocompletion

boolean assemblySupportsGeneSuggest(char *database);
// return true if this assembly has tables to support gene autocompletion

#endif /* SUGGEST_H */
