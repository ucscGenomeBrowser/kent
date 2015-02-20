/* tablesTables - this module deals with two types of tables SQL tables in a database,
 * and fieldedTable objects in memory. It has routines to do sortable, filterable web
 * displays on tables. */

#ifndef TABLESTABLES_H
#define TABLESTABLES_H

struct fieldedTable *fieldedTableFromDbQuery(struct sqlConnection *conn, char *query);
/* Return fieldedTable from a database query */

#endif /* TABLESTABLES_H */

