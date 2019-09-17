/* Test sqlCheck anti-sql-injection functions in jksql.c */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "jksql.h"

void usage()
/* display usage message */
{
printf(
 "sqlCheck - test anti-sql-injection functions\n"
 "\n"
 "Usage:\n"
 " sqlCheck type value\n"
 "\n"
 "where type can be \n"
 "  ID for Identifier -- these should be alphanumeric, underscore, and period but not allow spaces, quotes, etc\n"
 "  IL for Identifier List -- these should be identifiers comma-separated list. Currently '.' is allowed as a special exception.\n"
 "  ES for Escape the string using mysql -- this should allow all and escape all characters except 0 (which is useful for binary but not strings)\n"
 "  EE for Escape Every evil character -- this should append escaped all forbidden characters except 0 (which is useful for binary but not strings)\n"
 "\n"
 );
exit(1);
}

int main(int argc, char *argv[])
{
if (argc != 3)
    usage();

char *theType = argv[1];
char *value   = argv[2];

if (sameString(theType,"ID"))
    {
    printf("SELECT * FROM %s;\n", sqlCheckIdentifier(value));  // typically a table name or field name etc, is not quoted or escaped.
    }
else if (sameString(theType,"IL"))
    {
    printf("SELECT %s FROM table;\n", sqlCheckIdentifiersList(value));  // typically a comma-separated list of table or field names.
    }
else if (sameString(theType,"ES"))
    {
    struct dyString *dy = dyStringNew(0);
    sqlDyStringPrintf(dy, "INSERT INTO TABLE VALUES ('");
    sqlDyAppendEscaped(dy, value);  // typically used when there are unusual characters that need escaping.
    dyStringAppend(dy, "');");
    printf("%s\n", dy->string);
    }
else if (sameString(theType,"EE"))
    {
    struct dyString *dy = dyStringNew(0);
    sqlDyStringPrintf(dy, "INSERT INTO TABLE VALUES ('");
    sqlDyAppendEscaped(dy, value);  // typically used when there are unusual characters that need escaping.
    sqlDyAppendEscaped(dy, "\x1a\n\r\\\'\"");  // typically used when there are unusual characters that need escaping.
    dyStringAppend(dy, "');");
    printf("%s\n", dy->string);
    }
else if (sameString(theType,"XX")) // test sqlSafef
    {
    char query[1024];
    //sqlSafef(query, sizeof query, "SELECT * FROM %s where field = '%s'", value, "value");
    sqlSafef(query, sizeof query, "SELECT * FROM %s where field = '%s'", "table", value);
    //sqlSafef(query, sizeof query, "SELECT * FROM %s where id=%d and field like '%%%s'", "table", 3, value);
    //sqlSafef(query, sizeof query, "SELECT * FROM %s where id=%d and field like '%%%s' and ptr=%p", "table", 3, value, value);
    //sqlSafef(query, sizeof query, "SELECT * FROM %s where field = '%-s'", "table", value);
    //sqlSafef(query, sizeof query, "SELECT %-s FROM TABLE where field = '%s'", sqlCkIl(value), "value");
    printf("query=%s\n", query);
    }
else if (sameString(theType,"XY")) // test sqlDyStringPrintf
    {
    struct dyString *dy = dyStringNew(200);
    //sqlDyStringPrintf(dy, "SELECT * FROM %s where field = '%s'", value, "value");
    sqlDyStringPrintf(dy, "SELECT * FROM %s where field = '%s'", "table", value);
    //sqlDyStringPrintf(dy, " AND field2 = '%s'", value);  // make sure appending works without duplicating the NOSQLINJ prefix
    //sqlDyStringPrintf(dy, "SELECT * FROM %s where id=%d and field like '%%%s'", "table", 3, value);
    //sqlDyStringPrintf(dy, "SELECT * FROM %s where id=%d and field like '%%%s' and ptr=%p", "table", 3, value, value);
    //sqlDyStringPrintf(dy, "SELECT * FROM %s where field = '%-s'", "table", value);
    printf("dy=%s\n", dy->string);
    }
else
    {
    usage();
    }

return 0;
}
