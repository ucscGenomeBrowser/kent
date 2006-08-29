/* mgcClick - click handling for MGC-related tracks */
#include "common.h"
#include "hgc.h"
#include "mgcClick.h"
#include "ccdsClick.h"
#include "ccdsGeneMap.h"
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
    char *organism;   /* organism name for URL, case-sensitive */
    char *server;     /* MGC server */
};

static struct mgcDb getMgcDb()
/* get the mgc database info for the current host */
{
struct mgcDb mgcDb;
mgcDb.name = "MGC";
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
    mgcDb.organism = "Dr";
    mgcDb.server = "zgc";
    }
else if (startsWith("xenTro", database))
    {
    mgcDb.name = "XGC";
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

void printMgcUrl(int imageId)
/* print out an URL to link to the MGC site for a full-length MGC clone */
{
struct mgcDb mgcDb = getMgcDb();
printf("http://%s.nci.nih.gov/Reagents/CloneInfo?ORG=%s&IMAGE=%d",
       mgcDb.server, mgcDb.organism, imageId);
}

static void printMgcOrderUrl(int gi)
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

struct mgcInfo
/* information on a MGC clone collected from various tables */
{
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
    int version;
    int imageId;
    char *refSeqAccv;  // RefSeq info
    char refSeqAcc[GENBANK_ACC_BUFSZ];
    char *refSeqSum;
};

static struct mgcInfo *mgcInfoLoad(struct sqlConnection *conn, char *acc)
/* load various piece information for a clone from various tables. Done
 * upfront and results copied to avoid problems with row caching and multiple
 * queries */
{
struct mgcInfo *mi;
AllocVar(mi);

// data from gbCdnaInfo and friends
char query[1024];
safef(query, sizeof(query),
      "select "
      "description.name, organism.name, tissue.name, library.name,"
      "development.name, geneName.name, productName.name, mrnaClone.name,"
      "cds.name,gbCdnaInfo.moddate,gbCdnaInfo.version"
      " from "
      "gbCdnaInfo,description,organism,tissue,library,development,"
      "geneName,productName,mrnaClone,cds"
      " where "
      "(acc = \"%s\") and"
      "(description = description.id) and (organism = organism.id) and"
      "(tissue = tissue.id) and (library = library.id) and"
      "(development = development.id) and (geneName = geneName.id) and"
      "(productName = productName.id) and (mrnaClone = mrnaClone.id) and"
      "(cds = cds.id)", acc);
struct sqlResult *sr = sqlMustGetResult(conn, query);
char **row = sqlNextRow(sr);
int i = 0;
mi->desc = cloneString(row[i++]);
mi->organism = cloneString(row[i++]);
mi->tissue = cloneString(row[i++]);
mi->library = cloneString(row[i++]);
mi->development = cloneString(row[i++]);
mi->geneName = cloneString(row[i++]);
mi->productName = cloneString(row[i++]);
mi->clone = cloneString(row[i++]);
mi->cds = cloneString(row[i++]);
mi->moddate = cloneString(row[i++]);
mi->version = sqlUnsigned(row[i++]);
sqlFreeResult(&sr);

// get imageId and RefSeq acc from mgc status table
char *mgcStatTbl = sqlTableExists(conn, "mgcFullStatus")
    ? "mgcFullStatus" : "mgcStatus";
safef(query, sizeof(query), "select imageId, geneName from %s where acc = \"%s\"",
      mgcStatTbl, acc);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
i = 0;
mi->imageId = sqlUnsigned(row[i++]);
if (strlen(row[i]) > 0)
    {
    mi->refSeqAccv = cloneString(row[i++]);
    genbankDropVer(mi->refSeqAcc, mi->refSeqAccv);
    }
sqlFreeResult(&sr);

// if there is a RefSeq acc, get summary if available
if (mi->refSeqAccv != NULL)
    mi->refSeqSum = getRefSeqSummary(conn, mi->refSeqAcc);
return mi;
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

static void prCloneInfo(struct mgcInfo *mi)
/* print table of clone information */
{
webPrintLinkTableStart();
prCellLabelVal("Gene", mi->geneName);
webPrintLinkTableNewRow();
prCellLabelVal("Product", mi->productName);
webPrintLinkTableNewRow();
prCellLabelVal("Tissue", mi->tissue);
webPrintLinkTableNewRow();
prCellLabelVal("Library", mi->library);
webPrintLinkTableNewRow();
prCellLabelVal("Development", mi->development);
webPrintLinkTableNewRow();
prCellLabelVal("CDS", mi->cds);
webPrintLinkTableNewRow();
prCellLabelVal("Modification date", mi->moddate);
webPrintLinkTableEnd();
}

static void prCloneLinks(struct sqlConnection *conn, char *acc, struct mgcDb *mgcDb, struct mgcInfo *mi)
/* print table of clone links */
{
int imageId = getImageId(conn, acc);
int gi = getGI(conn, acc);

webPrintLinkTableStart();
webPrintLabelCell("Links");
webPrintLinkTableNewRow();

// link to NCBI clone order CGI
if (gi > 0)
    {
    webPrintLinkCellStart();
    printf("<a href=\"");
    printMgcOrderUrl(gi);
    printf("\" TARGET=_blank>Order %s clone</a>", mgcDb->name);
    webPrintLinkCellEnd();
    webPrintLinkTableNewRow();
    }

// link to MGC database
webPrintLinkCellStart();
printf("<a href=\"");
printMgcUrl(imageId);
printf("\" TARGET=_blank>%s clone database</a>", mgcDb->name);
webPrintLinkCellEnd();

// link to IMAGE database
webPrintLinkTableNewRow();
webPrintLinkCellStart();
printf("<a href=\"");
printImageUrl(imageId);
printf("\" TARGET=_blank>I.M.A.G.E database</a>");
webPrintLinkCellEnd();

// link to Genbank
webPrintLinkTableNewRow();
webPrintLinkCellStart();
printf("<a href=\"");
printEntrezNucleotideUrl(stdout, acc);
printf("\" TARGET=_blank>Genbank entry %s</a>", acc);
webPrintLinkCellEnd();

// link to RefSeq, if available
if (mi->refSeqAccv != NULL)
    {
    webPrintLinkTableNewRow();
    webPrintLinkCellStart();
    printf("<a href=\"");
    printEntrezNucleotideUrl(stdout, mi->refSeqAcc);
    printf("\" TARGET=_blank>RefSeq entry %s</a>", mi->refSeqAcc);
    webPrintLinkCellEnd();
    }

// CCDS details if available
struct ccdsGeneMap *ccdsGenes = getCcdsGenesForMappedGene(conn, acc, "ccdsMgcMap");
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
webEndSection();
}

static void prInfoLinks(struct sqlConnection *conn, char *acc, struct mgcDb *mgcDb,
                        struct mgcInfo *mi)
/* print clone info and links */
{
webNewSection("%s Clone Information and Links", mgcDb->name);
printf("<table border=0><tr valign=top><td>\n");
prCloneInfo(mi);
printf("<td>\n");
prCloneLinks(conn, acc, mgcDb, mi);
printf("</tr></table>\n");
webEndSection();
}

static void prAlign(struct sqlConnection *conn, struct psl *psl)
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
safef(other, sizeof(other), "%d&aliTrack=%s", psl->tStart, "mgcFullMrna");
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

static void prAligns(struct sqlConnection *conn, char *acc, int start)
/* print table of alignments */
{
struct psl* pslList = getAlignments(conn, "mgcFullMrna", acc);
slSort(&pslList, pslCmpMatch);

// header, print note about order only if we have multiple alignments and didn't
// come from another details page
webNewSection("Alignments");
if ((pslList->next != NULL) && (winStart < winEnd))
    printf("<font size=-3><em>The alignment you clicked on is shown first.</em></font>\n");

webPrintLinkTableStart();
webPrintLabelCell("genomic (browser)");
webPrintLabelCell("span");
webPrintLabelCell("");
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
            prAlign(conn, psl);
            }
    }
webPrintLinkTableEnd();

webEndSection();
}

static void prMethodsLink(struct sqlConnection *conn, char *track)
{
webNewSection("Description and Methods");
printf("Click <A HREF=\"%s&g=htcTrackHtml&table=%s\">here</A> for details",
       hgcPathAndSettings(), track);
webEndSection();
}

void doMgcGenes(struct trackDb *tdb, char *acc)
/* Process click on a mgcGenes track. */
{
struct sqlConnection *conn = hAllocConn();
int start = cartInt(cart, "o");
struct mgcDb mgcDb = getMgcDb();
struct mgcInfo *mi = mgcInfoLoad(conn, acc);

// initial section
cartWebStart(cart, "%s Clone %s.%d", mgcDb.name, acc, mi->version);
printf("<B>%s</B>", mi->geneName);
printf("<BR>%s", mi->desc);
if (mi->refSeqAccv != NULL)
    printf("<BR><B>RefSeq</B>: %s", mi->refSeqAccv);
if (mi->refSeqSum != NULL)
    printf("<BR><B>RefSeq Summary</B>: %s", mi->refSeqSum);

prInfoLinks(conn, acc, &mgcDb, mi);
prSeqLinks(conn, tdb->tableName, acc);
prAligns(conn, acc, start);
prMethodsLink(conn, tdb->tableName);

webEnd();
hFreeConn(&conn);
}

