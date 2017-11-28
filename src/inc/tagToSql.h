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
    double minVal;	    /* Min val */
    double maxVal;	    /* Max val */
    int maxChars;	    /* Maximum width of string representation */
    boolean isArray;	    /* True if is an array */
    };

struct tagTypeInfo *tagTypeInfoNew(char *name);
/* Return initialized new tagTypeInfo */

void tagTypeInfoFree(struct tagTypeInfo **pTti);
/* Free up a tagTypeInfo */

void tagTypeInfoAdd(struct tagTypeInfo *tti, char *val);
/* Fold in information about val into tti. */

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

void tagTypeInfoPrintSchemaLine(struct tagTypeInfo *tti, int useCount, struct hash *valHash, 
    boolean doLooseSchema, boolean doTightSchema, FILE *f);
/* Print out a line in tagStorm type schema */

#endif /* TAGTOSQL_H */
