/* gvfClick - handler for variants from GVF (http://www.sequenceontology.org/gvf.html) */

#include "common.h"
#include "htmshell.h"
#include "hgc.h"
#include "bed8Attrs.h"

static boolean isInteger(char *word)
/* Return TRUE if all characters of word are numeric. */
{
if (word == NULL)
    return FALSE;
char *s = word;
while (*s != '\0')
    if (! isdigit(*s++))
	return FALSE;
return TRUE;
}

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
// Note: this loop modifies ba->attrVals[i], assuming we won't use them again:
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
	// Uppercase for nice display, assuming user doesn't care which keywords are
	// user-defined vs. GVF standard:
	tag[0] = toupper(tag[0]);
    // GVF standard Start_range and End_range tags (1-based coords):
    if (sameString(tag, "Start_range") || sameString(tag, "End_range"))
	{
	char *copy = cloneString(ba->attrVals[i]);
	char *words[3];
	int wordCount = chopCommas(copy, words);
	if (wordCount == 2 &&
	    (sameString(".", words[0]) || isInteger(words[0])) &&
	    (sameString(".", words[1]) || isInteger(words[1])))
	    {
	    boolean isStartRange = sameString(tag, "Start_range");
	    char *rangeStart = words[0], *rangeEnd = words[1];
	    if (sameString(".", rangeStart))
		rangeStart = "unknown";
	    if (sameString(".", rangeEnd))
		rangeEnd = "unknown";
	    if (isStartRange)
		printf("<B>Start range</B>: outer start %s, inner start %s<BR>\n",
		       rangeStart, rangeEnd);
	    else
		printf("<B>End range</B>: inner end %s, outer end %s<BR>\n",
		       rangeStart, rangeEnd);
	    }
	else
	    // not formatted as expected, just print as-is:
	    printf("<B>%s</B>: %s<BR>\n", tag, htmlEncode(ba->attrVals[i]));
	}
    else
	{
	subChar(tag, '_', ' ');
	printf("<B>%s</B>: %s<BR>\n", tag, htmlEncode(ba->attrVals[i]));
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
/* printTrackHtml is done in genericClickHandlerPlus. */
}
