/* construct a filter for genbank entries */
#ifndef GBFILTER_H
#define GBFILTER_H

struct filter
/* Controls which records are chosen and which fields are written. */
{
    boolean isBAC;              /* are we selecting BACs */
    struct keyExp *keyExp;      /* If this expression evaluates true, the record is included. */
    struct slName *hideKeys;    /* A list of keys to not write out. */
};

struct filter *makeFilterFromFile(char *fileName);
/* Create filter from filter specification file. */

struct filter *makeFilterFromString(char *exprStr);
/* Create filter from a string with semi-colon seperated expressions. */

struct filter *makeFilterEmpty();
/* Create an empty filter that selects everything */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

