// cv.c stands for Controlled Vocabullary and this file contains the
// library API prototypes for reading and making sense of the contents of cv.ra.

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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

// CV Less common settings
#define CV_GEO                  "geo"
#define CV_LINEAGE              "lineage"
#define CV_ORDER_URL            "orderUrl"
#define CV_ORGANISM             "organism"
#define CV_PROTOCOL             "protocol"
#define CV_SEX                  "sex"
#define CV_TERM_ID              "termId"
#define CV_TERM_URL             "termUrl"
#define CV_TIER                 "tier"
#define CV_TISSUE               "tissue"
#define CV_VENDOR_ID            "vendorId"
#define CV_VENDOR_NAME          "vendorName"
#define CV_KARYOTYPE            "karyotype"
#define CV_ANTIBODY_DESCRIPTION  "antibodyDescription"
#define CV_TARGET_DESCRIPTION    "targetDescription"
#define CV_TARGET_ID             "targetId"
#define CV_TARGET_URL            "targetURL"
#define CV_DATA_GROUP            "dataGroup"

// Type of Terms defines
#define CV_TOT                  "typeOfTerm"
#define CV_TOT_HIDDEN           "hidden"
#define CV_TOT_CV_DEFINED       "cvDefined"
#define CV_TOT_PRIORITY         "priority"
#define CV_TOT_SEARCHABLE       "searchable"

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
#define CV_TERM_SEQ_PLATFORM    "seqPlatform"
#define CV_TERM_TREATMENT       "treatment"

#define CV_LABEL_EMPTY_IS_NONE  "None"

void cvTermJson(struct dyString *json, char *type, struct hash *termHash);
/* Print out CV term in JSON format. Currently just supports dataType, cellType, antibody
 * and antibody types */

void cvFileDeclare(const char *filePath);
// Declare an altername cv.ra file to use
// (The cv.ra file is normally discovered based upon CGI/Tool and envirnment)

const char *cvFile();
// return default location of cv.ra

const char *cvTypeNormalized(const char *sloppyTerm);
// returns the proper term to use when requesting a typeOfTerm

const char *cvTermNormalized(const char *sloppyTerm);
// returns the proper term to use when requesting a cvTerm hash

const struct hash *cvTermHash(const char *term);
// returns a hash of hashes of a term which should be defined in cv.ra
// NOTE: in static memory: DO NOT FREE

const struct hash *cvOneTermHash(const char *type,const char *term);
// returns a hash for a single term of a given type
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
    cvSearchByMultiSelect  =1,  // Search by drop down multi-select of supplied list
    cvSearchBySingleSelect =2,  // Search by drop down single-select of supplied list
    cvSearchByFreeText     =3,  // Search by free text field
    cvSearchByWildList     =4,  // Search by comma delimited list (accepts '%' wildcard)
    cvSearchByDateRange    =4,  // Search by discovered date range (NOT YET IMPLEMENTED)
    cvSearchByIntegerRange =5   // Search by discovered integer range (NOT YET IMPLEMENTED)
    };

enum cvSearchable cvSearchMethod(const char *term);
// returns whether the term is searchable

const char *cvValidationRule(const char *term);
// returns validation rule, trimmed of comment

enum cvDataType
// CV term may be recognizable as int or float
    {
    cvIndeterminant        =0,  // Indeterminant, likely string
    cvString               =1,  // Just about all terms are strings
    cvInteger              =2,  // Some are known integers
    cvFloat                =3,  // Floats are possible
    cvDate                 =4,  // Dates are expected as YYYY-MM-DD
    };

enum cvDataType cvDataType(const char *term);
// returns the dataType if it can be determined

boolean cvValidateTerm(const char *term,const char *val,char *reason,int len);
// returns TRUE if term is valid.  Can pass in a reason buffer of len to get reason.

const char *cvLabel(const char *type,const char *term);
// returns cv label if term found or else just term
// If type not supplied, must be a typeOfTerm definition

const char *cvTag(const char *type,const char *term);
// returns cv Tag if term found or else NULL

boolean cvTermIsHidden(const char *term);
// returns TRUE if term is defined as hidden in cv.ra

boolean cvTermIsCvDefined(const char *term);
// returns TRUE if the terms values are defined in the cv.ra
// For example anitobody is cv defined but expId isn't

boolean cvTermIsEmpty(const char *term,const char *val);
// returns TRUE if term has validation of "cv or None" and the val is None

char *cvLabNormalize(const char *sloppyTerm);
/* CV inconsistency work-arounds.  Return lab name trimmed of parenthesized trailing
 * info (a few ENCODE labs have this in metaDb and/or in CV term --
 * PI name embedded in parens in the CV term).  Also fixes other problems until
 * cleaned up in CV, metaDb and user processes.  Caller must free mem. */

#endif /* CV_H */

