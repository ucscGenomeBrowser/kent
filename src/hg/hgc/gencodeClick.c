/* gencodeClick - click handling for GENCODE tracks */
#include "common.h"
#include "hgc.h"
#include "gencodeClick.h"
#include "ccdsClick.h"
#include "genePred.h"
#include "genePredReader.h"
#include "ensFace.h"
#include "htmshell.h"
#include "jksql.h"
#include "encode/wgEncodeGencodeAttrs.h"
#include "encode/wgEncodeGencodeGeneSource.h"
#include "encode/wgEncodeGencodePdb.h"
#include "encode/wgEncodeGencodePubMed.h"
#include "encode/wgEncodeGencodeRefSeq.h"
#include "encode/wgEncodeGencodeTag.h"
#include "encode/wgEncodeGencodeTranscriptSource.h"
#include "encode/wgEncodeGencodeTranscriptSupport.h"
#include "encode/wgEncodeGencodeExonSupport.h"
#include "encode/wgEncodeGencodeUniProt.h"
#include "encode/wgEncodeGencodeAnnotationRemark.h"
#include "encode/wgEncodeGencodeTranscriptionSupportLevel.h"

/*
 * General notes:
 *  - this will be integrated into hgGene at some point, however this was
 *    done as part of hgc for timing reasons and to allow more time to design
 *    the hgGene part.
 *  - Tables below will output at least one row even if no data is available.
 *    
 */

/* Various URLs and URL templates.  At one time, these were in the ra file,
 * but that didn't prove that helpful and end up requiring updated the ra
 * files for every GENCODE version if a URL was added or changed. */
//FIXME: clean up RA files when CGIs no longer need them
static char *gencodeBiotypesUrl = "http://www.gencodegenes.org/gencode_biotypes.html";
static char *ensemblTranscriptIdUrl = "http://www.ensembl.org/Homo_sapiens/Transcript/Summary?db=core;t=%s";
static char *ensemblGeneIdUrl = "http://www.ensembl.org/Homo_sapiens/Gene/Summary?db=core;t=%s";
static char *vegaTranscriptIdUrl = "http://vega.sanger.ac.uk/Homo_sapiens/Transcript/Summary?db=core;t=%s";
static char *vegaGeneIdUrl = "http://vega.sanger.ac.uk/Homo_sapiens/Gene/Summary?db=core;g=%s";
static char *yalePseudoUrl = "http://tables.pseudogene.org/%s";
static char *hgncUrl = "http://www.genenames.org/data/hgnc_data.php?match=%s";
static char *geneCardsUrl = "http://www.genecards.org/cgi-bin/carddisp.pl?gene=%s";
static char *apprisHomeUrl = "http://appris.bioinfo.cnio.es/";
static char *apprisGeneUrl = "http://appris.bioinfo.cnio.es/report.html?id=%s&namespace=Ensembl_Gene_Id";
static char *apprisTranscriptUrl = "http://appris.bioinfo.cnio.es/report.html?id=%s&namespace=Ensembl_Transcript_Id";

static char *getBaseAcc(char *acc, char *accBuf, int accBufSize)
/* get the accession with version number dropped. */
{
safecpy(accBuf, accBufSize, acc);
char *dot = strchr(accBuf, '.');
if (dot != NULL)
    *dot = '\0';
return accBuf;
}

static char *getGencodeTable(struct trackDb *tdb, char *tableBase)
/* get a table name from the settings. */
{
return trackDbRequiredSetting(tdb, tableBase);
}

static int transAnnoCmp(const void *va, const void *vb)
/* Compare genePreds, sorting to keep select gene first.  The only cases
 * that annotations will be duplicated is if they are in the PAR and thus
 * on different chroms. */
{
const struct genePred *a = *((struct genePred **)va);
const struct genePred *b = *((struct genePred **)vb);
if (sameString(a->name, seqName))
    return -1;
else if (sameString(b->name, seqName))
    return 1;
else
    return strcmp(a->name, b->name);
}

static struct genePred *transAnnoLoad(struct sqlConnection *conn, struct trackDb *tdb, char *gencodeId)
/* load the gencode annotations and sort the one corresponding to the one that was clicked on is
 * first.  Should only have one or two. */
{
// must check chrom due to PAR
char where[256];
sqlSafefFrag(where, sizeof(where), "(chrom = \"%s\") and (name = \"%s\")", seqName, gencodeId);
struct genePred *transAnno = genePredReaderLoadQuery(conn, tdb->track, where);
slSort(&transAnno, transAnnoCmp);
return transAnno;
}

static struct wgEncodeGencodeAttrs *transAttrsLoad(struct trackDb *tdb, struct sqlConnection *conn, char *gencodeId)
/* load the gencode class information */
{
return sqlQueryObjs(conn, (sqlLoadFunc)wgEncodeGencodeAttrsLoad, sqlQuerySingle|sqlQueryMust,
                    "select * from %s where transcriptId = \"%s\"",
                    getGencodeTable(tdb, "wgEncodeGencodeAttrs"), gencodeId);
}

static void getGeneBounds(struct trackDb *tdb, struct sqlConnection *conn, struct genePred *transAnno,
                          int *geneChromStart, int *geneChromEnd)
/* find bounds for the gene */
{
// must check chrom due to PAR
char where[256];
sqlSafefFrag(where, sizeof(where), "(chrom = \"%s\") and (name2 = \"%s\")", seqName, transAnno->name2);
struct genePred *geneAnnos = genePredReaderLoadQuery(conn, tdb->track, where);
struct genePred *geneAnno;
*geneChromStart = transAnno->txStart;
*geneChromEnd = transAnno->txEnd;
for (geneAnno = geneAnnos; geneAnno != NULL; geneAnno = geneAnno->next)
    {
    *geneChromStart = min(*geneChromStart, geneAnno->txStart);
    *geneChromEnd = max(*geneChromEnd, transAnno->txEnd);
    }
genePredFreeList(&geneAnnos);
}

static void *metaDataLoad(struct trackDb *tdb, struct sqlConnection *conn, char *gencodeId, char *tableBase, char *keyCol, unsigned queryOpts, sqlLoadFunc loadFunc)
/* load autoSql objects for gencode meta data. */
{
return sqlQueryObjs(conn, loadFunc, queryOpts, "select * from %s where %s = \"%s\"",
                    getGencodeTable(tdb, tableBase), keyCol, gencodeId);
}

static int uniProtDatasetCmp(const void *va, const void *vb)
/* Compare wgEncodeGencodeUniProt by dateset */
{
const struct wgEncodeGencodeUniProt *a = *((struct wgEncodeGencodeUniProt **)va);
const struct wgEncodeGencodeUniProt *b = *((struct wgEncodeGencodeUniProt **)vb);
return a->dataset - b->dataset;
}

static char *getMethodDesc(char *source)
/* return the annotation method name based gene or transcript source */
{
// looks for being havana and/or ensembl
// classifies other sources as automatic (mt_genbank_import ncrna ncrna_pseudogene)
bool hasHav = containsStringNoCase(source, "havana") != NULL;
bool hasEns = containsStringNoCase(source, "ensembl") != NULL;
if (hasHav && hasEns)
    return "manual &amp; automatic";
else if (hasHav)
    return "manual";
else
    return "automatic";
}

static char *getLevelDesc(int level)
/* return english description for level */
{
if (level == 1)
    return "validated";
else if (level == 2)
    return "manual";
else if (level == 3)
    return "automatic";
else
    return "unknown";
}

static char *getSupportLevelDesc(struct wgEncodeGencodeTranscriptionSupportLevel *tsl)
/* return description for level */
{
static char buf[32];
if ((tsl == NULL) || (tsl->level <= 0))
    return "tslNA";
else
    {
    safef(buf, sizeof(buf), "tsl%d", tsl->level);
    return buf;
    }
}

static void prExtIdAnchor(char *id, char *urlTemplate)
/* if an id to an external database is not empty, print an HTML anchor to it */
{
if (!isEmpty(id))
    {
    char urlBuf[512];
    safef(urlBuf, sizeof(urlBuf), urlTemplate, id);
    printf("<a href=\"%s\" target=_blank>%s</a>", urlBuf, id);
    }
}

static void prTdExtIdAnchor(char *id, char *urlTemplate)
/* print a table data element with an anchor for a id */
{
printf("<td>");
prExtIdAnchor(id, urlTemplate);
}

static void writePosLink(char *chrom, int chromStart, int chromEnd)
/* write link to a genomic position */
{
printf("<a href=\"%s&db=%s&position=%s%%3A%d-%d\" target=_blank>%s:%d-%d</A>",
       hgTracksPathAndSettings(), database,
       chrom, chromStart, chromEnd, chrom, chromStart+1, chromEnd);
}

static void writeBasicInfoHtml(struct trackDb *tdb, char *gencodeId, struct genePred *transAnno, struct wgEncodeGencodeAttrs *transAttrs,
                               int geneChromStart, int geneChromEnd,
                               struct wgEncodeGencodeGeneSource *geneSource, struct wgEncodeGencodeTranscriptSource *transcriptSource,
                               bool haveTsl, struct wgEncodeGencodeTranscriptionSupportLevel *tsl)
/* write basic HTML info for all genes */
{
/*
 * notes:
 *   - According to Steve: `status' is not the same for ensembl and havana.  So either avoid displaying it
 *     or display it as `automatic status' or `manual status'.
 */

// basic gene and transcript information
printf("<table class=\"hgcCcds\" style=\"white-space: nowrap;\"><thead>\n");
printf("<tr><th><th>Transcript<th>Gene</tr>\n");
printf("</thead><tbody>\n");

printf("<tr><th>Gencode id");
prTdExtIdAnchor(transAttrs->transcriptId, ensemblTranscriptIdUrl);
prTdExtIdAnchor(transAttrs->geneId, ensemblGeneIdUrl);
printf("</tr>\n");

printf("<tr><th>HAVANA manual id");
prTdExtIdAnchor(transAttrs->havanaTranscriptId, vegaTranscriptIdUrl);
prTdExtIdAnchor(transAttrs->havanaGeneId, vegaGeneIdUrl);
printf("</tr>\n");

printf("<tr><th>Position");
printf("<td>");
writePosLink(transAnno->chrom, transAnno->txStart, transAnno->txEnd);
printf("<td>");
writePosLink(transAnno->chrom, geneChromStart, geneChromEnd);
printf("</tr>\n");

printf("<tr><th>Strand<td>%s<td></tr>\n", transAnno->strand);

printf("<tr><th><a href=\"%s\">Biotype</a><td>%s<td>%s</tr>\n", gencodeBiotypesUrl, transAttrs->transcriptType, transAttrs->geneType);

printf("<tr><th>Status<td>%s<td>%s</tr>\n", transAttrs->transcriptStatus, transAttrs->geneStatus);

printf("<tr><th>Annotation Level<td>%s (%d)<td></tr>\n", getLevelDesc(transAttrs->level), transAttrs->level);

printf("<tr><th>Annotation Method<td>%s<td>%s</tr>\n", getMethodDesc(transcriptSource->source), getMethodDesc(geneSource->source));

if (haveTsl)
    {
    char *tslDesc = getSupportLevelDesc(tsl);
    printf("<tr><th><a href=\"#tsl\">Transcription Support Level</a><td><a href=\"#%s\">%s</a><td></tr>\n", tslDesc, tslDesc);
    }
printf("<tr><th>HGNC gene symbol<td colspan=2>");
prExtIdAnchor(transAttrs->geneName, hgncUrl);
printf("</tr>\n");

printf("<tr><th>CCDS<td>");
if (!isEmpty(transAttrs->ccdsId))
    {
    printf("<a href=\"");
    printCcdsExtUrl(transAttrs->ccdsId);
    printf("\" target=_blank>%s</a>", transAttrs->ccdsId);
    }
printf("<td></tr>\n");

printf("<tr><th>GeneCards<td colspan=2>");
prExtIdAnchor(transAttrs->geneName, geneCardsUrl);
printf("</tr>\n");

printf("<tr><th><a href=\"%s\" target=_blank>APPRIS</a>\n", apprisHomeUrl);
char accBuf[64];
prTdExtIdAnchor(getBaseAcc(transAttrs->transcriptId, accBuf, sizeof(accBuf)), apprisTranscriptUrl);
prTdExtIdAnchor(getBaseAcc(transAttrs->geneId, accBuf, sizeof(accBuf)), apprisGeneUrl);
printf("</tr>\n");

// FIXME: add sequence here??
printf("</tbody></table>\n");
}

static void writeSequenceHtml(struct trackDb *tdb, char *gencodeId, struct genePred *transAnno)
/* write links to get sequences */
{
printf("<table class=\"hgcCcds\"><thead>\n");
printf("<tr><th colspan=\"2\">Sequences</tr>\n");
printf("</thead><tbody>\n");
if (transAnno->cdsStart < transAnno->cdsEnd)
    {
    // protein coding
    printf("<tr><td width=\"50%%\">");
    hgcAnchorSomewhere("htcGeneMrna", gencodeId, tdb->table, seqName);
    printf("Predicted mRNA</a>");
    printf("<td width=\"50%%\">");
    hgcAnchorSomewhere("htcTranslatedPredMRna", gencodeId, "translate", seqName);
    printf("Predicted protein</a></tr>\n");
    }
else
    {
    // non-protein coding
    printf("<tr><td width=\"50%%\">");
    hgcAnchorSomewhere("htcGeneMrna", gencodeId, tdb->table, seqName);
    printf("Predicted mRNA</a><td width=\"50%%\"></tr>\n");
    }
printf("</tbody></table>\n");
}

static void writeAnnotationRemarkHtml(struct wgEncodeGencodeAnnotationRemark *remarks)
/* write HTML links to remarks */
{
printf("<table class=\"hgcCcds\"><thead>\n");
printf("<tr><th colspan=\"1\">Annotation Remarks</tr>\n");
printf("</thead><tbody>\n");
// make sure at least one empty row in printed
if (remarks == NULL)
    printf("<tr><td></td></tr>\n");
struct wgEncodeGencodeAnnotationRemark *remark;
for (remark = remarks; remark != NULL; remark = remark->next)
    {
    char *encRemark = htmlEncode(remark->remark);
    printf("<tr><td>%s</td></tr>\n", encRemark);
    freeMem(encRemark);
    }
printf("</tbody></table>\n");
}

static void writePdbLinkHtml(struct wgEncodeGencodePdb *pdbs)
/* write HTML links to PDB */
{
printf("<table class=\"hgcCcds\"><thead>\n");
printf("<tr><th colspan=\"3\">Protein Data Bank</tr>\n");
printf("</thead><tbody>\n");
struct wgEncodeGencodePdb *pdb = pdbs;
int i, rowCnt = 0;
while ((pdb != NULL) || (rowCnt == 0))
    {
    printf("<tr>");
    for (i = 0; i < 3; i++)
        {
        printf("<td width=\"33.33%%\">");
        if (pdb != NULL)
            {
            printf("<a href=\"http://www.rcsb.org/pdb/cgi/explore.cgi?job=graphics&pdbId=%s\" target=_blank>%s</a>", pdb->pdbId, pdb->pdbId);
            pdb = pdb->next;
            }
        }
    printf("</tr>\n");
    rowCnt++;
    }
printf("</tbody></table>\n");
}

static void writePubMedEntry(struct wgEncodeGencodePubMed *pubMed)
/* write HTML table entry for a pubMed */
{
printf("<td width=\"33.33%%\"><a href=\"");
printEntrezPubMedUidUrl(stdout, pubMed->pubMedId);
printf("\" target=_blank>%d</a>", pubMed->pubMedId);
}

static void writePubMedLinkHtml(struct wgEncodeGencodePubMed *pubMeds)
/* write HTML links to PubMed */
{
printf("<table class=\"hgcCcds\"><thead>\n");
printf("<tr><th colspan=\"3\">PubMed</tr>\n");
printf("</thead><tbody>\n");
struct wgEncodeGencodePubMed *pubMed = pubMeds;
int i, rowCnt = 0;
while ((pubMed != NULL) || (rowCnt == 0))
    {
    printf("<tr>");
    for (i = 0; i < 3; i++)
        {
        if (pubMed != NULL)
            {
            writePubMedEntry(pubMed);
            pubMed = pubMed->next;
            }
        else
            printf("<td width=\"33.33%%\">");
        }
    printf("</tr>\n");
    rowCnt++;
    }
printf("</tbody></table>\n");
}

static void writeRefSeqEntry(struct wgEncodeGencodeRefSeq *refSeq)
/* write HTML table entry for a RefSeq */
{
printf("<td width=\"50%%\"><a href=\"");
printEntrezNucleotideUrl(stdout, refSeq->rnaAcc);
printf("\" target=_blank>%s</a>", refSeq->rnaAcc);
printf("<td width=\"50%%\">");
if (!isEmpty(refSeq->pepAcc))
    {
    printf("<a href=\"");
    printEntrezProteinUrl(stdout, refSeq->pepAcc);
    printf("\" target=_blank>%s</a>", refSeq->pepAcc);
    }
}

static void writeRefSeqLinkHtml(struct wgEncodeGencodeRefSeq *refSeqs)
/* write HTML links to RefSeq */
{
printf("<table class=\"hgcCcds\"><thead>\n");
printf("<tr><th colspan=\"2\">RefSeq</tr>\n");
printf("<tr class=\"hgcCcdsSub\"><th>RNA<th>Protein</tr>\n");
printf("</thead><tbody>\n");
struct wgEncodeGencodeRefSeq *refSeq = refSeqs;
int rowCnt = 0;
while ((refSeq != NULL) || (rowCnt == 0))
    {
    printf("<tr>");
    if (refSeq != NULL)
        {
        writeRefSeqEntry(refSeq);
        refSeq = refSeq->next;
        }
    else
        printf("<td width=\"50%%\"><td width=\"50%%\">");
    printf("</tr>\n");
    rowCnt++;
    }
printf("</tbody></table>\n");
}

static void writeUniProtEntry(struct wgEncodeGencodeUniProt *uniProt)
/* write HTML table entry for a UniProt */
{
printf("<td width=\"15%%\">%s", (uniProt->dataset == wgEncodeGencodeUniProtSwissProt) ? "SwissProt" : "TrEMBL");
printf("<td width=\"15%%\"><a href=\"");
printSwissProtAccUrl(stdout, uniProt->acc);
printf("\" target=_blank>%s</a>", uniProt->acc);
printf("<td width=\"20%%\"><a href=\"");
printSwissProtAccUrl(stdout, uniProt->name);
printf("\" target=_blank>%s</a>", uniProt->name);
}

static void writeUniProtLinkHtml(struct wgEncodeGencodeUniProt *uniProts)
/* write HTML links to UniProt */
{
printf("<table class=\"hgcCcds\"><thead>\n");
printf("<tr><th colspan=\"6\">UniProt</tr>\n");
printf("<tr class=\"hgcCcdsSub\"><th>Data set<th>Accession<th>Name<th>Data set<th>Accession<th>Name</tr>\n");
printf("</thead><tbody>\n");
int i, rowCnt = 0;
struct wgEncodeGencodeUniProt *uniProt = uniProts;
while ((uniProt != NULL) || (rowCnt == 0))
    {
    printf("<tr>");
    for (i = 0; i < 2; i++)
        {
        if (uniProt != NULL)
            {
            writeUniProtEntry(uniProt);
            uniProt = uniProt->next;
            }
        else
            printf("<td width=\"50%%\" colspan=3>");
        }
    printf("</tr>\n");
    rowCnt++;
    }
printf("</tbody></table>\n");
}

struct supportEvid
/* temporary struct for subset of supporting information displayed */
{
    struct supportEvid *next;
    char *seqId;              /* sequence id (memory not owned) */
    char *seqSrc;             /* evidence source database (memory not owned) */
};

static int supportEvidCmp(const void *va, const void *vb)
/* Compare two supportEvid objects. */
{
const struct supportEvid *a = *((struct supportEvid **)va);
const struct supportEvid *b = *((struct supportEvid **)vb);
int diff = strcmp(a->seqSrc, b->seqSrc);
if (diff == 0)
    diff = strcmp(a->seqId, b->seqId);
return diff;
}

static void transcriptSupportToSupportEvid(struct supportEvid **supportEvids, struct wgEncodeGencodeTranscriptSupport *transcriptSupports)
/* convert transcriptSupport to common structure */
{
struct wgEncodeGencodeTranscriptSupport *transcriptSupport;
for (transcriptSupport = transcriptSupports; transcriptSupport != NULL; transcriptSupport = transcriptSupport->next)
    {
    struct supportEvid *supportEvid;
    AllocVar(supportEvid);
    supportEvid->seqId = transcriptSupport->seqId;
    supportEvid->seqSrc = transcriptSupport->seqSrc;
    slAddHead(supportEvids, supportEvid);
    }
}

static void exonSupportToSupportEvid(struct supportEvid **supportEvids, struct wgEncodeGencodeExonSupport *exonSupports)
/* convert exonSupport to common structure */
{
struct wgEncodeGencodeExonSupport *exonSupport;
for (exonSupport = exonSupports; exonSupport != NULL; exonSupport = exonSupport->next)
    {
    struct supportEvid *supportEvid;
    AllocVar(supportEvid);
    supportEvid->seqId = exonSupport->seqId;
    supportEvid->seqSrc = exonSupport->seqSrc;
    slAddHead(supportEvids, supportEvid);
    }
}

static void sortUniqSupportExidence(struct supportEvid **supportEvids)
/* sort support evidence and make unique */
{
struct supportEvid *supportEvid, *supportEvids2 = NULL;
slSort(supportEvids, supportEvidCmp);
// make unique
while ((supportEvid = slPopHead(supportEvids)) != NULL)
    {
    if ((supportEvids2 == NULL) || (supportEvidCmp(&supportEvid, &supportEvids2) != 0))
        slAddHead(&supportEvids2, supportEvid);
    else
        freeMem(supportEvid);
    }
slReverse(&supportEvids2);
*supportEvids = supportEvids2;
}

static struct supportEvid *loadSupportEvid(struct wgEncodeGencodeTranscriptSupport *transcriptSupports,
                                           struct wgEncodeGencodeExonSupport *exonSupports)
/* load transcript and supporting evidence into a common structure */
{
struct supportEvid *supportEvids = NULL;
transcriptSupportToSupportEvid(&supportEvids, transcriptSupports);
exonSupportToSupportEvid(&supportEvids, exonSupports);
sortUniqSupportExidence(&supportEvids);
return supportEvids;
}

static char *getSupportEvidEnsemblUrl(char *gencodeId)
/* Generate a URL to the ensembl supporting evidence page.
 * WARNING: static return. */
{
static char url[256];
// generate organism part of url in the form: Homo_sapiens
char *sciName = hScientificName(database);
if (sciName == NULL)
    errAbort("can't get scientific name for %s", database);
char *space = strchr(sciName, ' ');
if (space != NULL)
    *space = '_';
safef(url, sizeof(url), "http://www.ensembl.org/%s/Transcript/SupportingEvidence?db=core;t=%s", sciName, gencodeId);
freeMem(sciName);
return url;
}

static void writeSupportExidenceEntry(struct supportEvid *supportEvid)
/* write HTML table entry  for a supporting evidence */
{
// FIXME: should link to sources when possible
printf("<td width=\"25%%\">%s", supportEvid->seqSrc);
printf("<td width=\"25%%\">%s", supportEvid->seqId);
}

static void writeSupportingEvidenceLinkHtml(char *gencodeId, struct wgEncodeGencodeTranscriptSupport *transcriptSupports,
                                            struct wgEncodeGencodeExonSupport *exonSupports)
/* write HTML links to supporting evidence */
{
struct supportEvid *supportEvids = loadSupportEvid(transcriptSupports, exonSupports);

printf("<table class=\"hgcCcds\"><thead>\n");
printf("<tr><th colspan=\"4\">Supporting Evidence (<a href=\"%s\" target=\"_blank\">view in Ensembl</a>)</tr>\n", getSupportEvidEnsemblUrl(gencodeId));
printf("<tr class=\"hgcCcdsSub\"><th>Source<th>Sequence<th>Source<th>Sequence</tr>\n");
printf("</thead><tbody>\n");
struct supportEvid *supportEvid = supportEvids;
int i, rowCnt = 0;
while ((supportEvid != NULL) || (rowCnt == 0))
    {
    printf("<tr>");
    for (i = 0; i < 2; i++)
        {
        if (supportEvid != NULL)
            {
            writeSupportExidenceEntry(supportEvid);
            supportEvid = supportEvid->next;
            }
        else
            printf("<td colspan=\"2\" width=\"50%%\">");
        }
    printf("</tr>\n");
    rowCnt++;
    }
printf("</tbody></table>\n");
slFreeList(&supportEvids);
}

static void writeTagEntry(struct wgEncodeGencodeTag *tag)
/* write HTML table entry for a Tag */
{
// FIXME: link to help once gencodegenes.org has it
printf("<td width=\"33.33%%\">%s", tag->tag);
}

static void writeTagLinkHtml(struct wgEncodeGencodeTag *tags)
/* write HTML links to Tag */
{
printf("<table class=\"hgcCcds\"><thead>\n");
printf("<tr><th colspan=3>Tags</tr>\n");
printf("</thead><tbody>\n");
int i, rowCnt = 0;
struct wgEncodeGencodeTag *tag = tags;
while ((tag != NULL) || (rowCnt == 0))
    {
    printf("<tr>");
    for (i = 0; i < 3; i++)
        {
        if (tag != NULL)
            {
            writeTagEntry(tag);
            tag = tag->next;
            }
        else
            printf("<td width=\"33.33%%\">");
        }
    printf("</tr>\n");
    rowCnt++;
    }
printf("</tbody></table>\n");
}

static void doGencodeGeneTrack(struct trackDb *tdb, char *gencodeId, struct sqlConnection *conn, struct genePred *transAnno)
/* Process click on a GENCODE gene annotation track. */
{
struct wgEncodeGencodeAttrs *transAttrs = transAttrsLoad(tdb, conn, gencodeId);
char *gencodeGeneId = transAttrs->geneId;
struct wgEncodeGencodeGeneSource *geneSource = metaDataLoad(tdb, conn, gencodeGeneId, "wgEncodeGencodeGeneSource", "geneId", sqlQueryMust|sqlQuerySingle, (sqlLoadFunc)wgEncodeGencodeGeneSourceLoad);
struct wgEncodeGencodeTranscriptSource *transcriptSource = metaDataLoad(tdb, conn, gencodeId, "wgEncodeGencodeTranscriptSource", "transcriptId", sqlQueryMust|sqlQuerySingle, (sqlLoadFunc)wgEncodeGencodeTranscriptSourceLoad);
bool haveRemarks = (trackDbSetting(tdb, "wgEncodeGencodeAnnotationRemark") != NULL);
struct wgEncodeGencodeAnnotationRemark *remarks = haveRemarks ? metaDataLoad(tdb, conn, gencodeId, "wgEncodeGencodeAnnotationRemark", "transcriptId", 0, (sqlLoadFunc)wgEncodeGencodeAnnotationRemarkLoad) : NULL;
struct wgEncodeGencodePdb *pdbs = metaDataLoad(tdb, conn, gencodeId, "wgEncodeGencodePdb", "transcriptId", sqlQueryMulti, (sqlLoadFunc)wgEncodeGencodePdbLoad);
struct wgEncodeGencodePubMed *pubMeds = metaDataLoad(tdb, conn, gencodeId, "wgEncodeGencodePubMed", "transcriptId", sqlQueryMulti, (sqlLoadFunc)wgEncodeGencodePubMedLoad);
struct wgEncodeGencodeRefSeq *refSeqs = metaDataLoad(tdb, conn, gencodeId, "wgEncodeGencodeRefSeq", "transcriptId", sqlQueryMulti, (sqlLoadFunc)wgEncodeGencodeRefSeqLoad);
struct wgEncodeGencodeTag *tags = metaDataLoad(tdb, conn, gencodeId, "wgEncodeGencodeTag", "transcriptId", sqlQueryMulti, (sqlLoadFunc)wgEncodeGencodeTagLoad);
struct wgEncodeGencodeTranscriptSupport *transcriptSupports = metaDataLoad(tdb, conn, gencodeId, "wgEncodeGencodeTranscriptSupport", "transcriptId", sqlQueryMulti, (sqlLoadFunc)wgEncodeGencodeTranscriptSupportLoad);
struct wgEncodeGencodeExonSupport *exonSupports = metaDataLoad(tdb, conn, gencodeId, "wgEncodeGencodeExonSupport", "transcriptId", sqlQueryMulti, (sqlLoadFunc)wgEncodeGencodeExonSupportLoad);
struct wgEncodeGencodeUniProt *uniProts = metaDataLoad(tdb, conn, gencodeId, "wgEncodeGencodeUniProt", "transcriptId", sqlQueryMulti, (sqlLoadFunc)wgEncodeGencodeUniProtLoad);
slSort(&uniProts, uniProtDatasetCmp);
bool haveTsl = (trackDbSetting(tdb, "wgEncodeGencodeTranscriptionSupportLevel") != NULL);
struct wgEncodeGencodeTranscriptionSupportLevel *tsl = haveTsl ? metaDataLoad(tdb, conn, gencodeId, "wgEncodeGencodeTranscriptionSupportLevel", "transcriptId", 0, (sqlLoadFunc)wgEncodeGencodeTranscriptionSupportLevelLoad) : NULL;

int geneChromStart, geneChromEnd;
getGeneBounds(tdb, conn, transAnno, &geneChromStart, &geneChromEnd);

char *title = "GENCODE Transcript Annotation";
char header[256];
safef(header, sizeof(header), "%s %s", title, gencodeId);
if (!isEmpty(transAttrs->geneName))
    safef(header, sizeof(header), "%s %s (%s)", title, gencodeId, transAttrs->geneName);
else
    safef(header, sizeof(header), "%s %s", title, gencodeId);
cartWebStart(cart, database, "%s", header);
printf("<H2>%s</H2>\n", header);

writeBasicInfoHtml(tdb, gencodeId, transAnno, transAttrs, geneChromStart, geneChromEnd, geneSource, transcriptSource, haveTsl, tsl);
writeTagLinkHtml(tags);
writeSequenceHtml(tdb, gencodeId, transAnno);
if (haveRemarks)
    writeAnnotationRemarkHtml(remarks);
writePdbLinkHtml(pdbs);
writePubMedLinkHtml(pubMeds);
writeRefSeqLinkHtml(refSeqs);
writeUniProtLinkHtml(uniProts);
writeSupportingEvidenceLinkHtml(gencodeId, transcriptSupports, exonSupports);
wgEncodeGencodeAttrsFree(&transAttrs);
wgEncodeGencodeAnnotationRemarkFreeList(&remarks);
wgEncodeGencodeGeneSourceFreeList(&geneSource);
wgEncodeGencodeTranscriptSourceFreeList(&transcriptSource);
wgEncodeGencodePdbFreeList(&pdbs);
wgEncodeGencodePubMedFreeList(&pubMeds);
wgEncodeGencodeRefSeqFreeList(&refSeqs);
wgEncodeGencodeTranscriptSupportFreeList(&transcriptSupports);
wgEncodeGencodeExonSupportFreeList(&exonSupports);
wgEncodeGencodeUniProtFreeList(&uniProts);
wgEncodeGencodeTranscriptionSupportLevelFreeList(&tsl);
}

static void doGencodeGene2WayPseudo(struct trackDb *tdb, char *gencodeId, struct sqlConnection *conn, struct genePred *pseudoAnno)
/* Process click on a GENCODE two-way pseudogene annotation track. */
{
char header[256];
safef(header, sizeof(header), "GENCODE 2-way consensus pseudogene %s", gencodeId);
cartWebStart(cart, database, "%s", header);
printf("<H2>%s</H2>\n", header);
printf("<b>Yale id:</b> ");
prExtIdAnchor(gencodeId, yalePseudoUrl);
printf("<br>");
printPos(pseudoAnno->chrom, pseudoAnno->txStart, pseudoAnno->txEnd, pseudoAnno->strand, FALSE, NULL);
}

static void doGencodeGenePolyA(struct trackDb *tdb, char *gencodeId, struct sqlConnection *conn, struct genePred *polyAAnno)
/* Process click on a GENCODE poly-A annotation track. */
{
char header[256];
safef(header, sizeof(header), "GENCODE PolyA Annotation %s (%s)", polyAAnno->name2, gencodeId);
cartWebStart(cart, database, "%s", header);
printf("<H2>%s</H2>\n", header);
printf("<b>Annotation id:</b> %s<br>", gencodeId);
printf("<b>Annotation Type:</b> %s<br>",polyAAnno->name2);
printPos(polyAAnno->chrom, polyAAnno->txStart, polyAAnno->txEnd, polyAAnno->strand, FALSE, NULL);
}

void doGencodeGene(struct trackDb *tdb, char *gencodeId)
/* Process click on a GENCODE annotation. */
{
struct sqlConnection *conn = hAllocConn(database);
struct genePred *anno = transAnnoLoad(conn, tdb, gencodeId);
if (startsWith("wgEncodeGencodeBasic", tdb->track)
    || startsWith("wgEncodeGencodeComp", tdb->track)
    || startsWith("wgEncodeGencodePseudoGene", tdb->track))
    doGencodeGeneTrack(tdb, gencodeId, conn, anno);
else if (startsWith("wgEncodeGencode2wayConsPseudo", tdb->track))
    doGencodeGene2WayPseudo(tdb, gencodeId, conn, anno);
else if (startsWith("wgEncodeGencodePolya", tdb->track))
    doGencodeGenePolyA(tdb, gencodeId, conn, anno);
else
    errAbort("doGencodeGene: track not handled: \"%s\"", tdb->track);

htmlHorizontalLine();
printTrackHtml(tdb);
cartWebEnd();

genePredFreeList(&anno);
hFreeConn(&conn);
}

bool isNewGencodeGene(struct trackDb *tdb)
/* is this a new-style gencode (>= V7) track, as indicated by
 * the presence of the wgEncodeGencodeVersion setting */
{
return trackDbSetting(tdb, "wgEncodeGencodeVersion") != NULL;
}
