/* rqlToSql.h - translation from parsed RQL (Ra Query Language - similar in syntax to SQL)
 * statements into SQL statements. */

#ifndef RQLTOSQL_H
#define RQLTOSQL_H

#include "rql.h"

char *rqlStatementToSql (struct rqlStatement *rql, struct slName *allFields);
/* Convert an rqlStatement into an equivalent SQL statement.  This requires a list
 * of all fields available, as RQL select statements can include wildcards in the field
 * list (e.g., 'select abc* from table' retrieves all fields whose names begin with abc). */

char *rqlParseToSqlWhereClause (struct rqlParse *p, boolean inComparison);
/* Convert an rqlParse into an equivalent SQL where clause (without the "where").
 * The RQL construction of just naming a field is converted to a SQL "is not NULL".
 * Leaks memory from a series of constructed and discarded strings. */

#endif /* RQLTOSQL_H */
