/* pslReader - object to read psl objects from database tables or files.  */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef PSLREADER_H
#define PSLREADER_H

/* Options to pslGetCreateSql */
#define PSL_TNAMEIX   0x01  /* create target name index */
#define PSL_WITH_BIN  0x02  /* add bin column */

char* pslGetCreateSql(char* table, unsigned options, int tNameIdxLen);
/* Get SQL required to create PSL table.  Options is a bit set consisting
 * of PSL_TNAMEIX, PSL_WITH_BIN, and PSL_XA_FORMAT.  tNameIdxLen is
 * the number of characters in target name to index.  If greater than
 * zero, must specify PSL_TNAMEIX.  If zero and PSL_TNAMEIX is specified,
 * to will default to 8. */


struct pslReader;
struct sqlConnection;

struct pslReader *pslReaderQuery(struct sqlConnection* conn,
                                 char* table, char* where);
/* Create a new pslReader to read from the given table in the database.
 * If where is not null, it is added as a where clause.  It will determine if
 * pslx columns are in the table. */

struct pslReader *pslReaderChromQuery(struct sqlConnection* conn,
                                      char* table, char* chrom,
                                      char* extraWhere);
/* Create a new pslReader to read all rows for a chrom in a database table.
 * If extraWhere is not null, it is added as an additional where condition. It
 * will determine if pslx columns are in the table. */

struct pslReader *pslReaderRangeQuery(struct sqlConnection* conn,
                                      char* table, char* chrom,
                                      int start, int end, 
                                      char* extraWhere);
/* Create a new pslReader to read a chrom range in a database table.  If
 * extraWhere is not null, it is added as an additional where condition. It
 * will determine if pslx columns are in the table. */

struct pslReader *pslReaderFile(char* pslFile, char* chrom);
/* Create a new pslReader to read from a file.  If chrom is not null,
 * only this chromsome is read.   Checks for psl header and pslx columns. */

struct psl *pslReaderNext(struct pslReader* pr);
/* Read the next psl, returning NULL if no more */

struct psl *pslReaderAll(struct pslReader* pr);
/* Read the all of psls */

void pslReaderFree(struct pslReader** prPtr);
/* Free the pslRead object. */

struct psl *pslReaderLoadQuery(struct sqlConnection* conn,
                               char* table, char* where);
/* Function that encapsulates doing a query and loading the results */

struct psl *pslReaderLoadRangeQuery(struct sqlConnection* conn,
                                    char* table, char* chrom,
                                    int start, int end, 
                                    char* extraWhere);
/* Function that encapsulates doing a range query and loading the results */

struct psl *pslReaderLoadDb(char* db, char* table, char* where);
/* Function that encapsulates reading a PSLs from a database If where is not
* null, it is added as a where clause.  It will determine if pslx columns are
* in the table. */

struct psl *pslReaderLoadFile(char* pslFile, char* chrom);
/* Function that encapsulates reading a psl file */

#endif
