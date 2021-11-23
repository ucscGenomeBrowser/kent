/* Copyright (C) 2009 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef LOWEUTILS_H
#define LOWEUTILS_H

int parseDelimitedString(char *inString, char delimiter, char *outString[], int outSize);
struct minGeneInfo* getGbProtCodeInfo(struct sqlConnection *conn, char *database, char* dbName, char *geneName);
void getGenomeClade(struct sqlConnection *conn, char *dbName, char *genome, char *clade);

#endif
