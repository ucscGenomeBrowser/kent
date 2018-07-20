#ifndef BIGBEDFILTER_H
#define BIGBEDFILTER_H

struct bigBedFilter
/* Filter on a field in a bigBed file. */
{
struct bigBedFilter *next;
int fieldNum;   // the field number
enum {COMPARE_LESS, COMPARE_MORE, COMPARE_BETWEEN, COMPARE_HASH} comparisonType;  // the type of the comparison
double value1, value2;
struct hash *valueHash;
};

struct bigBedFilter *bigBedMakeNumberFilter(struct cart *cart, struct bbiFile *bbi, struct trackDb *tdb, char *filter, char *defaultLimits,  char *field);
/* Add a bigBed filter using a trackDb filterBy statement. */

boolean bigBedFilterInterval(char **bedRow, struct bigBedFilter *filters);
/* Go through a row and filter based on filters.  Return TRUE if all filters are passed. */

struct bigBedFilter *bigBedBuildFilters(struct cart *cart, struct bbiFile *bbi, struct trackDb *tdb) ;
/* Build all the numeric and filterBy filters for a bigBed */

#endif /* BIGBEDFILTER_H */
