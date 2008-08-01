/* ccdsClick - click handling for CCDS track and related functions  */
#include "common.h"
#include "hgc.h"
#include "ccdsClick.h"
#include "ccdsInfo.h"
#include "ccdsGeneMap.h"
#include "geneSimilarities.h"
#include "genbank.h"
#include "genePred.h"
#include "genePredReader.h"
#include "ensFace.h"
#include "mgcClick.h"
#include "htmshell.h"

struct ccdsInfo *getCcdsUrlForSrcDb(struct sqlConnection *conn, char *acc)
/* Get a ccdsInfo object for a RefSeq, ensembl, or vega gene, if it
 * exists, otherwise return NULL */
{
if (hTableExists(database, "ccdsInfo"))
    return ccdsInfoSelectByMrna(conn, acc);
else
    return NULL;
}

void printCcdsUrlForSrcDb(struct sqlConnection *conn, struct ccdsInfo *ccdsInfo)
/* Print out CCDS URL for a refseq, ensembl, or vega gene, if it
 * exists.  */
{
printf("../cgi-bin/hgc?%s&g=ccdsGene&i=%s&c=%s&o=%d&l=%d&r=%d&db=%s",
       cartSidUrlString(cart), ccdsInfo->ccds, seqName, 
       winStart, winStart, winEnd, database);
}

void printCcdsForSrcDb(struct sqlConnection *conn, char *acc)
/* Print out CCDS link for a refseq, ensembl, or vega gene, if it
 * exists.  */
{
struct ccdsInfo *ccdsInfo = getCcdsUrlForSrcDb(conn, acc);;
if (ccdsInfo != NULL)
    {
    printf("<B>CCDS:</B> <A href=\"");
    printCcdsUrlForSrcDb(conn, ccdsInfo);
    printf("\">%s</A><BR>", ccdsInfo->ccds);
    }
}

struct ccdsGeneMap *getCcdsGenesForMappedGene(struct sqlConnection *conn, char *acc,
                                              char *mapTable)
/* get a list of ccds genes associated with a current and window from a
 * mapping table, or NULL */
{
struct ccdsGeneMap *ccdsGenes = NULL;
if (sqlTableExists(conn, mapTable) && sqlTableExists(conn, "ccdsInfo"))
    ccdsGenes = ccdsGeneMapSelectByGeneOver(conn, mapTable, acc, seqName,
                                       winStart, winEnd, 0.0);
slSort(&ccdsGenes, ccdsGeneMapCcdsIdCmp);
return ccdsGenes;
}


void printCcdsUrl(struct sqlConnection *conn, char *ccdsId)
/* Print out CCDS url for a gene  */
{
printf("../cgi-bin/hgc?%s&g=ccdsGene&i=%s&c=%s&o=%d&l=%d&r=%d&db=%s",
       cartSidUrlString(cart), ccdsId, seqName, 
       winStart, winStart, winEnd, database);
}

void printCcdsForMappedGene(struct sqlConnection *conn, char *acc,
                            char *mapTable)
/* Print out CCDS links for a gene mapped via a cddsGeneMap table  */
{
struct ccdsGeneMap *ccdsGenes = getCcdsGenesForMappedGene(conn, acc, mapTable);
if (ccdsGenes != NULL)
    {
    printf("<B>CCDS:</B> ");
    struct ccdsGeneMap *gene;
    for (gene = ccdsGenes; gene != NULL; gene = gene->next)
        {
        if (gene != ccdsGenes)
            printf(", ");
        printf("<A href=\"");
        printCcdsUrl(conn, gene->ccdsId);
        printf("\">%s</A>", gene->ccdsId);
        }
    printf("<BR>\n");
    }
}

static char *getCcdsGeneSymbol(struct sqlConnection *conn, struct ccdsInfo *rsCcds)
/* get the gene name for a CCDS */
{
struct ccdsInfo *ci;
char accBuf[GENBANK_ACC_BUFSZ], query[256];
char *geneSym = NULL;

for (ci = rsCcds; ci != NULL; ci = ci->next)
    {
    safef(query, sizeof(query), "select name from refLink where mrnaAcc='%s'",
          genbankDropVer(accBuf, ci->mrnaAcc));
    geneSym = sqlQuickString(conn, query);
    if (geneSym != NULL)
        return geneSym;
    }
return NULL;
}

static char *getCcdsRefSeqSummary(struct sqlConnection *conn, struct ccdsInfo *rsCcds)
/* get the refseq summary for a CCDS */
{
struct ccdsInfo *ci;
char accBuf[GENBANK_ACC_BUFSZ];
char *summary = NULL;

for (ci = rsCcds; ci != NULL; ci = ci->next)
    {
    summary = getRefSeqSummary(conn, genbankDropVer(accBuf, ci->mrnaAcc));
    if (summary != NULL)
        return summary;
    }
return NULL;
}

static struct ccdsGeneMap *ccdsGetGenes(struct sqlConnection *conn, char *mapTable,
                                        char *ccdsId)
/* Get ccdsGeneMap objects for a ccdsId.  Returns only
 * the best overlapping ones (ones with the same cdsSimilariy as
 * the highest cdsSimilariy. */
{
struct ccdsGeneMap *ccdsGenes = NULL, *bestCcdsGenes = NULL, *ccdsGene;

/* filter by chrom due to PAR */
ccdsGenes = ccdsGeneMapSelectByCcds(conn, mapTable, ccdsId, seqName, 0.0);
if (ccdsGenes == NULL)
    return NULL;

bestCcdsGenes = slPopHead(&ccdsGenes);  /* seed with first */
while ((ccdsGene = slPopHead(&ccdsGenes)) != NULL)
    {
    if (ccdsGene->cdsSimilarity == bestCcdsGenes->cdsSimilarity)
        {
        /* same as best, keep */
        slAddHead(&bestCcdsGenes, ccdsGene);
        }
    else if (ccdsGene->cdsSimilarity > bestCcdsGenes->cdsSimilarity)
        {
        /* new best, replace list */
        ccdsGeneMapFreeList(&bestCcdsGenes);
        bestCcdsGenes = ccdsGene;
        }
    else
        {
        /* worse, drop */
        ccdsGeneMapFree(&ccdsGene);
        }
    }

/* only keep one of each gene */
slUniqify(&bestCcdsGenes, ccdsGeneMapGeneIdCmp, ccdsGeneMapFree);

return bestCcdsGenes;
}

static void printCcdsHgGeneUrl(struct sqlConnection *conn, char *ccdsId, char* kgId)
/* output a URL to hgGene for a ccds */
{
char where[128];
struct genePredReader *gpr;
struct genePred *ccdsGene = NULL, *kgGene = NULL;

/* get ccds genePred to get location */
safef(where, sizeof(where), "chrom = '%s' and name = '%s'", seqName, ccdsId);
gpr = genePredReaderQuery(conn, "ccdsGene", where);
ccdsGene = genePredReaderAll(gpr);
genePredReaderFree(&gpr);
if (ccdsGene == NULL)
    errAbort("%s not found in ccdsGene table for chrom %s", ccdsId, seqName);
else if (ccdsGene->next != NULL)
    errAbort("multiple %s rows found in ccdsGene table for chrom %s", ccdsId, seqName);

/* get KG genePred, as need exact location for link */
safef(where, sizeof(where), "name = '%s' and strand = '%s'", kgId,
      ccdsGene->strand);
gpr = genePredReaderRangeQuery(conn, "knownGene", seqName,
                               ccdsGene->txStart, ccdsGene->txEnd, where);
kgGene = genePredReaderAll(gpr);
genePredReaderFree(&gpr);
if (kgGene == NULL)
    errAbort("%s not found in knownGene table for chrom %s", kgId, seqName);
else if (kgGene->next != NULL)
    errAbort("multiple %s rows found in knownGene table for chrom %s", kgId, seqName);

printf("../cgi-bin/hgGene?%s&%s=%s&%s=%s&%s=%s&%s=%d&%s=%d",
       cartSidUrlString(cart),
       "db", database,
       "hgg_gene", kgId,
       "hgg_chrom", seqName,
       "hgg_start", kgGene->txStart,
       "hgg_end", kgGene->txEnd);
genePredFree(&ccdsGene);
genePredFree(&kgGene);
}

static void ccdsNcbiRows(char *ccdsId, struct ccdsInfo *rsCcds)
/* output RefSeq CCDS entries */
{
struct ccdsInfo *ci;
for (ci = rsCcds; ci != NULL; ci = ci->next)
    {
    printf("<TR>");
    if (ci == rsCcds)
        printf("<TH ROWSPAN=%d>RefSeq", slCount(rsCcds));
    printf("<TD><A HREF=\"");
    printEntrezNucleotideUrl(stdout, ci->mrnaAcc);
    printf("\" TARGET=_blank>%s</A>&nbsp;", ci->mrnaAcc);
    printf("<TD><A HREF=\"");
    printEntrezProteinUrl(stdout, ci->protAcc);
    printf("\" TARGET=_blank>%s</A>", ci->protAcc);
    printf("</TR>\n");
    }
}

static void ccdsHinxtonRows(char *ccdsId, bool isVega,struct ccdsInfo *hinCcds)
/* output Ensembl or vega CCDS entries */
{
struct ccdsInfo *ci;
char *dbArg = isVega ? "&db=vega" : "";
char *ensGenome = ensOrgNameFromScientificName(scientificName);
if (ensGenome == NULL)
    errAbort("%s: ensOrgNameFromScientificName failed", ccdsId);

for (ci = hinCcds; ci != NULL; ci = ci->next)
    {
    printf("<TR>");
    if (ci == hinCcds)
        printf("<TH ROWSPAN=%d>%s", slCount(hinCcds),
               (isVega ? "Vega" : "Ensembl"));
    printf("<TD><A HREF=\"http://www.ensembl.org/%s/geneview?transcript=%s%s\""
           " TARGET=_blank>%s</A>&nbsp;",
           ensGenome, ci->mrnaAcc, dbArg, ci->mrnaAcc);
    printf("<TD><A HREF=\"http://www.ensembl.org/%s/protview?peptide=%s%s\""
           " TARGET=_blank>%s</A>",
           ensGenome, ci->protAcc, dbArg, ci->protAcc);
    printf("</TR>\n");
    }
}

static void ccdsKnownGenesRows(struct sqlConnection *conn, char *ccdsId)
/* output KnownGenes mapped to CCDS */
{
struct ccdsGeneMap *ccdsKgs = ccdsGetGenes(conn, "ccdsKgMap", ccdsId);
struct ccdsGeneMap *ccdsKg;
boolean havePb = hgPbOk(database);
for (ccdsKg = ccdsKgs; ccdsKg != NULL; ccdsKg = ccdsKg->next)
    {
    char *spId = kgIdToSpId(conn, ccdsKg->geneId);
    printf("<TR>");
    if (ccdsKg == ccdsKgs)
        printf("<TH ROWSPAN=%d>%s", slCount(ccdsKgs),
               (havePb ? "UCSC Genes/<br>Proteome Browser"
                : "UCSC Genes"));
    printf("<TD><A HREF=\"");
    printCcdsHgGeneUrl(conn, ccdsId, ccdsKg->geneId);
    printf("\" TARGET=_blank>%s</A>", ccdsKg->geneId);

    if (havePb)
        printf("<TD><A HREF=\"../cgi-bin/pbTracks?proteinID=%s&db=%s\""
               " TARGET=_blank>%s</A>", spId, database, spId);
    else
        printf("<TD>&nbsp;");
    freez(&spId);

    printf("</TR>\n");
    }
}

static void ccdsMgcRows(struct sqlConnection *conn, char *ccdsId)
/* output MGCs mapped to CCDS */
{
// only possible to get multiple CCDS genePreds in PAR, since we are linking
// to details, not browser, only use the first set.
struct geneSimilarities *geneSims
    = geneSimilaritiesBuildAll(conn, TRUE, ccdsId, "ccdsGene", "mgcGenes");

struct geneSim *mgc;
for (mgc = geneSims->genes; mgc != NULL; mgc = mgc->next)
    {
    printf("<TR>");
    if (mgc == geneSims->genes)
        printf("<TH ROWSPAN=%d>MGC", slCount(geneSims->genes));
    printf("<TD><A HREF=\"");
    printMgcDetailsUrl(mgc->gene->name, mgc->gene->txStart);
    printf("\">%s</A>", mgc->gene->name);
    printf("<TD>&nbsp;</TR>\n");
    }
geneSimilaritiesFreeList(&geneSims);
}

void doCcdsGene(struct trackDb *tdb, char *ccdsId)
/* Process click on a CCDS gene. */
{
struct sqlConnection *conn = hAllocConn(database);
struct ccdsInfo *rsCcds = ccdsInfoSelectByCcds(conn, ccdsId, ccdsInfoNcbi);
struct ccdsInfo *vegaCcds = ccdsInfoSelectByCcds(conn, ccdsId, ccdsInfoVega);
struct ccdsInfo *ensCcds = ccdsInfoSelectByCcds(conn, ccdsId, ccdsInfoEnsembl);
char *geneSym, *desc, *summary;

if (rsCcds == NULL)
    errAbort("database inconsistency: no NCBI ccdsInfo entries found for %s", ccdsId);
if ((vegaCcds == NULL) && (ensCcds == NULL))
    errAbort("database inconsistency: no Hinxton ccdsInfo entries found for %s", ccdsId);

ccdsInfoMRnaSort(&rsCcds);
ccdsInfoMRnaSort(&vegaCcds);
ccdsInfoMRnaSort(&ensCcds);

cartWebStart(cart, database, "CCDS Gene");

printf("<H2>Consensus CDS Gene %s</H2>\n", ccdsId);

/* table with basic information about the CCDS (2 columns) */
printf("<TABLE class=\"hgcCcds\"><TBODY>\n");

/* gene symbol */
geneSym = getCcdsGeneSymbol(conn, rsCcds);
if (geneSym != NULL)
    printf("<TR><TH>Gene<TD>%s</TR>\n", geneSym);
freez(&geneSym);

/* description */
desc = hGenBankGetDesc(database, rsCcds->mrnaAcc, TRUE);
if (desc != NULL)
    printf("<TR><TH>Description<TD>%s</TR>\n", desc);
freez(&desc);

/* CCDS sequence links */
printf("<TR>\n");
printf("<TH>Sequences");
printf("<TD>");
hgcAnchorSomewhere("htcGeneMrna", ccdsId, "ccdsGene", seqName);
printf("CDS</A>, &nbsp;");
hgcAnchorSomewhere("htcTranslatedPredMRna", ccdsId, "translate", seqName);
printf("protein</A>, &nbsp;");
hgcAnchorSomewhere( "htcGeneInGenome", ccdsId, "ccdsGene", seqName);
printf("genomic</A>");
printf("</TR>\n");

/* CCDS databases */
printf("<TR>\n");
printf("<TH>CCDS database");
printf("<TD> <A HREF=\"http://www.ncbi.nlm.nih.gov/CCDS/CcdsBrowse.cgi?REQUEST=CCDS&BUILDS=ALLBUILDS&DATA=%s\" TARGET=_blank>%s</A>",
       ccdsId, ccdsId);
printf("</TR>\n");

printf("</TBODY></TABLE>\n");
printf("<BR>\n");

/* table with links to other browser apps or external databases (3 columns) */
printf("<TABLE class=\"hgcCcds\">\n");
printf("<CAPTION>Associated Sequences</CAPTION>\n");
printf("<THEAD>\n");
printf("<TR><TH>&nbsp;<TH>mRNA<TH>Protein</TR>\n");
printf("</THEAD><TBODY>\n");
if (hTableExists(database, "ccdsKgMap"))
    ccdsKnownGenesRows(conn, ccdsId);
ccdsNcbiRows(ccdsId, rsCcds);
if (vegaCcds != NULL)
    ccdsHinxtonRows(ccdsId, TRUE, vegaCcds);
if (ensCcds != NULL)
    ccdsHinxtonRows(ccdsId, FALSE, ensCcds);
if (sqlTableExists(conn, "mgcGenes"))
    ccdsMgcRows(conn, ccdsId);

printf("</TBODY></TABLE>\n");

printf("<P><EM>Note: mRNA and protein sequences in other gene collections "
       "may differ from the CCDS sequences.</EM>\n");

summary = getCcdsRefSeqSummary(conn, rsCcds);
if (summary != NULL)
    {
    htmlHorizontalLine();
    printf("<H3>RefSeq summary of %s</H3>\n", ccdsId);
    printf("<P>%s</P>\n", summary);
    freez(&summary);
    }

printTrackHtml(tdb);
ccdsInfoFreeList(&rsCcds);
ccdsInfoFreeList(&vegaCcds);
ccdsInfoFreeList(&ensCcds);
hFreeConn(&conn);
}

