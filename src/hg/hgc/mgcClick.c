/* mgcClick - click handling for MGC and ORFEome related tracks */
#include "common.h"
#include "hgc.h"
#include "mgcClick.h"
#include "ccdsClick.h"
#include "ccdsGeneMap.h"
#include "gbMiscDiff.h"
#include "web.h"
#include "genbank.h"
#include "htmshell.h"

static char *mgcStatusDesc[][2] =
/* Table of MGC status codes to descriptions.  This is essentially duplicated
 * from MGC loading code, but we don't share the data to avoid creating some
 * dependencies.  Might want to move these to a shared file or just put these
 * in a table.  Although it is nice to be able to customize for the browser. */
{
    {"unpicked", "not picked"},
    {"picked", "picked"},
    {"notBack", "not back"},
    {"noDecision", "no decision yet"},
    {"fullLength", "full length"},
    {"fullLengthShort", "full length (short isoform)"},
    {"fullLengthSynthetic", "full length (synthetic, expression ready, no stop)"},
    {"incomplete", "incomplete"},
    {"chimeric", "chimeric"},
    {"frameShift", "frame shifted"},
    {"contaminated", "contaminated"},
    {"retainedIntron", "retained intron"},
    {"mixedWells", "mixed wells"},
    {"noGrowth", "no growth"},
    {"noInsert", "no insert"},
    {"no5est", "no 5' EST match"},
    {"microDel", "no cloning site / microdeletion"},
    {"artifact", "library artifacts"},
    {"noPolyATail", "no polyA-tail"},
    {"cantSequence", "unable to sequence"},
    {"inconsistentWithGene", "inconsistent with known gene structure"},
    {"manuallySupressed", "manually supressed"},
    {"plateContaminated", "plate contaminated"},
    {NULL, NULL}
};

static char *getMgcStatusDesc(struct sqlConnection *conn, int imageId)
/* get a description of the status of a MGC clone */
{
struct sqlResult *sr;
char **row;
char query[128];
char *desc = NULL;

/* mgcFullStatus is used on browsers with only the full-length (mgc genes)
 * track */
if (sqlTableExists(conn, "mgcStatus"))
    safef(query, sizeof(query),
          "SELECT status FROM mgcStatus WHERE (imageId = %d)", imageId);
else
    safef(query, sizeof(query),
          "SELECT status FROM mgcFullStatus WHERE (imageId = %d)", imageId);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    int i;
    for (i = 0; (mgcStatusDesc[i][0] != NULL) && (desc == NULL); i++)
        {
        if (sameString(row[0], mgcStatusDesc[i][0]))
            desc = mgcStatusDesc[i][1];
        }
    }
if (desc == NULL)
    desc = "missing or unknown MGC status; please report to browser maintainers";

sqlFreeResult(&sr);
return desc;
}                   

struct mgcDb
/* information about an MGC databases */
{
    char *name;       /* collection name */
    char *title;      /* collection title */
    char *organism;   /* organism name for URL, case-sensitive */
    char *server;     /* MGC server */
};

static struct mgcDb getMgcDb()
/* get the mgc database info for the current host */
{
struct mgcDb mgcDb;
mgcDb.name = "MGC";
mgcDb.title = "Mammalian Gene Collection";
mgcDb.server = "mgc";
/* NOTE: mgc server likes first letter of organism capitalized */
if (startsWith("hg", database))
    mgcDb.organism = "Hs";
else if (startsWith("mm", database))
    mgcDb.organism = "Mm";
else if (startsWith("rn", database))
    mgcDb.organism = "Rn";
else if (startsWith("bosTau", database))
    mgcDb.organism = "Bt";
else if (startsWith("danRer", database))
    {
    mgcDb.name = "ZGC";
    mgcDb.title = "Zebrafish Gene Collection";
    mgcDb.organism = "Dr";
    mgcDb.server = "zgc";
    }
else if (startsWith("xenTro", database))
    {
    mgcDb.name = "XGC";
    mgcDb.title = "Xenopus Gene Collection";
    mgcDb.organism = "Str";
    mgcDb.server = "xgc";
    }
else
    errAbort("can't map database \"%s\" to an MGC organism", database);
return mgcDb;
}

char *mgcDbName() 
/* get just the MGC collection name for the current ucsc database */
{
return getMgcDb().name;
}

void printMgcHomeUrl(struct mgcDb *mgcDb)
/* print out an URL to link to the MGC site */
{
printf("http://%s.nci.nih.gov/", mgcDb->server);
}

void printMgcUrl(int imageId)
/* print out an URL to link to the MGC site for a full-length MGC clone */
{
struct mgcDb mgcDb = getMgcDb();
printf("http://%s.nci.nih.gov/Reagents/CloneInfo?ORG=%s&IMAGE=%d",
       mgcDb.server, mgcDb.organism, imageId);
}

static void printOrderUrl(int gi)
/* print out an URL to link to the NCBI order CGI for a full-length MGC clone */
{
printf("http://www.ncbi.nlm.nih.gov/genome/clone/orderclone.cgi?db=nucleotide&uid=%d", gi);
}

static void printImageUrl(int imageId)
/* print out an URL to link to IMAGE database for a clone */
{
printf("http://image.llnl.gov/image/IQ/bin/singleCloneQuery?clone_id=%d", imageId);
}

void printMgcDetailsUrl(char *acc)
/* print out an URL to link to MGC details pages from another details page in
 * the browser.*/
{
// pass zero coordiates for window to indicate this isn't a browser click
printf("../cgi-bin/hgc?%s&g=mgcGenes&i=%s&l=0&r=0&db=%s",
       cartSidUrlString(cart), acc, database);
}

static void printMBLabValidDbUrl(char *acc)
/* print out an URL to link to Brent lab validation database */
{
printf("http://mblab.wustl.edu/cgi-bin/mgc.cgi?acc=%s&action=cloneRpt&alignment=Submit", acc);
}

struct cloneInfo
/* Information on a MGC or ORFeome clone collected from various tables */
{
    char *acc;
    char *desc;       // genbank info
    char *organism;
    char *tissue;
    char *library;
    char *development;
    char *geneName;
    char *productName;
    char *moddate;
    char *clone;
    char *cds;
    char *keyword;
    int version;
    int imageId;
    int gi;
    char *refSeqAccv;  // RefSeq info
    char refSeqAcc[GENBANK_ACC_BUFSZ];
    char *refSeqSum;
};

static boolean isInMBLabValidDb(char *acc)
/* check if an accession is in the Brent lab validation database */
{
boolean inMBLabValidDb = FALSE;
struct sqlConnection *fconn = sqlMayConnect("hgFixed");
if ((fconn != NULL) && sqlTableExists(fconn, "mgcMBLabValid"))
    {
    char query[64], buf[32];
    safef(query, sizeof(query), "select acc from mgcMBLabValid where acc=\"%s\"",
          acc);
    if (sqlQuickQuery(fconn, query, buf, sizeof(buf)) != NULL)
        inMBLabValidDb = TRUE;
    sqlDisconnect(&fconn);
    }
return inMBLabValidDb;
}

static int getGI(struct sqlConnection *conn, char *acc)
/* get the GI number from gbCdnaInfo, or -1 there is no GI column */
{
if (sqlFieldIndex(conn, "gbCdnaInfo", "gi") < 0)
    return -1;
else
    {
    char query[128];
    safef(query, sizeof(query), "select gi from gbCdnaInfo where acc=\"%s\"", acc);
    return sqlQuickNum(conn, query);
    }
}

static struct cloneInfo *cloneInfoLoad(struct sqlConnection *conn, char *acc)
/* Load various piece information for a clone from gbCdnaInfo relational
 * tables. */
{
struct cloneInfo *ci;
AllocVar(ci);
ci->acc = acc;

// data from gbCdnaInfo and friends
char query[1024];
safef(query, sizeof(query),
      "select "
      "description.name, organism.name, tissue.name, library.name,"
      "development.name, geneName.name, productName.name, mrnaClone.name,"
      "cds.name,keyword.name,gbCdnaInfo.moddate,gbCdnaInfo.version"
      " from "
      "gbCdnaInfo,description,organism,tissue,library,development,"
      "geneName,productName,mrnaClone,cds,keyword"
      " where "
      "(acc = \"%s\") and"
      "(description = description.id) and (organism = organism.id) and"
      "(tissue = tissue.id) and (library = library.id) and"
      "(development = development.id) and (geneName = geneName.id) and"
      "(productName = productName.id) and (mrnaClone = mrnaClone.id) and"
      "(cds = cds.id) and (keyword = keyword.id)", acc);
struct sqlResult *sr = sqlMustGetResult(conn, query);
char **row = sqlNextRow(sr);
int i = 0;
ci->desc = cloneString(row[i++]);
ci->organism = cloneString(row[i++]);
ci->tissue = cloneString(row[i++]);
ci->library = cloneString(row[i++]);
ci->development = cloneString(row[i++]);
ci->geneName = cloneString(row[i++]);
ci->productName = cloneString(row[i++]);
ci->clone = cloneString(row[i++]);
ci->cds = cloneString(row[i++]);
ci->keyword = cloneString(row[i++]);
ci->moddate = cloneString(row[i++]);
ci->version = sqlUnsigned(row[i++]);
sqlFreeResult(&sr);
ci->gi = getGI(conn, acc);
return ci;
}

static struct cloneInfo *mgcCloneInfoLoad(struct sqlConnection *conn, char *acc)
/* Load MGC clone information */
{
struct cloneInfo *ci = cloneInfoLoad(conn, acc);

// get imageId and RefSeq acc from mgc status table
char *mgcStatTbl = sqlTableExists(conn, "mgcFullStatus")
    ? "mgcFullStatus" : "mgcStatus";
char query[1024];
safef(query, sizeof(query), "select imageId, geneName from %s where acc = \"%s\"",
      mgcStatTbl, acc);
struct sqlResult *sr = sqlMustGetResult(conn, query);
char **row = sqlNextRow(sr);
int i = 0;
ci->imageId = sqlUnsigned(row[i++]);
if (strlen(row[i]) > 0)
    {
    ci->refSeqAccv = cloneString(row[i++]);
    genbankDropVer(ci->refSeqAcc, ci->refSeqAccv);
    }
sqlFreeResult(&sr);

// if there is a RefSeq acc, get summary if available
if (ci->refSeqAccv != NULL)
    ci->refSeqSum = getRefSeqSummary(conn, ci->refSeqAcc);

return ci;
}      

static struct cloneInfo *orfeomeCloneInfoLoad(struct sqlConnection *conn, char *acc)
/* Load ORFeome clone information */
{
struct cloneInfo *ci = cloneInfoLoad(conn, acc);
ci->imageId = getImageId(conn, acc);
return ci;
}

void printMgcRnaSpecs(struct trackDb *tdb, char *acc, int imageId)
/* print status information for MGC mRNA or EST; must have imageId */
{
struct sqlConnection *conn = hgAllocConn();
struct mgcDb mgcDb = getMgcDb();
char *statusDesc;
/* add status description */
statusDesc = getMgcStatusDesc(conn, imageId);
if (statusDesc != NULL)
    printf("<B>%s status:</B> %s<BR>", mgcDb.name, statusDesc);
hFreeConn(&conn);
}

static void prCellLabelVal(char *label, char *val)
/* print label and value as adjacent cells  */
{
webPrintLabelCell(label);
webPrintLinkCell(val);
}

static void prCloneInfo(struct cloneInfo *ci)
/* print table of clone information */
{
webPrintLinkTableStart();
prCellLabelVal("Gene", ci->geneName);
webPrintLinkTableNewRow();
prCellLabelVal("Product", ci->productName);
webPrintLinkTableNewRow();
prCellLabelVal("Tissue", ci->tissue);
webPrintLinkTableNewRow();
prCellLabelVal("Library", ci->library);
webPrintLinkTableNewRow();
prCellLabelVal("Development", ci->development);
webPrintLinkTableNewRow();
prCellLabelVal("CDS", ci->cds);
webPrintLinkTableNewRow();
prCellLabelVal("Modification date", ci->moddate);
webPrintLinkTableEnd();
}

static void prSeqLinks(struct sqlConnection *conn, char *table, char *acc)
/* print table of sequence links */
{
webNewSection("Sequences");
webPrintLinkTableStart();

webPrintLinkCellStart();
hgcAnchorSomewhere("htcDisplayMrna", acc, table, seqName);
printf("mRNA</a>"); 
webPrintLinkCellEnd();

webPrintLinkCellStart();
hgcAnchorSomewhere("htcTranslatedMRna", acc, table, seqName);
printf("Protein</A><br>");
webPrintLinkCellEnd();

webPrintLinkCellStart();
hgcAnchorSomewhere("htcGeneInGenome", acc, table, seqName);
printf("Genomic</A>");
webPrintLinkCellEnd();

webPrintLinkTableNewRow();

webPrintLinkCellStart();
hgcAnchorSomewhere("htcGeneMrna", acc, table, seqName);
printf("Reference genome mRNA</A>");
webPrintLinkCellEnd();

webPrintLinkCellStart();
hgcAnchorSomewhere("htcTranslatedPredMRna", acc, table, seqName);
printf("Reference genome protein</A>");
webPrintLinkCellEnd();

webFinishPartialLinkTable(1, 2, 3);

webPrintLinkTableEnd();
}

static void prAlign(struct sqlConnection *conn, char *pslTbl, struct psl *psl)
/* print an alignment */
{
// genomic location
webPrintLinkCellStart();
printf("<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">%s:%d-%d</A>",
       hgTracksPathAndSettings(), database, psl->tName, psl->tStart+1, psl->tEnd,
       psl->tName, psl->tStart+1, psl->tEnd);
webPrintLinkCellEnd();

// genomic span
webPrintLinkCellRightStart();
printf("%d", psl->tEnd-psl->tStart);
webPrintLinkCellEnd();

// strand
webPrintLinkCell(psl->strand);

// mRNA location, linked to aligment viewer
webPrintLinkCellStart();
char other[128];
safef(other, sizeof(other), "%d&aliTrack=%s", psl->tStart, pslTbl);
hgcAnchorSomewhere("htcCdnaAli", psl->qName, other, psl->tName);
printf("%s:%d-%d</A>", psl->qName, psl->qStart+1, psl->qEnd);
webPrintLinkCellEnd();

// identity
webPrintLinkCellRightStart();
printf("%.1f%%", 100.0 - pslCalcMilliBad(psl, TRUE) * 0.1);
webPrintLinkCellEnd();

// fraction aligned
webPrintLinkCellRightStart();
int aligned = psl->match + psl->misMatch + psl->repMatch;
printf("%.1f%%", 100.0*aligned/((float)psl->qSize));
webPrintLinkCellEnd();
}

static void prAligns(struct sqlConnection *conn, char *pslTbl, char *acc, int start)
/* print table of alignments */
{
struct psl* pslList = getAlignments(conn, pslTbl, acc);
assert(pslList != NULL);
slSort(&pslList, pslCmpMatch);

// header, print note about order only if we have multiple alignments and didn't
// come from another details page
webNewSection("Alignments");
if ((pslList->next != NULL) && (winStart < winEnd))
    printf("<font size=-1><em>The alignment you clicked on is shown first.</em></font>\n");

webPrintLinkTableStart();
webPrintLabelCell("genomic (browser)");
webPrintLabelCell("span");
webPrintLabelCell("&nbsp;");
webPrintLabelCell("mRNA (alignment details)");
webPrintLabelCell("identity");
webPrintLabelCell("aligned");

// print with clicked alignment first
struct psl* psl;
int pass;
for (pass = 1; pass <= 2; pass++)
    {
    for (psl = pslList; psl != NULL; psl = psl->next)
        if ((pass == 1) == (psl->tStart == start))
            {
            webPrintLinkTableNewRow();
            prAlign(conn, pslTbl, psl);
            }
    }
webPrintLinkTableEnd();
}

enum gbMiscDiffFields
/* optional fields in gbMiscDiff table */
{
    gbMiscDiffNotes = 0x01,
    gbMiscDiffGene = 0x02,
    gbMiscDiffReplace = 0x04
};

static unsigned getMiscDiffFields(struct gbMiscDiff *gmds)
/* find which fields are present */
{
struct gbMiscDiff *gmd;
unsigned flds = 0;
for (gmd = gmds; gmd != NULL; gmd = gmd->next)
    {
    if (gmd->notes != NULL)
        flds |= gbMiscDiffNotes;
    if (gmd->gene != NULL)
        flds |=gbMiscDiffGene;
    if (gmd->replacement != NULL)
        flds |= gbMiscDiffReplace;
    }
return flds;
}

static void prMiscDiffHdr(unsigned miscDiffFlds)
/* print a header row for miscDiffs table */
{
webPrintLabelCell("mRNA start");
webPrintLabelCell("mRNA end");
if (miscDiffFlds & gbMiscDiffGene)
    webPrintLabelCell("Gene");
if (miscDiffFlds & gbMiscDiffReplace)
    webPrintLabelCell("Replace");
if (miscDiffFlds & gbMiscDiffNotes)
    webPrintLabelCell("Notes");
}

static void prMiscDiff(struct gbMiscDiff *gmd, unsigned miscDiffFlds)
/* print any gbMiscDiff row */
{
webPrintLinkTableNewRow();
webPrintIntCell(gmd->mrnaStart);
webPrintIntCell(gmd->mrnaEnd);
if (miscDiffFlds & gbMiscDiffGene)
    webPrintLinkCell(gmd->gene);
if (miscDiffFlds & gbMiscDiffReplace)
    webPrintLinkCell(gmd->replacement);
if (miscDiffFlds & gbMiscDiffNotes)
    webPrintLinkCell(gmd->notes);
}

static void prMiscDiffs(struct sqlConnection *conn, char *acc)
/* print any gbMiscDiff rows for the accession */
{
struct gbMiscDiff *gmds = NULL, *gmd;
if (sqlTableExists(conn, "gbMiscDiff"))
    gmds = sqlQueryObjs(conn, (sqlLoadFunc)gbMiscDiffLoad, sqlQueryMulti,
                        "select * from gbMiscDiff where acc=\"%s\"", acc);
if (gmds != NULL)
    {
    unsigned miscDiffFlds = getMiscDiffFields(gmds);
    webNewSection("NCBI Clone Validation");
    webPrintLinkTableStart();
    prMiscDiffHdr(miscDiffFlds);
    for (gmd = gmds; gmd != NULL; gmd = gmd->next)
        prMiscDiff(gmd, miscDiffFlds);
    webPrintLinkTableEnd();
    }
}

static void prMethodsLink(struct sqlConnection *conn, char *track)
/* generate link to methods page */
{
webNewSection("Description and Methods");
printf("Click <A HREF=\"%s&g=htcTrackHtml&table=%s&c=%s&l=%d&r=%d\">here</A> for details",
       hgcPathAndSettings(), track, seqName, winStart, winEnd);
}

static boolean canOrderClone(boolean isMgc, struct cloneInfo *ci)
/* all clones in MGC can be ordered. 
 * FIXME: isMgc flag is provided because some synthetic clones don't have
 * db_xref MGC:, which they should */
{
return (ci->gi > 0) && (isMgc || strstr(ci->clone, "MGC:"));
}

static void prOrderLink(char *name, struct cloneInfo *ci)
/* create link to NCBI clone order CGI */
{
webPrintLinkTableNewRow();
webPrintLinkCellStart();
printf("<a href=\"");
printOrderUrl(ci->gi);
printf("\" TARGET=_blank>Order %s clone</a>", name);
webPrintLinkCellEnd();
}

static void prImageLink(struct cloneInfo *ci)
/* create link to IMAGE database */
{
webPrintLinkTableNewRow();
webPrintLinkCellStart();
printf("<a href=\"");
printImageUrl(ci->imageId);
printf("\" TARGET=_blank>IMAGE clone %d</a>", ci->imageId);
webPrintLinkCellEnd();
}

static void prGenbankLink(struct cloneInfo *ci)
/* create link to Genbank database */
{
webPrintLinkTableNewRow();
webPrintLinkCellStart();
printf("<a href=\"");
printEntrezNucleotideUrl(stdout, ci->acc);
printf("\" TARGET=_blank>Genbank %s</a>", ci->acc);
webPrintLinkCellEnd();
}

static void prMgcCloneLinks(struct sqlConnection *conn, struct mgcDb *mgcDb, struct cloneInfo *ci)
/* print table of clone links */
{
webPrintLinkTableStart();
webPrintLabelCell("Links");

// link to NCBI clone order CGI
if (canOrderClone(TRUE, ci))
    prOrderLink(mgcDb->name, ci);

// link to MGC database
webPrintLinkTableNewRow();
webPrintLinkCellStart();
printf("<a href=\"");
printMgcUrl(ci->imageId);
printf("\" TARGET=_blank>%s clone database</a>", mgcDb->name);
webPrintLinkCellEnd();

prImageLink(ci);
prGenbankLink(ci);

// link to RefSeq, if available
if (ci->refSeqAccv != NULL)
    {
    webPrintLinkTableNewRow();
    webPrintLinkCellStart();
    printf("<a href=\"");
    printEntrezNucleotideUrl(stdout, ci->refSeqAcc);
    printf("\" TARGET=_blank>RefSeq %s</a>", ci->refSeqAcc);
    webPrintLinkCellEnd();
    }

// CCDS details if available
struct ccdsGeneMap *ccdsGenes = getCcdsGenesForMappedGene(conn, ci->acc, "ccdsMgcMap");
if (ccdsGenes != NULL)
    {
    webPrintLinkTableNewRow();
    webPrintLinkCellStart();
    struct ccdsGeneMap *gene;
    for (gene = ccdsGenes; gene != NULL; gene = gene->next)
        {
        if (gene != ccdsGenes)
            printf(", ");
        printf("<A href=\"");
        printCcdsUrlForMappedGene(conn, gene);
        printf("\">%s</A>", gene->ccdsId);
        }
    webPrintLinkCellEnd();
    }

// Brent lab validation database
if (isInMBLabValidDb(ci->acc))
    {
    webPrintLinkTableNewRow();
    webPrintLinkCellStart();
    printf("<a href=\"");
    printMBLabValidDbUrl(ci->acc);
    printf("\" TARGET=_blank>Brent Lab Clone Validation</a>");
    webPrintLinkCellEnd();
    }
webPrintLinkTableEnd();
}

static void prMgcInfoLinks(struct sqlConnection *conn, char *acc, struct mgcDb *mgcDb,
                        struct cloneInfo *ci)
/* print clone info and links */
{
webNewSection("%s Clone Information and Links", mgcDb->name);
printf("<table border=0><tr valign=top><td>\n");
prCloneInfo(ci);
printf("<td>\n");
prMgcCloneLinks(conn, mgcDb, ci);
printf("</tr></table>\n");
}

void doMgcGenes(struct trackDb *tdb, char *acc)
/* Process click on a mgcGenes track. */
{
struct sqlConnection *conn = hAllocConn();
int start = cartInt(cart, "o");
struct mgcDb mgcDb = getMgcDb();
struct cloneInfo *mi = mgcCloneInfoLoad(conn, acc);

// initial section
cartWebStart(cart, "%s Clone %s.%d", mgcDb.name, acc, mi->version);
printf("<B>%s</B>\n", mi->geneName);
printf("<BR>%s\n", mi->desc);
if (mi->refSeqAccv != NULL)
    printf("<BR><B>RefSeq</B>: %s\n", mi->refSeqAccv);
if (mi->refSeqSum != NULL)
    printf("<BR><B>RefSeq Summary</B>: %s\n", mi->refSeqSum);
printf("<BR><B>Clone Source</B>: <A href=\"");
printMgcHomeUrl(&mgcDb);
printf("\" TARGET=_blank>%s</A>\n", mgcDb.title);

prMgcInfoLinks(conn, acc, &mgcDb, mi);
prSeqLinks(conn, tdb->tableName, acc);
prAligns(conn, "mgcFullMrna", acc, start);
prMiscDiffs(conn, acc);
prMethodsLink(conn, tdb->tableName);

hFreeConn(&conn);
}

static void prOrfeomeCloneLinks(struct sqlConnection *conn, char *acc, struct cloneInfo *ci)
/* print table of clone links */
{
webPrintLinkTableStart();
webPrintLabelCell("Links");
webPrintLinkTableNewRow();

// link to NCBI clone order CGI
// FIXME: can't currently figure out if an ORFeome clone is can be ordered,
// even those also in MGC seem broken.
if (canOrderClone(FALSE, ci))
    prOrderLink("ORFeome", ci);

#if 0
// link to ORFeome database
// FIXME: this doesn't appear to work, need to ask Christa
// http://www.orfeomecollaboration.org/bin/cloneStatus.pl
#endif

if (ci->imageClone > 0)
    prImageLink(ci);
prGenbankLink(ci);

webPrintLinkTableEnd();
}

static void prOrfeomeInfoLinks(struct sqlConnection *conn, char *acc, struct cloneInfo *ci)
/* print clone info and links */
{
webNewSection("ORFeome Clone Information and Links");
printf("<table border=0><tr valign=top><td>\n");
prCloneInfo(ci);
printf("<td>\n");
prOrfeomeCloneLinks(conn, acc, ci);
printf("</tr></table>\n");
}

void doOrfeomeGenes(struct trackDb *tdb, char *acc)
/* Process click on a orfeomeGenes track. */
{
struct sqlConnection *conn = hAllocConn();
int start = cartInt(cart, "o");
struct cloneInfo *mi = orfeomeCloneInfoLoad(conn, acc);

// initial section
cartWebStart(cart, "ORFeome Clone %s.%d", acc, mi->version);
printf("<BR><B>Clone Source</B>: <A href=\"http://www.orfeomecollaboration.org/\""
       " TARGET=_blank>ORFeome collaboration</A>\n");

prOrfeomeInfoLinks(conn, acc, mi);
prSeqLinks(conn, tdb->tableName, acc);
prAligns(conn, "orfeomeMRna", acc, start);
prMiscDiffs(conn, acc);
prMethodsLink(conn, tdb->tableName);

hFreeConn(&conn);
}
