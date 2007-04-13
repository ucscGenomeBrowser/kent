/* Handle details page for CGAP SAGE track. */

#include "common.h"
#include "cart.h"
#include "hgc.h"
#include "hCommon.h"
#include "hgColors.h"
#include "web.h"
#include "cgapSage/cgapSage.h"
#include "cgapSage/cgapSageLib.h"

static struct cgapSage *cgapSageTagLoad(struct sqlConnection *conn, struct trackDb *tdb)
/* Return the positional info (bed) with the list of libs/measurements. */
{
struct cgapSage *tag = NULL;
struct sqlResult *sr;
char **row;
char *chrom = cartString(cart, "c");
int start = cartInt(cart, "o");
int end = cartUsualInt(cart, "t", 0);
int rowOffset;
char extraWhere[128];
safef(extraWhere, sizeof(extraWhere), "chromStart=%d and chromEnd=%d", start, end);
sr = hOrderedRangeQuery(conn, tdb->tableName, chrom, start, end,
			extraWhere, &rowOffset);
row = sqlNextRow(sr);
if (row != NULL)
    tag = cgapSageLoad(row+rowOffset);
sqlFreeResult(&sr);
return tag;
}

static struct cgapSageLib *loadLibTable(struct sqlConnection *conn)
/* Just load up the whole cgapSageLib table. */
{
struct cgapSageLib *libs = NULL;
char query[256];
safef(query, sizeof(query), "select * from cgapSageLib");
libs = cgapSageLibLoadByQuery(conn, query);
return libs;
}

static struct hash *getLibHash(struct sqlConnection *conn)
/* Hash up a loaded cgapSageLib table. */
{
struct hash *hash = newHash(9);
struct cgapSageLib *libs = loadLibTable(conn);
struct cgapSageLib *lib;
for (lib = libs; lib != NULL; lib = lib->next)
    {
    char s[16];
    safef(s, sizeof(s), "%d", lib->libId);
    hashAdd(hash, s, lib);
    }
return hash;
}

static void printCgapSageHeader(struct cgapSage *tag)
/* Print basic information about the tag at the top. */
{
printf("<B>Tag sequence</B>: %s<BR>\n", tag->name);
printf("<B>Position:</B> "
       "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">%s:%d-%d</a><BR>\n",
       hgTracksPathAndSettings(), database, tag->chrom, tag->chromStart+1, tag->chromEnd, tag->chrom, tag->chromStart+1, tag->chromEnd);
if (tag->numSnps > 0)
    {
    int i;
    printf("<B>Number of SNPs associated with tag</B>: %d<BR>\n", tag->numSnps);
    printf("<B>SNPs associated with tag</B>: ");
    for (i = 0; i < tag->numSnps-1; i++)
	printf("%s, ", tag->snps[i]);
    printf("%s<BR>\n", tag->snps[i]);
    }
}

static void stringCellLabelVal(char *label, char *val, boolean newRow)
/* print label and value as adjacent cells  */
{
if (val && !sameString(val, ""))
    {
    webPrintLabelCell(label);
    webPrintLinkCell(val);
    if (newRow)
	webPrintLinkTableNewRow();
    }
}

static void intCellLabelVal(char *label, int val, boolean newRow)
/* much like stringCellLabelVal, but allowing numbers. */
{
char buf[32];
safef(buf, sizeof(buf), "%d", val);
stringCellLabelVal(label, buf, newRow);
}

static void printCgapLibSection(struct hash *libHash, char *libId)
/* Print a section about a library. */
{
struct cgapSageLib *lib = hashMustFindVal(libHash, libId);
webNewSection("CGAP Library Information");
webPrintLinkTableStart();
stringCellLabelVal("Library Name", lib->newLibName, TRUE);
stringCellLabelVal("Old Library Name", lib->oldLibName, TRUE);
intCellLabelVal("Total tags including linker", lib->totalTags, TRUE);
intCellLabelVal("Total tags excluding linker", lib->totalTagsNoLinker, TRUE);
intCellLabelVal("Unique tags", lib->uniqueTags, TRUE);
intCellLabelVal("Library quality", lib->quality, TRUE);
stringCellLabelVal("Organ/tissue", lib->tissue, TRUE);
stringCellLabelVal("Tissue preparation", lib->tissuePrep, TRUE);
stringCellLabelVal("Cell type", lib->cellType, TRUE);
stringCellLabelVal("Keywords", lib->keywords, TRUE);
stringCellLabelVal("Age", lib->age, TRUE);
stringCellLabelVal("Mutations", lib->mutations, TRUE);
stringCellLabelVal("Other information", lib->otherInfo, TRUE);
stringCellLabelVal("Tagging enzyme", lib->tagEnzyme, TRUE);
stringCellLabelVal("Anchoring enzyme", lib->anchorEnzyme, TRUE);
stringCellLabelVal("Tissue or cell line supplier", lib->cellSupplier, TRUE);
stringCellLabelVal("Library producer", lib->libProducer, TRUE);
stringCellLabelVal("Laboratory", lib->laboratory, TRUE);
stringCellLabelVal("References", lib->refs, FALSE);
webPrintLinkTableEnd();
}

static void tagTableRow(struct cgapSage *tag, unsigned libId, 
        unsigned freq, double tagTpm, 
	struct hash *libHash, char *clickedLibId, char *clickedTissue) 
/* Print one of the rows in the tag information section table.  */
{
struct cgapSageLib *lib;
char s[256];
safef(s, sizeof(s), "%d", libId);
lib = (struct cgapSageLib *)hashFindVal(libHash, s);
if ((clickedLibId && sameString(s, clickedLibId)) ||
    (clickedTissue && sameString(clickedTissue, lib->tissue)))
    {
    safef(s, sizeof(s), "<B>%d</B>", libId);
    webPrintLinkCell(s);
    safef(s, sizeof(s), "<A HREF=\"%s&o=%d&t=%d&c=%s&g=cgapSage&i=libId.%d\"><B>%s</B></A>", 
	  hgcPathAndSettings(), tag->chromStart, tag->chromEnd, tag->chrom, libId,
	  lib->newLibName);
    webPrintLinkCell(s);
    safef(s, sizeof(s), "<B>%s</B>", lib->tissue);
    webPrintLinkCell(s);
    safef(s, sizeof(s), "<B>%d</B>", freq);
    webPrintLinkCell(s);
    safef(s, sizeof(s), "<B>%d</B>", lib->totalTags);
    webPrintLinkCell(s);
    safef(s, sizeof(s), "<B>%.3f</B>", tagTpm);
    webPrintLinkCell(s);
    }
else
    {
    safef(s, sizeof(s), "%d", libId);
    webPrintLinkCell(s);  
    safef(s, sizeof(s), "<A HREF=\"%s&o=%d&t=%d&c=%s&g=cgapSage&i=libId.%d\">%s</A>", hgcPathAndSettings(), 
	  tag->chromStart, tag->chromEnd, tag->chrom, libId,
	  lib->newLibName);
    webPrintLinkCell(s);
    webPrintLinkCell(lib->tissue);
    safef(s, sizeof(s), "%d", freq);
    webPrintLinkCell(s);
    safef(s, sizeof(s), "%d", lib->totalTags);
    webPrintLinkCell(s);
    safef(s, sizeof(s), "%.3f", tagTpm);
    webPrintLinkCell(s);
    }
}

static void printCgapTagSection(struct cgapSage *tag, struct hash *libHash, char *itemName)
/* Print section with frequency and TPM info for all the libs with the tag. */
{
int i;
int N = tag->numLibs;
char *libId = NULL;
char *tissue = NULL;
if (startsWith("libId", itemName))
    libId = itemName + 6;
else
    tissue = itemName;
/* char *libSex = cgapSageLibSex(lib->sex); */
webNewSection("Libraries With Selected SAGE Tag");
webPrintLinkTableStart();
webPrintLabelCell("Library ID");
webPrintLabelCell("Library Name");
webPrintLabelCell("Tissue");
webPrintLabelCell("Tag Frequency");
webPrintLabelCell("Total Tags in Library");
webPrintLabelCell("Tags Per Million");

webPrintLinkTableNewRow();
for (i = 0; i < N-1; i++)
    {
    tagTableRow(tag, tag->libIds[i], tag->freqs[i], tag->tagTpms[i], libHash, libId, tissue);
    webPrintLinkTableNewRow();
    }
tagTableRow(tag, tag->libIds[N-1], tag->freqs[N-1], tag->tagTpms[N-1], libHash, libId, tissue);
webPrintLinkTableEnd();
}

void doCgapSage(struct trackDb *tdb, char *itemName)
/* CGAP SAGE details. */
{
struct sqlConnection *conn = hAllocConn();
struct cgapSage *tag = cgapSageTagLoad(conn, tdb);
struct hash *libHash = getLibHash(conn);
genericHeader(tdb, NULL);
/* Print out tag information. */
printCgapSageHeader(tag);
if (startsWith("libId", itemName))
    {
    printCgapLibSection(libHash, itemName + 6);    
    }
printCgapTagSection(tag, libHash, itemName);
webNewSection("Track Details");
printf("%s", tdb->html);
cgapSageFree(&tag);
hFreeConn(&conn);
hashFreeWithVals(&libHash, cgapSageLibFree);
}
