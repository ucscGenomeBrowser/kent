/* Things to do with ENCODE 3 validation. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef ENCODE3VALID_H
#define ENCODE3VALID_H

char *encode3CalcValidationKey(char *md5Hex, long long fileSize);
/* calculate validation key to discourage faking of validation.  Do freeMem on 
 *result when done. */

void encode3ValidateRcc(char *path);
/* Validate a nanostring rcc file. */

void encode3ValidateIdat(char *path);
/* Validate illumina idat file. */

boolean encode3IsGzipped(char *path);
/* Return TRUE if file at path starts with GZIP signature */

boolean encode3CheckEnrichedIn(char *enriched);
/* return TRUE if value is allowed */

struct encode3BedType 
/* Contain information about our beds with defined as files like narrowPeak. */
    {
    char *name;	    /* Base name without .as suffix */
    int bedFields;  /* Fields based on bed spec - come first */
    int extraFields; /* Beds past bed spec */
    };

extern struct encode3BedType encode3BedTypeTable[];	/* Table of supported bed types */
extern int encode3BedTypeCount;			/* Size of above table. */

struct encode3BedType *encode3BedTypeFind(char *name);
/* Return encode3BedType of given name.  Abort if not found */
    
struct encode3BedType *encode3BedTypeMayFind(char *name);
/* Return encode3BedType of given name, just return NULL if not found. */

#endif /* ENCODE3VALID_H */
