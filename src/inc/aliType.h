/* aliType - some definitions for type of alignment. */

#ifndef ALITYPE_H
#define ALITYPE_H

enum gfType
/* Types of sequence genoFind deals with. */
    {
    gftDna = 0,		/* DNA (genomic) */
    gftRna = 1,		/* RNA */
    gftProt = 2,         /* Protein. */
    gftDnaX = 3,		/* Genomic DNA translated to protein */
    gftRnaX = 4,         /* RNA translated to protein */
    };

char *gfTypeName(enum gfType type);
/* Return string representing type. */

enum gfType gfTypeFromName(char *name);
/* Return type from string. */

enum ffStringency
/* How tight of a match is required. */
    {
    ffExact = 0,	/* Only an exact match will do. */

    ffCdna = 1,		/* Near exact.  Tolerate long gaps in target (genomic) */
    ffTight = 2,        /* Near exact.  Not so tolerant of long gaps in target. */
    ffLoose = 3,        /* Less exact. */
    };

#endif /* ALITYPE_H */
