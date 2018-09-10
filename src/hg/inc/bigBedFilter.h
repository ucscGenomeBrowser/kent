/* bigBedFilter.h - Filter things from bigBeds */

/* Copyright (C) 2018 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef BIGBEDFILTER_H
#define BIGBEDFILTER_H

/* the values legal for *FilterType */
#define FILTERBY_SINGLE            "single"
#define FILTERBY_MULTIPLE          "multiple"
#define FILTERBY_SINGLE_LIST       "singleList"
#define FILTERBY_MULTIPLE_LIST_OR  "multipleListOr"
#define FILTERBY_MULTIPLE_LIST_AND "multipleListAnd"
#define FILTERTEXT_WILDCARD        "wildcard"
#define FILTERTEXT_REGEXP          "regexp"

/* cart and tdb variables */
#define FILTER_NUMBER_NAME    "Filter"
#define FILTER_TEXT_NAME      "FilterText"
#define FILTER_VALUES_NAME    "FilterValues"
#define FILTER_TYPE_NAME      "FilterType"
#define FILTER_NUMBER_WILDCARD    "*" FILTER_NUMBER_NAME
#define FILTER_TEXT_WILDCARD      "*" FILTER_TEXT_NAME
#define FILTER_VALUES_WILDCARD    "*" FILTER_VALUES_NAME
#define FILTER_TYPE_WILDCARD      "*" FILTER_TYPE_NAME

struct bigBedFilter
/* Filter on a field in a bigBed file. */
{
struct bigBedFilter *next;
int fieldNum;   // the field number
enum {COMPARE_LESS, COMPARE_MORE, COMPARE_BETWEEN, COMPARE_HASH, COMPARE_REGEXP, COMPARE_WILDCARD, COMPARE_HASH_LIST_AND, COMPARE_HASH_LIST_OR } comparisonType;  // the type of the comparison
double value1, value2;
struct hash *valueHash;
unsigned numValuesInHash;
regex_t regEx;
char *wildCardString;
};

struct bigBedFilter *bigBedMakeNumberFilter(struct cart *cart, struct bbiFile *bbi, struct trackDb *tdb, char *filter, char *defaultLimits,  char *field);
/* Add a bigBed filter using a trackDb filterBy statement. */

boolean bigBedFilterInterval(char **bedRow, struct bigBedFilter *filters);
/* Go through a row and filter based on filters.  Return TRUE if all filters are passed. */

struct bigBedFilter *bigBedBuildFilters(struct cart *cart, struct bbiFile *bbi, struct trackDb *tdb) ;
/* Build all the numeric and filterBy filters for a bigBed */

#endif /* BIGBEDFILTER_H */
