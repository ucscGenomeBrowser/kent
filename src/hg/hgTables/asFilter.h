/* asFilter - a module to help filter things from non-SQL data sources.  To
 * use you need to be able to turn a record into an array of strings, and
 * have an AutoSql .as file that describes the record.   Currently this is
 * used by bigBed and BAM. */

#ifndef ASFILTER_H
#define ASFILTER_H

enum asFilterDataType
/* High level data type. */
   {
   afdtNone = 0,
   afdtString = 1,
   afdtLong = 2,
   afdtDouble = 3,
   afdtChar = 4,
   };

struct asLongFilter
/* Filter on long value */
   {
   enum numericFilterType op;
   long long *thresholds;
   };

struct asDoubleFilter
/* Filter on double value */
   {
   enum numericFilterType op;
   double *thresholds;
   };

struct asCharFilter
/* Filter on a char value */
    {
    enum charFilterType op;
    char *matches;
    boolean invert;
    };

struct asStringFilter
/* Filter on a string value */
    {
    enum stringFilterType op;
    char **matches;
    boolean invert;
    };

union asFilterData
/* One of the above four. */
    {
    struct asLongFilter *l;
    struct asDoubleFilter *d;
    struct asCharFilter *c;
    struct asStringFilter *s;
    };

struct asFilterColumn
/* A type of filter applied to a column. */
   {
   struct asFilterColumn *next;
   struct asColumn *col;		/* Column we operate on. */
   int colIx;				/* Index of column. */
   enum asFilterDataType dataType;	/* Type of limit parameters. */
   union asFilterData filter;		/* Filter data including op. */
   };

struct asFilter
/* A filter that can be applied to weed out rows in a table with an associated .as file. */
    {
    struct asFilter *next;
    struct asFilterColumn *columnList;  /* A list of column filters to apply */
    };

struct asFilter *asFilterFromCart(struct cart *cart, char *db, char *table, struct asObject *as);
/* Examine cart for filter relevant to this table, and create object around it. */

boolean asFilterOnRow(struct asFilter *filter, char **row);
/* Return TRUE if row passes filter if any. */

#endif /* ASFILTER_H */
