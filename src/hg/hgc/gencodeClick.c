/* gencodeClick - click handling for GENCODE tracks */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "hgc.h"
#include "gencodeClick.h"
#include "gencodeTracksCommon.h"
#include "ccdsClick.h"
#include "genePred.h"
#include "genePredReader.h"
#include "ensFace.h"
#include "htmshell.h"
#include "jksql.h"
#include "regexHelper.h"
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
#include "encode/wgEncodeGencodeEntrezGene.h"
#include "encode/wgEncodeGencodeAnnotationRemark.h"
#include "encode/wgEncodeGencodeTranscriptionSupportLevel.h"
#include "encode/wgEncodeGencodeGeneSymbol.h"

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
static char *ensemblTranscriptIdUrl = "http://www.ensembl.org/%s/Transcript/Summary?db=core;t=%s";
static char *ensemblGeneIdUrl = "http://www.ensembl.org/%s/Gene/Summary?db=core;t=%s";
static char *ensemblProteinIdUrl = "http://www.ensembl.org/%s/Transcript/ProteinSummary?db=core;t=%s";
static char *ensemblSupportingEvidUrl = "http://www.ensembl.org/%s/Transcript/SupportingEvidence?db=core;t=%s";

static char *ensemblH37TranscriptIdUrl = "http://grch37.ensembl.org/%s/Transcript/Summary?db=core;t=%s";
static char *ensemblH37GeneIdUrl = "http://grch37.ensembl.org/%s/Gene/Summary?db=core;t=%s";
static char *ensemblH37ProteinIdUrl = "http://grch37.ensembl.org/%s/Transcript/ProteinSummary?db=core;t=%s";
static char *ensemblH37SupportingEvidUrl = "http://grch37.ensembl.org/%s/Transcript/SupportingEvidence?db=core;t=%s";

static char *gencodeBiotypesUrl = "http://www.gencodegenes.org/pages/biotypes.html";
static char *gencodeTagsUrl = "http://www.gencodegenes.org/pages/tags.html";

static char *yalePseudoUrl = "http://tables.pseudogene.org/%s";
static char *apprisHomeUrl = "https://appris.bioinfo.cnio.es/#/";
static char *apprisGeneUrl = "https://appris.bioinfo.cnio.es/#/database/id/%s/%s?sc=ensembl";

// species-specific
static char *hgncByIdUrl = "https://www.genenames.org/data/gene-symbol-report/#!/hgnc_id/%s";
static char *hgncBySymUrl = " https://www.genenames.org/data/gene-symbol-report/#!/symbol/%s";
static char *geneCardsUrl = "http://www.genecards.org/cgi-bin/carddisp.pl?gene=%s";
static char *mgiBySymUrl = "http://www.informatics.jax.org/quicksearch/summary?queryType=exactPhrase&query=%s";
static char *mgiByIdUrl = "http://www.informatics.jax.org/accession/%s";

static char* UNKNOWN = "unknown";

static boolean isGrcHuman()
/* is this a GRC human assembly? */
{
//bool result = FALSE;
//if (startsWith("hg", database))
//    result = TRUE;
//else if (!startsWith("mm", database))
//    warn("BUG: gencodeClick on wrong database: %s", database);
//return result;

if (startsWith("hg", database))
    return TRUE;
else if (startsWith("mm", database))
    return FALSE;
else
    errAbort("BUG: gencodeClick on wrong database: %s", database);
return FALSE;
}

static bool haveGencodeTable(struct sqlConnection *conn, struct trackDb *tdb, char *tableBase)
/* determine if a gencode table exists; it might be option or not in older releases */
{
return sqlTableExists(conn, gencodeGetTableName(tdb, tableBase));
}

static boolean isGrcH37Native(struct trackDb *tdb)
/* Is this GENCODE GRCh37 native build, which requires a different Ensembl site. */
{
// check for non-lifted GENCODE on GRCh37/hg19
if (sameString(database, "hg19"))
    return stringIn("lift37", gencodeGetVersion(tdb)) == NULL;
else
    return FALSE;
}

static boolean isRealGeneSymbol(char* geneName, boolean haveGeneSymbolSource,
                                struct wgEncodeGencodeGeneSymbol *geneSymbolSource)
/* Attempt to determine if this is a gene symbol assigned by HGNC/MGI or a
 * generate one.  newer versions of GENCODE have wgEncodeGencodeGeneSymbol,
 * which has the definitions.  With older version of GENCODE guess a guess
 * is make by trying to match older clone-based names ones.
 */
{
if (haveGeneSymbolSource)
    {
    return geneSymbolSource != NULL;
    }
else
    {
    // 'A' followed by one or more letters, then followed by numbers and a '.',
    // followed by an incrementing gene number.
    static const char *regexp = "^A[A-Z]+[0-9]+\\.[0-9]+$1)";
    return regexMatch(geneName, regexp);
    }
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

static bool isProteinCodingTrans(struct wgEncodeGencodeAttrs *transAttrs)
/* is a transcript protein coding? */
{
return sameString(transAttrs->transcriptClass, "coding");
}

static struct genePred *transAnnoLoad(struct sqlConnection *conn, struct trackDb *tdb, char *gencodeId)
/* load the gencode annotations and sort the one corresponding to the one that was clicked on is
 * first.  Should only have one or two. */
{
// must check chrom due to PAR
char where[256];
sqlSafef(where, sizeof(where), "(chrom = \"%s\") and (name = \"%s\")", seqName, gencodeId);
struct genePred *transAnno = genePredReaderLoadQuery(conn, tdb->track, where);
slSort(&transAnno, transAnnoCmp);
return transAnno;
}

static struct wgEncodeGencodeAttrs *transAttrsLoad(struct trackDb *tdb, struct sqlConnection *conn, char *gencodeId)
/* load the gencode attributes */
{
char query[1024];
sqlSafef(query, sizeof(query), "select * from %s where transcriptId = \"%s\"",
         gencodeGetTableName(tdb, "wgEncodeGencodeAttrs"), gencodeId);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row = sqlNextRow(sr);
if (row == NULL)
    errAbort("gencode transcript %s not found in %s", gencodeId,
             gencodeGetTableName(tdb, "wgEncodeGencodeAttrs"));
// older version don't have proteinId column.
struct wgEncodeGencodeAttrs *transAttrs = wgEncodeGencodeAttrsLoad(row, sqlCountColumns(sr));
sqlFreeResult(&sr);
return transAttrs;
}

static void getGeneBounds(struct trackDb *tdb, struct sqlConnection *conn, struct genePred *transAnno,
                          int *geneChromStart, int *geneChromEnd)
/* find bounds for the gene */
{
// must check chrom due to PAR
char where[256];
sqlSafef(where, sizeof(where), "(chrom = \"%s\") and (name2 = \"%s\")", seqName, transAnno->name2);
struct genePred *geneAnnos = genePredReaderLoadQuery(conn, tdb->track, where);
struct genePred *geneAnno;
*geneChromStart = transAnno->txStart;
*geneChromEnd = transAnno->txEnd;
for (geneAnno = geneAnnos; geneAnno != NULL; geneAnno = geneAnno->next)
    {
    *geneChromStart = min(*geneChromStart, geneAnno->txStart);
    *geneChromEnd = max(*geneChromEnd, geneAnno->txEnd);
    }
genePredFreeList(&geneAnnos);
}

static void *metaDataLoad(struct trackDb *tdb, struct sqlConnection *conn, char *gencodeId, char *tableBase, char *keyCol, unsigned queryOpts, sqlLoadFunc loadFunc)
/* load autoSql objects for gencode meta data. */
{
return sqlQueryObjs(conn, loadFunc, queryOpts, "select * from %s where %s = \"%s\"",
                    gencodeGetTableName(tdb, tableBase), keyCol, gencodeId);
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
// sometimes backmap doesn't get every entry method entry mapped.  Until that
// is fixed, allow it to be missing
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

static char* getScientificNameSym(void)
/* get the scientific name of an organism in the form "Homo_sapiens"
 *  WARNING: static return */
{
static char sciNameSym[128];
char *sciName = hScientificName(database);
if (sciName == NULL)
    errAbort("can't get scientific name for %s", database);
safecpy(sciNameSym, sizeof(sciNameSym), sciName);
freeMem(sciName);
subChar(sciNameSym, ' ', '_');
return sciNameSym;
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

#if UNUSED
static void prTdExtIdAnchor(char *id, char *urlTemplate)
/* print a table data element with an anchor for a id */
{
printf("<td>");
prExtIdAnchor(id, urlTemplate);
}
#endif

static void prEnsIdAnchor(char *id, char *urlTemplate)
/* if an id to an ensembl database is not empty, print an HTML anchor to it */
{
if (!isEmpty(id))
    {
    char idBuf[64], urlBuf[512];
    /* The lift37 releases append a '_N' modifier to the ids to indicate the are
     * mapped. N is an integer mapping version. Don't include this in link if it exists. */
    safecpy(idBuf, sizeof(idBuf), id);
    char *p = strchr(idBuf, '_');
    if (p != NULL)
        *p = '\0';
    safef(urlBuf, sizeof(urlBuf), urlTemplate, getScientificNameSym(), idBuf);
    printf("<a href=\"%s\" target=_blank>%s</a>", urlBuf, id);
    }
}

static void prTdEnsIdAnchor(char *id, char *urlTemplate)
/* print a table data element with an ensembl anchor for a id */
{
printf("<td>");
prEnsIdAnchor(id, urlTemplate);
}

static void prApprisTdAnchor(char *id, char *label, char *urlTemplate)
/* print a gene or transcript link to APPRIS */
{
// under bar separated, lower case species name.
char *speciesArg = hScientificName(database);
toLowerN(speciesArg, strlen(speciesArg));
subChar(speciesArg, ' ', '_');

char accBuf[64];
printf("<td><a href=\"");
printf(urlTemplate, speciesArg, gencodeBaseAcc(id, accBuf, sizeof(accBuf)));
printf("\" target=_blank>%s</a>", label);

freeMem(speciesArg);
}

static void writePosLink(char *chrom, int chromStart, int chromEnd)
/* write link to a genomic position */
{
printf("<a href=\"%s&db=%s&position=%s%%3A%d-%d\">%s:%d-%d</A>",
       hgTracksPathAndSettings(), database,
       chrom, chromStart, chromEnd, chrom, chromStart+1, chromEnd);
}

static bool geneHasApprisTranscripts(struct trackDb *tdb, struct sqlConnection *conn, struct wgEncodeGencodeAttrs *transAttrs)
/* check if any transcript in a gene has an APPRIS tags */
{
char query[1024];
sqlSafef(query, sizeof(query),
      "%s tag where tag.tag like 'appris%%' and transcriptId in "
      "(select transcriptId from %s where geneId='%s')",
      gencodeGetTableName(tdb, "wgEncodeGencodeTag"),
      gencodeGetTableName(tdb, "wgEncodeGencodeAttrs"),
      transAttrs->geneId);
return sqlRowCount(conn, query) > 0;
}

static char* findApprisTag(struct wgEncodeGencodeTag *tags)
/* search list for APPRIS tag or NULL */
{
struct wgEncodeGencodeTag *tag;
for (tag = tags; tag != NULL; tag = tag->next)
    {
    if (startsWith("appris_", tag->tag))
        return tag->tag;
    }
return NULL;
}

static char* apprisTagToSymbol(char* tag)
/* convert APPRIS tag to the symbol use by APPRIS. WARNING static return. */
{
// appris_principal_1 -> PRINCIPAL:1
static char buf[64];
safecpy(buf, sizeof(buf), tag+7);
touppers(buf);
subChar(buf, '_', ':');
return buf;
}

static void writeAprrisRow(struct sqlConnection *conn, struct trackDb *tdb,
                           struct wgEncodeGencodeAttrs *transAttrs,
                           struct wgEncodeGencodeTag *tags)
/* write row for APPRIS */
{
// Get labels to use. if transcript has an appris tag, then we link to the transcript.
// if it doesn;t have a appris tag, we can still link to the gene if any of the transcripts
// have appris tags
char* apprisTag = findApprisTag(tags);
char* transLabel = (apprisTag != NULL) ? apprisTagToSymbol(apprisTag) : NULL;
char *geneLabel = ((apprisTag != NULL) || geneHasApprisTranscripts(tdb, conn, transAttrs)) ? transAttrs->geneName : NULL;

// APPRIS gene and transcript now go to the same location
printf("<tr><th><a href=\"%s\" target=_blank>APPRIS</a>\n", apprisHomeUrl);
if (transLabel != NULL)
    prApprisTdAnchor(transAttrs->geneId, transLabel, apprisGeneUrl);
else
    printf("<td>&nbsp;");
if (geneLabel != NULL)
    prApprisTdAnchor(transAttrs->geneId, geneLabel, apprisGeneUrl);
else
    printf("<td>&nbsp;");
printf("</tr>\n");
}


static void writeHumanGeneLinkout(struct wgEncodeGencodeAttrs *transAttrs,
                                  boolean haveGeneSymbolSource, struct wgEncodeGencodeGeneSymbol *geneSymbolSource)
/* Write external gene database links for human if they appear to be a real
 * gene symbol Getting it wrong is not a disaster, as the target database will
 * just report not found.
 */
{
boolean isReal = isRealGeneSymbol(transAttrs->geneName, haveGeneSymbolSource, geneSymbolSource);
printf("<tr><th>HGNC gene information<td colspan=2>");
if (isReal)
    {
    if (haveGeneSymbolSource)
        prExtIdAnchor(geneSymbolSource->geneId, hgncByIdUrl);
    else
        prExtIdAnchor(transAttrs->geneName, hgncBySymUrl);

    }
printf("</tr>\n");

printf("<tr><th>GeneCards<td colspan=2>");
if (isReal)
    prExtIdAnchor(transAttrs->geneName, geneCardsUrl);
printf("</tr>\n");
}

static void writeMouseGeneLinkout(struct wgEncodeGencodeAttrs *transAttrs,
                                  boolean haveGeneSymbolSource, struct wgEncodeGencodeGeneSymbol *geneSymbolSource)
/* Write external gene database links for mouse if they appear to be a real
 * gene symbol Getting it wrong is not a disaster, as the target database will
 * just report not found.
 */
{
boolean isReal = isRealGeneSymbol(transAttrs->geneName, haveGeneSymbolSource, geneSymbolSource);
printf("<tr><th>MGI gene information<td colspan=2>");
if (isReal)
    {
    if (haveGeneSymbolSource)
        prExtIdAnchor(geneSymbolSource->geneId, mgiByIdUrl);
    else
        prExtIdAnchor(transAttrs->geneName, mgiBySymUrl);
    }
printf("</tr>\n");

}

static void writeBasicInfoHtml(struct sqlConnection *conn, struct trackDb *tdb, char *gencodeId, struct genePred *transAnno,
                               struct wgEncodeGencodeAttrs *transAttrs,
                               int geneChromStart, int geneChromEnd,
                               struct wgEncodeGencodeGeneSource *geneSource, struct wgEncodeGencodeTranscriptSource *transcriptSource,
                               struct wgEncodeGencodeTag *tags, bool haveTsl, struct wgEncodeGencodeTranscriptionSupportLevel *tsl,
                               boolean haveGeneSymbolSource, struct wgEncodeGencodeGeneSymbol *geneSymbolSource)
/* write basic HTML info for all genes */
{
// basic gene and transcript information
printf("<table class=\"hgcCcds\" style=\"white-space: nowrap;\"><thead>\n");
printf("<tr><th><th>Transcript<th>Gene</tr>\n");
printf("</thead><tbody>\n");

printf("<tr><th>GENCODE id");
prTdEnsIdAnchor(transAttrs->transcriptId,
                (isGrcH37Native(tdb) ? ensemblH37TranscriptIdUrl: ensemblTranscriptIdUrl));
prTdEnsIdAnchor(transAttrs->geneId,
                (isGrcH37Native(tdb) ? ensemblH37GeneIdUrl : ensemblGeneIdUrl));
printf("</tr>\n");

printf("<tr><th>Name");
printf("<td>%s", transAttrs->transcriptName);
printf("<td>%s", transAttrs->geneName);
printf("</tr>\n");

if (isNotEmpty(transAttrs->proteinId))
    {
    // protein id in database, maybe not for this transcript
    printf("<tr><th>Protein id");
    if (strlen(transAttrs->proteinId) > 0)
        prTdEnsIdAnchor(transAttrs->proteinId,
                        (isGrcH37Native(tdb) ? ensemblH37ProteinIdUrl: ensemblProteinIdUrl));
    else
        printf("<td>&nbsp;");
    printf("<td>");
    printf("</tr>\n");
    }

printf("<tr><th>HAVANA manual id");
printf("<td>%s", transAttrs->havanaTranscriptId);
printf("<td>%s", transAttrs->havanaGeneId);
printf("</tr>\n");

printf("<tr><th>Position");
printf("<td>");
writePosLink(transAnno->chrom, transAnno->txStart, transAnno->txEnd);
printf("<td>");
writePosLink(transAnno->chrom, geneChromStart, geneChromEnd);
printf("</tr>\n");

printf("<tr><th>Strand<td>%s<td></tr>\n", transAnno->strand);

printf("<tr><th><a href=\"%s\" target = _blank>Biotype</a><td>%s<td>%s</tr>\n", gencodeBiotypesUrl, transAttrs->transcriptType, transAttrs->geneType);

printf("<tr><th>Annotation Level<td>%s (%d)<td></tr>\n", getLevelDesc(transAttrs->level), transAttrs->level);

char *transSrcDesc = (transcriptSource != NULL) ? getMethodDesc(transcriptSource->source) : UNKNOWN;
char *geneSrcDesc = (geneSource != NULL) ? getMethodDesc(geneSource->source) : UNKNOWN;
printf("<tr><th>Annotation Method<td>%s<td>%s</tr>\n", transSrcDesc, geneSrcDesc);

if (haveTsl)
    {
    char *tslDesc = getSupportLevelDesc(tsl);
    printf("<tr><th><a href=\"#tsl\">Transcription Support Level</a><td><a href=\"#%s\">%s</a><td></tr>\n", tslDesc, tslDesc);
    }

if (isGrcHuman())
    writeHumanGeneLinkout(transAttrs, haveGeneSymbolSource, geneSymbolSource);
else
    writeMouseGeneLinkout(transAttrs, haveGeneSymbolSource, geneSymbolSource);


printf("<tr><th>CCDS<td>");
if (!isEmpty(transAttrs->ccdsId))
    {
    printf("<a href=\"");
    printCcdsExtUrl(transAttrs->ccdsId);
    printf("\" target=_blank>%s</a>", transAttrs->ccdsId);
    }
printf("<td></tr>\n");
if (transAttrs->transcriptRank > 0)
    {
    // older versions will have rank of zero
    printf("<tr><th>Transcript rank<td>%d<td></tr>\n",
           transAttrs->transcriptRank);
    }

if (isProteinCodingTrans(transAttrs))
    writeAprrisRow(conn, tdb, transAttrs, tags);

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
            printf("<a href=\"https://www.rcsb.org/structure/%s\" target=_blank>%s</a>", pdb->pdbId, pdb->pdbId);
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

static void writeEntrezGeneEntry(struct wgEncodeGencodeEntrezGene *entrezGene)
/* write HTML table entry for a entrezGene */
{
printf("<td width=\"33.33%%\"><a href=\"");
printEntrezGeneUrl(stdout, entrezGene->entrezGeneId);
printf("\" target=_blank>%d</a>", entrezGene->entrezGeneId);
}

static void writeEntrezGeneLinkHtml(struct wgEncodeGencodeEntrezGene *entrezGenes)
/* write HTML links to EntrezGene */
{
printf("<table class=\"hgcCcds\"><thead>\n");
printf("<tr><th colspan=\"3\">Entrez Gene</tr>\n");
printf("</thead><tbody>\n");
struct wgEncodeGencodeEntrezGene *entrezGene = entrezGenes;
int i, rowCnt = 0;
while ((entrezGene != NULL) || (rowCnt == 0))
    {
    printf("<tr>");
    for (i = 0; i < 3; i++)
        {
        if (entrezGene != NULL)
            {
            writeEntrezGeneEntry(entrezGene);
            entrezGene = entrezGene->next;
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

static void writeSupportEvidenceEntry(struct supportEvid *supportEvid)
/* write HTML table entry  for a supporting evidence */
{
// FIXME: should link to sources when possible
printf("<td width=\"25%%\">%s", supportEvid->seqSrc);
printf("<td width=\"25%%\">%s", supportEvid->seqId);
}

static void writeSupportingEvidenceLinkHtml(struct trackDb *tdb, char *gencodeId,
                                            struct wgEncodeGencodeTranscriptSupport *transcriptSupports,
                                            struct wgEncodeGencodeExonSupport *exonSupports)
/* write HTML links to supporting evidence */
{
struct supportEvid *supportEvids = loadSupportEvid(transcriptSupports, exonSupports);

printf("<table class=\"hgcCcds\"><thead>\n");
printf("<tr><th colspan=\"4\">Supporting Evidence (");
prEnsIdAnchor(gencodeId,
              (isGrcH37Native(tdb) ? ensemblH37SupportingEvidUrl: ensemblSupportingEvidUrl));
printf(")</tr>\n");
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
            writeSupportEvidenceEntry(supportEvid);
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
printf("<tr><th colspan=3><a href=\"%s\" target=_blank>Tags</a></tr>\n", gencodeTagsUrl);
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
struct wgEncodeGencodeGeneSource *geneSource = metaDataLoad(tdb, conn, gencodeGeneId, "wgEncodeGencodeGeneSource", "geneId", sqlQuerySingle, (sqlLoadFunc)wgEncodeGencodeGeneSourceLoad);
struct wgEncodeGencodeTranscriptSource *transcriptSource = metaDataLoad(tdb, conn, gencodeId, "wgEncodeGencodeTranscriptSource", "transcriptId", sqlQuerySingle, (sqlLoadFunc)wgEncodeGencodeTranscriptSourceLoad);
bool haveRemarks = haveGencodeTable(conn, tdb, "wgEncodeGencodeAnnotationRemark");
struct wgEncodeGencodeAnnotationRemark *remarks = haveRemarks ? metaDataLoad(tdb, conn, gencodeId, "wgEncodeGencodeAnnotationRemark", "transcriptId", 0, (sqlLoadFunc)wgEncodeGencodeAnnotationRemarkLoad) : NULL;
struct wgEncodeGencodePdb *pdbs = metaDataLoad(tdb, conn, gencodeId, "wgEncodeGencodePdb", "transcriptId", sqlQueryMulti, (sqlLoadFunc)wgEncodeGencodePdbLoad);
struct wgEncodeGencodePubMed *pubMeds = metaDataLoad(tdb, conn, gencodeId, "wgEncodeGencodePubMed", "transcriptId", sqlQueryMulti, (sqlLoadFunc)wgEncodeGencodePubMedLoad);
bool haveEntrezGene = haveGencodeTable(conn, tdb, "wgEncodeGencodeEntrezGene");
struct wgEncodeGencodeEntrezGene *entrezGenes = haveEntrezGene ? metaDataLoad(tdb, conn, gencodeId, "wgEncodeGencodeEntrezGene", "transcriptId", sqlQueryMulti, (sqlLoadFunc)wgEncodeGencodeEntrezGeneLoad) : NULL;
struct wgEncodeGencodeRefSeq *refSeqs = metaDataLoad(tdb, conn, gencodeId, "wgEncodeGencodeRefSeq", "transcriptId", sqlQueryMulti, (sqlLoadFunc)wgEncodeGencodeRefSeqLoad);
struct wgEncodeGencodeTag *tags = metaDataLoad(tdb, conn, gencodeId, "wgEncodeGencodeTag", "transcriptId", sqlQueryMulti, (sqlLoadFunc)wgEncodeGencodeTagLoad);
struct wgEncodeGencodeTranscriptSupport *transcriptSupports = metaDataLoad(tdb, conn, gencodeId, "wgEncodeGencodeTranscriptSupport", "transcriptId", sqlQueryMulti, (sqlLoadFunc)wgEncodeGencodeTranscriptSupportLoad);
struct wgEncodeGencodeExonSupport *exonSupports = NULL;
// exonSupports not available in back mapped GENCODE releases
if (haveGencodeTable(conn, tdb, "wgEncodeGencodeExonSupport"))
    exonSupports = metaDataLoad(tdb, conn, gencodeId, "wgEncodeGencodeExonSupport", "transcriptId", sqlQueryMulti, (sqlLoadFunc)wgEncodeGencodeExonSupportLoad);
struct wgEncodeGencodeUniProt *uniProts = metaDataLoad(tdb, conn, gencodeId, "wgEncodeGencodeUniProt", "transcriptId", sqlQueryMulti, (sqlLoadFunc)wgEncodeGencodeUniProtLoad);
slSort(&uniProts, uniProtDatasetCmp);
bool haveTsl = haveGencodeTable(conn, tdb, "wgEncodeGencodeTranscriptionSupportLevel");
struct wgEncodeGencodeTranscriptionSupportLevel *tsl = haveTsl ? metaDataLoad(tdb, conn, gencodeId, "wgEncodeGencodeTranscriptionSupportLevel", "transcriptId", 0, (sqlLoadFunc)wgEncodeGencodeTranscriptionSupportLevelLoad) : NULL;
boolean haveGeneSymbolSource = haveGencodeTable(conn, tdb, "wgEncodeGencodeGeneSymbol");
struct wgEncodeGencodeGeneSymbol *geneSymbolSource = haveGeneSymbolSource ? metaDataLoad(tdb, conn, gencodeId, "wgEncodeGencodeGeneSymbol", "transcriptId", 0, (sqlLoadFunc)wgEncodeGencodeGeneSymbolLoad) : NULL;
    
int geneChromStart, geneChromEnd;
getGeneBounds(tdb, conn, transAnno, &geneChromStart, &geneChromEnd);

char title[256];
safef(title, sizeof(title), "GENCODE V%s Transcript Annotation", gencodeGetVersion(tdb));
char header[256];
safef(header, sizeof(header), "%s %s", title, gencodeId);
if (!isEmpty(transAttrs->geneName))
    safef(header, sizeof(header), "%s %s (%s)", title, gencodeId, transAttrs->geneName);
else
    safef(header, sizeof(header), "%s %s", title, gencodeId);
cartWebStart(cart, database, "%s", header);
printf("<H2>%s</H2>\n", header);

writeBasicInfoHtml(conn, tdb, gencodeId, transAnno, transAttrs, geneChromStart, geneChromEnd, geneSource, transcriptSource, tags,
                   haveTsl, tsl, haveGeneSymbolSource, geneSymbolSource);
writeTagLinkHtml(tags);
writeSequenceHtml(tdb, gencodeId, transAnno);
if (haveRemarks)
    writeAnnotationRemarkHtml(remarks);
if (isProteinCodingTrans(transAttrs))
    writePdbLinkHtml(pdbs);
writePubMedLinkHtml(pubMeds);
if (haveEntrezGene)
    writeEntrezGeneLinkHtml(entrezGenes);
writeRefSeqLinkHtml(refSeqs);
if (isProteinCodingTrans(transAttrs))
    writeUniProtLinkHtml(uniProts);
writeSupportingEvidenceLinkHtml(tdb, gencodeId, transcriptSupports, exonSupports);

wgEncodeGencodeAttrsFree(&transAttrs);
wgEncodeGencodeAnnotationRemarkFreeList(&remarks);
wgEncodeGencodeGeneSourceFreeList(&geneSource);
wgEncodeGencodeTranscriptSourceFreeList(&transcriptSource);
wgEncodeGencodePdbFreeList(&pdbs);
wgEncodeGencodePubMedFreeList(&pubMeds);
wgEncodeGencodeEntrezGeneFreeList(&entrezGenes);
wgEncodeGencodeRefSeqFreeList(&refSeqs);
wgEncodeGencodeTranscriptSupportFreeList(&transcriptSupports);
wgEncodeGencodeExonSupportFreeList(&exonSupports);
wgEncodeGencodeUniProtFreeList(&uniProts);
wgEncodeGencodeGeneSymbolFreeList(&geneSymbolSource);
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

genePredFreeList(&anno);
hFreeConn(&conn);
}


bool isNewGencodeGene(struct trackDb *tdb)
/* is this a new-style gencode (>= V7) track, as indicated by
 * the presence of the wgEncodeGencodeVersion setting */
{
return trackDbSetting(tdb, "wgEncodeGencodeVersion") != NULL;
}
