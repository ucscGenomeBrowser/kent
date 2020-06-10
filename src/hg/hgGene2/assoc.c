#include "common.h"
#include "jksql.h"
#include "linefile.h"
#include "ra.h"
#include "trackDb.h"
#include "assoc.h"


char *getAssoc(struct sqlConnection *conn, char *table, char *field,  char *whereField, char *whereValue)
{
char query[4096];
char result[4096];

sqlSafef(query, sizeof query, "select %s from %s where %s  = '%s'", 
    field, table, whereField, whereValue);

return cloneString(sqlQuickQuery(conn, query, result, sizeof result));
}

char *assocGetRefSeq(struct sqlConnection *conn, char *id)
{
return getAssoc(conn, "gencodeToRefSeqV32", "rnaAcc", "transcriptId", id);
}

struct assoc *readAssocFromOpenRa(struct lineFile *lf)
{
struct assoc *assocList = NULL, *assoc;
struct hash *ra;
while ((ra = raNextRecord(lf)) != NULL)
    {
    AllocVar(assoc);
    slAddHead(&assocList, assoc);
    assoc->settings = ra;
    assoc->table = hashFindVal(ra, "table");
    assoc->idSql = hashFindVal(ra, "idSql");
    assoc->url = hashFindVal(ra, "url");
    assoc->shortLabel = hashFindVal(ra, "shortLabel");
    assoc->longLabel = hashFindVal(ra, "longLabel");
    }

slReverse(&assocList);

return assocList;
}

struct assoc *readAssoc(struct trackDb *tdb)
{
char *assocFile = trackDbSetting(tdb, "associations");

if (assocFile == NULL)
    return NULL;
struct lineFile *lf = udcWrapShortLineFile(assocFile, NULL, 100*1024);

struct assoc *assocs = readAssocFromOpenRa(lf);

return assocs;
}

void doAssociations(struct sqlConnection *conn, struct trackDb *tdb, char *geneId)
{
struct assoc *assocs = readAssoc(tdb);

char query[4096];
char answer[4096];
printf("<b>Associations</b>:<br>");
for(; assocs; assocs = assocs->next)
    {
    sqlSafef(query, sizeof(query), assocs->idSql, geneId);
    sqlQuickQuery(conn, query, answer, sizeof answer);
    if (!isEmpty(answer))
        {
        char buffer[4096];
        safef(buffer, sizeof buffer, assocs->url, answer);
        printf("<a href='%s'>%s</a><br>",buffer,assocs->shortLabel);
        }
    }
}
