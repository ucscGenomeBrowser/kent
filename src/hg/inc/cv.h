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

// Type of Terms defines
#define CV_TOT                  "typeOfTerm"
#define CV_TOT_HIDDEN           "hidden"
#define CV_TOT_CV_DEFINED       "cvDefined"

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
#define CV_TERM_CELL            MDB_VAR_CELL
#define CV_TERM_ANTIBODY        MDB_VAR_ANTIBODY
#define CV_TERM_CONTROL         "control"

const struct hash *mdbCvTermHash(char *term);
// returns a hash of hashes of a term which should be defined in cv.ra
// NOTE: in static memory: DO NOT FREE

const struct hash *mdbCvTermTypeHash();
// returns a hash of hashes of mdb and controlled vocabulary (cv) term types
// Those terms should contain label,descrition,searchable,cvDefined,hidden
// NOTE: in static memory: DO NOT FREE

struct slPair *mdbCvWhiteList(boolean searchTracks, boolean cvLinks);
// returns the official mdb/controlled vocabulary terms that have been whitelisted for certain uses.

enum mdbCvSearchable
// metadata Variavble are only certain declared types
    {
    cvsNotSearchable        =0,  // Txt is default
    cvsSearchByMultiSelect  =1,  // Search by drop down multi-select of supplied list (NOT YET IMPLEMENTED)
    cvsSearchBySingleSelect =2,  // Search by drop down single-select of supplied list
    cvsSearchByFreeText     =3,  // Search by free text field (NOT YET IMPLEMENTED)
    cvsSearchByDateRange    =4,  // Search by discovered date range (NOT YET IMPLEMENTED)
    cvsSearchByIntegerRange =5   // Search by discovered integer range (NOT YET IMPLEMENTED)
    };

enum mdbCvSearchable mdbCvSearchMethod(char *term);
// returns whether the term is searchable // TODO: replace with mdbCvWhiteList() returning struct

const char *cvLabel(char *term);
// returns cv label if term found or else just term

#endif /* CV_H */

