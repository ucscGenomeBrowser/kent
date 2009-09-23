#ifndef LOWEUTILS_H
#define LOWEUTILS_H

char *database;


int parseDelimitedString(char *inString, char delimiter, char *outString[], int outSize);
struct minGeneInfo* getGbProtCodeInfo(struct sqlConnection *conn, char* dbName, char *geneName);
void getGenomeClade(struct sqlConnection *conn, char *dbName, char *genome, char *clade);

#endif
