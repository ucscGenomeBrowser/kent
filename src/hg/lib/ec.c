#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "ecCode.h"
#include "ecAttribute.h"
#include "ecAttributeCode.h"

void getEcHtml(char *ecNumber)
/* fetch ec codes and descriptions and output html */
{
char query[1024];
struct sqlConnection *conn = hAllocConn("ec");
char *level1 = NULL;
char *level2 = NULL;
char *level3 = NULL;
//char *level4 = NULL;
struct ecAttribute attr;
char **row = NULL;
struct sqlResult *sr = NULL;

if (ecNumber == NULL)
    return;
if (conn == NULL)
    return;
safef(query,sizeof(query), "select distinct e.description from ecAttribute a , ecCode e where a.ec = \"%s\" and a.level1 = e.level1 and e.level2 = 0 ",ecNumber);
level1 = sqlQuickString(conn, query);
safef(query,sizeof(query), "select distinct e.description from ecAttribute a , ecCode e where a.ec = \"%s\" and a.level1 = e.level1 and a.level2 = e.level2 and e.level3 = 0 ",ecNumber);
level2 = sqlQuickString(conn, query);
safef(query,sizeof(query), "select distinct e.description from ecAttribute a , ecCode e where a.ec = \"%s\" and a.level1 = e.level1 and a.level2 = e.level2 and a.level3 = e.level3 and e.level4 = 0 ",ecNumber);
level3 = sqlQuickString(conn, query);
//safef(query,sizeof(query), "select distinct description from ecAttribute a where a.ec = \"%s\" ",ecNumber);
//level4 = sqlQuickString(conn, query);

printf("[ %s / %s / %s ] <BR>",
        (level1 != NULL) ? level1 :"n/a", 
        (level2 != NULL) ? level2 :"n/a", 
        (level3 != NULL) ? level3 :"n/a"
//        (level4 != NULL) ? level4 :"n/a"
        );
safef(query,sizeof(query), "select * from ecAttribute a where a.ec = \"%s\"",ecNumber);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *attrDesc = NULL;
    struct sqlConnection *conn2 = hAllocConn("ec");
    ecAttributeStaticLoad(row, &attr);
    safef(query,sizeof(query), "select description from ecAttributeCode where type = \"%s\" ",attr.type);
    attrDesc = sqlQuickString(conn2, query);
    if (differentString(attr.type, "DR"))
        printf("<B>EC %s:</B> %s<BR>", attrDesc != NULL ? attrDesc : "n/a",attr.description);
    hFreeConn(&conn2);
    }
hFreeConn(&conn);
}
