/* hgc - Human Genome Click processor - gets called when user clicks
 * on something in human tracks display. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include <float.h>
#include "obscure.h"
#include "hCommon.h"
#include "hash.h"
#include "binRange.h"
#include "bits.h"
#include "memgfx.h"
#include "hvGfx.h"
#include "portable.h"
#include "regexHelper.h"
#include "errAbort.h"
#include "dystring.h"
#include "nib.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "cart.h"
#include "jksql.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "fuzzyFind.h"
#include "seqOut.h"
#include "hdb.h"
#include "spDb.h"
#include "hui.h"
#include "hgRelate.h"
#include "htmlPage.h"
#include "psl.h"
#include "cogs.h"
#include "cogsxra.h"
#include "bed.h"
#include "cgh.h"
#include "agpFrag.h"
#include "agpGap.h"
#include "ctgPos.h"
#include "contigAcc.h"
#include "ctgPos2.h"
#include "clonePos.h"
#include "bactigPos.h"
#include "rmskOut.h"
#include "xenalign.h"
#include "isochores.h"
#include "simpleRepeat.h"
#include "cpgIsland.h"
#include "cpgIslandExt.h"
#include "genePred.h"
#include "genePredReader.h"
#include "pepPred.h"
#include "peptideAtlasPeptide.h"
#include "wabAli.h"
#include "genomicDups.h"
#include "est3.h"
#include "rnaGene.h"
#include "tRNAs.h"
#include "gbRNAs.h"
#include "encode/encodeRna.h"
#include "hgMaf.h"
#include "maf.h"
#include "stsMarker.h"
#include "stsMap.h"
#include "rhMapZfishInfo.h"
#include "recombRate.h"
#include "recombRateRat.h"
#include "recombRateMouse.h"
#include "stsInfo.h"
#include "stsInfo2.h"
#include "mouseSyn.h"
#include "mouseSynWhd.h"
#include "ensPhusionBlast.h"
#include "cytoBand.h"
#include "knownMore.h"
#include "snp125.h"
#include "snp125Ui.h"
#include "snp132Ext.h"
#include "snp.h"
#include "snpMap.h"
#include "snpExceptions.h"
#include "snp125Exceptions.h"
#include "snp125CodingCoordless.h"
#include "cnpIafrate.h"
#include "cnpIafrate2.h"
#include "cnpLocke.h"
#include "cnpSebat.h"
#include "cnpSebat2.h"
#include "cnpSharp.h"
#include "cnpSharp2.h"
#include "delHinds2.h"
#include "delConrad2.h"
#include "dgv.h"
#include "dgvPlus.h"
#include "tokenizer.h"
#include "softberryHom.h"
#include "borkPseudoHom.h"
#include "sanger22extra.h"
#include "ncbiRefLink.h"
#include "ncbiRefSeqLink.h"
#include "refLink.h"
#include "hgConfig.h"
#include "estPair.h"
#include "softPromoter.h"
#include "customTrack.h"
#include "trackHub.h"
#include "hubConnect.h"
#include "sage.h"
#include "sageExp.h"
#include "pslWScore.h"
#include "lfs.h"
#include "mcnBreakpoints.h"
#include "fishClones.h"
#include "featureBits.h"
#include "web.h"
#include "dbDb.h"
#include "jaxOrtholog.h"
#include "dnaProbe.h"
#include "ancientRref.h"
#include "jointalign.h"
#include "gcPercent.h"
#include "genMapDb.h"
#include "altGraphX.h"
#include "geneGraph.h"
#include "stsMapMouse.h"
#include "stsInfoMouse.h"
#include "dbSnpRs.h"
#include "genomicSuperDups.h"
#include "celeraDupPositive.h"
#include "celeraCoverage.h"
#include "sample.h"
#include "axt.h"
#include "axtInfo.h"
#include "jaxQTL.h"
#include "jaxQTL3.h"
#include "wgRna.h"
#include "ncRna.h"
#include "gbProtAnn.h"
#include "hgSeq.h"
#include "chain.h"
#include "chainDb.h"
#include "chainNetDbLoad.h"
#include "chainToPsl.h"
#include "chainToAxt.h"
#include "netAlign.h"
#include "stsMapRat.h"
#include "stsInfoRat.h"
#include "stsMapMouseNew.h"
#include "stsInfoMouseNew.h"
#include "vegaInfo.h"
#include "vegaInfoZfish.h"
#include "ensInfo.h"
#include "scoredRef.h"
#include "blastTab.h"
#include "hdb.h"
#include "hgc.h"
#include "genbank.h"
#include "pseudoGeneLink.h"
#include "axtLib.h"
#include "ensFace.h"
#include "bdgpGeneInfo.h"
#include "flyBaseSwissProt.h"
#include "flyBase2004Xref.h"
#include "affy10KDetails.h"
#include "affy120KDetails.h"
#include "encode/encodeRegionInfo.h"
#include "encode/encodeErge.h"
#include "encode/encodeErgeHssCellLines.h"
#include "encode/encodeStanfordPromoters.h"
#include "encode/encodeStanfordPromotersAverage.h"
#include "encode/encodeIndels.h"
#include "encode/encodeHapMapAlleleFreq.h"
#include "hapmapSnps.h"
#include "hapmapAllelesOrtho.h"
#include "hapmapAllelesSummary.h"
#include "sgdDescription.h"
#include "sgdClone.h"
#include "tfbsCons.h"
#include "tfbsConsMap.h"
#include "tfbsConsSites.h"
#include "tfbsConsFactors.h"
#include "simpleNucDiff.h"
#include "bgiGeneInfo.h"
#include "bgiSnp.h"
#include "bgiGeneSnp.h"
#include "botDelay.h"
#include "vntr.h"
#include "zdobnovSynt.h"
#include "HInv.h"
#include "bed5FloatScore.h"
#include "bed6FloatScore.h"
#include "pscreen.h"
#include "jalview.h"
#include "flyreg.h"
#include "putaInfo.h"
#include "gencodeIntron.h"
#include "cutter.h"
#include "switchDbTss.h"
#include "chicken13kInfo.h"
#include "gapCalc.h"
#include "chainConnect.h"
#include "dv.h"
#include "dvBed.h"
#include "dvXref2.h"
#include "omimTitle.h"
#include "dless.h"
#include "gv.h"
#include "gvUi.h"
#include "protVar.h"
#include "oreganno.h"
#include "oregannoUi.h"
#include "pgSnp.h"
#include "pgPhenoAssoc.h"
#include "pgSiftPred.h"
#include "pgPolyphenPred.h"
#include "bedDetail.h"
#include "ec.h"
#include "transMapClick.h"
#include "retroClick.h"
#include "mgcClick.h"
#include "ccdsClick.h"
#include "gencodeClick.h"
#include "memalloc.h"
#include "trashDir.h"
#include "kg1ToKg2.h"
#include "wikiTrack.h"
#include "grp.h"
#include "omicia.h"
#include "atomDb.h"
#include "pcrResult.h"
#include "twoBit.h"
#include "itemConf.h"
#include "chromInfo.h"
#include "gbWarn.h"
#include "mammalPsg.h"
#include "net.h"
#include "jsHelper.h"
#include "virusClick.h"
#include "gwasCatalog.h"
#include "mdb.h"
#include "yaleGencodeAssoc.h"
#include "itemDetailsHtml.h"
#include "trackVersion.h"
#include "numtsClick.h"
#include "geneReviewsClick.h"
#include "bigBed.h"
#include "bigPsl.h"
#include "bedTabix.h"
#include "longRange.h"
#include "hmmstats.h"
#include "aveStats.h"
#include "trix.h"
#include "bPlusTree.h"
#include "customFactory.h"
#include "dupTrack.h"
#include "iupac.h"
#include "clinvarSubLolly.h"
#include "jsHelper.h"
#include "errCatch.h"
#include "htslib/bgzf.h"
#include "htslib/kstring.h"
#include "pipeline.h"
#include "genark.h"
#include "chromAlias.h"
#include "dotPlot.h"
#include "quickLift.h"

static char *rootDir = "hgcData";

#define LINESIZE 70  /* size of lines in comp seq feature */
#define MAX_DISPLAY_QUERY_SEQ_SIZE 5000000  // Big enough for HLA alts

struct cart *cart;	/* User's settings. */
char *seqName;		/* Name of sequence we're working on. */
int winStart, winEnd;   /* Bounds of sequence. */
char *database;		/* Name of mySQL database. */
char *organism;		/* Colloquial name of organism. */
char *genome;		/* common name, e.g. Mouse, Human */
char *scientificName;	/* Scientific name of organism. */

/* for earlyBotCheck() function at the beginning of main() */
#define delayFraction   0.5    /* standard penalty is 1.0 for most CGIs */
                                /* this one is 0.5 */
boolean issueBotWarning = FALSE;

struct hash *trackHash;	/* A hash of all tracks - trackDb valued */

void printLines(FILE *f, char *s, int lineSize);

char mousedb[] = "mm3";

#define NUMTRACKS 9
int prevColor[NUMTRACKS]; /* used to optimize color change html commands */
int currentColor[NUMTRACKS]; /* used to optimize color change html commands */
int maxShade = 9;	/* Highest shade in a color gradient. */
Color shadesOfGray[10+1];	/* 10 shades of gray from white to black */

Color shadesOfRed[16];
boolean exprBedColorsMade = FALSE; /* Have the shades of red been made? */
int maxRGBShade = 16;

struct bed *sageExpList = NULL;
char ncbiOmimUrl[255] = {"https://www.ncbi.nlm.nih.gov/omim/"};

struct palInfo
{
    char *chrom;
    int left;
    int right;
    char *rnaName;
};


/* See this NCBI web doc for more info about entrezFormat:
 * https://www.ncbi.nlm.nih.gov/entrez/query/static/linking.html */
char *entrezFormat = "https://www.ncbi.nlm.nih.gov/entrez/query.fcgi?cmd=Search&db=%s&term=%s&doptcmdl=%s&tool=genome.ucsc.edu";
char *entrezPureSearchFormat = "https://www.ncbi.nlm.nih.gov/entrez/query.fcgi?cmd=PureSearch&db=%s&details_term=%s[%s] ";
char *ncbiGeneFormat = "https://www.ncbi.nlm.nih.gov/gene/%s";
char *entrezUidFormat = "https://www.ncbi.nlm.nih.gov/entrez/query.fcgi?cmd=Retrieve&db=%s&list_uids=%d&dopt=%s&tool=genome.ucsc.edu";
/* db=unists is not mentioned in NCBI's doc... so stick with this usage: */
char *unistsnameScript = "https://www.ncbi.nlm.nih.gov:80/entrez/query.fcgi?db=unists";
char *unistsScript = "https://www.ncbi.nlm.nih.gov/genome/sts/sts.cgi?uid=";
char *gdbScript = "http://www.gdb.org/gdb-bin/genera/accno?accessionNum=";
char *cloneDbScript = "https://www.ncbi.nlm.nih.gov/clone?term=";
char *traceScript = "https://www.ncbi.nlm.nih.gov/Traces/trace.cgi?cmd=retrieve&val=";
char *genMapDbScript = "http://genomics.med.upenn.edu/perl/genmapdb/byclonesearch.pl?clone=";
char *uniprotFormat = "http://www.uniprot.org/uniprot/%s";
char *dbSnpFormat = "https://www.ncbi.nlm.nih.gov/SNP/snp_ref.cgi?type=rs&rs=%s";
char *clinVarFormat = "https://www.ncbi.nlm.nih.gov/clinvar/?term=%s[clv_acc]";

/* variables for gv tables */
char *gvPrevCat = NULL;
char *gvPrevType = NULL;

/* initialized by getCtList() if necessary: */
struct customTrack *theCtList = NULL;

/* getDNA stuff actually works when the database doesn't exist! */
boolean dbIsFound = FALSE;

/* forwards */
char *getPredMRnaProtSeq(struct genePred *gp);
void doAltGraphXDetails(struct trackDb *tdb, char *item);

char* getEntrezNucleotideUrl(char *accession)
/* get URL for Entrez browser on a nucleotide. free resulting string */
{
char url[512];
safef(url, sizeof(url), entrezFormat, "Nucleotide", accession, "GenBank");
return cloneString(url);
}

void printNcbiGeneUrl(FILE *f, char *gene)
/* Print URL for Entrez browser on a nucleotide. */
{
fprintf(f, ncbiGeneFormat, gene);
}

void printEntrezNucleotideUrl(FILE *f, char *accession)
/* Print URL for Entrez browser on a nucleotide. */
{
fprintf(f, entrezFormat, "Nucleotide", accession, "GenBank");
}

void printEntrezEstUrl(FILE *f, char *accession)
/* Print URL for Entrez browser on a nucleotide. */
{
fprintf(f, entrezFormat, "nucest", accession, "GenBank");
}

void printEntrezProteinUrl(FILE *f, char *accession)
/* Print URL for Entrez browser on a protein. */
{
fprintf(f, entrezFormat, "Protein", accession, "GenPept");
}

static void printEntrezPubMedUrl(FILE *f, char *term)
/* Print URL for Entrez browser on a PubMed search. */
{
fprintf(f, entrezFormat, "PubMed", term, "DocSum");
}

static void printEntrezPubMedPureSearchUrl(FILE *f, char *term, char *keyword)
/* Print URL for Entrez browser on a PubMed search. */
{
fprintf(f, entrezPureSearchFormat, "PubMed", term, keyword);
}

void printEntrezPubMedUidAbstractUrl(FILE *f, int pmid)
/* Print URL for Entrez browser on a PubMed search. */
{
fprintf(f, entrezUidFormat, "PubMed", pmid, "Abstract");
}

void printEntrezPubMedUidUrl(FILE *f, int pmid)
/* Print URL for Entrez browser on a PubMed search. */
{
fprintf(f, entrezUidFormat, "PubMed", pmid, "Summary");
}

void printEntrezGeneUrl(FILE *f, int geneid)
/* Print URL for Entrez browser on a gene details page. */
{
fprintf(f, entrezUidFormat, "gene", geneid, "Graphics");
}

static void printEntrezOMIMUrl(FILE *f, int id)
/* Print URL for Entrez browser on an OMIM search. */
{
char buf[64];
snprintf(buf, sizeof(buf), "%d", id);
fprintf(f, entrezFormat, "OMIM", buf, "Detailed");
}

void printSwissProtAccUrl(FILE *f, char *accession)
/* Print URL for Swiss-Prot protein accession. */
{
fprintf(f, uniprotFormat, accession);
}

static void printSwissProtProteinUrl(FILE *f, char *accession)
/* Print URL for Swiss-Prot NiceProt on a protein. */
{
char *spAcc;
/* make sure accession number is used (not display ID) when linking to Swiss-Prot */
spAcc = uniProtFindPrimAcc(accession);
if (spAcc != NULL)
    {
    printSwissProtAccUrl(f, accession);
    }
else
    {
    fprintf(f, uniprotFormat, accession);
    }
}

static void printSwissProtVariationUrl(FILE *f, char *accession)
/* Print URL for Swiss-Prot variation data on a protein. */
{
if (accession != NULL)
    {
    fprintf(f, "\"http://www.expasy.org/cgi-bin/get-sprot-variant.pl?%s\"", accession);
    }
}

static void printOmimUrl(FILE *f, char *term)
/* Print URL for OMIM data on a protein. */
{
if (term != NULL)
    {
    fprintf(f, "\"https://www.ncbi.nlm.nih.gov/omim/%s\"", term);
    }
}

static void printEntrezUniSTSUrl(FILE *f, char *name)
/* Print URL for Entrez browser on a STS name. */
{
fprintf(f, "\"%s&term=%s\"", unistsnameScript, name);
}

static void printUnistsUrl(FILE *f, int id)
/* Print URL for UniSTS record for an id. */
{
fprintf(f, "\"%s%d\"", unistsScript, id);
}

/* Print URL for GDB browser for an id
 * GDB currently inoperative, so have temporarily disabled this function
 *
static void printGdbUrl(FILE *f, char *id)
{
fprintf(f, "%s", id);
}
*/

static void printCloneDbUrl(FILE *f, char *clone)
/* Print URL for Clone Registry at NCBI for a clone */
{
fprintf(f, "\"%s%s\"", cloneDbScript, clone);
}

static void printTraceTiUrl(FILE *f, char *name)
/* Print URL for Trace Archive at NCBI for a trace id (TI) */
{
fprintf(f, "\"%s%s\"", traceScript, name);
}

static void printTraceUrl(FILE *f, char *idType, char *name)
/* Print URL for Trace Archive at NCBI for an identifier specified by type */
{
fprintf(f, "\"%s%s%%3D%%27%s%%27\"", traceScript, idType, name);
}

static void printGenMapDbUrl(FILE *f, char *clone)
/* Print URL for GenMapDb at UPenn for a clone */
{
fprintf(f, "\"%s%s\"", genMapDbScript, clone);
}

static void printFlyBaseUrl(FILE *f, char *fbId)
/* Print URL for FlyBase browser. */
{
fprintf(f, "\"http://flybase.net/.bin/fbidq.html?%s\"", fbId);
}

static void printBDGPUrl(FILE *f, char *bdgpName)
/* Print URL for Berkeley Drosophila Genome Project browser. */
{
fprintf(f, "\"http://www.fruitfly.org/cgi-bin/annot/gene?%s\"", bdgpName);
}

char *hgTracksPathAndSettings()
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

char *hgcPathAndSettings()
/* Return path with this CGI script and session state variable. */
{
static struct dyString *dy = NULL;
if (dy == NULL)
    {
    dy = dyStringNew(128);
    dyStringPrintf(dy, "%s?%s", hgcName(), cartSidUrlString(cart));
    }
return dy->string;
}

static void hgcAnchorSomewhereExt(char *group, char *item, char *other, char *chrom, int start, int end, char *tbl)
/* Generate an anchor that calls click processing program with item
 * and other parameters. */
{
char *itemSafe = cgiEncode(item);
printf("<A HREF=\"%s&g=%s&i=%s&c=%s&l=%d&r=%d&o=%s&table=%s\">",
       hgcPathAndSettings(), group, itemSafe, chrom, start, end, other, tbl);
freeMem(itemSafe);
}

void hgcAnchorSomewhere(char *group, char *item, char *other, char *chrom)
/* Generate an anchor that calls click processing program with item
 * and other parameters. */
{
char *tbl = cgiUsualString("table", cgiString("g"));
hgcAnchorSomewhereExt(group, item, other, chrom, winStart, winEnd, tbl);
}

void hgcAnchorPosition(char *group, char *item)
/* Generate an anchor that calls click processing program with item
 * and group parameters. */
{
char *tbl = cgiUsualString("table", cgiString("g"));
printf("<A HREF=\"%s&g=%s&i=%s&table=%s\">",
       hgcPathAndSettings(), group, item, tbl);
}

void hgcAnchorWindow(char *group, char *item, int thisWinStart,
                     int thisWinEnd, char *other, char *chrom)
/* Generate an anchor that calls click processing program with item
 * and other parameters, INCLUDING the ability to specify left and
 * right window positions different from the current window*/
{
printf("<A HREF=\"%s&g=%s&i=%s&c=%s&l=%d&r=%d&o=%s\">",
       hgcPathAndSettings(), group, item, chrom,
       thisWinStart, thisWinEnd, other);
}


void hgcAnchorJalview(char *item, char *fa)
/* Generate an anchor to jalview. */
{
struct dyString *dy = cgiUrlString();
    printf("<A HREF=\"%s?%s&jalview=YES\">",
	    hgcName(), dy->string);
    dyStringFree(&dy);
}

void hgcAnchorTranslatedChain(int item, char *other, char *chrom, int cdsStart, int cdsEnd)
/* Generate an anchor that calls click processing program with item
 * and other parameters. */
{
char *tbl = cgiUsualString("table", cgiString("g"));
printf("<A HREF=\"%s&g=%s&i=%d&c=%s&l=%d&r=%d&o=%s&table=%s&qs=%d&qe=%d\">",
       hgcPathAndSettings(), "htcChainTransAli", item, chrom, winStart, winEnd, other,
       tbl, cdsStart, cdsEnd);
}
void hgcAnchorPseudoGene(char *item, char *other, char *chrom, char *tag, int start, int end, char *qChrom, int qStart, int qEnd, int chainId, char *db2)
/* Generate an anchor to htcPseudoGene. */
{
char *encodedItem = cgiEncode(item);
printf("<A HREF=\"%s&g=%s&i=%s&c=%s&l=%d&r=%d&o=%s&db2=%s&ci=%d&qc=%s&qs=%d&qe=%d&xyzzy=xyzzy#%s\">",
       hgcPathAndSettings(), "htcPseudoGene", encodedItem, chrom, start, end,
       other, db2, chainId, qChrom, qStart, qEnd, tag);
}

void hgcAnchorSomewhereDb(char *group, char *item, char *other,
                          char *chrom, char *db)
/* Generate an anchor that calls click processing program with item
 * and other parameters. */
{
printf("<A HREF=\"%s&g=%s&i=%s&c=%s&l=%d&r=%d&o=%s&db=%s\">",
       hgcPathAndSettings(), group, item, chrom, winStart, winEnd, other, db);
}

void hgcAnchor(char *group, char *item, char *other)
/* Generate an anchor that calls click processing program with item
 * and other parameters. */
{
hgcAnchorSomewhere(group, item, other, seqName);
}

void writeFramesetType()
/* Write document type that shows a frame set, rather than regular HTML. */
{
fputs("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Frameset//EN\">\n", stdout);
}

void htmlFramesetStart(char *title)
/* Write DOCTYPE HTML and HEAD sections for framesets. */
{
/* Print start of HTML. */
writeFramesetType();
puts("<HTML>");
char *meta = getCspMetaHeader();
printf("<HEAD>\n%s<TITLE>%s</TITLE>\n</HEAD>\n\n", meta, title);
freeMem(meta);
}

boolean clipToChrom(int *pStart, int *pEnd)
/* Clip start/end coordinates to fit in chromosome. */
{
static int chromSize = -1;

if (chromSize < 0)
    chromSize = hChromSize(database, seqName);
if (*pStart < 0) *pStart = 0;
if (*pEnd > chromSize) *pEnd = chromSize;
return *pStart < *pEnd;
}

struct genbankCds getCds(struct sqlConnection *conn, char *acc)
/* obtain and parse the CDS, errAbort if not found or invalid */
{
char query[256];
sqlSafef(query, sizeof(query), "select c.name from %s,%s c where (acc=\"%s\") and (c.id=cds)",
      gbCdnaInfoTable,cdsTable, acc);

char *cdsStr = sqlQuickString(conn, query);
if (cdsStr == NULL)
    errAbort("no CDS found for %s", acc);
struct genbankCds cds;
if (!genbankCdsParse(cdsStr, &cds))
    errAbort("can't parse CDS for %s: %s", acc, cdsStr);
return cds;
}


void printCappedSequence(int start, int end, int extra)
/* Print DNA from start to end including extra at either end.
 * Capitalize bits from start to end. */
{
struct dnaSeq *seq;
int s, e, i;
struct cfm *cfm;

if (!clipToChrom(&start, &end))
    return;
s = start - extra;
e = end + extra;
clipToChrom(&s, &e);

printf("<P>Here is the sequence around this feature: bases %d to %d of %s. "
       "The bases that contain the feature itself are in upper case.</P>\n",
       s, e, seqName);
seq = hDnaFromSeq(database, seqName, s, e, dnaLower);
toUpperN(seq->dna + (start-s), end - start);
printf("<PRE><TT>");
cfm = cfmNew(10, 50, TRUE, FALSE, stdout, s);
for (i=0; i<seq->size; ++i)
    {
    cfmOut(cfm, seq->dna[i], 0);
    }
cfmFree(&cfm);
printf("</TT></PRE>");
}

void printBand(char *chrom, int start, int end, boolean tableFormat)
/* Print all matching chromosome bands.  */
/* Ignore end if it is zero. */
{
char sband[HDB_MAX_BAND_STRING], eband[HDB_MAX_BAND_STRING];
boolean gotS = FALSE;
boolean gotE = FALSE;

if (start < 0)
    return;
gotS = hChromBand(database, chrom, start, sband);
/* if the start lookup fails, don't bother with the end lookup */
if (!gotS)
    return;
/* if no end chrom, print start band and exit */
if (end == 0)
    {
    if (tableFormat)
        printf("<TR><TH ALIGN=left>Band:</TH><TD>%s</TD></TR>\n",sband);
    else
        printf("<B>Band:</B> %s<BR>\n", sband);
    return;
}
gotE = hChromBand(database, chrom, end-1, eband);
/* if eband equals sband, just use sband */
if (gotE && sameString(sband,eband))
   gotE = FALSE;
if (!gotE)
    {
    if (tableFormat)
        printf("<TR><TH ALIGN=left>Band:</TH><TD>%s</TD></TR>\n",sband);
    else
        printf("<B>Band:</B> %s<BR>\n", sband);
    return;
    }
if (tableFormat)
    printf("<TR><TH ALIGN=left>Bands:</TH><TD>%s - %s</TD></TR>\n",sband, eband);
else
    printf("<B>Bands:</B> %s - %s<BR>\n", sband, eband);

}


void printPosOnChrom(char *chrom, int start, int end, char *strand,
		     boolean featDna, char *item)
/* Print position lines referenced to chromosome. Strand argument may be NULL */
{

printf("<B>Position:</B> "
       "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
       hgTracksPathAndSettings(), database, cgiEncode(chrom), start+1, end);
printf("%s:%d-%d</A><BR>\n", chrom, start+1, end);
/* printBand(chrom, (start + end)/2, 0, FALSE); */
printBand(chrom, start, end, FALSE);
printf("<B>Genomic Size:</B> %d<BR>\n", end - start);
if (strand != NULL && differentString(strand,".") && isNotEmpty(strand))
    printf("<B>Strand:</B> %s<BR>\n", strand);
else
    strand = "?";
if (featDna && end > start)
    {
    char *tbl = cgiUsualString("table", cgiString("g"));
    strand = cgiEncode(strand);
    printf("<A HREF=\"%s&o=%d&g=getDna&i=%s&c=%s&l=%d&r=%d&strand=%s&table=%s\">"
	   "View DNA for this feature</A>  (%s/%s)<BR>\n",  hgcPathAndSettings(),
	   start, (item != NULL ? cgiEncode(item) : ""),
	   cgiEncode(chrom), start, end, strand, tbl, trackHubSkipHubName(database), trackHubSkipHubName(hGenome(database)));
    }
}

void printPosOnScaffold(char *chrom, int start, int end, char *strand)
/* Print position lines referenced to scaffold.  'strand' argument may be null. */
{
    char *scaffoldName;
    int scaffoldStart, scaffoldEnd;

    if (!hScaffoldPos(database, chrom, start, end, &scaffoldName, &scaffoldStart, &scaffoldEnd))
        {
        printPosOnChrom(chrom, start,end,strand, FALSE, NULL);
        return;
        }
    printf("<B>Scaffold:</B> %s<BR>\n", scaffoldName);
    printf("<B>Begin in Scaffold:</B> %d<BR>\n", scaffoldStart+1);
    printf("<B>End in Scaffold:</B> %d<BR>\n", scaffoldEnd);
    printf("<B>Genomic Size:</B> %d<BR>\n", scaffoldEnd - scaffoldStart);
    if (strand != NULL)
	printf("<B>Strand:</B> %s<BR>\n", strand);
    else
	strand = "?";
}

void printPos(char *chrom, int start, int end, char *strand, boolean featDna,
	      char *item)
/* Print position lines.  'strand' argument may be null. */
{
if (sameWord(organism, "Fugu"))
    /* Fugu is the only chrUn-based scaffold assembly, so it
     * has non-general code here.  Later scaffold assemblies
     * treat scaffolds as chroms.*/
    printPosOnScaffold(chrom, start, end, strand);
else
    printPosOnChrom(chrom, start, end, strand, featDna, item);
}

void samplePrintPos(struct sample *smp, int smpSize)
/* Print first three fields of a sample 9 type structure in
 * standard format. */
{
if ( smpSize != 9 )
    errAbort("Invalid sample entry!\n It has %d fields instead of 9\n",
             smpSize);

printf("<B>Item:</B> %s<BR>\n", smp->name);
printf("<B>Score:</B> %d<BR>\n", smp->score);
printf("<B>Strand:</B> %s<BR>\n", smp->strand);
printPos(smp->chrom, smp->chromStart, smp->chromEnd, NULL, TRUE, smp->name);
}


void bedPrintPos(struct bed *bed, int bedSize, struct trackDb *tdb)
/* Print first bedSize fields of a bed type structure in
 * standard format. */
{
char *strand = NULL;
if (bedSize >= 4 && bed->name[0] != 0)
    {
    char *label = "Item", *tdbLabel = NULL;
    if (tdb && ((tdbLabel = trackDbSetting(tdb, "bedNameLabel")) != NULL))
	label = tdbLabel;
    printf("<B>%s:</B> %s<BR>\n", label, bed->name);
    }
if (bedSize >= 5)
    {
    if (!tdb || !trackDbSetting(tdb, "noScoreFilter"))
        {
        char *scoreLabel = trackDbSettingOrDefault(tdb, "scoreLabel", "Score");
	printf("<B>%s:</B> %d<BR>\n", scoreLabel, bed->score);
        }
    }
if (bedSize >= 6)
   {
   strand = bed->strand;
   }
printPos(bed->chrom, bed->chromStart, bed->chromEnd, strand, TRUE, bed->name);

}

void genericHeader(struct trackDb *tdb, char *item)
/* Put up generic track info. */
{
if (item != NULL && item[0] != 0)
    cartWebStart(cart, database, "%s: %s (%s)", genome, tdb->longLabel, item);
else
    cartWebStart(cart, database, "%s: %s", genome, tdb->longLabel);

// QA noticed that clicking the +- buttons to collapse item detail tables was
// generating messages in the Apache log if you went directly to an item page
// without first visiting hgTracks. Clicking those buttons causes a cartDump
// in order to save the state of visibility of the table, which in
// turn needs an hgsid in order to save the state correctly. However, because
// we aren't in a form, we have never saved the hgsid to a hidden
// input element, and so the javascript that creates the cartDump link attaches
// an empty 'hgsid=' parameter, which cartDump doesn't like. Since we aren't in
// a form, use the 'common' object to store the parameter so the links to cartDump
// are correct:
jsInlineF("var common = {hgsid:\"%s\"};\n", cartSessionId(cart));
}

void printItemDetailsHtml(struct trackDb *tdb, char *itemName)
/* if track has an itemDetailsHtml, retrieve and print the HTML */
{
char *tableName = trackDbSetting(tdb, "itemDetailsHtmlTable");
if (tableName != NULL)
    {
    struct sqlConnection *conn = hAllocConn(database);
    struct itemDetailsHtml *html, *htmls;
    // if the details table has chrom/start/end columns, then use these to lookup html
    if (sqlColumnExists(conn, tableName, "chrom"))
        {
        char *chrom = cgiString("c");
        int start   = cgiInt("o");
        int end     = cgiInt("t");

        htmls = sqlQueryObjs(conn, (sqlLoadFunc)itemDetailsHtmlLoad, sqlQueryMulti,
                       "select name, html from %s where \
                       name = '%s' and \
                       chrom = '%s' and \
                       start = '%d' and \
                       end = '%d'", tableName, itemName, chrom, start, end);
        }
    // otherwise, assume that the itemName is unique 
    else 
        htmls = sqlQueryObjs(conn, (sqlLoadFunc)itemDetailsHtmlLoad, sqlQueryMulti,
                       "select name, html from %s where name = '%s'", tableName, itemName);

    for (html = htmls; html != NULL; html = html->next)
        printf("<br>\n%s\n", html->html);
    itemDetailsHtmlFreeList(&htmls);
    hFreeConn(&conn);
    }
}

char *getIdInUrl(struct trackDb *tdb, char *itemName)
/* If we have an idInUrlSql tag, look up itemName in that, else just
 * return itemName. */
{
char *sql = trackDbSetting(tdb, "idInUrlSql");
char *id = itemName;
if (sql != NULL)
    {
    char query[1024];
    sqlSafef(query, sizeof(query), sql, itemName);
    struct sqlConnection *conn = hAllocConn(database);
    id = sqlQuickString(conn, query);
    hFreeConn(&conn);
    }
return id;
}

char *getUrlSetting(struct trackDb *tdb, char* urlSetting)
/* get the "url" setting for the current track */
{
char *url;
if (sameWord(urlSetting, "url"))
    url = tdb->url;
else
    url = trackDbSetting(tdb, urlSetting);
return url;
}

void printIframe(struct trackDb *tdb, char *itemName)
/* print an iframe with the URL specified in trackDb (iframeUrl), can have 
 * the standard codes in it (like $$ for itemName, etc)
 */
{
char *url = getUrlSetting(tdb, "iframeUrl");
if (url==NULL)
    return;
char *eUrl = replaceInUrl(url, itemName, cart, database, seqName, winStart, winEnd, 
                                tdb->track, FALSE, NULL);
if (eUrl==NULL)
    return;

char *iframeOptions = trackDbSettingOrDefault(tdb, "iframeOptions", "width='100%%' height='1024'");
// Resizing requires the hgcDetails pages to include a bit of javascript.
//
// Explanation how this works and why the javascript is needed:
// http://stackoverflow.com/questions/153152/resizing-an-iframe-based-on-content
// In short:
// - iframes have a fixed size in html, resizing can only be done in javascript
// - the iframed page cannot call the resize() function in the hgc html directly, as they have
//   been loaded from different webservers
// - one way around it is that the iframed page includes a helper page on our server and 
//   send their size to the helper page (pages can call functions of included pages)
// - the helper page then sends the size back to hgc (pages on the same server can
//   call each others' functions)
//   width='%s' height='%s' src='%s' seamless scrolling='%s' frameborder='%s'

printf(" \
<script> \
function resizeIframe(height) \
{ \
     document.getElementById('hgcIframe').height = parseInt(height)+10; \
} \
</script> \
 \
<iframe id='hgcIframe' src='%s' %s></iframe> \
<p>", eUrl, iframeOptions);
}

void printCustomUrlWithLabel(struct trackDb *tdb, char *itemName, char *itemLabel, 
                                char *urlSetting, boolean encode, struct slPair *fields)
/* Print custom URL specified in trackDb settings. */
{
char urlLabelSetting[32];

// replace the $$ and other wildchards with the url given in tdb 
char *url = getUrlSetting(tdb, urlSetting);
//char* eUrl = constructUrl(tdb, url, itemName, encode);
if (url==NULL || isEmpty(url))
    return;

char *eUrl = replaceInUrl(url, itemName, cart, database, seqName, winStart, winEnd, tdb->track, 
                            encode, fields);
if (eUrl==NULL)
    return;

/* create the url label setting for trackDb from the url
   setting prefix */
safef(urlLabelSetting, sizeof(urlLabelSetting), "%sLabel", urlSetting);
char *linkLabel = trackDbSettingOrDefault(tdb, urlLabelSetting, "Outside Link:");
char *eLinkLabel = replaceInUrl(linkLabel, itemName, cart, database, seqName, winStart, winEnd, tdb->track,
                            encode, fields);

// if we got no item name from hgTracks or the item name does not appear in the URL
// there is no need to show the item name at all
if (isEmpty(itemName) || !stringIn("$$", url))
    {
    printf("<A TARGET=_blank HREF='%s'>%s</A><BR>",eUrl, eLinkLabel);
    return;
    }

printf("<B>%s </B>",eLinkLabel);

printf("<A HREF=\"%s\" target=_blank>", eUrl);

if (sameWord(tdb->table, "npredGene"))
    {
    printf("%s (%s)</A><BR>\n", itemName, "NCBI MapView");
    }
else
    {
    char *label = itemName;
    if (isNotEmpty(itemLabel) && differentString(itemName, itemLabel))
        label = itemLabel;
    printf("%s</A><BR>\n", label);
    }
//freeMem(&eUrl); small memory leak
}

void printCustomUrlWithFields(struct trackDb *tdb, char *itemName, char *itemLabel, boolean encode,                                                      struct slPair *fields) 
/* Wrapper to call printCustomUrlWithLabel with additional fields to substitute */
{
char urlSetting[10];
safef(urlSetting, sizeof(urlSetting), "url");

printCustomUrlWithLabel(tdb, itemName, itemLabel, urlSetting, encode, fields);
}

void printCustomUrl(struct trackDb *tdb, char *itemName, boolean encode)
/* Wrapper to call printCustomUrlWithLabel using the url setting in trackDb */
{
printCustomUrlWithFields(tdb, itemName, itemName, encode, NULL);
}

void printOtherCustomUrlWithFields(struct trackDb *tdb, char *itemName, char *urlSetting, 
                                        boolean encode, struct slPair *fields)
/* Wrapper to call printCustomUrlWithLabel to use another url setting other than url in trackDb e.g. url2, this allows the use of multiple urls for a track
 to be set in trackDb. */
{
printCustomUrlWithLabel(tdb, itemName, itemName, urlSetting, encode, fields);
}

void printOtherCustomUrl(struct trackDb *tdb, char *itemName, char* urlSetting, boolean encode)
/* Wrapper to call printCustomUrlWithLabel to use another url setting other than url in trackDb e.g. url2, this allows the use of multiple urls for a track
 to be set in trackDb. */
{
printCustomUrlWithLabel(tdb, itemName, itemName, urlSetting, encode, NULL);
}

void genericSampleClick(struct sqlConnection *conn, struct trackDb *tdb,
                        char *item, int start, int smpSize)
/* Handle click in generic sample (wiggle) track. */
{
char table[HDB_MAX_TABLE_STRING];
boolean hasBin;
struct sample *smp;
char query[512];
struct sqlResult *sr;
char **row;
boolean firstTime = TRUE;

if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, &hasBin))
    errAbort("genericSampleClick track %s not found", tdb->table);
sqlSafef(query, sizeof query, "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d",
        table, item, seqName, start);

/*errAbort( "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d",
          table, item, seqName, start);*/


sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (firstTime)
	firstTime = FALSE;
    else
	htmlHorizontalLine();
    smp = sampleLoad(row+hasBin);
    samplePrintPos(smp, smpSize);
    }
}

void showBedTopScorers(struct bed *bedList, char *item, int start, int max)
/* Show a list of track items sorted by descending score,
 *  with current item highlighted.
 *  max is upper bound on how many items will be displayed. */
{
int i;
struct bed *bed;

puts("<B>Top-scoring elements in window:</B><BR>");
for (i=0, bed=bedList;  bed != NULL && i < max;  bed=bed->next, i++)
    {
    if (sameWord(item, bed->name) && bed->chromStart == start)
        printf("&nbsp;&nbsp;&nbsp;<B>%s</B> ", bed->name);
    else
        printf("&nbsp;&nbsp;&nbsp;%s ", bed->name);
    printf("(%s:%d-%d) %d<BR>\n",
               bed->chrom, bed->chromStart+1, bed->chromEnd, bed->score);

    }
if (bed != NULL)
    printf("(list truncated -- more than %d elements)<BR>\n", max);
}

void showBedTopScorersInWindow(struct sqlConnection *conn,
			       struct trackDb *tdb, char *item, int start,
			       int maxScorers, char *filterTable, int filterCt)
/* Show a list of track items in the current browser window, ordered by
 * score.  Track must be BED 5 or greater.  maxScorers is upper bound on
 * how many items will be displayed. If filterTable is not NULL and exists,
 * it contains the 100K top-scorers in the entire track, and filterCt
 * is the threshold for how many are candidates for display. */
{
struct sqlResult *sr = NULL;
char **row = NULL;
struct bed *bedList = NULL, *bed = NULL;
char table[HDB_MAX_TABLE_STRING];
boolean hasBin = FALSE;
char query[512];

if (filterTable)
    {
    /* Track display only shows top-scoring N elements -- restrict
     * the list to these.  Get them from the filter table */
    hasBin = hOffsetPastBin(database, hDefaultChrom(database), filterTable);
    sqlSafef(query, sizeof(query), "select * from %s order by score desc limit %d",
            filterTable, filterCt);
    }
else
    {
    if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, &hasBin))
	errAbort("showBedTopScorersInWindow track %s not found", tdb->table);
    
    sqlSafef(query, sizeof(query),
          "select * from %s where chrom = '%s' and chromEnd > %d and "
          "chromStart < %d order by score desc",
          table, seqName, winStart, winEnd);
    }
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row+hasBin, 5);
    if (!filterTable
    ||  (  sameString(bed->chrom, seqName)
        && bed->chromStart < winEnd
        && bed->chromEnd > winStart))
        {
        slAddHead(&bedList, bed);
        }
    else
        bedFree(&bed);
    }
sqlFreeResult(&sr);
if (bedList == NULL)
    return;
slReverse(&bedList);
showBedTopScorers(bedList, item, start, maxScorers);
}

void getBedTopScorers(struct sqlConnection *conn, struct trackDb *tdb,
                   char *table, char *item, int start, int bedSize)
/* This function determines if showTopScorers is set in trackDb and also */
/* if the filterTopScorers setting is on. Then it passes the relevant */
/* settings to showBedTopScorersInWindow() so that the top N scoring */
/* items in the window are listed on the details page */
{
char *showTopScorers = trackDbSetting(tdb, "showTopScorers");
char *filterTopScorers = trackDbSetting(tdb,"filterTopScorers");
boolean doFilterTopScorers = FALSE;
char *words[3];
char cartVar[512];
int filterTopScoreCt = 0;
char *filterTopScoreTable = NULL;

safef(cartVar, sizeof cartVar, "%s.%s", table, "filterTopScorersOn");
if (filterTopScorers != NULL)
    {
    if (chopLine(cloneString(filterTopScorers), words) == 3)
        {
        doFilterTopScorers = sameString(words[0], "on");
        filterTopScoreCt = atoi(words[1]);
        filterTopScoreTable = words[2];
        }
    }

if (bedSize >= 5 && showTopScorers != NULL)
    {
    /* list top-scoring elements in window */
    int maxScorers = sqlUnsigned(showTopScorers);
    doFilterTopScorers = cartCgiUsualBoolean(cart, cartVar, doFilterTopScorers);
    if (doFilterTopScorers && hTableExists(database, filterTopScoreTable))
        {
        /* limit to those in the top N, from table */
        safef(cartVar, sizeof cartVar, "%s.%s", table, "filterTopScorersCt");
        filterTopScoreCt = cartCgiUsualInt(cart, cartVar, filterTopScoreCt);
        }
    else
        /* show all */
        filterTopScoreTable = NULL;
    showBedTopScorersInWindow(conn, tdb, item, start, maxScorers,
                                filterTopScoreTable, filterTopScoreCt);
    }
}

void linkToOtherBrowser(char *otherDb, char *chrom, int start, int end);
void linkToOtherBrowserExtra(char *otherDb, char *chrom, int start, int end, char *extra);

static void printCompareGenomeLinks(struct trackDb *tdb,char *name)
/* if "compareGenomeLinks" exists then a table of the same name in n different databases is sought.
   if a row exist in the other db table matching the current item, then a link is printed */
{
char *setting = trackDbSettingClosestToHome(tdb,"compareGenomeLinks");
if (setting == NULL)
    return;
struct sqlConnection *conn = hAllocConn(database); // Need only to connect to one db
if (conn == NULL)
    return;

char *words[20];
setting = cloneString(setting);
int ix,cnt = chopLine(setting, words);
char query[512];
char extra[128];
boolean gotOne = FALSE;
for (ix=0;ix<cnt;ix++)
    {
    char *db = words[ix];          // db.table.column=title or db.table=title or db=title
    char *table,*column = "name";
    char *title = strchr(words[ix],'=');
    if (title==NULL) // Must have title
        continue;
    *title = '\0';
    title++;
    if ((table = strchr(words[ix],'.')) == NULL)
        table = tdb->table;
    else
        {
        *table++ = '\0';  // assigns before advance
        if ((words[ix] = strchr(table,'.')) != NULL)
            {
            *words[ix] = '\0';
            column = ++words[ix]; // advance before assigns
            }
        }
    sqlSafef(query,sizeof(query),"select chrom,chromStart,chromEnd from %s.%s where %s=\"%s\";",
          db,table,column,name);
    struct sqlResult *sr = sqlGetResult(conn, query);
    if (sr == NULL)
        continue;
    char **row = sqlNextRow(sr);
    if (row == NULL)
        continue;
    char *chrom = *row++;
    int beg = atoi(*row++);
    int end = atoi(*row);
    if (!gotOne)
        {
        gotOne = TRUE;
        printf("<P>The item \"%s\" has been located in other genomes:\n<UL>\n",name);
        }
    printf("<LI>");
    safef(extra,sizeof(extra),"%s=full",tdb->track);
    linkToOtherBrowserExtra(db, chrom, beg, end, extra);
    printf("%s</A></LI>\n",strSwapChar(title,'_',' '));
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
freeMem(setting);
if (gotOne)
    printf("</UL>\n");
else
    printf("<P>Currently the item \"%s\" has not been located in another genome.\n",name);
}

void mafPrettyOut(FILE *f, struct mafAli *maf, int lineSize,
	boolean onlyDiff, int blockNo, struct hash *hash);

void doAtom( struct trackDb *tdb, char *item)
{
char table[HDB_MAX_TABLE_STRING];
boolean hasBin;
//struct bed *bed;
char query[512];
struct sqlResult *sr;
char **row;
//boolean firstTime = TRUE;
int start = cartInt(cart, "o");
//struct sqlConnection *conn = hAllocConn(database);
char *user = cfgOption("db.user");
char *password = cfgOption("db.password");
struct sqlConnection *sc;
struct atom ret;

genericHeader(tdb, item);
if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, &hasBin))
    errAbort("mafPrettyOut track %s not found", tdb->table);
#if 0
sqlSafef(query, sizeof query, "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d", table, escapedName, seqName, start);
sr = sqlGetResult(conn, query);
printf("<B>This is the item you clicked on:</B><BR>\n");
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (firstTime)
	firstTime = FALSE;
    else
	htmlHorizontalLine();
    bed = bedLoadN(row+hasBin, 4);
    bedPrintPos(bed, 4, tdb);
    }
sqlFreeResult(&sr);

sqlSafef(query, sizeof query, "select * from %s where name = '%s'", table, escapedName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row+hasBin, 4);
    if (bed->chromStart != start)
	{
	htmlHorizontalLine();
	firstTime = FALSE;
	printf("<B>Another instances on %s:</B><BR>\n",database);
	bedPrintPos(bed, 4, tdb);
	}
    }
sqlFreeResult(&sr);
#endif

sc = sqlConnectRemote("localhost", user, password, "hgFixed");
sqlSafef(query, sizeof(query),
      "select * from %s where name = '%s'", table, item);
sr = sqlGetResult(sc, query);
printf("<B>Atom %s instances ('*' marks item you clicked on)</B><BR>\n",item);
printf("<PRE>\n");
//printf("Ins#\tSpecies\t\tChrom\tStart\tEnd\tStrand\n");
printf( "     # %-10s %-5s %12s %12s %10s    %s  %-10s %-10s\n",
    "species","chrom", "start", "end", "length", "strand","fivePrime","threePrime");
while ((row = sqlNextRow(sr)) != NULL)
    {
    atomStaticLoad(row, &ret);
    //atomOutput(&ret, stdout, '\t', '\n');
    linkToOtherBrowser(ret.species, ret.chrom, ret.start, ret.end);
    if (sameString(ret.chrom, seqName) && (start  == ret.start) &&
	sameString(ret.species, database))
	printf("* ");
    else
        printf("  ");
    printf( "%4d %-10s %-5s %12d %12d %10d      %c      %-10s %-10s\n",
            ret.instance, ret.species,ret.chrom, ret.start + 1, ret.end,
            ret.end - ret.start + 1, ret.strand[0],ret.fivePrime,ret.threePrime);
    }
printf("</A>");
sqlFreeResult(&sr);

if (!sameString("atom20080226d", table))
    return;

printf("<TABLE>");
printf("<THEAD>");
printf("<TBODY>");
printf("<TR><TH>");
printf("Suh Trees<BR>\n");
printf("<IMG src=http://hgwdev.gi.ucsc.edu/~braney/suhTrees/%s.tt.png><BR>",item);
printf("<TD><IMG src=http://hgwdev.gi.ucsc.edu/~braney/suhTrees/%s.gt.png><BR>",item);
printf("<TR><TH>");
printf("NJ Trees<BR>\n");
printf("<IMG src=http://hgwdev.gi.ucsc.edu/~braney/njTrees/%s.tt.png><BR>",item);
printf("<TD><IMG src=http://hgwdev.gi.ucsc.edu/~braney/njTrees/%s.gt.png><BR>",item);
printf("<TR><TH>");
/*
printf("Gap UPGMA Trees<BR>\n");
printf("<IMG src=http://hgwdev.gi.ucsc.edu/~braney/gap992Trees/%s.tt.png><BR>",item);
printf("<TD><IMG src=http://hgwdev.gi.ucsc.edu/~braney/gap992Trees/%s.gt.png><BR>",item);
printf("</TABLE>");

*/
return;

char buffer[4096];
struct mafFile *mf;
safef(buffer, sizeof buffer, "/gbdb/hgFixed/%s/%s.maf",table, item);
mf = mafMayOpen(buffer);
if (mf != NULL)
    {
    mafFileFree(&mf);
    mf = mafReadAll(buffer);
    struct mafAli *mafAli;
    int count = 1;
    int numBlocks = 0;

    for (mafAli=mf->alignments; mafAli; mafAli = mafAli->next)
	numBlocks++;

    for (mafAli=mf->alignments; mafAli; mafAli = mafAli->next)
	{
	printf("<BR><B>Multiple Alignment Block %d of %d</B><BR>",
	    count, numBlocks);
	mafPrettyOut(stdout, mafAli, 70, FALSE, count++, NULL);
	if (mafAli->next != NULL)
	    {
	    struct mafAli *next = mafAli->next;
	    struct mafComp *comp1 = mafAli->components;
	    struct mafComp *comp2 = next->components;

	    printf("<BR><B>Gaps:</B>\n");
	    for(; comp1 ; comp1 = comp1->next, comp2 = comp2->next)
		{
		int diff;
		char dbOnly[4096];

		diff = comp2->start - (comp1->start + comp1->size);

		safef(dbOnly, sizeof(dbOnly), "%s", comp1->src);
		chopPrefix(dbOnly);
		printf("%-20s %d\n",hOrganism(dbOnly), diff);
		}

	    printf("<BR>");
	    }
        }
    }
}

char **getIdNameMap(struct trackDb *tdb, struct asColumn *col, int *size)
/* Allocate and fill an array mapping id to name.  Currently limited to specific columns. */
{
char *idNameTable = trackDbSetting(tdb, "sourceTable");
if (!idNameTable || differentString("sourceIds", col->name))
    return NULL;

struct sqlResult *sr;
char query[256];
char **row;
char **idNames;

sqlSafef(query, sizeof(query), "select max(id) from %s", idNameTable);
struct sqlConnection *conn = hAllocConnTrack(database, tdb);
int maxId = sqlQuickNum(conn, query);
AllocArray(idNames, maxId+1);
sqlSafef(query, sizeof(query), "select id, name from %s", idNameTable);
sr = sqlGetResult(conn, query);
int id;
while ((row = sqlNextRow(sr)) != NULL)
    {
    id = sqlUnsigned(row[0]);
    if (id > maxId)
        errAbort("Internal error:  id %d > maxId %d in %s", id, maxId, idNameTable);
    idNames[id] = cloneString(row[1]);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
if (size)
    *size = maxId+1;
return idNames;
}

void printIdOrLinks(struct asColumn *col, struct hash *fieldToUrl, struct trackDb *tdb, char *idList)
/* if trackDb does not contain a "urls" entry for current column name, just print idList as it is.
 * Otherwise treat idList as a comma-sep list of IDs and print one row per id, with a link to url,
 * ($$ in url is OK, wildcards like $P, $p, are also OK)
 * */
{
// try to find a fieldName=url setting in the "urls" tdb statement, print id if not found
char *url = NULL;
if (fieldToUrl != NULL)
    url = (char*)hashFindVal(fieldToUrl, col->name);
if (url == NULL)
    {
    printf("<td class='bedExtraTblVal'>%s</td></tr>\n", idList);
    return;
    }

// split the id into parts and print each part as a link
struct slName *slIds = slNameListFromComma(idList);
struct slName *itemId = NULL;

// handle id->name mapping for multi-source items
int nameCount;
char **idNames = getIdNameMap(tdb, col, &nameCount);

printf("<td>");
for (itemId = slIds; itemId!=NULL; itemId = itemId->next) 
    {
    if (itemId != slIds)
        printf(", ");
    char *itemName = trimSpaces(itemId->name);

    if (idNames)
        {
        unsigned int id = sqlUnsigned(itemName);
        if (id < nameCount)
            itemName = idNames[sqlUnsigned(itemName)];
        }

    // a | character can optionally be used to separate the ID used for $$ from the name shown in the link (like in Wikimedia markup)
    char *idForUrl = itemName;
    boolean encode = TRUE;
    if (strstr(itemName, "|"))
        {
        char *parts[2];
	chopString(itemName, "|", parts, ArraySize(parts));
        idForUrl = parts[0];
        itemName = parts[1];
        encode = FALSE; // assume the link is already encoded
        }
    if (startsWith("http", itemName)) // the ID may be a full URL already, encoding would destroy it
        encode = FALSE;

    char *idUrl = replaceInUrl(url, idForUrl, cart, database, seqName, winStart, 
                    winEnd, tdb->track, encode, NULL);
    printf("<a href=\"%s\" target=\"_blank\">%s</a>", idUrl, itemName);
    } 
printf("</td></tr>\n");
freeMem(slIds);
//freeMem(idNames);
}

char *readOneLineMaybeBgzip(char *fileOrUrl, bits64 offset, bits64 len)
/* If fileOrUrl is bgzip-compressed and indexed, then use htslib's bgzf functions to
 * retrieve uncompressed data from offset; otherwise (plain text) use udc. If len is 0,
 * read up to next '\n' delimiter. */
{
char *line = needMem(len+1);
if (endsWith(fileOrUrl, ".gz"))
    {
    BGZF *fp = bgzf_open(fileOrUrl, "r");
    kstring_t str = { 0, 0, NULL };
    if (bgzf_index_load(fp, fileOrUrl, ".gzi") < 0)
        errAbort("bgzf_index_load failed to load .gzi index for %s", fileOrUrl);
    if (bgzf_useek(fp, offset, SEEK_SET) < 0)
        errAbort("bgzf_useek failed to seek to uncompressed offset %lld in %s", offset, fileOrUrl);

    // bgzf_getline is faster than bgzf_read(), so we only use the len param for error checking
    bits64 count = bgzf_getline(fp, '\n', &str);
    if (count == 0)
        errAbort("bgzf_getline unexpected end of file while parsing '%s'", fileOrUrl);
    else if (count < 0)
        errAbort("bgzf_getline unexpected error while parsing '%s'", fileOrUrl);
    else if (len > 0 && count != len)
        errAbort("bgzf_getline failed to read %lld bytes at uncompressed offset %lld in %s, got %lld",
                     len, offset, fileOrUrl, count);
    line = ks_release(&str);
    bgzf_close(fp);
    }
else
    {
    struct udcFile *udcF = udcFileOpen(fileOrUrl, NULL);
    udcSeek(udcF, offset);
    line = udcReadLine(udcF);
    if (line == NULL)
        errAbort("error reading line from '%s'", fileOrUrl);
    udcFileClose(&udcF);
    }
return line;
}

int extraFieldsStart(struct trackDb *tdb, int fieldCount, struct asObject *as)
/* return the index of the first extra field */
{
int start = 0;
char *type = cloneString(tdb->type);

if (sameString(type, "bedMethyl"))
    return 9;
char *word = nextWord(&type);
if (word && (sameWord(word,"bed") || startsWith("big", word)))
    {
    if (NULL != (word = nextWord(&type)))
        start = sqlUnsigned(word);
    else // custom beds and bigBeds may not have full type "begBed 9 +"
        start = max(0,slCount(as->columnList) - fieldCount);
    }
return start;
}

struct slPair *getExtraFields(struct trackDb *tdb, char **fields, int fieldCount)
/* return the extra field names and their values as a list of slPairs.  */
{
struct asObject *as = asForDb(tdb, database);
if (as == NULL)
    return NULL;
struct asColumn *col = as->columnList;

int start = extraFieldsStart(tdb, fieldCount, as);
// skip past known fields
for (;start !=0 && col != NULL;col=col->next)
    if (start > 0)
        start--;

struct slPair *extraFields = 0;
int count = 0;
for (;col != NULL && count < fieldCount;col=col->next)
    {
    struct slPair *slp;
    AllocVar(slp);
    char *fieldName = col->name;
    char *fieldVal = fields[count];
    slp->name = fieldName;
    slp->val = fieldVal;
    slAddHead(&extraFields, slp);
    count++;
    //printf("name %s, val %s, idx %d<br>", fieldName, fieldVal, count);
    }
slReverse(extraFields);
return extraFields;
}

struct slPair *getFields(struct trackDb *tdb, char **row)
/* return field names and their values as a list of slPairs.  */
// TODO: refactor with getExtraFields
{
struct asObject *as = asForDb(tdb, database);
if (as == NULL)
    return NULL;
int fieldCount = slCount(as->columnList);
struct slPair *fields = NULL;
struct asColumn *col = as->columnList;
int count = 0;
for (count = 0; col != NULL && count < fieldCount; col = col->next)
    {
    struct slPair *field;
    AllocVar(field);
    char *fieldName = col->name;
    char *fieldVal = row[count];
    field->name = fieldName;
    field->val = fieldVal;
    slAddHead(&fields, field);
    count++;
    }
slReverse(fields);
return fields;
}

void printEmbeddedTable(struct trackDb *tdb, struct embeddedTbl *thisTbl, struct dyString *dy)
// Pretty print a '|' and ';' encoded table or a JSON encoded table from a bigBed field
{
jsIncludeFile("hgc.js", NULL);
if (isNotEmpty(thisTbl->encodedTbl))
    {
    if (startsWith("_json", thisTbl->field) || startsWith("json", thisTbl->field))
        {
        struct jsonElement *jsElem = NULL;
        struct errCatch *errCatch = errCatchNew();
        if (errCatchStart(errCatch))
            jsElem = jsonParse(thisTbl->encodedTbl);
        errCatchEnd(errCatch);
        if (errCatch->gotError)
            warn("ERROR: JSON field '%s' for track '%s' is malformed: %s", thisTbl->field, tdb->track, errCatch->message->string);
        else if (errCatch->gotWarning)
            warn("Warning: %s", errCatch->message->string);
        errCatchFree(&errCatch);
        if (jsElem != NULL)
            {
            dyStringPrintf(dy, "{label: \"%s\", data: %s},", thisTbl->title != NULL ? thisTbl->title : thisTbl->field, thisTbl->encodedTbl);
            }
        }
    else
        {
        printf("<tr><td>%s</td><td>", thisTbl->title);
        printf("<table class='jsonTable'>\n");
        printf("<tr><td>");
        char table[4096];
        safef(table, sizeof(table), "%s", thisTbl->encodedTbl);
        int swapped = strSwapStrs(table, 4096, ";", "</td></tr><tr><td>");
        if (swapped == -1)
            errAbort("Error substituting ';' for '</tr><tr>' in hgc.c:printEmbeddedTable()");
        swapped = strSwapStrs(table, 4096, "|", "</td><td>");
        if (swapped == -1)
            errAbort("Error substituting '|' for '</td><td>' in hgc.c:printEmbeddedTable()");
        printf("%s</tr>\n", table);
        printf("</table>\n");
        printf("</td></tr>\n");
        }
    }
}

void printExtraDetailsTable(char *trackName, char *tableName, char *fileName, struct dyString *tableText)
// convert a tab-sep table to HTML
{
struct lineFile *lf = lineFileOnString(fileName, TRUE, tableText->string);
char *description = tableName != NULL ? tableName : "Additional Details";
printf("<p><b>%s</b></p>\n", description);
printf("<table class=\"bedExtraTbl\">\n");
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    printf("<tr><td>");
    char *toPrint = replaceChars(line, "\t", "</td><td>");
    printf("%s", toPrint);
    printf("</td></tr>\n");
    }
printf("</table>\n"); // closes bedExtraTbl
}

static struct slName *findFieldsInExtraFile(char *detailsTableUrl, struct asColumn *col, struct dyString *ds)
// return a list of the ${}-enclosed fields from an extra file
{
struct slName *foundFields = NULL;
char *table = udcFileReadAllIfExists(hReplaceGbdb(detailsTableUrl), NULL, 0, NULL);
if (table)
    {
    for (; col != NULL; col = col->next)
        {
        char field[256];
        safef(field, sizeof(field), "${%s}", col->name);
        if (stringIn(field, table))
            {
            struct slName *replaceField = slNameNew(col->name);
            slAddHead(&foundFields, replaceField);
            }
        }
    dyStringPrintf(ds, "%s", table);
    if (foundFields)
        slReverse(foundFields);
    }
return foundFields;
}

#define TDB_DYNAMICTABLE_SETTING "detailsDynamicTable"
#define TDB_DYNAMICTABLE_SETTING_2 "extraTableFields"
void getExtraTableFields(struct trackDb *tdb, struct slName **retFieldNames, struct embeddedTbl **retList, struct hash *embeddedTblHash)
/* Parse the trackDb field TDB_DYNAMICTABLE_FIELD into the field names and titles specified,
 * and fill out a hash keyed on the bigBed field name (which may be in an external file
 * and not in the bigBed itself) to a helper struct for storing user defined tables. */
{
struct slName *tmp, *embeddedTblSetting = slNameListFromComma(trackDbSetting(tdb, TDB_DYNAMICTABLE_SETTING));
struct slName *embeddedTblSetting2 = slNameListFromComma(trackDbSetting(tdb, TDB_DYNAMICTABLE_SETTING_2));
char *title = NULL, *fieldName = NULL;
for (tmp = embeddedTblSetting; tmp != NULL; tmp = tmp->next)
    {
    fieldName = cloneString(tmp->name);
    if (strchr(tmp->name, '|'))
        {
        title = strchr(fieldName, '|');
        *title++ = 0;
        }
    struct embeddedTbl *new;
    AllocVar(new);
    new->field = fieldName;
    new->title = title != NULL ? cloneString(title) : fieldName;
    slAddHead(retList, new);
    slNameAddHead(retFieldNames, fieldName);
    hashAdd(embeddedTblHash, fieldName, new);
    }
for (tmp = embeddedTblSetting2; tmp != NULL; tmp = tmp->next)
    {
    fieldName = cloneString(tmp->name);
    if (strchr(tmp->name, '|'))
        {
        title = strchr(fieldName, '|');
        *title++ = 0;
        }
    struct embeddedTbl *new;
    AllocVar(new);
    new->field = fieldName;
    new->title = title != NULL ? cloneString(title) : fieldName;
    slAddHead(retList, new);
    slNameAddHead(retFieldNames, fieldName);
    hashAdd(embeddedTblHash, fieldName, new);
    }
}

void printFieldLabel(char *entry)
/* print the field label, the first column in the table, as a <td>. Allow a
 * longer description after a |-char, as some fields are not easy to
 * understand. */
{
char *afterPipe = strchr(entry, '|');
if (afterPipe)
    *afterPipe = 0;

printf("<tr><td>%s", entry);

if (afterPipe)
    {
    // Could also have a "?" icon and show the description on mouse over
    afterPipe++; // skip past | character
    printf("<br><span class='bedExtraTblNote'>%s</small>", afterPipe);
    }

puts("</td>");
}

#define TDB_STATICTABLE_SETTING "extraDetailsTable"
#define TDB_STATICTABLE_SETTING_2 "detailsStaticTable"
int extraFieldsPrintAs(struct trackDb *tdb,struct sqlResult *sr,char **fields,int fieldCount, struct asObject *as)
// Any extra bed or bigBed fields (defined in as and occurring after N in bed N + types.
// sr may be null for bigBeds.
// Returns number of extra fields actually printed.
{
// We are trying to print extra fields so we need to figure out how many fields to skip
int start = extraFieldsStart(tdb, fieldCount, as);

struct asColumn *col = as->columnList;
char *urlsStr = trackDbSettingClosestToHomeOrDefault(tdb, "urls", NULL);
struct hash* fieldToUrl = hashFromString(urlsStr);
boolean skipEmptyFields = trackDbSettingOn(tdb, "skipEmptyFields");

// make list of fields to skip
char *skipFieldsStr = trackDbSetting(tdb, "skipFields");
struct slName *skipIds = NULL;
if (skipFieldsStr)
    skipIds = slNameListFromComma(skipFieldsStr);

// make list of fields that are separated from other fields
char *sepFieldsStr = trackDbSetting(tdb, "sepFields");
struct slName *sepFields = NULL;
if (sepFieldsStr)
    sepFields = slNameListFromComma(sepFieldsStr);

// make list of fields that we want to substitute
// this setting has format description|URLorFilePath, with the stuff before the pipe optional
char *extraDetailsTableName = NULL, *extraDetails = cloneString(trackDbSetting(tdb, TDB_STATICTABLE_SETTING));
if (extraDetails && strchr(extraDetails,'|'))
    {
    extraDetailsTableName = extraDetails;
    extraDetails = strchr(extraDetails,'|');
    *extraDetails++ = 0;
    }
struct dyString *extraTblStr = dyStringNew(0);
struct slName *detailsTableFields = NULL;
if (extraDetails)
    detailsTableFields = findFieldsInExtraFile(extraDetails, col, extraTblStr);
char *extraDetails2TableName = NULL, *extraDetails2 = cloneString(trackDbSetting(tdb, TDB_STATICTABLE_SETTING_2));
if (extraDetails2 && strchr(extraDetails2,'|'))
    {
    extraDetails2TableName = extraDetails2;
    extraDetails2 = strchr(extraDetails2,'|');
    *extraDetails2++ = 0;
    }
struct dyString *extraTbl2Str = dyStringNew(0);
struct slName *detailsTable2Fields = NULL;
if (extraDetails2)
    detailsTable2Fields = findFieldsInExtraFile(extraDetails2, col, extraTbl2Str);

struct hash *embeddedTblHash = hashNew(0);
struct slName *embeddedTblFields = NULL;
struct embeddedTbl *embeddedTblList = NULL;
getExtraTableFields(tdb, &embeddedTblFields, &embeddedTblList, embeddedTblHash);

// iterate over fields, print as table rows
int count = 0;
int printCount = 0;
for (;col != NULL && count < fieldCount;col=col->next)
    {
    if (start > 0)  // skip past already known fields
        {
        start--;
        continue;
        }
    int ix = count;
    if (sr != NULL)
        {
        ix = sqlFieldColumn(sr, col->name); // If sr provided, name must match sql columnn name!
        if (ix == -1 || ix > fieldCount)      // so extraField really just provides a label
            continue;
        }

    char *fieldName = col->name;
    count++;

    // don't print this field if we are gonna print it later in a custom table
    if (detailsTableFields && slNameInList(detailsTableFields, fieldName))
        {
        int fieldLen = strlen(fieldName);
        char *replaceField = needMem(fieldLen+4);
        replaceField[0] = '$';
        replaceField[1] = '{';
        strcpy(replaceField+2, fieldName);
        replaceField[fieldLen+2] = '}';
        replaceField[fieldLen+3] = 0;
        extraTblStr = dyStringSub(extraTblStr->string, replaceField, fields[ix]);
        continue;
        }

    // don't print this field if we are gonna print it later in a custom table
    if (detailsTable2Fields && slNameInList(detailsTable2Fields, fieldName))
        {
        int fieldLen = strlen(fieldName);
        char *replaceField = needMem(fieldLen+4);
        replaceField[0] = '$';
        replaceField[1] = '{';
        strcpy(replaceField+2, fieldName);
        replaceField[fieldLen+2] = '}';
        replaceField[fieldLen+3] = 0;
        extraTbl2Str = dyStringSub(extraTbl2Str->string, replaceField, fields[ix]);
        continue;
        }

    // similar to above, if the field contains an embedded table skip it here
    // and print it later
    if (embeddedTblFields)
        {
        struct embeddedTbl *new = hashFindVal(embeddedTblHash, fieldName);
        if (new)
            {
            new->encodedTbl = fields[ix];
            // this field will get printed later somehow, so make sure the
            // rest of this code knows to not open more table tags and close
            // the correct ones
            printCount++;
            continue;
            }
        }

    // do not print a row if the fieldName from the .as file is in the "skipFields" list
    // or if a field name starts with _. This makes bigBed extra fields consistent with
    // external extra fields in that _ field names have some meaning and are not shown
    if (startsWith("_", fieldName) || (skipIds && slNameInList(skipIds, fieldName)))
        continue;

    // skip this row if it's empty and "skipEmptyFields" option is set
    if (skipEmptyFields && isEmpty(fields[ix]))
        continue;

    if (printCount == 0)
        printf("<br><table class='bedExtraTbl'>");

    // split this table to separate current row from the previous one, if the trackDb option is set
    if (sepFields && slNameInList(sepFields, fieldName))
        printf("</tr></table>\n<p>\n<table class='bedExtraTbl'>");

    // field description
    char *entry;
    if (sameString(fieldName, "cdsStartStat") && sameString("enum('none','unk','incmpl','cmpl')", col->comment))
        entry = "Status of CDS start annotation (none, unknown, incomplete, or complete)";
    else if (sameString(fieldName, "cdsEndStat") && sameString("enum('none','unk','incmpl','cmpl')", col->comment))
        entry = "Status of CDS end annotation (none, unknown, incomplete, or complete)";
    else
        entry = col->comment;

    printFieldLabel(entry);

    if (col->isList || col->isArray || col->lowType->stringy || asTypesIsInt(col->lowType->type))
        printIdOrLinks(col, fieldToUrl, tdb, fields[ix]);
    else if (asTypesIsFloating(col->lowType->type))
        {
        double valDouble = strtod(fields[ix],NULL);
        if (errno == 0 && valDouble != 0)
            printf("<td>%g</td></tr>\n", valDouble);
        else
            printf("<td>%s</td></tr>\n", fields[ix]); // decided not to print error
        }
    else
        printf("<td class='bedExtraTblVal'>%s</td></tr>\n", fields[ix]);
    printCount++;
    }
if (skipIds)
    slFreeList(skipIds);
if (sepFields)
    slFreeList(sepFields);

if (embeddedTblFields)
    {
    printf("<br><table class='bedExtraTbl'>\n");

    struct embeddedTbl *thisTbl;
    struct dyString *tableLabelsDy = dyStringNew(0);
    for (thisTbl = embeddedTblList; thisTbl != NULL; thisTbl = thisTbl->next)
        {
        if (thisTbl->encodedTbl)
            {
            dyStringPrintf(tableLabelsDy, "var _jsonHgcLabels = [");
            printEmbeddedTable(tdb, thisTbl, tableLabelsDy);
            dyStringPrintf(tableLabelsDy, "];\n");
            }
        }

    jsInline(dyStringCannibalize(&tableLabelsDy));
    }

if (printCount > 0)
    printf("</table>\n");


if (detailsTableFields)
    {
    printExtraDetailsTable(tdb->track, extraDetailsTableName, extraDetails, extraTblStr);
    }
if (detailsTable2Fields)
    {
    printExtraDetailsTable(tdb->track, extraDetails2TableName, extraDetails2, extraTbl2Str);
    }

return printCount;
}

int extraFieldsPrint(struct trackDb *tdb,struct sqlResult *sr,char **fields,int fieldCount)
// Any extra bed or bigBed fields (defined in as and occurring after N in bed N + types.
// sr may be null for bigBeds.
// Returns number of extra fields actually printed.
{
struct asObject *as = asForDb(tdb, database);
if (as == NULL)
    return 0;

int ret =  extraFieldsPrintAs(tdb, sr, fields,fieldCount, as);
//asObjectFree(&as);

return ret;
}


void genericBedClick(struct sqlConnection *conn, struct trackDb *tdb,
		     char *item, int start, int bedSize)
/* Handle click in generic BED track. */
{
char table[HDB_MAX_TABLE_STRING];
boolean hasBin;
struct bed *bed;
char query[512];
struct sqlResult *sr;
char **row;
boolean firstTime = TRUE;

char *liftDb = cloneString(trackDbSetting(tdb, "quickLiftDb"));

char *db = database;
char *sqlTable = tdb->table;
if (liftDb != NULL)
    {
    if (isCustomTrack(trackHubSkipHubName(tdb->track)))
        {
        liftDb = CUSTOM_TRASH;
        sqlTable = trackDbSetting(tdb, "dbTableName");
        }
    db = liftDb;
    }

if (!hFindSplitTable(db, seqName, tdb->table, table, sizeof table, &hasBin))
    errAbort("genericBedClick track %s not found", tdb->table);

if (liftDb)
    {
    struct hash *chainHash = newHash(10);
    char *quickLiftFile = cloneString(trackDbSetting(tdb, "quickLiftUrl"));
    bed = (struct bed *)quickLiftSql(conn, quickLiftFile, sqlTable, seqName, winStart, winEnd,  NULL, NULL, (ItemLoader2)bedLoadN, bedSize, chainHash);
    bedPrintPos(bed, bedSize, tdb);

    //extraFieldsPrint(tdb,sr,row,sqlCountColumns(sr));
    }
else 
    {
    if (bedSize <= 3)
        sqlSafef(query, sizeof query, "select * from %s where chrom = '%s' and chromStart = %d", table, seqName, start);
    else
        {
        struct hTableInfo *hti = hFindTableInfoWithConn(conn, seqName, tdb->table);
        if (hti && *hti->nameField && differentString("name", hti->nameField))
            sqlSafef(query, sizeof query, "select * from %s where %s = '%s' and chrom = '%s' and chromStart = %d",
                table, hti->nameField, item, seqName, start);
        else
            sqlSafef(query, sizeof query, "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d",
                table, item, seqName, start);
        }
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        if (firstTime)
            firstTime = FALSE;
        else
            htmlHorizontalLine();
        bed = bedLoadN(row+hasBin, bedSize);
        bedPrintPos(bed, bedSize, tdb);

        extraFieldsPrint(tdb,sr,row,sqlCountColumns(sr));

        // check for seq1 and seq2 in columns 7+8 (eg, pairedTagAlign)
        char *setting = trackDbSetting(tdb, BASE_COLOR_USE_SEQUENCE);
        if (bedSize == 6 && setting && sameString(setting, "seq1Seq2"))
            printf("<br><B>Sequence 1:</B> %s<br><B>Sequence 2:</B> %s<br>\n",row[hasBin+6], row[hasBin+7]);
        printCompareGenomeLinks(tdb,bed->name);
        }
    sqlFreeResult(&sr);
    getBedTopScorers(conn, tdb, table, item, start, bedSize);
    }
printItemDetailsHtml(tdb, item);
}

#define INTRON 10
#define CODINGA 11
#define CODINGB 12
#define UTR5 13
#define UTR3 14
#define STARTCODON 15
#define STOPCODON 16
#define SPLICESITE 17
#define NONCONSPLICE 18
#define INFRAMESTOP 19
#define INTERGENIC 20
#define REGULATORY 21
#define LABEL 22

#define RED 0xFF0000
#define GREEN 0x00FF00
#define LTGREEN 0x33FF33
#define BLUE 0x0000FF
#define MEDBLUE 0x6699FF
#define PURPLE 0x9900cc
#define BLACK 0x000000
#define CYAN 0x00FFFF
#define ORANGE 0xDD6600
#define BROWN 0x663300
#define YELLOW 0xFFFF00
#define MAGENTA 0xFF00FF
#define GRAY 0xcccccc
#define LTGRAY 0x999999
#define WHITE 0xFFFFFF

int setAttributeColor(int class)
{
switch (class)
    {
    case STARTCODON:
	return GREEN;
    case STOPCODON:
	return RED;
    case CODINGA:
	return MEDBLUE;
    case CODINGB:
	return PURPLE;
    case UTR5:
    case UTR3:
	return ORANGE;
    case INTRON:
	return LTGRAY;
    case SPLICESITE:
    case NONCONSPLICE:
	return BLACK;
    case INFRAMESTOP:
	return MAGENTA;
    case REGULATORY:
	return YELLOW;
    case INTERGENIC:
	return GRAY;
    case LABEL:
    default:
	return BLACK;
    }
}

void startColorStr(struct dyString *dy, int color, int track)
{
currentColor[track] = color;
if (prevColor[track] != currentColor[track])
    dyStringPrintf(dy,"</span><span style='color:#%06X'>",color);
}

void stopColorStr(struct dyString *dy, int track)
{
prevColor[track] = currentColor[track];
}

void addTag(struct dyString *dy, struct dyString *tag)
{
dyStringPrintf(dy,"<A name=%s></a>",tag->string);
}

void setClassStr(struct dyString *dy, int class, int track)
{
if (class == STARTCODON)
    dyStringAppend(dy,"<A name=startcodon></a>");
startColorStr(dy,setAttributeColor(class),track);
}

void resetClassStr(struct dyString *dy, int track)
{
stopColorStr(dy,track);
}

boolean isBlue(char *s)
/* check for <a href name=class</a> to see if this is colored blue (coding region)*/
{
    /* check for blue */
    if (strstr(s,"6699FF") == NULL)
        return FALSE;
    else
        return TRUE;
}
int numberOfGaps(char *q,int size)
/* count number of gaps in a string array */
{
int i;
int count = 0;
for (i = 0 ; i<size ; i++)
    if (q[i] == '-') count++;
return (count);
}

void pseudoGeneClick(struct sqlConnection *conn, struct trackDb *tdb,
                     char *item, int start, int bedSize)
/* Handle click in track. */
{
char table[HDB_MAX_TABLE_STRING];
boolean hasBin;
struct bed *bed;
char query[512];
struct sqlResult *sr;
char **row;
boolean firstTime = TRUE;

if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, &hasBin))
    errAbort("pseudoGeneClick track %s not found", tdb->table);
if (bedSize <= 3)
    sqlSafef(query, sizeof query, "select * from %s where chrom = '%s' and chromStart = %d", table, seqName, start);
else
    sqlSafef(query, sizeof query, "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d",
	    table, item, seqName, start);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (firstTime)
	firstTime = FALSE;
    else
	htmlHorizontalLine();
    bed = bedLoadN(row+hasBin, bedSize);
    bedPrintPos(bed, bedSize, tdb);
    }
}

void axtOneGeneOut(char *otherDb, struct axt *axtList, int lineSize,
                   FILE *f, struct genePred *gp, char *nibFile)
/* Output axt and orf in pretty format. */
{
struct axt *axt;
int oneSize;
int i;
int tCodonPos = 1;
int qCodonPos = 1;
int tStart;
int tEnd;
int nextStart= gp->exonStarts[0] ;
int nextEnd = gp->exonEnds[0];
int nextEndIndex = 0;
int tCoding=FALSE;
int qCoding=FALSE;
int qStopCodon = FALSE;
int tFlip=TRUE; /* flag to control target alternating colors for exons (blue and purple) */
int qFlip=TRUE; /* flag to control query alternating colors for exons (blue and purple) */
int qClass=INTERGENIC;
int tClass=INTERGENIC;
int prevTclass=INTERGENIC;
int prevQclass=INTERGENIC;
int posStrand;
DNA qCodon[4];
DNA tCodon[4];
AA qProt, tProt = 0;
int tPtr = 0;
int prevEnd = 500000000;
int intronTruncated=FALSE;

if (gp->strand[0] == '+')
    {
    nextEndIndex = 0;
    nextStart = gp->exonStarts[nextEndIndex] ;
    nextEnd = gp->exonEnds[nextEndIndex];
    tStart =  gp->cdsStart ;
    tEnd = gp->cdsEnd-3  ;
    posStrand=TRUE;
    if (axtList != NULL)
        tPtr = axtList->tStart;
    }
else if (gp->strand[0] == '-')
    {
    nextEndIndex = (gp->exonCount)-1;
    nextStart = (gp->exonEnds[nextEndIndex]);
    nextEnd = (gp->exonStarts[nextEndIndex]);
    tStart =  gp->cdsEnd ;
    tEnd = gp->cdsStart ;
    posStrand=FALSE;
    if (axtList != NULL)
        tPtr = axtList->tEnd;
    }
else
    {
    errAbort("cannot determine start_codon position for %s on %s\n",gp->name,gp->chrom);
    exit(0);
    }

/* safef(nibFile, sizeof(nibFile), "%s/%s.nib",nibDir,gp->chrom); */
/* if no alignment , make a bad one */
if (axtList == NULL)
    {
    if (gp->strand[0] == '+')
        axtList = createAxtGap(nibFile,gp->chrom,tStart,tEnd ,gp->strand[0]);
    else
        axtList = createAxtGap(nibFile,gp->chrom,tEnd,tStart ,gp->strand[0]);
    }
/* append unaligned coding region to list */
if (posStrand)
    {
    if ((axtList->tStart)-1 > tStart)
        {
        struct axt *axtGap = createAxtGap(nibFile,gp->chrom,tStart,axtList->tStart,gp->strand[0]);
        slAddHead(&axtList, axtGap);
        tPtr = axtList->tStart;
        }
    }
else
    {
    if (axtList->tEnd < tStart)
        {
        struct axt *axtGap = createAxtGap(nibFile,gp->chrom,axtList->tEnd, tStart+1,gp->strand[0]);
        axtListReverse(&axtGap, database);
        slAddHead(&axtList, axtGap);
        tPtr = axtList->tEnd;
        }
    }

for (axt = axtList; axt != NULL; axt = axt->next)
    {
    char *q = axt->qSym;
    char *t = axt->tSym;
    int size = axt->symCount;
    int sizeLeft = size;
    int qPtr ;
    char qStrand = (axt->qStrand == gp->strand[0] ? '+' : '-');
    int qStart = axt->qStart;
    int qEnd = axt->qEnd;
    int qSize = 0;
    if (!sameString(axt->qName, "gap"))
        qSize = hChromSize(otherDb, axt->qName);
    if (qStrand == '-')
        {
        qStart = qSize - axt->qEnd;
        qEnd = qSize - axt->qStart;
        }
/*    fprintf(f, ">%s:%d-%d %s:%d-%d (%c) score %d coding %d-%d utr/coding %d-%d gene %c alignment %c\n",
 *          axt->tName, axt->tStart+1, axt->tEnd,
 *          axt->qName, qStart+1, qEnd, qStrand, axt->score,  tStart+1, tEnd, gp->txStart+1, gp->txEnd, gp->strand[0], axt->qStrand); */

    qPtr = qStart;
    if (gp->exonFrames == NULL)
        qCodonPos = tCodonPos; /* put translation back in sync */
    if (!posStrand)
        {
        qPtr = qEnd;
        /* skip to next exon if we are starting in the middle of a gene  - should not happen */
        while ((tPtr < nextEnd) && (nextEndIndex > 0))
            {
            nextEndIndex--;
            prevEnd = nextEnd;
            nextStart = (gp->exonEnds[nextEndIndex]);
            nextEnd = (gp->exonStarts[nextEndIndex]);
            if (nextStart > tStart)
                tClass = INTRON;
            }
        }
    else
        {
        /* skip to next exon if we are starting in the middle of a gene  - should not happen */
        while ((tPtr > nextEnd) && (nextEndIndex < gp->exonCount-2))
            {
            nextEndIndex++;
            prevEnd = nextEnd;
            nextStart = gp->exonStarts[nextEndIndex];
            nextEnd = gp->exonEnds[nextEndIndex];
            if (nextStart > tStart)
                tClass = INTRON;
            }
        }
    /* loop thru one base at a time */
    while (sizeLeft > 0)
        {
        struct dyString *dyT = dyStringNew(1024);
        struct dyString *dyQ = dyStringNew(1024);
        struct dyString *dyQprot = dyStringNew(1024);
        struct dyString *dyTprot = dyStringNew(1024);
        struct dyString *exonTag = dyStringNew(1024);
        oneSize = sizeLeft;
        if (oneSize > lineSize)
            oneSize = lineSize;
        setClassStr(dyT,tClass, 0);
        setClassStr(dyQ,qClass, 1);

        /* break up into linesize chunks */
        for (i=0; i<oneSize; ++i)
            {
            if (posStrand)
                {/*look for start of exon on positive strand*/
                if ((tClass==INTRON) && (tPtr >= nextStart) && (tPtr >= tStart) && (tPtr < tEnd))
                    {
                    tCoding=TRUE;
                    dyStringPrintf(exonTag, "exon%d",nextEndIndex+1);
                    addTag(dyT,exonTag);
                    if (gp->exonFrames != NULL && gp->exonFrames[nextEndIndex] != -1)
                        tCodonPos = gp->exonFrames[nextEndIndex]+1;
                    if (qStopCodon == FALSE)
                        {
                        qCoding=TRUE;
                        qCodonPos = tCodonPos; /* put translation back in sync */
                        qFlip = tFlip;
                        }
                    }
                else if ((tPtr >= nextStart) && (tPtr < tStart))
                    { /* start of UTR 5'*/
                    tClass=UTR5; qClass=UTR5;
                    }
                }
            else{
                if ((tClass==INTRON) && (tPtr <= nextStart) && (tPtr <= tStart) && (tPtr > tEnd))
                    { /*look for start of exon on neg strand */
                    tCoding=TRUE;
                    dyStringPrintf(exonTag, "exon%d",nextEndIndex+1);
                    addTag(dyT,exonTag);

                    if (qStopCodon == FALSE)
                        {
                        qCoding=TRUE;
                        if (gp->exonFrames != NULL && gp->exonFrames[nextEndIndex] != -1)
                            tCodonPos = gp->exonFrames[nextEndIndex]+1;
                        qCodonPos = tCodonPos; /* put translation back in sync */
                        qFlip = tFlip;
                        }
                    }
                else if ((tPtr <= nextStart-1) && (tPtr > tStart))
                    { /* start of UTR 5'*/
                    tClass=UTR5; qClass=UTR5;
                    }
	    }
            /* toggle between blue / purple color for exons */
            if (tCoding && tFlip )
                tClass=CODINGA;
            if (tCoding && (tFlip == FALSE) )
                tClass=CODINGB;
            if (qCoding && qFlip && !qStopCodon)
                qClass=CODINGA;
            if (qCoding && (qFlip == FALSE) && !qStopCodon)
                qClass=CODINGB;
            if (posStrand)
                {
                /* look for end of exon */
                if (tPtr == nextEnd)
                    {
                    tCoding=FALSE;
                    qCoding=FALSE;
                    tClass=INTRON;
                    qClass=INTRON;
                    nextEndIndex++;
                    nextStart = gp->exonStarts[nextEndIndex];
                    prevEnd = nextEnd;
                    nextEnd = gp->exonEnds[nextEndIndex];
                    if (gp->exonFrames != NULL && gp->exonFrames[nextEndIndex] != -1)
                        tCodonPos = gp->exonFrames[nextEndIndex]+1;
                    }
                }
            else
                {
                /* look for end of exon  negative strand */
                if (tPtr == nextEnd && tPtr != tEnd)
                    {
                    tCoding=FALSE;
                    qCoding=FALSE;
                    tClass=INTRON;
                    qClass=INTRON;
                    nextEndIndex--;
                    nextStart = (gp->exonEnds[nextEndIndex]);
                    prevEnd = nextEnd;
                    nextEnd = (gp->exonStarts[nextEndIndex]);
                    }
                }
            if (posStrand)
                {
                /* look for start codon and color it green*/
                if ((tPtr >= (tStart)) && (tPtr <=(tStart+2)))
                    {
                    if (gp->exonFrames != NULL && gp->cdsStartStat == cdsComplete)
                        {
                        tClass=STARTCODON;
                        qClass=STARTCODON;
                        }
                    else if(tClass != CODINGB)
                        {
                        tClass=CODINGA;
                        qClass=CODINGA;
                        }
                    tCoding=TRUE;
                    qCoding=TRUE;
                    if (tPtr == tStart)
                        {
                        if (gp->exonFrames != NULL && gp->exonFrames[nextEndIndex] != -1)
                            tCodonPos = gp->exonFrames[nextEndIndex]+1;
                        else
                            tCodonPos=1;
                        qCodonPos=tCodonPos;
                        }
                    }
                /* look for stop codon and color it red */
                if ((tPtr >= tEnd) && (tPtr <= (tEnd+2)))
                    {
                    if (gp->exonFrames != NULL && gp->cdsEndStat == cdsComplete)
                        {
                        tClass=STOPCODON;
                        qClass=STOPCODON;
                        }
                    tCoding=FALSE;
                    qCoding=FALSE;
                    }
                }
            else
                {
                /* look for start codon and color it green negative strand case*/
                if ((tPtr <= (tStart)) && (tPtr >=(tStart-2)))
                    {
                    if (gp->exonFrames != NULL && gp->cdsStartStat == cdsComplete)
                        {
                        tClass=STARTCODON;
                        qClass=STARTCODON;
                        }
                    else if (tClass!=CODINGB)
                        {
                        tClass=CODINGA;
                        qClass=CODINGA;
                        }
                    tCoding=TRUE;
                    qCoding=TRUE;
                    if (tPtr == tStart)
                        {
                        if (gp->exonFrames != NULL && gp->exonFrames[nextEndIndex] != -1)
                            tCodonPos = gp->exonFrames[nextEndIndex]+1;
                        else
                            tCodonPos=1;
                        }
                    qCodonPos=tCodonPos;
                    }
                /* look for stop codon and color it red - negative strand*/
                if ((tPtr <= tEnd+3) && (tPtr >= (tEnd+1)))
                    {
                    if (gp->exonFrames != NULL && gp->cdsEndStat == cdsComplete)
                        {
                        tClass=STOPCODON;
                        qClass=STOPCODON;
                        }
                    tCoding=FALSE;
                    qCoding=FALSE;
                    }
                }
            if (posStrand)
                {
                /* look for 3' utr and color it orange */
                if (tPtr == (tEnd +3) )
                    {
                    tClass = UTR3;
                    qClass = UTR3;
                    }
                }
            else
                {
                /* look for 3' utr and color it orange negative strand case*/
                if (tPtr == (tEnd) )
                    {
                    tClass = UTR3;
                    qClass = UTR3;
                    }
                }

            if (qCoding && qCodonPos == 3)
                {
                /* look for in frame stop codon and color it magenta */
                qCodon[qCodonPos-1] = q[i];
                qCodon[3] = 0;
                qProt = lookupCodon(qCodon);
                if (qProt == 'X') qProt = ' ';
                if (qProt == 0)
                    {
                    qProt = '*'; /* stop codon is * */
                    qClass = INFRAMESTOP;
                    }
                }

            /* write html to change color for all above cases t strand */
            if (tClass != prevTclass)
                {
                setClassStr(dyT,tClass,0);
                prevTclass = tClass;
                }
            dyStringAppendC(dyT,t[i]);
            /* write html to change color for all above cases q strand */
            if (qClass != prevQclass)
                {
                setClassStr(dyQ,qClass,0);
                prevQclass = qClass;
                }
            dyStringAppendC(dyQ,q[i]);
            if (tCoding && tFlip && (tCodonPos == 3))
                {
                tFlip=FALSE;
                }
            else if (tCoding && (tFlip == FALSE) && (tCodonPos == 3))
                {
                tFlip=TRUE;
                }
            if (qCoding && qFlip && (qCodonPos == 3))
                {
                qFlip=FALSE;
                }
            else if (qCoding && (qFlip == FALSE) && (qCodonPos == 3))
                {
                qFlip=TRUE;
                }
            /* translate dna to protein and append html */
            if (tCoding && tCodonPos == 3)
                {
                tCodon[tCodonPos-1] = t[i];
                tCodon[3] = 0;
                tProt = lookupCodon(tCodon);
                if (tProt == 'X') tProt = ' ';
                if (tProt == 0) tProt = '*'; /* stop codon is * */
                dyStringAppendC(dyTprot,tProt);
                }
            else
                {
                dyStringAppendC(dyTprot,' ');
                }
            if (qCoding && qCodonPos == 3)
                {
                qCodon[qCodonPos-1] = q[i];
                qCodon[3] = 0;
                qProt = lookupCodon(qCodon);
                if (qProt == 'X') qProt = ' ';
                if (qProt == 0)
                    {
                    qProt = '*'; /* stop codon is * */
                    /* qClass = INFRAMESTOP; */
                    qStopCodon = FALSE;
                    qCoding = TRUE;
                    }
                if (tProt == qProt) qProt = '|'; /* if the AA matches  print | */
                dyStringAppendC(dyQprot,qProt);
                }
            else
                {
                dyStringAppendC(dyQprot,' ');
                }
            /* move to next base and update reading frame */
            if (t[i] != '-')
                {
                if (posStrand)
                    {
                    tPtr++;
                    qPtr++;
                    }
                else
                    {
                    tPtr--;
                    qPtr--;
                    }
                if (tCoding)
                    {
                    tCodon[tCodonPos-1] = t[i];
                    tCodonPos++;
                    }
                if (tCodonPos>3) tCodonPos=1;
                }
            /*else
	      {
	      tClass=INTRON;
	      }*/
            /* update reading frame on other species */
            if (q[i] != '-')
                {
                if (qCoding)
                    {
                    qCodon[qCodonPos-1] = q[i];
                    qCodonPos++;
                    }
                if (qCodonPos>3) qCodonPos=1;
                }
            /*else
	      {
	      qClass=INTRON;
	      }*/
            }
        /* write labels in black */
        resetClassStr(dyT,0);
        setClassStr(dyT,LABEL,0);
        if (posStrand)
            {
            dyStringPrintf(dyT, " %d ",tPtr);
            if (tCoding)
                dyStringPrintf(dyT, "exon %d",(nextEndIndex == 0) ? 1 : nextEndIndex+1);
            }
        else
            {
            dyStringPrintf(dyT, " %d ",tPtr+1);
            if (tCoding)
                dyStringPrintf(dyT, "exon %d", (nextEndIndex == 0) ? 1 : nextEndIndex+1);
            }
#if 0 /* debug version */
        if (posStrand)
            dyStringPrintf(dyT, " %d thisExon=%d-%d xon %d",tPtr, gp->exonStarts[(nextEndIndex == 0) ? 0 : nextEndIndex - 1]+1, gp->exonEnds[(nextEndIndex == 0) ? 0 : nextEndIndex - 1],(nextEndIndex == 0) ? 1 : nextEndIndex);
        else
            dyStringPrintf(dyT, " %d thisExon=%d-%d xon %d",tPtr, gp->exonStarts[(nextEndIndex == gp->exonCount) ? gp->exonCount : nextEndIndex ]+1, gp->exonEnds[(nextEndIndex == gp->exonCount) ? gp->exonCount : nextEndIndex ],(nextEndIndex == 0) ? 1 : nextEndIndex);
#endif
        dyStringAppendC(dyT,'\n');
        resetClassStr(dyT,0);
        resetClassStr(dyQ,1);
        setClassStr(dyQ,LABEL,1);
        if (posStrand)
            dyStringPrintf(dyQ, " %d ",qPtr);
        else
            dyStringPrintf(dyQ, " %d ",qPtr);

        dyStringAppendC(dyQ,'\n');
        resetClassStr(dyQ,1);
        dyStringAppendC(dyQprot,'\n');
        dyStringAppendC(dyTprot,'\n');

#if 0 /* debug version */
        if (posStrand)
            printf(" %d nextExon=%d-%d xon %d t %d prevEnd %d diffs %d %d<br>",qPtr, nextStart+1,nextEnd,nextEndIndex+1, tPtr,prevEnd, tPtr-nextStart-70, tPtr-(prevEnd+70));
        else
            printf(" %d nextExon=%d-%d xon %d t %d prevEnd %d diffs %d %d<br>",qPtr, nextStart+1,nextEnd,nextEndIndex, tPtr, prevEnd, tPtr-nextStart-70, tPtr-(prevEnd+70));
#endif

        /* write out alignment, unless we are deep inside an intron */
        if (tClass != INTRON || (tClass == INTRON && tPtr < nextStart-LINESIZE && tPtr< (prevEnd + posStrand ? LINESIZE : -LINESIZE)))
            {
            intronTruncated = 0;
            fputs(dyTprot->string,f);
            fputs(dyT->string,f);

            for (i=0; i<oneSize; ++i)
                {
                if (toupper(q[i]) == toupper(t[i]) && isalpha(q[i]))
                    fputc('|', f);
                else
                    fputc(' ', f);
                }
            fputc('\n', f);

            fputs(dyQ->string,f);
            fputs(dyQprot->string,f);
            fputc('\n', f);
            }
        else
            {
            if (!(intronTruncated == TRUE))
                {
                printf("...intron truncated...<br>");
                intronTruncated = TRUE;
                }
            }
        /* look for end of line */
        if (oneSize > lineSize)
            oneSize = lineSize;
        sizeLeft -= oneSize;
        q += oneSize;
        t += oneSize;
        dyStringFree(&dyT);
        dyStringFree(&dyQ);
        dyStringFree(&dyQprot);
        dyStringFree(&dyTprot);
        }
    }
}

struct axt *getAxtListForGene(struct genePred *gp, char *nib, char *fromDb, char *toDb,
		       struct lineFile *lf)
/* get all axts for a gene */
{
struct axt *axt, *axtGap;
struct axt *axtList = NULL;
int prevEnd = gp->txStart;
// int prevStart = gp->txEnd;  unused variable
int tmp;

while ((axt = axtRead(lf)) != NULL)
    {
    if (sameString(gp->chrom, axt->tName)
    &&  (  (  (axt->tStart <= gp->cdsStart && axt->tEnd >= gp->cdsStart)
           || (axt->tStart <= gp->cdsEnd   && axt->tEnd >= gp->cdsEnd  ) )
        || (   axt->tStart <  gp->cdsEnd   && axt->tEnd >  gp->cdsStart  ) ) )
        {
        if (gp->strand[0] == '-')
            {
            reverseComplement(axt->qSym, axt->symCount);
            reverseComplement(axt->tSym, axt->symCount);
            tmp = hChromSize(fromDb, axt->qName) - axt->qStart;
            axt->qStart = hChromSize(fromDb, axt->qName) - axt->qEnd;
            axt->qEnd = tmp;
            if (prevEnd < (axt->tStart)-1)
                {
                axtGap = createAxtGap(nib,gp->chrom,prevEnd,(axt->tStart),gp->strand[0]);
                reverseComplement(axtGap->qSym, axtGap->symCount);
                reverseComplement(axtGap->tSym, axtGap->symCount);
                slAddHead(&axtList, axtGap);
                }
            }
        else if (prevEnd < (axt->tStart))
            {
            axtGap = createAxtGap(nib,gp->chrom,prevEnd,(axt->tStart),gp->strand[0]);
            slAddHead(&axtList, axtGap);
            }
        slAddHead(&axtList, axt);
        prevEnd = axt->tEnd;
        // prevStart = axt->tStart;  unused variable
        }
    if (sameString(gp->chrom, axt->tName) && (axt->tStart > gp->txEnd))
        {
        if ((prevEnd < axt->tStart) && prevEnd < min(gp->txEnd, axt->tStart))
            {
            axtGap = createAxtGap(nib,gp->chrom,prevEnd,min(axt->tStart,gp->txEnd),gp->strand[0]);
            if (gp->strand[0] == '-')
                {
                reverseComplement(axtGap->qSym, axtGap->symCount);
                reverseComplement(axtGap->tSym, axtGap->symCount);
                }
            slAddHead(&axtList, axtGap);
            }
        else
            if (axtList == NULL)
                {
                axtGap = createAxtGap(nib,gp->chrom,prevEnd,gp->txEnd,gp->strand[0]);
                if (gp->strand[0] == '-')
                    {
                    reverseComplement(axtGap->qSym, axtGap->symCount);
                    reverseComplement(axtGap->tSym, axtGap->symCount);
                    }
                slAddHead(&axtList, axtGap);
                }
        break;
        }
    }
if (gp->strand[0] == '+')
    slReverse(&axtList);
return axtList ;
}

struct axt *getAxtListForRange(struct genePred *gp, char *nib, char *fromDb, char *toDb,
		       char *alignment, char *qChrom, int qStart, int qEnd)
/* get all axts for a chain */
{
struct lineFile *lf ;
struct axt *axt, *axtGap;
struct axt *axtList = NULL;
int prevEnd = gp->txStart;
// int prevStart = gp->txEnd;  unused variable
int tmp;

lf = lineFileOpen(getAxtFileName(gp->chrom, toDb, alignment, fromDb), TRUE);
printf("file %s\n",lf->fileName);
while ((axt = axtRead(lf)) != NULL)
    {
/*    if (sameString(gp->chrom , axt->tName))
 *       printf("axt %s qstart %d axt tStart %d\n",axt->qName, axt->qStart,axt->tStart); */
    if ( sameString(gp->chrom, axt->tName)
    &&   sameString(   qChrom, axt->qName)
    &&   positiveRangeIntersection(     qStart,      qEnd, axt->qStart, axt->qEnd)
    &&   positiveRangeIntersection(gp->txStart, gp->txEnd, axt->tStart, axt->tEnd) )
        {
        if (gp->strand[0] == '-')
            {
            reverseComplement(axt->qSym, axt->symCount);
            reverseComplement(axt->tSym, axt->symCount);
            tmp = hChromSize(fromDb, axt->qName) - axt->qStart;
            axt->qStart = hChromSize(fromDb, axt->qName) - axt->qEnd;
            axt->qEnd = tmp;
            if (prevEnd < (axt->tStart)-1)
                {
                axtGap = createAxtGap(nib,gp->chrom,prevEnd,(axt->tStart)-1,gp->strand[0]);
                reverseComplement(axtGap->qSym, axtGap->symCount);
                reverseComplement(axtGap->tSym, axtGap->symCount);
                slAddHead(&axtList, axtGap);
                }
            }
        else if (prevEnd < (axt->tStart)-1)
            {
            axtGap = createAxtGap(nib,gp->chrom,prevEnd,(axt->tStart)-1,gp->strand[0]);
            slAddHead(&axtList, axtGap);
            }
        slAddHead(&axtList, axt);
        prevEnd = axt->tEnd;
        // prevStart = axt->tStart;  unused variable
        }
    if (sameString(gp->chrom, axt->tName) && (axt->tStart > gp->txEnd+20000))
        {
        if (axt->tStart > prevEnd)
            {
            axtGap = createAxtGap(nib,gp->chrom,prevEnd+1,(axt->tStart)-1,gp->strand[0]);
            if (gp->strand[0] == '-')
                {
                reverseComplement(axtGap->qSym, axtGap->symCount);
                reverseComplement(axtGap->tSym, axtGap->symCount);
                }
            slAddHead(&axtList, axtGap);
            }
        break;
        }
    }
if (gp->strand[0] == '+')
    slReverse(&axtList);
return axtList ;
}

void printCdsStatus(enum cdsStatus cdsStatus)
/* print a description of a genePred cds status */
{
switch (cdsStatus)
    {
    case cdsNone:        /* "none" - No CDS (non-coding)  */
        printf("none (non-coding)<br>\n");
        break;
    case cdsUnknown:     /* "unk" - CDS is unknown (coding, but not known)  */
        printf("unknown (coding, but not known)<br>\n");
        break;
    case cdsIncomplete:  /* "incmpl" - CDS is not complete at this end  */
        printf("<em>not</em> complete<br>\n");
        break;
    case cdsComplete:    /* "cmpl" - CDS is complete at this end  */
        printf("complete<br>\n");
        break;
    }
}

void showGenePos(char *name, struct trackDb *tdb)
/* Show gene prediction position and other info. */
{
char *rootTable = tdb->table;
char query[512];
char *liftDb = cloneString(trackDbSetting(tdb, "quickLiftDb"));
char *db = (liftDb == NULL) ? database : liftDb;
struct sqlConnection *conn = hAllocConn(db);
struct genePred *gpList = NULL, *gp = NULL;
char table[HDB_MAX_TABLE_STRING];
struct sqlResult *sr = NULL;
char **row = NULL;
char *classTable = trackDbSetting(tdb, GENEPRED_CLASS_TBL);


if (!hFindSplitTable(db, seqName, rootTable, table, sizeof table, NULL))
    errAbort("showGenePos track %s not found", rootTable);
sqlSafef(query, sizeof(query), "name = \"%s\"", name);
gpList = genePredReaderLoadQuery(conn, table, query);
for (gp = gpList; gp != NULL; gp = gp->next)
    {
    printPos(gp->chrom, gp->txStart, gp->txEnd, gp->strand, FALSE, NULL);
    if(sameString(tdb->type,"genePred")
    && startsWith("ENCODE Gencode",tdb->longLabel)
    && startsWith("ENST",name))
        {
        char *ensemblIdUrl = trackDbSetting(tdb, "ensemblIdUrl");

        printf("<b>Ensembl Transcript Id:&nbsp</b>");
        if (ensemblIdUrl != NULL)
            printf("<a href=\"%s%s\" target=\"_blank\">%s</a><br>", ensemblIdUrl,name,name);
        else
            printf("%s<br>",name);
        }
    if (gp->name2 != NULL && strlen(trimSpaces(gp->name2))> 0)
        {
        /* in Ensembl gene info downloaded from ftp site, sometimes the
           name2 field is populated with "noXref" because there is
           no alternate name. Replace this with "none" */
        printf("<b>Gene Symbol:");
        if ((strlen(gp->name2) < 1) || (sameString(gp->name2, "noXref")))
           printf("</b> none<br>\n");
        else
           printf("</b> %s<br>\n",gp->name2);
        }
    char *ensemblSource = NULL;
    if (sameString("ensGene", table))
	{
	if (hTableExists(db, "ensemblSource"))
	    {
	    sqlSafef(query, sizeof(query),
		"select source from ensemblSource where name='%s'", name);
	    ensemblSource = sqlQuickString(conn, query);
	    }
	}
    if ((gp->exonFrames != NULL) && (!genbankIsRefSeqNonCodingMRnaAcc(gp->name)))
	{
	if (ensemblSource && differentString("protein_coding",ensemblSource))
	    {
	    printf("<b>CDS Start: </b> none (non-coding)<BR>\n");
	    printf("<b>CDS End: </b> none (non-coding)<BR>\n");
	    }
	else
	    {
	    printf("<b>CDS Start: </b>");
	    printCdsStatus((gp->strand[0] == '+') ? gp->cdsStartStat : gp->cdsEndStat);
	    printf("<b>CDS End: </b>");
	    printCdsStatus((gp->strand[0] == '+') ? gp->cdsEndStat : gp->cdsStartStat);
	    }
	}
    /* if a gene class table exists, get gene class and print */
    if (classTable != NULL)
        {
        if (hTableExists(db, classTable))
           {
           sqlSafef(query, sizeof(query),
                "select class from %s where name = \"%s\"", classTable, name);
           sr = sqlGetResult(conn, query);
           /* print class */
           if ((row = sqlNextRow(sr)) != NULL)
              printf("<b>Prediction Class:</b> %s<br>\n", row[0]);
           sqlFreeResult(&sr);
           if (sqlFieldIndex(conn, classTable, "level") > 0 )
               {
               sqlSafef(query, sizeof(query),
                    "select level from %s where name = \"%s\"", classTable, name);
               sr = sqlGetResult(conn, query);
               if ((row = sqlNextRow(sr)) != NULL)
                  printf("<b>Level:&nbsp</b> %s<br>\n", row[0]);
               sqlFreeResult(&sr);
               }
           if (sqlFieldIndex(conn, classTable, "transcriptType") > 0 )
               {
               sqlSafef(query, sizeof(query),
                    "select transcriptType from %s where name = \"%s\"", classTable, name);
               sr = sqlGetResult(conn, query);
               if ((row = sqlNextRow(sr)) != NULL)
                  printf("<b>Transcript type:&nbsp</b> %s<br>\n", row[0]);
               sqlFreeResult(&sr);
               }
           if (sqlFieldIndex(conn, classTable, "geneDesc") > 0 )
               {
               sqlSafef(query, sizeof(query),
                    "select geneDesc from %s where name = \"%s\"", classTable, name);
               sr = sqlGetResult(conn, query);
               if ((row = sqlNextRow(sr)) != NULL)
                  if (differentString("NULL",row[0]))
                      printf("<b>Gene Description :</b> %s<br>\n", row[0]);
               sqlFreeResult(&sr);
               }
           if (sqlFieldIndex(conn, classTable, "type") > 0 )
               {
               sqlSafef(query, sizeof(query),
                    "select type from %s where name = \"%s\"", classTable, name);
               sr = sqlGetResult(conn, query);
               if ((row = sqlNextRow(sr)) != NULL)
                  if (differentString("NULL",row[0]))
                      printf("<b>Gene Type :</b> %s<br>\n", row[0]);
               }
           }
        }
    if (gp->next != NULL)
        printf("<br>");
    }
genePredFreeList(&gpList);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void showGenePosMouse(char *name, struct trackDb *tdb,
                      struct sqlConnection *connMm)
/* Show gene prediction position and other info. */
{
char query[512];
char *rootTable = tdb->table;
struct sqlResult *sr;
char **row;
struct genePred *gp = NULL;
boolean hasBin;
int posCount = 0;
char table[HDB_MAX_TABLE_STRING];

if (!hFindSplitTable(database, seqName, rootTable, table, sizeof table, &hasBin))
    errAbort("showGenePosMouse track %s not found", rootTable);
sqlSafef(query, sizeof query, "select * from %s where name = '%s'", table, name);
sr = sqlGetResult(connMm, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (posCount > 0)
        printf("<BR>\n");
    ++posCount;
    gp = genePredLoad(row + hasBin);
    printPos(gp->chrom, gp->txStart, gp->txEnd, gp->strand, FALSE, NULL);
    genePredFree(&gp);
    }
sqlFreeResult(&sr);
}

void linkToPal(char *track,  char *chrom, int start, int end, char *geneName)
/* Make anchor tag to open pal window */
{
printf("<A TITLE=\"%s\" HREF=\"%s?g=%s&i=%s&c=%s&l=%d&r=%d\">",
       geneName, hgPalName(), track, geneName, chrom, start, end);
    printf("CDS FASTA alignment</A> from multiple alignment");
}

void addPalLink(struct sqlConnection *conn, char *track,
    char *chrom, int start, int end, char *geneName)
{
struct slName *list = hTrackTablesOfType(conn, "wigMaf%%");

if (list != NULL)
    {
    puts("<LI>\n");
    linkToPal( track, chrom, start, end, geneName);
    puts("</LI>\n");
    }
}

void geneShowPosAndLinksPal(char *geneName, char *pepName, struct trackDb *tdb,
                         char *pepTable, char *pepClick,
                         char *mrnaClick, char *genomicClick, char *mrnaDescription,
                         struct palInfo *palInfo)
/* Show parts of gene common to everything. If pepTable is not null,
 * it's the old table name, but will check gbSeq first. */
{
char *geneTable = tdb->table;
boolean foundPep = FALSE;

showGenePos(geneName, tdb);

if (startsWith("ENCODE Gencode",tdb->longLabel))
    {
    char *yaleTable = trackDbSetting(tdb, "yalePseudoAssoc");

    if ((yaleTable != NULL) && (hTableExists(database, yaleTable)))
        {
        struct sqlConnection *conn = hAllocConn(database);
        char query[512];
        sqlSafef(query, sizeof(query),
            "select * from %s where transcript = '%s'", yaleTable, geneName);
        char buffer[512];
        struct sqlResult *sr = sqlGetResult(conn, query);
        char *yaleUrl = trackDbSetting(tdb, "yaleUrl");
        char **row;
        while ((row = sqlNextRow(sr)) != NULL)
            {
            struct yaleGencodeAssoc *ya = yaleGencodeAssocLoad(row);
            safef(buffer, sizeof buffer, "%s/%s",yaleUrl,ya->yaleId);
            printf("<B>Yale pseudogene:</B> <a href=\"%s\" target=\"_blank\">%s</a><br>\n", buffer, ya->yaleId);

            }
        sqlFreeResult(&sr);
        hFreeConn(&conn);
        }
    }

printf("<H3>Links to sequence:</H3>\n");
printf("<UL>\n");

if ((pepTable != NULL) && hGenBankHaveSeq(database, pepName, pepTable))
    {
    puts("<LI>\n");
    hgcAnchorSomewhere(pepClick, pepName, pepTable, seqName);
    printf("Predicted Protein</A> \n");
    puts("</LI>\n");
    foundPep = TRUE;
    }
if (!foundPep)
    {
    char *autoTranslate = trackDbSetting(tdb, "autoTranslate");
    if (autoTranslate == NULL || differentString(autoTranslate, "0"))
        {
        puts("<LI>\n");
        /* put out correct message to describe translated mRNA */
        if ( sameString(geneTable, "ensGene")
        ||   sameString(geneTable, "ws245Genes")
        ||   sameString(geneTable, "vegaGene")
        ||   sameString(geneTable, "vegaPseudoGene")
        ||   genbankIsRefSeqNonCodingMRnaAcc(geneName)
        ||   sameString(geneTable, "lincRNAsTranscripts") )
            {
            printf("Non-protein coding gene or gene fragment, no protein prediction available.");
            }
        else
            {
            hgcAnchorSomewhere("htcTranslatedPredMRna", geneName, "translate", seqName);
            printf("Translated Protein</A> from ");
            if (sameString(geneTable, "refGene") )
                {
		printf("genomic DNA\n");
                }
	    else
		{
                printf("predicted mRNA \n");
		}
	    foundPep = TRUE;
	    }
	puts("</LI>\n");
	}
    }

puts("<LI>\n");
hgcAnchorSomewhere(mrnaClick, geneName, geneTable, seqName);
/* hack to put out a correct message describing the mRNA */
if (sameString(mrnaClick, "htcGeneMrna"))
    printf("%s</A> from genomic sequences\n", mrnaDescription);
else
    printf("%s</A> (may be different from the genomic sequence)\n",
           mrnaDescription);
puts("</LI>\n");

puts("<LI>\n");
hgcAnchorSomewhere(genomicClick, geneName, geneTable, seqName);
printf("Genomic Sequence</A> from assembly\n");
puts("</LI>\n");

if (palInfo)
    {
    struct sqlConnection *conn = hAllocConn(database);
    addPalLink(conn, tdb->track,  palInfo->chrom, palInfo->left,
        palInfo->right, palInfo->rnaName);
    hFreeConn(&conn);
    }

printf("</UL>\n");
}

void geneShowPosAndLinks(char *geneName, char *pepName, struct trackDb *tdb,
			 char *pepTable, char *pepClick,
			 char *mrnaClick, char *genomicClick, char *mrnaDescription)
{
geneShowPosAndLinksPal(geneName, pepName, tdb,
			 pepTable, pepClick,
			 mrnaClick, genomicClick, mrnaDescription, NULL);
}

void geneShowPosAndLinksDNARefseq(char *geneName, char *pepName, struct trackDb *tdb,
				  char *pepTable, char *pepClick,
				  char *mrnaClick, char *genomicClick, char *mrnaDescription)
/* Show parts of a DNA based RefSeq gene */
{
char *geneTable = tdb->table;

showGenePos(geneName, tdb);
printf("<H3>Links to sequence:</H3>\n");
printf("<UL>\n");
puts("<LI>\n");
hgcAnchorSomewhere(genomicClick, geneName, geneTable, seqName);
printf("Genomic Sequence</A> from assembly\n");
puts("</LI>\n");
printf("</UL>\n");
}

void geneShowPosAndLinksMouse(char *geneName, char *pepName,
                              struct trackDb *tdb, char *pepTable,
                              struct sqlConnection *connMm, char *pepClick,
			      char *mrnaClick, char *genomicClick, char *mrnaDescription)
/* Show parts of gene common to everything */
{
char *geneTrack = tdb->track;

showGenePosMouse(geneName, tdb, connMm);
printf("<H3>Links to sequence:</H3>\n");
printf("<UL>\n");
if (pepTable != NULL && hTableExists(database, pepTable))
    {
    hgcAnchorSomewhereDb(pepClick, pepName, pepTable, seqName, mousedb);
    printf("<LI>Translated Protein</A> \n");
    }
hgcAnchorSomewhereDb(mrnaClick, geneName, geneTrack, seqName, mousedb);
printf("<LI>%s</A>\n", mrnaDescription);
hgcAnchorSomewhereDb(genomicClick, geneName, geneTrack, seqName, mousedb);
printf("<LI>Genomic Sequence</A> DNA sequence from assembly\n");
printf("</UL>\n");
}

void geneShowCommon(char *geneName, struct trackDb *tdb, char *pepTable)
/* Show parts of gene common to everything */
{
geneShowPosAndLinks(geneName, geneName, tdb, pepTable, "htcTranslatedProtein",
		    "htcGeneMrna", "htcGeneInGenome", "Predicted mRNA");
char *txInfo = trackDbSetting(tdb, "txInfo");
if (txInfo != NULL)
    showTxInfo(geneName, tdb, txInfo);
char *cdsEvidence = trackDbSetting(tdb, "cdsEvidence");
if (cdsEvidence != NULL)
    showCdsEvidence(geneName, tdb, cdsEvidence);
}

void geneShowMouse(char *geneName, struct trackDb *tdb, char *pepTable,
		   struct sqlConnection *connMm)
/* Show parts of gene common to everything */
{
geneShowPosAndLinksMouse(geneName, geneName, tdb, pepTable, connMm,
                         "htcTranslatedProtein", "htcGeneMrna", "htcGeneInGenome",
                         "Predicted mRNA");
}

void genericGenePredClick(struct sqlConnection *conn, struct trackDb *tdb,
                          char *item, int start, char *pepTable, char *mrnaTable)
/* Handle click in generic genePred track. */
{
char *oldToNew = trackDbSetting(tdb, "oldToNew");
if (oldToNew != NULL && sqlTableExists(conn, oldToNew))
    {
    char query[512];
    sqlSafef(query, sizeof(query),
        "select * from %s where oldId = '%s' and oldChrom='%s' and oldStart=%d",
            oldToNew, item, seqName, start);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row;
    while ((row = sqlNextRow(sr)) != NULL)
        {
	struct kg1ToKg2 *x = kg1ToKg2Load(row);
	printf("<B>Old ID:</B> %s<BR>\n", x->oldId);
	printf("<B>New ID:</B> %s<BR>\n", naForEmpty(x->newId));
	printf("<B>Old/New Mapping:</B> %s<BR>\n", x->status);
	if (x->note[0] != 0)
	    printf("<B>Notes:</B> %s<BR>\n", x->note);
	printf("<BR>\n");
	}
    sqlFreeResult(&sr);
    }
geneShowCommon(item, tdb, pepTable);
printItemDetailsHtml(tdb, item);
}

void pslDumpHtml(struct psl *pslList)
/* print out psl header and data */
{
struct psl* psl;
printf("<PRE><TT>\n");
printf("#match\tmisMatches\trepMatches\tnCount\tqNumInsert\tqBaseInsert\ttNumInsert\tBaseInsert\tstrand\tqName\tqSize\tqStart\tqEnd\ttName\ttSize\ttStart\ttEnd\tblockCount\tblockSizes\tqStarts\ttStarts\n");
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    pslTabOut(psl, stdout);
    }
printf("</TT></PRE>\n");
}

struct twoBitFile *getTwoBitFileFromUrlInSettings(struct trackDb *tdb, char *settingName)
/* Assume settingName is a URL to a two bit file and try and return it */
{
struct twoBitFile *tbf = NULL;
char *url = trackDbSetting(tdb, settingName);
if (url != NULL)
   tbf = twoBitOpen(url);
return tbf;
}

struct twoBitFile *getOtherTwoBitUrl(struct trackDb *tdb)
/* Return open two bit file if otherTwoBitUrl setting is present */
{
return getTwoBitFileFromUrlInSettings(tdb, "otherTwoBitUrl");
}

struct psl *getPslAndSeq(struct trackDb *tdb, char *chromName, struct bigBedInterval *bb, 
    unsigned seqTypeField, DNA **retSeq, char **retCdsStr)
/* Read in psl and query sequence out of a bbiInverval on a give chromosome */
{
struct psl *psl= pslFromBigPsl(chromName, bb, seqTypeField, retSeq, retCdsStr);
DNA *dna = *retSeq;
if (dna == NULL)
    {
    struct twoBitFile *otherTbf = getOtherTwoBitUrl(tdb);
    struct dnaSeq *seq = NULL;
    if (otherTbf)
        {
        seq = twoBitReadSeqFrag(otherTbf, psl->qName, 0, 0);
        if (seq)
            *retSeq = dna = seq->dna;
        }
    }
return psl;
}

void genericBigPslClick(struct sqlConnection *conn, struct trackDb *tdb,
                     char *item, int start, int end)
/* Handle click in big psl track. */
{
struct psl* pslList = NULL;
char *fileName = bbiNameFromSettingOrTable(tdb, conn, tdb->table);
struct bbiFile *bbi =  bigBedFileOpenAlias(fileName, chromAliasFindAliases);
struct lm *lm = lmInit(0);
int ivStart = start, ivEnd = end;

if (start == end)
    {  
    // item is an insertion; expand the search range from 0 bases to 2 so we catch it:
    ivStart = max(0, start-1);
    ivEnd++;
    }  

if (cfgOptionBooleanDefault("drawDot", FALSE))
    bigPslDotPlot(tdb, bbi, seqName, winStart, winEnd);

boolean showEvery = sameString(item, "PrintAllSequences");
boolean showAll = trackDbSettingOn(tdb, "showAll");
unsigned seqTypeField =  bbExtraFieldIndex(bbi, "seqType");
struct bigBedInterval *bb, *bbList = NULL;

// If showAll is on, show all alignments with this qName, not just the
// selected one.
if (showEvery)
    {
    struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);
    for (chrom = chromList; chrom != NULL; chrom = chrom->next)
        {
        char *chromName = chrom->name;
        int start = 0, end = chrom->size;
        int itemsLeft = 0;  // Zero actually means no limit.... 
        struct bigBedInterval *intervalList = bigBedIntervalQuery(bbi, chromName,
            start, end, itemsLeft, lm);
        slCat(&bbList, intervalList);
        }
    }
else if (showAll)
    {
    int fieldIx;
    struct bptFile *bpt = bigBedOpenExtraIndex(bbi, "name", &fieldIx);
    bbList = bigBedNameQuery(bbi, bpt, fieldIx, item, lm);
    }
else
    bbList = bigBedIntervalQuery(bbi, seqName, ivStart, ivEnd, 0, lm);

/* print out extra fields */
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    char *restFields[256];
    int restCount = chopTabs(cloneString(bb->rest), restFields);
    if (sameString(restFields[0], item))
        {
        /* print standard position information */
        char *strand = restFields[2];
        int bedSize = 25;
        int restBedFields = bedSize - 3;
        struct slPair *extraFieldPairs = NULL;
        if (restCount > restBedFields)
            {
            char **extraFields = (restFields + restBedFields);
            int extraFieldCount = restCount - restBedFields;
            extraFieldPairs = getExtraFields(tdb, extraFields, extraFieldCount);
            char *itemForUrl = getIdInUrl(tdb, item);
            printCustomUrlWithFields(tdb, item, item, item == itemForUrl, extraFieldPairs);
            if (itemForUrl)
                printIframe(tdb, itemForUrl);
            printPos(seqName, ivStart, ivEnd, strand, FALSE, item);
            int printCount = extraFieldsPrint(tdb,NULL,extraFields, extraFieldCount);
            printCount += 0;
            }
        }
    }

char *bedRow[32];
char startBuf[16], endBuf[16];

int lastChromId = -1;
char chromName[bbi->chromBpt->keySize+1];

boolean firstTime = TRUE;
struct hash *seqHash = hashNew(0);
struct dyString *sequencesText = dyStringNew(256);
int sequencesFound = 0;
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    bbiCachedChromLookup(bbi, bb->chromId, lastChromId, chromName, sizeof(chromName));

    lastChromId=bb->chromId;
    bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, 4);
    if (showEvery || sameString(bedRow[3], item))
	{
        char *cdsStr, *seq;
        struct psl *psl= getPslAndSeq(tdb, chromName, bb, seqTypeField, &seq, &cdsStr);
        slAddHead(&pslList, psl);

        // we're assuming that if there are multiple psl's with the same id that
        // they are the same query sequence so we only put out one set of sequences
        if (!hashLookup(seqHash, bedRow[3]) && !isEmpty(seq))    // if there is a query sequence
            {
            if (firstTime)
		{
		firstTime = FALSE;
		printf("<H3>Links to sequence:</H3>\n");
		printf("<UL>\n");
		}

            if (!isEmpty(cdsStr))  // if we have CDS 
                {
                puts("<LI>\n");
                hgcAnchorSomewhereExt("htcTranslatedBigPsl", bedRow[3], "translate", bedRow[0], sqlUnsigned(bedRow[1]), sqlUnsigned(bedRow[2]), tdb->track);
		if (showEvery)
		    printf("Translated Amino Acid Sequence</A> from Query Sequence %s\n", bedRow[3]);
		else
		    printf("Translated Amino Acid Sequence</A> from Query Sequence\n");
                puts("</LI>\n");
                }

	    if (psl->qSize <= MAX_DISPLAY_QUERY_SEQ_SIZE)
		{
		puts("<LI>\n");
		hgcAnchorSomewhereExt("htcBigPslSequence", bedRow[3], tdb->track, bedRow[0], sqlUnsigned(bedRow[1]), sqlUnsigned(bedRow[2]), tdb->track);
		if (showEvery)
		    printf("Query Sequence %s</A> \n", bedRow[3]);
		else
		    printf("Query Sequence</A> \n");
		puts("</LI>\n");
		}

            hashAdd(seqHash, bedRow[3], "");
	    dyStringPrintf(sequencesText, ">%s\n%s\n\n", bedRow[3], seq);
	    ++sequencesFound;

            }
	}
    if (!showEvery && !firstTime)
	break;
    }
if (!firstTime)
    printf("</UL>\n");
freeHash(&seqHash);

char *sort = cartUsualString(cart, "sort", pslSortList[0]);
pslSortListByVar(&pslList, sort);

if (showEvery)
    printf("<H3>Genomic Alignments</H3>");
else
    printf("<H3>%s/Genomic Alignments</H3>", item);
if (showEvery || pslIsProtein(pslList))
    printAlignmentsSimple(pslList, start, "htcBigPslAli", tdb->table, item);
else
    printAlignmentsExtra(pslList, start, "htcBigPslAli", "htcBigPslAliInWindow",
        tdb->table, item);
pslFreeList(&pslList);


if (showEvery && sequencesFound > 0)
    {  
    printf("<BR>\n");
    printf("Input Sequences:<BR>\n");
    printf("<textarea rows='8' cols='60' readonly>\n");
    printf("%s", sequencesText->string);
    printf("</textarea>\n");
    dyStringFree(&sequencesText);
    }

printItemDetailsHtml(tdb, item);
}

void genericPslClick(struct sqlConnection *conn, struct trackDb *tdb,
                     char *item, int start, char *subType)
/* Handle click in generic psl track. */
{
struct psl* pslList = getAlignments(conn, tdb->table, item);

/* check if there is an alignment available for this sequence.  This checks
 * both genbank sequences and other sequences in the seq table.  If so,
 * set it up so they can click through to the alignment. */
if (hGenBankHaveSeq(database, item, NULL))
    {
    printf("<H3>%s/Genomic Alignments</H3>", item);
    if (sameString("protein", subType))
        printAlignments(pslList, start, "htcProteinAli", tdb->table, item);
    else
        printAlignments(pslList, start, "htcCdnaAli", tdb->table, item);
    }
else
    {
    /* just dump the psls */
    pslDumpHtml(pslList);
    }
pslFreeList(&pslList);
printItemDetailsHtml(tdb, item);
}


static char *getParentTableName(struct trackDb *tdb)
/* Get the track table or composite track parent table if applicable. */
{
tdb = trackDbTopLevelSelfOrParent(tdb);
return tdb->table;
}

static char *getParentTrackName(struct trackDb *tdb)
/* Get the track name or composite track parent name if applicable. */
{
tdb = trackDbTopLevelSelfOrParent(tdb);
return tdb->track;
}


void printTBSchemaLink(struct trackDb *tdb)
/* Make link to TB schema -- unless this is an on-the-fly (tableless) track. */
{
if (hTableOrSplitExists(database, tdb->table))
    {
    char *trackTable = getParentTableName(tdb);
    printf("<P><A HREF=\"../cgi-bin/hgTables?db=%s&hgta_group=%s&hgta_track=%s"
	   "&hgta_table=%s&position=%s:%d-%d&"
           "hgta_doSchema=describe+table+schema\" target=ucscSchema title='Open schema in new window'>"
	   "View table schema</A></P>\n",
	   database, tdb->grp, trackTable, tdb->table,
	   seqName, winStart+1, winEnd);
    }
}

void printTrackUiLink(struct trackDb *tdb)
/* Make link to hgTrackUi. */
{
char *trackName = getParentTrackName(tdb);
struct trackDb *parentTdb = tdb;
if (!sameString(trackName, tdb->track))
    parentTdb = hTrackDbForTrack(database, trackName);
printf("<P><A HREF=\"%s?g=%s&%s\">"
       "Go to %s track controls</A></P>\n",
       hTrackUiForTrack(tdb->track), trackName, cartSidUrlString(cart), parentTdb->shortLabel);
}

void printDataRestrictionDate(struct trackDb *tdb)
/* If this annotation has a dateUnrestricted trackDb setting, print it */
{
char *restrictionDate = encodeRestrictionDateDisplay(database,tdb);
if (restrictionDate != NULL)
    {
    printf("<A HREF=\"/ENCODE/terms.html\" TARGET=_BLANK><B>Restricted until</A>:</B> %s <BR>\n",
                restrictionDate);
    freeMem(restrictionDate);
    }
}

static void printOrigAssembly(struct trackDb *tdb)
/* If this annotation has been lifted, print the original
 * freeze, as indicated by the "origAssembly" trackDb setting */
{
trackDbPrintOrigAssembly(tdb, database);
}

static char *getHtmlFromSelfOrParent(struct trackDb *tdb, char *liftDb)
/* Get html from self or from parent if not in self. */
{
for (;tdb != NULL; tdb = tdb->parent)
    {
    if (liftDb && (tdb->html == NULL))
        tdb->html = getTrackHtml(liftDb, tdb->table);
    if (tdb->html != NULL && tdb->html[0] != 0)
        return tdb->html;
    }
return NULL;
}

void printTrackHtml(struct trackDb *tdb)
/* If there's some html associated with track print it out. Also print
 * last update time for data table and make a link
 * to the TB table schema page for this table. */
{
if (!isCustomTrack(tdb->track))
    {
    printRelatedTracks(database, trackHash, tdb, cart);
    extraUiLinks(database, tdb, cart);
    printTrackUiLink(tdb);
    printOrigAssembly(tdb);
    printDataVersion(database, tdb);
    printUpdateTime(database, tdb, NULL);
    printDataRestrictionDate(tdb);
    }
char *liftDb = cloneString(trackDbSetting(tdb, "quickLiftDb"));
char *html = getHtmlFromSelfOrParent(tdb, liftDb);
if (html != NULL && html[0] != 0)
    {
    htmlHorizontalLine();

    // Add pennantIcon
    printPennantIconNote(tdb);

    // Wrap description html in div with limited width, so when the page is very wide
    // due to long details, the user doesn't have to scroll right to read the description.
    puts("<div class='readableWidth'>");
    puts(html);
    puts("</div>");
    }
hPrintf("<BR>\n");
}

struct chain *chainLoadItemInRange(struct trackDb *tdb, char *item)
/* Load up parts of chain that intersect seqName:winStart-winEnd */
{
struct chain *chain = NULL;
int id = sqlUnsigned(item);
if (startsWith("big", tdb->type))
    {
    char *fileName = trackDbSetting(tdb, "bigDataUrl");
    char *linkFileName = trackDbSetting(tdb, "linkDataUrl");
    if (linkFileName == NULL)
        {
        char *bigDataUrl = cloneString(trackDbSetting(tdb, "bigDataUrl"));
        char *dot = strrchr(bigDataUrl, '.');
        if (dot == NULL)
            errAbort("No linkDataUrl in track %s", tdb->track);
        *dot = 0;
        char buffer[4096];
        safef(buffer, sizeof buffer, "%s.link.bb", bigDataUrl);
        linkFileName = buffer;
        }

    chain = chainLoadIdRangeHub(database, fileName, linkFileName, seqName, winStart, winEnd, id);
    }
else
    {
    chain = chainLoadIdRange(database, tdb->table, seqName, winStart, winEnd, id);
    }
return chain;
}


void qChainRangePlusStrand(struct chain *chain, int *retQs, int *retQe)
/* Return range of bases covered by chain on q side on the plus
 * strand. */
{
if (chain == NULL)
    errAbort("Can't find range in null query chain.");
if (chain->qStrand == '-')
    {
    *retQs = chain->qSize - chain->qEnd+1;
    *retQe = chain->qSize - chain->qStart;
    }
else
    {
    *retQs = chain->qStart+1;
    *retQe = chain->qEnd;
    }
}

struct chain *chainDbLoad(struct sqlConnection *conn, char *db, char *track,
			  char *chrom, int id)
/* Load chain. */
{
char table[HDB_MAX_TABLE_STRING];
char query[256];
struct sqlResult *sr;
char **row;
boolean hasBin;
struct chain *chain;

if (!hFindSplitTable(db, seqName, track, table, sizeof table, &hasBin))
    errAbort("No %s track in database %s for %s", track, db, seqName);
sqlSafef(query, sizeof(query),
	 "select * from %s where id = %d", table, id);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL)
    errAbort("Can't find %d in %s", id, table);
chain = chainHeadLoad(row + hasBin);
sqlFreeResult(&sr);
chainDbAddBlocks(chain, track, conn);
return chain;
}

void linkToOtherBrowserHub(char *otherDb, char *chrom, int start, int end,  char *hubUrl)
/* Make anchor tag to open another browser window. */
{
printf("<A TARGET=\"_blank\" HREF=\"%s?genome=%s&position=%s%%3A%d-%d&hubUrl=%s\">",
       hgTracksName(), otherDb, chrom, start+1, end, hubUrl);
}

void linkToOtherBrowserExtra(char *otherDb, char *chrom, int start, int end, char *extra)
/* Make anchor tag to open another browser window. */
{
printf("<A TARGET=\"_blank\" HREF=\"%s?db=%s&%s&position=%s%%3A%d-%d\">",
       hgTracksName(), otherDb, extra, chrom, start+1, end);
}

void linkToOtherBrowserSearch(char *otherDb, char *tag)
/* Make anchor tag to open another browser window. */
{
printf("<A TARGET=\"_blank\" HREF=\"%s?db=%s&ct=&position=%s\">",
       hgTracksName(), otherDb, tag);
}

void linkToOtherBrowser(char *otherDb, char *chrom, int start, int end)
/* Make anchor tag to open another browser window. */
{
printf("<A TARGET=\"_blank\" HREF=\"%s?db=%s&ct=&position=%s%%3A%d-%d\">",
       hgTracksName(), otherDb, chrom, start+1, end);
}

void linkToOtherBrowserTitle(char *otherDb, char *chrom, int start, int end, char *title)
/* Make anchor tag to open another browser window. */
{
printf("<A TARGET=\"_blank\" TITLE=\"%s\" HREF=\"%s?db=%s&ct=&position=%s%%3A%d-%d\">",
       title, hgTracksName(), otherDb, chrom, start+1, end);
}

void chainToOtherBrowser(struct chain *chain, char *otherDb, char *otherOrg, char *hubUrl)
/* Put up link that lets us use chain to browser on
 * corresponding window of other species. */
{
struct chain *subChain = NULL, *toFree = NULL;
int qs,qe;
chainSubsetOnT(chain, winStart, winEnd, &subChain, &toFree);
if (subChain != NULL && otherOrg != NULL)
    {
    qChainRangePlusStrand(subChain, &qs, &qe);
    if (hubUrl)
        linkToOtherBrowserHub(otherDb, subChain->qName, qs-1, qe, hubUrl);
    else
        linkToOtherBrowser(otherDb, subChain->qName, qs-1, qe);
    printf("Open %s browser</A> at position corresponding to the part of chain that is in this window.<BR>\n", trackHubSkipHubName(otherOrg));
    }
chainFree(&toFree);
}

struct dnaSeq *otherChromSeq(struct twoBitFile *otherTbf, char *otherDb, 
    char *otherChrom, int otherStart, int otherEnd)
/* This fetches sequence from tdb->otherTwoBitUrl if available,  if not
 * tries to find it via otherDb */
{
if (otherTbf != NULL)
    {
    return twoBitReadSeqFrag(otherTbf, otherChrom, otherStart, otherEnd);
    }
else
    {
    return hChromSeq(otherDb, otherChrom, otherStart, otherEnd);
    }
}

void doSnakeChainClick(struct trackDb *tdb, char *itemName, char *otherDb)
/* Put up page for chain snakes. */
{
struct trackDb *parentTdb = trackDbTopLevelSelfOrParent(tdb);
char *qName = cartOptionalString(cart, "qName");
int qs = atoi(cartOptionalString(cart, "qs"));
int qe = atoi(cartOptionalString(cart, "qe"));
int qWidth = atoi(cartOptionalString(cart, "qWidth"));

char *aliasQName = qName;

boolean otherIsActive = FALSE;
char *hubUrl = NULL;
if (hDbIsActive(otherDb))
   otherIsActive = TRUE;
else 
    hubUrl = genarkUrl(otherDb); // may be NULL

char headerText[256];
safef(headerText, sizeof headerText, "reference: %s, query: %s\n", trackHubSkipHubName(database), trackHubSkipHubName(otherDb) );
genericHeader(parentTdb, headerText);

if (hubUrl != NULL)
    printf("<A HREF=\"hgTracks?hubUrl=%s&genome=%s&position=%s:%d-%d\" TARGET=_BLANK>%s:%d-%d</A> link to block in query assembly: <B>%s</B></A><BR>\n", hubUrl, otherDb, aliasQName,  qs, qe,   aliasQName, qs, qe, trackHubSkipHubName(otherDb));
else if (otherIsActive)
    printf("<A HREF=\"hgTracks?db=%s&position=%s:%d-%d\" TARGET=_BLANK>%s:%d-%d</A> link to block in query assembly: <B>%s</B></A><BR>\n", otherDb, aliasQName, qs, qe, aliasQName, qs, qe, trackHubSkipHubName(otherDb));

int qCenter = (qs + qe) / 2;
int newQs = qCenter - qWidth/2;
int newQe = qCenter + qWidth/2;
if (hubUrl != NULL)
   printf("<A HREF=\"hgTracks?hubUrl=%s&genome=%s&position=%s:%d-%d\" TARGET=\"_blank\">%s:%d-%d</A> link to same window size in query assembly: <B>%s</B></A><BR>\n", hubUrl,otherDb, aliasQName, newQs, newQe,aliasQName, newQs, newQe, trackHubSkipHubName(otherDb) );
else if (otherIsActive)
    printf("<A HREF=\"hgTracks?db=%s&position=%s:%d-%d\" TARGET=\"_blank\">%s:%d-%d</A> link to same window size in query assembly: <B>%s</B></A><BR>\n", otherDb, aliasQName, newQs, newQe,aliasQName, newQs, newQe, trackHubSkipHubName(otherDb) );
printTrackHtml(tdb);
} 

void genericChainClick(struct sqlConnection *conn, struct trackDb *tdb,
                       char *item, int start, char *otherDb)
/* Handle click in chain track, at least the basics. */
{
boolean doSnake = cartOrTdbBoolean(cart, tdb, "doSnake" , FALSE) && cfgOptionBooleanDefault("canSnake", TRUE);

if (doSnake)
    return doSnakeChainClick(tdb, item, otherDb);

struct twoBitFile *otherTbf = getOtherTwoBitUrl(tdb);
char *thisOrg = hOrganism(database);
char *otherOrg = NULL;
struct chain *chain = NULL, *subChain = NULL, *toFree = NULL;
int chainWinSize;
double subSetScore = 0.0;
int qs, qe;
boolean nullSubset = FALSE;
boolean otherIsActive = FALSE;
char *hubUrl = NULL;   // if otherDb is a genark browser

if (hDbIsActive(otherDb))
    otherIsActive = TRUE;
else 
    hubUrl = genarkUrl(otherDb); // may be NULL

if (! sameWord(otherDb, "seq"))
    {
    otherOrg = hOrganism(otherDb);
    }
if (otherOrg == NULL)
    {
    /* use first word of chain label (count on org name as first word) */
    otherOrg = firstWordInLine(cloneString(tdb->shortLabel));
    }

chain = chainLoadItemInRange(tdb, item);
if (startsWith("big", tdb->type))
    {
    if (!otherIsActive) // if this isn't a native database, check to see if it's a hub
        {
        struct trackHubGenome *genome = trackHubGetGenomeUndecorated(otherDb);
        if (genome)
            {
            otherIsActive = TRUE;
            otherDb = genome->name;
            }
        }
    }

chainSubsetOnT(chain, winStart, winEnd, &subChain, &toFree);

if (subChain == NULL)
    nullSubset = TRUE;
else if (otherIsActive && subChain != chain)
    {
    char *linearGap = trackDbSettingOrDefault(tdb, "chainLinearGap", "loose");
    struct gapCalc *gapCalc = gapCalcFromFile(linearGap);
    struct axtScoreScheme *scoreScheme = axtScoreSchemeDefault();
    int qStart = subChain->qStart;
    int qEnd   = subChain->qEnd  ;
    struct dnaSeq *tSeq = hDnaFromSeq(database, subChain->tName, subChain->tStart, subChain->tEnd, dnaLower);
    struct dnaSeq *qSeq = NULL;
    char *matrix = trackDbSetting(tdb, "matrix");
    if (matrix != NULL)
        {
        char *words[64];
        int size = chopByWhite(matrix, words, 64) ;
        if (size == 2 && atoi(words[0]) == 16)
            {
            scoreScheme = axtScoreSchemeFromBlastzMatrix(words[1], 400, 30);
            }
        else
            {
            if (size != 2)
                errAbort("error parsing matrix entry in trackDb, expecting 2 word got %d ",
                        size);
            else
                errAbort("error parsing matrix entry in trackDb, size 16 matrix, got %d ",
                        atoi(words[0]));
            }
        }

    if (subChain->qStrand == '-')
        reverseIntRange(&qStart, &qEnd, subChain->qSize);
    qSeq = otherChromSeq(otherTbf, otherDb, subChain->qName, qStart, qEnd);
    if (subChain->qStrand == '-')
        reverseComplement(qSeq->dna, qSeq->size);
    subChain->score = chainCalcScoreSubChain(subChain, scoreScheme, gapCalc,
        qSeq, tSeq);
    subSetScore = subChain->score;
    }
chainFree(&toFree);

printf("<B>%s position:</B> <A HREF=\"%s?%s&db=%s&position=%s:%d-%d\">%s:%d-%d</A>"
       "  size: %d <BR>\n",
       trackHubSkipHubName(thisOrg), hgTracksName(), cartSidUrlString(cart), database,
       chain->tName, chain->tStart+1, chain->tEnd, chain->tName, chain->tStart+1, chain->tEnd,
       chain->tEnd-chain->tStart);
printf("<B>Strand:</B> %c<BR>\n", chain->qStrand);
qChainRangePlusStrand(chain, &qs, &qe);
if (sameWord(otherDb, "seq"))
    {
    printf("<B>%s position:</B> %s:%d-%d  size: %d<BR>\n",
	otherOrg, chain->qName, qs, qe, chain->qEnd - chain->qStart);
    }
else
    {
    /* prints link to other db browser only if db exists and is active */
    /* else just print position with no link for the other db */
    printf("<B>%s position: </B>", otherOrg);
    if (otherIsActive)
        printf(" <A target=\"_blank\" href=\"%s?db=%s&position=%s%%3A%d-%d\">",
               hgTracksName(), otherDb, chain->qName, qs, qe);
    else if (hubUrl != NULL)
        printf(" <A target=\"_blank\" href=\"%s?genome=%s&hubUrl=%s&position=%s%%3A%d-%d\">",
               hgTracksName(), otherDb, hubUrl, chain->qName, qs, qe);
    printf("%s:%d-%d", chain->qName, qs, qe);
    if (otherIsActive || hubUrl)
        printf("</A>");
    printf(" size: %d<BR>\n", chain->qEnd - chain->qStart);
    }
printf("<B>Chain ID:</B> %s<BR>\n", item);
printf("<B>Score:</B> %1.0f\n", chain->score);

if (nullSubset)
    printf("<B>Score within browser window:</B> N/A (no aligned bases)<BR>\n");
else if (otherIsActive && subChain != chain)
    printf("<B>&nbsp;&nbsp;Approximate Score within browser window:</B> %1.0f<BR>\n",
	   subSetScore);
else
    printf("<BR>\n");

boolean normScoreAvailable = chainDbNormScoreAvailable(tdb);

if (normScoreAvailable)
    {
    char tableName[HDB_MAX_TABLE_STRING];
    if (!hFindSplitTable(database, chain->tName, tdb->table, tableName, sizeof tableName, NULL))
	errAbort("genericChainClick track %s not found", tdb->table);
    char query[256];
    struct sqlResult *sr;
    char **row;
    sqlSafef(query, ArraySize(query),
	 "select normScore from %s where id = '%s'", tableName, item);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
        double normScore = atof(row[0]);
        int basesAligned = chain->score / normScore;
	printf("<B>Normalized Score:</B> %1.0f (aligned bases: %d)", normScore, basesAligned);
        }
    sqlFreeResult(&sr);
    printf("<BR>\n");
    }

printf("<BR>Fields above refer to entire chain or gap, not just the part inside the window.<BR>\n");
printf("<BR>\n");

chainWinSize = min(winEnd-winStart, chain->tEnd - chain->tStart);
/* Show alignment if the database exists and */
/* if there is a chromInfo table for that database and the sequence */
/* file exists. This means that alignments can be shown on the archive */
/* server (or in other cases) if there is a database with a chromInfo table, */
/* the sequences are available and there is an entry added to dbDb for */
/* the otherDb. */
if (otherTbf != NULL || 
    (!startsWith("big", tdb->type) && sqlDatabaseExists(otherDb) 
     && chromSeqFileExists(otherDb, chain->qName)))
    {
    if (chainWinSize < 1000000)
        {
        printf("View ");
        hgcAnchorSomewhere("htcChainAli", item, tdb->track, chain->tName);
        printf("DNA sequence alignment</A> details of parts of chain within browser "
           "window.<BR>\n");
        }
    else
        {
        printf("Zoom so that browser window covers 1,000,000 bases or less "
           "and return here to see alignment details.<BR>\n");
        }
    }

if (!sameWord(otherDb, "seq") && (otherIsActive || hubUrl))
    {
    chainToOtherBrowser(chain, otherDb, otherOrg, hubUrl);
    }
/*
if (!sameWord(otherDb, "seq") && (hDbIsActive(otherDb)))
    {
    chainToOtherBrowser(chain, otherDb, otherOrg);
    }
*/
chainFree(&chain);
}

char *trackTypeInfo(char *track)
/* Return type info on track. You can freeMem result when done. */
{
struct slName *trackDbs = hTrackDbList(), *oneTrackDb;
struct sqlConnection *conn = hAllocConn(database);
char buf[512];
char query[256];
for (oneTrackDb = trackDbs; oneTrackDb != NULL; oneTrackDb = oneTrackDb->next)
    {
    if (sqlTableExists(conn, oneTrackDb->name))
        {
        sqlSafef(query, sizeof(query),
              "select type from %s where tableName = '%s'",  oneTrackDb->name, track);
        if (sqlQuickQuery(conn, query, buf, sizeof(buf)) != NULL)
            break;
        }
    }
if (oneTrackDb == NULL)
    errAbort("%s isn't in the trackDb from the hg.conf", track);
slNameFreeList(&trackDbs);
hFreeConn(&conn);
return cloneString(buf);
}

void findNib(char *db, char *chrom, char nibFile[512])
/* Find nib file corresponding to chromosome in given database. */
{
struct sqlConnection *conn = sqlConnect(db);
char query[256];

sqlSafef(query, sizeof(query),
	 "select fileName from chromInfo where chrom = '%s'", chrom);
if (sqlQuickQuery(conn, query, nibFile, 512) == NULL)
    errAbort("Sequence %s isn't in database %s", chrom, db);
sqlDisconnect(&conn);
}

struct dnaSeq *loadGenomePart(char *db,
                              char *chrom, int start, int end)
/* Load genomic dna from given database and position. */
{
char nibFile[512];
findNib(db, chrom, nibFile);
return hFetchSeq(nibFile, chrom, start, end);
}

void printLabeledNumber(char *org, char *label, long long number)
/* Print label: in bold face, and number with commas. */
{
char *space = " ";
if (org == NULL)
    org = space = "";
printf("<B>%s%s%s:</B> ", org, space, label);
printLongWithCommas(stdout, number);
printf("<BR>\n");
}

void printLabeledPercent(char *org, char *label, long p, long q)
/* Print label: in bold, then p, and then 100 * p/q */
{
char *space = " ";
if (org == NULL)
    org = space = "";
printf("<B>%s%s%s:</B> ", org, space, label);
printLongWithCommas(stdout, p);
if (q != 0)
    printf(" (%3.1f%%)", 100.0 * p / q);
printf("<BR>\n");
}

void genericNetClick(struct sqlConnection *conn, struct trackDb *tdb,
                     char *item, int start, char *otherDb, char *chainTrack)
/* Generic click handler for net tracks. */
{
char table[HDB_MAX_TABLE_STRING];
boolean hasBin;
char query[256];
struct sqlResult *sr;
char **row;
struct netAlign *net;
char *org = hOrganism(database);
char *otherOrg = hOrganism(otherDb);
char *otherOrgBrowser = otherOrg;
int tSize, qSize;
int netWinSize;
struct chain *chain;

if (otherOrg == NULL)
    {
    /* use first word in short track label */
    otherOrg = firstWordInLine(cloneString(tdb->shortLabel));
    }
if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, &hasBin))
    errAbort("genericNetClick track %s not found", tdb->table);
sqlSafef(query, sizeof(query),
	 "select * from %s where tName = '%s' and tStart <= %d and tEnd > %d "
	 "and level = %s",
	 table, seqName, start, start, item);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find %s:%d in %s", seqName, start, table);

net = netAlignLoad(row+hasBin);
sqlFreeResult(&sr);
tSize = net->tEnd - net->tStart;
qSize = net->qEnd - net->qStart;

if (net->chainId != 0)
    {
    netWinSize = min(winEnd-winStart, net->tEnd - net->tStart);
    printf("<BR>\n");
    /* Show alignment if the database exists and */
    /* if there is a chromInfo table for that database and the sequence */
    /* file exists. This means that alignments can be shown on the archive */
    /* server (or in other cases) if there is a database with a chromInfo */
    /* table, the sequences are available and there is an entry added to */
    /* dbDb for the otherDb. */
    if (chromSeqFileExists(otherDb, net->qName))
        {
        if (netWinSize < 1000000)
	    {
	    int ns = max(winStart, net->tStart);
	    int ne = min(winEnd, net->tEnd);
	    if (ns < ne)
	        {
	        char id[20];
	        snprintf(id, sizeof(id), "%d", net->chainId);
	        hgcAnchorWindow("htcChainAli", id, ns, ne, chainTrack, seqName);
	        printf("View alignment details of parts of net within browser window</A>.<BR>\n");
	        }
	    else
	        {
	        printf("Odd, net not in window<BR>\n");
	        }
	    }
        else
	    {
	    printf("To see alignment details zoom so that the browser window covers 1,000,000 bases or less.<BR>\n");
	    }
        }
    chain = chainDbLoad(conn, database, chainTrack, seqName, net->chainId);
    if (chain != NULL)
        {
         /* print link to browser for otherDb only if otherDb is active */
        if (hDbIsActive(otherDb))
	    chainToOtherBrowser(chain, otherDb, otherOrgBrowser, NULL);
	chainFree(&chain);
	}
    htmlHorizontalLine();
    }
printf("<B>Type:</B> %s<BR>\n", net->type);
printf("<B>Level:</B> %d<BR>\n", (net->level+1)/2);
printf("<B>%s position:</B> %s:%d-%d<BR>\n",
       org, net->tName, net->tStart+1, net->tEnd);
printf("<B>%s position:</B> %s:%d-%d<BR>\n",
       otherOrg, net->qName, net->qStart+1, net->qEnd);
printf("<B>Strand:</B> %c<BR>\n", net->strand[0]);
printLabeledNumber(NULL, "Score", net->score);
if (net->chainId)
    {
    printf("<B>Chain ID:</B> %u<BR>\n", net->chainId);
    printLabeledNumber(NULL, "Bases aligning", net->ali);
    if (net->qOver >= 0)
	printLabeledNumber(otherOrg, "parent overlap", net->qOver);
    if (net->qFar >= 0)
	printLabeledNumber(otherOrg, "parent distance", net->qFar);
    if (net->qDup >= 0)
	printLabeledNumber(otherOrg, "bases duplicated", net->qDup);
    }
if (net->tN >= 0)
    printLabeledPercent(org, "N's", net->tN, tSize);
if (net->qN >= 0)
    printLabeledPercent(otherOrg, "N's", net->qN, qSize);
if (net->tTrf >= 0)
    printLabeledPercent(org, "tandem repeat (trf) bases", net->tTrf, tSize);
if (net->qTrf >= 0)
    printLabeledPercent(otherOrg, "tandem repeat (trf) bases", net->qTrf, qSize);
if (net->tR >= 0)
    printLabeledPercent(org, "RepeatMasker bases", net->tR, tSize);
if (net->qR >= 0)
    printLabeledPercent(otherOrg, "RepeatMasker bases", net->qR, qSize);
if (net->tOldR >= 0)
    printLabeledPercent(org, "old repeat bases", net->tOldR, tSize);
if (net->qOldR >= 0)
    printLabeledPercent(otherOrg, "old repeat bases", net->qOldR, qSize);
if (net->tNewR >= 0)
    printLabeledPercent(org, "new repeat bases", net->tOldR, tSize);
if (net->qNewR >= 0)
    printLabeledPercent(otherOrg, "new repeat bases", net->qOldR, qSize);
if (net->tEnd >= net->tStart)
    printLabeledNumber(org, "size", net->tEnd - net->tStart);
if (net->qEnd >= net->qStart)
    printLabeledNumber(otherOrg, "size", net->qEnd - net->qStart);
printf("<BR>Fields above refer to entire chain or gap, not just the part inside the window.<BR>\n");
netAlignFree(&net);
}

void tfbsConsSites(struct trackDb *tdb, char *item)
/* detail page for tfbsConsSites track */
{
boolean printedPlus = FALSE;
boolean printedMinus = FALSE;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);
char table[HDB_MAX_TABLE_STRING];
boolean hasBin;
char query[512];
struct sqlResult *sr;
char **row;
struct tfbsConsSites *tfbsConsSites;
struct tfbsConsSites *tfbsConsSitesList = NULL;
struct tfbsConsFactors *tfbsConsFactor;
struct tfbsConsFactors *tfbsConsFactorList = NULL;
boolean firstTime = TRUE;
char *mappedId = NULL;

genericHeader(tdb, item);

if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, &hasBin))
    errAbort("tfbsConsSites track %s not found", tdb->table);
sqlSafef(query, sizeof query, "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d",
	    table, item, seqName, start);
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    tfbsConsSites = tfbsConsSitesLoad(row+hasBin);
    slAddHead(&tfbsConsSitesList, tfbsConsSites);
    }
sqlFreeResult(&sr);
slReverse(&tfbsConsSitesList);

if (!hFindSplitTable(database, seqName, "tfbsConsFactors", table, sizeof table, &hasBin))
    errAbort("tfbsConsSites table tfbsConsFactors not found");
sqlSafef(query, sizeof query, "select * from %s where name = '%s' ", table, item);
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    tfbsConsFactor = tfbsConsFactorsLoad(row+hasBin);
    slAddHead(&tfbsConsFactorList, tfbsConsFactor);
    }
sqlFreeResult(&sr);
slReverse(&tfbsConsFactorList);

if (tfbsConsFactorList)
    mappedId = cloneString(tfbsConsFactorList->ac);

printf("<B style='font-size:large;'>Transcription Factor Binding Site information:</B><BR><BR><BR>");
for(tfbsConsSites=tfbsConsSitesList ; tfbsConsSites != NULL ; tfbsConsSites = tfbsConsSites->next)
    {
    /* print each strand only once */
    if ((printedMinus && (tfbsConsSites->strand[0] == '-')) || (printedPlus && (tfbsConsSites->strand[0] == '+')))
	continue;

    if (!firstTime)
        htmlHorizontalLine();
    else
	firstTime = FALSE;

    printf("<B>Item:</B> %s<BR>\n", tfbsConsSites->name);
    if (mappedId != NULL)
	printCustomUrl(tdb, mappedId, FALSE);
    printf("<B>Score:</B> %d<BR>\n", tfbsConsSites->score );
    printf("<B>zScore:</B> %.2f<BR>\n", tfbsConsSites->zScore );
    printf("<B>Strand:</B> %s<BR>\n", tfbsConsSites->strand);
    printPos(tfbsConsSites->chrom, tfbsConsSites->chromStart, tfbsConsSites->chromEnd, NULL, TRUE, tfbsConsSites->name);
    printedPlus = printedPlus || (tfbsConsSites->strand[0] == '+');
    printedMinus = printedMinus || (tfbsConsSites->strand[0] == '-');
    }

if (tfbsConsFactorList)
    {
    htmlHorizontalLine();
    printf("<B style='font-size:large;'>Transcription Factors known to bind to this site:</B><BR><BR>");
    for(tfbsConsFactor =tfbsConsFactorList ; tfbsConsFactor  != NULL ; tfbsConsFactor  = tfbsConsFactor ->next)
	{
	if (!sameString(tfbsConsFactor->species, "N"))
	    {
	    printf("<BR><B>Factor:</B> %s<BR>\n", tfbsConsFactor->factor);
	    printf("<B>Species:</B> %s<BR>\n", tfbsConsFactor->species);
	    printf("<B>SwissProt ID:</B> %s<BR>\n", sameString(tfbsConsFactor->id, "N")? "unknown": tfbsConsFactor->id);
	    }
	}
    }

printTrackHtml(tdb);
hFreeConn(&conn);
}

void tfbsCons(struct trackDb *tdb, char *item)
/* detail page for tfbsCons track */
{
boolean printFactors = FALSE;
boolean printedPlus = FALSE;
boolean printedMinus = FALSE;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);
char table[HDB_MAX_TABLE_STRING];
boolean hasBin;
char query[512];
struct sqlResult *sr;
char **row;
struct tfbsCons *tfbs;
struct tfbsCons *tfbsConsList = NULL;
struct tfbsConsMap tfbsConsMap;
boolean firstTime = TRUE;
char *mappedId = NULL;

genericHeader(tdb, item);

if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, &hasBin))
    errAbort("tfbsCons track %s not found", tdb->table);
sqlSafef(query, sizeof query, "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d",
	    table, item, seqName, start);
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    tfbs = tfbsConsLoad(row+hasBin);
    slAddHead(&tfbsConsList, tfbs);
    }
sqlFreeResult(&sr);
slReverse(&tfbsConsList);

if (hTableExists(database, "tfbsConsMap"))
    {
    sqlSafef(query, sizeof query, "select * from tfbsConsMap where id = '%s'", tfbsConsList->name);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	tfbsConsMapStaticLoad(row, &tfbsConsMap);
	mappedId = cloneString(tfbsConsMap.ac);
	}
    }
sqlFreeResult(&sr);

printf("<B style='font-size:large;'>Transcription Factor Binding Site information:</B><BR><BR><BR>");
for(tfbs=tfbsConsList ; tfbs != NULL ; tfbs = tfbs->next)
    {
    if (!sameString(tfbs->species, "N"))
	printFactors = TRUE;

    /* print each strand only once */
    if ((printedMinus && (tfbs->strand[0] == '-')) || (printedPlus && (tfbs->strand[0] == '+')))
	continue;

    if (!firstTime)
        htmlHorizontalLine();
    else
	firstTime = FALSE;

    printf("<B>Item:</B> %s<BR>\n", tfbs->name);
    if (mappedId != NULL)
	printCustomUrl(tdb, mappedId, FALSE);
    printf("<B>Score:</B> %d<BR>\n", tfbs->score );
    printf("<B>Strand:</B> %s<BR>\n", tfbs->strand);
    printPos(tfbsConsList->chrom, tfbs->chromStart, tfbs->chromEnd, NULL, TRUE, tfbs->name);
    printedPlus = printedPlus || (tfbs->strand[0] == '+');
    printedMinus = printedMinus || (tfbs->strand[0] == '-');
    }

if (printFactors)
    {
    htmlHorizontalLine();
    printf("<B style='font-size:large;'>Transcription Factors known to bind to this site:</B><BR><BR>");
    for(tfbs=tfbsConsList ; tfbs != NULL ; tfbs = tfbs->next)
	{
	/* print only the positive strand when factors are on both strands */
	if ((tfbs->strand[0] == '-') && printedPlus)
	    continue;

	if (!sameString(tfbs->species, "N"))
	    {
	    printf("<BR><B>Factor:</B> %s<BR>\n", tfbs->factor);
	    printf("<B>Species:</B> %s<BR>\n", tfbs->species);
	    printf("<B>SwissProt ID:</B> %s<BR>\n", sameString(tfbs->id, "N")? "unknown": tfbs->id);

	    }
	}
    }

printTrackHtml(tdb);
hFreeConn(&conn);
}

void firstEF(struct trackDb *tdb, char *item)
{
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);
char table[HDB_MAX_TABLE_STRING];
boolean hasBin;
struct bed *bed;
char query[512];
struct sqlResult *sr;
char **row;
boolean firstTime = TRUE;

/* itemForUrl = item; */
genericHeader(tdb, item);
printCustomUrl(tdb, item, FALSE);
/* printCustomUrl(tdb, itemForUrl, item == itemForUrl); */

if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, &hasBin))
    errAbort("firstEF track %s not found", tdb->table);
sqlSafef(query, sizeof query, "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d",
	    table, item, seqName, start);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (firstTime)
	firstTime = FALSE;
    else
	htmlHorizontalLine();
    bed = bedLoadN(row+hasBin, 6);

    printf("<B>Item:</B> %s<BR>\n", bed->name);
    printf("<B>Probability:</B> %g<BR>\n", bed->score / 1000.0);
    printf("<B>Strand:</B> %s<BR>\n", bed->strand);
    printPos(bed->chrom, bed->chromStart, bed->chromEnd, NULL, TRUE, bed->name);
    }
printTrackHtml(tdb);
hFreeConn(&conn);
}

void doBed5FloatScore(struct trackDb *tdb, char *item)
/* Handle click in BED 5+ track: BED 5 with 0-1000 score (for useScore
 * shading in hgTracks) plus real score for display in details page. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char table[HDB_MAX_TABLE_STRING];
boolean hasBin;
struct bed5FloatScore *b5;
struct dyString *query = dyStringNew(512);
char **row;
boolean firstTime = TRUE;
int start = cartInt(cart, "o");
int bedSize = 5;

if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, &hasBin))
    errAbort("doBed5FloatScore track %s not found", tdb->table);
sqlDyStringPrintf(query, "select * from %s where chrom = '%s' and ",
	       table, seqName);
hAddBinToQuery(winStart, winEnd, query);
sqlDyStringPrintf(query, "name = '%s' and chromStart = %d", item, start);
sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (firstTime)
	firstTime = FALSE;
    else
	htmlHorizontalLine();
    b5 = bed5FloatScoreLoad(row+hasBin);
    bedPrintPos((struct bed *)b5, 4, tdb);
    printf("<B>Score:</B> %f<BR>\n", b5->floatScore);
    if (sameString(tdb->type, "bed5FloatScoreWithFdr"))
        {
        if (row[7] != NULL)
           printf("<B>False Discovery Rate (FDR):</B> %s%%<BR>\n", row[7]);
        }
    }
getBedTopScorers(conn, tdb, table, item, start, bedSize);

sqlFreeResult(&sr);
hFreeConn(&conn);
/* printTrackHtml is done in genericClickHandlerPlus. */
}

void doBed6FloatScore(struct trackDb *tdb, char *item)
/* Handle click in BED 4+ track that's like BED 6 but with floating pt score */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char table[HDB_MAX_TABLE_STRING];
boolean hasBin;
struct bed6FloatScore *b6 = NULL;
struct dyString *query = dyStringNew(512);
char **row;
boolean firstTime = TRUE;
int start = cartInt(cart, "o");

genericHeader(tdb, item);

if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, &hasBin))
    errAbort("doBed6FloatScore track %s not found", tdb->table);
sqlDyStringPrintf(query, "select * from %s where chrom = '%s' and ",
	       table, seqName);
hAddBinToQuery(winStart, winEnd, query);
sqlDyStringPrintf(query, "name = '%s' and chromStart = %d", item, start);
sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (firstTime)
	firstTime = FALSE;
    else
	htmlHorizontalLine();
    b6 = bed6FloatScoreLoad(row+hasBin);
    bedPrintPos((struct bed *)b6, 4, tdb);
    printf("<B>Score:</B> %f<BR>\n", b6->score);
    printf("<B>Strand:</B> %s<BR>\n", b6->strand);
    }
sqlFreeResult(&sr);

// Support for motif display if configured in trackDb
// TODO - share code with factorSource
char *motifPwmTable = trackDbSetting(tdb, "motifPwmTable");
struct dnaMotif *motif = NULL;
if (motifPwmTable != NULL && sqlTableExists(conn, motifPwmTable))
    {
    motif = loadDnaMotif(b6->name, motifPwmTable);
    if (motif == NULL)
        return;
    struct dnaSeq *seq = hDnaFromSeq(database, b6->chrom, b6->chromStart, b6->chromEnd, dnaLower);
    motifLogoAndMatrix(&seq, 1, motif);
    }
hFreeConn(&conn);
/* printTrackHtml is done in genericClickHandlerPlus. */
}

void doColoredExon(struct trackDb *tdb, char *item)
/* Print information for coloredExon type tracks. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char query[256];
char **row;
genericHeader(tdb, item);
sqlSafef(query, sizeof(query), "select chrom,chromStart,chromEnd,name,score,strand from %s where name='%s'", tdb->table, item);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    struct bed *itemBed = bedLoad6(row);
    bedPrintPos(itemBed, 6, tdb);
    bedFree(&itemBed);
    }
else
    {
    hPrintf("Could not find info for %s<BR>\n", item);
    }
sqlFreeResult(&sr);
printTrackHtml(tdb);
hFreeConn(&conn);
}

void doImageItemBed(struct trackDb *tdb, char *item)
/* Print bed plus an image */
{
}

void doChromGraph(struct trackDb *tdb)
/* Print information for coloredExon type tracks. */
{
genericHeader(tdb, NULL);
printTrackHtml(tdb);
}

static void genericContainerClick(struct sqlConnection *conn, char *containerType,
	struct trackDb *tdb, char *item, char *itemForUrl)
/* Print page for container of some sort. */
{
if (sameString(containerType, "multiWig"))
    {
    errAbort("It's suprising that multiWig container gets to hgc. It should go to hgTrackUi.");
    }
else
    {
    errAbort("Unrecognized container type %s for %s", containerType, tdb->track);
    }
}

static void doBedMethyl(struct trackDb *tdb, char *item)
/* Handle a click on a bedMethyl custom track */
{
int start = cartInt(cart, "o");

struct sqlConnection *conn = hAllocConnTrack(CUSTOM_TRASH, tdb);
database=CUSTOM_TRASH;
tdb->table = trackDbSetting(tdb, "dbTableName");

genericBedClick(conn, tdb, item, start, 9);
}

static void doLongTabix(struct trackDb *tdb, char *item)
/* Handle a click on a long range interaction */
{
char *bigDataUrl = hashFindVal(tdb->settingsHash, "bigDataUrl");
struct bedTabixFile *btf = bedTabixFileMayOpen(bigDataUrl, NULL, 0, 0);
char *chromName = cartString(cart, "c");
struct bed *list = bedTabixReadBeds(btf, chromName, winStart, winEnd, bedLoad5);
bedTabixFileClose(&btf);
unsigned maxWidth;
struct longRange *longRangeList = parseLongTabix(list, &maxWidth, 0);
struct longRange *longRange, *ourLongRange = NULL;
unsigned itemNum = sqlUnsigned(item);
unsigned count = slCount(longRangeList);
double *doubleArray;

AllocArray(doubleArray, count);

int ii = 0;
for(longRange = longRangeList; longRange; longRange = longRange->next, ii++)
    {
    if (longRange->id == itemNum)
        {
        ourLongRange = longRange;
        }
    doubleArray[ii] = longRange->score;
    }

if (ourLongRange == NULL)
    errAbort("cannot find long range item with id %d\n", itemNum);

printf("Item you clicked on:<BR>\n");
printf("&nbsp;&nbsp;&nbsp;&nbsp;<B>ID:</B> %u<BR>\n", ourLongRange->id);
if (!ourLongRange->hasColor)
    // if there's color, then there's no score in this format
    printf("<B>Score:</B> %g<BR>\n", ourLongRange->score);

unsigned padding =  (ourLongRange->e - ourLongRange->s) / 10;
int s = ourLongRange->s - padding; 
int e = ourLongRange->e + padding; 
if (s < 0 ) 
    s = 0;
int chromSize = hChromSize(database, seqName);
if (e > chromSize)
    e = chromSize;

char sStartPosBuf[1024], sEndPosBuf[1024];
char eStartPosBuf[1024], eEndPosBuf[1024];
// FIXME:  longRange should store region starts, not centers
sprintLongWithCommas(sStartPosBuf, ourLongRange->s - ourLongRange->sw/2 + 1);
sprintLongWithCommas(sEndPosBuf, ourLongRange->s + ourLongRange->sw/2 + 1);
sprintLongWithCommas(eStartPosBuf, ourLongRange->e - ourLongRange->ew/2 + 1);
sprintLongWithCommas(eEndPosBuf, ourLongRange->e + ourLongRange->ew/2 + 1);
char sWidthBuf[1024], eWidthBuf[1024];
char regionWidthBuf[1024];
sprintLongWithCommas(sWidthBuf, ourLongRange->sw);
sprintLongWithCommas(eWidthBuf, ourLongRange->ew);
sprintLongWithCommas(regionWidthBuf, ourLongRange->ew + ourLongRange->e - ourLongRange->s);

if (differentString(ourLongRange->sChrom, ourLongRange->eChrom))
    {
    printf("<B>Current region: </B>");
    printf("<A HREF=\"hgTracks?position=%s:%s-%s \" TARGET=_BLANK>%s:%s-%s (%s bp)</A><BR>\n",  
            ourLongRange->sChrom, sStartPosBuf, sEndPosBuf,
            ourLongRange->sChrom, sStartPosBuf,sEndPosBuf, sWidthBuf);
    printf("<B>Paired region: </B>");
    printf("<A HREF=\"hgTracks?position=%s:%s-%s \" TARGET=_BLANK>%s:%s-%s (%s bp)<BR></A><BR>\n",  
            ourLongRange->eChrom, eStartPosBuf, eEndPosBuf, 
            ourLongRange->eChrom, eStartPosBuf, eEndPosBuf, eWidthBuf);
    }
else
    {
    printf("<B>Lower region: </B>");
    printf("<A HREF=\"hgTracks?position=%s:%s-%s \" TARGET=_BLANK>%s:%s-%s (%s bp)</A><BR>\n",  
            ourLongRange->sChrom, sStartPosBuf,sEndPosBuf, 
            ourLongRange->sChrom, sStartPosBuf,sEndPosBuf, sWidthBuf);
    printf("<B>Upper region: </B>");
    printf("<A HREF=\"hgTracks?position=%s:%s-%s \" TARGET=_BLANK>%s:%s-%s (%s bp)<BR></A><BR>\n",  
            ourLongRange->eChrom, eStartPosBuf, eEndPosBuf, 
            ourLongRange->eChrom, eStartPosBuf, eEndPosBuf, eWidthBuf);
    printf("<B>Interaction region: </B>");
    printf("<A HREF=\"hgTracks?position=%s:%s-%s \" TARGET=_BLANK>%s:%s-%s (%s bp)<BR></A><BR>\n",  
            ourLongRange->eChrom, sStartPosBuf, eEndPosBuf, 
            ourLongRange->eChrom, sStartPosBuf, eEndPosBuf, regionWidthBuf);
    }
if (ourLongRange->hasColor)
    return;

struct aveStats *as = aveStatsCalc(doubleArray, count);
printf("<BR>Statistics on the scores of all items in window (go to track controls to set minimum score to display):\n");

printf("<TABLE BORDER=1>\n");
printf("<TR><TD><B>Q1</B></TD><TD>%f</TD></TR>\n", as->q1);
printf("<TR><TD><B>median</B></TD><TD>%f</TD></TR>\n", as->median);
printf("<TR><TD><B>Q3</B></TD><TD>%f</TD></TR>\n", as->q3);
printf("<TR><TD><B>average</B></TD><TD>%f</TD></TR>\n", as->average);
printf("<TR><TD><B>min</B></TD><TD>%f</TD></TR>\n", as->minVal);
printf("<TR><TD><B>max</B></TD><TD>%f</TD></TR>\n", as->maxVal);
printf("<TR><TD><B>count</B></TD><TD>%d</TD></TR>\n", as->count);
printf("<TR><TD><B>total</B></TD><TD>%f</TD></TR>\n", as->total);
printf("<TR><TD><B>standard deviation</B></TD><TD>%f</TD></TR>\n", as->stdDev);
printf("</TABLE>\n");
}

void quickLiftChainClick(struct sqlConnection *conn, struct trackDb *tdb,
                       char *item, int start, char *otherDb)
/* Handle a click on the quickChain track */
{
struct twoBitFile *otherTbf = getOtherTwoBitUrl(tdb);
char *otherOrg = NULL;
struct chain *chain = NULL, *subChain = NULL, *toFree = NULL;
int chainWinSize;
boolean otherIsActive = FALSE;

if (hDbIsActive(otherDb))
    otherIsActive = TRUE;

if (! sameWord(otherDb, "seq"))
    {
    otherOrg = hOrganism(otherDb);
    }
if (otherOrg == NULL)
    {
    /* use first word of chain label (count on org name as first word) */
    otherOrg = firstWordInLine(cloneString(tdb->shortLabel));
    }

chain = chainLoadItemInRange(tdb, item);
if (startsWith("big", tdb->type))
    {
    if (!otherIsActive) // if this isn't a native database, check to see if it's a hub
        {
        struct trackHubGenome *genome = trackHubGetGenomeUndecorated(otherDb);
        if (genome)
            {
            otherIsActive = TRUE;
            otherDb = genome->name;
            }
        }
    }

chainSubsetOnT(chain, winStart, winEnd, &subChain, &toFree);

char position[128];
char *ourPos, *otherPos;
int seqStart = cartInt(cart, "l");
int seqEnd =   cartInt(cart, "r");
char *chromName = cartString(cart, "c");
safef(position, 128, "%s:%d-%d", chromName, seqStart, seqEnd);
ourPos = cloneString(addCommasToPos(database, position));
printf("<B>%s position:</B> %s<BR>", trackHubSkipHubName(database), ourPos);

int qs,qe;
qChainRangePlusStrand(subChain, &qs, &qe);
safef(position, 128, "%s:%d-%d", subChain->qName, qs-1, qe);
otherPos = cloneString(addCommasToPos(otherDb, position));
printf("<B>%s position: </B>", trackHubSkipHubName(otherDb));
linkToOtherBrowser(otherDb, subChain->qName, qs-1, qe);
printf("%s</A><BR><BR>",  otherPos);
chainWinSize = min(winEnd-winStart, chain->tEnd - chain->tStart);

if (otherTbf != NULL || 
    (!startsWith("big", tdb->type) && sqlDatabaseExists(otherDb) 
     && chromSeqFileExists(otherDb, chain->qName)))
    {
    if (chainWinSize < 1000000)
        {
        printf("View ");
        hgcAnchorSomewhere("htcChainAli", item, tdb->track, chain->tName);
        printf("DNA sequence alignment of whole window.</A><BR><BR>");
        }
    else
        {
        printf("Zoom so that browser window covers 1,000,000 bases or less "
           "and return here to see alignment details.<BR>\n");
        }
    }

char *liftDb = cloneString(trackDbSetting(tdb, "quickLiftDb"));
char *quickLiftFile = cloneString(trackDbSetting(tdb, "quickLiftUrl"));
struct quickLiftRegions *hr, *regions = quickLiftGetRegions(database, liftDb, quickLiftFile, chromName, seqStart, seqEnd);

unsigned deletions = 0;
unsigned insertions = 0;
unsigned mismatches = 0;
unsigned doubles = 0;

for(hr = regions; hr; hr = hr->next)
    {
    switch(hr->type)
        {
        case QUICKTYPE_INSERT:
            insertions++;
            break;
        case QUICKTYPE_DEL:
            deletions++;
            break;
        case QUICKTYPE_DOUBLE:
            doubles++;
            break;
        case QUICKTYPE_MISMATCH:
            mismatches++;
            break;
        }
    }

if (deletions)
    {
    printf("<BR><B>Deletions in Window:</B><BR>");
    printf("<TABLE border=\"1\"> <TR>\n");
    printf("<TR><TD>%s Position</TD><TD>%s Position</TD><TD>Bases</TD><TD>Alignment</TD><TR>", trackHubSkipHubName(database), trackHubSkipHubName(otherDb));
    for(hr = regions; hr; hr = hr->next)
        {
        if (hr->type != QUICKTYPE_DEL)
            continue;

        char *ourPos, *otherPos;
        snprintf(position, 128, "%s:%ld-%ld", hr->chrom, hr->chromStart, hr->chromEnd);
        ourPos = cloneString(addCommasToPos(database, position));
        snprintf(position, 128, "%s:%ld-%ld", hr->oChrom, hr->oChromStart, hr->oChromEnd);
        otherPos = cloneString(addCommasToPos(database, position));
        printf("<TR><TD>%s</TD><TD>%s</TD><TD>%.*s</TD><TD>",   ourPos, otherPos, hr->otherBaseCount, hr->otherBases);
        hgcAnchorSomewhereExt("htcChainAli", item, tdb->track, chain->tName, hr->chromStart - 10, hr->chromEnd + 10, tdb->track);
            printf("alignment</A></TD></TR>");

        }
    printf("</TABLE>");
    }

if (insertions)
    {
    printf("<BR><B>Insertions in Window:</B><BR>");
    printf("<TABLE border=\"1\"> <TR>\n");
    printf("<TR><TD>%s Position</TD><TD>%s Position</TD><TD>Bases</TD><TD>Alignment</TD><TR>", trackHubSkipHubName(database), trackHubSkipHubName(otherDb));
    for(hr = regions; hr; hr = hr->next)
        {
        if (hr->type != QUICKTYPE_INSERT)
            continue;

        char *ourPos, *otherPos;
        snprintf(position, 128, "%s:%ld-%ld", hr->chrom, hr->chromStart, hr->chromEnd);
        ourPos = cloneString(addCommasToPos(database, position));
        snprintf(position, 128, "%s:%ld-%ld", hr->oChrom, hr->oChromStart, hr->oChromEnd);
        otherPos = cloneString(addCommasToPos(database, position));
        printf("<TR><TD>%s</TD><TD>%s</TD><TD>%.*s</TD><TD>",   ourPos, otherPos, hr->baseCount, hr->bases);
        hgcAnchorSomewhereExt("htcChainAli", item, tdb->track, chain->tName, hr->chromStart - 10, hr->chromEnd + 10, tdb->track);
            printf("alignment</A></TD></TR>");

        }
    printf("</TABLE>");
    }

if (doubles)
    {
    printf("<BR><B>Double Gaps in Window:</B><BR>");
    printf("<TABLE border=\"1\"> <TR>\n");
    printf("<TR><TD>%s Position</TD><TD>%s Position</TD><TD># Bases in %s</TD><TD>#Bases in %s</TD><TD>Alignment</TD><TR>", trackHubSkipHubName(database), trackHubSkipHubName(otherDb), trackHubSkipHubName(database), trackHubSkipHubName(otherDb));
    for(hr = regions; hr; hr = hr->next)
        {
        if (hr->type != QUICKTYPE_DOUBLE)
            continue;

        char *ourPos, *otherPos;
        snprintf(position, 128, "%s:%ld-%ld", hr->chrom, hr->chromStart, hr->chromEnd);
        ourPos = cloneString(addCommasToPos(database, position));
        snprintf(position, 128, "%s:%ld-%ld", hr->oChrom, hr->oChromStart, hr->oChromEnd);
        otherPos = cloneString(addCommasToPos(database, position));
        printf("<TR><TD>%s</TD><TD>%s</TD><TD>%d</TD><TD>%d</TD><TD>",   ourPos, otherPos, hr->baseCount, hr->otherBaseCount);
        hgcAnchorSomewhereExt("htcChainAli", item, tdb->track, chain->tName, hr->chromStart - 10, hr->chromEnd + 10, tdb->track);
            printf("alignment</A></TD></TR>");

        }
    printf("</TABLE>");
    }

if (mismatches)
    {
    printf("<BR><B>Mismatches in Window:</B><BR>");
    printf("<TABLE border=\"1\"> <TR>\n");
    printf("<TR><TD>%s Position</TD><TD>%s Position</TD><TD>Change</TD><TD>Alignment</TD><TR>", trackHubSkipHubName(database), trackHubSkipHubName(otherDb));
    for(hr = regions; hr; hr = hr->next)
        {
        if (hr->type != QUICKTYPE_MISMATCH)
            continue;

        char *ourPos, *otherPos;
        snprintf(position, 128, "%s:%ld-%ld", hr->chrom, hr->chromStart, hr->chromEnd);
        ourPos = cloneString(addCommasToPos(database, position));
        snprintf(position, 128, "%s:%ld-%ld", hr->oChrom, hr->oChromStart, hr->oChromEnd);
        otherPos = cloneString(addCommasToPos(database, position));
        printf("<TR><TD>%s</TD><TD>%s</TD><TD>%.*s -> %.*s</TD><TD>",   ourPos, otherPos, hr->otherBaseCount, hr->otherBases, hr->baseCount, hr->bases);
        hgcAnchorSomewhereExt("htcChainAli", item, tdb->track, chain->tName, hr->chromStart - 10, hr->chromEnd + 10, tdb->track);
            printf("alignment</A></TD></TR>");

        }
    printf("</TABLE>");
    }
}
void genericClickHandlerPlus(
        struct trackDb *tdb, char *item, char *itemForUrl, char *plus)
/* Put up generic track info, with additional text appended after item. */
{
char *dupe, *type, *words[16], *headerItem;
int wordCount;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
struct sqlConnection *conn = NULL;
char *imagePath = trackDbSetting(tdb, ITEM_IMAGE_PATH);
char *container = trackDbSetting(tdb, "container");

char *liftDb = cloneString(trackDbSetting(tdb, "quickLiftDb"));

if (liftDb)
    {
    if (isCustomTrack(trackHubSkipHubName(tdb->track)))
        {
        liftDb = CUSTOM_TRASH;
        tdb->table = trackDbSetting(tdb, "dbTableName");
        }
    if (!trackHubDatabase(liftDb))
        conn = hAllocConnTrack(liftDb, tdb);
    }
else if (!trackHubDatabase(database))
    conn = hAllocConnTrack(database, tdb);
if (itemForUrl == NULL)
    itemForUrl = item;
dupe = cloneString(tdb->type);
wordCount = chopLine(dupe, words);
headerItem = cloneString(item);
type = words[0];

/* Suppress printing item name in page header, as it is not informative for these types of
 * tracks... */
if (container == NULL && wordCount > 0)
    {
    if (sameString(type, "maf") || sameString(type, "wigMaf") || sameString(type, "bigMaf") || sameString(type, "netAlign")
        || sameString(type, "encodePeak"))
        headerItem = NULL;
    else if ((  sameString(type, "narrowPeak")
             || sameString(type, "broadPeak")
             || sameString(type, "gappedPeak") )
         &&  headerItem
         &&  sameString(headerItem, ".") )
        headerItem = NULL;
    }
/* Print header. */
genericHeader(tdb, headerItem);

if (differentString(type, "bigInteract") && differentString(type, "interact"))
    {
    // skip generic URL code as these may have multiple items returned for a click
    itemForUrl = getIdInUrl(tdb, item);
    if (itemForUrl != NULL && trackDbSetting(tdb, "url") && differentString(type, "bigBed")
            && differentString(type, "bigPsl") && differentString(type, "bigGenePred"))
        {
        printCustomUrl(tdb, itemForUrl, item == itemForUrl);
        printIframe(tdb, itemForUrl);
        }
    }
if (plus != NULL)
    {
    fputs(plus, stdout);
    }
if (container != NULL)
    {
    genericContainerClick(conn, container, tdb, item, itemForUrl);
    }
else if (wordCount > 0)
    {
    type = words[0];
    if (sameString(type, "bed"))
	{
	int num = 0;
	if (wordCount > 1)
	    num = atoi(words[1]);
	if (num < 3) num = 3;
        genericBedClick(conn, tdb, item, start, num);
	}
    else if (sameString(type, "bedMethyl"))
	{
        genericBedClick(conn, tdb, item, start, 9);
	}
    else if (sameString(type, "bigGenePred"))
        {
	int num = 12;
        genericBigBedClick(conn, tdb, item, start, end, num);
	}
    else if (sameString(type, "bigBed"))
        {
	int num = 0;
	if (wordCount > 1)
	    num = atoi(words[1]);
	if (num < 3) num = 3;
        genericBigBedClick(conn, tdb, item, start, end, num);
	}
    else if (sameString(type, "sample"))
	{
	int num = 9;
        genericSampleClick(conn, tdb, item, start, num);
	}
    else if (sameString(type, "genePred"))
        {
	char *pepTable = NULL, *mrnaTable = NULL;
	if ((wordCount > 1) && !sameString(words[1], "."))
	    pepTable = words[1];
	if ((wordCount > 2) && !sameString(words[2], "."))
	    mrnaTable = words[2];
	genericGenePredClick(conn, tdb, item, start, pepTable, mrnaTable);
	}
    else if ( sameString(type, "bigPsl")) {
	genericBigPslClick(conn, tdb, item, start, end);
	}
    else if (sameString(type, "psl"))
        {
	char *subType = ".";
	if (wordCount > 1)
	    subType = words[1];
	genericPslClick(conn, tdb, item, start, subType);
	}
    else if (sameString(type, "netAlign"))
        {
	if (wordCount < 3)
	    errAbort("Missing field in netAlign track type field");
	genericNetClick(conn, tdb, item, start, words[1], words[2]);
	}
    else if (sameString(type, "bigQuickLiftChain")) 
        {
	quickLiftChainClick(conn, tdb, item, start, words[1]);
	}
    else if (sameString(type, "chain") || sameString(type, "bigChain") )
        {
	if (wordCount < 2)
	    errAbort("Missing field in chain track type field");
	genericChainClick(conn, tdb, item, start, words[1]);
	}
    else if (sameString(type, "maf"))
        {
	genericMafClick(conn, tdb, item, start);
	}
    else if (sameString(type, "wigMaf") ||  sameString(type, "bigMaf"))
        {
	genericMafClick(conn, tdb, item, start);
        }
    else if (startsWith("wigMafProt", type))
        {
	genericMafClick(conn, tdb, item, start);
        }
    else if (sameString(type, "axt"))
        {
	genericAxtClick(conn, tdb, item, start, words[1]);
	}
    else if (sameString(type, "expRatio"))
        {
	doExpRatio(tdb, item, NULL);
	}
    else if (sameString(type, "coloredExon"))
	{
	doColoredExon(tdb, item);
	}
    else if (sameString(type, "encodePeak") || sameString(type, "narrowPeak") ||
	     sameString(type, "broadPeak") || sameString(type, "gappedPeak"))
	{
	doEncodePeak(tdb, NULL, item);
	}
    else if (sameString(type, "bigNarrowPeak"))
	{
	doBigEncodePeak(tdb, NULL, item);
	}
    else if (sameString(type, "encodeFiveC"))
	{
	doEncodeFiveC(conn, tdb);
        }
    else if (sameString(type, "peptideMapping"))
	{
	doPeptideMapping(conn, tdb, item);
	}
    else if (sameString(type, "chromGraph"))
	{
	doChromGraph(tdb);
	}
    else if (sameString(type, "wig"))
        {
	genericWiggleClick(conn, tdb, item, start);
        }
    else if (sameString(type, "bigWig"))
        {
	genericBigWigClick(conn, tdb, item, start);
	}
    else if (sameString(type, "factorSource"))
        {
	doFactorSource(conn, tdb, item, start, end);
	}
    else if (sameString(type, "bed5FloatScore") ||
             sameString(type, "bed5FloatScoreWithFdr"))
	{
	doBed5FloatScore(tdb, item);
	}
    else if (sameString(type, "bed6FloatScore"))
	{
	doBed6FloatScore(tdb, item);
	}
    else if (sameString(type, "altGraphX"))
        {
	doAltGraphXDetails(tdb,item);
	}
    //add bedDetail here
    else if (startsWith("bedDetail", type))
        {
        doBedDetail(tdb, NULL, item);
        }
    else if (sameString(type, "bigMethyl") )
	{
        genericBigBedClick(conn, tdb, item, start, end, 0);
	}
    else if (sameString(type, "bigLolly") )
	{
        genericBigBedClick(conn, tdb, item, start, end, 0);
	}
    else if (sameString(type, "bigDbSnp") )
	{
        doBigDbSnp(tdb, item);
	}
    else if (sameString(type, "interaction") )
	{
	int num = 12;
        genericBedClick(conn, tdb, item, start, num);
	}
    else if (startsWith("gvf", type))
        {
        doGvf(tdb, item);
        }
    else if (sameString(type, "bam") || sameString(type, "cram"))
	doBamDetails(tdb, item);
    else if ( startsWith("longTabix", type))
	doLongTabix(tdb, item);
    else if (sameWord("interact", type) || sameWord("bigInteract", type))
	doInteractDetails(tdb, item);
    }
if (imagePath)
    {
    char *bigImagePath = trackDbSettingClosestToHome(tdb, ITEM_BIG_IMAGE_PATH);
    char *bothWords[2];
    int shouldBeTwo = chopLine(imagePath, bothWords);
    if (shouldBeTwo != 2)
	errAbort("itemImagePath setting for %s track incorrect. Needs to be \"itemImagePath <path> <suffix>\".", tdb->track);
    printf("<BR><IMG SRC=\"%s/%s.%s\"><BR><BR>\n", bothWords[0], item, bothWords[1]);
    shouldBeTwo = chopLine(bigImagePath, bothWords);
    if (shouldBeTwo != 2)
	errAbort("bigItemImagePath setting for %s track incorrect. Needs to be \"itemImagePath <path> <suffix>\".", tdb->track);
    printf("<A HREF=\"%s/%s.%s\">Download Original Image</A><BR>\n", bothWords[0], item, bothWords[1]);
    }

if ((sameString(tdb->table,"altLocations") || sameString(tdb->table,"fixLocations")) &&
    strchr(item,'_'))
    {
    // Truncate item (alt/fix sequence name) at colon if found:
    char itemCpy[strlen(item)+1];
    safecpy(itemCpy, sizeof(itemCpy), item);
    char *p = strchr(itemCpy, ':');
    if (p)
        *p = '\0';
    char *hgsid = cartSessionId(cart);
    char *desc = sameString(tdb->table, "altLocations") ? "alternate haplotype" : "fix patch";
    printf("<A HREF=\"hgTracks?hgsid=%s&virtModeType=singleAltHaplo&singleAltHaploId=%s\">"
           "Show this %s placed on its chromosome</A><BR>\n",
           hgsid, itemCpy, desc);
    }

printTrackHtml(tdb);
freez(&dupe);
hFreeConn(&conn);
}

void genericClickHandler(struct trackDb *tdb, char *item, char *itemForUrl)
/* Put up generic track info */
{
genericClickHandlerPlus(tdb, item, itemForUrl, NULL);
}

void savePosInTextBox(char *chrom, int start, int end)
/* Save basic position/database info in text box and hidden var.
   Positions becomes chrom:start-end*/
{
char position[128];
char *newPos;
snprintf(position, 128, "%s:%d-%d", chrom, start, end);
newPos = addCommasToPos(database, position);
cgiMakeTextVar("getDnaPos", newPos, strlen(newPos) + 2);
cgiContinueHiddenVar("db");
}

char *hgTablesUrl(boolean usePos, char *track)
/* Make up URL for table browser. */
{
struct dyString *url = dyStringNew(0);
dyStringAppend(url, "../cgi-bin/hgTables?");
dyStringAppend(url, cartSidUrlString(cart));
dyStringPrintf(url, "&db=%s", database);
if (usePos)
    {
    dyStringPrintf(url, "&position=%s:%d-%d", seqName, winStart+1, winEnd);
    dyStringAppend(url, "&hgta_regionType=range");
    }
if (track != NULL)
    {
    struct trackDb *tdb = hashFindVal(trackHash, track);
    if (tdb != NULL)
	{
	char *grp = tdb->grp;
	if (grp != NULL && grp[0] != 0)
	    {
	    dyStringPrintf(url, "&hgta_group=%s", grp);
	    dyStringPrintf(url, "&hgta_track=%s", track);
	    dyStringPrintf(url, "&hgta_table=%s", track);
	    }
	}
    }
return dyStringCannibalize(&url);
}

char *traceUrl(char *traceId)
/* Make up URL for trace archive. */
{
struct dyString *url = dyStringNew(0);
dyStringAppend(url, "https://www.ncbi.nlm.nih.gov/Traces/trace.cgi?");
dyStringPrintf(url, "cmd=retrieve&size=1&val=%s&", traceId);
dyStringAppend(url, "file=trace&dopt=trace");
return dyStringCannibalize(&url);
}

void doGetDna1()
/* Do first get DNA dialog. */
{
struct hTableInfo *hti = NULL;
char *tbl = cgiUsualString("table", "");
if (dbIsFound && tbl[0] != 0)
    {
    char rootName[HDB_MAX_TABLE_STRING];
    char parsedChrom[HDB_MAX_CHROM_STRING];
    hParseTableName(database, tbl, rootName, parsedChrom);
    if (!trackHubDatabase(database))
	hti = hFindTableInfo(database, seqName, rootName);
    }
char *thisOrg = hOrganism(database);
cartWebStart(cart, database, "Get DNA in Window (%s/%s)", trackHubSkipHubName(database), trackHubSkipHubName(thisOrg));
printf("<H2>Get DNA for </H2>\n");
printf("<FORM ACTION=\"%s\">\n\n", hgcName());
cartSaveSession(cart);
cgiMakeHiddenVar("g", "htcGetDna2");
cgiMakeHiddenVar("table", tbl);
cgiContinueHiddenVar("i");
cgiContinueHiddenVar("o");
cgiContinueHiddenVar("t");
cgiContinueHiddenVar("l");
cgiContinueHiddenVar("r");
puts("Position ");
savePosInTextBox(seqName, winStart+1, winEnd);

if (tbl[0] == 0)
    {
    puts("<P>"
         "Note: This page retrieves genomic DNA for a single region. "
         "If you would prefer to get DNA for many items in a particular track, "
         "or get DNA with formatting options based on gene structure (introns, exons, UTRs, etc.), try using the ");
    printf("<A HREF=\"%s\" TARGET=_blank>", hgTablesUrl(TRUE, NULL));
    puts("Table Browser</A> with the \"sequence\" output format. You can also use the ");
    printf("<A HREF=\"../goldenPath/help/api.html\" TARGET=_blank>");
    puts("REST API</A> with the <b>/getData/sequence</b> endpoint function "
         "to extract sequence data with coordinates.");
    }
else
    {
    puts("<P>"
         "Note: if you would prefer to get DNA for more than one feature of "
         "this track at a time, try the ");
    printf("<A HREF=\"%s\" TARGET=_blank>", hgTablesUrl(FALSE, tbl));
    puts("Table Browser</A> using the output format sequence.");
    }

hgSeqOptionsHtiCart(hti,cart);
puts("<P>");
cgiMakeButton("submit", "Get DNA");
if (dbIsFound)
    cgiMakeButton("submit", EXTENDED_DNA_BUTTON);
puts("</FORM><P>");
if (dbIsFound)
    puts("Note: The \"Mask repeats\" option applies only to \"Get DNA\", not to \"Extended case/color options\". <P>");
}

boolean dnaIgnoreTrack(char *track)
/* Return TRUE if this is one of the tracks too boring
 * to put DNA on. */
{
return (sameString("cytoBand", track) ||
	sameString("gcPercent", track) ||
	sameString("gold", track) ||
	sameString("gap", track) ||
	startsWith("mouseSyn", track));
}

struct customTrack *getCtList()
/* initialize theCtList if necessary and return it */
{
if (theCtList == NULL)
    theCtList = customTracksParseCart(database, cart, NULL, NULL);
return(theCtList);
}

struct trackDb *tdbForCustomTracks()
/* Load custom tracks (if any) and translate to list of trackDbs */
{
struct customTrack *ctList = getCtList();
struct customTrack *ct;
struct trackDb *tdbList = NULL, *tdb;

for (ct=ctList;  ct != NULL;  ct=ct->next)
    {
    AllocVar(tdb);
    tdb->track = ct->tdb->track;
    tdb->table = ct->tdb->table;
    tdb->shortLabel = ct->tdb->shortLabel;
    tdb->type = ct->tdb->type;
    tdb->longLabel = ct->tdb->longLabel;
    tdb->visibility = ct->tdb->visibility;
    tdb->priority = ct->tdb->priority;
    tdb->colorR = ct->tdb->colorR;
    tdb->colorG = ct->tdb->colorG;
    tdb->colorB = ct->tdb->colorB;
    tdb->altColorR = ct->tdb->altColorR;
    tdb->altColorG = ct->tdb->altColorG;
    tdb->altColorB = ct->tdb->altColorB;
    tdb->useScore = ct->tdb->useScore;
    tdb->private = ct->tdb->private;
    tdb->url = ct->tdb->url;
    tdb->grp = ct->tdb->grp;
    tdb->canPack = ct->tdb->canPack;
    trackDbPolish(tdb);
    slAddHead(&tdbList, tdb);
    }

slReverse(&tdbList);
return(tdbList);
}

static struct trackDb *getCustomTrackTdb(char *table)
/* Find the trackDb structure for a custom track table. */
{
struct customTrack *ctList = getCtList();
struct customTrack *ct = NULL;
for (ct = ctList; ct != NULL; ct = ct->next)
    if (sameString(table, ct->tdb->track))
	return  ct->tdb;
return NULL;
}

struct trackDb *getTdbForTrackName(char *trackName)
/* Given a track name (which may have ct_ or hub_ prepended), tdb */
{
assert(trackHash != NULL);
if (isCustomTrack(trackName))
    return getCustomTrackTdb(trackName);
else if (isHubTrack(trackName))
    return hubConnectAddHubForTrackAndFindTdb( database, trackName, NULL, trackHash);
else
    return hashMustFindVal(trackHash, trackName);
}


struct customTrack *lookupCt(char *name)
/* Return custom track for name, or NULL. */
{
struct customTrack *ct;

for (ct=getCtList();  ct != NULL;  ct=ct->next)
    if (sameString(name, ct->tdb->track))
	return(ct);

return(NULL);
}


void parseSs(char *ss, char **retPslName, char **retFaName, char **retQName)
/* Parse space separated 'ss' item. */
{
static char buf[512*2];
int wordCount;
char *words[4];
strcpy(buf, ss);
wordCount = chopLine(buf, words);

if (wordCount < 1)
    errAbort("Empty user cart variable ss.");
*retPslName = words[0];
if (retFaName != NULL)
    {
    if (wordCount < 2)
	errAbort("Expecting psl filename and fa filename in cart variable ss, but only got one word: %s", ss);
    *retFaName = words[1];
    }
if (retQName != NULL)
    {
    if (wordCount < 3)
	errAbort("Expecting psl filename, fa filename and query name in cart variable ss, but got this: %s", ss);
    *retQName = words[2];
    }
}

boolean ssFilesExist(char *ss)
/* Return TRUE if both files in ss exist. -- Copied from hgTracks! */
{
char *faFileName, *pslFileName;
parseSs(ss, &pslFileName, &faFileName, NULL);
return fileExists(pslFileName) && fileExists(faFileName);
}

struct trackDb *tdbForUserPsl()
/* Load up user's BLAT results into trackDb. */
{
char *ss = cartOptionalString(cart, "ss");

if ((ss != NULL) && !ssFilesExist(ss))
    {
    ss = NULL;
    cartRemove(cart, "ss");
    }

if (ss == NULL)
    return(NULL);
else
    {
    struct trackDb *tdb;
    AllocVar(tdb);
    tdb->track = cloneString(USER_PSL_TRACK_NAME);
    tdb->table = cloneString(USER_PSL_TRACK_NAME);
    tdb->shortLabel = cloneString(USER_PSL_TRACK_LABEL);
    tdb->type = cloneString("psl");
    tdb->longLabel = cloneString(USER_PSL_TRACK_LONGLABEL);
    tdb->visibility = tvFull;
    tdb->priority = 11.0;
    trackDbPolish(tdb);
    return(tdb);
    }
}

struct trackDb *rFindUnderstandableTrack(char *db, struct trackDb *tdb)
// If any leaf is usable in getting DNA then that leaf's tdb is returned.
{
if (tdb->subtracks != NULL)
    return rFindUnderstandableTrack(db,tdb->subtracks);

if (fbUnderstandTrack(db, tdb) && !dnaIgnoreTrack(tdb->table))
    return tdb;
else
    return NULL;
}

boolean forestHasUnderstandableTrack(char *db, struct trackDb *tdb)
// TRUE if any leaf is usable in getting DNA.
{
return (rFindUnderstandableTrack(db, tdb) != NULL);
}

struct trackDb* loadTracks()
/* load native tracks, cts, userPsl and track Hubs and return tdbList */
{
struct trackDb *tdbList = hTrackDb(database);
struct trackDb *ctdbList = tdbForCustomTracks();
struct trackDb *utdbList = tdbForUserPsl();

struct grp *pGrpList = NULL;
struct trackDb *hubList = hubCollectTracks(database, &pGrpList);

ctdbList = slCat(ctdbList, tdbList);
ctdbList = slCat(ctdbList, hubList);
tdbList = slCat(utdbList, ctdbList);
return tdbList;
}


void doGetDnaExtended1()
/* Do extended case/color get DNA options. */
{
boolean revComp  = cartUsualBoolean(cart, "hgSeq.revComp", FALSE);
boolean maskRep  = cartUsualBoolean(cart, "hgSeq.maskRepeats", FALSE);
int padding5     = cartUsualInt(cart, "hgSeq.padding5", 0);
int padding3     = cartUsualInt(cart, "hgSeq.padding3", 0);
int lineWidth    = cartUsualInt(cart, "lineWidth", 60);
char *casing     = cartUsualString(cart, "hgSeq.casing", "");
char *repMasking = cartUsualString(cart, "hgSeq.repMasking", "");
boolean caseUpper= FALSE;
char *pos = NULL;

struct trackDb *tdbList = loadTracks();

cartWebStart(cart, database, "Extended DNA Case/Color");

if (NULL != (pos = stripCommas(cartOptionalString(cart, "getDnaPos"))))
    hgParseChromRange(database, pos, &seqName, &winStart, &winEnd);
if (winEnd - winStart > 5000000)
    {
    printf("Please zoom in to 5 million bases or less to color the DNA");
    return;
    }

printf("<H1>Extended DNA Case/Color Options</H1>\n");
puts(
     "Use this page to highlight features in genomic DNA text. "
     "DNA covered by a particular track can be highlighted by "
     "case, underline, bold, italic, or color.  See below for "
     "details about color, and for examples. <B>Tracks in "
     "&quot;hide&quot; display mode are not shown in the grid below.</B> <P>");

if (cgiBooleanDefined("hgSeq.maskRepeats"))
    cartSetBoolean(cart, "hgSeq.maskRepeats", maskRep);
if (*repMasking != 0)
    cartSetString(cart, "hgSeq.repMasking", repMasking);
if (maskRep)
    {
    struct trackDb *rtdb;
    char *visString = cartOptionalString(cart, "rmsk");
    for (rtdb = tdbList;  rtdb != NULL;  rtdb=rtdb->next)
	{
	if (startsWith(rtdb->table, "rmsk"))
	    break;
	}
    printf("<P> <B>Note:</B> repeat masking style from previous page will <B>not</B> apply to this page.\n");
    if ((rtdb != NULL) &&
	((visString == NULL) || !sameString(visString, "hide")))
	printf("Use the case/color options for the RepeatMasker track below. <P>\n");
    else
	printf("Unhide the RepeatMasker track in the genome browser, then return to this page and use the case/color options for the RepeatMasker track below. <P>\n");
    }
cartSetInt(cart, "padding5", padding5);
cartSetInt(cart, "padding3", padding3);
if (sameString(casing, "upper"))
    caseUpper = TRUE;
if (*casing != 0)
    cartSetString(cart, "hgSeq.casing", casing);

printf("<FORM ACTION=\"%s\" METHOD=\"%s\">\n\n", hgcName(), cartUsualString(cart, "formMethod", "POST"));
cartSaveSession(cart);
cgiMakeHiddenVar("g", "htcGetDna3");

if (NULL != (pos = stripCommas(cartOptionalString(cart, "getDnaPos"))))
    {
    hgParseChromRange(database, pos, &seqName, &winStart, &winEnd);
    }
puts("Position ");
savePosInTextBox(seqName, winStart+1 - (revComp ? padding3 : padding5), winEnd + (revComp ? padding5 : padding3));
printf(" Reverse complement ");
cgiMakeCheckBox("hgSeq.revComp", revComp);
printf("<BR>\n");
printf("Letters per line ");
cgiMakeIntVar("lineWidth", lineWidth, 3);
printf(" Default case: ");
cgiMakeRadioButton("hgSeq.casing", "upper", caseUpper);
printf(" Upper ");
cgiMakeRadioButton("hgSeq.casing", "lower", !caseUpper);
printf(" Lower ");
cgiMakeButton("Submit", "Submit");
printf("<BR>\n");
printf("<TABLE BORDER=1>\n");
printf("<TR><TD>Track<BR>Name</TD><TD>Toggle<BR>Case</TD><TD>Under-<BR>line</TD><TD>Bold</TD><TD>Italic</TD><TD>Red</TD><TD>Green</TD><TD>Blue</TD></TR>\n");
struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    char *table = tdb->table;
    char *track = tdb->track;
    if ( sameString(USER_PSL_TRACK_NAME, table)
    ||   lookupCt(track) != NULL
    ||   (  tdbVisLimitedByAncestors(cart,tdb,TRUE,TRUE) != tvHide
         && forestHasUnderstandableTrack(database, tdb) ) )
        {
        char *visString = cartUsualString(cart, track, hStringFromTv(tdb->visibility));
         if (differentString(visString, "hide") && tdb->parent)
            {
            char *parentVisString = cartUsualString(cart, tdb->parentName,
                                        hStringFromTv(tdb->parent->visibility));
            if (sameString("hide", parentVisString))
                visString = "hide";
            }
	char buf[128];
	if (sameString(visString, "hide"))
	    {
	    char varName[256];
	    safef(varName, sizeof varName, "%s_case", track);
	    cartSetBoolean(cart, varName, FALSE);
	    safef(varName, sizeof varName, "%s_u", track);
	    cartSetBoolean(cart, varName, FALSE);
	    safef(varName, sizeof varName, "%s_b", track);
	    cartSetBoolean(cart, varName, FALSE);
	    safef(varName, sizeof varName, "%s_i", track);
	    cartSetBoolean(cart, varName, FALSE);
	    safef(varName, sizeof varName, "%s_red", track);
	    cartSetInt(cart, varName, 0);
	    safef(varName, sizeof varName, "%s_green", track);
	    cartSetInt(cart, varName, 0);
	    safef(varName, sizeof varName, "%s_blue", track);
	    cartSetInt(cart, varName, 0);
	    }
	else
	    {
	    printf("<TR>");
	    printf("<TD>%s</TD>", tdb->shortLabel);
	    safef(buf, sizeof buf, "%s_case", tdb->track);
	    printf("<TD>");
	    cgiMakeCheckBox(buf, cartUsualBoolean(cart, buf, FALSE));
	    printf("</TD>");
	    safef(buf, sizeof buf, "%s_u", tdb->track);
	    printf("<TD>");
	    cgiMakeCheckBox(buf, cartUsualBoolean(cart, buf, FALSE));
	    printf("</TD>");
	    safef(buf, sizeof buf, "%s_b", tdb->track);
	    printf("<TD>");
	    cgiMakeCheckBox(buf, cartUsualBoolean(cart, buf, FALSE));
	    printf("</TD>");
	    safef(buf, sizeof buf, "%s_i", tdb->track);
	    printf("<TD>");
	    cgiMakeCheckBox(buf, cartUsualBoolean(cart, buf, FALSE));
	    printf("</TD>");
	    printf("<TD>");
	    safef(buf, sizeof buf, "%s_red", tdb->track);
	    cgiMakeIntVar(buf, cartUsualInt(cart, buf, 0), 3);
	    printf("</TD>");
	    printf("<TD>");
	    safef(buf, sizeof buf, "%s_green", tdb->track);
	    cgiMakeIntVar(buf, cartUsualInt(cart, buf, 0), 3);
	    printf("</TD>");
	    printf("<TD>");
	    safef(buf, sizeof buf, "%s_blue", tdb->track);
	    cgiMakeIntVar(buf, cartUsualInt(cart, buf, 0), 3);
	    printf("</TD>");
	    printf("</TR>\n");
	    }
	}
    }
printf("</TABLE>\n");
printf("</FORM>\n");
printf("<H3>Coloring Information and Examples</H3>\n");
puts("The color values range from 0 (darkest) to 255 (lightest) and are additive.\n");
puts("The examples below show a few ways to highlight individual tracks, "
     "and their interplay. It's good to keep it simple at first.  It's easy "
     "to make pretty, but completely cryptic, displays with this feature.");
puts(
     "<UL>"
     "<LI>To put exons from RefSeq Genes in upper case red text, check the "
     "appropriate box in the Toggle Case column and set the color to pure "
     "red, RGB (255,0,0). Upon submitting, any RefSeq Gene within the "
     "designated chromosomal interval will now appear in red capital letters.\n"
     "<LI>To see the overlap between RefSeq Genes and Genscan predictions try "
     "setting the RefSeq Genes to red (255,0,0) and Genscan to green (0,255,0). "
     "Places where the RefSeq Genes and Genscan overlap will be painted yellow "
     "(255,255,0).\n"
     "<LI>To get a level-of-coverage effect for tracks like Spliced Ests with "
     "multiple overlapping items, initially select a darker color such as deep "
     "green, RGB (0,64,0). Nucleotides covered by a single EST will appear dark "
     "green, while regions covered with more ESTs get progressively brighter &mdash; "
     "saturating at 4 ESTs."
     "<LI>Another track can be used to mask unwanted features. Setting the "
     "RepeatMasker track to RGB (255,255,255) will white-out Genscan predictions "
     "of LINEs but not mainstream host genes; masking with RefSeq Genes will show "
     "what is new in the gene prediction sector."
     "</UL>");
puts("<H3>Further Details and Ideas</H3>");
puts("<P>Copying and pasting the web page output to a text editor such as Word "
     "will retain upper case but lose colors and other formatting. That is still "
     "useful because other web tools such as "
     "<A HREF=\"https://www.ncbi.nlm.nih.gov/blast\" TARGET=_BLANK>NCBI Blast</A> "
     "can be set to ignore lower case.  To fully capture formatting such as color "
     "and underlining, view the output as \"source\" in your web browser, or download "
     "it, or copy the output page into an html editor.</P>");
puts("<P>The default line width of 60 characters is standard, but if you have "
     "a reasonable sized monitor it's useful to set this higher - to 125 characters "
     "or more.  You can see more DNA at once this way, and fewer line breaks help "
     "in finding DNA strings using the web browser search function.</P>");
puts("<P>Be careful about requesting complex formatting for a very large "
     "chromosomal region.  After all the html tags are added to the output page, "
     "the file size may exceed size limits that your browser, clipboard, and "
     "other software can safely display.  The tool will format 10 Mb and more, however.</P>");
trackDbFreeList(&tdbList);
}

void doGetBlastPep(char *readName, char *table)
/* get predicted protein */
{
int qStart;
struct psl *psl;
int start, end;
struct sqlResult *sr;
struct sqlConnection *conn = hAllocConn(database);
struct dnaSeq *tSeq;
char query[256], **row;
char fullTable[HDB_MAX_TABLE_STRING];
boolean hasBin;
char *buffer, *str;
int i, j;
char *ptr;

start = cartInt(cart, "o");
if (!hFindSplitTable(database, seqName, table, fullTable, sizeof fullTable, &hasBin))
    errAbort("doGetBlastPep track %s not found", table);
sqlSafef(query, sizeof query, "select * from %s where qName = '%s' and tName = '%s' and tStart=%d",
	fullTable, readName, seqName, start);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find alignment for %s at %d", readName, start);
psl = pslLoad(row+hasBin);
sqlFreeResult(&sr);
hFreeConn(&conn);
printf("<PRE><TT>");
end = psl->tEnd;
if (psl->strand[1] == '+')
    end = psl->tStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1] *3;
if ((ptr = strchr(readName, '.')) != NULL)
    *ptr++ = 0;

printf(">%s-%s\n", readName,database);
tSeq = hDnaFromSeq(database, psl->tName, start, end, dnaLower);

if (psl->strand[1] == '-')
    {
    start = psl->tSize - end;
    reverseComplement(tSeq->dna, tSeq->size);
    }

str = buffer = needMem(psl->qSize + 1);

qStart = 0;
for (i=0; i<psl->blockCount; ++i)
    {
    int ts = psl->tStarts[i] - start;
    int sz = psl->blockSizes[i];

    for (;qStart < psl->qStarts[i]; qStart++)
	*str++ = 'X';

    for (j=0; j<sz; ++j)
	{
	int codonStart = ts + 3*j;
	DNA *codon = &tSeq->dna[codonStart];
	if ((*str = lookupCodon(codon)) == 0)
	    *str = '*';
	str++;
	qStart++;
	}
    }

*str = 0;
printLines(stdout, buffer, 50);
printf("</TT></PRE>");
}


void doGetDna2()
/* Do second DNA dialog (or just fetch DNA) */
{
char *tbl = cgiUsualString("table", "");
char *action = cgiUsualString("submit", "");
int itemCount;
char *pos = NULL;
char *chrom = NULL;
int start = 0;
int end = 0;

if (sameString(action, EXTENDED_DNA_BUTTON))
    {
    doGetDnaExtended1();
    return;
    }
// This output probably should be just text/plain but
// trying to support the fancy warn handler box requires html.
// But we want to keep it very simple and close to a plain text dump.

cartHtmlStart("DNA");
puts("<PRE>");
if (tbl[0] == 0)
    {
    itemCount = 1;
    if ( NULL != (pos = stripCommas(cartOptionalString(cart, "getDnaPos"))) &&
         hgParseChromRange((dbIsFound ? database : NULL), pos, &chrom, &start, &end))
        {
        hgSeqRange(database, chrom, start, end, '?', "dna");
        }
    else
        {
        hgSeqRange(database, seqName, cartInt(cart, "l"), cartInt(cart, "r"),
                   '?', "dna");
        }
    }
else
    {
    struct hTableInfo *hti = NULL;
    char rootName[HDB_MAX_TABLE_STRING];
    char parsedChrom[HDB_MAX_CHROM_STRING];

    /* use the values from the dnaPos dialog box */
    if (!( NULL != (pos = stripCommas(cartOptionalString(cart, "getDnaPos"))) &&
         hgParseChromRange(database, pos, &chrom, &start, &end)))
	 {
	 /* if can't get DnaPos from dialog box, use "o" and "t" */
	 start = cartInt(cart, "o");
	 end = cartInt(cart, "t");
	 }

    /* Table might be a custom track if it's not in the database,
     * or bigBed if it is in the database but has only one column called 'fileName';
     * in which case, just get DNA as if no table were given. */
    hParseTableName(database, tbl, rootName, parsedChrom);
    if (!trackHubDatabase(database))
	hti = hFindTableInfo(database, seqName, rootName);
    if (hti == NULL || hti->startField[0] == 0)
	{
	itemCount = 1;
	hgSeqRange(database, seqName, start, end, '?', tbl);
	}
    else
	{
	char *where = NULL;
	char *item = cgiUsualString("i", "");
	char buf[256];
	if ((hti->nameField[0] != 0) && (item[0] != 0))
	    {
	    sqlSafef(buf, sizeof(buf), "%s = '%s'", hti->nameField, item);
	    where = buf;
	    }
	itemCount = hgSeqItemsInRange(database, tbl, seqName, start, end, where);
	}
    }
if (itemCount == 0)
    printf("\n# No results returned from query.\n\n");

puts("</PRE>");
}

struct hTableInfo *ctToHti(struct customTrack *ct)
/* Create an hTableInfo from a customTrack. */
{
struct hTableInfo *hti;

AllocVar(hti);
hti->rootName = cloneString(ct->tdb->table);
hti->isPos = TRUE;
hti->isSplit = FALSE;
hti->hasBin = FALSE;
hti->type = cloneString(ct->tdb->type);
int fieldCount = 3;
if (sameWord(ct->dbTrackType, "bedDetail"))
    fieldCount = ct->fieldCount - 2;
else if (sameWord(ct->dbTrackType, "pgSnp"))
    fieldCount = 4;
else
    fieldCount = ct->fieldCount;
if (fieldCount >= 3)
    {
    strncpy(hti->chromField, "chrom", 32);
    strncpy(hti->startField, "chromStart", 32);
    strncpy(hti->endField, "chromEnd", 32);
    }
if (fieldCount >= 4)
    {
    strncpy(hti->nameField, "name", 32);
    }
if (fieldCount >= 5)
    {
    strncpy(hti->scoreField, "score", 32);
    }
if (fieldCount >= 6)
    {
    strncpy(hti->strandField, "strand", 32);
    }
if (fieldCount >= 8)
    {
    strncpy(hti->cdsStartField, "thickStart", 32);
    strncpy(hti->cdsEndField, "thickEnd", 32);
    hti->hasCDS = TRUE;
    }
if (fieldCount >= 12)
    {
    strncpy(hti->countField, "blockCount", 32);
    strncpy(hti->startsField, "chromStarts", 32);
    strncpy(hti->endsSizesField, "blockSizes", 32);
    hti->hasBlocks = TRUE;
    }

return(hti);
}

struct hTableInfo *htiForUserPsl()
/* Create an hTableInfo for user's BLAT results. */
{
struct hTableInfo *hti;

AllocVar(hti);
hti->rootName = cloneString(USER_PSL_TRACK_NAME);
hti->isPos = TRUE;
hti->isSplit = FALSE;
hti->hasBin = FALSE;
hti->type = cloneString("psl");
strncpy(hti->chromField, "tName", 32);
strncpy(hti->startField, "tStart", 32);
strncpy(hti->endField, "tEnd", 32);
strncpy(hti->nameField, "qName", 32);
/* psl can be scored... but strictly speaking, does not have a score field! */
strncpy(hti->strandField, "strand", 32);
hti->hasCDS = FALSE;
strncpy(hti->countField, "blockCount", 32);
strncpy(hti->startsField, "tStarts", 32);
strncpy(hti->endsSizesField, "tSizes", 32);
hti->hasBlocks = TRUE;

return(hti);
}

struct bed *bedFromUserPsl()
/* Load up user's BLAT results into bedList. */
{
struct bed *bedList = NULL;
char *ss = cartOptionalString(cart, "ss");

if ((ss != NULL) && ! ssFilesExist(ss))
    {
    ss = NULL;
    cartRemove(cart, "ss");
    }

if (ss == NULL)
    return(NULL);
else
    {
    struct lineFile *f;
    struct psl *psl;
    enum gfType qt, tt;
    char *faFileName, *pslFileName;
    int i;

    parseSs(ss, &pslFileName, &faFileName, NULL);
    pslxFileOpen(pslFileName, &qt, &tt, &f);
    while ((psl = pslNext(f)) != NULL)
	{
	struct bed *bed;
	AllocVar(bed);
	bed->chrom = cloneString(seqName);
	bed->chromStart = psl->tStart;
	bed->chromEnd = psl->tEnd;
	bed->name = cloneString(psl->qName);
	bed->score = pslScore(psl);
	if ((psl->strand[0] == '-' && psl->strand[1] == '+') ||
	    (psl->strand[0] == '+' && psl->strand[1] == '-'))
	    strncpy(bed->strand, "-", 2);
	else
	    strncpy(bed->strand, "+", 2);
	bed->thickStart = bed->chromStart;
	bed->thickEnd   = bed->chromEnd;
	bed->blockCount = psl->blockCount;
	bed->chromStarts = needMem(bed->blockCount * sizeof(int));
	bed->blockSizes  = needMem(bed->blockCount * sizeof(int));
	for (i=0;  i < bed->blockCount;  i++)
	    {
	    bed->chromStarts[i] = psl->tStarts[i];
	    bed->blockSizes[i]  = psl->blockSizes[i];
	    }
	if (qt == gftProt)
	    for (i=0;  i < bed->blockCount;  i++)
		{
		/* If query is protein, blockSizes are in aa units; fix 'em. */
		bed->blockSizes[i] *= 3;
		}
	if (psl->strand[1] == '-')
	    {
	    /* psl: if target strand is '-', flip the coords.
	     * (this is the target part of pslRc from src/lib/psl.c) */
	    for (i=0;  i < bed->blockCount;  ++i)
		{
		bed->chromStarts[i] =
		    psl->tSize - (bed->chromStarts[i] +
				  bed->blockSizes[i]);
		}
	    reverseInts(bed->chromStarts, bed->blockCount);
	    reverseInts(bed->blockSizes, bed->blockCount);
	    assert(bed->chromStart == bed->chromStarts[0]);
	    }
        /* translate absolute starts to relative starts (after handling
         * target-strand coord-flipping) */
	for (i=0;  i < bed->blockCount;  i++)
	    {
	    bed->chromStarts[i] -= bed->chromStart;
	    }
	slAddHead(&bedList, bed);
	pslFree(&psl);
	}
    lineFileClose(&f);
    slReverse(&bedList);
    return(bedList);
    }
}


void addColorToRange(int r, int g, int b, struct rgbColor *colors, int start, int end)
/* Add rgb values to colors array from start to end.  Don't let values
 * exceed 255 */
{
struct rgbColor *c;
int rr, gg, bb;
int i;
for (i=start; i<end; ++i)
    {
    c = colors+i;
    rr = c->r + r;
    if (rr > 255) rr = 255;
    c->r = rr;
    gg = c->g + g;
    if (gg > 255) gg = 255;
    c->g = gg;
    bb = c->b + b;
    if (bb > 255) bb = 255;
    c->b = bb;
    }
}

void getDnaHandleBits(char *track, char *type, Bits *bits,
                      int winStart, int winEnd, boolean isRc,
                      struct featureBits *fbList)
/* See if track_type variable exists, and if so set corresponding bits. */
{
char buf[256];
struct featureBits *fb;
int s,e;
int winSize = winEnd - winStart;

safef(buf, sizeof buf, "%s_%s", track, type);
if (cgiBoolean(buf))
    {
    for (fb = fbList; fb != NULL; fb = fb->next)
	{
	s = fb->start - winStart;
	e = fb->end - winStart;
	if (isRc)
	    reverseIntRange(&s, &e, winSize);
	bitSetRange(bits, s, e - s);
	}
    }
}

static boolean clipFbToWindow( struct featureBits *fb, int winStart, int winEnd)
{
if ((fb->start > winEnd) || (fb->end < winStart))
    return FALSE;

if (fb->start < winStart)
    fb->start = winStart;
if (fb->end > winEnd)
    fb->end = winEnd;

return TRUE;
}

static struct featureBits *getBigBedFbList(struct trackDb *tdb, char *seqName, int winStart, int winEnd)
/* Get a list of featureBits structures from a bigBed file. */
{
struct lm *lm = lmInit(0);
char *fileName = bbiNameFromSettingOrTable(tdb, NULL, tdb->table);
struct bbiFile *bbi =  bigBedFileOpenAlias(fileName, chromAliasFindAliases);
struct bigBedInterval *bb, *bbList = bigBedIntervalQuery(bbi, seqName, winStart, winEnd, 0, lm);
char *bedRow[32];
char startBuf[16], endBuf[16];
struct featureBits *fbList = NULL, *fb;
//struct bed *bedList = NULL;
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    bigBedIntervalToRow(bb, seqName, startBuf, endBuf, bedRow, ArraySize(bedRow));
    struct bed *bed = bedLoadN(bedRow, bbi->definedFieldCount);
    if (bbi->definedFieldCount >= 12)
        {
        int ii;
        for (ii = 0; ii < bed->blockCount; ii++)
            {
            AllocVar(fb);
            fb->name = bed->name;
            fb->start = bed->chromStart + bed->chromStarts[ii];
            fb->end = bed->chromStart + bed->chromStarts[ii] + bed->blockSizes[ii];
            fb->strand = '+';
            if (bed->strand[0])
                fb->strand = bed->strand[0];
            if (!clipFbToWindow(fb, winStart,winEnd))
                continue;
            slAddHead(&fbList, fb);
            }
        }
    else
        {
        AllocVar(fb);
        fb->name = bed->name;
        fb->start = bed->chromStart;
        fb->end = bed->chromEnd;
        fb->strand = '+';
        if (bed->strand[0])
            fb->strand = bed->strand[0];
        if (clipFbToWindow(fb, winStart,winEnd))
            slAddHead(&fbList, fb);
        }
    }
return fbList;
}

static struct featureBits *vcfLoadInterval(struct trackDb *tdb, int start, int end)
{
struct featureBits *fbList = NULL;
if (sameString(tdb->type, "vcf") || sameString(tdb->type, "vcfPhasedTrio"))
   doVcfDetailsExt(tdb, NULL, &fbList, start, end);
else if (sameString(tdb->type, "vcfTabix"))
   doVcfTabixDetailsExt(tdb, NULL, &fbList, start, end);
return fbList;
}


void doGetDna3()
/* Fetch DNA in extended color format */
{
struct dnaSeq *seq;
struct cfm *cfm;
int i;
boolean isRc = cartUsualBoolean(cart, "hgSeq.revComp", FALSE);
boolean defaultUpper = sameString(cartString(cart, "hgSeq.casing"), "upper");
int winSize;
int lineWidth = cartInt(cart, "lineWidth");
struct rgbColor *colors;

struct trackDb *tdbList = loadTracks();

char *pos = NULL;
Bits *uBits;	/* Underline bits. */
Bits *iBits;    /* Italic bits. */
Bits *bBits;    /* Bold bits. */

if (NULL != (pos = stripCommas(cartOptionalString(cart, "getDnaPos"))))
    hgParseChromRange(database, pos, &seqName, &winStart, &winEnd);

winSize = winEnd - winStart;
uBits = bitAlloc(winSize);	/* Underline bits. */
iBits = bitAlloc(winSize);	/* Italic bits. */
bBits = bitAlloc(winSize);	/* Bold bits. */

cartWebStart(cart, database, "Extended DNA Output");
printf("<PRE><TT>");
printf(">%s:%d-%d %s\n", seqName, winStart+1, winEnd,
       (isRc ? "(reverse complement)" : ""));
seq = hDnaFromSeq(database, seqName, winStart, winEnd, dnaLower);
if (isRc)
    reverseComplement(seq->dna, seq->size);
if (defaultUpper)
    touppers(seq->dna);

AllocArray(colors, winSize);

struct trackDb* tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    char *track = tdb->track;
    char *table = tdb->table;
    struct featureBits *fbList = NULL, *fb;
    struct customTrack *ct = lookupCt(track);
    if (sameString(USER_PSL_TRACK_NAME, table)
    ||  ct != NULL
    ||  (   tdbVisLimitedByAncestors(cart,tdb,TRUE,TRUE) != tvHide
        && forestHasUnderstandableTrack(database, tdb) ) )
        
        {
        char buf[256];
        int r,g,b;
        /* to save a LOT of time, don't fetch track features unless some
         * coloring/formatting has been specified for them. */
	boolean hasSettings = FALSE;
	safef(buf, sizeof(buf), "%s_u", track);
	hasSettings |= cgiBoolean(buf);
	safef(buf, sizeof(buf), "%s_b", track);
	hasSettings |= cgiBoolean(buf);
	safef(buf, sizeof(buf), "%s_i", track);
	hasSettings |= cgiBoolean(buf);
	safef(buf, sizeof(buf), "%s_case", track);
	hasSettings |= cgiBoolean(buf);
	safef(buf, sizeof(buf), "%s_red", track);
	hasSettings |= (cgiOptionalInt(buf, 0) != 0);
	safef(buf, sizeof(buf), "%s_green", track);
	hasSettings |= (cgiOptionalInt(buf, 0) != 0);
	safef(buf, sizeof(buf), "%s_blue", track);
	hasSettings |= (cgiOptionalInt(buf, 0) != 0);
	if (! hasSettings)
	    continue;

	if (sameString(USER_PSL_TRACK_NAME, track))
	    {
	    struct hTableInfo *hti = htiForUserPsl();
	    struct bedFilter *bf;
	    struct bed *bedList, *bedList2;
	    AllocVar(bf);
	    bedList = bedFromUserPsl();
	    bedList2 = bedFilterListInRange(bedList, bf, seqName, winStart,
					    winEnd);
	    fbList = fbFromBed(database, track, hti, bedList2, winStart, winEnd,
			       TRUE, FALSE);
	    bedFreeList(&bedList);
	    bedFreeList(&bedList2);
	    }
	else if (ct != NULL)
	    {
	    struct hTableInfo *hti = ctToHti(ct);
	    struct bedFilter *bf;
	    struct bed *bedList2, *ctBedList = NULL;
	    AllocVar(bf);
            if (ct->dbTrack && (!sameString(tdb->type, "vcf")))
                {
                struct bed *bed;
                int fieldCount = ct->fieldCount;
                if ((ct->dbTrackType != NULL) && sameString(ct->dbTrackType, "pgSnp"))
                    fieldCount = 4;
                char query[512];
                int rowOffset;
                char **row;
                struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
                struct sqlResult *sr = NULL;

                sqlSafef(query, sizeof(query), "select * from %s", ct->dbTableName);
                sr = hRangeQuery(conn, ct->dbTableName, seqName,
                    winStart, winEnd, NULL, &rowOffset);
                while ((row = sqlNextRow(sr)) != NULL)
                    {
                    bed = bedLoadN(row+rowOffset, fieldCount);
                    if (bf == NULL || bedFilterOne(bf, bed))
                        {
                        struct bed *copy = cloneBed(bed);
                        slAddHead(&ctBedList, copy);
                        }
                    }
                sqlFreeResult(&sr);
                hFreeConn(&conn);
                }
            else if (sameString(ct->dbTrackType, "bigBed"))
                {
                struct lm *lm = lmInit(0);
                struct bigBedInterval *bb, *bbList = bigBedIntervalQuery(ct->bbiFile, seqName, winStart, winEnd, 0, lm);
                char *bedRow[32];
                char startBuf[16], endBuf[16];
                for (bb = bbList; bb != NULL; bb = bb->next)
                    {
                    bigBedIntervalToRow(bb, seqName, startBuf, endBuf, bedRow, ArraySize(bedRow));
                    struct bed *bed = bedLoadN(bedRow, ct->bbiFile->definedFieldCount);
                    slAddHead(&ctBedList, bed);
                    }
                }
            else
                {
                ctBedList = ct->bedList;
                }
	    if (startsWith("vcf", tdb->type))
                {
		fbList = vcfLoadInterval(ct->tdb, winStart, winEnd);
                }
            else
		{
		bedList2 = bedFilterListInRange(ctBedList, bf, seqName, winStart,
					    winEnd);
		fbList = fbFromBed(database, track, hti, bedList2, winStart, winEnd,
			       TRUE, FALSE);
		bedFreeList(&bedList2);
		}
            if (!ct->bedList)
                bedFreeList(&ctBedList);
	    }
	else
            {
            if (tdb->subtracks)
                {
                struct slRef *refLeaves = trackDbListGetRefsToDescendantLeaves(tdb->subtracks);
                struct slRef *refLeaf = NULL;
                while ((refLeaf = slPopHead(&refLeaves)) != NULL)
                    {
                    struct trackDb *tdbLeaf = refLeaf->val;
                    if (tdbVisLimitedByAncestors(cart,tdbLeaf,TRUE,TRUE) != tvHide
                    &&  fbUnderstandTrack(database, tdbLeaf)
                    && !dnaIgnoreTrack(tdbLeaf->table))
                        {
                        struct featureBits *fbLeafList;
			if (startsWith("vcf", tdbLeaf->type))
			    fbLeafList = vcfLoadInterval(tdbLeaf, winStart, winEnd);
                        else if (startsWith("big", tdbLeaf->type))
                            fbLeafList = getBigBedFbList(tdbLeaf, seqName, winStart, winEnd);
                        else
                            fbLeafList = fbGetRange(database, tdbLeaf->table, seqName, winStart, winEnd);
                        if (fbLeafList != NULL)
                            fbList = slCat(fbList,fbLeafList);
                        }
                    freeMem(refLeaf);
                    }
                }
            else
                {
                if (startsWith("vcf", tdb->type))
                    fbList = vcfLoadInterval(tdb, winStart, winEnd);
                else if (startsWith("big", tdb->type))
                    fbList = getBigBedFbList(tdb, seqName, winStart, winEnd);
                else
                    fbList = fbGetRange(database, tdb->table, seqName, winStart, winEnd);
                }
            }

        /* Flip underline/italic/bold bits. */
        getDnaHandleBits(track, "u", uBits, winStart, winEnd, isRc, fbList);
        getDnaHandleBits(track, "b", bBits, winStart, winEnd, isRc, fbList);
	getDnaHandleBits(track, "i", iBits, winStart, winEnd, isRc, fbList);

	/* Toggle case if necessary. */
	safef(buf, sizeof buf, "%s_case", track);
	if (cgiBoolean(buf))
	    {
	    for (fb = fbList; fb != NULL; fb = fb->next)
	        {
		DNA *dna;
		int start = fb->start - winStart;
		int end  = fb->end - winStart;
		int size = fb->end - fb->start;
		if (isRc)
		    reverseIntRange(&start, &end, seq->size);
		dna = seq->dna + start;
		if (defaultUpper)
		    toLowerN(dna, size);
		else
		    toUpperN(dna, size);
		}
	    }

	/* Add in RGB values if necessary. */
	safef(buf, sizeof buf, "%s_red", track);
	r = cartInt(cart, buf);
	safef(buf, sizeof buf, "%s_green", track);
	g = cartInt(cart, buf);
	safef(buf, sizeof buf, "%s_blue", track);
	b = cartInt(cart, buf);
	if (r != 0 || g != 0 || b != 0)
	    {
	    for (fb = fbList; fb != NULL; fb = fb->next)
	        {
		int s = fb->start - winStart;
		int e = fb->end - winStart;
		if (isRc)
		    reverseIntRange(&s, &e, winEnd - winStart);
		addColorToRange(r, g, b, colors, s, e);
		}
	    }
	}
    }

cfm = cfmNew(0, lineWidth, FALSE, FALSE, stdout, 0);
for (i=0; i<seq->size; ++i)
    {
    struct rgbColor *color = colors+i;
    int c = (color->r<<16) + (color->g<<8) + color->b;
    cfmOutExt(cfm, seq->dna[i], c,
	      bitReadOne(uBits, i), bitReadOne(bBits, i), bitReadOne(iBits, i));
    }
cfmFree(&cfm);
freeDnaSeq(&seq);
bitFree(&uBits);
bitFree(&iBits);
bitFree(&bBits);
}

void medlineLinkedTermLine(char *title, char *text, char *search, char *keyword)
/* Produce something that shows up on the browser as
 *     TITLE: value
 * with the value hyperlinked to medline using a specified search term. */
{
char *encoded = cgiEncode(search);
char *encodedKeyword = cgiEncode(keyword);

printf("<B>%s:</B> ", title);
if (sameWord(text, "n/a") || sameWord(text, "none"))
    printf("n/a<BR>\n");
else
    {
    printf("<A HREF=\"");
    printEntrezPubMedPureSearchUrl(stdout, encoded, encodedKeyword);
    printf("\" TARGET=_blank>%s</A><BR>\n", text);
    }
freeMem(encoded);
}
void medlineLinkedLine(char *title, char *text, char *search)
/* Produce something that shows up on the browser as
 *     TITLE: value
 * with the value hyperlinked to medline. */
{
char *encoded = cgiEncode(search);

printf("<B>%s:</B> ", title);
if (sameWord(text, "n/a"))
    printf("n/a<BR>\n");
else
    {
    printf("<A HREF=\"");
    printEntrezPubMedUrl(stdout, encoded);
    printf("\" TARGET=_blank>%s</A><BR>\n", text);
    }
freeMem(encoded);
}

void medlineProductLinkedLine(char *title, char *text)
/* Produce something that shows up on the browser as
 *     TITLE: value
 * with the value hyperlinked to medline.
 * Replaces commas in the product name with spaces, as commas sometimes
 * interfere with PubMed search */
{
    subChar(text, ',', ' ');
    medlineLinkedLine(title, text, text);
}

void appendAuthor(struct dyString *dy, char *gbAuthor, int len)
/* Convert from  Kent,W.J. to Kent WJ and append to dy.
 * gbAuthor gets eaten in the process.
 * Also strip web URLs since Entrez doesn't like those. */
{
char buf[MAXURLSIZE];
char *ptr;

if (len >= sizeof(buf))
    warn("author %s too long to process", gbAuthor);
else
    {
    memcpy(buf, gbAuthor, len);
    buf[len] = 0;
    stripChar(buf, '.');
    subChar(buf, ',' , ' ');
    if ((ptr = strstr(buf, " http://")) != NULL)
        *ptr = 0;
    if ((ptr = strstr(buf, " https://")) != NULL)
        *ptr = 0;
    dyStringAppend(dy, buf);
    dyStringAppend(dy, " ");
    }
}

void gbToEntrezAuthor(char *authors, struct dyString *dy)
/* Convert from Genbank author format:
 *      Kent,W.J., Haussler,D. and Zahler,A.M.
 * to Entrez search format:
 *      Kent WJ,Haussler D,Zahler AM
 */
{
char *s = authors, *e;

/* Parse first authors, which will be terminated by '.,' */
while ((e = strstr(s, ".,i ")) != NULL)
    {
    int len = e - s + 1;
    appendAuthor(dy, s, len);
    s += len+2;
    }
if ((e = strstr(s, " and")) != NULL)
    {
    int len = e - s;
    appendAuthor(dy, s, len);
    s += len+4;
    }
if ((s = skipLeadingSpaces(s)) != NULL && s[0] != 0)
    {
    int len = strlen(s);
    appendAuthor(dy, s, len);
    }
}

/* --- !!! Riken code is under development Fan. 4/16/02 */
void printRikenInfo(char *acc, struct sqlConnection *conn )
/* Print Riken annotation info */
{
struct sqlResult *sr;
char **row;
char query[512];
char *seqid, *accession, *comment;
// char *qualifier, *anntext, *datasrc, *srckey, *href, *evidence;

accession = acc;
sqlSafef(query, sizeof(query),
         "select seqid from rikenaltid where altid='%s';", accession);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);

if (row != NULL)
    {
    seqid=cloneString(row[0]);

    sqlSafef(query, sizeof(query),
             "select Qualifier, Anntext, Datasrc, Srckey, Href, Evidence "
             "from rikenann where seqid='%s';", seqid);

    sqlFreeResult(&sr);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);

    while (row !=NULL)
	{
	// qualifier = row[0];  unused variable
	// anntext   = row[1];  unused variable
	// datasrc   = row[2];  unused variable
	// srckey    = row[3];  unused variable
	// href      = row[4];  unused variable
        // evidence  = row[5];  unused variable
        row = sqlNextRow(sr);
        }

    sqlSafef(query, sizeof(query),
             "select comment from rikenseq where id='%s';", seqid);
    sqlFreeResult(&sr);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);

    if (row != NULL)
	{
	comment = row[0];
	printf("<B>Riken/comment:</B> %s<BR>\n",comment);
	}
    }
}

void printGeneCards(char *geneName)
/* Print out a link to GeneCards (Human only). */
{
if (startsWith("hg", database) && isNotEmpty(geneName))
    {
    printf("<B>GeneCards:</B> "
	   "<A HREF = \"http://www.genecards.org/cgi-bin/cardsearch.pl?"
	   "search=%s\" TARGET=_blank>%s</A><BR>\n",
	   geneName, geneName);
    }
}

int getImageId(struct sqlConnection *conn, char *acc)
/* get the image id for a clone, or 0 if none */
{
int imageId = 0;
if (sqlTableExists(conn, imageCloneTable))
    {
    struct sqlResult *sr;
    char **row;
    char query[128];
    sqlSafef(query, sizeof(query),
          "select imageId from %s where acc = '%s'",imageCloneTable, acc);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        imageId = sqlUnsigned(row[0]);
    sqlFreeResult(&sr);
    }
return imageId;
}

void htcDisplayMrna(char *acc)
/* Display mRNA available from genback or seq table.. */
{
struct dnaSeq *seq = hGenBankGetMrna(database, acc, NULL);
if (seq == NULL)
    errAbort("mRNA sequence %s not found", acc);

cartHtmlStart("mRNA sequence");
printf("<PRE><TT>");
faWriteNext(stdout, seq->name, seq->dna, seq->size);
printf("</TT></PRE>");
dnaSeqFree(&seq);
}

static int getEstTranscriptionDir(struct sqlConnection *conn, struct psl *psl)
/* get the direction of transcription for an EST; return splice support count */
{
char query[256], estOrient[64];
sqlSafef(query, sizeof(query),
      "select intronOrientation from %s.estOrientInfo where chrom = '%s' and chromStart = %d and name = '%s'",
      database, psl->tName, psl->tStart, psl->qName);
if (sqlQuickQuery(conn, query, estOrient, sizeof(estOrient)) != NULL)
    return sqlSigned(estOrient) * ((psl->strand[0] == '+') ? 1 : -1);
else
    return 0;
}

static struct gbWarn *checkGbWarn(struct sqlConnection *conn, char *acc)
/* check if there is a gbWarn entry for this accession, return NULL if none */
{
struct gbWarn *gbWarn = NULL;
if (sqlTableExists(conn, gbWarnTable))
    gbWarn = sqlQueryObjs(conn, (sqlLoadFunc)gbWarnLoad, sqlQuerySingle,
                          "SELECT * FROM %s WHERE acc = \"%s\"", gbWarnTable, acc);
return gbWarn;
}

static void printGbWarn(char *acc, struct gbWarn *gbWarn)
/* print descriptive information about an accession in the gbWarn table */
{
char *msg = NULL;
switch (gbWarn->reason) {
case gbWarnInvitroNorm:
    msg = "is from the InVitroGen/Genoscope full-length library.  Some of the entries "
        "associated with this dataset appear to have been aligned to the reference "
        "genome and the sequences subsequently modified to match the genome. This "
        "process may have resulted in apparent high-quality alignments to pseudogenes.";
    break;
case gbWarnAthRage:
    msg = "is from the Athersys RAGE library.  These sequences were created by inducing expression and may not "
        "be an indication of in vivo expression.";
    break;
case gbWarnOrestes:
    msg = "is from an ORESTES library.  This protocol includes a PCR step subject to genomic contamination.";
    break;
}
assert(msg != NULL);
char *msg2= "Care should be taken in using alignments of this sequence as evidence of transcription.";
printf("<B>Warning:<span style='color:red;'> %s %s %s</span></B><BR>\n", acc, msg, msg2);
}

static void printRnaSpecs(struct trackDb *tdb, char *acc, struct psl *psl)
/* Print auxiliarry info on RNA. */
{
struct dyString *dy = dyStringNew(1024);
struct sqlConnection *conn = hAllocConn(database);
struct sqlConnection *conn2= hAllocConn(database);
struct sqlResult *sr;
char **row;
char rgdEstId[512];
char query[256];
char *type,*direction,*orgFullName,*library,*clone,*sex,*tissue,
    *development,*cell,*cds,*description, *author,*geneName,
    *date,*productName;
// char *source;  unused variable
// int seqSize,fileSize;  unused variables
// long fileOffset;  unused variable
// char *extFile;    unused variable
boolean hasVersion = hHasField(database, gbCdnaInfoTable, "version");
boolean haveGbSeq = sqlTableExists(conn, gbSeqTable);
char *seqTbl = haveGbSeq ? gbSeqTable : "seq";
char *version = NULL;
struct trackDb *tdbRgdEst;
char *chrom = cartString(cart, "c");
int start = cartInt(cart, "o");
int end = cartUsualInt(cart, "t",0);
struct gbWarn *gbWarn = checkGbWarn(conn, acc);

/* This sort of query and having to keep things in sync between
 * the first clause of the select, the from clause, the where
 * clause, and the results in the row ... is really tedious.
 * One of my main motivations for going to a more object
 * based rather than pure relational approach in general,
 * and writing 'autoSql' to help support this.  However
 * the pure relational approach wins for pure search speed,
 * and these RNA fields are searched.  So it looks like
 * the code below stays.  Be really careful when you modify
 * it.
 *
 * Uses the gbSeq table if available, otherwise use seq for older databases.
 */
sqlDyStringPrintf(dy,
               "select g.type,g.direction,"
               "so.name,o.name,l.name,m.name,"
               "se.name,t.name,dev.name,ce.name,cd.name,"
               "des.name,a.name,gene.name,p.name,"
               "gbS.size,g.moddate,gbS.gbExtFile,gbS.file_offset,gbS.file_size ");

/* If the gbCdnaInfoTAble table has a "version" column then will show it */
if (hasVersion)
    {
    sqlDyStringPrintf(dy,
                   ", g.version ");
    }

sqlDyStringPrintf(dy,
               " from %s g,%s gbS,%s so,%s o,%s l,%s m,%s se,%s t,"
               "%s dev,%s ce,%s cd,%s des,%s a,%s gene,%s p"
               " where g.acc = '%s' and g.id = gbS.id ",
               gbCdnaInfoTable,seqTbl, sourceTable, organismTable, libraryTable, mrnaCloneTable, sexTable, tissueTable, developmentTable, cellTable, cdsTable, descriptionTable, authorTable, geneNameTable, productNameTable,  acc);
sqlDyStringPrintf(dy,
               "and g.source = so.id and g.organism = o.id "
               "and g.library = l.id and g.mrnaClone = m.id "
               "and g.sex = se.id and g.tissue = t.id "
               "and g.development = dev.id and g.cell = ce.id "
               "and g.cds = cd.id and g.description = des.id "
               "and g.author = a.id and g.geneName = gene.id "
               "and g.productName = p.id");

sr = sqlMustGetResult(conn, dy->string);
row = sqlNextRow(sr);
if (row != NULL)
    {
    type=row[0];direction=row[1];
      // source=row[2];  unused variable
    orgFullName=row[3];library=row[4];clone=row[5];
    sex=row[6];tissue=row[7];development=row[8];cell=row[9];cds=row[10];description=row[11];
    author=row[12];geneName=row[13];productName=row[14];
    // seqSize = sqlUnsigned(row[15]);   unused variable
    date = row[16];
    // ext_file = row[17];  unused variable
    // fileOffset=sqlUnsigned(row[18]);  unused variable
    // fileSize=sqlUnsigned(row[19]);    unused variable
    boolean isEst = sameWord(type, "est");

    if (hasVersion)
        {
        version = row[20];
        }


    /* Now we have all the info out of the database and into nicely named
     * local variables.  There's still a few hoops to jump through to
     * format this prettily on the web with hyperlinks to NCBI. */
    printf("<H2>Information on %s <A HREF=\"",  type);
    if (isEst)
	printEntrezEstUrl(stdout, acc);
    else
	printEntrezNucleotideUrl(stdout, acc);
    printf("\" TARGET=_blank>%s</A></H2>\n", acc);

    printf("<B>Description:</B> %s<BR>\n", description);
    if (gbWarn != NULL)
        printGbWarn(acc, gbWarn);

    medlineLinkedLine("Gene", geneName, geneName);
    medlineProductLinkedLine("Product", productName);
    dyStringClear(dy);
    gbToEntrezAuthor(author, dy);
    medlineLinkedLine("Author", author, dy->string);
    printf("<B>Organism:</B> ");
    printf("<A href=\"https://www.ncbi.nlm.nih.gov/Taxonomy/Browser/wwwtax.cgi?mode=Undef&name=%s&lvl=0&srchmode=1\" TARGET=_blank>",
	   cgiEncode(orgFullName));
    printf("%s</A><BR>\n", orgFullName);
    printf("<B>Tissue:</B> %s<BR>\n", tissue);
    printf("<B>Development stage:</B> %s<BR>\n", development);
    printf("<B>Cell line:</B> %s<BR>\n", cell);
    printf("<B>Sex:</B> %s<BR>\n", sex);
    printf("<B>Library:</B> %s<BR>\n", library);
    printf("<B>Clone:</B> %s<BR>\n", clone);
    if (isEst)
        {
        printf("<B>Read direction: </B>");
        if (direction[0] != '0')
            printf("%s' (guessed from GenBank description)<BR>\n", direction);
        else
            printf("unknown (can't guess from GenBank description)<BR>");
        }
    else
        printf("<B>CDS:</B> %s<BR>\n", cds);
    printf("<B>Date:</B> %s<BR>\n", date);
    if (hasVersion)
        {
        printf("<B>Version:</B> %s<BR>\n", version);
        }
    /* print RGD EST Report link if it is Rat genome and it has a link to RGD */
    if (sameWord(organism, "Rat"))
	{
        if (hTableExists(database, "rgdEstLink"))
            {
            sqlSafef(query, sizeof(query),
                     "select id from %s.rgdEstLink where name = '%s';",  database, acc);
            if (sqlQuickQuery(conn2, query, rgdEstId, sizeof(rgdEstId)) != NULL)
                {
                tdbRgdEst = hashFindVal(trackHash, "rgdEst");
                printf("<B>RGD EST Report: ");
                printf("<A HREF=\"%s%s\" target=_blank>", tdbRgdEst->url, rgdEstId);
                printf("RGD:%s</B></A><BR>\n", rgdEstId);
                }
            }
        }
    if (isEst && hTableExists(database, "estOrientInfo") && (psl != NULL))
        {
        int estOrient = getEstTranscriptionDir(conn2, psl);
        if (estOrient != 0)
            printf("<B>EST transcribed from %c strand </B>(supported by %d splice sites).<BR>\n",
                   (estOrient > 0 ? '+' : '-' ), abs(estOrient));
        }
    if (hGenBankHaveSeq(database, acc, NULL))
        {
        printf("<B>%s sequence:</B> ", type);
        hgcAnchorSomewhere("htcDisplayMrna", acc, tdb->track, seqName);
        printf("%s</A><BR>\n", acc);
        }
    }
else
    {
    warn("Couldn't find %s in %s table", gbCdnaInfoTable, acc);
    }
if (end != 0 && differentString(chrom,"0") && isNotEmpty(chrom))
    {
    printf("<B>Position:</B> "
           "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
                  hgTracksPathAndSettings(), database, chrom, start+1, end);
    printf("%s:%d-%d</A><BR>\n", chrom, start+1, end);
    }

gbWarnFree(&gbWarn);
sqlFreeResult(&sr);
dyStringFree(&dy);
hFreeConn(&conn);
hFreeConn(&conn2);
}

static boolean isPslToPrintByClick(struct psl *psl, int startFirst, boolean isClicked)
/* Determine if a psl should be printed based on if it was or was not the one that was clicked
 * on.
 */
{
return ((psl->tStart == startFirst) && sameString(psl->tName, seqName)) == isClicked;
}

void printAlignmentsSimple(struct psl *pslList, int startFirst, char *hgcCommand,
                           char *tableName, char *itemIn)
/* Print list of mRNA alignments, don't add extra textual link when
 * doesn't honor hgcCommand. */
{
struct psl *psl;
int aliCount = slCount(pslList);
boolean isClicked;
if (pslList == NULL || tableName == NULL)
    return;

boolean showEvery = (strstr(itemIn, "PrintAllSequences") > 0);
if (!showEvery && (aliCount > 1))
    printf("The alignment you clicked on is first in the table below.<BR>\n");

printf("<PRE><TT>");
if (startsWith("chr", pslList->tName))
    printf("BROWSER | SIZE IDENTITY CHROMOSOME  STRAND    START     END              QUERY      START  END  TOTAL\n");
else
    printf("BROWSER | SIZE IDENTITY  SCAFFOLD   STRAND    START     END              QUERY      START  END  TOTAL\n");
printf("-----------------------------------------------------------------------------------------------------\n");
for (isClicked = 1; isClicked >= 0; isClicked -= 1)
    {
    for (psl = pslList; psl != NULL; psl = psl->next)
	{
	if (isPslToPrintByClick(psl, startFirst, isClicked))
	    {
            char otherString[512];
            char *qName = itemIn;
	    if (showEvery)
		qName = replaceChars(itemIn, "PrintAllSequences", psl->qName);
	    safef(otherString, sizeof(otherString), "%d&aliTable=%s", psl->tStart, tableName);
            printf("<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">browser</A> | ",
                   hgTracksPathAndSettings(), database, psl->tName, psl->tStart+1, psl->tEnd);
	    if (psl->qSize <= MAX_DISPLAY_QUERY_SEQ_SIZE) // Only anchor if small enough 
		hgcAnchorWindow(hgcCommand, qName, psl->tStart, psl->tEnd, otherString, psl->tName);
            char *displayChromName = chromAliasGetDisplayChrom(database, cart, psl->tName);
	    printf("%5d  %5.1f%%  %9s     %s %9d %9d  %20s %5d %5d %5d",
		   psl->match + psl->misMatch + psl->repMatch,
		   100.0 - pslCalcMilliBad(psl, TRUE) * 0.1,
		   skipChr(displayChromName), psl->strand, psl->tStart + 1, psl->tEnd,
		   psl->qName, psl->qStart+1, psl->qEnd, psl->qSize);
	    if (psl->qSize <= MAX_DISPLAY_QUERY_SEQ_SIZE)
	        printf("</A>");
	    printf("\n");

	    if (showEvery)
                 freeMem(qName);
	    }
	}
    }
printf("</TT></PRE>");
}

void printAlignmentsExtra(struct psl *pslList, int startFirst, char *hgcCommand, char *hgcCommandInWindow,
		     char *tableName, char *itemIn)
/* Print list of mRNA alignments with special "in window" alignment function. */
{
if (pslList == NULL || tableName == NULL)
    return;
printAlignmentsSimple(pslList, startFirst, hgcCommand, tableName, itemIn);

struct psl *psl = pslList;
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    if ( pslTrimToTargetRange(psl, winStart, winEnd) != NULL
        &&
	!startsWith("xeno", tableName)
	&& !(startsWith("user", tableName) && pslIsProtein(psl))
	&& psl->tStart == startFirst
        && sameString(psl->tName, seqName)
	)
	{
        boolean showEvery = (strstr(itemIn, "PrintAllSequences") > 0);
	char *qName = itemIn;
	if (showEvery)
	    qName = replaceChars(itemIn, "PrintAllSequences", psl->qName);

        char otherString[512];
	safef(otherString, sizeof(otherString), "%d&aliTable=%s",
	      psl->tStart, tableName);
	hgcAnchorSomewhere(hgcCommandInWindow, qName, otherString, psl->tName);
	printf("<BR>View details of parts of alignment within browser window</A>.<BR>\n");
	}
    }
}

void printAlignments(struct psl *pslList, int startFirst, char *hgcCommand,
		     char *tableName, char *itemIn)
/* Print list of mRNA alignments. */
{
printAlignmentsExtra(pslList, startFirst, hgcCommand, "htcCdnaAliInWindow", tableName, itemIn);
}

static struct psl *getAlignmentsTName(struct sqlConnection *conn, char *table, char *acc,
                                      char *tName)
/* get the list of alignments for the specified acc and tName (if given). */
{
struct sqlResult *sr = NULL;
char **row;
struct psl *psl, *pslList = NULL;
boolean hasBin;
char splitTable[HDB_MAX_TABLE_STRING];
char query[1024];
if (!hFindSplitTable(database, seqName, table, splitTable, sizeof splitTable, &hasBin))
    errAbort("can't find table %s or %s_%s", table, seqName, table);
if (isNotEmpty(tName))
    sqlSafef(query, sizeof(query), "select * from %s where qName = '%s' and tName = '%s'",
             splitTable, acc, tName);
else
    sqlSafef(query, sizeof(query), "select * from %s where qName = '%s'", splitTable, acc);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row+hasBin);
    slAddHead(&pslList, psl);
    }
sqlFreeResult(&sr);
slReverse(&pslList);
return pslList;
}

struct psl *getAlignments(struct sqlConnection *conn, char *table, char *acc)
/* get the list of alignments for the specified acc */
{
return getAlignmentsTName(conn, table, acc, NULL);
}

struct psl *loadPslRangeT(char *table, char *qName, char *tName, int tStart, int tEnd)
/* Load a list of psls given qName tName tStart tEnd */
{
struct sqlResult *sr = NULL;
char **row;
struct psl *psl = NULL, *pslList = NULL;
boolean hasBin;
char splitTable[HDB_MAX_TABLE_STRING];
char query[256];
struct sqlConnection *conn = hAllocConn(database);

if (!hFindSplitTable(database, seqName, table, splitTable, sizeof splitTable, &hasBin))
    errAbort("loadPslRangeT track %s not found", table);
sqlSafef(query, sizeof(query), "select * from %s where qName = '%s' and tName = '%s' and tEnd > %d and tStart < %d", splitTable, qName, tName, tStart, tEnd);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row+hasBin);
    slAddHead(&pslList, psl);
    }
sqlFreeResult(&sr);
slReverse(&pslList);
hFreeConn(&conn);
return pslList;
}

void doHgRna(struct trackDb *tdb, char *acc)
/* Click on an individual RNA. */
{
char *track = tdb->track;
char *table = tdb->table;
struct sqlConnection *conn = hAllocConn(database);
char *type;
int start = cartInt(cart, "o");
struct psl *pslList = NULL;

if (sameString("xenoMrna", track) || sameString("xenoBestMrna", track) || sameString("xenoEst", track) || sameString("sim4", track) )
    {
    char temp[256];
    safef(temp, sizeof temp, "non-%s RNA", organism);
    type = temp;
    }
else if ( sameWord("blatzHg17KG", track)  )
    {
    type = "Human mRNA";
    }
else if (stringIn("estFiltered",track))
    {
    type = "EST";
    }
else if (stringIn("est", track) || stringIn("Est", track))
    {
    type = "EST";
    //  table = "all_est";	// Should fall out of wash now
    }
else if (startsWith("psu", track))
    {
    type = "Pseudo & Real Genes";
    table = "psu";
    }
else if (sameWord("xenoBlastzMrna", track) )
    {
    type = "Blastz to foreign mRNA";
    }
else if (startsWith("mrnaBlastz",track  ))
    {
    type = "mRNA";
    }
else if (startsWith("pseudoMrna",track) || startsWith("pseudoGeneLink",track))
    {
    type = "mRNA";
    table = "pseudoMrna";
    }
else if (startsWith("celeraMrna",track))
    {
    type = "mRNA";
    }
else if (startsWith("all_mrnaFiltered",track))
    {
    type = "mRNA";
    }
else
    {
    type = "mRNA";
    // table = "all_mrna";  // should fall out of wash now
    }

/* Print non-sequence info. */
cartWebStart(cart, database, "%s", acc);

printRnaSpecs(tdb, acc, pslList);

/* Get alignment info. */
pslList = getAlignments(conn, table, acc);
if (pslList == NULL)
    {
    /* this was not actually a click on an aligned item -- we just
     * want to display RNA info, so leave here */
    hFreeConn(&conn);
    htmlHorizontalLine();
    printf("mRNA %s alignment does not meet minimum alignment criteria on this assembly.", acc);
    return;
    }
htmlHorizontalLine();
printf("<H3>%s/Genomic Alignments</H3>", type);
if (startsWith("mrnaBlastz",tdb->table))
    slSort(&pslList, pslCmpScoreDesc);

printAlignments(pslList, start, "htcCdnaAli", table, acc);

printTrackHtml(tdb);
hFreeConn(&conn);
}

void printPslFormat(struct sqlConnection *conn, struct trackDb *tdb, char *item, int start,
                    char *subType)
/* Handles click in affyU95 or affyU133 tracks */
{
struct psl  *pslList = getAlignments(conn, tdb->table, item);
struct psl *psl;
char *face = "Times"; /* specifies font face to use */
char *fsize = "+1"; /* specifies font size */

/* check if there is an alignment available for this sequence.  This checks
 * both genbank sequences and other sequences in the seq table.  If so,
 * set it up so they can click through to the alignment. */
if (hGenBankHaveSeq(database, item, NULL))
    {
    printf("<H3>%s/Genomic Alignments</H3>", item);
    printAlignments(pslList, start, "htcCdnaAli", tdb->table, item);
    }
else
    {
    /* print out the psls */
    printf("<PRE><TT>");
    printf("<span style='font-family:%s; font-size:%s;'>\n", face, fsize);

    for (psl = pslList;  psl != NULL; psl = psl->next)
       {
       pslOutFormat(psl, stdout, '\n', '\n');
       }
    printf("</span></TT></PRE>\n");
    }
pslFreeList(&pslList);
}

void doAffy(struct trackDb *tdb, char *item, char *itemForUrl)
/* Display information for Affy tracks*/

{
char *dupe, *type, *words[16];
char *orthoTable = trackDbSetting(tdb, "orthoTable");
char *otherDb = trackDbSetting(tdb, "otherDb");
int wordCount;
int start = cartInt(cart, "o");
char query[256];
char **row;
struct sqlResult *sr = NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlConnection *conn2 = hAllocConn(database);

if (itemForUrl == NULL)
    itemForUrl = item;
dupe = cloneString(tdb->type);
genericHeader(tdb, item);
wordCount = chopLine(dupe, words);
printCustomUrl(tdb, itemForUrl, item == itemForUrl);

/* If this is the affyZebrafish track, check for human ortholog information */
if (sameString("affyZebrafish", tdb->table))
    {
    if (orthoTable != NULL && hTableExists(database, orthoTable))
        {
        sqlSafef(query, sizeof(query), "select geneSymbol, description from %s where name = '%s' ", orthoTable, item);
        sr = sqlMustGetResult(conn, query);
        row = sqlNextRow(sr);
        if (row != NULL)
            {
            printf("<P><HR ALIGN=\"CENTER\"></P>\n<TABLE>\n");
            printf("<TR><TH ALIGN=left><H2>Human %s Ortholog:</H2></TH><TD>%s</TD></TR>\n", otherDb, row[0]);
            printf("<TR><TH ALIGN=left>Ortholog Description:</TH><TD>%s</TD></TR>\n",row[1]);
            printf("</TABLE>\n");
            }
        }
    }
if (wordCount > 0)
    {
    type = words[0];

    if (sameString(type, "psl"))
        {
	char *subType = ".";
	if (wordCount > 1)
	    subType = words[1];
        printPslFormat(conn2, tdb, item, start, subType);
	}
    }
printTrackHtml(tdb);
freez(&dupe);
hFreeConn(&conn);
hFreeConn(&conn2);
}

void doZfishRHmap(struct trackDb *tdb, char *itemName)
/* Put up Radiation Hybrid map information for Zebrafish */
{
char *dupe, *type, *words[16];
char query[256];
struct sqlResult *sr = NULL;
char **row = NULL;
struct rhMapZfishInfo *rhInfo = NULL;
int wordCount;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);
struct sqlConnection *conn1 = hAllocConn(database);
boolean rhMapInfoExists = sqlTableExists(conn, "rhMapZfishInfo");

dupe = cloneString(tdb->type);
wordCount = chopLine(dupe, words);

genericHeader(tdb, itemName);

/* Print out RH map information if available */

if (rhMapInfoExists)
    {
    sqlSafef(query, sizeof query, "SELECT * FROM rhMapZfishInfo WHERE name = '%s'", itemName);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
        rhInfo = rhMapZfishInfoLoad(row);
        if (rhInfo != NULL)
            {
            printf("<H2>Information on %s </H2>\n", itemName);
            if (!sameString(rhInfo->zfinId, ""))
                {
                printf("<H3>");
                printCustomUrl(tdb, rhInfo->zfinId, TRUE);
                printf("</H3>\n");
                }
            printf("<P><HR ALIGN=\"CENTER\"></P>\n<TABLE>\n");
            printf("<TR><TH ALIGN=left>Linkage group:</TH><TD>%s</TD></TR>\n", rhInfo->linkageGp);
            printf("<TR><TH ALIGN=left>Position on linkage group:</TH><TD>%d</TD></TR>\n", rhInfo->position);
            printf("<TR><TH ALIGN=left>Distance (cR):</TH><TD>%d</TD></TR>\n", rhInfo->distance);
            printf("<TR><TH ALIGN=left>Marker type:</TH><TD>%s</TD></TR>\n", rhInfo->markerType);
            printf("<TR><TH ALIGN=left>Marker source:</TH><TD>%s</TD></TR>\n", rhInfo->source);
            printf("<TR><TH ALIGN=left>Mapping institution:</TH><TD>%s</TD></TR>\n", rhInfo->mapSite);
            printf("<TR><TH ALIGN=left>Forward Primer:</TH><TD>%s</TD></TR>\n", rhInfo->leftPrimer);
            printf("<TR><TH ALIGN=left>Reverse Primer:</TH><TD>%s</TD></TR>\n", rhInfo->rightPrimer);
            printf("</TABLE>\n");
            }
        }
    }

dupe = cloneString(tdb->type);
wordCount = chopLine(dupe, words);
if (wordCount > 0)
    {
    type = words[0];

    if (sameString(type, "psl"))
        {
	char *subType = ".";
	if (wordCount > 1)
	    subType = words[1];
        printPslFormat(conn1, tdb, itemName, start, subType);
	}
    }
printTrackHtml(tdb);
freez(&dupe);
hFreeConn(&conn);
hFreeConn(&conn1);
}

void doRikenRna(struct trackDb *tdb, char *item)
/* Put up Riken RNA stuff. */
{
char query[512];
struct sqlResult *sr;
char **row;
struct sqlConnection *conn = sqlConnect("mgsc");

genericHeader(tdb, item);
sqlSafef(query, sizeof query, "select * from rikenMrna where qName = '%s'", item);
sr = sqlGetResult(conn, query);
printf("<PRE><TT>\n");
printf("#match\tmisMatches\trepMatches\tnCount\tqNumInsert\tqBaseInsert\ttNumInsert\tBaseInsert\tstrand\tqName\tqSize\tqStart\tqEnd\ttName\ttSize\ttStart\ttEnd\tblockCount\tblockSizes\tqStarts\ttStarts\n");
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct psl *psl = pslLoad(row+1);
    pslTabOut(psl, stdout);
    }
printf("</TT></PRE>\n");
sqlDisconnect(&conn);

printTrackHtml(tdb);
}

void doYaleTars(struct trackDb *tdb, char *item, char *itemForUrl)
/* Display information for Affy tracks*/

{
char *dupe, *type, *words[16], *chrom = NULL, *strand = NULL;
char *item2 = NULL;
int wordCount, end = 0;
int start = cartInt(cart, "o");
char query[256];
char **row;
struct sqlResult *sr = NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlConnection *conn2 = hAllocConn(database);

if (itemForUrl == NULL)
    {
    if (startsWith("TAR", item))
        {
        /* Remove TAR prefix from item */
        item2 = strchr(item, 'R');
        item2++;
        itemForUrl = item2;
        }
     else
        itemForUrl = item;
     }
dupe = cloneString(tdb->type);
genericHeader(tdb, item);
wordCount = chopLine(dupe, words);
printCustomUrl(tdb, itemForUrl, item == itemForUrl);

sqlSafef(query, sizeof(query), "select tName, tEnd, strand from %s where qName='%s' and tStart=%d;", tdb->table, item, start);

sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);

/* load PSL into struct */
if (row != NULL)
    {
    chrom = cloneString(row[0]);
    end = sqlUnsigned(row[1]);
    strand = cloneString(row[2]);
    }
printPos(chrom, start, end, strand, TRUE, item);
if (wordCount > 0)
    {
    type = words[0];

    if (sameString(type, "psl"))
        {
	char *subType = ".";
	if (wordCount > 1)
	    subType = words[1];
        printPslFormat(conn2, tdb, item, start, subType);
	}
    }
printTrackHtml(tdb);
freez(&dupe);
hFreeConn(&conn);
hFreeConn(&conn2);
}

void printPcrTargetMatch(struct targetDb *target, struct psl *tpsl,
			 boolean mustGetItem)
/* Show the non-genomic target PCR result and its genomic mapping. */
{
char *acc = pcrResultItemAccession(tpsl->tName);
char *name = pcrResultItemName(tpsl->tName);
char niceName[256];
if (name == NULL || sameString(acc, name))
    safecpy(niceName, sizeof(niceName), acc);
else
    safef(niceName, sizeof(niceName), "%s (%s)", name, acc);
printf("<B>Position in %s:</B> <A HREF=\"%s?%s&db=%s&position=%s\">%s</A>"
       ":%d-%d<BR>\n",
       target->description, hgTracksName(), cartSidUrlString(cart), database,
       acc, niceName, tpsl->tStart+1, tpsl->tEnd);
printf("<B>Size in %s:</B> %d<BR>\n", niceName,
       tpsl->tEnd - tpsl->tStart);
if (tpsl->strand[0] == '-')
    printf("&nbsp;&nbsp;"
	   "<EM>Warning: the match is on the reverse strand of %s</EM><BR>\n",
	   niceName);

struct psl *itemPsl = NULL, *otherPsls = NULL, *gpsl;
int itemStart = cartInt(cart, "o");
int itemEnd = cartInt(cart, "t");
int rowOffset = hOffsetPastBin(database, seqName, target->pslTable);
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[2048];
sqlSafef(query, sizeof(query), "select * from %s where qName = '%s'",
      target->pslTable, acc);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gpsl = pslLoad(row+rowOffset);
    struct psl *pslTrimmed = pslTrimToQueryRange(gpsl, tpsl->tStart,
						 tpsl->tEnd);
    if (sameString(gpsl->tName, seqName) &&
	((gpsl->tStart == itemStart && gpsl->tEnd == itemEnd) ||
	 (pslTrimmed->tStart == itemStart && pslTrimmed->tEnd == itemEnd)))
	itemPsl = pslTrimmed;
    else
	slAddHead(&otherPsls, pslTrimmed);
    pslFree(&gpsl);
    }
hFreeConn(&conn);
if (mustGetItem && itemPsl == NULL)
    errAbort("Did not find record for amplicon in %s at %s:%d-%d",
	     niceName, seqName, itemStart, itemEnd);
char strand[2];
strand[1] = '\0';
if (itemPsl != NULL)
    {
    if (itemPsl->strand[1] == '\0')
	strand[0] = itemPsl->strand[0];
    else if (itemPsl->strand[0] != itemPsl->strand[1])
	strand[0] = '-';
    else
	strand[0] = '+';
    if (itemPsl != NULL)
	printPosOnChrom(itemPsl->tName, itemPsl->tStart, itemPsl->tEnd,
			strand, FALSE, tpsl->tName);
    }
slSort(&otherPsls, pslCmpTarget);
if (itemPsl != NULL && otherPsls != NULL)
    printf("<B>Other matches in genomic alignments of %s:</B><BR>\n",
	   niceName);
for (gpsl = otherPsls;  gpsl != NULL;  gpsl = gpsl->next)
    {
    if (gpsl->strand[1] == '\0')
	strand[0] = gpsl->strand[0];
    else if (gpsl->strand[0] != gpsl->strand[1])
	strand[0] = '-';
    else
	strand[0] = '+';
    printPosOnChrom(gpsl->tName, gpsl->tStart, gpsl->tEnd, strand, FALSE,
		    tpsl->tName);
    }
pslFree(&itemPsl);
pslFreeList(&otherPsls);
}

static void upperMatch(char *dna, char *primer, int size)
/* Uppercase DNA where it matches primer. -- copied from gfPcrLib.c. */
{
int i;
for (i=0; i<size; ++i)
    {
    if (dna[i] == primer[i])
        dna[i] = toupper(dna[i]);
    }
}

void printPcrSequence(char *item, struct targetDb *target, struct psl *psl,
		      char *fPrimer, char *rPrimer)
/* Print the amplicon sequence (as on hgPcr results page). */
{
int productSize = psl->tEnd - psl->tStart;
char *ffPrimer = cloneString(fPrimer);
char *rrPrimer = cloneString(rPrimer);
int rPrimerSize = strlen(rPrimer);
struct dnaSeq *seq;
if (stringIn("__", item) && target != NULL)
    {
    /* Use seq+extFile if specified; otherwise just retrieve from seqFile. */
    if (isNotEmpty(target->seqTable) && isNotEmpty(target->extFileTable))
	{
	struct sqlConnection *conn = hAllocConn(database);
	seq = hDnaSeqGet(database, psl->tName, target->seqTable,
			 target->extFileTable);
	hFreeConn(&conn);
	char *dna = cloneStringZ(seq->dna + psl->tStart, productSize);
	freeMem(seq->dna);
	seq->dna = dna;
	}
    else
	{
	struct twoBitFile *tbf = twoBitOpen(target->seqFile);
	seq = twoBitReadSeqFrag(tbf, psl->tName, psl->tStart,
				psl->tEnd);
	twoBitClose(&tbf);
	}
    }
else
    seq = hChromSeq(database, psl->tName, psl->tStart, psl->tEnd);
char *dna = seq->dna;
tolowers(dna);
if (psl->strand[0] == '-')
    reverseComplement(dna, productSize);
printf("<TT><PRE>");
/* The rest of this is loosely copied from gfPcrLib.c:outputFa(): */
char *tNameForPos = (target != NULL ? pcrResultItemAccession(psl->tName) : psl->tName);
printf("><A HREF=\"%s?%s&db=%s&position=%s",
       hgTracksName(), cartSidUrlString(cart), database, tNameForPos);
if (target == NULL)
    printf(":%d-%d", psl->tStart+1, psl->tEnd);
printf("\">%s:%d%c%d</A> %dbp %s %s\n",
       psl->tName, psl->tStart+1, psl->strand[0], psl->tEnd,
       productSize, fPrimer, rPrimer);

/* Flip reverse primer to be in same direction and case as sequence, to
 * compare with sequence: */
reverseComplement(rrPrimer, rPrimerSize);
tolowers(rrPrimer);
tolowers(ffPrimer);

/* Capitalize where sequence and primer match, and write out sequence. */
upperMatch(dna, ffPrimer, strlen(ffPrimer));
upperMatch(dna + productSize - rPrimerSize, rrPrimer, rPrimerSize);
faWriteNext(stdout, NULL, dna, productSize);
printf("</PRE></TT>");
}

static void pslFileGetPrimers(char *pslFileName, char *transcript, int ampStart, int ampEnd,
        char **retFPrimer, char **retRPrimer)
/* Use a psl file to get primers associated with the transcript */
{
char *pslFields[21];
struct lineFile *lf = lineFileOpen(pslFileName, TRUE);
char *target = cloneString(transcript);
char *pipe = strchr(target, '|');
*pipe = '\0';
while (lineFileRow(lf, pslFields))
    {
    struct psl *psl = pslLoad(pslFields);
    if (sameString(psl->tName, target) && ampStart == psl->tStart && ampEnd == psl->tEnd)
        {
        char *pair = psl->qName;
        char *under = strchr(pair, '_');
        *under = '\0';
        *retFPrimer = cloneString(pair);
        *retRPrimer = cloneString(under+1);
        break;
        }
    }
lineFileClose(&lf);
}

void doPcrResult(char *track, char *item)
/* Process click on PCR of user's primers. */
{
struct trackDb *tdb = pcrResultFakeTdb();
char *pslFileName, *primerFileName;
struct targetDb *target;
cartWebStart(cart, database, "PCR Results");
if (! pcrResultParseCart(database, cart, &pslFileName, &primerFileName, &target))
    errAbort("PCR Result track has disappeared!");

char *fPrimer = NULL, *rPrimer = NULL;
int ampStart, ampEnd;
char *targetSeqName = NULL;
boolean targetSearch = stringIn("__", item) != NULL;
if (targetSearch)
    {
    /* item (from hgTracks) is |-separated: target sequence name,
     * amplicon start offset in target sequence, and amplicon end offset. */
    char *words[3];
    int wordCount = chopByChar(cloneString(item), '|', words, ArraySize(words));
    if (wordCount != 3)
	errAbort("doPcrResult: expected 3 |-sep'd words but got '%s'", item);
    targetSeqName = words[0];
    if (endsWith(targetSeqName, "__"))
	targetSeqName[strlen(targetSeqName)-2] = '\0';
    ampStart = atoi(words[1]), ampEnd = atoi(words[2]);
    // use the psl file to find the right primer pair
    pslFileGetPrimers(pslFileName, item, ampStart, ampEnd, &fPrimer, &rPrimer);
    }
else if (stringIn("_", item))
    {
    // the item name contains the forward and reverse primers
    int maxSplits = 2;
    char *splitQName[maxSplits];
    int numSplits = chopString(cloneString(item), "_", splitQName, sizeof(splitQName));
    if (numSplits == maxSplits)
        {
        fPrimer = splitQName[0];
        touppers(fPrimer);
        rPrimer = splitQName[1];
        touppers(rPrimer);
        }
    }
else
    {
    pcrResultGetPrimers(primerFileName, &fPrimer, &rPrimer, NULL);
    }
printf("<H2>PCR Results (<TT>%s %s</TT>)</H2>\n", fPrimer, rPrimer);
printf("<B>Forward primer:</B> 5' <TT>%s</TT> 3'<BR>\n", fPrimer);
printf("<B>Reverse primer:</B> 5' <TT>%s</TT> 3'<BR>\n", rPrimer);
if (targetSearch)
    printf("<B>Search target:</B> %s<BR>\n", target->description);

struct psl *itemPsl = NULL, *otherPsls = NULL, *psl;
if (targetSearch)
    {
    pcrResultGetPsl(pslFileName, target, targetSeqName, seqName, ampStart, ampEnd,
		    &itemPsl, &otherPsls, fPrimer, rPrimer);
    printPcrTargetMatch(target, itemPsl, TRUE);
    }
else
    {
    pcrResultGetPsl(pslFileName, NULL, item,
		    seqName, cartInt(cart, "o"), cartInt(cart, "t"),
		    &itemPsl, &otherPsls, fPrimer, rPrimer);
    printPosOnChrom(itemPsl->tName, itemPsl->tStart, itemPsl->tEnd,
		    itemPsl->strand, FALSE, NULL);
    }

if (otherPsls != NULL)
    {
    puts("<HR>");
    printf("<B>Other matches for these primers:</B><BR>\n");
    for (psl = otherPsls;  psl != NULL;  psl = psl->next)
	{
	puts("<BR>");
	if (stringIn("__", psl->tName))
	    printPcrTargetMatch(target, psl, FALSE);
	else
	    printPosOnChrom(psl->tName, psl->tStart, psl->tEnd,
			    psl->strand, FALSE, NULL);
	}
    puts("<HR>");
    }
printPcrSequence(item, target, itemPsl, fPrimer, rPrimer);

puts("<BR><HR>");
printTrackHtml(tdb);
}

void doUserPsl(char *track, char *item)
/* Process click on user-defined alignment. */
{
int start = cartInt(cart, "o");
struct lineFile *lf;
struct psl *pslList = NULL, *psl;
char *pslName, *faName, *qName;
enum gfType qt, tt;
boolean isProt;

cartWebStart(cart, database, "BLAT Search Alignments");

printf("<H2>BLAT Search Alignments</H2>\n");
printf("<A href id='hgUsualPslShowAllLink'>Show All</A>\n");
printf("<H3>Click on a line to see detailed letter-by-letter display</H3>\n");

printf("<div id=hgUsualPslShowItem style='display: block'>\n");
parseSs(item, &pslName, &faName, &qName);
pslxFileOpen(pslName, &qt, &tt, &lf);
while ((psl = pslNext(lf)) != NULL)
    {
    if (sameString(psl->qName, qName))
	{
	slAddHead(&pslList, psl);
	}
    else
	{
	pslFree(&psl);
	}
    }
slSort(&pslList, pslCmpQueryScore);
lineFileClose(&lf);
printAlignments(pslList, start, "htcUserAli", "user", item);
pslFreeList(&pslList);
printf("</div>\n");


printf("<div id=hgUsualPslShowAll style='display: none'>\n");
// get hidden rest of alignments.
pslxFileOpen(pslName, &qt, &tt, &lf);
isProt = (qt == gftProt);
while ((psl = pslNext(lf)) != NULL)
    {
    slAddHead(&pslList, psl);
    }
slSort(&pslList, pslCmpQueryScore);
lineFileClose(&lf);

char allItems[1024];
safef(allItems, sizeof allItems, "%s %s %s", pslName, faName, "PrintAllSequences");

printAlignments(pslList, start, "htcUserAli", "user", allItems);
pslFreeList(&pslList);

printf("<BR>\n");
printf("Input Sequences:<BR>\n");
printf("<textarea rows='8' cols='60' readonly>\n");
struct dnaSeq *oSeq, *oSeqList = faReadAllSeq(faName, !isProt);
for (oSeq = oSeqList; oSeq != NULL; oSeq = oSeq->next)
    {
    printf(">%s\n",oSeq->name);
    printf("%s\n",oSeq->dna);
    printf("\n");
    }
printf("</textarea>\n");

printf("</div>\n");

jsInline("$('#hgUsualPslShowAllLink').click(function(){\n"
	"  $('#hgUsualPslShowItem')[0].style.display = 'none';\n"
	"  $('#hgUsualPslShowAll')[0].style.display = 'block';\n"
	"  $('#hgUsualPslShowAllLink')[0].style.display = 'none';\n"
	"return false;\n"
	"});\n");

webIncludeHelpFile(USER_PSL_TRACK_NAME, TRUE);
}

void doHgGold(struct trackDb *tdb, char *fragName)
/* Click on a fragment of golden path. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlConnection *conn2 = hAllocConn(database);
struct sqlConnection *conn3 = hAllocConn(database);
char query[256];
struct sqlResult *sr;
char **row;
char query2[256];
struct sqlResult *sr2;
char **row2;
char query3[256];
struct sqlResult *sr3;
char **row3;
struct agpFrag frag;
struct contigAcc contigAcc;
int start = cartInt(cart, "o");
boolean hasBin;
char splitTable[HDB_MAX_TABLE_STRING];
char *chp;
char *accession1, *accession2, *spanner, *variation, *varEvidence,
    *contact, *remark, *comment;
// char *evaluation;  unused variable
char *secondAcc, *secondAccVer;
char *tmpString;
int first;

cartWebStart(cart, database, "%s", fragName);
if (!hFindSplitTable(database, seqName, tdb->table, splitTable, sizeof splitTable, &hasBin))
    errAbort("doHgGold track %s not found", tdb->table);
sqlSafef(query, sizeof query, "select * from %s where frag = '%s' and chromStart = %d",
	splitTable, fragName, start);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
agpFragStaticLoad(row+hasBin, &frag);

printf("<B>Entrez nucleotide:</B><A TARGET=_blank HREF='https://www.ncbi.nlm.nih.gov/nuccore/%s'> %s</A><BR>\n", fragName, fragName);
printf("<B>Clone Fragment ID:</B> %s<BR>\n", frag.frag);
printf("<B>Clone Fragment Type:</B> %s<BR>\n", frag.type);
printf("<B>Clone Bases:</B> %d-%d<BR>\n", frag.fragStart+1, frag.fragEnd);

if (hTableExists(database, "contigAcc"))
    {
    sqlSafef(query2, sizeof query2, "select * from contigAcc where contig = '%s'", frag.frag);
    if ((sr2 = sqlGetResult(conn2, query2)))
        {
        row = sqlNextRow(sr2);
        if (row)
            {
            contigAccStaticLoad(row, &contigAcc);
            printf("<B>Genbank Accession: <A HREF=");
            printEntrezNucleotideUrl(stdout, contigAcc.acc);
            printf(" TARGET=_BLANK>%s</A></B><BR>\n", contigAcc.acc);
            }
        sqlFreeResult(&sr2);
        }
    }

printPos(frag.chrom, frag.chromStart, frag.chromEnd, frag.strand, FALSE, NULL);

if (hTableExists(database, "certificate"))
    {
    first = 1;
    again:
    tmpString = cloneString(frag.frag);
    chp = strstr(tmpString, ".");
    if (chp != NULL) *chp = '\0';

    if (first)
	{
        sqlSafef(query2, sizeof query2, "select * from certificate where accession1='%s';", tmpString);
	}
    else
	{
        sqlSafef(query2, sizeof query2, "select * from certificate where accession2='%s';", tmpString);
	}
    sr2 = sqlMustGetResult(conn2, query2);
    row2 = sqlNextRow(sr2);
    while (row2 != NULL)
        {
        printf("<HR>");
        accession1      = row2[0];
        accession2      = row2[1];
        spanner         = row2[2];
        // evaluation      = row2[3];  unused variable
        variation       = row2[4];
        varEvidence     = row2[5];
        contact         = row2[6];
        remark          = row2[7];
        comment         = row2[8];

        if (first)
            {
	    secondAcc = accession2;
	    }
	else
	    {
	    secondAcc = accession1;
            }

        sqlSafef(query3, sizeof query3, "select frag from %s where frag like '%s.%c';",
                splitTable, secondAcc, '%');
        sr3 = sqlMustGetResult(conn3, query3);
        row3 = sqlNextRow(sr3);
        if (row3 != NULL)
	    {
            secondAccVer = row3[0];
	    }
	else
	    {
	    secondAccVer = secondAcc;
	    }

	printf("<H3>Non-standard Join Certificate: </H3>\n");

	printf("The join between %s and %s is not standard due to a ", frag.frag, secondAccVer);
	printf("sub-optimal sequence alignment between the overlapping regions of the ");
	printf("clones.  The following details are provided by the ");
	printf("sequencing center to support the joining of these two clones:<BR><BR>");

	printf("<B>Joined with Fragment: </B> %s<BR>\n", secondAccVer);

	if (strcmp(spanner, "") != 0) printf("<B>Spanner: </B> %s<BR>\n", spanner);
	/* if (strcmp(evaluation, "") != 0) printf("<B>Evaluation: </B> %s<BR>\n", evaluation); */
	if (strcmp(variation, "") != 0) printf("<B>Variation: </B> %s<BR>\n", variation);
	if (strcmp(varEvidence, "")!= 0) printf("<B>Variation Evidence: </B> %s<BR>\n", varEvidence);
	if (strcmp(remark, "") != 0) printf("<B>Remark: </B> %s<BR>\n", remark);
	if (strcmp(comment, "") != 0) printf("<B>Comment: </B> %s<BR>\n", comment);
        if (strcmp(contact, "") != 0)
	    printf("<B>Contact: </B> <A HREF=\"mailto:%s\">%s</A><BR>", contact, contact);

	sqlFreeResult(&sr3);
	row2 = sqlNextRow(sr2);
	}
    sqlFreeResult(&sr2);

    if (first)
	{
	first = 0;
	goto again;
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
hFreeConn(&conn2);
hFreeConn(&conn3);
printTrackHtml(tdb);
}

void doHgGap(struct trackDb *tdb, char *gapType)
/* Print a teeny bit of info about a gap. */
{
struct sqlConnection *conn = hAllocConn(database);
char query[256];
struct sqlResult *sr;
char **row;
struct agpGap gap;
int start = cartInt(cart, "o");
boolean hasBin;
char splitTable[HDB_MAX_TABLE_STRING];

cartWebStart(cart, database, "Gap in Sequence");
if (!hFindSplitTable(database, seqName, tdb->table, splitTable, sizeof splitTable, &hasBin))
    errAbort("doHgGap track %s not found", tdb->table);
if (sameString(tdb->table, splitTable))
    sqlSafef(query, sizeof(query), "select * from %s where chrom = '%s' and "
          "chromStart = %d",
	  splitTable, seqName, start);
else
    sqlSafef(query, sizeof(query), "select * from %s where chromStart = %d",
	  splitTable, start);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL)
    errAbort("Couldn't find gap at %s:%d", seqName, start);
agpGapStaticLoad(row+hasBin, &gap);

printf("<B>Gap Type:</B> %s<BR>\n", gap.type);
printf("<B>Bridged:</B> %s<BR>\n", gap.bridge);
printPos(gap.chrom, gap.chromStart, gap.chromEnd, NULL, FALSE, NULL);
printTrackHtml(tdb);

sqlFreeResult(&sr);
hFreeConn(&conn);
}


void selectOneRow(struct sqlConnection *conn, char *table, char *query,
		  struct sqlResult **retSr, char ***retRow)
/* Do query and return one row offset by bin as needed. */
{
char fullTable[HDB_MAX_TABLE_STRING];
boolean hasBin;
char **row;
if (!hFindSplitTable(database, seqName, table, fullTable, sizeof fullTable, &hasBin))
    errAbort("Table %s doesn't exist in database", table);
*retSr = sqlGetResult(conn, query);
if ((row = sqlNextRow(*retSr)) == NULL)
    errAbort("No match to query '%s'", query);
*retRow = row + hasBin;
}


void doHgContig(struct trackDb *tdb, char *ctgName)
/* Click on a contig. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlConnection *conn2 = hAllocConn(database);
char query[256], query2[256], ctgUrl[256];
struct sqlResult *sr, *sr2;
char **row;
struct ctgPos *ctg;
struct ctgPos2 *ctg2 = NULL;
int cloneCount;
struct contigAcc contigAcc;

char *ncbiTerm = cgiEncode(ctgName);
safef(ctgUrl, sizeof(ctgUrl), "%s%s", NUCCORE_SEARCH, ncbiTerm);

genericHeader(tdb, ctgName);
char *url = tdb->url;
if (sameWord(database,"oryCun2"))
    printf("<B>Name:</B>&nbsp;%s<BR>\n", ctgName);
else if (isNotEmpty(url))
    {
    if (sameWord(url, "none"))
	printf("<B>Name:</B>&nbsp;%s<BR>\n", ctgName);
    else
	printCustomUrl(tdb, ctgName, TRUE);
    }
else
    printf("<B>Name:</B>&nbsp;<A HREF=\"%s\" TARGET=_blank>%s</A><BR>\n",
	ctgUrl, ctgName);
freeMem(ncbiTerm);
sqlSafef(query, sizeof(query), "select * from %s where contig = '%s'",
	tdb->table, ctgName);
selectOneRow(conn, tdb->table, query, &sr, &row);

if (sameString("ctgPos2", tdb->table))
    {
    ctg2 = ctgPos2Load(row);
    printf("<B>Type:</B> %s<BR>\n", ctg2->type);
    ctg = (struct ctgPos*)ctg2;
    }
else
    ctg = ctgPosLoad(row);

sqlFreeResult(&sr);

if (hTableExists(database, "contigAcc"))
    {
    sqlSafef(query2, sizeof query2, "select * from contigAcc where contig = '%s'", ctgName);
    if ((sr2 = sqlGetResult(conn2, query2)))
        {
        row = sqlNextRow(sr2);
        if (row)
            {
            contigAccStaticLoad(row, &contigAcc);
            printf("<B>Genbank Accession: <A HREF=");
            printEntrezNucleotideUrl(stdout, contigAcc.acc);
            printf(" TARGET=_BLANK>%s</A></B><BR>\n", contigAcc.acc);
            }
        sqlFreeResult(&sr2);
        }
    }

if (hTableExists(database, "clonePos"))
    {
    sqlSafef(query, sizeof query, "select count(*) from clonePos"
                   " where chrom = '%s' and chromEnd >= %d and chromStart <= %d",
            ctg->chrom, ctg->chromStart, ctg->chromEnd);
    cloneCount = sqlQuickNum(conn, query);
    printf("<B>Total Clones:</B> %d<BR>\n", cloneCount);
    }
printPos(ctg->chrom, ctg->chromStart, ctg->chromEnd, NULL, TRUE, ctg->contig);
printTrackHtml(tdb);

hFreeConn(&conn);
hFreeConn(&conn2);
}

char *cloneStageName(char *stage)
/* Expand P/D/F. */
{
switch (stage[0])
    {
    case 'P':
	return "predraft (less than 4x coverage shotgun)";
    case 'D':
	return "draft (at least 4x coverage shotgun)";
    case 'F':
	return "finished";
    default:
	return "unknown";
    }
}

void doHgCover(struct trackDb *tdb, char *cloneName)
/* Respond to click on clone. */
{
struct sqlConnection *conn = hAllocConn(database);
char query[256];
struct sqlResult *sr;
char **row;
struct clonePos *clone;
int fragCount;

cartWebStart(cart, database, "%s", cloneName);
sqlSafef(query, sizeof query, "select * from %s where name = '%s'", tdb->table, cloneName);
selectOneRow(conn, tdb->table, query, &sr, &row);
clone = clonePosLoad(row);
sqlFreeResult(&sr);

sqlSafef(query, sizeof query,
        "select count(*) from %s_gl where end >= %d and start <= %d and frag like '%s%%'",
        clone->chrom, clone->chromStart, clone->chromEnd, clone->name);
fragCount = sqlQuickNum(conn, query);

printf("<H2>Information on <A HREF=\"");
printEntrezNucleotideUrl(stdout, cloneName);
printf("\" TARGET=_blank>%s</A></H2>\n", cloneName);
printf("<B>GenBank: <A HREF=\"");
printEntrezNucleotideUrl(stdout, cloneName);
printf("\" TARGET=_blank>%s</A></B> <BR>\n", cloneName);
printf("<B>Status:</B> %s<BR>\n", cloneStageName(clone->stage));
printf("<B>Fragments:</B> %d<BR>\n", fragCount);
printf("<B>Size:</B> %d bases<BR>\n", clone->seqSize);
printf("<B>Chromosome:</B> %s<BR>\n", skipChr(clone->chrom));
printf("<BR>\n");

hFreeConn(&conn);
printTrackHtml(tdb);
}

void doHgClone(struct trackDb *tdb, char *fragName)
/* Handle click on a clone. */
{
char cloneName[128];
fragToCloneVerName(fragName, cloneName);
doHgCover(tdb, cloneName);
}

void doBactigPos(struct trackDb *tdb, char *bactigName)
/* Click on a bactig. */
{
struct bactigPos *bactig;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
char goldTable[16];
char ctgStartStr[16];
int ctgStart;

genericHeader(tdb, bactigName);
sqlSafef(query, sizeof query, "select * from %s where name = '%s'", tdb->table, bactigName);
selectOneRow(conn, tdb->table, query, &sr, &row);
bactig = bactigPosLoad(row);
sqlFreeResult(&sr);
printf("<B>Name:</B> %s<BR>\n", bactigName);

snprintf(goldTable, sizeof(goldTable), "%s_gold", seqName);

puts("<B>First contig:</B>");
if (hTableExists(database, goldTable))
    {
    sqlSafef(query, sizeof(query),
	     "select chromStart from %s where frag = \"%s\"",
	     goldTable, bactig->startContig);
    ctgStart = sqlQuickNum(conn, query);
    snprintf(ctgStartStr, sizeof(ctgStartStr), "%d", ctgStart);
    hgcAnchor("gold", bactig->startContig, ctgStartStr);
    }
printf("%s</A><BR>\n", bactig->startContig);

puts("<B>Last contig:</B>");
if (hTableExists(database, goldTable))
    {
    sqlSafef(query, sizeof(query),
	     "select chromStart from %s where frag = \"%s\"",
	     goldTable, bactig->endContig);
    ctgStart = sqlQuickNum(conn, query);
    snprintf(ctgStartStr, sizeof(ctgStartStr), "%d", ctgStart);
    hgcAnchor("gold", bactig->endContig, ctgStartStr);
    }
printf("%s</A><BR>\n", bactig->endContig);

printPos(bactig->chrom, bactig->chromStart, bactig->chromEnd, NULL, FALSE,NULL);
printTrackHtml(tdb);

hFreeConn(&conn);
}


int showGfAlignment(struct psl *psl, bioSeq *qSeq, FILE *f,
		    enum gfType qType, int qStart, int qEnd, char *qName)
/* Show protein/DNA alignment or translated DNA alignment. */
{
int blockCount;
int tStart = psl->tStart;
int tEnd = psl->tEnd;
char tName[256];
struct dnaSeq *tSeq;

/* protein psl's have a tEnd that isn't quite right */
if ((psl->strand[1] == '+') && (qType == gftProt))
    tEnd = psl->tStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1] * 3;

tSeq = hDnaFromSeq(database, seqName, tStart, tEnd, dnaLower);

freez(&tSeq->name);
tSeq->name = cloneString(psl->tName);
safef(tName, sizeof(tName), "%s.%s", organism, psl->tName);
if (qName == NULL)
    fprintf(f, "<H2>Alignment of %s and %s:%d-%d</H2>\n",
	    psl->qName, psl->tName, psl->tStart+1, psl->tEnd);
else
    fprintf(f, "<H2>Alignment of %s and %s:%d-%d</H2>\n",
	    qName, psl->tName, psl->tStart+1, psl->tEnd);

fputs("Click on links in the frame to the left to navigate through "
      "the alignment.\n", f);
blockCount = pslShowAlignment(psl, qType == gftProt,
                              qName, qSeq, qStart, qEnd,
                              tName, tSeq, tStart, tEnd, f);
freeDnaSeq(&tSeq);
return blockCount;
}

static struct ffAli *pslToFfAliAndSequence(struct psl *psl, struct dnaSeq *qSeq,
				    boolean *retIsRc, struct dnaSeq **retSeq,
				    int *retTStart)
/* Given psl, dig up target sequence and convert to ffAli.
 * Note: if strand is -, this does a pslRc to psl! */
{
int tStart, tEnd, tRcAdjustedStart;
struct dnaSeq *dnaSeq;

tStart = psl->tStart - 100;
if (tStart < 0) tStart = 0;
if (retTStart)
    *retTStart = tStart;

tEnd  = psl->tEnd + 100;
if (tEnd > psl->tSize) tEnd = psl->tSize;

dnaSeq = hDnaFromSeq(database, seqName, tStart, tEnd, dnaLower);
freez(&dnaSeq->name);
dnaSeq->name = cloneString(psl->tName);
if (retSeq)
    *retSeq = dnaSeq;

tRcAdjustedStart = tStart;

if (psl->strand[1] == '-')
    pslRc(psl);

if (psl->strand[0] == '-')
    {
    if (retIsRc)
	*retIsRc = TRUE;
    reverseComplement(dnaSeq->dna, dnaSeq->size);
    pslRc(psl);
    tRcAdjustedStart = psl->tSize - tEnd;
    }
return pslToFfAli(psl, qSeq, dnaSeq, tRcAdjustedStart);
}

static void ffStartEndQ(char *qDna, struct ffAli *ff, int *retStartQ, int *retEndQ)
/* Return query start and end */
{
if (ff == NULL)
    *retStartQ = *retEndQ = 0;
else
    {
    struct ffAli *right = ffRightmost(ff);
    *retStartQ = ff->nStart - qDna;
    *retEndQ = right->nEnd - qDna;
    }
}

int showPartialDnaAlignment(struct psl *wholePsl,
			    struct dnaSeq *rnaSeq, FILE *body,
			    int cdsS, int cdsE, boolean restrictToWindow)
/* Show (part of) alignment for accession.  wholePsl is the whole alignment;
 * if restrictToWindow then display the part of the alignment in the current
 * browser window. */
{
/* Do a consistency check between rnaSeq and psl*/
int rnaSize = rnaSeq->size;
if (rnaSize != wholePsl->qSize)
    {
    fprintf(body, "<p><b>Cannot display alignment. Size of rna %s is %d has changed since alignment was performed when it was %d.\n",
            wholePsl->qName, rnaSize, wholePsl->qSize);
    return 0;
    }

/* Possibly just do smaller subset that is within window. */
struct psl *psl = wholePsl;
if (restrictToWindow)
    {
    psl = pslTrimToTargetRange(wholePsl, winStart, winEnd);
    if (psl == NULL)
       fprintf(body, "<p>No alignment of %s within browser window.", wholePsl->qName);
    }

struct dnaSeq *dnaSeq;
int wholeTStart;
int partTStart = psl->tStart, partTEnd = psl->tEnd;
DNA *rna = rnaSeq->dna;
boolean isRc = FALSE;
struct ffAli *ffAli;
int blockCount;

/* Don't forget -- this may change psl if on reverse strand! */
ffAli = pslToFfAliAndSequence(psl, rnaSeq, &isRc, &dnaSeq,
				   &wholeTStart);

int rnaStart = 0;
int rnaEnd = rnaSize;

if (restrictToWindow)
    {
    /* Find start/end in ffAli, which is clipped to target from PSL and maybe RC'd */
    ffStartEndQ(rna, ffAli, &rnaStart, &rnaEnd);

    /* Add 100 bases either side if possible */
    if (rnaStart >= 100) rnaStart -= 100;
    if (rnaEnd + 100 <= rnaSize) rnaEnd += 100;
    }

/* Write body heading info. */
char *displayChromName = chromAliasGetDisplayChrom(database, cart, psl->tName);
fprintf(body, "<H2>Alignment of %s and %s:%d-%d</H2>\n",
	psl->qName, displayChromName, partTStart+1, partTEnd);
fprintf(body, "Click on links in the frame to the left to navigate through "
	"the alignment.\n");

blockCount = ffShAliPart(body, ffAli, wholePsl->qName,
                         rna + rnaStart, rnaEnd - rnaStart, rnaStart,
			 displayChromName, dnaSeq->dna, dnaSeq->size,
			 wholeTStart, 8, FALSE, isRc,
			 FALSE, TRUE, TRUE, TRUE, TRUE,
                         cdsS, cdsE, partTStart, partTEnd);
return blockCount;
}

void showSomeAlignment(struct psl *psl, bioSeq *oSeq,
                       enum gfType qType, int qStart, int qEnd,
                       char *qName, int cdsS, int cdsE)
/* Display protein or DNA alignment in a frame. */
{
int blockCount, i;
struct tempName indexTn, bodyTn;
FILE *index, *body;

trashDirFile(&indexTn, "index", "index", ".html");
trashDirFile(&bodyTn, "body", "body", ".html");

/* Writing body of alignment. */
body = mustOpen(bodyTn.forCgi, "w");
htmStartDirDepth(body, psl->qName, 2);
if (qType == gftRna || qType == gftDna)
    blockCount = showPartialDnaAlignment(psl, oSeq, body, cdsS, cdsE, FALSE);
else
    blockCount = showGfAlignment(psl, oSeq, body, qType, qStart, qEnd, qName);
htmEnd(body);
fclose(body);
chmod(bodyTn.forCgi, 0666);

/* Write index. */
index = mustOpen(indexTn.forCgi, "w");
if (qName == NULL)
    qName = psl->qName;
htmStartDirDepth(index, qName, 2);
fprintf(index, "<H3>Alignment of %s</H3>", qName);
fprintf(index, "<A HREF=\"../%s#cDNA\" TARGET=\"body\">%s</A><BR>\n", bodyTn.forCgi, qName);
char *displayChromName = chromAliasGetDisplayChrom(database, cart, psl->tName);
fprintf(index, "<A HREF=\"../%s#genomic\" TARGET=\"body\">%s.%s</A><BR>\n", bodyTn.forCgi, hOrganism(database), displayChromName);
for (i=1; i<=blockCount; ++i)
    {
    fprintf(index, "<A HREF=\"../%s#%d\" TARGET=\"body\">block%d</A><BR>\n",
	    bodyTn.forCgi, i, i);
    }
fprintf(index, "<A HREF=\"../%s#ali\" TARGET=\"body\">together</A><BR>\n", bodyTn.forCgi);
htmEnd(index);
fclose(index);
chmod(indexTn.forCgi, 0666);

/* Write (to stdout) the main html page containing just the frame info. */
puts("<FRAMESET COLS = \"13%,87% \" >");
printf("  <FRAME SRC=\"%s\" NAME=\"index\">\n", indexTn.forCgi);
printf("  <FRAME SRC=\"%s\" NAME=\"body\">\n", bodyTn.forCgi);
puts("<NOFRAMES><BODY></BODY></NOFRAMES>");
puts("</FRAMESET>");
puts("</HTML>\n");
exit(0);	/* Avoid cartHtmlEnd. */
}


void showSomePartialDnaAlignment(struct psl *partPsl, struct psl *wholePsl,
				 bioSeq *oSeq, char *qName, int cdsS, int cdsE)
/* Display protein or DNA alignment in a frame. */
{
int blockCount, i;
struct tempName indexTn, bodyTn;
FILE *index, *body;

trashDirFile(&indexTn, "index", "index", ".html");
trashDirFile(&bodyTn, "body", "body", ".html");

/* Writing body of alignment. */
body = mustOpen(bodyTn.forCgi, "w");
htmStartDirDepth(body, partPsl->qName, 2);
blockCount = showPartialDnaAlignment(wholePsl, oSeq, body, cdsS, cdsE, TRUE);
htmEnd(body);
fclose(body);
chmod(bodyTn.forCgi, 0666);

/* Write index. */
index = mustOpen(indexTn.forCgi, "w");
if (qName == NULL)
    qName = partPsl->qName;
htmStartDirDepth(index, qName, 2);
fprintf(index, "<H3>Alignment of %s</H3>", qName);
fprintf(index, "<A HREF=\"../%s#cDNA\" TARGET=\"body\">%s</A><BR>\n", bodyTn.forCgi, qName);
if (partPsl != wholePsl)
    fprintf(index, "<A HREF=\"../%s#cDNAStart\" TARGET=\"body\">%s in browser window</A><BR>\n", bodyTn.forCgi, qName);
fprintf(index, "<A HREF=\"../%s#genomic\" TARGET=\"body\">%s.%s</A><BR>\n", bodyTn.forCgi, hOrganism(database), partPsl->tName);
for (i=1; i<=blockCount; ++i)
    {
    fprintf(index, "<A HREF=\"../%s#%d\" TARGET=\"body\">block%d</A><BR>\n",
	    bodyTn.forCgi, i, i);
    }
fprintf(index, "<A HREF=\"../%s#ali\" TARGET=\"body\">together</A><BR>\n", bodyTn.forCgi);
htmEnd(index);
fclose(index);
chmod(indexTn.forCgi, 0666);

/* Write (to stdout) the main html page containing just the frame info. */
if (partPsl != wholePsl)
    printf("<FRAMESET COLS = \"13%%,87%% \" "
	   "ONLOAD=\"body.location.href = '%s#cDNAStart';\">\n",
	   bodyTn.forCgi);
else
    puts("<FRAMESET COLS = \"13%,87% \" >");
printf("  <FRAME SRC=\"%s\" NAME=\"index\">\n", indexTn.forCgi);
printf("  <FRAME SRC=\"%s\" NAME=\"body\">\n", bodyTn.forCgi);
puts("<NOFRAMES><BODY></BODY></NOFRAMES>");
puts("</FRAMESET>");
puts("</HTML>\n");
exit(0);	/* Avoid cartHtmlEnd. */
}

static void getCdsStartAndStop(struct sqlConnection *conn, char *acc, char *trackTable,
			       uint *retCdsStart, uint *retCdsEnd)
/* Get cds start and stop, if available */
{
struct trackDb *tdb = hashFindVal(trackHash, trackTable);
// Note: this variable was previously named cdsTable but unfortunately the
// hg/(inc|lib)/genbank.[hc] code uses the global var cdsTable!
char *tdbCdsTable = tdb ? trackDbSetting(tdb, "cdsTable") : NULL;
if (isEmpty(tdbCdsTable) && startsWith("ncbiRefSeq", trackTable))
    tdbCdsTable = "ncbiRefSeqCds";
if (isNotEmpty(tdbCdsTable) && hTableExists(database, tdbCdsTable))
    {
    char query[256];
    sqlSafef(query, sizeof(query), "select cds from %s where id = '%s'", tdbCdsTable, acc);
    char *cdsString = sqlQuickString(conn, query);
    if (isNotEmpty(cdsString))
        genbankParseCds(cdsString, retCdsStart, retCdsEnd);
    }
else if (sqlTableExists(conn, gbCdnaInfoTable))
    {
    char accChopped[512];
    safecpy(accChopped, sizeof(accChopped), acc);
    chopSuffix(accChopped);
    char query[256];
    sqlSafef(query, sizeof query, "select cds from %s where acc = '%s'",
             gbCdnaInfoTable, accChopped);
    char *cdsId = sqlQuickString(conn, query);
    if (isNotEmpty(cdsId))
        {
        sqlSafef(query, sizeof query, "select name from %s where id = '%s'", cdsTable, cdsId);
        char *cdsString = sqlQuickString(conn, query);
        if (isNotEmpty(cdsString))
            genbankParseCds(cdsString, retCdsStart, retCdsEnd);
        }
    }
}

void htcBigPslAli(char *acc)
/* Show alignment for accession in bigPsl file. */
{
struct psl *psl;
char *aliTable;
int start;
unsigned int cdsStart = 0, cdsEnd = 0;
struct sqlConnection *conn = NULL;
struct trackDb *tdb = NULL;

aliTable = cartString(cart, "aliTable");
if (isCustomTrack(aliTable))
    {
    struct customTrack *ct = lookupCt(aliTable);
    tdb = ct->tdb;
    }
else
    tdb = hashFindVal(trackHash, aliTable);
if (tdb == NULL)
    errAbort("BUG: bigPsl alignment table '%s' not found; this maybe causes by `.' in track names", aliTable);
             
if (!trackHubDatabase(database))
    conn = hAllocConnTrack(database, tdb);

char title[1024];
safef(title, sizeof title, "%s vs Genomic [%s]", acc, aliTable);
htmlFramesetStart(title);

/* Get some environment vars. */
start = cartInt(cart, "l");
int end = cartInt(cart, "r");
char *chrom = cartString(cart, "c");

char *seq, *cdsString = NULL;
struct lm *lm = lmInit(0);
char *fileName = bbiNameFromSettingOrTable(tdb, conn, tdb->table);
struct bbiFile *bbi =  bigBedFileOpenAlias(fileName, chromAliasFindAliases);
struct bigBedInterval *bb, *bbList = bigBedIntervalQuery(bbi, chrom, start, end, 0, lm);
char *bedRow[32];
char startBuf[16], endBuf[16];
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    bigBedIntervalToRow(bb, seqName, startBuf, endBuf, bedRow, ArraySize(bedRow));
    struct bed *bed = bedLoadN(bedRow, 12);
    if (sameString(bed->name, acc) && (bb->start == start) && (bb->end == end))
	{
	bb->next = NULL;
	break;
	}
    }
if (bb == NULL)
    errAbort("item %s not found in range %s:%d-%d in bigBed %s (%s)",
             acc, chrom, start, end, tdb->table, fileName);
unsigned seqTypeField =  bbExtraFieldIndex(bbi, "seqType");
psl = getPslAndSeq(tdb, seqName, bb, seqTypeField, &seq, &cdsString);
if (cdsString)
    genbankParseCds(cdsString,  &cdsStart, &cdsEnd);


if (seq == NULL)
    {
    printf("Sequence for %s not available.\n", psl->qName);
    return;
    }
struct dnaSeq *rnaSeq = newDnaSeq(seq, strlen(seq), acc);
enum gfType type = gftRna;
if (pslIsProtein(psl))
    type = gftProt;
showSomeAlignment(psl, rnaSeq, type, 0, rnaSeq->size, NULL, cdsStart, cdsEnd);
}

void htcBigPslAliInWindow(char *acc)
/* Show alignment in window for accession in bigPsl file. */
{
struct psl *partPsl, *wholePsl;
char *aliTable;
int start;
unsigned int cdsStart = 0, cdsEnd = 0;
struct trackDb *tdb = NULL;

aliTable = cartString(cart, "aliTable");
if (isCustomTrack(aliTable))
    {
    struct customTrack *ct = lookupCt(aliTable);
    tdb = ct->tdb;
    }
else
    tdb = hashFindVal(trackHash, aliTable);
char title[1024];
safef(title, sizeof title, "%s vs Genomic [%s]", acc, aliTable);
htmlFramesetStart(title);

/* Get some environment vars. */
start = cartInt(cart, "l");
int end = cartInt(cart, "r");
char *chrom = cartString(cart, "c");

char *seq, *cdsString = NULL;
struct lm *lm = lmInit(0);
char *fileName = bbiNameFromSettingOrTable(tdb, NULL, tdb->table);
struct bbiFile *bbi =  bigBedFileOpenAlias(fileName, chromAliasFindAliases);
struct bigBedInterval *bb, *bbList = bigBedIntervalQuery(bbi, chrom, start, end, 0, lm);
char *bedRow[32];
char startBuf[16], endBuf[16];
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    bigBedIntervalToRow(bb, seqName, startBuf, endBuf, bedRow, ArraySize(bedRow));
    struct bed *bed = bedLoadN(bedRow, 12);
    if (sameString(bed->name, acc))
	{
	bb->next = NULL;
	break;
	}
    }
unsigned seqTypeField =  bbExtraFieldIndex(bbi, "seqType");
wholePsl = getPslAndSeq(tdb, seqName, bb, seqTypeField, &seq, &cdsString);

if (seq == NULL)
    {
    printf("Sequence for %s not available.\n", wholePsl->qName);
    return;
    }
if (cdsString)
    genbankParseCds(cdsString,  &cdsStart, &cdsEnd);

if (wholePsl->tStart >= winStart && wholePsl->tEnd <= winEnd)
    partPsl = wholePsl;
else
    partPsl = pslTrimToTargetRange(wholePsl, winStart, winEnd);
struct dnaSeq *rnaSeq = newDnaSeq(seq, strlen(seq), acc);
showSomePartialDnaAlignment(partPsl, wholePsl, rnaSeq,
                            NULL, cdsStart, cdsEnd);
}

static struct dnaSeq *getBaseColorSequence(char *itemName, char *table)
/* Grab sequence using the sequence and extFile table names out of BASE_COLOR_USE_SEQUENCE. */
{
struct trackDb *tdb = hashMustFindVal(trackHash, table);
char *spec = trackDbRequiredSetting(tdb, BASE_COLOR_USE_SEQUENCE);
char *specCopy = cloneString(spec);

// value is: extFile seqTbl extFileTbl
// or:       db [dddBbb1]
char *words[3];
int nwords = chopByWhite(specCopy, words, ArraySize(words));
if (sameString(words[0], "extFile") && (nwords == ArraySize(words)))
    return hDnaSeqGet(database, itemName, words[1], words[2]);
else if (sameString(words[0], "db"))
    {
    char *db = (nwords == 2) ? words[1] : database;
    return hChromSeq(db, itemName, 0, 0);
    }
else
    errAbort("invalid %s track setting: %s", BASE_COLOR_USE_SEQUENCE, spec);
return NULL;
}

void htcCdnaAli(char *acc)
/* Show alignment for accession. */
{
char query[256];
char table[HDB_MAX_TABLE_STRING];
char accTmp[64];
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
struct psl *psl;
struct dnaSeq *rnaSeq;
char *aliTable;
int start;
unsigned int cdsStart = 0, cdsEnd = 0;
boolean hasBin;
char accChopped[512] ;
safef(accChopped, sizeof(accChopped), "%s",acc);
chopSuffix(accChopped);

aliTable = cartString(cart, "aliTable");
char *accForTitle = startsWith("ncbiRefSeq", aliTable) ? acc : accChopped;
char title[1024];
safef(title, sizeof title, "%s vs Genomic [%s]", accForTitle, aliTable);
htmlFramesetStart(title);

/* Get some environment vars. */
start = cartInt(cart, "o");

conn = hAllocConn(database);
getCdsStartAndStop(conn, acc, aliTable, &cdsStart, &cdsEnd);

/* Look up alignments in database */
if (!hFindSplitTable(database, seqName, aliTable, table, sizeof table, &hasBin))
    errAbort("Failed to find aliTable=%s", aliTable);
sqlSafef(query, sizeof query, "select * from %s where qName like '%s%%' and tName=\"%s\" and tStart=%d",
	table, acc, seqName, start);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find alignment for %s at %d", acc, start);
psl = pslLoad(row+hasBin);
sqlFreeResult(&sr);

/* get bz rna snapshot for blastz alignments */
if (sameString("mrnaBlastz", aliTable) || sameString("pseudoMrna", aliTable))
    {
    struct sqlConnection *conn = hAllocConn(database);
    unsigned retId = 0;
    safef(accTmp, sizeof accTmp, "bz-%s", acc);
    if (hRnaSeqAndIdx(accTmp, &rnaSeq, &retId, conn) == -1)
        rnaSeq = hRnaSeq(database, acc);
    hFreeConn(&conn);
    }
else if (sameString("HInvGeneMrna", aliTable))
    {
    /* get RNA accession for the gene id in the alignment */
    sqlSafef(query, sizeof query, "select mrnaAcc from HInv where geneId='%s'", acc);
    rnaSeq = hRnaSeq(database, sqlQuickString(conn, query));
    }
else if (sameString("ncbiRefSeqPsl", aliTable) || startsWith("altSeqLiftOverPsl", aliTable) ||
         startsWith("fixSeqLiftOverPsl", aliTable))
    {
    rnaSeq = getBaseColorSequence(acc, aliTable);
    }
else
    {
    char *cdnaTable = NULL;
    struct trackDb *tdb = hashFindVal(trackHash, aliTable);
    if (tdb != NULL)
	cdnaTable = trackDbSetting(tdb, "cdnaTable");
    if (isNotEmpty(cdnaTable) && hTableExists(database, cdnaTable))
	rnaSeq = hGenBankGetMrna(database, acc, cdnaTable);
    else
	rnaSeq = hRnaSeq(database, acc);
    }

if (NULL == rnaSeq)
    {
	printf("RNA sequence not found: '%s'", acc);
    }
else
    {
    if (startsWith("xeno", aliTable))
        showSomeAlignment(psl, rnaSeq, gftDnaX, 0, rnaSeq->size, NULL, cdsStart, cdsEnd);
    else
        showSomeAlignment(psl, rnaSeq, gftDna, 0, rnaSeq->size, NULL, cdsStart, cdsEnd);
    }
hFreeConn(&conn);
}

void htcCdnaAliInWindow(char *acc)
/* Show part of alignment in browser window for accession. */
{
struct psl *wholePsl, *partPsl;
struct dnaSeq *rnaSeq;
char *aliTable;
int start;
unsigned int cdsStart = 0, cdsEnd = 0;
char accChopped[512] ;
safef(accChopped, sizeof(accChopped), "%s",acc);
chopSuffix(accChopped);

/* Get some environment vars. */
aliTable = cartString(cart, "aliTable");
start = cartInt(cart, "o");

char *accForTitle = startsWith("ncbiRefSeq", aliTable) ? acc : accChopped;
char title[1024];
safef(title, sizeof title, "%s vs Genomic [%s]", accForTitle, aliTable);
htmlFramesetStart(title);

if (startsWith("user", aliTable))
    {
    char *pslName, *faName, *qName;
    struct lineFile *lf;
    bioSeq *oSeqList = NULL, *oSeq = NULL;
    struct psl *psl;
    int start;
    enum gfType tt, qt;
    boolean isProt;
    char *ss = cartOptionalString(cart, "ss");

    if ((ss != NULL) && !ssFilesExist(ss))
	{
	ss = NULL;
	cartRemove(cart, "ss");
	errAbort("hgBlat temporary files not found");
	}

    start = cartInt(cart, "o");

    // itemIn is three words, the first two are the ss files and the third is the accession
    char *itemIn = cloneString(acc);
    char *words[3];
    int numWords = chopByWhite(itemIn, words, 3);
    if (numWords != 3)
        errAbort("ItemIn string doesn't have three words.");
    qName = words[2];

    parseSs(ss, &pslName, &faName, NULL);
    pslxFileOpen(pslName, &qt, &tt, &lf);
    isProt = (qt == gftProt);
    if (isProt)
	errAbort("hgBlat protein alignments not supported for htcCdnaAliInWindow");
    while ((psl = pslNext(lf)) != NULL)
	{
        if (sameString(psl->tName, seqName)
	 && sameString(psl->qName, qName)
         && psl->tStart == start
	    )
	    break;
	pslFree(&psl);
	}
    lineFileClose(&lf);
    if (psl == NULL)
	errAbort("Couldn't find alignment at %s:%d", seqName, start);
    oSeqList = faReadAllSeq(faName, !isProt);
    for (oSeq = oSeqList; oSeq != NULL; oSeq = oSeq->next)
	{
	if (sameString(oSeq->name, qName))
	    break;
	}
    if (oSeq == NULL)
	errAbort("%s is in %s but not in %s. Internal error.", qName, pslName, faName);
    wholePsl = psl;
    rnaSeq = oSeq;
    }
else
    {
    /* Look up alignments in database */
    struct sqlConnection *conn = hAllocConn(database);
    getCdsStartAndStop(conn, acc, aliTable, &cdsStart, &cdsEnd);

    char table[64];
    boolean hasBin;
    if (!hFindSplitTable(database, seqName, aliTable, table, sizeof table, &hasBin))
	errAbort("aliTable %s not found", aliTable);
    char query[256];
    sqlSafef(query, sizeof(query),
         "select * from %s where qName = '%s' and tName=\"%s\" and tStart=%d", 
         table, acc, seqName, start);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row;
    if ((row = sqlNextRow(sr)) == NULL)
	errAbort("Couldn't find alignment for %s at %d", acc, start);
    wholePsl = pslLoad(row+hasBin);
    sqlFreeResult(&sr);

    if (startsWith("ucscRetroAli", aliTable) || startsWith("retroMrnaAli", aliTable) ||
        sameString("pseudoMrna", aliTable) || startsWith("altSeqLiftOverPsl", aliTable) ||
        startsWith("fixSeqLiftOverPsl", aliTable) || startsWith("ncbiRefSeqPsl", aliTable))
	{
        rnaSeq = getBaseColorSequence(acc, aliTable);
	}
    else if (sameString("HInvGeneMrna", aliTable))
	{
	/* get RNA accession for the gene id in the alignment */
	sqlSafef(query, sizeof(query), "select mrnaAcc from HInv where geneId='%s'",
	      acc);
	rnaSeq = hRnaSeq(database, sqlQuickString(conn, query));
	}
    else
	{
	char *cdnaTable = NULL;
	struct trackDb *tdb = hashFindVal(trackHash, aliTable);
	if (tdb != NULL)
	    cdnaTable = trackDbSetting(tdb, "cdnaTable");
	if (isNotEmpty(cdnaTable) && hTableExists(database, cdnaTable))
	    rnaSeq = hGenBankGetMrna(database, acc, cdnaTable);
	else
	    rnaSeq = hRnaSeq(database, acc);
	}
    hFreeConn(&conn);
    }
/* Get partial psl for part of alignment in browser window: */
if (wholePsl->tStart >= winStart && wholePsl->tEnd <= winEnd)
    partPsl = wholePsl;
else
    partPsl = pslTrimToTargetRange(wholePsl, winStart, winEnd);

if (startsWith("xeno", aliTable))
    errAbort("htcCdnaAliInWindow does not support translated alignments.");
else
    showSomePartialDnaAlignment(partPsl, wholePsl, rnaSeq,
				NULL, cdsStart, cdsEnd);
}

void htcChainAli(char *item)
/* Draw detailed alignment representation of a chain. */
{
struct chain *chain;
struct psl *fatPsl, *psl = NULL;
char *track = cartString(cart, "o");
struct trackDb *tdb = getTdbForTrackName(track);
char *type = cloneString(tdb->type);
char *typeWords[2];
char *otherDb = NULL, *org = NULL, *otherOrg = NULL;
struct dnaSeq *qSeq = NULL;
char name[128];

/* Figure out other database. */
if (chopLine(type, typeWords) < ArraySize(typeWords))
    errAbort("type line for %s is short in trackDb", track);
otherDb = typeWords[1];
if (! sameWord(otherDb, "seq"))
    {
    otherOrg = hOrganism(otherDb);
    }
org = hOrganism(database);

/* Load up subset of chain and convert it to part of psl
 * that just fits inside of window. */
chain = chainLoadItemInRange(tdb, item);
if (chain->blockList == NULL)
    {
    printf("None of chain is actually in the window");
    return;
    }
fatPsl = chainToPsl(chain);

chainFree(&chain);

psl = pslTrimToTargetRange(fatPsl, winStart, winEnd);
pslFree(&fatPsl);

struct twoBitFile *otherTbf = getOtherTwoBitUrl(tdb);
if (sameWord(otherDb, "seq"))
    {
    qSeq = hExtSeqPart(database, psl->qName, psl->qStart, psl->qEnd);
    safef(name, sizeof name, "%s", psl->qName);
    }
else if (otherDb != NULL)
    {
    qSeq = loadGenomePart(otherDb, psl->qName, psl->qStart, psl->qEnd);
    safef(name, sizeof name, "%s.%s", otherOrg, psl->qName);
    }
else if (otherTbf != NULL)
    {
    qSeq = twoBitReadSeqFragLower(otherTbf, psl->qName, psl->qStart, psl->qEnd);
    safef(name, sizeof name, "%s", psl->qName);
    }
if (qSeq == NULL)
    {
    errAbort("Can't find query sequence in htcChainAli");
    }
char title[1024];
safef(title, sizeof title, "%s %s vs %s %s ",
       (otherOrg == NULL ? "" : otherOrg), psl->qName, org, psl->tName );
htmlFramesetStart(title);
showSomeAlignment(psl, qSeq, gftDnaX, psl->qStart, psl->qEnd, name, 0, 0);
}

void htcChainTransAli(char *item)
/* Draw detailed alignment representation of a chain with translated protein */
{
struct chain *chain;
struct psl *fatPsl, *psl = NULL;
char *track = cartString(cart, "o");
char *type = trackTypeInfo(track);
char *typeWords[2];
char *otherDb = NULL, *org = NULL, *otherOrg = NULL;
struct dnaSeq *qSeq = NULL;
char name[128];
int cdsStart = cgiInt("qs");
int cdsEnd = cgiInt("qe");

/* Figure out other database. */
if (chopLine(type, typeWords) < ArraySize(typeWords))
    errAbort("type line for %s is short in trackDb", track);
otherDb = typeWords[1];
if (! sameWord(otherDb, "seq"))
    {
    otherOrg = hOrganism(otherDb);
    }
org = hOrganism(database);

/* Load up subset of chain and convert it to part of psl
 * that just fits inside of window. */
struct trackDb *tdb = getTdbForTrackName(track);
chain = chainLoadItemInRange(tdb, item);
if (chain->blockList == NULL)
    {
    printf("None of chain is actually in the window");
    return;
    }
fatPsl = chainToPsl(chain);

chainFree(&chain);

psl = pslTrimToTargetRange(fatPsl, winStart, winEnd);
pslFree(&fatPsl);

if (sameWord(otherDb, "seq"))
    {
    qSeq = hExtSeq(database, psl->qName);
    safef(name, sizeof name, "%s", psl->qName);
    }
else
    {
    qSeq = loadGenomePart(otherDb, psl->qName, psl->qStart, psl->qEnd);
    safef(name, sizeof name, "%s.%s", otherOrg, psl->qName);
    }
char title[1024];
safef(title, sizeof title, "%s %s vs %s %s ",
       (otherOrg == NULL ? "" : otherOrg), psl->qName, org, psl->tName );
htmlFramesetStart(title);
/*showSomeAlignment(psl, qSeq, gftDnaX, psl->qStart, psl->qEnd, name, 0, 0); */
showSomeAlignment(psl, qSeq, gftDnaX, psl->qStart, psl->qEnd, name, cdsStart, cdsEnd);
}

void htcUserAli(char *fileNames)
/* Show alignment for accession. */
{
char *pslName, *faName, *qName;
struct lineFile *lf;
bioSeq *oSeqList = NULL, *oSeq = NULL;
struct psl *psl;
int start;
enum gfType tt, qt;
boolean isProt;

char title[1024];
safef(title, sizeof title, "User Sequence vs Genomic");
htmlFramesetStart(title);

start = cartInt(cart, "o");
parseSs(fileNames, &pslName, &faName, &qName);
pslxFileOpen(pslName, &qt, &tt, &lf);
isProt = (qt == gftProt);
while ((psl = pslNext(lf)) != NULL)
    {
    if (sameString(psl->tName, seqName) && psl->tStart == start && sameString(psl->qName, qName))
        break;
    pslFree(&psl);
    }
lineFileClose(&lf);
if (psl == NULL)
    errAbort("Couldn't find alignment at %s:%d", seqName, start);
oSeqList = faReadAllSeq(faName, !isProt);
for (oSeq = oSeqList; oSeq != NULL; oSeq = oSeq->next)
    {
    if (sameString(oSeq->name, qName))
	break;
    }
if (oSeq == NULL)  errAbort("%s is in %s but not in %s. Internal error.", qName, pslName, faName);
showSomeAlignment(psl, oSeq, qt, 0, oSeq->size, NULL, 0, 0);
}

void htcProteinAli(char *readName, char *table)
/* Show protein to translated dna alignment for accession. */
{
struct psl *psl;
int start;
enum gfType qt = gftProt;
struct sqlResult *sr;
struct sqlConnection *conn = hAllocConn(database);
struct dnaSeq *seq = NULL;
char query[256], **row;
char fullTable[HDB_MAX_TABLE_STRING];
boolean hasBin;
char buffer[256];
int addp = 0;
char *pred = NULL;

char title[1024];
safef(title, sizeof title, "Protein Sequence vs Genomic");
htmlFramesetStart(title);

addp = cartUsualInt(cart, "addp",0);
pred = cartUsualString(cart, "pred",NULL);
start = cartInt(cart, "o");
if (!hFindSplitTable(database, seqName, table, fullTable, sizeof fullTable, &hasBin))
    errAbort("track %s not found", table);
sqlSafef(query, sizeof query, "select * from %s where qName = '%s' and tName = '%s' and tStart=%d",
	fullTable, readName, seqName, start);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find alignment for %s at %d", readName, start);
psl = pslLoad(row+hasBin);
sqlFreeResult(&sr);
if ((addp == 1) || (pred != NULL))
    {
    char *ptr;

    safef(buffer, sizeof buffer, "%s",readName);

    if (!(sameString(pred, "ce3.blastWBPep01")
	    || sameString(pred, "ce9.blastSGPep01")
	    || sameString(pred, "ce6.blastSGPep01")
	    || sameString(pred, "ce4.blastSGPep01"))  &&
	(ptr = strchr(buffer, '.')) != NULL)
	{
	*ptr = 0;
	psl->qName = cloneString(buffer);
	*ptr++ = 'p';
	*ptr = 0;
	}
    if (addp == 1)
	seq = hPepSeq(database, buffer);
    else
	{
	sqlSafef(query, sizeof(query),
	    "select seq from %s where name = '%s'", pred, psl->qName);
	sr = sqlGetResult(conn, query);
	if ((row = sqlNextRow(sr)) != NULL)
	    seq = newDnaSeq(cloneString(row[0]), strlen(row[0]), psl->qName);
	else
	    errAbort("Cannot find sequence for '%s' in %s",psl->qName, pred);
	sqlFreeResult(&sr);
	}
    }
else
    seq = hPepSeq(database, readName);
hFreeConn(&conn);
showSomeAlignment(psl, seq, qt, 0, seq->size, NULL, 0, 0);
}

void htcBlatXeno(char *readName, char *table)
/* Show alignment for accession. */
{
struct psl *psl;
int start;
struct sqlResult *sr;
struct sqlConnection *conn = hAllocConn(database);
struct dnaSeq *seq;
char query[256], **row;
char fullTable[HDB_MAX_TABLE_STRING];
boolean hasBin;

char title[1024];
safef(title, sizeof title, "Sequence %s", readName);
htmlFramesetStart(title);

start = cartInt(cart, "o");
if (!hFindSplitTable(database, seqName, table, fullTable, sizeof fullTable, &hasBin))
    errAbort("track %s not found", table);
sqlSafef(query, sizeof query, "select * from %s where qName = '%s' and tName = '%s' and tStart=%d",
	fullTable, readName, seqName, start);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find alignment for %s at %d", readName, start);
psl = pslLoad(row+hasBin);
sqlFreeResult(&sr);
hFreeConn(&conn);
seq = hExtSeq(database, readName);
showSomeAlignment(psl, seq, gftDnaX, 0, seq->size, NULL, 0, 0);
}

void writeMatches(FILE *f, char *a, char *b, int count)
/* Write a | where a and b agree, a ' ' elsewhere. */
{
int i;
for (i=0; i<count; ++i)
    {
    if (a[i] == b[i])
        fputc('|', f);
    else
        fputc(' ', f);
    }
}

void fetchAndShowWaba(char *table, char *name)
/* Fetch and display waba alignment. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int start = cartInt(cart, "o");
struct wabAli *wa = NULL;
int qOffset;
char strand = '+';

sqlSafef(query, sizeof query, "select * from %s where query = '%s' and chrom = '%s' and chromStart = %d",
	table, name, seqName, start);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Sorry, couldn't find alignment of %s at %d of %s in database",
	     name, start, seqName);
wa = wabAliLoad(row);
printf("<PRE><TT>");
qOffset = wa->qStart;
if (wa->strand[0] == '-')
    {
    strand = '-';
    qOffset = wa->qEnd;
    }
xenShowAli(wa->qSym, wa->tSym, wa->hSym, wa->symCount, stdout,
	   qOffset, wa->chromStart, strand, '+', 60);
printf("</TT></PRE>");

wabAliFree(&wa);
hFreeConn(&conn);
}

void doHgTet(struct trackDb *tdb, char *name)
/* Do thing with tet track. */
{
cartWebStart(cart, database, "Fish Alignment");
printf("Alignment between fish sequence %s (above) and human chromosome %s (below)\n",
       name, skipChr(seqName));
fetchAndShowWaba("waba_tet", name);
}


void doHgCbr(struct trackDb *tdb, char *name)
/* Do thing with cbr track. */
{
cartWebStart(cart, database, "Worm Alignment");
printf("Alignment between C briggsae sequence %s (above) and C elegans chromosome %s (below)\n",
       name, skipChr(seqName));
fetchAndShowWaba("wabaCbr", name);
}

void doHgRepeat(struct trackDb *tdb, char *repeat)
/* Do click on a repeat track. */
{
int offset = cartInt(cart, "o");
cartWebStart(cart, database, "Repeat");
if (offset >= 0)
    {
    struct sqlConnection *conn = hAllocConn(database);

    struct sqlResult *sr;
    char **row;
    struct rmskOut *ro;
    char query[256];
    char table[HDB_MAX_TABLE_STRING];
    boolean hasBin;
    int start = cartInt(cart, "o");

    if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, &hasBin))
	errAbort("Track %s not found", tdb->table);
    sqlSafef(query, sizeof query, "select * from %s where  repName = '%s' and genoName = '%s' and genoStart = %d",
	    table, repeat, seqName, start);
    sr = sqlGetResult(conn, query);
    if (sameString(tdb->table,"rmskNew"))
        printf("<H3>CENSOR Information</H3>\n");
    else
        printf("<H3>RepeatMasker Information</H3>\n");
    while ((row = sqlNextRow(sr)) != NULL)
	{
	ro = rmskOutLoad(row+hasBin);
	printf("<B>Name:</B> <A HREF='http://www.girinst.org/protected/repbase_extract.php?access=%s'>%s</A>\n", 
            ro->repName, ro->repName);
    printf("(link requires <A HREF='http://www.girinst.org/accountservices/register.php'>registration</A>)<BR>");
	printf("<B>Family:</B> %s<BR>\n", ro->repFamily);
	printf("<B>Class:</B> %s<BR>\n", ro->repClass);
	printf("<B>SW Score:</B> %d<BR>\n", ro->swScore);
	printf("<B>Divergence:</B> %3.1f%%<BR>\n", 0.1 * ro->milliDiv);
	printf("<B>Deletions:</B>  %3.1f%%<BR>\n", 0.1 * ro->milliDel);
	printf("<B>Insertions:</B> %3.1f%%<BR>\n", 0.1 * ro->milliIns);
	printf("<B>Begin in repeat:</B> %d<BR>\n",
	       (ro->strand[0] == '-' ? ro->repLeft : ro->repStart));
	printf("<B>End in repeat:</B> %d<BR>\n", ro->repEnd);
	printf("<B>Left in repeat:</B> %d<BR>\n",
	       (ro->strand[0] == '-' ? -ro->repStart : -ro->repLeft));
	printPos(seqName, ro->genoStart, ro->genoEnd, ro->strand, TRUE,
		 ro->repName);
	}
    hFreeConn(&conn);
    }
else
    {
    if (sameString(repeat, "SINE"))
	printf("This track contains the short interspersed nuclear element (SINE) class of repeats, which includes ALUs.\n");
    else if (sameString(repeat, "LINE"))
        printf("This track contains the long interspersed nuclear element (LINE) class of repeats.\n");
    else if (sameString(repeat, "LTR"))
        printf("This track contains the class of long terminal repeats (LTRs), which includes retroposons.\n");
    else
        printf("This track contains the %s class of repeats.\n", repeat);
    printf("Click on an individual repeat element within the track for more information about that item.<BR>\n");
    }
printTrackHtml(tdb);
}

void doHgIsochore(struct trackDb *tdb, char *item)
/* do click on isochore track. */
{
cartWebStart(cart, database, "Isochore Info");
printf("<H2>Isochore Information</H2>\n");
if (cgiVarExists("o"))
    {
    struct isochores *iso;
    struct sqlConnection *conn = hAllocConn(database);
    struct sqlResult *sr;
    char **row;
    char query[256];
    int start = cartInt(cart, "o");
    sqlSafef(query, sizeof query, "select * from %s where  name = '%s' and chrom = '%s' and chromStart = %d",
	    tdb->table, item, seqName, start);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	iso = isochoresLoad(row);
	printf("<B>Type:</B> %s<BR>\n", iso->name);
	printf("<B>GC Content:</B> %3.1f%%<BR>\n", 0.1*iso->gcPpt);
	printf("<B>Chromosome:</B> %s<BR>\n", skipChr(iso->chrom));
	printf("<B>Begin in chromosome:</B> %d<BR>\n", iso->chromStart);
	printf("<B>End in chromosome:</B> %d<BR>\n", iso->chromEnd);
	printf("<B>Size:</B> %d<BR>\n", iso->chromEnd - iso->chromStart);
	printf("<BR>\n");
	isochoresFree(&iso);
	}
    hFreeConn(&conn);
    }
printTrackHtml(tdb);
}

void doSimpleRepeat(struct trackDb *tdb, char *item)
/* Print info on simple repeat. */
{
cartWebStart(cart, database, "Simple Repeat Info");
printf("<H2>Simple Tandem Repeat Information</H2>\n");
if (cgiVarExists("o"))
    {
    struct simpleRepeat *rep;
    struct sqlConnection *conn = hAllocConn(database);
    struct sqlResult *sr;
    char **row;
    char query[256];
    int start = cartInt(cart, "o");
    int rowOffset = hOffsetPastBin(database, seqName, tdb->table);
    sqlSafef(query, sizeof query, "select * from %s where  name = '%s' and chrom = '%s' and chromStart = %d",
	    tdb->table, item, seqName, start);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	rep = simpleRepeatLoad(row+rowOffset);
	printf("<B>Period:</B> %d<BR>\n", rep->period);
	printf("<B>Copies:</B> %4.1f<BR>\n", rep->copyNum);
	printf("<B>Consensus size:</B> %d<BR>\n", rep->consensusSize);
	printf("<B>Match Percentage:</B> %d%%<BR>\n", rep->perMatch);
	printf("<B>Insert/Delete Percentage:</B> %d%%<BR>\n", rep->perIndel);
	printf("<B>Score:</B> %d<BR>\n", rep->score);
	printf("<B>Entropy:</B> %4.3f<BR>\n", rep->entropy);
	printf("<B>Sequence:</B> %s<BR>\n", rep->sequence);
	printPos(seqName, rep->chromStart, rep->chromEnd, NULL, TRUE,
		 rep->name);
	printf("<BR>\n");
	simpleRepeatFree(&rep);
	}
    hFreeConn(&conn);
    }
else
    {
    puts("<P>Click directly on a repeat for specific information on that repeat</P>");
    }
printTrackHtml(tdb);
}

void hgSoftPromoter(char *track, char *item)
/* Print info on Softberry promoter. */
{
cartWebStart(cart, database, "Softberry TSSW Promoter");
printf("<H2>Softberry TSSW Promoter Prediction %s</H2>", item);

if (cgiVarExists("o"))
    {
    struct softPromoter *pro;
    struct sqlConnection *conn = hAllocConn(database);
    struct sqlResult *sr;
    char **row;
    char query[256];
    int start = cartInt(cart, "o");
    int rowOffset = hOffsetPastBin(database, seqName, track);
    sqlSafef(query, sizeof query, "select * from %s where  name = '%s' and chrom = '%s' and chromStart = %d",
	    track, item, seqName, start);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	pro = softPromoterLoad(row+rowOffset);
	bedPrintPos((struct bed *)pro, 3, NULL);
	printf("<B>Short Name:</B> %s<BR>\n", pro->name);
	printf("<B>Full Name:</B> %s<BR>\n", pro->origName);
	printf("<B>Type:</B> %s<BR>\n", pro->type);
	printf("<B>Score:</B> %f<BR>\n", pro->origScore);
	printf("<B>Block Info:</B> %s<BR>\n", pro->blockString);
	printf("<BR>\n");
	htmlHorizontalLine();
	printCappedSequence(pro->chromStart, pro->chromEnd, 100);
	softPromoterFree(&pro);
	htmlHorizontalLine();
	}
    hFreeConn(&conn);
    }
printf("<P>This track was kindly provided by Victor Solovyev (EOS Biotechnology Inc.) on behalf of ");
printf("<A HREF=\"http://www.softberry.com\" TARGET=_blank>Softberry Inc.</A> ");
puts("using the TSSW program. "
     "Commercial use of these predictions is restricted to viewing in "
     "this browser.  Please contact Softberry Inc. to make arrangements "
     "for further commercial access.  Further information from Softberry on"
     "this track appears below.</P>"

     "<P>\"Promoters were predicted by Softberry promoter prediction program TSSW in "
     "regions up to 3000 from known starts of coding regions (ATG codon) or known "
     "mapped 5'-mRNA ends. We found that limiting promoter search to  such regions "
     "drastically reduces false positive predictions. Also, we have very strong "
     "thresholds for prediction of TATA-less promoters to minimize false positive "
     "predictions. </P>"
     " "
     "<P>\"Our promoter prediction software accurately predicts about 50% promoters "
     "accurately with a small average deviation from true start site. Such accuracy "
     "makes possible experimental work with found promoter candidates. </P>"
     " "
     "<P>\"For 20 experimentally verified promoters on Chromosome 22, TSSW predicted "
     "15, placed 12 of them  within (-150,+150) region from true TSS and 6 (30% of "
     "all promoters) - within -8,+2 region from true TSS.\" </P>");
}

void doCpgIsland(struct trackDb *tdb, char *item)
/* Print info on CpG Island. */
{
char *table = tdb->table;
boolean isExt = hHasField(database, table, "obsExp");
cartWebStart(cart, database, "CpG Island Info");
printf("<H2>CpG Island Info</H2>\n");
if (cgiVarExists("o"))
    {
    struct cpgIsland *island;
    struct cpgIslandExt *islandExt = NULL;
    struct sqlConnection *conn = hAllocConn(database);
    struct sqlResult *sr;
    char **row;
    char query[256];
    int start = cartInt(cart, "o");
    int rowOffset = hOffsetPastBin(database, seqName, table);
    sqlSafef(query, sizeof query, "select * from %s where  name = '%s' and chrom = '%s' and chromStart = %d",
	    table, item, seqName, start);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	if (isExt)
	    {
	    islandExt = cpgIslandExtLoad(row+rowOffset);
	    island = (struct cpgIsland *)islandExt;
	    }
	else
	    island = cpgIslandLoad(row+rowOffset);
	if (! startsWith("CpG: ", island->name))
	    printf("<B>Name:</B> %s<BR>\n", island->name);
	bedPrintPos((struct bed *)island, 3, tdb);
	printf("<B>Size:</B> %d<BR>\n", island->chromEnd - island->chromStart);
	printf("<B>CpG count:</B> %d<BR>\n", island->cpgNum);
	printf("<B>C count plus G count:</B> %d<BR>\n", island->gcNum);
	printf("<B>Percentage CpG:</B> %1.1f%%<BR>\n", island->perCpg);
	printf("<B>Percentage C or G:</B> %1.1f%%<BR>\n", island->perGc);
	if (islandExt != NULL)
	    printf("<B>Ratio of observed to expected CpG:</B> %1.2f<BR>\n",
		   islandExt->obsExp);
	printf("<BR>\n");
	cpgIslandFree(&island);
	}
    hFreeConn(&conn);
    }
else
    {
    puts("<P>Click directly on a CpG island for specific information on that island</P>");
    }
printTrackHtml(tdb);
}

void htcIlluminaProbesAlign(char *item)
/* If the click came from "show alignment" on the Illumina */
/* probes details page, show the standard alignment page. */
{
struct psl *psl;
struct dnaSeq *seq;
struct sqlResult *sr;
struct sqlConnection *conn = hAllocConn(database);
char query[256], **row;
int start;
char *pslTable = cgiUsualString("pslTable", "illuminaProbesAlign");
char *seqTable = cgiUsualString("seqTable", "illuminaProbesSeq");
char *probeName = item;
char *probeString;
int rowOffset = hOffsetPastBin(database, seqName, pslTable);

char title[1024];
safef(title, sizeof title, "Sequence %s", probeName);
htmlFramesetStart(title);
start = cartInt(cart, "o");
/* get psl */
sqlSafef(query, sizeof(query), "select * from %s where qName = '%s' and tName = '%s' and tStart=%d",
	pslTable, probeName, seqName, start);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find alignment for %s at %d", probeName, start);
psl = pslLoad(row+rowOffset);
sqlFreeResult(&sr);
sqlSafef(query, sizeof(query), "select seq from %s where id = '%s'", seqTable, probeName);
probeString = sqlNeedQuickString(conn, query);
seq = newDnaSeq(probeString, strlen(probeString), probeName);
hFreeConn(&conn);
showSomeAlignment(psl, seq, gftDna, 0, seq->size, probeName, 0, 0);
pslFree(&psl);
freeDnaSeq(&seq);
freeMem(probeString);
}

void doIlluminaProbes(struct trackDb *tdb, char *item)
/* The details page of the Illumina Probes track. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset = hOffsetPastBin(database, seqName, tdb->table);
char query[256];
int start = cartInt(cart, "o");
genericHeader(tdb, item);
sqlSafef(query, sizeof(query), "select * from %s where name = '%s' and chromStart = '%d'", tdb->table, item, start);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    struct bed *bed = bedLoad12(row+rowOffset);
    printf("<B>Probe ID:</B> %s<BR>\n", bed->name);
    printf("<B>Position:</B> "
	   "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
	   hgTracksPathAndSettings(), database, bed->chrom, bed->chromStart+1, bed->chromEnd);
    printf("%s:%d-%d</A><BR>\n", bed->chrom, bed->chromStart+1, bed->chromEnd);
    printf("<B>Alignment Score:</B> %d<BR>\n", bed->score);
    if ((bed->itemRgb == 1) || (bed->itemRgb == 2))
        /* The "show alignment" link. */
        {
        char other[256];
        char *pslTable = trackDbRequiredSetting(tdb, "pslTable");
	char *seqTable = trackDbRequiredSetting(tdb, "seqTable");
	safef(other, sizeof(other), "%d&pslTable=%s&seqTable=%s", bed->chromStart, pslTable, seqTable);
	hgcAnchor("htcIlluminaProbesAlign", item, other);
	printf("View Alignment</A><BR>\n");
	}
    bedFree(&bed);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
printTrackHtml(tdb);
}

void doSwitchDbTss(struct trackDb *tdb, char *item)
/* Print SwitchDB TSS details. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset = hOffsetPastBin(database, seqName, tdb->table);
char query[256];
genericHeader(tdb, item);
sqlSafef(query, sizeof(query), "select * from %s where name = '%s'", tdb->table, item);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    struct switchDbTss tss;
    switchDbTssStaticLoad(row+rowOffset, &tss);
    printPosOnChrom(tss.chrom, tss.chromStart, tss.chromEnd, tss.strand, FALSE, item);
    printf("<B>Gene Model:</B> %s<BR>\n", tss.gmName);
    printf("<B>Gene Model Position:</B> "
       "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
       hgTracksPathAndSettings(), database, tss.chrom, tss.gmChromStart+1, tss.gmChromEnd);
    printf("%s:%d-%d</A><BR>\n", tss.chrom, tss.gmChromStart+1, tss.gmChromEnd);
    printf("<B>TSS Confidence Score:</B> %.1f<BR>\n", tss.confScore);
    printf("<B>Pseudogene TSS: </B>%s<BR>\n", (tss.isPseudo == 1) ? "Yes" : "No");
    }
else
    {
    puts("<P>Click directly on a TSS for specific information on that TSS</P>");
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
printTrackHtml(tdb);
}

void printLines(FILE *f, char *s, int lineSize)
/* Print s, lineSize characters (or less) per line. */
{
int len = strlen(s);
int start;
int oneSize;

for (start = 0; start < len; start += lineSize)
    {
    oneSize = len - start;
    if (oneSize > lineSize)
        oneSize = lineSize;
    mustWrite(f, s+start, oneSize);
    fputc('\n', f);
    }
if (start != len)
    fputc('\n', f);
}

void showProteinPrediction(char *pepName, char *table)
/* Fetch and display protein prediction. */
{
/* checks both gbSeq and table */
aaSeq *seq = hGenBankGetPep(database, pepName, table);
if (seq == NULL)
    {
    warn("Predicted peptide %s is not avaliable", pepName);
    }
else
    {
    printf("<PRE><TT>");
    printf(">%s\n", pepName);
    printLines(stdout, seq->dna, 50);
    printf("</TT></PRE>");
    dnaSeqFree(&seq);
    }
}

boolean isGenieGeneName(char *name)
/* Return TRUE if name is in form to be a genie name. */
{
char *s, *e;
int prefixSize;

e = strchr(name, '.');
if (e == NULL)
    return FALSE;
prefixSize = e - name;
if (prefixSize > 3 || prefixSize == 0)
    return FALSE;
s = e+1;
if (!startsWith("ctg", s))
    return FALSE;
e = strchr(name, '-');
if (e == NULL)
    return FALSE;
return TRUE;
}

char *hugoToGenieName(char *hugoName, char *table)
/* Covert from hugo to genie name. */
{
struct sqlConnection *conn = hAllocConn(database);
char query[256];
static char buf[256], *name;

sqlSafef(query, sizeof query, "select transId from %s where name = '%s'", table, hugoName);
name = sqlQuickQuery(conn, query, buf, sizeof(buf));
hFreeConn(&conn);
if (name == NULL)
    errAbort("Database inconsistency: couldn't find gene name %s in knownInfo",
	     hugoName);
return name;
}

void displayProteinPrediction(char *pepName, char *pepSeq)
/* display a protein prediction. */
{
printf("<PRE><TT>");
printf(">%s length=%d\n", pepName,(int)strlen(pepSeq));
printLines(stdout, pepSeq, 50);
printf("</TT></PRE>");
}

void htcTranslatedProtein(char *pepName)
/* Display translated protein. */
{
char *table = cartString(cart, "o");
/* checks both gbSeq and table */
aaSeq *seq = hGenBankGetPep(database, pepName, table);
cartHtmlStart("Protein Translation");
if (seq == NULL)
    {
    warn("Predicted peptide %s is not avaliable", pepName);
    }
else
    {
    displayProteinPrediction(pepName, seq->dna);
    dnaSeqFree(&seq);
    }
}

static struct genePred *getGenePredForPositionSql(char *table, char *geneName)
/* find the genePred for the current gene using an SQL table*/
{
struct genePred *gpList = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
struct genePred *gp;
int rowOffset = hOffsetPastBin(database, seqName, table);

sqlSafef(query, sizeof(query), "select * from %s where name = \"%s\"", table, geneName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row+rowOffset);
    slAddHead(&gpList, gp);
    }

sqlFreeResult(&sr);
hFreeConn(&conn);
return gpList;
}

struct genePred *getGenePredForPositionBigGene(struct trackDb *tdb,  char *geneName)
/* Find the genePred to the current gene using a bigGenePred. */
{
char *fileName = hReplaceGbdb(trackDbSetting(tdb, "bigDataUrl"));
struct bbiFile *bbi =  bigBedFileOpenAlias(fileName, chromAliasFindAliases);
struct lm *lm = lmInit(0);
char *quickLiftFile = cloneString(trackDbSetting(tdb, "quickLiftUrl"));
struct bigBedInterval *bb, *bbList = NULL;
struct hash *chainHash = NULL;
if (quickLiftFile)
    bbList = quickLiftGetIntervals(quickLiftFile, bbi, seqName, winStart, winEnd, &chainHash);
else
    bbList = bigBedIntervalQuery(bbi, seqName, winStart, winEnd, 0, lm);
struct genePred *gpList = NULL;
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    struct genePred *gp = NULL;
    if (quickLiftFile)
        {
        struct bed *bed;
        if ((bed = quickLiftIntervalsToBed(bbi, chainHash, bb)) != NULL)
            {
            struct bed *bedCopy = cloneBed(bed);
            
            char startBuf[16], endBuf[16];
            char *bedRow[bbi->fieldCount];
            bigBedIntervalToRow(bb, seqName, startBuf, endBuf, bedRow, ArraySize(bedRow));

            // bedRow[5] has original strand in it, bedCopy has new strand.  If they're different we want to reverse exonFrames
            boolean changedStrand = FALSE;
            if (*bedRow[5] != *bedCopy->strand)
                changedStrand = TRUE;
            gp =(struct genePred *) genePredFromBedBigGenePred(seqName, bedCopy, bb, changedStrand);
            }
        }
    else
        gp = (struct genePred *)genePredFromBigGenePred(seqName, bb); 

    if ((gp != NULL) && sameString(gp->name, geneName))
	slAddHead(&gpList, gp);
    }
lmCleanup(&lm);

return gpList;
}

static struct genePred *getGenePredForPosition(char *table, char *geneName)
/* Build a genePred list for the given table and gene name. */
{
struct genePred *gpList = NULL;

if (isCustomTrack(table))
    {
    struct trackDb *tdb = getCustomTrackTdb(table);
    gpList = getGenePredForPositionBigGene(tdb,  geneName);
    }
else if (isHubTrack(table))
    {
    struct trackDb *tdb = hubConnectAddHubForTrackAndFindTdb( database, table, NULL, trackHash);
    gpList =  getGenePredForPositionBigGene(tdb, geneName);
    }
else
    {
    struct trackDb *tdb = hashFindVal(trackHash, table);
    char *bigDataUrl = trackDbSetting(tdb, "bigDataUrl");
    if (bigDataUrl)
        gpList =  getGenePredForPositionBigGene(tdb, geneName);
    else
        gpList =  getGenePredForPositionSql(table, geneName);
    }

return gpList;
}

void htcTranslatedPredMRna(char *geneName)
/* Translate virtual mRNA defined by genePred to protein and display it. */
{
char *table = cartString(cart, "table");
struct genePred *gp = NULL;
char protName[256];
char *prot = NULL;

cartHtmlStart("Protein Translation from Genome");
gp = getGenePredForPosition(table, geneName);

if (gp == NULL)
    errAbort("%s not found in %s when translating to protein",
             geneName, table);
else if (gp->cdsStart == gp->cdsEnd)
    errAbort("No CDS defined: no protein translation for %s", geneName);
prot = getPredMRnaProtSeq(gp);
safef(protName, sizeof(protName), "%s_prot", geneName);
displayProteinPrediction(protName, prot);

freez(&prot);
genePredFree(&gp);
}

void htcBigPsl( char *acc, boolean translate)
/* Output bigPsl query sequence, translated to amino acid sequence if requested. */
{
struct sqlConnection *conn = NULL;

if (!trackHubDatabase(database))
    conn = hAllocConn(database);

int start = cartInt(cart, "l");
int end = cartInt(cart, "r");
char *table = cartString(cart, "table");

struct trackDb *tdb ;
if (isCustomTrack(table))
    {
    struct customTrack *ct = lookupCt(table);
    tdb = ct->tdb;
    }
else if (isHubTrack(table))
    tdb = hubConnectAddHubForTrackAndFindTdb( database, table, NULL, trackHash);
else
    tdb = hashFindVal(trackHash, table);
char *fileName = bbiNameFromSettingOrTable(tdb, conn, tdb->table);
struct bbiFile *bbi =  bigBedFileOpenAlias(fileName, chromAliasFindAliases);
struct lm *lm = lmInit(0);
int ivStart = start, ivEnd = end;
if (start == end)
    {  
    // item is an insertion; expand the search range from 0 bases to 2 so we catch it:
    ivStart = max(0, start-1);
    ivEnd++;
    }  

unsigned seqTypeField =  bbExtraFieldIndex(bbi, "seqType");
struct bigBedInterval *bb, *bbList = NULL;

bbList = bigBedIntervalQuery(bbi, seqName, ivStart, ivEnd, 0, lm);

char *bedRow[32];
char startBuf[16], endBuf[16];

int lastChromId = -1;
char chromName[bbi->chromBpt->keySize+1];

for (bb = bbList; bb != NULL; bb = bb->next)
    {
    bbiCachedChromLookup(bbi, bb->chromId, lastChromId, chromName, sizeof(chromName));

    lastChromId=bb->chromId;
    bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, 4);

    // we're assuming that if there are multiple psl's with the same id that
    // they are the same query sequence so we only put out one sequence
    if (sameString(bedRow[3], acc))
	{
        char *cdsStr, *seq;
	getPslAndSeq(tdb, chromName, bb, seqTypeField, &seq, &cdsStr);

        if (translate)
            {
            struct genbankCds cds;
            if (!genbankCdsParse(cdsStr, &cds))
                errAbort("can't parse CDS for %s: %s", acc, cdsStr);
            int protBufSize = ((cds.end-cds.start)/3)+4;
            char *prot = needMem(protBufSize);

            seq[cds.end] = '\0';
            dnaTranslateSome(seq+cds.start, prot, protBufSize);

            cartHtmlStart("Protein Translation of query sequence");
            displayProteinPrediction(acc, prot);
            return;
            }
        else
            {
            cartHtmlStart("Query sequence");
            printf("<PRE><TT>");
            printf(">%s length=%d\n", acc,(int)strlen(seq));
            printLines(stdout, seq, 50);
            printf("</TT></PRE>");
            return;
            }
	}
    }
}

void htcTranslatedMRna(struct trackDb *tdb, char *acc)
/* Translate mRNA to protein and display it. */
{
struct sqlConnection *conn = hAllocConn(database);
struct genbankCds cds = getCds(conn, acc);
struct dnaSeq *mrna = hGenBankGetMrna(database, acc, NULL);
if (mrna == NULL)
    errAbort("mRNA sequence %s not found", acc);
if (cds.end > mrna->size)
    errAbort("CDS bounds exceed length of mRNA for %s", acc);

int protBufSize = ((cds.end-cds.start)/3)+4;
char *prot = needMem(protBufSize);

mrna->dna[cds.end] = '\0';
dnaTranslateSome(mrna->dna+cds.start, prot, protBufSize);

cartHtmlStart("Protein Translation of mRNA");
displayProteinPrediction(acc, prot);
}

void getCdsInMrna(struct genePred *gp, int *retCdsStart, int *retCdsEnd)
/* Given a gene prediction, figure out the
 * CDS start and end in mRNA coordinates. */
{
int missingStart = 0, missingEnd = 0;
int exonStart, exonEnd, exonSize, exonIx;
int totalSize = 0;

for (exonIx = 0; exonIx < gp->exonCount; ++exonIx)
    {
    exonStart = gp->exonStarts[exonIx];
    exonEnd = gp->exonEnds[exonIx];
    exonSize = exonEnd - exonStart;
    totalSize += exonSize;
    missingStart += exonSize - positiveRangeIntersection(exonStart, exonEnd, gp->cdsStart, exonEnd);
    missingEnd += exonSize - positiveRangeIntersection(exonStart, exonEnd, exonStart, gp->cdsEnd);
    }
*retCdsStart = missingStart;
*retCdsEnd = totalSize - missingEnd;
}

int genePredCdnaSize(struct genePred *gp)
/* Return total size of all exons. */
{
int totalSize = 0;
int exonIx;

for (exonIx = 0; exonIx < gp->exonCount; ++exonIx)
    {
    totalSize += (gp->exonEnds[exonIx] - gp->exonStarts[exonIx]);
    }
return totalSize;
}

struct dnaSeq *getCdnaSeq(struct genePred *gp)
/* Load in cDNA sequence associated with gene prediction. */
{
int txStart = gp->txStart;
struct dnaSeq *genoSeq = hDnaFromSeq(database, gp->chrom, txStart, gp->txEnd,  dnaLower);
struct dnaSeq *cdnaSeq;
int cdnaSize = genePredCdnaSize(gp);
int cdnaOffset = 0, exonStart, exonSize, exonIx;

AllocVar(cdnaSeq);
cdnaSeq->dna = needMem(cdnaSize+1);
cdnaSeq->size = cdnaSize;
for (exonIx = 0; exonIx < gp->exonCount; ++exonIx)
    {
    exonStart = gp->exonStarts[exonIx];
    exonSize = gp->exonEnds[exonIx] - exonStart;
    memcpy(cdnaSeq->dna + cdnaOffset, genoSeq->dna + (exonStart - txStart), exonSize);
    cdnaOffset += exonSize;
    }
assert(cdnaOffset == cdnaSeq->size);
freeDnaSeq(&genoSeq);
return cdnaSeq;
}

struct dnaSeq *getCdsSeq(struct genePred *gp)
/* Load in genomic CDS sequence associated with gene prediction. */
{
struct dnaSeq *genoSeq = hDnaFromSeq(database, gp->chrom, gp->cdsStart, gp->cdsEnd,  dnaLower);
struct dnaSeq *cdsSeq;
int cdsSize = genePredCodingBases(gp);
int cdsOffset = 0, exonStart, exonEnd, exonSize, exonIx;

AllocVar(cdsSeq);
cdsSeq->dna = needMem(cdsSize+1);
cdsSeq->size = cdsSize;
for (exonIx = 0; exonIx < gp->exonCount; ++exonIx)
    {
    genePredCdsExon(gp, exonIx, &exonStart, &exonEnd);
    exonSize = (exonEnd - exonStart);
    if (exonSize > 0)
        {
        memcpy(cdsSeq->dna + cdsOffset, genoSeq->dna + (exonStart - gp->cdsStart), exonSize);
        cdsOffset += exonSize;
        }
    }
assert(cdsOffset == cdsSeq->size);
freeDnaSeq(&genoSeq);
if (gp->strand[0] == '-')
    reverseComplement(cdsSeq->dna, cdsSeq->size);
return cdsSeq;
}

char *getPredMRnaProtSeq(struct genePred *gp)
/* Get the predicted mRNA from the genome and translate it to a
 * protein. free returned string. */
{
struct dnaSeq *cdsDna = getCdsSeq(gp);
int protBufSize = (cdsDna->size/3)+4;
char *prot = needMem(protBufSize);
int offset = 0;

/* get frame offset, if available and needed */
if (gp->exonFrames != NULL)
{
    if (gp->strand[0] == '+' && gp->cdsStartStat != cdsComplete)
        offset = (3 - gp->exonFrames[0]) % 3;
    else if (gp->strand[0] == '-' && gp->cdsEndStat != cdsComplete)
        offset = (3 - gp->exonFrames[gp->exonCount-1]) % 3;
}
/* NOTE: this fix will not handle the case in which frame is shifted
 * internally or at multiple exons, as when frame-shift gaps occur in
 * an alignment of an mRNA to the genome.  Going to have to come back
 * and address that later... (acs) */

dnaTranslateSome(cdsDna->dna+offset, prot, protBufSize);
dnaSeqFree(&cdsDna);
return prot;
}



void ncbiRefSeqSequence(char *itemName)
{
char *table = cartString(cart, "o");
struct dnaSeq *rnaSeq = getBaseColorSequence(itemName, table );
cartHtmlStart("RefSeq mRNA Sequence");

printf("<PRE><TT>");
printf(">%s\n", itemName);
faWriteNext(stdout, NULL, rnaSeq->dna, rnaSeq->size);
printf("</TT></PRE>");
}


void htcGeneMrna(char *geneName)
/* Display cDNA predicted from genome */
{
char *table = cartString(cart, "o");
cartHtmlStart("Predicted mRNA from Genome");
struct genePred *gp, *gpList = getGenePredForPosition(table, geneName), *next;
int cdsStart, cdsEnd;
struct dnaSeq *seq;

for(gp = gpList; gp; gp = next)
    {
    next = gp->next;
    seq = getCdnaSeq(gp);
    getCdsInMrna(gp, &cdsStart, &cdsEnd);
    toUpperN(seq->dna + cdsStart, cdsEnd - cdsStart);
    if (gp->strand[0] == '-')
	{
        reverseComplement(seq->dna, seq->size);
	}
    printf("<PRE><TT>");
    printf(">%s\n", geneName);
    faWriteNext(stdout, NULL, seq->dna, seq->size);
    printf("</TT></PRE>");
    genePredFree(&gp);
    freeDnaSeq(&seq);
    }
}

void htcRefMrna(char *geneName)
/* Display mRNA associated with a refSeq gene. */
{
/* check both gbSeq and refMrna */
struct dnaSeq *seq = hGenBankGetMrna(database, geneName, "refMrna");
if (seq == NULL)
    errAbort("RefSeq mRNA sequence %s not found", geneName);

cartHtmlStart("RefSeq mRNA");
printf("<PRE><TT>");
faWriteNext(stdout, seq->name, seq->dna, seq->size);
printf("</TT></PRE>");
dnaSeqFree(&seq);
}

void cartContinueRadio(char *var, char *val, char *defaultVal)
/* Put up radio button, checking it if it matches val */
{
char *oldVal = cartUsualString(cart, var, defaultVal);
cgiMakeRadioButton(var, val, sameString(oldVal, val));
}

void htcGeneInGenome(char *geneName)
/* Put up page that lets user display genomic sequence
 * associated with gene. */
{
char *tbl = cgiString("o");

cartWebStart(cart, database, "Genomic Sequence Near Gene");
printf("<H2>Get Genomic Sequence Near Gene</H2>");

puts("<P>"
     "Note: if you would prefer to get DNA for more than one feature of "
     "this track at a time, try the ");
printf("<A HREF=\"%s\" TARGET=_blank>", hgTablesUrl(FALSE, tbl));
puts("Table Browser</A> using the output format sequence.");

printf("<FORM ACTION=\"%s\">\n\n", hgcName());
cartSaveSession(cart);
cgiMakeHiddenVar("g", "htcDnaNearGene");
cgiContinueHiddenVar("i");
printf("\n");
cgiContinueHiddenVar("db");
printf("\n");

cgiContinueHiddenVar("c");
printf("\n");
cgiContinueHiddenVar("l");
printf("\n");
cgiContinueHiddenVar("r");
printf("\n");
cgiContinueHiddenVar("o");
printf("\n");

if (isCustomTrack(tbl) || startsWith("hub_", tbl))
    hgSeqOptions(cart, database, tbl);
else
    {
    struct trackDb *tdb = hashFindVal(trackHash, tbl);
    char *bigDataUrl = trackDbSetting(tdb, "bigDataUrl");
    if (bigDataUrl)
        {
        // we asssume that this is a bigGenePred table if we got here with it
        hgSeqFeatureRegionOptions(cart, TRUE, TRUE);
        hgSeqDisplayOptions(cart, TRUE, TRUE, FALSE);
        }
    else
        hgSeqOptions(cart, database, tbl);
    }

cgiMakeButton("submit", "submit");
printf("</FORM>");
}

void htcGeneAlignment(char *geneName)
/* Put up page that lets user display genomic sequence
 * associated with gene. */
{
cartWebStart(cart, database, "Aligned Annotated Genomic Sequence ");
printf("<H2>Align a gene prediction to another species or the same species and view codons and translated proteins.</H2>");
printf("<FORM ACTION=\"%s\">\n\n", hgcName());
cartSaveSession(cart);
cgiMakeHiddenVar("g", "htcDnaNearGene");
cgiContinueHiddenVar("i");
printf("\n");
cgiContinueHiddenVar("db");
printf("\n");
cgiContinueHiddenVar("c");
printf("\n");
cgiContinueHiddenVar("l");
printf("\n");
cgiContinueHiddenVar("r");
printf("\n");
cgiContinueHiddenVar("o");
printf("\n");
hgSeqOptions(cart, database, cgiString("o"));
cgiMakeButton("submit", "submit");
printf("</FORM>");
}

void toUpperExons(int startOffset, struct dnaSeq *seq, struct genePred *gp)
/* Upper case bits of DNA sequence that are exons according to gp. */
{
int s, e, size;
int exonIx;
int seqStart = startOffset, seqEnd = startOffset + seq->size;

if (seqStart < gp->txStart)
    seqStart = gp->txStart;
if (seqEnd > gp->txEnd)
    seqEnd = gp->txEnd;

for (exonIx = 0; exonIx < gp->exonCount; ++exonIx)
    {
    s = gp->exonStarts[exonIx];
    e = gp->exonEnds[exonIx];
    if (s < seqStart) s = seqStart;
    if (e > seqEnd) e = seqEnd;
    if ((size = e - s) > 0)
	{
	s -= startOffset;
	if (s < 0 ||  s + size > seq->size)
	    errAbort("Out of range! %d-%d not in %d-%d", s, s+size, 0, size);
	toUpperN(seq->dna + s, size);
	}
    }
}


static struct bed *getBedsFromBigBedRange(struct trackDb *tdb, char *geneName)
/* get a list of beds from a bigBed in the current range */
{
struct bbiFile *bbi;
char *fileName = hReplaceGbdb(trackDbSetting(tdb, "bigDataUrl"));
bbi =  bigBedFileOpenAlias(fileName, chromAliasFindAliases);
struct lm *lm = lmInit(0);
char *quickLiftFile = cloneString(trackDbSetting(tdb, "quickLiftUrl"));
struct hash *chainHash = NULL;
struct bigBedInterval *bb, *bbList = NULL;
if (quickLiftFile)
    bbList = quickLiftGetIntervals(quickLiftFile, bbi, seqName, winStart, winEnd, &chainHash);
else
    bbList = bigBedIntervalQuery(bbi, seqName, winStart, winEnd, 0, lm);

struct bed *bedList = NULL;
char *bedRow[32];
char startBuf[16], endBuf[16];
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    bigBedIntervalToRow(bb, seqName, startBuf, endBuf, bedRow, ArraySize(bedRow));
    struct bed *bed = NULL;
    if (quickLiftFile)
        {
        if ((bed = quickLiftIntervalsToBed(bbi, chainHash, bb)) == NULL)
            errAbort("can't port %s",bedRow[3]);
        }
    else
        bed = bedLoadN(bedRow, 12);
    if (sameString(bed->name, geneName))
	slAddHead(&bedList, bed);
    }
lmCleanup(&lm);

return bedList;
}

static int getSeqForBigGene(struct trackDb *tdb, char *geneName)
/* Output sequence for a gene in a bigGenePred file. */
{
struct hTableInfo *hti;
AllocVar(hti);
hti->hasCDS = TRUE;
hti->hasBlocks = TRUE;
hti->rootName = tdb->table;

struct bed *bedList = getBedsFromBigBedRange(tdb, geneName);
int itemCount = hgSeqBed(database, hti, bedList);
freez(&hti);
bedFreeList(&bedList);
return itemCount;
}

void htcDnaNearGene( char *geneName)
/* Fetch DNA near a gene. */
{
cartWebStart(cart, database, "%s", geneName);
char *table    = cartString(cart, "o");
int itemCount;
puts("<PRE>");
struct trackDb *tdb = NULL;

if (isHubTrack(table))
    {
    tdb = hubConnectAddHubForTrackAndFindTdb( database, table, NULL, trackHash);
    itemCount = getSeqForBigGene(tdb, geneName);
    }
else if (isCustomTrack(table))
    {
    tdb = getCustomTrackTdb(table);
    itemCount = getSeqForBigGene(tdb, geneName);
    }
else
    {
    tdb = hashFindVal(trackHash, table);
    char *bigDataUrl = trackDbSetting(tdb, "bigDataUrl");
    if (bigDataUrl)
        {
        itemCount = getSeqForBigGene(tdb, geneName);
        }
    else
        {
        char constraints[256];
        sqlSafef(constraints, sizeof(constraints), "name = '%s'", geneName);
        itemCount = hgSeqItemsInRange(database, table, seqName, winStart, winEnd, constraints);
        }
    }
if (itemCount == 0)
    printf("\n# No results returned from query.\n\n");
puts("</PRE>");
}

void htcTrackHtml(struct trackDb *tdb)
/* Handle click to display track html */
{
cartWebStart(cart, database, "%s", tdb->shortLabel);
printTrackHtml(tdb);
}

void doViralProt(struct trackDb *tdb, char *geneName)
/* Handle click on known viral protein track. */
{
struct sqlConnection *conn = hAllocConn(database);
int start = cartInt(cart, "o");
struct psl *pslList = NULL;

cartWebStart(cart, database, "Viral Gene");
printf("<H2>Viral Gene %s</H2>\n", geneName);
printCustomUrl(tdb, geneName, TRUE);

pslList = getAlignments(conn, "chr1_viralProt", geneName);
htmlHorizontalLine();
printf("<H3>Protein Alignments</H3>");
printAlignments(pslList, start, "htcProteinAli", "chr1_viralProt", geneName);
printTrackHtml(tdb);
}

static boolean pslTrimListToTargetRange(struct psl *pslList, int winStart, int winEnd,
                                        int *retQRangeStart, int *retQRangeEnd)
/* If the current window overlaps the target coords of any psl(s) in pslList, then return TRUE and
 * set *retQRange{Start,End} to span all query coord ranges corresponding to target coord ranges.
 * errAbort if qName is not consistent across all psls.
 * Otherwise return FALSE and set *retQRange{Start,End} to -1. */
{
boolean foundOverlap = FALSE;
char *qName = NULL;
int qRangeStart = -1, qRangeEnd = -1;
struct psl *psl;
for (psl = pslList;  psl != NULL;  psl = psl->next)
    {
    if (qName == NULL)
        qName = psl->qName;
    else if (differentString(qName, psl->qName))
        errAbort("pslTrimListToTargetRange: inconsistent qName: got both '%s' and '%s'",
                 qName, psl->qName);
    if (psl->tStart >= winStart && psl->tEnd <= winEnd)
        {
        foundOverlap = TRUE;
        if (qRangeStart < 0 || psl->qStart < qRangeStart)
            qRangeStart = psl->qStart;
        if (qRangeEnd < 0 || psl->qEnd > qRangeEnd)
            qRangeEnd = psl->qEnd;
        }
    else
        {
        struct psl *partPsl = pslTrimToTargetRange(psl, winStart, winEnd);
        if (partPsl)
            {
            foundOverlap = TRUE;
            if (qRangeStart < 0 || partPsl->qStart < qRangeStart)
                qRangeStart = partPsl->qStart;
            if (qRangeEnd < 0 || partPsl->qEnd > qRangeEnd)
                qRangeEnd = partPsl->qEnd;
            }
        }
    }
*retQRangeStart = qRangeStart;
*retQRangeEnd = qRangeEnd;
return foundOverlap;
}

void doPslAltSeq(struct trackDb *tdb, char *item)
/* Details for alignments between chromosomes and alt haplogtype or fix patch sequences. */
{
char *chrom = cartString(cart, "c");
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);

genericHeader(tdb, item);
printCustomUrl(tdb, item, TRUE);

puts("<P>");
struct psl *pslList = getAlignmentsTName(conn, tdb->table, item, chrom);
if (pslList)
    {
    printf("<B>Alignment of %s to %s:</B><BR>\n", item, pslList->tName);
    printAlignments(pslList, start, "htcCdnaAli", tdb->table, item);

    char *hgsid = cartSessionId(cart);
    if (hgIsOfficialChromName(database, item))
        {
        int rangeStart = 0, rangeEnd = 0;
        if (pslTrimListToTargetRange(pslList, winStart, winEnd, &rangeStart, &rangeEnd))
            {
            printf("<A HREF='hgTracks?hgsid=%s&position=%s:%d-%d'>"
                   "View corresponding position range on %s</A><BR>\n",
                   hgsid, item, rangeStart+1, rangeEnd, item);
            }
        }
    char *altFix = item;
    if (!endsWith(altFix, "alt") && !endsWith(altFix, "fix"))
        altFix = pslList->tName;
    if (hgIsOfficialChromName(database, altFix))
        printf("<A HREF=\"hgTracks?hgsid=%s&virtModeType=singleAltHaplo&singleAltHaploId=%s\">"
               "Show %s placed on its chromosome</A><BR>\n",
               hgsid, altFix, altFix);

    puts("<P><B>Alignment stats:</B><BR>");
    // Sometimes inversions cause alignments to be split up; just sum up all the stats.
    int totalBlocks = 0, totalSize = 0, totalMatch = 0, totalMismatch = 0, totalN = 0;
    int totalTIns = 0, totalQIns = 0;
    struct psl *psl;
    for (psl = pslList;  psl != NULL;  psl = psl->next)
        {
        totalBlocks += psl->blockCount;
        int i;
        for (i=0;  i < psl->blockCount;  i++)
            totalSize += psl->blockSizes[i];
        totalMatch += (psl->match + psl->repMatch);
        totalMismatch += psl->misMatch;
        totalN += psl->nCount;
        totalTIns += psl->tBaseInsert;
        totalQIns += psl->qBaseInsert;
        }
    printf("%d block(s) covering %d bases<BR>\n"
           "%d matching bases (%.2f%%)<BR>\n"
           "%d mismatching bases (%.2f%%)<BR>\n"
           "%d N bases(%.2f%%)<BR>\n"
           "%d bases inserted in %s<BR>\n"
           "%d bases inserted in %s<BR>\n",
           totalBlocks, totalSize,
           totalMatch, 100.0 * (totalMatch / (double)totalSize),
           totalMismatch, 100.0 * (totalMismatch / (double)totalSize),
           totalN, 100.0 * (totalN / (double)totalSize),
           totalTIns, pslList->tName, totalQIns, item);
    }
else
    warn("Unable to find alignment for '%s' in table %s", item, tdb->table);
printTrackHtml(tdb);
hFreeConn(&conn);
}

void doPslDetailed(struct trackDb *tdb, char *item)
/* Fairly generic PSL handler -- print out some more details about the
 * alignment. */
{
int start = cartInt(cart, "o");
int total = 0, i = 0;
struct psl *pslList = NULL;
struct sqlConnection *conn = hAllocConn(database);

genericHeader(tdb, item);
printCustomUrl(tdb, item, TRUE);

puts("<P>");
puts("<B>Alignment Summary:</B><BR>\n");
pslList = getAlignments(conn, tdb->table, item);
printAlignments(pslList, start, "htcCdnaAli", tdb->table, item);

puts("<P>");
total = 0;
for (i=0;  i < pslList -> blockCount;  i++)
    {
    total += pslList->blockSizes[i];
    }
printf("%d block(s) covering %d bases<BR>\n"
       "%d matching bases<BR>\n"
       "%d mismatching bases<BR>\n"
       "%d N bases<BR>\n"
       "%d bases inserted in %s<BR>\n"
       "%d bases inserted in %s<BR>\n"
       "score: %d<BR>\n",
       pslList->blockCount, total,
       pslList->match + pslList->repMatch,
       pslList->misMatch,
       pslList->nCount,
       pslList->tBaseInsert, hOrganism(database),
       pslList->qBaseInsert, item,
       pslScore(pslList));

printTrackHtml(tdb);
hFreeConn(&conn);
}

void printEnsemblCustomUrl(struct trackDb *tdb, char *itemName, boolean encode,
    char *archive)
/* Print Ensembl Gene URL. */
{
char *shortItemName;
// char *genomeStr = ""; unused variable
char *genomeStrEnsembl = "";
struct sqlConnection *conn = hAllocConn(database);
char cond_str[256], cond_str2[256], cond_str3[256];
char *proteinID = NULL;
char *ensPep;
char *chp;
char ensUrl[256];
char *ensemblIdUrl = trackDbSettingOrDefault(tdb, "ensemblIdUrl", "http://www.ensembl.org");

/* shortItemName is the name without the "." + version */
shortItemName = cloneString(itemName);
/* ensembl gene names are different from their usual naming scheme on ce6/ce11*/
if (! (startsWith("ce6", database) || startsWith("ce11", database)))
    {
    chp = strstr(shortItemName, ".");
    if (chp != NULL)
	*chp = '\0';
    }
genomeStrEnsembl = ensOrgNameFromScientificName(scientificName);
if (genomeStrEnsembl == NULL)
    {
    warn("Organism %s not found!", organism); fflush(stdout);
    return;
    }

/* print URL that links to Ensembl transcript details */
if (sameString(ensemblIdUrl, "http://www.ensembl.org") && archive != NULL)
    safef(ensUrl, sizeof(ensUrl), "http://%s.archive.ensembl.org/%s",
            archive, genomeStrEnsembl);
else
    {
    /* trackDb ensemblIdUrl might be more than just top level URL,
     * simply take it as given, e.g. criGriChoV1
     */
    if (countChars(ensemblIdUrl, '/') > 2)
      safef(ensUrl, sizeof(ensUrl), "%s", ensemblIdUrl);
    else
      safef(ensUrl, sizeof(ensUrl), "%s/%s", ensemblIdUrl, genomeStrEnsembl);
    }

char query[512];
char *geneName = NULL;
if (hTableExists(database, "ensemblToGeneName"))
    {
    sqlSafef(query, sizeof(query), "select value from ensemblToGeneName where name='%s'", itemName);
    geneName = sqlQuickString(conn, query);
    }
char *ensemblSource = NULL;
if (hTableExists(database, "ensemblSource"))
    {
    sqlSafef(query, sizeof(query), "select source from ensemblSource where name='%s'", itemName);
    ensemblSource = sqlQuickString(conn, query);
    }

sqlSafef(query, sizeof(query), "name = \"%s\"", itemName);
struct genePred *gpList = genePredReaderLoadQuery(conn, "ensGene", query);
if (gpList && gpList->name2)
    {
    printf("<B>Ensembl Gene Link: </B>");
    if ((strlen(gpList->name2) < 1) || sameString(gpList->name2, "noXref"))
       printf("none<BR>\n");
    else
       {
       printf("<A HREF=\"%s/geneview?gene=%s\" "
	    "target=_blank>%s</A><BR>", ensUrl, gpList->name2, gpList->name2);
       if (! (ensemblSource && differentString("protein_coding",ensemblSource)))
          {
          printf("<B>Ensembl Gene Tree: </B>");
          printf("<A HREF=\"%s/Gene/Compara_Tree?g=%s&t=%s\" "
             "target=_blank>%s</A><br>", ensUrl, gpList->name2, shortItemName, gpList->name2);
          }
       }
    }
genePredFreeList(&gpList);

printf("<B>Ensembl Transcript: </B>");
printf("<A HREF=\"%s/transview?transcript=%s\" "
               "target=_blank>", ensUrl, shortItemName);
printf("%s</A><br>", itemName);

if (hTableExists(database, "superfamily"))
    {
    sqlSafef(cond_str, sizeof(cond_str), "transcript_name='%s'", shortItemName);

    /* This is necessary, Ensembl kept changing their gene_xref table definition and content.*/
    proteinID = NULL;

    if (hTableExists(database, "ensemblXref3"))
        {
        /* use ensemblXref3 for Ensembl data release after ensembl34d */
        sqlSafef(cond_str3, sizeof(cond_str3), "transcript='%s'", shortItemName);
        ensPep = sqlGetField(database, "ensemblXref3", "protein", cond_str3);
	if (ensPep != NULL) proteinID = ensPep;
	}

    if (hTableExists(database, "ensTranscript") && (proteinID == NULL))
        {
        proteinID = sqlGetField(database, "ensTranscript", "translation_name", cond_str);
        }
    else
        {
        if (hTableExists(database, "ensGeneXref"))
            {
	    proteinID = sqlGetField(database, "ensGeneXref","translation_name", cond_str);
            }
        else if (hTableExists(database, "ensemblXref2"))
            {
            proteinID = sqlGetField(database, "ensemblXref2","translation_name", cond_str);
            }
        else
            {
            if (hTableExists(database, "ensemblXref"))
                {
                proteinID=sqlGetField(database, "ensemblXref","translation_name",cond_str);
                }
            }
        }
    if (proteinID != NULL)
        {
        printf("<B>Ensembl Protein: </B>");
        printf("<A HREF=\"%s/protview?peptide=%s\" target=_blank>",
            ensUrl, proteinID);
        printf("%s</A><BR>\n", proteinID);
        }

#ifdef NOT
    /* get genomeStr to be used in Superfamily URL */
    if (sameWord(organism, "human"))
        {
	genomeStr = "hs";
	}
    else
	{
        if (sameWord(organism, "mouse"))
	    {
	    genomeStr = "mm";
	    }
        else
            {
	    if (sameWord(organism, "rat"))
                {
                genomeStr = "rn";
                }
            else
                {
                if (sameWord(organism, "dog"))
                    {
                    genomeStr = "dg";
                    }
                else
                    {
                    warn("Organism %s not found!", organism);
                    return;
                    }
                }
            }
        }
/* superfamily does not update with ensGene updates, stop printing an
	invalid URL */
    sqlSafef(cond_str, "name='%s'", shortItemName);
    char *ans = sqlGetField(conn, database, "superfamily", "name", cond_str);
    if (ans != NULL)
	{
	/* double check to make sure trackDb is also updated to be in sync with existence of supfamily table */
	struct trackDb *tdbSf = hashFindVal(trackHash, "superfamily");
        if (tdbSf != NULL)
	    {
            char supfamURL[512];
            printf("<B>Superfamily Link: </B>");
            safef(supfamURL, sizeof(supfamURL), "<A HREF=\"%s%s;seqid=%s\" target=_blank>",
                      tdbSf->url, genomeStr, proteinID);
            printf("%s%s</A><BR>\n", supfamURL, proteinID);
            }
        }
#endif
    }
if (hTableExists(database, "ensGtp") && (proteinID == NULL))
    {
    /* shortItemName removes version number but sometimes the ensGtp */
    /* table has a transcript with version number so exact match not used */
    sqlSafef(cond_str2, sizeof(cond_str2), "transcript like '%s%%'", shortItemName);
    proteinID=sqlGetField(database, "ensGtp","protein",cond_str2);
    if (proteinID != NULL)
        {
	printf("<B>Ensembl Protein: </B>");
	printf("<A HREF=\"%s/protview?peptide=%s\" target=_blank>",
	    ensUrl,proteinID);
	printf("%s</A><BR>\n", proteinID);
	}
    else
	{
	printf("<B>Ensembl Protein: </B>none (non-coding)<BR>\n");
	}
    }
if (geneName)
    {
    printf("<B>Gene Name: </B>%s<BR>\n", geneName);
    freeMem(geneName);
    }
if (ensemblSource)
    {
    printf("<B>Ensembl Type: </B>%s<BR>\n", ensemblSource);
    freeMem(ensemblSource);
    }
freeMem(shortItemName);
}

void printEnsemblOrVegaCustomUrl(struct trackDb *tdb, char *itemName, boolean encode, char *archive)
/* Print Ensembl Gene URL. */
{
boolean isEnsembl = FALSE;
boolean isVega = FALSE;
boolean hasEnsGtp = FALSE;
boolean hasVegaGtp = FALSE;
char *shortItemName;
char *genomeStrEnsembl = "";
struct sqlConnection *conn = hAllocConn(database);
char cond_str[256], cond_str2[256];
char *geneID = NULL;
char *proteinID = NULL;
char *chp;
char dbUrl[256];
char geneType[256];
char gtpTable[256];

if (startsWith("ens", tdb->table))
   {
   isEnsembl = TRUE;
   safef(geneType, sizeof(geneType), "Ensembl");
   safef(gtpTable, sizeof(gtpTable), "ensGtp");
   if (hTableExists(database, gtpTable))
      hasEnsGtp = TRUE;
   }
else if (startsWith("vega", tdb->table))
   {
   isVega = TRUE;
   safef(geneType, sizeof(geneType), "Vega");
   safef(gtpTable, sizeof(gtpTable), "vegaGtp");
   if (hTableExists(database, gtpTable))
      hasVegaGtp = TRUE;
   }
/* shortItemName is the name without the "." + version */
shortItemName = cloneString(itemName);
/* ensembl gene names are different from their usual naming scheme on ce6/ce11*/
if (! (startsWith("ce6", database) || startsWith("ce11", database)) )
    {
    chp = strstr(shortItemName, ".");
    if (chp != NULL)
	*chp = '\0';
    }
genomeStrEnsembl = ensOrgNameFromScientificName(scientificName);
if (genomeStrEnsembl == NULL)
    {
    warn("Organism %s not found!", organism); fflush(stdout);
    return;
    }

/* print URL that links to Ensembl or Vega transcript details */
if (isEnsembl)
    {
    if (archive != NULL)
       safef(dbUrl, sizeof(dbUrl), "http://%s.archive.ensembl.org/%s",
            archive, genomeStrEnsembl);
    else
        safef(dbUrl, sizeof(dbUrl), "http://www.ensembl.org/%s", genomeStrEnsembl);
    }
else if (isVega)
    safef(dbUrl, sizeof(dbUrl), "http://vega.sanger.ac.uk/%s", genomeStrEnsembl);

boolean nonCoding = FALSE;
char query[512];
sqlSafef(query, sizeof(query), "name = \"%s\"", itemName);
struct genePred *gpList = genePredReaderLoadQuery(conn, tdb->table, query);
if (gpList && (gpList->cdsStart == gpList->cdsEnd))
    nonCoding = TRUE;
genePredFreeList(&gpList);
/* get gene and protein IDs */
if ((isEnsembl && hasEnsGtp) || (isVega && hasVegaGtp))
    {
    /* shortItemName removes version number but sometimes the ensGtp */
    /* table has a transcript with version number so exact match not used */
    sqlSafef(cond_str, sizeof(cond_str), "transcript like '%s%%'", shortItemName);
    geneID=sqlGetField(database, gtpTable,"gene",cond_str);
    sqlSafef(cond_str2, sizeof(cond_str2), "transcript like '%s%%'", shortItemName);
    proteinID=sqlGetField(database, gtpTable,"protein",cond_str2);
    }

/* Print gene, transcript and protein links */
if (geneID != NULL)
    {
    printf("<B>%s Gene: </B>", geneType);
    printf("<A HREF=\"%s/geneview?gene=%s\" "
	    "target=_blank>%s</A><BR>", dbUrl, geneID, geneID);
    }
printf("<B>%s Transcript: </B>", geneType);
printf("<A HREF=\"%s/transview?transcript=%s\" "
           "target=_blank>%s</A><BR>", dbUrl, shortItemName, itemName);
if (proteinID != NULL)
    {
    printf("<B>%s Protein: </B>", geneType);
    if (nonCoding)
        printf("none (non-coding)<BR>\n");
    else
        printf("<A HREF=\"%s/protview?peptide=%s\" "
	      "target=_blank>%s</A><BR>", dbUrl, proteinID, proteinID);
    }
freeMem(shortItemName);
}

void doEnsemblGene(struct trackDb *tdb, char *item, char *itemForUrl)
/* Put up Ensembl Gene track info or Ensembl NonCoding track info. */
{
char *dupe, *type, *words[16];
int wordCount;
int start = cartInt(cart, "o");
char condStr[256];
char headerTitle[512];

if (itemForUrl == NULL)
    itemForUrl = item;
dupe = cloneString(tdb->type);

struct trackVersion *trackVersion = getTrackVersion(database, tdb->track);
if ((trackVersion != NULL) && !isEmpty(trackVersion->version))
    safef(headerTitle, sizeof(headerTitle), "%s - Ensembl %s", item, trackVersion->version);
else
    safef(headerTitle, sizeof(headerTitle), "%s", item);

genericHeader(tdb, headerTitle);
wordCount = chopLine(dupe, words);
char *archive = trackDbSetting(tdb, "ensArchive");
if (archive == NULL)
    {
    if ((trackVersion != NULL) && !isEmpty(trackVersion->dateReference))
	{
	if (differentWord("current", trackVersion->dateReference))
	    archive = cloneString(trackVersion->dateReference);
	}
    }
printEnsemblCustomUrl(tdb, itemForUrl, item == itemForUrl, archive);
sqlSafef(condStr, sizeof condStr, "name='%s'", item);

struct sqlConnection *conn = hAllocConn(database);

/* if this is a non-coding gene track, then print the biotype and
   the external ID */
if (sameWord(tdb->table, "ensGeneNonCoding"))
    {
    char query[256];
    struct sqlResult *sr = NULL;
    char **row;
    sqlSafef(query, sizeof(query), "select biotype, extGeneId from %s where %s",
          tdb->table, condStr);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
        printf("<B>Gene Type:</B> %s<BR>\n", row[0]);
        printf("<B>External Gene ID:</B> %s<BR>\n", row[1]);
        }
    sqlFreeResult(&sr);
    }
else
    {
    /* print CCDS if this is not a non-coding gene */
    printCcdsForSrcDb(conn, item);
    printf("<BR>\n");
    }

if (hTableExists(database, "ensInfo"))
    {
    struct sqlResult *sr;
    char query[256], **row;
    struct ensInfo *info = NULL;

    sqlSafef(query, sizeof(query),
          "select * from ensInfo where name = '%s'", item);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        info = ensInfoLoad(row);
        /* no need to print otherId field, this is the same as name 2 in
           the ensGene table and it is printed by showGenePos() */
        /* convert the status to lower case */
        tolowers(info->status);
        printf("<B>Ensembl Gene Type:</B> %s %s<BR>\n", info->status,
                info->class);
        printf("<B>Ensembl Gene:</B> %s<BR>\n", info->geneId);
        printf("<B>Ensembl Gene Description:</B> %s<BR>\n", info->geneDesc);
        ensInfoFree(&info);
        }
    sqlFreeResult(&sr);
    }

/* skip the rest if this gene is not in ensGene */
sqlSafef(condStr, sizeof condStr, "name='%s'", item);
if (sqlGetField(database, tdb->table, "name", condStr) != NULL)
    {
    if (wordCount > 0)
        {
        type = words[0];
        if (sameString(type, "genePred"))
            {
	    char *pepTable = NULL, *mrnaTable = NULL;
	    if (wordCount > 1)
                pepTable = words[1];
	    if (wordCount > 2)
                mrnaTable = words[2];
	    genericGenePredClick(conn, tdb, item, start, pepTable, mrnaTable);
	    }
        }
    }
printTrackHtml(tdb);
freez(&dupe);
hFreeConn(&conn);
}

void printSuperfamilyCustomUrl(struct trackDb *tdb, char *itemName, boolean encode)
/* Print Superfamily URL. */
{
char *url = tdb->url;
if (url != NULL && url[0] != 0)
    {
    char supfamURL[1024];
    char *genomeStr;
    struct sqlConnection *conn = hAllocConn(database);
    char query[256];
    struct sqlResult *sr;
    char **row;

    printf("The corresponding protein %s has the following Superfamily domain(s):", itemName);
    printf("<UL>\n");

    sqlSafef(query, sizeof query,
            "select description from sfDescription where proteinID='%s';",
            itemName);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    while (row != NULL)
        {
        printf("<li>%s", row[0]);
        row = sqlNextRow(sr);
        }
    sqlFreeResult(&sr);
    hFreeConn(&conn);

    printf("</UL>");

    if (sameWord(organism, "human"))
        {
        genomeStr = "hs";
	}
    else
	{
        if (sameWord(organism, "mouse"))
	    {
	    genomeStr = "mm";
	    }
	else
	    {
	    if (sameWord(organism, "rat"))
                {
                genomeStr = "rn";
                }
            else
                {
                warn("Organism %s not found!", organism);
                return;
		}
	    }
	}

    printf("<B>Superfamily Link: </B>");
    safef(supfamURL, sizeof supfamURL, "<A HREF=\"%s%s;seqid=%s\" target=_blank>",
	    url, genomeStr, itemName);
    printf("%s%s</A><BR><BR>\n", supfamURL, itemName);
    }
}

void doSuperfamily(struct trackDb *tdb, char *item, char *itemForUrl)
/* Put up Superfamily track info. */
{
struct sqlConnection *conn = hAllocConn(database);
char query[256];
struct sqlResult *sr;
char **row;
char *chrom, *chromStart, *chromEnd;
char *transcript;

if (itemForUrl == NULL)
    itemForUrl = item;

genericHeader(tdb, item);

printSuperfamilyCustomUrl(tdb, itemForUrl, item == itemForUrl);
if (hTableExists(database, "ensGeneXref"))
    {
    sqlSafef(query, sizeof query, "translation_name='%s'", item);
    transcript = sqlGetField(database, "ensGeneXref", "transcript_name", query);

    sqlSafef(query, sizeof query,
            "select chrom, chromStart, chromEnd from superfamily where name='%s';", transcript);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
        chrom      = row[0];
        chromStart = row[1];
        chromEnd   = row[2];
        printf("<HR>");
        printPosOnChrom(chrom, atoi(chromStart), atoi(chromEnd), NULL, TRUE, transcript);
        }
    sqlFreeResult(&sr);
    }
if (hTableExists(database, "ensemblXref3"))
    {
    sqlSafef(query, sizeof query, "protein='%s'", item);
    transcript = sqlGetField(database, "ensemblXref3", "transcript", query);

    sqlSafef(query, sizeof query,
            "select chrom, chromStart, chromEnd from superfamily where name='%s';", transcript);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
        chrom      = row[0];
        chromStart = row[1];
        chromEnd   = row[2];
        printf("<HR>");
        printPosOnChrom(chrom, atoi(chromStart), atoi(chromEnd), NULL, TRUE, transcript);
        }
    sqlFreeResult(&sr);
    }
printTrackHtml(tdb);
}

void doOmimAv(struct trackDb *tdb, char *avName)
/* Process click on an OMIM AV. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
char *chp;
char *omimId, *avSubFdId;
char *avDescStartPos, *avDescLen;
char *omimTitle = cloneString("");
char *geneSymbol = NULL;
int iAvDescStartPos = 0;
int iAvDescLen = 0;

struct lineFile *lf;
char *line;
int lineSize;

cartWebStart(cart, database, "%s (%s)", tdb->longLabel, avName);

sqlSafef(query, sizeof(query), "select * from omimAv where name = '%s'", avName);
sr = sqlGetResult(conn, query);

if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find %s in omimAv table - database inconsistency.", avName);
sqlFreeResult(&sr);

omimId = strdup(avName);
chp = strstr(omimId, ".");
*chp = '\0';

chp++;
avSubFdId = chp;

sqlSafef(query, sizeof(query), "select title, geneSymbol from hgFixed.omimTitle where omimId = %s", omimId);
sr = sqlGetResult(conn, query);

if ((row = sqlNextRow(sr)) != NULL)
    {
    omimTitle  = cloneString(row[0]);
    geneSymbol = cloneString(row[1]);
    }
sqlFreeResult(&sr);

printf("<H4>OMIM <A HREF=\"");
printEntrezOMIMUrl(stdout, atoi(omimId));
printf("\" TARGET=_blank>%s</A>: %s; %s</H4>\n", omimId, omimTitle, geneSymbol);

sqlSafef(query, sizeof(query),
"select startPos, length from omimSubField where omimId='%s' and subFieldId='%s' and fieldType='AV'",
      omimId, avSubFdId);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find %s in omimSubField table - database inconsistency.", avName);
else
    {
    avDescStartPos = cloneString(row[0]);
    avDescLen	   = cloneString(row[1]);
    iAvDescStartPos = atoi(avDescStartPos);
    iAvDescLen      = atoi(avDescLen);
    }
sqlFreeResult(&sr);

lf = lineFileOpen("/gbdb/hg17/omim/omim.txt", TRUE);
lineFileSeek(lf,(size_t)(iAvDescStartPos), 0);
lineFileNext(lf, &line, &lineSize);
printf("<h4>");
printf(".%s %s ", avSubFdId, line);
lineFileNext(lf, &line, &lineSize);
printf("[%s]\n", line);
printf("</h4>");

while ((lf->lineStart + lf->bufOffsetInFile) < (iAvDescStartPos + iAvDescLen))
    {
    lineFileNext(lf, &line, &lineSize);
    printf("%s\n", line);
    }

htmlHorizontalLine();

printTrackHtml(tdb);
hFreeConn(&conn);
}

void doRgdQtl(struct trackDb *tdb, char *item)
/* Put up RGD QTL info. */
{
struct sqlConnection *conn = hAllocConn(database);
char query[256];
struct sqlResult *sr;
char **row;
char *otherDb = trackDbSetting(tdb, "otherDb");
char *qtlOrg;
if (sameString(tdb->table, "rgdQtl"))
    qtlOrg = organism;
else if (isNotEmpty(otherDb))
    qtlOrg = hOrganism(otherDb);
else
    qtlOrg = "";

genericHeader(tdb, item);
printf("<B>%s QTL %s: ", qtlOrg, item);
sqlSafef(query, sizeof(query),
      "select description from %sLink where name='%s';",
      tdb->table, item);
sr = sqlMustGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    printf("%s", row[0]);
sqlFreeResult(&sr);
printf("</B><BR>\n");

if (isNotEmpty(tdb->url))
    {
    boolean gotId = FALSE;
    sqlSafef(query, sizeof(query), "select id from %sLink where name='%s';",
	  tdb->table, item);
    sr = sqlMustGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	char *qtlId = row[0];
	printf(gotId ? ", \n\t" : "<B>RGD QTL Report:</B> ");
        printf("<B><A HREF=\"%s%s\" target=_blank>", tdb->url, qtlId);
        printf("RGD:%s</A></B>", qtlId);
	gotId = TRUE;
        }
    if (gotId)
	printf("\n<BR>\n");
    sqlFreeResult(&sr);
    }

int start=cartInt(cart, "o"), end=cartInt(cart, "t");
struct bed *selectedPos=NULL, *otherPosList=NULL, *bed=NULL;
sqlSafef(query, sizeof(query),
      "select chrom, chromStart, chromEnd from %s where name='%s' "
      "order by (chromEnd-chromStart);",
      tdb->table, item);
sr = sqlMustGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoad3(row);
    if (selectedPos == NULL && sameString(bed->chrom, seqName) &&
	bed->chromStart == start && bed->chromEnd == end)
	selectedPos = bed;
    else
	slAddHead(&otherPosList, bed);
    }
sqlFreeResult(&sr);
if (selectedPos)
    printPosOnChrom(seqName, start, end, NULL, FALSE, item);

if (otherPosList)
    printf("<BR>%s QTL %s is also mapped to these locations "
	   "(largest genomic size first):</BR>\n", qtlOrg, item);
for (bed = otherPosList;  bed != NULL;  bed = bed->next)
    {
    printf("<HR>");
    printPosOnChrom(bed->chrom, bed->chromStart, bed->chromEnd,
		    NULL, FALSE, item);
    }
hFreeConn(&conn);
printTrackHtml(tdb);
}

void printGadDetails(struct trackDb *tdb, char *itemName, boolean encode)
/* Print details of a GAD entry. */
{
int refPrinted = 0;
boolean showCompleteGadList;

struct sqlConnection *conn = hAllocConn(database);
char query[256];
struct sqlResult *sr;
char **row;
char *chrom, *chromStart, *chromEnd;
struct dyString *currentCgiUrl;
char *diseaseClass;

char *url = tdb->url;

if (url != NULL && url[0] != 0)
    {
    showCompleteGadList = FALSE;
    if (cgiOptionalString("showAllRef") != NULL)
        {
        if (sameWord(cgiOptionalString("showAllRef"), "Y") ||
	    sameWord(cgiOptionalString("showAllRef"), "y") )
	    {
	    showCompleteGadList = TRUE;
	    }
	}
    currentCgiUrl = cgiUrlString();

    printf("<H3>Gene %s: ", itemName);
    sqlSafef(query, sizeof(query), "select geneName from gadAll where geneSymbol='%s';", itemName);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)printf("%s", row[0]);
    printf("</H3>");
    sqlFreeResult(&sr);

    printf("<B>Genetic Association Database: ");
    printf("%s</B>\n", itemName);

    printf("<BR><B>CDC HuGE Published Literature:  ");
    printf("<A HREF=\"https://phgkb.cdc.gov/PHGKB/searchSummary.action"
    	"?Mysubmit=Search&firstQuery=%s&__checkbox_gwas=true\" target=_blank>",
	itemName);
    printf("%s</A></B>\n", itemName);

    sqlSafef(query, sizeof(query),
          "select distinct g.omimId, o.title from gadAll g, hgFixed.omimTitle o where g.geneSymbol='%s' and g.omimId <>'.' and g.omimId=o.omimId",
          itemName);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL) printf("<BR><B>OMIM: </B>");
    while (row != NULL)
        {
	printf("<A HREF=\"%s%s\" target=_blank>",
		"https://www.ncbi.nlm.nih.gov/omim/", row[0]);
	printf("%s</B></A> %s\n", row[0], row[1]);
	row = sqlNextRow(sr);
        }
    sqlFreeResult(&sr);

    /* List disease classes associated with the gene */
    sqlSafef(query, sizeof(query),
          "select distinct diseaseClass from gadAll where geneSymbol='%s' and association = 'Y' order by diseaseClass",
    itemName);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);

    if (row != NULL)
        {
        diseaseClass = row[0];
	printf("<BR><B>Disease Class:  </B>");
	printf("%s", diseaseClass);
        row = sqlNextRow(sr);
        }

    while (row != NULL)
        {
        diseaseClass = row[0];
	printf(", %s", diseaseClass);
        row = sqlNextRow(sr);
	}
    sqlFreeResult(&sr);

    /* List diseases associated with the gene */
    sqlSafef(query, sizeof(query),
          "select distinct broadPhen from gadAll where geneSymbol='%s' and association = 'Y' order by broadPhen;",
    itemName);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);

    if (row != NULL)
        {
	printf("<BR><B>Positive Disease Associations:  </B>");
	printf("%s\n", row[0]);
        row = sqlNextRow(sr);
        }

    while (row != NULL)
        {
	printf(", %s\n", row[0]);
        row = sqlNextRow(sr);
	}
    sqlFreeResult(&sr);

    refPrinted = 0;
    sqlSafef(query, sizeof(query),
          "select broadPhen,reference,title,journal, pubMed, conclusion from gadAll where geneSymbol='%s' and association = 'Y' and title != '' order by broadPhen",
       itemName);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);

    if (row != NULL) printf("<BR><BR><B>Related Studies: </B><OL>");
    while (row != NULL)
        {
        printf("<LI><B>%s </B>", row[0]);

	printf("<br>%s, %s, %s.\n", row[1], row[2], row[3]);
	if (!sameWord(row[4], ""))
	    {
	    printf(" [PubMed ");
	    printf("<A HREF=\"");
	    printEntrezPubMedUidAbstractUrl(stdout, atoi(row[4]));
	    printf("\" target=_blank>%s</B></A>]\n", row[4]);
	    }
	printf("<br><i>%s</i>\n", row[5]);

	printf("</LI>\n");
        refPrinted++;
        if ((!showCompleteGadList) && (refPrinted >= 5)) break;
	row = sqlNextRow(sr);
        }
    sqlFreeResult(&sr);
    printf("</OL>");

    if ((!showCompleteGadList) && (row != NULL))
        {
        printf("<B>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; more ...  </B>");
        printf("<A HREF=\"%s?showAllRef=Y&%s\">click here to view the complete list</A> ",
               hgcName(), currentCgiUrl->string);
        }

    sqlSafef(query, sizeof(query),
          "select chrom, chromStart, chromEnd from gad where name='%s';", itemName);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
	chrom      = row[0];
        chromStart = row[1];
	chromEnd   = row[2];
	printf("<HR>");
	printPosOnChrom(chrom, atoi(chromStart), atoi(chromEnd), NULL, FALSE, itemName);
        }
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }
}

void doGad(struct trackDb *tdb, char *item, char *itemForUrl)
/* Put up GAD track info. */
{
genericHeader(tdb, item);
printGadDetails(tdb, item, FALSE);
printTrackHtml(tdb);
}

void printCosmicDetails(struct trackDb *tdb, char *itemName)
/* Print details of a COSMIC entry. */
{
struct sqlConnection *conn  = hAllocConn(database);
struct sqlConnection *conn2 = hAllocConn(database);
char query[1024];
char query2[1024];
struct sqlResult *sr;
struct sqlResult *sr2;
char **row;
char **row2;

char *chp;
char indent1[40] = {"&nbsp;&nbsp;&nbsp;&nbsp;"};
char indent2[40] = {""};

char *gene_name, *accession_number;
// char $source, *cosmic_mutation_id;  unused variable
char *mut_description, *mut_syntax_cds, *mut_syntax_aa;
char *chromosome, *grch37_start, *grch37_stop, *mut_nt;
char *mut_aa, *tumour_site, *mutated_samples, *examined_samples, *mut_freq;
char *url = tdb->url;

char *chrom, *chromStart, *chromEnd;
chrom      = cartOptionalString(cart, "c");
chromStart = cartOptionalString(cart, "o");
chromEnd   = cartOptionalString(cart, "t");

sqlSafef(query, sizeof(query),
      "select source,cosmic_mutation_id,gene_name,accession_number,mut_description,mut_syntax_cds,mut_syntax_aa,"
      "chromosome,grch37_start,grch37_stop,mut_nt,mut_aa,tumour_site,mutated_samples,examined_samples,mut_freq"
      " from cosmicRaw where cosmic_mutation_id='%s'",
      itemName);

sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    int ii;
    boolean multipleTumorSites;
    char *indentString;

    ii=0;

    ii++; // source              = row[ii];ii++;  unused variable
    ii++; // cosmic_mutation_id  = row[ii];ii++;  unused variable
    gene_name           = row[ii];ii++;
    accession_number    = row[ii];ii++;
    mut_description     = row[ii];ii++;
    mut_syntax_cds      = row[ii];ii++;
    mut_syntax_aa       = row[ii];ii++;

    chromosome          = row[ii];ii++;
    grch37_start        = row[ii];ii++;
    grch37_stop         = row[ii];ii++;
    mut_nt              = row[ii];ii++;
    mut_aa              = row[ii];ii++;
    tumour_site         = row[ii];ii++;
    mutated_samples     = row[ii];ii++;
    examined_samples    = row[ii];ii++;
    mut_freq            = row[ii];ii++;

    // chromosome name adjustment
    if (sameString(chromosome, "23"))
	chromosome = "X";    
    if (sameString(chromosome, "24"))
	chromosome = "Y";    
    if (sameString(chromosome, "25"))
	chromosome = "M";    

    chp = strstr(itemName, "COSM")+strlen("COSM");
    printf("<B>COSMIC ID:</B> <A HREF=\"%s%s\" TARGET=_BLANK>%s</A> (details at COSMIC site)", url, chp, chp);

    // Embed URL to COSMIC site per COSMICT request.
    // printf("<BR><B>Source:</B> ");
    // printf("<A HREF=\"http://cancer.sanger.ac.uk/cancergenome/projects/cosmic/\" TARGET=_BLANK>%s</A>\n", source);

    printf("<BR><B>Gene Name:</B> %s\n", gene_name);
    printf("<BR><B>Accession Number:</B> %s\n", accession_number);
    printf("<BR><B>Genomic Position:</B> chr%s:%s-%s", chromosome, grch37_start, grch37_stop);
    printf("<BR><B>Mutation Description:</B> %s\n", mut_description);
    printf("<BR><B>Mutation Syntax CDS:</B> %s\n", mut_syntax_cds);
    printf("<BR><B>Mutation Syntax AA:</B> %s\n", mut_syntax_aa);
    printf("<BR><B>Mutation NT:</B> %s\n", mut_nt);
    printf("<BR><B>Mutation AA:</B> %s\n", mut_aa);

    sqlSafef(query2, sizeof(query2),
      "select count(tumour_site) from cosmicRaw where cosmic_mutation_id='%s'", itemName);

    sr2 = sqlMustGetResult(conn2, query2);
    row2 = sqlNextRow(sr2);
    if ((atoi(row2[0])) > 1)
        {
	multipleTumorSites = TRUE;
        indentString = indent1;
	}
    else
        {
        multipleTumorSites = FALSE;
        indentString = indent2;
        }
    sqlFreeResult(&sr2);

    sqlSafef(query2, sizeof(query2),
      "select tumour_site,mutated_samples,examined_samples,mut_freq "
      " from cosmicRaw where cosmic_mutation_id='%s' order by tumour_site",
      itemName);

    sr2 = sqlMustGetResult(conn2, query2);
    row2 = sqlNextRow(sr2);
    while (row2 != NULL)
        {
        int ii;
        ii=0;
        tumour_site             = row2[ii];ii++;
        mutated_samples         = row2[ii];ii++;
        examined_samples        = row2[ii];ii++;
        mut_freq                = row2[ii];ii++;

        if (multipleTumorSites) printf("<BR>");
        printf("<BR><B>%sTumor Site:</B> %s\n",         indentString, tumour_site);
        printf("<BR><B>%sMutated Samples:</B> %s\n",    indentString, mutated_samples);
        printf("<BR><B>%sExamined Samples:</B> %s\n",   indentString, examined_samples);
        printf("<BR><B>%sMutation Frequency:</B> %s\n", indentString, mut_freq);
        row2 = sqlNextRow(sr2);
        }
    sqlFreeResult(&sr2);

    sqlSafef(query2, sizeof(query2),
      "select sum(mutated_samples) from cosmicRaw where cosmic_mutation_id='%s'",
      itemName);

    sr2 = sqlMustGetResult(conn2, query2);
    row2 = sqlNextRow(sr2);
    if (row2 != NULL)
        {
        printf("<BR><BR><B>Total Mutated Samples:</B> %s\n", row2[0]);
        //printf("<br>%s ", row2[0]);
        }
    sqlFreeResult(&sr2);

    sqlSafef(query2, sizeof(query2),
      "select sum(examined_samples) from cosmicRaw where cosmic_mutation_id='%s'",
      itemName);
    sr2 = sqlMustGetResult(conn2, query2);
    row2 = sqlNextRow(sr2);
    if (row2 != NULL)
        {
        printf("<BR><B>Total Examined Samples:</B> %s\n", row2[0]);
	}
    sqlFreeResult(&sr2);
    sqlSafef(query2, sizeof(query2),
      "select sum(mutated_samples)*100/sum(examined_samples) from cosmicRaw where cosmic_mutation_id='%s'",
      itemName);
    sr2 = sqlMustGetResult(conn2, query2);
    row2 = sqlNextRow(sr2);
    if (row2 != NULL)
        {
        char *chp;
	chp = strstr(row2[0], ".");
	if ((chp != NULL) && (strlen(chp) > 3))
	   {
	   chp++;
	   chp++;
	   chp++;
	   chp++;
	   *chp = '\0';
	   }
	printf("<BR><B>Total Mutation Frequency:</B> %s%c\n", row2[0], '%');
	//printf("<br>%s", row2[0]);
	}
    sqlFreeResult(&sr2);

    }

sqlFreeResult(&sr);
hFreeConn(&conn);

printf("<HR>");
printPosOnChrom(chrom, atoi(chromStart), atoi(chromEnd), NULL, FALSE, itemName);
}

void doCosmic(struct trackDb *tdb, char *item)
/* Put up COSMIC track info. */
{
genericHeader(tdb, item);
printCosmicDetails(tdb, item);
printTrackHtml(tdb);
}

void printDecipherSnvsDetails(struct trackDb *tdb, char *itemName, boolean encode)
/* Print details of a DECIPHER entry. */
{
char *db = database;
char *liftDb = cloneString(trackDbSetting(tdb, "quickLiftDb"));
if (liftDb != NULL) 
    db = liftDb;
struct sqlConnection *conn = hAllocConn(db);
char query[256];
struct sqlResult *sr;
char **row;
char *strand={"+"};
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
char *chrom = cartString(cart, "c");

/* So far, we can just remove "chr" from UCSC chrom names to get DECIPHER names */
char *decipherChrom = chrom;
if (startsWithNoCase("chr", decipherChrom))
    decipherChrom += 3;

printf("<H3>Patient %s </H3>", itemName);

/* print phenotypes and other information, if available */
if (sqlFieldIndex(conn, "decipherSnvsRaw", "phenotypes") >= 0)
    {
    sqlSafef(query, sizeof(query),
        "select phenotypes, refAllele, altAllele, transcript, gene, genotype, "
        "inheritance, pathogenicity, contribution "
        "from decipherSnvsRaw where id = '%s' and chr = '%s' and start = %d and end = %d",
        itemName, decipherChrom, start+1, end);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if ((row != NULL) && strlen(row[0]) >= 1)
        {
        char *phenoString = replaceChars(row[0], "|", "</li>\n<li>");
        printf("<b>Phenotypes:</b>\n<ul>\n"
               "<li>%s</li>\n"
               "</ul>\n", phenoString);
        // freeMem(phenoString);
        }
    if (row != NULL)
        {
        char *hgsidString = cartSidUrlString(cart);
        if (isNotEmpty(row[1]))
            {
            printf("<b>Ref Allele:</b> %s\n<br>\n", row[1]);
            }
        if (isNotEmpty(row[2]))
            {
            printf("<b>Alt Allele:</b> %s\n<br>\n", row[2]);
            }
        if (isNotEmpty(row[3]))
            {
            printf("<b>Transcript:</b> <a href='../cgi-bin/hgTracks?%s&position=%s'>%s</a>\n<br>\n",
                hgsidString, row[3], row[3]);
            }
        if (isNotEmpty(row[4]))
            {
            printf("<b>Gene:</b> <a href='../cgi-bin/hgTracks?%s&position=%s'>%s</a>\n<br>\n",
                hgsidString, row[4], row[4]);
            }
        if (isNotEmpty(row[5]))
            {
            printf("<b>Genotype:</b> %s\n<br>\n", row[5]);
            }
        if (isNotEmpty(row[6]))
            {
            printf("<b>Inheritance:</b> %s\n<br>\n", row[6]);
            }
        if (isNotEmpty(row[7]))
            {
            printf("<b>Pathogenicity:</b> %s\n<br>\n", row[7]);
            }
        if (isNotEmpty(row[8]))
            {
            printf("<b>Contribution:</b> %s\n<br>\n", row[8]);
            }
        }
    sqlFreeResult(&sr);
    }
else
    {
    sqlSafef(query, sizeof(query),
          "select distinct phenotype from decipherSnvsRaw where id ='%s' order by phenotype", itemName);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if ((row != NULL) && strlen(row[0]) >= 1)
        {
        printf("<B>Phenotype: </B><UL>");
        while (row != NULL)
            {
        printf("<LI>");
        printf("%s\n", row[0]);
        row = sqlNextRow(sr);
            }
        printf("</UL>");
        }
    sqlFreeResult(&sr);
    }

/* link to Ensembl DECIPHER Patient View page */
printf("<B>Patient View: </B>\n");
printf("More details on patient %s at ", itemName);
printf("<A HREF=\"%s%s\" target=_blank>",
       "https://www.deciphergenomics.org/patient/", itemName);
printf("DECIPHER</A>.<BR><BR>");

/* print position info */
printPosOnChrom(chrom, start, end, strand, TRUE, itemName);

hFreeConn(&conn);
}

void doDecipherSnvs(struct trackDb *tdb, char *item, char *itemForUrl)
/* Put up DECIPHER track info. */
{
genericHeader(tdb, item);
printDecipherSnvsDetails(tdb, item, FALSE);
printTrackHtml(tdb);
}

void printDecipherCnvsDetails(struct trackDb *tdb, char *itemName, boolean encode)
/* Print details of a DECIPHER entry. */
{
char *db = database;
char *liftDb = cloneString(trackDbSetting(tdb, "quickLiftDb"));
if (liftDb != NULL) 
    db = liftDb;
struct sqlConnection *conn = hAllocConn(db);
char query[256];
struct sqlResult *sr;
char **row;
struct sqlConnection *conn2 = hAllocConn(db);
char query2[256];
struct sqlResult *sr2;
char **row2;
char *strand={"+"};
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
char *chrom = cartString(cart, "c");

/* So far, we can just remove "chr" from UCSC chrom names to get DECIPHER names */
char *decipherChrom = chrom;
if (startsWithNoCase("chr", decipherChrom))
    decipherChrom += 3;

printf("<H3>Patient %s </H3>", itemName);

/* print phenotypes and other information, if available */
if (sqlFieldIndex(conn, "decipherRaw", "phenotypes") >= 0)
    {
    sqlSafef(query, sizeof(query),
        "select phenotypes, mean_ratio, inheritance, pathogenicity, contribution "
        "from decipherRaw where id = '%s' and chr = '%s' and start = %d and end = %d",
        itemName, decipherChrom, start+1, end);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if ((row != NULL) && strlen(row[0]) >= 1)
        {
        char *phenoString = replaceChars(row[0], "|", "</li>\n<li>");
        printf("<b>Phenotypes:</b>\n<ul>\n"
               "<li>%s</li>\n"
               "</ul>\n", phenoString);
        // freeMem(phenoString);
        }
    if (row != NULL)
        {
        if (isNotEmpty(row[1]))
            {
            printf("<b>Mean Ratio:</b> %s\n<br>\n", row[1]);
            }
        if (isNotEmpty(row[2]))
            {
            printf("<b>Inheritance:</b> %s\n<br>\n", row[2]);
            }
        if (isNotEmpty(row[3]))
            {
            printf("<b>Pathogenicity:</b> %s\n<br>\n", row[3]);
            }
        if (isNotEmpty(row[4]))
            {
            printf("<b>Contribution:</b> %s\n<br>\n", row[4]);
            }
        }
    sqlFreeResult(&sr);
    }
else
    {
    sqlSafef(query, sizeof(query),
          "select distinct phenotype from decipherRaw where id ='%s' order by phenotype", itemName);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if ((row != NULL) && strlen(row[0]) >= 1)
        {
        printf("<B>Phenotype: </B><UL>");
        while (row != NULL)
            {
        printf("<LI>");
        printf("%s\n", row[0]);
        row = sqlNextRow(sr);
            }
        printf("</UL>");
        }
    sqlFreeResult(&sr);
    }

/* link to Ensembl DECIPHER Patient View page */
printf("<B>Patient View: </B>\n");
printf("More details on patient %s at ", itemName);
printf("<A HREF=\"%s%s\" target=_blank>",
       "https://www.deciphergenomics.org/patient/", itemName);
printf("DECIPHER</A>.<BR><BR>");

/* print position info */
printPosOnChrom(chrom, start, end, strand, TRUE, itemName);

/* print UCSC Genes in the reported region */
sqlSafef(query, sizeof(query),
      "select distinct t.name "
      // mysql 5.7: SELECT list w/DISTINCT must include all fields in ORDER BY list (#18626)
      ", geneSymbol "
      "from knownCanonToDecipher t, kgXref x  where value ='%s' and x.kgId=t.name order by geneSymbol", itemName);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    printf("<BR><B>UCSC Canonical Gene(s) in this genomic region: </B><UL>");
    while (row != NULL)
        {
	sqlSafef(query2, sizeof(query2),
        "select geneSymbol, kgId, description from kgXref where kgId ='%s'", row[0]);
	sr2 = sqlMustGetResult(conn2, query2);
	row2 = sqlNextRow(sr2);
	if (row2 != NULL)
            {
	    printf("<LI>");
            printf("<A HREF=\"%s%s\" target=_blank>","./hgGene\?hgg_chrom=none&hgg_gene=", row2[1]);
            printf("%s (%s)</A> ", row2[0], row2[1]);
	    printf(" %s", row2[2]);
	    }
        sqlFreeResult(&sr2);
	row = sqlNextRow(sr);
	}
    sqlFreeResult(&sr);
    printf("</UL>");
    }
hFreeConn(&conn);
hFreeConn(&conn2);
}

void doDecipherCnvs(struct trackDb *tdb, char *item, char *itemForUrl)
/* Put up DECIPHER track info. */
{
genericHeader(tdb, item);
printDecipherCnvsDetails(tdb, item, FALSE);
printTrackHtml(tdb);
}

char *gbCdnaGetDescription(struct sqlConnection *conn, char *acc)
/* return mrna description, or NULL if not available. freeMem result */
{
char query[1024];
if (!sqlTableExists(conn, gbCdnaInfoTable))
    return NULL;
sqlSafef(query, sizeof(query),
      "select d.name from %s g,%s d where (acc = '%s') and (g.description = d.id)", gbCdnaInfoTable, descriptionTable, acc);
char *desc = sqlQuickString(conn, query);
if ((desc == NULL) || sameString(desc, "n/a") || (strlen(desc) == 0))
    freez(&desc);
return desc;
}

void printOmimGeneDetails(struct trackDb *tdb, char *itemName, boolean encode)
/* Print details of an OMIM Gene entry. */
{
struct sqlConnection *conn  = hAllocConn(database);
struct sqlConnection *conn2 = hAllocConn(database);
char query[256];
struct sqlResult *sr;
char **row;
char *url = tdb->url;
char *kgId= NULL;
char *title1 = NULL;
char *geneSymbols = NULL;
char *chrom, *chromStart, *chromEnd;
char *kgDescription = NULL;
char *refSeq;

chrom      = cartOptionalString(cart, "c");
chromStart = cartOptionalString(cart, "o");
chromEnd   = cartOptionalString(cart, "t");

if (url != NULL && url[0] != 0)
    {
    /* check if the entry is in morbidmap, if so remember the assoicated gene symbols */
    sqlSafef(query, sizeof(query),
          "select geneSymbols from omimMorbidMap where omimId=%s;", itemName);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
	geneSymbols = cloneString(row[0]);
	}
    sqlFreeResult(&sr);

    /* get corresponding KG ID */
    sqlSafef(query, sizeof(query),
	  "select k.transcript from knownCanonical k where k.chrom='%s' and k.chromStart=%s and k.chromEnd=%s",
	  chrom, chromStart, chromEnd);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
	kgId = cloneString(row[0]);
	}
    sqlFreeResult(&sr);

    /* use geneSymbols from omimMorbidMap if available */
    if (geneSymbols!= NULL)
        {
	printf("<B>OMIM gene or syndrome:</B> %s", geneSymbols);
	printf("<BR>\n");

	/* display disorder for genes in morbidmap */
        sqlSafef(query, sizeof(query), "select description from omimMorbidMap where omimId=%s;",
              itemName);
        sr = sqlMustGetResult(conn, query);
        while ((row = sqlNextRow(sr)) != NULL)
            {
            printf("<B>Disorder:</B> %s", row[0]);
            printf("<BR>\n");
            }
        sqlFreeResult(&sr);
	}
    else
	{
	/* display gene symbol(s) from omimGeneMap2  */
        sqlSafef(query, sizeof(query), "select geneSymbol from omimGeneMap2 where omimId=%s;", itemName);
        sr = sqlMustGetResult(conn, query);
        row = sqlNextRow(sr);
        if (row != NULL)
            {
            printf("<B>OMIM Gene Symbol:</B> %s", row[0]);
            printf("<BR>\n");
            sqlFreeResult(&sr);
            }
	else
            {
            /* get gene symbol from kgXref if the entry is not in morbidmap and omim genemap */
            sqlSafef(query, sizeof(query), "select geneSymbol from kgXref where kgId='%s';", kgId);

            sr = sqlMustGetResult(conn, query);
            row = sqlNextRow(sr);
            if (row != NULL)
                {
                printf("<B>UCSC Gene Symbol:</B> %s", row[0]);
                printf("<BR>\n");
                }
            sqlFreeResult(&sr);
            }
	}
    printf("<B>OMIM Database ");
    printf("<A HREF=\"%s%s\" target=_blank>", url, itemName);
    printf("%s</A></B>", itemName);

    sqlSafef(query, sizeof(query),
          "select geneName from omimGeneMap2 where omimId=%s;", itemName);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
        if (row[0] != NULL)
            {
            title1 = cloneString(row[0]);
            printf(": %s", title1);
            }
        }
    sqlFreeResult(&sr);

    printf("<BR>\n");

    if (kgId != NULL)
        {
        printf("<B>UCSC Canonical Gene ");
        printf("<A HREF=\"%s%s&hgg_chrom=none\" target=_blank>",
               "../cgi-bin/hgGene?hgg_gene=", kgId);
        printf("%s</A></B>: ", kgId);

        sqlSafef(query, sizeof(query), "select refseq from kgXref where kgId='%s';", kgId);
        sr = sqlMustGetResult(conn, query);
        row = sqlNextRow(sr);
        if (row != NULL)
	    {
	    refSeq = strdup(row[0]);
	    kgDescription = gbCdnaGetDescription(conn2, refSeq);
	    }
	sqlFreeResult(&sr);
        hFreeConn(&conn2);

	if (kgDescription == NULL)
	    {
            sqlSafef(query, sizeof(query), "select description from kgXref where kgId='%s';", kgId);
            sr = sqlMustGetResult(conn, query);
            row = sqlNextRow(sr);
            if (row != NULL)
                {
                printf("%s", row[0]);
                }

            sqlFreeResult(&sr);
            }
        else
            {
	    printf("%s", kgDescription);
	    }
        printf("<BR>\n");

	sqlSafef(query, sizeof(query),
              "select i.transcript from knownIsoforms i, knownCanonical c where c.transcript='%s' and i.clusterId=c.clusterId and i.transcript <>'%s'",
	      kgId, kgId);
        sr = sqlMustGetResult(conn, query);
	if (sr != NULL)
	    {
	    int printedCnt;
	    printedCnt = 0;
	    while ((row = sqlNextRow(sr)) != NULL)
                {
                if (printedCnt < 1)
		    printf("<B>Other UCSC Gene(s) in the same cluster: </B>");
		else
		    printf(", ");
                printf("<A HREF=\"%s%s&hgg_chrom=none\" target=_blank>",
                       "../cgi-bin/hgGene?hgg_gene=", row[0]);
                printf("%s</A></B>", row[0]);
                printedCnt++;
		}
            if (printedCnt >= 1) printf("<BR>\n");
	    }
	sqlFreeResult(&sr);
	}
    }

printf("<HR>");
printPosOnChrom(chrom, atoi(chromStart), atoi(chromEnd), NULL, FALSE, itemName);
}

#include "omim.h"

static void showOmimDisorderTable(struct sqlConnection *conn, char *url, char *itemName)
{
/* display disorder(s) as a table, in the same format as on the OMIM webpages, 
 * e.g. see the "Gene-Phenotype-Relationships" table at https://www.omim.org/entry/601542 */
struct sqlResult *sr;
char query[256];
char **row;

// be tolerant of old table schema
if (sqlColumnExists(conn, "omimPhenotype", "inhMode"))
    sqlSafef(query, sizeof(query),
          "select description, %s, phenotypeId, inhMode from omimPhenotype where omimId=%s order by description",
          omimPhenotypeClassColName, itemName);
else
    // E.g. on a mirror that has not updated their OMIM tables yet
    sqlSafef(query, sizeof(query),
          "select description, %s, phenotypeId, 'data-missing' from omimPhenotype where omimId=%s order by description",
          omimPhenotypeClassColName, itemName);

sr = sqlStoreResult(conn, query);

if (sqlCountRows(sr)==0) {
    sqlFreeResult(&sr);
    return;
}

char *phenotypeClass, *phenotypeId, *disorder, *inhMode;

puts("<table style='margin-top: 15px' class='omimTbl'>\n");
puts("<thead>\n");
puts("<th>Phenotype</th>\n");
puts("<th style='width:100px'>Phenotype MIM Number</th>\n");
puts("<th>Inheritance</th>\n");
puts("<th>Phenotype Key</th>\n");
puts("</thead>\n");

puts("<tbody>\n");
while ((row = sqlNextRow(sr)) != NULL)
    {
    disorder       = row[0];
    phenotypeClass = row[1];
    phenotypeId    = row[2];
    inhMode        = row[3];

    puts("<tr>\n");

    puts("<td>");
    if (disorder)
        puts(disorder);
    puts("</td>\n");

    puts("<td>");
    if (phenotypeId && (!sameWord(phenotypeId, "-1")))
        printf("<a HREF=\"%s%s\" target=_blank>%s</a>", url, phenotypeId, phenotypeId);
    puts("</td>\n");

    puts("<td>");
    if (inhMode)
        puts(inhMode);
    puts("</td>");

    puts("<td>");
    if (phenotypeClass && !sameWord(phenotypeClass, "-1"))
        {
        puts(phenotypeClass);
        if (isdigit(phenotypeClass[0]))
            {
            int phenoClass = atoi(phenotypeClass);
            char* descs[] = 
                { 
                "disease was positioned by mapping of the wild-type gene",
                "disorder itself was mapped",
                "molecular basis of the disease is known",
                "disorder is a chromosome deletion of duplication syndrome"
                };
            if (phenoClass>=1 && phenoClass<=4)
                {
                puts(" - ");
                puts(descs[phenoClass-1]);
                }
            else
                // just in case that they ever add another class in the future
                puts(phenotypeClass);
            }
        }
    puts("</td>");

    puts("</tr>");
    }

sqlFreeResult(&sr);
puts("<tbody>\n");
puts("</table>\n");
}

void printOmimGene2Details(struct trackDb *tdb, char *itemName, boolean encode)
/* Print details of an omimGene2 entry. */
{
struct sqlConnection *conn  = hAllocConn(database);
char query[256];
struct sqlResult *sr;
char **row;
char *url = tdb->url;
char *chrom, *chromStart, *chromEnd;

chrom      = cartOptionalString(cart, "c");
chromStart = cartOptionalString(cart, "o");
chromEnd   = cartOptionalString(cart, "t");

printf("<div id='omimText'>");
if (url != NULL && url[0] != 0)
    {
    printf("<B>MIM gene number: ");
    printf("<A HREF=\"%s%s\" target=_blank>", url, itemName);
    printf("%s</A></B><BR>", itemName);

    // disable NCBI link until they work it out with OMIM
    /*
    printf("<BR>\n");
    printf("<B>OMIM page at NCBI: ");
    printf("<A HREF=\"%s%s\" target=_blank>", ncbiOmimUrl, itemName);
    printf("%s</A></B>", itemName);
    */

    struct dyString *symQuery = dyStringNew(1024);
    sqlDyStringPrintf(symQuery, "SELECT approvedSymbol from omimGeneMap2 where omimId=%s", itemName);
    char *approvSym = sqlQuickString(conn, symQuery->string);
    if (approvSym) {
	printf("<B>HGNC-approved symbol:</B> %s", approvSym);
    }

    sqlSafef(query, sizeof(query),
          "select geneName from omimGeneMap2 where omimId=%s;", itemName);
    char *longName = sqlQuickString(conn, query);
    if (longName) {
	printf(" &mdash; %s", longName);
        freez(&longName);
    }
    puts("<BR><BR>");

    printPosOnChrom(chrom, atoi(chromStart), atoi(chromEnd), NULL, FALSE, itemName);

    sqlSafef(query, sizeof(query),
          "select geneSymbol from omimGeneMap2 where omimId=%s;", itemName);
    char *altSyms = sqlQuickString(conn, query);

    if (altSyms)
        {
        if (approvSym) 
            {
            char symRe[255];
            safef(symRe, sizeof(symRe), "^%s, ", approvSym);
            altSyms = replaceRegEx(altSyms, "", symRe, 0);
            }
	printf("<B>Alternative symbols:</B> %s", altSyms);
	printf("<BR>\n");
        freez(&altSyms);
        }
    if (approvSym)
        freez(&approvSym);

    // show RefSeq Gene link(s)
    sqlSafef(query, sizeof(query),
          "select distinct locusLinkId from %s l, omim2gene g, refGene r where l.omimId=%s and g.geneId=l.locusLinkId and g.entryType='gene' and chrom='%s' and txStart = %s and txEnd= %s",
	  refLinkTable, itemName, chrom, chromStart, chromEnd);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
        char *geneId;
        geneId = strdup(row[0]);
        sqlFreeResult(&sr);

        sqlSafef(query, sizeof(query),
              "select distinct l.mrnaAcc from %s l where locusLinkId = '%s' order by mrnaAcc asc", refLinkTable, geneId);
        sr = sqlMustGetResult(conn, query);
        if (sr != NULL)
	    {
	    int printedCnt;
	    printedCnt = 0;
	    while ((row = sqlNextRow(sr)) != NULL)
                {
                if (printedCnt < 1)
		    printf("<B>RefSeq Gene(s): </B>");
                else
		    printf(", ");
                printf("<A HREF=\"%s%s&o=%s&t=%s\">", "../cgi-bin/hgc?g=refGene&i=",
                       row[0], chromStart, chromEnd);
                printf("%s</A></B>", row[0]);
	        printedCnt++;
	        }
            if (printedCnt >= 1) printf("<BR>\n");
	    }
        sqlFreeResult(&sr);
        }

    // show Related UCSC Gene links
    char *knownDatabase = hdbDefaultKnownDb(database);
    sqlSafef(query, sizeof(query),
          "select distinct kgId from %s.kgXref x, %s l, omim2gene g where x.refseq = mrnaAcc and l.omimId=%s and g.omimId=l.omimId and g.entryType='gene'",
	  knownDatabase, refLinkTable, itemName);
    sr = sqlMustGetResult(conn, query);
    if (sr != NULL)
	{
	int printedCnt;
	printedCnt = 0;
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    if (printedCnt < 1)
		printf("<B>Related Transcripts: </B>");
	    else
		printf(", ");
            printf("<A HREF=\"%s%s&hgg_chrom=none\">", "../cgi-bin/hgGene?hgg_gene=", row[0]);
            printf("%s</A></B>", row[0]);
	    printedCnt++;
	    }
        if (printedCnt >= 1) printf("<BR>\n");
	}
    sqlFreeResult(&sr);

    // show GeneReviews  link(s)
    if (sqlTableExists(conn, "geneReviewsDetail"))
        {
        sqlSafef(query, sizeof(query),
          "select distinct r.name2 from %s l, omim2gene g, refGene r where l.omimId=%s and g.geneId=l.locusLinkId and g.entryType='gene' and chrom='%s' and txStart = %s and txEnd= %s",
        refLinkTable, itemName, chrom, chromStart, chromEnd);
        sr = sqlMustGetResult(conn, query);
        if (sr != NULL)
            {
            while ((row = sqlNextRow(sr)) != NULL)
                {
                prGRShortRefGene(row[0]);
                }
            }
        sqlFreeResult(&sr);
        }

    showOmimDisorderTable(conn, url, itemName);
    }

printf("</div>"); // #omimText
}

static void printOmimLocationDetails(struct trackDb *tdb, char *itemName, boolean encode)
/* Print details of an OMIM Class 3 Gene entry. */
{
char *liftDb = cloneString(trackDbSetting(tdb, "quickLiftDb"));
char *db = (liftDb == NULL) ? database : liftDb;
struct sqlConnection *conn  = hAllocConn(db);
struct sqlConnection *conn2 = hAllocConn(db);
char query[256];
struct sqlResult *sr;
char **row;
char *url = tdb->url;
char *kgId = NULL;
char *title1 = NULL;
char *geneSymbol = NULL;
char *chrom, *chromStart, *chromEnd;
char *kgDescription = NULL;
char *refSeq;
char *omimId;

chrom      = cartOptionalString(cart, "c");
chromStart = cartOptionalString(cart, "o");
chromEnd   = cartOptionalString(cart, "t");

omimId = itemName;

if (url != NULL && url[0] != 0)
    {
    printf("<B>OMIM: ");
    printf("<A HREF=\"%s%s\" target=_blank>", url, itemName);
    printf("%s</A></B>", itemName);
    sqlSafef(query, sizeof(query),
          "select geneName from omimGeneMap2 where omimId=%s;", itemName);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
        if (row[0] != NULL)
            {
            title1 = cloneString(row[0]);
            printf(": %s", title1);
            }
        }
    sqlFreeResult(&sr);
    printf("<BR>");

    /* get corresponding KG ID */
    sqlSafef(query, sizeof(query),
          "select k.transcript from knownCanonical k where k.chrom='%s' and k.chromStart=%s and k.chromEnd=%s",
          chrom, chromStart, chromEnd);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
        kgId = cloneString(row[0]);
        }
    sqlFreeResult(&sr);

    // disable NCBI link until they work it out with OMIM
    /*
    printf("<B>OMIM page at NCBI: ");
    printf("<A HREF=\"%s%s\" target=_blank>", ncbiOmimUrl, itemName);
    printf("%s</A></B><BR>", itemName);
    */

    printf("<B>Location: </B>");
    sqlSafef(query, sizeof(query),
          "select location from omimGeneMap2 where omimId=%s;", itemName);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
	if (row[0] != NULL)
	    {
	    char *locStr;
	    locStr= cloneString(row[0]);
            printf("%s\n", locStr);
	    }
	}
    sqlFreeResult(&sr);

    printf("<BR>\n");
    sqlSafef(query, sizeof(query),
          "select geneSymbol from omimGeneMap2 where omimId=%s;", itemName);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
	geneSymbol = cloneString(row[0]);
	}
    sqlFreeResult(&sr);

    sqlSafef(query, sizeof(query),"select omimId from omimPhenotype where omimId=%s\n", omimId);
    if (sqlQuickNum(conn, query) > 0)
        {
	char *phenotypeClass, *phenotypeId, *disorder;

	printf("<B>Gene symbol(s):</B> %s", geneSymbol);
	printf("<BR>\n");

	/* display disorder for genes in morbidmap */
        sqlSafef(query, sizeof(query),
	      "select description, %s, phenotypeId from omimPhenotype where omimId=%s order by description",
	      omimPhenotypeClassColName, itemName);
        sr = sqlMustGetResult(conn, query);
        printf("<B>Disorder(s):</B><UL>\n");
        while ((row = sqlNextRow(sr)) != NULL)
            {
	    disorder       = row[0];
            phenotypeClass = row[1];
            phenotypeId    = row[2];
            printf("<LI>%s", disorder);
            if (phenotypeId != NULL)
                {
                if (!sameWord(phenotypeId, "-1"))
                    {
                    printf(" (phenotype <A HREF=\"%s%s\" target=_blank>", url, phenotypeId);
                    printf("%s</A></B>)", phenotypeId);
		    }
		if (!sameWord(phenotypeClass, "-1"))
		    {
                    printf(" (%s)", phenotypeClass);
		    }
		}
	    printf("<BR>\n");
	    }
	printf("</UL>\n");
        sqlFreeResult(&sr);
	}
    else
	{
	/* display gene symbol(s) from omimGenemap  */
        sqlSafef(query, sizeof(query), "select geneSymbol from omimGeneMap2 where omimId=%s;", itemName);
        sr = sqlMustGetResult(conn, query);
        row = sqlNextRow(sr);
        if (row != NULL)
            {
            printf("<B>OMIM Gene Symbol:</B> %s", row[0]);
            printf("<BR>\n");
            sqlFreeResult(&sr);
            }
	else
            {
            /* get gene symbol from kgXref if the entry is not in morbidmap and omim genemap */
            sqlSafef(query, sizeof(query), "select geneSymbol from kgXref where kgId='%s';", kgId);

            sr = sqlMustGetResult(conn, query);
            row = sqlNextRow(sr);
            if (row != NULL)
                {
                printf("<B>UCSC Gene Symbol:</B> %s", row[0]);
                printf("<BR>\n");
                }
            sqlFreeResult(&sr);
            }
	}

    if (kgId != NULL)
        {
        printf("<B>UCSC Canonical Gene ");
        printf("<A HREF=\"%s%s&hgg_chrom=none\" target=_blank>",
               "../cgi-bin/hgGene?hgg_gene=", kgId);
        printf("%s</A></B>: ", kgId);

        sqlSafef(query, sizeof(query), "select refseq from kgXref where kgId='%s';", kgId);
        sr = sqlMustGetResult(conn, query);
        row = sqlNextRow(sr);
        if (row != NULL)
	    {
	    refSeq = strdup(row[0]);
	    kgDescription = gbCdnaGetDescription(conn2, refSeq);
	    }
	sqlFreeResult(&sr);
        hFreeConn(&conn2);

	if (kgDescription == NULL)
	    {
            sqlSafef(query, sizeof(query), "select description from kgXref where kgId='%s';", kgId);
            sr = sqlMustGetResult(conn, query);
            row = sqlNextRow(sr);
            if (row != NULL)
                {
                printf("%s", row[0]);
                }

            sqlFreeResult(&sr);
            }
        else
            {
	    printf("%s", kgDescription);
	    }
        printf("<BR>\n");

	sqlSafef(query, sizeof(query),
	      "select i.transcript from knownIsoforms i, knownCanonical c where c.transcript='%s' and i.clusterId=c.clusterId and i.transcript <>'%s'",
	      kgId, kgId);
        sr = sqlMustGetResult(conn, query);
	if (sr != NULL)
	    {
	    int printedCnt;
	    printedCnt = 0;
	    while ((row = sqlNextRow(sr)) != NULL)
                {
	        if (printedCnt < 1)
		    printf("<B>Other UCSC Gene(s) in the same cluster: </B>");
		else
		    printf(", ");
                printf("<A HREF=\"%s%s&hgg_chrom=none\" target=_blank>",
                       "../cgi-bin/hgGene?hgg_gene=", row[0]);
                printf("%s</A></B>", row[0]);
                printedCnt++;
		}
            if (printedCnt >= 1) printf("<BR>\n");
	    }
	sqlFreeResult(&sr);
	}
    }

printf("<HR>");
printPosOnChrom(chrom, atoi(chromStart), atoi(chromEnd), NULL, FALSE, itemName);
}

void doOmimLocation(struct trackDb *tdb, char *item)
/* Put up OmimGene track info. */
{
genericHeader(tdb, item);
printOmimLocationDetails(tdb, item, FALSE);
printTrackHtml(tdb);
}

void printOmimAvSnpDetails(struct trackDb *tdb, char *itemName, boolean encode)
/* Print details of an OMIM AvSnp entry. */
{
char *liftDb = cloneString(trackDbSetting(tdb, "quickLiftDb"));
char *db = (liftDb == NULL) ? database : liftDb;
struct sqlConnection *conn  = hAllocConn(db);
char query[256];
struct sqlResult *sr;
char **row;
char *url = tdb->url;
char *title1 = NULL;
char *chrom, *chromStart, *chromEnd;
char *avId;
char *dbSnpId;
char *chp;
char avString[255];
char *avDesc = NULL;

chrom      = cartOptionalString(cart, "c");
chromStart = cartOptionalString(cart, "o");
chromEnd   = cartOptionalString(cart, "t");

avId       = strdup(itemName);

chp = strstr(avId, "-");
if (chp != NULL) *chp = '\0';

safef(avString, sizeof(avString), "%s", itemName);

chp = strstr(itemName, ".");
*chp = '\0';

chp = avString;
chp = strstr(avString, ".");
*chp = '#';

if (url != NULL && url[0] != 0)
    {
    sqlSafef(query, sizeof(query),
          "select m.geneName,  format(seqNo/10000,4), v.description"
           " from omimGeneMap2 m, omimAv v"
          " where m.omimId=%s and m.omimId=v.omimId and v.avId='%s';", itemName, avId);

    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
        if (row[0] != NULL)
            {
            title1 = cloneString(row[0]);
            }
        avDesc = cloneString(row[2]);
        }
    sqlFreeResult(&sr);

    printf("<B>OMIM Allelic Variant: ");
    printf("<A HREF=\"%s%s\" target=_blank>", url, avString);
    printf("%s</A></B>", avId);
    printf(" %s", avDesc ? avDesc : "");

    printf("<BR><B>OMIM: ");
    printf("<A HREF=\"%s%s\" target=_blank>", url, itemName);
    printf("%s</A></B>", itemName);
    if (title1 != NULL) printf(": %s", title1);

    // disable NCBI link until they work it out with OMIM
    /*
    printf("<BR>\n");
    printf("<B>OMIM page at NCBI: ");
    printf("<A HREF=\"%s%s\" target=_blank>", ncbiOmimUrl, itemName);
    printf("%s</A></B><BR>", itemName);
    */

    sqlSafef(query, sizeof(query),
          "select repl2 from omimAv where avId=%s;", avId);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
      	if (row[0] != NULL)
	          printf("<BR><B>Amino Acid Replacement:</B> %s\n", row[0]);
	      }
    sqlFreeResult(&sr);

    printf("<BR>\n");

    sqlSafef(query, sizeof(query),
          "select dbSnpId from omimAv where avId='%s'", avId);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    dbSnpId = cloneString("-");
    if (row != NULL)
      	dbSnpId = cloneString(row[0]);
    sqlFreeResult(&sr);

    if (!sameWord(dbSnpId, "-"))
        {
        struct slName *snpIdList, *thisSnpId;

        printf("<b>dbSNP/ClinVar:</b> \n");

        /* for each variant, print name and build a link for it if possible */
        snpIdList = slNameListFromComma(dbSnpId);
        while ((thisSnpId = slPopHead(&snpIdList)) != NULL)
            {
            if (strncmp(thisSnpId->name, "rs", 2) == 0) /* dbSnp ID */
                printDbSnpRsUrl (thisSnpId->name, "%s", thisSnpId->name);
            else if (strncmp(thisSnpId->name, "SCV", 3) == 0) /* ClinVar ID */
                {
                char clinVarUrl[2048];
                safef (clinVarUrl, sizeof(clinVarUrl), clinVarFormat, thisSnpId->name);
                printf ("<a href=\"%s\" target=\"_blank\">%s</a>", clinVarUrl, thisSnpId->name);
                }
            else
                printf ("%s", thisSnpId->name);

            slNameFree(&thisSnpId);

            if (snpIdList != NULL)
                printf (",");
            }
        printf("<br>\n");
        }
    }

printf("<hr>\n");
printPosOnChrom(chrom, atoi(chromStart), atoi(chromEnd), NULL, FALSE, itemName);
}

void doOmimAvSnp(struct trackDb *tdb, char *item)
/* Put up OmimGene track info. */
{
genericHeader(tdb, item);
printOmimAvSnpDetails(tdb, item, FALSE);
printTrackHtml(tdb);
}

void doOmimGene2(struct trackDb *tdb, char *item)
/* Put up OmimGene track info. */
{
cartWebStart(cart, database, "OMIM genes - %s", item);
printOmimGene2Details(tdb, item, FALSE);
printTrackHtml(tdb);
}

void doOmimGene(struct trackDb *tdb, char *item)
/* Put up OmimGene track info. */
{
genericHeader(tdb, item);
printOmimGeneDetails(tdb, item, FALSE);
printTrackHtml(tdb);
}

void printRgdSslpCustomUrl(struct trackDb *tdb, char *itemName, boolean encode)
/* Print RGD QTL URL. */
{
char *url = tdb->url;
char *sslpId;
char *chrom, *chromStart, *chromEnd;

if (url != NULL && url[0] != 0)
    {
    struct sqlConnection *conn = hAllocConn(database);
    char query[256];
    struct sqlResult *sr;
    char **row;

    sqlSafef(query, sizeof(query), "select id from rgdSslpLink where name='%s';", itemName);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
	sslpId = row[0];
        printf("<H2>Rat SSLP: %s</H2>", itemName);
        printf("<B>RGD SSLP Report: ");
        printf("<A HREF=\"%s%s\" target=_blank>", url, sslpId);
        printf("RGD:%s</B></A>\n", sslpId);
        }
    sqlFreeResult(&sr);

    sqlSafef(query, sizeof query, "select chrom, chromStart, chromEnd from rgdSslp where name='%s';", itemName);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
        chrom      = row[0];
        chromStart = row[1];
        chromEnd   = row[2];
        printf("<HR>");
        printPosOnChrom(chrom, atoi(chromStart), atoi(chromEnd), NULL, FALSE, itemName);
        }
    sqlFreeResult(&sr);

    hFreeConn(&conn);
    }
}

void doVax003Vax004(struct trackDb *tdb, char *item)
/* Put up VAX 004 info. */
{
char *id;
struct sqlConnection *conn = hAllocConn(database);
char *aliTbl = tdb->table;
int start = cartInt(cart, "o");
char cond_str[255], *subjId;

genericHeader(tdb, item);

id = item;
printf("<H3>Sequence ID: %s", id);
printf("</H3>\n");

/* display subject ID */
sqlSafef(cond_str, sizeof cond_str, "dnaSeqId='%s'", id);
subjId = sqlGetField(database,"gsIdXref", "subjId", cond_str);
printf("<H3>Subject ID: ");
printf("<A HREF=\"../cgi-bin/gsidSubj?hgs_subj=%s\">", subjId);
printf("%s</A>\n", subjId);
printf("</H3>");

/* print alignments that track was based on */
struct psl *pslList = getAlignments(conn, aliTbl, item);
printf("<H3>Genomic Alignments</H3>");
printAlignments(pslList, start, "htcCdnaAli", tdb->table, item);
hFreeConn(&conn);

printTrackHtml(tdb);
}

void doUniGene3(struct trackDb *tdb, char *item)
/* Put up UniGene info. */
{
char *url = tdb->url;
char *id;
struct sqlConnection *conn = hAllocConn(database);
char *aliTbl = tdb->table;
int start = cartInt(cart, "o");

genericHeader(tdb, item);

id = strstr(item, "Hs.")+strlen("Hs.");
printf("<H3>%s UniGene: ", organism);
printf("<A HREF=\"%s%s\" target=_blank>", url, id);
printf("%s</B></A>\n", item);
printf("</H3>\n");

/* print alignments that track was based on */
struct psl *pslList = getAlignments(conn, aliTbl, item);
printf("<H3>Genomic Alignments</H3>");
printAlignments(pslList, start, "htcCdnaAli", tdb->table, item);
hFreeConn(&conn);

printTrackHtml(tdb);
}

void doRgdSslp(struct trackDb *tdb, char *item, char *itemForUrl)
/* Put up Superfamily track info. */
{
if (itemForUrl == NULL)
    itemForUrl = item;

genericHeader(tdb, item);
printRgdSslpCustomUrl(tdb, itemForUrl, item == itemForUrl);
printTrackHtml(tdb);
}

static boolean isBDGPName(char *name)
/* Return TRUE if name is from BDGP (matching {CG,TE,CR}0123{,4}{,-R?})  */
{
int len = strlen(name);
boolean isBDGP = FALSE;
if (startsWith("CG", name) || startsWith("TE", name) || startsWith("CR", name))
    {
    int numNum = 0;
    int i;
    for (i=2;  i < len;  i++)
	{
	if (isdigit(name[i]))
	    numNum++;
	else
	    break;
	}
    if ((numNum >= 4) && (numNum <= 5))
	{
	if (i == len)
	    isBDGP = TRUE;
	else if ((i == len-3) &&
		 (name[i] == '-') && (name[i+1] == 'R') && isalpha(name[i+2]))
	    isBDGP = TRUE;
	}
    }
return(isBDGP);
}

void doRgdGene(struct trackDb *tdb, char *rnaName)
/* Process click on a RGD gene. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
char *sqlRnaName = rnaName;
struct refLink *rl;
char *rgdId;
int start = cartInt(cart, "o");

/* Make sure to escape single quotes for DB parseability */
if (strchr(rnaName, '\''))
    sqlRnaName = replaceChars(rnaName, "'", "''");

cartWebStart(cart, database, "%s", tdb->longLabel);

sqlSafef(query, sizeof(query), "select * from %s where mrnaAcc = '%s'", refLinkTable, sqlRnaName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find %s in %s table - this accession may no longer be available.", rnaName, refLinkTable);
rl = refLinkLoad(row);
sqlFreeResult(&sr);
printf("<H2>Gene %s</H2>\n", rl->name);

sqlSafef(query, sizeof(query), "select id from rgdGeneLink where refSeq = '%s'", sqlRnaName);

sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find %s in rgdGeneLink table - database inconsistency.", rnaName);
rgdId = cloneString(row[0]);
sqlFreeResult(&sr);

printf("<B>RGD Gene Report: </B> <A HREF=\"");
printf("%s%s", tdb->url, rgdId);
printf("\" TARGET=_blank>RGD:%s</A><BR>", rgdId);

printf("<B>NCBI RefSeq: </B> <A HREF=\"");
printEntrezNucleotideUrl(stdout, rl->mrnaAcc);
printf("\" TARGET=_blank>%s</A>", rl->mrnaAcc);

/* If refSeqStatus is available, report it: */
if (sqlTableExists(conn, refSeqStatusTable))
    {
    sqlSafef(query, sizeof(query), "select status from %s where mrnaAcc = '%s'",
	    refSeqStatusTable, sqlRnaName);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
	printf("&nbsp;&nbsp; Status: <B>%s</B>", row[0]);
	}
    sqlFreeResult(&sr);
    }
puts("<BR>");

if (rl->omimId != 0)
    {
    printf("<B>OMIM:</B> <A HREF=\"");
    printEntrezOMIMUrl(stdout, rl->omimId);
    printf("\" TARGET=_blank>%d</A><BR>\n", rl->omimId);
    }
if (rl->locusLinkId != 0)
    {
    printf("<B>Entrez Gene:</B> ");
    printf("<A HREF=\"https://www.ncbi.nlm.nih.gov/entrez/query.fcgi?db=gene&cmd=Retrieve&dopt=Graphics&list_uids=%d\" TARGET=_blank>",
           rl->locusLinkId);
    printf("%d</A><BR>\n", rl->locusLinkId);
    }

htmlHorizontalLine();

/* print alignments that track was based on */
{
char *aliTbl = (sameString(tdb->table, "rgdGene") ? "refSeqAli" : "xenoRGDAli");
struct psl *pslList = getAlignments(conn, aliTbl, rl->mrnaAcc);
printf("<H3>mRNA/Genomic Alignments</H3>");
printAlignments(pslList, start, "htcCdnaAli", aliTbl, rl->mrnaAcc);
}

htmlHorizontalLine();

geneShowPosAndLinks(rl->mrnaAcc, rl->protAcc, tdb, refPepTable, "htcTranslatedProtein",
		    "htcRefMrna", "htcGeneInGenome", "mRNA Sequence");

printTrackHtml(tdb);
hFreeConn(&conn);
}

void doRgdGene2(struct trackDb *tdb, char *rgdGeneId)
/* Process click on a RGD gene. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
char *sqlRnaName = rgdGeneId;
char *rgdId = NULL;
char *chp;
char *GeneID, *Name, *note;
char *rgdPathwayName;

/* Make sure to escape single quotes for DB parseability */
if (strchr(rgdGeneId, '\''))
    sqlRnaName = replaceChars(rgdGeneId, "'", "''");

cartWebStart(cart, database, "%s", tdb->longLabel);

chp = strstr(rgdGeneId, ":");
if (chp != NULL)
    {
    chp++;
    rgdId = strdup(chp);
    }
else
    {
    errAbort("Couldn't find %s.", rgdGeneId);
    }

sqlSafef(query, sizeof(query), "select GeneID, Name, note from rgdGeneXref where rgdGeneId = '%s'", sqlRnaName);

sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find %s in rgdGeneXref table - database inconsistency.", rgdGeneId);
GeneID = cloneString(row[0]);
Name   = cloneString(row[1]);
note   = cloneString(row[2]);

sqlFreeResult(&sr);

printf("<H2>Gene %s</H2>\n", Name);
printf("<B>RGD Gene Report: </B> <A HREF=\"");
printf("%s%s", tdb->url, rgdId);
printf("\" TARGET=_blank>RGD:%s</A>", rgdId);

printf("<BR><B>GeneID: </B> %s ", GeneID);
printf("<BR><B>Gene Name: </B> %s ", Name);
printf("<BR><B>Note: </B> %s ", note);

sqlSafef(query, sizeof(query), "select extAC from rgdGeneXref2 where rgdGeneId = '%s' and extDB='IMAGE'", rgdGeneId);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    char *image;
    image = cloneString(row[0]);
    printf("<BR><B>IMAGE Clone: </B>");
    printf("<A HREF=\"");
    printf("%s%s", "http://www.imageconsortium.org/IQ/bin/singleCloneQuery?clone_id=", image);
    printf("\" TARGET=_blank> %s</A>", image);
    row = sqlNextRow(sr);
    while (row != NULL)
	{
	image = cloneString(row[0]);
	printf(", <A HREF=\"");
	printf("%s%s", "http://www.imageconsortium.org/IQ/bin/singleCloneQuery?clone_id=", image);
	printf("\" TARGET=_blank>%s</A>", image);
        row = sqlNextRow(sr);
	}
    }
sqlFreeResult(&sr);

sqlSafef(query, sizeof(query), "select extAC from rgdGeneXref2 where rgdGeneId = '%s' and extDB='MGC'", rgdGeneId);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    char *mgc;
    mgc = cloneString(row[0]);
    printf("<BR><B>MGC: </B>");
    printf("<A HREF=\"");
    printf("%s%s", "http://mgc.nci.nih.gov/Genes/CloneList?ORG=Rn&LIST=", mgc);
    printf("\" TARGET=_blank> %s</A>", mgc);
    row = sqlNextRow(sr);
    while (row != NULL)
	{
	mgc = cloneString(row[0]);
	printf(", <A HREF=\"");
	printf("%s%s", "http://mgc.nci.nih.gov/Genes/CloneList?ORG=Rn&LIST=", mgc);
	printf("\" TARGET=_blank>%s</A>", mgc);
        row = sqlNextRow(sr);
	}
    }
sqlFreeResult(&sr);

htmlHorizontalLine();
printf("<H3>RGD Pathway(s)</H3>\n");
sqlSafef(query, sizeof(query),
"select p.rgdPathwayId, p.name from rgdGenePathway g, rgdPathway p where g.rgdGeneId = '%s' and g.rgdPathwayId=p.rgdPathwayId", rgdGeneId);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find %s in rgdGenePathway table - database inconsistency.", rgdGeneId);
printf("<UL>");
while (row != NULL)
    {
    rgdPathwayName = cloneString(row[1]);
    printf("<LI><B>%s</B><BR>", rgdPathwayName);
    row = sqlNextRow(sr);
    }
sqlFreeResult(&sr);
printf("</UL>");
printf("<A HREF=\"");
printf("%s%s%s", "http://rgd.mcw.edu/tools/genes/gene_ont_view.cgi?id=", rgdId, "#Pathway");
printf("\" TARGET=_blank> %s</A> </H3>", "Click here for more RGD pathway details related to this gene...");

htmlHorizontalLine();

printTrackHtml(tdb);
hFreeConn(&conn);
}
char *getRefSeqCdsCompleteness(struct sqlConnection *conn, char *acc)
/* get description of RefSeq CDS completeness or NULL if not available */
{
/* table mapping names to descriptions */
static char *cmplMap[][2] =
    {
    {"Unknown", "completeness unknown"},
    {"Complete5End", "5' complete"},
    {"Complete3End", "3' complete"},
    {"FullLength", "full length"},
    {"IncompleteBothEnds", "5' and 3' incomplete"},
    {"Incomplete5End", "5' incomplete"},
    {"Incomplete3End", "3' incomplete"},
    {"Partial", "partial"},
    {NULL, NULL}
    };
if (sqlTableExists(conn, refSeqSummaryTable))
    {
    char query[256], buf[64], *cmpl;
    int i;
    sqlSafef(query, sizeof(query),
          "select completeness from %s where mrnaAcc = '%s'",
          refSeqSummaryTable, acc);
    cmpl = sqlQuickQuery(conn, query, buf, sizeof(buf));
    if (cmpl != NULL)
        {
        for (i = 0; cmplMap[i][0] != NULL; i++)
            {
            if (sameString(cmpl, cmplMap[i][0]))
                return cmplMap[i][1];
            }
        }
    }
return NULL;
}

char *getRefSeqSummary(struct sqlConnection *conn, char *acc)
/* RefSeq summary or NULL if not available; free result */
{
char * summary = NULL;
if (sqlTableExists(conn, refSeqSummaryTable))
    {
    char query[256];
    sqlSafef(query, sizeof(query),
          "select summary from %s where mrnaAcc = '%s'", refSeqSummaryTable, acc);
    summary = sqlQuickString(conn, query);
    summary = abbreviateRefSeqSummary(summary);
    }

return summary;
}

char *geneExtraImage(char *geneFileBase)
/* check if there is a geneExtra image for the specified gene, if so return
 * the relative URL in a static buffer, or NULL if it doesn't exist */
{
static char *imgExt[] = {"png", "gif", "jpg", NULL};
static char path[256];
int i;

for (i = 0; imgExt[i] != NULL; i++)
    {
    safef(path, sizeof(path), "../htdocs/geneExtra/%s.%s", geneFileBase, imgExt[i]);
    if (access(path, R_OK) == 0)
        {
        safef(path, sizeof(path), "../geneExtra/%s.%s", geneFileBase, imgExt[i]);
        return path;
        }
    }
return NULL;
}

void addGeneExtra(char *geneName)
/* create html table columns with geneExtra data, see hgdocs/geneExtra/README
 * for details */
{
char geneFileBase[256], *imgPath, textPath[256];

/* lower-case gene name used as key */
safef(geneFileBase, sizeof(geneFileBase), "%s", geneName);
tolowers(geneFileBase);

/* add image column, if exists */
imgPath = geneExtraImage(geneFileBase);

if (imgPath != NULL)
    printf("<td><img src=\"%s\">", imgPath);

/* add text column, if exists */
safef(textPath, sizeof(textPath), "../htdocs/geneExtra/%s.txt", geneFileBase);
if (access(textPath, R_OK) == 0)
    {
    FILE *fh = mustOpen(textPath, "r");
    printf("<td valign=\"center\">");
    copyOpenFile(fh, stdout);
    fclose(fh);
    }
}

int gbCdnaGetVersion(struct sqlConnection *conn, char *acc)
/* return mrna/est version, or 0 if not available */

{
int ver = 0;
if (!sqlTableExists(conn, gbCdnaInfoTable))
    {
    warn("Genbank information not shown below, the table %s is not installed "
        "on this server. ", gbCdnaInfoTable);
    //"The information below is a shortened version of the one shown on the "
    //"<a href=\"http://genome.ucsc.edu\">UCSC site</a>", database);
    return 0;
    }

if (hHasField(database, gbCdnaInfoTable, "version"))
    {
    char query[128];
    sqlSafef(query, sizeof(query),
          "select version from %s where acc = '%s'", gbCdnaInfoTable, acc);
    ver = sqlQuickNum(conn, query);
    }
return ver;
}

static void prRefGeneXenoInfo(struct sqlConnection *conn, struct refLink *rl)
/* print xeno refseq info, including linking to the browser, if any  */
{
char query[256];
sqlSafef(query, sizeof(query), "select o.name from %s g,%s o "
      "where (g.acc = '%s') and (o.id = g.organism)",
      gbCdnaInfoTable, organismTable, rl->mrnaAcc);
char *org = sqlQuickString(conn, query);
if (org == NULL)
    org = cloneString("unknown");
printf("<B>Organism:</B> %s<BR>", org);
char *xenoDb = hDbForSciName(org);
if ((xenoDb != NULL) && hDbIsActive(xenoDb) && hTableExists(xenoDb, "refSeqAli"))
    {
    printf("<B>UCSC browser: </B> \n");
    linkToOtherBrowserSearch(xenoDb, rl->mrnaAcc);
    printf("%s on %s (%s)</B> \n", rl->mrnaAcc, hOrganism(xenoDb), xenoDb);
    printf("</A><BR>");
    }
freeMem(org);
}

void prRefGeneInfo(struct sqlConnection *conn, char *rnaName,
                   char *sqlRnaName, struct refLink *rl, boolean isXeno)
/* print basic details information and links for a RefGene */
{
struct sqlResult *sr;
char **row;
char query[256];
int ver = gbCdnaGetVersion(conn, rl->mrnaAcc);
char *cdsCmpl = NULL;

printf("<td valign=top nowrap>\n");
if (isXeno)
    {
    if (startsWith("panTro", database))
        printf("<H2>Other RefSeq Gene %s</H2>\n", rl->name);
    else
        printf("<H2>Non-%s RefSeq Gene %s</H2>\n", organism, rl->name);
    }
else
    printf("<H2>RefSeq Gene %s</H2>\n", rl->name);
printf("<B>RefSeq:</B> <A HREF=\"");
printEntrezNucleotideUrl(stdout, rl->mrnaAcc);
if (ver > 0)
    printf("\" TARGET=_blank>%s.%d</A>", rl->mrnaAcc, ver);
else
    printf("\" TARGET=_blank>%s</A>", rl->mrnaAcc);

/* If refSeqStatus is available, report it: */
if (sqlTableExists(conn, refSeqStatusTable))
    {
    sqlSafef(query, sizeof(query), "select status from %s where mrnaAcc = '%s'",
          refSeqStatusTable, sqlRnaName);
    char *stat = sqlQuickString(conn, query);
    if (stat != NULL)
	printf("&nbsp;&nbsp; <B>Status: </B>%s", stat);
    }
puts("<BR>");
char *desc = gbCdnaGetDescription(conn, rl->mrnaAcc);
if (desc != NULL)
    {
    printf("<B>Description:</B> ");
    htmlTextOut(desc);
    printf("<BR>\n");
    }

if (isXeno)
    prRefGeneXenoInfo(conn, rl);
else
    printCcdsForSrcDb(conn, rl->mrnaAcc);

cdsCmpl = getRefSeqCdsCompleteness(conn, sqlRnaName);
if (cdsCmpl != NULL)
    {
    printf("<B>CDS:</B> %s<BR>", cdsCmpl);
    }
if (rl->omimId != 0)
    {
    printf("<B>OMIM:</B> <A HREF=\"");
    printEntrezOMIMUrl(stdout, rl->omimId);
    printf("\" TARGET=_blank>%d</A><BR>\n", rl->omimId);
    }
if (rl->locusLinkId != 0)
    {
    printf("<B>Entrez Gene:</B> ");
    printf("<A HREF=\"https://www.ncbi.nlm.nih.gov/entrez/query.fcgi?db=gene&cmd=Retrieve&dopt=Graphics&list_uids=%d\" TARGET=_blank>",
           rl->locusLinkId);
    printf("%d</A><BR>\n", rl->locusLinkId);

    if ( (strstr(database, "mm") != NULL) && hTableExists(database, "MGIid"))
        {
        char *mgiID;
	sqlSafef(query, sizeof(query), "select MGIid from MGIid where LLid = '%d';",
		rl->locusLinkId);

	sr = sqlGetResult(conn, query);
	if ((row = sqlNextRow(sr)) != NULL)
	    {
	    printf("<B>Mouse Genome Informatics:</B> ");
	    mgiID = cloneString(row[0]);

	    printf("<A HREF=\"http://www.informatics.jax.org/marker/%s\" TARGET=_BLANK>%s</A><BR>\n",mgiID, mgiID);
	    }
	else
	    {
	    /* per Carol Bult from Jackson Lab 4/12/02, JAX do not always agree
	     * with Locuslink on seq to gene association.
	     * Thus, not finding a MGIid even if a LocusLink ID
	     * exists is always a possibility. */
	    }
	sqlFreeResult(&sr);
	}
    }
if (!startsWith("Worm", organism))
    {
    if (startsWith("dm", database))
	{
        /* PubMed never seems to have BDGP gene IDs... so if that's all
         * that's given for a name/product, ignore name / truncate product. */
	char *cgp = strstr(rl->product, "CG");
	if (cgp != NULL)
	    {
	    char *cgWord = firstWordInLine(cloneString(cgp));
	    char *dashp = strchr(cgWord, '-');
	    if (dashp != NULL)
		*dashp = 0;
	    if (isBDGPName(cgWord))
		*cgp = 0;
	    }
	if (! isBDGPName(rl->name))
	    medlineLinkedLine("PubMed on Gene", rl->name, rl->name);
	if (rl->product[0] != 0)
	    medlineProductLinkedLine("PubMed on Product", rl->product);
	}
    else
	{
	medlineLinkedLine("PubMed on Gene", rl->name, rl->name);
	if (rl->product[0] != 0)
	    medlineProductLinkedLine("PubMed on Product", rl->product);
	}
    printf("\n");
    printGeneCards(rl->name);
    }
if (hTableExists(database, "jaxOrtholog"))
    {
    struct jaxOrtholog jo;
    char * sqlRlName = rl->name;

    /* Make sure to escape single quotes for DB parseability */
    if (strchr(rl->name, '\''))
        {
        sqlRlName = replaceChars(rl->name, "'", "''");
        }
    sqlSafef(query, sizeof(query), "select * from jaxOrtholog where humanSymbol='%s'", sqlRlName);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	jaxOrthologStaticLoad(row, &jo);
	printf("<B>MGI Mouse Ortholog:</B> ");
	printf("<A HREF=\"http://www.informatics.jax.org/marker/%s\" target=_BLANK>", jo.mgiId);
	printf("%s</A><BR>\n", jo.mouseSymbol);
	}
    sqlFreeResult(&sr);
    }
if (startsWith("hg", database))
    {
    printf("\n");
    printf("<B>AceView:</B> ");
    printf("<A HREF = \"https://www.ncbi.nlm.nih.gov/IEB/Research/Acembly/av.cgi?db=human&l=%s\" TARGET=_blank>",
	   rl->name);
    printf("%s</A><BR>\n", rl->name);
    }
prGRShortRefGene(rl->name);

}

void prKnownGeneInfo(struct sqlConnection *conn, char *rnaName,
                   char *sqlRnaName, struct refLink *rl)
/* print basic details information and links for a Known Gene */
{
struct sqlResult *sr;
char **row;
char query[256];
int ver = gbCdnaGetVersion(conn, rl->mrnaAcc);
char *cdsCmpl = NULL;

printf("<td valign=top nowrap>\n");

printf("<H2>Known Gene %s</H2>\n", rl->name);
printf("<B>KnownGene:</B> <A HREF=\"");
printEntrezNucleotideUrl(stdout, rl->mrnaAcc);
if (ver > 0)
    printf("\" TARGET=_blank>%s.%d</A>", rl->mrnaAcc, ver);
else
    printf("\" TARGET=_blank>%s</A>", rl->mrnaAcc);
fflush(stdout);

puts("<BR>");
char *desc = gbCdnaGetDescription(conn, rl->mrnaAcc);
if (desc != NULL)
    {
    printf("<B>Description:</B> ");
    htmlTextOut(desc);
    printf("<BR>\n");
    }

printCcdsForSrcDb(conn, rl->mrnaAcc);

cdsCmpl = getRefSeqCdsCompleteness(conn, sqlRnaName);
if (cdsCmpl != NULL)
    {
    printf("<B>CDS:</B> %s<BR>", cdsCmpl);
    }
if (rl->omimId != 0)
    {
    printf("<B>OMIM:</B> <A HREF=\"");
    printEntrezOMIMUrl(stdout, rl->omimId);
    printf("\" TARGET=_blank>%d</A><BR>\n", rl->omimId);
    }
if (rl->locusLinkId != 0)
    {
    printf("<B>Entrez Gene:</B> ");
    printf("<A HREF=\"https://www.ncbi.nlm.nih.gov/entrez/query.fcgi?db=gene&cmd=Retrieve&dopt=Graphics&list_uids=%d\" TARGET=_blank>",
           rl->locusLinkId);
    printf("%d</A><BR>\n", rl->locusLinkId);

    if ( (strstr(database, "mm") != NULL) && hTableExists(database, "MGIid"))
        {
        char *mgiID;
	sqlSafef(query, sizeof(query), "select MGIid from MGIid where LLid = '%d';",
		rl->locusLinkId);

	sr = sqlGetResult(conn, query);
	if ((row = sqlNextRow(sr)) != NULL)
	    {
	    printf("<B>Mouse Genome Informatics:</B> ");
	    mgiID = cloneString(row[0]);

	    printf("<A HREF=\"http://www.informatics.jax.org/marker/%s\" TARGET=_BLANK>%s</A><BR>\n",mgiID, mgiID);
	    }
	else
	    {
	    /* per Carol Bult from Jackson Lab 4/12/02, JAX do not always agree
	     * with Locuslink on seq to gene association.
	     * Thus, not finding a MGIid even if a LocusLink ID
	     * exists is always a possibility. */
	    }
	sqlFreeResult(&sr);
	}
    }
}

void doKnownGene(struct trackDb *tdb, char *rnaName)
/* Process click on a known gene. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
char *kgId = cartString(cart, "i");
char *sqlRnaName = rnaName;
char *summary = NULL;
struct refLink rlR;
struct refLink *rl;
int start = cartInt(cart, "o");
int left = cartInt(cart, "l");
int right = cartInt(cart, "r");
char *chrom = cartString(cart, "c");
/* Make sure to escape single quotes for DB parseability */
if (strchr(rnaName, '\''))
    {
    sqlRnaName = replaceChars(rnaName, "'", "''");
    }
/* get refLink entry */
if (strstr(rnaName, "NM_") != NULL)
    {
    sqlSafef(query, sizeof(query), "select * from %s where mrnaAcc = '%s'", refLinkTable, sqlRnaName);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) == NULL)
        errAbort("Couldn't find %s in %s table - this accession may no longer be available.",
                 rnaName, refLinkTable);
    rl = refLinkLoad(row);
    sqlFreeResult(&sr);
    }
else
    {
    rlR.name    = strdup(kgId);
    rlR.mrnaAcc = strdup(kgId);
    rlR.locusLinkId = 0;
    rl = &rlR;
    }

cartWebStart(cart, database, "Known Gene");
printf("<table border=0>\n<tr>\n");
prKnownGeneInfo(conn, rnaName, sqlRnaName, rl);

printf("</tr>\n</table>\n");

/* optional summary text */
summary = getRefSeqSummary(conn, kgId);
if (summary != NULL)
    {
    htmlHorizontalLine();
    printf("<H3>Summary of %s</H3>\n", kgId);
    printf("<P>%s</P>\n", summary);
    freeMem(summary);
    }
htmlHorizontalLine();

/* print alignments that track was based on */
{
char *aliTbl;

if (strstr(kgId, "NM_"))
    {
    aliTbl = strdup("refSeqAli");
    }
else
    {
    aliTbl = strdup("all_mrna");
    }
struct psl *pslList = getAlignments(conn, aliTbl, rl->mrnaAcc);
printf("<H3>mRNA/Genomic Alignments</H3>");
printAlignments(pslList, start, "htcCdnaAli", aliTbl, kgId);
}
htmlHorizontalLine();

struct palInfo *palInfo = NULL;

if (genbankIsRefSeqCodingMRnaAcc(rnaName))
    {
    AllocVar(palInfo);
    palInfo->chrom = chrom;
    palInfo->left = left;
    palInfo->right = right;
    palInfo->rnaName = rnaName;
    }

geneShowPosAndLinksPal(rl->mrnaAcc, rl->protAcc, tdb, refPepTable, "htcTranslatedProtein",
		    "htcRefMrna", "htcGeneInGenome", "mRNA Sequence",palInfo);

printTrackHtml(tdb);
hFreeConn(&conn);
}

static struct refLink *printRefSeqInfo( struct sqlConnection *conn, struct trackDb *tdb, char *rnaName, char *version)
{
struct sqlResult *sr;
char **row;
char query[256];
char *sqlRnaName = rnaName;
char *summary = NULL;
boolean isXeno = sameString(tdb->table, "xenoRefGene");
struct refLink *rl;

/* Make sure to escape single quotes for DB parseability */
if (strchr(rnaName, '\''))
    {
    sqlRnaName = replaceChars(rnaName, "'", "''");
    }
/* get refLink entry */
if (version == NULL)
    {
    sqlSafef(query, sizeof(query), "select * from %s  where mrnaAcc = '%s'", refLinkTable, sqlRnaName);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) == NULL)
        errAbort("This accession (%s) is no longer in our database. Check NCBI for status on this accession.", rnaName); 
    rl = refLinkLoad(row);
    sqlFreeResult(&sr);
    }
else
    {
    sqlSafef(query, sizeof(query), "select * from %s r, %s g where mrnaAcc = '%s' and r.mrnaAcc=g.acc and g.version='%s'", refLinkTable,gbCdnaInfoTable, sqlRnaName, version);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) == NULL)
	{
	sqlFreeResult(&sr);
	return NULL;
	}
    rl = refLinkLoad(row);
    sqlFreeResult(&sr);
    }

/* print the first section with info  */
printf("<table border=0>\n<tr>\n");
prRefGeneInfo(conn, rnaName, sqlRnaName, rl, isXeno);
addGeneExtra(rl->name);  /* adds columns if extra info is available */

printf("</tr>\n</table>\n");

/* optional summary text */
summary = getRefSeqSummary(conn, sqlRnaName);
if (summary != NULL)
    {
    htmlHorizontalLine();
    printf("<H3>Summary of %s</H3>\n", rl->name);
    printf("<P>%s</P>\n", summary);
    freeMem(summary);
    }
htmlHorizontalLine();

return rl;
}

void doNcbiRefSeq(struct trackDb *tdb, char *itemName)
/* Process click on a NCBI RefSeq gene. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
struct ncbiRefSeqLink *nrl;

cartWebStart(cart, database, "%s - %s ", tdb->longLabel, itemName);

/* get refLink entry */
sqlSafef(query, sizeof(query), "select * from ncbiRefSeqLink where id = '%s'", itemName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find %s in ncbiRefSeqLink table.", itemName);
nrl = ncbiRefSeqLinkLoad(row);
sqlFreeResult(&sr);

printf("<h2>RefSeq Gene %s</h2><br>\n", nrl->name);
printf("<b>RefSeq:</b> <a href='");
printEntrezNucleotideUrl(stdout, nrl->id);
printf("' target=_blank>%s</a>", nrl->id);
printf("&nbsp;&nbsp;<b>Status: </b>%s<br>\n", nrl->status);
printf("<b>Description:</b> %s<br>\n", nrl->product);
if (differentWord(nrl->gbkey, ""))
    {
    printf("<b>Molecule type:</b> %s<br>\n", nrl->gbkey);
    }
if (differentWord(nrl->pseudo, ""))
    {
    printf("<b>Pseudogene:</b> %s<br>\n", nrl->pseudo);
    }
if (differentWord(nrl->source, ""))
    {
    printf("<b>Source:</b> %s<br>\n", nrl->source);
    }
if (differentWord(nrl->gene_biotype, ""))
    {
    printf("<b>Biotype:</b> %s<br>\n", nrl->gene_biotype);
    }
if (differentWord(nrl->gene_synonym, ""))
    {
    printf("<b>Synonyms:</b> %s<br>\n", nrl->gene_synonym);
    }
if (differentWord(nrl->ncrna_class, ""))
    {
    printf("<b>ncRNA class:</b> %s<br>\n", nrl->ncrna_class);
    }
if (differentWord(nrl->note, ""))
    {
    printf("<b>Other notes:</b> %s<br>\n", nrl->note);
    }
if (differentWord(nrl->omimId, ""))
    {
    printf("<b>OMIM:</b> <a href='");
    printEntrezOMIMUrl(stdout, sqlSigned(nrl->omimId));
    printf("' target=_blank>%s</a><br>\n", nrl->omimId);
    }
if (differentWord(nrl->mrnaAcc, "") && differentWord(nrl->mrnaAcc,nrl->id))
    {
    printf("<b>mRNA:</b> ");
    printf("<a href='https://www.ncbi.nlm.nih.gov/nuccore/%s' target=_blank>", nrl->mrnaAcc);
    printf("%s</a><br>\n", nrl->mrnaAcc);
    }
if (differentWord(nrl->genbank, "") && differentWord(nrl->genbank,nrl->id))
    {
    printf("<b>Genbank:</b> ");
    printf("<a href='https://www.ncbi.nlm.nih.gov/nuccore/%s' target=_blank>", nrl->genbank);
    printf("%s</a><br>\n", nrl->genbank);
    }
if (differentWord(nrl->protAcc, ""))
    {
    printf("<b>Protein:</b> ");
    printf("<a href='https://www.ncbi.nlm.nih.gov/protein/%s' target=_blank>", nrl->protAcc);
    printf("%s</a><br>\n", nrl->protAcc);
    }

if (differentWord(nrl->hgnc, ""))
    {  /* legacy support of mm10 override of hgnc column until this table
        * is updated on the RR */
    if (startsWith("MGI", nrl->hgnc))
        {
        printf("<b>MGI:</b> "
           "<a href=\"http://www.informatics.jax.org/marker/%s\" target=_blank>%s</a><br>\n",
           nrl->hgnc, nrl->hgnc);
        }
    else
        {
         printf("<b>HGNC:</b> ");
         printf("<a href='http://www.genenames.org/data/gene-symbol-report/#!/hgnc_id/%s' target=_blank>", nrl->hgnc);

         printf("%s</a><br>\n", nrl->hgnc);
        }
    }

if (sqlColumnExists(conn, "ncbiRefSeqLink", "externalId"))
    {
    if (differentWord(nrl->externalId, ""))
	{
	char *urlsStr = trackDbSetting(tdb, "dbPrefixUrls");
	struct hash* dbToUrl = hashFromString(urlsStr);
	char *labelsStr = trackDbSetting(tdb, "dbPrefixLabels");
	struct hash* dbToLabel = hashFromString(labelsStr);
	if (dbToUrl)
	    {
	    if (!dbToLabel)
	        errAbort("can not find trackDb dbPrefixLabels to correspond with dbPrefixUrls\n");
	    char *databasePrefix = cloneString(database);
	    while (strlen(databasePrefix) && isdigit(lastChar(databasePrefix)))
	        trimLastChar(databasePrefix);
	    struct hashEl *hel = hashLookup(dbToUrl, databasePrefix);
	    if (hel)
		{
		struct hashEl *label = hashLookup(dbToLabel, databasePrefix);
		if (!label)
		    errAbort("missing trackDb dbPrefixLabels for database prefix: '%s'\n", databasePrefix);
		char *url = (char *)hel->val;
		char *labelStr = (char *)label->val;
		char *idUrl = replaceInUrl(url, nrl->externalId, cart, database,
		    nrl->externalId, winStart, winEnd, tdb->track, TRUE, NULL);
		printf("<b>%s:</b> ", labelStr);
		printf("<a href='%s' target='_blank'>%s</a><br>\n",
		    idUrl, nrl->externalId);
                }
	    }
	}
    }

if (differentWord(nrl->locusLinkId, ""))
    {
    printf("<b>Entrez Gene:</b> ");
    printf("<a href='https://www.ncbi.nlm.nih.gov/entrez/query.fcgi?db=gene&cmd=Retrieve&dopt=Graphics&list_uids=%s' TARGET=_blank>",
           nrl->locusLinkId);
    printf("%s</a><br>\n", nrl->locusLinkId);
    }

if (differentWord(nrl->name,""))
    {
    printGeneCards(nrl->name);
    if (startsWith("hg", database))
        {
        printf("<b>AceView:</b> ");
        printf("<a href = 'https://www.ncbi.nlm.nih.gov/IEB/Research/Acembly/av.cgi?db=human&l=%s' target=_blank>",
	   nrl->name);
        printf("%s</a><br>\n", nrl->name);
        }
    }
htmlHorizontalLine();
if (differentWord("", nrl->description))
    {
    printf("Summary of <b>%s</b><br>\n%s<br>\n", nrl->name, nrl->description);
    htmlHorizontalLine();
    }

static boolean hasSequence = TRUE;
struct psl *pslList = getAlignments(conn, "ncbiRefSeqPsl", itemName);
// if the itemName isn't found, it might be found as the nrl->mrnaAcc
if (! pslList)
    pslList = getAlignments(conn, "ncbiRefSeqPsl", nrl->mrnaAcc);
if (pslList)
    {
    char query[256];
    /* verify itemName has RNA sequence to work with */
    sqlSafef(query, sizeof(query), "select id from seqNcbiRefSeq where acc='%s' limit 1", itemName);
    char * result= sqlQuickString(conn, query);
    if (isEmpty(result))
        {
        printf ("<h4>No sequence available for %s, can't display alignment.</h4>\n", itemName);
        hasSequence = FALSE;
        }
    else
        {
        printf("<H3>mRNA/Genomic Alignments (%s)</H3>", itemName);
        int start = cartInt(cart, "o");
        printAlignments(pslList, start, "htcCdnaAli", "ncbiRefSeqPsl", itemName);
        }
    }
else
    {
    printf ("<h4>Missing alignment for %s</h4><br>\n", itemName);
    }

htmlHorizontalLine();

if (! ( sameString(tdb->track, "ncbiRefSeqPsl") || sameString(tdb->track, "ncbiRefSeqOther" ) ) )
    showGenePos(itemName, tdb);

printf("<h3>Links to sequence:</h3>\n");
printf("<ul>\n");
if (differentWord("", nrl->protAcc))
    {
    puts("<li>\n");
    hgcAnchorSomewhere("htcTranslatedProtein", nrl->protAcc, "ncbiRefSeqPepTable", seqName);
    printf("Predicted Protein</a> \n");
    puts("</li>\n");
    }
if (hasSequence)
    {
    puts("<li>\n");
    hgcAnchorSomewhere("ncbiRefSeqSequence", itemName, "ncbiRefSeqPsl", seqName);
    printf("%s</a> may be different from the genomic sequence.\n",
	   "Predicted mRNA");
    puts("</li>\n");
    }
puts("<LI>\n");
hgcAnchorSomewhere("getDna", itemName, tdb->track, seqName);
printf("Genomic Sequence</A> from assembly\n");
puts("</LI>\n");

printf("</ul>\n");

printTrackHtml(tdb);
hFreeConn(&conn);
}

void doRefGene(struct trackDb *tdb, char *rnaName)
/* Process click on a known RefSeq gene. */
{
struct sqlConnection *conn = hAllocConn(database);
int start = cartInt(cart, "o");
int left = cartInt(cart, "l");
int right = cartInt(cart, "r");
char *chrom = cartString(cart, "c");

boolean isXeno = sameString(tdb->table, "xenoRefGene");
if (isXeno)
    cartWebStart(cart, database, "Non-%s RefSeq Gene", organism);
else
    cartWebStart(cart, database, "RefSeq Gene");
struct refLink *rl = printRefSeqInfo( conn, tdb, rnaName, NULL);

/* print alignments that track was based on */
char *aliTbl = (sameString(tdb->table, "refGene") ? "refSeqAli" : "xenoRefSeqAli");
if (hTableExists(database, aliTbl))
    {
    struct psl *pslList = getAlignments(conn, aliTbl, rl->mrnaAcc);
    printf("<H3>mRNA/Genomic Alignments</H3>");
    printAlignments(pslList, start, "htcCdnaAli", aliTbl, rl->mrnaAcc);
    }
else
    warn("Sequence alignment links not shown below, the table %s.refSeqAli is not installed " 
            "on this server", database);

htmlHorizontalLine();

struct palInfo *palInfo = NULL;

if (genbankIsRefSeqCodingMRnaAcc(rnaName))
    {
    AllocVar(palInfo);
    palInfo->chrom = chrom;
    palInfo->left = left;
    palInfo->right = right;
    palInfo->rnaName = rnaName;
    }

geneShowPosAndLinksPal(rl->mrnaAcc, rl->protAcc, tdb, refPepTable, "htcTranslatedProtein",
		    "htcRefMrna", "htcGeneInGenome", "mRNA Sequence",palInfo);

printTrackHtml(tdb);
hFreeConn(&conn);
}

char *kgIdToSpId(struct sqlConnection *conn, char* kgId)
/* get the swissprot id for a known genes id; resulting string should be
 * freed */
{
char query[512];
sqlSafef(query, sizeof(query), "select spID from kgXref where kgID='%s'", kgId);
return sqlNeedQuickString(conn, query);
}

void doHInvGenes(struct trackDb *tdb, char *item)
/* Process click on H-Invitational genes track. */
{
struct sqlConnection *conn = hAllocConn(database);
char query[256];
struct sqlResult *sr;
char **row;
int start = cartInt(cart, "o");
struct psl *pslList = NULL;
struct HInv *hinv;

/* Print non-sequence info. */
genericHeader(tdb, item);

sqlSafef(query, sizeof(query), "select * from HInv where geneId = '%s'", item);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hinv = HInvLoad(row);
    if (hinv != NULL)
	{
        printf("<B> Gene ID: </B> <A HREF=\"http://www.jbirc.jbic.or.jp/hinv/soup/pub_Detail.pl?acc_id=%s\" TARGET=_blank> %s <BR></A>",
                hinv->mrnaAcc, hinv->geneId );
        printf("<B> Cluster ID: </B> <A HREF=\"http://www.jbirc.jbic.or.jp/hinv/soup/pub_Locus.pl?locus_id=%s\" TARGET=_blank> %s <BR></A>",
                hinv->clusterId, hinv->clusterId );
        printf("<B> cDNA Accession: </B> <A HREF=\"http://getentry.ddbj.nig.ac.jp/cgi-bin/get_entry.pl?%s\" TARGET=_blank> %s <BR></A>",
                hinv->mrnaAcc, hinv->mrnaAcc );
        }
    }
htmlHorizontalLine();

/* print alignments that track was based on */
pslList = getAlignments(conn, "HInvGeneMrna", item);
puts("<H3>mRNA/Genomic Alignments</H3>");
printAlignments(pslList, start, "htcCdnaAli", "HInvGeneMrna", item);

printTrackHtml(tdb);
hFreeConn(&conn);
}

char *getGi(char *ncbiFaHead)
/* Get GI number from NCBI FA format header. */
{
char *s;
static char gi[64];

if (!startsWith("gi|", ncbiFaHead))
    return NULL;
ncbiFaHead += 3;
strncpy(gi, ncbiFaHead, sizeof(gi));
s = strchr(gi, '|');
if (s != NULL)
    *s = 0;
return trimSpaces(gi);
}

void showSAM_T02(char *itemName)
{
char query2[256];
struct sqlResult *sr2;
char **row2;
struct sqlConnection *conn2 = hAllocConn(database);
char cond_str[256];
char *predFN;
char *homologID;
char *SCOPdomain;
char *chain;
char goodSCOPdomain[40];
int  first = 1;
float  eValue;
char *chp;
int homologCount;
int gotPDBFile;

printf("<B>Protein Structure Analysis and Prediction by ");
printf("<A HREF=\"http://www.soe.ucsc.edu/research/compbio/SAM_T02/sam-t02-faq.html\"");
printf(" TARGET=_blank>SAM-T02</A></B><BR><BR>\n");

printf("<B>Multiple Alignment:</B> ");
/* printf("<A HREF=\"http://www.soe.ucsc.edu/~karplus/SARS/%s/summary.html#alignment",  */
printf("<A HREF=\"../SARS/%s/summary.html#alignment",
       itemName);
printf("\" TARGET=_blank>%s</A><BR>\n", itemName);

printf("<B>Secondary Structure Predictions:</B> ");
/* printf("<A HREF=\"http://www.soe.ucsc.edu/~karplus/SARS/%s/summary.html#secondary-structure",  */
printf("<A HREF=\"../SARS/%s/summary.html#secondary-structure",
       itemName);
printf("\" TARGET=_blank>%s</A><BR>\n", itemName);

printf("<B>3D Structure Prediction (PDB file):</B> ");
gotPDBFile = 0;
sqlSafef(cond_str, sizeof(cond_str), "proteinID='%s' and evalue <1.0e-5;", itemName);
if (sqlGetField(database, "protHomolog", "proteinID", cond_str) != NULL)
    {
    sqlSafef(cond_str, sizeof(cond_str), "proteinID='%s'", itemName);
    predFN = sqlGetField(database, "protPredFile", "predFileName", cond_str);
    if (predFN != NULL)
	{
	printf("<A HREF=\"../SARS/%s/", itemName);
	/* printf("%s.t2k.undertaker-align.pdb\">%s</A><BR>\n", itemName,itemName); */
	printf("%s\">%s</A><BR>\n", predFN,itemName);
	gotPDBFile = 1;
	}
    }
if (!gotPDBFile)
    {
    printf("No high confidence level structure prediction available for this sequence.");
    printf("<BR>\n");
    }
printf("<B>3D Structure of Close Homologs:</B> ");
homologCount = 0;
strcpy(goodSCOPdomain, "dummy");

conn2= hAllocConn(database);
sqlSafef(query2, sizeof(query2),
	"select homologID,eValue,SCOPdomain,chain from sc1.protHomolog where proteinID='%s' and evalue <= 0.01;",
	itemName);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
if (row2 != NULL)
    {
    while (row2 != NULL)
	{
	homologID = row2[0];
	sscanf(row2[1], "%e", &eValue);
	SCOPdomain = row2[2];
	chp = SCOPdomain+strlen(SCOPdomain)-1;
	while (*chp != '.') chp--;
	*chp = '\0';
	chain = row2[3];
        if (eValue <= 1.0e-10)
	    strcpy(goodSCOPdomain, SCOPdomain);
	else
	    {
	    if (strcmp(goodSCOPdomain,SCOPdomain) != 0)
		goto skip;
	    else
		if (eValue > 0.1) goto skip;
	    }
	if (first)
            first = 0;
        else
            printf(", ");

        printf("<A HREF=\"http://www.rcsb.org/pdb/cgi/explore.cgi?job=graphics&pdbId=%s",
               homologID);
        if (strlen(chain) >= 1)
	    printf("\"TARGET=_blank>%s(chain %s)</A>", homologID, chain);
	else
	    printf("\"TARGET=_blank>%s</A>", homologID);
	homologCount++;

	skip:
	row2 = sqlNextRow(sr2);
	}
    }
hFreeConn(&conn2);
sqlFreeResult(&sr2);
if (homologCount == 0)
    printf("None<BR>\n");

printf("<BR><B>Details:</B> ");
printf("<A HREF=\"../SARS/%s/summary.html", itemName);
printf("\" TARGET=_blank>%s</A><BR>\n", itemName);

htmlHorizontalLine();
}

void showHomologies(char *geneName, char *table)
/* Show homology info. */
{
struct sqlConnection *conn = hAllocConn(database);
char query[256];
struct sqlResult *sr;
char **row;
boolean isFirst = TRUE, gotAny = FALSE;
char *gi;
struct softberryHom hom;

if (sqlTableExists(conn, table))
    {
    sqlSafef(query, sizeof(query), "select * from %s where name = '%s'", table, geneName);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	softberryHomStaticLoad(row, &hom);
	if ((gi = getGi(hom.giString)) == NULL)
	    continue;
	if (isFirst)
	    {
	    htmlHorizontalLine();
	    printf("<H3>Protein Homologies:</H3>\n");
	    isFirst = FALSE;
	    gotAny = TRUE;
	    }
	printf("<A HREF=\"");
	char temp[256];
	safef(temp, sizeof(temp), "%s", gi);
	printEntrezProteinUrl(stdout, temp);
	printf("\" TARGET=_blank>%s</A> %s<BR>", hom.giString, hom.description);
	}
    }
if (gotAny)
    htmlHorizontalLine();
hFreeConn(&conn);
}

void showPseudoHomologies(char *geneName, char *table)
/* Show homology info. */
{
struct sqlConnection *conn = hAllocConn(database);
char query[256];
struct sqlResult *sr;
char **row;
boolean isFirst = TRUE, gotAny = FALSE;
struct borkPseudoHom hom;
char *parts[10];
int partCount;
char *clone;

if (sqlTableExists(conn, table))
    {
    sqlSafef(query, sizeof(query), "select * from %s where name = '%s'", table, geneName);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	borkPseudoHomStaticLoad(row, &hom);
/*	if ((gi = getGi(hom.giString)) == NULL)
 *	    continue; */
	if (isFirst)
	    {
	    htmlHorizontalLine();
	    printf("<H3>Aligning Protein :</H3>\n");
	    isFirst = FALSE;
	    gotAny = TRUE;
	    }
	clone = cloneStringZ(hom.protRef,80);
	partCount = chopString(hom.protRef, "_", parts, ArraySize(parts));
	if (partCount > 1)
	    {
	    printf("<A HREF=");
	    char temp[256];
	    safef(temp, sizeof(temp), "%s", parts[1]);
	    printSwissProtProteinUrl(stdout, temp);
	    printf(" TARGET=_blank>Jump to SwissProt %s </A> " ,geneName);
	    }
	printf(" %s <BR><BR>Alignment Information:<BR><BR>%s<BR>", clone, hom.description);
	}
    }
if (gotAny)
    htmlHorizontalLine();
hFreeConn(&conn);
}

void pseudoPrintPosHeader(struct bed *bed)
/*    print header of pseudogene record */
{
printf("<p>");
printf("<B>%s PseudoGene:</B> %s:%d-%d   %d bp<BR>\n", hOrganism(database),  bed->chrom, bed->chromStart, bed->chromEnd, bed->chromEnd-bed->chromStart);
printf("Strand: %c",bed->strand[0]);
printf("<p>");
}

void pseudoPrintPos(struct psl *pseudoList, struct pseudoGeneLink *pg, char *alignTable, int start, char *acc)
/*    print details of pseudogene record */
{
char query[256];
struct dyString *dy = dyStringNew(1024);
char pfamDesc[128], *pdb;
char chainTable[64];
char chainTable_chrom[64];
struct sqlResult *sr;
char **row;
struct sqlConnection *conn = hAllocConn(database);
int first = 0;

safef(chainTable,sizeof(chainTable), "selfChain");
if (!hTableExists(database, chainTable) )
    safef(chainTable,sizeof(chainTable), "chainSelf");
printf("<B>Description:</B> Retrogenes are processed mRNAs that are inserted back into the genome. Most are pseudogenes, and some are functional genes or anti-sense transcripts that may impede mRNA translation.<p>\n");
printf("<B>Percent of retro that breaks net relative to Mouse : </B>%d&nbsp;%%<br>\n",pg->overlapMouse);
printf("<B>Percent of retro that breaks net relative to Dog   : </B>%d&nbsp;%%<br>\n",pg->overlapDog);
printf("<B>Percent of retro that breaks net relative to Macaque : </B>%d&nbsp;%%<br>\n",pg->overlapRhesus);
printf("<B>Exons&nbsp;Inserted:</B>&nbsp;%d&nbsp;out&nbsp;of&nbsp;%d&nbsp;%d Parent Splice Sites <br>\n",pg->exonCover - pg->conservedSpliceSites, pg->exonCover,pg->exonCount);
printf("<B>Conserved&nbsp;Splice&nbsp;Sites:</B>&nbsp;%d&nbsp;<br>\n",pg->conservedSpliceSites);
printf("<B>PolyA&nbsp;tail:</B>&nbsp;%d As&nbsp;out&nbsp;of&nbsp;%d&nbsp;bp <B>PolyA Percent&nbsp;Id:&nbsp;</B>%5.1f&nbsp;%%\n",pg->polyA,pg->polyAlen, (float)pg->polyA*100/(float)pg->polyAlen);
printf("&nbsp;(%d&nbsp;bp&nbsp;from&nbsp;end&nbsp;of&nbsp;retrogene)<br>\n",pg->polyAstart);
printf("<B>Bases&nbsp;matching:</B>&nbsp;%d&nbsp;\n", pg->matches);
printf("(%d&nbsp;%% of gene)<br>\n",pg->coverage);
if (!sameString(pg->overName, "none"))
    printf("<B>Bases&nbsp;overlapping mRNA:</B>&nbsp;%s&nbsp;(%d&nbsp;bp)<br>\n", pg->overName, pg->maxOverlap);
else
    printf("<B>No&nbsp;overlapping mRNA</B><br>");
if (sameString(pg->type, "singleExon"))
    printf("<b>Overlap with Parent:&nbsp;</b>%s<p>\n",pg->type);
else
    printf("<b>Type of RetroGene:&nbsp;</b>%s<p>\n",pg->type);
if (pseudoList != NULL)
    {
    printf("<H4>RetroGene/Gene Alignment</H4>");

    printAlignments(pseudoList, start, "htcCdnaAli", alignTable, acc);
    }
printf("<H4>Annotation for Gene locus that spawned RetroGene</H4>");

printf("<ul>");
if (!sameString(pg->refSeq,"noRefSeq"))
    {
    printf("<LI><B>RefSeq:</B> %s \n", pg->refSeq);
    linkToOtherBrowserExtra(database, pg->gChrom, pg->rStart, pg->rEnd, "refGene=pack");
    printf("%s:%d-%d \n", pg->gChrom, pg->rStart, pg->rEnd);
    printf("</A></LI>");
    }
if (!sameString(pg->kgName,"noKg"))
    {
    printf("<LI><B>KnownGene:</B> " );
    printf("<A TARGET=\"_blank\" ");
    printf("HREF=\"../cgi-bin/hgGene?%s&%s=%s&%s=%s&%s=%s&%s=%d&%s=%d\" ",
                cartSidUrlString(cart),
                "db", database,
                "hgg_gene", pg->kgName,
                "hgg_chrom", pg->gChrom,
                "hgg_start", pg->kStart,
                "hgg_end", pg->kEnd);
    printf(">%s</A>  ",pg->kgName);
    linkToOtherBrowserExtra(database, pg->gChrom, pg->kStart, pg->kEnd, "knownGene=pack");
    printf("%s:%d-%d \n", pg->gChrom, pg->kStart, pg->kEnd);
    printf("</A></LI>");
    if (hTableExists(database, "knownGene"))
        {
        char *description;
        sqlSafef(query, sizeof(query),
                "select proteinId from knownGene where name = '%s'", pg->kgName);
        description = sqlQuickString(conn, query);
        if (description != NULL)
            {
            printf("<LI><B>SwissProt ID: </B> " );
            printf("<A TARGET=\"_blank\" HREF=");
            printSwissProtProteinUrl(stdout, description);
            printf(">%s</A>",description);
            freez(&description);
            printf("</LI>" );
            }
        }
    }
else
    {
    /* display mrna */
    printf("<LI><B>mRna:</B> %s \n", pg->name);
    linkToOtherBrowserExtra(database, pg->gChrom, pg->gStart, pg->gEnd, "mrna=pack");
    printf("%s:%d-%d \n", pg->gChrom, pg->gStart, pg->gEnd);
    printf("</A></LI>");
    }
if (!sameString(pg->mgc,"noMgc"))
    {
    printf("<LI><B>%s Gene:</B> %s \n", mgcDbName(), pg->mgc);
    linkToOtherBrowserExtra(database, pg->gChrom, pg->mStart, pg->mEnd, "mgcGenes=pack");
    printf("%s:%d-%d \n", pg->gChrom, pg->mStart, pg->mEnd);
    printf("</A></LI>");
    }

printf("</ul>");
/* display pfam domains */

printf("<p>");
pdb = hPdbFromGdb(database);
safef(pfamDesc, 128, "%s.pfamDesc", pdb);
if (hTableExists(database, "knownToPfam") && hTableExists(database, pfamDesc))
    {
    sqlSafef(query, sizeof(query),
          "select description from knownToPfam kp, %s p where pfamAC = value and kp.name = '%s'",
            pfamDesc, pg->kgName);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        char *description = row[0];
        if (description == NULL)
            description = cloneString("n/a");
        printf("<B>Pfam Domain:</B> %s <p>", description);
        }
    sqlFreeResult(&sr);
    }

if (hTableExists(database, "all_mrna"))
    {
    char parent[255];
    struct psl *pslList = NULL;
    char *dotPtr ;
    safef(parent, sizeof(parent), "%s",pg->name);
    /* strip off version and unique suffix when looking for parent gene*/
    dotPtr = rStringIn(".",parent) ;
    if (dotPtr != NULL)
        *dotPtr = '\0';
    pslList = loadPslRangeT("all_mrna", parent, pg->gChrom, pg->gStart, pg->gEnd);
#ifdef NOT_USED
    char callClustal[512] = "YES";
    char inPath[512];
    safef(inPath, sizeof(inPath),
            "../pseudogene/pseudoHumanExp.%s.%s.%d.aln",
            pg->name, pg->chrom, pg->chromStart);
    /* either display a link to jalview or call it */
    if (callClustal != NULL && sameString(callClustal, "YES"))
        {
        displayJalView(inPath, "CLUSTAL", NULL);
        printf(" Display alignment of retrogene and parent mRNAs. ");
        }
//    else
//        {
//        hgcAnchorJalview(pg->name,  faTn.forCgi);
//        printf("JalView alignment of parent gene to retroGene</a>\n");
//        }
#endif /* NOT_USED */

    if (pslList != NULL)
        {
        printAlignments(pslList, pslList->tStart, "htcCdnaAli", "all_mrna", \
                pg->name);
        htmlHorizontalLine();
        safef(chainTable_chrom,sizeof(chainTable_chrom), "%s_chainSelf",\
                pg->chrom);
        if (hTableExists(database, chainTable_chrom) )
            {
                /* lookup chain */
            sqlDyStringPrintf(dy,
                           "select id, score, qStart, qEnd, qStrand, qSize from %s_%s where ",
                pg->chrom, chainTable);
            hAddBinToQuery(pg->chromStart, pg->chromEnd, dy);
            if (sameString(pg->gStrand,pg->strand))
                sqlDyStringPrintf(dy,
                    "tEnd > %d and tStart < %d and qName = '%s' and qEnd > %d and qStart < %d and qStrand = '+' ",
                    pg->chromStart, pg->chromEnd, pg->gChrom, pg->gStart, pg->gEnd);
            else
                {
                sqlDyStringPrintf(dy,"tEnd > %d and tStart < %d and qName = '%s' and qEnd > %d "
                                  "and qStart < %d and qStrand = '-'",
                               pg->chromStart, pg->chromEnd, pg->gChrom,
                               hChromSize(database, pg->gChrom)-(pg->gEnd),
                               hChromSize(database, pg->gChrom)-(pg->gStart));
                }
            sqlDyStringPrintf(dy, " order by qStart");
            sr = sqlGetResult(conn, dy->string);
            while ((row = sqlNextRow(sr)) != NULL)
                {
                int chainId, score;
                unsigned int qStart, qEnd, qSize;
                char qStrand;
                if (first == 0)
                    {
                    printf("<H4>Gene/PseudoGene Alignment (multiple records are a result of breaks in the human Self Chaining)</H4>\n");
                    printf("Shows removed introns, frameshifts and in frame stops.\n");
                    first = 1;
                    }
                chainId = sqlUnsigned(row[0]);
                score = sqlUnsigned(row[1]);
                qStart = sqlUnsigned(row[2]);
                qEnd = sqlUnsigned(row[3]);
                qStrand =row[4][0];
                qSize = sqlUnsigned(row[5]);
                if (qStrand == '-')
                    {
                    unsigned int tmp = qSize - qEnd;
                    qEnd = qSize - qStart;
                    qStart = tmp;
                    }
                /* if (pg->chainId == 0) pg->chainId = chainId; */
                puts("<ul><LI>\n");
                hgcAnchorPseudoGene(pg->kgName, "knownGene", pg->chrom, "startcodon",
                                    pg->chromStart, pg->chromEnd, pg->gChrom, pg->kStart,
                                    pg->kEnd, chainId, database);
                printf("Annotated alignment</a> using self chain.\n");
                printf("Score: %d \n", score);
                puts("</LI>\n");
                printf("<ul>Raw alignment: ");
                hgcAnchorTranslatedChain(chainId, chainTable, pg->chrom, pg->gStart, pg->gEnd);
                printf("%s:%d-%d </A></ul> </ul>\n", pg->gChrom,qStart,qEnd);
                }
            sqlFreeResult(&sr);
            }
        }
    }
printf("<p>RetroGene&nbsp;Score:&nbsp;%d \n",pg->score);
printf("Alignment&nbsp;Score:&nbsp;%d&nbsp;<br>\n",pg->axtScore);
if (pg->posConf != 0)
    printf("AdaBoost&nbsp;Confidence:</B>&nbsp;%4.3f&nbsp;\n",pg->posConf);
}

void doPseudoPsl(struct trackDb *tdb, char *acc)
/* Click on an pseudogene based on mrna alignment. */
{
char *tableName = tdb->table;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
char where[256];
struct pseudoGeneLink *pg;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
int winStart = cartInt(cart, "l");
int winEnd = cartInt(cart, "r");
struct psl *pslList = NULL;
char *chrom = cartString(cart, "c");
char *alignTable = cgiUsualString("table", cgiString("g"));
int rowOffset = 0;
/* Get alignment info. */
if (sameString(alignTable,"pseudoGeneLink2"))
    alignTable = cloneString("pseudoMrna2");
else if (sameString(alignTable,"pseudoGeneLink3"))
    alignTable = cloneString("pseudoMrna3");
else if (startsWith("pseudoGeneLink",alignTable))
    alignTable = cloneString("pseudoMrna");
if (startsWith("pseudoUcsc",alignTable))
    {
    alignTable = cloneString("pseudoMrna");
    tableName = cloneString("pseudoGeneLink3");
    }
if (hTableExists(database, alignTable) )
    {
    pslList = loadPslRangeT(alignTable, acc, chrom, winStart, winEnd);
    }
else
    errAbort("Table %s not found.\n",alignTable);
slSort(&pslList, pslCmpScoreDesc);

/* print header */
genericHeader(tdb, acc);
/* Print non-sequence info. */
cartWebStart(cart, database, "%s", acc);


sqlSafef(where, sizeof(where), "name = '%s'", acc);
sr = hRangeQuery(conn, tableName, chrom, start, end, where, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    pg = pseudoGeneLinkLoad(row+rowOffset);
    if (pg != NULL)
        {
        pseudoPrintPos(pslList, pg, alignTable, start, acc);
        }
    }
printTrackHtml(tdb);

sqlFreeResult(&sr);
hFreeConn(&conn);
}



void doSoftberryPred(struct trackDb *tdb, char *geneName)
/* Handle click on Softberry gene track. */
{
genericHeader(tdb, geneName);
showHomologies(geneName, "softberryHom");
if (sameWord(database, "sc1"))showSAM_T02(geneName);
geneShowCommon(geneName, tdb, "softberryPep");
printTrackHtml(tdb);
}

void doPseudoPred(struct trackDb *tdb, char *geneName)
/* Handle click on Softberry gene track. */
{
genericHeader(tdb, geneName);
showPseudoHomologies(geneName, "borkPseudoHom");
geneShowCommon(geneName, tdb, "borkPseudoPep");
printTrackHtml(tdb);
}

void doEncodePseudoPred(struct trackDb *tdb, char *geneName)
{
char query[256], *headerItem, *name2 = NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int start = cartInt(cart, "o");

headerItem = cloneString(geneName);
genericHeader(tdb, headerItem);
printCustomUrl(tdb, geneName, FALSE);
if ((sameString(tdb->table, "encodePseudogeneConsensus")) ||
         (sameString(tdb->table, "encodePseudogeneYale")))
    {
    sqlSafef(query, sizeof(query), "select name2 from %s where name = '%s'", tdb->table, geneName);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        name2 = cloneString(row[0]);
        }
    printOtherCustomUrl(tdb, name2, "url2", TRUE);
    }
genericGenePredClick(conn, tdb, geneName, start, NULL, NULL);
printTrackHtml(tdb);
hFreeConn(&conn);
}

void showOrthology(char *geneName, char *table, struct sqlConnection *connMm)
/* Show mouse Orthlogous info. */
{
char query[256];
struct sqlResult *sr;
char **row;
boolean isFirst = TRUE, gotAny = FALSE;
char *gi;
struct softberryHom hom;


if (sqlTableExists(connMm, table))
    {
    sqlSafef(query, sizeof(query), "select * from %s where name = '%s'", table, geneName);
    sr = sqlGetResult(connMm, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	softberryHomStaticLoad(row, &hom);
	if ((gi = getGi(hom.giString)) == NULL)
	    continue;
	if (isFirst)
	    {
	    htmlHorizontalLine();
	    printf("<H3>Protein Homologies:</H3>\n");
	    isFirst = FALSE;
	    gotAny = TRUE;
	    }
	printf("<A HREF=\"");
	char temp[256];
	safef(temp, sizeof(temp), "%s[gi]", gi);
	printEntrezProteinUrl(stdout, query);
	printf("\" TARGET=_blank>%s</A> %s<BR>", hom.giString, hom.description);
	}
    }
if (gotAny)
    htmlHorizontalLine();
sqlFreeResult(&sr);
}

void doMouseOrtho(struct trackDb *tdb, char *geneName)
/* Handle click on MouseOrtho gene track. */
{
struct sqlConnection *connMm = sqlConnect(mousedb);
genericHeader(tdb, geneName);
showOrthology(geneName, "softberryHom",connMm);
tdb = hashFindVal(trackHash, "softberryGene");
geneShowMouse(geneName, tdb, "softberryPep", connMm);
printTrackHtml(tdb);
sqlDisconnect(&connMm);
}

void showSangerExtra(char *geneName, char *extraTable)
/* Show info from sanger22extra table if it exists. */
{
if (hTableExists(database, extraTable))
    {
    struct sanger22extra se;
    char query[256];
    struct sqlConnection *conn = hAllocConn(database);
    struct sqlResult *sr;
    char **row;

    sqlSafef(query, sizeof(query), "select * from %s where name = '%s'", extraTable, geneName);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	sanger22extraStaticLoad(row, &se);
	printf("<B>Name:</B>  %s<BR>\n", se.name);
	if (!sameString(se.name, se.locus))
	    printf("<B>Locus:</B> %s<BR>\n", se.locus);
	printf("<B>Description:</B> %s<BR>\n", se.description);
	printf("<B>Gene type:</B> %s<BR>\n", se.geneType);
	if (se.cdsType[0] != 0 && !sameString(se.geneType, se.cdsType))
	    printf("<B>CDS type:</B> %s<BR>\n", se.cdsType);
	}
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }
}

void doSangerGene(struct trackDb *tdb, char *geneName, char *pepTable, char *mrnaTable, char *extraTable)
/* Handle click on Sanger gene track. */
{
genericHeader(tdb, geneName);
showSangerExtra(geneName, extraTable);
geneShowCommon(geneName, tdb, pepTable);
printTrackHtml(tdb);
}

void doTrnaGenesGb(struct trackDb *tdb, char *trnaName)
{
struct tRNAs *trna;
char query[512];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;

int start   = cartInt(cart, "o");
int end     = cartInt(cart, "t");

genericHeader(tdb,trnaName);

rowOffset = hOffsetPastBin(database, seqName, tdb->table);
sqlSafef(query, ArraySize(query),
      "select * from %s where name = '%s' and chromStart=%d and chromEnd=%d",
tdb->table, trnaName, start, end);

sr = sqlGetResult(conn, query);

/* use TABLE to align image with other info side by side */
printf("<TABLE>");
while ((row = sqlNextRow(sr)) != NULL)
    {
    char imgFileName[512];
    char encodedName[255];
    char *chp1, *chp2;
    int i, len;
    printf("<TR>");
    printf("<TD valign=top>");

    trna = tRNAsLoad(row+rowOffset);

    printf("<B>tRNA name: </B>%s<BR>\n",trna->name);
    printf("<B>tRNA isotype: </B> %s<BR>\n",trna->aa);
    printf("<B>tRNA anticodon: </B> %s<BR>\n",trna->ac);
    printf("<B>tRNAscan-SE score: </B> %.2f bits<BR>\n",trna->trnaScore);
    printf("<B>Intron(s): </B> %s<BR>\n",trna->intron);
    printf("<B>Genomic size: </B> %d nt<BR>\n",trna->chromEnd-trna->chromStart);
    printf("<B>Position:</B> "
       "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
       hgTracksPathAndSettings(), database, trna->chrom, trna->chromStart+1, trna->chromEnd);
    printf("%s:%d-%d</A><BR>\n", trna->chrom, trna->chromStart+1, trna->chromEnd);
    printf("<B>Strand:</B> %s<BR>\n", trna->strand);
    if (!sameString(trna->genomeUrl, ""))
        {
        printf("<BR><A HREF=\"%s\" TARGET=_blank>View summary of all genomic tRNA predictions</A><BR>\n"
	       , trna->genomeUrl);
        printf("<BR><A HREF=\"%s\" TARGET=_blank>View complete details for this tRNA</A><BR>\n", trna->trnaUrl);
	}

    if (trna->next != NULL)
      printf("<hr>\n");

    printf("</TD>");

    printf("<TD>");

    /* encode '?' in tRNA name into "%3F" */
    len = strlen(trna->name);
    chp1 = trna->name;
    chp2 = encodedName;
    for (i=0; i<len; i++)
        {
	if (*chp1 == '?')
	    {
	    *chp2 = '%';
	    chp2++; *chp2 = '3';
	    chp2++; *chp2 = 'F';
	    }
	else
	   {
	   *chp2 = *chp1;
	   }
	chp1++;
	chp2++;
	}
    *chp2 = '\0';

    safef(imgFileName, sizeof imgFileName, "../htdocs/RNA-img/%s/%s-%s.gif", database,database,trna->name);
    if (fileExists(imgFileName))
        {
        printf(
	"<img align=right src=\"../RNA-img/%s/%s-%s.gif\" alt='tRNA secondary structure for %s'>\n",
        database,database,encodedName,trna->name);
        }
    else
        {
        printf(
	"<img align=right src=\"../RNA-img/%s/%s-%s.gif\" alt='tRNA secondary structure is not available for %s'>\n",
        database,database,trna->name,trna->name);
	}
    printf("</TD>");

    printf("</TR>");
    }

printf("</TABLE>");
sqlFreeResult(&sr);
hFreeConn(&conn);
printTrackHtml(tdb);
tRNAsFree(&trna);
}

void doVegaGeneZfish(struct trackDb *tdb, char *name)
/* Handle click on Vega gene track for zebrafish. */
{
struct vegaInfoZfish *vif = NULL;
char query[256];
struct sqlResult *sr;
char **row;

genericHeader(tdb, name);
if (hTableExists(database, "vegaInfoZfish"))
    {
    struct sqlConnection *conn = hAllocConn(database);

    sqlSafef(query, sizeof(query),
	  "select * from vegaInfoZfish where transcriptId = '%s'", name);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
	AllocVar(vif);
	vegaInfoZfishStaticLoad(row, vif);
	}
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }

printCustomUrl(tdb, name, TRUE);
if (vif != NULL)
    {
    /* change confidence to lower case and display with method for gene type */
    tolowers(vif->confidence);
    printf("<B>VEGA Gene Type:</B> %s %s<BR>\n", vif->confidence, vif->method);
    printf("<B>VEGA Gene Name:</B> %s<BR>\n", vif->sangerName);
    if (differentString(vif->geneDesc, "NULL"))
        printf("<B>VEGA Gene Description:</B> %s<BR>\n", vif->geneDesc);
    printf("<B>VEGA Gene Id:</B> %s<BR>\n", vif->geneId);
    printf("<B>VEGA Transcript Id:</B> %s<BR>\n", name);
    printf("<B>ZFIN Id:</B> ");
    printf("<A HREF=\"http://zfin.org/cgi-bin/webdriver?MIval=aa-markerview.apg&OID=%s\" TARGET=_blank>%s</A><BR>\n", vif->zfinId, vif->zfinId);
    printf("<B>Official ZFIN Gene Symbol:</B> %s<BR>\n", vif->zfinSymbol);
    /* get information for the cloneId from */

    printf("<B>Clone Id:</B> \n");
    struct sqlConnection *conn2 = hAllocConn(database);
    sqlSafef(query, sizeof(query),
	 "select cloneId from vegaToCloneId where transcriptId = '%s'", name);
    sr = sqlGetResult(conn2, query);
    if ((row = sqlNextRow(sr)) != NULL)
        printf("%s", row[0]);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        printf(" ,%s ", row[0]);
        }
    printf("<BR>\n");
    sqlFreeResult(&sr);
    hFreeConn(&conn2);
    }
geneShowCommon(name, tdb, "vegaPep");
printTrackHtml(tdb);
}

void doVegaGene(struct trackDb *tdb, char *item, char *itemForUrl)
/* Handle click on Vega gene track. */
{
struct vegaInfo *vi = NULL;
char versionString[256];
char dateReference[256];
char headerTitle[512];

/* see if hgFixed.trackVersion exists */
boolean trackVersionExists = hTableExists("hgFixed", "trackVersion");
/* assume nothing found */
versionString[0] = 0;
dateReference[0] = 0;

if (trackVersionExists)
    {
    char query[256];
    struct sqlConnection *conn = hAllocConn(database);
    sqlSafef(query, sizeof(query), "select version,dateReference from hgFixed.trackVersion where db = '%s' AND name = 'vegaGene' order by updateTime DESC limit 1", database);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row;

    /* in case of NULL result from the table */
    versionString[0] = 0;
    while ((row = sqlNextRow(sr)) != NULL)
	{
	safef(versionString, sizeof(versionString), "Vega %s",
		row[0]);
	safef(dateReference, sizeof(dateReference), "%s",
		row[1]);
	}
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }

if (itemForUrl == NULL)
    itemForUrl = item;

if (versionString[0])
    safef(headerTitle, sizeof(headerTitle), "%s - %s", item, versionString);
else
    safef(headerTitle, sizeof(headerTitle), "%s", item);

genericHeader(tdb, headerTitle);

if (hTableExists(database, "vegaInfo"))
    {
    char query[256];
    struct sqlConnection *conn = hAllocConn(database);
    struct sqlResult *sr;
    char **row;

    sqlSafef(query, sizeof(query),
	  "select * from vegaInfo where transcriptId = '%s'", item);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
	AllocVar(vi);
	vegaInfoStaticLoad(row, vi);
	}
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }
/* No archive for Vega */
char *archive = NULL;
printEnsemblOrVegaCustomUrl(tdb, itemForUrl, item == itemForUrl, archive);

if (vi != NULL)
    {
    printf("<B>VEGA Gene Type:</B> %s<BR>\n", vi->method);
    printf("<B>VEGA Gene Name:</B> %s<BR>\n", vi->otterId);
    if (differentString(vi->geneDesc, "NULL"))
        printf("<B>VEGA Gene Description:</B> %s<BR>\n", vi->geneDesc);
    printf("<B>VEGA Gene Id:</B> %s<BR>\n", vi->geneId);
    printf("<B>VEGA Transcript Id:</B> %s<BR>\n", item);
    }
geneShowCommon(item, tdb, "vegaPep");
printTrackHtml(tdb);
}

void doBDGPGene(struct trackDb *tdb, char *geneName)
/* Show Berkeley Drosophila Genome Project gene info. */
{
struct bdgpGeneInfo *bgi = NULL;
struct flyBaseSwissProt *fbsp = NULL;
char *geneTable = tdb->table;
char *truncName = cloneString(geneName);
char *ptr = strchr(truncName, '-');
char infoTable[128];
char pepTable[128];
char query[512];

if (ptr != NULL)
    *ptr = 0;
safef(infoTable, sizeof(infoTable), "%sInfo", geneTable);

genericHeader(tdb, geneName);

if (hTableExists(database, infoTable))
    {
    struct sqlConnection *conn = hAllocConn(database);
    struct sqlResult *sr;
    char **row;
    sqlSafef(query, sizeof(query),
	  "select * from %s where bdgpName = \"%s\";",
	  infoTable, truncName);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	bgi = bdgpGeneInfoLoad(row);
	if (hTableExists(database, "flyBaseSwissProt"))
	    {
	    sqlSafef(query, sizeof(query),
		  "select * from flyBaseSwissProt where flyBaseId = \"%s\"",
		  bgi->flyBaseId);
	    sqlFreeResult(&sr);
	    sr = sqlGetResult(conn, query);
	    if ((row = sqlNextRow(sr)) != NULL)
		fbsp = flyBaseSwissProtLoad(row);
	    }
	}
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }
if (bgi != NULL)
    {
    if (!sameString(bgi->symbol, geneName))
	{
	printf("<B>Gene symbol:</B> %s<BR>\n", bgi->symbol);
	}
    if (fbsp != NULL)
	{
	printf("<B>SwissProt:</B> <A HREF=");
	printSwissProtProteinUrl(stdout, fbsp->swissProtId);
	printf(" TARGET=_BLANK>%s</A> (%s) %s<BR>\n",
	       fbsp->swissProtId, fbsp->spSymbol, fbsp->spGeneName);
	}
    printf("<B>FlyBase:</B> <A HREF=");
    printFlyBaseUrl(stdout, bgi->flyBaseId);
    printf(" TARGET=_BLANK>%s</A><BR>\n", bgi->flyBaseId);
    printf("<B>BDGP:</B> <A HREF=");
    printBDGPUrl(stdout, truncName);
    printf(" TARGET=_BLANK>%s</A><BR>\n", truncName);
    }
printCustomUrl(tdb, geneName, FALSE);
showGenePos(geneName, tdb);
if (bgi != NULL)
    {
    if (bgi->go != NULL && bgi->go[0] != 0)
	{
	struct sqlConnection *goConn = sqlMayConnect("go");
	char *goTerm = NULL;
	char *words[10];
	char buf[512];
	int wordCount = chopCommas(bgi->go, words);
	int i;
	puts("<B>Gene Ontology terms from BDGP:</B> <BR>");
	for (i=0;  i < wordCount && words[i][0] != 0;  i++)
	    {
	    if (i > 0 && sameWord(words[i], words[i-1]))
		continue;
	    goTerm = "";
	    if (goConn != NULL)
		{
		sqlSafef(query, sizeof(query),
		      "select name from term where acc = 'GO:%s';",
		      words[i]);
		goTerm = sqlQuickQuery(goConn, query, buf, sizeof(buf));
		if (goTerm == NULL)
		    goTerm = "";
		}
	    printf("&nbsp;&nbsp;&nbsp;GO:%s: %s<BR>\n",
		   words[i], goTerm);
	    }
	sqlDisconnect(&goConn);
	}
    if (bgi->cytorange != NULL && bgi->cytorange[0] != 0)
	{
	printf("<B>Cytorange:</B> %s<BR>", bgi->cytorange);
	}
    }
printf("<H3>Links to sequence:</H3>\n");
printf("<UL>\n");

safef(pepTable, sizeof(pepTable), "%sPep", geneTable);
if (hGenBankHaveSeq(database, geneName, pepTable))
    {
    puts("<LI>\n");
    hgcAnchorSomewhere("htcTranslatedProtein", geneName, pepTable,
		       seqName);
    printf("Predicted Protein</A> \n");
    puts("</LI>\n");
    }

puts("<LI>\n");
hgcAnchorSomewhere("htcGeneMrna", geneName, tdb->track, seqName);
printf("%s</A> may be different from the genomic sequence.\n",
       "Predicted mRNA");
puts("</LI>\n");

puts("<LI>\n");
hgcAnchorSomewhere("htcGeneInGenome", geneName, tdb->track, seqName);
printf("Genomic Sequence</A> from assembly\n");
puts("</LI>\n");
printf("</UL>\n");
printTrackHtml(tdb);
}

char *pepTableFromType(char *type)
/* If type (should be from tdb->type) starts with "genePred xxxx",
 * return "xxxx" as the pepTable for this track. */
{
char *dupe, *words[16];
int wordCount;
char *pepTable = NULL;
dupe = cloneString(type);
wordCount = chopLine(dupe, words);

if (wordCount > 1 && sameWord(words[0], "genePred") && words[1] != NULL)
    pepTable = cloneString(words[1]);
freeMem(dupe);
return pepTable;
}

struct bed *getBedAndPrintPos(struct trackDb *tdb, char *name, int maxN)
/* Dig up the bed for this item just to print the position. */
{
struct bed *bed = NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row = NULL;
char query[256];
char table[HDB_MAX_TABLE_STRING];
boolean hasBin = FALSE;
int n = atoi(tdb->type + 4);
int start = cgiInt("o");
if (n < 3)
    n = 3;
if (n > maxN)
    n = maxN;
if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, &hasBin))
    errAbort("track %s not found", tdb->table);
sqlSafef(query, sizeof(query),
      "select * from %s where chrom = '%s' and chromStart = %d "
      "and name = '%s'",
      table, seqName, start, name);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row+hasBin, n);
    bedPrintPos(bed, n, tdb);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return bed;
}

void printFBLinkLine(char *label, char *id)
/* If id is not NULL/empty, print a label and link to FlyBase. */
{
if (isNotEmpty(id))
    {
    printf("<B>%s:</B> <A HREF=", label);
    printFlyBaseUrl(stdout, id);
    printf(" TARGET=_BLANK>%s</A><BR>\n", id);
    }
}

void showFlyBase2004Xref(char *xrefTable, char *geneName)
/* Show FlyBase gene info provided as of late 2004
 * (D. mel. v4.0 / D. pseud. 1.0).  Assumes xrefTable exists
 * and matches flyBase2004Xref.sql! */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
struct flyBase2004Xref *xref = NULL;
struct flyBaseSwissProt *fbsp = NULL;
char query[512];

sqlSafef(query, sizeof(query),
      "select * from %s where name = \"%s\";", xrefTable, geneName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    xref = flyBase2004XrefLoad(row);
    if (hTableExists(database, "flyBaseSwissProt") && isNotEmpty(xref->fbgn))
	{
	sqlSafef(query, sizeof(query),
	      "select * from flyBaseSwissProt where flyBaseId = \"%s\"",
	      xref->fbgn);
	sqlFreeResult(&sr);
	sr = sqlGetResult(conn, query);
	if ((row = sqlNextRow(sr)) != NULL)
	    fbsp = flyBaseSwissProtLoad(row);
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
if (xref != NULL)
    {
    if (isNotEmpty(xref->symbol) && !sameString(xref->symbol, geneName))
	{
	printf("<B>Gene symbol:</B> %s<BR>\n", xref->symbol);
	}
    if (isNotEmpty(xref->synonyms))
	{
	int last = strlen(xref->synonyms) - 1;
	if (xref->synonyms[last] == ',')
	    xref->synonyms[last] = 0;
	printf("<B>Synonyms:</B> ");
	htmlTextOut(xref->synonyms);
	printf("<BR>\n");
	}
    if (fbsp != NULL)
	{
	printf("<B>SwissProt:</B> <A HREF=");
	printSwissProtProteinUrl(stdout, fbsp->swissProtId);
	printf(" TARGET=_BLANK>%s</A> (%s) %s<BR>\n",
	       fbsp->swissProtId, fbsp->spSymbol, fbsp->spGeneName);
	}
    if (isNotEmpty(xref->type))
	printf("<B>Type:</B> %s<BR>\n", xref->type);
    printFBLinkLine("FlyBase Gene", xref->fbgn);
    printFBLinkLine("FlyBase Protein", xref->fbpp);
    printFBLinkLine("FlyBase Annotation", xref->fban);
    }
}

void doFlyBaseGene(struct trackDb *tdb, char *geneName)
/* Show FlyBase gene info. */
{
char *xrefTable = trackDbSettingOrDefault(tdb, "xrefTable", "flyBase2004Xref");
genericHeader(tdb, geneName);

/* Note: if we need to expand to a different xref table definition, do
 * some checking here.  For now, assume it's flyBase2004Xref-compatible: */
if (hTableExists(database, xrefTable))
    showFlyBase2004Xref(xrefTable, geneName);

printCustomUrl(tdb, geneName, FALSE);
if (startsWith("genePred", tdb->type))
    {
    char *pepTable = pepTableFromType(tdb->type);
    showGenePos(geneName, tdb);
    printf("<H3>Links to sequence:</H3>\n");
    printf("<UL>\n");

    if (pepTable != NULL && hGenBankHaveSeq(database, geneName, pepTable))
	{
	puts("<LI>\n");
	hgcAnchorSomewhere("htcTranslatedProtein", geneName, pepTable,
			   seqName);
        printf("Predicted Protein</A> \n");
	puts("</LI>\n");
	}
    else
        errAbort("Doh, no go for %s from %s<BR>\n", geneName, pepTable);

    puts("<LI>\n");
    hgcAnchorSomewhere("htcGeneMrna", geneName, tdb->track, seqName);
    printf("%s</A> may be different from the genomic sequence.\n",
	   "Predicted mRNA");
    puts("</LI>\n");

    puts("<LI>\n");
    hgcAnchorSomewhere("htcGeneInGenome", geneName, tdb->track, seqName);
    printf("Genomic Sequence</A> from assembly\n");
    puts("</LI>\n");
    printf("</UL>\n");
    }
else if (startsWith("bed", tdb->type))
    {
    struct bed *bed = getBedAndPrintPos(tdb, geneName, 4);
    if (bed != NULL && bed->strand[0] != 0)
	printf("<B>Strand:</B> %s<BR>\n", bed->strand);
    bedFree(&bed);
    }
printTrackHtml(tdb);
}

void doBGIGene(struct trackDb *tdb, char *geneName)
/* Show Beijing Genomics Institute gene annotation info. */
{
struct bgiGeneInfo *bgi = NULL;
char *geneTable = tdb->table;
char infoTable[128];
char pepTable[128];
char query[512];

safef(infoTable, sizeof(infoTable), "%sInfo", geneTable);

genericHeader(tdb, geneName);

if (hTableExists(database, infoTable))
    {
    struct sqlConnection *conn = hAllocConn(database);
    struct sqlResult *sr;
    char **row;
    sqlSafef(query, sizeof(query),
	  "select * from %s where name = \"%s\";", infoTable, geneName);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	bgi = bgiGeneInfoLoad(row);
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }
printCustomUrl(tdb, geneName, FALSE);
showGenePos(geneName, tdb);
if (bgi != NULL)
    {
    printf("<B>Annotation source:</B> %s<BR>\n", bgi->source);
    if (bgi->go != NULL && bgi->go[0] != 0 && !sameString(bgi->go, "None"))
	{
	struct sqlConnection *goConn = sqlMayConnect("go");
	char *goTerm = NULL;
	char *words[16];
	char buf[512];
	int wordCount = chopCommas(bgi->go, words);
	int i;
	puts("<B>Gene Ontology terms from BGI:</B> <BR>");
	for (i=0;  i < wordCount && words[i][0] != 0;  i++)
	    {
	    if (i > 0 && sameWord(words[i], words[i-1]))
		continue;
	    goTerm = "";
	    if (goConn != NULL)
		{
		sqlSafef(query, sizeof(query),
		      "select name from term where acc = 'GO:%s';",
		      words[i]);
		goTerm = sqlQuickQuery(goConn, query, buf, sizeof(buf));
		if (goTerm == NULL)
		    goTerm = "";
		}
	    printf("&nbsp;&nbsp;&nbsp;GO:%s: %s<BR>\n",
		   words[i], goTerm);
	    }
	sqlDisconnect(&goConn);
	}
    if (bgi->ipr != NULL && bgi->ipr[0] != 0 && !sameString(bgi->ipr, "None"))
	{
	char *words[16];
	int wordCount = chopByChar(bgi->ipr, ';', words, ArraySize(words));
	int i;
	printf("<B>Interpro terms from BGI:</B> <BR>\n");
	for (i=0;  i < wordCount && words[i][0] != 0;  i++)
	    {
	    printf("&nbsp;&nbsp;&nbsp;%s<BR>\n", words[i]);
	    }
	}
    if (hTableExists(database, "bgiGeneSnp") && hTableExists(database, "bgiSnp"))
	{
	struct sqlConnection *conn = hAllocConn(database);
	struct sqlConnection *conn2 = hAllocConn(database);
	struct sqlResult *sr;
	struct sqlResult *sr2;
	struct bgiSnp snp;
	struct bgiGeneSnp gs;
	char **row;
	int rowOffset = hOffsetPastBin(database, seqName, "bgiSnp");
	boolean init = FALSE;
	sqlSafef(query, sizeof(query),
	      "select * from bgiGeneSnp where geneName = '%s'", geneName);
	sr = sqlGetResult(conn, query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    if (! init)
		{
		printf("<B>BGI SNPs associated with gene %s:</B> <BR>\n",
		       geneName);
		init = TRUE;
		}
	    bgiGeneSnpStaticLoad(row, &gs);
	    sqlSafef(query, sizeof(query),
		  "select * from bgiSnp where name = '%s'", gs.snpName);
	    sr2 = sqlGetResult(conn2, query);
	    if ((row = sqlNextRow(sr2)) != NULL)
		{
		bgiSnpStaticLoad(row+rowOffset, &snp);
		printf("&nbsp;&nbsp;&nbsp;<A HREF=%s&g=bgiSnp&i=%s&db=%s&c=%s&o=%d&t=%d>%s</A>: %s",
		       hgcPathAndSettings(), gs.snpName, database,
		       seqName, snp.chromStart, snp.chromEnd, gs.snpName,
		       gs.geneAssoc);
		if (gs.effect[0] != 0)
		    printf(", %s", gs.effect);
		if (gs.phase[0] != 0)
		    printf(", phase %c", gs.phase[0]);
		if (gs.siftComment[0] != 0)
		    printf(", SIFT comment: %s", gs.siftComment);
		puts("<BR>");
		}
	    sqlFreeResult(&sr2);
	    }
	sqlFreeResult(&sr);
	hFreeConn(&conn);
	hFreeConn(&conn2);
	}
    }
printf("<H3>Links to sequence:</H3>\n");
printf("<UL>\n");

safef(pepTable, sizeof(pepTable), "%sPep", geneTable);
if (hGenBankHaveSeq(database, geneName, pepTable))
    {
    puts("<LI>\n");
    hgcAnchorSomewhere("htcTranslatedProtein", geneName, pepTable,
		       seqName);
    printf("Predicted Protein</A> \n");
    puts("</LI>\n");
    }

puts("<LI>\n");
hgcAnchorSomewhere("htcGeneMrna", geneName, tdb->track, seqName);
printf("%s</A> may be different from the genomic sequence.\n",
       "Predicted mRNA");
puts("</LI>\n");

puts("<LI>\n");
hgcAnchorSomewhere("htcGeneInGenome", geneName, tdb->track, seqName);
printf("Genomic Sequence</A> from assembly\n");
puts("</LI>\n");
printf("</UL>\n");
printTrackHtml(tdb);
}


void doBGISnp(struct trackDb *tdb, char *itemName)
/* Put up info on a Beijing Genomics Institute SNP. */
{
char *table = tdb->table;
struct bgiSnp snp;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset = hOffsetPastBin(database, seqName, table);

genericHeader(tdb, itemName);

sqlSafef(query, sizeof(query),
      "select * from %s where name = '%s'", table, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bgiSnpStaticLoad(row+rowOffset, &snp);
    bedPrintPos((struct bed *)&snp, 3, tdb);
    printf("<B>SNP Type:</B> %s<BR>\n",
           (snp.snpType[0] == 'S') ? "Substitution" :
	   (snp.snpType[0] == 'I') ? "Insertion" : "Deletion");
    printf("<B>SNP Sequence:</B> %s<BR>\n", snp.snpSeq);
    printf("<B>SNP in Broiler?:</B> %s<BR>\n", snp.inBroiler);
    printf("<B>SNP in Layer?:</B> %s<BR>\n", snp.inLayer);
    printf("<B>SNP in Silkie?:</B> %s<BR>\n", snp.inSilkie);
    if (hTableExists(database, "bgiGeneSnp") && hTableExists(database, "bgiGene"))
	{
	struct genePred *bg;
	struct sqlConnection *conn2 = hAllocConn(database);
	struct sqlConnection *conn3 = hAllocConn(database);
	struct sqlResult *sr2, *sr3;
	struct bgiGeneSnp gs;
	sqlSafef(query, sizeof(query),
	      "select * from bgiGeneSnp where snpName = '%s'", snp.name);
	sr2 = sqlGetResult(conn2, query);
	while ((row = sqlNextRow(sr2)) != NULL)
	    {
	    bgiGeneSnpStaticLoad(row, &gs);
	    sqlSafef(query, sizeof(query),
		  "select * from bgiGene where name = '%s'", gs.geneName);
	    sr3 = sqlGetResult(conn3, query);
	    while ((row = sqlNextRow(sr3)) != NULL)
		{
		bg = genePredLoad(row);
		printf("<B>Associated gene:</B> <A HREF=%s&g=bgiGene&i=%s&c=%s&db=%s&o=%d&t=%d&l=%d&r=%d>%s</A>: %s",
		       hgcPathAndSettings(), gs.geneName,
		       seqName, database, bg->txStart, bg->txEnd,
		       bg->txStart, bg->txEnd, gs.geneName, gs.geneAssoc);
		if (gs.effect[0] != 0)
		    printf(" %s", gs.effect);
		if (gs.phase[0] != 0)
		    printf(" phase %c", gs.phase[0]);
		if (gs.siftComment[0] != 0)
		    printf(", SIFT comment: %s", gs.siftComment);
		puts("<BR>");
		}
	    sqlFreeResult(&sr3);
	    }
	hFreeConn(&conn3);
	sqlFreeResult(&sr2);
	hFreeConn(&conn2);
	}
    printf("<B>Quality Scores:</B> %d in reference, %d in read<BR>\n",
	   snp.qualChr, snp.qualReads);
    printf("<B>Left Primer Sequence:</B> %s<BR>\n", snp.primerL);
    printf("<B>Right Primer Sequence:</B> %s<BR>\n", snp.primerR);
    if (snp.snpType[0] != 'S')
        {
        if (snp.questionM[0] == 'H')
	    printf("<B>Indel Confidence</B>: High\n");
        if (snp.questionM[0] == 'L')
	    printf("<B>Indel Confidence</B>: Low\n");
        }
    }
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}


void parseChromPointPos(char *pos, char *retChrom, int *retPos)
/* Parse out chrN:123 into chrN and 123. */
{
char *s, *e;
int len;
e = strchr(pos, ':');
if (e == NULL)
    errAbort("No : in chromosome point position %s", pos);
len = e - pos;
memcpy(retChrom, pos, len);
retChrom[len] = 0;
s = e+1;
*retPos = atoi(s);
}

void doGenomicDups(struct trackDb *tdb, char *dupName)
/* Handle click on genomic dup track. */
{
struct genomicDups dup;
char query[512];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char oChrom[64];
int oStart;

cartWebStart(cart, database, "Genomic Duplications");
printf("<H2>Genomic Duplication Region</H2>\n");
if (cgiVarExists("o"))
    {
    int start = cartInt(cart, "o");
    int rowOffset = hOffsetPastBin(database, seqName, tdb->table);
    parseChromPointPos(dupName, oChrom, &oStart);

    sqlSafef(query, sizeof query, "select * from %s where chrom = '%s' and chromStart = %d "
	    "and otherChrom = '%s' and otherStart = %d",
	    tdb->table, seqName, start, oChrom, oStart);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)))
	{
	genomicDupsStaticLoad(row+rowOffset, &dup);
	printf("<B>Region Position:</B> <A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
	       hgTracksPathAndSettings(),
	       database, dup.chrom, dup.chromStart, dup.chromEnd);
	printf("%s:%d-%d</A><BR>\n", dup.chrom, dup.chromStart, dup.chromEnd);
	printf("<B>Other Position:</B> <A HREF=\"%s&db=%s&position=%s%%3A%d-%d\" TARGET=_blank>",
	       hgTracksName(),
	       database, dup.otherChrom, dup.otherStart, dup.otherEnd);
	printf("%s:%d-%d</A><BR>\n", dup.otherChrom, dup.otherStart, dup.otherEnd);
	printf("<B>Relative orientation:</B> %s<BR>\n", dup.strand);
	printf("<B>Percent identity:</B> %3.1f%%<BR>\n", 0.1*dup.score);
	printf("<B>Size:</B> %d<BR>\n", dup.alignB);
	printf("<B>Bases matching:</B> %d<BR>\n", dup.matchB);
	printf("<B>Bases not matching:</B> %d<BR>\n", dup.mismatchB);
	htmlHorizontalLine();
	}
    }
else
    {
    puts("<P>Click directly on a repeat for specific information on that repeat</P>");
    }
printTrackHtml(tdb);
hFreeConn(&conn);
}

void htcExtSeq(char *item)
/* Print out DNA from some external but indexed .fa file. */
{
struct dnaSeq *seq;
cartHtmlStart(item);
seq = hExtSeq(database, item);
printf("<PRE><TT>");
faWriteNext(stdout, item, seq->dna, seq->size);
printf("</TT></PRE>");
freeDnaSeq(&seq);
}

void doBlatMouse(struct trackDb *tdb, char *itemName)
/* Handle click on blatMouse track. */
{
char query[256];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
int start = cartInt(cart, "o");
struct psl *pslList = NULL, *psl;
char *tiNum = strrchr(itemName, '|');
boolean hasBin;
char table[HDB_MAX_TABLE_STRING];

/* Print heading info including link to NCBI. */
if (tiNum != NULL)
    ++tiNum;
cartWebStart(cart, database, "%s", itemName);
printf("<H1>Information on Mouse %s %s</H1>",
       (tiNum == NULL ? "Contig" : "Read"), itemName);

/* Print links to NCBI and to sequence. */
if (tiNum != NULL)
    {
    printf("Link to ");
    printf("<A HREF=\"https://www.ncbi.nlm.nih.gov/Traces/trace.cgi?val=%s\" TARGET=_blank>", tiNum);
    printf("NCBI Trace Repository for %s\n</A><BR>\n", itemName);
    }
printf("Get ");
printf("<A HREF=\"%s&g=htcExtSeq&c=%s&l=%d&r=%d&i=%s\">",
       hgcPathAndSettings(), seqName, winStart, winEnd, itemName);
printf("Mouse DNA</A><BR>\n");

/* Print info about mate pair. */
if (tiNum != NULL && sqlTableExists(conn, "mouseTraceInfo"))
    {
    char buf[256];
    char *templateId;
    boolean gotMate = FALSE;
    sqlSafef(query, sizeof query, "select templateId from mouseTraceInfo where ti = '%s'", itemName);
    templateId = sqlQuickQuery(conn, query, buf, sizeof(buf));
    if (templateId != NULL)
        {
	sqlSafef(query, sizeof query, "select ti from mouseTraceInfo where templateId = '%s'", templateId);
	sr = sqlGetResult(conn, query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    char *ti = row[0];
	    if (!sameString(ti, itemName))
	        {
		printf("Get ");
		printf("<A HREF=\"%s&g=htcExtSeq&c=%s&l=%d&r=%d&i=%s\">",
		       hgcPathAndSettings(), seqName, winStart, winEnd, ti);
		printf("DNA for read on other end of plasmid</A><BR>\n");
		gotMate = TRUE;
		}
	    }
	sqlFreeResult(&sr);
	}
    if (!gotMate)
	printf("No read from other end of plasmid in database.<BR>\n");
    }

/* Get alignment info and print. */
printf("<H2>Alignments</H2>\n");
if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, &hasBin))
    errAbort("track %s not found", tdb->table);
sqlSafef(query, sizeof query, "select * from %s where qName = '%s'", table, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row+hasBin);
    slAddHead(&pslList, psl);
    }
sqlFreeResult(&sr);
slReverse(&pslList);
printAlignments(pslList, start, "htcBlatXeno", tdb->table, itemName);
printTrackHtml(tdb);
}

boolean parseRange(char *range, char **retSeq, int *retStart, int *retEnd)
/* Parse seq:start-end into components. */
{
char *s, *e;
s = strchr(range, ':');
if (s == NULL)
    return FALSE;
*s++ = 0;
e = strchr(s, '-');
if (e == NULL)
    return FALSE;
*e++ = 0;
if (!isdigit(s[0]) || !isdigit(e[0]))
    return FALSE;
*retSeq = range;
*retStart = atoi(s);
*retEnd = atoi(e);
return TRUE;
}

void mustParseRange(char *range, char **retSeq, int *retStart, int *retEnd)
/* Parse seq:start-end or die. */
{
if (!parseRange(range, retSeq, retStart, retEnd))
    errAbort("Malformed range %s", range);
}

struct psl *loadPslAt(char *track, char *qName, int qStart, int qEnd, char *tName, int tStart, int tEnd)
/* Load a specific psl */
{
struct dyString *dy = dyStringNew(1024);
struct sqlConnection *conn = hAllocConn(database);
char table[HDB_MAX_TABLE_STRING];
boolean hasBin;
struct sqlResult *sr;
char **row;
struct psl *psl;

if (!hFindSplitTable(database, tName, track, table, sizeof table, &hasBin))
    errAbort("track %s not found", track);
sqlDyStringPrintf(dy, "select * from %s ", table);
sqlDyStringPrintf(dy, "where qStart = %d ", qStart);
sqlDyStringPrintf(dy, "and qEnd = %d ", qEnd);
sqlDyStringPrintf(dy, "and qName = '%s' ", qName);
sqlDyStringPrintf(dy, "and tStart = %d ", tStart);
sqlDyStringPrintf(dy, "and tEnd = %d ", tEnd);
sqlDyStringPrintf(dy, "and tName = '%s'", tName);
sr = sqlGetResult(conn, dy->string);
row = sqlNextRow(sr);
if (row == NULL)
    errAbort("Couldn't loadPslAt %s:%d-%d", tName, tStart, tEnd);
psl = pslLoad(row + hasBin);
sqlFreeResult(&sr);
dyStringFree(&dy);
hFreeConn(&conn);
return psl;
}

struct psl *loadPslFromRangePair(char *track, char *rangePair)
/* Load a specific psl given 'qName:qStart-qEnd tName:tStart-tEnd' in rangePair. */
{
char *qRange = NULL, *tRange = NULL;
char *qName = NULL, *tName = NULL;
int qStart = 0, qEnd = 0, tStart = 0, tEnd = 0;
qRange = nextWord(&rangePair);
tRange = nextWord(&rangePair);
if (tRange == NULL)
    errAbort("Expecting two ranges in loadPslFromRangePair");
mustParseRange(qRange, &qName, &qStart, &qEnd);
mustParseRange(tRange, &tName, &tStart, &tEnd);
return loadPslAt(track, qName, qStart, qEnd, tName, tStart, tEnd);
}

void longXenoPsl1Given(struct trackDb *tdb, char *item,
                       char *otherOrg, char *otherChromTable,
                       char *otherDb, struct psl *psl, char *pslTableName )
/* Put up cross-species alignment when the second species
 * sequence is in a nib file, AND psl record is given. */
{
char otherString[256];
char *thisOrg = hOrganism(database);

cartWebStart(cart, database, "%s", tdb->longLabel);
printf("<B>%s position:</B> <a target=\"_blank\" href=\"%s?db=%s&position=%s%%3A%d-%d\">%s:%d-%d</a><BR>\n",
       otherOrg, hgTracksName(), otherDb, psl->qName, psl->qStart+1, psl->qEnd,
       psl->qName, psl->qStart+1, psl->qEnd);
printf("<B>%s size:</B> %d<BR>\n", otherOrg, psl->qEnd - psl->qStart);
printf("<B>%s position:</B> %s:%d-%d<BR>\n", thisOrg,
       psl->tName, psl->tStart+1, psl->tEnd);

printf("<B>%s size:</B> %d<BR>\n", thisOrg, psl->tEnd - psl->tStart);
printf("<B>Identical Bases:</B> %d<BR>\n", psl->match + psl->repMatch);
printf("<B>Number of Gapless Aligning Blocks:</B> %d<BR>\n", psl->blockCount );
printf("<B>Percent identity within gapless aligning blocks:</B> %3.1f%%<BR>\n", 0.1*(1000 - pslCalcMilliBad(psl, FALSE)));
printf("<B>Strand:</B> %s<BR>\n",psl->strand);
printf("<B>Browser window position:</B> %s:%d-%d<BR>\n", seqName, winStart+1, winEnd);
printf("<B>Browser window size:</B> %d<BR>\n", winEnd - winStart);
safef(otherString, sizeof otherString, "%d&pslTable=%s&otherOrg=%s&otherChromTable=%s&otherDb=%s", psl->tStart,
	pslTableName, otherOrg, otherChromTable, otherDb);

if (pslTrimToTargetRange(psl, winStart, winEnd) != NULL)
    {
    hgcAnchorSomewhere("htcLongXenoPsl2", item, otherString, psl->tName);
    printf("<BR>View details of parts of alignment within browser window</A>.<BR>\n");
    }
}

/*
   Multipurpose function to show alignments in details pages where applicable
*/
void longXenoPsl1(struct trackDb *tdb, char *item,
		  char *otherOrg, char *otherChromTable, char *otherDb)
/* Put up cross-species alignment when the second species
 * sequence is in a nib file. */
{
struct psl *psl = NULL;
char otherString[256];
char *thisOrg = hOrganism(database);

cartWebStart(cart, database, "%s", tdb->longLabel);
psl = loadPslFromRangePair(tdb->table, item);
printf("<B>%s position:</B> <a target=\"_blank\" href=\"%s?db=%s&position=%s%%3A%d-%d\">%s:%d-%d</a><BR>\n",
       otherOrg, hgTracksName(), otherDb, psl->qName, psl->qStart+1, psl->qEnd,
       psl->qName, psl->qStart+1, psl->qEnd);
printf("<B>%s size:</B> %d<BR>\n", otherOrg, psl->qEnd - psl->qStart);
printf("<B>%s position:</B> %s:%d-%d<BR>\n", thisOrg,
       psl->tName, psl->tStart+1, psl->tEnd);
printf("<B>%s size:</B> %d<BR>\n", thisOrg,
       psl->tEnd - psl->tStart);
printf("<B>Identical Bases:</B> %d<BR>\n", psl->match + psl->repMatch);
printf("<B>Number of Gapless Aligning Blocks:</B> %d<BR>\n", psl->blockCount );
printf("<B>Percent identity within gapless aligning blocks:</B> %3.1f%%<BR>\n", 0.1*(1000 - pslCalcMilliBad(psl, FALSE)));
printf("<B>Strand:</B> %s<BR>\n",psl->strand);
printf("<B>Browser window position:</B> %s:%d-%d<BR>\n", seqName, winStart+1, winEnd);
printf("<B>Browser window size:</B> %d<BR>\n", winEnd - winStart);
safef(otherString, sizeof otherString, "%d&pslTable=%s&otherOrg=%s&otherChromTable=%s&otherDb=%s", psl->tStart,
	tdb->table, otherOrg, otherChromTable, otherDb);
/* joni */
if (pslTrimToTargetRange(psl, winStart, winEnd) != NULL)
    {
    hgcAnchorSomewhere("htcLongXenoPsl2", item, otherString, psl->tName);
    printf("<BR>View details of parts of alignment within browser window</A>.<BR>\n");
    }

if (containsStringNoCase(otherDb, "zoo"))
    printf("<P><A HREF='%s&db=%s'>Go to the browser view of the %s</A><BR>\n",
	   hgTracksPathAndSettings(), otherDb, otherOrg);
printTrackHtml(tdb);
}

/* Multipurpose function to show alignments in details pages where applicable
   Show the URL from trackDb as well.
   Only used for the Chimp tracks right now. */
void longXenoPsl1Chimp(struct trackDb *tdb, char *item,
		       char *otherOrg, char *otherChromTable, char *otherDb)
/* Put up cross-species alignment when the second species
 * sequence is in a nib file. */
{
struct psl *psl = NULL;
char otherString[256];
char *cgiItem = cgiEncode(item);
char *thisOrg = hOrganism(database);

cartWebStart(cart, database, "%s", tdb->longLabel);
psl = loadPslFromRangePair(tdb->table, item);
printf("<B>%s position:</B> %s:%d-%d<BR>\n", otherOrg,
       psl->qName, psl->qStart+1, psl->qEnd);
printf("<B>%s size:</B> %d<BR>\n", otherOrg, psl->qEnd - psl->qStart);
printf("<B>%s position:</B> %s:%d-%d<BR>\n", thisOrg,
       psl->tName, psl->tStart+1, psl->tEnd);
printf("<B>%s size:</B> %d<BR>\n", thisOrg,
       psl->tEnd - psl->tStart);
printf("<B>Identical Bases:</B> %d<BR>\n", psl->match + psl->repMatch);
printf("<B>Number of Gapless Aligning Blocks:</B> %d<BR>\n", psl->blockCount );
printf("<B>Percent identity within gapless aligning blocks:</B> %3.1f%%<BR>\n", 0.1*(1000 - pslCalcMilliBad(psl, FALSE)));
printf("<B>Strand:</B> %s<BR>\n",psl->strand);
printf("<B>Browser window position:</B> %s:%d-%d<BR>\n", seqName, winStart+1, winEnd);
printf("<B>Browser window size:</B> %d<BR>\n", winEnd - winStart);
safef(otherString, sizeof otherString, "%d&pslTable=%s&otherOrg=%s&otherChromTable=%s&otherDb=%s", psl->tStart,
	tdb->table, otherOrg, otherChromTable, otherDb);

printCustomUrl(tdb, item, TRUE);
printTrackHtml(tdb);
freez(&cgiItem);
}

void longXenoPsl1zoo2(struct trackDb *tdb, char *item,
                      char *otherOrg, char *otherChromTable)
/* Put up cross-species alignment when the second species
 * sequence is in a nib file. */
{
struct psl *psl = NULL;
char otherString[256];
char anotherString[256];
char *thisOrg = hOrganism(database);

cartWebStart(cart, database, "%s", tdb->longLabel);
psl = loadPslFromRangePair(tdb->table, item);
printf("<B>%s position:</B> %s:%d-%d<BR>\n", otherOrg,
       psl->qName, psl->qStart+1, psl->qEnd);
printf("<B>%s size:</B> %d<BR>\n", otherOrg, psl->qEnd - psl->qStart);
printf("<B>%s position:</B> %s:%d-%d<BR>\n", thisOrg,
       psl->tName, psl->tStart+1, psl->tEnd);
printf("<B>%s size:</B> %d<BR>\n", thisOrg,
       psl->tEnd - psl->tStart);
printf("<B>Identical Bases:</B> %d<BR>\n", psl->match + psl->repMatch);
printf("<B>Number of Gapless Aligning Blocks:</B> %d<BR>\n", psl->blockCount );
printf("<B>Strand:</B> %s<BR>\n",psl->strand);
printf("<B>Percent identity within gapless aligning blocks:</B> %3.1f%%<BR>\n", 0.1*(1000 - pslCalcMilliBad(psl, FALSE)));
printf("<B>Browser window position:</B> %s:%d-%d<BR>\n", seqName, winStart, winEnd);
printf("<B>Browser window size:</B> %d<BR>\n", winEnd - winStart);

safef(anotherString, sizeof anotherString, "%s",otherOrg);
toUpperN(anotherString,1);
printf("Link to <a href=\"http://hgwdev-tcbruen.gi.ucsc.edu/cgi-bin/hgTracks?db=zoo%s1&position=chr1:%d-%d\">%s database</a><BR>\n",
       anotherString, psl->qStart, psl->qEnd, otherOrg);

safef(otherString, sizeof otherString, "%d&pslTable=%s&otherOrg=%s&otherChromTable=%s", psl->tStart,
        tdb->table, otherOrg, otherChromTable);
if (pslTrimToTargetRange(psl, winStart, winEnd) != NULL)
    {
    hgcAnchorSomewhere("htcLongXenoPsl2", item, otherString, psl->tName);
    printf("<BR>View details of parts of alignment within browser window</A>.<BR>\n");
    }
printTrackHtml(tdb);
}

void doAlignmentOtherDb(struct trackDb *tdb, char *item)
/* Put up cross-species alignment when the second species
 * is another db, indicated by the 3rd word of tdb->type. */
{
char *otherOrg;
char *otherDb;
char *words[8];
char *typeLine = cloneString(tdb->type);
int wordCount = chopLine(typeLine, words);
if (wordCount < 3 || !(sameString(words[0], "psl") && sameString(words[1], "xeno")))
    errAbort("doAlignmentOtherDb: trackDb type must be \"psl xeno XXX\" where XXX is the name of the other database.");
otherDb = words[2];
otherOrg = hOrganism(otherDb);
longXenoPsl1(tdb, item, otherOrg, "chromInfo", otherDb);
}

void doMultAlignZoo(struct trackDb *tdb, char *item, char *otherName )
/* Put up cross-species alignment when the second species
 * sequence is in a nib file. */
{
char chromStr[64];

/* Check to see if name is one of zoo names */
if (!(strcmp(otherName,"human")
      && strcmp(otherName,"chimp")
      && strcmp(otherName,"baboon")
      && strcmp(otherName,"cow")
      && strcmp(otherName,"pig")
      && strcmp(otherName,"cat")
      && strcmp(otherName,"dog")
      && strcmp(otherName,"mouse")
      && strcmp(otherName,"rat")
      && strcmp(otherName,"chicken")
      && strcmp(otherName,"fugu")
      && strcmp(otherName,"tetra")
      && strcmp(otherName,"zebrafish")))
    {
    safef( chromStr, sizeof chromStr, "%sChrom" , otherName );
    longXenoPsl1zoo2(tdb, item, otherName, chromStr );
    }
}

#ifdef OLD
struct chain *getChainFromRange(char *chainTable, char *chrom, int chromStart, int chromEnd)
/* get a list of chains for a range */
{
char chainTable_chrom[256];
struct dyString *dy = dyStringNew(128);
struct chain *chainList = NULL;
struct sqlConnection *conn = hAllocConn(database);
safef(chainTable_chrom, 256, "%s_%s",chrom, chainTable);


if (hTableExists(database, chainTable_chrom) )
    {
    /* lookup chain if not stored */
    char **row;
    struct sqlResult *sr = NULL;
    sqlDyStringPrintf(dy, "select id, score, qStart, qEnd, qStrand, qSize from %s where ",
                   chainTable_chrom);
    hAddBinToQuery(chromStart, chromEnd, dy);
    sqlDyStringPrintf(dy, "tEnd > %d and tStart < %d ", chromStart,chromEnd);
    sqlDyStringPrintf(dy, " order by qStart");
    sr = sqlGetResult(conn, dy->string);

    while ((row = sqlNextRow(sr)) != NULL)
        {
        int chainId = 0;
        unsigned int qStart, qEnd, qSize;
        struct chain *chain = NULL;
        char qStrand;
        chainId = sqlUnsigned(row[0]);
        qStart = sqlUnsigned(row[2]);
        qEnd = sqlUnsigned(row[3]);
        qStrand =row[4][0];
        qSize = sqlUnsigned(row[5]);
        if (qStrand == '-')
            {
            unsigned int tmp = qSize - qEnd;
            qEnd = qSize - qStart;
            qStart = tmp;
            }
        chain = NULL;
        if (chainId != 0)
            {
            chain = chainLoadIdRange(database, chainTable, chrom, chromStart, chromEnd, chainId);
            if (chain != NULL)
                slAddHead(&chainList, chain);
            }
        }
    sqlFreeResult(&sr);
    }
return chainList;
}
#endif /* OLD */

void htcPseudoGene(char *htcCommand, char *item)
/* Interface for selecting & displaying alignments from axtInfo
 * for an item from a genePred table. */
{
struct genePred *gp = NULL;
struct axtInfo *aiList = NULL;
struct axt *axtList = NULL;
struct sqlResult *sr;
char **row;
char trackTemp[256];
char *track = cartString(cart, "o");
char *chrom = cartString(cart, "c");
char *name = cartOptionalString(cart, "i");
char *db2 = cartString(cart, "db2");
int tStart = cgiInt("l");
int tEnd = cgiInt("r");
char *qChrom = cgiOptionalString("qc");
int chainId = cgiInt("ci");
int qStart = cgiInt("qs");
int qEnd = cgiInt("qe");
char table[HDB_MAX_TABLE_STRING];
char query[512];
char nibFile[512];
char qNibFile[512];
char qNibDir[512];
char tNibDir[512];
char path[512];
boolean hasBin;
struct sqlConnection *conn = hAllocConn(database);
struct sqlConnection *conn2;
struct hash *qChromHash = hashNew(0);
struct cnFill *fill;
struct chain *chain;
struct dnaSeq *tChrom = NULL;

cartWebStart(cart, database, "Alignment of %s in %s to pseudogene in %s",
	     name, hOrganism(db2), hOrganism(database));
conn2 = hAllocConn(db2);

/* get nibFile for pseudoGene */
sqlSafef(query, sizeof query, "select fileName from chromInfo where chrom = '%s'",  chrom);
if (sqlQuickQuery(conn, query, nibFile, sizeof(nibFile)) == NULL)
    errAbort("Sequence %s isn't in chromInfo", chrom);

/* get nibFile for Gene in other species */
sqlSafef(query, sizeof query, "select fileName from chromInfo where chrom = '%s'" ,qChrom);
if (sqlQuickQuery(conn2, query, qNibFile, sizeof(qNibFile)) == NULL)
    errAbort("Sequence chr1 isn't in chromInfo");

/* get gp */
if (!hFindSplitTable(db2, qChrom, track, table, sizeof table, &hasBin))
    errAbort("htcPseudoGene: table %s not found.\n",track);
else if (sameString(track, "mrna"))
    {
    struct psl *psl = NULL ;
    sqlSafef(query, sizeof(query),
             "select * from %s where qName = '%s' and tName = '%s' and tStart = %d ",
             table, name, qChrom, qStart
             );
    sr = sqlGetResult(conn2, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
        psl = pslLoad(row+hasBin);
        if (psl != NULL)
            gp = genePredFromPsl(psl, psl->tStart, psl->tEnd, 10);
        }
    sqlFreeResult(&sr);
    }
else if (table != NULL)
    {
    sqlSafef(query, sizeof(query),
             "select * from %s where name = '%s' and chrom = '%s' ",
             table, name, qChrom
             );
    sr = sqlGetResult(conn2, query);
    if ((row = sqlNextRow(sr)) != NULL)
        gp = genePredLoad(row + hasBin);
    sqlFreeResult(&sr);
    }
if (gp == NULL)
    errAbort("htcPseudoGene: Could not locate gene prediction (db=%s, table=%s, name=%s, in range %s:%d-%d) %s",
             db2, table, name, qChrom, qStart+1, qEnd, query);

/* extract nib directory from nibfile */
if (strrchr(nibFile,'/') != NULL)
    strncpy(tNibDir, nibFile, strlen(nibFile)-strlen(strrchr(nibFile,'/')));
else
    errAbort("Cannot find nib directory for %s\n",nibFile);
tNibDir[strlen(nibFile)-strlen(strrchr(nibFile,'/'))] = '\0';

if (strrchr(qNibFile,'/') != NULL)
    strncpy(qNibDir, qNibFile, strlen(qNibFile)-strlen(strrchr(qNibFile,'/')));
else
    errAbort("Cannot find nib directory for %s\n",qNibFile);
qNibDir[strlen(qNibFile)-strlen(strrchr(qNibFile,'/'))] = '\0';

safef(path, sizeof path, "%s/%s.nib", tNibDir, chrom);

/* load chain */
if (sameString(database,db2))
    {
    track = "selfChain";
    if (!hTableExists(database, "chr1_selfChain"))
        track = "chainSelf";
    }
else
    {
    safef(trackTemp, sizeof trackTemp, "%sChain",hOrganism(db2));
    trackTemp[0] = tolower(trackTemp[0]);
    track = trackTemp;
    }
if (chainId > 0 )
    {
    chain = chainDbLoad(conn, database, track, chrom, chainId);

    /* get list of axts for a chain */
    AllocVar(fill);
    fill->qName = cloneString(qChrom);
    fill->tSize = tEnd-tStart;
    fill->tStart = tStart;
    fill->chainId = chainId;
    fill->qSize = gp->txEnd - gp->txStart;
    fill->qStart = max(qStart, gp->txStart);
    fill->children = NULL;
    fill->next = NULL;
    fill->qStrand = chain->qStrand;

    tChrom = nibLoadPartMasked(NIB_MASK_MIXED, nibFile,
            fill->tStart, fill->tSize);
    axtList = netFillToAxt(fill, tChrom, hChromSize(database, chrom), qChromHash, qNibDir, chain, TRUE);
    /* make sure list is in correct order */
    if (axtList != NULL)
        if (axtList->next != NULL)
            if ((gp->strand[0] == '+' && axtList->tStart > axtList->next->tStart)
                || (gp->strand[0] == '-' && axtList->tStart < axtList->next->tStart) )
                slReverse(&axtList);


    /* fill in gaps between axt blocks */
    /* allows display of aligned coding regions */
    axtFillGap(&axtList,qNibDir, gp->strand[0]);

    if (gp->strand[0] == '-')
        axtListReverse(&axtList, database);
    if (axtList != NULL)
        if (axtList->next != NULL)
            if ((axtList->next->tStart < axtList->tStart && gp->strand[0] == '+') ||
                (axtList->next->tStart > axtList->tStart && (gp->strand[0] == '-')))
                slReverse(&axtList);

    /* output fancy formatted alignment */
    puts("<PRE><TT>");
    axtOneGeneOut(database, axtList, LINESIZE, stdout , gp, qNibFile);
    puts("</TT></PRE>");
    }

axtInfoFreeList(&aiList);
hFreeConn(&conn2);
}

void htcLongXenoPsl2(char *htcCommand, char *item)
/* Display alignment - loading sequence from nib file. */
{
char *pslTable = cgiString("pslTable");
char *otherOrg = cgiString("otherOrg");
char *otherDb = cgiString("otherDb");
struct psl *psl = loadPslFromRangePair(pslTable,  item);
char *qChrom;
char *ptr;
char name[128];
struct dnaSeq *qSeq = NULL;

/* In hg10 tables, psl->qName can be org.chrom.  Strip it down to just
 * the chrom: */
qChrom = psl->qName;
if ((ptr = strchr(qChrom, '.')) != NULL)
    qChrom = ptr+1;

/* Make sure that otherOrg's chrom size matches psl's qSize */
if (hChromSize(database, qChrom) != psl->qSize)
    errAbort("Alignment's query size for %s is %d, but the size of %s in database %s is %d.  Incorrect database in trackDb.type?",
	     qChrom, psl->qSize, qChrom, otherDb, hChromSize(otherDb, qChrom));

psl = pslTrimToTargetRange(psl, winStart, winEnd);

qSeq = loadGenomePart(otherDb, qChrom, psl->qStart, psl->qEnd);
snprintf(name, sizeof(name), "%s.%s", otherOrg, qChrom);
char title[1024];
safef(title, sizeof title, "%s %dk", name, psl->qStart/1000);
htmlFramesetStart(title);
showSomeAlignment(psl, qSeq, gftDnaX, psl->qStart, psl->qEnd, name, 0, 0);
}

void doAlignCompGeno(struct trackDb *tdb, char *itemName, char *otherGenome)
    /* Handle click on blat or blastz track in a generic fashion */
    /* otherGenome is the text to display for genome name on details page */
{
char query[256];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
char *chrom = cartString(cart, "c");
struct psl *pslList = NULL, *psl;
boolean hasBin;
char table[HDB_MAX_TABLE_STRING];

char *typeLine = cloneString(tdb->type);
char *words[8];
int wordCount = chopLine(typeLine, words);
if (wordCount == 3)
    {
    if (sameString(words[0], "psl") &&
        sameString(words[1], "xeno"))
            {
            /* words[2] will contain other db */
            doAlignmentOtherDb(tdb, itemName);
            freeMem(typeLine);
            return;
            }
    }
freeMem(typeLine);
cartWebStart(cart, database, "%s", itemName);
printPosOnChrom(chrom,start,end,NULL,FALSE,NULL);
printf("<H1>Information on %s Sequence %s</H1>", otherGenome, itemName);

printf("Get ");
printf("<A HREF=\"%s&g=htcExtSeq&c=%s&l=%d&r=%d&i=%s\">",
               hgcPathAndSettings(), seqName, winStart, winEnd, itemName);
printf("%s DNA</A><BR>\n", otherGenome);

/* Get alignment info and print. */
printf("<H2>Alignments</H2>\n");
if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, &hasBin))
    errAbort("doAlignCompGeno track %s not found", tdb->table);

/* if this is a non-split table then query with tName */
if (startsWith(tdb->table, table))
    sqlSafef(query, sizeof(query), "select * from %s where qName = '%s' and tName = '%s'", table, itemName,seqName);
else
    sqlSafef(query, sizeof(query), "select * from %s where qName = '%s'", table, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row+hasBin);
    slAddHead(&pslList, psl);
    }
sqlFreeResult(&sr);
slReverse(&pslList);
printAlignments(pslList, start, "htcBlatXeno", tdb->table, itemName);
printTrackHtml(tdb);
}

void doTSS(struct trackDb *tdb, char *itemName)
/* Handle click on DBTSS track. */
{
char query[256];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row = NULL;
int start = cartInt(cart, "o");
struct psl *pslList = NULL, *psl = NULL;
boolean hasBin = TRUE;
char *table = "refFullAli"; /* Table with the pertinent PSL data */

cartWebStart(cart, database, "%s", itemName);
printf("<H1>Information on DBTSS Sequence %s</H1>", itemName);
printf("Get ");
printf("<A HREF=\"%s&g=htcExtSeq&c=%s&l=%d&r=%d&i=%s\">",
       hgcPathAndSettings(), seqName, winStart, winEnd, itemName);
printf("Sequence</A><BR>\n");

/* Get alignment info and print. */
printf("<H2>Alignments</H2>\n");
sqlSafef(query, sizeof query, "select * from %s where qName = '%s'", table, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row + hasBin);
    slAddHead(&pslList, psl);
    }

sqlFreeResult(&sr);
slReverse(&pslList);
printAlignments(pslList, start, "htcCdnaAli", tdb->table, itemName);
printTrackHtml(tdb);
}

void doEst3(char *itemName)
/* Handle click on EST 3' end track. */
{
struct est3 el;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset;

cartWebStart(cart, database, "EST 3' Ends");
printf("<H2>EST 3' Ends</H2>\n");

rowOffset = hOffsetPastBin(database, seqName, "est3");
sqlSafef(query, sizeof query, "select * from est3 where chrom = '%s' and chromStart = %d",
	seqName, start);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    est3StaticLoad(row+rowOffset, &el);
    printf("<B>EST 3' End Count:</B> %d<BR>\n", el.estCount);
    bedPrintPos((struct bed *)&el, 3, NULL);
    printf("<B>strand:</B> %s<BR>\n", el.strand);
    htmlHorizontalLine();
    }

puts("<P>This track shows where clusters of EST 3' ends hit the "
     "genome.  In many cases these represent the 3' ends of genes. "
     "This data was kindly provided by Lukas Wagner and Greg Schuler "
     "at NCBI.  Additional filtering was applied by Jim Kent.</P>");
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doEncodeRna(struct trackDb *tdb, char *itemName)
/* Handle click on encodeRna track. */
{
struct encodeRna rna;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset;
struct slName *nameList, *sl;

genericHeader(tdb, itemName);
rowOffset = hOffsetPastBin(database, seqName, tdb->table);
sqlSafef(query, sizeof query, "select * from %s where chrom = '%s' and chromStart = %d and name = '%s'",
      tdb->table, seqName, start, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    encodeRnaStaticLoad(row + rowOffset, &rna);
    printf("<B>name:</B> %s<BR>\n", rna.name);
    bedPrintPos((struct bed *)&rna, 3, tdb);
    printf("<B>strand:</B> %s<BR>\n", rna.strand);
    printf("<B>type:</B> %s<BR>\n", rna.type);
    printf("<B>score:</B> %2.1f<BR><BR>\n", rna.fullScore);
    printf("<B>is pseudo-gene:</B> %s<BR>\n", (rna.isPsuedo ? "yes" : "no"));
    printf("<B>is Repeatmasked:</B> %s<BR>\n", (rna.isRmasked ? "yes" : "no"));
    printf("<B>is Transcribed:</B> %s<BR>\n", (rna.isTranscribed ? "yes" : "no"));
    printf("<B>is an evoFold prediction:</B> %s<BR>\n", (rna.isPrediction ? "yes" : "no"));
    printf("<B>program predicted with:</B> %s<BR>\n", rna.source);
    printf("<BR><B>This region is transcribed in: </B>");
    nameList = slNameListFromString(rna.transcribedIn,',');
    if(nameList==NULL||sameString(nameList->name,"."))
      printf("<BR>&nbsp;&nbsp;&nbsp;&nbsp;Not transcribed\n");
    else
      for (sl=nameList;sl!=NULL;sl=sl->next)
          printf("<BR>&nbsp;&nbsp;&nbsp;&nbsp;%s\n",sl->name);
    }
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}


void doRnaGene(struct trackDb *tdb, char *itemName)
/* Handle click on RNA Genes track. */
{
struct rnaGene rna;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset;

genericHeader(tdb, itemName);
rowOffset = hOffsetPastBin(database, seqName, tdb->table);
sqlSafef(query, sizeof query, "select * from %s where chrom = '%s' and chromStart = %d and name = '%s'",
	tdb->table, seqName, start, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    rnaGeneStaticLoad(row + rowOffset, &rna);
    printf("<B>name:</B> %s<BR>\n", rna.name);
    printf("<B>type:</B> %s<BR>\n", rna.type);
    printf("<B>score:</B> %2.1f<BR>\n", rna.fullScore);
    printf("<B>is pseudo-gene:</B> %s<BR>\n", (rna.isPsuedo ? "yes" : "no"));
    printf("<B>program predicted with:</B> %s<BR>\n", rna.source);
    printf("<B>strand:</B> %s<BR>\n", rna.strand);
    bedPrintPos((struct bed *)&rna, 3, tdb);
    htmlHorizontalLine();
    }
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doStsMarker(struct trackDb *tdb, char *marker)
/* Respond to click on an STS marker. */
{
char *table = tdb->table;
char query[256];
char title[256];
struct sqlConnection *conn = hAllocConn(database);
boolean stsInfo2Exists = sqlTableExists(conn, "stsInfo2");
boolean stsInfoExists = sqlTableExists(conn, "stsInfo");
boolean stsMapExists = sqlTableExists(conn, "stsMap");
struct sqlConnection *conn1 = hAllocConn(database);
struct sqlResult *sr = NULL, *sr1 = NULL;
char **row;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
struct stsMap stsRow;
struct stsInfo *infoRow = NULL;
struct stsInfo2 *info2Row = NULL;
char stsid[20];
int i;
struct psl *pslList = NULL, *psl;
int pslStart;
char *sqlMarker = marker;
boolean hasBin;

/* Make sure to escpae single quotes for DB parseability */
if (strchr(marker, '\''))
    sqlMarker = replaceChars(marker, "'", "''");

/* Print out non-sequence info */
safef(title, sizeof title, "STS Marker %s", marker);
cartWebStart(cart, database, "%s", title);

/* Find the instance of the object in the bed table */
sqlSafef(query, sizeof query, "SELECT * FROM %s WHERE name = '%s' "
               "AND chrom = '%s' AND chromStart = %d "
               "AND chromEnd = %d",
        table, sqlMarker, seqName, start, end);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
hasBin = hOffsetPastBin(database, seqName, table);
if (row != NULL)
    {
    if (stsMapExists)
        stsMapStaticLoad(row+hasBin, &stsRow);
    else
        /* Load and convert from original bed format */
        {
        struct stsMarker oldStsRow;
        stsMarkerStaticLoad(row+hasBin, &oldStsRow);
	stsMapFromStsMarker(&oldStsRow, &stsRow);
	}
    if (stsInfo2Exists)
        {
        /* Find the instance of the object in the stsInfo2 table */
	sqlFreeResult(&sr);
	sqlSafef(query, sizeof query, "SELECT * FROM stsInfo2 WHERE identNo = '%d'", stsRow.identNo);
	sr = sqlMustGetResult(conn, query);
	row = sqlNextRow(sr);
	if (row != NULL)
	    {
            int i;
	    char **cl;
	    cl = (char **)needMem(52*sizeof(char *));
	    for (i = 0; i < 52; ++i)
		cl[i] = cloneString(row[i]);
	    info2Row = stsInfo2Load(row);
	    infoRow = stsInfoLoad(cl);
	    freeMem(cl);
	    }
	}
    else if (stsInfoExists)
        {
        /* Find the instance of the object in the stsInfo table */
	sqlFreeResult(&sr);
	sqlSafef(query, sizeof query, "SELECT * FROM stsInfo WHERE identNo = '%d'", stsRow.identNo);
	sr = sqlMustGetResult(conn, query);
	row = sqlNextRow(sr);
	if (row != NULL)
	    infoRow = stsInfoLoad(row);
	}
    if (((stsInfo2Exists) || (stsInfoExists)) && (row != NULL))
	{
	printf("<TABLE>\n");
	printf("<TR><TH ALIGN=left>Chromosome:</TH><TD>%s</TD></TR>\n", seqName);
	printf("<TR><TH ALIGN=left>Start:</TH><TD>%d</TD></TR>\n",start+1);
	printf("<TR><TH ALIGN=left>End:</TH><TD>%d</TD></TR>\n",end);
	printBand(seqName, start, end, TRUE);
	printf("</TABLE>\n");
	htmlHorizontalLine();

	/* Print out marker name and links to UniSTS, Genebank, GDB */
	if (infoRow->nameCount > 0)
	    {
	    printf("<TABLE>\n");
	    printf("<TR><TH>Other names:</TH><TD>%s",infoRow->otherNames[0]);
            for (i = 1; i < infoRow->nameCount; i++)
		printf(", %s",infoRow->otherNames[i]);
	    printf("</TD></TR>\n</TABLE>\n");
	    htmlHorizontalLine();
	    }
	printf("<TABLE>\n");
	printf("<TR><TH ALIGN=left>UCSC STS id:</TH><TD>%d</TD></TR>\n", stsRow.identNo);
	printf("<TR><TH ALIGN=left>UniSTS id:</TH><TD><A HREF=");
	printUnistsUrl(stdout, infoRow->dbSTSid);
	printf(" TARGET=_BLANK>%d</A></TD></TR>\n", infoRow->dbSTSid);
        if (infoRow->otherDbstsCount > 0)
	    {
	    printf("<TR><TH ALIGN=left>Related UniSTS ids:</TH>");
            for (i = 0; i < infoRow->otherDbstsCount; i++)
		{
		printf("<TD><A HREF=");
		printUnistsUrl(stdout, infoRow->otherDbSTS[i]);
		printf(" TARGET=_BLANK>%d</A></TD>", infoRow->otherDbSTS[i]);
		}
	    printf("</TR>\n");
            }
        if (infoRow->gbCount > 0)
	    {
	    printf("<TR><TH ALIGN=left>Genbank:</TH>");
            for (i = 0; i < infoRow->gbCount; i++)
		{
		printf("<TD><A HREF=\"");
		printEntrezNucleotideUrl(stdout, infoRow->genbank[i]);
		printf("\" TARGET=_BLANK>%s</A></TD>", infoRow->genbank[i]);
		}
	    printf("</TR>\n");
            }
        if (infoRow->gdbCount > 0)
	    {
	    printf("<TR><TH ALIGN=left>GDB:</TH>");
            for (i = 0; i < infoRow->gdbCount; i++)
		{
		printf("<TD>");
		printf("%s</TD>", infoRow->gdb[i]);
		}
	    printf("</TR>\n");
            }
	printf("<TR><TH ALIGN=left>Organism:</TH><TD>%s</TD></TR>\n",infoRow->organism);
	printf("</TABLE>\n");
	htmlHorizontalLine();
	/* Print out primer information */
	if (!sameString(infoRow->leftPrimer,""))
	    {
	    printf("<TABLE>\n");
	    printf("<TR><TH ALIGN=left>Left Primer:</TH><TD>%s</TD></TR>\n",infoRow->leftPrimer);
	    printf("<TR><TH ALIGN=left>Right Primer:</TH><TD>%s</TD></TR>\n",infoRow->rightPrimer);
	    printf("<TR><TH ALIGN=left>Distance:</TH><TD>%s bps</TD></TR>\n",infoRow->distance);
	    printf("</TABLE>\n");
	    htmlHorizontalLine();
	    }
	/* Print out information from STS maps for this marker */
        if ((!sameString(infoRow->genethonName,""))
	    || (!sameString(infoRow->marshfieldName,""))
	    || (stsInfo2Exists && info2Row != NULL && (!sameString(info2Row->decodeName,""))))
	    {
            printf("<H3>Genetic Map Positions</H3>\n");
	    printf("<TABLE>\n");
	    printf("<TH>&nbsp</TH><TH ALIGN=left WIDTH=150>Name</TH><TH ALIGN=left WIDTH=150>Chromosome</TH><TH ALIGN=left WIDTH=150>Position</TH></TR>\n");
            if (!sameString(infoRow->genethonName,""))
		printf("<TH ALIGN=left>Genethon:</TH><TD WIDTH=150>%s</TD><TD WIDTH=150>%s</TD><TD WIDTH=150>%.2f</TD></TR>\n",
		       infoRow->genethonName, infoRow->genethonChr, infoRow->genethonPos);
	    if (!sameString(infoRow->marshfieldName,""))
		printf("<TH ALIGN=left>Marshfield:</TH><TD WIDTH=150>%s</TD><TD WIDTH=150>%s</TD><TD WIDTH=150>%.2f</TD></TR>\n",
		       infoRow->marshfieldName, infoRow->marshfieldChr,
		       infoRow->marshfieldPos);
	    if ((stsInfo2Exists) && (!sameString(info2Row->decodeName,"")))
		printf("<TH ALIGN=left>deCODE:</TH><TD WIDTH=150>%s</TD><TD WIDTH=150>%s</TD><TD WIDTH=150>%.2f</TD></TR>\n",
		       info2Row->decodeName, info2Row->decodeChr,
		       info2Row->decodePos);
	    printf("</TABLE><P>\n");
	    }
        if (!sameString(infoRow->wiyacName,""))
	    {
            printf("<H3>Whitehead YAC Map Position</H3>\n");
	    printf("<TABLE>\n");
	    printf("<TH>&nbsp</TH><TH ALIGN=left WIDTH=150>Name</TH><TH ALIGN=left WIDTH=150>Chromosome</TH><TH ALIGN=left WIDTH=150>Position</TH></TR>\n");
	    printf("<TH ALIGN=left>WI YAC:</TH><TD WIDTH=150>%s</TD><TD WIDTH=150>%s</TD><TD WIDTH=150>%.2f</TD></TR>\n",
		   infoRow->wiyacName, infoRow->wiyacChr, infoRow->wiyacPos);
	    printf("</TABLE><P>\n");
	    }
        if ((!sameString(infoRow->wirhName,""))
	    || (!sameString(infoRow->gm99gb4Name,""))
	    || (!sameString(infoRow->gm99g3Name,""))
	    || (!sameString(infoRow->tngName,"")))
	    {
            printf("<H3>RH Map Positions</H3>\n");
	    printf("<TABLE>\n");
            if ((!sameString(infoRow->wirhName,""))
		|| (!sameString(infoRow->gm99gb4Name,""))
		|| (!sameString(infoRow->gm99g3Name,"")))
		printf("<TH>&nbsp</TH><TH ALIGN=left WIDTH=150>Name</TH><TH ALIGN=left WIDTH=150>Chromosome</TH><TH ALIGN=left WIDTH=150>Position (LOD)</TH></TR>\n");
	    else
		printf("<TH>&nbsp</TH><TH ALIGN=left WIDTH=150>Name</TH><TH ALIGN=left WIDTH=150>Chromosome</TH><TH ALIGN=left WIDTH=150>Position</TH></TR>\n");
	    if (!sameString(infoRow->gm99gb4Name,""))
		printf("<TH ALIGN=left>GM99 Gb4:</TH><TD WIDTH=150>%s</TD><TD WIDTH=150>%s</TD><TD WIDTH=150>%.2f (%.2f)</TD></TR>\n",
		       infoRow->gm99gb4Name, infoRow->gm99gb4Chr, infoRow->gm99gb4Pos,
		       infoRow->gm99gb4LOD);
	    if (!sameString(infoRow->gm99g3Name,""))
		printf("<TH ALIGN=left>GM99 G3:</TH><TD WIDTH=150>%s</TD><TD WIDTH=150>%s</TD><TD WIDTH=150>%.2f (%.2f)</TD></TR>\n",
		       infoRow->gm99g3Name, infoRow->gm99g3Chr, infoRow->gm99g3Pos,
		       infoRow->gm99g3LOD);
	    if (!sameString(infoRow->wirhName,""))
		printf("<TH ALIGN=left>WI RH:</TH><TD WIDTH=150>%s</TD><TD WIDTH=150>%s</TD><TD WIDTH=150>%.2f (%.2f)</TD></TR>\n",
		       infoRow->wirhName, infoRow->wirhChr, infoRow->wirhPos,
		       infoRow->wirhLOD);
	    if (!sameString(infoRow->tngName,""))
		printf("<TH ALIGN=left>Stanford TNG:</TH><TD WIDTH=150>%s</TD><TD WIDTH=150>%s</TD><TD WIDTH=150>%.2f</TD></TR>\n",
		       infoRow->tngName, infoRow->tngChr, infoRow->tngPos);
	    printf("</TABLE><P>\n");
	    }
	/* Print out alignment information - full sequence */
	webNewSection("Genomic Alignments:");
        sqlSafef(query, sizeof query, "SELECT * FROM all_sts_seq WHERE qName = '%d'",
                infoRow->identNo);
	sr1 = sqlGetResult(conn1, query);
	hasBin = hOffsetPastBin(database, seqName, "all_sts_seq");
	i = 0;
	pslStart = 0;
	while ((row = sqlNextRow(sr1)) != NULL)
            {
	    psl = pslLoad(row+hasBin);
	    if ((sameString(psl->tName, seqName)) && (abs(psl->tStart - start) < 1000))
		pslStart = psl->tStart;
	    slAddHead(&pslList, psl);
	    i++;
	    }
	slReverse(&pslList);
        if (i > 0)
	    {
	    printf("<H3>Full sequence:</H3>\n");
	    safef(stsid, sizeof stsid, "%d", infoRow->identNo);
	    printAlignments(pslList, pslStart, "htcCdnaAli", "all_sts_seq", stsid);
	    sqlFreeResult(&sr1);
	    htmlHorizontalLine();
	    }
	slFreeList(&pslList);
	/* Print out alignment information - primers */
	safef(stsid, sizeof stsid, "dbSTS_%d", infoRow->dbSTSid);
        sqlSafef(query, sizeof query, "SELECT * FROM all_sts_primer WHERE qName = '%s'",
                stsid);
	hasBin = hOffsetPastBin(database, seqName, "all_sts_primer");
	sr1 = sqlGetResult(conn1, query);
	i = 0;
	pslStart = 0;
	while ((row = sqlNextRow(sr1)) != NULL)
            {
	    psl = pslLoad(row+hasBin);
	    if ((sameString(psl->tName, seqName)) && (abs(psl->tStart - start) < 1000))
		pslStart = psl->tStart;
	    slAddHead(&pslList, psl);
	    i++;
	    }
	slReverse(&pslList);
        if (i > 0)
	    {
	    printf("<H3>Primers:</H3>\n");
	    printAlignments(pslList, pslStart, "htcCdnaAli", "all_sts_primer", stsid);
	    sqlFreeResult(&sr1);
	    }
	slFreeList(&pslList);
	stsInfoFree(&infoRow);
	}
    else
	{
	printf("<TABLE>\n");
	printf("<TR><TH ALIGN=left>Chromosome:</TH><TD>%s</TD></TR>\n", seqName);
	printf("<TR><TH ALIGN=left>Position:</TH><TD>%d</TD></TR>\n", (stsRow.chromStart+stsRow.chromEnd)>>1);
	printf("<TR><TH ALIGN=left>UCSC STS id:</TH><TD>%d</TD></TR>\n", stsRow.identNo);
	if (!sameString(stsRow.ctgAcc, "-"))
	    printf("<TR><TH ALIGN=left>Clone placed on:</TH><TD>%s</TD></TR>\n", stsRow.ctgAcc);
	if (!sameString(stsRow.otherAcc, "-"))
	    printf("<TR><TH ALIGN=left>Other clones hit:</TH><TD>%s</TD></TR>\n", stsRow.otherAcc);
	if (!sameString(stsRow.genethonChrom, "0"))
	    printf("<TR><TH ALIGN=left>Genethon:</TH><TD>chr%s</TD><TD>%.2f</TD></TR>\n", stsRow.genethonChrom, stsRow.genethonPos);
	if (!sameString(stsRow.marshfieldChrom, "0"))
	    printf("<TR><TH ALIGN=left>Marshfield:</TH><TD>chr%s</TD><TD>%.2f</TD></TR>\n", stsRow.marshfieldChrom, stsRow.marshfieldPos);
	if (!sameString(stsRow.gm99Gb4Chrom, "0"))
	    printf("<TR><TH ALIGN=left>GeneMap99 GB4:</TH><TD>chr%s</TD><TD>%.2f</TD></TR>\n", stsRow.gm99Gb4Chrom, stsRow.gm99Gb4Pos);
	if (!sameString(stsRow.shgcG3Chrom, "0"))
	    printf("<TR><TH ALIGN=left>GeneMap99 G3:</TH><TD>chr%s</TD><TD>%.2f</TD></TR>\n", stsRow.shgcG3Chrom, stsRow.shgcG3Pos);
	if (!sameString(stsRow.wiYacChrom, "0"))
	    printf("<TR><TH ALIGN=left>Whitehead YAC:</TH><TD>chr%s</TD><TD>%.2f</TD></TR>\n", stsRow.wiYacChrom, stsRow.wiYacPos);
	if (!sameString(stsRow.wiRhChrom, "0"))
	    printf("<TR><TH ALIGN=left>Whitehead RH:</TH><TD>chr%s</TD><TD>%.2f</TD></TR>\n", stsRow.wiRhChrom, stsRow.wiRhPos);
	if (!sameString(stsRow.shgcTngChrom, "0"))
	    printf("<TR><TH ALIGN=left>Stanford TNG:</TH><TD>chr%s</TD><TD>%.2f</TD></TR>\n", stsRow.shgcTngChrom, stsRow.shgcTngPos);
	if (!sameString(stsRow.fishChrom, "0"))
            printf("<TR><TH ALIGN=left>FISH:</TH><TD>%s.%s - %s.%s</TD></TR>\n", stsRow.fishChrom,
		   stsRow.beginBand, stsRow.fishChrom, stsRow.endBand);
	printf("</TABLE>\n");
	htmlHorizontalLine();
	if (stsRow.score == 1000)
	    printf("<H3>This is the only location found for %s</H3>\n",marker);
	else
	    {
	    sqlFreeResult(&sr);
	    printf("<H4>Other locations found for %s in the genome:</H4>\n", marker);
	    printf("<TABLE>\n");
	    sqlSafef(query, sizeof query, "SELECT * FROM %s WHERE name = '%s' "
                           "AND (chrom != '%s' OR chromStart != %d OR chromEnd != %d)",
                    table, marker, seqName, start, end);
	    sr = sqlGetResult(conn,query);
	    hasBin = hOffsetPastBin(database, seqName, table);
	    while ((row = sqlNextRow(sr)) != NULL)
		{
                if (stsMapExists)
                    stsMapStaticLoad(row+hasBin, &stsRow);
                else
                    /* Load and convert from original bed format */
                    {
                    struct stsMarker oldStsRow;
                    stsMarkerStaticLoad(row+hasBin, &oldStsRow);
		    stsMapFromStsMarker(&oldStsRow, &stsRow);
		    }
		printf("<TR><TD>%s:</TD><TD>%d</TD></TR>\n",
		       stsRow.chrom, (stsRow.chromStart+stsRow.chromEnd)>>1);
		}
            printf("</TABLE>\n");
	    }
	htmlHorizontalLine();
	}
    }
webNewSection("Notes:");
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
hFreeConn(&conn1);
}

void doStsMapMouse(struct trackDb *tdb, char *marker)
/* Respond to click on an STS marker. */
{
char *table = tdb->table;
char title[256];
char query[256];
struct sqlConnection *conn = hAllocConn(database);
struct sqlConnection *conn1 = hAllocConn(database);
struct sqlResult *sr = NULL, *sr1 = NULL;
char **row;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
char *hgsid = cartSessionId(cart);
struct stsMapMouse stsRow;
struct stsInfoMouse *infoRow;
char stsid[20];
int i;
struct psl *pslList = NULL, *psl;
int pslStart;

/* Print out non-sequence info */
safef(title, sizeof title, "STS Marker %s", marker);
cartWebStart(cart, database, "%s", title);

/* Find the instance of the object in the bed table */
sqlSafef(query, sizeof query, "SELECT * FROM %s WHERE name = '%s' "
               "AND chrom = '%s' AND chromStart = %d "
               "AND chromEnd = %d",
        table, marker, seqName, start, end);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    stsMapMouseStaticLoad(row, &stsRow);
    /* Find the instance of the object in the stsInfo table */
    sqlFreeResult(&sr);
    sqlSafef(query, sizeof query, "SELECT * FROM stsInfoMouse WHERE identNo = '%d'", stsRow.identNo);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
	{
	infoRow = stsInfoMouseLoad(row);
	printf("<TABLE>\n");
	printf("<TR><TH ALIGN=left>Chromosome:</TH><TD>%s</TD></TR>\n", seqName);
	printf("<TR><TH ALIGN=left>Start:</TH><TD>%d</TD></TR>\n",start+1);
	printf("<TR><TH ALIGN=left>End:</TH><TD>%d</TD></TR>\n",end);
	printf("</TABLE>\n");
	htmlHorizontalLine();
	printf("<TABLE>\n");
        printf("<TR><TH ALIGN=left>MGI Marker ID:</TH><TD><B>MGI:</B>");
	printf("<A HREF = \"http://www.informatics.jax.org/marker/MGI:%d\" TARGET=_blank>%d</A></TD></TR>\n", infoRow->MGIMarkerID, infoRow->MGIMarkerID);
        printf("<TR><TH ALIGN=left>MGI Probe ID:</TH><TD><B>MGI:</B>");
	printf("<A HREF = \"http://www.informatics.jax.org/marker/MGI:%d\" TARGET=_blank>%d</A></TD></TR>\n", infoRow->MGIPrimerID, infoRow->MGIPrimerID);
	printf("</TABLE>\n");
	htmlHorizontalLine();
	/* Print out primer information */
	printf("<TABLE>\n");
	printf("<TR><TH ALIGN=left>Left Primer:</TH><TD>%s</TD></TR>\n",infoRow->primer1);
	printf("<TR><TH ALIGN=left>Right Primer:</TH><TD>%s</TD></TR>\n",infoRow->primer2);
	printf("<TR><TH ALIGN=left>Distance:</TH><TD>%s bps</TD></TR>\n",infoRow->distance);
	printf("</TABLE>\n");
	htmlHorizontalLine();
	/* Print out information from genetic maps for this marker */
        printf("<H3>Genetic Map Position</H3>\n");
	printf("<TABLE>\n");
	printf("<TH>&nbsp</TH><TH ALIGN=left WIDTH=150>Name</TH><TH ALIGN=left WIDTH=150>Chromosome</TH><TH ALIGN=left WIDTH=150>Position</TH></TR>\n");
        printf("<TH ALIGN=left>&nbsp</TH><TD WIDTH=150>%s</TD><TD WIDTH=150>%s</TD><TD WIDTH=150>%.2f</TD></TR>\n",
               infoRow->stsMarkerName, infoRow->Chr, infoRow->geneticPos);
        printf("</TABLE><P>\n");

        /* Print out alignment information - full sequence */
        webNewSection("Genomic Alignments:");
        safef(stsid, sizeof stsid, "%d", infoRow->MGIPrimerID);
        sqlSafef(query, sizeof query, "SELECT * FROM all_sts_primer"
                       " WHERE  qName = '%s' AND  tStart = '%d' AND tEnd = '%d'",stsid, start, end);
        sr1 = sqlGetResult(conn1, query);
        i = 0;
        pslStart = 0;
	while ((row = sqlNextRow(sr1)) != NULL)
            {
	    psl = pslLoad(row);
	    if ((sameString(psl->tName, seqName)) && (abs(psl->tStart - start) < 1000))
		pslStart = psl->tStart;
	    slAddHead(&pslList, psl);
	    i++;
	    }
	slReverse(&pslList);
        if (i > 0)
	    {
	    printf("<H3>Primers:</H3>\n");
	    printAlignments(pslList, pslStart, "htcCdnaAli", "all_sts_primer", stsid);
	    sqlFreeResult(&sr1);
	    }
	slFreeList(&pslList);
	stsInfoMouseFree(&infoRow);
	}
    htmlHorizontalLine();

    if (stsRow.score == 1000)
	printf("<H3>This is the only location found for %s</H3>\n",marker);
    else
	{
	sqlFreeResult(&sr);
	printf("<H4>Other locations found for %s in the genome:</H4>\n", marker);
	printf("<TABLE>\n");
	sqlSafef(query, sizeof query, "SELECT * FROM %s WHERE name = '%s' "
                       "AND (chrom != '%s' OR chromStart != %d OR chromEnd != %d)",
                table, marker, seqName, start, end);
	sr = sqlGetResult(conn,query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    stsMapMouseStaticLoad(row, &stsRow);
	    printf("<TR><TD>%s:</TD><TD><A HREF = \"../cgi-bin/hgc?hgsid=%s&o=%u&t=%d&g=stsMapMouse&i=%s&c=%s\" target=_blank>%d</A></TD></TR>\n",
		   stsRow.chrom, hgsid, stsRow.chromStart,stsRow.chromEnd, stsRow.name, stsRow.chrom,(stsRow.chromStart+stsRow.chromEnd)>>1);
	    }
	printf("</TABLE>\n");
	}
    }
webNewSection("Notes:");
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
hFreeConn(&conn1);
}



void doStsMapMouseNew(struct trackDb *tdb, char *marker)
/* Respond to click on an STS marker. */
{
char *table = tdb->table;
char title[256];
char query[256];
char query1[256];
struct sqlConnection *conn = hAllocConn(database);
struct sqlConnection *conn1 = hAllocConn(database);
struct sqlResult *sr = NULL, *sr1 = NULL, *sr2 = NULL;
char **row;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
char *hgsid = cartSessionId(cart);
struct stsMapMouseNew stsRow;
struct stsInfoMouseNew *infoRow;
char stsid[20];
char stsPrimer[40];
char stsClone[45];
int i;
struct psl *pslList = NULL, *psl;
int pslStart;
 char sChar='%';

/* Print out non-sequence info */

safef(title, sizeof title, "STS Marker %s\n", marker);
/* safef(title, sizeof title, "STS Marker <A HREF=\"http://www.informatics.jax.org/searches/marker_report.cgi?string\%%3AmousemarkerID=%s\" TARGET=_BLANK>%s</A>\n", marker, marker); */
cartWebStart(cart, database, "%s", title);

/* Find the instance of the object in the bed table */
sqlSafef(query, sizeof query, "SELECT * FROM %s WHERE name = '%s' "
                "AND chrom = '%s' AND chromStart = %d "
                "AND chromEnd = %d",
	        table, marker, seqName, start, end);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    stsMapMouseNewStaticLoad(row, &stsRow);
    /* Find the instance of the object in the stsInfo table */
    sqlFreeResult(&sr);
    sqlSafef(query, sizeof query, "SELECT * FROM stsInfoMouseNew WHERE identNo = '%d'", stsRow.identNo);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
	{
	infoRow = stsInfoMouseNewLoad(row);
	printf("<TABLE>\n");
	printf("<TR><TH ALIGN=left>Chromosome:</TH><TD>%s</TD></TR>\n", seqName);
	printf("<TR><TH ALIGN=left>Start:</TH><TD>%d</TD></TR>\n",start+1);
	printf("<TR><TH ALIGN=left>End:</TH><TD>%d</TD></TR>\n",end);
	printf("</TABLE>\n");
        htmlHorizontalLine();
        printf("<TABLE>\n");
        printf("<TR><TH ALIGN=left>UCSC STS Marker ID:</TH><TD>%d</TD></TR>\n", infoRow->identNo);
        if (infoRow->UiStsId != 0)
            printf("<TR><TH ALIGN=left>UniSts Marker ID:</TH><TD>"
                   "<A HREF=\"https://www.ncbi.nlm.nih.gov/genome/sts/sts.cgi?uid=%d\" "
                   "TARGET=_BLANK>%d</A></TD></TR>\n", infoRow->UiStsId, infoRow->UiStsId);
        if (infoRow->MGIId != 0)
            printf("<TR><TH ALIGN=left>MGI Marker ID:</TH><TD><B>"
                   "<A HREF=\"http://www.informatics.jax.org/searches/marker_report.cgi?"
                   "accID=MGI%c3A%d\" TARGET=_BLANK>%d</A></TD></TR>\n",
                   sChar,infoRow->MGIId,infoRow->MGIId );
        if (strcmp(infoRow->MGIName, ""))
            printf("<TR><TH ALIGN=left>MGI Marker Name:</TH><TD>%s</TD></TR>\n", infoRow->MGIName);
        printf("</TABLE>\n");
        htmlHorizontalLine();
	/* Print out primer information */
	printf("<TABLE>\n");
	printf("<TR><TH ALIGN=left>Left Primer:</TH><TD>%s</TD></TR>\n",infoRow->primer1);
	printf("<TR><TH ALIGN=left>Right Primer:</TH><TD>%s</TD></TR>\n",infoRow->primer2);
	printf("<TR><TH ALIGN=left>Distance:</TH><TD>%s bps</TD></TR>\n",infoRow->distance);
	printf("</TABLE>\n");
	htmlHorizontalLine();
	/* Print out information from genetic maps for this marker */
	if(strcmp(infoRow->wigName, "") || strcmp(infoRow->mgiName, "") || strcmp(infoRow->rhName, ""))
	    printf("<H3>Map Position</H3>\n<TABLE>\n");
	if(strcmp(infoRow->wigName, ""))
            {
            printf("<TR><TH>&nbsp</TH><TH ALIGN=left WIDTH=150>Name</TH><TH ALIGN=left WIDTH=150>Chromosome</TH><TH ALIGN=left WIDTH=150>Position</TH></TR>\n");
            printf("<TR><TH ALIGN=left>&nbsp</TH><TD WIDTH=150>%s</TD><TD WIDTH=150>%s</TD><TD WIDTH=150>%.2f</TD></TR>\n",
                   infoRow->wigName, infoRow->wigChr, infoRow->wigGeneticPos);
            }
        if (strcmp(infoRow->mgiName, ""))
            {
            printf("<TR><TH>&nbsp</TH><TH ALIGN=left WIDTH=150>Name</TH><TH ALIGN=left WIDTH=150>Chromosome</TH><TH ALIGN=left WIDTH=150>Position</TH></TR>\n");
            printf("<TR><TH ALIGN=left>&nbsp</TH><TD WIDTH=150>%s</TD><TD WIDTH=150>%s</TD><TD WIDTH=150>%.2f</TD></TR>\n",
                   infoRow->mgiName, infoRow->mgiChr, infoRow->mgiGeneticPos);
            }
        if (strcmp(infoRow->rhName, ""))
            {
            printf("<TR><TH>&nbsp</TH><TH ALIGN=left WIDTH=150>Name</TH><TH ALIGN=left WIDTH=150>Chromosome</TH><TH ALIGN=left WIDTH=150>Position</TH><TH ALIGN=left WIDTH=150>Score</TH?</TR>\n");
            printf("<TR><TH ALIGN=left>&nbsp</TH><TD WIDTH=150>%s</TD><TD WIDTH=150>%s</TD><TD WIDTH=150>%.2f</TD><TD WIDTH=150>%.2f</TD></TR>\n",
                   infoRow->rhName, infoRow->rhChr, infoRow->rhGeneticPos, infoRow->RHLOD);
            }
        printf("</TABLE><P>\n");

        /* Print out alignment information - full sequence */
        webNewSection("Genomic Alignments:");
        safef(stsid, sizeof stsid, "%d", infoRow->identNo);
	safef(stsPrimer, sizeof stsPrimer, "%d_%s", infoRow->identNo, infoRow->name);
        safef(stsClone, sizeof stsClone, "%d_%s_clone", infoRow->identNo, infoRow->name);

        /* find sts in primer alignment info */
        sqlSafef(query, sizeof query, "SELECT * FROM all_sts_primer WHERE  qName = '%s' AND  tStart = '%d' "
                "AND tEnd = '%d'",stsPrimer, start, end);
        sr1 = sqlGetResult(conn1, query);
        i = 0;
        pslStart = 0;
        while ((row = sqlNextRow(sr1)) != NULL )
            {
            psl = pslLoad(row);
            fflush(stdout);
            if ((sameString(psl->tName, seqName)) && (abs(psl->tStart - start) < 1000))
		pslStart = psl->tStart;
	    slAddHead(&pslList, psl);
	    i++;
	  }
	slReverse(&pslList);
        if (i > 0)
	  {
	    printf("<H3>Primers:</H3>\n");
	    printAlignments(pslList, pslStart, "htcCdnaAli", "all_sts_primer", stsPrimer);
	    sqlFreeResult(&sr1);
	  }
	slFreeList(&pslList);
	stsInfoMouseNewFree(&infoRow);

	/* Find sts in clone sequece alignment info */
        sqlSafef(query1, sizeof query1, "SELECT * FROM all_sts_primer WHERE  qName = '%s' AND  tStart = '%d' AND tEnd = '%d'",stsClone, start, end);
	sr2 = sqlGetResult(conn1, query1);
        i = 0;
        pslStart = 0;
        while ((row = sqlNextRow(sr2)) != NULL )
            {
            psl = pslLoad(row);
            fflush(stdout);
            if ((sameString(psl->tName, seqName)) && (abs(psl->tStart - start) < 1000))
		pslStart = psl->tStart;
	    slAddHead(&pslList, psl);
	    i++;
	  }
	slReverse(&pslList);
        if (i > 0)
	  {
	    printf("<H3>Clone:</H3>\n");
	    printAlignments(pslList, pslStart, "htcCdnaAli", "all_sts_primer", stsClone);
	    sqlFreeResult(&sr1);
	  }
	slFreeList(&pslList);
	stsInfoMouseNewFree(&infoRow);
	}

	htmlHorizontalLine();

	if (stsRow.score == 1000)
	    printf("<H3>This is the only location found for %s</H3>\n",marker);
        else
	    {
            sqlFreeResult(&sr);
            printf("<H4>Other locations found for %s in the genome:</H4>\n", marker);
            printf("<TABLE>\n");
            sqlSafef(query, sizeof query, "SELECT * FROM %s WHERE name = '%s' "
                           "AND (chrom != '%s' OR chromStart != %d OR chromEnd != %d)",
                           table, marker, seqName, start, end);
            sr = sqlGetResult(conn,query);
            while ((row = sqlNextRow(sr)) != NULL)
                {
                stsMapMouseNewStaticLoad(row, &stsRow);
                printf("<TR><TD>%s:</TD><TD><A HREF = \"../cgi-bin/hgc?hgsid=%s&o=%u&t=%d&"
                       "g=stsMapMouseNew&i=%s&c=%s\" target=_blank>%d</A></TD></TR>\n",
                       stsRow.chrom, hgsid, stsRow.chromStart,stsRow.chromEnd, stsRow.name,
                       stsRow.chrom,(stsRow.chromStart+stsRow.chromEnd)>>1);
		}
	    printf("</TABLE>\n");
	    }
    }
webNewSection("Notes:");
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
hFreeConn(&conn1);
}


void doStsMapRat(struct trackDb *tdb, char *marker)
/* Respond to click on an STS marker. */
{
char *table = tdb->table;
char title[256];
char query[256];
char query1[256];
struct sqlConnection *conn = hAllocConn(database);
struct sqlConnection *conn1 = hAllocConn(database);
struct sqlResult *sr = NULL, *sr1 = NULL, *sr2 = NULL;
char **row;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
char *hgsid = cartSessionId(cart);
struct stsMapRat stsRow;
struct stsInfoRat *infoRow;
char stsid[20];
char stsPrimer[40];
char stsClone[45];
int i;
struct psl *pslList = NULL, *psl;
int pslStart;
boolean hasBin = FALSE;

/* Print out non-sequence info */
safef(title, sizeof title, "STS Marker %s", marker);
cartWebStart(cart, database, "%s", title);

/* Find the instance of the object in the bed table */
sqlSafef(query, sizeof(query), "name = '%s'", marker);
sr = hRangeQuery(conn, table, seqName, start, end, query, &hasBin);
row = sqlNextRow(sr);
if (row != NULL)
    {
    stsMapRatStaticLoad(row+hasBin, &stsRow);
    /* Find the instance of the object in the stsInfo table */
    sqlFreeResult(&sr);
    sqlSafef(query, sizeof query, "SELECT * FROM stsInfoRat WHERE identNo = '%d'", stsRow.identNo);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
	{
	infoRow = stsInfoRatLoad(row);
	printf("<TABLE>\n");
	printf("<TR><TH ALIGN=left>Chromosome:</TH><TD>%s</TD></TR>\n", seqName);
	printf("<TR><TH ALIGN=left>Start:</TH><TD>%d</TD></TR>\n",start+1);
	printf("<TR><TH ALIGN=left>End:</TH><TD>%d</TD></TR>\n",end);
	printf("</TABLE>\n");
        htmlHorizontalLine();
        printf("<TABLE>\n");
        printf("<TR><TH ALIGN=left>UCSC STS Marker ID:</TH><TD>%d</TD></TR>\n", infoRow->identNo);
        if (infoRow->UiStsId != 0)
            printf("<TR><TH ALIGN=left>UniSts Marker ID:</TH><TD>"
                   "<A HREF=\"https://www.ncbi.nlm.nih.gov/genome/sts/sts.cgi?uid=%d\" "
                   "TARGET=_BLANK>%d</A></TD></TR>\n", infoRow->UiStsId, infoRow->UiStsId);
        if (infoRow->RGDId != 0)
            printf("<TR><TH ALIGN=left>RGD Marker ID:</TH><TD><B>"
                   "<A HREF=\"http://rgd.mcw.edu/tools/query/query.cgi?id=%d\" "
                   "TARGET=_BLANK>%d</A></TD></TR>\n", infoRow->RGDId,infoRow->RGDId );
        if (strcmp(infoRow->RGDName, ""))
            printf("<TR><TH ALIGN=left>RGD Marker Name:</TH><TD>%s</TD></TR>\n", infoRow->RGDName);
        printf("</TABLE>\n");
        htmlHorizontalLine();
	/* Print out primer information */
	printf("<TABLE>\n");
	printf("<TR><TH ALIGN=left>Left Primer:</TH><TD>%s</TD></TR>\n",infoRow->primer1);
	printf("<TR><TH ALIGN=left>Right Primer:</TH><TD>%s</TD></TR>\n",infoRow->primer2);
	printf("<TR><TH ALIGN=left>Distance:</TH><TD>%s bps</TD></TR>\n",infoRow->distance);
	printf("</TABLE>\n");
	htmlHorizontalLine();
	/* Print out information from genetic maps for this marker */
	if(strcmp(infoRow->fhhName, "") || strcmp(infoRow->shrspName, "") || strcmp(infoRow->rhName, ""))
	    printf("<H3>Map Position</H3>\n<TABLE>\n");
	if(strcmp(infoRow->fhhName, ""))
	    {
	    printf("<TR><TH>&nbsp</TH><TH ALIGN=left WIDTH=150>Name</TH><TH ALIGN=left WIDTH=150>Chromosome</TH><TH ALIGN=left WIDTH=150>Position</TH></TR>\n");
	    printf("<TR><TH ALIGN=left>&nbsp</TH><TD WIDTH=150>%s</TD><TD WIDTH=150>%s</TD><TD WIDTH=150>%.2f</TD></TR>\n",
                   infoRow->fhhName, infoRow->fhhChr, infoRow->fhhGeneticPos);
	    }
	if(strcmp(infoRow->shrspName, ""))
	    {
	    printf("<TR><TH>&nbsp</TH><TH ALIGN=left WIDTH=150>Name</TH><TH ALIGN=left WIDTH=150>Chromosome</TH><TH ALIGN=left WIDTH=150>Position</TH></TR>\n");
	    printf("<TR><TH ALIGN=left>&nbsp</TH><TD WIDTH=150>%s</TD><TD WIDTH=150>%s</TD><TD WIDTH=150>%.2f</TD></TR>\n",
                   infoRow->shrspName, infoRow->shrspChr, infoRow->shrspGeneticPos);
	    }
	if(strcmp(infoRow->rhName, ""))
	    {
	    printf("<TR><TH>&nbsp</TH><TH ALIGN=left WIDTH=150>Name</TH><TH ALIGN=left WIDTH=150>Chromosome</TH><TH ALIGN=left WIDTH=150>Position</TH><TH ALIGN=left WIDTH=150>Score</TH?</TR>\n");
	    printf("<TR><TH ALIGN=left>&nbsp</TH><TD WIDTH=150>%s</TD><TD WIDTH=150>%s</TD><TD WIDTH=150>%.2f</TD><TD WIDTH=150>%.2f</TD></TR>\n",
                   infoRow->rhName, infoRow->rhChr, infoRow->rhGeneticPos, infoRow->RHLOD);
	    }
	printf("</TABLE><P>\n");

	/* Print out alignment information - full sequence */
	webNewSection("Genomic Alignments:");
	safef(stsid, sizeof stsid, "%d", infoRow->identNo);
	safef(stsPrimer, sizeof stsPrimer, "%d_%s", infoRow->identNo, infoRow->name);
	safef(stsClone, sizeof stsClone, "%d_%s_clone", infoRow->identNo, infoRow->name);

	/* find sts in primer alignment info */
        sqlSafef(query, sizeof(query), "qName = '%s'", stsPrimer);
	sr1 = hRangeQuery(conn1, "all_sts_primer", seqName, start, end, query,
			  &hasBin);
	i = 0;
	pslStart = 0;
	while ((row = sqlNextRow(sr1)) != NULL )
            {
	    psl = pslLoad(row+hasBin);
	    fflush(stdout);
	    if ((sameString(psl->tName, seqName)) && (abs(psl->tStart - start) < 1000))
		pslStart = psl->tStart;
	    slAddHead(&pslList, psl);
	    i++;
	    }
	slReverse(&pslList);
        if (i > 0)
	    {
	    printf("<H3>Primers:</H3>\n");
	    printAlignments(pslList, pslStart, "htcCdnaAli", "all_sts_primer", stsPrimer);
	    sqlFreeResult(&sr1);
	    }
	slFreeList(&pslList);
	stsInfoRatFree(&infoRow);

	/* Find sts in clone sequece alignment info */
        sqlSafef(query1, sizeof(query1), "qName = '%s'", stsClone);
	sr2 = hRangeQuery(conn1, "all_sts_primer", seqName, start, end, query1,
			  &hasBin);
	i = 0;
	pslStart = 0;
	while ((row = sqlNextRow(sr2)) != NULL )
            {
	    psl = pslLoad(row+hasBin);
	    fflush(stdout);
	    if ((sameString(psl->tName, seqName)) && (abs(psl->tStart - start) < 1000))
		pslStart = psl->tStart;
	    slAddHead(&pslList, psl);
	    i++;
	    }
	slReverse(&pslList);
        if (i > 0)
	    {
	    printf("<H3>Clone:</H3>\n");
	    printAlignments(pslList, pslStart, "htcCdnaAli", "all_sts_primer", stsClone);
	    sqlFreeResult(&sr1);
	    }
	slFreeList(&pslList);
	stsInfoRatFree(&infoRow);
	}

    htmlHorizontalLine();

    if (stsRow.score == 1000)
	printf("<H3>This is the only location found for %s</H3>\n",marker);
    else
	{
	sqlFreeResult(&sr);
	printf("<H4>Other locations found for %s in the genome:</H4>\n", marker);
	printf("<TABLE>\n");
	sqlSafef(query, sizeof(query), "name = '%s'", marker);
	sr = hRangeQuery(conn, table, seqName, start, end, query, &hasBin);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    stsMapRatStaticLoad(row+hasBin, &stsRow);
	    printf("<TR><TD>%s:</TD><TD><A HREF = \"../cgi-bin/hgc?hgsid=%s&o=%u&t=%d&g=stsMapRat&i=%s&c=%s\" target=_blank>%d</A></TD></TR>\n",
		   stsRow.chrom, hgsid, stsRow.chromStart,stsRow.chromEnd, stsRow.name, stsRow.chrom,(stsRow.chromStart+stsRow.chromEnd)>>1);
	    }
	printf("</TABLE>\n");
	}
    }
webNewSection("Notes:");
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
hFreeConn(&conn1);
}

void doFishClones(struct trackDb *tdb, char *clone)
/* Handle click on the FISH clones track */
{
char query[256];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
struct fishClones *fc;
int i;

/* Print out non-sequence info */
cartWebStart(cart, database, "%s", clone);


/* Find the instance of the object in the bed table */
sqlSafef(query, sizeof query, "SELECT * FROM fishClones WHERE name = '%s' "
               "AND chrom = '%s' AND chromStart = %d "
                "AND chromEnd = %d",
        clone, seqName, start, end);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    fc = fishClonesLoad(row);
    /* Print out general sequence positional information */
    printf("<H2><A HREF=");
    printCloneDbUrl(stdout, clone);
    printf(" TARGET=_BLANK>%s</A></H2>\n", clone);
    htmlHorizontalLine();
    printf("<TABLE>\n");
    printf("<TR><TH ALIGN=left>Chromosome:</TH><TD>%s</TD></TR>\n", seqName);
    printf("<TR><TH ALIGN=left>Start:</TH><TD>%d</TD></TR>\n",start+1);
    printf("<TR><TH ALIGN=left>End:</TH><TD>%d</TD></TR>\n",end);
    printBand(seqName, start, end, TRUE);
    printf("</TABLE>\n");
    htmlHorizontalLine();

    /* Print out information about the clone */
    printf("<H4>Placement of %s on draft sequence was determined using the location of %s</H4>\n",
	   clone, fc->placeType);
    printf("<TABLE>\n");
    if (fc->accCount > 0)
        {
	printf("<TR><TH>Genbank Accession:</TH>");
        for (i = 0; i < fc->accCount; i++)
	    {
	    printf("<TD><A HREF=\"");
	    printEntrezNucleotideUrl(stdout, fc->accNames[i]);
            printf("\" TARGET=_BLANK>%s</A></TD>", fc->accNames[i]);
	    }
	printf("</TR>\n");
	}
    if (fc->stsCount > 0)
        {
	printf("<TR><TH ALIGN=left>STS Markers within clone:</TH>");
        for (i = 0; i < fc->stsCount; i++)
	    {
	    printf("<TD>%s</TD>", fc->stsNames[i]);
	    }
	printf("</TR>\n");
        }
    if (fc->beCount > 0)
        {
	printf("<TR><TH ALIGN=left>BAC end sequence:</TH>");
        for (i = 0; i < fc->beCount; i++)
	    {
	    printf("<TD><A HREF=\"");
	    printEntrezNucleotideUrl(stdout, fc->beNames[i]);
	    printf("\" TARGET=_BLANK>%s</A></TD>", fc->beNames[i]);
	    }
	printf("</TR>\n");
        }
    printf("</TABLE>\n");

    /* Print out FISH placement information */
    webNewSection("FISH Placements");
    /*printf("<H3>Placements of %s by FISH</H3>\n", clone);*/
    printf("<TABLE>\n");
    printf("<TR><TH ALIGN=left WIDTH=100>Lab</TH><TH>Band Position</TH></TR>\n");
    for (i = 0; i < fc->placeCount; i++)
        {
	if (sameString(fc->bandStarts[i],fc->bandEnds[i]))
	    {
	    printf("<TR><TD WIDTH=100 ALIGN=left>%s</TD><TD ALIGN=center>%s</TD></TR>",
		   fc->labs[i], fc->bandStarts[i]);
	    }
	else
	    {
	    printf("<TR><TD WIDTH=100 ALIGN=left>%s</TD><TD ALIGN=center>%s - %s</TD></TR>",
		   fc->labs[i], fc->bandStarts[i], fc->bandEnds[i]);
	    }
	}

    }
printf("</TABLE>\n");
webNewSection("Notes:");
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doRecombRate(struct trackDb *tdb)
/* Handle click on the Recombination Rate track */
{
char query[256];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
struct recombRate *rr;

/* Print out non-sequence info */
cartWebStart(cart, database, "Recombination Rates");

/* Find the instance of the object in the bed table */
sqlSafef(query, sizeof query, "SELECT * FROM recombRate WHERE "
               "chrom = '%s' AND chromStart = %d "
               "AND chromEnd = %d",
        seqName, start, end);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    rr = recombRateLoad(row);
    /* Print out general sequence positional information */
    printf("<TABLE>\n");
    printf("<TR><TH ALIGN=left>Chromosome:</TH><TD>%s</TD></TR>\n", seqName);
    printf("<TR><TH ALIGN=left>Start:</TH><TD>%d</TD></TR>\n",start+1);
    printf("<TR><TH ALIGN=left>End:</TH><TD>%d</TD></TR>\n",end);
    printBand(seqName, start, end, TRUE);
    printf("<TR><TH ALIGN=left>deCODE Sex-Averaged Rate:</TH><TD>%3.1f cM/Mb</TD></TR>\n", rr->decodeAvg);
    printf("<TR><TH ALIGN=left>deCODE Female Rate:</TH><TD>%3.1f cM/Mb</TD></TR>\n", rr->decodeFemale);
    printf("<TR><TH ALIGN=left>deCODE Male Rate:</TH><TD>%3.1f cM/Mb</TD></TR>\n", rr->decodeMale);
    printf("<TR><TH ALIGN=left>Marshfield Sex-Averaged Rate:</TH><TD>%3.1f cM/Mb</TD></TR>\n", rr->marshfieldAvg);
    printf("<TR><TH ALIGN=left>Marshfield Female Rate:</TH><TD>%3.1f cM/Mb</TD></TR>\n", rr->marshfieldFemale);
    printf("<TR><TH ALIGN=left>Marshfield Male Rate:</TH><TD>%3.1f cM/Mb</TD></TR>\n", rr->marshfieldMale);
    printf("<TR><TH ALIGN=left>Genethon Sex-Averaged Rate:</TH><TD>%3.1f cM/Mb</TD></TR>\n", rr->genethonAvg);
    printf("<TR><TH ALIGN=left>Genethon Female Rate:</TH><TD>%3.1f cM/Mb</TD></TR>\n", rr->genethonFemale);
    printf("<TR><TH ALIGN=left>Genethon Male Rate:</TH><TD>%3.1f cM/Mb</TD></TR>\n", rr->genethonMale);
    printf("</TABLE>\n");
    freeMem(rr);
    }
webNewSection("Notes:");
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doRecombRateRat(struct trackDb *tdb)
/* Handle click on the rat Recombination Rate track */
{
char query[256];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
struct recombRateRat *rr;

/* Print out non-sequence info */
cartWebStart(cart, database, "Recombination Rates");


/* Find the instance of the object in the bed table */
sqlSafef(query, sizeof query, "SELECT * FROM recombRateRat WHERE "
               "chrom = '%s' AND chromStart = %d "
               "AND chromEnd = %d",
        seqName, start, end);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    rr = recombRateRatLoad(row);
    /* Print out general sequence positional information */
    printf("<TABLE>\n");
    printf("<TR><TH ALIGN=left>Chromosome:</TH><TD>%s</TD></TR>\n", seqName);
    printf("<TR><TH ALIGN=left>Start:</TH><TD>%d</TD></TR>\n",start+1);
    printf("<TR><TH ALIGN=left>End:</TH><TD>%d</TD></TR>\n",end);
    printBand(seqName, start, end, TRUE);
    printf("<TR><TH ALIGN=left>SHRSPxBN Sex-Averaged Rate:</TH><TD>%3.1f cM/Mb</TD></TR>\n", rr->shrspAvg);
    printf("<TR><TH ALIGN=left>FHHxACI Sex-Averaged Rate:</TH><TD>%3.1f cM/Mb</TD></TR>\n", rr->fhhAvg);
    printf("</TABLE>\n");
    freeMem(rr);
    }
webNewSection("Notes:");
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doRecombRateMouse(struct trackDb *tdb)
/* Handle click on the mouse Recombination Rate track */
{
char query[256];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
struct recombRateMouse *rr;

/* Print out non-sequence info */
cartWebStart(cart, database, "Recombination Rates");

/* Find the instance of the object in the bed table */
sqlSafef(query, sizeof query, "SELECT * FROM recombRateMouse WHERE "
               "chrom = '%s' AND chromStart = %d "
               "AND chromEnd = %d",
        seqName, start, end);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    rr = recombRateMouseLoad(row);
    /* Print out general sequence positional information */
    printf("<TABLE>\n");
    printf("<TR><TH ALIGN=left>Chromosome:</TH><TD>%s</TD></TR>\n", seqName);
    printf("<TR><TH ALIGN=left>Start:</TH><TD>%d</TD></TR>\n",start+1);
    printf("<TR><TH ALIGN=left>End:</TH><TD>%d</TD></TR>\n",end);
    printBand(seqName, start, end, TRUE);
    printf("<TR><TH ALIGN=left>WI Genetic Map Sex-Averaged Rate:</TH><TD>%3.1f cM/Mb</TD></TR>\n", rr->wiAvg);
    printf("<TR><TH ALIGN=left>MGD Genetic Map Sex-Averaged Rate:</TH><TD>%3.1f cM/Mb</TD></TR>\n", rr->mgdAvg);
    printf("</TABLE>\n");
    freeMem(rr);
    }
webNewSection("Notes:");
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doGenMapDb(struct trackDb *tdb, char *clone)
/* Handle click on the GenMapDb clones track */
{
char query[256];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
struct genMapDb *upc;
int size;

/* Print out non-sequence info */
cartWebStart(cart, database, "GenMapDB BAC Clones");

/* Find the instance of the object in the bed table */
sqlSafef(query, sizeof query, "SELECT * FROM genMapDb WHERE name = '%s' "
               "AND chrom = '%s' AND chromStart = %d "
               "AND chromEnd = %d",
        clone, seqName, start, end);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    upc = genMapDbLoad(row);
    /* Print out general sequence positional information */
    printf("<H2><A HREF=");
    printGenMapDbUrl(stdout, clone);
    printf(" TARGET=_BLANK>%s</A></H2>\n", clone);
    htmlHorizontalLine();
    printf("<TABLE>\n");
    printf("<TR><TH ALIGN=left>Chromosome:</TH><TD>%s</TD></TR>\n", seqName);
    printf("<TR><TH ALIGN=left>Start:</TH><TD>%d</TD></TR>\n",start+1);
    printf("<TR><TH ALIGN=left>End:</TH><TD>%d</TD></TR>\n",end);
    size = end - start + 1;
    printf("<TR><TH ALIGN=left>Size:</TH><TD>%d</TD></TR>\n",size);
    printBand(seqName, start, end, TRUE);
    printf("</TABLE>\n");
    htmlHorizontalLine();

    /* Print out information about the clone */
    printf("<H4>Placement of %s on draft sequence was determined using BAC end sequences and/or an STS marker</H4>\n",clone);
    printf("<TABLE>\n");
    if (upc->accT7)
	{
	printf("<TR><TH ALIGN=left>T7 end sequence:</TH>");
	printf("<TD><A HREF=\"");
	printEntrezNucleotideUrl(stdout, upc->accT7);
	printf("\" TARGET=_BLANK>%s</A></TD>", upc->accT7);
        printf("<TD>%s:</TD><TD ALIGN=right>%d</TD><TD ALIGN=LEFT> - %d</TD>",
	       seqName, upc->startT7, upc->endT7);
	printf("</TR>\n");
	}
    if (upc->accSP6)
	{
	printf("<TR><TH ALIGN=left>SP6 end sequence:</TH>");
	printf("<TD><A HREF=\"");
	printEntrezNucleotideUrl(stdout, upc->accSP6);
	printf("\" TARGET=_BLANK>%s</A></TD>", upc->accSP6);
        printf("<TD>%s:</TD><TD ALIGN=right>%d</TD><TD ALIGN=LEFT> - %d</TD>",
	       seqName, upc->startSP6, upc->endSP6);
	printf("</TR>\n");
	}
    if (upc->stsMarker)
	{
	printf("<TR><TH ALIGN=left>STS Marker:</TH>");
	printf("<TD><A HREF=\"");
	printEntrezUniSTSUrl(stdout, upc->stsMarker);
	printf("\" TARGET=_BLANK>%s</A></TD>", upc->stsMarker);
        printf("<TD>%s:</TD><TD ALIGN=right>%d</TD><TD ALIGN=LEFT> - %d</TD>",
	       seqName, upc->stsStart, upc->stsEnd);
	printf("</TR>\n");
	}
    printf("</TABLE>\n");
    }
webNewSection("Notes:");
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doMouseOrthoDetail(struct trackDb *tdb, char *itemName)
/* Handle click on mouse synteny track. */
{
struct mouseSyn el;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset;

cartWebStart(cart, database, "Mouse Synteny");
printf("<H2>Mouse Synteny</H2>\n");

sqlSafef(query, sizeof query, "select * from %s where chrom = '%s' and chromStart = %d",
	tdb->table, seqName, start);
rowOffset = hOffsetPastBin(database, seqName, tdb->table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    htmlHorizontalLine();
    mouseSynStaticLoad(row+rowOffset, &el);
    printf("<B>mouse chromosome:</B> %s<BR>\n", el.name+6);
    printf("<B>human chromosome:</B> %s<BR>\n", skipChr(el.chrom));
    printf("<B>human starting base:</B> %d<BR>\n", el.chromStart);
    printf("<B>human ending base:</B> %d<BR>\n", el.chromEnd);
    printf("<B>size:</B> %d<BR>\n", el.chromEnd - el.chromStart);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
printTrackHtml(tdb);
}

void doMouseSyn(struct trackDb *tdb, char *itemName)
/* Handle click on mouse synteny track. */
{
struct mouseSyn el;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset;

cartWebStart(cart, database, "Mouse Synteny");
printf("<H2>Mouse Synteny</H2>\n");

sqlSafef(query, sizeof query, "select * from %s where chrom = '%s' and chromStart = %d",
	tdb->table, seqName, start);
rowOffset = hOffsetPastBin(database, seqName, tdb->table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    htmlHorizontalLine();
    mouseSynStaticLoad(row+rowOffset, &el);
    printf("<B>mouse chromosome:</B> %s<BR>\n", el.name+6);
    printf("<B>human chromosome:</B> %s<BR>\n", skipChr(el.chrom));
    printf("<B>human starting base:</B> %d<BR>\n", el.chromStart);
    printf("<B>human ending base:</B> %d<BR>\n", el.chromEnd);
    printf("<B>size:</B> %d<BR>\n", el.chromEnd - el.chromStart);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
printTrackHtml(tdb);
}

void doMouseSynWhd(struct trackDb *tdb, char *itemName)
/* Handle click on Whitehead mouse synteny track. */
{
struct mouseSynWhd el;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset;

cartWebStart(cart, database, "Mouse Synteny (Whitehead)");
printf("<H2>Mouse Synteny (Whitehead)</H2>\n");

sqlSafef(query, sizeof query, "select * from %s where chrom = '%s' and chromStart = %d",
	tdb->table, seqName, start);
rowOffset = hOffsetPastBin(database, seqName, tdb->table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    htmlHorizontalLine();
    mouseSynWhdStaticLoad(row+rowOffset, &el);
    printf("<B>mouse chromosome:</B> %s<BR>\n", el.name);
    printf("<B>mouse starting base:</B> %d<BR>\n", el.mouseStart+1);
    printf("<B>mouse ending base:</B> %d<BR>\n", el.mouseEnd);
    printf("<B>human chromosome:</B> %s<BR>\n", skipChr(el.chrom));
    printf("<B>human starting base:</B> %d<BR>\n", el.chromStart+1);
    printf("<B>human ending base:</B> %d<BR>\n", el.chromEnd);
    printf("<B>strand:</B> %s<BR>\n", el.strand);
    printf("<B>segment label:</B> %s<BR>\n", el.segLabel);
    printf("<B>size:</B> %d (mouse), %d (human)<BR>\n",
	   (el.mouseEnd - el.mouseStart), (el.chromEnd - el.chromStart));
    }
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doEnsPhusionBlast(struct trackDb *tdb, char *itemName)
/* Handle click on Ensembl Phusion Blast synteny track. */
{
struct ensPhusionBlast el;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char *org = hOrganism(database);
char *tbl = cgiUsualString("table", cgiString("g"));
char *elname, *ptr, *xenoDb, *xenoOrg, *xenoChrom;
char query[256];
int rowOffset;

cartWebStart(cart, database, "%s", tdb->longLabel);
printf("<H2>%s</H2>\n", tdb->longLabel);

sqlSafef(query, sizeof query, "select * from %s where chrom = '%s' and chromStart = %d",
	tdb->table, seqName, start);
rowOffset = hOffsetPastBin(database, seqName, tdb->table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    htmlHorizontalLine();
    ensPhusionBlastStaticLoad(row+rowOffset, &el);
    elname = cloneString(el.name);
    if ((ptr = strchr(elname, '.')) != NULL)
	{
	*ptr = 0;
	xenoChrom = ptr+1;
	xenoDb = elname;
	xenoOrg = hOrganism(xenoDb);
	}
    else
	{
	xenoChrom = elname;
	xenoDb = NULL;
	xenoOrg = "Other Organism";
	}
    printf("<B>%s chromosome:</B> %s<BR>\n", xenoOrg, xenoChrom);
    printf("<B>%s starting base:</B> %d<BR>\n", xenoOrg, el.xenoStart+1);
    printf("<B>%s ending base:</B> %d<BR>\n", xenoOrg, el.xenoEnd);
    printf("<B>%s chromosome:</B> %s<BR>\n", org, skipChr(el.chrom));
    printf("<B>%s starting base:</B> %d<BR>\n", org, el.chromStart+1);
    printf("<B>%s ending base:</B> %d<BR>\n", org, el.chromEnd);
    printf("<B>score:</B> %d<BR>\n", el.score);
    printf("<B>strand:</B> %s<BR>\n", el.strand);
    printf("<B>size:</B> %d (%s), %d (%s)<BR>\n",
	   (el.xenoEnd - el.xenoStart), xenoOrg,
	   (el.chromEnd - el.chromStart), org);
    if (xenoDb != NULL)
	{
	printf("<A HREF=\"%s?db=%s&position=%s:%d-%d\" TARGET=_BLANK>%s Genome Browser</A> at %s:%d-%d <BR>\n",
               hgTracksName(),
	       xenoDb, xenoChrom, el.xenoStart, el.xenoEnd,
	       xenoOrg, xenoChrom, el.xenoStart, el.xenoEnd);

	}
    printf("<A HREF=\"%s&o=%d&g=getDna&i=%s&c=%s&l=%d&r=%d&strand=%s&table=%s\">"
	   "View DNA for this feature</A><BR>\n",  hgcPathAndSettings(),
	   el.chromStart, cgiEncode(el.name),
	   el.chrom, el.chromStart, el.chromEnd, el.strand, tbl);
    freez(&elname);
    }
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void printDbSnpRsUrl(char *rsId, char *labelFormat, ...)
/* Print a link to dbSNP's report page for an rs[0-9]+ ID. */
{
char dbSnpUrl[2048];
safef (dbSnpUrl, sizeof(dbSnpUrl), dbSnpFormat, rsId);
printf ("<a href=\"%s\" target=\"_blank\">", dbSnpUrl);

va_list args;
va_start(args, labelFormat);
vprintf(labelFormat, args);
va_end(args);
printf("</a>");
}

char *validateOrGetRsId(char *name, struct sqlConnection *conn)
/* If necessary, get the rsId from the affy120K or affy10K table,
   given the affyId.  rsId is more common, affy120K is next, affy10K least.
 * returns "valid" if name is already a valid rsId,
           new rsId if it is found in the affy tables, or
           0 if no valid rsId is found */
{
char  *rsId = cloneString(name);
struct affy120KDetails *a120K = NULL;
struct affy10KDetails *a10K = NULL;
char   query[512];

if (strncmp(rsId,"rs",2)) /* is not a valid rsId, so it must be an affyId */
    {
    sqlSafef(query, sizeof(query), /* more likely to be affy120K, so check first */
	  "select * from affy120KDetails where affyId = '%s'", name);
    a120K = affy120KDetailsLoadByQuery(conn, query);
    if (a120K != NULL) /* found affy120K record */
	rsId = cloneString(a120K->rsId);
    affy120KDetailsFree(&a120K);
    if (strncmp(rsId,"rs",2)) /* not a valid affy120K snp, might be affy10K */
	{
        sqlSafef(query, sizeof(query),
	      "select * from affy10KDetails where affyId = '%s'", name);
	a10K = affy10KDetailsLoadByQuery(conn, query);
	if (a10K != NULL) /* found affy10K record */
	    rsId = cloneString(a10K->rsId);
	affy10KDetailsFree(&a10K);
	if (strncmp(rsId,"rs",2)) /* not valid affy10K snp */
	    return 0;
	}
    /* not all affy snps have valid rsIds, so return if it is invalid */
    if (strncmp(rsId,"rs",2) || strlen(rsId)<4 || sameString(rsId,"rs0")) /* not a valid rsId */
	return 0;
    }
else
    rsId = cloneString("valid");
return rsId;
}

char *doDbSnpRs(char *name)
/* print additional SNP details
 * returns "valid" if name is already a valid rsId,
           new rsId if it is found in the affy tables, or
           0 if no valid rsId is found */
{
struct sqlConnection *hgFixed = sqlConnect("hgFixed");
char  *rsId = validateOrGetRsId(name, hgFixed);
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char   query[512];
struct dbSnpRs *snp = NULL;
char  *dbOrg = cloneStringZ(database,2);

toUpperN(dbOrg,1); /* capitalize first letter */
if (rsId) /* a valid rsId exists */
    {
    if (sameString(rsId, "valid"))
	sqlSafef(query, sizeof(query),
	      "select * "
	      "from   dbSnpRs%s "
	      "where  rsId = '%s'", dbOrg, name);
    else
	sqlSafef(query, sizeof(query),
	      "select * "
	      "from   dbSnpRs%s "
	      "where  rsId = '%s'", dbOrg, rsId);
    snp = dbSnpRsLoadByQuery(hgFixed, query);
    if (snp != NULL)
	{
	printf("<BR>\n");
	if(snp->avHetSE>0)
	    {
	    printf("<B><A HREF=\"https://www.ncbi.nlm.nih.gov/SNP/Hetfreq.html\" target=\"_blank\">");
	    printf("Average Heterozygosity</A>:</B> %f<BR>\n",snp->avHet);
	    printf("<B><A HREF=\"https://www.ncbi.nlm.nih.gov/SNP/Hetfreq.html\" target=\"_blank\">");
	    printf("Standard Error of Avg. Het.</A>: </B> %f<BR>\n", snp->avHetSE);
	    }
	else
	    {
	    printf("<B><A HREF=\"https://www.ncbi.nlm.nih.gov/SNP/Hetfreq.html\" target=\"_blank\">");
	    printf("Average Heterozygosity</A>:</B> Not Known<BR>\n");
	    printf("<B><A HREF=\"https://www.ncbi.nlm.nih.gov/SNP/Hetfreq.html\" target=\"_blank\">");
	    printf("Standard Error of Avg. Het.</A>: </B> Not Known<BR>\n");
            }
//      printf("<B><A HREF=\"https://www.ncbi.nlm.nih.gov/SNP/snp_legend.cgi?legend=snpFxnColor\" "
//             "target=\"_blank\">");
//      printf("Functional Status</A>:</B> <span style='font-family:Courier;'>%s<BR></span>\n",
//             snp->func);
        printf("<B>Functional Status:</B> <span style='font-family:Courier;'>%s<BR></span>\n",
               snp->func);
        printf("<B><A HREF=\"https://www.ncbi.nlm.nih.gov/SNP/snp_legend.cgi?legend=validation\" "
               "target=\"_blank\">");
        printf("Validation Status</A>:</B> <span style='font-family:Courier;'>%s<BR></span>\n",
               snp->valid);
//      printf("<B>Validation Status:</B> <span style='font-family:Courier;'>%s<BR></span>\n",
//             snp->valid);
        printf("<B>Allele1:          </B> <span style='font-family:Courier;'>%s<BR></span>\n",
               snp->allele1);
        printf("<B>Allele2:          </B> <span style='font-family:Courier;'>%s<BR>\n",
               snp->allele2);
        printf("<B>Sequence in Assembly</B>:&nbsp;%s<BR>\n", snp->assembly);
        printf("<B>Alternate Sequence</B>:&nbsp;&nbsp;&nbsp;%s<BR></span>\n", snp->alternate);
        }
    dbSnpRsFree(&snp);
    }
sqlDisconnect(&hgFixed);
if (sameString(dbOrg,"Hg"))
    {
    sqlSafef(query, sizeof(query),
	  "select source, type from snpMap where  name = '%s'", name);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	printf("<B><A HREF=\"#source\">Variant Source</A></B>: &nbsp;%s<BR>\n",row[0]);
	printf("<B><A HREF=\"#type\">Variant Type</A></B>: &nbsp;%s\n",row[1]);
	}
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }
return rsId;
}

void doSnpEntrezGeneLink(struct trackDb *tdb, char *name)
/* print link to EntrezGene for this SNP */
{
struct sqlConnection *conn = hAllocConn(database);
char *table = tdb->table;
if (hTableExists(database, "knownGene") && sqlTableExists(conn, refLinkTable) &&
    hTableExists(database, "mrnaRefseq") && hTableExists(database, table))
    {
    struct sqlResult *sr;
    char **row;
    char query[512];

    sqlSafef(query, sizeof(query),
	  "select distinct        "
	  "       rl.locusLinkID, "
	  "       rl.name         "
	  "from   knownGene  kg,  "
	  "       %s         rl,  "
	  "       %s         snp, "
	  "       mrnaRefseq mrs  "
	  "where  snp.chrom  = kg.chrom       "
	  "  and  kg.name    = mrs.mrna       "
	  "  and  mrs.refSeq = rl.mrnaAcc     "
	  "  and  kg.txStart < snp.chromStart "
	  "  and  kg.txEnd   > snp.chromEnd   "
	  "  and  snp.name   = '%s'",refLinkTable, table, name);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	printf("<BR><A HREF=\"https://www.ncbi.nlm.nih.gov/SNP/snp_ref.cgi?");
	printf("geneId=%s\" TARGET=_blank>Entrez Gene for ", row[0]);
	printf("%s</A><BR>\n", row[1]);
	}
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
}

void doSnpOld(struct trackDb *tdb, char *itemName)
/* Put up info on a SNP. */
{
char *snpTable = tdb->table;
struct snp snp;
struct snpMap snpMap;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset;
char *printId;

cartWebStart(cart, database, "Simple Nucleotide Polymorphism (SNP)");
printf("<H2>Simple Nucleotide Polymorphism (SNP) %s</H2>\n", itemName);
sqlSafef(query, sizeof query,
	"select * "
	"from   %s "
	"where  chrom = '%s' "
	"  and  chromStart = %d "
	"  and  name = '%s'",
        snpTable, seqName, start, itemName);
rowOffset = hOffsetPastBin(database, seqName, snpTable);
sr = sqlGetResult(conn, query);
if (sameString(snpTable,"snpMap"))
    while ((row = sqlNextRow(sr)) != NULL)
	{
	snpMapStaticLoad(row+rowOffset, &snpMap);
	bedPrintPos((struct bed *)&snpMap, 3, tdb);
	}
else
    while ((row = sqlNextRow(sr)) != NULL)
	{
	snpStaticLoad(row+rowOffset, &snp);
	bedPrintPos((struct bed *)&snp, 3, tdb);
	}
/* write dbSnpRs details if found. */
printId = doDbSnpRs(itemName);
if (printId)
    {
    puts("<BR>");
    if (sameString(printId, "valid"))
        {
	printDbSnpRsUrl(itemName, "dbSNP link");
	putchar('\n');
	doSnpEntrezGeneLink(tdb, itemName);
	}
    else
	{
	printDbSnpRsUrl(printId, "dbSNP link (%s)", printId);
	putchar('\n');
	doSnpEntrezGeneLink(tdb, printId);
	}
    }
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void writeSnpException(char *exceptionList, char *itemName, int rowOffset,
                       char *chrom, int chromStart, struct trackDb *tdb)
{
char    *tokens;
struct   lineFile      *lf;
struct   tokenizer     *tkz;
struct   snpExceptions  se;
struct   sqlConnection *conn = hAllocConn(database);
struct   sqlResult     *sr;
char   **row;
char     query[256];
char    *id;
char    *br=" ";
char    *noteColor="#7f0000";
boolean  firstException=TRUE;
boolean  multiplePositions=FALSE;

if (sameString(exceptionList,"0"))
    return;
tokens=cloneString(exceptionList);
lf=lineFileOnString("snpExceptions", TRUE, tokens);
tkz=tokenizerOnLineFile(lf);
while ((id=tokenizerNext(tkz))!=NULL)
    {
    if (firstException)
	{
        printf("<BR><B style='color:%s;'>Note(s):</B><BR>\n",noteColor);
	firstException=FALSE;
	}
    if (sameString(id,",")) /* is there a tokenizer that doesn't return separators? */
	continue;
    if (sameString(id,"18")||sameString(id,"19")||sameString(id,"20"))
	multiplePositions=TRUE;
    br=cloneString("<BR>");
    sqlSafef(query, sizeof(query), "select * from snpExceptions where exceptionId = %s", id);
    sr = sqlGetResult(conn, query);
     /* exceptionId is a primary key; at most 1 record returned */
    while ((row = sqlNextRow(sr))!=NULL)
	{
	snpExceptionsStaticLoad(row, &se);
        printf("&nbsp;&nbsp;&nbsp;<B style='color:%s;'>%s</B><BR>\n",
	       noteColor,se.description);
	}
    }
printf("%s\n",br);
if (multiplePositions)
    {
    struct snp snp;
    printf("<B style='color:#7f0000;'>Other Positions</B>:<BR><BR>");
    sqlSafef(query, sizeof(query), "select * from snp where name='%s'", itemName);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr))!=NULL)
	{
	snpStaticLoad(row+rowOffset, &snp);
	if (differentString(chrom,snp.chrom) || chromStart!=snp.chromStart)
	    {
	    bedPrintPos((struct bed *)&snp, 3, tdb);
	    printf("<BR>\n");
	    }
	}
    }
}

void printSnpInfo(struct snp snp)
/* print info on a snp */
{
if (differentString(snp.strand,"?")) {printf("<B>Strand: </B>%s\n", snp.strand);}
printf("<BR><B>Observed: </B>%s\n",                                 snp.observed);
printf("<BR><B><A HREF=\"#Source\">Source</A>: </B>%s\n",           snp.source);
printf("<BR><B><A HREF=\"#MolType\">Molecule Type</A>: </B>%s\n",   snp.molType);
printf("<BR><B><A HREF=\"#Class\">Variant Class</A>: </B>%s\n",     snp.class);
printf("<BR><B><A HREF=\"#Valid\">Validation Status</A>: </B>%s\n", snp.valid);
printf("<BR><B><A HREF=\"#Func\">Function</A>: </B>%s\n",           snp.func);
printf("<BR><B><A HREF=\"#LocType\">Location Type</A>: </B>%s\n",   snp.locType);
if (snp.avHet>0)
    printf("<BR><B><A HREF=\"#AvHet\">Average Heterozygosity</A>: </B>%.3f +/- %.3f", snp.avHet, snp.avHetSE);
printf("<BR>\n");
}

off_t getSnpSeqFileOffset(struct trackDb *tdb, struct snp *snp)
/* do a lookup in snpSeq for the offset */
{
char *snpSeqSetting = trackDbSetting(tdb, "snpSeq");
char snpSeqTable[128];
char query[256];
char **row;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
off_t offset = 0;

if (isNotEmpty(snpSeqSetting))
    {
    if (hTableExists(database, snpSeqSetting))
	safecpy(snpSeqTable, sizeof(snpSeqTable), snpSeqSetting);
    else
	return -1;
    }
else
    {
    safef(snpSeqTable, sizeof(snpSeqTable), "%sSeq", tdb->table);
    if (!hTableExists(database, snpSeqTable))
	{
	safecpy(snpSeqTable, sizeof(snpSeqTable), "snpSeq");
	if (!hTableExists(database, snpSeqTable))
	    return -1;
	}
    }
sqlSafef(query, sizeof(query), "select file_offset from %s where acc='%s'",
      snpSeqTable, snp->name);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL)
   return -1;
offset = sqlLongLong(row[0]);
sqlFreeResult(&sr);
hFreeConn(&conn);
return offset;
}


char *getSnpSeqFile(struct trackDb *tdb, int version)
/* find location of snp.fa and test existence. */
{
char *seqFile = trackDbSetting(tdb, "snpSeqFile");
if (isNotEmpty(seqFile))
    {
    if (fileExists(seqFile))
	return cloneString(seqFile);
    else
	return NULL;
    }
char seqFileBuf[512];
safef(seqFileBuf, sizeof(seqFileBuf), "/gbdb/%s/snp/%s.fa",
      database, tdb->table);
if (fileExists(seqFileBuf))
    return cloneString(seqFileBuf);
safef(seqFileBuf, sizeof(seqFileBuf), "/gbdb/%s/snp/snp%d.fa", database, version);
if (fileExists(seqFileBuf))
    return cloneString(seqFileBuf);
safef(seqFileBuf, sizeof(seqFileBuf), "/gbdb/%s/snp/snp.fa", database);
if (fileExists(seqFileBuf))
    return cloneString(seqFileBuf);
return NULL;
}


void printNullAlignment(int l, int r, int q)
/* Print out a double-sided gap for unaligned insertion SNP. */
{
int digits = max(digitsBaseTen(r), digitsBaseTen(q));
printf("%0*d - %0*d\n"
       "%*s"
       "  (dbSNP-annotated position was not re-aligned to "
       "observed allele code -- see adjacent alignments)\n"
       "%0*d - %0*d</B>\n\n", digits, l, digits, r,
       (digits*2 + 3), "", digits, q+1, digits, q);
}

void printOffsetAndBoldAxt(struct axt *axtIn, int lineWidth,
			   struct axtScoreScheme *ss, int tOffset, int qOffset,
			   int boldStart, int boldEnd,
			   boolean tIsRc, int tSize, int qSize)
/* Given an axt block, break it into multiple blocks for printing if
 * the bold range falls in the middle; add t & qOffset to t & q
 * coords; and print all blocks.  boldStart and boldEnd are relative to
 * the target sequence used to make axtIn (start at 0, not tOffset).
 * tIsRc means that the target sequence that was aligned to create axtIn
 * was reverse-complemented so we want to display t coords backwards;
 * that includes reversing block coords within the target seq range. */
{
struct axt axtBlock;
int nullQStart = 0;

/* (Defining a macro because a function would have an awful lot of arguments.)
 * First extract the portion of axtIn for this block.  If tIsRc, then
 * reverse the block's t coords within the range of (0, tSize].
 * Add t and qOffset, swap target and query so that target sequence is on top,
 * and print it out, adding bold tags before and afterwards if isBold. */
#define doBlock(blkStart, blkEnd, isBold) \
    if (isBold) printf("<B>"); \
    if (axtGetSubsetOnT(axtIn, &axtBlock, blkStart, blkEnd, ss, isBold)) \
        { \
        if (tIsRc) \
            { \
            int tmp = axtBlock.tStart; \
            axtBlock.tStart = tSize - axtBlock.tEnd;  \
            axtBlock.tEnd = tSize - tmp; \
            } \
        axtBlock.tStart += tOffset;  axtBlock.tEnd += tOffset; \
        axtBlock.qStart += qOffset;  axtBlock.qEnd += qOffset; \
        nullQStart = axtBlock.qEnd; \
        axtSwap(&axtBlock, tSize, qSize); \
        axtPrintTraditionalExtra(&axtBlock, lineWidth, ss, stdout, \
                                 FALSE, tIsRc); \
        } \
    else if (isBold) \
        { \
        int ins = (tIsRc ? tSize - blkEnd : blkEnd) + tOffset; \
        int l = tIsRc ? ins : ins+1,  r = tIsRc ? ins+1 : ins; \
        printNullAlignment(l, r, nullQStart); \
        } \
    if (isBold) printf("</B>");

/* First block: before bold range */
doBlock(axtIn->tStart, boldStart, FALSE);
/* Second block: bold range */
doBlock(boldStart, boldEnd, TRUE);
/* Third block: after bold range */
doBlock(boldEnd, axtIn->tEnd, FALSE);

#undef doBlock
}

void generateAlignment(struct dnaSeq *tSeq, struct dnaSeq *qSeq,
		       int lineWidth, int tOffset, int qOffset,
		       int boldStart, int boldEnd, boolean tIsRc)
/* Use axtAffine to align tSeq to qSeq.  Print the resulting alignment.
 * tOffset and qOffset are added to the respective sets of coordinates for
 * printing.  If boldStart and boldEnd have any overlap with the aligned
 * tSeq, print that region as a separate block, in bold.  boldStart and
 * boldEnd are relative to the start of tSeq (start at 0 not tOffset).
 * tIsRc means that tSeq has been reverse-complemented so we want to
 * display t coords backwards. */
{
int matchScore = 100;
int misMatchScore = 100;
int gapOpenPenalty = 400;
int gapExtendPenalty = 50;
struct axtScoreScheme *ss = axtScoreSchemeSimpleDna(matchScore, misMatchScore, gapOpenPenalty, gapExtendPenalty);
struct axt *axt = axtAffine(qSeq, tSeq, ss), *axtBlock=axt;

hPrintf("<PRE>");
if (axt == NULL)
   {
   printf("%s and %s don't align\n", tSeq->name, qSeq->name);
   return;
   }

printf("<B>Alignment between genome (%s, %c strand; %d bp) and "
       "dbSNP sequence (%s; %d bp)</B>\n",
       tSeq->name, (tIsRc ? '-' : '+'), tSeq->size, qSeq->name, qSeq->size);
for (axtBlock=axt;  axtBlock !=NULL;  axtBlock = axtBlock->next)
    {
    printf("ID (including gaps) %3.1f%%, coverage (of both) %3.1f%%\n\n",
           axtIdWithGaps(axtBlock)*100,
	   axtCoverage(axtBlock, qSeq->size, tSeq->size)*100);
    printOffsetAndBoldAxt(axtBlock, lineWidth, ss, tOffset, qOffset,
			  boldStart, boldEnd, tIsRc, tSeq->size, qSeq->size);
    }

axtFree(&axt);
hPrintf("</PRE>");
}

void printSnpAlignment(struct trackDb *tdb, struct snp *snp, int version)
/* Get flanking sequences from table; align and print */
{
char *fileName = NULL;
char *variation = NULL;

char *line;
struct lineFile *lf = NULL;
static int maxFlank = 1000;
static int lineWidth = 100;

boolean gotVar = FALSE;
boolean leftFlankTrimmed = FALSE;
boolean rightFlankTrimmed = FALSE;

struct dyString *seqDbSnp5 = dyStringNew(512);
struct dyString *seqDbSnp3 = dyStringNew(512);
struct dyString *seqDbSnpTemp = dyStringNew(512);

char *leftFlank = NULL;
char *rightFlank = NULL;

struct dnaSeq *dnaSeqDbSnp5 = NULL;
struct dnaSeq *dnaSeqDbSnpO = NULL;
struct dnaSeq *dnaSeqDbSnp3 = NULL;
struct dnaSeq *seqDbSnp = NULL;
struct dnaSeq *seqNib = NULL;

int len5 = 0;
int len3 = 0;
int start = 0;
int end = 0;
int skipCount = 0;

off_t offset = 0;

fileName = getSnpSeqFile(tdb, version);
if (!fileName)
    return;

offset = getSnpSeqFileOffset(tdb, snp);
if (offset == -1)
    return;

lf = lineFileOpen(fileName, TRUE);
lineFileSeek(lf, offset, SEEK_SET);
/* skip the header line */
lineFileNext(lf, &line, NULL);
if (!startsWith(">rs", line))
    errAbort("Expected FASTA header, got this line:\n%s\nat offset %lld "
	     "in file %s", line, (long long)offset, fileName);

while (lineFileNext(lf, &line, NULL))
    {
    stripString(line, " ");
    int len = strlen(line);
    if (len == 0)
        break;
    else if (len == 1 && isIupacAmbiguous(line[0]))
        {
	gotVar = TRUE;
	variation = cloneString(line);
	}
    else if (gotVar)
        dyStringAppend(seqDbSnp3, line);
    else
        dyStringAppend(seqDbSnp5, line);
    }
lineFileClose(&lf);

if (variation == NULL)
    {
    printf("<P>Could not parse ambiguous SNP base out of dbSNP "
	   "sequence, so can't display re-alignment of flanking sequences.\n");
    return;
    }

/* trim */
/* axtAffine has a limit of 100,000,000 bases for query x target */
leftFlank = dyStringCannibalize(&seqDbSnp5);
rightFlank = dyStringCannibalize(&seqDbSnp3);
len5 = strlen(leftFlank);
len3 = strlen(rightFlank);
if (len5 > maxFlank)
    {
    skipCount = len5 - maxFlank;
    leftFlank = leftFlank + skipCount;
    leftFlankTrimmed = TRUE;
    len5 = strlen(leftFlank);
    }
if (len3 > maxFlank)
    {
    rightFlank[maxFlank] = '\0';
    rightFlankTrimmed = TRUE;
    len3 = strlen(rightFlank);
    }

/* get genomic coords */
int isRc = sameString(snp->strand, "-");
if (isRc)
    {
    start = snp->chromStart - len3;
    end = snp->chromEnd + len5;
    }
else
    {
    start = snp->chromStart - len5;
    end = snp->chromEnd + len3;
    }
int genoLen3 = len3;
int genoLen5 = len5;
if (start < 0)
    {
    if (isRc)
	genoLen3 += start;
    else
	genoLen5 += start;
    start = 0;
    }
int chromSize = hChromSize(database, snp->chrom);
if (end > chromSize)
    {
    if (isRc)
	genoLen5 += (chromSize - end);
    else
	genoLen3 += (chromSize - end);
    end = chromSize;
    }

/* do the lookup */
seqNib = hChromSeqMixed(database, snp->chrom, start, end);
if (seqNib == NULL)
    errAbort("Couldn't get genomic sequence around %s (%s:%d-%d)",
	     snp->name, snp->chrom, start+1, end);
if (isRc)
    reverseComplement(seqNib->dna, seqNib->size);
char betterName[512];
safef(betterName, sizeof(betterName), "%s %s:%d-%d",
      database, seqName, start+1, end);
seqNib->name = cloneString(betterName);

jsBeginCollapsibleSection(cart, tdb->track, "realignment",
			  "Re-alignment of the SNP's flanking sequences to the genomic sequence",
			  FALSE);
printf("Note: this alignment was computed by UCSC and may not be identical to "
       "NCBI's alignment used to map the SNP.\n");

printf("<PRE><B>Genomic sequence around %s (%s:%d-%d, %s strand):</B>\n",
       snp->name, snp->chrom, start+1, end,
       isRc ? "reverse complemented for -" : "+");
int snpWidth = snp->chromEnd - snp->chromStart;
writeSeqWithBreaks(stdout, seqNib->dna, genoLen5, lineWidth);
printf("<B>");
if (snp->chromEnd > snp->chromStart)
    writeSeqWithBreaks(stdout, seqNib->dna + genoLen5, snpWidth, lineWidth);
else
    printf("-\n");
printf("</B>");
writeSeqWithBreaks(stdout, seqNib->dna + seqNib->size - genoLen3, genoLen3,
		   lineWidth);
printf("</PRE>\n");

printf("\n<PRE><B>dbSNP flanking sequences and observed allele code for %s"
       ":</B>\n", snp->name);
printf("(Uses ");
printf("<A HREF=\"../goldenPath/help/iupac.html\"" );
printf("TARGET=_BLANK>IUPAC ambiguity codes</A>");
printf(")\n");
if (leftFlankTrimmed)
    printf("Left flank trimmed to %d bases.\n", maxFlank);
if (rightFlankTrimmed)
    printf("Right flank trimmed to %d bases.\n", maxFlank);
dnaSeqDbSnp5 = newDnaSeq(leftFlank, len5, "dbSNP seq 5");
dnaSeqDbSnpO = newDnaSeq(variation, strlen(variation),"dbSNP seq O");
dnaSeqDbSnp3 = newDnaSeq(rightFlank, len3, "dbSNP seq 3");
writeSeqWithBreaks(stdout, dnaSeqDbSnp5->dna, dnaSeqDbSnp5->size, lineWidth);
printf("<B>");
writeSeqWithBreaks(stdout, dnaSeqDbSnpO->dna, dnaSeqDbSnpO->size, lineWidth);
printf("</B>");
writeSeqWithBreaks(stdout, dnaSeqDbSnp3->dna, dnaSeqDbSnp3->size, lineWidth);
printf("</PRE>\n");

/* create seqDbSnp */
dyStringAppend(seqDbSnpTemp, leftFlank);
dyStringAppend(seqDbSnpTemp, variation);
dyStringAppend(seqDbSnpTemp, rightFlank);
seqDbSnp = newDnaSeq(seqDbSnpTemp->string, strlen(seqDbSnpTemp->string),
		     snp->name);
if (seqDbSnp == NULL)
    {
    warn("Couldn't get sequences");
    return;
    }
seqDbSnp->size = strlen(seqDbSnp->dna);

generateAlignment(seqNib, seqDbSnp, lineWidth, start, skipCount,
		  genoLen5, genoLen5 + snpWidth, isRc);
jsEndCollapsibleSection();
}

void doSnp(struct trackDb *tdb, char *itemName)
/* Process SNP details. */
{
char   *snpTable = tdb->table;
struct snp snp;
int    start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char   query[256];
int    rowOffset=hOffsetPastBin(database, seqName, snpTable);
int    firstOne=1;
char  *exception=0;
char  *chrom="";
int    chromStart=0;

cartWebStart(cart, database, "Simple Nucleotide Polymorphism (SNP)");
printf("<H2>Simple Nucleotide Polymorphism (SNP) %s</H2>\n", itemName);
sqlSafef(query, sizeof(query), "select * from %s where chrom='%s' and "
      "chromStart=%d and name='%s'", snpTable, seqName, start, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr))!=NULL)
    {
    snpStaticLoad(row+rowOffset, &snp);
    if (firstOne)
	{
	exception=cloneString(snp.exception);
	chrom = cloneString(snp.chrom);
	chromStart = snp.chromStart;
	bedPrintPos((struct bed *)&snp, 3, tdb);
	printf("<BR>\n");
	firstOne=0;
	}
    printSnpInfo(snp);
    }
if (startsWith("rs",itemName))
    {
    printDbSnpRsUrl(itemName, "dbSNP");
    putchar('\n');
    doSnpEntrezGeneLink(tdb, itemName);
    }
if (hTableExists(database, "snpExceptions") && differentString(exception,"0"))
    writeSnpException(exception, itemName, rowOffset, chrom, chromStart, tdb);
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doAffy120KDetails(struct trackDb *tdb, char *name)
/* print additional SNP details */
{
struct sqlConnection *conn = sqlConnect("hgFixed");
char query[1024];
struct affy120KDetails *snp = NULL;
sqlSafef(query, sizeof(query),
         "select  affyId, rsId, baseA, baseB, sequenceA, sequenceB, "
	 "        enzyme, minFreq, hetzyg, avHetSE, "
         "        NA04477, NA04479, NA04846, NA11036, NA11038, NA13056, "
         "        NA17011, NA17012, NA17013, NA17014, NA17015, NA17016, "
         "        NA17101, NA17102, NA17103, NA17104, NA17105, NA17106, "
         "        NA17201, NA17202, NA17203, NA17204, NA17205, NA17206, "
         "        NA17207, NA17208, NA17210, NA17211, NA17212, NA17213, "
         "        PD01, PD02, PD03, PD04, PD05, PD06, PD07, PD08, "
         "        PD09, PD10, PD11, PD12, PD13, PD14, PD15, PD16, "
         "        PD17, PD18, PD19, PD20, PD21, PD22, PD23, PD24  "
         "from    affy120KDetails "
         "where   affyId = %s", name);
snp = affy120KDetailsLoadByQuery(conn, query);
if (snp!=NULL)
    {
    printf("<BR>\n");
    printf("<B>Sample Prep Enzyme:</B> <I>%s</I><BR>\n",snp->enzyme);
    printf("<B>Minimum Allele Frequency:</B> %.3f<BR>\n",snp->minFreq);
    printf("<B>Heterozygosity:</B> %.3f<BR>\n",snp->hetzyg);
    printf("<B>Base A:          </B> <span style='font-family:Courier;'>%s</span><BR>\n",
	   snp->baseA);
    printf("<B>Base B:          </B> <span style='font-family:Courier;'>%s</span><BR>\n",
	   snp->baseB);
    printf("<B>Sequence of Allele A:</B>&nbsp;<span style='font-family:Courier;'>");
    printf("%s</span><BR>\n",snp->sequenceA);
    printf("<B>Sequence of Allele B:</B>&nbsp;<span style='font-family:Courier;'>");
    printf("%s</span><BR>\n",snp->sequenceB);
    if (isNotEmpty(snp->rsId))
	{
	puts("<BR>");
	printDbSnpRsUrl(snp->rsId, "dbSNP link for %s", snp->rsId);
	puts("<BR>");
	}
    doSnpEntrezGeneLink(tdb, snp->rsId);
    printf("<BR>Genotypes:<BR>");
    printf("\n<BR><span style='font-family:Courier;'>");
    printf("NA04477:&nbsp;%s&nbsp;&nbsp;", snp->NA04477);
    printf("NA04479:&nbsp;%s&nbsp;&nbsp;", snp->NA04479);
    printf("NA04846:&nbsp;%s&nbsp;&nbsp;", snp->NA04846);
    printf("NA11036:&nbsp;%s&nbsp;&nbsp;", snp->NA11036);
    printf("NA11038:&nbsp;%s&nbsp;&nbsp;", snp->NA11038);
    printf("NA13056:&nbsp;%s&nbsp;&nbsp;", snp->NA13056);
    printf("\n<BR>NA17011:&nbsp;%s&nbsp;&nbsp;", snp->NA17011);
    printf("NA17012:&nbsp;%s&nbsp;&nbsp;", snp->NA17012);
    printf("NA17013:&nbsp;%s&nbsp;&nbsp;", snp->NA17013);
    printf("NA17014:&nbsp;%s&nbsp;&nbsp;", snp->NA17014);
    printf("NA17015:&nbsp;%s&nbsp;&nbsp;", snp->NA17015);
    printf("NA17016:&nbsp;%s&nbsp;&nbsp;", snp->NA17016);
    printf("\n<BR>NA17101:&nbsp;%s&nbsp;&nbsp;", snp->NA17101);
    printf("NA17102:&nbsp;%s&nbsp;&nbsp;", snp->NA17102);
    printf("NA17103:&nbsp;%s&nbsp;&nbsp;", snp->NA17103);
    printf("NA17104:&nbsp;%s&nbsp;&nbsp;", snp->NA17104);
    printf("NA17105:&nbsp;%s&nbsp;&nbsp;", snp->NA17105);
    printf("NA17106:&nbsp;%s&nbsp;&nbsp;", snp->NA17106);
    printf("\n<BR>NA17201:&nbsp;%s&nbsp;&nbsp;", snp->NA17201);
    printf("NA17202:&nbsp;%s&nbsp;&nbsp;", snp->NA17202);
    printf("NA17203:&nbsp;%s&nbsp;&nbsp;", snp->NA17203);
    printf("NA17204:&nbsp;%s&nbsp;&nbsp;", snp->NA17204);
    printf("NA17205:&nbsp;%s&nbsp;&nbsp;", snp->NA17205);
    printf("NA17206:&nbsp;%s&nbsp;&nbsp;", snp->NA17206);
    printf("\n<BR>NA17207:&nbsp;%s&nbsp;&nbsp;", snp->NA17207);
    printf("NA17208:&nbsp;%s&nbsp;&nbsp;", snp->NA17208);
    printf("NA17210:&nbsp;%s&nbsp;&nbsp;", snp->NA17210);
    printf("NA17211:&nbsp;%s&nbsp;&nbsp;", snp->NA17211);
    printf("NA17212:&nbsp;%s&nbsp;&nbsp;", snp->NA17212);
    printf("NA17213:&nbsp;%s&nbsp;&nbsp;", snp->NA17213);
    printf("\n<BR>PD01:&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;", snp->PD01);
    printf("PD02:&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;", snp->PD02);
    printf("PD03:&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;", snp->PD03);
    printf("PD04:&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;", snp->PD04);
    printf("PD05:&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;", snp->PD05);
    printf("PD06:&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;", snp->PD06);
    printf("\n<BR>PD07:&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;", snp->PD07);
    printf("PD08:&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;", snp->PD08);
    printf("PD09:&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;", snp->PD09);
    printf("PD10:&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;", snp->PD10);
    printf("PD11:&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;", snp->PD11);
    printf("PD12:&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;", snp->PD12);
    printf("\n<BR>PD13:&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;", snp->PD13);
    printf("PD14:&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;", snp->PD14);
    printf("PD15:&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;", snp->PD15);
    printf("PD16:&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;", snp->PD16);
    printf("PD17:&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;", snp->PD17);
    printf("PD18:&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;", snp->PD18);
    printf("\n<BR>PD19:&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;", snp->PD19);
    printf("PD20:&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;", snp->PD20);
    printf("PD21:&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;", snp->PD21);
    printf("PD22:&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;", snp->PD22);
    printf("PD23:&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;", snp->PD23);
    printf("PD24:&nbsp;&nbsp;&nbsp;&nbsp;%s&nbsp;&nbsp;", snp->PD24);
    printf("\n</span>\n");
    }
affy120KDetailsFree(&snp);
sqlDisconnect(&conn);
}

void doCnpLocke(struct trackDb *tdb, char *itemName)
{
char *table = tdb->table;
struct cnpLocke thisItem;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset = hOffsetPastBin(database, seqName, table);
int start = cartInt(cart, "o");

genericHeader(tdb, itemName);
printf("<B>NCBI Clone Registry: </B><A href=");
printCloneDbUrl(stdout, itemName);
printf(" target=_blank>%s</A><BR>\n", itemName);
sqlSafef(query, sizeof(query),
      "select * from %s where chrom = '%s' and "
      "chromStart=%d and name = '%s'", table, seqName, start, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    cnpLockeStaticLoad(row+rowOffset, &thisItem);
    bedPrintPos((struct bed *)&thisItem, 3, tdb);
    printf("<BR><B>Variation Type</B>: %s\n",thisItem.variationType);
    }
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doCnpIafrate(struct trackDb *tdb, char *itemName)
{
char *table = tdb->table;
struct cnpIafrate cnpIafrate;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset = hOffsetPastBin(database, seqName, table);
int start = cartInt(cart, "o");

genericHeader(tdb, itemName);
printf("<B>NCBI Clone Registry: </B><A href=");
printCloneDbUrl(stdout, itemName);
printf(" target=_blank>%s</A><BR>\n", itemName);
sqlSafef(query, sizeof(query),
      "select * from %s where chrom = '%s' and "
      "chromStart=%d and name = '%s'", table, seqName, start, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    cnpIafrateStaticLoad(row+rowOffset, &cnpIafrate);
    bedPrintPos((struct bed *)&cnpIafrate, 3, tdb);
    printf("<BR><B>Variation Type</B>: %s\n",cnpIafrate.variationType);
    printf("<BR><B>Score</B>: %g\n",cnpIafrate.score);
    }
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doCnpIafrate2(struct trackDb *tdb, char *itemName)
{
char *table = tdb->table;
struct cnpIafrate2 thisItem;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset = hOffsetPastBin(database, seqName, table);
int start = cartInt(cart, "o");

genericHeader(tdb, itemName);
printf("<B>NCBI Clone Registry: </B><A href=");
printCloneDbUrl(stdout, itemName);
printf(" target=_blank>%s</A><BR>\n", itemName);
sqlSafef(query, sizeof(query),
      "select * from %s where chrom = '%s' and "
      "chromStart=%d and name = '%s'", table, seqName, start, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    cnpIafrate2StaticLoad(row+rowOffset, &thisItem);
    bedPrintPos((struct bed *)&thisItem, 3, tdb);
    printf("<BR><B>Cohort Type</B>: %s\n",thisItem.cohortType);
    if (strstr(thisItem.cohortType, "Control"))
        {
        printf("<BR><B>Control Gain Count</B>: %d\n",thisItem.normalGain);
        printf("<BR><B>Control Loss Count</B>: %d\n",thisItem.normalLoss);
	}
    if (strstr(thisItem.cohortType, "Patient"))
        {
        printf("<BR><B>Patient Gain Count</B>: %d\n",thisItem.patientGain);
        printf("<BR><B>Patient Loss Count</B>: %d\n",thisItem.patientLoss);
	}
    }
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doDelHinds2(struct trackDb *tdb, char *itemName)
{
char *table = tdb->table;
struct delHinds2 thisItem;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset = hOffsetPastBin(database, seqName, table);
int start = cartInt(cart, "o");

genericHeader(tdb, itemName);
sqlSafef(query, sizeof(query),
      "select * from %s where chrom = '%s' and "
      "chromStart=%d and name = '%s'", table, seqName, start, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    delHinds2StaticLoad(row+rowOffset, &thisItem);
    bedPrintPos((struct bed *)&thisItem, 3, tdb);
    printf("<BR><B>Frequency</B>: %3.2f%%\n",thisItem.frequency);
    }
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doDelConrad2(struct trackDb *tdb, char *itemName)
{
char *table = tdb->table;
struct delConrad2 thisItem;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset = hOffsetPastBin(database, seqName, table);
int start = cartInt(cart, "o");

genericHeader(tdb, itemName);
sqlSafef(query, sizeof(query),
      "select * from %s where chrom = '%s' and "
      "chromStart=%d and name = '%s'", table, seqName, start, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    delConrad2StaticLoad(row+rowOffset, &thisItem);
    bedPrintPos((struct bed *)&thisItem, 3, tdb);
    printf("<BR><B>HapMap individual</B>: %s\n",thisItem.offspring);
    printf("<BR><B>HapMap population</B>: %s\n",thisItem.population);
    }
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}


void doCnpSebat(struct trackDb *tdb, char *itemName)
{
char *table = tdb->table;
struct cnpSebat cnpSebat;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset = hOffsetPastBin(database, seqName, table);
int start = cartInt(cart, "o");

genericHeader(tdb, itemName);
sqlSafef(query, sizeof(query),
      "select * from %s where chrom = '%s' and "
      "chromStart=%d and name = '%s'", table, seqName, start, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    cnpSebatStaticLoad(row+rowOffset, &cnpSebat);
    bedPrintPos((struct bed *)&cnpSebat, 3, tdb);
    printf("<BR><B>Number of probes</B>: %d\n",cnpSebat.probes);
    printf("<BR><B>Number of individuals</B>: %d\n",cnpSebat.individuals);
    }
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doCnpSebat2(struct trackDb *tdb, char *itemName)
{
char *table = tdb->table;
struct cnpSebat2 cnpSebat2;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset = hOffsetPastBin(database, seqName, table);
int start = cartInt(cart, "o");

genericHeader(tdb, itemName);
sqlSafef(query, sizeof(query),
      "select * from %s where chrom = '%s' and "
      "chromStart=%d and name = '%s'", table, seqName, start, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    cnpSebat2StaticLoad(row+rowOffset, &cnpSebat2);
    bedPrintPos((struct bed *)&cnpSebat2, 3, tdb);
    printf("<BR><B>Number of probes</B>: %d\n",cnpSebat2.probes);
    }
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}


void printCnpSharpDetails(struct cnpSharp cnpSharp)
{
printf("<B>Name:               </B> %s <BR>\n",     cnpSharp.name);
printf("<B>Variation type:     </B> %s <BR>\n",     cnpSharp.variationType);
printf("<B>Cytoband:           </B> %s <BR>\n",     cnpSharp.cytoName);
printf("<B>Strain:             </B> %s <BR>\n",     cnpSharp.cytoStrain);
printf("<B>Duplication Percent:</B> %.1f %%<BR>\n", cnpSharp.dupPercent*100);
printf("<B>Repeat Percent:     </B> %.1f %%<BR>\n", cnpSharp.repeatsPercent*100);
printf("<B>LINE Percent:       </B> %.1f %%<BR>\n", cnpSharp.LINEpercent*100);
printf("<B>SINE Percent:       </B> %.1f %%<BR>\n", cnpSharp.SINEpercent*100);
printf("<B>LTR Percent:        </B> %.1f %%<BR>\n", cnpSharp.LTRpercent*100);
printf("<B>DNA Percent:        </B> %.1f %%<BR>\n", cnpSharp.DNApercent*100);
printf("<B>Disease Percent:    </B> %.1f %%<BR>\n", cnpSharp.diseaseSpotsPercent*100);
}

void printCnpSharpSampleData(char *itemName)
{
struct sqlConnection *hgFixed1 = sqlConnect("hgFixed");
struct sqlConnection *hgFixed2 = sqlConnect("hgFixed");
char query[256], query2[1024];
char **row;
struct sqlResult *sr1, *sr2;
float sample, cutoff;

printf("<BR>\n");
sqlSafef(query, sizeof(query), "select distinct substring(sample,1,5) from cnpSharpCutoff order by sample");
sr1 = sqlGetResult(hgFixed1, query);
while ((row = sqlNextRow(sr1)) != NULL)
    {
    char *pop=row[0];
    printf("<table border=\"1\" cellpadding=\"0\" ><tr>");
    sqlSafef(query2, sizeof(query2),
	  "select s1.sample, s1.gender, s1.value, c1.value, s2.value, c2.value "
	  "from   cnpSharpSample s1, cnpSharpSample s2, cnpSharpCutoff c1, cnpSharpCutoff c2 "
	  "where  s1.sample=s2.sample and s1.sample=c1.sample and s1.sample=c2.sample "
	  "  and  s1.batch=1 and s2.batch=2 and c1.batch=1 and c2.batch=2 and s1.bac='%s' "
	  "  and  s1.bac=s2.bac and s1.sample like '%s%%' order by s1.sample", itemName, pop);
    sr2 = sqlGetResult(hgFixed2, query2);
    while ((row = sqlNextRow(sr2)) != NULL)
	{
	if (sameString(row[1],"M")) printf("<TD width=160 bgcolor=\"#99FF99\">");
	else                        printf("<TD width=160 bgcolor=\"#FFCCFF\">");
	printf("%s</TD>\n",row[0]);
	}
    printf("</TR><TR>\n");
    sqlFreeResult(&sr2);
    sr2 = sqlGetResult(hgFixed2, query2);
    while ((row = sqlNextRow(sr2)) != NULL)
	{
	sample = sqlFloat(row[2]);
	cutoff = sqlFloat(row[3]);
        if (sameString(row[2],"NA"))
	    printf("<TD width=160 >&nbsp; NA / %.3f </TD>\n",cutoff);
	else if (sample>=cutoff)
	    printf("<TD width=160  bgcolor=\"yellow\">&nbsp; %.3f / %.3f </TD>\n",sample,cutoff);
	else if (sample<= 0-cutoff)
	    printf("<TD width=160  bgcolor=\"gray\">&nbsp; %.3f / -%.3f </TD>\n",sample,cutoff);
	else printf("<TD width=160 >&nbsp; %.3f / %.3f </TD>\n",sample,cutoff);
	}
    printf("</TR><TR>\n");
    sqlFreeResult(&sr2);
    sr2 = sqlGetResult(hgFixed2, query2);
    while ((row = sqlNextRow(sr2)) != NULL)
	{
	sample = sqlFloat(row[4]);
	cutoff = sqlFloat(row[5]);
        if (sameString(row[4],"NA"))
	    printf("<TD width=160 >&nbsp; NA / %.3f </TD>\n",cutoff);
	else if (sample>=cutoff)
	    printf("<TD width=160  bgcolor=\"yellow\">&nbsp; %.3f / %.3f </TD>\n",sample,cutoff);
	else if (sample<= 0-cutoff)
	    printf("<TD width=160  bgcolor=\"gray\">&nbsp; %.3f / -%.3f </TD>\n",sample,cutoff);
	else printf("<TD width=160 >&nbsp; %.3f / %.3f </TD>\n",sample,cutoff);
	}
    sqlFreeResult(&sr2);
    printf("</tr></table>\n");
    }
sqlFreeResult(&sr1);
hFreeConn(&hgFixed1);
hFreeConn(&hgFixed2);

printf("<BR><B>Legend for individual values in table:</B>\n");
printf("&nbsp;&nbsp;<table>");
printf("<TR><TD>Title Color:</TD><TD bgcolor=\"#FFCCFF\">Female</TD><TD bgcolor=\"#99FF99\">Male</TD></TR>\n");
printf("<TR><TD>Value Color:</TD><TD bgcolor=\"yellow\" >Above Threshold</TD><TD bgcolor=\"gray\">Below negative threshold</TD></TR>\n");
printf("<TR><TD>Data Format:</TD><TD>Value / Threshold</TD></TR>\n");
printf("</table>\n");

}

void doCnpSharp(struct trackDb *tdb, char *itemName)
{
char *table = tdb->table;
struct cnpSharp cnpSharp;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset = hOffsetPastBin(database, seqName, table);
int start = cartInt(cart, "o");
char variantSignal;
char *itemCopy = cloneString(itemName);

variantSignal = lastChar(itemName);
if (variantSignal == '*')
   stripChar(itemCopy, '*');
if (variantSignal == '?')
   stripChar(itemCopy, '?');
if (variantSignal == '#')
   stripChar(itemCopy, '#');
genericHeader(tdb, itemCopy);
printf("<B>NCBI Clone Registry: </B><A href=");
printCloneDbUrl(stdout, itemCopy);
printf(" target=_blank>%s</A><BR>\n", itemCopy);
if (variantSignal == '*' || variantSignal == '?' || variantSignal == '#')
    printf("<B>Note this BAC was found to be variant.   See references.</B><BR>\n");
sqlSafef(query, sizeof(query),
      "select * from %s where chrom = '%s' and "
      "chromStart=%d and name = '%s'", table, seqName, start, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    cnpSharpStaticLoad(row+rowOffset, &cnpSharp);
    bedPrintPos((struct bed *)&cnpSharp, 3, tdb);
    printCnpSharpDetails(cnpSharp);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
// printCnpSharpSampleData(itemName);
printTrackHtml(tdb);
}


void doCnpSharp2(struct trackDb *tdb, char *itemName)
{
char *table = tdb->table;
struct cnpSharp2 cnpSharp2;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset = hOffsetPastBin(database, seqName, table);
int start = cartInt(cart, "o");

genericHeader(tdb, itemName);
printf("<B>NCBI Clone Registry: </B><A href=");
printCloneDbUrl(stdout, itemName);
printf(" target=_blank>%s</A><BR>\n", itemName);
sqlSafef(query, sizeof(query),
      "select * from %s where chrom = '%s' and "
      "chromStart=%d and name = '%s'", table, seqName, start, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    cnpSharp2StaticLoad(row+rowOffset, &cnpSharp2);
    bedPrintPos((struct bed *)&cnpSharp2, 3, tdb);
    printf("<B>Name: </B> %s <BR>\n", cnpSharp2.name);
    printf("<B>Variation type: </B> %s <BR>\n", cnpSharp2.variationType);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
// printCnpSharpSampleData(itemName);
printTrackHtml(tdb);
}

void doDgv(struct trackDb *tdb, char *id)
/* Details for Database of Genomic Variants (updated superset of cnp*). */
{
struct dgv dgv;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[512];
int rowOffset = hOffsetPastBin(database, seqName, tdb->table);
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
genericHeader(tdb, id);
printCustomUrl(tdb, id, FALSE);

sqlSafef(query, sizeof(query), "select * from %s where name = '%s' "
      "and chrom = '%s' and chromStart = %d and chromEnd = %d",
      tdb->table, id, seqName, start, end);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    dgvStaticLoad(row+rowOffset, &dgv);
    if (dgv.chromStart != dgv.thickStart ||
	(dgv.chromEnd != dgv.thickEnd && dgv.thickEnd != dgv.chromStart))
	{
	printf("<B>Variant Position:</B> "
	       "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">%s:%d-%d</A><BR>\n",
	       hgTracksPathAndSettings(), database,
	       dgv.chrom, dgv.thickStart+1, dgv.thickEnd,
	       dgv.chrom, dgv.thickStart+1, dgv.thickEnd);
	printBand(dgv.chrom, dgv.thickStart, dgv.thickEnd, FALSE);
	printf("<B>Variant Genomic Size:</B> %d<BR>\n",
	       dgv.thickEnd - dgv.thickStart);
	printf("<B>Locus:</B> "
	       "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">%s:%d-%d</A><BR>\n",
	       hgTracksPathAndSettings(), database,
	       dgv.chrom, dgv.chromStart+1, dgv.chromEnd,
	       dgv.chrom, dgv.chromStart+1, dgv.chromEnd);
	printf("<B>Locus Genomic Size:</B> %d<BR>\n",
	       dgv.chromEnd - dgv.chromStart);
	}
    else
	{
	printf("<B>Position:</B> "
	       "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">%s:%d-%d</A><BR>\n",
	       hgTracksPathAndSettings(), database,
	       dgv.chrom, dgv.chromStart+1, dgv.chromEnd,
	       dgv.chrom, dgv.chromStart+1, dgv.chromEnd);
	printBand(dgv.chrom, dgv.chromStart, dgv.chromEnd, FALSE);
	printf("<B>Genomic Size:</B> %d<BR>\n", dgv.chromEnd - dgv.chromStart);
	}
    printf("<B>Variant Type:</B> %s<BR>\n", dgv.varType);
    printf("<B>Reference:</B> <A HREF=\"");
    printEntrezPubMedUidAbstractUrl(stdout, dgv.pubMedId);
    printf("\" TARGET=_BLANK>%s</A><BR>\n", dgv.reference);
    printf("<B>Method/platform:</B> %s<BR>\n", dgv.method);
    printf("<B>Sample:</B> %s<BR>\n", dgv.sample);
    if (isNotEmpty(dgv.landmark))
	printf("<B>Landmark:</B> %s<BR>\n", dgv.landmark);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
printTrackHtml(tdb);
}

static void maybePrintCoriellLinks(struct trackDb *tdb, char *commaSepIds)
/* If id looks like a Coriell NA ID, print a link to Coriell, otherwise just print id. */
{
char *coriellUrlBase = trackDbSetting(tdb, "coriellUrlBase");
struct slName *id, *sampleIds = slNameListFromComma(commaSepIds);
for (id = sampleIds;  id != NULL;  id = id->next)
    {
    if (startsWith("NA", id->name) && countLeadingDigits(id->name+2) == strlen(id->name+2)
	&& isNotEmpty(coriellUrlBase))
	{
	// I don't know why coriell doesn't have direct links to NA's but oh well,
	// we can substitute 'GM' for 'NA' to get to the page...
	char *gmId = cloneString(id->name);
	gmId[0] = 'G';  gmId[1] = 'M';
	printf("<A HREF=\"%s%s\" TARGET=_BLANK>%s</A>", coriellUrlBase, gmId, id->name);
	freeMem(gmId);
	}
    else
	printf("%s", id->name);
    if (id->next != NULL)
	printf(", ");
    }
slNameFreeList(&sampleIds);
}

static void printBrowserPosLinks(char *commaSepIds)
/* Print hgTracks links with position=id. */
{
struct slName *id, *sampleIds = slNameListFromComma(commaSepIds);
for (id = sampleIds;  id != NULL;  id = id->next)
    {
    char *searchTerm = cgiEncode(trimSpaces(id->name));
    printf("<A HREF=\"%s&position=%s\">%s</A>", hgTracksPathAndSettings(), searchTerm, id->name);
    if (id->next != NULL)
	printf(", ");
    freeMem(searchTerm);
    }
slNameFreeList(&sampleIds);
}

void doDgvPlus(struct trackDb *tdb, char *id)
/* Details for Database of Genomic Variants, July 2013 and later. */
{
struct dgvPlus dgv;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[512];
int rowOffset = hOffsetPastBin(database, seqName, tdb->table);
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
genericHeader(tdb, id);
printCustomUrl(tdb, id, FALSE);

sqlSafef(query, sizeof(query), "select * from %s where name = '%s' "
      "and chrom = '%s' and chromStart = %d and chromEnd = %d",
      tdb->table, id, seqName, start, end);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    dgvPlusStaticLoad(row+rowOffset, &dgv);
    printf("<B>Position:</B> "
	   "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">%s:%d-%d</A><BR>\n",
	   hgTracksPathAndSettings(), database,
	   dgv.chrom, dgv.chromStart+1, dgv.chromEnd,
	   dgv.chrom, dgv.chromStart+1, dgv.chromEnd);
    printBand(dgv.chrom, dgv.chromStart, dgv.chromEnd, FALSE);
    printf("<B>Genomic size:</B> %d<BR>\n", dgv.chromEnd - dgv.chromStart);
    printf("<B>Variant type:</B> %s<BR>\n", dgv.varType);
    printf("<B>Reference:</B> <A HREF=\"");
    printEntrezPubMedUidAbstractUrl(stdout, dgv.pubMedId);
    printf("\" TARGET=_BLANK>%s</A><BR>\n", dgv.reference);
    printf("<B>Method:</B> %s<BR>\n", dgv.method);
    if (isNotEmpty(dgv.platform))
	printf("<B>Platform:</B> %s<BR>\n", dgv.platform);
    if (isNotEmpty(dgv.cohortDescription))
	printf("<B>Sample cohort description:</B> %s<BR>\n", dgv.cohortDescription);
    if (isNotEmpty(dgv.samples))
	{
	printf("<B>Sample IDs:</B> ");
	maybePrintCoriellLinks(tdb, dgv.samples);
	printf("<BR>\n");
	}
    printf("<B>Sample size:</B> %u<BR>\n", dgv.sampleSize);
    if (dgv.observedGains != 0 || dgv.observedLosses != 0)
	{
	printf("<B>Observed gains:</B> %u<BR>\n", dgv.observedGains);
	printf("<B>Observed losses:</B> %u<BR>\n", dgv.observedLosses);
	}
    if (isNotEmpty(dgv.mergedVariants))
	{
	printf("<B>Merged variants:</B> ");
	printBrowserPosLinks(dgv.mergedVariants);
	printf("<BR>\n");
	}
    if (isNotEmpty(dgv.supportingVariants))
	{
	printf("<B>Supporting variants:</B> ");
	printBrowserPosLinks(dgv.supportingVariants);
	printf("<BR>\n");
	}
    if (isNotEmpty(dgv.genes))
	{
	printf("<B>Genes:</B> ");
	printBrowserPosLinks(dgv.genes);
	printf("<BR>\n");
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
printTrackHtml(tdb);
}

void doAffy120K(struct trackDb *tdb, char *itemName)
/* Put up info on an Affymetrix SNP. */
{
char *table = tdb->table;
struct snp snp;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset;

cartWebStart(cart, database, "Single Nucleotide Polymorphism (SNP)");
printf("<H2>Single Nucleotide Polymorphism (SNP) %s</H2>\n", itemName);
sqlSafef(query, sizeof query, "select * "
	       "from   affy120K "
	       "where  chrom = '%s' "
	       "  and  chromStart = %d "
	       "  and  name = '%s'",
               seqName, start, itemName);
rowOffset = hOffsetPastBin(database, seqName, table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    snpStaticLoad(row+rowOffset, &snp);
    bedPrintPos((struct bed *)&snp, 3, tdb);
    }
doAffy120KDetails(tdb, itemName);
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doAffy10KDetails(struct trackDb *tdb, char *name)
/* print additional SNP details */
{
struct sqlConnection *conn = sqlConnect("hgFixed");
char query[1024];
struct affy10KDetails *snp=NULL;

sqlSafef(query, sizeof(query),
         "select  affyId, rsId, tscId, baseA, baseB, "
         "sequenceA, sequenceB, enzyme "
/** minFreq, hetzyg, and avHetSE are waiting for additional data from Affy **/
/*	 "        , minFreq, hetzyg, avHetSE "*/
         "from    affy10KDetails "
         "where   affyId = '%s'", name);
snp = affy10KDetailsLoadByQuery(conn, query);
if (snp!=NULL)
    {
    printf("<BR>\n");
    printf("<B>Sample Prep Enzyme:      </B> <I>XbaI</I><BR>\n");
/** minFreq, hetzyg, and avHetSE are waiting for additional data from Affy **/
/*  printf("<B>Minimum Allele Frequency:</B> %.3f<BR>\n",snp->minFreq);*/
/*  printf("<B>Heterozygosity:          </B> %.3f<BR>\n",snp->hetzyg);*/
/*  printf("<B>Average Heterozygosity:  </B> %.3f<BR>\n",snp->avHetSE);*/
    printf("<B>Base A:                  </B> <span style='font-family:Courier;'>");
    printf("%s</span><BR>\n",snp->baseA);
    printf("<B>Base B:                  </B> <span style='font-family:Courier;'>");
    printf("%s</span><BR>\n",snp->baseB);
    printf("<B>Sequence of Allele A:    </B>&nbsp;<span style='font-family:Courier;'>");
    printf("%s</span><BR>\n",snp->sequenceA);
    printf("<B>Sequence of Allele B:    </B>&nbsp;<span style='font-family:Courier;'>");
    printf("%s</span><BR>\n",snp->sequenceB);

    printf("<P><A HREF=\"https://www.affymetrix.com/LinkServlet?probeset=");
    printf("%s", snp->affyId);
    printf("\" TARGET=_blank>Affymetrix NetAffx Analysis Center link for ");
    printf("%s</A></P>\n", snp->affyId);

    if (strncmp(snp->rsId,"unmapped",8))
	{
	puts("<P>");
	printDbSnpRsUrl(snp->rsId, "dbSNP link for %s", snp->rsId);
	puts("</P>");
	}
    printf("<BR><A HREF=\"http://snp.cshl.org/cgi-bin/snp?name=");
    printf("%s\" TARGET=_blank>TSC link for %s</A>\n",
	   snp->tscId, snp->tscId);
    doSnpEntrezGeneLink(tdb, snp->rsId);
    }
/* else errAbort("<BR>Error in Query:\n%s<BR>\n",query); */
affy10KDetailsFree(&snp);
sqlDisconnect(&conn);
}

void doAffy10K(struct trackDb *tdb, char *itemName)
/* Put up info on an Affymetrix SNP. */
{
char *table = tdb->table;
struct snp snp;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset;

cartWebStart(cart, database, "Single Nucleotide Polymorphism (SNP)");
printf("<H2>Single Nucleotide Polymorphism (SNP) %s</H2>\n", itemName);
sqlSafef(query, sizeof query, "select * "
	       "from   affy10K "
	       "where  chrom = '%s' "
	       "  and  chromStart = %d "
	       "  and  name = '%s'",
               seqName, start, itemName);
rowOffset = hOffsetPastBin(database, seqName, table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    snpStaticLoad(row+rowOffset, &snp);
    bedPrintPos((struct bed *)&snp, 3, tdb);
    }
doAffy10KDetails(tdb, itemName);
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void printSnpOrthoSummary(struct trackDb *tdb, char *rsId, char *observed)
/* helper function for printSnp125Info */
{
char *orthoTable = snp125OrthoTable(tdb, NULL);
if (isNotEmpty(orthoTable) && hTableExists(database, orthoTable))
    {
    struct sqlConnection *conn = hAllocConn(database);
    struct sqlResult *sr;
    char **row;
    char query[512];
    sqlSafef(query, sizeof(query),
          "select chimpAllele from %s where name='%s'", orthoTable, rsId);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	printf("<B>Summary: </B>%s>%s (chimp allele displayed first, "
	       "then '>', then human alleles)<br>\n", row[0], observed);
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }
}

#define FOURBLANKCELLS "<TD></TD><TD></TD><TD></TD><TD></TD>"

static char *abbreviateAllele(char *allele)
/* If allele is >50bp then return an abbreviated version with first & last 20 bases and length;
 * otherwise just return (cloned) allele. */
{
int length = strlen(allele);
if (length > 50)
    {
    struct dyString *dyAbbr = dyStringCreate("%.20s", allele);
    dyStringAppend(dyAbbr, "...");
    dyStringAppend(dyAbbr, allele+length - 20);
    dyStringPrintf(dyAbbr, " (%d bases)", length);
    return dyStringCannibalize(&dyAbbr);
    }
return cloneString(allele);
}

void printSnpAlleleRows(struct snp125 *snp, int version)
/* Print the UCSC ref allele (and dbSNP if it differs), as row(s) of a
 * 6-column table. */
{
if (sameString(snp->strand,"+") ||
    strchr(snp->refUCSC, '(')) // don't try to revComp refUCSC if it is "(N bp insertion)" etc.
    {
    printf("<TR><TD><B>Reference allele:&nbsp;</B></TD>"
	   "<TD align=center>%s</TD>"FOURBLANKCELLS"</TR>\n", abbreviateAllele(snp->refUCSC));
    if (!sameString(snp->refUCSC, snp->refNCBI))
	printf("<TR><TD><B>dbSnp reference allele:&nbsp;</B></TD>"
	       "<TD align=center>%s</TD>"FOURBLANKCELLS"</TR>\n", abbreviateAllele(snp->refNCBI));
    }
else if (sameString(snp->strand,"-"))
    {
    char *refUCSCRevComp = cloneString(snp->refUCSC);
    reverseComplement(refUCSCRevComp, strlen(refUCSCRevComp));
    printf("<TR><TD><B>Reference allele:&nbsp;</B></TD>"
	   "<TD align=center>%s</TD>"FOURBLANKCELLS"</TR>\n", abbreviateAllele(refUCSCRevComp));
    if (version < 127 && !sameString(refUCSCRevComp, snp->refNCBI))
	printf("<TR><TD><B>dbSnp reference allele:&nbsp;</B></TD>"
	       "<TD align=center>%s</TD>"FOURBLANKCELLS"</TR>\n", abbreviateAllele(snp->refNCBI));
    else if (version >= 127 && !sameString(snp->refUCSC, snp->refNCBI))
	{
	char *refNCBIRevComp = cloneString(snp->refNCBI);
        if (! strchr(snp->refNCBI, '('))
            reverseComplement(refNCBIRevComp, strlen(refNCBIRevComp));
	printf("<TR><TD><B>dbSnp reference allele:&nbsp;</B></TD>"
	       "<TD align=center>%s</TD>"FOURBLANKCELLS"</TR>\n",
	       abbreviateAllele(refNCBIRevComp));
	}
    }
}

#define TINYPADDING 3
void printSnpOrthoOneRow(char *orthoName, char *orthoDb,
			 char *orthoAllele, char *orthoStrand,
			 char *orthoChrom, int orthoStart, int orthoEnd)
/* Print out a 6-column table row describing an orthologous allele. */
{
printf("<TR><TD><B>%s allele:&nbsp;</B></TD>"
       "<TD align=center>%s</TD>\n", orthoName, orthoAllele);
if (!sameString(orthoAllele, "?"))
    {
    printf("<TD>&nbsp;&nbsp;&nbsp;<B>%s strand:&nbsp;</B></TD>"
	   "<TD>%s</TD>\n", orthoName, orthoStrand);
    printf("<TD>&nbsp;&nbsp;&nbsp;<B>%s position:&nbsp;</B></TD>"
	   "<TD>\n", orthoName);
    if (isNotEmpty(orthoDb))
	linkToOtherBrowser(orthoDb, orthoChrom,
			   orthoStart-TINYPADDING, orthoEnd+TINYPADDING);
    printf("%s:%d-%d\n", orthoChrom, orthoStart+1, orthoEnd);
    printf("%s</TD>\n",
	   isNotEmpty(orthoDb) ? "</A>" : "");
    }
else
    printf(FOURBLANKCELLS"\n");
printf("</TR>\n");
}


void printSnpOrthoRows(struct trackDb *tdb, struct snp125 *snp)
/* If a chimp+macaque ortho table was specified, print out the orthos
 * (if any), as rows of a 6-column table. */
{
int speciesCount = 0;
char *orthoTable = snp125OrthoTable(tdb, &speciesCount);
if (isNotEmpty(orthoTable) && hTableExists(database, orthoTable))
    {
    struct sqlConnection *conn = hAllocConn(database);
    struct sqlResult *sr;
    char **row;
    char query[1024];
    if (speciesCount == 2)
	sqlSafef(query, sizeof(query),
	 "select chimpChrom, chimpStart, chimpEnd, chimpAllele, chimpStrand, "
	 "macaqueChrom, macaqueStart, macaqueEnd, macaqueAllele, macaqueStrand "
	 "from %s where chrom='%s' and bin=%d and chromStart=%d and name='%s'",
	 orthoTable, seqName, binFromRange(snp->chromStart, snp->chromEnd),
	 snp->chromStart, snp->name);
    else
	sqlSafef(query, sizeof(query),
	 "select chimpChrom, chimpStart, chimpEnd, chimpAllele, chimpStrand, "
	 "orangChrom, orangStart, orangEnd, orangAllele, orangStrand, "
	 "macaqueChrom, macaqueStart, macaqueEnd, macaqueAllele, macaqueStrand "
	 "from %s where chrom='%s' and bin=%d and chromStart=%d and name='%s'",
	 orthoTable, seqName, binFromRange(snp->chromStart, snp->chromEnd),
	 snp->chromStart, snp->name);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	char *chimpChrom = row[0];
	int chimpStart = sqlUnsigned(row[1]);
	int chimpEnd = sqlUnsigned(row[2]);
	char *chimpAllele = row[3];
	char *chimpStrand = row[4];
	char *chimpDb = trackDbSetting(tdb, "chimpDb");
	printSnpOrthoOneRow("Chimp", chimpDb, chimpAllele, chimpStrand,
			    chimpChrom, chimpStart, chimpEnd);
	char *orangChrom, *orangAllele, *orangStrand, *orangDb;
	int orangStart, orangEnd;
	char *macaqueChrom, *macaqueAllele, *macaqueStrand, *macaqueDb;
	int macaqueStart, macaqueEnd;
	if (speciesCount == 2)
	    {
	    macaqueChrom = row[5];
	    macaqueStart = sqlUnsigned(row[6]);
	    macaqueEnd = sqlUnsigned(row[7]);
	    macaqueAllele = row[8];
	    macaqueStrand = row[9];
	    macaqueDb = trackDbSetting(tdb, "macaqueDb");
	    }
	else
	    {
	    orangChrom = row[5];
	    orangStart = sqlUnsigned(row[6]);
	    orangEnd = sqlUnsigned(row[7]);
	    orangAllele = row[8];
	    orangStrand = row[9];
	    orangDb = trackDbSetting(tdb, "orangDb");
	    printSnpOrthoOneRow("Orangutan", orangDb, orangAllele, orangStrand,
				orangChrom, orangStart, orangEnd);
	    macaqueChrom = row[10];
	    macaqueStart = sqlUnsigned(row[11]);
	    macaqueEnd = sqlUnsigned(row[12]);
	    macaqueAllele = row[13];
	    macaqueStrand = row[14];
	    macaqueDb = trackDbSetting(tdb, "macaqueDb");
	    }
	printSnpOrthoOneRow("Macaque", macaqueDb, macaqueAllele, macaqueStrand,
			    macaqueChrom, macaqueStart, macaqueEnd);
	sqlFreeResult(&sr);
	hFreeConn(&conn);
	}
    }
}

void printSnpAlleleAndOrthos(struct trackDb *tdb, struct snp125 *snp,
			     int version)
/* Print the UCSC ref allele (and dbSNP if it differs).  If a
 * chimp+macaque ortho table was specified, print out the orthos (if
 * any).  Wrap a table around them all so that if there are dbSNP,
 * chimp and/or macaque alleles, we can line them up nicely with the
 * reference allele. */
{
printf("<TABLE border=0 cellspacing=0 cellpadding=0>\n");
printSnpAlleleRows(snp, version);
printSnpOrthoRows(tdb, snp);
printf("</TABLE>");
}

static char getSnpTxBase(struct genePred *gene, int exonIx, int snpStart, int offset)
/* Find the reference assembly base that is offset bases from snpStart in the translated sequence
 * of the gene. */
// Room for improvement: check for mRNA sequence associated with this gene, and use it if exists.
{
char base = 'N';
boolean ranOffEnd = FALSE;
int i;
int snpPlusOffset = snpStart;
if (offset >= 0)
    {
    int exonEnd = gene->exonEnds[exonIx];
    for (i = 0;  i < offset;  i++)
	{
	snpPlusOffset++;
	if (exonEnd <= snpPlusOffset)
	    {
	    if (++exonIx < gene->exonCount)
		{
		exonEnd = gene->exonEnds[exonIx];
		snpPlusOffset = gene->exonStarts[exonIx];
		}
	    else
		ranOffEnd = TRUE;
	    }
	}
    }
else
    {
    int exonStart = gene->exonStarts[exonIx];
    for (i = 0;  i > offset;  i--)
	{
	snpPlusOffset--;
	if (exonStart > snpPlusOffset)
	    {
	    if (--exonIx >= 0)
		{
		exonStart = gene->exonStarts[exonIx];
		snpPlusOffset = gene->exonEnds[exonIx] - 1;
		}
	    else
		ranOffEnd = TRUE;
	    }
	}
    }
if (! ranOffEnd)
    {
    struct dnaSeq *seq =
	hDnaFromSeq(database, gene->chrom, snpPlusOffset, snpPlusOffset+1, dnaUpper);
    base = seq->dna[0];
    }
return base;
}

char *getSymbolForGeneName(char *db, char *geneTable, char *geneId)
/* Given a gene track and gene accession, look up the symbol if we know where to look
 * and if we find it, return a string with both symbol and acc. */
{
struct dyString *dy = dyStringNew(32);
char buf[256];
char *sym = NULL;
if (sameString(geneTable, "knownGene") || sameString(geneTable, "refGene"))
    {
    struct sqlConnection *conn = hAllocConn(db);
    char query[256];
    query[0] = '\0';
    if (sameString(geneTable, "knownGene"))
	sqlSafef(query, sizeof(query), "select geneSymbol from kgXref where kgID = '%s'", geneId);
    else if (sameString(geneTable, "refGene"))
	sqlSafef(query, sizeof(query), "select name from %s where mrnaAcc = '%s'", refLinkTable, geneId);
    sym = sqlQuickQuery(conn, query, buf, sizeof(buf)-1);
    hFreeConn(&conn);
    }
if (sym != NULL)
    dyStringPrintf(dy, "%s (%s)", sym, geneId);
else
    dyStringAppend(dy, geneId);
return dyStringCannibalize(&dy);
}

#define firstTwoColumnsPctS "<TR><TD>%s&nbsp;&nbsp;</TD><TD>%s&nbsp;</TD><TD>"

void getSnp125RefCodonAndSnpPos(struct snp125 *snp, struct genePred *gene, int exonIx,
				int *pSnpCodonPos, char refCodon[4], char *pRefAA)
/* Given a single-base snp and a coding gene/exon containing it, determine the snp's position
 * in the codon and the reference codon & amino acid. */
{
boolean geneIsRc = sameString(gene->strand, "-");
int snpStart = snp->chromStart, snpEnd = snp->chromEnd;
int exonStart = gene->exonStarts[exonIx], exonEnd = gene->exonEnds[exonIx];
int cdsStart = gene->cdsStart, cdsEnd = gene->cdsEnd;
int exonFrame = gene->exonFrames[exonIx];
if (exonFrame == -1)
    exonFrame = 0;
if (cdsEnd < exonEnd) exonEnd = cdsEnd;
if (cdsStart > exonStart) exonStart = cdsStart;
int snpCodonPos = geneIsRc ? (2 - ((exonEnd - snpEnd) + exonFrame) % 3) :
			     (((snpStart - exonStart) + exonFrame) % 3);
refCodon[0] = getSnpTxBase(gene, exonIx, snpStart, -snpCodonPos);
refCodon[1] = getSnpTxBase(gene, exonIx, snpStart, 1 - snpCodonPos);
refCodon[2] = getSnpTxBase(gene, exonIx, snpStart, 2 - snpCodonPos);
refCodon[3] = '\0';
if (geneIsRc)
    {
    reverseComplement(refCodon, strlen(refCodon));
    snpCodonPos = 2 - snpCodonPos;
    }
if (pSnpCodonPos != NULL)
    *pSnpCodonPos = snpCodonPos;
if (pRefAA != NULL)
    {
    if (isMito(seqName))
        *pRefAA = lookupMitoCodon(refCodon);
    else
        *pRefAA = lookupCodon(refCodon);
    if (*pRefAA == '\0') *pRefAA = '*';
    }
}

static char *highlightCodonBase(char *codon, int offset)
/* If codon is a triplet and offset is 0 to 2, highlight the base at the offset.
 * Otherwise just return the given codon sequence unmodified.
 * Don't free the return value! */
{
static struct dyString *dy = NULL;
if (dy == NULL)
    dy = dyStringNew(0);
dyStringClear(dy);
if (strlen(codon) != 3)
    dyStringAppend(dy, codon);
else if (offset == 0)
    dyStringPrintf(dy, "<B>%c</B>%c%c", codon[0], codon[1], codon[2]);
else if (offset == 1)
    dyStringPrintf(dy, "%c<B>%c</B>%c", codon[0], codon[1], codon[2]);
else if (offset == 2)
    dyStringPrintf(dy, "%c%c<B>%c</B>", codon[0], codon[1], codon[2]);
else
    dyStringAppend(dy, codon);
return dy->string;
}

void printSnp125FunctionInCDS(struct snp125 *snp, char *geneTable, char *geneTrack,
			      struct genePred *gene, int exonIx, char *geneName)
/* Show the effect of each observed allele of snp on the given exon of gene. */
{
char *refAllele = cloneString(snp->refUCSC);
boolean refIsAlpha = isalpha(refAllele[0]);
boolean geneIsRc = sameString(gene->strand, "-"), snpIsRc = sameString(snp->strand, "-");
if (geneIsRc && refIsAlpha)
    reverseComplement(refAllele, strlen(refAllele));
int refAlleleSize = sameString(refAllele, "-") ? 0 : refIsAlpha ? strlen(refAllele) : -1;
boolean refIsSingleBase = (refAlleleSize == 1 && refIsAlpha);
int snpCodonPos = 0;
char refCodon[4], refAA = '\0';
if (refIsSingleBase)
    getSnp125RefCodonAndSnpPos(snp, gene, exonIx, &snpCodonPos, refCodon, &refAA);
char *alleleStr = cloneString(snp->observed);
char *indivAlleles[64];
int alleleCount = chopString(alleleStr, "/", indivAlleles, ArraySize(indivAlleles));
int j;
for (j = 0;  j < alleleCount;  j++)
    {
    char *al = indivAlleles[j];
    boolean alIsAlpha = (isalpha(al[0]) && !sameString(al, "lengthTooLong"));
    if ((snpIsRc ^ geneIsRc) && alIsAlpha)
	reverseComplement(al, strlen(al));
    char alBase = al[0];
    if (alBase == '\0' || sameString(al, refAllele))
	continue;
    int alSize = sameString(al, "-") ? 0 : alIsAlpha ? strlen(al) : -1;
    if (alSize != refAlleleSize && alSize >= 0 && refAlleleSize >=0)
	{

	int diff = alSize - refAlleleSize;
	if ((diff % 3) != 0)
	    printf(firstTwoColumnsPctS "%s\n",
		   geneTrack, geneName, snpMisoLinkFromFunc("frameshift"));
	else if (diff > 0)
	    printf(firstTwoColumnsPctS "%s (insertion of %d codon%s)\n",
		   geneTrack, geneName, snpMisoLinkFromFunc("inframe_insertion"),
		   (int)(diff/3), (diff > 3) ?  "s" : "");
	else
	    printf(firstTwoColumnsPctS "%s (deletion of %d codon%s)\n",
		   geneTrack, geneName, snpMisoLinkFromFunc("inframe_deletion"),
		   (int)(-diff/3), (diff < -3) ?  "s" : "");
	}
    else if (alSize == 1 && refIsSingleBase)
        {
        char snpCodon[4];
        safecpy(snpCodon, sizeof(snpCodon), refCodon);
        snpCodon[snpCodonPos] = alBase;
        char snpAA = '\0';
        if (isMito(seqName))
            snpAA = lookupMitoCodon(snpCodon);
        else
            snpAA = lookupCodon(snpCodon);
        if (snpAA == '\0') snpAA = '*';
        char refCodonHtml[16], snpCodonHtml[16];
        safecpy(refCodonHtml, sizeof(refCodonHtml), highlightCodonBase(refCodon, snpCodonPos));
        safecpy(snpCodonHtml, sizeof(snpCodonHtml), highlightCodonBase(snpCodon, snpCodonPos));
        if (refAA != snpAA)
            {
            if (refAA == '*')
                printf(firstTwoColumnsPctS "%s %c (%s) --> %c (%s)\n",
                       geneTrack, geneName, snpMisoLinkFromFunc("stop-loss"),
                       refAA, refCodonHtml, snpAA, snpCodonHtml);
            else if (snpAA == '*')
                printf(firstTwoColumnsPctS "%s %c (%s) --> %c (%s)\n",
                       geneTrack, geneName, snpMisoLinkFromFunc("nonsense"),
                       refAA, refCodonHtml, snpAA, snpCodonHtml);
            else
                printf(firstTwoColumnsPctS "%s %c (%s) --> %c (%s)\n",
                       geneTrack, geneName, snpMisoLinkFromFunc("missense"),
                       refAA, refCodonHtml, snpAA, snpCodonHtml);
            }
        else
            {
            if (refAA == '*')
                printf(firstTwoColumnsPctS "%s %c (%s) --> %c (%s)\n",
                       geneTrack, geneName, snpMisoLinkFromFunc("stop_retained_variant"),
                       refAA, refCodonHtml, snpAA, snpCodonHtml);
            else
                printf(firstTwoColumnsPctS "%s %c (%s) --> %c (%s)\n",
                       geneTrack, geneName, snpMisoLinkFromFunc("coding-synon"),
                       refAA, refCodonHtml, snpAA, snpCodonHtml);
            }
        }
    else
	printf(firstTwoColumnsPctS "%s %s --> %s\n",
	       geneTrack, geneName, snpMisoLinkFromFunc("cds-synonymy-unknown"),
               abbreviateAllele(refAllele), abbreviateAllele(al));
    }
}

void printSnp125FunctionInGene(char *db, struct snp125 *snp, char *geneTable, char *geneTrack,
			       struct genePred *gene)
/* Given a SNP and a gene that overlaps it, say where in the gene it overlaps
 * and if in CDS, say what effect the coding alleles have. */
{
int snpStart = snp->chromStart, snpEnd = snp->chromEnd;
int cdsStart = gene->cdsStart, cdsEnd = gene->cdsEnd;
boolean geneIsRc = sameString(gene->strand, "-");
char *geneName = getSymbolForGeneName(db, geneTable, gene->name);
int i, iStart = 0, iEnd = gene->exonCount, iIncr = 1;
if (geneIsRc)
    { iStart = gene->exonCount - 1;  iEnd = -1;  iIncr = -1; }
for (i = iStart;  i != iEnd;  i += iIncr)
    {
    int exonStart = gene->exonStarts[i], exonEnd = gene->exonEnds[i];
    if (snpEnd > exonStart && snpStart < exonEnd)
	{
	if (snpEnd > cdsStart && snpStart < cdsEnd)
	    printSnp125FunctionInCDS(snp, geneTable, geneTrack, gene, i, geneName);
	else if (cdsEnd > cdsStart)
	    {
	    boolean is5Prime = ((geneIsRc && (snpStart >= cdsEnd)) ||
				(!geneIsRc && (snpEnd < cdsStart)));
	    printf(firstTwoColumnsPctS "%s\n", geneTrack, geneName,
		   snpMisoLinkFromFunc((is5Prime) ? "untranslated-5" : "untranslated-3"));
	    }
	else
	    printf(firstTwoColumnsPctS "%s\n", geneTrack, geneName,
		   snpMisoLinkFromFunc("ncRNA"));
	}
    // SO term splice_region_variant applies to first/last 3 bases of exon
    // and first/last 3-8 bases of intron
    if ((i > 0 && snpStart < exonStart+3 && snpEnd > exonStart) ||
	(i < gene->exonCount-1 && snpStart < exonEnd && snpEnd > exonEnd-3))
	printf(", %s", snpMisoLinkFromFunc("splice_region_variant"));
    puts("</TD></TR>");
    if (i > 0)
	{
	int intronStart = gene->exonEnds[i-1], intronEnd = gene->exonStarts[i];
	if (snpEnd < intronStart || snpStart > intronEnd)
	    continue;
	if (snpStart < intronStart+2 && snpEnd > intronStart)
	    printf(firstTwoColumnsPctS "%s</TD></TR>\n", geneTrack, geneName,
		   snpMisoLinkFromFunc(geneIsRc ? "splice-3" : "splice-5"));
	else if (snpStart < intronStart+8 && snpEnd > intronStart+2)
	    printf(firstTwoColumnsPctS "%s, %s</TD></TR>\n", geneTrack, geneName,
		   snpMisoLinkFromFunc("intron_variant"),
		   snpMisoLinkFromFunc("splice_region_variant"));
	else if (snpStart < intronEnd-8 && snpEnd > intronStart+8)
	    printf(firstTwoColumnsPctS "%s</TD></TR>\n", geneTrack, geneName,
		   snpMisoLinkFromFunc("intron"));
	else if (snpStart < intronEnd-2 && snpEnd > intronEnd-8)
	    printf(firstTwoColumnsPctS "%s, %s</TD></TR>\n", geneTrack, geneName,
		   snpMisoLinkFromFunc("intron_variant"),
		   snpMisoLinkFromFunc("splice_region_variant"));
	else if (snpStart < intronEnd && snpEnd > intronEnd-2)
	    printf(firstTwoColumnsPctS "%s</TD></TR>\n", geneTrack, geneName,
		   snpMisoLinkFromFunc(geneIsRc ? "splice-5" : "splice-3"));
	}
    }
}

void printSnp125NearGenes(struct sqlConnection *conn, struct snp125 *snp, char *geneTable,
			  char *geneTrack)
/* Search upstream and downstream of snp for neigh */
{
struct sqlResult *sr;
char query[512];
char **row;
int snpStart = snp->chromStart, snpEnd = snp->chromEnd;
int nearCount = 0;
int maxDistance = 10000;
/* query to the left: */
sqlSafef(query, sizeof(query), "select name,txEnd,strand from %s "
      "where chrom = '%s' and txStart < %d and txEnd > %d",
      geneTable, snp->chrom, snpStart, snpStart - maxDistance);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *gene = row[0];
    char *geneName = getSymbolForGeneName(sqlGetDatabase(conn), geneTable, gene);
    int end = sqlUnsigned(row[1]);
    char *strand = row[2];
    boolean isRc = strand[0] == '-';
    printf(firstTwoColumnsPctS "%s (%d bases %sstream)</TD></TR>\n",
	   geneTrack, geneName, snpMisoLinkFromFunc(isRc ? "near-gene-5" : "near-gene-3"),
	   (snpStart - end + 1), (isRc ? "up" : "down"));
    nearCount++;
    }
sqlFreeResult(&sr);
/* query to the right: */
sqlSafef(query, sizeof(query), "select name,txStart,strand from %s "
      "where chrom = '%s' and txStart < %d and txEnd > %d",
      geneTable, snp->chrom, snpEnd + maxDistance, snpEnd);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *gene = row[0];
    char *geneName = getSymbolForGeneName(sqlGetDatabase(conn), geneTable, gene);
    int start = sqlUnsigned(row[1]);
    char *strand = row[2];
    boolean isRc = strand[0] == '-';
    printf(firstTwoColumnsPctS "%s (%d bases %sstream)</TD></TR>\n",
	   geneTrack, geneName, snpMisoLinkFromFunc(isRc ? "near-gene-3" : "near-gene-5"),
	   (start - snpEnd + 1), (isRc ? "down" : "up"));
    nearCount++;
    }
sqlFreeResult(&sr);
if (nearCount == 0)
    printf("<TR><TD>%s&nbsp;&nbsp;</TD><TD></TD><TD>%s</TD></TR>", geneTrack,
           snpMisoLinkFromFunc("intergenic_variant"));
}

static struct genePred *getGPsWithFrames(struct sqlConnection *conn, char *geneTable,
					 char *chrom, int start, int end)
/* Given a known-to-exist genePred table name and a range, return
 * genePreds in range with exonFrames populated. */
{
struct genePred *gpList = NULL;
boolean hasBin;
struct sqlResult *sr = hRangeQuery(conn, geneTable, chrom, start, end, NULL, &hasBin);
struct sqlConnection *conn2 = hAllocConn(sqlGetDatabase(conn));
boolean hasFrames = (sqlFieldIndex(conn2, geneTable, "exonFrames") == hasBin + 14);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    int fieldCount = hasBin + (hasFrames ? 15 : 10);
    struct genePred *gp;
    if (hasFrames)
        {
	gp = genePredExtLoad(row+hasBin, fieldCount);
        // Some tables have an exonFrames column but it's empty...
        if (gp->exonFrames == NULL)
            genePredAddExonFrames(gp);
        }
    else
	{
	gp = genePredLoad(row+hasBin);
	genePredAddExonFrames(gp);
	}
    slAddHead(&gpList, gp);
    }
sqlFreeResult(&sr);
hFreeConn(&conn2);
return gpList;
}

void printSnp125FunctionShared(char *db, struct snp125 *snp, struct slName *geneTracks)
{
struct sqlConnection *conn = hAllocConn(db);
struct slName *gt;
boolean first = TRUE;
for (gt = geneTracks;  gt != NULL;  gt = gt->next)
    if (sqlTableExists(conn, gt->name))
	{
	if (first)
	    {
	    printf("<BR><B>UCSC's predicted function relative to selected gene tracks:</B>\n");
	    printf("<TABLE border=0 cellspacing=0 cellpadding=0>\n");
	    }
	struct genePred *geneList = getGPsWithFrames(conn, gt->name, snp->chrom,
						     snp->chromStart, snp->chromEnd);
	struct genePred *gene;
	char query[256];
	char buf[256];
	sqlSafef(query, sizeof(query), "select shortLabel from trackDb where tableName='%s'",
	      gt->name);
	char *shortLabel = sqlQuickQuery(conn, query, buf, sizeof(buf)-1);
	if (shortLabel == NULL) shortLabel = gt->name;
	for (gene = geneList;  gene != NULL;  gene = gene->next)
	    printSnp125FunctionInGene(db, snp, gt->name, shortLabel, gene);
	if (geneList == NULL)
	    printSnp125NearGenes(conn, snp, gt->name, shortLabel);
	first = FALSE;
	}
if (! first)
    printf("</TABLE>\n");
hFreeConn(&conn);
}

void printSnp125Function(struct trackDb *tdb, struct snp125 *snp)
/* If the user has selected a gene track for functional annotation,
 * report how this SNP relates to any nearby genes. */
{
char varName[512];
safef(varName, sizeof(varName), "%s_geneTrack", tdb->track);
struct slName *geneTracks = cartOptionalSlNameList(cart, varName);
if (geneTracks == NULL && !cartListVarExists(cart, varName))
    {
    char *defaultGeneTracks = trackDbSetting(tdb, "defaultGeneTracks");
    if (isNotEmpty(defaultGeneTracks))
	geneTracks = slNameListFromComma(defaultGeneTracks);
    else
	return;
    }

char *db = database;
char *liftDb = cloneString(trackDbSetting(tdb, "quickLiftDb"));
if (liftDb != NULL)
    db = liftDb;
printSnp125FunctionShared(db, snp, geneTracks);
}

void printSnp153Function(struct trackDb *tdb, struct snp125 *snp)
/* If the user has selected a gene track for functional annotation,
 * report how this SNP relates to any nearby genes. */
{

struct slName *geneTracks = NULL;

struct trackDb *correctTdb = tdbOrAncestorByName(tdb, tdb->track);

struct slName *defaultGeneTracks = slNameListFromComma(trackDbSetting(tdb, "defaultGeneTracks"));

char *db = database;
char *liftDb = cloneString(trackDbSetting(tdb, "quickLiftDb"));
if (liftDb != NULL)
    db = liftDb;
struct trackDb *geneTdbList = snp125FetchGeneTracks(db, cart);
struct trackDb *gTdb;
for (gTdb = geneTdbList; gTdb; gTdb=gTdb->next)
    {
    char *trackName = gTdb->track;
    char suffix[512];
    safef(suffix, sizeof(suffix), "geneTrack.%s", trackName);
    boolean option = cartUsualBooleanClosestToHome(cart, correctTdb, FALSE, suffix, slNameInList(defaultGeneTracks,trackName));
    if (option)
        {
        slNameAddHead(&geneTracks, trackName);
        }
    }

if (geneTracks)
    printSnp125FunctionShared(db, snp, geneTracks);
}

char *dbSnpFuncFromInt(unsigned char funcCode)
/* Translate an integer function code from NCBI into an abbreviated description.
 * Do not free return value! */
// Might be a good idea to flesh this out with all codes, libify, and share with
// snpNcbiToUcsc instead of partially duplicating.
{
switch (funcCode)
    {
    case 3:
	return "coding-synon";
    case 8:
	return "cds-reference";
    case 41:
	return "nonsense";
    case 42:
	return "missense";
    case 43:
	return "stop-loss";
    case 44:
	return "frameshift";
    case 45:
	return "cds-indel";
    default:
	{
	static char buf[16];
	safef(buf, sizeof(buf), "%d", funcCode);
	return buf;
	}
    }

}

void printSnp125CodingAnnotations(char *db, struct trackDb *tdb, struct snp125 *snp)
/* If tdb specifies extra table(s) that contain protein-coding annotations,
 * show the effects of SNP on transcript coding sequences. */
{
char *tables = trackDbSetting(tdb, "codingAnnotations");
if (isEmpty(tables))
    return;
struct sqlConnection *conn = hAllocConn(db);
struct slName *tbl, *tableList = slNameListFromString(tables, ',');
struct dyString *query = dyStringNew(0);
for (tbl = tableList;  tbl != NULL;  tbl = tbl->next)
    {
    if (!sqlTableExists(conn, tbl->name))
	continue;
    char setting[512];
    safef(setting, sizeof(setting), "codingAnnoLabel_%s", tbl->name);
    char *label = trackDbSettingOrDefault(tdb, setting, NULL);
    if (label == NULL && endsWith(tbl->name, "DbSnp"))
	label = "dbSNP";
    else
	label = tbl->name;
    boolean hasBin = hIsBinned(database, tbl->name);
    boolean hasCoords = (sqlFieldIndex(conn, tbl->name, "chrom") != -1);
    int rowOffset = hasBin + (hasCoords ? 3 : 0);
    dyStringClear(query);
    sqlDyStringPrintf(query, "select * from %s where name = '%s'", tbl->name, snp->name);
    if (hasCoords)
	sqlDyStringPrintf(query, " and chrom = '%s' and chromStart = %d", seqName, snp->chromStart);
    struct sqlResult *sr = sqlGetResult(conn, query->string);
    char **row;
    boolean first = TRUE;
    while ((row = sqlNextRow(sr)) != NULL)
	{
	if (first)
	    {
	    printf("<BR><B>Coding annotations by %s:</B><BR>\n", label);
	    first = FALSE;
	    }
	struct snp125CodingCoordless *anno = snp125CodingCoordlessLoad(row+rowOffset);
	int i;
	boolean gotRef = (anno->funcCodes[0] == 8);
	for (i = 0;  i < anno->alleleCount;  i++)
	    {
	    memSwapChar(anno->peptides[i], strlen(anno->peptides[i]), 'X', '*');
	    if (anno->funcCodes[i] == 8)
		continue;
	    char *txName = anno->transcript;
	    if (startsWith("NM_", anno->transcript))
		txName = getSymbolForGeneName(db, "refGene", anno->transcript);
	    char *func = dbSnpFuncFromInt(anno->funcCodes[i]);
	    printf("%s: %s ", txName, snpMisoLinkFromFunc(func));
	    if (sameString(func, "frameshift") || sameString(func, "cds-indel"))
		{
		puts("<BR>");
		continue;
		}
	    if (gotRef)
		printf("%s (%s) --> ", anno->peptides[0],
		       highlightCodonBase(anno->codons[0], anno->frame));
	    printf("%s (%s)<BR>\n", anno->peptides[i],
		   highlightCodonBase(anno->codons[i], anno->frame));
	    }
	}
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
}

void printSnp132ExtraColumns(struct trackDb *tdb, struct snp132Ext *snp)
/* Print columns new in snp132 */
{
// Skip exceptions column; handled below in writeSnpExceptionWithVersion
printf("<TR><TD><B><A HREF=\"#Submitters\">Submitter Handles</A>&nbsp;&nbsp;</TD><TD></B>");
int i;
for (i=0;  i < snp->submitterCount;  i++)
    printf("%s<A HREF=\"https://www.ncbi.nlm.nih.gov/SNP/snp_viewTable.cgi?h=%s\" TARGET=_BLANK>"
	   "%s</A>", (i > 0 ? ", " : ""), snp->submitters[i], snp->submitters[i]);
printf("</TD></TR>\n");
if (snp->alleleFreqCount > 0)
    {
    boolean gotNonIntN = FALSE;
    double total2NDbl = 0.0;
    for (i = 0;  i < snp->alleleFreqCount;  i++)
        total2NDbl += snp->alleleNs[i];
    int total2N = round(total2NDbl);
    printf("<TR><TD><B><A HREF=\"#AlleleFreq\">Allele Frequencies</A>&nbsp;&nbsp;</B></TD><TD>");
    for (i = 0;  i < snp->alleleFreqCount;  i++)
	{
	printf("%s%s: %.3f%% ", (i > 0 ? "; " : ""), snp->alleles[i], (snp->alleleFreqs[i]*100.0));
	// alleleNs should be integers (counts of chromosomes in which allele was observed)
	// but dbSNP extrapolates them from reported frequency and reported sample count,
	// so sometimes they are not integers.  Present them as integers when we can, warn
	// when we can't.
	double f = snp->alleleFreqs[i], n = snp->alleleNs[i];
	if (f > 0)
	    {
	    int roundedN = round(n);
	    if (fabs(n - roundedN) < 0.01)
		printf("(%d / %d)", roundedN, total2N);
	    else
		{
		gotNonIntN = TRUE;
		printf("(%.3f / %.3f)", n, total2NDbl);
		}
	    }
	}
    printf("</TR></TABLE>\n");
    if (gotNonIntN)
	printf(" <em>Note: dbSNP extrapolates allele counts from reported frequencies and "
	       "reported 2N sample sizes (total %d); non-integer allele count may imply "
	       "an inaccuracy in one of the reported numbers.</em><BR>\n", total2N);
    }
else
    puts("</TABLE>");
if (isNotEmpty(snp->bitfields) && differentString(snp->bitfields, "unknown"))
    {
    printf("<BR><B><A HREF=\"#Bitfields\">Miscellaneous properties annotated by dbSNP</A>:"
	   "</B>\n\n");
    struct slName *bitfields = slNameListFromComma(snp->bitfields);
    if (slNameInList(bitfields, "clinically-assoc"))
	printf("<BR> SNP is in OMIM/OMIA and/or "
	       "at least one submitter is a Locus-Specific Database "
	       "(&quot;clinically associated&quot;)\n");
    if (slNameInList(bitfields, "has-omim-omia"))
	printf("<BR> SNP is in OMIM/OMIA\n");
    if (slNameInList(bitfields, "microattr-tpa"))
	printf("<BR> SNP has a microattribution or third-party annotation\n");
    if (slNameInList(bitfields, "submitted-by-lsdb"))
	printf("<BR> SNP was submitted by Locus-Specific Database\n");
    if (slNameInList(bitfields, "maf-5-all-pops"))
	printf("<BR> Minor Allele Frequency is at least 5%% in all "
	       "populations assayed\n");
    else if (slNameInList(bitfields, "maf-5-some-pop"))
	printf("<BR> Minor Allele Frequency is at least 5%% in at least one "
	       "population assayed\n");
    if (slNameInList(bitfields, "genotype-conflict"))
	printf("<BR> Quality check: Different genotypes have been submitted "
	       "for the same individual\n");
    if (slNameInList(bitfields, "rs-cluster-nonoverlapping-alleles"))
	printf("<BR> Quality check: The reference SNP cluster contains "
	       "submitted SNPs with non-overlapping alleles\n");
    if (slNameInList(bitfields, "observed-mismatch"))
	printf("<BR> Quality check: The reference sequence allele at the "
	       "mapped position is not present in the SNP allele list\n");
    puts("<BR>");
    }
}

// Defined below -- call has since moved up from doSnpWithVersion to printSnp125Info
// so exceptions appear before our coding annotations.
void writeSnpExceptionWithVersion(struct trackDb *tdb, struct snp132Ext *snp, int version);
/* Print out descriptions of exceptions, if any, for this snp. */

void printSnp125Info(struct trackDb *tdb, struct snp132Ext *snp, int version)
/* print info on a snp125 */
{
struct snp125 *snp125 = (struct snp125 *)snp;
printSnpOrthoSummary(tdb, snp->name, snp->observed);
if (differentString(snp->strand,"?"))
    printf("<B>Strand: </B>%s<BR>\n", snp->strand);
printf("<B>Observed: </B>%s<BR>\n", snp->observed);
printSnpAlleleAndOrthos(tdb, snp125, version);
puts("<BR><TABLE border=0 cellspacing=0 cellpadding=0>");
if (version <= 127)
    printf("<TR><TD><B><A HREF=\"#LocType\">Location Type</A></B></TD><TD>%s</TD></TR>\n",
	   snp->locType);
printf("<TR><TD><B><A HREF=\"#Class\">Class</A></B></TD><TD>%s</TD></TR>\n", snp->class);
printf("<TR><TD><B><A HREF=\"#Valid\">Validation</A></B></TD><TD>%s</TD></TR>\n", snp->valid);
printf("<TR><TD><B><A HREF=\"#Func\">Function</A></B></TD><TD>%s</TD></TR>\n",
       snpMisoLinkFromFunc(snp->func));
printf("<TR><TD><B><A HREF=\"#MolType\">Molecule Type</A>&nbsp;&nbsp;</B></TD><TD>%s</TD></TR>\n",
       snp->molType);
if (snp->avHet>0)
    printf("<TR><TD><B><A HREF=\"#AvHet\">Average Heterozygosity</A>&nbsp;&nbsp;</TD>"
	   "<TD></B>%.3f +/- %.3f</TD></TR>\n", snp->avHet, snp->avHetSE);
printf("<TR><TD><B><A HREF=\"#Weight\">Weight</A></B></TD><TD>%d</TD></TR>\n", snp->weight);
if (version >= 132)
    printSnp132ExtraColumns(tdb, snp);
else
    printf("</TABLE>\n");
char *db = database;
char *liftDb = cloneString(trackDbSetting(tdb, "quickLiftDb"));
if (liftDb != NULL)
    db = liftDb;
printSnp125CodingAnnotations(db, tdb, snp125);
writeSnpExceptionWithVersion(tdb, snp, version);
printSnp125Function(tdb, snp125);
}

static char *getExcDescTable(struct trackDb *tdb)
/* Look up snpExceptionDesc in tdb and provide default if not found.  Don't free return value! */
{
static char excDescTable[128];
char *excDescTableSetting = trackDbSetting(tdb, "snpExceptionDesc");
if (excDescTableSetting)
    safecpy(excDescTable, sizeof(excDescTable), excDescTableSetting);
else
    safef(excDescTable, sizeof(excDescTable), "%sExceptionDesc", tdb->table);
return excDescTable;
}

static boolean writeOneSnpException(char *exc, char *desc, boolean alreadyFound)
/* Print out the description of exc, unless exc is already displayed elsewhere. */
{
// Don't bother reporting MultipleAlignments here -- right after this,
// we have a whole section about the other mappings.
if (differentString(exc, "MultipleAlignments") &&
    // Also exclude a couple that are from dbSNP not UCSC, and noted above (bitfields).
    differentString(exc, "GenotypeConflict") &&
    differentString(exc, "ClusterNonOverlappingAlleles"))
    {
    if (isEmpty(desc))
	desc = exc;
    if (!alreadyFound)
	printf("<BR><B>UCSC Annotations:</B><BR>\n");
    printf("%s<BR>\n", desc);
    return TRUE;
    }
return FALSE;
}

void writeSnpExceptionFromTable(struct trackDb *tdb, char *itemName)
/* Print out exceptions, if any, for this snp. */
{
char *exceptionsTableSetting = trackDbSetting(tdb, "snpExceptions");
char exceptionsTable[128];
if (exceptionsTableSetting)
    safecpy(exceptionsTable, sizeof(exceptionsTable), exceptionsTableSetting);
else
    safef(exceptionsTable, sizeof(exceptionsTable), "%sExceptions", tdb->table);
char *excDescTable = getExcDescTable(tdb);
if (hTableExists(database, exceptionsTable) && hTableExists(database, excDescTable))
    {
    struct sqlConnection *conn = hAllocConn(database);
    struct sqlResult *sr;
    char **row;
    char   query[1024];
    int    start = cartInt(cart, "o");
    sqlSafef(query, sizeof(query),
	  "select description, %s.exception from %s, %s "
	  "where chrom = \"%s\" and chromStart = %d and name = \"%s\" "
	  "and %s.exception = %s.exception",
	  excDescTable, excDescTable, exceptionsTable,
	  seqName, start, itemName, excDescTable, exceptionsTable);
    sr = sqlGetResult(conn, query);
    boolean gotExc = FALSE;
    while ((row = sqlNextRow(sr))!=NULL)
	gotExc |= writeOneSnpException(row[1], row[0], gotExc);
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }
}

static void writeSnpExceptionFromColumn(struct trackDb *tdb, struct snp132Ext *snp)
/* Hash the contents of exception description table, and for each exception listed
 * in snp->exceptions, print out its description. */
{
char *excDescTable = getExcDescTable(tdb);
if (hTableExists(database, excDescTable))
    {
    static struct hash *excDesc = NULL;
    if (excDesc == NULL)
	{
	excDesc = hashNew(0);
	struct sqlConnection *conn = hAllocConn(database);
	char query[512];
	sqlSafef(query, sizeof(query), "select exception,description from %s", excDescTable);
	struct sqlResult *sr = sqlGetResult(conn, query);
	char **row;
	while ((row = sqlNextRow(sr))!=NULL)
	    hashAdd(excDesc, row[0], cloneString(row[1]));
	sqlFreeResult(&sr);
	hFreeConn(&conn);
	}
    struct slName *excList = slNameListFromComma(snp->exceptions), *exc;
    boolean gotExc = FALSE;
    for (exc = excList;  exc != NULL;  exc = exc->next)
	{
	char *desc = hashFindVal(excDesc, exc->name);
	gotExc |= writeOneSnpException(exc->name, desc, gotExc);
	}
    }
}

void writeSnpExceptionWithVersion(struct trackDb *tdb, struct snp132Ext *snp, int version)
/* Print out descriptions of exceptions, if any, for this snp. */
{
if (version >= 132)
    writeSnpExceptionFromColumn(tdb, snp);
else
    writeSnpExceptionFromTable(tdb, snp->name);
}

struct snp *snp125ToSnp(struct snp125 *snp125)
/* Copy over the bed6 plus observed fields. */
{
struct snp *snp;
AllocVar(snp);
snp->chrom = cloneString(snp125->chrom);
snp->chromStart = snp125->chromStart;
snp->chromEnd = snp125->chromEnd;
snp->name = cloneString(snp125->name);
snp->score = snp125->score;
if (sameString(snp125->strand, "+"))
    snp->strand[0] = '+';
else if (sameString(snp125->strand, "-"))
    snp->strand[0] = '-';
else
    snp->strand[0] = '?';
snp->strand[1] = '\0';
snp->observed = cloneString(snp125->observed);
return snp;
}

void checkForHgdpGeo(struct sqlConnection *conn, struct trackDb *tdb, char *itemName, int start)
{
char *hgdpGeoTable = "hgdpGeo"; // make this a trackDb setting
if (!hTableExists(database, hgdpGeoTable))
    return;
struct sqlResult *sr;
char **row;
char query[512];
sqlSafef(query, sizeof(query),
      "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d",
      hgdpGeoTable, itemName, seqName, start);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    struct hgdpGeo geo;
    hgdpGeoStaticLoad(row+1, &geo);
    char title[1024];
    safef(title, sizeof(title), "Human Genome Diversity Project SNP"
	  "<IMG name=\"hgdpImgIcon\" height=40 width=55 class='bigBlue' src=\"%s\">",
	  hgdpPngFilePath(itemName));
    jsBeginCollapsibleSection(cart, tdb->track, "hgdpGeo", title, FALSE);
    printf("Note: These annotations are taken directly from the "
	   "<A HREF=\"http://hgdp.uchicago.edu/\" TARGET=_BLANK>HGDP Selection Browser</A>, "
	   "and may indicate the allele on the opposite strand from that given above.<BR>\n");
    printf("<B>Ancestral Allele:</B> %c<BR>\n", geo.ancestralAllele);
    printf("<B>Derived Allele:</B> %c<BR>\n", geo.derivedAllele);
    printf("<TABLE><TR><TD>\n");
    hgdpGeoFreqTable(&geo);
    printf("</TD><TD valign=top>\n");
    hgdpGeoImg(&geo);
    printf("</TD></TR></TABLE>\n");
    jsEndCollapsibleSection();
    }
sqlFreeResult(&sr);
}

void checkForHapmap(struct sqlConnection *conn, struct trackDb *tdb, char *itemName)
{
boolean isPhaseIII = sameString(trackDbSettingOrDefault(tdb, "hapmapPhase", "II"), "III");
boolean gotHapMap = FALSE;
char query[512];
if (!isPhaseIII && sqlTableExists(conn, "hapmapAllelesSummary"))
    {
    sqlSafef(query, sizeof(query),
	  "select count(*) from hapmapAllelesSummary where name = '%s'", itemName);
    if (sqlQuickNum(conn, query) > 0)
	gotHapMap = TRUE;
    }
else
    {
    int i;
    for (i = 0;  hapmapPhaseIIIPops[i] != NULL;  i++)
	{
	char table[HDB_MAX_TABLE_STRING];
	safef(table, sizeof(table), "hapmapSnps%s", hapmapPhaseIIIPops[i]);
	if (sqlTableExists(conn, table))
	    {
	    sqlSafef(query, sizeof(query),
		  "select count(*) from %s where name = '%s'", table, itemName);
	    if (sqlQuickNum(conn, query) > 0)
		{
		gotHapMap = TRUE;
		break;
		}
	    }
	}
    }
struct trackDb *hsTdb = hashFindVal(trackHash, "hapmapSnps");
if (gotHapMap && hsTdb != NULL)
    {
    printf("<TR><TD colspan=2><B><A HREF=\"%s", hgTracksPathAndSettings());
    // If hapmapSnps is hidden, make it dense; if it's pack etc., leave it alone.
    if (sameString("hide", cartUsualString(cart, "hapmapSnps",
					   trackDbSettingOrDefault(hsTdb, "visibility", "hide"))))
	printf("&hapmapSnps=dense");
    printf("\"> HapMap SNP</A> </B></TD></TR>\n");
    }
}

static void checkForGwasCatalog(struct sqlConnection *conn, struct trackDb *tdb, char *item)
/* If item is in gwasCatalog, add link to make the track visible. */
{
char *gcTable = "gwasCatalog";
if (sqlTableExists(conn, gcTable))
    {
    char query[512];
    sqlSafef(query, sizeof(query), "select count(*) from %s where name = '%s'", gcTable, item);
    if (sqlQuickNum(conn, query) > 0)
	{
	struct trackDb *gcTdb = hashFindVal(trackHash, gcTable);
	if (gcTdb != NULL)
	    {
	    printf("<TR><TD colspan=2>><B><A HREF=\"%s", hgTracksPathAndSettings());
	    // If gcTable is hidden, make it dense; otherwise, leave it alone.
	    if (sameString("hide",
			   cartUsualString(cart, gcTable,
					   trackDbSettingOrDefault(gcTdb, "visibility", "hide"))))
		printf("&%s=dense", gcTable);
	    printf("\">%s SNP</A> </B></TD></TR>\n", gcTdb->shortLabel);
	    }
	}
    }
}

static void checkForMupit(struct sqlConnection *conn, struct trackDb *tdb, int start)
/* Print a link to MuPIT if the item is in the mupitRanges table */
{
if (sqlTableExists(conn, "mupitRanges"))
    {
    struct sqlResult *sr = hRangeQuery(conn, "mupitRanges", seqName, start, start+1, NULL, NULL);
    char **row = NULL;
    if ((row = sqlNextRow(sr)) != NULL)
        {
        int mupitPosition = start + 1; // mupit uses 1-based coords
        printf("<TR><TD colspan=2><B>");
        if (sameString(database, "hg19"))
            printf("<A HREF=\"http://hg19.cravat.us/MuPIT_Interactive/?gm=%s:%d\">", seqName, mupitPosition);
        else if (sameString(database, "hg38"))
            printf("<A HREF=\"http://mupit.icm.jhu.edu/MuPIT_Interactive/?gm=%s:%d\">", seqName, mupitPosition);
        printf("MuPIT Structure</A></B></TD></TR>\n");
        }
    }
}

void printOtherSnpMappings(char *table, char *name, int start,
			   struct sqlConnection *conn, int rowOffset, struct trackDb *tdb)
/* If this SNP (from any bed4+ table) is not uniquely mapped, print the other mappings. */
{
char query[512];
sqlSafef(query, sizeof(query), "select * from %s where name='%s'",
      table, name);
struct sqlResult *sr = sqlGetResult(conn, query);
int snpCount = 0;
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct bed *snp = bedLoad3(row + rowOffset);
    if (snp->chromStart != start || differentString(snp->chrom, seqName))
	{
	printf("<BR>\n");
	if (snpCount == 0)
	    printf("<B>This SNP maps to these additional locations:</B><BR><BR>\n");
	snpCount++;
	bedPrintPos((struct bed *)snp, 3, NULL);
	}
    }
sqlFreeResult(&sr);
}

void doSnpWithVersion(struct trackDb *tdb, char *itemName, int version)
/* Process SNP details. */
{
char   *table = tdb->table;
struct snp132Ext *snp;
struct snp *snpAlign = NULL;
int    start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char   query[512];
int    rowOffset=hOffsetPastBin(database, seqName, table);

genericHeader(tdb, NULL);
printf("<H2>dbSNP build %d %s</H2>\n", version, itemName);
sqlSafef(query, sizeof(query), "select * from %s where chrom='%s' and "
      "chromStart=%d and name='%s'", table, seqName, start, itemName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    if (version >= 132)
	snp = snp132ExtLoad(row+rowOffset);
    else
	snp = (struct snp132Ext *)snp125Load(row+rowOffset);
    printCustomUrl(tdb, itemName, FALSE);
    bedPrintPos((struct bed *)snp, 3, tdb);
    snpAlign = snp125ToSnp((struct snp125 *)snp);
    printf("<BR>\n");
    printSnp125Info(tdb, snp, version);
    doSnpEntrezGeneLink(tdb, itemName);
    }
else
    errAbort("SNP %s not found at %s base %d", itemName, seqName, start);
sqlFreeResult(&sr);

printOtherSnpMappings(table, itemName, start, conn, rowOffset, tdb);
puts("<BR>");
// Make table for collapsible sections:
puts("<TABLE>");
checkForGwasCatalog(conn, tdb, itemName);
checkForHgdpGeo(conn, tdb, itemName, start);
checkForHapmap(conn, tdb, itemName);
checkForMupit(conn, tdb, start);
printSnpAlignment(tdb, snpAlign, version);
puts("</TABLE>");
printTrackHtml(tdb);
hFreeConn(&conn);
}


void doTigrGeneIndex(struct trackDb *tdb, char *item)
/* Put up info on tigr gene index item. */
{
char *animal = cloneString(item);
char *id = strchr(animal, '_');
char buf[128];

if (id == NULL)
    {
    animal = "human";
    id = item;
    }
else
    *id++ = 0;
if (sameString(animal, "cow"))
    animal = "cattle";
else if (sameString(animal, "chicken"))
    animal = "g_gallus";
else if (sameString(animal, "Dmelano"))
    animal = "drosoph";

safef(buf, sizeof buf, "species=%s&tc=%s ", animal, id);
genericClickHandler(tdb, item, buf);
}

void doJaxQTL(struct trackDb *tdb, char *item)
/* Put up info on Quantitative Trait Locus from Jackson Lab. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char query[512];
char **row;
int start = cartInt(cart, "o");
boolean isBed4 = startsWith("bed 4", tdb->type);
boolean hasBin = hIsBinned(database, tdb->table);

genericHeader(tdb, item);
sqlSafef(query, sizeof(query),
      "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d",
      tdb->table, item, seqName, start);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    char *itemForUrl=NULL, *name=NULL, *description=NULL, *marker=NULL;
    float cMscore = 0.0;
    struct bed *bed = bedLoadN(row+hasBin, 4);
    if (isBed4)
	{
	char *oDb = trackDbSetting(tdb, "otherDb");
	char *oTable = trackDbSetting(tdb, "otherDbTable");
	itemForUrl = name = bed->name;
	if (isNotEmpty(oDb) && isNotEmpty(oTable))
	    {
	    struct sqlConnection *conn2 = hAllocConn(database);
	    char buf[1024];
	    sqlSafef(query, sizeof(query),
		  "select description from %s.%s where name = '%s'",
		  oDb, oTable, name);
            description =
		cloneString(sqlQuickQuery(conn2, query, buf, sizeof(buf)-1));
	    sqlSafef(query, sizeof(query),
		  "select mgiID from %s.%s where name = '%s'",
		  oDb, oTable, name);
            itemForUrl =
		cloneString(sqlQuickQuery(conn2, query, buf, sizeof(buf)-1));
	    }
	}
    else
	{
	struct jaxQTL *jaxQTL = jaxQTLLoad(row);
	itemForUrl = jaxQTL->mgiID;
	name = jaxQTL->name;
	description = jaxQTL->description;
	cMscore = jaxQTL->cMscore;
	marker = jaxQTL->marker;
	}
    printCustomUrl(tdb, itemForUrl, FALSE);
    printf("<B>QTL:</B> %s<BR>\n", name);
    if (isNotEmpty(description))
	printf("<B>Description:</B> %s <BR>\n", description);
    if (cMscore != 0.0)
	printf("<B>cM position of marker associated with peak LOD score:</B> "
	       "%3.1f<BR>\n", cMscore);
    if (isNotEmpty(marker))
	printf("<B>MIT SSLP marker with highest correlation:</B> %s<BR>",
	       marker);
    bedPrintPos(bed, 3, tdb);
    }
printTrackHtml(tdb);

sqlFreeResult(&sr);
hFreeConn(&conn);
}

#define GWAS_NOT_REPORTED "<em>Not reported</em>"
#define GWAS_NONE_SIGNIFICANT "<em>None significant</em>"

static char *subNrNs(char *str)
/* The GWAS catalog has "NR" or "NS" for many values -- substitute those with something
 * more readable.  Don't free return value, it might be static. */
{
if (isEmpty(str) || sameString("NR", str))
    return GWAS_NOT_REPORTED;
if (sameString("NS", str))
    return GWAS_NONE_SIGNIFICANT;
struct dyString *dy1 = dyStringSub(str, "[NR]", "[" GWAS_NOT_REPORTED "]");
struct dyString *dy2 = dyStringSub(dy1->string, "[NS]", "[" GWAS_NONE_SIGNIFICANT "]");
return dyStringCannibalize(&dy2);
}

static boolean isSnpAndAllele(char *str)
/* Return TRUE if str ~ /^rs[0-9]+-.+/ . */
{
if (isEmpty(str) || !startsWith("rs", str))
    return FALSE;
char *p = str + 2;
if (! isdigit(*p++))
    return FALSE;
while (isdigit(*p))
    p++;
if (*p++ != '-')
    return FALSE;
if (*p == '\0')
    return FALSE;
return TRUE;
}

static char *splitSnpAndAllele(char *str, char **retAllele)
/* If str is a rsID+allele, return the rsID and if retAllele is non-null, set it
 * to the allele portion.  Don't free *retAllele.  If str is not rsID+allele,
 * return NULL. */
{
if (isSnpAndAllele(str))
    {
    char *rsID = cloneString(str);
    char *allele = strchr(rsID, '-');
    if (allele == NULL)
	errAbort("splitSnpAndAllele: isSnpAllele() allowed %s", str);
    *allele++ = '\0';
    if (retAllele != NULL)
	*retAllele = firstWordInLine(allele);
    return rsID;
    }
else
    {
    if (retAllele != NULL)
	*retAllele = NULL;
    return NULL;
    }
}

static char *getSnpAlleles(struct sqlConnection *conn, char *snpTable, char *snpName)
/* Look up snpName's observed alleles in snpTable.  Returns NULL if not found. */
{
char query[512];
char buf[256]; // varchar(255)
sqlSafef(query, sizeof(query), "select observed from %s where name = '%s'", snpTable, snpName);
return cloneString(sqlQuickQuery(conn, query, buf, sizeof(buf)-1));
}

static void gwasCatalogCheckSnpAlleles(struct trackDb *tdb, struct gwasCatalog *gc)
/* Look up the SNP's observed alleles in the snp track and warn if they are
 * complementary (hence the risk allele is ambiguous because strand is often
 * not specified in journal articles). */
{
char *snpTable = trackDbSetting(tdb, "snpTable");
if (isEmpty(snpTable))
    return;
struct sqlConnection *conn = hAllocConn(database);
if (sqlTableExists(conn, snpTable) && isSnpAndAllele(gc->riskAllele))
    {
    char *riskAllele = NULL, *strongSNP = splitSnpAndAllele(gc->riskAllele, &riskAllele);
    char *snpVersion = trackDbSettingOrDefault(tdb, "snpVersion", "?");
    char *dbSnpAlleles = getSnpAlleles(conn, snpTable, strongSNP);
    if (dbSnpAlleles == NULL)
	dbSnpAlleles = "<em>not found</em>";
    boolean showBoth = differentString(strongSNP, gc->name);
    printf("<B>dbSNP build %s observed alleles for %s%s:</B> %s<BR>\n",
	   snpVersion, (showBoth ? "Strongest SNP " : ""), strongSNP, dbSnpAlleles);
    if (stringIn("C/G", dbSnpAlleles) || stringIn("A/T", dbSnpAlleles))
	printf("<em>Note: when SNP alleles are complementary (A/T or C/G), take care to "
	       "determine the strand/orientation of the given risk allele from the "
	       "original publication (above).</em><BR>\n");
    if (showBoth)
	{
	dbSnpAlleles = getSnpAlleles(conn, snpTable, gc->name);
	if (dbSnpAlleles == NULL)
	    dbSnpAlleles = "<em>not found</em>";
	printf("<B>dbSNP build %s observed alleles for mapped SNP %s:</B> %s<BR>\n",
	       snpVersion, gc->name, dbSnpAlleles);
	}
    }
hFreeConn(&conn);
}

void doGwasCatalog(struct trackDb *tdb, char *item)
/* Show details from NHGRI's Genome-Wide Association Study catalog. */
{
int itemStart = cartInt(cart, "o"), itemEnd = cartInt(cart, "t");
genericHeader(tdb, item);
struct sqlConnection *conn = hAllocConn(database);
struct dyString *dy = dyStringNew(512);
sqlDyStringPrintf(dy, "select * from %s where chrom = '%s' and ", tdb->table, seqName);
hAddBinToQuery(itemStart, itemEnd, dy);
sqlDyStringPrintf(dy, "chromStart = %d and name = '%s'", itemStart, item);
struct sqlResult *sr = sqlGetResult(conn, dy->string);
int rowOffset = hOffsetPastBin(database, seqName, tdb->table);
boolean first = TRUE;
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (first)
	first = FALSE;
    else
	printf("<HR>\n");
    struct gwasCatalog *gc = gwasCatalogLoad(row+rowOffset);
    printCustomUrl(tdb, item, FALSE);
    printPos(gc->chrom, gc->chromStart, gc->chromEnd, NULL, TRUE, gc->name);
    printf("<B>Reported region:</B> %s<BR>\n", gc->region);
    printf("<B>Publication:</B> %s <em>et al.</em> "
	   "<A HREF=\"", gc->author);
    printEntrezPubMedUidAbstractUrl(stdout, gc->pubMedID);
    printf("\" TARGET=_BLANK>%s</A>%s <em>%s.</em> %s<BR>\n",
	   gc->title, (endsWith(gc->title, ".") ? "" : "."), gc->journal, gc->pubDate);
    printf("<B>Disease or trait:</B> %s<BR>\n", subNrNs(gc->trait));
    printf("<B>Initial sample size:</B> %s<BR>\n", subNrNs(gc->initSample));
    printf("<B>Replication sample size:</B> %s<BR>\n", subNrNs(gc->replSample));
    printf("<B>Reported gene(s):</B> %s<BR>\n", subNrNs(gc->genes));
    char *strongAllele = NULL, *strongRsID = splitSnpAndAllele(gc->riskAllele, &strongAllele);
    if (strongRsID)
	{
	printf("<B>Strongest SNP-Risk allele:</B> ");
	printDbSnpRsUrl(strongRsID, "%s", strongRsID);
	printf("-%s<BR>\n", strongAllele);
	}
    else
	printf("<B>Strongest SNP-Risk allele:</B> %s<BR>\n", subNrNs(gc->riskAllele));
    gwasCatalogCheckSnpAlleles(tdb, gc);
    printf("<B>Risk Allele Frequency:</B> %s<BR>\n", subNrNs(gc->riskAlFreq));
    if (isEmpty(gc->pValueDesc) || sameString(gc->pValueDesc, "NS"))
	printf("<B>p-Value:</B> %s<BR>\n", subNrNs(gc->pValue));
    else if (gc->pValueDesc[0] == '(')
	printf("<B>p-Value:</B> %s %s<BR>\n", gc->pValue, subNrNs(gc->pValueDesc));
    else
	printf("<B>p-Value:</B> %s (%s)<BR>\n", gc->pValue, subNrNs(gc->pValueDesc));
    printf("<B>Odds Ratio or beta:</B> %s<BR>\n", subNrNs(gc->orOrBeta));
    printf("<B>95%% confidence interval:</B> %s<BR>\n", subNrNs(gc->ci95));
    printf("<B>Platform:</B> %s<BR>\n", subNrNs(gc->platform));
    printf("<B>Copy Number Variant (CNV)?:</B> %s<BR>\n",
	   (gc->cnv == gwasCatalogY ? "Yes" : "No"));
    }
sqlFreeResult(&sr);
printTrackHtml(tdb);
}

void ncRnaPrintPos(struct bed *bed, int bedSize)
/* Print first two fields of an ncRna entry in
 * standard format. */
{
char *strand = NULL;
if (bedSize >= 4)
    printf("<B>Item:</B> %s<BR>\n", bed->name);
if (bedSize >= 6)
   {
   strand = bed->strand;
   }
printPos(bed->chrom, bed->chromStart, bed->chromEnd, strand, TRUE, bed->name);
}

void doNcRna(struct trackDb *tdb, char *item)
/* Handle click in ncRna track. */
{
struct ncRna *ncRna;
char table[HDB_MAX_TABLE_STRING];
boolean hasBin;
struct bed *bed;
char query[512];
struct sqlResult *sr;
char **row;
struct sqlConnection *conn = hAllocConn(database);
int bedSize;

genericHeader(tdb, item);
bedSize = 8;
if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, &hasBin))
    errAbort("track %s not found", tdb->table);
sqlSafef(query, sizeof query, "select * from %s where name = '%s'", table, item);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    ncRna = ncRnaLoad(row);
    printCustomUrl(tdb, item, TRUE);
    printf("<B>Type:</B> %s<BR>", ncRna->type);
    if (ncRna->extGeneId != NULL
    &&  !sameWord(ncRna->extGeneId, ""))
        {
        printf("<B>External Gene ID:</B> %s<BR>", ncRna->extGeneId);
        }
    bed = bedLoadN(row+hasBin, bedSize);
    ncRnaPrintPos(bed, bedSize);
    }
sqlFreeResult(&sr);
printTrackHtml(tdb);
}

void doWgRna(struct trackDb *tdb, char *item)
/* Handle click in wgRna track. */
{
struct wgRna *wgRna;
char table[HDB_MAX_TABLE_STRING];
boolean hasBin;
struct bed *bed;
char query[512];
struct sqlResult *sr;
char **row;
struct sqlConnection *conn = hAllocConn(database);
int bedSize;

genericHeader(tdb, item);
bedSize = 8;
if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, &hasBin))
    errAbort("track %s not found", tdb->table);
sqlSafef(query, sizeof query, "select * from %s where name = '%s'", table, item);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    wgRna = wgRnaLoad(row);

    /* display appropriate RNA type and URL */
    if (sameWord(wgRna->type, "HAcaBox"))
        {
        printCustomUrl(tdb, item, TRUE);
        printf("<B>RNA Type:</B> H/ACA Box snoRNA\n");
	}
    if (sameWord(wgRna->type, "CDBox"))
        {
	printCustomUrl(tdb, item, TRUE);
        printf("<B>RNA Type:</B> CD Box snoRNA\n");
	}
    if (sameWord(wgRna->type, "scaRna"))
        {
	printCustomUrl(tdb, item, TRUE);
        printf("<B>RNA Type:</B> small Cajal body-specific RNA\n");
	}
    if (sameWord(wgRna->type, "miRna"))
        {
	printOtherCustomUrl(tdb, item, "url2", TRUE);
	printf("<B>RNA Type:</B> microRNA\n");
	}
    printf("<BR>");
    bed = bedLoadN(row+hasBin, bedSize);
    bedPrintPos(bed, bedSize, tdb);
    }
sqlFreeResult(&sr);
printTrackHtml(tdb);
}

void doJaxQTL3(struct trackDb *tdb, char *item)
/* Put up info on Quantitative Trait Locus from Jackson Lab. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char query[256];
char **row;
int start = cartInt(cart, "o");
struct jaxQTL3 *jaxQTL;

genericHeader(tdb, item);
sqlSafef(query, sizeof query, "select * from jaxQTL3 where name = '%s' and chrom = '%s' and chromStart = %d",
        item, seqName, start);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    jaxQTL = jaxQTL3Load(row);
    printf("<B>Jax/MGI Link: </B>");
    printf("<a TARGET=\"_blank\" href=\"http://www.informatics.jax.org/marker/%s\">%s</a><BR>\n",
           jaxQTL->mgiID, jaxQTL->mgiID);
    printf("<B>QTL:</B> %s<BR>\n", jaxQTL->name);
    printf("<B>Description:</B> %s <BR>\n", jaxQTL->description);

    if (!sameWord("", jaxQTL->flank1))
        {
        printf("<B>Flank Marker 1: </B>");
	printf("<a TARGET=\"_blank\" href=\"http://www.informatics.jax.org/javawi2/servlet/WIFetch?page=searchTool&query=%s", jaxQTL->flank1);
	printf("+&selectedQuery=Genes+and+Markers\">%s</a><BR>\n", jaxQTL->flank1);
        }
    if (!sameWord("", jaxQTL->marker))
        {
	printf("<B>Peak Marker: </B>");
	printf("<a TARGET=\"_blank\" href=\"http://www.informatics.jax.org/javawi2/servlet/WIFetch?page=searchTool&query=%s", jaxQTL->marker);
	printf("+&selectedQuery=Genes+and+Markers\">%s</a><BR>\n", jaxQTL->marker);
        }
    if (!sameWord("", jaxQTL->flank2))
        {
	printf("<B>Flank Marker 2: </B>");
	printf("<a TARGET=\"_blank\" href=\"http://www.informatics.jax.org/javawi2/servlet/WIFetch?page=searchTool&query=%s", jaxQTL->flank2);
        printf("+&selectedQuery=Genes+and+Markers\">%s</a><BR>\n", jaxQTL->flank2);
        }

    /* no cMscore for current release*/
    /*printf("<B>cM position of marker associated with peak LOD score:</B> %3.1f<BR>\n",
      jaxQTL->cMscore);
    */

    printf("<B>Chromosome:</B> %s<BR>\n", skipChr(seqName));
    printBand(seqName, start, 0, FALSE);
    }
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doJaxAllele(struct trackDb *tdb, char *item)
/* Show gene prediction position and other info. */
{
char query[512];
struct sqlConnection *conn = hAllocConn(database);
struct sqlConnection *conn2 = hAllocConn(database);
boolean hasBin;
char aliasTable[256], phenoTable[256];
struct sqlResult *sr = NULL;
char **row = NULL;
boolean first = TRUE;

genericHeader(tdb, item);
safef(aliasTable, sizeof(aliasTable), "%sInfo", tdb->table);
safef(phenoTable, sizeof(phenoTable), "jaxAllelePheno");
sqlSafef(query, sizeof(query), "name = \"%s\"", item);
sr = hRangeQuery(conn, tdb->table, seqName, winStart, winEnd, query, &hasBin);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct bed *bed = bedLoadN(row+hasBin, 12);
    /* Watch out for case-insensitive matches (e.g. one allele is <sla>,
     * another is <Sla>): */
    if (! sameString(bed->name, item))
	continue;
    if (first)
	first = FALSE;
    else
	printf("<BR>");
    printf("<B>MGI Representative Transcript:</B> ");
    htmTextOut(stdout, bed->name);
    puts("<BR>");
    if (hTableExists(database, aliasTable))
	{
	struct sqlResult *sr2 = NULL;
	char **row2 = NULL;
	char query2[1024];
	sqlSafef(query2, sizeof(query2),
	      "select mgiId,source,name from %s where name = '%s'",
	      aliasTable, bed->name);
	sr2 = sqlGetResult(conn2, query2);
	while ((row2 = sqlNextRow(sr2)) != NULL)
	    {
	    /* Watch out for case-insensitive matches: */
	    if (! sameString(bed->name, row2[2]))
		continue;
	    if (isNotEmpty(row2[0]))
		printCustomUrl(tdb, row2[0], TRUE);
	    printf("<B>Allele Type:</B> %s<BR>\n", row2[1]);
	    }
	sqlFreeResult(&sr2);
	}
    if (hTableExists(database, phenoTable))
	{
	struct sqlResult *sr2 = NULL;
	char **row2 = NULL;
	char query2[1024];
	struct slName *phenoList, *pheno;
	sqlSafef(query2, sizeof(query2),
	      "select phenotypes,allele from %s where allele = '%s'",
	      phenoTable, bed->name);
	sr2 = sqlGetResult(conn2, query2);
	while ((row2 = sqlNextRow(sr2)) != NULL)
	    {
	    /* Watch out for case-insensitive matches: */
	    if (! sameString(bed->name, row2[1]))
		continue;
	    boolean firstP = TRUE;
	    phenoList = slNameListFromComma(row2[0]);
	    slNameSort(&phenoList);
	    printf("<B>Associated Phenotype(s):</B> ");
	    for (pheno = phenoList;  pheno != NULL;  pheno = pheno->next)
		{
		if (firstP)
		    firstP = FALSE;
		else
		    printf(", ");
		printf("%s", pheno->name);
		}
	    printf("<BR>\n");
	    }
	sqlFreeResult(&sr2);
	}
    printPos(bed->chrom, bed->chromStart, bed->chromEnd, bed->strand,
	     FALSE, NULL);
    bedFree(&bed);
    }
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn2);
hFreeConn(&conn);
}

void doJaxPhenotype(struct trackDb *tdb, char *item)
/* Show gene prediction position and other info. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row = NULL;
boolean hasBin;
char query[512];
char aliasTable[256], phenoTable[256];
struct slName *phenoList = NULL, *pheno = NULL;
boolean first = TRUE;
char *selectedPheno = NULL;

/* Parse out the selected phenotype passed in from hgTracks. */
if ((selectedPheno = strstr(item, " source=")) != NULL)
    {
    *selectedPheno = '\0';
    selectedPheno += strlen(" source=");
    }
genericHeader(tdb, item);
safef(aliasTable, sizeof(aliasTable), "%sAlias", tdb->table);
safef(phenoTable, sizeof(phenoTable), "jaxAllelePheno");
sqlSafef(query, sizeof(query), "name = \"%s\"", item);
sr = hRangeQuery(conn, tdb->table, seqName, winStart, winEnd, query,
		 &hasBin);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct bed *bed = bedLoadN(row+hasBin, 12);
    if (first)
	{
	first = FALSE;
	printf("<B>MGI Representative Transcript:</B> ");
	htmTextOut(stdout, bed->name);
	puts("<BR>");
	if (hTableExists(database, aliasTable))
	    {
	    struct sqlConnection *conn2 = hAllocConn(database);
	    char query2[512];
	    char buf[512];
	    char *mgiId;
	    sqlSafef(query2, sizeof(query2),
		  "select alias from %s where name = '%s'", aliasTable, item);
	    mgiId = sqlQuickQuery(conn2, query2, buf, sizeof(buf));
	    if (mgiId != NULL)
		printCustomUrl(tdb, mgiId, TRUE);
	    hFreeConn(&conn2);
	    }
	printPos(bed->chrom, bed->chromStart, bed->chromEnd, bed->strand,
		 FALSE, NULL);
	bedFree(&bed);
	}
    pheno = slNameNew(row[hasBin+12]);
    slAddHead(&phenoList, pheno);
    }
sqlFreeResult(&sr);
printf("<B>Phenotype(s) at this locus: </B> ");
first = TRUE;
slNameSort(&phenoList);
for (pheno = phenoList;  pheno != NULL;  pheno = pheno->next)
    {
    if (first)
	first = FALSE;
    else
	printf(", ");
    if (selectedPheno && sameString(pheno->name, selectedPheno))
	printf("<B>%s</B>", pheno->name);
    else
	printf("%s", pheno->name);
    }
puts("<BR>");
if (hTableExists(database, phenoTable) && selectedPheno)
    {
    struct trackDb *alleleTdb = hMaybeTrackInfo(conn, "jaxAllele");
    struct sqlConnection *conn2 = hAllocConn(database);
    char query2[512];
    char buf[512];
    char alleleTable[256];
    safef(alleleTable, sizeof(alleleTable), "jaxAlleleInfo");
    boolean gotAllele = hTableExists(database, alleleTable);
    sqlSafef(query, sizeof(query),
	  "select allele from %s where transcript = '%s' "
	  "and phenotypes like '%%%s%%'",
	  phenoTable, item, selectedPheno);
    first = TRUE;
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	char *mgiId = NULL;
	if (first)
	    {
	    first = FALSE;
	    printf("<B>Allele(s) Associated with %s Phenotype:</B> ",
		   selectedPheno);
	    }
	else
	    printf(", ");
	if (gotAllele)
	    {
	    sqlSafef(query2, sizeof(query2),
		  "select mgiID from jaxAlleleInfo where name = '%s'",
		  row[0]);
	    mgiId = sqlQuickQuery(conn2, query2, buf, sizeof(buf));
	    }
	if (mgiId && alleleTdb && alleleTdb->url)
	    {
	    struct dyString *dy = dyStringSub(alleleTdb->url, "$$", mgiId);
	    printf("<A HREF=\"%s\" TARGET=_BLANK>", dy->string);
	    dyStringFree(&dy);
	    }
	htmTextOut(stdout, row[0]);
	if (mgiId && alleleTdb && alleleTdb->url)
	    printf("</A>");
	}
    sqlFreeResult(&sr);
    hFreeConn(&conn2);
    if (!first)
	puts("<BR>");
    }
printTrackHtml(tdb);
hFreeConn(&conn);
}

void doJaxAliasGenePred(struct trackDb *tdb, char *item)
/* Show gene prediction position and other info. */
{
char query[512];
struct sqlConnection *conn = hAllocConn(database);
struct sqlConnection *conn2 = hAllocConn(database);
struct genePred *gpList = NULL, *gp = NULL;
boolean hasBin;
char table[HDB_MAX_TABLE_STRING];
char aliasTable[256];
boolean gotAlias = FALSE;

genericHeader(tdb, item);
safef(aliasTable, sizeof(aliasTable), "%sAlias", tdb->table);
gotAlias = hTableExists(database, aliasTable);
if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, &hasBin))
    errAbort("track %s not found", tdb->table);
sqlSafef(query, sizeof(query), "name = \"%s\"", item);
gpList = genePredReaderLoadQuery(conn, table, query);
for (gp = gpList; gp != NULL; gp = gp->next)
    {
    if (gotAlias)
	{
	char query2[1024];
	char buf[512];
	char *mgiId;
	sqlSafef(query2, sizeof(query2),
	      "select alias from %s where name = '%s'", aliasTable, item);
	mgiId = sqlQuickQuery(conn2, query2, buf, sizeof(buf));
	if (mgiId != NULL)
	    printCustomUrl(tdb, mgiId, TRUE);
	}
    printPos(gp->chrom, gp->txStart, gp->txEnd, gp->strand, FALSE, NULL);
    if (gp->next != NULL)
        printf("<br>");
    }
printTrackHtml(tdb);
genePredFreeList(&gpList);
hFreeConn(&conn2);
hFreeConn(&conn);
}


void doEncodeRegion(struct trackDb *tdb, char *item)
/* Print region desription, along with generic info */
{
char *descr;
char *plus = NULL;
char buf[128];
if ((descr = getEncodeRegionDescr(item)) != NULL)
    {
    safef(buf, sizeof(buf), "<B>Description:</B> %s<BR>\n", descr);
    plus = buf;
    }
genericClickHandlerPlus(tdb, item, NULL, plus);
}

char *getEncodeName(char *item)
/* the item is in the format 'ddddddd/nnn' where the first seven 'd' characters
   are the digits of the identifier, and the variable-length 'n' chatacters
   are the name of the object.  Return the name. */
{
char *dupe=cloneString(item);
return dupe+8;
}

char *getEncodeId(char *item)
/* the item is in the format 'ddddddd/nnn' where the first seven 'd' characters
   are the digits of the identifier, and the variable-length 'n' chatacters
   are the name of the object.  Return the ID portion. */
{
char *id = cloneString(item);
id[7]='\0';
return id;
}

void doEncodeErge(struct trackDb *tdb, char *item)
/* Print ENCODE data from dbERGE II */
{
struct sqlConnection *conn = hAllocConn(database);
char query[1024];
struct encodeErge *ee=NULL;
int start = cartInt(cart, "o");
char *newLabel = tdb->longLabel + 7; /* removes 'ENCODE ' from label */
char *encodeName = getEncodeName(item);
char *encodeId = getEncodeId(item);

cartWebStart(cart, database, "ENCODE Region Data: %s", newLabel);
printf("<H2>ENCODE Region <span style='text-decoration:underline;'>%s</span> Data for %s.</H2>\n",
       newLabel, encodeName);
genericHeader(tdb, encodeName);

genericBedClick(conn, tdb, item, start, 14);
/*	reserved field has changed to itemRgb in code 2004-11-22 - Hiram */
sqlSafef(query, sizeof(query),
	 "select   chrom, chromStart, chromEnd, name, score, strand, "
	 "         thickStart, thickEnd, reserved, blockCount, blockSizes, "
	 "         chromStarts, Id, color "
	 "from     %s "
	 "where    name = '%s' and chromStart = %d "
	 "order by Id ", tdb->table, item, start);
for (ee = encodeErgeLoadByQuery(conn, query); ee!=NULL; ee=ee->next)
    {
    printf("<BR>\n");
    if (ee->Id>0)
	{
	printf("<BR>Additional information for <A HREF=\"http://dberge.cse.psu.edu/");
	printf("cgi-bin/dberge_query?mode=Submit+query&disp=brow+data&pid=");
	printf("%s\" TARGET=_blank>%s</A>\n is available from <A ", encodeId, encodeName);
	printf("HREF=\"http://globin.cse.psu.edu/dberge/testmenu.html\" ");
	printf("TARGET=_blank>dbERGEII</A>.\n");
	}
    }
printTrackHtml(tdb);
encodeErgeFree(&ee);
hFreeConn(&conn);
}

void doEncodeErgeHssCellLines(struct trackDb *tdb, char *item)
/* Print ENCODE data from dbERGE II */
{
struct sqlConnection *conn = hAllocConn(database);
char query[1024];
struct encodeErgeHssCellLines *ee=NULL;
int start = cartInt(cart, "o");
char *dupe, *words[16];
int wordCount=0;
char *encodeName = getEncodeName(item);
char *encodeId = getEncodeId(item);
int i;

cartWebStart(cart, database, "ENCODE Region Data: %s", tdb->longLabel+7);
printf("<H2>ENCODE Region <span style='text-decoration:underline;'>%s</span> Data for %s</H2>\n",
       tdb->longLabel+7, encodeName);
genericHeader(tdb, item);

dupe = cloneString(tdb->type);
wordCount = chopLine(dupe, words);
genericBedClick(conn, tdb, item, start, atoi(words[1]));
/*	reserved field has changed to itemRgb in code 2004-11-22 - Hiram */
sqlSafef(query, sizeof(query),
	 "select   chrom, chromStart, chromEnd, name, score, strand, "
	 "         thickStart, thickEnd, reserved, blockCount, blockSizes, "
	 "         chromStarts, Id, color, allLines "
	 "from     %s "
	 "where    name = '%s' and chromStart = %d "
	 "order by Id ", tdb->table, item, start);
for (ee = encodeErgeHssCellLinesLoadByQuery(conn, query); ee!=NULL; ee=ee->next)
    {
    if (ee->Id>0)
	{
	printf("<BR><B>Cell lines:</B> ");
	dupe = cloneString(ee->allLines);
	wordCount = chopCommas(dupe, words);
	for (i=0; i<wordCount-1; i++)
	    {
	    printf("%s, ", words[i]);
	    }
	printf("%s.\n",words[wordCount-1]);
	printf("<BR><BR>Additional information for <A HREF=\"http://dberge.cse.psu.edu/");
	printf("cgi-bin/dberge_query?mode=Submit+query&disp=brow+data&pid=");
	printf("%s\" TARGET=_blank>%s</A>\n is available from <A ", encodeId, encodeName);
	printf("HREF=\"http://globin.cse.psu.edu/dberge/testmenu.html\" ");
	printf("TARGET=_blank>dbERGEII</A>.\n");
	}
    }
printTrackHtml(tdb);
encodeErgeHssCellLinesFree(&ee);
hFreeConn(&conn);
}


void doEncodeIndels(struct trackDb *tdb, char *itemName)
{
char *table = tdb->table;
struct encodeIndels encodeIndel;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset = hOffsetPastBin(database, seqName, table);
int start = cartInt(cart, "o");
boolean firstTime = TRUE;

genericHeader(tdb, itemName);

sqlSafef(query, sizeof(query),
      "select * from %s where chrom = '%s' and "
      "chromStart=%d and name = '%s'", table, seqName, start, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    encodeIndelsStaticLoad(row+rowOffset, &encodeIndel);
    if (firstTime)
        {
        printf("<B>Variant and Reference Sequences: </B><BR>\n");
        printf("<PRE><TT>%s<BR>\n", encodeIndel.variant);
        printf("%s</TT></PRE><BR>\n", encodeIndel.reference);
        bedPrintPos((struct bed *)&encodeIndel, 3, tdb);
        firstTime = FALSE;
        printf("-----------------------------------------------------<BR>\n");
        }
    printf("<B>Trace Name:</B> %s <BR>\n", encodeIndel.traceName);
    printf("<B>Trace Id:</B> ");
    printf("<A HREF=\"%s\" TARGET=_blank> %s</A> <BR>\n",
            traceUrl(encodeIndel.traceId), encodeIndel.traceId);
    printf("<B>Trace Pos:</B> %d <BR>\n", encodeIndel.tracePos);
    printf("<B>Trace Strand:</B> %s <BR>\n", encodeIndel.traceStrand);
    printf("<B>Quality Score:</B> %d <BR>\n", encodeIndel.score);
    printf("-----------------------------------------------------<BR>\n");
    }
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doGbProtAnn(struct trackDb *tdb, char *item)
/* Show extra info for GenBank Protein Annotations track. */
{
struct sqlConnection *conn  = hAllocConn(database);
struct sqlResult *sr;
char query[256];
char **row;
int start = cartInt(cart, "o");
struct gbProtAnn *gbProtAnn;

genericHeader(tdb, item);
sqlSafef(query, sizeof query, "select * from gbProtAnn where name = '%s' and chrom = '%s' and chromStart = %d",
        item, seqName, start);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    gbProtAnn = gbProtAnnLoad(row);
    printCustomUrl(tdb, item, TRUE);
    printf("<B>Product:</B> %s<BR>\n", gbProtAnn->product);
    if (gbProtAnn->note[0] != 0)
	printf("<B>Note:</B> %s <BR>\n", gbProtAnn->note);
    printf("<B>GenBank Protein: </B>");
    printf("<A HREF=\"https://www.ncbi.nlm.nih.gov/entrez/viewer.fcgi?val=%s\"",
	    gbProtAnn->proteinId);
    printf(" TARGET=_blank>%s</A><BR>\n", gbProtAnn->proteinId);

    htmlHorizontalLine();
    showSAM_T02(gbProtAnn->proteinId);

    printPos(seqName, gbProtAnn->chromStart, gbProtAnn->chromEnd, "+", TRUE,
	     gbProtAnn->name);
    }
printTrackHtml(tdb);

sqlFreeResult(&sr);
hFreeConn(&conn);
}

bool matchTableOrHandler(char *word, struct trackDb *tdb)
/* return true if word matches either the table name or the trackHandler setting of the tdb struct */
{
if (NULL == tdb)
    return FALSE;
char* handler = trackDbSetting(tdb, "trackHandler");
return (sameWord(word, tdb->table) || (handler!=NULL && sameWord(word, handler)));
}

void doLinkedFeaturesSeries(char *track, char *clone, struct trackDb *tdb)
/* Create detail page for linked features series tracks */
{
char query[256];
char title[256];
struct sqlConnection *conn = hAllocConn(database), *conn1 = hAllocConn(database);
struct sqlResult *sr = NULL, *sr2 = NULL, *srb = NULL;
char **row, **row1, **row2, **rowb;
char *lfLabel = NULL;
char *table = NULL;
char *intName = NULL;
char pslTable[HDB_MAX_TABLE_STRING];
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
int length = end - start;
int i;
struct lfs *lfs;
struct psl *pslList = NULL, *psl;
boolean hasBin = hOffsetPastBin(database, seqName, track);

/* Determine type */
if (matchTableOrHandler("bacEndPairs", tdb))
    {
    safef(title, sizeof title, "Location of %s using BAC end sequences", clone);
    lfLabel = "BAC ends";
    table = track;
    }
if (matchTableOrHandler("bacEndSingles", tdb))
     {
     safef(title, sizeof title, "Location of %s using BAC end sequences", clone);
     lfLabel = "BAC ends";
     table = track;
     }
if (matchTableOrHandler("bacEndPairsBad", tdb))
    {
    safef(title, sizeof title, "Location of %s using BAC end sequences", clone);
    lfLabel = "BAC ends";
    table = track;
    }
if (matchTableOrHandler("bacEndPairsLong", tdb))
    {
    safef(title, sizeof title, "Location of %s using BAC end sequences", clone);
    lfLabel = "BAC ends";
    table = track;
    }
if (matchTableOrHandler("fosEndPairs", tdb))
    {
    safef(title, sizeof title, "Location of %s using fosmid end sequences", clone);
    lfLabel = "Fosmid ends";
    table = track;
    }
if (matchTableOrHandler("fosEndPairsBad", tdb))
    {
    safef(title, sizeof title, "Location of %s using fosmid end sequences", clone);
    lfLabel = "Fosmid ends";
    table = track;
    }
if (matchTableOrHandler("fosEndPairsLong", tdb))
    {
    safef(title, sizeof title, "Location of %s using fosmid end sequences", clone);
    lfLabel = "Fosmid ends";
    table = track;
    }
if (matchTableOrHandler("earlyRep", tdb))
    {
    safef(title, sizeof title, "Location of %s using cosmid end sequences", clone);
    lfLabel = "Early Replication Cosmid Ends";
    table = track;
    }
if (matchTableOrHandler("earlyRepBad", tdb))
    {
    safef(title, sizeof title, "Location of %s using cosmid end sequences", clone);
    lfLabel = "Early Replication Cosmid Ends";
    table = track;
    }
if (trackDbSetting(tdb, "lfPslTable"))
    {
    safef(title, sizeof title, "Location of %s using clone end sequences", clone);
    lfLabel = "Clone ends";
    table = track;
    }

/* Print out non-sequence info */
cartWebStart(cart, database, "%s", title);

/* Find the instance of the object in the bed table */
sqlSafef(query, sizeof query, "SELECT * FROM %s WHERE name = '%s' "
               "AND chrom = '%s' AND chromStart = %d "
               "AND chromEnd = %d",
        table, clone, seqName, start, end);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    lfs = lfsLoad(row+hasBin);
    if (sameString("bacEndPairs", track) || sameString("bacEndSingles", track))
	{
        if (sameString("Zebrafish", organism) )
            {
            /* query to bacCloneXRef table to get Genbank accession */
            /* and internal Sanger name for clones */
            sqlSafef(query, sizeof query, "SELECT genbank, intName FROM bacCloneXRef WHERE name = '%s'", clone);
            srb = sqlMustGetResult(conn1, query);
            rowb = sqlNextRow(srb);
            if (rowb != NULL)
                {
	        printf("<H2><A HREF=");
	        printCloneDbUrl(stdout, clone);
	        printf(" TARGET=_BLANK>%s</A></H2>\n", clone);
                if (rowb[0] != NULL)
                    {
                    printf("<H3>Genbank Accession: <A HREF=");
                    printEntrezNucleotideUrl(stdout, rowb[0]);
                    printf(" TARGET=_BLANK>%s</A></H3>\n", rowb[0]);
                    }
                else
                    printf("<H3>Genbank Accession: n/a");
                intName = cloneString(rowb[1]);
                }
            else
                printf("<H2>%s</H2>\n", clone);
            }
        else if (sameString("Dog", organism) ||
	         sameString("Zebra finch", organism))
            {
            printf("<H2><A HREF=");
            printTraceUrl(stdout, "clone_id", clone);
            printf(" TARGET=_BLANK>%s</A></H2>\n", clone);
            }
	else if (trackDbSetting(tdb, "notNCBI"))
	    {
	    printf("<H2>%s</H2>\n", clone);
	    }
        else if (startsWith(tdb->track, "trace"))
            {
            printTraceUrl(stdout, "clone_id", clone);
            }
        else
            {
	    printf("<H2><A HREF=");
	    printCloneDbUrl(stdout, clone);
	    printf(" TARGET=_BLANK>%s</A></H2>\n", clone);
	    }
        }
    else if (trackDbSetting(tdb, "lfPslTable"))
        {
        printf("<H2><A HREF=");
        printCloneDbUrl(stdout, clone);
        printf(" TARGET=_BLANK>%s</A></H2>\n", clone);

        }
    else
	{
	printf("<B>%s</B>\n", clone);
	}

    printf("<P><HR ALIGN=\"CENTER\"></P>\n<TABLE>\n");
    printf("<TR><TH ALIGN=left>Chromosome:</TH><TD>%s</TD></TR>\n",seqName);
    printf("<TR><TH ALIGN=left>Start:</TH><TD>%d</TD></TR>\n",start+1);
    printf("<TR><TH ALIGN=left>End:</TH><TD>%d</TD></TR>\n",end);
    printf("<TR><TH ALIGN=left>Length:</TH><TD>%d</TD></TR>\n",length);
    printf("<TR><TH ALIGN=left>Strand:</TH><TD>%s</TD></TR>\n", lfs->strand);
    printf("<TR><TH ALIGN=left>Score:</TH><TD>%d</TD></TR>\n", lfs->score);

    if ((sameString("Zebrafish", organism)) && ((sameString("bacEndPairs", track)) || (sameString("bacEndSingles", track))) )
        {
        /* print Sanger FPC name (internal name) */
        printf("<TR><TH ALIGN=left>Sanger FPC Name:</TH><TD>");
        if (intName != NULL)
            printf("%s</TD></TR>\n", intName);
        else
            printf("n/a</TD></TR>\n");
        /* print associated STS information for this BAC clone */
        //printBacStsXRef(clone);
        }
    else
        {
        printBand(seqName, start, end, TRUE);
        printf("</TABLE>\n");
        printf("<P><HR ALIGN=\"CENTER\"></P>\n");
        }
    if (lfs->score == 1000)
        {
	printf("<H4>This is the only location found for %s</H4>\n",clone);
	}
    else
        {
	//printOtherLFS(clone, table, start, end);
	}

    safef(title, sizeof title, "Genomic alignments of %s:", lfLabel);
    webNewSection("%s",title);

    for (i = 0; i < lfs->lfCount; i++)
        {
        sqlFreeResult(&sr);
        if (!hFindSplitTable(database, seqName, lfs->pslTable, pslTable, sizeof pslTable, &hasBin))
	    errAbort("track %s not found", lfs->pslTable);

        if (isEmpty(pslTable) && trackDbSetting(tdb, "lfPslTable"))
            safecpy(pslTable, sizeof(pslTable), trackDbSetting(tdb, "lfPslTable"));
            
        sqlSafef(query, sizeof query, "SELECT * FROM %s WHERE qName = '%s'",
                       pslTable, lfs->lfNames[i]);
        sr = sqlMustGetResult(conn, query);
        while ((row1 = sqlNextRow(sr)) != NULL)
            {
	    psl = pslLoad(row1+hasBin);
            slAddHead(&pslList, psl);
            }
        slReverse(&pslList);

        if ((!sameString("fosEndPairs", track))
            && (!sameString("earlyRep", track))
            && (!sameString("earlyRepBad", track)))
	    {
            if (sameWord(organism, "Zebrafish") )
                {
                /* query to bacEndAlias table to get Genbank accession */
                sqlSafef(query, sizeof query, "SELECT * FROM bacEndAlias WHERE alias = '%s' ",
                        lfs->lfNames[i]);

                sr2 = sqlMustGetResult(conn, query);
                row2 = sqlNextRow(sr2);
                if (row2 != NULL)
                    {
                    printf("<H3>%s\tAccession: <A HREF=", lfs->lfNames[i]);
                    printEntrezNucleotideUrl(stdout, row2[2]);
                    printf(" TARGET=_BLANK>%s</A></H3>\n", row2[2]);
                    }
                else
                    {
                    printf("<B>%s</B>\n",lfs->lfNames[i]);
                    }
                sqlFreeResult(&sr2);
                }
            else if (sameString("Dog", organism) ||
		     sameString("Zebra finch", organism))
                {
                printf("<H3><A HREF=");
                printTraceUrl(stdout, "trace_name", lfs->lfNames[i]);
                printf(" TARGET=_BLANK>%s</A></H3>\n",lfs->lfNames[i]);
                }
	    else if (trackDbSetting(tdb, "notNCBI"))
		{
		printf("<H3>%s</H3>\n", lfs->lfNames[i]);
		}
	    else if (trackDbSetting(tdb, "lfPslTable"))
		{
                printf("<H3><A HREF=");
                printTraceTiUrl(stdout, lfs->lfNames[i]);
                printf(" TARGET=_BLANK>%s</A></H3>\n",lfs->lfNames[i]);
		}
            else
                {
	        printf("<H3><A HREF=");
	        printEntrezNucleotideUrl(stdout, lfs->lfNames[i]);
	        printf(" TARGET=_BLANK>%s</A></H3>\n",lfs->lfNames[i]);
                }
	    }
        else
	    {
	    printf("<B>%s</B>\n", lfs->lfNames[i]);
	    }
	printAlignments(pslList, lfs->lfStarts[i], "htcCdnaAli", lfs->pslTable, lfs->lfNames[i]);
	htmlHorizontalLine();
	pslFreeList(&pslList);
	}
    }
else
    {
    warn("Couldn't find %s in %s table", clone, table);
    }
sqlFreeResult(&sr);
sqlFreeResult(&sr2);
sqlFreeResult(&srb);
webNewSection("Notes:");
printTrackHtml(tdb);
hFreeConn(&conn);
hFreeConn(&conn1);
}

void fillCghTable(int type, char *tissue, boolean bold)
/* Get the requested records from the database and print out HTML table */
{
char query[256];
char currName[64];
int rowOffset;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
struct cgh *cghRow;

if (tissue)
    sqlSafef(query, sizeof query, "type = %d AND tissue = '%s' ORDER BY name, chromStart", type, tissue);
else
    sqlSafef(query, sizeof query, "type = %d ORDER BY name, chromStart", type);
sr = hRangeQuery(conn, "cgh", seqName, winStart, winEnd, query, &rowOffset);
while ((row = sqlNextRow(sr)))
    {
    cghRow = cghLoad(row);
    if (strcmp(currName,cghRow->name))
	{
        if (bold)
	    printf("</TR>\n<TR>\n<TH>%s</TH>\n",cghRow->name);
	else
	    printf("</TR>\n<TR>\n<TD>%s</TD>\n",cghRow->name);
	strcpy(currName,cghRow->name);
	}
    if (bold)
	printf("<TH ALIGN=right>%.6f</TH>\n",cghRow->score);
    else
	printf("<TD ALIGN=right>%.6f</TD>\n",cghRow->score);
    }
sqlFreeResult(&sr);
}


/* Evan Eichler's stuff */

void doCeleraDupPositive(struct trackDb *tdb, char *dupName)
/* Handle click on celeraDupPositive track. */
{
struct celeraDupPositive dup;
char query[512];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int celeraVersion = 0;
int i = 0;
cartWebStart(cart, database, "%s", tdb->longLabel);

if (sameString(database, "hg15"))
    celeraVersion = 3;
else
    celeraVersion = 4;

if (cgiVarExists("o"))
    {
    int start = cgiInt("o");
    int rowOffset = hOffsetPastBin(database, seqName, tdb->table);

    sqlSafef(query, sizeof(query),
	  "select * from %s where chrom = '%s' and chromStart = %d and name= '%s'",
	  tdb->table, seqName, start, dupName);
    sr = sqlGetResult(conn, query);
    i = 0;
    while ((row = sqlNextRow(sr)))
	{
	if (i > 0)
	    htmlHorizontalLine();
	celeraDupPositiveStaticLoad(row+rowOffset, &dup);
	printf("<B>Duplication Name:</B> %s<BR>\n", dup.name);
	bedPrintPos((struct bed *)(&dup), 3, tdb);
	if (!sameString(dup.name, dup.fullName))
	    printf("<B>Full Descriptive Name:</B> %s<BR>\n", dup.fullName);
	if (dup.bpAlign > 0)
	    {
	    printf("<B>Fraction BP Match:</B> %3.4f<BR>\n", dup.fracMatch);
	    printf("<B>Alignment Length:</B> %3.0f<BR>\n", dup.bpAlign);
	    }
	if (!startsWith("WSSD No.", dup.name))
	    {
	    printf("<A HREF=\"http://humanparalogy.gs.washington.edu"
		   "/eichler/celera%d/cgi-bin/celera%d.pl"
		   "?search=%s&type=pdf \" Target=%s_PDF>"
		   "<B>Clone Read Depth Graph (PDF)</B></A><BR>",
		   celeraVersion, celeraVersion, dup.name, dup.name);
	    printf("<A HREF=\"http://humanparalogy.gs.washington.edu"
		   "/eichler/celera%d/cgi-bin/celera%d.pl"
		   "?search=%s&type=jpg \" Target=%s_JPG>"
		   "<B>Clone Read Depth Graph (JPG)</B></A><BR>",
		   celeraVersion, celeraVersion, dup.name, dup.name);
	    }
	i++;
	}
    }
else
    {
    puts("<P>Click directly on a duplication for information on that "
	 "duplication.</P>");
    }

printTrackHtml(tdb);
hFreeConn(&conn);
}

void parseSuperDupsChromPointPos(char *pos, char *retChrom, int *retPos,
				 int *retID)
/* Parse out (No.)?NNNN[.,]chrN:123 into NNNN and chrN and 123. */
{
char *words[16];
int wordCount = 0;
char *sep = ",.:";
char *origPos = pos;
if (startsWith("No.", pos))
    pos += strlen("No.");
pos = cloneString(pos);
wordCount = chopString(pos, sep, words, ArraySize(words));
if (wordCount < 2 || wordCount > 3)
    errAbort("parseSuperDupsChromPointPos: Expected something like "
	     "(No\\.)?([0-9]+[.,])?[a-zA-Z0-9_]+:[0-9]+ but got %s", origPos);
if (wordCount == 3)
    {
    *retID = sqlUnsigned(words[0]);
    safecpy(retChrom, 64, words[1]);
    *retPos = sqlUnsigned(words[2]);
    }
else
    {
    *retID = -1;
    safecpy(retChrom, 64, words[0]);
    *retPos = sqlUnsigned(words[1]);
    }
}


void doGenomicSuperDups(struct trackDb *tdb, char *dupName)
/* Handle click on genomic dup track. */
{
cartWebStart(cart, database, "%s", tdb->longLabel);

if (cgiVarExists("o"))
    {
    struct genomicSuperDups dup;
    struct dyString *query = dyStringNew(512);
    struct sqlConnection *conn = hAllocConn(database);
    struct sqlResult *sr;
    char **row;
    char oChrom[64];
    int oStart;
    int dupId;
    int rowOffset;
    int start = cgiInt("o");
    int end   = cgiInt("t");
    char *alignUrl = NULL;
    if (sameString("hg18", database))
	alignUrl = "http://humanparalogy.gs.washington.edu/build36";
    else if (sameString("hg17", database))
	alignUrl = "http://humanparalogy.gs.washington.edu";
    else if (sameString("hg15", database) || sameString("hg16", database))
	alignUrl = "http://humanparalogy.gs.washington.edu/jab/der_oo33";
    rowOffset = hOffsetPastBin(database, seqName, tdb->table);
    parseSuperDupsChromPointPos(dupName, oChrom, &oStart, &dupId);
    sqlDyStringPrintf(query, "select * from %s where chrom = '%s' and ",
		   tdb->table, seqName);
    if (rowOffset > 0)
	hAddBinToQuery(start, end, query);
    if (dupId >= 0)
	sqlDyStringPrintf(query, "uid = %d and ", dupId);
    sqlDyStringPrintf(query, "chromStart = %d and otherStart = %d",
		   start, oStart);
    sr = sqlGetResult(conn, query->string);
    while ((row = sqlNextRow(sr)))
	{
	genomicSuperDupsStaticLoad(row+rowOffset, &dup);
	bedPrintPos((struct bed *)(&dup), 4, tdb);
	printf("<B>Other Position:</B> "
	       "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">"
	       "%s:%d-%d</A> &nbsp;&nbsp;&nbsp;\n",
               hgTracksPathAndSettings(), database,
	       dup.otherChrom, dup.otherStart+1, dup.otherEnd,
	       dup.otherChrom, dup.otherStart+1, dup.otherEnd);
	printf("<A HREF=\"%s&o=%d&t=%d&g=getDna&i=%s&c=%s&l=%d&r=%d&strand=%s&db=%s&table=%s\">"
	       "View DNA for other position</A><BR>\n",
	       hgcPathAndSettings(), dup.otherStart, dup.otherEnd, "",
	       dup.otherChrom, dup.otherStart, dup.otherEnd, dup.strand,
	       database, tdb->track);
	printf("<B>Other Position Relative Orientation:</B>%s<BR>\n",
	       dup.strand);
	if(sameString("canFam1", database))
	{
		printf("<B>Filter Verdict:</B> %s<BR>\n", dup.verdict);
		printf("&nbsp;&nbsp;&nbsp;<B> testResult:</B>%s<BR>\n", dup.testResult);
		printf("&nbsp;&nbsp;&nbsp;<B> chits:</B>%s<BR>\n", dup.chits);
		printf("&nbsp;&nbsp;&nbsp;<B> ccov:</B>%s<BR>\n", dup.ccov);
		printf("&nbsp;&nbsp;&nbsp;<B> posBasesHit:</B>%d<BR>\n",
		       dup.posBasesHit);
	}
	if (alignUrl != NULL)
	    printf("<A HREF=%s/%s "
		   "TARGET=\"%s:%d-%d\">Optimal Global Alignment</A><BR>\n",
		   alignUrl, dup.alignfile, dup.chrom,
		   dup.chromStart, dup.chromEnd);
	printf("<B>Alignment Length:</B> %d<BR>\n", dup.alignL);
	printf("&nbsp;&nbsp;&nbsp;<B>Indels #:</B> %d<BR>\n", dup.indelN);
	printf("&nbsp;&nbsp;&nbsp;<B>Indels bp:</B> %d<BR>\n", dup.indelS);
	printf("&nbsp;&nbsp;&nbsp;<B>Aligned Bases:</B> %d<BR>\n", dup.alignB);
        printf("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<B>Matching bases:</B> %d<BR>\n",
	       dup.matchB);
	printf("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<B>Mismatched bases:</B> %d<BR>\n",
	       dup.mismatchB);
	printf("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<B>Transitions:</B> %d<BR>\n",
	       dup.transitionsB);
	printf("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<B>Transverions:</B> %d<BR>\n",
	       dup.transversionsB);
	printf("&nbsp;&nbsp;&nbsp;<B>Fraction Matching:</B> %3.4f<BR>\n",
	       dup.fracMatch);
	printf("&nbsp;&nbsp;&nbsp;<B>Fraction Matching with Indels:</B> %3.4f<BR>\n",
	       dup.fracMatchIndel);
	printf("&nbsp;&nbsp;&nbsp;<B>Jukes Cantor:</B> %3.4f<BR>\n", dup.jcK);
	}
    dyStringFree(&query);
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }
else
    puts("<P>Click directly on a repeat for specific information on that repeat</P>");
printTrackHtml(tdb);
}
/* end of Evan Eichler's stuff */

void doCgh(char *track, char *tissue, struct trackDb *tdb)
/* Create detail page for comparative genomic hybridization track */
{
char query[256];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;

/* Print out non-sequence info */
cartWebStart(cart, database, "%s", tissue);

/* Print general range info */
printf("<H2>UCSF Comparative Genomic Hybridizations - %s</H2>\n", tissue);
printf("<P><HR ALIGN=\"CENTER\"></P>\n<TABLE>\n");
printf("<TR><TH ALIGN=left>Chromosome:</TH><TD>%s</TD></TR>\n",seqName);
printf("<TR><TH ALIGN=left>Start window:</TH><TD>%d</TD></TR>\n",winStart);
printf("<TR><TH ALIGN=left>End window:</TH><TD>%d</TD></TR>\n",winEnd);
printf("</TABLE>\n");
printf("<P><HR ALIGN=\"CENTER\"></P>\n");

/* Find the names of all of the clones in this range */
printf("<TABLE>\n");
printf("<TR><TH>Cell Line</TH>");
sqlSafef(query, sizeof query, "SELECT spot from cgh where chrom = '%s' AND "
               "chromStart <= '%d' AND chromEnd >= '%d' AND "
               "tissue = '%s' AND type = 3 GROUP BY spot ORDER BY chromStart",
	seqName, winEnd, winStart, tissue);
sr = sqlMustGetResult(conn, query);
while ((row = sqlNextRow(sr)))
    printf("<TH>Spot %s</TH>",row[0]);
printf("</TR>\n");
sqlFreeResult(&sr);

/* Find the relevant tissues type records in the range */
fillCghTable(3, tissue, FALSE);
printf("<TR><TD>&nbsp;</TD></TR>\n");

/* Find the relevant tissue average records in the range */
fillCghTable(2, tissue, TRUE);
printf("<TR><TD>&nbsp;</TD></TR>\n");

/* Find the all tissue average records in the range */
fillCghTable(1, NULL, TRUE);
printf("<TR><TD>&nbsp;</TD></TR>\n");

printf("</TR>\n</TABLE>\n");
hFreeConn(&conn);
}

void doMcnBreakpoints(char *track, char *name, struct trackDb *tdb)
/* Create detail page for MCN breakpoints track */
{
char query[256];
char title[256];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
char **row;
struct mcnBreakpoints *mcnRecord;

/* Print out non-sequence info */
safef(title, sizeof title, "MCN Breakpoints - %s",name);
cartWebStart(cart, database, "%s", title);

/* Print general range info */
/*printf("<H2>MCN Breakpoints - %s</H2>\n", name);
  printf("<P><HR ALIGN=\"CENTER\"></P>");*/
printf("<TABLE>\n");
printf("<TR><TH ALIGN=left>Chromosome:</TH><TD>%s</TD></TR>\n",seqName);
printf("<TR><TH ALIGN=left>Begin in Chromosome:</TH><TD>%d</TD></TR>\n",start);
printf("<TR><TH ALIGN=left>End in Chromosome:</TH><TD>%d</TD></TR>\n",end);
printBand(seqName, start, end, TRUE);
printf("</TABLE>\n");

/* Find all of the breakpoints in this range for this name*/
sqlSafef(query, sizeof query, "SELECT * FROM mcnBreakpoints WHERE chrom = '%s' AND "
               "chromStart = %d and chromEnd = %d AND name = '%s'",
	seqName, start, end, name);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)))
    {
    printf("<P><HR ALIGN=\"CENTER\"></P>\n");
    mcnRecord = mcnBreakpointsLoad(row);
    printf("<TABLE>\n");
    printf("<TR><TH ALIGN=left>Case ID:</TH><TD>%s</TD></TR>", mcnRecord->caseId);
    printf("<TR><TH ALIGN=left>Breakpoint ID:</TH><TD>%s</TD></TR>", mcnRecord->bpId);
    printf("<TR><TH ALIGN=left>Trait:</TH><TD>%s</TD><TD>%s</TD></TR>", mcnRecord->trId, mcnRecord->trTxt);
    printf("<TR><TH ALIGN=left>Trait Group:</TH><TD>%s</TD><TD>%s</TD></TR>", mcnRecord->tgId, mcnRecord->tgTxt);
    printf("</TR>\n</TABLE>\n");
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doProbeDetails(struct trackDb *tdb, char *item)
{
struct sqlConnection *conn = hAllocConn(database);
struct dnaProbe *dp = NULL;
char query[256];

genericHeader(tdb, item);
sqlSafef(query, sizeof(query), "select * from dnaProbe where name='%s'",  item);
dp = dnaProbeLoadByQuery(conn, query);
if(dp != NULL)
    {
    printf("<h3>Probe details:</h3>\n");
    printf("<b>Name:</b> %s  <span style='font-size:x-small;'>"
           "[dbName genomeVersion strand coordinates]</span><br>\n",dp->name);
    printf("<b>Dna:</b> %s", dp->dna );
    printf("[<a href=\"hgBlat?type=DNA&genome=hg8&sort=&query,score&output=hyperlink&userSeq=%s\">blat (blast like alignment)</a>]<br>", dp->dna);
    printf("<b>Size:</b> %d<br>", dp->size );
    printf("<b>Chrom:</b> %s<br>", dp->chrom );
    printf("<b>ChromStart:</b> %d<br>", dp->start+1 );
    printf("<b>ChromEnd:</b> %d<br>", dp->end );
    printf("<b>Strand:</b> %s<br>", dp->strand );
    printf("<b>3' Dist:</b> %d<br>", dp->tpDist );
    printf("<b>Tm:</b> %f <span style='font-size:x-small;'>"
           "[scores over 100 are allowed]</span><br>", dp->tm );
    printf("<b>%%GC:</b> %f<br>", dp->pGC );
    printf("<b>Affy:</b> %d <span style='font-size:x-small;'>"
           "[1 passes, 0 doesn't pass Affy heuristic]</span><br>", dp->affyHeur );
    printf("<b>Sec Struct:</b> %f<br>", dp->secStruct);
    printf("<b>blatScore:</b> %d<br>", dp->blatScore );
    printf("<b>Comparison:</b> %f<br>", dp->comparison);
    }
/* printf("<h3>Genomic Details:</h3>\n");
 * genericBedClick(conn, tdb, item, start, 1); */
printTrackHtml(tdb);
hFreeConn(&conn);
}

void doChicken13kDetails(struct trackDb *tdb, char *item)
{
struct sqlConnection *conn = hAllocConn(database);
struct chicken13kInfo *chick = NULL;
char query[256];
int start = cartInt(cart, "o");

genericHeader(tdb, item);
sqlSafef(query, sizeof(query), "select * from chicken13kInfo where id='%s'",  item);
chick = chicken13kInfoLoadByQuery(conn, query);
if (chick != NULL)
    {
    printf("<b>Probe name:</b> %s<br>\n", chick->id);
    printf("<b>Source:</b> %s<br>\n", chick->source);
    printf("<b>PCR Amplification code:</b> %s<br>\n", chick->pcr);
    printf("<b>Library:</b> %s<br>\n", chick->library);
    printf("<b>Source clone name:</b> %s<br>\n", chick->clone);
    printf("<b>Library:</b> %s<br>\n", chick->library);
    printf("<b>Genbank accession:</b> %s<br>\n", chick->gbkAcc);
    printf("<b>BLAT alignment:</b> %s<br>\n", chick->blat);
    printf("<b>Source annotation:</b> %s<br>\n", chick->sourceAnnot);
    printf("<b>TIGR assigned TC:</b> %s<br>\n", chick->tigrTc);
    printf("<b>TIGR TC annotation:</b> %s<br>\n", chick->tigrTcAnnot);
    printf("<b>BLAST determined annotation:</b> %s<br>\n", chick->blastAnnot);
    printf("<b>Comment:</b> %s<br>\n", chick->comment);
    }
genericBedClick(conn, tdb, item, start, 1);
printTrackHtml(tdb);
hFreeConn(&conn);
}

void perlegenDetails(struct trackDb *tdb, char *item)
{
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);
char table[HDB_MAX_TABLE_STRING];
boolean hasBin;
struct bed *bed;
char query[512];
struct sqlResult *sr;
char **row;
boolean firstTime = TRUE;
int numSnpsReq = -1;
if(tdb == NULL)
    errAbort("TrackDb entry null for perlegen, item=%s\n", item);

genericHeader(tdb, item);
printCustomUrl(tdb, item, FALSE);
if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, &hasBin))
    errAbort("track %s not found", tdb->table);
sqlSafef(query, sizeof query, "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d",
        table, item, seqName, start);
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    char *name;
    /* set up for first time */
    if (firstTime)
	firstTime = FALSE;
    else
	htmlHorizontalLine();
    bed = bedLoadN(row+hasBin, 12);

    /* chop leading digits off name which should be in x/yyyyyy format */
    name = strstr(bed->name, "/");
    if(name == NULL)
        name = bed->name;
    else
        name++;

    /* determine number of SNPs required from score */
    switch(bed->score)
        {
	case 1000:
	    numSnpsReq = 0;
	    break;
	case 650:
	    numSnpsReq = 1;
	    break;
	case 500:
	    numSnpsReq = 2;
	    break;
	case 250:
	    numSnpsReq = 3;
	    break;
	case 50:
	    numSnpsReq = 4;
	    break;
	}

    /* finish off report ... */
    printf("<B>Block:</B> %s<BR>\n", name);
    printf("<B>Number of SNPs in block:</B> %d<BR>\n", bed->blockCount);
    printf("<B>Number of SNPs to represent block:</B> %d<BR>\n",numSnpsReq);
    printf("<B>Strand:</B> %s<BR>\n", bed->strand);
    bedPrintPos(bed, 3, tdb);
    }
printTrackHtml(tdb);
hFreeConn(&conn);
}

void haplotypeDetails(struct trackDb *tdb, char *item)
{
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);
char table[HDB_MAX_TABLE_STRING];
boolean hasBin;
struct bed *bed;
char query[512];
struct sqlResult *sr;
char **row;
boolean firstTime = TRUE;
if(tdb == NULL)
    errAbort("TrackDb entry null for haplotype, item=%s\n", item);

genericHeader(tdb, item);
printCustomUrl(tdb, item, TRUE);
if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, &hasBin))
    errAbort("track %s not found", tdb->table);
sqlSafef(query, sizeof query, "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d",
        table, item, seqName, start);
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    /* set up for first time */
    if (firstTime)
	firstTime = FALSE;
    else
	htmlHorizontalLine();
    bed = bedLoadN(row+hasBin, 12);

    /* finish off report ... */
    printf("<B>Block:</B> %s<BR>\n", bed->name);
    printf("<B>Number of SNPs in block:</B> %d<BR>\n", bed->blockCount);
    /*    printf("<B>Number of SNPs to represent block:</B> %d<BR>\n",numSnpsReq);*/
    printf("<B>Strand:</B> %s<BR>\n", bed->strand);
    bedPrintPos(bed, 3, tdb);
    }
printTrackHtml(tdb);
hFreeConn(&conn);
}

void mitoDetails(struct trackDb *tdb, char *item)
{
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);
char table[HDB_MAX_TABLE_STRING];
boolean hasBin;
struct bed *bed;
char query[512];
struct sqlResult *sr;
char **row;
boolean firstTime = TRUE;
int numSnpsReq = -1;
if(tdb == NULL)
    errAbort("TrackDb entry null for mitoSnps, item=%s\n", item);

genericHeader(tdb, item);
printCustomUrl(tdb, item, TRUE);
if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, &hasBin))
    errAbort("track %s not found", tdb->table);
sqlSafef(query, sizeof query, "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d",
        table, item, seqName, start);
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    char *name;
    /* set up for first time */
    if (firstTime)
	firstTime = FALSE;
    else
	htmlHorizontalLine();
    bed = bedLoadN(row+hasBin, 12);

    /* chop leading digits off name which should be in xx/yyyyyy format */
    name = strstr(bed->name, "/");
    if(name == NULL)
        name = bed->name;
    else
        name++;

    /* determine number of SNPs required from score */
    switch(bed->score)
        {
	case 1000:
	    numSnpsReq = 0;
	    break;
	case 650:
	    numSnpsReq = 1;
	    break;
	case 500:
	    numSnpsReq = 2;
	    break;
	case 250:
	    numSnpsReq = 3;
	    break;
	case 50:
	    numSnpsReq = 4;
	    break;
	}
    /* finish off report ... */
    printf("<B>Block:</B> %s<BR>\n", name);
    printf("<B>Number of SNPs in block:</B> %d<BR>\n", bed->blockCount);
    printf("<B>Number of SNPs to represent block:</B> %d<BR>\n",numSnpsReq);
    printf("<B>Strand:</B> %s<BR>\n", bed->strand);
    bedPrintPos(bed, 3, tdb);
    }
printTrackHtml(tdb);
hFreeConn(&conn);
}

void ancientRDetails(struct trackDb *tdb, char *item)
{
struct sqlConnection *conn = hAllocConn(database);
char table[HDB_MAX_TABLE_STRING];
boolean hasBin;
struct bed *bed = NULL;
char query[512];
struct sqlResult *sr = NULL;
char **row;
boolean firstTime = TRUE;
double ident = -1.0;
struct ancientRref *ar = NULL;

if(tdb == NULL)
    errAbort("TrackDb entry null for ancientR, item=%s\n", item);
genericHeader(tdb, item);
printCustomUrl(tdb, item, TRUE);
if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, &hasBin))
    errAbort("track %s not found", tdb->table);
sqlSafef(query, sizeof query, "select * from %s where name = '%s' and chrom = '%s'",
        table, item, seqName );
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    char *name;
    /* set up for first time */
    if (firstTime)
	firstTime = FALSE;
    else
	htmlHorizontalLine();
    bed = bedLoadN(row+hasBin, 12);

    name = bed->name;

    /* get % identity from score */
    ident = ((bed->score + 500.0)/1500.0)*100.0;

    /* finish off report ... */
    printf("<h4><i>Joint Alignment</i></h4>");
    printf("<B>ID:</B> %s<BR>\n", name);
    printf("<B>Number of aligned blocks:</B> %d<BR>\n", bed->blockCount);

    if( ident == 50.0 )
        printf("<B>Percent identity of aligned blocks:</B> <= %g%%<BR>\n", ident);
    else
        printf("<B>Percent identity of aligned blocks:</B> %g%%<BR>\n", ident);

    printf("<h4><i>Human Sequence</i></h4>");
    printf("<B>Strand:</B> %s<BR>\n", bed->strand);
    bedPrintPos(bed, 3, tdb);

    }

/* look in associated table 'ancientRref' to get human/mouse alignment*/
sqlSafef(query, sizeof query, "select * from %sref where id = '%s'", table, item );
sr = sqlGetResult( conn, query );
while ((row = sqlNextRow(sr)) != NULL )
    {
    ar = ancientRrefLoad(row);

    printf("<h4><i>Repeat</i></h4>");
    printf("<B>Name:</B> %s<BR>\n", ar->name);
    printf("<B>Class:</B> %s<BR>\n", ar->class);
    printf("<B>Family:</B> %s<BR>\n", ar->family);

    /* print the aligned sequences in html on multiple rows */
    htmlHorizontalLine();
    printf("<i>human sequence on top, mouse on bottom</i><br><br>" );
    htmlPrintJointAlignment( ar->hseq, ar->mseq, 80,
			     bed->chromStart, bed->chromEnd, bed->strand );
    }

printTrackHtml(tdb);
hFreeConn(&conn);
}

void doGcDetails(struct trackDb *tdb, char *itemName)
/* Show details for gc percent */
{
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
struct gcPercent *gc;
boolean hasBin;
char table[HDB_MAX_TABLE_STRING];

cartWebStart(cart, database, "Percentage GC in 20,000 Base Windows (GC)");

if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, &hasBin))
    errAbort("track %s not found", tdb->table);
sqlSafef(query, sizeof query, "select * from %s where chrom = '%s' and chromStart = %d and name = '%s'",
	table, seqName, start, itemName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gc = gcPercentLoad(row + hasBin);
    printPos(gc->chrom, gc->chromStart, gc->chromEnd, NULL, FALSE, NULL);
    printf("<B>GC Percentage:</B> %3.1f%%<BR>\n", ((float)gc->gcPpt)/10);
    gcPercentFree(&gc);
    }
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void chuckHtmlStart(char *title)
/* Prints the header appropriate for the title
 * passed in. Links html to chucks stylesheet for
 * easier maintaince
 */
{
printf("<HTML>\n<HEAD>\n%s", getCspMetaHeader());
// FIXME blueStyle should not be absolute to genome-test and should be called by:
//       webIncludeResourceFile("blueStyle.css");
printf("<LINK REL=STYLESHEET TYPE=\"text/css\" href=\"http://genome-test.gi.ucsc.edu/style/blueStyle.css\" title=\"Chuck Style\">\n");
printf("<title>%s</title>\n</head><body bgcolor=\"#f3f3ff\">",title);
}

void chuckHtmlContactInfo()
/* Writes out Chuck's email so people bother Chuck instead of Jim */
{
puts("<br><br><span style='font-size:x-small;'><i>If you have comments and/or suggestions please email "
     "<a href=\"mailto:sugnet@soe.ucsc.edu\">sugnet@soe.ucsc.edu</a>.</span>\n");
}


void abbr(char *s, char *fluff)
/* Cut out fluff from s. */
{
int len;
s = strstr(s, fluff);
if (s != NULL)
    {
    len = strlen(fluff);
    strcpy(s, s+len);
    }
}

void printTableHeaderName(char *name, char *clickName, char *url)
/* creates a table to display a name vertically,
 * basically creates a column of letters */
{
int i, length;
char *header = cloneString(name);
header = cloneString(header);
subChar(header,'_',' ');
length = strlen(header);
if(url == NULL)
    url = cloneString("");
/* printf("<b>Name:</b> %s\t<b>clickName:</b> %s\n", name,clickName); */
if (strstr(clickName,name))
    printf("<table border=0 cellspacing=0 cellpadding=0 bgcolor='#D9E4F8'>\n");
else
    printf("<table border=0 cellspacing=0 cellpadding=0>\n");
for(i = 0; i < length; i++)
    {
    if (header[i] == ' ')
        printf("<tr><td align=center>&nbsp</td></tr>\n");
    else
        {
        if (strstr(clickName,name))
            printf("<tr><td align=center bgcolor='#D9E4F8'>");
        else
            printf("<tr><td align=center>");

        /* if we have a url, create a reference */
        if (differentString(url,""))
            printf("<a href=\"%s\" TARGET=_BLANK>%c</a>", url, header[i]);
	else
	    printf("%c", header[i]);

	printf("</td></tr>");
	}
    printf("\n");
    }
printf("</table>\n");
freez(&header);
}

struct sageExp *loadSageExps(char *tableName, struct bed  *bedist)
/* load the sage experiment data. */
{
struct sqlConnection *sc = NULL;
/* struct sqlConnection *sc = sqlConnectRemote("localhost", user, password, "hgFixed"); */
char query[256];
struct sageExp *seList = NULL, *se=NULL;
char **row;
struct sqlResult *sr = NULL;
if(hTableExists(database, tableName))
    sc = hAllocConn(database);
else
    sc = hAllocConn("hgFixed");

sqlSafef(query, sizeof query,"select * from sageExp order by num");
sr = sqlGetResult(sc,query);
while((row = sqlNextRow(sr)) != NULL)
    {
    se = sageExpLoad(row);
    slAddHead(&seList,se);
    }
sqlFreeResult(&sr);
hFreeConn(&sc);
slReverse(&seList);
return seList;
}

struct sage *loadSageData(char *table, struct bed* bedList)
/* load the sage data by constructing a query based on the qNames of the bedList */
{
struct sqlConnection *sc = NULL;
struct dyString *query = dyStringNew(2048);
struct sage *sgList = NULL, *sg=NULL;
struct bed *bed=NULL;
char **row;
int count=0;
struct sqlResult *sr = NULL;
if(hTableExists(database, table))
    sc = hAllocConn(database);
else
    sc = hAllocConn("hgFixed");
sqlDyStringPrintf(query, "select * from sage where ");
for(bed=bedList;bed!=NULL;bed=bed->next)
    {
    if (count++)
        {
        sqlDyStringPrintf(query," or uni=%d ", atoi(bed->name + 3 ));
        }
    else
	{
	sqlDyStringPrintf(query," uni=%d ", atoi(bed->name + 3));
	}
    }
sr = sqlGetResult(sc,query->string);
while((row = sqlNextRow(sr)) != NULL)
    {
    sg = sageLoad(row);
    slAddHead(&sgList,sg);
    }
sqlFreeResult(&sr);
hFreeConn(&sc);
slReverse(&sgList);
dyStringFree(&query);
return sgList;
}

int sageBedWSListIndex(struct bed *bedList, int uni)
/* find the index of a bed by the unigene identifier in a bed list */
{
struct bed *bed;
int count =0;
char buff[128];
safef(buff, sizeof buff, "Hs.%d", uni);
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    if(sameString(bed->name,buff))
	return count;
    count++;
    }
errAbort("Didn't find the unigene tag %s",buff);
return 0;
}

int sortSageByBedOrder(const void *e1, const void *e2)
/* used by slSort to sort the sage experiment data using the order of the beds */
{
const struct sage *s1 = *((struct sage**)e1);
const struct sage *s2 = *((struct sage**)e2);
return(sageBedWSListIndex(sageExpList,s1->uni) - sageBedWSListIndex(sageExpList,s2->uni));
}

void printSageGraphUrl(struct sage *sgList)
/* print out a url to a cgi script which will graph the results */
{
struct sage *sg = NULL;
if (sgList == NULL)
    return;
printf("Please click ");
printf("<a target=_blank href=\"../cgi-bin/sageVisCGI?");
for(sg = sgList; sg != NULL; sg = sg->next)
    {
    if(sg->next == NULL)
	printf("u=%d", sg->uni);
    else
	printf("u=%d&", sg->uni);

    }
printf("&db=%s",database);
printf("\">here</a>");
printf(" to see the data as a graph.\n");
}

void printSageReference(struct sage *sgList, struct trackDb *tdb)
{
printf("%s", tdb->html);
printTBSchemaLink(tdb);
}

void sagePrintTable(struct bed *bedList, char *itemName, struct trackDb *tdb)
/* load up the sage experiment data using bed->qNames and display it as a table */
{
struct sageExp *seList = NULL, *se =NULL;
struct sage *sgList=NULL, *sg=NULL;
int featureCount;
int count=0;
seList=loadSageExps("sageExp",bedList);
sgList = loadSageData("sage", bedList);
slSort(&sgList,sortSageByBedOrder);

printSageReference(sgList, tdb);
/* temporarily disable this link until debugged and fixed.  Fan
printSageGraphUrl(sgList);
*/
printf("<BR>\n");
for(sg=sgList; sg != NULL; sg = sg->next)
    {
    char buff[256];
    safef(buff, sizeof buff, "Hs.%d", sg->uni);
    }
featureCount= slCount(sgList);
printf("<basefont size=-1>\n");
printf("<table cellspacing=0 style='border:1px solid black;'>\n");
printf("<tr>\n");
printf("<th align=center>Sage Experiment</th>\n");
printf("<th align=center>Tissue</th>\n");
printf("<th align=center colspan=%d valign=top>Uni-Gene Clusters<br>(<b>Median</b> [Ave &plusmn Stdev])</th>\n",featureCount);
printf("</tr>\n<tr><td>&nbsp</td><td>&nbsp</td>\n");
for(sg = sgList; sg != NULL; sg = sg->next)
    {
    char buff[32];
    char url[256];
    safef(buff, sizeof buff, "Hs.%d", sg->uni);
    printf("<td valign=top align=center>\n");
    safef(url, sizeof url, "https://www.ncbi.nlm.nih.gov/SAGE/SAGEcid.cgi?cid=%d&org=Hs",sg->uni);
    printTableHeaderName(buff, itemName, url);
    printf("</td>");
    }
printf("</tr>\n");
/* for each experiment write out the name and then all of the values */
for(se=seList;se!=NULL;se=se->next)
    {
    char *tmp;
    tmp = strstr(se->exp,"_");
    if(++count%2)
	printf("<tr>\n");
    else
	printf("<tr bgcolor=\"#bababa\">\n");
    printf("<td align=left>");
    printf("%s</td>\n", tmp ? (tmp+1) : se->exp);

    printf("<td align=left>%s</td>\n", se->tissueType);
    for(sg=sgList; sg!=NULL; sg=sg->next)
        {
        if (sg->aves[se->num] == -1.0)
            printf("<td>N/A</td>");
        else
            printf("<td>  <b>%4.1f</b> <span style='font-size:x-small;'>[%.2f &plusmn %.2f]</span></td>\n",
		   sg->meds[se->num],sg->aves[se->num],sg->stdevs[se->num]);
	}
    printf("</tr>\n");
    }
printf("</table>\n");
}


struct bed *bedWScoreLoadByChrom(char *table, char *chrom, int start, int end)
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
struct bed *bedWS, *bedWSList = NULL;
char **row;
char query[256];
struct hTableInfo *hti = hFindTableInfo(database, seqName, table);
if(hti == NULL)
    errAbort("Can't find table: (%s) %s", seqName, table);
else if(hti && sameString(hti->startField, "tStart"))
    sqlSafef(query, sizeof(query),
             "select qName,tStart,tEnd from %s where tName='%s' and tStart < %u and tEnd > %u",
             table, seqName, winEnd, winStart);
else if(hti && sameString(hti->startField, "chromStart"))
    sqlSafef(query, sizeof(query),
             "select name,chromStart,chromEnd from %s"
             " where chrom='%s' and chromStart < %u and chromEnd > %u",
             table, seqName, winEnd, winStart);
else
    errAbort("%s doesn't have tStart or chromStart", table);
sr = sqlGetResult(conn, query);
while((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(bedWS);
    bedWS->name = cloneString(row[0]);
    bedWS->chromStart = sqlUnsigned(row[1]);
    bedWS->chromEnd = sqlUnsigned(row[2]);
    bedWS->chrom = cloneString(seqName);
    slAddHead(&bedWSList, bedWS);
    }
slReverse(&bedWSList);
sqlFreeResult(&sr);
hFreeConn(&conn);
return bedWSList;
}

/* Lowe Lab additions */
void doSageDataDisp(char *tableName, char *itemName, struct trackDb *tdb)
{
struct bed *sgList = NULL;
int sgCount=0;
chuckHtmlStart("Sage Data Requested");
printf("<h2>Sage Data for: %s %d-%d</h2>\n", seqName, winStart+1, winEnd);
puts("<table cellpadding=0 cellspacing=0><tr><td>\n");

sgList = bedWScoreLoadByChrom(tableName, seqName, winStart, winEnd);

sgCount = slCount(sgList);
if(sgCount > 50)
    printf("<hr><p>That will create too big of a table, try creating a window with less than 50 elements.<hr>\n");
else
    {
    sageExpList = sgList;
    sagePrintTable(sgList, itemName, tdb);
    }
printf("</td></tr></table>\n");
/*zeroBytes(buff,64);
  safe(buff, sizeof buff, "%d", winStart);
  cgiMakeHiddenVar("winStart", buff);
  zeroBytes(buff,64);
  safef(buff, sizeof buff, "%d", winEnd);
  cgiMakeHiddenVar("winEnd", buff);
  cgiMakeHiddenVar("db",database);
  printf("<br>\n");*/
chuckHtmlContactInfo();
}

void makeGrayShades(struct hvGfx *hvg)
/* Make eight shades of gray in display. */
{
int i;
for (i=0; i<=maxShade; ++i)
    {
    struct rgbColor rgb;
    int level = 255 - (255*i/maxShade);
    if (level < 0) level = 0;
    rgb.r = rgb.g = rgb.b = level;
    rgb.a = 255;
    shadesOfGray[i] = hvGfxFindRgb(hvg, &rgb);
    }
shadesOfGray[maxShade+1] = MG_RED;
}

void mgMakeColorGradient(struct memGfx *mg,
                         struct rgbColor *start, struct rgbColor *end,
                         int steps, Color *colorIxs)
/* Make a color gradient that goes smoothly from start
 * to end colors in given number of steps.  Put indices
 * in color table in colorIxs */
{
double scale = 0, invScale;
double invStep;
int i;
int r,g,b;

steps -= 1;	/* Easier to do the calculation in an inclusive way. */
invStep = 1.0/steps;
for (i=0; i<=steps; ++i)
    {
    invScale = 1.0 - scale;
    r = invScale * start->r + scale * end->r;
    g = invScale * start->g + scale * end->g;
    b = invScale * start->b + scale * end->b;
    colorIxs[i] = mgFindColor(mg, r, g, b);
    scale += invStep;
    }
}

void makeRedGreenShades(struct memGfx *mg)
/* Allocate the  shades of Red, Green and Blue */
{
static struct rgbColor black = {0, 0, 0, 255};
static struct rgbColor red = {255, 0, 0, 255};
mgMakeColorGradient(mg, &black, &red, maxRGBShade+1, shadesOfRed);
exprBedColorsMade = TRUE;
}

char *altGraphXMakeImage(struct trackDb *tdb, struct altGraphX *ag)
/* Create a drawing of splicing pattern. */
{
MgFont *font = mgSmallFont();
int fontHeight = mgFontLineHeight(font);
struct spaceSaver *ssList = NULL;
struct hash *heightHash = NULL;
int rowCount = 0;
struct tempName gifTn;
int pixWidth = atoi(cartUsualString(cart, "pix", DEFAULT_PIX_WIDTH ));
int pixHeight = 0;
struct hvGfx *hvg;
int lineHeight = 0;
double scale = 0;

scale = (double)pixWidth/(ag->tEnd - ag->tStart);
lineHeight = 2 * fontHeight +1;
altGraphXLayout(ag, ag->tStart, ag->tEnd, scale, 100, &ssList, &heightHash, &rowCount);
pixHeight = rowCount * lineHeight;
trashDirFile(&gifTn, "hgc", "hgc", ".png");
hvg = hvGfxOpenPng(pixWidth, pixHeight, gifTn.forCgi, FALSE);
makeGrayShades(hvg);
hvGfxSetClip(hvg, 0, 0, pixWidth, pixHeight);
altGraphXDrawPack(ag, ssList, hvg, 0, 0, pixWidth, lineHeight, lineHeight-1,
		  ag->tStart, ag->tEnd, scale, font, MG_BLACK, shadesOfGray, "Dummy", NULL);
hvGfxUnclip(hvg);
hvGfxClose(&hvg);
printf(
       "<IMG SRC = \"%s\" BORDER=1 WIDTH=%d HEIGHT=%d><BR>\n",
       gifTn.forHtml, pixWidth, pixHeight);
return cloneString(gifTn.forHtml);
}

char *agXStringForEdge(struct altGraphX *ag, int i)
/* classify an edge as intron or exon */
{
if(ag->vTypes[ag->edgeStarts[i]] == ggSoftStart ||
   ag->vTypes[ag->edgeStarts[i]] == ggHardStart)
    return "exon";
else if (ag->vTypes[ag->edgeStarts[i]] == ggSoftEnd ||
	 ag->vTypes[ag->edgeStarts[i]] == ggHardEnd)
    return "intron";
else
    return "unknown";
}

char *agXStringForType(enum ggVertexType t)
/* convert a type to a string */
{
switch (t)
    {
    case ggSoftStart:
	return "ss";
    case ggHardStart:
	return "hs";
    case ggSoftEnd:
	return "se";
    case ggHardEnd:
	return "he";
    default:
	return "NA";
    }
}

void printAltGraphXEdges(struct altGraphX *ag)
/* Print out at table showing all of the vertexes and
   edges of an altGraphX. */
{
int i = 0, j = 0;
printf("<table cellpadding=1 border=1>\n");
printf("</table>\n");
printf("<table cellpadding=0 cellspacing=0>\n");
printf("<tr><th><b>Vertices</b></th><th><b>Edges</b></th></tr>\n");
printf("<tr><td valign=top>\n");
printf("<table cellpadding=1 border=1>\n");
printf("<tr><th><b>Number</b></th><th><b>Type</b></th></tr>\n");
for(i=0; i<ag->vertexCount; i++)
    {
    printf("<tr><td>%d</td><td>%s</td></tr>\n", i, agXStringForType(ag->vTypes[i]));
    }
printf("</table>\n");
printf("</td><td valign=top>\n");
printf("<table cellpadding=1 border=1>\n");
printf("<tr><th><b>Start</b></th><th><b>End</b></th><th><b>Type</b></th><th><b>Evidence</b></th></tr>\n");
for(i=0; i<ag->edgeCount; i++)
    {
    struct evidence *e =  slElementFromIx(ag->evidence, i);
    printf("<tr><td>%d</td><td>%d</td>",           ag->edgeStarts[i], ag->edgeEnds[i]);
    printf("<td><a href=\"%s&position=%s:%d-%d&mrna=full&intronEst=full&refGene=full&altGraphX=full\">%s</a></td><td>",
           hgTracksPathAndSettings(),
           ag->tName,
           ag->vPositions[ag->edgeStarts[i]],
	   ag->vPositions[ag->edgeEnds[i]],
	   agXStringForEdge(ag, i));
    for(j=0; j<e->evCount; j++)
	printf("%s, ", ag->mrnaRefs[e->mrnaIds[j]]);
    printf("</td></tr>\n");
    }
printf("</table>\n");
}

void doAltGraphXDetails(struct trackDb *tdb, char *item)
/* do details page for an altGraphX */
{
int id = atoi(item);
char query[256];
struct altGraphX *ag = NULL;
struct altGraphX *orthoAg = NULL;
char buff[128];
struct sqlConnection *conn = hAllocConn(database);
char *image = NULL;

/* Load the altGraphX record and start page. */
if (id != 0)
    {
    sqlSafef(query, sizeof(query),"select * from %s where id=%d", tdb->table, id);
    ag = altGraphXLoadByQuery(conn, query);
    }
else
    {
    sqlSafef(query, sizeof(query),
             "select * from %s where tName like '%s' and tStart <= %d and tEnd >= %d",
             tdb->table, seqName, winEnd, winStart);
    ag = altGraphXLoadByQuery(conn, query);
    }
if (ag == NULL)
    errAbort("hgc::doAltGraphXDetails() - couldn't find altGraphX with id=%d", id);
genericHeader(tdb, ag->name);
printPosOnChrom(ag->tName, ag->tStart, ag->tEnd, ag->strand, FALSE, NULL);

/* Print a display of the Graph. */
printf("<b>Plots of Alt-Splicing:</b>");
printf("<center>\n");
if(sameString(tdb->table, "altGraphXPsb2004"))
    printf("Common Splicing<br>");
printf("Alt-Splicing drawn to scale.<br>");
image = altGraphXMakeImage(tdb,ag);
freez(&image);
/* Normally just print graph with exons scaled up. For conserved
   track also display orthologous loci. */
if(differentString(tdb->table, "altGraphXPsb2004"))
    {
    struct altGraphX *copy = altGraphXClone(ag);
    altGraphXVertPosSort(copy);
    altGraphXEnlargeExons(copy);
    printf("<br>Alt-Splicing drawn with exons enlarged.<br>\n");
    image = altGraphXMakeImage(tdb,copy);
    freez(&image);
    altGraphXFree(&copy);
    }
else
    {
    struct sqlConnection *orthoConn = NULL;
    struct altGraphX *origAg = NULL;
    char *db2="mm3";
    sqlSafef(query, sizeof(query), "select * from altGraphX where name='%s'", ag->name);
    origAg = altGraphXLoadByQuery(conn, query);
    puts("<br><center>Human</center>\n");
    altGraphXMakeImage(tdb,origAg);
    orthoConn = hAllocConn(db2);
    sqlSafef(query, sizeof(query), "select orhtoAgName from orthoAgReport where agName='%s'", ag->name);
    sqlQuickQuery(conn, query, buff, sizeof(buff));
    sqlSafef(query, sizeof(query), "select * from altGraphX where name='%s'", buff);
    orthoAg = altGraphXLoadByQuery(orthoConn, query);
    if(differentString(orthoAg->strand, origAg->strand))
	{
	altGraphXReverseComplement(orthoAg);
	puts("<br>Mouse (opposite strand)\n");
	}
    else
	puts("<br>Mouse\n");
    printf("<a HREF=\"%s&db=%s&position=%s:%d-%d&mrna=squish&intronEst=squish&refGene=pack&altGraphX=full\"",
	   hgTracksName(),
	   "mm3", orthoAg->tName, orthoAg->tStart, orthoAg->tEnd);
    printf(" ALT=\"Zoom to browser coordinates of altGraphX\">");
    printf("<span style='font-size:smaller;'>[%s.%s:%d-%d]</span></a><br><br>\n", "mm3",
	   orthoAg->tName, orthoAg->tStart, orthoAg->tEnd);
    altGraphXMakeImage(tdb,orthoAg);
    }
printf("<br><a HREF=\"%s&position=%s:%d-%d&mrna=full&intronEst=full&refGene=full&altGraphX=full\"",
       hgTracksPathAndSettings(), ag->tName, ag->tStart, ag->tEnd);
printf(" ALT=\"Zoom to browser coordinates of Alt-Splice\">");
printf("Jump to browser for %s</a><span style='font-size:smaller;'> [%s:%d-%d] </span><br><br>\n",
       ag->name, ag->tName, ag->tStart, ag->tEnd);
if(cgiVarExists("agxPrintEdges"))
    printAltGraphXEdges(ag);
printf("</center>\n");
hFreeConn(&conn);
}


struct lineFile *openExtLineFile(unsigned int extFileId)
/* Open line file corresponding to id in extFile table. */
{
char *path = hExtFileName(database, "extFile", extFileId);
struct lineFile *lf = lineFileOpen(path, TRUE);
freeMem(path);
return lf;
}

void printSampleWindow( struct psl *thisPsl, int thisWinStart, int
                        thisWinEnd, char *winStr, char *otherOrg, char *otherDb,
			char *pslTableName )
{
char otherString[256];
char pslItem[1024];

safef(pslItem, sizeof pslItem, "%s:%d-%d %s:%d-%d", 
    thisPsl->qName, thisPsl->qStart, thisPsl->qEnd, thisPsl->tName, thisPsl->tStart, thisPsl->tEnd );
safef(otherString, sizeof otherString, "%d&pslTable=%s&otherOrg=%s&otherChromTable=%s&otherDb=%s", thisPsl->tStart,
	pslTableName, otherOrg, "chromInfo" , otherDb );
if (pslTrimToTargetRange(thisPsl, thisWinStart, thisWinEnd) != NULL)
    {
    hgcAnchorWindow("htcLongXenoPsl2", pslItem, thisWinStart,
		    thisWinEnd, otherString, thisPsl->tName);
    printf("%s</A>\n", winStr );
    }
}


void firstAndLastPosition( int *thisStart, int *thisEnd, struct psl *thisPsl )
/*return the first and last base of a psl record (not just chromStart
 * and chromEnd but the actual blocks.*/
{
*thisStart = thisPsl->tStarts[0];
*thisEnd = thisPsl->tStarts[thisPsl->blockCount - 1];
if( thisPsl->strand[1] == '-' )
    {
    *thisStart = thisPsl->tSize - *thisStart;
    *thisEnd = thisPsl->tSize - *thisEnd;
    }
*thisEnd += thisPsl->blockSizes[thisPsl->blockCount - 1];
}

boolean sampleClickRelevant( struct sample *smp, int i, int left, int right,
			     int humMusWinSize, int thisStart, int thisEnd )
/* Decides if a sample is relevant for the current window and psl
 * record start and end positions */
{

if ((smp->chromStart + smp->samplePosition[i] - humMusWinSize / 2 + 1) < left
&&  (smp->chromStart + smp->samplePosition[i] + humMusWinSize / 2    ) < left )
    return(0);

if( smp->chromStart + smp->samplePosition[i] -
                                                humMusWinSize / 2  + 1  < thisStart
&&  (smp->chromStart + smp->samplePosition[i] + humMusWinSize / 2     ) < thisStart  )
    return(0);

if ((smp->chromStart + smp->samplePosition[i] -  humMusWinSize / 2 + 1) > right
&&  (smp->chromStart + smp->samplePosition[i] +  humMusWinSize / 2    ) > right )
    return(0);


if( smp->chromStart + smp->samplePosition[i] -
                                                humMusWinSize / 2 + 1  > thisEnd
    && smp->chromStart + smp->samplePosition[i] +
                                                humMusWinSize / 2      > thisEnd  )
    return(0);

return(1);
}

static double whichNum( double tmp, double min0, double max0, int n)
/*gets range nums. from bin values*/
{
return( (max0 - min0)/(double)n * tmp + min0 );
}

void humMusSampleClick(struct sqlConnection *conn, struct trackDb *tdb,
		       char *item, int start, int smpSize, char *otherOrg, char *otherDb,
		       char *pslTableName, boolean printWindowFlag )
/* Handle click in humMus sample (wiggle) track. */
{
int humMusWinSize = 50;
int i;
char table[HDB_MAX_TABLE_STRING];
boolean hasBin;
struct sample *smp;
char query[512];
char tempTableName[1024];
struct sqlResult *sr;
char **row;
char **pslRow;
boolean firstTime = TRUE;
struct psl *thisPsl;
char str[256];
char thisItem[256];
char otherString[256] = "";
struct sqlResult *pslSr;
struct sqlConnection *conn2 = hAllocConn(database);
int thisStart, thisEnd;
int left = cartIntExp( cart, "l" );
int right = cartIntExp( cart, "r" );
char *winOn = cartUsualString( cart, "win", "F" );

if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, &hasBin))
    errAbort("track %s not found", tdb->table);
sqlSafef(query, sizeof query, "select * from %s where name = '%s' and chrom = '%s'",
	table, item, seqName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (firstTime)
	firstTime = FALSE;
    else
	htmlHorizontalLine();
    smp = sampleLoad(row+hasBin);
    safef(tempTableName, sizeof tempTableName, "%s_%s", smp->chrom, pslTableName );
    if (!hFindSplitTable(database, seqName, pslTableName, table, sizeof table, &hasBin))
	errAbort("table %s not found", pslTableName);
    sqlSafef(query, sizeof query, "select * from %s where tName = '%s' and tEnd >= %d and tStart <= %d"
	    , table, smp->chrom, smp->chromStart+smp->samplePosition[0]
	    , smp->chromStart+smp->samplePosition[smp->sampleCount-1] );

    pslSr = sqlGetResult(conn2, query);
    if(!sameString(winOn,"T"))
	{
	while(( pslRow = sqlNextRow(pslSr)) != NULL )
	    {
	    thisPsl = pslLoad( pslRow+hasBin );
	    firstAndLastPosition( &thisStart, &thisEnd, thisPsl );
	    snprintf(thisItem, 256, "%s:%d-%d %s:%d-%d", thisPsl->qName,
		     thisPsl->qStart, thisPsl->qEnd, thisPsl->tName,
		     thisPsl->tStart, thisPsl->tEnd );
	    longXenoPsl1Given(tdb, thisItem, otherOrg, "chromInfo",
			      otherDb, thisPsl, pslTableName );
	    safef(otherString, sizeof otherString, "%d&win=T", thisPsl->tStart );
	    hgcAnchorSomewhere( tdb->track, item, otherString, thisPsl->tName );
	    printf("View individual alignment windows\n</a>");
	    printf("<br><br>");
	    }
	}
    else
	{
	cartSetString( cart, "win", "F" );
	printf("<h3>Alignments Windows </h3>\n"
	       "<b>start&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;stop"
	       "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;L-score</b><br>" );
	while(( pslRow = sqlNextRow(pslSr)) != NULL )
	    {
	    thisPsl = pslLoad( pslRow+hasBin );
	    firstAndLastPosition( &thisStart, &thisEnd, thisPsl );
	    for( i=0; i<smp->sampleCount; i++ )
		{
		if( !sampleClickRelevant( smp, i, left, right, humMusWinSize,
					  thisStart, thisEnd ) )
		    continue;
                snprintf( str, 256,
			  "%d&nbsp;&nbsp;&nbsp;&nbsp;%d&nbsp;&nbsp;&nbsp;&nbsp;%g<br>",
			  max( smp->chromStart + smp->samplePosition[i] -
			       humMusWinSize / 2 + 1, thisStart + 1),
			  min(smp->chromStart +  smp->samplePosition[i] +
			      humMusWinSize / 2, thisEnd ),
			  whichNum(smp->sampleHeight[i],0.0,8.0,1000) );
                /* 0 to 8.0 is the fixed total L-score range for
                 * all these conservation tracks. Scores outside
                 * this range are truncated. */
		printSampleWindow( thisPsl,
				   smp->chromStart + smp->samplePosition[i] -
				   humMusWinSize / 2,
				   smp->chromStart + smp->samplePosition[i] +
				   humMusWinSize / 2,
				   str, otherOrg, otherDb, pslTableName );
		}
	    printf("<br>");
	    }
        }
    }
}

void footPrinterSampleClick(struct sqlConnection *conn, struct trackDb *tdb,
                            char *item, int start, int smpSize)
/* Handle click in humMus sample (wiggle) track. */
{
char table[HDB_MAX_TABLE_STRING];
boolean hasBin;
struct sample *smp;
char query[512];
char tempTableName[1024];
struct sqlResult *sr;
char **row;
boolean firstTime = TRUE;
char filename[10000];
char pslTableName[128] = "blastzBestMouse";
int offset;
int motifid;

if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, &hasBin))
    errAbort("track %s not found", tdb->table);
sqlSafef(query, sizeof query, "select * from %s where name = '%s'",
	table, item);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (firstTime)
	firstTime = FALSE;
    else
	htmlHorizontalLine();
    smp = sampleLoad(row+hasBin);

    sscanf(smp->name,"footPrinter.%d.%d",&offset,&motifid);
    safef(filename, sizeof filename, "../zoo_blanchem/new_raw2_offset%d.fa.main.html?motifID=%d", offset, motifid);

    safef(tempTableName, sizeof tempTableName,"%s_%s", smp->chrom, pslTableName );
    if (!hFindSplitTable(database, seqName, pslTableName, table, sizeof table, &hasBin))
	errAbort("table %s not found", pslTableName);
    sqlSafef(query, sizeof query, "select * from %s where tName = '%s' and tEnd >= %d and tStart <= %d" ,
	    table, smp->chrom, smp->chromStart+smp->samplePosition[0],
	    smp->chromStart+smp->samplePosition[smp->sampleCount-1] );

    printf("Content-Type: text/html\n\n<HTML><BODY><SCRIPT>\n");
    printf("location.replace('%s')\n",filename);
    printf("</SCRIPT> <NOSCRIPT> No JavaScript support. "
           "Click <b><a href=\"%s\">continue</a></b> for "
           "the requested GenBank report. </NOSCRIPT>\n",
           filename);
    }
}

void humMusClickHandler(struct trackDb *tdb, char *item,
        char *targetName, char *targetDb, char *targetTable, boolean printWindowFlag )
/* Put up sample track info. */
{
char *words[16], *dupe = cloneString(tdb->type);
int num;
int wordCount;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);

genericHeader(tdb, item);
wordCount = chopLine(dupe, words);
if (wordCount > 0)
    {
    num = 0;
    if (wordCount > 1)
	num = atoi(words[1]);
    if (num < 3) num = 3;
        humMusSampleClick( conn, tdb, item, start, num, targetName, targetDb, targetTable, printWindowFlag );
    }
printTrackHtml(tdb);
freez(&dupe);
hFreeConn(&conn);
}

void footPrinterClickHandler(struct trackDb *tdb, char *item )
/* Put up generic track info. */
{
char *words[16], *dupe = cloneString(tdb->type);
int num;
int wordCount;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);

wordCount = chopLine(dupe, words);
if (wordCount > 0)
    {
    num = 0;
    if (wordCount > 1)
	num = atoi(words[1]);
    if (num < 3) num = 3;
    footPrinterSampleClick(conn, tdb, item, start, num);
    }
printTrackHtml(tdb);
freez(&dupe);
hFreeConn(&conn);
}

void bigPslHandlingCtAndGeneric(struct sqlConnection *conn, struct trackDb *tdb,
                     char *item, int start, int end)
/* Special option to show all alignments for blat ct psl */
{

if (startsWith("ct_blat", tdb->track) && !sameString(item,"PrintAllSequences"))
    printf("<A href id='genericPslShowAllLink'>Show All</A>\n");

printf("<div id=genericPslShowItem style='display: block'>\n");
genericBigPslClick(NULL, tdb, item, start, end);
printf("</div>\n");

if (startsWith("ct_blat", tdb->track) && !sameString(item,"PrintAllSequences"))
    {
    printf("<div id=genericPslShowAll style='display: none'>\n");
    genericBigPslClick(NULL, tdb, "PrintAllSequences", start, end);

    printf("</div>\n");

    jsInline("$('#genericPslShowAllLink').click(function(){\n"
	    "  $('#genericPslShowAll')[0].style.display = 'block';\n"
	    "  $('#genericPslShowItem')[0].style.display = 'none';\n"
	    "  $('#genericPslShowAllLink')[0].style.display = 'none';\n"
	    "return false;\n"
	    "});\n");
    }
}

void hgCustom(char *trackId, char *fileItem)
/* Process click on custom track. */
{
char *fileName, *itemName;
struct customTrack *ctList = getCtList();
struct customTrack *ct;
struct bed *bed = (struct bed *)NULL;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
char *item = cartString(cart, "i");
char *type;
fileName = nextWord(&fileItem);
for (ct = ctList; ct != NULL; ct = ct->next)
    if (sameString(trackId, ct->tdb->track))
	break;
if (ct == NULL)
    errAbort("Couldn't find '%s' in '%s'", trackId, fileName);
type = ct->tdb->type;
cartWebStart(cart, database, "Custom Track: %s", ct->tdb->shortLabel);
itemName = skipLeadingSpaces(fileItem);
printf("<H2>%s</H2>\n", ct->tdb->longLabel);
if (sameWord(type, "array"))
    doExpRatio(ct->tdb, fileItem, ct);
else if ( startsWith( "bedMethyl", type))
    doBedMethyl(ct->tdb, item);
else if ( startsWith( "longTabix", type))
    doLongTabix(ct->tdb, item);
else if (sameWord(type, "encodePeak"))
    doEncodePeak(ct->tdb, ct, fileName);
else if (sameWord(type, "bigNarrowPeak"))
    doBigEncodePeak(ct->tdb, NULL, item);
else if (sameWord(type, "bigWig"))
    bigWigCustomClick(ct->tdb);
else if (sameWord(type, "bigChain"))
    genericChainClick(NULL, ct->tdb, item, start, "seq");
else if (sameWord(type, "bigPsl"))
     bigPslHandlingCtAndGeneric(NULL, ct->tdb, item, start, end);
else if (sameWord(type, "bigMaf"))
    genericMafClick(NULL, ct->tdb, item, start);
else if (sameWord(type, "bigDbSnp"))
    doBigDbSnp(ct->tdb, item);
else if (sameWord(type, "bigBed") || sameWord(type, "bigGenePred") || sameWord(type, "bigLolly") || sameWord(type, "bigMethyl"))
    bigBedCustomClick(ct->tdb);
else if (startsWith("bigRmsk", type))
    doBigRmskRepeat(ct->tdb, item);
else if (sameWord(type, "bigBarChart") || sameWord(type, "barChart"))
    doBarChartDetails(ct->tdb, item);
else if (sameWord(type, "bigInteract") || sameWord(type, "interact"))
    doInteractDetails(ct->tdb, item);
else if (sameWord(type, "bam") || sameWord(type, "cram"))
    doBamDetails(ct->tdb, itemName);
else if (sameWord(type, "vcfTabix") || sameWord(type, "vcfPhasedTrio"))
    doVcfTabixDetails(ct->tdb, itemName);
else if (sameWord(type, "vcf"))
    doVcfDetails(ct->tdb, itemName);
else if (sameWord(type, "makeItems"))
    doMakeItemsDetails(ct, fileName);	// fileName is first word, which is, go figure, id
else if (ct->wiggle)
    {
    if (ct->dbTrack)
	{
	struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
	genericWiggleClick(conn, ct->tdb, fileItem, start);
	hFreeConn(&conn);
	}
    else
	genericWiggleClick(NULL, ct->tdb, fileItem, start);
    /*	the NULL is for conn, don't need that for custom tracks */
    }
else if (ct->dbTrack && startsWith("bedGraph", ct->dbTrackType))
    {
    printf("<P><A HREF=\"../cgi-bin/hgTables?db=%s&hgta_group=%s&hgta_track=%s"
           "&hgta_table=%s&position=%s:%d-%d&"
           "hgta_doSchema=describe+table+schema\" TARGET=_BLANK>"
           "View table schema</A></P>\n",
           database, ct->tdb->grp, ct->tdb->table, ct->tdb->table,
           seqName, winStart+1, winEnd);
    }
else if (ct->dbTrack && sameString(ct->dbTrackType, "maf"))
    {
    struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
    struct sqlConnection *conn2 = hAllocConn(CUSTOM_TRASH);
    char *saveTable = ct->tdb->table;
    char *saveTrack = ct->tdb->track;
    ct->tdb->table = ct->tdb->track = ct->dbTableName;
    customMafClick(conn, conn2, ct->tdb);
    ct->tdb->table = saveTable;
    ct->tdb->track = saveTrack;
    hFreeConn(&conn2);
    hFreeConn(&conn);
    }
else if (ct->dbTrack && sameWord(type, "bedDetail"))
    {
    doBedDetail(ct->tdb, ct, itemName);
    }
else if (ct->dbTrack && sameWord(type, "pgSnp"))
    {
    doPgSnp(ct->tdb, itemName, ct);
    }
else
    {
    if (ct->dbTrack)
	{
	int rowOffset;
	char **row;
	struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
	struct sqlResult *sr = NULL;
	int start = cartInt(cart, "o");
	int end = cartInt(cart, "t");

	struct dyString *where = sqlDyStringCreate("chromStart = '%d' and chromEnd = '%d'", start, end);
	if (ct->fieldCount >= 4)
	    {
	    sqlDyStringPrintf(where, " and name = '%s'", itemName);
	    }
	sr = hRangeQuery(conn, ct->dbTableName, seqName, start, end,
                     dyStringContents(where), &rowOffset);
        dyStringFree(&where);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    bedFree(&bed);
	    bed = bedLoadN(row+rowOffset, ct->fieldCount);
	    }
	sqlFreeResult(&sr);
	hFreeConn(&conn);
	}
    if (ct->fieldCount < 4)
	{
	if (! ct->dbTrack)
	    {
	    for (bed = ct->bedList; bed != NULL; bed = bed->next)
		if (bed->chromStart == start && sameString(seqName, bed->chrom))
		    break;
	    }
	if (bed)
	    printPos(bed->chrom, bed->chromStart, bed->chromEnd, NULL,
		TRUE, NULL);
	if (ct->dbTrack)
	    printUpdateTime(CUSTOM_TRASH, ct->tdb, ct);
	printTrackHtml(ct->tdb);
	return;
	}
    else
	{
	if (! ct->dbTrack)
	    {
	    for (bed = ct->bedList; bed != NULL; bed = bed->next)
		if (bed->chromStart == start && sameString(seqName, bed->chrom))
		    if (bed->name == NULL || sameString(itemName, bed->name) )
			break;
	    }
	}
    if (bed == NULL)
	errAbort("Couldn't find %s@%s:%d in %s", itemName, seqName,
		start, fileName);
    printCustomUrl(ct->tdb, itemName, TRUE);
    bedPrintPos(bed, ct->fieldCount, NULL);
    }
printTrackUiLink(ct->tdb);
printUpdateTime(CUSTOM_TRASH, ct->tdb, ct);
printTrackHtml(ct->tdb);
}

void blastProtein(struct trackDb *tdb, char *itemName)
/* Show protein to translated dna alignment for accession. */
{
char startBuf[64], endBuf[64];
int start = cartInt(cart, "o");
boolean isClicked;
struct psl *psl = 0;
struct sqlResult *sr = NULL;
struct sqlConnection *conn = hAllocConn(database);
char query[256], **row;
struct psl* pslList = getAlignments(conn, tdb->table, itemName);
char *useName = itemName;
char *acc = NULL, *prot = NULL;
char *gene = NULL, *pos = NULL;
char *ptr;
char *buffer;
char *spAcc;
boolean isCe = FALSE;
boolean isDm = FALSE;
boolean isSacCer = FALSE;
char *pred = trackDbSettingOrDefault(tdb, "pred", "NULL");
char *blastRef = trackDbSettingOrDefault(tdb, "blastRef", "NULL");

if (sameString("blastSacCer1SG", tdb->table))
    isSacCer = TRUE;
if (startsWith("blastDm", tdb->table))
    isDm = TRUE;
if (startsWith("blastCe", tdb->table))
    isCe = TRUE;
buffer = needMem(strlen(itemName)+ 1);
strcpy(buffer, itemName);
acc = buffer;
if (blastRef != NULL)
    {
    char *thisDb = cloneString(blastRef);
    char *table;

    if ((table = strchr(thisDb, '.')) != NULL)
	{
	*table++ = 0;
	if (hTableExists(thisDb, table))
	    {
	    if (!isCe && (ptr = strchr(acc, '.')))
		*ptr = 0;
	    sqlSafef(query, sizeof(query), "select geneId, extra1, refPos from %s where acc = '%s'", blastRef, acc);
	    sr = sqlGetResult(conn, query);
	    if ((row = sqlNextRow(sr)) != NULL)
		{
		useName = row[0];
		prot = row[1];
		pos = row[2];
		}
	    sqlFreeResult(&sr);
	    }
        }
    }
else if ((pos = strchr(acc, '.')) != NULL)
    {
    *pos++ = 0;
    if ((gene = strchr(pos, '.')) != NULL)
	{
	*gene++ = 0;
	useName = gene;
	if (!isDm && !isCe && ((prot = strchr(gene, '.')) != NULL))
	    *prot++ = 0;
	}
    }
if (isDm == TRUE)
    cartWebStart(cart, database, "FlyBase Protein %s", useName);
else if (isSacCer == TRUE)
    cartWebStart(cart, database, "Yeast Protein %s", useName);
else if (isCe == TRUE)
    cartWebStart(cart, database, "C. elegans Protein %s", useName);
else
    cartWebStart(cart, database, "Human Protein %s", useName);
if (pos != NULL)
    {
    if (isDm == TRUE)
	{
	char *dmDb = cloneString(strchr(tdb->track, 'D'));

	*dmDb = tolower(*dmDb);
	*strchr(dmDb, 'F') = 0;

	printf("<B>D. melanogaster position:</B>\n");
	printf("<A TARGET=_blank HREF=\"%s?position=%s&db=%s\">",
	    hgTracksName(), pos, dmDb);
	}
    else if (isCe == TRUE)
	{
	char *assembly;
	if (sameString("blastWBRef01", tdb->table))
	    assembly = "ce3";
	else if (sameString("blastCe9SG", tdb->table))
	    assembly = "ce9";
	else if (sameString("blastCe6SG", tdb->table))
	    assembly = "ce6";
	else if (sameString("blastCe4SG", tdb->table))
	    assembly = "ce4";
	else
	    assembly = "ce3";
	printf("<B>C. elegans position:</B>\n");
	printf("<A TARGET=_blank HREF=\"%s?position=%s&db=%s\">",
	    hgTracksName(), pos, assembly);
	}
    else if (isSacCer == TRUE)
	{
	char *assembly = "sacCer1";
	printf("<B>Yeast position:</B>\n");
	printf("<A TARGET=_blank HREF=\"%s?position=%s&db=%s\">",
	    hgTracksName(), pos, assembly);
	}
    else
	{
	char *assembly;
	if (sameString("blastHg16KG", tdb->table))
	    assembly = "hg16";
	else if (sameString("blastHg17KG", tdb->table))
	    assembly = "hg17";
	else
	    assembly = "hg18";
	printf("<B>Human position:</B>\n");
	printf("<A TARGET=_blank HREF=\"%s?position=%s&db=%s\">",
	    hgTracksName(), pos, assembly);
	}
    printf("%s</A><BR>",pos);
    }
if (acc != NULL)
    {
    if (isDm== TRUE)
	printf("<B>FlyBase Entry:</B> <A HREF=\" %s%s", tdb->url, acc);
    else if (isSacCer== TRUE)
	printf("<B>SGD Entry:</B> <A HREF=\" %s%s", tdb->url, acc);
    else if (isCe == TRUE)
	printf("<B>Wormbase ORF Name:</B> <A HREF=\" %s%s", tdb->url, acc);
    else
	{
	printf("<B>Human mRNA:</B> <A HREF=\"");
	printEntrezNucleotideUrl(stdout, acc);
	}
    printf("\" TARGET=_blank>%s</A><BR>\n", acc);
    }
if (!isDm && (prot != NULL) && !sameString("(null)", prot) && sqlTableExists(conn,"proteome.uniProtAlias"))
    {
    printf("<B>UniProtKB:</B> ");
    printf("<A HREF=");
    printSwissProtProteinUrl(stdout, prot);

    spAcc = uniProtFindPrimAcc(prot);
    if (spAcc == NULL)
        {
	printf(" TARGET=_blank>%s</A></B><BR>\n", prot);
        }
    else
        {
	printf(" TARGET=_blank>%s</A></B><BR>\n", spAcc);
        }
    }
printf("<B>Protein length:</B> %d<BR>\n",pslList->qSize);

slSort(&pslList, pslCmpMatch);
if (slCount(pslList) > 1)
    printf("<P>The alignment you clicked on is first in the table below.<BR>\n");
printf("<TT><PRE>");
printf("ALIGNMENT PEPTIDE COVERAGE IDENTITY  START END EXTENT  STRAND   LINK TO BROWSER \n");
printf("--------------------------------------------------------------------------------\n");
for (isClicked = 1; isClicked >= 0; isClicked -= 1)
    {
    for (psl = pslList; psl != NULL; psl = psl->next)
	{
	if (isPslToPrintByClick(psl, start, isClicked))
	    {
	    printf("<A HREF=\"%s&o=%d&g=htcProteinAli&i=%s&c=%s&l=%d&r=%d&db=%s&aliTable=%s&pred=%s\">",
		hgcPathAndSettings(), psl->tStart, psl->qName,  psl->tName,
		psl->tStart, psl->tEnd, database,tdb->track, pred);
	    printf("alignment</A> ");
	    printf("<A HREF=\"%s&o=%d&g=htcGetBlastPep&i=%s&c=%s&l=%d&r=%d&db=%s&aliTable=%s\">",
	           hgcPathAndSettings(), psl->tStart, psl->qName,  psl->tName,
                   psl->tStart, psl->tEnd, database,tdb->track);
            printf("peptide</A> ");
            printf("%5.1f%%    %5.1f%% %5d %5d %5.1f%%    %c   ",
                   100.0 * (psl->match + psl->repMatch + psl->misMatch) / psl->qSize,
                   100.0 * (psl->match + psl->repMatch) / (psl->match + psl->repMatch + psl->misMatch),
                   psl->qStart+1, psl->qEnd,
                   100.0 * (psl->qEnd - psl->qStart) / psl->qSize, psl->strand[1]);
            printf("<A HREF=\"%s&position=%s:%d-%d&db=%s&ss=%s+%s\">",
                   hgTracksPathAndSettings(),
                   psl->tName, psl->tStart + 1, psl->tEnd, database,
		   tdb->track, itemName);
	    sprintLongWithCommas(startBuf, psl->tStart + 1);
	    sprintLongWithCommas(endBuf, psl->tEnd);
	    printf("%s:%s-%s</A> <BR>",psl->tName,startBuf, endBuf);
	    if (isClicked)
		printf("\n");
	    }
	}
    }
    printf("</PRE></TT>");
    /* Add description */
    printTrackHtml(tdb);
    hFreeConn(&conn);
}

static void doSgdOther(struct trackDb *tdb, char *item)
/* Display information about other Sacchromyces Genome Database
 * other (not-coding gene) info. */
{
struct sqlConnection *conn = hAllocConn(database);
struct dyString *dy = dyStringNew(1024);
if (sqlTableExists(conn, "sgdOtherDescription"))
    {
    /* Print out description and type if available. */
    struct sgdDescription sgd;
    struct sqlResult *sr;
    char query[256], **row;
    sqlSafef(query, sizeof(query),
          "select * from sgdOtherDescription where name = '%s'", item);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	sgdDescriptionStaticLoad(row, &sgd);
	dyStringPrintf(dy, "<B>Description:</B> %s<BR>\n", sgd.description);
	dyStringPrintf(dy, "<B>Type:</B> %s<BR>\n", sgd.type);
	}
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
genericClickHandlerPlus(tdb, item, NULL, dy->string);
dyStringFree(&dy);
}


void doPeptideAtlas(struct trackDb *tdb, char *item)
/* PeptideAtlas item details display. Peptide details are in hgFixed.<table>Peptides */
{
char query[512];
struct sqlResult *sr;
char **row;

int start = cartInt(cart, "o");
int end = cartInt(cart, "t");

genericHeader(tdb, item);
printCustomUrl(tdb, item, FALSE);
struct sqlConnection *conn = hAllocConn(database);
char peptideTable[128];
safef(peptideTable, sizeof(peptideTable), "%sPeptides", tdb->table);

// peptide info
struct sqlConnection *connFixed= hAllocConn("hgFixed");
if (sqlTableExists(connFixed, peptideTable))
    {
    sqlSafef(query, sizeof(query), "select * from %s where accession = '%s'", peptideTable, item);
    sr = sqlGetResult(connFixed, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
        struct peptideAtlasPeptide *peptideInfo;
        AllocVar(peptideInfo);
        peptideAtlasPeptideStaticLoad(row, peptideInfo);
        printf("<b>Peptide sequence:</b> %s<br>\n", peptideInfo->sequence);
        printf("<b>Peptide size:</b> %d<br>\n", (int)strlen(peptideInfo->sequence));
        printf("<b>Samples where observed:</b> %d<br>\n", peptideInfo->sampleCount);
        printf("<b>Proteotypic score:</b> %.3f<br>\n", peptideInfo->proteotypicScore);
        printf("<b>Hydrophobicity:</b> %.3f<br><br>\n", peptideInfo->hydrophobicity);
        }
    sqlFreeResult(&sr);
    }
hFreeConn(&connFixed);

// peptide mappings
sqlSafef(query, sizeof(query), "select * from %s where name = '%s' order by chrom, chromStart, chromEnd", 
        tdb->table, item);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
struct bed *bed = NULL, *beds = NULL;
int rowOffset = hOffsetPastBin(database, seqName, tdb->table);
while (row != NULL)
    {
    bed = bedLoadN(row + rowOffset, 12);
    if (sameString(bed->chrom, seqName) && bed->chromStart == start && bed->chromEnd == end)
        bedPrintPos(bed, 12, tdb);
    else
        slAddHead(&beds, bed);
    row = sqlNextRow(sr);
    }
if (beds != NULL)
    {
    slSort(&beds, bedCmp);
    printf("<br><b>Other mappings of this peptide:</b> %d<br>\n", slCount(beds));
    for (bed = beds; bed != NULL; bed = bed->next)
        {
        printf("&nbsp;&nbsp;&nbsp;&nbsp;"
               "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
               hgTracksPathAndSettings(), database, bed->chrom, bed->chromStart+1, bed->chromEnd);
        printf("%s:%d-%d</A><BR>\n", bed->chrom, bed->chromStart+1, bed->chromEnd);
        }
    }
hFreeConn(&conn);
printTrackHtml(tdb);
}

static void doSgdClone(struct trackDb *tdb, char *item)
/* Display information about other Sacchromyces Genome Database
 * other (not-coding gene) info. */
{
struct sqlConnection *conn = hAllocConn(database);
struct dyString *dy = dyStringNew(1024);

if (sqlTableExists(conn, "sgdClone"))
    {
    /* print out url with ATCC number */
    struct sgdClone sgd;
    struct sqlResult *sr;
    char query[256], **row;
    sqlSafef(query, sizeof(query),
          "select * from sgdClone where name = '%s'", item);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	sgdCloneStaticLoad(row+1, &sgd);
	dyStringPrintf(dy, "<B>ATCC catalog number:</B><A HREF=\"http://www.atcc.org/ATCCAdvancedCatalogSearch/ProductDetails/tabid/452/Default.aspx?ATCCNum=%s&Template=uniqueClones\" TARGET=_blank>%s</A><BR>\n", sgd.atccName, sgd.atccName);
	}
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
genericClickHandlerPlus(tdb, item,  NULL, dy->string);
dyStringFree(&dy);
}

static void doSimpleDiff(struct trackDb *tdb, char *otherOrg)
/* Print out simpleDiff info. */
{
struct simpleNucDiff snd;
struct sqlConnection *conn = hAllocConn(database);
char fullTable[HDB_MAX_TABLE_STRING];
char query[256], **row;
struct sqlResult *sr;
int rowOffset;
int start = cartInt(cart, "o");

genericHeader(tdb, NULL);
if (!hFindSplitTable(database, seqName, tdb->table, fullTable, sizeof fullTable, &rowOffset))
    errAbort("No %s table in database %s", tdb->table, database);
sqlSafef(query, sizeof(query), "select * from %s where chrom = '%s' and chromStart=%d",
    fullTable, seqName, start);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    simpleNucDiffStaticLoad(row + rowOffset, &snd);
    printf("<B>%s sequence:</B> %s<BR>\n", hOrganism(database), snd.tSeq);
    printf("<B>%s sequence:</B> %s<BR>\n", otherOrg, snd.qSeq);
    bedPrintPos((struct bed*)&snd, 3, tdb);
    printf("<BR>\n");
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
printTrackHtml(tdb);
}

static void doVntr(struct trackDb *tdb, char *item)
/* Perfect microsatellite repeats from VNTR program (Gerome Breen). */
{
struct vntr vntr;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
char extra[256];
int rowOffset = 0;
int start = cartInt(cart, "o");

genericHeader(tdb, item);
genericBedClick(conn, tdb, item, start, 4);
sqlSafef(extra, sizeof(extra), "chromStart = %d", start);
sr = hRangeQuery(conn, tdb->table, seqName, winStart, winEnd, extra,
		 &rowOffset);
if ((row = sqlNextRow(sr)) != NULL)
    {
    vntrStaticLoad(row + rowOffset, &vntr);
    printf("<B>Number of perfect repeats:</B> %.02f<BR>\n", vntr.repeatCount);
    printf("<B>Distance to last microsatellite repeat:</B> ");
    if (vntr.distanceToLast == -1)
	printf("n/a (first in chromosome)<BR>\n");
    else
	printf("%d<BR>\n", vntr.distanceToLast);
    printf("<B>Distance to next microsatellite repeat:</B> ");
    if (vntr.distanceToNext == -1)
	printf("n/a (last in chromosome)<BR>\n");
    else
	printf("%d<BR>\n", vntr.distanceToNext);
    if (isNotEmpty(vntr.forwardPrimer) &&
	! sameString("Design_Failed", vntr.forwardPrimer))
	{
	printf("<B>Forward PCR primer:</B> %s<BR>\n", vntr.forwardPrimer);
	printf("<B>Reverse PCR primer:</B> %s<BR>\n", vntr.reversePrimer);
	printf("<B>PCR product length:</B> %s<BR>\n", vntr.pcrLength);
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
printTrackHtml(tdb);
}

static void doZdobnovSynt(struct trackDb *tdb, char *item)
/* Gene homology-based synteny blocks from Zdobnov, Bork et al. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
char query[256];
int start = cartInt(cart, "o");
char fullTable[HDB_MAX_TABLE_STRING];
boolean hasBin = FALSE;

genericHeader(tdb, item);
genericBedClick(conn, tdb, item, start, 4);
if (!hFindSplitTable(database, seqName, tdb->table, fullTable, sizeof fullTable, &hasBin))
    errAbort("track %s not found", tdb->table);
sqlSafef(query, sizeof(query), "select * from %s where name = '%s'",
      fullTable, item);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    struct zdobnovSynt *zd = zdobnovSyntLoad(row + hasBin);
    int l = cgiInt("l");
    int r = cgiInt("r");
    int i = 0;
    puts("<B>Homologous gene names in window:</B>");
    for (i=0;  i < zd->blockCount;  i++)
	{
	int bStart = zd->chromStarts[i] + zd->chromStart;
	int bEnd = bStart + zd->blockSizes[i];
	if (bStart <= r && bEnd >= l)
	    {
	    printf(" %s", zd->geneNames[i]);
	    }
	}
    puts("");
    zdobnovSyntFree(&zd);
    }
else
    errAbort("query returned no results: \"%s\"", query);
sqlFreeResult(&sr);
hFreeConn(&conn);
printTrackHtml(tdb);
}


static void doDeweySynt(struct trackDb *tdb, char *item)
/* Gene homology-based synteny blocks from Dewey, Pachter. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
int start = cartInt(cart, "o");
char fullTable[HDB_MAX_TABLE_STRING];
boolean hasBin = FALSE;
struct bed *bed = NULL;
char query[512];

genericHeader(tdb, item);
if (!hFindSplitTable(database, seqName, tdb->table, fullTable, sizeof fullTable, &hasBin))
    errAbort("track %s not found", tdb->table);
sqlSafef(query, sizeof(query),
      "select * from %s where chrom = '%s' and chromStart = %d",
      fullTable, seqName, start);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    char *words[4];
    int wordCount = 0;
    bed = bedLoad6(row+hasBin);
    bedPrintPos(bed, 4, tdb);
    printf("<B>Strand:</B> %s<BR>\n", bed->strand);
    wordCount = chopByChar(bed->name, '.', words, ArraySize(words));
    if (wordCount == 3 && hDbExists(words[1]))
	{
	char *otherOrg = hOrganism(words[1]);
	printf("<A TARGET=\"_blank\" HREF=\"%s?db=%s&position=%s\">",
	       hgTracksName(), words[1], cgiEncode(words[2]));
	printf("Open %s browser</A> at %s.<BR>\n", otherOrg, words[2]);
	}
    bedFree(&bed);
    }
else
    errAbort("query returned no results: \"%s\"", query);
sqlFreeResult(&sr);
hFreeConn(&conn);
printTrackHtml(tdb);
}


void doScaffoldEcores(struct trackDb *tdb, char *item)
/* Creates details page and gets the scaffold co-ordinates for unmapped */
/* genomes for display and to use to create the correct outside link URL */
{
char *dupe, *words[16];
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);
int num;
struct bed *bed = NULL;
char query[512];
struct sqlResult *sr;
char **row;
char *scaffoldName;
int scaffoldStart, scaffoldEnd;
struct dyString *itemUrl = dyStringNew(128), *d;
char *old = "_";
char *new = "";
char *pat = "fold";
int hasBin = 1;
dupe = cloneString(tdb->type);
chopLine(dupe,words);
/* get bed size */
num = 0;
num = atoi(words[1]);

/* get data for this item */
sqlSafef(query, sizeof query, "select * from %s where name = '%s' and chromStart = %d", tdb->table, item, start);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    bed = bedLoadN(row+hasBin, num);

genericHeader(tdb, item);
/* convert chromosome co-ordinates to scaffold position and */
/* make into item for URL */
if (hScaffoldPos(database, bed->chrom, bed->chromStart, bed->chromEnd, &scaffoldName,            &scaffoldStart, &scaffoldEnd) )
   {
    scaffoldStart += 1;
   dyStringPrintf(itemUrl, "%s:%d-%d", scaffoldName, scaffoldStart,                           scaffoldEnd);
   /* remove underscore in scaffold name and change to "scafN" */
   d = dyStringSub(itemUrl->string, old, new);
   itemUrl = dyStringSub(d->string, pat, new);
   printCustomUrl(tdb, itemUrl->string, TRUE);
   }

genericBedClick(conn, tdb, item, start, num);
printTrackHtml(tdb);

dyStringFree(&itemUrl);
freez(&dupe);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

char *stripBDGPSuffix(char *name)
/* cloneString(name), and if it ends in -R[A-Z], strip that off. */
{
char *stripped = cloneString(name);
int len = strlen(stripped);
if (stripped[len-3] == '-' &&
    stripped[len-2] == 'R' &&
    isalpha(stripped[len-1]))
    stripped[len-3] = 0;
return(stripped);
}

static void doGencodeIntron(struct trackDb *tdb, char *item)
/* Intron validation from ENCODE Gencode/Havana gene predictions */
{
struct sqlConnection *conn = hAllocConn(database);
int start = cartInt(cart, "o");
struct gencodeIntron *intron, *intronList = NULL;
char query[256];
int rowOffset = hOffsetPastBin(database, seqName, tdb->table);

genericHeader(tdb, item);
sqlSafef(query, sizeof query,
        "select * from %s where name='%s' and chrom='%s' and chromStart=%d",
                tdb->table, item, seqName, start);
intronList = gencodeIntronLoadByQuery(conn, query, rowOffset);
for (intron = intronList; intron != NULL; intron = intron->next)
    {
    printf("<B>Intron:</B> %s<BR>\n", intron->name);
    printf("<B>Status:</B> %s<BR>\n", intron->status);
    printf("<B>Gene:</B> %s<BR>\n", intron->geneId);
    printf("<B>Transcript:</B> %s<BR>\n", intron->transcript);
    printPos(intron->chrom, intron->chromStart,
            intron->chromEnd, intron->strand, TRUE, intron->name);
    }
hFreeConn(&conn);
printTrackHtml(tdb);
}


static void printESPDetails(char **row, struct trackDb *tdb)
/* Print details from a cell line subtrack table of encodeStanfordPromoters. */
{
struct encodeStanfordPromoters *esp = encodeStanfordPromotersLoad(row);
bedPrintPos((struct bed *)esp, 6, tdb);
printf("<B>Gene model ID:</B> %s<BR>\n", esp->geneModel);
printf("<B>Gene description:</B> %s<BR>\n", esp->description);
printf("<B>Luciferase signal A:</B> %d<BR>\n", esp->lucA);
printf("<B>Renilla signal A:</B> %d<BR>\n", esp->renA);
printf("<B>Luciferase signal B:</B> %d<BR>\n", esp->lucB);
printf("<B>Renilla signal B:</B> %d<BR>\n", esp->renB);
printf("<B>Average Luciferase/Renilla Ratio:</B> %g<BR>\n", esp->avgRatio);
printf("<B>Normalized Luciferase/Renilla Ratio:</B> %g<BR>\n", esp->normRatio);
printf("<B>Normalized and log2 transformed Luciferase/Renilla Ratio:</B> %g<BR>\n",
       esp->normLog2Ratio);
}

static void printESPAverageDetails(char **row, struct trackDb *tdb)
/* Print details from the averaged subtrack table of encodeStanfordPromoters. */
{
struct encodeStanfordPromotersAverage *esp =
    encodeStanfordPromotersAverageLoad(row);
bedPrintPos((struct bed *)esp, 6, tdb);
printf("<B>Gene model ID:</B> %s<BR>\n", esp->geneModel);
printf("<B>Gene description:</B> %s<BR>\n", esp->description);
printf("<B>Normalized and log2 transformed Luciferase/Renilla Ratio:</B> %g<BR>\n",
       esp->normLog2Ratio);
}

void doEncodeStanfordPromoters(struct trackDb *tdb, char *item)
/* Print ENCODE Stanford Promoters data. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row = NULL;
int start = cartInt(cart, "o");
char fullTable[HDB_MAX_TABLE_STRING];
boolean hasBin = FALSE;
char query[1024];

cartWebStart(cart, database, "%s", tdb->longLabel);
genericHeader(tdb, item);
if (!hFindSplitTable(database, seqName, tdb->table, fullTable, sizeof fullTable, &hasBin))
    errAbort("track %s not found", tdb->table);
sqlSafef(query, sizeof(query),
     "select * from %s where chrom = '%s' and chromStart = %d and name = '%s'",
      fullTable, seqName, start, item);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    if (endsWith(tdb->table, "Average"))
	printESPAverageDetails(row+hasBin, tdb);
    else
	printESPDetails(row+hasBin, tdb);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
printTrackHtml(tdb);
}

void doEncodeStanfordRtPcr(struct trackDb *tdb, char *item)
/* Print ENCODE Stanford RTPCR data. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row = NULL;
int start = cartInt(cart, "o");
char fullTable[HDB_MAX_TABLE_STRING];
boolean hasBin = FALSE;
char query[1024];

cartWebStart(cart, database, "%s", tdb->longLabel);
genericHeader(tdb, item);
if (!hFindSplitTable(database, seqName, tdb->table, fullTable, sizeof fullTable, &hasBin))
    errAbort("track %s not found", tdb->table);
sqlSafef(query, sizeof(query),
     "select * from %s where chrom = '%s' and chromStart = %d and name = '%s'",
      fullTable, seqName, start, item);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    struct bed *bed = bedLoadN(row+hasBin, 5);
    bedPrintPos(bed, 5, tdb);
    printf("<B>Primer pair ID:</B> %s<BR>\n", row[hasBin+5]);
    printf("<B>Count:</B> %s<BR>\n", row[hasBin+6]);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
printTrackHtml(tdb);
}

void doEncodeHapMapAlleleFreq(struct trackDb *tdb, char *itemName)
{
char *table = tdb->table;
struct encodeHapMapAlleleFreq alleleFreq;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset = hOffsetPastBin(database, seqName, table);
int start = cartInt(cart, "o");

genericHeader(tdb, itemName);

sqlSafef(query, sizeof(query),
      "select * from %s where chrom = '%s' and "
      "chromStart=%d and name = '%s'", table, seqName, start, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    encodeHapMapAlleleFreqStaticLoad(row+rowOffset, &alleleFreq);
    printf("<B>Variant:</B> %s<BR>\n", alleleFreq.otherAllele);
    printf("<B>Reference:</B> %s<BR>\n", alleleFreq.refAllele);
    bedPrintPos((struct bed *)&alleleFreq, 3, tdb);
    printf("<B>Reference Allele Frequency:</B> %f <BR>\n", alleleFreq.refAlleleFreq);
    printf("<B>Other Allele Frequency:</B> %f <BR>\n", alleleFreq.otherAlleleFreq);
    printf("<B>Center:</B> %s <BR>\n", alleleFreq.center);
    printf("<B>Total count:</B> %d <BR>\n", alleleFreq.totalCount);
    printf("-----------------------------------------------------<BR>\n");
    }
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}


void showHapmapMonomorphic(struct hapmapAllelesSummary *summaryItem)
{
if (summaryItem->totalAlleleCountCEU > 0)
    {
    printf("<TR>");
    printf("<TD>CEU</TD>");
    printf("<TD bgcolor = \"lightgrey\">%d (100%%)</TD>", summaryItem->totalAlleleCountCEU);
    printf("</TR>\n");
    }
else
    printf("<TR><TD>CEU</TD><TD>not available</TD></TR>\n");

if (summaryItem->totalAlleleCountCHB > 0)
    {
    printf("<TR>");
    printf("<TD>CHB</TD>");
    printf("<TD bgcolor = \"lightgrey\">%d (100%%)</TD>", summaryItem->totalAlleleCountCHB);
    printf("</TR>\n");
    }
else
    printf("<TR><TD>CHB</TD><TD>not available</TD></TR>\n");

if (summaryItem->totalAlleleCountJPT > 0)
    {
    printf("<TR>");
    printf("<TD>JPT</TD>");
    printf("<TD bgcolor = \"lightgrey\">%d (100%%)</TD>", summaryItem->totalAlleleCountJPT);
    printf("</TR>\n");
    }
else
    printf("<TR><TD>JPT</TD><TD>not available</TD></TR>\n");

if (summaryItem->totalAlleleCountYRI > 0)
    {
    printf("<TR>");
    printf("<TD>YRI</TD>");
    printf("<TD bgcolor = \"lightgrey\">%d (100%%)</TD>", summaryItem->totalAlleleCountYRI);
    printf("</TR>\n");
    }
else
    printf("<TR><TD>YRI</TD><TD>not available</TD></TR>\n");
}

void showOneHapmapRow(char *pop, char *allele1, char *allele2, char *majorAllele,
                      int majorCount, int totalCount)
{
int count1 = 0;
int count2 = 0;
float freq1 = 0.0;
float freq2 = 0.0;

if (majorCount == 0)
    {
    printf("<TR><TD>%s</TD><TD align=center>-</TD><TD align=center>-</TD></TR>\n", pop);
    return;
    }

if (sameString(allele1, majorAllele))
    {
    count1 = majorCount;
    count2 = totalCount - majorCount;
    }
else
    {
    count2 = majorCount;
    count1 = totalCount - majorCount;
    }

freq1 = 100.0 * count1 / totalCount;
freq2 = 100.0 * count2 / totalCount;

printf("<TR>");
printf("<TD>%s</TD>", pop);
if (count1 > count2)
    {
    printf("<TD bgcolor = \"lightgrey\" align=right>%d (%3.2f%%)</TD>", count1, freq1);
    printf("<TD align=right>%d (%3.2f%%)</TD>", count2, freq2);
    }
else if (count1 < count2)
    {
    printf("<TD align=right>%d (%3.2f%%)</TD>", count1, freq1);
    printf("<TD bgcolor = \"lightgrey\" align=right>%d (%3.2f%%)</TD>", count2, freq2);
    }
else
    {
    printf("<TD align=right>%d (%3.2f%%)</TD>", count1, freq1);
    printf("<TD align=right>%d (%3.2f%%)</TD>", count2, freq2);
    }
printf("</TR>\n");

}

void showHapmapAverageRow(char *label, float freq1)
{
float freq2 = 1.0 - freq1;
printf("<TR><TD>%s</TD>", label);
if (freq1 > 0.5)
    {
    printf("<TD bgcolor = \"lightgrey\" align=right>(%3.2f%%)</TD>", freq1*100);
    printf("<TD align=right>(%3.2f%%)</TD>", freq2*100);
    }
else if (freq1 < 0.5)
    {
    printf("<TD align=right>(%3.2f%%)</TD>", freq1*100);
    printf("<TD bgcolor = \"lightgrey\" align=right>(%3.2f%%)</TD>", freq2*100);
    }
else
    {
    printf("<TD align=right>(%3.2f%%)</TD>", freq1*100);
    printf("<TD align=right>(%3.2f%%)</TD>", freq2*100);
    }
printf("</TR>\n");
}

void doHapmapSnpsSummaryTable(struct sqlConnection *conn, struct trackDb *tdb, char *itemName,
			      boolean showOrtho)
/* Use the hapmapAllelesSummary table (caller checks for existence) to display allele
 * frequencies for the 4 HapMap Phase II populations. */
{
char *table = tdb->table;
struct hapmapAllelesSummary *summaryItem;
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset = hOffsetPastBin(database, seqName, table);
int start = cartInt(cart, "o");
float het = 0.0;

sqlSafef(query, sizeof(query), "select * from hapmapAllelesSummary where chrom = '%s' and "
      "chromStart=%d and name = '%s'", seqName, start, itemName);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
summaryItem = hapmapAllelesSummaryLoad(row+rowOffset);

printf("<BR><B>Allele frequencies in each population (major allele highlighted):</B><BR>\n");
printf("<TABLE BORDER=1>\n");
if (differentString(summaryItem->allele2, "none"))
    {
    printf("<TR><TH>Population</TH> <TH>%s</TH> <TH>%s</TH></TR>\n", summaryItem->allele1, summaryItem->allele2);
    showOneHapmapRow("CEU", summaryItem->allele1, summaryItem->allele2, summaryItem->majorAlleleCEU,
                            summaryItem->majorAlleleCountCEU, summaryItem->totalAlleleCountCEU);
    showOneHapmapRow("CHB", summaryItem->allele1, summaryItem->allele2, summaryItem->majorAlleleCHB,
                            summaryItem->majorAlleleCountCHB, summaryItem->totalAlleleCountCHB);
    showOneHapmapRow("JPT", summaryItem->allele1, summaryItem->allele2, summaryItem->majorAlleleJPT,
                            summaryItem->majorAlleleCountJPT, summaryItem->totalAlleleCountJPT);
    showOneHapmapRow("YRI", summaryItem->allele1, summaryItem->allele2, summaryItem->majorAlleleYRI,
                            summaryItem->majorAlleleCountYRI, summaryItem->totalAlleleCountYRI);
    }
else
    {
    printf("<TR><TH>Population</TH> <TH>%s</TH></TR>\n", summaryItem->allele1);
    showHapmapMonomorphic(summaryItem);
    }
printf("</TABLE>\n");

het = summaryItem->score / 10.0;
printf("<BR><B>Expected Heterozygosity (from total allele frequencies):</B> %3.2f%%<BR>\n", het);


if (showOrtho && (differentString(summaryItem->chimpAllele, "none") ||
		  differentString(summaryItem->macaqueAllele, "none")))
    {
    printf("<BR><B>Orthologous alleles:</B><BR>\n");
    printf("<TABLE BORDER=1>\n");
    printf("<TR><TH>Species</TH> <TH>Allele</TH> <TH>Quality Score</TH></TR>\n");
    if (differentString(summaryItem->chimpAllele, "none"))
        {
        printf("<TR>");
        printf("<TD>Chimp</TD>");
        printf("<TD>%s</TD>", summaryItem->chimpAllele);
        printf("<TD>%d</TD>", summaryItem->chimpAlleleQuality);
        printf("</TR>");
	}
    if (differentString(summaryItem->macaqueAllele, "none"))
        {
        printf("<TR>");
        printf("<TD>Macaque</TD>");
        printf("<TD>%s</TD>", summaryItem->macaqueAllele);
        printf("<TD>%d</TD>", summaryItem->macaqueAlleleQuality);
        printf("</TR>");
	}
    printf("</TABLE>\n");
    }

sqlFreeResult(&sr);
}

void doHapmapSnpsAllPops(struct sqlConnection *conn, struct trackDb *tdb, char *itemName,
			 boolean showOrtho)
/* Show item's SNP allele frequencies for each of the 11 HapMap Phase III
 * populations, as well as chimp and macaque if showOrtho. */
{
int i;
printf("<BR><B>Allele frequencies in each population (major allele highlighted):</B><BR>\n");
printf("<TABLE BORDER=1>\n");
// Do a first pass to gather up alleles and counts:
char *majorAlleles[HAP_PHASEIII_POPCOUNT];
int majorCounts[HAP_PHASEIII_POPCOUNT], haploCounts[HAP_PHASEIII_POPCOUNT];
int totalA1Count = 0, totalA2Count = 0, totalHaploCount = 0;
float sumHet = 0.0;
int sumA1A1 = 0, sumA1A2 = 0, sumA2A2 = 0;
int popCount = 0;
char *allele1 = NULL, *allele2 = NULL;
for (i=0;  i < HAP_PHASEIII_POPCOUNT;  i++)
    {
    char *popCode = hapmapPhaseIIIPops[i];
    struct hapmapSnps *item = NULL;
    char table[HDB_MAX_TABLE_STRING];
    safef(table, sizeof(table), "hapmapSnps%s", popCode);
    if (sqlTableExists(conn, table))
	{
	char query[512];
	sqlSafef(query, sizeof(query), "select * from %s where name = '%s' and chrom = '%s'",
	      table, itemName, seqName);
	struct sqlResult *sr = sqlGetResult(conn, query);
	char **row;
	if ((row = sqlNextRow(sr)) != NULL)
	    {
	    int rowOffset = hOffsetPastBin(database, seqName, table);
	    item = hapmapSnpsLoad(row+rowOffset);
	    }
	sqlFreeResult(&sr);
	}
    majorAlleles[i] = "";
    majorCounts[i] = 0;
    haploCounts[i] = 0;
    if (item != NULL)
	{
	majorAlleles[i] = item->allele1;
        majorCounts[i] = 2*item->homoCount1 + item->heteroCount;
	if (item->homoCount1 < item->homoCount2)
	    {
	    majorAlleles[i] = item->allele2;
	    majorCounts[i] = 2*item->homoCount2 + item->heteroCount;
	    }
	haploCounts[i] = 2*(item->homoCount1 + item->homoCount2 + item->heteroCount);
	if (allele1 == NULL)
	    {
	    allele1 = item->allele1;
	    allele2 = item->allele2;
	    }
	else if (!sameString(allele1, item->allele1) ||
		 (isNotEmpty(allele2) && isNotEmpty(item->allele2) &&
		  !sameString(allele2, item->allele2)))
	    warn("Allele order in hapmapSnps%s (%s/%s) is different from earlier table(s) (%s/%s)",
		 popCode, item->allele1, item->allele2, allele1, allele2);
	totalA1Count += 2*item->homoCount1 + item->heteroCount;
	totalA2Count += 2*item->homoCount2 + item->heteroCount;
	totalHaploCount += haploCounts[i];
	sumHet += ((float)item->heteroCount /
		   (item->homoCount1 + item->homoCount2 + item->heteroCount));
	sumA1A1 += item->homoCount1;
	sumA1A2 += item->heteroCount;
	sumA2A2 += item->homoCount2;
	popCount++;
	}
    }
printf("<TR><TH>Population</TH> <TH>%s</TH> <TH>%s</TH></TR>\n", allele1, allele2);
for (i=0;  i < HAP_PHASEIII_POPCOUNT;  i++)
    showOneHapmapRow(hapmapPhaseIIIPops[i], allele1, allele2, majorAlleles[i],
		     majorCounts[i], haploCounts[i]);
showHapmapAverageRow("Average", (float)totalA1Count / totalHaploCount);
printf("</TABLE>\n");

printf("<BR><B>Average of populations' observed heterozygosities:</B> %3.2f%%<BR>\n",
       (100.0 * sumHet/popCount));

if (showOrtho)
    {
    boolean showedHeader = FALSE;
    int i;
    for (i = 0;  hapmapOrthoSpecies[i] != NULL; i++)
	{
	char table[HDB_MAX_TABLE_STRING];
	safef(table, sizeof(table), "hapmapAlleles%s", hapmapOrthoSpecies[i]);
	if (sqlTableExists(conn, table))
	    {
	    if (!showedHeader)
		{
		printf("<BR><B>Orthologous alleles from reference genome assemblies:</B><BR>\n");
		printf("<TABLE BORDER=1>\n");
		printf("<TR><TH>Species</TH> <TH>Allele</TH> <TH>Quality Score</TH></TR>\n");
		showedHeader = TRUE;
		}
	    char query[512];
	    sqlSafef(query, sizeof(query),
		  "select orthoAllele, score, strand from %s where name = '%s' and chrom = '%s'",
		  table, itemName, seqName);
	    struct sqlResult *sr = sqlGetResult(conn, query);
	    char **row;
	    if ((row = sqlNextRow(sr)) != NULL)
		{
		char *allele = row[0];
		char *qual = row[1];
		char *strand = row[2];
		if (sameString("-", strand))
		    reverseComplement(allele, strlen(allele));
		printf("<TR><TD>%s</TD><TD>%s</TD><TD>%s</TD></TR>",
		       hapmapOrthoSpecies[i], allele, qual);
		}
	    else
		printf("<TR><TD>%s</TD><TD>N/A</TD><TD>N/A</TD></TR>", hapmapOrthoSpecies[i]);
	    sqlFreeResult(&sr);
	    }
	}
    if (showedHeader)
	printf("</TABLE>\n");
    }
}

void doHapmapSnpsSummary(struct sqlConnection *conn, struct trackDb *tdb, char *itemName,
			 boolean showOrtho)
/* Display per-population allele frequencies. */
{
boolean isPhaseIII = sameString(trackDbSettingOrDefault(tdb, "hapmapPhase", "II"), "III");
if (!isPhaseIII && sqlTableExists(conn, "hapmapAllelesSummary"))
    doHapmapSnpsSummaryTable(conn, tdb, itemName, showOrtho);
else
    doHapmapSnpsAllPops(conn, tdb, itemName, showOrtho);
}

void doHapmapSnps(struct trackDb *tdb, char *itemName)
/* assume just one hapmap snp at a given location */
{
char *table = tdb->table;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset = hOffsetPastBin(database, seqName, table);
int start = cartInt(cart, "o");
int majorCount = 0;
int minorCount = 0;
char *majorAllele = NULL;
char *minorAllele = NULL;
char popCode[4];
safencpy(popCode, sizeof(popCode), table + strlen("hapmapSnps"), 3);
popCode[3] = '\0';

genericHeader(tdb, itemName);

sqlSafef(query, sizeof(query),
      "select * from %s where chrom = '%s' and "
      "chromStart=%d and name = '%s'", table, seqName, start, itemName);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
struct hapmapSnps *item = hapmapSnpsLoad(row+rowOffset);
printf("<B>SNP rsId:</B> ");
printDbSnpRsUrl(itemName, "%s", itemName);
puts("<BR>");
printf("<B>Position:</B> <A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">%s:%d-%d</A><BR>\n",
       hgTracksPathAndSettings(), database, item->chrom, item->chromStart+1, item->chromEnd,
       item->chrom, item->chromStart+1, item->chromEnd);
printf("<B>Strand:</B> %s<BR>\n", item->strand);
printf("<B>Polymorphism type:</B> %s<BR>\n", item->observed);
if (item->homoCount1 >= item->homoCount2)
    {
    majorAllele = cloneString(item->allele1);
    majorCount = item->homoCount1;
    minorCount = item->homoCount2;
    minorAllele = cloneString(item->allele2);
    }
else
    {
    majorAllele = cloneString(item->allele2);
    majorCount = item->homoCount2;
    minorCount = item->homoCount1;
    minorAllele = cloneString(item->allele1);
    }

printf("<BR><B>Genotype counts for %s:</B><BR>\n", popCode);
printf("<TABLE BORDER=1>\n");
printf("<TR><TD>Major allele (%s) homozygotes</TD><TD align=right>%d individuals</TD></TR>\n",
       majorAllele, majorCount);
if (minorCount > 0)
    printf("<TR><TD>Minor allele (%s) homozygotes</TD><TD align=right>%d individuals</TD></TR>\n",
	   minorAllele, minorCount);
if (item->heteroCount > 0)
    printf("<TR><TD>Heterozygotes (%s)</TD><TD align=right>%d individuals</TD></TR>\n",
	   item->observed, item->heteroCount);
printf("<TR><TD>Total</TD><TD align=right>%d individuals</TD></TR>\n",
       majorCount + minorCount + item->heteroCount);
printf("</TABLE>\n");

sqlFreeResult(&sr);

/* Show allele frequencies for all populations */
doHapmapSnpsSummary(conn, tdb, itemName, TRUE);
printTrackHtml(tdb);
hFreeConn(&conn);
}

void doHapmapOrthos(struct trackDb *tdb, char *itemName)
/* could assume just one match */
{
char *table = tdb->table;
struct hapmapAllelesOrtho *ortho;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset = hOffsetPastBin(database, seqName, table);
int start = cartInt(cart, "o");
char *otherDb = NULL;
char *otherDbName = NULL;

genericHeader(tdb, itemName);

sqlSafef(query, sizeof(query),
      "select * from %s where chrom = '%s' and "
      "chromStart=%d and name = '%s'", table, seqName, start, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ortho = hapmapAllelesOrthoLoad(row+rowOffset);
    printf("<B>Human Position:</B> "
           "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
                  hgTracksPathAndSettings(), database, ortho->chrom, ortho->chromStart+1, ortho->chromEnd);
    printf("%s:%d-%d</A><BR>\n", ortho->chrom, ortho->chromStart+1, ortho->chromEnd);
    printf("<B>Human Strand: </B> %s\n", ortho->strand);
    printf("<BR>");
    printf("<B>Polymorphism type:</B> %s<BR>\n", ortho->observed);

    if (startsWith("hapmapAllelesChimp", table))
        {
        otherDb = "panTro2";
	otherDbName = "Chimp";
	}
    if (startsWith("hapmapAllelesMacaque", table))
        {
        otherDb = "rheMac2";
	otherDbName = "Macaque";
	}

    printf("<B>%s </B>", otherDbName);
    printf("<B>Position:</B> "
           "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
                  hgTracksPathAndSettings(), otherDb, ortho->orthoChrom, ortho->orthoStart, ortho->orthoEnd);
    linkToOtherBrowser(otherDb, ortho->orthoChrom, ortho->orthoStart, ortho->orthoEnd);
    printf("%s:%d-%d</A><BR>\n", ortho->orthoChrom, ortho->orthoStart+1, ortho->orthoEnd);

    printf("<B>%s </B>", otherDbName);
    printf("<B>Strand:</B> %s\n", ortho->orthoStrand);
    printf("<BR>");

    printf("<B>%s </B>", otherDbName);
    printf("<B>Allele:</B> %s\n", ortho->orthoAllele);
    printf("<BR>");
    printf("<B>%s </B>", otherDbName);
    printf("<B>Allele Quality (0-100):</B> %d\n", ortho->score);

    }

printf("<BR>\n");
/* get summary data (allele frequencies) here */
/* don't repeat ortho display */
doHapmapSnpsSummary(conn, tdb, itemName, FALSE);
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}


void printSnpAllele(char *orthoDb, int snpVersion, char *rsId)
/* check whether snpAlleles exists for a database */
/* if found, print value */
{
char tableName[512];
struct sqlConnection *conn = sqlConnect(orthoDb);
char query[256];
struct sqlResult *sr;
char **row = NULL;

safef(tableName, sizeof(tableName), "snp%d%sorthoAllele", snpVersion, database);
if (!hTableExists(orthoDb, tableName))
    {
    sqlDisconnect(&conn);
    return;
    }

sqlSafef(query, sizeof(query), "select allele from %s where name = '%s'", tableName, rsId);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (!row)
    {
    sqlDisconnect(&conn);
    return;
    }
printf("<B>%s Allele:</B> %s<BR>\n", orthoDb, row[0]);
sqlFreeResult(&sr);
sqlDisconnect(&conn);
}


static char *fbgnFromCg(char *cgId)
/* Given a BDGP ID, looks up its FBgn ID because FlyBase query no longer
 * supports BDGP IDs.  Returns NULL if not found.
 * Do not free the statically allocated result. */
{
static char result[32];  /* Ample -- FBgn ID's are 11 chars long. */
char query[512];
if (hTableExists(database, "flyBase2004Xref"))
    sqlSafef(query, sizeof(query),
	  "select fbgn from flyBase2004Xref where name = '%s';", cgId);
else if (hTableExists(database, "bdgpGeneInfo"))
    sqlSafef(query, sizeof(query),
	  "select flyBaseId from bdgpGeneInfo where bdgpName = '%s';", cgId);
else
    return NULL;
struct sqlConnection *conn = hAllocConn(database);
char *resultOrNULL =  sqlQuickQuery(conn, query, result, sizeof(result));
hFreeConn(&conn);
return resultOrNULL;
}

static void doPscreen(struct trackDb *tdb, char *item)
/* P-Screen (BDGP Gene Disruption Project) P el. insertion locations/genes. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
int start = cartInt(cart, "o");
char fullTable[HDB_MAX_TABLE_STRING];
boolean hasBin = FALSE;
char query[512];

genericHeader(tdb, item);
if (!hFindSplitTable(database, seqName, tdb->table, fullTable, sizeof fullTable, &hasBin))
    errAbort("track %s not found", tdb->table);
sqlSafef(query, sizeof(query),
     "select * from %s where chrom = '%s' and chromStart = %d and name = '%s'",
      fullTable, seqName, start, item);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    struct pscreen *psc = pscreenLoad(row+hasBin);
    int i;
    printCustomUrl(tdb, psc->name, FALSE);
    printPosOnChrom(psc->chrom, psc->chromStart, psc->chromEnd, psc->strand,
		    FALSE, psc->name);
    if (psc->stockNumber != 0)
	printf("<B>Stock number:</B> "
	       "<A HREF=\"http://flystocks.bio.indiana.edu/Reports/%d.html\" "
	       "TARGET=_BLANK>%d</A><BR>\n", psc->stockNumber,
	       psc->stockNumber);
    for (i=0;  i < psc->geneCount;  i++)
	{
	char gNum[4];
	if (psc->geneCount > 1)
	    safef(gNum, sizeof(gNum), " %d", i+1);
	else
	    gNum[0] = 0;
	if (isNotEmpty(psc->geneIds[i]))
	    {
	    char *idType = "FlyBase";
	    char *fbgnId = psc->geneIds[i];
	    if (isBDGPName(fbgnId))
		{
		char *stripped = stripBDGPSuffix(psc->geneIds[i]);
		idType = "BDGP";
		fbgnId = fbgnFromCg(stripped);
		}
	    if (fbgnId == NULL)
		printf("<B>Gene%s %s ID:</B> %s<BR>\n",
		       gNum, idType, psc->geneIds[i]);
	    else
		printf("<B>Gene%s %s ID:</B> "
		       "<A HREF=\"http://flybase.net/reports/%s.html\" "
		       "TARGET=_BLANK>%s</A><BR>\n",
		       gNum, idType, fbgnId, psc->geneIds[i]);
	    }
	}
    pscreenFree(&psc);
    }
else
    errAbort("query returned no results: \"%s\"", query);
sqlFreeResult(&sr);
hFreeConn(&conn);
printTrackHtml(tdb);
}

static void doOligoMatch(char *item)
/* Print info about oligo match. */
{
char *oligo = cartUsualString(cart,
	oligoMatchVar, cloneString(oligoMatchDefault));
touppers(oligo);
cartWebStart(cart, database, "Perfect Matches to Short Sequence");
printf("<B>Sequence:</B> %s<BR>\n", oligo);
printf("<B>Chromosome:</B> %s<BR>\n", seqName);
printf("<B>Start:</B> %s<BR>\n", item+1);
printf("<B>Strand:</B> %c<BR>\n", item[0]);
webIncludeHelpFile(OLIGO_MATCH_TRACK_NAME, TRUE);
}

struct slName *cutterIsoligamers(struct cutter *myEnzyme)
/* Find enzymes with same cut site. */
{
struct sqlConnection *conn;
struct cutter *cutters = NULL;
struct slName *ret = NULL;

conn = hAllocConn("hgFixed");
char query[1024];
sqlSafef(query, sizeof query, "select * from cutters");
cutters = cutterLoadByQuery(conn, query);
ret = findIsoligamers(myEnzyme, cutters);
hFreeConn(&conn);
cutterFreeList(&cutters);
return ret;
}

void cutterPrintSite(struct cutter *enz)
/* Print out the enzyme REBASE style. */
{
int i;
for (i = 0; i < enz->size+1; i++)
    {
    if (i == enz->cut)
	printf("^");
    else if (i == enz->cut + enz->overhang)
	printf("v");
    if (i < enz->size)
	printf("%c", enz->seq[i]);
    }
}

static void doCuttersEnzymeList(struct sqlConnection *conn, char *getBed, char *c, char *l, char *r)
/* Print out list of enzymes (BED). This function will exit the program. */
{
struct cutter *cut = NULL;
char query[100];
struct dnaSeq *winDna;
struct bed *bedList = NULL, *oneBed;
int s, e;
if (!c || !l || !r)
    errAbort("Bad Range");
s = atoi(l);
e = atoi(r);
winDna = hDnaFromSeq(database, c, s, e, dnaUpper);
if (sameString(getBed, "all"))
    sqlSafef(query, sizeof(query), "select * from cutters");
else
    sqlSafef(query, sizeof(query), "select * from cutters where name=\'%s\'", getBed);
cut = cutterLoadByQuery(conn, query);
bedList = matchEnzymes(cut, winDna, s);
printf("<HTML>\n<HEAD>\n%s<TITLE>Enzyme Output</TITLE></HEAD>\n<BODY><PRE><TT>", getCspMetaHeader());
for (oneBed = bedList; oneBed != NULL; oneBed = oneBed->next)
    {
    freeMem(oneBed->chrom);
    oneBed->chrom = cloneString(c);
    bedTabOutN(oneBed, 6, stdout);
    }
puts("</TT></PRE>\n");
cartFooter();
bedFreeList(&bedList);
cutterFreeList(&cut);
hFreeConn(&conn);
exit(0);
}

static void doCutters(char *item)
/* Print info about a restriction enzyme. */
{
struct sqlConnection *conn;
struct cutter *cut = NULL;
char query[100];
char *doGetBed = cgiOptionalString("doGetBed");
char *c = cgiOptionalString("c");
char *l = cgiOptionalString("l");
char *r = cgiOptionalString("r");
conn = hAllocConn("hgFixed");
if (doGetBed)
    doCuttersEnzymeList(conn, doGetBed, c, l, r);
sqlSafef(query, sizeof(query), "select * from cutters where name=\'%s\'", item);
cut = cutterLoadByQuery(conn, query);
cartWebStart(cart, database, "Restriction Enzymes from REBASE");
if (cut)
    {
    char *o = cgiOptionalString("o");
    char *t = cgiOptionalString("t");
    struct slName *isoligs = cutterIsoligamers(cut);
    printf("<B>Enzyme Name:</B> %s<BR>\n", cut->name);
    /* Display position only if click came from hgTracks. */
    if (c && o && t)
        {
	int left = atoi(o);
	int right = atoi(t);
	printPosOnChrom(c, left, right, NULL, FALSE, cut->name);
        }
    puts("<B>Recognition Sequence: </B>");
    cutterPrintSite(cut);
    puts("<BR>\n");
    printf("<B>Palindromic: </B>%s<BR>\n", (cut->palindromic) ? "YES" : "NO");
    if (cut->numSciz > 0)
        {
	int i;
	puts("<B>Isoschizomers: </B>");
	for (i = 0; i < cut->numSciz-1; i++)
	    printf("<A HREF=\"%s&g=%s&i=%s\">%s</A>, ", hgcPathAndSettings(), CUTTERS_TRACK_NAME, cut->scizs[i], cut->scizs[i]);
	printf("<A HREF=\"%s&g=%s&i=%s\">%s</A><BR>\n", hgcPathAndSettings(), CUTTERS_TRACK_NAME, cut->scizs[cut->numSciz-1], cut->scizs[cut->numSciz-1]);
	}
    if (isoligs)
	{
	struct slName *cur;
	puts("<B>Isoligamers: </B>");
	for (cur = isoligs; cur->next != NULL; cur = cur->next)
	    printf("<A HREF=\"%s&g=%s&i=%s\">%s</A>, ", hgcPathAndSettings(), CUTTERS_TRACK_NAME, cur->name, cur->name);
	printf("<A HREF=\"%s&g=%s&i=%s\">%s</A><BR>\n", hgcPathAndSettings(), CUTTERS_TRACK_NAME, cur->name, cur->name);
	slFreeList(&isoligs);
	}
    if (cut->numRefs > 0)
	{
	int i, count = 1;
	char **row;
	struct sqlResult *sr;
	puts("<B>References:</B><BR>\n");
	sqlSafef(query, sizeof(query), "select * from rebaseRefs");
	sr = sqlGetResult(conn, query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    int refNum = atoi(row[0]);
            for (i = 0; i < cut->numRefs; i++)
		{
		if (refNum == cut->refs[i])
		    printf("%d. %s<BR>\n", count++, row[1]);
		}
	    }
	sqlFreeResult(&sr);
        }
    if (c && o && t)
        {
	puts("<BR><B>Download BED of enzymes in this browser range:</B>&nbsp");
	printf("<A HREF=\"%s&g=%s&l=%s&r=%s&c=%s&doGetBed=all\">all enzymes</A>, ", hgcPathAndSettings(), CUTTERS_TRACK_NAME, l, r, c);
	printf("<A HREF=\"%s&g=%s&l=%s&r=%s&c=%s&doGetBed=%s\">just %s</A><BR>\n", hgcPathAndSettings(), CUTTERS_TRACK_NAME, l, r, c, cut->name, cut->name);
	}
    }
webIncludeHelpFile(CUTTERS_TRACK_NAME, TRUE);
cutterFree(&cut);
hFreeConn(&conn);
}

static void doAnoEstTcl(struct trackDb *tdb, char *item)
/* Print info about AnoEst uniquely-clustered item. */
{
struct sqlConnection *conn = hAllocConn(database);
int start = cartInt(cart, "o");
genericHeader(tdb, item);
printCustomUrl(tdb, item, TRUE);
genericBedClick(conn, tdb, item, start, 12);
if (hTableExists(database, "anoEstExpressed"))
    {
    char query[512];

    sqlSafef(query, sizeof(query),
	  "select 1 from anoEstExpressed where name = '%s'", item);
    if (sqlQuickNum(conn, query))
	puts("<B>Expressed:</B> yes<BR>");
    else
	puts("<B>Expressed:</B> no<BR>");
    }
hFreeConn(&conn);
printTrackHtml(tdb);
}


void mammalPsgTableRow(char *test, char *description, float pVal, unsigned isFdrSignificant)
/* print single row of the overview table for mammal PSG track */
{
char *start = "";
char *end = "";

if (isFdrSignificant)
    {
    start = "<b>";
    end = "</b>";
    }

if (pVal<=1)
    {
    printf("<tr><td>%s%s%s</td><td>%s%s%s</td><td>%s%.02g%s</tr>\n",
	   start,test,end,
	   start,description,end,
           start,pVal,end);
    }
}

void doMammalPsg(struct trackDb *tdb, char *itemName)
/* create details page for mammalPsg track */
{
struct mammalPsg *mammalPsg = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char *bayesianFiguresUrl = "../images/mammalPsg";

genericHeader(tdb, itemName);
sqlSafef(query, sizeof query, "select * from %s where name = '%s'", tdb->table, itemName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    mammalPsg = mammalPsgLoad(row);
else
    errAbort("Can't find item '%s'", itemName);

sqlFreeResult(&sr);

/* first print the same thing that you would print for ordinary bed track */
bedPrintPos((struct bed *) mammalPsg,12,tdb);

/* rows showing the results of individual likelihood ratio tests */
printf("<p><b>Likelihood ratio tests for positive selection:</b></p>\n");
printf("<p><table border=1>\n");
printf("<tr><th>Test</th><th>Description</th><th>P-value</th>");
mammalPsgTableRow("A","all branches",mammalPsg->lrtAllPValue,mammalPsg->lrtAllIsFdr);
mammalPsgTableRow("B","branch leading to primates",mammalPsg->lrtPrimateBrPValue,mammalPsg->lrtPrimateBrIsFdr);
mammalPsgTableRow("C","primate clade",mammalPsg->lrtPrimateClPValue,mammalPsg->lrtPrimateClIsFdr);
mammalPsgTableRow("D","branch leading to rodents",mammalPsg->lrtRodentBrPValue,mammalPsg->lrtRodentBrIsFdr);
mammalPsgTableRow("E","rodent clade",mammalPsg->lrtRodentClPValue,mammalPsg->lrtRodentClIsFdr);
mammalPsgTableRow("F","human branch",mammalPsg->lrtHumanPValue,mammalPsg->lrtHumanIsFdr);
mammalPsgTableRow("G","chimp branch",mammalPsg->lrtChimpPValue,mammalPsg->lrtChimpIsFdr);
mammalPsgTableRow("H","branch leading to hominids",mammalPsg->lrtHominidPValue,mammalPsg->lrtHominidIsFdr);
mammalPsgTableRow("I","macaque branch",mammalPsg->lrtMacaquePValue,mammalPsg->lrtMacaqueIsFdr);
printf("</table></p>\n");
printf("<p>(FDR significant P-value shown in boldface)</p>\n");

/* pictures showing the Bayesian analysis */

if (mammalPsg->bestHist > 0)
    {
    printf("<p><b>Results of Bayesian analysis:</b><br>\n");
    printf("<table border=0 cellpadding=20><tr>\n");
    printf("<td align=center><b>Best model - posterior prob. %.2f</b><br>&nbsp;<br><img src=\"%s/model-%d-1.jpg\"></td>\n",
	   mammalPsg->bestHistPP,bayesianFiguresUrl,mammalPsg->bestHist);
    printf("<td align=center><b>2nd best - posterior prob. %.2f</b><br>&nbsp;<br><img src=\"%s/model-%d-1.jpg\"></td>\n",
	   mammalPsg->nextBestHistPP,bayesianFiguresUrl,mammalPsg->nextBestHist);
    printf("</tr></table></p>\n");
    }

printTrackHtml(tdb);
hFreeConn(&conn);
}

void doDless(struct trackDb *tdb, char *itemName)
/* create details page for DLESS */
{
struct dless *dless = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
boolean approx;
enum {CONS, GAIN, LOSS} elementType;

genericHeader(tdb, itemName);
sqlSafef(query, sizeof query, "select * from %s where name = '%s'", tdb->table, itemName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    dless = dlessLoad(row);
else
    errAbort("Can't find item '%s'", itemName);

sqlFreeResult(&sr);

approx = sameString(dless->condApprox, "approx");
if (sameString(dless->type, "conserved"))
    elementType = CONS;
else if (sameString(dless->type, "gain"))
    elementType = GAIN;
else
    elementType = LOSS;

if (elementType == CONS)
    printf("<B>Prediction:</B> conserved in all species<BR>\n");
else
    printf("<B>Prediction:</B> %s of element on branch above node labeled \"%s\"<BR>\n",
           elementType == GAIN ? "gain" : "loss", dless->branch);
printPos(dless->chrom, dless->chromStart, dless->chromEnd, NULL,
         FALSE, dless->name);
printf("<B>Log-odds score:</B> %.1f bits<BR>\n", dless->score);

if (elementType == CONS)
    {
    printf("<B>P-value of conservation:</B> %.2e<BR><BR>\n", dless->pConsSub);
    printf("<B>Numbers of substitutions:</B>\n<UL>\n");
    printf("<LI>Null distribution: mean = %.2f, var = %.2f, 95%% c.i. = [%d, %d]\n",
           dless->priorMeanSub, dless->priorVarSub, dless->priorMinSub,
           dless->priorMaxSub);
    printf("<LI>Posterior distribution: mean = %.2f, var = %.2f\n</UL>\n",
           dless->postMeanSub, dless->postVarSub);
    }
else
    {
    printf("<B>P-value of conservation in subtree:</B> %.2e<BR>\n",
           dless->pConsSub);
    printf("<B>P-value of conservation in rest of tree:</B> %.2e<BR>\n",
           dless->pConsSup);
    printf("<B>P-value of conservation in subtree given total:</B> %.2e%s<BR>\n",
           dless->pConsSubCond, approx ? "*" : "");
    printf("<B>P-value of conservation in rest of tree given total:</B> %.2e%s<BR><BR>\n",
           dless->pConsSupCond, approx ? "*" : "");
    printf("<B>Numbers of substitutions in subtree beneath event</B>:\n<UL>\n");
    printf("<LI>Null distribution: mean = %.2f, var = %.2f, 95%% c.i. = [%d, %d]\n",
           dless->priorMeanSub, dless->priorVarSub, dless->priorMinSub,
           dless->priorMaxSub);
    printf("<LI>Posterior distribution: mean = %.2f, var = %.2f\n",
           dless->postMeanSub, dless->postVarSub);
    printf("</UL><B>Numbers of substitutions in rest of tree:</B>\n<UL>\n");
    printf("<LI>Null distribution: mean = %.2f, var = %.2f, 95%% c.i. = [%d, %d]\n",
           dless->priorMeanSup, dless->priorVarSup, dless->priorMinSup,
           dless->priorMaxSup);
    printf("<LI>Posterior distribution: mean = %.2f, var = %.2f\n</UL>\n",
           dless->postMeanSup, dless->postVarSup);
    if (approx)
        printf("* = Approximate p-value (usually conservative)<BR>\n");
    }

printTrackHtml(tdb);
hFreeConn(&conn);
}

void showSomeAlignment2(struct psl *psl, bioSeq *qSeq, enum gfType qType, int qStart,
                        int qEnd, char *entryName, char *geneName, char *geneTable, int cdsS,
                        int cdsE)
/* Display protein or DNA alignment in a frame. */
{
int blockCount = 0, i = 0, j= 0, *exnStarts = NULL, *exnEnds = NULL;
struct tempName indexTn, bodyTn;
FILE *index, *body;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
struct genePred *gene = NULL;
char **row, query[256];
int tStart = psl->tStart;
int tEnd = psl->tEnd;
char tName[256];
struct dnaSeq *tSeq;
char *tables[4] = {"luGene", "refGene", "mgcGenes", "luGene2"};

/* open file to write to */
trashDirFile(&indexTn, "index", "index", ".html");
trashDirFile(&bodyTn, "body", "body", ".html");
body = mustOpen(bodyTn.forCgi, "w");

/* get query genes struct info*/
for(i = 0; i < 4; i++)
    {
    if(sqlTableExists(conn, tables[i]))
	{
	sqlSafef(query, sizeof query, "SELECT * FROM %s WHERE name = '%s'"
		"AND chrom = '%s' AND txStart <= %d "
		"AND txEnd >= %d",
		tables[i], geneName, psl->qName, qStart, qEnd);
	sr = sqlMustGetResult(conn, query);
	if((row = sqlNextRow(sr)) != NULL)
	    {
	    int hasBin = 0;
	    if(hOffsetPastBin(database, psl->qName, tables[i]))
		hasBin=1;
	    gene = genePredLoad(row+hasBin);
	    break;
	    }
	else
	    sqlFreeResult(&sr);
	}
    }
if(i == 4)
    errAbort("Can't find query for %s in %s. This entry may no longer exist\n", geneName, geneTable);


AllocArray(exnStarts, gene->exonCount);
AllocArray(exnEnds, gene->exonCount);
for(i = 0; i < gene->exonCount; i++)
    {
    if(gene->exonStarts[i] < qEnd && gene->exonEnds[i] > qStart)
	{
	exnStarts[j] = gene->exonStarts[i] > qStart ? gene->exonStarts[i] : qStart;
	exnEnds[j] = gene->exonEnds[i] < qEnd ? gene->exonEnds[i] : qEnd;
	j++;
	}
    }
genePredFree(&gene);

/* Writing body of alignment. */
body = mustOpen(bodyTn.forCgi, "w");
htmStartDirDepth(body, psl->qName, 2);

/* protein psl's have a tEnd that isn't quite right */
if ((psl->strand[1] == '+') && (qType == gftProt))
    tEnd = psl->tStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1] * 3;

tSeq = hDnaFromSeq(database, seqName, psl->tStart, psl->tEnd, dnaLower);

freez(&tSeq->name);
tSeq->name = cloneString(psl->tName);
safef(tName, sizeof(tName), "%s.%s", organism, psl->tName);
if (psl->qName == NULL)
    fprintf(body, "<H2>Alignment of %s and %s:%d-%d</H2>\n",
	    entryName, psl->tName, psl->tStart+1, psl->tEnd);
else
    fprintf(body, "<H2>Alignment of %s and %s:%d-%d</H2>\n",
	    entryName, psl->tName, psl->tStart+1, psl->tEnd);

fputs("Click on links in the frame to the left to navigate through "
      "the alignment.\n", body);

safef(tName, sizeof(tName), "%s.%s", organism, psl->tName);
blockCount = pslGenoShowAlignment(psl, qType == gftProt, entryName, qSeq, qStart, qEnd,
                                  tName, tSeq, tStart, tEnd, exnStarts, exnEnds, j, body);
freez(&exnStarts);
freez(&exnEnds);
freeDnaSeq(&tSeq);

htmEnd(body);
fclose(body);
chmod(bodyTn.forCgi, 0666);

/* Write index. */
index = mustOpen(indexTn.forCgi, "w");
if (entryName == NULL)
    entryName = psl->qName;
htmStartDirDepth(index, entryName, 2);
fprintf(index, "<H3>Alignment of %s</H3>", entryName);
fprintf(index, "<A HREF=\"../%s#cDNA\" TARGET=\"body\">%s</A><BR>\n", bodyTn.forCgi, entryName);
fprintf(index, "<A HREF=\"../%s#genomic\" TARGET=\"body\">%s.%s</A><BR>\n", bodyTn.forCgi, hOrganism(database), psl->tName);
for (i=1; i<=blockCount; ++i)
    {
    fprintf(index, "<A HREF=\"../%s#%d\" TARGET=\"body\">block%d</A><BR>\n",
	    bodyTn.forCgi, i, i);
    }
fprintf(index, "<A HREF=\"../%s#ali\" TARGET=\"body\">together</A><BR>\n", bodyTn.forCgi);
htmEnd(index);
fclose(index);
chmod(indexTn.forCgi, 0666);

/* Write (to stdout) the main html page containing just the frame info. */
puts("<FRAMESET COLS = \"13%,87% \" >");
printf("  <FRAME SRC=\"%s\" NAME=\"index\">\n", indexTn.forCgi);
printf("  <FRAME SRC=\"%s\" NAME=\"body\">\n", bodyTn.forCgi);
puts("<NOFRAMES><BODY></BODY></NOFRAMES>");
puts("</FRAMESET>");
puts("</HTML>\n");
exit(0);	/* Avoid cartHtmlEnd. */
}


void potentPslAlign(char *htcCommand, char *item)
{/* show the detail psl alignment between genome */
char *pslTable = cgiString("pslTable");
char *chrom = cgiString("chrom");
int start = cgiInt("cStart");
int end = cgiInt("cEnd");
struct psl *psl = NULL;
struct dnaSeq *qSeq = NULL;
char *db = cgiString("db");
char name[64];
char query[256], fullTable[HDB_MAX_TABLE_STRING];
char **row;
boolean hasBin;
struct sqlResult *sr = NULL;
struct sqlConnection *conn = hAllocConn(database);

if (!hFindSplitTable(database, seqName, pslTable, fullTable, sizeof fullTable, &hasBin))
    errAbort("track %s not found", pslTable);

sqlSafef(query, sizeof query, "SELECT * FROM %s WHERE "
        "tName = '%s' AND tStart = %d "
	"AND tEnd = %d",
        pslTable, chrom, start, end);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if(row != NULL)
    {
    psl = pslLoad(row+hasBin);
    }
else
    {
    errAbort("No alignment infomation\n");
    }
qSeq = loadGenomePart(db, psl->qName, psl->qStart, psl->qEnd);
safef(name, sizeof name, "%s in %s(%d-%d)", item,psl->qName, psl->qStart, psl->qEnd);

char title[1024];
safef(title, sizeof title, "%s %dk", name, psl->qStart/1000);
htmlFramesetStart(title);
showSomeAlignment2(psl, qSeq, gftDnaX, psl->qStart, psl->qEnd, name, item, "", psl->qStart, psl->qEnd);
}

void doPutaFrag(struct trackDb *tdb, char *item)
/* display the potential pseudo and coding track */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row, table[256], query[256], *parts[6];
struct putaInfo *info = NULL;
int start = cartInt(cart, "o"),  end = cartInt(cart, "t");
char *db = cgiString("db");
char *name = cartString(cart, "i"),  *chr = cartString(cart, "c");
char pslTable[256];
char otherString[256];

safef(table, sizeof table, "putaInfo");
safef(pslTable, sizeof pslTable, "potentPsl");
cartWebStart(cart, database, "Putative Coding or Pseudo Fragments");
sqlSafef(query, sizeof query, "SELECT * FROM %s WHERE name = '%s' "
        "AND chrom = '%s' AND chromStart = %d "
        "AND chromEnd = %d",
         table, name, chr, start, end);

sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);

if(row != NULL)
    {
    info = putaInfoLoad(row+1);
    }
else
    {
    errAbort("Can't find information for %s in data base\n", name);
    }
sqlFreeResult(&sr);

char *tempName = cloneString(name);
chopByChar(tempName, '|',parts, 4);

printf("<B>%s</B> is homologous to the known gene: <A HREF=\"", name);
printEntrezNucleotideUrl(stdout, parts[0]);
printf("\" TARGET=_blank>%s</A><BR>\n", parts[0]);
printf("<B>%s </B>is aligned here with score : %d<BR><BR>\n", parts[0], info->score);

/* print the info about the stamper gene */
printf("<B> %s</B><BR>\n", parts[0]);
printf("<B>Genomic location of the mapped part of %s</B>: <A HREF=\""
       "%s?db=%s&position=%s:%d-%d\" TARGET=_blank>%s(%s):%d-%d </A> <BR>\n",
       parts[0], hgTracksName(), db, info->oChrom, info->oChromStart, info->oChromEnd,
       info->oChrom, parts[2],info->oChromStart+1, info->oChromEnd);
printf("<B>Mapped %s Exons</B>: %d of %d. <BR> <B>Mapped %s CDS exons</B>: %d of %d <BR>\n", parts[0], info->qExons[0], info->qExons[1], parts[0], info->qExons[2], info->qExons[3]);

printf("<b>Aligned %s bases</B>:%d of %d with %f identity. <BR> <B>Aligned %s CDS bases</B>:  %d of %d with %f identity.<BR><BR>\n", parts[0],info->qBases[0], info->qBases[1], info->id[0], parts[0], info->qBases[2], info->qBases[3], info->id[1]);

/* print info about the stamp putative element */
printf("<B>%s </B><BR> <B>Genomic location: </B>"
       " <A HREF=\"%s?db=%s&position=%s:%d-%d\" >%s(%s): %d - %d</A> <BR> <B> Element Structure: </B> %d putative exons and %d putative cds exons<BR><BR>\n",
       name, hgTracksName(), db, info->chrom, info->chromStart+1, info->chromEnd, info->chrom, info->strand, info->chromStart+1, info->chromEnd, info->tExons[0], info->tExons[1]);
if(info->repeats[0] > 0)
    {
    printf("Repeats elements inserted into %s <BR>\n", name);
    }
if(info->stop >0)
    {
    int k = 0;
    printf("Premature stops in block ");
    for(k = 0; k < info->blockCount; k++)
	{
	if(info->stops[k] > 0)
	    {
	    if(info->strand[0] == '+')
		printf("%d ",k+1);
	    else
		printf("%d ", info->blockCount - k);
	    }
	}
    printf("<BR>\n");
    }


/* show genome sequence */
hgcAnchorSomewhere("htcGeneInGenome", info->name, tdb->track, seqName);
printf("View DNA for this putative fragment</A><BR>\n");

/* show the detail alignment */
sqlSafef(query, sizeof query, "SELECT * FROM %s WHERE "
	"tName = '%s' AND tStart = %d "
	"AND tEnd = %d AND strand = '%c%c'",
	pslTable, info->chrom, info->chromStart, info->chromEnd, parts[2][0], info->strand[0]);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if(row != NULL)
    {
    safef(otherString, sizeof otherString, "&db=%s&pslTable=%s&chrom=%s&cStart=%d&cEnd=%d&strand=%s&qStrand=%s",
	    database, pslTable, info->chrom,info->chromStart, info->chromEnd, info->strand, parts[2]);
    hgcAnchorSomewhere("potentPsl", parts[0], otherString, info->chrom);
    printf("<BR>View details of parts of alignment </A>.</BR>\n");
    }
sqlFreeResult(&sr);
putaInfoFree(&info);
hFreeConn(&conn);
}

void doInterPro(struct trackDb *tdb, char *itemName)
{
char condStr[255];
char *desc;
struct sqlConnection *conn;

genericHeader(tdb, itemName);

conn = hAllocConn(database);
sqlSafef(condStr, sizeof condStr, "interProId='%s'", itemName);
desc = sqlGetField("proteome", "interProXref", "description", condStr);

printf("<B>Item:</B> %s <BR>\n", itemName);
printf("<B>Description:</B> %s <BR>\n", desc);
printf("<B>Outside Link:</B> ");
printf("<A HREF=");

printf("http://www.ebi.ac.uk/interpro/DisplayIproEntry?ac=%s", itemName);
printf(" Target=_blank> %s </A> <BR>\n", itemName);

printTrackHtml(tdb);
hFreeConn(&conn);
}

void doDv(struct trackDb *tdb, char *itemName)
{
char *table = tdb->table;
struct dvBed dvBed;
struct dv *dv;
struct dvXref2 *dvXref2;
struct omimTitle *omimTitle;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr, *sr2, *sr3, *sr4;
char **row;
char query[256], query2[256], query3[256], query4[256];

int rowOffset = hOffsetPastBin(database, seqName, table);
int start = cartInt(cart, "o");

genericHeader(tdb, itemName);

printf("<B>Item:</B> %s <BR>\n", itemName);
printf("<B>Outside Link:</B> ");
printf("<A HREF=");
printSwissProtVariationUrl(stdout, itemName);
printf(" Target=_blank> %s </A> <BR>\n", itemName);

sqlSafef(query, sizeof(query),
      "select * from %s where chrom = '%s' and "
      "chromStart=%d and name = '%s'", table, seqName, start, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    dvBedStaticLoad(row+rowOffset, &dvBed);
    bedPrintPos((struct bed *)&dvBed, 3, tdb);
    }
sqlFreeResult(&sr);

sqlSafef(query2, sizeof(query2), "select * from dv where varId = '%s' ", itemName);
sr2 = sqlGetResult(conn, query2);
while ((row = sqlNextRow(sr2)) != NULL)
    {
    /* not using static load */
    dv = dvLoad(row);
    printf("<B>Swiss-prot ID:</B> %s <BR>\n", dv->spID);
    printf("<B>Start:</B> %d <BR>\n", dv->start);
    printf("<B>Length:</B> %d <BR>\n", dv->len);
    printf("<B>Original:</B> %s <BR>\n", dv->orig);
    printf("<B>Variant:</B> %s <BR>\n", dv->variant);
    dvFree(&dv);
    }
sqlFreeResult(&sr2);

sqlSafef(query3, sizeof(query3), "select * from dvXref2 where varId = '%s' ", itemName);
char *protDbName = hPdbFromGdb(database);
struct sqlConnection *protDbConn = hAllocConn(protDbName);
sr3 = sqlGetResult(protDbConn, query3);
while ((row = sqlNextRow(sr3)) != NULL)
    {
    dvXref2 = dvXref2Load(row);
    if (sameString("MIM", dvXref2->extSrc))
        {
        printf("<B>OMIM:</B> ");
        printf("<A HREF=");
        printOmimUrl(stdout, dvXref2->extAcc);
        printf(" Target=_blank> %s</A> \n", dvXref2->extAcc);
	/* nested query here */
        if (hTableExists(database, "omimTitle"))
	    {
            sqlSafef(query4, sizeof(query4), "select * from omimTitle where omimId = '%s' ", dvXref2->extAcc);
            sr4 = sqlGetResult(conn, query4);
            while ((row = sqlNextRow(sr4)) != NULL)
                {
		omimTitle = omimTitleLoad(row);
		printf("%s\n", omimTitle->title);
		omimTitleFree(&omimTitle);
		}
	    }
	    printf("<BR>\n");
	}
    dvXref2Free(&dvXref2);
    }
sqlFreeResult(&sr3);
hFreeConn(&protDbConn);

printTrackHtml(tdb);
hFreeConn(&conn);
}

void printOregannoLink (struct oregannoLink *link)
/* this prints a link for oreganno */
{
struct hash *linkInstructions = NULL;
struct hash *thisLink = NULL;
char *linktype, *label = NULL;

hgReadRa(database, organism, rootDir, "links.ra", &linkInstructions);
/* determine how to do link from .ra file */
thisLink = hashFindVal(linkInstructions, link->raKey);
if (thisLink == NULL)
    return; /* no link found */
/* type determined by fields eg url */
linktype = hashFindVal(thisLink, "url");
label = hashFindVal(thisLink, "label");
if (linktype != NULL)
    {
    char url[256];
    char *accFlag = hashFindVal(thisLink, "acc");
    if (accFlag == NULL)
        safecpy(url, sizeof(url), linktype);
    else
        {
        char *accNum = hashFindVal(thisLink, "accNum");
        if (accNum == NULL)
            safef(url, sizeof(url), linktype, link->attrAcc);
        else if (sameString(accNum, "2"))
            {
            char *val[2];
	    char *copy = cloneString(link->attrAcc);
            if (2 == chopString(copy, ",", val, 2))
	        safef(url, sizeof(url), linktype, val[0], val[1]);
            }
        }
    if (label == NULL)
        label = "";  /* no label */
    printf("%s - <A HREF=\"%s\" TARGET=\"_BLANK\">%s</A>\n", label, url, link->attrAcc);
    }
}

void doOreganno(struct trackDb *tdb, char *itemName)
{
char *table = tdb->table;
struct oreganno *r = NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
char *prevLabel = NULL;
int i = 0, listStarted = 0;

//int start = cartInt(cart, "o");

genericHeader(tdb, itemName);

/* postion, band, genomic size */
sqlSafef(query, sizeof(query),
      "select * from %s where name = '%s'", table, itemName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    r = oregannoLoad(row);
    printf("<B>ORegAnno ID:</B> %s <BR>\n", r->id);
    #if 0 // all the same as the ID for now
        printf("<B>ORegAnno name:</B> %s <BR>\n", r->name);
    #endif
    printf("<B>Strand:</B> %s<BR>\n", r->strand);
    bedPrintPos((struct bed *)r, 3, tdb);
    /* start html list for attributes */
    printf("<DL>");
    }
sqlFreeResult(&sr);

if (sameString(table, "oregannoOther"))
    {
    printf("<B>Attributes as described from other species</B><BR>\n");
    }
/* fetch and print the attributes */
for (i=0; i < oregannoAttrSize; i++)
    {
    int used = 0;
    char *tab;
    if (sameString(table, "oregannoOther"))
        tab = cloneString("oregannoOtherAttr");
    else
	tab = cloneString("oregannoAttr");
    /* names are quote safe, come from oregannoUi.c */
    sqlSafef(query, sizeof(query), "select * from %s where id = '%s' and attribute = '%s'", tab, r->id, oregannoAttributes[i]);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        struct oregannoAttr attr;
        used++;
        if (used == 1)
            {
            if (!prevLabel || differentString(prevLabel, oregannoAttrLabel[i]))
                {
                if (listStarted == 0)
                    listStarted = 1;
                else
                    printf("</DD>");

                printf("<DT><b>%s:</b></DT><DD>\n", oregannoAttrLabel[i]);
                freeMem(prevLabel);
                prevLabel = cloneString(oregannoAttrLabel[i]);
                }
            }
        oregannoAttrStaticLoad(row, &attr);
        printf("%s ", attr.attrVal);
        printf("<BR>\n");
        }
    freeMem(tab);
    if (sameString(table, "oregannoOther"))
        tab = cloneString("oregannoOtherLink");
    else
        tab = cloneString("oregannoLink");
    sqlSafef(query, sizeof(query), "select * from %s where id = '%s' and attribute = '%s'", tab, r->id, oregannoAttributes[i]);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        struct oregannoLink link;
        used++;
        if (used == 1)
            {
            if (!prevLabel || differentString(prevLabel, oregannoAttrLabel[i]))
                {
                if (listStarted == 0)
                    listStarted = 1;
                else
                    printf("</DD>");

                printf("<DT><b>%s:</b></DT><DD>\n", oregannoAttrLabel[i]);
                freeMem(prevLabel);
                prevLabel = cloneString(oregannoAttrLabel[i]);
                }
            }
        oregannoLinkStaticLoad(row, &link);
        printOregannoLink(&link);
        printf("<BR>\n");
        }
    freeMem(tab);
    }
if (listStarted > 0)
    printf("</DD></DL>");

oregannoFree(&r);
freeMem(prevLabel);
printTrackHtml(tdb);
hFreeConn(&conn);
}

void doSnpArray (struct trackDb *tdb, char *itemName, char *dataSource)
{
char *table = tdb->table;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int start = cartInt(cart, "o");
int end = 0;
// char *chrom = cartString(cart, "c");
char nibName[HDB_MAX_PATH_STRING];
struct dnaSeq *seq;

genericHeader(tdb, itemName);

/* Affy uses their own identifiers */
if (sameString(dataSource, "Affy"))
    sqlSafef(query, sizeof(query),
        "select chromEnd, strand, observed, rsId from %s where chrom = '%s' and chromStart=%d", table, seqName, start);
else
    sqlSafef(query, sizeof(query), "select chromEnd, strand, observed from %s where chrom = '%s' and chromStart=%d", table, seqName, start);

sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    end = sqlUnsigned(row[0]);
    printPosOnChrom(seqName, start, end, row[1], FALSE, NULL);
    printf("<B>Polymorphism:</B> %s \n", row[2]);

    if (end == start + 1)
        {
        hNibForChrom(database, seqName, nibName);
        seq = hFetchSeq(nibName, seqName, start, end);
	touppers(seq->dna);
        if (sameString(row[1], "-"))
           reverseComplement(seq->dna, 1);
        printf("<BR><B>Reference allele:</B> %s \n", seq->dna);
        }

    if (sameString(dataSource, "Affy"))
        {
        printf("<BR><BR><A HREF=\"https://www.affymetrix.com/LinkServlet?probeset=%s\" TARGET=_blank>NetAffx</A> (log in required, registration is free)\n", itemName);
        if (regexMatch(row[3], "^rs[0-9]+$"))
            {
	    printf("<BR>");
	    printDbSnpRsUrl(row[3], "dbSNP (%s)", row[3]);
	    }
	}
    else if (regexMatch(itemName, "^rs[0-9]+$"))
        {
	printf("<BR>");
	printDbSnpRsUrl(itemName, "dbSNP (%s)", itemName);
	}
    }
sqlFreeResult(&sr);
printTrackHtml(tdb);
hFreeConn(&conn);
}

void doSnpArray2 (struct trackDb *tdb, char *itemName, char *dataSource)
/* doSnpArray2 is essential the same as doSnpArray except that the strand is blanked out */
/* This is a temp solution for 3 Illumina SNP Arrays to blank out strand info for non-dbSnp entries */
/* Should be removed once Illumina comes up with a clear defintion of their strand data */
{
char *table = tdb->table;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int start = cartInt(cart, "o");
int end = 0;
char nibName[HDB_MAX_PATH_STRING];
struct dnaSeq *seq;

genericHeader(tdb, itemName);
/* Affy uses their own identifiers */
if (sameString(dataSource, "Affy"))
    sqlSafef(query, sizeof(query),
        "select chromEnd, strand, observed, rsId from %s where chrom = '%s' and chromStart=%d", table, seqName, start);
else
    sqlSafef(query, sizeof(query), "select chromEnd, strand, observed from %s where chrom = '%s' and chromStart=%d", table, seqName, start);

sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    end = sqlUnsigned(row[0]);

    /* force strand info to be blank for non-dbSnp entries, per Illumina's request */
    printPosOnChrom(seqName, start, end, " ", FALSE, NULL);
    printf("<B>Polymorphism:</B> %s \n", row[2]);

    if (end == start + 1)
        {
        hNibForChrom(database, seqName, nibName);
        seq = hFetchSeq(nibName, seqName, start, end);
	touppers(seq->dna);
        if (sameString(row[1], "-"))
           reverseComplement(seq->dna, 1);
        printf("<BR><B>Reference allele:</B> %s \n", seq->dna);
        }

    if (sameString(dataSource, "Affy"))
        {
        printf("<BR><BR><A HREF=\"https://www.affymetrix.com/LinkServlet?probeset=%s\" TARGET=_blank>NetAffx</A> (log in required, registration is free)\n", itemName);
        if (regexMatch(row[3], "^rs[0-9]+$"))
            {
            printf("<BR>");
	    printDbSnpRsUrl(row[3], "dbSNP (%s)", row[3]);
	    }
	}
    else if (regexMatch(itemName, "^rs[0-9]+$"))
        {
        printf("<BR>");
	printDbSnpRsUrl(itemName, "dbSNP (%s)", itemName);
	}
    }
sqlFreeResult(&sr);
printTrackHtml(tdb);
hFreeConn(&conn);
}

void printGvAttrCatType (int i)
/* prints new category and type labels for attributes as needed */
{
/* only print name and category if different */
if (gvPrevCat == NULL)
    {
    /* print start of both */
    /* if need to print category layer, here is where print first */
    printf("<DT><B>%s:</B></DT><DD>\n", gvAttrTypeDisplay[i]);
    gvPrevCat = cloneString(gvAttrCategory[i]);
    gvPrevType = cloneString(gvAttrTypeDisplay[i]);
    }
else if (differentString(gvPrevCat, gvAttrCategory[i]))
    {
    /* end last, and print start of both */
    printf("</DD>");
    /* if/when add category here is where to print next */
    printf("<DT><B>%s:</B></DT><DD>\n", gvAttrTypeDisplay[i]);
    freeMem(gvPrevType);
    gvPrevType = cloneString(gvAttrTypeDisplay[i]);
    freeMem(gvPrevCat);
    gvPrevCat = cloneString(gvAttrCategory[i]);
    }
else if (sameString(gvPrevCat, gvAttrCategory[i]) &&
        differentString(gvPrevType, gvAttrTypeDisplay[i]))
    {
    /* print new name */
    printf("</DD>");
    printf("<DT><B>%s:</B></DT><DD>\n", gvAttrTypeDisplay[i]);
    freeMem(gvPrevType);
    gvPrevType = cloneString(gvAttrTypeDisplay[i]);
    }
/* else don't need type or category */
}

void printLinksRaLink (char *acc, char *raKey, char *displayVal)
/* print a link with instructions in hgcData/links.ra file */
{
struct hash *linkInstructions = NULL;
struct hash *thisLink = NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
char *linktype, *label;
char *doubleEntry = NULL;

hgReadRa(database, organism, rootDir, "links.ra", &linkInstructions);

/* determine how to do link from .ra file */
thisLink = hashFindVal(linkInstructions, raKey);
if (thisLink == NULL)
    return; /* no link found */
/* type determined by fields: url = external, dataSql = internal, others added later? */
/* need to print header here for some displays */
linktype = hashFindVal(thisLink, "dataSql");
label = hashFindVal(thisLink, "label");
if (label == NULL)
    label = "";
if (linktype != NULL)
    {
    sqlSafef(query, sizeof(query), linktype, acc);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        /* should this print more than 1 column, get count from ra? */
        if (row[0] != NULL)
            {
            /* print label and result */
            printf("<B>%s</B> - %s", label, row[0]);
            /* check for link */
            doubleEntry = hashFindVal(thisLink, "dataLink");
            if (doubleEntry != NULL)
                {
                char url[512];
                struct hash *newLink;
                char *accCol = NULL, *format = NULL;
                int colNum = 1;
                newLink = hashFindVal(linkInstructions, doubleEntry);
                accCol = hashFindVal(thisLink, "dataLinkCol");
                if (newLink == NULL || accCol == NULL)
                   errAbort("missing required fields in .ra file");
                colNum = atoi(accCol);
                format = hashFindVal(newLink, "url");
                safef(url, sizeof(url), format, row[colNum - 1]);
                printf(" - <A HREF=\"%s\" TARGET=_blank>%s</A>\n",
                    url, row[colNum - 1]);
                }
            printf("<BR>\n");
            }
        }
    sqlFreeResult(&sr);
    }
else
    {
    linktype = hashFindVal(thisLink, "url");
    if (linktype != NULL)
        {
        char url[512];
        char *encodedAcc = cgiEncode(acc);
        char *encode = hashFindVal(thisLink, "needsEncoded");
        if (encode != NULL && sameString(encode, "yes"))
            safef(url, sizeof(url), linktype, encodedAcc);
        else
            safef(url, sizeof(url), linktype, acc);
        if (displayVal == NULL || sameString(displayVal, ""))
            printf("<B>%s</B> - <A HREF=\"%s\" TARGET=_blank>%s</A><BR>\n", label, url, acc);
        else
            printf("<B>%s</B> - <A HREF=\"%s\" TARGET=_blank>%s</A><BR>\n", label, url, displayVal);
        }
    }
hFreeConn(&conn);
return;
}

int printProtVarLink (char *id, int i)
{
struct protVarLink *link = NULL;
struct hash *linkInstructions = NULL;
struct hash *thisLink = NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlConnection *conn2 = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
char *linktype, *label;
char *doubleEntry = NULL;
int attrCnt = 0;

hgReadRa(database, organism, rootDir, "links.ra", &linkInstructions);
sqlSafef(query, sizeof(query),
     "select * from protVarLink where id = '%s' and attrType = '%s'",
     id, gvAttrTypeKey[i]);
/* attrType == gvAttrTypeKey should be quote safe */

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct sqlResult *sr2;
    char **row2;

    attrCnt++;
    link = protVarLinkLoad(row);
    /* determine how to do link from .ra file */
    thisLink = hashFindVal(linkInstructions, link->raKey);
    if (thisLink == NULL)
        continue; /* no link found */
    /* type determined by fields: url = external, dataSql = internal, others added later? */
    printGvAttrCatType(i); /* only print header if data */
    linktype = hashFindVal(thisLink, "dataSql");
    label = hashFindVal(thisLink, "label");
    if (label == NULL)
        label = "";
    if (linktype != NULL)
        {
        sqlSafef(query, sizeof(query), linktype, link->acc);
        sr2 = sqlGetResult(conn2, query);
        while ((row2 = sqlNextRow(sr2)) != NULL)
            {
            /* should this print more than 1 column, get count from ra? */
            if (row2[0] != NULL)
                {
                /* print label and result */
                printf("<B>%s</B> - %s", label, row2[0]);
                /* check for link */
                doubleEntry = hashFindVal(thisLink, "dataLink");
                if (doubleEntry != NULL)
                    {
                    char url[512];
                    struct hash *newLink;
                    char *accCol = NULL, *format = NULL;
                    int colNum = 1;
	            newLink = hashFindVal(linkInstructions, doubleEntry);
                    accCol = hashFindVal(thisLink, "dataLinkCol");
                    if (newLink == NULL || accCol == NULL)
                       errAbort("missing required fields in .ra file");
                    colNum = atoi(accCol);
                    format = hashFindVal(newLink, "url");
                    safef(url, sizeof(url), format, row2[colNum - 1]);
                    printf(" - <A HREF=\"%s\" TARGET=_blank>%s</A>\n",
                        url, row2[colNum - 1]);
                    }
                printf("<BR>\n");
                }
            }
        sqlFreeResult(&sr2);
        }
    else
        {
        linktype = hashFindVal(thisLink, "url");
        if (linktype != NULL)
            {
            char url[1024];
            char *encodedAcc = cgiEncode(link->acc);
            char *encode = hashFindVal(thisLink, "needsEncoded");
            if (encode != NULL && sameString(encode, "yes"))
                safef(url, sizeof(url), linktype, encodedAcc);
            else
                safef(url, sizeof(url), linktype, link->acc);
            if (sameString(link->displayVal, ""))
                printf("<B>%s</B> - <A HREF=\"%s\" TARGET=_blank>%s</A><BR>\n", label, url, link->acc);
            else
                printf("<B>%s</B> - <A HREF=\"%s\" TARGET=_blank>%s</A><BR>\n", label, url, link->displayVal);
            }
        }
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
hFreeConn(&conn2);
return attrCnt;
}

int printGvLink (char *id, int i)
{
struct gvLink *link = NULL;
struct hash *linkInstructions = NULL;
struct hash *thisLink = NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlConnection *conn2 = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
char *linktype, *label;
char *doubleEntry = NULL;
int attrCnt = 0;

hgReadRa(database, organism, rootDir, "links.ra", &linkInstructions);
sqlSafef(query, sizeof(query),
     "select * from hgFixed.gvLink where id = '%s' and attrType = '%s'",
     id, gvAttrTypeKey[i]);
/* attrType == gvAttrTypeKey should be quote safe */

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct sqlResult *sr2;
    char **row2;

    attrCnt++;
    link = gvLinkLoad(row);
    /* determine how to do link from .ra file */
    thisLink = hashFindVal(linkInstructions, link->raKey);
    if (thisLink == NULL)
        continue; /* no link found */
    /* type determined by fields: url = external, dataSql = internal, others added later? */
    printGvAttrCatType(i); /* only print header if data */
    linktype = hashFindVal(thisLink, "dataSql");
    label = hashFindVal(thisLink, "label");
    if (label == NULL)
        label = "";
    if (linktype != NULL)
        {
        sqlSafef(query, sizeof(query), linktype, link->acc);
        sr2 = sqlGetResult(conn2, query);
        while ((row2 = sqlNextRow(sr2)) != NULL)
            {
            /* should this print more than 1 column, get count from ra? */
            if (row2[0] != NULL)
                {
                /* print label and result */
                printf("<B>%s</B> - %s", label, row2[0]);
                /* check for link */
                doubleEntry = hashFindVal(thisLink, "dataLink");
                if (doubleEntry != NULL)
                    {
                    char url[512];
                    struct hash *newLink;
                    char *accCol = NULL, *format = NULL;
                    int colNum = 1;
	            newLink = hashFindVal(linkInstructions, doubleEntry);
                    accCol = hashFindVal(thisLink, "dataLinkCol");
                    if (newLink == NULL || accCol == NULL)
                       errAbort("missing required fields in .ra file");
                    colNum = atoi(accCol);
                    format = hashFindVal(newLink, "url");
                    safef(url, sizeof(url), format, row2[colNum - 1]);
                    printf(" - <A HREF=\"%s\" TARGET=_blank>%s</A>\n",
                        url, row2[colNum - 1]);
                    }
                printf("<BR>\n");
                }
            }
        sqlFreeResult(&sr2);
        }
    else
        {
        linktype = hashFindVal(thisLink, "url");
        if (linktype != NULL)
            {
            char url[1024];
            char *encodedAcc = cgiEncode(link->acc);
            char *encode = hashFindVal(thisLink, "needsEncoded");
            if (encode != NULL && sameString(encode, "yes"))
                safef(url, sizeof(url), linktype, encodedAcc);
            else
                safef(url, sizeof(url), linktype, link->acc);
            /* bounce srcLinks through PSU first for disclaimers */
            if (sameString(link->attrType, "srcLink"))
                {
                char *copy = cgiEncode(url);
                safef(url, sizeof(url), "http://phencode.bx.psu.edu/cgi-bin/phencode/link-disclaimer?src=%s&link=%s", link->raKey, copy);
                }
            if (sameString(link->displayVal, ""))
                printf("<B>%s</B> - <A HREF=\"%s\" TARGET=_blank>%s</A><BR>\n", label, url, link->acc);
            else
                printf("<B>%s</B> - <A HREF=\"%s\" TARGET=_blank>%s</A><BR>\n", label, url, link->displayVal);
            }
        }
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
hFreeConn(&conn2);
return attrCnt;
}

void doOmicia(struct trackDb *tdb, char *itemName)
/* this prints the detail page for the Omicia track */
{
struct omiciaLink *link = NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];

/* print generic bed start */
doBed6FloatScore(tdb, itemName);

/* print links */
sqlSafef(query, sizeof(query), "select * from omiciaLink where id = '%s'", itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    link = omiciaLinkLoad(row);
    printLinksRaLink(link->acc, link->raKey, link->displayVal);
    }
sqlFreeResult(&sr);

printTrackHtml(tdb);
}

void doOmiciaOld (struct trackDb *tdb, char *itemName)
/* this prints the detail page for the Omicia OMIM track */
{
char *table = tdb->table;
struct omiciaLink *link = NULL;
struct omiciaAttr *attr = NULL;
void *omim = NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int start = cartInt(cart, "o");

genericHeader(tdb, itemName);
printf("<B>Name:</B> %s<BR>\n", itemName);
sqlSafef(query, sizeof(query),
      "select * from %s where chrom = '%s' and "
      "chromStart = %d and name = '%s'", table, seqName, start, itemName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    float score;
    struct omiciaAuto *om;
    if (sameString(table, "omiciaAuto"))
        omim = omiciaAutoLoad(row);
    else
        omim = omiciaHandLoad(row);
    om = (struct omiciaAuto *)omim;
    printPos(om->chrom, om->chromStart, om->chromEnd, om->strand, TRUE, om->name);
    /* print score separately, so can divide by 100 to retrieve original */
    score = (float)om->score / 100.00;
    printf("<B>Confidence score:</B> %g<BR>\n", score);
    }
sqlFreeResult(&sr);

/* print links */
sqlSafef(query, sizeof(query), "select * from omiciaLink where id = '%s'", itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    link = omiciaLinkLoad(row);
    printLinksRaLink(link->acc, link->raKey, link->displayVal);
    }
sqlFreeResult(&sr);

/* print attributes */
sqlSafef(query, sizeof(query), "select * from omiciaAttr where id = '%s' order by attrType", itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    attr = omiciaAttrLoad(row);
    /* start with simple case print label and value */
    printf("<B>%s:</B> %s<BR>\n", attr->attrType, attr->attrVal);
    }
sqlFreeResult(&sr);

printTrackHtml(tdb);
}

void doProtVar (struct trackDb *tdb, char *itemName)
/* this prints the detail page for the UniProt variation track */
{
char *table = tdb->table;
struct protVarPos *mut = NULL;
struct protVar *details = NULL;
struct protVarAttr attr;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int hasAttr = 0;
int i;
int start = cartInt(cart, "o");

/* official name, position, band, genomic size */
sqlSafef(query, sizeof(query), "select * from protVar where id = '%s'", itemName);
details = protVarLoadByQuery(conn, query);

genericHeader(tdb, details->name);

/* change label based on species */
if (sameString(organism, "Human"))
    printf("<B>HGVS name:</B> %s <BR>\n", details->name);
else
    printf("<B>Official name:</B> %s <BR>\n", details->name);
sqlSafef(query, sizeof(query),
      "select * from %s where chrom = '%s' and "
      "chromStart=%d and name = '%s'", table, seqName, start, itemName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    mut = protVarPosLoad(row);
    printPos(mut->chrom, mut->chromStart, mut->chromEnd, mut->strand, TRUE, mut->name);
    }
sqlFreeResult(&sr);
printf("*Note the DNA retrieved by the above link is the genomic sequence.<br>");

/* print location and mutation type fields */
printf("<B>location:</B> %s<BR>\n", details->location);
printf("<B>type:</B> %s<BR>\n", details->baseChangeType);
/* add note here about exactness of coordinates */
if (details->coordinateAccuracy == 0)
    {
    printf("<B>note:</B> The coordinates for this mutation are only estimated.<BR>\n");
    }

printf("<DL>");

/* loop through attributes (uses same lists as gv*) */
for(i=0; i<gvAttrSize; i++)
    {
    /* check 2 attribute tables for each type */
    sqlSafef(query, sizeof(query),
        "select * from protVarAttr where id = '%s' and attrType = '%s'",
        itemName, gvAttrTypeKey[i]);
    /* attrType == gvAttrTypeKey should be quote safe */
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        hasAttr++;
        protVarAttrStaticLoad(row, &attr);
        printGvAttrCatType(i); /* only print header, if data */
        /* print value */
        printf("%s<BR>", attr.attrVal);
        }
    sqlFreeResult(&sr);
    hasAttr += printProtVarLink(itemName, i);
    }
if (hasAttr > 0)
    printf("</DD>");
printf("</DL>\n");

protVarPosFree(&mut);
freeMem(gvPrevCat);
freeMem(gvPrevType);
printTrackHtml(tdb);
hFreeConn(&conn);
}

void doGv(struct trackDb *tdb, char *itemName)
/* this prints the detail page for the Genome variation track */
{
char *table = tdb->table;
struct gvPos *mut = NULL;
struct gv *details = NULL;
struct gvAttr attr;
struct gvAttrLong attrLong;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
int hasAttr = 0;
int i;
int start = cartInt(cart, "o");

/* official name, position, band, genomic size */
sqlSafef(query, sizeof(query), "select * from hgFixed.gv where id = '%s'", itemName);
details = gvLoadByQuery(conn, query);

genericHeader(tdb, details->name);

/* change label based on species */
if (sameString(organism, "Human"))
    printf("<B>HGVS name:</B> %s <BR>\n", details->name);
else
    printf("<B>Official name:</B> %s <BR>\n", details->name);
sqlSafef(query, sizeof(query),
      "select * from %s where chrom = '%s' and "
      "chromStart=%d and name = '%s'", table, seqName, start, itemName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    char *strand = NULL;
    mut = gvPosLoad(row);
    strand = mut->strand;
    printPos(mut->chrom, mut->chromStart, mut->chromEnd, strand, TRUE, mut->name);
    }
sqlFreeResult(&sr);
if (mut == NULL)
    errAbort("Couldn't find variant %s at %s %d", itemName, seqName, start);
printf("*Note the DNA retrieved by the above link is the genomic sequence.<br>");

/* fetch and print the source */
sqlSafef(query, sizeof(query),
      "select * from hgFixed.gvSrc where srcId = '%s'", details->srcId);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    struct gvSrc *src = gvSrcLoad(row);
    printf("<B>source:</B> %s", src->lsdb);
    printf("<BR>\n");
    }
sqlFreeResult(&sr);

/* print location and mutation type fields */
printf("<B>location:</B> %s<BR>\n", details->location);
printf("<B>type:</B> %s<BR>\n", details->baseChangeType);
/* add note here about exactness of coordinates */
if (details->coordinateAccuracy == 0)
    {
    printf("<B>note:</B> The coordinates for this mutation are only estimated.<BR>\n");
    }

printf("<DL>");

/* loop through attributes */
for(i=0; i<gvAttrSize; i++)
    {
    /* check all 3 attribute tables for each type */
    sqlSafef(query, sizeof(query),
        "select * from hgFixed.gvAttrLong where id = '%s' and attrType = '%s'",
	itemName, gvAttrTypeKey[i]);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        hasAttr++;
        gvAttrLongStaticLoad(row, &attrLong);
        printGvAttrCatType(i); /* only print header, if data */
        /* print value */
        printf("%s<BR>", attrLong.attrVal);
        }
    sqlFreeResult(&sr);
    sqlSafef(query, sizeof(query),
        "select * from hgFixed.gvAttr where id = '%s' and attrType = '%s'",
        itemName, gvAttrTypeKey[i]);
    /* attrType == gvAttrTypeKey should be quote safe */
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        hasAttr++;
        gvAttrStaticLoad(row, &attr);
        printGvAttrCatType(i); /* only print header, if data */
        /* print value */
        printf("%s<BR>", attr.attrVal);
        }
    sqlFreeResult(&sr);
    hasAttr += printGvLink(itemName, i);
    }
if (hasAttr > 0)
    printf("</DD>");
printf("</DL>\n");

/* split code from printTrackHtml */
printTBSchemaLink(tdb);

printOrigAssembly(tdb);
printDataVersion(database, tdb);
printUpdateTime(database, tdb, NULL);

if (tdb->html != NULL && tdb->html[0] != 0)
    {
    htmlHorizontalLine();
    puts(tdb->html);
    }
hPrintf("<BR>\n");

gvPosFree(&mut);
freeMem(gvPrevCat);
freeMem(gvPrevType);
//printTrackHtml(tdb);
hFreeConn(&conn);
}

void doPgSnp(struct trackDb *tdb, char *itemName, struct customTrack *ct)
/* print detail page for personal genome track (pgSnp) */
{
char *table;
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
char query[256];
if (ct == NULL)
    {
    table = tdb->table;
    conn = hAllocConn(database);
    }
else
    {
    table = ct->dbTableName;
    conn = hAllocConn(CUSTOM_TRASH);
    //ct->tdb
    }

genericHeader(tdb, itemName);

sqlSafef(query, sizeof(query), "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d", 
    table, itemName, seqName, cartInt(cart, "o"));
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    struct pgSnp *el = pgSnpLoad(row);
    char *all[8];
    char *freq[8];
    char *score[8];
    char *name = cloneString(el->name);
    char *fr = NULL;
    char *sc = NULL;
    char *siftTab = trackDbSetting(tdb, "pgSiftPredTab");
    char *polyTab = trackDbSetting(tdb, "pgPolyphenPredTab");
    int i = 0;
    printPos(el->chrom, el->chromStart, el->chromEnd, "+", TRUE, el->name);
    printf("Alleles are relative to forward strand of reference genome:<br>\n");
    printf("<table class=\"descTbl\">"
	   "<tr><th>Allele</th><th>Frequency</th><th>Quality Score</th></tr>\n");
    chopByChar(name, '/', all, el->alleleCount);
    if (differentString(el->alleleFreq, ""))
        {
        fr = cloneString(el->alleleFreq);
        chopByChar(fr, ',', freq, el->alleleCount);
        }
    if (el->alleleScores != NULL)
        {
        sc = cloneString(el->alleleScores);
        chopByChar(sc, ',', score, el->alleleCount);
        }
    for (i=0; i < el->alleleCount; i++)
        {
        if (sameString(el->alleleFreq, "") || sameString(freq[i], "0"))
            freq[i] = "not available";
        if (sc == NULL || sameString(sc, ""))
            score[i] = "not available";
        printf("<tr><td>%s</td><td>%s</td><td>%s</td></tr>", all[i], freq[i], score[i]);
        }
    printf("</table>");
    if (!trackHubDatabase(database))
        printPgDbLink(database, tdb, el);
    if (siftTab != NULL)
        printPgSiftPred(database, siftTab, el);
    if (polyTab != NULL)
        printPgPolyphenPred(database, polyTab, el);
    char *genePredTable = "knownGene";
    if (!trackHubDatabase(database))
        printSeqCodDisplay(database, el, genePredTable);
    }
sqlFreeResult(&sr);
printTrackHtml(tdb);
hFreeConn(&conn);
}

void doPgPhenoAssoc(struct trackDb *tdb, char *itemName)
{
char *table = tdb->table;
struct pgPhenoAssoc *pheno = NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
struct dyString *query = dyStringNew(512);
int start = cartInt(cart, "o");

genericHeader(tdb, itemName);

sqlDyStringPrintf(query, "select * from %s where chrom = '%s' and ",
               table, seqName);
sqlDyStringPrintf(query, "name = '%s' and chromStart = %d", itemName, start);
sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    pheno = pgPhenoAssocLoad(row);
    bedPrintPos((struct bed *)pheno, 4, tdb);
    printf("Personal Genome phenotype: <a href=\"%s\">link to phenotype source</a><BR>\n", pheno->srcUrl);
    }
printTrackHtml(tdb);
}

void doAllenBrain(struct trackDb *tdb, char *itemName)
/* Put up page for Allen Brain Atlas. */
{
char *table = tdb->table;
struct psl *pslList;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn(database);
char *url, query[512];

genericHeader(tdb, itemName);

sqlSafef(query, sizeof(query),
        "select url from allenBrainUrl where name = '%s'", itemName);
url = sqlQuickString(conn, query);
printf("<H3><A HREF=\"%s\" target=_blank>", url);
printf("Click here to open Allen Brain Atlas on this probe.</A></H3><BR>");

pslList = getAlignments(conn, table, itemName);
puts("<H3>Probe/Genome Alignments</H3>");
printAlignments(pslList, start, "htcCdnaAli", table, itemName);

printTrackHtml(tdb);
hFreeConn(&conn);
}

void doExaptedRepeats(struct trackDb *tdb, char *itemName)
/* Respond to click on the exaptedRepeats track. */
{
struct sqlConnection *conn = hAllocConn(database);
char query[256];
struct sqlResult *sr;
char **row;
char *chr, *name;
unsigned int chromStart, chromEnd;
boolean blastzAln;

cartWebStart(cart, database, "%s", itemName);
sqlSafef(query, sizeof query, "select * from %s where name = '%s'", tdb->table, itemName);
selectOneRow(conn, tdb->table, query, &sr, &row);
chr = cloneString(row[0]);
chromStart = sqlUnsigned(row[1]);
chromEnd = sqlUnsigned(row[2]);
name = cloneString(row[3]);
blastzAln = (sqlUnsigned(row[4])==1);

printPos(chr, chromStart, chromEnd, NULL, TRUE, name);
printf("<B>Item:</B> %s<BR>\n", name);
if(blastzAln){printf("<B>Alignment to the repeat consensus verified with blastz:</B> yes<BR>\n");}
else{printf("<B>Alignment to repeat consensus verified with blastz:</B> no<BR>\n");}

sqlFreeResult(&sr);
hFreeConn(&conn);
printTrackHtml(tdb);
}

void doIgtc(struct trackDb *tdb, char *itemName)
/* Details for International Gene Trap Consortium. */
{
char *name = cloneString(itemName);
char *source = NULL;
char *encodedName = cgiEncode(itemName);

cgiDecode(name, name, strlen(name));
source = strrchr(name, '_');
if (source == NULL)
    source = "Unknown";
else
    source++;

genericHeader(tdb, name);
printf("<B>Source:</B> %s<BR>\n", source);
printCustomUrl(tdb, name, TRUE);
if (startsWith("psl", tdb->type))
    {
    struct sqlConnection *conn = hAllocConn(database);
    struct sqlResult *sr = NULL;
    struct dyString *query = dyStringNew(512);
    char **row = NULL;
    int rowOffset = hOffsetPastBin(database, seqName, tdb->table);
    int start = cartInt(cart, "o");
    int end = cartInt(cart, "t");
    sqlDyStringPrintf(query, "select * from %s where tName = '%s' and ",
		   tdb->table, seqName);
    if (rowOffset)
	hAddBinToQuery(start, end, query);
    sqlDyStringPrintf(query, "tStart = %d and qName = '%s'", start, itemName);
    sr = sqlGetResult(conn, query->string);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	struct psl *psl = pslLoad(row+rowOffset);
	printPos(psl->tName, psl->tStart, psl->tEnd, psl->strand, TRUE,
		 psl->qName);
	if (hGenBankHaveSeq(database, itemName, NULL))
	    {
	    printf("<H3>%s/Genomic Alignments</H3>", name);
	    printAlignments(psl, start, "htcCdnaAli", tdb->table,
			    encodedName);
	    }
	else
	    {
	    printf("<B>Alignment details:</B>\n");
	    pslDumpHtml(psl);
	    }
	pslFree(&psl);
	}
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }
else
    warn("Unsupported type \"%s\" for IGTC (expecting psl).", tdb->type);
printTrackHtml(tdb);
}

void doRdmr(struct trackDb *tdb, char *item)
/* details page for rdmr track */
{
struct sqlConnection *conn = hAllocConn(database);
char query[512];
struct sqlResult *sr;
char **row;
int ii;

char *chrom,*chromStart,*chromEnd,*fibroblast,*iPS,*absArea,*gene,*dist2gene,*relation2gene,*dist2island,*relation2island,*fdr;

genericHeader(tdb, item);

sqlSafef(query, sizeof(query),
"select chrom,chromStart,chromEnd,fibroblast,iPS,absArea,gene,dist2gene,relation2gene,dist2island,relation2island,fdr from rdmrRaw where gene = '%s'",
item);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);

    ii = 0;
chrom       = row[ii];ii++;
chromStart  = row[ii];ii++;
chromEnd    = row[ii];ii++;
fibroblast  = row[ii];ii++;
iPS         = row[ii];ii++;
absArea     = row[ii];ii++;
gene        = row[ii];ii++;
    dist2gene	= row[ii];ii++;
    relation2gene = row[ii];ii++;
    dist2island	= row[ii];ii++;
    relation2island = row[ii];ii++;
    fdr		= row[ii];

    printf("<B>Closest Gene:</B> %s\n", gene);fflush(stdout);
    printf("<BR><B>Genomic Position:</B> %s:%s-%s", chrom, chromStart, chromEnd);

    printf("<BR><B>Fibroblast M value:</B> %s\n", fibroblast);
    printf("<BR><B>iPS M value:</B> %s\n", iPS);
    printf("<BR><B>Absolute area:</B> %s", absArea);
    printf("<BR><B>Distance to gene:</B> %s\n", dist2gene);
    printf("<BR><B>Relation to gene:</B> %s\n", relation2gene);
    printf("<BR><B>Distance to CGI:</B> %s\n",  dist2island);
    printf("<BR><B>Relation to CGI:</B> %s\n", relation2island);
    printf("<BR><B>False discovery rate:</B> %s\n", fdr);
sqlFreeResult(&sr);
printTrackHtml(tdb);
hFreeConn(&conn);
}
void doKomp(struct trackDb *tdb, char *item)
/* KnockOut Mouse Project */
{
struct sqlConnection *conn = hAllocConn(database);
char query[512];
struct sqlResult *sr;
char **row;
genericHeader(tdb, item);
char defaultExtra[HDB_MAX_TABLE_STRING];
safef(defaultExtra, sizeof(defaultExtra), "%sExtra", tdb->table);
char *extraTable = trackDbSettingOrDefault(tdb, "xrefTable", defaultExtra);
boolean gotExtra = sqlTableExists(conn, extraTable);
if (gotExtra)
    {
    char mgiId[256];
    sqlSafef(query, sizeof(query), "select alias from %s where name = '%s'",
	  extraTable, item);
    sqlQuickQuery(conn, query, mgiId, sizeof(mgiId));
    char *ptr = strchr(mgiId, ',');
    if (!startsWith("MGI:", mgiId) || ptr == NULL)
	errAbort("Where is the MGI ID?: '%s'", mgiId);
    else
	*ptr = '\0';
    // Use the MGI ID to show all centers that are working on this gene:
    sqlSafef(query, sizeof(query), "select name,alias from %s where alias like '%s,%%'",
	  extraTable, mgiId);
    sr = sqlGetResult(conn, query);
    char lastMgiId[16];
    lastMgiId[0] = '\0';
    puts("<TABLE BORDERWIDTH=0 CELLPADDING=0>");
    while ((row = sqlNextRow(sr)) != NULL)
	{
	char *words[3];
	int wordCount = chopCommas(row[1], words);
	if (wordCount >= 3)
	    {
	    char *mgiId = words[0], *center = words[1], *status = words[2];
	    if (!sameString(mgiId, lastMgiId))
		{
		printf("<TR><TD colspan=2>");
		printCustomUrl(tdb, mgiId, FALSE);
		printf("</TD></TR>\n<TR><TD colspan=2>");
		printOtherCustomUrl(tdb, mgiId, "mgiUrl", FALSE);
		printf("</TD></TR>\n");
		safecpy(lastMgiId, sizeof(lastMgiId), mgiId);
		}
	    printf("<TR><TD><B>Center: </B>%s</TD>\n", center);
	    ptr = strrchr(row[0], '_');
	    if (ptr != NULL)
		printf("<TD><B>Design ID: </B>%s</TD>\n", ptr+1);
	    printf("<TD><B>Status: </B>%s", status);
	    if ((ptr != NULL) && (strstr(status, "vailable") != NULL))
		{
		char *productStr;
		char *chp;
		productStr = strdup(status);
		chp = strstr(productStr, "vailable");
		chp--;
		chp--;
		*chp = '\0';
		printf(" (<A HREF=\"http://www.knockoutmouse.org/search_results?criteria=%s\" target=_blank>",
		       ++ptr);
		printf("order %s)", productStr);fflush(stdout);
		}
	    printf("</TD></TR>\n");
	    }
	}
    puts("<TR><TD colspan=2>");
    sqlFreeResult(&sr);
    }
sqlSafef(query, sizeof(query), "select chrom,chromStart,chromEnd from %s "
      "where name = '%s'", tdb->table, item);
sr = sqlGetResult(conn, query);
char lastChr[32];
int lastStart = -1;
int lastEnd = -1;
lastChr[0] = '\0';
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *chr = row[0];
    int start = atoi(row[1]), end = atoi(row[2]);
    if (!sameString(chr, lastChr) || start != lastStart || end != lastEnd)
	printPos(chr, start, end, NULL, TRUE, item);
    safecpy(lastChr, sizeof(lastChr), chr);
    lastStart = start;
    lastEnd = end;
    }
sqlFreeResult(&sr);
if (gotExtra)
    puts("</TD></TR></TABLE>");
printTrackHtml(tdb);
hFreeConn(&conn);
}

void doHgIkmc(struct trackDb *tdb, char *item)
/* Human Genome Map of KnockOut Mouse Project */
{
struct sqlConnection *conn = hAllocConn(database);
char query[512];
struct sqlResult *sr;
char **row;
genericHeader(tdb, item);
char defaultExtra[HDB_MAX_TABLE_STRING];
safef(defaultExtra, sizeof(defaultExtra), "%sExtra", tdb->table);
char *extraTable = trackDbSettingOrDefault(tdb, "xrefTable", defaultExtra);
boolean gotExtra = sqlTableExists(conn, extraTable);
if (gotExtra)
    {
    char mgiId[256];
    char *designId;

    sqlSafef(query, sizeof(query), "select alias from %s where name = '%s'",
	  extraTable, item);
    sqlQuickQuery(conn, query, mgiId, sizeof(mgiId));
    char *ptr = strchr(mgiId, ',');
    if (!startsWith("MGI:", mgiId) || ptr == NULL)
	errAbort("Where is the MGI ID?: '%s'", mgiId);
    else
	*ptr = '\0';
    ptr++;
    designId = ptr;
    ptr = strchr(ptr, ',');
    *ptr = '\0';

    // Show entries with the MGI ID and design ID
    sqlSafef(query, sizeof(query), "select name,alias from %s where alias like '%s,%s%%'",
          extraTable, mgiId, designId);
    sr = sqlGetResult(conn, query);
    char lastMgiId[16];
    lastMgiId[0] = '\0';
    puts("<TABLE BORDERWIDTH=0 CELLPADDING=0>");
    while ((row = sqlNextRow(sr)) != NULL)
	{
	char *words[4];
	int wordCount = chopCommas(row[1], words);
	if (wordCount >= 3)
	    {
	    char *mgiId = words[0], *center = words[2], *status = words[3];
	    if (!sameString(mgiId, lastMgiId))
		{
		printf("<TR><TD colspan=2>");
		printCustomUrl(tdb, mgiId, FALSE);
		printf("</TD></TR>\n<TR><TD colspan=2>");
		printOtherCustomUrl(tdb, mgiId, "mgiUrl", FALSE);
		printf("</TD></TR>\n");
		safecpy(lastMgiId, sizeof(lastMgiId), mgiId);
		}
	    printf("<TR><TD><B>Center: </B>%s</TD>\n", center);
	    ptr = strrchr(row[0], '_');
	    if (ptr != NULL)
		printf("<TD><B>Design ID: </B>%s</TD>\n", ptr+1);
	    printf("<TD><B>Status: </B>%s", status);
	    if ((ptr != NULL) && (strstr(status, "vailable") != NULL))
		{
		char *productStr;
		char *chp;
		productStr = strdup(status);
		chp = strstr(productStr, "vailable");
		chp--;
		chp--;
		*chp = '\0';
		printf(" (<A HREF=\"http://www.komp.org/geneinfo.php?project=%s\" target=_blank>",
		       ++ptr);
		printf("order %s)", productStr);fflush(stdout);
		}
	    printf("</TD></TR>\n");
	    }
	}
    puts("<TR><TD colspan=2>");
    sqlFreeResult(&sr);
    }
sqlSafef(query, sizeof(query), "select chrom,chromStart,chromEnd from %s "
      "where name = '%s'", tdb->table, item);
sr = sqlGetResult(conn, query);
char lastChr[32];
int lastStart = -1;
int lastEnd = -1;
lastChr[0] = '\0';
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *chr = row[0];
    int start = atoi(row[1]), end = atoi(row[2]);
    if (!sameString(chr, lastChr) || start != lastStart || end != lastEnd)
	printPos(chr, start, end, NULL, TRUE, item);
    safecpy(lastChr, sizeof(lastChr), chr);
    lastStart = start;
    lastEnd = end;
    }
sqlFreeResult(&sr);
if (gotExtra)
    puts("</TD></TR></TABLE>");
printTrackHtml(tdb);
hFreeConn(&conn);
}

void doUCSFDemo(struct trackDb *tdb, char *item)
{
genericHeader(tdb, item);

printf("<B>Name:</B> %s<BR>\n", item);

/* this prints the detail page for the clinical information for Cancer Demo datasets */
char *table = tdb->table;
char *cliniTable=NULL, *key=NULL;
char query[256];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;

if (sameString(table, "CGHBreastCancerUCSF") || sameString(table, "expBreastCancerUCSF"))
    {
    cliniTable = "phenBreastTumors";
    key = "id";

    /* er, pr */
    printf("<BR>");
    printf("<TABLE BORDER=1>\n");
    printf("<TR><TH>ER</TH> <TH>PR</TH></TR>\n");
    sqlSafef(query, sizeof(query), "select er, pr from %s where %s = '%s' ", cliniTable, key, item);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
        printf("<TR>");
        printf("<TD>%s</TD>", row[0]);
        printf("<TD>%s</TD>", row[1]);
        printf("</TR>");
	}
    printf("</TABLE>\n");
    sqlFreeResult(&sr);

    /* subEuc, subCor */
    printf("<BR>");
    printf("<TABLE BORDER=1>\n");
    printf("<TR><TH>subEuc</TH> <TH>subCor</TH></TR>\n");
    sqlSafef(query, sizeof(query), "select subEuc, subCor from %s where %s = '%s' ", cliniTable, key, item);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
        printf("<TR>");
        printf("<TD>%s</TD>", row[0]);
        printf("<TD>%s</TD>", row[1]);
        printf("</TR>");
	}
    printf("</TABLE>\n");
    sqlFreeResult(&sr);

    /* subtypes */
    printf("<BR>");
    printf("<TABLE BORDER=1>\n");
    printf("<TR><TH>subtype2</TH> <TH>subtype3</TH> <TH>subtype4</TH> <TH>subtype5</TH></TR>\n");
    sqlSafef(query, sizeof(query),
          "select subtype2, subtype3, subtype4, subtype5 from %s where %s = '%s' ",
          cliniTable, key, item);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
        printf("<TR>");
        printf("<TD>%s</TD>", row[0]);
        printf("<TD>%s</TD>", row[1]);
        printf("<TD>%s</TD>", row[2]);
        printf("<TD>%s</TD>", row[3]);
        printf("</TR>");
	}
    printf("</TABLE>\n");
    sqlFreeResult(&sr);

    /* stage, size, nodalStatus, SBRGrade */
    printf("<BR>");
    printf("<TABLE BORDER=1>\n");
    printf("<TR><TH>Stage</TH> <TH>Size</TH> <TH>Nodal status</TH> <TH>SBR Grade</TH></TR>\n");
    sqlSafef(query, sizeof(query),
        "select stage, size, nodalStatus, SBRGrade from %s where %s = '%s' ", cliniTable, key, item);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
        printf("<TR>");
        printf("<TD>%s</TD>", row[0]);
        printf("<TD>%s</TD>", row[1]);
        printf("<TD>%s</TD>", row[2]);
        printf("<TD>%s</TD>", row[3]);
        printf("</TR>");
	}
    printf("</TABLE>\n");
    sqlFreeResult(&sr);

    /* race, familyHistory, ageDx */
    printf("<BR>");
    printf("<TABLE BORDER=1>\n");
    printf("<TR><TH>Race</TH> <TH>Family history</TH> <TH>Age of Diagnosis</TH> </TR>\n");
    sqlSafef(query, sizeof(query),
        "select race, familyHistory, ageDx from %s where %s = '%s' ", cliniTable, key, item);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
        printf("<TR>");
        printf("<TD>%s</TD>", row[0]);
        printf("<TD>%s</TD>", row[1]);
        printf("<TD>%s</TD>", row[2]);
        printf("</TR>");
	}
    printf("</TABLE>\n");
    sqlFreeResult(&sr);


    /* rad, chemo, horm, erb, p53, ki67 */
    printf("<BR>");
    printf("<TABLE BORDER=1>\n");
    printf("<TR><TH>Rad</TH> <TH>Chemo</TH> <TH>Horm</TH> <TH>ERB</TH> <TH>p53</TH>");
    printf("<TH>ki67</TH></TR>\n");
    sqlSafef(query, sizeof(query),
        "select rad, chemo, horm, erb, p53, ki67 from %s where %s = '%s' ", cliniTable, key, item);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
        printf("<TR>");
        printf("<TD>%s</TD>", row[0]);
        printf("<TD>%s</TD>", row[1]);
        printf("<TD>%s</TD>", row[2]);
        printf("<TD>%s</TD>", row[3]);
        printf("<TD>%s</TD>", row[4]);
        printf("<TD>%s</TD>", row[5]);
        printf("</TR>");
	}
    printf("</TABLE>\n");
    sqlFreeResult(&sr);

    /* T/N/M */
    printf("<BR>");
    printf("<TABLE BORDER=1>\n");
    printf("<TR><TH>T</TH> <TH>N</TH> <TH>M</TH></TR>\n");
    sqlSafef(query, sizeof(query), "select T, N, M from %s where %s = '%s' ", cliniTable, key, item);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
        printf("<TR>");
        printf("<TD>%s</TD>", row[0]);
        printf("<TD>%s</TD>", row[1]);
        printf("<TD>%s</TD>", row[2]);
        printf("</TR>");
	}
    printf("</TABLE>\n");
    sqlFreeResult(&sr);

    /* times */
    printf("<BR><B>Times:</B><BR>\n");
    printf("<TABLE BORDER=1>\n");
    printf("<TR><TH>Type</TH> <TH>Binary</TH> <TH>Value</TH></TR>\n");
    sqlSafef(query, sizeof(query),
          "select overallBinary, overallTime, diseaseBinary, diseaseTime, "
          "allrecBinary, allrecTime, distrecBinary, distrecTime from %s where %s = '%s' ",
          cliniTable, key, item);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
        printf("<TR><TD>Overall</TD> <TD>%s</TD> <TD>%s</TD></TR>", row[0], row[1]);
        printf("<TR><TD>Disease</TD> <TD>%s</TD> <TD>%s</TD></TR>", row[2], row[3]);
        printf("<TR><TD>Allrec</TD> <TD>%s</TD> <TD>%s</TD></TR>", row[4], row[5]);
        printf("<TR><TD>Distrec</TD> <TD>%s</TD> <TD>%s</TD></TR>", row[6], row[7]);
	}
    printf("</TABLE>\n");
    sqlFreeResult(&sr);

    /* affyChipId */
    printf("<BR>");
    sqlSafef(query, sizeof(query), "select affyChipId from %s where %s = '%s' ", cliniTable, key, item);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
        printf("<B>Affy Chip ID:</B> %s\n", row[0]);
	}
    printf("</TABLE>\n");
    sqlFreeResult(&sr);

    return;
    }
else if ( sameString(table, "cnvLungBroadv2"))
    {
    cliniTable = "tspLungClinical";
    key = "tumorID";
    }
else
    return;

htmlHorizontalLine();

sqlSafef(query, sizeof(query),
      "select * from %s where %s = '%s' ", cliniTable, key,item);

sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    int numFields = sqlCountColumns(sr);
    int i;
    char *fieldName=NULL, *value=NULL;
    for (i=0; i< numFields; i++)
	{
	fieldName = sqlFieldName(sr);
	value = row[i];
	printf("%s: <B>%s</B><BR>\n", fieldName, value);
	}
    }
sqlFreeResult(&sr);
//printTrackHtml(tdb);
//hFreeConn
}

void doConsIndels(struct trackDb *tdb, char *item)
/* Display details about items in the Indel-based Conservation track. */
{
struct dyString *dy = dyStringNew(1024);
struct sqlConnection *conn = hAllocConn(database);
struct itemConf *cf;
char confTable[128];

/* create name for confidence table containing posterior probability and
   false discovery rate (FDR). */
safef(confTable, sizeof(confTable), "%sConf", tdb->table);

if (sqlTableExists(conn, confTable))
    {
    /* print the posterior probability and FDR if available */
    struct sqlResult *sr;
    char query[256], **row;

    sqlSafef(query, sizeof(query),
          "select * from %s where id = '%s'", confTable, item);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        cf = itemConfLoad(row);
        dyStringPrintf(dy, "<B>Posterior Probability:</B> %.4g<BR>\n", cf->probability);
        dyStringPrintf(dy, "<B>False Discovery Rate (FDR):</B> %.2f<BR>\n", cf->fdr);
        itemConfFree(&cf);
        }
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
genericClickHandlerPlus(tdb, item, NULL, dy->string);
dyStringFree(&dy);
}

#define KIDD_EICHLER_DISC_PREFIX "kiddEichlerDisc"

void printKiddEichlerNcbiLinks(struct trackDb *tdb, char *item)
/* If we have a table that maps kiddEichler IDs to NCBI IDs, print links. */
{
char *ncbiAccXref = trackDbSetting(tdb, "ncbiAccXref");
if (isNotEmpty(ncbiAccXref) && hTableExists(database, ncbiAccXref))
    {
    struct sqlConnection *conn = hAllocConn(database);
    struct sqlResult *sr;
    char **row;
    char *cloneName = cloneString(item);
    char *postUnderscore = strchr(cloneName, '_');
    char query[512];
    /* In kiddEichlerDiscG248, all clone names have a WIBR2-\w+_ prefix
     * before the G248\w+ clone name given in the files used to make this
     * table, e.g. WIBR2-1962P18_G248P85919H9,transchrm_chr4 -- skip that
     * prefix.  Then strip all kiddEichlerDisc* names' ,.* suffixes. */
    if (startsWith("WIBR2-", cloneName) && postUnderscore != NULL)
	cloneName = postUnderscore+1;
    chopPrefixAt(cloneName, ',');
    sqlSafef(query, sizeof(query),
	  "select cloneAcc, endF, endR from %s where name = '%s'",
	  ncbiAccXref, cloneName);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	if (isNotEmpty(row[0]))
	    printf("<B>Clone Report and Sequence (NCBI Nucleotide): </B>"
		   "<A HREF=\"%s\" TARGET=_BLANK>%s</A><BR>\n",
		   getEntrezNucleotideUrl(row[0]), row[0]);
	char *endUrlFormat = trackDbSetting(tdb, "pairedEndUrlFormat");
	/* Truncate cloneName to get library name: ABC* are followed by _,
	 * G248 are not. */
	char *libId = cloneName;
	if (startsWith("G248", libId))
	    libId[strlen("G248")] = '\0';
	else if (startsWith("ABC", libId))
	    chopPrefixAt(libId, '_');
	if (endUrlFormat && differentStringNullOk(row[1], "0"))
	    {
	    printf("<B>Forward End Read (NCBI Trace Archive): </B>"
		   "<A HREF=\"");
	    printf(endUrlFormat, libId, row[1]);
	    printf("\" TARGET=_BLANK>%s</A><BR>\n", row[1]);
	    }
	if (endUrlFormat && differentStringNullOk(row[2], "0"))
	    {
	    printf("<B>Reverse End Read (NCBI Trace Archive): </B>"
		   "<A HREF=\"");
	    printf(endUrlFormat, libId, row[2]);
	    printf("\" TARGET=_BLANK>%s</A><BR>\n", row[2]);
	    }
	}
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }
}

void doKiddEichlerDisc(struct trackDb *tdb, char *item)
/* Discordant clone end mappings from Kidd..Eichler 2008. */
{
struct sqlConnection *conn = hAllocConn(database);
char query[512];
struct sqlResult *sr;
char **row;
boolean hasBin;
struct bed *bed;
boolean firstTime = TRUE;
int start = cartInt(cart, "o");

genericHeader(tdb, item);
if (! startsWith(KIDD_EICHLER_DISC_PREFIX, tdb->table))
    errAbort("track tableName must begin with "KIDD_EICHLER_DISC_PREFIX
	     " but instead it is %s", tdb->table);
hasBin = hOffsetPastBin(database, seqName, tdb->table);
/* We don't need to add bin to this because name is indexed: */
sqlSafef(query, sizeof(query), "select * from %s where name = '%s' "
	       "and chrom = '%s' and chromStart = %d",
	       tdb->table, item, seqName, start);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (firstTime)
	firstTime = FALSE;
    else
	htmlHorizontalLine();
    bed = bedLoadN(row+hasBin, 12);
    int lastBlk = bed->blockCount - 1;
    int endForUrl = (bed->chromStart + bed->chromStarts[lastBlk] +
		     bed->blockSizes[lastBlk]);
    char *endFudge = trackDbSetting(tdb, "endFudge");
    if (endFudge && !strstr(bed->name, "OEA"))
	endForUrl += atoi(endFudge);
    char sampleName[16];
    safecpy(sampleName, sizeof(sampleName),
	    tdb->table + strlen(KIDD_EICHLER_DISC_PREFIX));
    touppers(sampleName);
    char itemPlus[2048];
    safef(itemPlus, sizeof(itemPlus),
	  "%s&o=%d&t=%d&g=%s_discordant&%s_discordant=full",
	  cgiEncode(item), start, endForUrl, sampleName, sampleName);
    printCustomUrlWithLabel(tdb, itemPlus, item, "url", FALSE, NULL);
    printKiddEichlerNcbiLinks(tdb, bed->name);
    printf("<B>Score:</B> %d<BR>\n", bed->score);
    printPosOnChrom(bed->chrom, bed->chromStart, bed->chromEnd, bed->strand,
		    TRUE, bed->name);
    }
sqlFreeResult(&sr);
printTrackHtml(tdb);
}

void doBedDetail(struct trackDb *tdb, struct customTrack *ct, char *itemName)
/* generate the detail page for a custom track of bedDetail type */
{
char *table;
struct bedDetail *r = NULL;
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
char query[256];
char *chrom = cartString(cart,"c");  /* don't assume name is unique */
int start = cgiInt("o");
int end = cgiInt("t");
int bedPart = 4;
if (ct == NULL)
    {
    char *words[3];
    int cnt = chopLine(cloneString(tdb->type), words);
    if (cnt > 1)
        bedPart = atoi(words[1]) - 2;
    table = tdb->table;
    conn = hAllocConn(database);
    genericHeader(tdb, itemName);
    }
else
    {
    table = ct->dbTableName;
    conn = hAllocConn(CUSTOM_TRASH);
    bedPart = ct->fieldCount - 2;
    /* header handled by custom track handler */
    }

/* postion, band, genomic size */
sqlSafef(query, sizeof(query),
      "select * from %s where chrom = '%s' and chromStart = %d and chromEnd = %d and name = '%s'", 
	table, chrom, start, end, itemName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    r = bedDetailLoadWithGaps(row, bedPart+2);
    if (isNotEmpty(r->id))
        {
        printCustomUrl(tdb, r->id, TRUE);
        printf("<br>\n");
        }

    bedPrintPos((struct bed*)r, bedPart, tdb);
    printf("<br>");
    if (isNotEmpty(r->id) && !sameString("qPcrPrimers", table))
        printf("<B>ID:</B> %s <BR>\n", r->id);

    if  (isNotEmpty(r->description))
        printf("%s <BR>\n", r->description);
    }
sqlFreeResult(&sr);
/* do not print this for custom tracks, they do this later */
if (ct == NULL)
    printTrackHtml(tdb);

bedDetailFree(&r);
hFreeConn(&conn);
}

struct trackDb *tdbForTableArg()
/* get trackDb for track passed in table arg */
{
char *table = cartString(cart, "table");
struct trackDb *tdb = hashFindVal(trackHash, table);
if (tdb == NULL)
    errAbort("no trackDb entry for %s", table);
return tdb;
}

void doQPCRPrimers(struct trackDb *tdb, char *itemName)
/* Put up page for QPCRPrimers. */
{
genericHeader(tdb, itemName);
doBedDetail(tdb, NULL, itemName);
} /* end of doQPCRPrimers */

void doSnakeClick(struct trackDb *tdb, char *itemName)
/* Put up page for snakes. */
{
struct trackDb *parentTdb = trackDbTopLevelSelfOrParent(tdb);
char *otherSpecies = trackDbSetting(tdb, "otherSpecies");
if (otherSpecies == NULL)
    otherSpecies = trackHubSkipHubName(tdb->table) + strlen("snake");

char *hubName = cloneString(database);
char otherDb[4096];
char *qName = cartOptionalString(cart, "qName");
int qs = atoi(cartOptionalString(cart, "qs"));
int qe = atoi(cartOptionalString(cart, "qe"));
int qWidth = atoi(cartOptionalString(cart, "qWidth"));
char *qTrack = cartString(cart, "g");
if(isHubTrack(qTrack) && ! trackHubDatabase(database))
    hubName = cloneString(qTrack);

/* current mouse strain hal file has incorrect chrom names */
char *aliasQName = qName;
// aliasQName = "chr1";  // temporarily make this work for the mouse hal

if(trackHubDatabase(database) || isHubTrack(qTrack))
    {
    char *ptr = strchr(hubName + 4, '_');
    *ptr = 0;
    safef(otherDb, sizeof otherDb, "%s_%s", hubName, otherSpecies);
    }
else
    {
    safef(otherDb, sizeof otherDb, "%s", otherSpecies);
    }

char headerText[256];
safef(headerText, sizeof headerText, "reference: %s, query: %s\n", trackHubSkipHubName(database), trackHubSkipHubName(otherDb) );
genericHeader(parentTdb, headerText);

printf("<A HREF=\"hgTracks?db=%s&position=%s:%d-%d&%s_snake%s=full\" TARGET=_BLANK>%s:%d-%d</A> link to block in query assembly: <B>%s</B></A><BR>\n", otherDb, aliasQName, qs, qe, hubName, trackHubSkipHubName(database), aliasQName, qs, qe, trackHubSkipHubName(otherDb));

int qCenter = (qs + qe) / 2;
int newQs = qCenter - qWidth/2;
int newQe = qCenter + qWidth/2;
printf("<A HREF=\"hgTracks?db=%s&position=%s:%d-%d&%s_snake%s=full\" TARGET=\"_blank\">%s:%d-%d</A> link to same window size in query assembly: <B>%s</B></A><BR>\n", otherDb, aliasQName, newQs, newQe,hubName, trackHubSkipHubName(database), aliasQName, newQs, newQe, trackHubSkipHubName(otherDb) );
printTrackHtml(tdb);
} 

bool vsameWords(char *a, va_list args)
/* returns true if a is sameWord as any arg, all args must be char*  */
{
bool found = FALSE;
char *b;
while ((b = va_arg(args, char *)) != NULL)
    {
    if (sameWord(a, b))
        {
        found = TRUE;
        break;
        }
    }
return found;
}

bool sameAltWords(char *a, char *b, ...)
/* returns true if a is sameWord as any of the variables or b is sameWord as any of them */
{
va_list args;
va_start(args, b);
bool res = vsameWords(a, args);
va_end(args);

if (!res && (b != NULL))
    {
    va_start(args, b);
    res = vsameWords(b, args);
    va_end(args);
    }
return res;
}

bool sameWords(char *a, ...)
/* returns true if a is equal to any b */
{
va_list args;
va_start(args, a);
bool res = vsameWords(a, args);
va_end(args);
return res;
}

void printAddWbr(char *text, int distance) 
/* a crazy hack for firefox/mozilla that is unable to break long words in tables
 * We need to add a <wbr> tag every x characters in the text to make text breakable.
 */
{
int i;
i = 0;
char *c;
c = text;
bool doNotBreak = FALSE;
while (*c != 0) 
    {
    if ((*c=='&') || (*c=='<'))
       doNotBreak = TRUE;
    if (*c==';' || (*c =='>'))
       doNotBreak = FALSE;

    printf("%c", *c);
    if (i % distance == 0 && ! doNotBreak) 
        printf("<wbr>");
    c++;
    i++;
    }
}

static char *replaceSuffix(char *input, char *newSuffix)
/* Given a filename with a suffix, replace existing suffix with a new suffix. */
{
char buffer[4096];
safecpy(buffer, sizeof buffer, input);
char *dot = strrchr(buffer, '.');
safecpy(dot+1, sizeof buffer - 1 - (dot - buffer), newSuffix);
return cloneString(buffer);
}

static void makeBigPsl(char *pslName, char *faName, char *db, char *outputBigBed)
/* Make a bigPsl with the blat results. */
{
char *bigPslFile = replaceSuffix(outputBigBed, "bigPsl");

char faNameBuffer[strlen("-fa=") + strlen(faName) + 1];
safef(faNameBuffer, sizeof faNameBuffer, "-fa=%s", faName);
char *cmd11[] = {"loader/pslToBigPsl", pslName,  faNameBuffer, "stdout", NULL};
char *cmd12[] = {"sort","-k1,1","-k2,2n", NULL};
char **cmds1[] = { cmd11, cmd12, NULL};
struct pipeline *pl = pipelineOpen(cmds1, pipelineWrite, bigPslFile, NULL, 0);
pipelineWait(pl);

char buf[4096];
char *twoBitDir;
if (trackHubDatabase(db))
    {
    struct trackHubGenome *genome = trackHubGetGenome(db);
    twoBitDir = genome->twoBitPath;
    }
else
    {
    safef(buf, sizeof(buf), "/gbdb/%s", db);
    twoBitDir = hReplaceGbdbSeqDir(buf, db);
    safef(buf, sizeof(buf), "%s%s.2bit", twoBitDir, db);
    twoBitDir = buf;
    }

char udcDir[strlen(udcDefaultDir()) + strlen("-udcDir=") + 1];
safef(udcDir, sizeof udcDir, "-udcDir=%s", udcDefaultDir());
char *cmd2[] = {"loader/bedToBigBed","-verbose=0",udcDir,"-extraIndex=name","-sizesIs2Bit", "-tab", "-as=loader/bigPsl.as","-type=bed12+13", bigPslFile, twoBitDir, outputBigBed, NULL};
pl = pipelineOpen1(cmd2, pipelineRead, NULL, NULL, 0);
pipelineWait(pl);

unlink(bigPslFile);
}

static void buildBigPsl(char *fileNames)
/* Build a custom track with a bigPsl file out of blat results.
 * Bring up the bigPsl detail page with all the alignments. */
{
char *trackName = cartString(cart, "trackName");
char *trackDescription = cartString(cart, "trackDescription");
char *pslName, *faName, *qName;
parseSs(fileNames, &pslName, &faName, &qName);

struct tempName bigBedTn;
trashDirDateFile(&bigBedTn, "hgBlat", "bp", ".bb");
char *bigBedFile = bigBedTn.forCgi;
makeBigPsl(pslName, faName, database, bigBedFile);

char* host = getenv("HTTP_HOST");

boolean isProt = cgiOptionalString("isProt") != NULL;
char *customTextTemplate = "track type=bigPsl indelDoubleInsert=on indelQueryInsert=on pslFile=%s visibility=pack showAll=on htmlUrl=http://%s/goldenPath/help/hgUserPsl.html %s bigDataUrl=%s name=\"%s\" description=\"%s\" colorByStrand=\"0,0,0 0,0,150\" mouseOver=\"${oChromStart}-${oChromEnd} of ${oChromSize} bp, strand ${oStrand}\"\n";  
char *extraForMismatch = "indelPolyA=on showDiffBasesAllScales=. baseColorUseSequence=lfExtra baseColorDefault=diffBases";
  
if (isProt)
    extraForMismatch = "";
char buffer[4096];
safef(buffer, sizeof buffer, customTextTemplate, bigBedTn.forCgi, host, extraForMismatch, bigBedTn.forCgi, trackName, trackDescription);

struct customTrack *ctList = getCtList();
struct customTrack *newCts = customFactoryParse(database, buffer, FALSE, NULL, NULL);
theCtList = customTrackAddToList(ctList, newCts, NULL, FALSE);

customTracksSaveCart(database, cart, theCtList);

cartSetString(cart, "i", "PrintAllSequences");
hgCustom(newCts->tdb->track, NULL);
}

void doHPRCTable(struct trackDb *tdb, char *itemName)
/* Put up a generic bigBed details page, with a table of links to turn on related
 *  * chain tracks with visibility toggles */
{
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
genericHeader(tdb, itemName);
genericBigBedClick(NULL, tdb, itemName, start, end, 0);
printTrackHtml(tdb);
// tell the javscript to reorganize the column of assemblies:
jsIncludeFile("hgc.js", NULL);
jsInlineF("var doHPRCTable = true;\n");
}

boolean findNameBasedHandler(struct trackDb *tdb, char *track, char *item);

void doMiddle()
/* Generate body of HTML. */
{
char *track = cartString(cart, "g");
char *item = cloneString(cartOptionalString(cart, "i"));
char *parentWigMaf = cartOptionalString(cart, "parentWigMaf");
struct trackDb *tdb = NULL;

char *dupWholeName = NULL;
boolean isDup = isDupTrack(track);
if (isDup)
    {
    dupWholeName = track;
    track = dupTrackSkipToSourceName(track);
    }

if (issueBotWarning)
    {
    char *ip = getenv("REMOTE_ADDR");
    botDelayMessage(ip, botDelayMillis);
    }

/*	database and organism are global variables used in many places	*/
getDbAndGenome(cart, &database, &genome, NULL);
chromAliasSetup(database);
organism = hOrganism(database);
scientificName = hScientificName(database);

initGenbankTableNames(database);

dbIsFound = trackHubDatabase(database) || sqlDatabaseExists(database);

// Try to deal with virt chrom position used by hgTracks.
// Hack the cart vars to set to a non virtual chrom mode position
if (sameString("virt", cartString(cart, "c"))
 || sameString("getDna", cartUsualString(cart, "g", "")) )
    {
    char *nvPos = cartUsualString(cart, "nonVirtPosition", NULL);
    if (nvPos)
	{
	// parse non-virtual position 
	char *pos = cloneString(nvPos);
	char *colon = strchr(pos, ':');
	if (!colon)
	errAbort("position has no colon");
	char *dash = strchr(pos, '-');
	if (!dash)
	errAbort("position has no dash");
	*colon = 0;
	*dash = 0;
	char *chromName = cloneString(pos);
	int winStart = atol(colon+1) - 1;
	int winEnd = atol(dash+1);
	cartSetString(cart, "position", nvPos);
	cartSetString(cart, "c", chromName);
	cartSetInt(cart, "l", winStart);
	cartSetInt(cart, "r", winEnd);
	}
    }


if (dbIsFound)
    seqName = hgOfficialChromName(database, cartString(cart, "c"));
else
    seqName = cartString(cart, "c");

winStart = cartUsualInt(cart, "l", 0);
winEnd = cartUsualInt(cart, "r", 0);

/* Allow faked-out c=0 l=0 r=0 (e.g. for unaligned mRNAs) but not just any
 * old bogus position: */
if (seqName == NULL)
    {
    if (winStart != 0 || winEnd != 0)
	webAbortNoHttpHeader("CGI variable error", 
		 "hgc: bad input variables c=%s l=%d r=%d",
		 cartString(cart, "c"), winStart, winEnd);
    else
	seqName = hDefaultChrom(database);
    }

struct customTrack *ct = NULL;
if (isCustomTrack(track))
    {
    struct customTrack *ctList = getCtList();
    for (ct = ctList; ct != NULL; ct = ct->next)
        if (sameString(track, ct->tdb->track))
            break;
    }

if ((!isCustomTrack(track) && dbIsFound)
||  ((ct!= NULL) && (((ct->dbTrackType != NULL) &&  sameString(ct->dbTrackType, "maf"))|| sameString(ct->tdb->type, "bigMaf"))))
    {
    trackHash = makeTrackHashWithComposites(database, seqName, TRUE);
    if (sameString("htcBigPslAli", track) || sameString("htcBigPslAliInWindow", track) )
	{
	char *aliTable = cartString(cart, "aliTable");
	if (isHubTrack(aliTable))	
	    tdb = hubConnectAddHubForTrackAndFindTdb( database, aliTable, NULL, trackHash);
	}
    else if (isHubTrack(track))
	{
	tdb = hubConnectAddHubForTrackAndFindTdb( database, track, NULL, trackHash);
	}
    if (parentWigMaf)
        {
        int wordCount, i;
        char *words[16];
        char *typeLine;
        char *wigType = needMem(128);
        tdb = hashFindVal(trackHash, parentWigMaf);
        if (!tdb)
            errAbort("can not find trackDb entry for parentWigMaf track %s.",
                    parentWigMaf);
        typeLine = cloneString(tdb->type);
        wordCount = chopLine(typeLine, words);
        if (wordCount < 1)
            errAbort("trackDb entry for parentWigMaf track %s has corrupt type line.",
                    parentWigMaf);
        safef(wigType, 128, "wig ");
        for (i = 1; i < wordCount; ++i)
            {
            strncat(wigType, words[i], 128 - strlen(wigType));
            strncat(wigType, " ", 128 - strlen(wigType));
            }
        strncat(wigType, "\n", 128 - strlen(wigType));
        tdb->type = wigType;
        tdb->track = cloneString(track);
        tdb->table = cloneString(track);
        freeMem(typeLine);
        cartRemove(cart, "parentWigMaf");	/* ONE TIME ONLY USE !!!	*/
        }
    else
	{
        tdb = hashFindVal(trackHash, track);
	if (tdb == NULL)
	    {
            if (startsWith("all_mrna", track))
		tdb = hashFindVal(trackHash, "mrna");
                  /* Oh what a tangled web we weave. */
	    }
	}
    }

if (isDup)
    {
    struct dupTrack *dupList = dupTrackListFromCart(cart);
    struct dupTrack *dup = dupTrackFindInList(dupList, dupWholeName);
    if (dup != NULL)
	{
	tdb = dupTdbFrom(tdb, dup);
	track = dupWholeName;
	}
    }

// do we want to avoid named based handling on this track?
boolean avoidHandler = trackDbSettingOn(tdb, "avoidHandler");
boolean calledHandler = FALSE;

if (!avoidHandler)
    calledHandler = findNameBasedHandler(tdb, track, item);

if (!calledHandler)
    {
    if (tdb != NULL)
        {
        genericClickHandler(tdb, item, NULL);
        }
    else
        {
        cartWebStart(cart, database, "%s", track);
        warn("Sorry, clicking there doesn't do anything yet (%s).", track);
        }
    }

cartHtmlEnd();
}

boolean findNameBasedHandler(struct trackDb *tdb, char *track, char *item)
// call hander routine based on name.  Return TRUE if we called a handler
{
char* handler = trackDbSetting(tdb, "trackHandler");

char *table = (tdb ? tdb->table : track);
if (sameWord(table, "getDna"))
    {
    htmlDoNotTranslate();
    doGetDna1();
    }
else if (sameWord(table, "htcGetDna2"))
    {
    htmlDoNotTranslate();
    doGetDna2();
    }
else if (sameWord(table, "htcGetDna3"))
    {
    htmlDoNotTranslate();
    doGetDna3();
    }
else if (sameWord(table, "htcGetDnaExtended1"))
    {
    htmlDoNotTranslate();
    doGetDnaExtended1();
    }
else if (sameWord(table, "buildBigPsl"))
    {
    buildBigPsl(item);
    }
else if (sameWord(table, "htcListItemsAssayed"))
    {
    doPeakClusterListItemsAssayed();
    }

/* Lowe Lab Stuff */
#ifdef LOWELAB
 else if (loweLabClick(table, item, tdb))
   {
     /* do nothing, everything handled in loweLabClick */
   }
#endif
else if (sameWord(table, G_DELETE_WIKI_ITEM))
    {
    doDeleteWikiItem(item, seqName, winStart, winEnd);
    }
else if (sameWord(table, G_ADD_WIKI_COMMENTS))
    {
    doAddWikiComments(item, seqName, winStart, winEnd);
    }
else if (sameWord(table, G_CREATE_WIKI_ITEM))
    {
    doCreateWikiItem(item, seqName, winStart, winEnd);
    }
else if (sameString(track, "variome"))
    doVariome(item, seqName, winStart, winEnd);
else if (sameString(track, "variome.create"))
    doCreateVariomeItem(item, seqName, winStart, winEnd);
else if (sameString(track, "variome.delete"))
    doDeleteVariomeItem(item, seqName, winStart, winEnd);
else if (sameString(track, "variome.addComments"))
    doAddVariomeComments(item, seqName, winStart, winEnd);
else if (startsWith("transMap", table))
    transMapClickHandler(tdb, item);
else if (startsWith("hgcTransMapCdnaAli", table))
    {
    char *aliTable = cartString(cart, "aliTable");
    char *track = hGetTrackForTable(database, aliTable);
    tdb = hashMustFindVal(trackHash, track);
    transMapShowCdnaAli(tdb, item);
    }
else if (sameWord(table, "mrna") || sameWord(table, "mrna2") ||
	 startsWith("all_mrna",table) ||
	 sameWord(table, "all_est") ||
	 sameWord(table, "celeraMrna") ||
         sameWord(table, "est") || sameWord(table, "intronEst") ||
         sameWord(table, "xenoMrna") || sameWord(table, "xenoBestMrna") ||
         startsWith("mrnaBlastz",table ) || startsWith("mrnaBad",table ) ||
         sameWord(table, "xenoBlastzMrna") || sameWord(table, "sim4") ||
         sameWord(table, "xenoEst") || sameWord(table, "psu") ||
         sameWord(table, "tightMrna") || sameWord(table, "tightEst") ||
	 sameWord(table, "blatzHg17KG") || sameWord(table, "mapHg17KG")
         )
    {
    doHgRna(tdb, item);
    }
else if (startsWith("HLTOGAannot", trackHubSkipHubName(table)))
    {
    doHillerLabTOGAGene(database, tdb, item, table);
    }
else if (startsWith("pseudoMrna",table ) || startsWith("pseudoGeneLink",table )
        || sameWord("pseudoUcsc",table))
    {
    doPseudoPsl(tdb, item);
    }
else if (startsWith("retroMrna",table ) || startsWith("retroAugust",table )|| startsWith("retroCds",table )|| startsWith("ucscRetro",table ))
    {
    retroClickHandler(tdb, item);
    }
else if (sameString(table, "hgcRetroCdnaAli"))
    retroShowCdnaAli(item);
else if (sameWord(table, "affyU95")
	|| sameWord(table, "affyU133")
	|| sameWord(table, "affyU74")
	|| sameWord(table, "affyRAE230")
	|| sameWord(table, "affyZebrafish")
	|| sameWord(table, "affyGnf1h")
	|| sameWord(table, "affyMOE430v2")
	|| sameWord(table, "affyGnf1m") )
    {
    doAffy(tdb, item, NULL);
    }
else if (sameWord(table, WIKI_TRACK_TABLE))
    doWikiTrack(item, seqName, winStart, winEnd);
else if (sameWord(table, OLIGO_MATCH_TRACK_NAME))
    doOligoMatch(item);
else if (sameWord(table, "refFullAli"))
    {
    doTSS(tdb, item);
    }
else if (sameWord(table, "rikenMrna"))
    {
    doRikenRna(tdb, item);
    }
else if (sameWord(table, "cgapSage"))
    {
    doCgapSage(tdb, item);
    }
else if (sameWord(table, "ctgPos") || sameWord(table, "ctgPos2"))
    {
    doHgContig(tdb, item);
    }
else if (sameWord(table, "clonePos"))
    {
    doHgCover(tdb, item);
    }
else if (sameWord(table, "bactigPos"))
    {
    doBactigPos(tdb, item);
    }
else if (sameWord(table, "hgClone"))
    {
    tdb = hashFindVal(trackHash, "clonePos");
    doHgClone(tdb, item);
    }
else if (sameWord(table, "gold"))
    {
    doHgGold(tdb, item);
    }
else if (sameWord(table, "gap"))
    {
    doHgGap(tdb, item);
    }
else if (sameWord(table, "tet_waba"))
    {
    doHgTet(tdb, item);
    }
else if (sameWord(table, "wabaCbr"))
    {
    doHgCbr(tdb, item);
    }
else if (startsWith("rmskJoined", table))
    {
    doJRepeat(tdb, item);
    }
else if (tdb != NULL && startsWith("bigRmsk", tdb->type))
    {
    doBigRmskRepeat(tdb, item);
    }
else if (startsWith("rmsk", table))
    {
    doHgRepeat(tdb, item);
    }
else if (sameWord(table, "isochores"))
    {
    doHgIsochore(tdb, item);
    }
else if (sameWord(table, "simpleRepeat"))
    {
    doSimpleRepeat(tdb, item);
    }
else if (startsWith("cpgIsland", table))
    {
    doCpgIsland(tdb, item);
    }
else if (sameWord(table, "illuminaProbes"))
    {
    doIlluminaProbes(tdb, item);
    }
else if (sameWord(table, "htcIlluminaProbesAlign"))
    {
    htcIlluminaProbesAlign(item);
    }
else if (sameWord(table, "switchDbTss"))
    {
    doSwitchDbTss(tdb, item);
    }
else if (sameWord(table, "omimLocation"))
    {
    doOmimLocation(tdb, item);
    }
else if (sameWord(table, "omimAvSnp"))
    {
    doOmimAvSnp(tdb, item);
    }
else if (sameWord(table, "omimGeneClass2"))
    {
    doOmimGene2(tdb, item);
    }
else if (sameAltWords(table, handler, "omimGene2", "omimGene2bb", NULL))
    {
    doOmimGene2(tdb, item);
    }
else if (sameWord(table, "omimAv"))
    {
    doOmimAv(tdb, item);
    }
else if (sameWord(table, "rgdGene"))
    {
    doRgdGene(tdb, item);
    }
else if (sameWord(table, "rgdGene2"))
    {
    doRgdGene2(tdb, item);
    }
else if (sameWord(table, "rgdEst"))
    {
    doHgRna(tdb, item);
    }
else if (sameWord(table, "rgdSslp"))
    {
    doRgdSslp(tdb, item, NULL);
    }
else if (sameWord(table, "gad"))
    {
    doGad(tdb, item, NULL);
    }
else if (sameWord(table, "decipherOld"))
    {
    doDecipherCnvs(tdb, item, NULL);
    }
else if (sameWord(table, trackHubSkipHubName("decipherSnvs")))
    {
    doDecipherSnvs(tdb, item, NULL);
    }
else if (sameWord(table, "omimGene"))
    {
    doOmimGene(tdb, item);
    }
else if (sameWord(table, "rgdQtl") || sameWord(table, "rgdRatQtl"))
    {
    doRgdQtl(tdb, item);
    }
else if (sameWord(table, "superfamily"))
    {
    doSuperfamily(tdb, item, NULL);
    }
else if (sameWord(table, "ensGene") || sameWord (table, "ensGeneNonCoding"))
    {
    doEnsemblGene(tdb, item, NULL);
    }
else if (sameWord(table, "xenoRefGene"))
    {
    doRefGene(tdb, item);
    }
else if (sameWord(table, "knownGene"))
    {
    doKnownGene(tdb, item);
    }
else if (matchTableOrHandler("ncbiRefSeq", tdb) ||
         sameWord(table, "ncbiRefSeqPsl") ||
         sameWord(table, "ncbiRefSeqCurated") ||
         sameWord(table, "ncbiRefSeqPredicted") )
    {
    doNcbiRefSeq(tdb, item);
    }
else if (sameWord(table, "ncbiRefSeqOther") )
    {
    genericClickHandler(tdb, item, NULL);
    }
else if (sameWord(table, "refGene") )
    {
    doRefGene(tdb, item);
    }
else if (sameWord(table, "ccdsGene"))
    {
    doCcdsGene(tdb, item);
    }
else if (isNewGencodeGene(tdb))
    {
    doGencodeGene(tdb, item);
    }
else if (sameWord(table, "mappedRefSeq"))
    /* human refseqs on chimp browser */
    {
    doRefGene(tdb, item);
    }
else if (sameWord(table, "mgcGenes") || sameWord(table, "mgcFullMrna"))
    {
    doMgcGenes(tdb, item);
    }
else if (sameWord(table, "orfeomeGenes") || sameWord(table, "orfeomeMrna"))
    {
    doOrfeomeGenes(tdb, item);
    }
else if (startsWith("viralProt", table))
    {
    doViralProt(tdb, item);
    }
else if (sameWord("otherSARS", table))
    {
    doPslDetailed(tdb, item);
    }
else if (sameWord(table, "softberryGene"))
    {
    doSoftberryPred(tdb, item);
    }
else if (sameWord(table, "borkPseudo"))
    {
    doPseudoPred(tdb, item);
    }
else if (sameWord(table, "borkPseudoBig"))
    {
    doPseudoPred(tdb, item);
    }
else if (startsWith("encodePseudogene", table))
    {
    doEncodePseudoPred(tdb,item);
    }
else if (sameWord(table, "sanger22"))
    {
    doSangerGene(tdb, item, "sanger22pep", "sanger22mrna", "sanger22extra");
    }
else if (sameWord(table,"tRNAs"))
    {
    doTrnaGenesGb(tdb, item);
    }
else if (sameWord(table, "sanger20"))
    {
    doSangerGene(tdb, item, "sanger20pep", "sanger20mrna", "sanger20extra");
    }
else if (sameWord(table, "vegaGene") || sameWord(table, "vegaPseudoGene") )
    {
    doVegaGene(tdb, item, NULL);
    }
else if ((sameWord(table, "vegaGene") || sameWord(table, "vegaPseudoGene")) && hTableExists(database, "vegaInfoZfish"))
    {
    doVegaGeneZfish(tdb, item);
    }
else if (sameWord(table, "genomicDups"))
    {
    doGenomicDups(tdb, item);
    }
else if (sameWord(table, "blatMouse") || sameWord(table, "bestMouse")
	 || sameWord(table, "blastzTest") || sameWord(table, "blastzTest2"))
    {
    doBlatMouse(tdb, item);
    }
else if (startsWith("multAlignWebb", table))
    {
    doMultAlignZoo(tdb, item, &table[13] );
    }
else if (sameWord(table, "exaptedRepeats"))
    {
    doExaptedRepeats(tdb, item);
    }
/*
  Generalized code to show strict chain blastz alignments in the zoo browsers
*/
else if (containsStringNoCase(table, "blastzStrictChain")
         && containsStringNoCase(database, "zoo"))
    {
    int len = strlen("blastzStrictChain");
    char *orgName = &table[len];
    char dbName[32] = "zoo";
    strcpy(&dbName[3], orgName);
    len = strlen(orgName);
    strcpy(&dbName[3 + len], "3");
    longXenoPsl1(tdb, item, orgName, "chromInfo", dbName);
    }
 else if (sameWord(table, "blatChimp") ||
         sameWord(table, "chimpBac") ||
         sameWord(table, "bacChimp"))
    {
    longXenoPsl1Chimp(tdb, item, "Chimpanzee", "chromInfo", database);
    }
else if (sameWord(table, "htcLongXenoPsl2"))
    {
    htcLongXenoPsl2(table, item);
    }
else if (sameWord(table, "htcCdnaAliInWindow"))
    {
    htcCdnaAliInWindow(item);
    }
else if (sameWord(table, "htcPseudoGene"))
    {
    htcPseudoGene(table, item);
    }
else if (sameWord(table, "tfbsConsSites"))
    {
    tfbsConsSites(tdb, item);
    }
else if (sameWord(table, "tfbsCons"))
    {
    tfbsCons(tdb, item);
    }
else if (startsWith("atom", table) && !startsWith("atomMaf", table))
    {
    doAtom(tdb, item);
    }
else if (sameWord(table, "firstEF"))
    {
    firstEF(tdb, item);
    }
else if ( sameWord(table, "blastHg16KG") ||  sameWord(table, "blatHg16KG" ) ||
        startsWith("blastDm",  table) || sameWord(table, "blastMm6KG") ||
        sameWord(table, "blastSacCer1SG") || sameWord(table, "blastHg17KG") ||
        startsWith("blastCe", table) || sameWord(table, "blastHg18KG") )
    {
    blastProtein(tdb, item);
    }
else if (startsWith("altSeqLiftOverPsl", table) || startsWith("fixSeqLiftOverPsl", table))
    {
    doPslAltSeq(tdb, item);
    }
else if (sameWord(table, "chimpSimpleDiff"))
    {
    doSimpleDiff(tdb, "Chimp");
    }
/* This is a catch-all for blastz/blat tracks -- any special cases must be
 * above this point! */
else if (startsWith("map", table) ||startsWith("blastz", table) || startsWith("blat", table) || startsWith("tblast", table) || endsWith(table, "Blastz"))
    {
    char *genome = "Unknown";
    if (startsWith("tblast", table))
        genome = &table[6];
    if (startsWith("map", table))
        genome = &table[3];
    if (startsWith("blat", table))
        genome = &table[4];
    if (startsWith("blastz", table))
        genome = &table[6];
    else if (endsWith(table,"Blastz"))
        {
        genome = table;
        *strstr(genome, "Blastz") = 0;
        }
    if (hDbExists(genome))
        {
        /* handle table that include other database name
         * in trackname; e.g. blatCe1, blatCb1, blatCi1, blatHg15, blatMm3...
         * Uses genome column from database table as display text */
        genome = hGenome(genome);
        }
    doAlignCompGeno(tdb, item, genome);
    }
else if (sameWord(table, "rnaGene"))
    {
    doRnaGene(tdb, item);
    }
else if (startsWith("rnaStruct", table) 
         || sameWord(table, "RfamSeedFolds")
	 || sameWord(table, "RfamFullFolds")
	 || sameWord(table, "rfamTestFolds")
	 || sameWord(table, "evofold")
	 || sameWord(table, "evofoldV2")
	 || sameWord(table, "evofoldRaw")
	 || sameWord(table, "encode_tba23EvoFold")
	 || sameWord(table, "encodeEvoFold")
	 || sameWord(table, "rnafold")
	 || sameWord(table, "rnaTestFolds")
	 || sameWord(table, "rnaTestFoldsV2")
	 || sameWord(table, "rnaTestFoldsV3")
	 || sameWord(table, "mcFolds")
	 || sameWord(table, "rnaEditFolds")
	 || sameWord(table, "altSpliceFolds")
         || stringIn(table, "rnaSecStr"))
    {
    doRnaSecStr(tdb, item);
    }
else if (sameWord(table, "fishClones"))
    {
    doFishClones(tdb, item);
    }
else if (sameWord(table, "stsMarker"))
    {
    doStsMarker(tdb, item);
    }
else if (sameWord(table, "stsMapMouse"))
    {
    doStsMapMouse(tdb, item);
    }
else if (sameWord(table, "stsMapMouseNew")) /*steal map rat code for new mouse sts table. */
    {
    doStsMapMouseNew(tdb, item);
    }
else if(sameWord(table, "stsMapRat"))
    {
    doStsMapRat(tdb, item);
    }
else if (sameWord(table, "stsMap"))
    {
    doStsMarker(tdb, item);
    }
else if (sameWord(table, "rhMap"))
    {
    doZfishRHmap(tdb, item);
    }
else if (sameWord(table, "yaleBertoneTars"))
    {
    doYaleTars(tdb, item, NULL);
    }
else if (sameWord(table, "recombRate"))
    {
    doRecombRate(tdb);
    }
else if (sameWord(table, "recombRateRat"))
    {
    doRecombRateRat(tdb);
    }
else if (sameWord(table, "recombRateMouse"))
    {
    doRecombRateMouse(tdb);
    }
else if (sameWord(table, "genMapDb"))
    {
    doGenMapDb(tdb, item);
    }
else if (sameWord(table, "mouseSynWhd"))
    {
    doMouseSynWhd(tdb, item);
    }
else if (sameWord(table, "ensRatMusHom"))
    {
    doEnsPhusionBlast(tdb, item);
    }
else if (sameWord(table, "mouseSyn"))
    {
    doMouseSyn(tdb, item);
    }
else if (sameWord(table, "mouseOrtho"))
    {
    doMouseOrtho(tdb, item);
    }
else if (sameWord(table, USER_PSL_TRACK_NAME))
    {
    doUserPsl(table, item);
    }
else if (sameWord(table, PCR_RESULT_TRACK_NAME))
    {
    doPcrResult(table, item);
    }
else if (sameWord(table, "softPromoter"))
    {
    hgSoftPromoter(table, item);
    }
else if (isCustomTrack(table))
    {
    if (tdb != NULL)
        {
        char *origTrackName = trackDbSetting(tdb, "origTrackName");
        if (origTrackName)
            table = origTrackName;
        }
    hgCustom(table, item);
    }
else if (sameWord(table, "snpTsc") || sameWord(table, "snpNih") || sameWord(table, "snpMap"))
    {
    doSnpOld(tdb, item);
    }
else if (sameWord(table, "snp"))
    {
    doSnp(tdb, item);
    }
else if (snpVersion(table) >= 125)
    {
    doSnpWithVersion(tdb, item, snpVersion(table));
    }
else if (sameWord(table, "cnpIafrate"))
    {
    doCnpIafrate(tdb, item);
    }
else if (sameWord(table, "cnpLocke"))
    {
    doCnpLocke(tdb, item);
    }
else if (sameWord(table, "cnpIafrate2"))
    {
    doCnpIafrate2(tdb, item);
    }
else if (sameWord(table, "cnpSebat"))
    {
    doCnpSebat(tdb, item);
    }
else if (sameWord(table, "cnpSebat2"))
    {
    doCnpSebat2(tdb, item);
    }
else if (sameWord(table, "cnpSharp"))
    {
    doCnpSharp(tdb, item);
    }
else if (sameWord(table, "cnpSharp2"))
    {
    doCnpSharp2(tdb, item);
    }
else if (sameWord(table, "delHinds2"))
    {
    doDelHinds2(tdb, item);
    }
else if (sameWord(table, "delConrad2"))
    {
    doDelConrad2(tdb, item);
    }
else if (sameWord(table, "dgv") || sameWord(table, "dgvBeta"))
    {
    doDgv(tdb, item);
    }
else if ((sameWord(table, "dgvMerged") || sameWord(table, "dgvSupporting")) && (tdb && !startsWith("bigBed", tdb->type)))
    {
    doDgvPlus(tdb, item);
    }
else if (sameWord(table, "affy120K"))
    {
    doAffy120K(tdb, item);
    }
else if (sameWord(table, "affy10K"))
    {
    doAffy10K(tdb, item);
    }
else if (sameWord(table, "uniGene_2") || sameWord(table, "uniGene"))
    {
    doSageDataDisp(table, item, tdb);
    }
else if (sameWord(table, "uniGene_3"))
    {
    doUniGene3(tdb, item);
    }
else if (sameWord(table, "vax003") || sameWord(table, "vax004"))
    {
    doVax003Vax004(tdb, item);
    }
else if (sameWord(table, "tigrGeneIndex"))
    {
    doTigrGeneIndex(tdb, item);
    }
//else if ((sameWord(table, "bacEndPairs")) || (sameWord(table, "bacEndPairsBad")) || (sameWord(table, "bacEndPairsLong")) || (sameWord(table, "bacEndSingles")))
    //{
    //doLinkedFeaturesSeries(table, item, tdb);
    //}
else if (sameAltWords(table, handler, "bacEndPairs", "bacEndPairsBad", "bacEndPairsLong", "bacEndSingles", NULL))
    {
    doLinkedFeaturesSeries(table, item, tdb);
    }
else if ((sameWord(table, "fosEndPairs")) || (sameWord(table, "fosEndPairsBad")) || (sameWord(table, "fosEndPairsLong")))
    {
    doLinkedFeaturesSeries(table, item, tdb);
    }
 else if ((sameWord(table, "earlyRep")) || (sameWord(table, "earlyRepBad")))
    {
    doLinkedFeaturesSeries(table, item, tdb);
    }
else if (sameWord(table, "cgh"))
    {
    doCgh(table, item, tdb);
    }
else if (sameWord(table, "mcnBreakpoints"))
    {
    doMcnBreakpoints(table, item, tdb);
    }
else if (sameWord(table, "htcChainAli"))
    {
    htcChainAli(item);
    }
else if (sameWord(table, "htcChainTransAli"))
    {
    htcChainTransAli(item);
    }
else if (sameWord(table, "htcBigPslAliInWindow"))
    {
    htcBigPslAliInWindow(item);
    }
else if (sameWord(table, "htcBigPslAli"))
    {
    htcBigPslAli(item);
    }
else if (sameWord(table, "htcCdnaAli"))
    {
    htcCdnaAli(item);
    }
else if (sameWord(table, "htcUserAli"))
    {
    htcUserAli(item);
    }
else if (sameWord(table, "htcGetBlastPep"))
    {
    doGetBlastPep(item, cartString(cart, "aliTable"));
    }
else if (sameWord(table, "htcProteinAli"))
    {
    htcProteinAli(item, cartString(cart, "aliTable"));
    }
else if (sameWord(table, "htcBlatXeno"))
    {
    htcBlatXeno(item, cartString(cart, "aliTable"));
    }
else if (sameWord(table, "htcExtSeq"))
    {
    htcExtSeq(item);
    }
else if (sameWord(table, "htcTranslatedProtein"))
    {
    htcTranslatedProtein(item);
    }
else if (sameWord(table, "htcBigPslSequence"))
    {
    htcBigPsl(item, FALSE);
    }
else if (sameWord(table, "htcTranslatedBigPsl"))
    {
    htcBigPsl(item, TRUE);
    }
else if (sameWord(table, "htcTranslatedPredMRna"))
    {
    htcTranslatedPredMRna(item);
    }
else if (sameWord(table, "htcTranslatedMRna"))
    {
    htcTranslatedMRna(tdbForTableArg(), item);
    }
else if (sameWord(table, "ncbiRefSeqSequence"))
    {
    ncbiRefSeqSequence(item);
    }
else if (sameWord(table, "htcGeneMrna"))
    {
    htcGeneMrna(item);
    }
else if (sameWord(table, "htcRefMrna"))
    {
    htcRefMrna(item);
    }
else if (sameWord(table, "htcDisplayMrna"))
    {
    htcDisplayMrna(item);
    }
else if (sameWord(table, "htcGeneInGenome"))
    {
    htcGeneInGenome(item);
    }
else if (sameWord(table, "htcDnaNearGene"))
    {
    htcDnaNearGene( item);
    }
else if (sameWord(table, "getMsBedAll"))
    {
    getMsBedExpDetails(tdb, item, TRUE);
    }
else if (sameWord(table, "getMsBedRange"))
    {
    getMsBedExpDetails(tdb, item, FALSE);
    }
else if (sameWord(table, "perlegen"))
    {
    perlegenDetails(tdb, item);
    }
else if (sameWord(table, "haplotype"))
    {
    haplotypeDetails(tdb, item);
    }
else if (sameWord(table, "mitoSnps"))
    {
    mitoDetails(tdb, item);
    }
else if (sameWord(table, "loweProbes"))
    {
    doProbeDetails(tdb, item);
    }
else if (sameWord(table, "chicken13k"))
    {
    doChicken13kDetails(tdb, item);
    }
else if( sameWord(table, "ancientR"))
    {
    ancientRDetails(tdb, item);
    }
else if( sameWord(table, "gcPercent"))
    {
    doGcDetails(tdb, item);
    }
else if (sameWord(table, "htcTrackHtml"))
    {
    htcTrackHtml(tdbForTableArg());
    }

/*Evan's stuff*/
else if (sameWord(table, "genomicSuperDups"))
    {
    doGenomicSuperDups(tdb, item);
    }
else if (sameWord(table, "celeraDupPositive"))
    {
    doCeleraDupPositive(tdb, item);
    }

else if (sameWord(table, "triangle") || sameWord(table, "triangleSelf") || sameWord(table, "transfacHit") )
    {
    doTriangle(tdb, item, "dnaMotif");
    }
else if (sameWord(table, "esRegGeneToMotif"))
    {
    doTriangle(tdb, item, "esRegMotif");
    }
else if (startsWith("wgEncodeRegTfbsClusteredMotifs", table))
    {
    doTriangle(tdb, item, "transRegCodeMotif");
    }
else if (sameWord(table, "transRegCode"))
    {
    doTransRegCode(tdb, item, "transRegCodeMotif");
    }
else if (sameWord(table, "transRegCodeProbe"))
    {
    doTransRegCodeProbe(tdb, item, "transRegCode", "transRegCodeMotif",
                        "transRegCodeCondition", "growthCondition");
    }
else if (tdb != NULL && startsWith("wgEncodeRegDnaseClustered", tdb->track))
    {
    doPeakClusters(tdb, item);
    }

else if( sameWord( table, "humMusL" ) || sameWord( table, "regpotent" ))
    {
    humMusClickHandler( tdb, item, "Mouse", "mm2", "blastzBestMouse", 0);
    }
else if( sameWord( table, "musHumL" ))
    {
    humMusClickHandler( tdb, item, "Human", "hg12", "blastzBestHuman_08_30" , 0);
    }
else if( sameWord( table, "mm3Rn2L" ))
    {
    humMusClickHandler( tdb, item, "Rat", "rn2", "blastzBestRat", 0 );
    }
else if( sameWord( table, "hg15Mm3L" ))
    {
    humMusClickHandler( tdb, item, "Mouse", "mm3", "blastzBestMm3", 0 );
    }
else if( sameWord( table, "mm3Hg15L" ))
    {
    humMusClickHandler( tdb, item, "Human", "hg15", "blastzNetHuman" , 0);
    }
else if( sameWord( table, "footPrinter" ))
    {
    footPrinterClickHandler( tdb, item );
    }
else if (sameWord(table, "jaxQTL3"))
    {
    doJaxQTL3(tdb, item);
    }
else if (startsWith("jaxQTL", table) || startsWith("jaxQtl", table))
    {
    doJaxQTL(tdb, item);
    }
else if (startsWith("jaxAllele", table) || startsWith("jaxGeneTrap", table))
    {
    doJaxAllele(tdb, item);
    }
else if (startsWith("jaxPhenotype", table))
    {
    doJaxPhenotype(tdb, item);
    }
else if (startsWith("jaxRepTranscript", table))
    {
    doJaxAliasGenePred(tdb, item);
    }
else if (sameWord(table, "wgRna"))
    {
    doWgRna(tdb, item);
    }
else if (sameWord(table, "ncRna"))
    {
    doNcRna(tdb, item);
    }
else if (sameWord(table, "gbProtAnn"))
    {
    doGbProtAnn(tdb, item);
    }
else if (sameWord(table, "bdgpGene") || sameWord(table, "bdgpNonCoding"))
    {
    doBDGPGene(tdb, item);
    }
else if (sameWord(table, "flyBaseGene") || sameWord(table, "flyBaseNoncoding"))
    {
    doFlyBaseGene(tdb, item);
    }
else if (sameWord(table, "bgiGene"))
    {
    doBGIGene(tdb, item);
    }
else if (sameWord(table, "bgiSnp"))
    {
    doBGISnp(tdb, item);
    }
else if (sameWord(table, "encodeRna"))
    {
    doEncodeRna(tdb, item);
    }
else if (sameWord(table, "encodeRegions"))
    {
    doEncodeRegion(tdb, item);
    }
else if (sameWord(table, "encodeErgeHssCellLines"))
    {
    doEncodeErgeHssCellLines(tdb, item);
    }
else if (sameWord(table, "encodeErge5race")   || sameWord(table, "encodeErgeInVitroFoot")  || \
         sameWord(table, "encodeErgeDNAseI")  || sameWord(table, "encodeErgeMethProm")     || \
         sameWord(table, "encodeErgeExpProm") || sameWord(table, "encodeErgeStableTransf") || \
         sameWord(table, "encodeErgeBinding") || sameWord(table, "encodeErgeTransTransf")  || \
         sameWord(table, "encodeErgeSummary"))
    {
    doEncodeErge(tdb, item);
    }
else if(sameWord(table, "HInvGeneMrna"))
    {
    doHInvGenes(tdb, item);
    }
else if(sameWord(table, "sgdClone"))
    {
    doSgdClone(tdb, item);
    }
else if (sameWord(table, "sgdOther"))
    {
    doSgdOther(tdb, item);
    }
else if (sameWord(table, "vntr"))
    {
    doVntr(tdb, item);
    }
else if (sameWord(table, "luNega") || sameWord (table, "luPosi") || sameWord (table, "mRNARemains") || sameWord(table, "pseudoUcsc2"))
    {
    doPutaFrag(tdb, item);
    }
else if (sameWord(table, "potentPsl") || sameWord(table, "rcntPsl") )
    {
    potentPslAlign(table, item);
    }
else if (startsWith("zdobnov", table))
    {
    doZdobnovSynt(tdb, item);
    }
else if (startsWith("deweySynt", table))
    {
    doDeweySynt(tdb, item);
    }
else if (startsWith("eponine", table))
    {
    doBed6FloatScore(tdb, item);
    printTrackHtml(tdb);
    }
else if (sameWord(organism, "fugu") && startsWith("ecores", table))
    {
    doScaffoldEcores(tdb, item);
    }
else if (startsWith("pscreen", table))
    {
    doPscreen(tdb, item);
    }
else if (startsWith("flyreg", table))
    {
    doFlyreg(tdb, item);
    }
/* ENCODE table */
else if (startsWith("encodeGencodeIntron", table) &&
	 sameString(tdb->type, "bed 6 +"))
    {
    doGencodeIntron(tdb, item);
    }
else if (sameWord(table, "encodeIndels"))
    {
    doEncodeIndels(tdb, item);
    }
else if (startsWith("encodeStanfordPromoters", table))
    {
    doEncodeStanfordPromoters(tdb, item);
    }
else if (startsWith("encodeStanfordRtPcr", table))
    {
    doEncodeStanfordRtPcr(tdb, item);
    }
else if (startsWith("encodeHapMapAlleleFreq", table))
    {
    doEncodeHapMapAlleleFreq(tdb, item);
    }
else if (sameString("cutters", table))
    {
    doCutters(item);
    }
else if (sameString("anoEstTcl", table))
    {
    doAnoEstTcl(tdb, item);
    }
else if (sameString("interPro", table))
    {
    doInterPro(tdb, item);
    }
else if (sameString("dvBed", table))
    {
    doDv(tdb, item);
    }
else if (startsWith("hapmapSnps", table) &&
	 (strlen(table) == 13 ||
	  (endsWith(table, "PhaseII") && strlen(table) == 20)))
    {
    doHapmapSnps(tdb, item);
    }
else if (startsWith("hapmapAllelesChimp", table) ||
         startsWith("hapmapAllelesMacaque", table))
    {
    doHapmapOrthos(tdb, item);
    }
else if (sameString("snpArrayAffy250Nsp", table) ||
         sameString("snpArrayAffy250Sty", table) ||
         sameString("snpArrayAffy5", table) ||
         sameString("snpArrayAffy6", table) ||
         sameString("snpArrayAffy10", table) ||
         sameString("snpArrayAffy10v2", table) ||
         sameString("snpArrayAffy50HindIII", table) ||
         sameString("snpArrayAffy50XbaI", table))
    {
    doSnpArray(tdb, item, "Affy");
    }
else if (sameString("snpArrayIllumina300", table) ||
         sameString("snpArrayIllumina550", table) ||
	 sameString("snpArrayIllumina650", table) ||
	 (sameString("snpArrayIllumina1M", table) && startsWith("rs", item) ) ||
	 (sameString("snpArrayIlluminaHuman660W_Quad", table) && startsWith("rs", item) ) ||
	 (sameString("snpArrayIlluminaHumanOmni1_Quad", table) && startsWith("rs", item) ) ||
	 (sameString("snpArrayIlluminaHumanCytoSNP_12", table) && startsWith("rs", item) ) )
    {
    doSnpArray(tdb, item, "Illumina");
    }
else if (
	 (sameString("snpArrayIlluminaHuman660W_Quad", table) && !startsWith("rs", item) ) ||
	 (sameString("snpArrayIlluminaHumanOmni1_Quad", table) && !startsWith("rs", item) ) ||
	 (sameString("snpArrayIlluminaHumanCytoSNP_12", table) && !startsWith("rs", item) ) )
    {
    /* special processing for non-dbSnp entries of the 3 new Illumina arrays */
    doSnpArray2(tdb, item, "Illumina");
    }
else if (sameString("pgVenter", table) ||
         sameString("pgWatson", table) ||
         sameString("pgYri", table)    ||
         sameString("pgCeu", table)    ||
         sameString("pgChb", table)    ||
         sameString("pgJpt", table)    ||
	 sameString("pgSjk", table)    ||
	 startsWith("pgYoruban", table) ||
	 (startsWith("pgNA", table) && isdigit(table[4])) ||
         sameString("hbPgTest", table) ||
         sameString("hbPgWild", table) ||
	 sameString("pgYh1", table) ||
         sameString("pgKb1", table) ||
         sameString("pgNb1", table) || sameString("pgNb1Indel", table) ||
         sameString("pgTk1", table) || sameString("pgTk1Indel", table) ||
         sameString("pgMd8", table) || sameString("pgMd8Indel", table) ||
         sameString("pgKb1Illum", table) ||
         sameString("pgKb1454", table) || sameString("pgKb1Indel", table) ||
         sameString("pgKb1Comb", table) ||
         sameString("pgAbtSolid", table) ||
         sameString("pgAbt", table) || sameString("pgAbt454", table) ||
         sameString("pgAbt454indels", table) ||
         sameString("pgAbtIllum", table) ||
         sameString("pgAk1", table) ||
         sameString("pgQuake", table) ||
         sameString("pgIrish", table) ||
         sameString("pgSaqqaq", table) ||
         sameString("pgSaqqaqHc", table) ||
         sameString("pgTest", table) )
    {
    doPgSnp(tdb, item, NULL);
    }
else if (startsWith("pg", table) &&
         (endsWith(table, "PhenCode") || endsWith(table, "Snpedia") || endsWith(table, "Hgmd")) )
    {
    doPgPhenoAssoc(tdb, item);
    }
else if (sameString("gvPos", table))
    {
    doGv(tdb, item);
    }
else if (sameString("protVarPos", table))
    {
    doProtVar(tdb, item);
    }
else if (sameString("oreganno", table))
    {
    doOreganno(tdb, item);
    }
else if (sameString("oregannoOther", table))
    {
    /* Regions from another species lifted and displayed in current */
    /* same table structure, just different table name/table */
    doOreganno(tdb, item);
    }
else if (sameString("allenBrainAli", table))
    {
    doAllenBrain(tdb, item);
    }
else if (sameString("dless", table) || sameString("encodeDless", table))
    {
    doDless(tdb, item);
    }
else if (sameString("mammalPsg", table))
   {
     doMammalPsg(tdb, item);
   }
else if (sameString("igtc", table))
    {
    doIgtc(tdb, item);
    }
else if (sameString("rdmr", table))
    {
    doRdmr(tdb, item);
    }
else if (startsWith("komp", table) || startsWith("ikmc", table))
    {
    doKomp(tdb, item);
    }
else if (sameString("hgIkmc", table))
    {
    doHgIkmc(tdb, item);
    }
else if (startsWith("dbRIP", table))
    {
    dbRIP(tdb, item, NULL);
    }
else if (sameString("omicia", table)) //Auto", table) || sameString("omiciaHand", table))
    {
    doOmicia(tdb, item);
    }
else if ( sameString("expRatioUCSFDemo", table) || sameString("cnvLungBroadv2", table) || sameString("CGHBreastCancerUCSF", table) || sameString("expBreastCancerUCSF", table))
    {
    doUCSFDemo(tdb, item);
    }
else if (startsWith("consIndels", table))
    {
    doConsIndels(tdb,item);
    }
else if (startsWith(KIDD_EICHLER_DISC_PREFIX, table))
    {
    doKiddEichlerDisc(tdb, item);
    }
else if (startsWith("hgdpGeo", table))
    {
    doHgdpGeo(tdb, item);
    }
else if (startsWith("gwasCatalog", table))
    {
    doGwasCatalog(tdb, item);
    }
else if (sameString("par", table))
    {
    doParDetails(tdb, item);
    }
else if (startsWith("pubs", trackHubSkipHubName(table)))
    {
    doPubsDetails(tdb, item);
    }
else if (tdb != NULL && startsWith("bedDetail", tdb->type))
    {
    doBedDetail(tdb, NULL, item);
    }
else if (startsWith("numtS", table))
    {
    doNumtS(tdb, item);
    }
else if (sameString("cosmic", table))
    {
    doCosmic(tdb, item);
    }
else if (startsWith("geneReviews", table))
    {
    doGeneReviews(tdb, item);
    }
else if (startsWith("qPcrPrimers", table))
    {
    doQPCRPrimers(tdb, item);
    }
else if (sameString("lrg", table))
    {
    doLrg(tdb, item);
    }
else if (sameWord(table, "htcLrgCdna"))
    {
    htcLrgCdna(item);
    }
else if (startsWith("peptideAtlas", table))
    {
    doPeptideAtlas(tdb, item);
    }
else if (startsWith("gtexGene", table))
    {
    doGtexGeneExpr(tdb, item);
    }
else if (startsWith("gtexEqtlCluster", table))
    {
    doGtexEqtlDetails(tdb, item);
    }
else if (startsWith("snake", trackHubSkipHubName(table)))
    {
    doSnakeClick(tdb, item);
    }
else if (startsWith("clinvarSubLolly", trackHubSkipHubName(table)))
    {
    doClinvarSubLolly(tdb, item);
    }
else if (tdb != NULL &&
        (startsWithWord("vcfTabix", tdb->type) || sameWord("vcfPhasedTrio", tdb->type)))
    {
    doVcfTabixDetails(tdb, item);
    }
else if (tdb != NULL && startsWithWord("vcf", tdb->type))
    {
    doVcfDetails(tdb, item);
    }
else if (tdb != NULL && 
        (startsWithWord("barChart", tdb->type) || startsWithWord("bigBarChart", tdb->type)))
    {
    doBarChartDetails(tdb, item);
    printTrackHtml(tdb);
    }
else if (startsWith("hprcDeletions", table) || startsWith("hprcInserts", table) || startsWith("hprcArr", table)|| startsWith("hprcDouble", table))
    {
    doHPRCTable(tdb, item);
    }
else if (tdb && sameString(tdb->type, "lorax"))
    {
    doLorax(tdb, item);
    }
else
    return FALSE;

return TRUE;
}

struct hash *orgDbHash = NULL;

void initOrgDbHash()
/* Function to initialize a hash of organism names that hash to a database ID.
 * This is used to show alignments by hashing the organism associated with the
 * track to the database name where the chromInfo is stored. For example, the
 * mousBlat track in the human browser would hash to the mm2 database. */
{
orgDbHash = hashNew(8);
}

void cartDoMiddle(struct cart *theCart)
/* Save cart and do main middle handler. */
{
initOrgDbHash();
cart = theCart;
doMiddle();
}

char *excludeVars[] = {"Submit", "submit", "g", "i", "aliTable", "addp", "pred", NULL};

int main(int argc, char *argv[])
{
long enteredMainTime = clock1000();
/* 0, 0, == use default 10 second for warning, 20 second for immediate exit */
issueBotWarning = earlyBotCheck(enteredMainTime, "hgc", delayFraction, 0, 0, "html");
pushCarefulMemHandler(LIMIT_2or6GB);
cgiSpoof(&argc,argv);
cartEmptyShell(cartDoMiddle, hUserCookie(), excludeVars, NULL);
cgiExitTime("hgc", enteredMainTime);
return 0;
}


