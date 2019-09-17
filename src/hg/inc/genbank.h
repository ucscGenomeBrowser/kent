/* genbank.h - Various functions for dealing with genbank data */

/* Copyright (C) 2008 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef GENBANK_H
#define GENBANK_H

/* buffer size for a genbank or refseq accession, with version */
#define GENBANK_ACC_BUFSZ 24

#ifndef HASH_H
#include "hash.h"
#endif

#ifndef LINEFILE_H
#include "linefile.h"
#endif
struct psl;

struct genbankCds
/* structure return information about parsed CDS */
{
    int start;                    /* start, in [0..n) coords */
    int end;                      /* end, in [0..n) coords */
    boolean startComplete;        /* is start complete? */ 
    boolean endComplete;          /* is end complete? */ 
    boolean complement;           /* is on reverse strand */
};

boolean genbankCdsParse(char *cdsStr, struct genbankCds* cds);
/* Parse a genbank CDS, returning TRUE if it can be successfuly parsed, FALSE
 * if there are problems.  If a join() is specified, the first and last
 * coordinates are used for the CDS.  Incomplete CDS specifications will still
 * return the start or end.  start/end are set to 0 on error. */

boolean genbankParseCds(char *cdsStr, unsigned *cdsStart, unsigned *cdsEnd);
/* Compatiblity function, genbankCdsParse is prefered. Parse a genbank CDS,
 * returning TRUE if it can be successfuly parsed, FALSE if there are
 * problems.  If a join() is specified, the first and last coordinates are
 * used for the CDS.  Incomplete CDS specifications will still return the
 * start or end.  cdsStart and cdsEnd are set to -1 on error.
 */

struct genbankCds genbankCdsToGenome(struct genbankCds* cds, struct psl *psl);
/* Convert set cdsStart/end from mrna to genomic coordinates using an
 * alignment.  Returns a genbankCds object with genomic (positive strand)
 * coordinates */

boolean genbankIsRefSeqAcc(char *acc);
/* determine if a accession appears to be from RefSeq */

boolean genbankIsRefSeqCodingMRnaAcc(char *acc);
/* determine if a accession appears to be a protein-coding RefSeq
 * accession. */

boolean genbankIsRefSeqNonCodingMRnaAcc(char *acc);
/* determine if a accession appears to be a non-protein-coding RefSeq
 * accession. */

char* genbankDropVer(char *outAcc, char *inAcc);
/* strip the version from a genbank id.  Input and output
 * strings maybe the same. acc length is checked against
 * GENBANK_ACC_BUFSZ. */

void genbankExceptionsHash(char *fileName, 
	struct hash **retSelenocysteineHash, struct hash **retAltStartHash);
/* Will read a genbank exceptions file, and return two hashes parsed out of
 * it filled with the accessions having the two exceptions we can handle, 
 * selenocysteines, and alternative start codons. */


extern char *gbCdnaInfoTable;       
extern char *descriptionTable;       
extern char *productNameTable;       
extern char *organismTable;       
extern char *authorTable;       
extern char *cdsTable;       
extern char *tissueTable;       
extern char *developmentTable;       
extern char *geneNameTable;       
extern char *refLinkTable;       
extern char *refPepTable;       
extern char *cellTable;       
extern char *sourceTable;
extern char *libraryTable;
extern char *mrnaCloneTable;
extern char *sexTable;
extern char *keywordTable;
extern char *gbSeqTable;
extern char *gbExtFileTable;
extern char *gbMiscDiffTable;
extern char *imageCloneTable;
extern char *gbWarnTable;
extern char *refSeqStatusTable;
extern char *refSeqSummaryTable;

void initGenbankTableNames(char *database);
/* read hg.conf to get alternate table names */

boolean isGenbankPatentAccession(char *acc);
/* Is this an accession prefix allocated to patent sequences. */

#endif
