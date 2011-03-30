// cv.c stands for Controlled Vocabullary and this file contains the
// library API prototypes for reading and making sense of the contents of cv.ra.

#ifndef CV_H
#define CV_H

#include "jksql.h"
#define CV_FILE_NAME            "cv.ra"

// CV Common settings
#define CV_TERM                 "term"
#define CV_TYPE                 "type"
#define CV_LABEL                "label"
#define CV_TAG                  "tag"
#define CV_TARGET               "target"
#define CV_TITLE                "title"
#define CV_DESCRIPTION          "description"

// Type of Terms defines
#define CV_TOT                  "typeOfTerm"
#define CV_TOT_HIDDEN           "hidden"
#define CV_TOT_CV_DEFINED       "cvDefined"
#define CV_TOT_PRIORITY         "priority"

// Validation Rules
#define CV_VALIDATE                 "validate"
#define CV_VALIDATE_CV              "cv"
#define CV_VALIDATE_CV_OR_NONE      "cv or None"
#define CV_VALIDATE_CV_OR_CONTROL   "cv or control"
#define CV_VALIDATE_DATE            "date"
#define CV_VALIDATE_EXISTS          "exists"
#define CV_VALIDATE_FLOAT           "float"
#define CV_VALIDATE_INT             "integer"
#define CV_VALIDATE_LIST            "list:"
#define CV_VALIDATE_REGEX           "regex:"
#define CV_VALIDATE_NONE            "none"

// CV TERMS (NOTE: UGLY Terms in cv.ra are hidden inside cv.c APIS)
#define CV_TERM_GRANT           "grant"
#define CV_TERM_LAB             "lab"
#define CV_TERM_CELL            "cell"
#define CV_TERM_ANTIBODY        "antibody"
#define CV_TERM_CONTROL         "control"
#define CV_TERM_DATA_TYPE       "dataType"
#define CV_TERM_LOCALIZATION    "localization"
#define CV_TERM_VIEW            "view"


char *cvTypeNormalized(char *sloppyTerm);
// returns (on stack) the proper term to use when requesting a typeOfTerm

char *cvTermNormalized(char *sloppyTerm);
// returns (on stack) the proper term to use when requesting a cvTerm hash

const struct hash *cvTermHash(char *term);
// returns a hash of hashes of a term which should be defined in cv.ra
// NOTE: in static memory: DO NOT FREE

const struct hash *cvTermTypeHash();
// returns a hash of hashes of mdb and controlled vocabulary (cv) term types
// Those terms should contain label,descrition,searchable,cvDefined,hidden
// NOTE: in static memory: DO NOT FREE

struct slPair *cvWhiteList(boolean searchTracks, boolean cvLinks);
// returns the official mdb/controlled vocabulary terms that have been whitelisted for certain uses.

enum cvSearchable
// metadata Variavble are only certain declared types
    {
    cvNotSearchable        =0,  // Txt is default
    cvSearchByMultiSelect  =1,  // Search by drop down multi-select of supplied list (NOT YET IMPLEMENTED)
    cvSearchBySingleSelect =2,  // Search by drop down single-select of supplied list
    cvSearchByFreeText     =3,  // Search by free text field (NOT YET IMPLEMENTED)
    cvSearchByDateRange    =4,  // Search by discovered date range (NOT YET IMPLEMENTED)
    cvSearchByIntegerRange =5   // Search by discovered integer range (NOT YET IMPLEMENTED)
    };

enum cvSearchable cvSearchMethod(char *term);
// returns whether the term is searchable

const char *cvLabel(char *term);
// returns cv label if term found or else just term

#endif /* CV_H */

