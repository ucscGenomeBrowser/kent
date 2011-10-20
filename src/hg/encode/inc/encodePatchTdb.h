/* encodePatchTdb - Lay a trackDb.ra file from the pipeline gently on top of the trackDb system. */

#ifndef ENCODEPATCHTDB_H
#define ENCODEPATCHTDB_H

struct raTag
/* A tag in a .ra file. */
    {
    struct raTag *next;
    char *name;		/* Name of tag. */
    char *val;		/* Value of tag. */
    char *text;		/* Text - including white space and comments before tag. */
    };

struct raRecord
/* A record in a .ra file. */
    {
    struct raRecord *next;	/* Next in list. */
    struct raRecord *parent;	/* Parent if any. */
    struct raRecord *children;	/* Children - youngest to oldest. */
    struct raRecord *olderSibling;	/* Parent to older sibling if any. */
    char *key;			/* First word in track line if any. */
    struct raTag *tagList;	/* List of tags that make us up. */
    int startLineIx, endLineIx; /* Start and end in file for error reporting. */
    struct raFile *file;	/* Pointer to file we are in. */
    char *endComments;		/* Some comments that may follow record. */
    struct raRecord *subtracks;	/* Subtracks of this track. */
    boolean isRemoved;		/* If set, suppresses output. */
    };

struct raFile
/* A file full of ra's. */
    {
    struct raFile *next;	  /* Next (in include list) */
    char *name;		  	  /* Name of file */
    struct raRecord *recordList;  /* List of all records in file */
    char *endSpace;		  /* Text after last record. */
    struct hash *trackHash;	  /* Hash of records that have keys */
    };

struct raFile *raFileRead(char *fileName);
/* Read in file */

void writeTdbFile(struct raFile *tdbFile, char *outName);
/* Write it back to a file outName */


boolean renameTrack(struct raFile *tdbFile, char *oldTrack, char *newTrack, int pass, char **warnMsg);
/* Rename track, or just check if it is found 
 * pass 0 is checking, pass 1 is renaming*/

char *findCompositeInIncluder(struct raFile *includer, char *composite, int *numTagsFound, boolean removeAlpha, char **warnMsg);
/* Find <composite>{.other}.ra in includer file trackDb.wgEncode.ra
 * Return compositeName (no path) or NULL if error, give warnings.
 * Must have tag alpha or no tags. 
 * If removeAlpha is true, then the alpha tag is removed, 
 * or if no tags then adds tags beta,pubic */

boolean addCompositeToIncluder(struct raFile *includer, char *composite, char *newCompositeRaName, char **warnMsg);
/* Find <composite>{.other}.ra in includer file trackDb.wgEncode.ra
 * Return compositeName (no path) or NULL if error, give warnings.
 * Split the record, inserting below all the records found, with tag alpha */

void encodePatchTdb(char *patchFileName, char *tdbFileName);
/* encodePatchTdb - Lay a trackDb.ra file from the pipeline gently on top of the trackDb system. */

#endif /* ENCODEPATCHTDB_H */
