#include "common.h"
#include "errabort.h"
#include "bed.h"
#include "cart.h"
#include "cheapcgi.h"
#include "hPrint.h"
#include "hui.h"
#include "jksql.h"
#include "web.h"
#include "loweutils.h"
#include "hgConfig.h"
#include "hdb.h"
#include "minGeneInfo.h"

int parseDelimitedString(char *inString, char delimiter, char *outString[], int outSize)
{
    int arrayCount = 0;
    int charCount = 0;
    int start = 0;
    char *out = NULL;

    for (charCount = 0; charCount < strlen(inString); charCount++)
    {
        if (inString[charCount] == delimiter)
        {
            if (arrayCount < outSize)
            {
                out = malloc(sizeof(char) * (charCount - start + 1));
                memcpy(out, inString + start, sizeof(char) * (charCount - start));
                out[charCount - start] = '\0';
                outString[arrayCount++] = out;
                start = charCount + 1;
            }
        }
    }
    if (arrayCount < outSize)
    {
        out = malloc(sizeof(char) * (charCount - start + 1));
        memcpy(out, inString + start, sizeof(char) * (charCount - start));
        out[charCount - start] = '\0';
        outString[arrayCount++] = out;
    }

    return arrayCount;
}
struct minGeneInfo* getGbProtCodeInfo(struct sqlConnection *conn, char* dbName, char *geneName)
{
    char query[512];
    struct sqlResult *sr = NULL;
    char **row;
    struct minGeneInfo* ginfo = NULL;
    char gbProtCodeXra[50];

    if (strcmp(database, dbName) == 0)
        strcpy(gbProtCodeXra, "gbProtCodeXra");
    else
    {
        strcpy(gbProtCodeXra, dbName);
        strcat(gbProtCodeXra, ".gbProtCodeXra");
    }
    if (hTableExists(dbName, "gbProtCodeXra"))
    {
        sprintf(query, "select * from %s where name = '%s'", gbProtCodeXra, geneName);
    sr = sqlGetResult(conn, query);
        if ((row = sqlNextRow(sr)) != NULL)
    ginfo = minGeneInfoLoad(row);
    }

    if (sr != NULL)
       sqlFreeResult(&sr);
    return ginfo;
}
void getGenomeClade(struct sqlConnection *conn, char *dbName, char *genome, char *clade)
{
    char query[512];
    struct sqlResult *srDb;
    char **rowDb;
    struct sqlConnection *connCentral = hConnectCentral();

    sprintf(query, "select count(*) from genomeClade a, dbDb b, clade c where a.genome = b.genome and a.clade = c.name and b.name = '%s'",
            dbName);
    srDb = sqlGetResult(connCentral, query);
    if ((rowDb = sqlNextRow(srDb)) != NULL)
    {
        sqlFreeResult(&srDb);
        sprintf(query, "select a.genome, c.label from genomeClade a, dbDb b, clade c where a.genome = b.genome and a.clade = c.name and b.name = '%s'",
                dbName);
        srDb = sqlGetResult(connCentral, query);
        if ((rowDb = sqlNextRow(srDb)) != NULL)
        {
            strcpy(genome, rowDb[0]);
            strcpy(clade, rowDb[1]);
        }
    }
    sqlFreeResult(&srDb);
    hDisconnectCentral(&connCentral);
}
