/* genePredReader - object to read genePred objects from database tables
 * or files.  */

/* Copyright (C) 2004 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef GENEPREDREADER_H
#define GENEPREDREADER_H

struct sqlConnection;
struct genePredReader;
struct genePred;

struct genePredReader *genePredReaderQuery(struct sqlConnection* conn,
                                           char* table, char* where);
/* Create a new genePredReader to read from the given table in the database.
 * If where is not null, it is added as a where clause.  It will determine if
 * extended genePred columns are in the table.
 */

struct genePredReader *genePredReaderRangeQuery(struct sqlConnection* conn,
                                                char* table, char* chrom,
                                                int start, int end, 
                                                char* extraWhere);
/* Create a new genePredReader to read a chrom range in a database table.  If
 * extraWhere is not null, it is added as an additional where condition. It
 * will determine if extended genePred columns are in the table. */

struct genePredReader *genePredReaderFile(char* gpFile, char* chrom);
/* Create a new genePredReader to read from a file.  If chrom is not null,
 * only this chromsome is read.  The rows must contain columns in the order in
 * the struct, and they must be present up to the last specfied optional
 * field.  Missing intermediate fields must have zero or empty columns, they
 * may not be omitted. */

struct genePred *genePredReaderNext(struct genePredReader* gpr);
/* Read the next genePred, returning NULL if no more */

struct genePred *genePredReaderAll(struct genePredReader* gpr);
/* Read the all of genePreds */

void genePredReaderFree(struct genePredReader** gprPtr);
/* Free the genePredRead object. */

struct genePred *genePredReaderLoadQuery(struct sqlConnection* conn,
                                         char* table, char* where);
/* Function that encapsulates doing a query and loading the results */

struct genePred *genePredReaderLoadRangeQuery(struct sqlConnection* conn,
                                              char* table, char* chrom,
                                              int start, int end, 
                                              char* extraWhere);
/* Function that encapsulates doing a range query and loading the results */

struct genePred *genePredReaderLoadFile(char* gpFile, char* chrom);
/* Function that encapsulates reading a genePred file */


#endif
 
