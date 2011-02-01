/* gvfClick - handler for variants from GVF (http://www.sequenceontology.org/gvf.html) */

#include "common.h"
#include "htmshell.h"
#include "hgc.h"
#include "bed8Attrs.h"

void doGvf(struct trackDb *tdb, char *item)
/* Show details for variants represented as GVF, stored in a bed8Attrs table */
{
struct sqlConnection *conn = hAllocConn(database);
int start = cartInt(cart, "o");
char query[1024];
safef(query, sizeof(query),
      "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d",
      tdb->table, item, seqName, start);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("doGvfDetails: can't find item '%s' in %s at %s:%d", item, database, seqName, start);
int rowOffset = hOffsetPastBin(database, seqName, tdb->table);
struct bed8Attrs *ba = bed8AttrsLoad(row+rowOffset);
bedPrintPos((struct bed *)ba, 6, tdb);
int i = 0;
for (i = 0;  i < ba->attrCount;  i++)
    {
    // The ID is the bed8Attrs name and has already been displayed:
    if (sameString(ba->attrTags[i], "ID"))
	continue;
    cgiDecode(ba->attrVals[i], ba->attrVals[i], strlen(ba->attrVals[i]));
    char *tag = ba->attrTags[i];
    // User-defined keywords used in dbVar's GVF:
    if (sameString(tag, "var_type"))
	tag = "Variant type";
    else if (sameString(tag, "clinical_int"))
	tag = "Clinical interpretation";
    else if (islower(tag[0]))
	tag[0] = toupper(tag[0]);
    // GVF standard Start_range and End_range tags:
//    if (sameString(tag, "Start_range"))
	{
	}
//    else if (sameString(tag, "End_range"))
	{
	}
//    else
	{
	subChar(tag, '_', ' ');
	printf("<B>%s</B>: %s<BR>\n", tag, htmlEncode(ba->attrVals[i]));
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
/* printTrackHtml is done in genericClickHandlerPlus. */
}
