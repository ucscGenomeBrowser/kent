/* gencodeClick - click handling for GENCODE tracks */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "hgc.h"
#include "gencodeClick.h"
#include "ccdsClick.h"
#include "genePred.h"
#include "genePredReader.h"
#include "ensFace.h"
#include "htmshell.h"
#include "jksql.h"
#include "regexHelper.h"
#include "gencodeAttrs.h"
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
static char *hgncSymbolUrl = "https://www.genenames.org/data/gene-symbol-report/#!/symbol/%s";
static char *hgncIdUrl = "https://www.genenames.org/data/gene-symbol-report/#!/hgnc_id/%s";
static char *mgiSymbolUrl = "http://www.informatics.jax.org/searchtool/Search.do?query=%s";
static char *mgiIdUrl = "http://www.informatics.jax.org/accession/%s";
static char *geneCardsUrl = "http://www.genecards.org/cgi-bin/carddisp.pl?gene=%s";
static char *apprisHomeUrl = "http://appris-tools.org/";
static char *apprisGeneUrl = "http://appris-tools.org/#/database/id/%s/%s?sc=ensembl";

static char* UNKNOWN = "unknown";

static char *getBaseAcc(char *acc, char *accBuf, int accBufSize)
/* get the accession with version number dropped. */
{
safecpy(accBuf, accBufSize, acc);
char *dot = strchr(accBuf, '.');
if (dot != NULL)
    *dot = '\0';
return accBuf;
}

static char* getGencodeOldName(char *baseName)
/* get the old variable or table name starting with wgEncodeGencode as opposed
 * to 'gencode'.  WARNING: static return. */
{
static char buf[256];
safef(buf, sizeof(buf), "wgEncode%s", baseName);
toUpperN(buf + 8, 1);  // g -> G
return buf;
}
  
static char *getGencodeTableVar(struct trackDb *tdb, char *tableBase)
/* return value table variable name, of NULL if in trackDb */
{
char* tbl = trackDbSetting(tdb, tableBase);
if (tbl == NULL)
    tbl = trackDbSetting(tdb, getGencodeOldName(tableBase));
return tbl;
}

static bool haveGencodeTable(struct trackDb *tdb, char *tableBase)
/* determine if table is in settings and thus in this gencode release */
{
return getGencodeTableVar(tdb, tableBase) != NULL;
}

static char *getGencodeTable(struct trackDb *tdb, char *tableBase)
/* get a table name from the settings or error. */
{
char* tbl = getGencodeTableVar(tdb, tableBase);
if (tbl == NULL)
    errAbort("Missing table variable for track %s (%s or %s)",
             tdb->track, tableBase, getGencodeOldName(tableBase));
return tbl;
}

static char* getGencodeVersion(struct trackDb *tdb)
/* get the GENCODE version or NULL for < V7, which is not supported
 * by this module. */
{
return getGencodeTableVar(tdb, "gencodeVersion");
}

static boolean isHuman()
/* are we human (otherwise mouse) */
{
return startsWith("hg", database);
}


static boolean isGrcH37Native(struct trackDb *tdb)
/* Is this GENCODE GRCh37 native build, which requires a different Ensembl site. */
{
// check for non-lifted GENCODE on GRCh37/hg19
if (sameString(database, "hg19"))
    return stringIn("lift37", getGencodeVersion(tdb)) == NULL;
else
    return FALSE;
}

static boolean isFakeGeneSymbol(char* sym)
/* is this a static gene symbol? */
{
static const char *regexp = "^AC[0-9]+\\.[0-9]+$";
return regexMatch(sym, regexp);
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
sqlSafefFrag(where, sizeof(where), "(chrom = \"%s\") and (name = \"%s\")", seqName, gencodeId);
struct genePred *transAnno = genePredReaderLoadQuery(conn, tdb->track, where);
slSort(&transAnno, transAnnoCmp);
return transAnno;
}

static struct wgEncodeGencodeAttrs *transAttrsOldLoad(struct sqlResult *sr, char** row)
/* load attributes from the wgEncodeGencodeAttrs format table */
{
// older version don't have proteinId column.
struct wgEncodeGencodeAttrs *transAttrs = wgEncodeGencodeAttrsLoad(row, sqlCountColumns(sr));
return transAttrs;
}

static struct wgEncodeGencodeAttrs *transAttrsNewLoad(char** row)
/* load attributes from the gencodeAttrs format table, converting to the older format */
{
struct gencodeAttrs *transAttrs = gencodeAttrsLoad(row);
struct wgEncodeGencodeAttrs *transAttrsOld;
AllocVar(transAttrsOld);

transAttrsOld->geneId = transAttrs->geneId;
transAttrsOld->geneName = transAttrs->geneName;
transAttrsOld->geneType = transAttrs->geneType;
transAttrsOld->geneStatus = cloneString("");
transAttrsOld->transcriptId = transAttrs->transcriptId;
transAttrsOld->transcriptName = transAttrs->transcriptName;
transAttrsOld->transcriptType = transAttrs->transcriptType;
transAttrsOld->transcriptStatus = cloneString("");
transAttrsOld->havanaGeneId = cloneString("");
transAttrsOld->havanaTranscriptId = cloneString("");
transAttrsOld->ccdsId = transAttrs->ccdsId;
transAttrsOld->level = transAttrs->level;
transAttrsOld->transcriptClass = transAttrs->transcriptClass;
transAttrsOld->proteinId = transAttrs->proteinId;
return transAttrsOld;
}

static struct wgEncodeGencodeAttrs *transAttrsLoad(struct trackDb *tdb, struct sqlConnection *conn, char *gencodeId)
/* load the gencode attributes */
{
char query[1024];
sqlSafef(query, sizeof(query), "select * from %s where transcriptId = \"%s\"",
         getGencodeTable(tdb, "gencodeAttrs"), gencodeId);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row = sqlNextRow(sr);
if (row == NULL)
    errAbort("gencode transcript %s not found in %s", gencodeId,
             getGencodeTable(tdb, "gencodeAttrs"));
// older version don't have proteinId column.
struct wgEncodeGencodeAttrs *transAttrs;
if (sqlFieldColumn(sr, "havanaGeneId") >= 0)
    transAttrs = transAttrsOldLoad(sr, row);
else
    transAttrs = transAttrsNewLoad(row);
sqlFreeResult(&sr);
return transAttrs;
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

static void *metaDataLoadOptional(struct trackDb *tdb, struct sqlConnection *conn, char *gencodeId, char *tableBase, char *keyCol, unsigned queryOpts, sqlLoadFunc loadFunc)
/* Load autoSql objects for gencode meta data from an optional gencode table. Probably older assembly  */
{
if (haveGencodeTable(tdb, tableBase))
    return metaDataLoad(tdb, conn, gencodeId, tableBase, keyCol, queryOpts, loadFunc);
else
    return NULL;
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

static void prExtIdAnchor(char *name, char *id, char *urlTemplate)
/* if an id to an external database is not empty, print an HTML anchor to it */
{
if (!isEmpty(id))
    {
    char urlBuf[512];
    safef(urlBuf, sizeof(urlBuf), urlTemplate, id);
    printf("<a href=\"%s\" target=_blank>%s</a>", urlBuf, name);
    }
}

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
printf(urlTemplate, speciesArg, getBaseAcc(id, accBuf, sizeof(accBuf)));
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
sqlSafefFrag(query, sizeof(query),
      "%s tag where tag.tag like 'appris%%' and transcriptId in "
      "(select transcriptId from %s where geneId='%s')",
      getGencodeTable(tdb, "gencodeTag"),
      getGencodeTable(tdb, "gencodeAttrs"),
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

// some labels for gene symbol rows
static char *hgncSrcLabel = "HGNC gene symbol";
static char *mgiSrcLabel = "MGI gene symbol";
static char *genericSrcLabel = "Gene symbol";
static char *fakeSrcLabel = "Generated gene symbol";

static void writeGeneSymbolRowsOld(struct wgEncodeGencodeAttrs *transAttrs)
/* Output HTML rows for gene symbol for older GENCODE that doesn't have HGNC|MGI ids */
{
char *srcLabel = isFakeGeneSymbol(transAttrs->geneName) ? fakeSrcLabel
    : (isHuman() ? hgncSrcLabel : mgiSrcLabel);

printf("<tr><th>%s<td colspan=2>", srcLabel);
if (isFakeGeneSymbol(transAttrs->geneName))
    puts(transAttrs->geneName);
else
    prExtIdAnchor(transAttrs->geneName, transAttrs->geneName, (isHuman() ? hgncSymbolUrl : mgiSymbolUrl)) ;
printf("</tr>\n");
}

static void writeGeneSymbolRowsNew(struct wgEncodeGencodeAttrs *transAttrs,
                                  struct wgEncodeGencodeGeneSymbol *geneSymbol)
/* Output HTML rows for gene symbol for new GENCODE that have HGNC|MGI ids */
{
boolean isFake = (geneSymbol == NULL) && isFakeGeneSymbol(transAttrs->geneName); // be sure it is fake
char *srcLabel;
if (geneSymbol != NULL)
    srcLabel = (isHuman() ? hgncSrcLabel : mgiSrcLabel);
else if (isFake)
    srcLabel = fakeSrcLabel;
else
    srcLabel = genericSrcLabel;
printf("<tr><th>%s<td colspan=2>", srcLabel);
if (geneSymbol != NULL)
    prExtIdAnchor(geneSymbol->symbol, geneSymbol->geneId,
                  isHuman() ? hgncIdUrl : mgiIdUrl);
else
    puts(transAttrs->geneName);
printf("</tr>\n");
}

static void writeGeneSymbolRows(struct sqlConnection *conn,
                                struct trackDb *tdb,
                                struct wgEncodeGencodeAttrs *transAttrs)
/* Output HTML row for gene symbol if it makes sense */
{
if (haveGencodeTable(tdb, "gencodeGeneSymbol"))
    {
    struct wgEncodeGencodeGeneSymbol *geneSymbol = metaDataLoad(tdb, conn, transAttrs->transcriptId, "gencodeGeneSymbol", "transcriptId", 0, (sqlLoadFunc)wgEncodeGencodeGeneSymbolLoad);
    writeGeneSymbolRowsNew(transAttrs, geneSymbol);
    wgEncodeGencodeGeneSymbolFreeList(&geneSymbol);
    }
else
    writeGeneSymbolRowsOld(transAttrs);
}

static void writeBasicInfoHtml(struct sqlConnection *conn, struct trackDb *tdb, char *gencodeId, struct genePred *transAnno,
                               struct wgEncodeGencodeAttrs *transAttrs,
                               int geneChromStart, int geneChromEnd,
                               struct wgEncodeGencodeGeneSource *geneSource, struct wgEncodeGencodeTranscriptSource *transcriptSource,
                               struct wgEncodeGencodeTag *tags, struct wgEncodeGencodeTranscriptionSupportLevel *tsl)
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

if (transAttrs->proteinId != NULL)
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

if (strlen(transAttrs->havanaTranscriptId) > 0)
    {
    printf("<tr><th>HAVANA manual id");
    printf("<td>%s", transAttrs->havanaTranscriptId);
    printf("<td>%s", transAttrs->havanaGeneId);
    printf("</tr>\n");
    }

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

if (tsl != NULL)
    {
    char *tslDesc = getSupportLevelDesc(tsl);
    printf("<tr><th><a href=\"#tsl\">Transcription Support Level</a><td><a href=\"#%s\">%s</a><td></tr>\n", tslDesc, tslDesc);
    }
writeGeneSymbolRows(conn, tdb, transAttrs);

printf("<tr><th>CCDS<td>");
if (!isEmpty(transAttrs->ccdsId))
    {
    printf("<a href=\"");
    printCcdsExtUrl(transAttrs->ccdsId);
    printf("\" target=_blank>%s</a>", transAttrs->ccdsId);
    }
printf("<td></tr>\n");

if (isHuman())
    {
    printf("<tr><th>GeneCards<td colspan=2>");
    if (!isFakeGeneSymbol(transAttrs->geneName))
        prExtIdAnchor(transAttrs->geneName, transAttrs->geneName, geneCardsUrl);
    printf("</tr>\n");
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
struct wgEncodeGencodeGeneSource *geneSource = metaDataLoad(tdb, conn, gencodeGeneId, "gencodeGeneSource", "geneId", sqlQuerySingle, (sqlLoadFunc)wgEncodeGencodeGeneSourceLoad);
struct wgEncodeGencodeTranscriptSource *transcriptSource = metaDataLoad(tdb, conn, gencodeId, "gencodeTranscriptSource", "transcriptId", sqlQuerySingle, (sqlLoadFunc)wgEncodeGencodeTranscriptSourceLoad);
struct wgEncodeGencodeAnnotationRemark *remarks = metaDataLoadOptional(tdb, conn, gencodeId, "gencodeAnnotationRemark", "transcriptId", 0, (sqlLoadFunc)wgEncodeGencodeAnnotationRemarkLoad);
struct wgEncodeGencodePdb *pdbs = metaDataLoad(tdb, conn, gencodeId, "gencodePdb", "transcriptId", sqlQueryMulti, (sqlLoadFunc)wgEncodeGencodePdbLoad);
struct wgEncodeGencodePubMed *pubMeds = metaDataLoad(tdb, conn, gencodeId, "gencodePubMed", "transcriptId", sqlQueryMulti, (sqlLoadFunc)wgEncodeGencodePubMedLoad);
struct wgEncodeGencodeEntrezGene *entrezGenes = metaDataLoadOptional(tdb, conn, gencodeId, "gencodeEntrezGene", "transcriptId", sqlQueryMulti, (sqlLoadFunc)wgEncodeGencodeEntrezGeneLoad);
struct wgEncodeGencodeRefSeq *refSeqs = metaDataLoad(tdb, conn, gencodeId, "gencodeRefSeq", "transcriptId", sqlQueryMulti, (sqlLoadFunc)wgEncodeGencodeRefSeqLoad);
struct wgEncodeGencodeTag *tags = metaDataLoad(tdb, conn, gencodeId, "gencodeTag", "transcriptId", sqlQueryMulti, (sqlLoadFunc)wgEncodeGencodeTagLoad);
struct wgEncodeGencodeTranscriptSupport *transcriptSupports = metaDataLoad(tdb, conn, gencodeId, "gencodeTranscriptSupport", "transcriptId", sqlQueryMulti, (sqlLoadFunc)wgEncodeGencodeTranscriptSupportLoad);
struct wgEncodeGencodeExonSupport *exonSupports = metaDataLoadOptional(tdb, conn, gencodeId, "gencodeExonSupport", "transcriptId", sqlQueryMulti, (sqlLoadFunc)wgEncodeGencodeExonSupportLoad);
struct wgEncodeGencodeUniProt *uniProts = metaDataLoad(tdb, conn, gencodeId, "gencodeUniProt", "transcriptId", sqlQueryMulti, (sqlLoadFunc)wgEncodeGencodeUniProtLoad);
slSort(&uniProts, uniProtDatasetCmp);
struct wgEncodeGencodeTranscriptionSupportLevel *tsl = metaDataLoadOptional(tdb, conn, gencodeId, "gencodeTranscriptionSupportLevel", "transcriptId", 0, (sqlLoadFunc)wgEncodeGencodeTranscriptionSupportLevelLoad);


int geneChromStart, geneChromEnd;
getGeneBounds(tdb, conn, transAnno, &geneChromStart, &geneChromEnd);

char title[256];
safef(title, sizeof(title), "GENCODE V%s Transcript Annotation", getGencodeVersion(tdb));
char header[256];
safef(header, sizeof(header), "%s %s", title, gencodeId);
if (!isEmpty(transAttrs->geneName))
    safef(header, sizeof(header), "%s %s (%s)", title, gencodeId, transAttrs->geneName);
else
    safef(header, sizeof(header), "%s %s", title, gencodeId);
cartWebStart(cart, database, "%s", header);
printf("<H2>%s</H2>\n", header);

writeBasicInfoHtml(conn, tdb, gencodeId, transAnno, transAttrs, geneChromStart, geneChromEnd, geneSource, transcriptSource, tags, tsl);
writeTagLinkHtml(tags);
writeSequenceHtml(tdb, gencodeId, transAnno);
writeAnnotationRemarkHtml(remarks);
if (isProteinCodingTrans(transAttrs))
    writePdbLinkHtml(pdbs);
writePubMedLinkHtml(pubMeds);
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
prExtIdAnchor(gencodeId, gencodeId, yalePseudoUrl);
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

static boolean isGenecodeTrack(char* tableBase,
                               struct trackDb *tdb)
/* is this an gencode track? */
{
return startsWith(tableBase, tdb->track) ||
    startsWith(getGencodeOldName(tableBase), tdb->track);
}

void doGencodeGene(struct trackDb *tdb, char *gencodeId)
/* Process click on a GENCODE annotation. */
{
struct sqlConnection *conn = hAllocConn(database);
struct genePred *anno = transAnnoLoad(conn, tdb, gencodeId);
if (isGenecodeTrack("gencodeBasic", tdb)
    || isGenecodeTrack("gencodeComp", tdb)
    || isGenecodeTrack("gencodeAnnot", tdb)
    || isGenecodeTrack("gencodePseudoGene", tdb))
    doGencodeGeneTrack(tdb, gencodeId, conn, anno);
else if (isGenecodeTrack("gencode2wayConsPseudo", tdb))
    doGencodeGene2WayPseudo(tdb, gencodeId, conn, anno);
else if (isGenecodeTrack("gencodePolya", tdb))
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
return getGencodeVersion(tdb) != NULL;
}
