/* transMapClick - transMap click handling */

/*
 * Tables associated with transMap:
 *   transMapXxxGene - gene predictions
 *   transMapXxxAli - mapped PSLs aligments
 *   transMapXxxAliGene - mapped alignmetns with CDS and frame annotation
 *   transMapXxxChk - gene-check results
 *   transMapXxxChkDetails - gene-check details
 *   transMapXxxInfo - mapping information, including source.
 *   transMapXxxAttr - optional coloring attr table
 *
 * Xxx is:
 *    - Ref - RefSeq genes
 *    - MRna - GenBank mRNAs
 * other prefixes supported:
 *    tranMapAnc - ancestor tree mappings
 *    transMapSrc -sources genes (for debugging), *Info table is not required.
 */

#include "common.h"
#include "hgc.h"
#include "transMapClick.h"
#include "transMapInfo.h"
#include "geneCheck.h"
#include "geneCheckDetails.h"
#include "geneCheckWidget.h"
#include "genbank.h"

/* space to allocate for a id */
#define ID_BUFSZ 64

struct mappingInfo
/* various pieces of information about mapping from table name and 
 * transMapXxxInfo table */
{
    char mappedId[64];         /* id of mapped gene */
    char tblPre[64];           /* table prefix */
    char geneSet[6];           /* source gene set abbrv used in table name */
    struct transMapInfo *tmi;  /* general info for mapped gene (NULL for src gene) */
    boolean isSrc;             /* is this a source mapping? */
    boolean indirect;          /* an indirect mapping */
    char gbAcc[ID_BUFSZ];      /* src accession */
    short gbVer;               /* version from gbId */
    char seqId[ID_BUFSZ];      /* id used to look up sequence, different than
                                * srcAcc if multiple levels of mappings have
                                * been done */
    char *sym;                 /* src gene symbol and desc */
    char *desc;
    short gbCurVer;            /* version from genbank table */
};

static char *getTransMapTbl(struct mappingInfo *mi, char *suffix)
/* get a transMap table name; Warning: static return */
{
static char tbl[64];
safef(tbl, sizeof(tbl), "%s%s%s", mi->tblPre, mi->geneSet, suffix);
return tbl;
}

static void parseSrcId(char *srcId, struct mappingInfo *mi)
/* parse srcId parts and save in mi */
{

/* direct mappings (cDNA is aligned to source organism) have ids in the form:
 * NM_012345.1-1.1 Indirect mappings, where mapped predictions are used to
 * form a gene set that is mapped again, have ids in the form
 * db:NM_012345.1_1.1, where db is the source organism, and '_1' is added so
 * that db:NM_012345.1_1 uniquely identifies the source. */

char idBuf[ID_BUFSZ];
safef(idBuf, sizeof(idBuf), "%s", srcId);
char *id = idBuf;

/* find various parts */
char *colon = strchr(id, ':');
char *dot = strchr(id, '.');
char *under = NULL, *dash = NULL;
if (dot != NULL)
    {
    under = strchr(dot, '_');
    dash = strchr(dot, '-');
    }
if ((dot == NULL) || (dash == NULL))
    errAbort("can't parse accession from srcId: %s", srcId);
mi->indirect = (colon != NULL);

/* id used to get sequence is before `-'.  For direct, it excludes
 * genbank version */
*dash = '\0';
if (!mi->indirect)
    *dot = '\0';
safef(mi->seqId, sizeof(mi->seqId), "%s", id);

/* now pull out genbank accession for obtaining description */
id = (colon != NULL) ? colon+1 : id;
*dot = '\0';

safef(mi->gbAcc, sizeof(mi->gbAcc), "%s", id);
if (under != NULL)
    *under = '\0';
mi->gbVer = sqlSigned(dot+1);
}

static void getGenbankInfo(struct sqlConnection *conn, struct mappingInfo *mi)
/* get source gene info and version from gbCdnaInfo and save in mi */
{
char query[512], **row;
struct sqlResult *sr;
char *defDb = hDefaultDb();

/* if id has been modified for multi-level ancestor mappings:
 *    NM_012345.1-1.1 -> db:NM_012345a.1.1
 * then hack it back to the original accession.  However, don't get version,
 * since the sequence is different.
 */

safef(query, sizeof(query),
      "select gbCdnaInfo.version, geneName.name, description.name "
      "from %s.gbCdnaInfo, %s.geneName, %s.description "
      "where gbCdnaInfo.acc=\"%s\" and gbCdnaInfo.geneName=geneName.id and gbCdnaInfo.description = description.id",
      defDb, defDb, defDb, mi->gbAcc);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    mi->gbCurVer = sqlSigned(row[0]);
    mi->sym = cloneString(row[1]);
    mi->desc = cloneString(row[2]);
    }
sqlFreeResult(&sr);
}

static struct mappingInfo *mappingInfoNew(struct sqlConnection *conn,
                                          char *tbl, char *mappedId)
/* load mapping info for a mapped gene */
{
struct mappingInfo *mi;
AllocVar(mi);

safef(mi->mappedId, sizeof(mi->mappedId), "%s", mappedId);
if (startsWith("transMapAnc", tbl))
    strcpy(mi->tblPre, "transMapAnc");
else if (startsWith("transMapSrc", tbl))
    {
    strcpy(mi->tblPre, "transMapSrc");
    mi->isSrc = TRUE;
    }
else
    strcpy(mi->tblPre, "transMap");
int preLen = strlen(mi->tblPre);

if (startsWith("Ref", tbl+preLen))
    strcpy(mi->geneSet, "Ref");
else if (startsWith("MRna", tbl+preLen))
    strcpy(mi->geneSet, "MRna");
else
    errAbort("can't determine source gene set from table: %s", tbl);

if (!mi->isSrc)
    {
    mi->tmi = sqlQueryObjs(conn, (sqlLoadFunc)transMapInfoLoad, sqlQueryMust|sqlQuerySingle,
                           "select * from %s where mappedId='%s'",
                           getTransMapTbl(mi, "Info"), mappedId);
    parseSrcId(mi->tmi->srcId, mi);
    }
else
    parseSrcId(mappedId, mi);

getGenbankInfo(conn, mi);
return mi;
}

static void mappingInfoFree(struct mappingInfo **mip)
/* free mappingInfo object */
{
struct mappingInfo *mi = *mip;
if (mi != NULL)
    {
    transMapInfoFree(&mi->tmi);
    freeMem(mi->sym);
    freeMem(mi->desc);
    }
}

static void displaySrcGene(struct sqlConnection *conn, struct mappingInfo *mi)
/* display information about the source gene that was mapped */
{
/* description will be NULL if deleted */
getGenbankInfo(conn, mi);

/* construct URL to browser */
char srcGeneUrl[1024];
safef(srcGeneUrl, sizeof(srcGeneUrl),
      "../cgi-bin/hgTracks?db=%s&position=%s:%d-%d",
      mi->tmi->srcDb, mi->tmi->srcChrom, mi->tmi->srcStart, mi->tmi->srcEnd);

printf("<TABLE class=\"transMap\">\n");
printf("<CAPTION>Source gene</CAPTION>\n");
printf("<TBODY>\n");
printf("<TR CLASS=\"transMapLeft\"><TD>%s", mi->tmi->srcDb);
printf("<TD CLASS=\"transMapNoWrap\"><A HREF=\"%s\" target=_blank>%s</A>", srcGeneUrl, mi->tmi->srcId);
if (mi->desc == NULL)
    printf("<TD>&nbsp;<TD>gene no longer in source database");
else
    printf("<TD>%s<TD>%s", mi->sym, mi->desc);
printf("</TR>\n");
printf("</TBODY></TABLE>\n");
}


static void prGeneMappingRow(char *label, unsigned origCnt, unsigned mapCnt)
/* display one row in the gene mapping table */
{
float frac = (origCnt == 0) ? 0.0 : ((float)mapCnt)/origCnt;
printf("<TR><TH>%s<TD>%d<TD>%d<TD>%0.2f</TR>\n", label, origCnt, mapCnt, frac);
}

static void displayMappingInfo(struct sqlConnection *conn, struct mappingInfo *mi)
/* display information from a transMap table */
{
struct transMapInfo *tmi = mi->tmi;
printf("<TABLE class=\"transMap\">\n");
printf("<CAPTION>Mapping stats</CAPTION>\n");
printf("<THEAD>\n");
printf("<TR><TH>          <TH>%s    <TH COLSPAN=2>mapped</TR>\n", tmi->srcDb);
printf("<TR><TH>          <TH>count <TH>count<TH>frac</TR>\n");
printf("</THEAD><TBODY>\n");
prGeneMappingRow("exons", tmi->srcExonCnt, tmi->mappedExonCnt);
prGeneMappingRow("bases", tmi->srcBaseCnt, tmi->mappedBaseCnt);
prGeneMappingRow("CDS exons", tmi->srcCdsExonCnt, tmi->mappedCdsExonCnt);
prGeneMappingRow("CDS bases", tmi->srcCdsBaseCnt, tmi->mappedCdsBaseCnt);
printf("</TBODY></TABLE>\n");
}

static void displayAligns(struct sqlConnection *conn, struct mappingInfo *mi)
/* display cDNA alignments */
{
int start = cartInt(cart, "o");
char *alignTbl = getTransMapTbl(mi, "Ali");

/* this should only ever have one alignment */
struct psl *psl = getAlignments(conn, alignTbl, mi->mappedId);
printf("<H3>mRNA/Genomic Alignments</H3>");

/* display identity as 0.0%, since it's not known */
psl->misMatch = psl->match;
psl->match = 0;
printAlignments(psl, start, "hgcTransMapCdnaAli", alignTbl, mi->mappedId);
pslFreeList(&psl);
}

static struct geneCheck *displayGeneCheck(struct sqlConnection *conn, struct mappingInfo *mi)
/* display gene-check results; return true geneCheck object, which must be freed */
{
struct geneCheck *gc
    = sqlQueryObjs(conn, (sqlLoadFunc)geneCheckLoad, sqlQueryMust|sqlQuerySingle,
                   "select * from %s where acc='%s'", 
                   getTransMapTbl(mi, "Chk"), mi->mappedId);
geneCheckWidgetSummary(gc, "transMap", "Gene checks\n");
return gc;
}

static void displayGeneCheckDetails(struct sqlConnection *conn, struct mappingInfo *mi,
                                    struct geneCheck *gc)
/* display gene-check details */
{
struct geneCheckDetails *gcdList
    = sqlQueryObjs(conn, (sqlLoadFunc)geneCheckDetailsLoad, sqlQueryMulti,
                   "select * from %s where acc='%s'",
                   getTransMapTbl(mi, "ChkDetails"), gc->acc);
geneCheckWidgetDetails(cart, gc, gcdList, "transMap", "Gene check details", NULL);
geneCheckDetailsFreeList(&gcdList);
}

static void displayGeneCheckResults(struct sqlConnection *conn, struct mappingInfo *mi)
/* display row with geneCheck info, if tables exist */
{
char *tbl = getTransMapTbl(mi, "Chk");
if (sqlTableExists(conn, tbl))
    {
    printf("<TR><TD>\n");
    struct geneCheck *gc = displayGeneCheck(conn, mi);
    printf("<TD COLSPAN=2>\n");
    if (!sameString(gc->stat, "ok"))
        displayGeneCheckDetails(conn, mi, gc);
    printf("</TR>\n");
    geneCheckFree(&gc);
    }
}

void transMapClickHandler(struct trackDb *tdb, char *mappedId)
/* Handle click on a transMap tracks */
{
struct sqlConnection *conn = hAllocConn();
struct mappingInfo *mi = mappingInfoNew(conn, tdb->tableName, mappedId);

genericHeader(tdb, mappedId);
printf("<TABLE class=\"transMapLayout\">\n");

if (!mi->isSrc)
    {
    printf("<TR><TD COLSPAN=3>\n");
    displaySrcGene(conn, mi);
    printf("</TR>\n");
    }

if (sqlTableExists(conn, getTransMapTbl(mi, "Ali")))
    {
    printf("<TR><TD COLSPAN=3>\n");
    displayAligns(conn, mi);
    printf("</TR>\n");
    }

if (!mi->isSrc)
    {
    printf("<TR><TD COLSPAN=3>\n");
    displayMappingInfo(conn, mi);
    printf("</TR>\n");
    }

displayGeneCheckResults(conn, mi);
printf("</TABLE>\n");

printTrackHtml(tdb);
mappingInfoFree(&mi);
hFreeConn(&conn);
}

static struct genbankCds getCds(struct sqlConnection *conn, struct mappingInfo *mi)
/* Get CDS, return empty genebankCds if not found or can't parse  */
{
char query[256];
safef(query, sizeof(query),
      "select cds.name "
      "from %s.gbCdnaInfo, %s.cds "
      "where gbCdnaInfo.acc=\"%s\" and gbCdnaInfo.cds=cds.id",
      mi->tmi->srcDb, mi->tmi->srcDb, mi->gbAcc);

struct sqlResult *sr = sqlMustGetResult(conn, query);
char **row = sqlNextRow(sr);
struct genbankCds cds;
if ((row == NULL) || !genbankCdsParse(row[0], &cds))
    ZeroVar(&cds);  /* can't get or parse cds */
sqlFreeResult(&sr);
return cds;
}

static struct psl *loadAlign(struct sqlConnection *conn, struct mappingInfo *mi, int start)
/* load a psl that must exist */
{
char table[256], query[256];
boolean hasBin;
hFindSplitTable(seqName, getTransMapTbl(mi, "Ali"), table, &hasBin);

safef(query, sizeof(query), "select * from %s where qName = '%s' and tStart = %d",
      table, mi->mappedId, start);
struct sqlResult *sr= sqlMustGetResult(conn, query);
char **row= sqlNextRow(sr);
struct psl *psl = pslLoad(row+hasBin);
sqlFreeResult(&sr);
return psl;
}

void transMapShowCdnaAli(char *mappedId)
/* Show alignment for accession, mostly ripped off from htcCdnaAli */
{
char *track = cartString(cart, "aliTrack");
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn();
struct mappingInfo *mi = mappingInfoNew(conn, track, mappedId);
struct genbankCds cds = getCds(conn, mi);

/* Print start of HTML. */
writeFramesetType();
puts("<HTML>");
printf("<HEAD>\n<TITLE>%s vs Genomic</TITLE>\n</HEAD>\n\n", mi->seqId);

/* Look up alignment and sequence in database.  Always get sequence
 * from defaultDb */
struct psl *psl = loadAlign(conn, mi, start);
struct sqlConnection *defDbConn = sqlConnect(hDefaultDb());
struct dnaSeq *rnaSeq;
if (hRnaSeqAndIdx(mi->seqId, &rnaSeq, NULL, NULL, defDbConn) < 0)
    errAbort("can't get mRNA sequence from %s for %s", hDefaultDb(),
             mi->seqId);
sqlDisconnect(&defDbConn);

showSomeAlignment(psl, rnaSeq, gftDna, 0, rnaSeq->size, NULL, cds.start, cds.end);
pslFree(&psl);
dnaSeqFree(&rnaSeq);
mappingInfoFree(&mi);
hFreeConn(&conn);
}

