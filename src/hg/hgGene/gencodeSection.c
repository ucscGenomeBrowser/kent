/* gencodeSection - click handling for GENCODE tracks */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "hdb.h"
#include "hCommon.h"
#include "hgGene.h"
#include "gencodeSection.h"
#include "genePred.h"
#include "genePredReader.h"
#include "ensFace.h"
#include "htmshell.h"
#include "jksql.h"
#include "regexHelper.h"
#include "gencodeAttrs.h"
#include "gencodeToUniProt.h"
#include "gencodeTranscriptionSupportLevel.h"
#include "gencodeTag.h"
#include "gencodeGeneSource.h"
#include "gencodeToPdb.h"
#include "gencodeToPubMed.h"
#include "gencodeToRefSeq.h"
#include "gencodeTranscriptSource.h"
#include "gencodeTranscriptSupport.h"
#include "gencodeExonSupport.h"
#include "gencodeToUniProt.h"
#include "gencodeToEntrezGene.h"
#include "gencodeAnnotationRemark.h"
#include "gencodeTranscriptionSupportLevel.h"

char *entrezUidFormat = "https://www.ncbi.nlm.nih.gov/entrez/query.fcgi?cmd=Retrieve&db=%s&list_uids=%d&dopt=%s&tool=genome.ucsc.edu";

static void printEntrezPubMedUidUrl(FILE *f, int pmid)
/* Print URL for Entrez browser on a PubMed search. */
{
fprintf(f, entrezUidFormat, "PubMed", pmid, "Summary");
}

static void printEntrezGeneUrl(FILE *f, int geneid)
/* Print URL for Entrez browser on a gene details page. */
{
fprintf(f, entrezUidFormat, "gene", geneid, "Graphics");
}

static void printCcdsExtUrl(char *ccdsId)
/* Print out URL to link to CCDS database at NCBI */
{
printf("https://www.ncbi.nlm.nih.gov/CCDS/CcdsBrowse.cgi?REQUEST=CCDS&BUILDS=ALLBUILDS&DATA=%s", ccdsId);
}

static char *hgTracksPathAndSettings()
/* Return path with hgTracks CGI path and session state variable. */
{
static struct dyString *dy = NULL;
if (dy == NULL)
    {
    dy = dyStringNew(128);
    dyStringPrintf(dy, "%s?%s", hgTracksName(), cartSidUrlString(cart));
    }
return dy->string;
}

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

//static char *yalePseudoUrl = "http://tables.pseudogene.org/%s";
static char *hgncUrl = " https://www.genenames.org/data/gene-symbol-report/#!/symbol/%s";
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

static bool haveGencodeTable(struct trackDb *tdb, char *tableBase)
/* determine if table is in settings and thus in this gencode release */
{
return trackDbSetting(tdb, tableBase) != NULL;
}

static char *getGencodeTable(struct trackDb *tdb, char *tableBase)
/* get a table name from the settings. */
{
char buffer[strlen(tableBase) + 10];
// the version number is after the table name base
char *tableVersion = &tdb->table[strlen("gencodeAnnot")];
safef(buffer, sizeof buffer, "%s%s",tableBase,tableVersion);
return cloneString(buffer);
}

static char* getGencodeVersion(struct trackDb *tdb)
/* get the GENCODE version or NULL for < V7, which is not supported
 * by this module. */
{
return trackDbSetting(tdb, "gencodeVersion");
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
if (sameString(a->name, curGeneChrom))
    return -1;
else if (sameString(b->name, curGeneChrom))
    return 1;
else
    return strcmp(a->name, b->name);
}

static bool isProteinCodingTrans(struct gencodeAttrs *transAttrs)
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
sqlSafef(where, sizeof(where), "(chrom = \"%s\") and (name = \"%s\")", curGeneChrom, gencodeId);
struct genePred *transAnno = genePredReaderLoadQuery(conn, tdb->table, where);
slSort(&transAnno, transAnnoCmp);
return transAnno;
}

static struct gencodeAttrs *transAttrsLoad(struct trackDb *tdb, struct sqlConnection *conn, char *gencodeId)
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
//struct gencodeAttrs *transAttrs = gencodeAttrsLoad(row, sqlCountColumns(sr));
struct gencodeAttrs *transAttrs = gencodeAttrsLoad(row);
sqlFreeResult(&sr);
return transAttrs;
}

static void getGeneBounds(struct trackDb *tdb, struct sqlConnection *conn, struct genePred *transAnno,
                          int *geneChromStart, int *geneChromEnd)
/* find bounds for the gene */
{
// must check chrom due to PAR
char where[256];
sqlSafef(where, sizeof(where), "(chrom = \"%s\") and (name2 = \"%s\")", curGeneChrom, transAnno->name2);
struct genePred *geneAnnos = genePredReaderLoadQuery(conn, tdb->table, where);
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
/* Compare gencodeToUniProt by dateset */
{
const struct gencodeToUniProt *a = *((struct gencodeToUniProt **)va);
const struct gencodeToUniProt *b = *((struct gencodeToUniProt **)vb);
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

static char *getSupportLevelDesc(struct gencodeTranscriptionSupportLevel *tsl)
/* return description for level */
{
static char buf[32];
if ((tsl == NULL) || (tsl->level <= 0))
    return "tslNA";
else
    {
    safef(buf, sizeof(buf), "%s", tsl->level);
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

static bool geneHasApprisTranscripts(struct trackDb *tdb, struct sqlConnection *conn, struct gencodeAttrs *transAttrs)
/* check if any transcript in a gene has an APPRIS tags */
{
char query[1024];
sqlSafef(query, sizeof(query),
      "%s tag where tag.tag like 'appris%%' and transcriptId in "
      "(select transcriptId from %s where geneId='%s')",
      getGencodeTable(tdb, "gencodeTag"),
      getGencodeTable(tdb, "gencodeAttrs"),
      transAttrs->geneId);
return sqlRowCount(conn, query) > 0;
}

static char* findApprisTag(struct gencodeTag *tags)
/* search list for APPRIS tag or NULL */
{
struct gencodeTag *tag;
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
                           struct gencodeAttrs *transAttrs,
                           struct gencodeTag *tags)
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

static void writeBasicInfoHtml(struct sqlConnection *conn, struct trackDb *tdb, char *gencodeId, struct genePred *transAnno,
                               struct gencodeAttrs *transAttrs,
                               int geneChromStart, int geneChromEnd,
                               struct gencodeGeneSource *geneSource, struct gencodeTranscriptSource *transcriptSource,
                               struct gencodeTag *tags, bool haveTsl, struct gencodeTranscriptionSupportLevel *tsl)
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

#ifdef NOTW
printf("<tr><th>HAVANA manual id");
printf("<td>%s", transAttrs->havanaTranscriptId);
printf("<td>%s", transAttrs->havanaGeneId);
printf("</tr>\n");
#endif

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
printf("<tr><th>HGNC gene symbol<td colspan=2>");
if (!isFakeGeneSymbol(transAttrs->geneName))
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
if (!isFakeGeneSymbol(transAttrs->geneName))
    prExtIdAnchor(transAttrs->geneName, geneCardsUrl);
printf("</tr>\n");

if (isProteinCodingTrans(transAttrs))
    writeAprrisRow(conn, tdb, transAttrs, tags);

// FIXME: add sequence here??
printf("</tbody></table>\n");
}

static void writeAnnotationRemarkHtml(struct gencodeAnnotationRemark *remarks)
/* write HTML links to remarks */
{
printf("<table class=\"hgcCcds\"><thead>\n");
printf("<tr><th colspan=\"1\">Annotation Remarks</tr>\n");
printf("</thead><tbody>\n");
// make sure at least one empty row in printed
if (remarks == NULL)
    printf("<tr><td></td></tr>\n");
struct gencodeAnnotationRemark *remark;
for (remark = remarks; remark != NULL; remark = remark->next)
    {
    char *encRemark = htmlEncode(remark->remark);
    printf("<tr><td>%s</td></tr>\n", encRemark);
    freeMem(encRemark);
    }
printf("</tbody></table>\n");
}

static void writePdbLinkHtml(struct gencodeToPdb *pdbs)
/* write HTML links to PDB */
{
printf("<table class=\"hgcCcds\"><thead>\n");
printf("<tr><th colspan=\"3\">Protein Data Bank</tr>\n");
printf("</thead><tbody>\n");
struct gencodeToPdb *pdb = pdbs;
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

static void writePubMedEntry(struct gencodeToPubMed *pubMed)
/* write HTML table entry for a pubMed */
{
printf("<td width=\"33.33%%\"><a href=\"");
printEntrezPubMedUidUrl(stdout, pubMed->pubMedId);
printf("\" target=_blank>%d</a>", pubMed->pubMedId);
}

static void writePubMedLinkHtml(struct gencodeToPubMed *pubMeds)
/* write HTML links to PubMed */
{
printf("<table class=\"hgcCcds\"><thead>\n");
printf("<tr><th colspan=\"3\">PubMed</tr>\n");
printf("</thead><tbody>\n");
struct gencodeToPubMed *pubMed = pubMeds;
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

static void writeEntrezGeneEntry(struct gencodeToEntrezGene *entrezGene)
/* write HTML table entry for a entrezGene */
{
printf("<td width=\"33.33%%\"><a href=\"");
printEntrezGeneUrl(stdout, entrezGene->entrezGeneId);
printf("\" target=_blank>%d</a>", entrezGene->entrezGeneId);
}

static void writeEntrezGeneLinkHtml(struct gencodeToEntrezGene *entrezGenes)
/* write HTML links to EntrezGene */
{
printf("<table class=\"hgcCcds\"><thead>\n");
printf("<tr><th colspan=\"3\">Entrez Gene</tr>\n");
printf("</thead><tbody>\n");
struct gencodeToEntrezGene *entrezGene = entrezGenes;
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

static void transcriptSupportToSupportEvid(struct supportEvid **supportEvids, struct gencodeTranscriptSupport *transcriptSupports)
/* convert transcriptSupport to common structure */
{
struct gencodeTranscriptSupport *transcriptSupport;
for (transcriptSupport = transcriptSupports; transcriptSupport != NULL; transcriptSupport = transcriptSupport->next)
    {
    struct supportEvid *supportEvid;
    AllocVar(supportEvid);
    supportEvid->seqId = transcriptSupport->seqId;
    supportEvid->seqSrc = transcriptSupport->seqSrc;
    slAddHead(supportEvids, supportEvid);
    }
}

static void exonSupportToSupportEvid(struct supportEvid **supportEvids, struct gencodeExonSupport *exonSupports)
/* convert exonSupport to common structure */
{
struct gencodeExonSupport *exonSupport;
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

static struct supportEvid *loadSupportEvid(struct gencodeTranscriptSupport *transcriptSupports,
                                           struct gencodeExonSupport *exonSupports)
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
                                            struct gencodeTranscriptSupport *transcriptSupports,
                                            struct gencodeExonSupport *exonSupports)
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

static void writeTagEntry(struct gencodeTag *tag)
/* write HTML table entry for a Tag */
{
// FIXME: link to help once gencodegenes.org has it
printf("<td width=\"33.33%%\">%s", tag->tag);
}

static void writeTagLinkHtml(struct gencodeTag *tags)
/* write HTML links to Tag */
{
printf("<table class=\"hgcCcds\"><thead>\n");
printf("<tr><th colspan=3><a href=\"%s\" target=_blank>Tags</a></tr>\n", gencodeTagsUrl);
printf("</thead><tbody>\n");
int i, rowCnt = 0;
struct gencodeTag *tag = tags;
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
struct gencodeAttrs *transAttrs = transAttrsLoad(tdb, conn, gencodeId);
char *gencodeGeneId = transAttrs->geneId;
struct gencodeGeneSource *geneSource = metaDataLoad(tdb, conn, gencodeGeneId, "gencodeGeneSource", "geneId", sqlQuerySingle, (sqlLoadFunc)gencodeGeneSourceLoad);
struct gencodeTranscriptSource *transcriptSource = metaDataLoad(tdb, conn, gencodeId, "gencodeTranscriptSource", "transcriptId", sqlQuerySingle, (sqlLoadFunc)gencodeTranscriptSourceLoad);
bool haveRemarks = haveGencodeTable(tdb, "gencodeAnnotationRemark");
struct gencodeAnnotationRemark *remarks = haveRemarks ? metaDataLoad(tdb, conn, gencodeId, "gencodeAnnotationRemark", "transcriptId", 0, (sqlLoadFunc)gencodeAnnotationRemarkLoad) : NULL;
struct gencodeToPdb *pdbs = metaDataLoad(tdb, conn, gencodeId, "gencodeToPdb", "transcriptId", sqlQueryMulti, (sqlLoadFunc)gencodeToPdbLoad);
struct gencodeToPubMed *pubMeds = metaDataLoad(tdb, conn, gencodeId, "gencodeToPubMed", "transcriptId", sqlQueryMulti, (sqlLoadFunc)gencodeToPubMedLoad);
bool haveEntrezGene = haveGencodeTable(tdb, "gencodeToEntrezGene");
struct gencodeToEntrezGene *entrezGenes = haveEntrezGene ? metaDataLoad(tdb, conn, gencodeId, "gencodeToEntrezGene", "transcriptId", sqlQueryMulti, (sqlLoadFunc)gencodeToEntrezGeneLoad) : NULL;
struct gencodeToRefSeq *refSeqs = metaDataLoad(tdb, conn, gencodeId, "gencodeToRefSeq", "transcriptId", sqlQueryMulti, (sqlLoadFunc)gencodeToRefSeqLoad);
struct gencodeTag *tags = metaDataLoad(tdb, conn, gencodeId, "gencodeTag", "transcriptId", sqlQueryMulti, (sqlLoadFunc)gencodeTagLoad);
struct gencodeTranscriptSupport *transcriptSupports = metaDataLoad(tdb, conn, gencodeId, "gencodeTranscriptSupport", "transcriptId", sqlQueryMulti, (sqlLoadFunc)gencodeTranscriptSupportLoad);
struct gencodeExonSupport *exonSupports = NULL;
// exonSupports not available in back mapped GENCODE releases
if (haveGencodeTable(tdb, "gencodeExonSupport"))
    exonSupports = metaDataLoad(tdb, conn, gencodeId, "gencodeExonSupport", "transcriptId", sqlQueryMulti, (sqlLoadFunc)gencodeExonSupportLoad);
struct gencodeToUniProt *uniProts = metaDataLoad(tdb, conn, gencodeId, "gencodeToUniProt", "transcriptId", sqlQueryMulti, (sqlLoadFunc)gencodeToUniProtLoad);
slSort(&uniProts, uniProtDatasetCmp);
bool haveTsl = haveGencodeTable(tdb, "gencodeTranscriptionSupportLevel");
struct gencodeTranscriptionSupportLevel *tsl = haveTsl ? metaDataLoad(tdb, conn, gencodeId, "gencodeTranscriptionSupportLevel", "transcriptId", 0, (sqlLoadFunc)gencodeTranscriptionSupportLevelLoad) : NULL;

int geneChromStart, geneChromEnd;
getGeneBounds(tdb, conn, transAnno, &geneChromStart, &geneChromEnd);

writeBasicInfoHtml(conn, tdb, gencodeId, transAnno, transAttrs, geneChromStart, geneChromEnd, geneSource, transcriptSource, tags, haveTsl, tsl);
writeTagLinkHtml(tags);
if (haveRemarks)
    writeAnnotationRemarkHtml(remarks);
if (isProteinCodingTrans(transAttrs))
    writePdbLinkHtml(pdbs);
writePubMedLinkHtml(pubMeds);
if (haveEntrezGene)
    writeEntrezGeneLinkHtml(entrezGenes);
writeSupportingEvidenceLinkHtml(tdb, gencodeId, transcriptSupports, exonSupports);

gencodeAttrsFree(&transAttrs);
gencodeAnnotationRemarkFreeList(&remarks);
gencodeGeneSourceFreeList(&geneSource);
gencodeTranscriptSourceFreeList(&transcriptSource);
gencodeToPdbFreeList(&pdbs);
gencodeToPubMedFreeList(&pubMeds);
gencodeToEntrezGeneFreeList(&entrezGenes);
gencodeToRefSeqFreeList(&refSeqs);
gencodeTranscriptSupportFreeList(&transcriptSupports);
gencodeExonSupportFreeList(&exonSupports);
gencodeToUniProtFreeList(&uniProts);
gencodeTranscriptionSupportLevelFreeList(&tsl);
}

static void gencodePrint(struct section *section, struct sqlConnection *conn,
	char *gencodeId)
{
struct trackDb *tdb = globalTdb;
struct genePred *anno = transAnnoLoad(conn, tdb, gencodeId);

doGencodeGeneTrack(tdb, gencodeId, conn, anno);

genePredFreeList(&anno);
}


bool isNewGencodeGene(struct trackDb *tdb)
/* is this a new-style gencode (>= V7) track, as indicated by
 * the presence of the gencodeVersion setting */
{
return getGencodeVersion(tdb) != NULL;
}

static boolean gencodeExists(struct section *section, struct sqlConnection *conn, char *geneId)
/* Return TRUE if necessary database exists and has something
 *  * on this one. */
{
struct trackDb *tdb = globalTdb;

if (startsWith("gencodeAnnot", tdb->table))
    return TRUE;
return FALSE;
}

struct section *gencodeSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create links section. */
{
struct section *section = sectionNew(sectionRa, "gencode");
section->exists = gencodeExists;
section->print = gencodePrint;
section->raFile = "gencode.ra";
return section;
}

