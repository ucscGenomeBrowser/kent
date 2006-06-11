/* transMapClick - transMap click handling */

/*
 * Tables associated with transMap:
 *   transMapXxxGene - gene predictions
 *   transMapXxxAli - mapped PSLs aligments
 *   transMapXxxAliGene - mapped alignmetns with CDS and frame annotation
 *   transMapXxxChk - gene-check results
 *   transMapXxxInfo - mapping information, including source.
 *   transMapXxxAttr - optional coloring attr table
 *
 * Xxx is:
 *    - Ref - RefSeq genes
 *    - MRna - GenBank mRNAs
 */

#include "common.h"
#include "hgc.h"
#include "transMapClick.h"
#include "transMapInfo.h"
#include "genbank.h"

struct mappingInfo
/* various pieces of information about mapping from table name and 
 * transMapXxxInfo table */
{
    char tblPre[64];           /* table prefix */
    char geneSet[6];           /* source gene set abbrv used in table name */
    struct transMapInfo *tmi;  /* general info for mapped gene */
    char srcAcc[18];           /* src accession */
    short srcVer;              /* version from srcId */
    char *sym;                 /* src gene symbol and desc */
    char *desc;
    short gbVer;               /* version from genbank table */
};

static void parseSrcId(struct mappingInfo *mi)
/* parse srcId parts and save in mi */
{
char *dash=NULL, *dot;
dot = strchr(mi->tmi->srcId, '.');
if (dot != NULL)
    dash = strchr(dot, '-');
if ((dot == NULL) || (dash == NULL))
    errAbort("can't parse accession from srcId: %s", mi->tmi->srcId);
safef(mi->srcAcc, sizeof(mi->srcAcc), "%.*s", (int)(dot-mi->tmi->srcId), mi->tmi->srcId);
*dash = '\0';
mi->srcVer = sqlSigned(dot+1);
*dash = '-';
}

static void getGenbankInfo(struct sqlConnection *conn, struct mappingInfo *mi)
/* get source gene info and version from gbCdnaInfo and save in mi */
{
char query[512], **row;
struct sqlResult *sr;

safef(query, sizeof(query),
      "select gbCdnaInfo.version, geneName.name, description.name "
      "from %s.gbCdnaInfo, %s.geneName, %s.description "
      "where gbCdnaInfo.acc=\"%s\" and gbCdnaInfo.geneName=geneName.id and gbCdnaInfo.description = description.id",
      mi->tmi->srcDb, mi->tmi->srcDb, mi->tmi->srcDb, mi->srcAcc);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    mi->gbVer = sqlSigned(row[0]);
    mi->sym = cloneString(row[1]);
    mi->desc = cloneString(row[2]);
    }
sqlFreeResult(&sr);
}

static struct mappingInfo mappingInfoLoad(struct sqlConnection *conn,
                                          char *tbl, char *mappedId)
/* load mapping info for a mapped gene */
{
struct mappingInfo mi;
int preLen;
ZeroVar(&mi);

if (startsWith("transMapAnc", tbl))
    strcpy(mi.tblPre, "transMapAnc");
else
    strcpy(mi.tblPre, "transMap");
preLen = strlen(mi.tblPre);

if (startsWith("Ref", tbl+preLen))
    strcpy(mi.geneSet, "Ref");
else if (startsWith("MRna", tbl+preLen))
    strcpy(mi.geneSet, "MRna");
else
    errAbort("can't determine source gene set from table: %s", tbl);

mi.tmi = sqlQueryObjs(conn, (sqlLoadFunc)transMapInfoLoad, sqlQueryMust|sqlQuerySingle,
                      "select * from %s%sInfo where mappedId='%s'",
                      mi.tblPre, mi.geneSet, mappedId);
parseSrcId(&mi);
getGenbankInfo(conn, &mi);
return mi;
}

static void mappingInfoCleanup(struct mappingInfo *mi)
/* cleanup resources in mappingInfo object (which is allocated on stack) */
{
transMapInfoFree(&mi->tmi);
freeMem(mi->sym);
freeMem(mi->desc);
}

static void displaySrcGene(struct sqlConnection *conn, struct mappingInfo *mi)
/* display information about the source gene that was mapped */
{
char srcGeneUrl[1024];

/* description will be NULL if deleted */
getGenbankInfo(conn, mi);

/* construct URL to browser */
safef(srcGeneUrl, sizeof(srcGeneUrl),
      "../cgi-bin/hgTracks?db=%s&position=%s:%d-%d",
      mi->tmi->srcDb, mi->tmi->srcChrom, mi->tmi->srcStart, mi->tmi->srcEnd);

printf("<TABLE class=\"transMap\">\n");
printf("<CAPTION>Source gene</CAPTION>\n");
printf("<TBODY>\n");
printf("<TR CLASS=\"transMapLeft\"><TD>%s", mi->tmi->srcDb);
printf("<TD CLASS=\"transMapNoWrap\"><A HREF=\"%s\" target=_blank>%s</A>", srcGeneUrl, mi->srcAcc);
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
char alignTbl[128];
struct psl *psl;
safef(alignTbl, sizeof(alignTbl), "%s%sAli", mi->tblPre, mi->geneSet);

/* this should only ever have one alignment */
psl = getAlignments(conn, alignTbl, mi->tmi->mappedId);
printf("<H3>mRNA/Genomic Alignments</H3>");
/* display identity as 0.0%, since it's not known */
psl->misMatch = psl->match;
psl->match = 0;
printAlignments(psl, start, "hgcTransMapCdnaAli", alignTbl, mi->tmi->mappedId);
pslFreeList(&psl);
}

void transMapClickHandler(struct trackDb *tdb, char *mappedId)
/* Handle click on a transMap tracks */
{
struct sqlConnection *conn = hAllocConn();
struct mappingInfo mi = mappingInfoLoad(conn, tdb->tableName, mappedId);

genericHeader(tdb, mappedId);
printf("<TABLE border=0>\n");
printf("<TR CLASS=\"transMapLayout\">\n");
printf("<TD COLSPAN=3>\n");
displaySrcGene(conn, &mi);
printf("</TR>\n");
printf("<TR CLASS=\"transMapLayout\">\n");
printf("<TD>\n");
displayMappingInfo(conn, &mi);
printf("<TD>\n");
#if 0
struct geneCheck *gc = displayGeneCheck(conn, &mti, mappedId);
printf("<TD>\n");
displayProtSim(conn, &mti, mappedId);
#endif
printf("</TR>\n");
#if 0
if (!sameString(gc->stat, "ok"))
    {
    printf("<TR CLASS=\"transMapLayout\">\n");
    printf("<TD COLSPAN=3>\n");
    displayGeneCheckDetails(conn, &mti, gc);
    printf("</TR>\n");
    }
#endif
printf("</TABLE>\n");
displayAligns(conn, &mi);
printTrackHtml(tdb);
#if 0
geneCheckFree(&gc);
#endif
mappingInfoCleanup(&mi);
hFreeConn(&conn);
}

static struct genbankCds getCds(struct sqlConnection *conn, struct mappingInfo *mi)
/* Get CDS, return empty genebankCds if not found or can't parse  */
{
char query[256];
struct sqlResult *sr;
struct genbankCds cds;
char **row;

safef(query, sizeof(query),
      "select cds.name "
      "from %s.gbCdnaInfo, %s.cds "
      "where gbCdnaInfo.acc=\"%s\" and gbCdnaInfo.cds=cds.id",
      mi->tmi->srcDb, mi->tmi->srcDb, mi->srcAcc);

sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if ((row == NULL) || !genbankCdsParse(row[0], &cds))
    ZeroVar(&cds);  /* can't get or parse cds */
sqlFreeResult(&sr);
return cds;
}

static struct psl *loadAlign(struct sqlConnection *conn, struct mappingInfo *mi, int start)
/* load a psl that must exist */
{
char rootTable[256], table[256], query[256];
boolean hasBin;
struct sqlResult *sr;
char **row;
struct psl *psl;

safef(rootTable, sizeof(rootTable), "%s%sAli", mi->tblPre, mi->geneSet);
hFindSplitTable(seqName, rootTable, table, &hasBin);

safef(query, sizeof(query), "select * from %s where qName = '%s' and tStart = %d",
      table, mi->tmi->mappedId, start);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
psl = pslLoad(row+hasBin);
sqlFreeResult(&sr);
return psl;
}

void transMapShowCdnaAli(char *mappedId)
/* Show alignment for accession, mostly ripped off from htcCdnaAli */
{
char *track = cartString(cart, "aliTrack");
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn();
struct sqlConnection *defDbConn = NULL;
struct mappingInfo mi = mappingInfoLoad(conn, track, mappedId);
struct genbankCds cds = getCds(conn, &mi);
struct psl *psl;
struct dnaSeq *rnaSeq;

/* Print start of HTML. */
writeFramesetType();
puts("<HTML>");
printf("<HEAD>\n<TITLE>%s vs Genomic</TITLE>\n</HEAD>\n\n", mi.srcAcc);

/* Look up alignment and sequence in database.  Always get sequence
 * from defaultDb */
psl = loadAlign(conn, &mi, start);
defDbConn = sqlConnect(hDefaultDb());
if (hRnaSeqAndIdx(mi.srcAcc, &rnaSeq, NULL, NULL, defDbConn) < 0)
    errAbort("can't get mRNA sequence from %s for %s", hDefaultDb(),
             mi.srcAcc);
sqlDisconnect(&defDbConn);

showSomeAlignment(psl, rnaSeq, gftDna, 0, rnaSeq->size, NULL, cds.start, cds.end);
pslFree(&psl);
dnaSeqFree(&rnaSeq);
mappingInfoCleanup(&mi);
hFreeConn(&conn);
}

