/* bigBedFilter.h - Filter things from bigBeds */

/* Copyright (C) 2018 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef BIGBEDFILTER_H
#define BIGBEDFILTER_H

/* the values legal for *FilterType */
#define FILTERBY_SINGLE                 "single"
#define FILTERBY_MULTIPLE               "multiple"
#define FILTERBY_SINGLE_LIST            "singleList"
#define FILTERBY_MULTIPLE_LIST_OR       "multipleListOr"
#define FILTERBY_MULTIPLE_LIST_ONLY_OR  "multipleListOnlyOr"
#define FILTERBY_MULTIPLE_LIST_ONLY_AND "multipleListOnlyAnd"
#define FILTERBY_MULTIPLE_LIST_AND      "multipleListAnd"
#define FILTERBY_DEFAULT                FILTERBY_MULTIPLE
#define FILTERTEXT_WILDCARD             "wildcard"
#define FILTERTEXT_REGEXP               "regexp"

/* cart and tdb variables */
#define FILTER_NUMBER_NAME_LOW    "filter"
#define FILTER_TEXT_NAME_LOW      "filterText"
#define FILTER_VALUES_NAME_LOW    "filterValues"
#define FILTER_VALUES_DEFAULT_NAME_LOW    "filterValuesDefault"
#define FILTER_TYPE_NAME_LOW      "filterType"
#define FILTER_LABEL_NAME_LOW     "filterLabel"
#define FILTER_NUMBER_WILDCARD_LOW    FILTER_NUMBER_NAME_LOW ".*"
#define FILTER_TEXT_WILDCARD_LOW      FILTER_TEXT_NAME_LOW ".*"
#define FILTER_VALUES_WILDCARD_LOW    FILTER_VALUES_NAME_LOW ".*"
#define FILTER_VALUES_DEFAULT_WILDCARD_LOW    FILTER_VALUES_DEFAULT_NAME_LOW ".*"
#define FILTER_TYPE_WILDCARD_LOW      FILTER_TYPE_NAME_LOW ".*"

#define FILTER_NUMBER_NAME_CAP    "Filter"
#define FILTER_TEXT_NAME_CAP      "FilterText"
#define FILTER_VALUES_NAME_CAP    "FilterValues"
#define FILTER_VALUES_DEFAULT_NAME_CAP    "FilterValuesDefault"
#define FILTER_TYPE_NAME_CAP      "FilterType"
#define FILTER_LABEL_NAME_CAP     "FilterLabel"
#define FILTER_NUMBER_WILDCARD_CAP    "*" FILTER_NUMBER_NAME_CAP
#define FILTER_TEXT_WILDCARD_CAP      "*" FILTER_TEXT_NAME_CAP
#define FILTER_VALUES_WILDCARD_CAP    "*" FILTER_VALUES_NAME_CAP
#define FILTER_VALUES_DEFAULT_WILDCARD_CAP    "*" FILTER_VALUES_DEFAULT_NAME_CAP
#define FILTER_TYPE_WILDCARD_CAP      "*" FILTER_TYPE_NAME_CAP

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

struct trackDbFilter
/* a bigBed filter in trackDb. */
{
struct trackDbFilter *next;
char *name;        /* the actual trackDb variable */
char *fieldName;   /* the field it applies to */
char *setting;     /* the setting of the trackDb variable */
};


struct bigBedFilter *bigBedMakeNumberFilter(struct cart *cart, struct bbiFile *bbi, struct trackDb *tdb, char *filter, char *defaultLimits,  char *field);
/* Add a bigBed filter using a trackDb filterBy statement. */

boolean bigBedFilterInterval(char **bedRow, struct bigBedFilter *filters);
/* Go through a row and filter based on filters.  Return TRUE if all filters are passed. */

struct bigBedFilter *bigBedBuildFilters(struct cart *cart, struct bbiFile *bbi, struct trackDb *tdb) ;
/* Build all the numeric and filterBy filters for a bigBed */

struct trackDbFilter *tdbGetTrackNumFilters( struct trackDb *tdb);
// get the number filters out of trackDb

struct trackDbFilter *tdbGetTrackTextFilters( struct trackDb *tdb);
// get the text filters out of trackDb

struct trackDbFilter *tdbGetTrackFilterByFilters( struct trackDb *tdb);
// get the values filters out of trackDb

char *getFilterType(struct cart *cart, struct trackDb *tdb, char *field, char *def);
// figure out how the trackDb is specifying the FILTER_TYPE variable and return its setting

#endif /* BIGBEDFILTER_H */
