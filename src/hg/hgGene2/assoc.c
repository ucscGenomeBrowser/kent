#include "common.h"
#include "jksql.h"


char *getAssoc(struct sqlConnection *conn, char *table, char *field,  char *whereField, char *whereValue)
{
char query[4096];
char result[4096];

sqlSafef(query, sizeof query, "select %s from %s where %s  = '%s'", 
    field, table, whereField, whereValue);

return cloneString(sqlQuickQuery(conn, query, result, sizeof result));
}

char *assocGetRefSeq(struct sqlConnection *conn, char *id)
{
return getAssoc(conn, "gencodeToRefSeqV32", "rnaAcc", "transcriptId", id);
}

