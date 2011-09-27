/* asFilter - a module to help filter things from non-SQL data sources.  To
 * use you need to be able to turn a record into an array of strings, and
 * have an AutoSql .as file that describes the record.   Currently this is
 * used by bigBed and BAM. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "localmem.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "cart.h"
#include "web.h"
#include "hdb.h"
#include "asParse.h"
#include "asFilter.h"
#include "hgTables.h"


static void asCharFilterFree(struct asCharFilter **pFilter)
/* Free up memory associated with filter. */
{
struct asCharFilter *filter = *pFilter;
if (filter != NULL)
    {
    freeMem(filter->matches);
    freez(pFilter);
    }
}

static void asStringFilterFree(struct asStringFilter **pFilter)
/* Free up memory associated with filter. */
{
struct asStringFilter *filter = *pFilter;
if (filter != NULL)
    {
    freeMem(filter->matches);
    freez(pFilter);
    }
}

static struct asDoubleFilter *asDoubleFilterFromCart(struct cart *cart, char *fieldPrefix)
/* Get filter settings for double out of cart. */
{
struct asDoubleFilter *filter = NULL;
char varName[256];
safef(varName, sizeof(varName), "%s%s", fieldPrefix, filterCmpVar);
char *cmp = cartOptionalString(cart, varName);
safef(varName, sizeof(varName), "%s%s", fieldPrefix, filterPatternVar);
char *pat = cartOptionalString(cart, varName);
if (!isEmpty(cmp) && !sameString(cmp, "ignored") && !isEmpty(pat))
    {
    AllocVar(filter);
    cgiToDoubleFilter(cmp, pat, &filter->op, &filter->thresholds);
    }
return filter;
}

static struct asLongFilter *asLongFilterFromCart(struct cart *cart, char *fieldPrefix)
/* Get filter settings for double out of cart. */
{
struct asLongFilter *filter = NULL;
char varName[256];
safef(varName, sizeof(varName), "%s%s", fieldPrefix, filterCmpVar);
char *cmp = cartOptionalString(cart, varName);
safef(varName, sizeof(varName), "%s%s", fieldPrefix, filterPatternVar);
char *pat = cartOptionalString(cart, varName);
if (!isEmpty(cmp) && !sameString(cmp, "ignored") && !isEmpty(pat))
    {
    AllocVar(filter);
    cgiToLongFilter(cmp, pat, &filter->op, &filter->thresholds);
    }
return filter;
}

static struct asCharFilter *asCharFilterFromCart(struct cart *cart, char *fieldPrefix)
/* Get filter settings for double out of cart. */
{
struct asCharFilter *filter = NULL;
char varName[256];
safef(varName, sizeof(varName), "%s%s", fieldPrefix, filterDdVar);
char *dd = cartOptionalString(cart, varName);
safef(varName, sizeof(varName), "%s%s", fieldPrefix, filterPatternVar);
char *pat = cartOptionalString(cart, varName);
if (!isEmpty(dd) && !isEmpty(pat))
    {
    AllocVar(filter);
    cgiToCharFilter(dd, pat, &filter->op, &filter->matches, &filter->invert);
    if (filter->op == cftIgnore)	// Filter out nop
	asCharFilterFree(&filter);
    }
return filter;
}

static struct asStringFilter *asStringFilterFromCart(struct cart *cart, char *fieldPrefix)
/* Get filter settings for double out of cart. */
{
struct asStringFilter *filter = NULL;
char varName[256];
safef(varName, sizeof(varName), "%s%s", fieldPrefix, filterDdVar);
char *dd = cartOptionalString(cart, varName);
safef(varName, sizeof(varName), "%s%s", fieldPrefix, filterPatternVar);
char *pat = cartOptionalString(cart, varName);
if (!isEmpty(dd) && !isEmpty(pat))
    {
    AllocVar(filter);
    cgiToStringFilter(dd, pat, &filter->op, &filter->matches, &filter->invert);
    if (filter->op == sftIgnore)	// Filter out nop
	asStringFilterFree(&filter);
    }
return filter;
}

static boolean asFilterString(struct asStringFilter *filter, char *x)
/* Return TRUE if x passes filter. */
{
return bedFilterString(x, filter->op, filter->matches, filter->invert);
}

static boolean asFilterLong(struct asLongFilter *filter, long long x)
/* Return TRUE if x passes filter. */
{
return bedFilterLong(x, filter->op, filter->thresholds);
}

static boolean asFilterDouble(struct asDoubleFilter *filter, double x)
/* Return TRUE if x passes filter. */
{
return bedFilterDouble(x, filter->op, filter->thresholds);
}

static boolean asFilterChar(struct asCharFilter *filter, char x)
/* Return TRUE if x passes filter. */
{
return bedFilterChar(x, filter->op, filter->matches, filter->invert);
}

static boolean asFilterOneCol(struct asFilterColumn *filtCol, char *s)
/* Return TRUE if s passes filter. */
{
switch (filtCol->dataType)
    {
    case afdtString:
        return asFilterString(filtCol->filter.s, s);
    case afdtLong:
        return asFilterLong(filtCol->filter.l, atoll(s));
    case afdtDouble:
        return asFilterDouble(filtCol->filter.d, atof(s));
    case afdtChar:
        return asFilterChar(filtCol->filter.c, s[0]);
    default:
        internalErr();
	return FALSE;
    }
}

boolean asFilterOnRow(struct asFilter *filter, char **row)
/* Return TRUE if row passes filter if any. */
{
if (filter != NULL)
    {
    struct asFilterColumn *col;
    for (col = filter->columnList; col != NULL; col = col->next)
	{
	if (!asFilterOneCol(col, row[col->colIx]))
	    return FALSE;
	}
    }
return TRUE;
}

struct asFilter *asFilterFromCart(struct cart *cart, char *db, char *table, struct asObject *as)
/* Examine cart for filter relevant to this table, and create object around it. */
{
/* Get list of filter variables for this table. */
char tablePrefix[128], fieldPrefix[192];
safef(tablePrefix, sizeof(tablePrefix), "%s%s.%s.", hgtaFilterVarPrefix, db, table);

struct asFilter *asFilter;
AllocVar(asFilter);

struct asColumn *col;
int colIx = 0;
for (col = as->columnList; col != NULL; col = col->next, ++colIx)
    {
    safef(fieldPrefix, sizeof(fieldPrefix), "%s%s.", tablePrefix, col->name);
    struct asTypeInfo *lt = col->lowType;
    union asFilterData lowFilter;
    enum asFilterDataType dataType = afdtNone;	
    lowFilter.d = NULL;
    switch (lt->type)
	{
	case t_double:
	case t_float:
	    lowFilter.d = asDoubleFilterFromCart(cart, fieldPrefix);
	    dataType = afdtDouble;
	    break;
	case t_char:
	    lowFilter.c = asCharFilterFromCart(cart, fieldPrefix);
	    dataType = afdtChar;
	    break;
	case t_int:
	case t_uint:
	case t_short:
	case t_ushort:
	case t_byte:
	case t_ubyte:
	case t_off:
	    lowFilter.l = asLongFilterFromCart(cart, fieldPrefix);
	    dataType = afdtLong;
	    break;
	case t_string:
	case t_lstring:
	    lowFilter.s = asStringFilterFromCart(cart, fieldPrefix);
	    dataType = afdtString;
	    break;
	case t_object:
	case t_simple:
	case t_enum:
	case t_set:
	default:
	    internalErr();
	    break;
	}
    if (lowFilter.d != NULL)
        {
	struct asFilterColumn *colFilt;
	AllocVar(colFilt);
	colFilt->col = col;
	colFilt->colIx = colIx;
	colFilt->dataType = dataType;
	colFilt->filter = lowFilter;
	slAddHead(&asFilter->columnList, colFilt);
	}
    }
slReverse(&asFilter->columnList);
return asFilter;
}

