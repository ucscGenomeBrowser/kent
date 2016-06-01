/* autoUpgrade.c -- if possible, add a new column to an existing table.  If it fails,
 * try again every few minutes in case permissions are granted. */

/* Copyright (C) 2016 The Regents of the University of California
 * See README in this or parent directory for licensing information. */

#ifndef AUTOUPGRADE_H
#define AUTOUPGRADE_H

#include "jksql.h"

void autoUpgradeTableAddColumn(struct sqlConnection *conn, char *tableName, char *columnName,
                               char *type, boolean notNull, char *defaultVal);
/* Try to upgrade the table by adding column in a safe way handling success, failures
 * and retries with multiple CGIs running.
 * type must be a valid SQL type string like "varchar(255)", "longblob", "tinyint" etc.
 * If notNull is TRUE then 'NOT NULL' will be added to the column definition.
 * defaultVal must be a valid SQL expression (quoted if necessary) for type, for example
 * "''" for a string type, "0.0" for float, or "NULL" if notNull is FALSE. */

#endif /* AUTOUPGRADE_H */
