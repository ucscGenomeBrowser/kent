/* code to support suggesting genes given a prefix typed by the user. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "suggest.h"


char *connGeneSuggestTable(struct sqlConnection *conn)
// return name of gene suggest table if this connection has tables to support gene autocompletion, NULL otherwise
{
if(sqlTableExists(conn, "knownCanonical"))
    return "knownCanonical";
else if(sqlTableExists(conn, "refGene"))
    return "refGene";
else
    return NULL;
}

boolean assemblySupportsGeneSuggest(char *database)
// return true if this assembly has tables to support gene autocompletion
{
struct sqlConnection *conn = hAllocConn(database);
char *table = connGeneSuggestTable(conn);
hFreeConn(&conn);
return table != NULL;
}

char *assemblyGeneSuggestTrack(char *database)
// return name of gene suggest track if this assembly has tables to support gene autocompletion, NULL otherwise
// Do NOT free returned string.
{
struct sqlConnection *conn = hAllocConn(database);
char *table = connGeneSuggestTable(conn);
hFreeConn(&conn);
if(table != NULL)
    {
    if(sameString(table, "knownCanonical"))
        return "knownGene";
    else
        return "refGene";
    }
else
    return NULL;
}
