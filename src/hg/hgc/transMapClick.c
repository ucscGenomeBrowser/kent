/* transMapClick - transMap click handling */

/* Copyright (C) 2010 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* FIXME: the two version support should be be delete once support
 * for table version is fully dropped.
 */

/*
 * For the older table-based version of transMap, the following
 * tables are used.  These are mapped into the bigTransMap objects.
 *
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
#include "bigTransMap.h"
#include "bigBed.h"
#include "bigPsl.h"
#include "transMapStuff.h"
#include "transMapInfo.h"
#include "transMapSrc.h"
#include "transMapGene.h"
#include "genbank.h"

enum geneSrcSetType
/* constants for source sets */
{
    ENSEMBL_SRC_SET,
    REFSEQ_SRC_SET,
    RNA_SRC_SET,
    EST_SRC_SET,
    UCSC_GENES_SRC_SET,
    UNKNOWN_SRC_SET
};

struct transMapBag
/* object contain collected information on a specific transMap mapping
* this has both PSL and bigTransMap objects */
{
    struct psl *psl;              // transMap alignment
    struct bigTransMap *meta;     // bigTransMap record for metadata
    enum geneSrcSetType srcSet;   // source data set (not owned)
    boolean srcDbIsActive;        // source database is active
};

static char *guessTranscriptType(struct transMapGene *gene)
/* guess the transcript type when not known */
{
if (gene == NULL)
    return "unknown";  // EST
else if (sameString(gene->cds, "n/a") || sameString(gene->cds, ""))
    return "non_coding";
else    
    return "protein_coding";
}

static enum geneSrcSetType guessGeneSrcSet(struct trackDb *tdb)
/* guess the the source set from the table name for table
 * based-transmap */
{
if (stringIn("UcscGenes", tdb->table))
    return UCSC_GENES_SRC_SET;
else if (stringIn("RefSeq", tdb->table))
    return REFSEQ_SRC_SET;
else if (stringIn("MRna", tdb->table))
    return RNA_SRC_SET;
else if (stringIn("SplicedEst", tdb->table))
    return EST_SRC_SET;
else
    return UNKNOWN_SRC_SET;
}

static char *chainSubsetToBigStr(enum transMapInfoChainSubset cs)
/* convert chain subset type to string used in bigTransMap */
{
switch (cs)
    {
    case transMapInfoUnknown:
        assert(FALSE);
        return "unknown";
    case transMapInfoAll:
        return "all";
    case transMapInfoSyn:
        return "syn";
    case transMapInfoRbest:
        return "rbest";
    }
assert(FALSE);
return NULL;
}

static struct bigTransMap *buildFakeBigTransMapRec(struct transMapInfo *info,
                                                   struct transMapSrc *src,
                                                   struct transMapGene *gene)
/* build a partial bigTransMap record from the tables; only metadata fields
 * are filled in. */
{
struct bigTransMap *bigTransMap;
AllocVar(bigTransMap);
bigTransMap->seqType = 1;
bigTransMap->name = cloneString(src->id);
bigTransMap->srcDb = cloneString(src->db);
bigTransMap->srcChrom = cloneString(src->chrom);
bigTransMap->srcChromStart = src->chromStart;
bigTransMap->srcChromEnd = src->chromEnd;
bigTransMap->srcIdent = (int)(1000.0*src->ident);
bigTransMap->srcAligned = (int)(1000.0*src->aligned);
if (gene != NULL)
    {
    bigTransMap->geneName = cloneString(gene->geneName);
    bigTransMap->geneId = cloneString(gene->geneId);
    bigTransMap->oCDS = cloneString(gene->cds);
    }
bigTransMap->geneType = cloneString(guessTranscriptType(gene)); // set for ESTs
bigTransMap->transcriptType = cloneString(guessTranscriptType(gene));
bigTransMap->chainType = cloneString(chainSubsetToBigStr(info->chainSubset));
bigTransMap->commonName = hOrganism(info->srcDb);
if (bigTransMap->commonName == NULL)
    bigTransMap->commonName = cloneString("");
bigTransMap->scientificName = hScientificName(info->srcDb);
if (bigTransMap->scientificName == NULL)
    bigTransMap->scientificName = cloneString("");
bigTransMap->orgAbbrev = cloneString(emptyForNull(hOrgShortForDb(info->srcDb)));
return bigTransMap;
}

static struct transMapBag *transMapBagLoadDb(struct trackDb *tdb, char *mappedId)
/* load information from various tables for database version of transmap */
{
struct sqlConnection *conn = hAllocConn(database);
struct transMapBag *bag;
AllocVar(bag);
bag->psl = getAlignments(conn, tdb->table, mappedId);

char *transMapInfoTbl = trackDbRequiredSetting(tdb, transMapInfoTblSetting);
struct transMapInfo *info = transMapInfoQuery(conn, transMapInfoTbl, mappedId);

char *transMapSrcTbl = trackDbRequiredSetting(tdb, transMapSrcTblSetting);
struct transMapSrc *src = transMapSrcQuery(conn, transMapSrcTbl, info->srcDb, info->srcId);

struct transMapGene *gene = NULL;
char *transMapGeneTbl = trackDbSetting(tdb, transMapGeneTblSetting);
if (transMapGeneTbl != NULL)
    gene = transMapGeneQuery(conn, transMapGeneTbl,
                             info->srcDb, transMapIdToSeqId(info->srcId));
bag->srcSet = guessGeneSrcSet(tdb);
bag->srcDbIsActive = hDbIsActive(info->srcDb);
bag->meta = buildFakeBigTransMapRec(info, src, gene);
transMapInfoFree(&info);
transMapSrcFree(&src);
transMapGeneFree(&gene);
hFreeConn(&conn);
return bag;
}

static enum geneSrcSetType getGeneSrcSet(struct trackDb *tdb)
/* get the geneSrcSetType from trackDb */
{
char *srcSet = trackDbRequiredSetting(tdb, "transMapSrcSet");
if (sameString(srcSet, "ensembl"))
    return ENSEMBL_SRC_SET;
else if (sameString(srcSet, "refseq"))
    return REFSEQ_SRC_SET;
else if (sameString(srcSet, "rna"))
    return RNA_SRC_SET;
else if (sameString(srcSet, "est"))
    return EST_SRC_SET;
else
    return UNKNOWN_SRC_SET;
}

static char *formatGeneSrcSet(enum geneSrcSetType srcSet)
/* get display version of source set */
{
switch (srcSet)
    {
    case ENSEMBL_SRC_SET:
        return "Ensembl";
    case REFSEQ_SRC_SET:
        return "RefSeq RNA";
    case RNA_SRC_SET:
        return "GenBank RNA";
    case EST_SRC_SET:
        return "GenBank EST";
    case UCSC_GENES_SRC_SET:
        return "UCSC Genes";
    case UNKNOWN_SRC_SET:
        return "Unknown";
    }
return "Unknown";
}

static struct transMapBag *transMapBagLoadBig(struct trackDb *tdb, char *mappedId)
/* load information from bigTransMap file */
{
struct sqlConnection *conn = hAllocConn(database);
struct transMapBag *bag;
AllocVar(bag);

char *fileName = bbiNameFromSettingOrTable(tdb, conn, tdb->table);
char *chrom = cartString(cart, "c");
struct bbiFile *bbi = bigBedFileOpen(fileName);
struct lm *lm = lmInit(0);
int fieldIx;
struct bptFile *bpt = bigBedOpenExtraIndex(bbi, "name", &fieldIx);
struct bigBedInterval *bb = bigBedNameQuery(bbi, bpt, fieldIx, mappedId, lm);
if (slCount(bb) != 1)
    errAbort("expected 1 item named \"%s\", got %d from %s", mappedId, slCount(bb), fileName);
char *fields[bbi->fieldCount];
char startBuf[32], endBuf[32];
int bbFieldCount = bigBedIntervalToRow(bb, chrom, startBuf, endBuf, fields,
                                       bbi->fieldCount);
if (bbFieldCount != BIGTRANSMAP_NUM_COLS)
    errAbort("expected %d fields in bigTransMap record, got %d in %s",
             BIGTRANSMAP_NUM_COLS, bbFieldCount, fileName);
bag->psl = pslFromBigPsl(chrom, bb, 0, NULL, NULL); 
bag->meta = bigTransMapLoad(fields);
bag->srcSet = getGeneSrcSet(tdb);
bag->srcDbIsActive = hDbIsActive(bag->meta->srcDb);

bigBedFileClose(&bbi);
lmCleanup(&lm);
hFreeConn(&conn);
return bag;
}

static void transMapBagFree(struct transMapBag **bagPtr)
/* free the bag */
{
struct transMapBag *bag = *bagPtr;
if (bag != NULL)
    {
    pslFree(&bag->psl);
    bigTransMapFree(&bag->meta);
    freez(bagPtr);
    }
}

static void prOrgScientificDb(char *db)
/* print organism and scientific name for a database.
 */
{
char *org = hOrganism(db);
char *sciName = hScientificName(db);
if ((org != NULL) && (sciName != NULL))
    printf("%s (%s)", org, sciName);
else
    printf("n/a");
freeMem(org);
freeMem(sciName);
}

static void displayMapped(struct transMapBag *bag)
/* display information about the mapping alignment */
{
printf("<TABLE class=\"transMap\">\n");
printf("<CAPTION>TransMap Alignment</CAPTION>\n");
printf("<TBODY>\n");

// organism/assembly
printf("<TR CLASS=\"transMapLeft\"><TD>Organism<TD>");
prOrgScientificDb(database);
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

// chain type used in mapping
printf("<TR CLASS=\"transMapLeft\"><TD>Chain subset<TD>%s</TR>\n",
       bag->meta->chainType);
printf("</TBODY></TABLE>\n");
}

static void displaySource(struct transMapBag *bag)
/* display information about the source gene that was mapped */
{
printf("<TABLE class=\"transMap\">\n");
printf("<CAPTION>Source Alignment</CAPTION>\n");

printf("<TBODY>\n");
// organism/assembly
printf("<TR CLASS=\"transMapLeft\"><TD>Organism<TD>%s (%s)</TR>\n",
       bag->meta->commonName, bag->meta->scientificName);
printf("<TR CLASS=\"transMapLeft\"><TD>Genome<TD>%s</TR>\n", bag->meta->srcDb);
printf("<TR CLASS=\"transMapLeft\"><TD>Source<TD>%s</TR>\n", formatGeneSrcSet(bag->srcSet));

// position
printf("<TR CLASS=\"transMapLeft\"><TD>Position\n");
printf("<TD CLASS=\"transMapNoWrap\">");
if (bag->srcDbIsActive)
    printf("<A HREF=\"%s?db=%s&position=%s:%d-%d\" target=_blank>"
           "%s:%d-%d</A>",
           hgTracksName(), bag->meta->srcDb,
           bag->meta->srcChrom, bag->meta->srcChromStart, bag->meta->srcChromEnd,
           bag->meta->srcChrom, bag->meta->srcChromStart, bag->meta->srcChromEnd);
else
    printf("%s:%d-%d", bag->meta->srcChrom, bag->meta->srcChromStart, bag->meta->srcChromEnd);
printf("</TR>\n");

// % identity and % aligned, values stored as 1000*fraction ident
printf("<TR CLASS=\"transMapLeft\"><TD>Identity<TD>%0.1f%%</TR>\n",
       bag->meta->srcIdent/10.0);
printf("<TR CLASS=\"transMapLeft\"><TD>Aligned<TD>%0.1f%%</TR>\n",
       bag->meta->srcAligned/10.0);

// gene and CDS
printf("<TR CLASS=\"transMapLeft\"><TD>Gene<TD>%s</TR>\n",
       strOrNbsp(bag->meta->geneName));
printf("<TR CLASS=\"transMapLeft\"><TD>Gene Id<TD>%s</TR>\n",
       strOrNbsp(bag->meta->geneId));
printf("<TR CLASS=\"transMapLeft\"><TD>Gene Type<TD>%s</TR>\n",
       strOrNbsp(bag->meta->geneType));
printf("<TR CLASS=\"transMapLeft\"><TD>Transcript Id<TD>%s</TR>\n",
       transMapIdToAcc(bag->meta->name));
printf("<TR CLASS=\"transMapLeft\"><TD>Transcript Type<TD>%s</TR>\n",
       strOrNbsp(bag->meta->transcriptType));
printf("<TR CLASS=\"transMapLeft\"><TD>CDS<TD>%s</TR>\n",
       strOrNbsp(bag->meta->oCDS));
printf("</TBODY></TABLE>\n");
}

static void displayAligns(struct trackDb *tdb, struct transMapBag *bag)
/* display cDNA alignments */
{
int start = cartInt(cart, "o");
printf("<H3>mRNA/Genomic Alignments</H3>");
printAlignmentsSimple(bag->psl, start, "hgcTransMapCdnaAli", tdb->table, bag->psl->qName);
}

void transMapClickHandler(struct trackDb *tdb, char *mappedId)
/* Handle click on a transMap tracks */
{
if (tdb == NULL)
    errAbort("transMapClickHandler called without trackDb");
if (mappedId == NULL)
    errAbort("transMapClickHandler called without mappedId");
struct transMapBag *bag = (trackDbSetting(tdb, "bigDataUrl") == NULL)
    ? transMapBagLoadDb(tdb, mappedId)
    : transMapBagLoadBig(tdb, mappedId);

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
}

static struct dnaSeq *getCdnaSeqDb(struct trackDb *tdb, char *name)
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
struct dnaSeq *seq = hDnaSeqMustGet(NULL, name, words[1], words[2]);
freeMem(specCopy);
return seq;
}

void transMapShowCdnaAli(struct trackDb *tdb, char *mappedId)
/* Show alignment for mappedId, mostly ripped off from htcCdnaAli */
{
if (tdb == NULL)
    errAbort("transMapShowCdnaAli called without trackDb");
if (mappedId == NULL)
    errAbort("transMapShowCdnaAli called without mappedId");
struct transMapBag *bag = NULL;
struct dnaSeq *seq = NULL;
if (trackDbSetting(tdb, "bigDataUrl") == NULL)
    {
    bag = transMapBagLoadDb(tdb, mappedId);
    seq = getCdnaSeqDb(tdb, transMapIdToSeqId(mappedId));
    }
else
    {
    bag = transMapBagLoadBig(tdb, mappedId);
    seq = newDnaSeq(cloneString(bag->meta->oSequence), strlen(bag->meta->oSequence),
                    mappedId);
    }

struct genbankCds cds;
if (isEmpty(bag->meta->oCDS) || !genbankCdsParse(bag->meta->oCDS, &cds))
    ZeroVar(&cds);  /* can't get or parse CDS, so zero it */

char title[1024];
safef(title, sizeof title, "%s vs Genomic", mappedId);
htmlFramesetStart(title);
showSomeAlignment(bag->psl, seq, gftDna, 0, seq->size, NULL, cds.start, cds.end);
dnaSeqFree(&seq);
transMapBagFree(&bag);
}
