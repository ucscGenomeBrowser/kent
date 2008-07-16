/* transMapClick - transMap click handling */

/*
 * Per-genome tables associated with transMap:
 *   transMapAliXxx - mapped PSLs alignments
 *   transMapInfoXxx - information for mapped alignments
 * hgFixed tables associated with transMap:
 *   transMapSrcXxx - information about source alignments
 *   transMapGeneXxx - gene information
 *   transMapSeqXxx - seq table for accessing sequence
 *   transMapExtFileXxx - extFile table
 *
 * Xxx is:
 *    - UcscGene - UCSC genes
 *    - RefSeq - RefSeq mRNAs
 *    - MRna - GenBank mRNAs
 *    - SplicedEst - GenBank spliced ESTs
 */

#include "common.h"
#include "hgc.h"
#include "hui.h"
#include "hCommon.h"
#include "transMapClick.h"
#include "transMapStuff.h"
#include "transMapInfo.h"
#include "transMapSrc.h"
#include "transMapGene.h"
#include "genbank.h"


struct transMapBag
/* object contain collected information on a specific transMap mapping */
{
    struct psl *psl;              // transMap alignment
    struct transMapInfo *info;    // addition information about mapping
    struct transMapSrc *src;      // source information
    struct transMapGene *gene;    // gene information
    boolean srcDbIsActive;        // source database is active
};

static struct transMapBag *transMapBagLoad(struct sqlConnection *conn,
                                           struct trackDb *tdb, char *mappedId,
                                           boolean getSrcRec)
/* load information from various tables */
{
struct transMapBag *bag;
AllocVar(bag);
bag->psl = getAlignments(conn, tdb->tableName, mappedId);

char *transMapInfoTbl = trackDbRequiredSetting(tdb, transMapInfoTblSetting);
bag->info = transMapInfoQuery(conn, transMapInfoTbl, mappedId);

if (getSrcRec)
    {
    char *transMapSrcTbl = trackDbRequiredSetting(tdb, transMapSrcTblSetting);
    bag->src = transMapSrcQuery(conn, transMapSrcTbl, bag->info->srcDb, bag->info->srcId);
    }

char *transMapGeneTbl = trackDbSetting(tdb, transMapGeneTblSetting);
if (transMapGeneTbl != NULL)
    bag->gene = transMapGeneQuery(conn, transMapGeneTbl,
                                  bag->info->srcDb, transMapIdToAcc(bag->info->srcId));
bag->srcDbIsActive = hDbIsActive(bag->info->srcDb);
return bag;
}

static void transMapBagFree(struct transMapBag **bagPtr)
/* free the bag */
{
struct transMapBag *bag = *bagPtr;
if (bag != NULL)
    {
    pslFree(&bag->psl);
    transMapInfoFree(&bag->info);
    transMapSrcFree(&bag->src);
    transMapGeneFree(&bag->gene);
    freez(bagPtr);
    }
}

static void prOrgScientific(char *db)
/* print organism and scientific name for a database. */
{
char *org = hOrganism(db);
char *sciName = hScientificName(db);
if ((org != NULL) && (sciName != NULL))
    printf("%s (%s)", org, sciName);
else
    printf("%s", db);
freeMem(org);
freeMem(sciName);
}

static char *chainSubsetDesc(enum transMapInfoChainSubset cs)
/* get description for chain subset */
{
switch (cs)
    {
    case transMapInfoUnknown:
        return "unknown";
    case transMapInfoAll:
        return "all";
    case transMapInfoSyn:
        return "syntenic";
    case transMapInfoRbest:
        return "reciprocal best";
    }
return NULL;
}

static void displayMapped(struct transMapBag *bag)
/* display information about the mapping alignment */
{
printf("<TABLE class=\"transMap\">\n");
printf("<CAPTION>TransMap Alignment</CAPTION>\n");
printf("<TBODY>\n");

// organism/assembly
printf("<TR CLASS=\"transMapLeft\"><TD>Organism<TD>");
prOrgScientific(database);
printf("</TR>\n");
printf("<TR CLASS=\"transMapLeft\"><TD>Genome<TD>%s</TR>\n", database);

// position
printf("<TR CLASS=\"transMapLeft\">");
printf("<TD>Position<TD CLASS=\"transMapNoWrap\">");
printf("<A HREF=\"%s&position=%s:%d-%d\">",
      hgTracksPathAndSettings(),
      bag->psl->tName, bag->psl->tStart, bag->psl->tEnd);
printf("%s:%d-%d</A>", bag->psl->tName, bag->psl->tStart, bag->psl->tEnd);
printf("</TR>\n");

// % identity and % aligned
printf("<TR CLASS=\"transMapLeft\"><TD>Identity<TD>%0.1f%%</TR>\n",
       100.0*pslIdent(bag->psl));
printf("<TR CLASS=\"transMapLeft\"><TD>Aligned<TD>%0.1f%%</TR>\n",
       100.0*pslQueryAligned(bag->psl));

// chain used in mapping
printf("<TR CLASS=\"transMapLeft\"><TD>Chain ID<TD>%s</TR>\n",
       bag->info->mappingId);
printf("<TR CLASS=\"transMapLeft\"><TD>Chain subset<TD>%s</TR>\n",
       chainSubsetDesc(bag->info->chainSubset));
printf("</TBODY></TABLE>\n");
}

static void displaySource(struct transMapBag *bag)
/* display information about the source gene that was mapped */
{
printf("<TABLE class=\"transMap\">\n");
printf("<CAPTION>Source Alignment</CAPTION>\n");
printf("<TBODY>\n");
// organism/assembly
printf("<TR CLASS=\"transMapLeft\"><TD>Organism<TD>");
if (bag->srcDbIsActive)
    prOrgScientific(bag->info->srcDb);
else
    printf("%s", bag->info->srcDb);
printf("</TR>\n");
printf("<TR CLASS=\"transMapLeft\"><TD>Genome<TD>%s</TR>\n", bag->info->srcDb);

// position
printf("<TR CLASS=\"transMapLeft\"><TD>Position\n");
if (bag->srcDbIsActive)
    printf("<TD CLASS=\"transMapNoWrap\"><A HREF=\"%s?db=%s&position=%s:%d-%d\" target=_blank>"
           "%s:%d-%d</A>",
           hgTracksName(), bag->src->db,
           bag->src->chrom, bag->src->chromStart, bag->src->chromEnd,
           bag->src->chrom, bag->src->chromStart, bag->src->chromEnd);
else
    printf("%s:%d-%d", bag->src->chrom, bag->src->chromStart, bag->src->chromEnd);
printf("</TR>\n");

// % identity and % aligned
printf("<TR CLASS=\"transMapLeft\"><TD>Identity<TD>%0.1f%%</TR>\n",
       100.0*bag->src->ident);
printf("<TR CLASS=\"transMapLeft\"><TD>Aligned<TD>%0.1f%%</TR>\n",
       100.0*bag->src->aligned);

// gene and CDS
printf("<TR CLASS=\"transMapLeft\"><TD>Gene<TD>%s</TR>\n",
       (((bag->gene != NULL) && (strlen(bag->gene->geneName) > 0))
        ? bag->gene->geneName : "&nbsp;"));
printf("<TR CLASS=\"transMapLeft\"><TD>CDS<TD>%s</TR>\n",
       (((bag->gene != NULL) && (strlen(bag->gene->cds) > 0))
        ? bag->gene->cds : "&nbsp;"));
printf("</TBODY></TABLE>\n");
}

static void displayAligns(struct trackDb *tdb, struct transMapBag *bag)
/* display cDNA alignments */
{
int start = cartInt(cart, "o");
printf("<H3>mRNA/Genomic Alignments</H3>");
printAlignmentsSimple(bag->psl, start, "hgcTransMapCdnaAli", tdb->tableName, bag->info->mappedId);
}

void transMapClickHandler(struct trackDb *tdb, char *mappedId)
/* Handle click on a transMap tracks */
{
struct sqlConnection *conn = hAllocConn();
struct transMapBag *bag = transMapBagLoad(conn, tdb, mappedId, TRUE);

genericHeader(tdb, mappedId);
printf("<TABLE class=\"transMapLayout\">\n");

printf("<TR><TD>\n");
displayMapped(bag);
printf("<TD>\n");
displaySource(bag);
printf("<TD>&nbsp;\n");
printf("</TR>\n");
printf("<TR><TD COLSPAN=3>\n");
displayAligns(tdb, bag);
printf("</TR>\n");
printf("</TABLE>\n");

printTrackHtml(tdb);
transMapBagFree(&bag);
hFreeConn(&conn);
}

static struct dnaSeq *getCdnaSeq(struct trackDb *tdb, char *name)
/* look up sequence name in seq and extFile tables specified
 * for base coloring. */
{
/* FIXME: this is really a rip off of some of the code in
 * hgTracks/cds.c; really need to centralize it somewhere */
char *spec = trackDbRequiredSetting(tdb, BASE_COLOR_USE_SEQUENCE);
char *specCopy = cloneString(spec);

// value is: extFile seqTbl extFileTbl
char *words[3];
int nwords = chopByWhite(specCopy, words, ArraySize(words));
if ((nwords != ArraySize(words)) || !sameString(words[0], "extFile"))
    errAbort("invalid %s track setting: %s", BASE_COLOR_USE_SEQUENCE, spec);
struct dnaSeq *seq = hDnaSeqGet(NULL, name, words[1], words[2]);
freeMem(specCopy);
return seq;
}

void transMapShowCdnaAli(struct trackDb *tdb, char *mappedId)
/* Show alignment for mappedId, mostly ripped off from htcCdnaAli */
{
struct sqlConnection *conn = hAllocConn();
struct transMapBag *bag = transMapBagLoad(conn, tdb, mappedId, FALSE);
struct genbankCds cds;
if ((bag->gene == NULL) || (strlen(bag->gene->cds) == 0)
    || !genbankCdsParse(bag->gene->cds, &cds))
    ZeroVar(&cds);  /* can't get or parse cds */
struct dnaSeq *seq = getCdnaSeq(tdb, transMapIdToAcc(mappedId));

writeFramesetType();
puts("<HTML>");
printf("<HEAD>\n<TITLE>%s vs Genomic</TITLE>\n</HEAD>\n\n", mappedId);
showSomeAlignment(bag->psl, seq, gftDna, 0, seq->size, NULL, cds.start, cds.end);
dnaSeqFree(&seq);
transMapBagFree(&bag);
hFreeConn(&conn);
}
