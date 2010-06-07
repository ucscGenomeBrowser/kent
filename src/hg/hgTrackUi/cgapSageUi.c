#include "common.h"
#include "cheapcgi.h"
#include "hgTrackUi.h"
#include "trackDb.h"
#include "cgapSage/cgapSage.h"
#include "cgapSage/cgapSageLib.h"

static struct slName *getListFromCgapSageLibs(struct sqlConnection *conn, char *column, boolean returnIds, boolean distinct)
/* Return [unique] list of tissues sorted alphabetically. */
{
struct slName *list = NULL;
char query[256];
char **row;
struct sqlResult *sr;
safef(query, sizeof(query), "select %s%s%s from cgapSageLib order by %s", (distinct) ? "distinct " : "", 
      column, (returnIds) ? ",libId" : "", column);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *word = (returnIds) ? row[1] : row[0];
    slNameAddHead(&list, word);
    }
slReverse(&list);
sqlFreeResult(&sr);
return list;
}

static void cgapSageDropList(struct slName *choices, struct slName *valList, char *dropName, char *selected)
/* Make a drop list from the slName list. */
{
struct slName *choice;
char **items;
char **vals;
int i;
int size = slCount(choices);
AllocArray(items, size + 1);
items[0] = "All";
for (choice = choices, i = 1; choice != NULL; choice = choice->next, i++)
    items[i] = choice->name;
if (valList)
    {
    struct slName *val;
    int vSize = slCount(valList);
    if (size != vSize)
	errAbort("Number of Lib IDs and Lib Names should be the same but they aren not.");
    AllocArray(vals, size + 1);
    vals[0] = "All";
    for (val = valList, i = 1; val != NULL; val = val->next, i++)
	vals[i] = val->name;
    cgiMakeDropListWithVals(dropName, items, vals, size + 1, selected);
    }
else
    cgiMakeDropList(dropName, items, size + 1, selected);
}

void cgapSageUi(struct trackDb *tdb)
/* CGAP SAGE UI options. Highlight certain libs/tissues and filter by score. */
{
struct sqlConnection *conn = hAllocConn(database);
struct slName *tissueList = getListFromCgapSageLibs(conn, "tissue", FALSE, TRUE);
char *tissueHl = cartUsualString(cart, "cgapSage.tissueHl", "All");
puts("<BR><B>Tissue:</B>");
cgapSageDropList(tissueList, NULL, "cgapSage.tissueHl", tissueHl);
hFreeConn(&conn);
slNameFreeList(&tissueList);
}
