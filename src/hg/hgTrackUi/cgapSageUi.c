#include "common.h"
#include "cheapcgi.h"
#include "hgTrackUi.h"
#include "trackDb.h"
#include "cgapSage/cgapSage.h"
#include "cgapSage/cgapSageLib.h"

static struct slName *getListFromCgapSageLibs(struct sqlConnection *conn, char *column)
/* Return [unique] list of tissues sorted alphabetically. */
{
struct slName *list = NULL;
char query[256];
char **row;
struct sqlResult *sr;
safef(query, sizeof(query), "select distinct %s from cgapSageLib order by %s", column, column);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    slNameAddHead(&list, row[0]);
    }
slReverse(&list);
sqlFreeResult(&sr);
return list;
}

static void cgapSageDropList(struct slName *choices, char *dropName, char *selected)
/* Make a drop list from the slName list. */
{
struct slName *choice;
char **items;
int i;
int size = slCount(choices);
AllocArray(items, size + 1);
items[0] = "None";
for (choice = choices, i = 1; choice != NULL; choice = choice->next, i++)
    items[i] = choice->name;
cgiMakeDropList(dropName, items, size + 1, selected);
}

void cgapSageUi(struct trackDb *tdb)
/* CGAP SAGE UI options. Highlight certain libs/tissues and filter by score. */
{
struct sqlConnection *conn = hAllocConn();
struct slName *tissueList = getListFromCgapSageLibs(conn, "tissue");
struct slName *libList = getListFromCgapSageLibs(conn, "newLibName");
char *tissueHl = cartUsualString(cart, "cgapSage.tissueHl", "None");
char *libHl = cartUsualString(cart, "cgapSage.libHl", "None");
puts("<BR><B>Tissue:</B>");
cgapSageDropList(tissueList, "cgapSage.tissueHl", tissueHl);
puts("<BR>\n");
puts("<BR><B>Library:</B>\n");
cgapSageDropList(libList, "cgapSage.libHl", libHl);
hFreeConn(&conn);
}
