/* genePredReader - object to read genePred objects from database tables
 * or files.  */

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

struct genePredReader *genePredReaderFile(char* gpFile, unsigned optFields,
                                          char* chrom);
/* Create a new genePredReader to read from a file.  If chrom is not null,
 * only this chromsome is read.  optFields is the bitset of optional fields
 * to include.  They must be in the same order as genePred. */

struct genePred *genePredReaderNext(struct genePredReader* gpr);
/* Read the next genePred, returning NULL if no more */

struct genePred *genePredReaderAll(struct genePredReader* gpr);
/* Read the all of genePreds */

void genePredReaderFree(struct genePredReader** gprPtr);
/* Free the genePredRead object. */

struct genePred *genePredReaderDoQuery(struct sqlConnection* conn,
                                       char* table, char* where);
/* Function that encapsulates doing a query and loading the results */

struct genePred *genePredReaderDoRangeQuery(struct sqlConnection* conn,
                                            char* table, char* chrom,
                                            int start, int end, 
                                            char* extraWhere);
/* Function that encapsulates doing a range query and loading the results */

struct genePred *genePredReaderDoFile(char* gpFile, unsigned optFields,
                                      char* chrom);
/* Function that encapsulates reading a genePred file */


#endif
 
