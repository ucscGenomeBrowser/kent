/* tagToSql - convert tagStorm to a SQL table.  See src/tagStorm/tagStormToTab/tagStormToTab.c
 * for how to use this. */

#ifndef TAGTOSQL_H
#define TAGTOSQL_H

#ifndef TAGSTORM_H
#include "tagStorm.h"
#endif 

struct tagTypeInfo
/* Information on a tag type */
    {
    struct tagTypeInfo *next;
    char *name;		    /* Name of tag */
    boolean isUnsigned;	    /* True if an unsigned integer */
    boolean isInt;	    /* True if an integer */
    boolean isNum;	    /* True if a number (real or integer) */
    long long maxIntVal;    /* Maximum value for integer or unsigned */
    long long minIntVal;    /* Minimum value for integer */
    int maxChars;	    /* Maximum width of string representation */
    };

void tagTypeInfoDump(struct tagTypeInfo *list, char *fileName);
/* Dump out types to file */

void tagStormInferTypeInfo(struct tagStorm *tagStorm, 
    struct tagTypeInfo **retList, struct hash **retHash);
/* Go through every tag/value in storm and return a hash that is
 * keyed by tag and a tagTypeInfo as a value */

void tagStormToSqlCreate(struct tagStorm *tagStorm,
    char *tableName, struct tagTypeInfo *ttiList, struct hash *ttiHash,
    char **keyFields, int keyFieldCount, struct dyString *createString);
/* Make a mysql create string out of ttiList/ttiHash, which is gotten from
 * tagStormInferTypeInfo.  Result is in createString */

void tagStanzaToSqlInsert(struct tagStanza *stanza, char *table, struct dyString *dy);
/* Put SQL insert statement for stanza into dy. */

#endif /* TAGTOSQL_H */
