/* Parse genbank metadata from RA files. */
#ifndef GBMDPARSE_H
#define GBMDPARSE_H

#include "common.h"
#include "hgRelate.h"

struct sqlConnection;
struct dbLoadOptions;
struct dyString;

/* Globals containing ra field info for current record that are not stored
 * in unique string tables.  */
extern char raAcc[];
extern char raDir;  /* direction */
extern unsigned raDnaSize;
extern off_t raFaOff;
extern unsigned raFaSize;
extern time_t raModDate;
extern short raVersion;
extern char *raRefSeqStatus;
extern char *raRefSeqCompleteness;
extern struct dyString* raRefSeqSummary;
extern unsigned raLocusLinkId;
extern unsigned raGeneId;
extern unsigned raOmimId;
extern char raCds[];
extern char raProtAcc[];
extern short raProtVersion;
extern unsigned raProtSize;
extern off_t raProtFaOff;
extern unsigned raProtFaSize;
extern struct dyString* raLocusTag;
extern unsigned raGi;
extern char raMol[];
extern struct gbMiscDiff *raMiscDiffs;
extern char *raWarn;

char* gbMDParseEntry();
/* Parse the next record from a ra file into current metadata state.
 * Returns accession or NULL on EOF. */

void gbMDParseOpen(char* raPath, struct sqlConnection *conn,
                   struct dbLoadOptions* options, char *tmpDir);
/* open a ra file for parsing */

void gbMDParseClose();
/* close currently open ra file */

HGID raFieldCurId(char *raField);
/* get the last id that was stored for a raField */

char* raFieldCurVal(char *raField);
/* get the last string that was stored for a raField */

void raFieldClear(char *raField);
/* Clear a RA field value */

void gbMDParseCommit(struct sqlConnection *conn);
/* commit data it ra fields tables */

void gbMDParseFree();
/* Free global memory for ra fields */
#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
