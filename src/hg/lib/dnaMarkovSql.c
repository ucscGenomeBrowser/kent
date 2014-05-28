/* Load markov objects from sql db. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "dnaMarkovSql.h"
#include "dnaseq.h"
#include "dnaMarkov.h"
#include "hdb.h"

boolean loadMark2(struct sqlConnection *conn, char *table, char *chrom, unsigned start, unsigned end, double mark2[5][5][5])
// Load 2nd-order markov model from given table
{
boolean found = FALSE;
int rowOffset;
struct sqlResult *sr = hRangeQuery(conn, table, chrom, start, end, NULL, &rowOffset);
char **row;
// XXXX use "best" model instead of first if there are multiple (i.e. maximize overlap with start/end).
if((row = sqlNextRow(sr)) != NULL)
    {
    dnaMark2Deserialize(row[rowOffset + 3], mark2);
    found = TRUE;
    }
sqlFreeResult(&sr);
return found;
}
