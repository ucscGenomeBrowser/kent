/* bigBedFilter.h - Filter or highlight things from bigBeds */

/* Copyright (C) 2018 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef BIGBEDFILTER_H
#define BIGBEDFILTER_H

/* First the filtering variables */
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
/* End of filter variables */

/* The highlight variables */
/* the values legal for *HighlightType */
#define HIGHLIGHTBY_SINGLE                 "single"
#define HIGHLIGHTBY_MULTIPLE               "multiple"
#define HIGHLIGHTBY_SINGLE_LIST            "singleList"
#define HIGHLIGHTBY_MULTIPLE_LIST_OR       "multipleListOr"
#define HIGHLIGHTBY_MULTIPLE_LIST_ONLY_OR  "multipleListOnlyOr"
#define HIGHLIGHTBY_MULTIPLE_LIST_ONLY_AND "multipleListOnlyAnd"
#define HIGHLIGHTBY_MULTIPLE_LIST_AND      "multipleListAnd"
#define HIGHLIGHTBY_DEFAULT                HIGHLIGHTBY_MULTIPLE
#define HIGHLIGHTTEXT_WILDCARD             "wildcard"
#define HIGHLIGHTTEXT_REGEXP               "regexp"

/* cart and tdb variables */
#define HIGHLIGHT_NUMBER_NAME_LOW    "highlight"
#define HIGHLIGHT_TEXT_NAME_LOW      "highlightText"
#define HIGHLIGHT_VALUES_NAME_LOW    "highlightValues"
#define HIGHLIGHT_VALUES_DEFAULT_NAME_LOW    "highlightValuesDefault"
#define HIGHLIGHT_TYPE_NAME_LOW      "highlightType"
#define HIGHLIGHT_LABEL_NAME_LOW     "highlightLabel"
#define HIGHLIGHT_NUMBER_WILDCARD_LOW    HIGHLIGHT_NUMBER_NAME_LOW ".*"
#define HIGHLIGHT_TEXT_WILDCARD_LOW      HIGHLIGHT_TEXT_NAME_LOW ".*"
#define HIGHLIGHT_VALUES_WILDCARD_LOW    HIGHLIGHT_VALUES_NAME_LOW ".*"
#define HIGHLIGHT_VALUES_DEFAULT_WILDCARD_LOW    HIGHLIGHT_VALUES_DEFAULT_NAME_LOW ".*"
#define HIGHLIGHT_TYPE_WILDCARD_LOW      HIGHLIGHT_TYPE_NAME_LOW ".*"

#define HIGHLIGHT_NUMBER_NAME_CAP    "Highlight"
#define HIGHLIGHT_TEXT_NAME_CAP      "HighlightText"
#define HIGHLIGHT_VALUES_NAME_CAP    "HighlightValues"
#define HIGHLIGHT_VALUES_DEFAULT_NAME_CAP    "HighlightValuesDefault"
#define HIGHLIGHT_TYPE_NAME_CAP      "HighlightType"
#define HIGHLIGHT_LABEL_NAME_CAP     "HighlightLabel"
#define HIGHLIGHT_NUMBER_WILDCARD_CAP    "*" HIGHLIGHT_NUMBER_NAME_CAP
#define HIGHLIGHT_TEXT_WILDCARD_CAP      "*" HIGHLIGHT_TEXT_NAME_CAP
#define HIGHLIGHT_VALUES_WILDCARD_CAP    "*" HIGHLIGHT_VALUES_NAME_CAP
#define HIGHLIGHT_VALUES_DEFAULT_WILDCARD_CAP    "*" HIGHLIGHT_VALUES_DEFAULT_NAME_CAP
#define HIGHLIGHT_TYPE_WILDCARD_CAP      "*" HIGHLIGHT_TYPE_NAME_CAP
#define HIGHLIGHT_COLOR_DEFAULT "#ffff00" // defaults to highlighter yellow
#define HIGHLIGHT_COLOR_CART_VAR "highlightColor"
/* End highlight variables */

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
boolean isHighlight; // Are we highlighting or filtering?
};

struct trackDbFilter
/* a bigBed filter in trackDb. */
{
struct trackDbFilter *next;
char *name;        /* the actual trackDb variable */
char *fieldName;   /* the field it applies to */
char *setting;     /* the setting of the trackDb variable */
};


struct bigBedFilter *bigBedMakeNumberFilter(struct cart *cart, struct bbiFile *bbi, struct trackDb *tdb, char *filter, char *defaultLimits,  char *field, boolean isHighlight);
/* Add a bigBed filter using a trackDb filterBy statement. */

boolean bigBedFilterInterval(struct bbiFile *bbi, char **bedRow, struct bigBedFilter *filters);
/* Go through a row and filter based on filters.  Return TRUE if all filters are passed. */

struct bigBedFilter *bigBedBuildFilters(struct cart *cart, struct bbiFile *bbi, struct trackDb *tdb) ;
/* Build all the numeric and filterBy filters for a bigBed */

struct trackDbFilter *tdbGetTrackNumFilters( struct trackDb *tdb);
// get the number filters out of trackDb

struct trackDbFilter *tdbGetTrackTextFilters( struct trackDb *tdb);
// get the text filters out of trackDb

struct trackDbFilter *tdbGetTrackFilterByFilters( struct trackDb *tdb);
// get the values filters out of trackDb

struct trackDbFilter *tdbGetTrackNumHighlights( struct trackDb *tdb);

struct trackDbFilter *tdbGetTrackTextHighlights( struct trackDb *tdb);
// get the text filters out of trackDb

struct trackDbFilter *tdbGetTrackHighlightByHighlights( struct trackDb *tdb);
// get the values filters out of trackDb

char *getFilterType(struct cart *cart, struct trackDb *tdb, char *field, char *def);
// figure out how the trackDb is specifying the FILTER_TYPE variable and return its setting

#endif /* BIGBEDFILTER_H */
