/* Copyright (C) 2009 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef LOWEUTILS_H
#define LOWEUTILS_H

char *database;


int parseDelimitedString(char *inString, char delimiter, char *outString[], int outSize);
struct minGeneInfo* getGbProtCodeInfo(struct sqlConnection *conn, char* dbName, char *geneName);
void getGenomeClade(struct sqlConnection *conn, char *dbName, char *genome, char *clade);

#endif
