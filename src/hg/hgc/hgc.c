/* hgc - Human Genome Click processor - gets called when user clicks
 * on something in human tracks display. */

#include "common.h"
#include "hCommon.h"
#include "hash.h"
#include "bits.h"
#include "memgfx.h"
#include "portable.h"
#include "errabort.h"
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
#include "hui.h"
#include "hgRelate.h"
#include "psl.h"
#include "bed.h"
#include "cgh.h" 
#include "agpFrag.h"
#include "agpGap.h"
#include "ctgPos.h"
#include "clonePos.h"
#include "bactigPos.h"
#include "rmskOut.h"
#include "xenalign.h"
#include "isochores.h"
#include "simpleRepeat.h"
#include "cpgIsland.h"
#include "genePred.h"
#include "pepPred.h"
#include "wabAli.h"
#include "genomicDups.h"
#include "est3.h"
#include "rnaGene.h"
#include "stsMarker.h"
#include "stsMap.h"
#include "recombRate.h"
#include "stsInfo.h"
#include "stsInfo2.h"
#include "mouseSyn.h"
#include "mouseSynWhd.h"
#include "ensPhusionBlast.h"
#include "cytoBand.h"
#include "knownMore.h"
#include "snp.h"
#include "softberryHom.h"
#include "borkPseudoHom.h"
#include "sanger22extra.h"
#include "refLink.h"
#include "hgConfig.h"
#include "estPair.h"
#include "softPromoter.h"
#include "customTrack.h"
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
#include "dnaMotif.h"
#include "dbSnpRS.h"
#include "genomicSuperDups.h"
#include "celeraDupPositive.h"
#include "celeraCoverage.h"
#include "sample.h"
#include "axt.h"
#include "axtInfo.h"
#include "jaxQTL.h"
#include "gbProtAnn.h"
#include "hgSeq.h"
#include "chain.h"
#include "chainDb.h"
#include "chainNetDbLoad.h"
#include "chainToPsl.h"
#include "netAlign.h"
#include "stsMapRat.h"
#include "stsInfoRat.h"
#include "stsMapMouseNew.h"
#include "stsInfoMouseNew.h"
#include "vegaInfo.h"
#include "scoredRef.h"
#include "minGeneInfo.h"
#include "tigrCmrGene.h"
#include "llaInfo.h"
#include "hgc.h"
#include "genbank.h"
#include "pseudoGeneLink.h"
#include "axtLib.h"
#include "ensFace.h"

static char const rcsid[] = "$Id: hgc.c,v 1.483 2003/09/29 20:56:34 baertsch Exp $";

#define LINESIZE 70  /* size of lines in comp seq feature */

struct cart *cart;	/* User's settings. */
char *seqName;		/* Name of sequence we're working on. */
int winStart, winEnd;   /* Bounds of sequence. */
char *database;		/* Name of mySQL database. */
char *organism;		/* Colloquial name of organism. */
char *scientificName;	/* Scientific name of organism. */

char *protDbName;	/* Name of proteome database */
struct hash *trackHash;	/* A hash of all tracks - trackDb valued */

char mousedb[] = "mm3";

/* JavaScript to automatically submit the form when certain values are
 * changed. */
char *onChangeAssemblyText = "onchange=\"document.orgForm.submit();\"";

#define NUMTRACKS 9
int prevColor[NUMTRACKS]; /* used to opetimize color change html commands */
int currentColor[NUMTRACKS]; /* used to opetimize color change html commands */
int maxShade = 9;	/* Highest shade in a color gradient. */
Color shadesOfGray[10+1];	/* 10 shades of gray from white to black */

Color shadesOfRed[16];
boolean exprBedColorsMade = FALSE; /* Have the shades of red been made? */
int maxRGBShade = 16;

struct bed *sageExpList = NULL;

/* See this NCBI web doc for more info about entrezFormat:
 * http://www.ncbi.nlm.nih.gov/entrez/query/static/linking.html */
char *entrezFormat = "http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?cmd=Search&db=%s&term=%s&doptcmdl=%s&tool=genome.ucsc.edu";
/* db=unists is not mentioned in NCBI's doc... so stick with this usage: */
char *unistsnameScript = "http://www.ncbi.nlm.nih.gov:80/entrez/query.fcgi?db=unists";
char *unistsScript = "http://www.ncbi.nlm.nih.gov/genome/sts/sts.cgi?uid=";
char *gdbScript = "http://gdb.wehi.edu.au/gdb-bin/genera/accno?accessionNum=";
char *cloneRegScript = "http://www.ncbi.nlm.nih.gov/genome/clone/clname.cgi?stype=Name&list=";
char *genMapDbScript = "http://genomics.med.upenn.edu/perl/genmapdb/byclonesearch.pl?clone=";

/* initialized by getCtList() if necessary: */
struct customTrack *theCtList = NULL;

/* forwards */
struct psl *getAlignments(struct sqlConnection *conn, char *table, char *acc);
void printAlignments(struct psl *pslList, 
		     int startFirst, char *hgcCommand, char *typeName, char *itemIn);

void hgcStart(char *title)
/* Print out header of web page with title.  Set
 * error handler to normal html error handler. */
{
cartHtmlStart(title);
}

static void printEntrezNucleotideUrl(FILE *f, char *accession)
/* Print URL for Entrez browser on a nucleotide. */
{
fprintf(f, entrezFormat, "Nucleotide", accession, "GenBank");
}

static void printEntrezProteinUrl(FILE *f, char *accession)
/* Print URL for Entrez browser on a protein. */
{
fprintf(f, entrezFormat, "Protein", accession, "GenPept");
}

static void printEntrezPubMedUrl(FILE *f, char *term)
/* Print URL for Entrez browser on a PubMed search. */
{
fprintf(f, entrezFormat, "PubMed", term, "DocSum");
}

static void printEntrezOMIMUrl(FILE *f, int id)
/* Print URL for Entrez browser on an OMIM search. */
{
char buf[64];
snprintf(buf, sizeof(buf), "%d", id);
fprintf(f, entrezFormat, "OMIM", buf, "Detailed");
}

static void printSwissProtProteinUrl(FILE *f, char *accession)
/* Print URL for SwissProt browser on a protein. */
{
fprintf(f, "\"http://www.expasy.org/cgi-bin/niceprot.pl?%s\"", accession);
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

static void printGdbUrl(FILE *f, char *id)
/* Print URL for GDB browser for an id */
{
fprintf(f, "\"%s%s\"", gdbScript, id);
}

static void printCloneRegUrl(FILE *f, char *clone)
/* Print URL for Clone Registry at NCBI for a clone */
{
fprintf(f, "\"%s%s\"", cloneRegScript, clone);
}

static void printGenMapDbUrl(FILE *f, char *clone)
/* Print URL for GenMapDb at UPenn for a clone */
{
fprintf(f, "\"%s%s\"", genMapDbScript, clone);
}

char *hgcPath()
/* Return path of this CGI script. */
{
return "../cgi-bin/hgc";
}

char *hgcPathAndSettings()
/* Return path with this CGI script and session state variable. */
{
static struct dyString *dy = NULL;
if (dy == NULL)
    {
    dy = newDyString(128);
    dyStringPrintf(dy, "%s?%s", hgcPath(), cartSidUrlString(cart));
    }
return dy->string;
}

void hgcAnchorSomewhere(char *group, char *item, char *other, char *chrom)
/* Generate an anchor that calls click processing program with item 
 * and other parameters. */
{
char *tbl = cgiUsualString("table", cgiString("g"));
printf("<A HREF=\"%s&g=%s&i=%s&c=%s&l=%d&r=%d&o=%s&table=%s\">",
       hgcPathAndSettings(), group, item, chrom, winStart, winEnd, other,
       tbl);
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


void hgcAnchorGenePsl(char *item, char *other, char *chrom, char *tag)
/* Generate an anchor to htcGenePsl. */
{
struct dbDb *dbList = hGetAxtInfoDbs();
struct axtInfo *aiList = NULL;
char *db2;
char *encodedItem = cgiEncode(item);
if (dbList != NULL)
    db2 = dbList->name;
else
    db2 = mousedb;
aiList = hGetAxtAlignments(db2);
printf("<A HREF=\"%s&g=%s&i=%s&c=%s&l=%d&r=%d&o=%s&alignment=%s&db2=%s&xyzzy=xyzzy#%s\">",
       hgcPathAndSettings(), "htcGenePsl", encodedItem, chrom, winStart, winEnd,
       other, cgiEncode(aiList->alignment), db2, tag);
dbDbFreeList(&dbList);
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

boolean clipToChrom(int *pStart, int *pEnd)
/* Clip start/end coordinates to fit in chromosome. */
{
static int chromSize = -1;

if (chromSize < 0)
    chromSize = hChromSize(seqName);
if (*pStart < 0) *pStart = 0;
if (*pEnd > chromSize) *pEnd = chromSize;
return *pStart < *pEnd;
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
seq = hDnaFromSeq(seqName, s, e, dnaLower);
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


void printPosOnChrom(char *chrom, int start, int end, char *strand, boolean featDna)
/* Print position lines referenced to chromosome. Strand argument may be NULL */
{
char band[64];

printf("<B>Chromosome:</B> %s<BR>\n", skipChr(chrom));
if (hChromBand(chrom, (start + end)/2, band))
    printf("<B>Band:</B> %s<BR>\n", band);
printf("<B>Begin in Chromosome:</B> %d<BR>\n", start+1);
printf("<B>End in Chromosome:</B> %d<BR>\n", end);
printf("<B>Genomic Size:</B> %d<BR>\n", end - start);
if (strand != NULL)
    printf("<B>Strand:</B> %s<BR>\n", strand);
else
    strand = "?";
if (featDna)
    {
    char *tbl = cgiUsualString("table", cgiString("g"));
    printf("<A HREF=\"%s&o=%d&g=getDna&i=mixed&c=%s&l=%d&r=%d&strand=%s&table=%s\">"
	   "View DNA for this feature</A><BR>\n",  hgcPathAndSettings(),
	   start, chrom, start, end, strand, tbl);
    }
}


void printPosOnScaffold(char *chrom, int start, int end, char *strand)
/* Print position lines referenced to scaffold.  'strand' argument may be null. */
{
    char *scaffoldName;
    int scaffoldStart, scaffoldEnd;

    if (!hScaffoldPos(chrom, start, end, &scaffoldName, &scaffoldStart, &scaffoldEnd))
        {
        printPosOnChrom(chrom, start,end,strand, FALSE);
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

void printPos(char *chrom, int start, int end, char *strand, boolean featDna)
/* Print position lines.  'strand' argument may be null. */
{
    if (sameWord(organism, "Fugu"))
        /* use this for unmapped genomes */
        printPosOnScaffold(chrom, start, end, strand);
    else
        printPosOnChrom(chrom, start, end, strand, featDna);
}

void samplePrintPos(struct sample *smp, int smpSize)
/* Print first three fields of a sample 9 type structure in
 * standard format. */
{
if( smpSize != 9 ) 
    errAbort( "Invalid sample entry!\n It has %d fields instead of 9\n" );

printf("<B>Item:</B> %s<BR>\n", smp->name);
printf("<B>Score:</B> %d<BR>\n", smp->score);
printf("<B>Strand:</B> %s<BR>\n", smp->strand);
printPos(smp->chrom, smp->chromStart, smp->chromEnd, NULL, TRUE);
}


void bedPrintPos(struct bed *bed, int bedSize)
/* Print first three fields of a bed type structure in
 * standard format. */
{
if (bedSize > 3)
    printf("<B>Item:</B> %s<BR>\n", bed->name);
if (bedSize > 4)
    printf("<B>Score:</B> %d<BR>\n", bed->score);
if (bedSize > 5)
   printf("<B>Strand:</B> %s<BR>\n", bed->strand);
printPos(bed->chrom, bed->chromStart, bed->chromEnd, NULL, TRUE);
}

void genericHeader(struct trackDb *tdb, char *item)
/* Put up generic track info. */
{
if (item != NULL && item[0] != 0)
    cartWebStart(cart, "%s (%s)", tdb->longLabel, item);
else
    cartWebStart(cart, "%s", tdb->longLabel);
}

static struct dyString *subMulti(char *orig, int subCount, 
				 char *in[], char *out[])
/* Perform multiple substitions on orig. */
{
int i;
struct dyString *s = newDyString(256), *d = NULL;

dyStringAppend(s, orig);
for (i=0; i<subCount; ++i)
    {
    d = dyStringSub(s->string, in[i], out[i]);
    dyStringFree(&s);
    s = d;
    d = NULL;
    }
return s;
}

void printCustomUrl(struct trackDb *tdb, char *itemName, boolean encode)
/* Print custom URL. */
{
char *url = tdb->url;
if (url != NULL && url[0] != 0)
    {
    char *before, *after = "", *s;
    struct dyString *uUrl = NULL;
    struct dyString *eUrl = NULL;
    char startString[64], endString[64];
    char fixedUrl[1024];
    char *ins[7], *outs[7];
    char *eItem = (encode ? cgiEncode(itemName) : cloneString(itemName));

    sprintf(startString, "%d", winStart);
    sprintf(endString, "%d", winEnd);
    ins[0] = "$$";
    outs[0] = itemName;
    ins[1] = "$T";
    outs[1] = tdb->tableName;
    ins[2] = "$S";
    outs[2] = seqName;
    ins[3] = "$[";
    outs[3] = startString;
    ins[4] = "$]";
    outs[4] = endString;
    ins[5] = "$s";
    outs[5] = skipChr(seqName);
    ins[6] = "$D";
    outs[6] = database;
    uUrl = subMulti(url, ArraySize(ins), ins, outs);
    outs[0] = eItem;
    eUrl = subMulti(url, ArraySize(ins), ins, outs);
    printf("<B>Outside Link: </B>");
    printf("<A HREF=\"%s\" target=_blank>", eUrl->string);
    
    if (sameWord(tdb->tableName, "npredGene"))
    	{
   	printf("%s (%s)</A><BR>\n", itemName, "NCBI MapView");
	}
    else
    	{
    	printf("%s</A><BR>\n", uUrl->string);
	}
    freeMem(eItem);
    freeDyString(&uUrl);
    freeDyString(&eUrl);
    }
}

void genericSampleClick(struct sqlConnection *conn, struct trackDb *tdb, 
			char *item, int start, int smpSize)
/* Handle click in generic sample (wiggle) track. */
{
char table[64];
boolean hasBin;
struct sample *smp;
char query[512];
struct sqlResult *sr;
char **row;
boolean firstTime = TRUE;

hFindSplitTable(seqName, tdb->tableName, table, &hasBin);
sprintf(query, "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d",
        table, item, seqName, start);

//errAbort( "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d",
//        table, item, seqName, start);


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

void genericBedClick(struct sqlConnection *conn, struct trackDb *tdb, 
		     char *item, int start, int bedSize)
/* Handle click in generic BED track. */
{
char table[64];
boolean hasBin;
struct bed *bed;
char query[512];
struct sqlResult *sr;
char **row;
boolean firstTime = TRUE;

hFindSplitTable(seqName, tdb->tableName, table, &hasBin);
if (bedSize <= 3)
    sprintf(query, "select * from %s where chrom = '%s' and chromStart = %d", table, seqName, start);
else
    sprintf(query, "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d",
	    table, item, seqName, start);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (firstTime)
	firstTime = FALSE;
    else
	htmlHorizontalLine();
    bed = bedLoadN(row+hasBin, bedSize);
    bedPrintPos(bed, bedSize);
    }
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
#define BLUE 0x0000FF
#define MEDBLUE 0x6699FF
#define PURPLE 0x9900cc
#define BLACK 0x000000
#define CYAN 0x00FFFF
#define ORANGE 0xFF6600
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
	return LTGRAY;
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
    dyStringPrintf(dy,"</FONT><FONT COLOR=\"%06X\">",color);
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
char table[64];
boolean hasBin;
struct bed *bed;
char query[512];
struct sqlResult *sr;
char **row;
boolean firstTime = TRUE;

hFindSplitTable(seqName, tdb->tableName, table, &hasBin);
if (bedSize <= 3)
    sprintf(query, "select * from %s where chrom = '%s' and chromStart = %d", table, seqName, start);
else
    sprintf(query, "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d",
	    table, item, seqName, start);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (firstTime)
	firstTime = FALSE;
    else
	htmlHorizontalLine();
    bed = bedLoadN(row+hasBin, bedSize);
    bedPrintPos(bed, bedSize);
    }
}

void axtOneGeneOut(struct axt *axtList, int lineSize, 
		   FILE *f, struct genePred *gp, char *nibFile)
/* Output axt and orf in pretty format. */
{
struct axt *axt;
int oneSize;
int i;
char *rcText = "";
int phase = 0;
int exonPointer = 0;
int tCodonPos = 1;
int qCodonPos = 1;
int tOffset;
int qOffset;
int qStart;
int qEnd;
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
//char nibFile[128];

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

//safef(nibFile, sizeof(nibFile), "%s/%s.nib",nibDir,gp->chrom);
/* if no alignment , make a bad one */
if (axtList == NULL)
    if (gp->strand[0] == '+')
        axtList = createAxtGap(nibFile,gp->chrom,tStart,tEnd ,gp->strand[0]);
    else
        axtList = createAxtGap(nibFile,gp->chrom,tEnd,tStart ,gp->strand[0]);

/* append unaligned coding region to list */ 
if (posStrand)
    {
    if ((axtList->tStart)-1 > tStart)
        {
        struct axt *axtGap = createAxtGap(nibFile,gp->chrom,tStart,axtList->tStart-1,gp->strand[0]);
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
        qSize = hChromSize2(axt->qName);
    if (qStrand == '-')
        {
        qStart = qSize - axt->qEnd;
        qEnd = qSize - axt->qStart;
        }
    fprintf(f, ">%s:%d-%d %s:%d-%d (%c) score %d coding %d-%d utr/coding %d-%d gene %c alignment %c\n", 
	    axt->tName, axt->tStart+1, axt->tEnd,
	    axt->qName, qStart+1, qEnd, qStrand, axt->score,  tStart+1, tEnd, gp->txStart+1, gp->txEnd, gp->strand[0], axt->qStrand);

    qPtr = qStart;
    qCodonPos = tCodonPos; /* put translation back in sync */
    if (!posStrand)
        {
        qPtr = qEnd;
        /* skip to next exon fi we are starting in the middle of a gene  - should not happen */
        while ((tPtr < nextEnd) && (nextEndIndex > 0))
            {
            nextEndIndex--;
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
            nextStart = gp->exonStarts[nextEndIndex];
            nextEnd = gp->exonEnds[nextEndIndex];
            if (nextStart > tStart)
                tClass = INTRON;
            }
        }
    /* loop thru one base at a time */
    while (sizeLeft > 0)
        {
        struct dyString *dyT = newDyString(1024);
        struct dyString *dyQ = newDyString(1024);
        struct dyString *dyQprot = newDyString(1024);
        struct dyString *dyTprot = newDyString(1024);
        struct dyString *exonTag = newDyString(1024);
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
                    nextEnd = gp->exonEnds[nextEndIndex];
                    }
                }
            else
                {
                /* look for end of exon  negative strand */
                if (tPtr == nextEnd)
                    {
                    tCoding=FALSE;
                    qCoding=FALSE;
                    tClass=INTRON;
                    qClass=INTRON;
                    nextEndIndex--;
                    nextStart = (gp->exonEnds[nextEndIndex]);
                    nextEnd = (gp->exonStarts[nextEndIndex]);
                    }
                }
            if (posStrand)
                {
                /* look for start codon and color it green*/
                if ((tPtr >= (tStart)) && (tPtr <=(tStart+2)))
                    {
                    tClass=STARTCODON;
                    qClass=STARTCODON;
                    tCoding=TRUE;
                    qCoding=TRUE;
                    if (tPtr == tStart) 
                        {
                        tCodonPos=1;
                        qCodonPos=1;
                        }
                    }
                /* look for stop codon and color it red */
                if ((tPtr >= tEnd) && (tPtr <= (tEnd+2)))
                    {
                    tClass=STOPCODON;
                    qClass=STOPCODON;
                    tCoding=FALSE;
                    qCoding=FALSE;
                    }
                }
            else
                {
                /* look for start codon and color it green negative strand case*/
                if ((tPtr <= (tStart)) && (tPtr >=(tStart-2)))
                    {
                    tClass=STARTCODON;
                    qClass=STARTCODON;
                    tCoding=TRUE;
                    qCoding=TRUE;
                    if (tPtr == tStart-3) 
                        {
                        tCodonPos=1;
                        qCodonPos=1;
                        }
                    }
                /* look for stop codon and color it red - negative strand*/
                if ((tPtr <= tEnd+3) && (tPtr >= (tEnd+1)))
                    {
                    tClass=STOPCODON;
                    qClass=STOPCODON;
                    tCoding=FALSE;
                    qCoding=FALSE;
                    }
                }
            if (posStrand)
                {
                /* look for 3' utr and color it dk grey */
                if (tPtr == (tEnd +3) )
                    {
                    tClass = UTR3;
                    qClass = UTR3;
                    }
                }
            else 
                {
                /* look for 3' utr and color it dk grey negative strand case*/
                if (tPtr == (tEnd ) )
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
                    //qClass = INFRAMESTOP;
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
        /* debug version
	   if (posStrand)
	   dyStringPrintf(dyT, " %d thisExon=%d-%d xon %d",tPtr, gp->exonStarts[(nextEndIndex == 0) ? 0 : nextEndIndex - 1]+1, gp->exonEnds[(nextEndIndex == 0) ? 0 : nextEndIndex - 1],(nextEndIndex == 0) ? 1 : nextEndIndex);
	   else
	   dyStringPrintf(dyT, " %d thisExon=%d-%d xon %d",tPtr, gp->exonStarts[(nextEndIndex == gp->exonCount) ? gp->exonCount : nextEndIndex ]+1, gp->exonEnds[(nextEndIndex == gp->exonCount) ? gp->exonCount : nextEndIndex ],(nextEndIndex == 0) ? 1 : nextEndIndex);
	*/
        dyStringAppendC(dyT,'\n');
        resetClassStr(dyT,0);
        resetClassStr(dyQ,1);
        setClassStr(dyQ,LABEL,1);
        if (posStrand)
            dyStringPrintf(dyQ, " %d ",qPtr);
        else
            dyStringPrintf(dyQ, " %d ",qPtr);
        /* debug version
	   if (posStrand)
	   dyStringPrintf(dyQ, " %d nextExon=%d-%d xon %d",qPtr, nextStart+1,nextEnd,nextEndIndex+1);
	   else
	   dyStringPrintf(dyQ, " %d nextExon=%d-%d xon %d",qPtr, nextStart+1,nextEnd,nextEndIndex);
	*/
        dyStringAppendC(dyQ,'\n');
        resetClassStr(dyQ,1);
        dyStringAppendC(dyQprot,'\n');
        dyStringAppendC(dyTprot,'\n');

        /* skip regions with no alignment and no colored coding regions */
        //if ( numberOfGaps(q,oneSize) < oneSize /*|| isBlue(dyT->string)*/) 
            {
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
        /* look for end of line */
        if (oneSize > lineSize)
            oneSize = lineSize;
        sizeLeft -= oneSize;
        q += oneSize;
        t += oneSize;
        freeDyString(&dyT);
        freeDyString(&dyQ);
        freeDyString(&dyQprot);
        freeDyString(&dyTprot);
        }
    }
}

struct axt *getAxtListForGene(struct genePred *gp, char *nib, char *fromDb, char *toDb,
		       char *alignment)
/* get all axts for a gene */
{
struct lineFile *lf ;
struct axt *axt, *axtGap;
struct axt *axtList = NULL;
int prevEnd = gp->txStart;
int prevStart = gp->txEnd;
int tmp;

lf = lineFileOpen(getAxtFileName(gp->chrom, toDb, alignment, fromDb), TRUE);
while ((axt = axtRead(lf)) != NULL)
    {
    if (sameString(gp->chrom , axt->tName) && 
	(
	 (((axt->tStart <= gp->cdsStart) && (axt->tEnd >= gp->cdsStart)) || ((axt->tStart <= gp->cdsEnd) && (axt->tEnd >= gp->cdsEnd)))
	 || (axt->tStart < gp->cdsEnd && axt->tEnd > gp->cdsStart)
	 )
	)
        {
        if (gp->strand[0] == '-')
            {
            reverseComplement(axt->qSym, axt->symCount);
            reverseComplement(axt->tSym, axt->symCount);
            tmp = hChromSize2(axt->qName) - axt->qStart;
            axt->qStart = hChromSize2(axt->qName) - axt->qEnd;
            axt->qEnd = tmp;
            if (prevEnd < (axt->tStart)-1)
                {
                axtGap = createAxtGap(nib,gp->chrom,prevEnd,(axt->tStart),gp->strand[0]);
                reverseComplement(axtGap->qSym, axtGap->symCount);
                reverseComplement(axtGap->tSym, axtGap->symCount);
                slAddHead(&axtList, axtGap);
                }
            }
        else if (prevEnd < (axt->tStart)-1)
            {
            axtGap = createAxtGap(nib,gp->chrom,prevEnd,(axt->tStart),gp->strand[0]);
            slAddHead(&axtList, axtGap);
            }
        slAddHead(&axtList, axt);
        prevEnd = axt->tEnd;
        prevStart = axt->tStart;
        }
    if (sameString(gp->chrom, axt->tName) && (axt->tStart > gp->txEnd)) 
        {
        if (prevEnd < axt->tStart) //&& gp->txEnd > axt->tStart)
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
int prevStart = gp->txEnd;
int tmp;

lf = lineFileOpen(getAxtFileName(gp->chrom, toDb, alignment, fromDb), TRUE);
printf("file %s\n",lf->fileName);
while ((axt = axtRead(lf)) != NULL)
    {
//    if (sameString(gp->chrom , axt->tName))
 //           printf("axt %s qstart %d axt tStart %d\n",axt->qName, axt->qStart,axt->tStart);
    if (sameString(gp->chrom , axt->tName) && 
         (sameString(qChrom, axt->qName) && positiveRangeIntersection(qStart, qEnd, axt->qStart, axt->qEnd) )&&
         positiveRangeIntersection(gp->txStart, gp->txEnd, axt->tStart, axt->tEnd) 
         /* (
         (((axt->tStart <= gp->cdsStart) && (axt->tEnd >= gp->cdsStart)) || ((axt->tStart <= gp->cdsEnd) && (axt->tEnd >= gp->cdsEnd)))
	 || (axt->tStart < gp->cdsEnd && axt->tEnd > gp->cdsStart)
	 ) */
	)
        {
        if (gp->strand[0] == '-')
            {
            reverseComplement(axt->qSym, axt->symCount);
            reverseComplement(axt->tSym, axt->symCount);
            tmp = hChromSize2(axt->qName) - axt->qStart;
            axt->qStart = hChromSize2(axt->qName) - axt->qEnd;
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
        prevStart = axt->tStart;
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
void showGenePos(char *name, struct trackDb *tdb)
/* Show gene prediction position and other info. */
{
char *track = tdb->tableName;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct genePred *gp = NULL;
boolean hasBin; 
int posCount = 0;
char table[64];

hFindSplitTable(seqName, track, table, &hasBin);
sprintf(query, "select * from %s where name = '%s'", table, name);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (posCount > 0)
        printf("<BR>\n");
    ++posCount;
    gp = genePredLoad(row + hasBin);
    printPos(gp->chrom, gp->txStart, gp->txEnd, gp->strand, FALSE);
    genePredFree(&gp);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void showGenePosMouse(char *name, struct trackDb *tdb, 
		      struct sqlConnection *connMm)
/* Show gene prediction position and other info. */
{
char query[512];
char *track = tdb->tableName;
struct sqlResult *sr;
char **row;
struct genePred *gp = NULL;
boolean hasBin; 
int posCount = 0;
char table[64] ;

hFindSplitTable(seqName, track, table, &hasBin);
sprintf(query, "select * from %s where name = '%s'", table, name);
sr = sqlGetResult(connMm, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (posCount > 0)
        printf("<BR>\n");
    ++posCount;
    gp = genePredLoad(row + hasBin);
    printPos(gp->chrom, gp->txStart, gp->txEnd, gp->strand, FALSE);
    genePredFree(&gp);
    }
sqlFreeResult(&sr);
}

void geneShowPosAndLinks(char *geneName, char *pepName, struct trackDb *tdb, 
			 char *pepTable, char *pepClick, 
			 char *mrnaClick, char *genomicClick, char *mrnaDescription)
/* Show parts of gene common to everything. If pepTable is not null,
 * it's the old table name, but will check gbSeq first. */
{
char *geneTable = tdb->tableName;
char other[256];

showGenePos(geneName, tdb);
printf("<H3>Links to sequence:</H3>\n");
printf("<UL>\n");

if ((pepTable != NULL) && hGenBankHaveSeq(pepName, pepTable))
    {
    puts("<LI>\n");
    hgcAnchorSomewhere(pepClick, pepName, pepTable, seqName);
    printf("Predicted Protein</A> \n"); 
    puts("</LI>\n");
    }

puts("<LI>\n");
hgcAnchorSomewhere(mrnaClick, geneName, geneTable, seqName);
printf("%s</A> may be different from the genomic sequence.\n", 
       mrnaDescription);
puts("</LI>\n");

puts("<LI>\n");
hgcAnchorSomewhere(genomicClick, geneName, geneTable, seqName);
printf("Genomic Sequence</A> from assembly\n");
puts("</LI>\n");

if (hTableExists("axtInfo"))
    {
    puts("<LI>\n");
    hgcAnchorGenePsl(geneName, geneTable, seqName, "startcodon");
    printf("Comparative Sequence</A> Annotated codons and translated protein with alignment to another species <BR>\n");
    puts("</LI>\n");
    }
printf("</UL>\n");
}

void geneShowPosAndLinksDNARefseq(char *geneName, char *pepName, struct trackDb *tdb,
				  char *pepTable, char *pepClick,
				  char *mrnaClick, char *genomicClick, char *mrnaDescription)
/* Show parts of a DNA based RefSeq gene */
{
char *geneTable = tdb->tableName;
char other[256];

showGenePos(geneName, tdb);
printf("<H3>Links to sequence:</H3>\n");
printf("<UL>\n");
puts("<LI>\n");
hgcAnchorSomewhere(genomicClick, geneName, geneTable, seqName);
printf("Genomic Sequence</A> from assembly\n");
puts("</LI>\n");

if (hTableExists("axtInfo"))
    {
    puts("<LI>\n");
    hgcAnchorGenePsl(geneName, geneTable, seqName, "startcodon");
    printf("Comparative Sequence</A> Annotated codons and translated protein with alignment to another species <BR>\n");
    puts("</LI>\n");
    }
printf("</UL>\n");
}

void geneShowPosAndLinksMouse(char *geneName, char *pepName, 
			      struct trackDb *tdb, char *pepTable, 
			      struct sqlConnection *connMm, char *pepClick, 
			      char *mrnaClick, char *genomicClick, char *mrnaDescription)
/* Show parts of gene common to everything */
{
char *geneTable = tdb->tableName;
char other[256];
showGenePosMouse(geneName, tdb, connMm);
printf("<H3>Links to sequence:</H3>\n");
printf("<UL>\n");
if (pepTable != NULL && hTableExists(pepTable))
    {
    hgcAnchorSomewhereDb(pepClick, pepName, pepTable, seqName, mousedb);
    printf("<LI>Translated Protein</A> \n"); 
    }
hgcAnchorSomewhereDb(mrnaClick, geneName, geneTable, seqName, mousedb);
printf("<LI>%s</A>\n", mrnaDescription);
hgcAnchorSomewhereDb(genomicClick, geneName, geneTable, seqName, mousedb);
printf("<LI>Genomic Sequence</A> DNA sequence from assembly\n");
printf("</UL>\n");
}

void geneShowCommon(char *geneName, struct trackDb *tdb, char *pepTable)
/* Show parts of gene common to everything */
{
geneShowPosAndLinks(geneName, geneName, tdb, pepTable, "htcTranslatedProtein",
		    "htcGeneMrna", "htcGeneInGenome", "Predicted mRNA");
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
geneShowCommon(item, tdb, pepTable);
}

void genericPslClick(struct sqlConnection *conn, struct trackDb *tdb, 
		     char *item, int start, char *subType)
/* Handle click in generic psl track. */
{
struct psl* pslList = getAlignments(conn, tdb->tableName, item);
struct psl* psl;

/* check if there is an alignment available for this sequence.  This checks
 * both genbank sequences and other sequences in the seq table.  If so,
 * set it up so they can click through to the alignment. */
if (hGenBankHaveSeq(item, NULL))
    {
    printf("<H3>%s/Genomic Alignments</H3>", item);
    printAlignments(pslList, start, "htcCdnaAli", tdb->tableName, item);
    }
else
    {
    /* just dump the psls */
    printf("<PRE><TT>\n");
    printf("#match\tmisMatches\trepMatches\tnCount\tqNumInsert\tqBaseInsert\ttNumInsert\tBaseInsert\tstrand\tqName\tqSize\tqStart\tqEnd\ttName\ttSize\ttStart\ttEnd\tblockCount\tblockSizes\tqStarts\ttStarts\n");
    for (psl = pslList; psl != NULL; psl = psl->next)
        {
        pslTabOut(psl, stdout);
        }
    printf("</TT></PRE>\n");
    }
pslFreeList(&pslList);
}

void printTrackHtml(struct trackDb *tdb)
/* If there's some html associated with track print it out. */
{
if (tdb->html != NULL && tdb->html[0] != 0)
    {
    htmlHorizontalLine();
    puts(tdb->html);
    }
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
char table[64];
char query[256];
struct sqlResult *sr;
char **row;
int rowOffset;
struct chain *chain;

if (!hFindSplitTableDb(db, seqName, track, table, &rowOffset))
    errAbort("No %s track in database %s for %s", track, db, seqName);
snprintf(query, sizeof(query), 
	 "select * from %s where id = %d", table, id);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL)
    errAbort("Can't find %d in %s", id, table);
chain = chainHeadLoad(row + rowOffset);
sqlFreeResult(&sr);
chainDbAddBlocks(chain, track, conn);
return chain;
}

void linkToOtherBrowser(char *otherDb, char *chrom, int start, int end)
/* Make anchor tag to open another browser window. */
{
printf("<A TARGET=\"_blank\" HREF=\"/cgi-bin/hgTracks?db=%s&position=%s%%3A%d-%d\">",
   otherDb, chrom, start+1, end);
}

void chainToOtherBrowser(struct chain *chain, char *otherDb, char *otherOrg)
/* Put up link that lets us use chain to browser on
 * corresponding window of other species. */
{
struct chain *subChain = NULL, *toFree = NULL;
int qs,qe;
chainSubsetOnT(chain, winStart, winEnd, &subChain, &toFree);
if (subChain != NULL)
    {
    qChainRangePlusStrand(subChain, &qs, &qe);
    linkToOtherBrowser(otherDb, subChain->qName, qs, qe);
    printf("Open %s browser</A> at position corresponding to the part of chain that is in this window.<BR>\n", otherOrg);
    }
chainFree(&toFree);
}

void genericChainClick(struct sqlConnection *conn, struct trackDb *tdb, 
		       char *item, int start, char *otherDb)
/* Handle click in chain track, at least the basics. */
{
char *track = tdb->tableName;
char *thisOrg = hOrganism(database);
char *otherOrg = NULL;
struct chain *chain = NULL, *subChain = NULL, *toFree = NULL;
int chainWinSize;
int qs, qe;

if (! sameWord(otherDb, "seq"))
    {
    otherOrg = hOrganism(otherDb);
    }

chain = chainDbLoad(conn, database, track, seqName, atoi(item));
printf("<B>%s position:</B> %s:%d-%d</a>  size: %d <BR>\n",
       thisOrg, chain->tName, chain->tStart+1, chain->tEnd, chain->tEnd-chain->tStart);
printf("<B>Strand:</B> %c<BR>\n", chain->qStrand);
qChainRangePlusStrand(chain, &qs, &qe);
if (sameWord(otherDb, "seq"))
    {
    printf("<B>%s position:</B> %s:%d-%d  size: %d<BR>\n",
	   chain->qName, chain->qName, qs, qe, chain->qEnd - chain->qStart);
    }
else
    {
    printf("<B>%s position:</B> <A target=\"_blank\" href=\"/cgi-bin/hgTracks?db=%s&position=%s%%3A%d-%d\">%s:%d-%d</A>  size: %d<BR>\n",
	   otherOrg, otherDb, chain->qName, qs, qe, chain->qName, 
	   qs, qe, chain->qEnd - chain->qStart);
    }
printf("<B>Chain ID:</B> %s<BR>\n", item);
printf("<B>Score:</B> %1.0f<BR>\n", chain->score);
printf("<BR>\n");

chainWinSize = min(winEnd-winStart, chain->tEnd - chain->tStart);
if (chainWinSize < 1000000)
    {
    hgcAnchorSomewhere("htcChainAli", item, track, chain->tName);
    printf("View details of parts of chain within browser window</A>.<BR>\n");
    }
else
    {
    printf("Zoom so that browser window covers 1,000,000 bases or less "
           "and return here to see alignment details.<BR>\n");
    }
if (! sameWord(otherDb, "seq"))
    {
    chainToOtherBrowser(chain, otherDb, otherOrg);
    }

chainFree(&chain);
}

char *trackTypeInfo(char *track)
/* Return type info on track. You can freeMem result when done. */
{
char *trackDb = hTrackDbName();
struct sqlConnection *conn = hAllocConn();
char buf[512];
char query[256];
snprintf(query, sizeof(query), 
	 "select type from %s where tableName = '%s'",  trackDb, track);
if (sqlQuickQuery(conn, query, buf, sizeof(buf)) == NULL)
    errAbort("%s isn't in %s", track, trackDb);
hFreeConn(&conn);
return cloneString(buf);
}

void findNib(char *db, char *chrom, char nibFile[512])
/* Find nib file corresponding to chromosome in given database. */
{
struct sqlConnection *conn = sqlConnect(db);
char query[256];

snprintf(query, sizeof(query),
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
return nibLoadPart(nibFile, start, end - start);
}

void genericNetClick(struct sqlConnection *conn, struct trackDb *tdb, 
		     char *item, int start, char *otherDb, char *chainTrack)
/* Generic click handler for net tracks. */
{
char table[64];
int rowOffset;
char query[256];
struct sqlResult *sr;
char **row;
struct netAlign *net;
char *org = hOrganism(database);
char *otherOrg = hOrganism(otherDb);
int tSize, qSize;
int netWinSize;
struct chain *chain;

hFindSplitTable(seqName, tdb->tableName, table, &rowOffset);
snprintf(query, sizeof(query), 
	 "select * from %s where tName = '%s' and tStart <= %d and tEnd > %d "
	 "and level = %s",
	 table, seqName, start, start, item);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find %s:%d in %s", seqName, start, table);
net = netAlignLoad(row+rowOffset);
sqlFreeResult(&sr);
tSize = net->tEnd - net->tStart;
qSize = net->qEnd - net->qStart;
if (net->chainId != 0)
    {
    netWinSize = min(winEnd-winStart, net->tEnd - net->tStart);
    printf("<BR>\n");
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
    chain = chainDbLoad(conn, database, chainTrack, seqName, net->chainId);
    if (chain != NULL)
        {
	chainToOtherBrowser(chain, otherDb, otherOrg);
	chainFree(&chain);
	}
    htmlHorizontalLine();
    }
printf("<B>Type:</B> %s<BR>\n", net->type);
printf("<B>Level:</B> %d<BR>\n", net->level/2 + 1);
printf("<B>%s position:</B> %s:%d-%d<BR>\n", 
       org, net->tName, net->tStart+1, net->tEnd);
printf("<B>%s position:</B> %s:%d-%d<BR>\n", 
       otherOrg, net->qName, net->qStart+1, net->qEnd);
printf("<B>Strand:</B> %c<BR>\n", net->strand[0]);
printf("<B>Score:</B> %1.1f<BR>\n", net->score);
if (net->chainId)
    {
    printf("<B>Chain ID:</B> %u<BR>\n", net->chainId);
    printf("<B>Bases aligning:</B> %u<BR>\n", net->ali);
    printf("<B>%s parent overlap:</B> %u<BR>\n", otherOrg, net->qOver);
    printf("<B>%s parent distance:</B> %u<BR>\n", otherOrg, net->qFar);
    printf("<B>%s bases duplicated:</B> %u<BR>\n", otherOrg, net->qDup);
    }
printf("<B>N's in %s:</B> %u (%1.1f%%)<BR>\n", org, net->tN, 100.0*net->tN/tSize);
printf("<B>N's in %s:</B> %u (%1.1f%%)<BR>\n", otherOrg, net->qN, 100.0*net->qN/qSize);
printf("<B>%s tandem repeat (trf) bases:</B> %u (%1.1f%%)<BR>\n", 
       org, net->tTrf, 100.0*net->tTrf/tSize);
printf("<B>%s tandem repeat (trf) bases:</B> %u (%1.1f%%)<BR>\n", 
       otherOrg, net->qTrf, 100.0*net->qTrf/qSize);
printf("<B>%s RepeatMasker bases:</B> %u (%1.1f%%)<BR>\n", 
       org, net->tR, 100.0*net->tR/tSize);
printf("<B>%s RepeatMasker bases:</B> %u (%1.1f%%)<BR>\n", 
       otherOrg, net->qR, 100.0*net->qR/qSize);
printf("<B>%s old repeat bases:</B> %u (%1.1f%%)<BR>\n", 
       org, net->tOldR, 100.0*net->tOldR/tSize);
printf("<B>%s old repeat bases:</B> %u (%1.1f%%)<BR>\n", 
       otherOrg, net->qOldR, 100.0*net->qOldR/qSize);
printf("<B>%s new repeat bases:</B> %u (%1.1f%%)<BR>\n", 
       org, net->tNewR, 100.0*net->tNewR/tSize);
printf("<B>%s new repeat bases:</B> %u (%1.1f%%)<BR>\n", 
       otherOrg, net->qNewR, 100.0*net->qNewR/qSize);
printf("<B>%s size:</B> %d<BR>\n", org, net->tEnd - net->tStart);
printf("<B>%s size:</B> %d<BR>\n", otherOrg, net->qEnd - net->qStart);
netAlignFree(&net);
}

void firstEF(struct trackDb *tdb, char *item)
{
char *dupe, *type, *words[16];
char title[256];
int wordCount;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn();
char table[64];
boolean hasBin;
struct bed *bed;
char query[512];
struct sqlResult *sr;
char **row;
boolean firstTime = TRUE;

//itemForUrl = item;
dupe = cloneString(tdb->type);
genericHeader(tdb, item);
wordCount = chopLine(dupe, words);
printCustomUrl(tdb, item, FALSE);
//printCustomUrl(tdb, itemForUrl, item == itemForUrl);

hFindSplitTable(seqName, tdb->tableName, table, &hasBin);
sprintf(query, "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d",
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
    printPos(bed->chrom, bed->chromStart, bed->chromEnd, NULL, TRUE);
    }
printTrackHtml(tdb);
freez(&dupe);
hFreeConn(&conn);
}

void genericClickHandler(struct trackDb *tdb, char *item, char *itemForUrl)
/* Put up generic track info. */
{
char *dupe, *type, *words[16];
char title[256];
int wordCount;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn();

if (itemForUrl == NULL)
    itemForUrl = item;
dupe = cloneString(tdb->type);
genericHeader(tdb, item);
wordCount = chopLine(dupe, words);
printCustomUrl(tdb, itemForUrl, item == itemForUrl);


if (wordCount > 0)
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
    else if (sameString(type, "sample"))
	{
	int num = 9;
        genericSampleClick(conn, tdb, item, start, num);
	}
    else if (sameString(type, "genePred"))
        {
	char *pepTable = NULL, *mrnaTable = NULL;
	if (wordCount > 1)
	    pepTable = words[1];
	if (wordCount > 2)
	    mrnaTable = words[2];
	genericGenePredClick(conn, tdb, item, start, pepTable, mrnaTable);
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
    else if (sameString(type, "chain"))
        {
	if (wordCount < 2)
	    errAbort("Missing field in chain track type field");
	genericChainClick(conn, tdb, item, start, words[1]);
	}
    else if (sameString(type, "maf"))
        {
	genericMafClick(conn, tdb, item, start);
	}
    else if (sameString(type, "axt"))
        {
	genericAxtClick(conn, tdb, item, start, words[1]);
	}
    else if (sameString(type, "expRatio"))
        {
	genericExpRatio(conn, tdb, item, start);
	}
    }
printTrackHtml(tdb);
freez(&dupe);
hFreeConn(&conn);
}

void savePosInTextBox(char *chrom, int start, int end)
/* Save basic position/database info in text box and hidden var. 
   Positions becomes chrom:start-end*/
{
char position[128];
snprintf(position, 128, "%s:%d-%d", chrom, start, end);
cgiMakeTextVar("getDnaPos", position, strlen(position) + 2);
cgiContinueHiddenVar("db");
}

void doGetDna1()
/* Do first get DNA dialog. */
{
struct hTableInfo *hti;
char *tbl = cgiUsualString("table", "");
char rootName[256];
char parsedChrom[32];
hParseTableName(tbl, rootName, parsedChrom);
hti = hFindTableInfo(seqName, rootName);
cartWebStart(cart, "Get DNA in Window");
printf("<H2>Get DNA for </H2>\n");
printf("<FORM ACTION=\"%s\">\n\n", hgcPath());
cartSaveSession(cart);
cgiMakeHiddenVar("g", "htcGetDna2");
cgiMakeHiddenVar("table", tbl);
cgiContinueHiddenVar("o");
cgiContinueHiddenVar("t");
cgiContinueHiddenVar("l");
cgiContinueHiddenVar("r");
puts("Position ");
savePosInTextBox(seqName, winStart+1, winEnd);

if (tbl[0] == 0)
    {
    puts("<P>"
	 "Note: if you would prefer to get DNA for features of a particular "
	 "track or table, try the ");
    printf("<A HREF=\"%s?%s&db=%s&position=%s:%d-%d\" TARGET=_BLANK>",
	   hgTextName(), cartSidUrlString(cart), database,
	   seqName, winStart+1, winEnd);
    puts("Table Browser</A>: select a positional table, "
	 "perform an Advanced Query and select FASTA as the output format.");
    }
else
    {
    char hgTextTbl[128];
    snprintf(hgTextTbl, sizeof(hgTextTbl), "chr1_%s", tbl);
    if (hTableExists(hgTextTbl))
	snprintf(hgTextTbl, sizeof(hgTextTbl), "%s.chrN_%s", database, tbl);
    else
	snprintf(hgTextTbl, sizeof(hgTextTbl), "%s.%s", database, tbl);
    puts("<P>"
	 "Note: if you would prefer to get DNA for more than one feature of "
	 "this track at a time, try the ");
    printf("<A HREF=\"%s?%s&db=%s&position=%s:%d-%d&table0=%s&phase=table\" TARGET=_BLANK>",
	   hgTextName(), cartSidUrlString(cart), database,
	   seqName, winStart+1, winEnd, hgTextTbl);
    puts("Table Browser</A>: "
	 "perform an Advanced Query and select FASTA as the output format.");
    }

hgSeqOptionsHti(hti);
puts("<P>");
cgiMakeButton("submit", "Get DNA");
cgiMakeButton("submit", "Extended case/color options");
puts("</FORM><P>");
puts("Note: The \"Mask repeats\" option applies only to \"Get DNA\", not to \"Extended case/color options\". <P>");
}

void maskRepeats(char *chrom, int chromStart, int chromEnd, DNA *dna, int offset, boolean soft)
/* Lower case bits corresponding to repeats. */
{
int rowOffset;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;

sr = hRangeQuery(conn, "rmsk", chrom, chromStart, chromEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct rmskOut ro;
    rmskOutStaticLoad(row+rowOffset, &ro);
    if (ro.genoEnd > chromEnd) ro.genoEnd = chromEnd;
    if (ro.genoStart < chromStart) ro.genoStart = chromStart;
    if (soft)
	toLowerN(dna+ro.genoStart-offset, ro.genoEnd - ro.genoStart);
    else
        memset(dna+ro.genoStart-offset, 'n', ro.genoEnd - ro.genoStart);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
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
    theCtList = customTracksParseCart(cart, NULL, NULL);
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
    tdb->tableName = ct->tdb->tableName;
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


struct customTrack *lookupCt(char *name)
/* Return custom track for name, or NULL. */
{
struct customTrack *ct;

for (ct=getCtList();  ct != NULL;  ct=ct->next)
    if (sameString(name, ct->tdb->tableName))
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
    tdb->tableName = cloneString("hgUserPsl");
    tdb->shortLabel = cloneString("BLAT Sequence");
    tdb->type = cloneString("psl");
    tdb->longLabel = cloneString("Your Sequence from BLAT Search");
    tdb->visibility = tvFull;
    tdb->priority = 11.0;
    trackDbPolish(tdb);
    return(tdb);
    }
}

void doGetDnaExtended1()
/* Do extended case/color get DNA options. */
{
struct trackDb *tdbList = hTrackDb(seqName), *tdb;
struct trackDb *ctdbList = tdbForCustomTracks();
struct trackDb *utdbList = tdbForUserPsl();
boolean isRc     = cartUsualBoolean(cart, "hgc.dna.rc", FALSE);
boolean revComp  = cgiBoolean("hgSeq.revComp");
boolean maskRep  = cgiBoolean("hgSeq.maskRepeats");
int padding5     = cgiOptionalInt("hgSeq.padding5", 0);
int padding3     = cgiOptionalInt("hgSeq.padding3", 0);
char *casing     = cgiUsualString("hgSeq.casing", "");
char *repMasking = cgiUsualString("hgSeq.repMasking", "");
boolean caseUpper= FALSE;
char *pos = NULL;

    
ctdbList = slCat(ctdbList, tdbList);
tdbList = slCat(utdbList, ctdbList);

cartWebStart(cart, "Extended DNA Case/Color");

if (NULL != (pos = cartOptionalString(cart, "getDnaPos")))
    hgParseChromRange(pos, &seqName, &winStart, &winEnd);
if (winEnd - winStart > 1000000)
    {
    printf("Please zoom in to 1 million bases or less to color the DNA");
    return;
    }

printf("<H1>Extended DNA Case/Color Options</H1>\n");
puts(
     "Use this page to highlight features in genomic DNA text. "
     "DNA covered by a particular track can be hilighted by "
     "case, underline, bold, italic, or color.  See below for "
     "details about color, and for examples. <P>");

if (cgiBooleanDefined("hgSeq.revComp"))
    {
    isRc = revComp;
    // don't set revComp in cart -- it shouldn't be a default.
    }
if (cgiBooleanDefined("hgSeq.maskRepeats"))
    cartSetBoolean(cart, "hgSeq.maskRepeats", revComp);
if (*repMasking != 0)
    cartSetString(cart, "hgSeq.repMasking", repMasking);
if (maskRep)
    {
    struct trackDb *rtdb;
    char *visString = cartOptionalString(cart, "rmsk");
    for (rtdb = tdbList;  rtdb != NULL;  rtdb=rtdb->next)
	{
	if (sameString(rtdb->tableName, "rmsk"))
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

printf("<FORM ACTION=\"%s\" METHOD=\"POST\">\n\n", hgcPath());
cartSaveSession(cart);
cgiMakeHiddenVar("g", "htcGetDna3");

if (NULL != (pos = cartOptionalString(cart, "getDnaPos")))
    {
    hgParseChromRange(pos, &seqName, &winStart, &winEnd);
    }
puts("Position ");
savePosInTextBox(seqName, winStart+1 - (revComp ? padding3 : padding5), winEnd + (revComp ? padding5 : padding3));
printf(" Reverse complement ");
cgiMakeCheckBox("hgc.dna.rc", isRc);
printf("<BR>\n");
printf("Letters per line ");
cgiMakeIntVar("lineWidth", 60, 3);
printf(" Default case: ");
cgiMakeRadioButton("case", "upper", caseUpper);
printf(" Upper ");
cgiMakeRadioButton("case", "lower", !caseUpper);
printf(" Lower ");
cgiMakeButton("Submit", "Submit");
printf("<BR>\n");
printf("<TABLE BORDER=1>\n");
printf("<TR><TD>Track<BR>Name</TD><TD>Toggle<BR>Case</TD><TD>Under-<BR>line</TD><TD>Bold</TD><TD>Italic</TD><TD>Red</TD><TD>Green</TD><TD>Blue</TD></TR>\n");
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    char *track = tdb->tableName;
    if (sameString("hgUserPsl", track) ||
	(lookupCt(track) != NULL) ||
	(fbUnderstandTrack(track) && !dnaIgnoreTrack(track)))
	{
	char *visString = cartOptionalString(cart, track);
	char buf[128];
	if (visString != NULL && sameString(visString, "hide"))
	    {
	    char varName[256];
	    sprintf(varName, "%s_case", track);
	    cartSetBoolean(cart, varName, FALSE);
	    sprintf(varName, "%s_u", track);
	    cartSetBoolean(cart, varName, FALSE);
	    sprintf(varName, "%s_b", track);
	    cartSetBoolean(cart, varName, FALSE);
	    sprintf(varName, "%s_i", track);
	    cartSetBoolean(cart, varName, FALSE);
	    sprintf(varName, "%s_red", track);
	    cartSetInt(cart, varName, 0);
	    sprintf(varName, "%s_green", track);
	    cartSetInt(cart, varName, 0);
	    sprintf(varName, "%s_blue", track);
	    cartSetInt(cart, varName, 0);
	    }
	else
	    {
	    printf("<TR>");
	    printf("<TD>%s</TD>", tdb->shortLabel);
	    sprintf(buf, "%s_case", tdb->tableName);
	    printf("<TD>");
	    cgiMakeCheckBox(buf, cartUsualBoolean(cart, buf, FALSE));
	    printf("</TD>");
	    sprintf(buf, "%s_u", tdb->tableName);
	    printf("<TD>");
	    cgiMakeCheckBox(buf, cartUsualBoolean(cart, buf, FALSE));
	    printf("</TD>");
	    sprintf(buf, "%s_b", tdb->tableName);
	    printf("<TD>");
	    cgiMakeCheckBox(buf, cartUsualBoolean(cart, buf, FALSE));
	    printf("</TD>");
	    sprintf(buf, "%s_i", tdb->tableName);
	    printf("<TD>");
	    cgiMakeCheckBox(buf, cartUsualBoolean(cart, buf, FALSE));
	    printf("</TD>");
	    printf("<TD>");
	    sprintf(buf, "%s_red", tdb->tableName);
	    cgiMakeIntVar(buf, cartUsualInt(cart, buf, 0), 3);
	    printf("</TD>");
	    printf("<TD>");
	    sprintf(buf, "%s_green", tdb->tableName);
	    cgiMakeIntVar(buf, cartUsualInt(cart, buf, 0), 3);
	    printf("</TD>");
	    printf("<TD>");
	    sprintf(buf, "%s_blue", tdb->tableName);
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
puts("The examples below show a few ways to hilight individual tracks, "
     "and their interplay. It's good to keep it simple at first.  It's easy "
     "to make pretty but completely cryptic displays with this feature.");
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
     "green, while regions covered with more ESTs get progressively brighter -- "
     "saturating at 4 ESTs."
     "<LI>Another track can be used to mask unwanted features. Setting the "
     "RepeatMasker track to RGB (255,255,255) will white-out Genscan predictions "
     "of LINEs but not mainstream host genes; masking with RefSeq Genes will show "
     "what is new in the gene prediction sector."
     "</UL>");
puts("<H3>Further Details and Ideas</H3>");
puts("<P>Copying and pasting the web page output to a text editor such as Word "
     "will retain upper case but lose colors and other formatting. That's still "
     "useful because other web tools such as "
     "<A HREF=\"http://www.ncbi.nlm.nih.gov/blast/index.nojs.cgi\">NCBI Blast</A> "
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
     "other software can safely display.  The tool will format 10Mbp and more though.</P>");
trackDbFreeList(&tdbList);
}

void doGetDna2()
/* Do second DNA dialog (or just fetch DNA) */
{
char *tbl = cgiUsualString("table", "");
char *action = cgiUsualString("submit", "");
int itemCount;

if (sameString(action, "Extended case/color options"))
    {
    doGetDnaExtended1();
    return;
    }


puts("<PRE>");
if (tbl[0] == 0)
    {
    char *pos = NULL;
    char *chrom = NULL;
    int start = 0;
    int end = 0;

    itemCount = 1;
    if ( NULL != (pos = cartOptionalString(cart, "getDnaPos")) &&
         hgParseChromRange(pos, &chrom, &start, &end))
        {
        hgSeqRange(chrom, start, end, '?', "dna");
        }
    else
        {        
        hgSeqRange(seqName, cartInt(cart, "l"), cartInt(cart, "r"),
                   '?', "dna");
        }
    }
else
    {
    char rootName[256];
    char parsedChrom[32];
    /* Table might be a custom track; if it's not in the database, 
     * just get DNA as if no table were given. */
    hParseTableName(tbl, rootName, parsedChrom);
    if (hFindTableInfo(seqName, rootName) == NULL)
	{
	itemCount = 1;
	hgSeqRange(seqName, cartInt(cart, "o"), cartInt(cart, "t"),
		   '?', tbl);
	}
    else
	itemCount = hgSeqItemsInRange(tbl, seqName, cartInt(cart, "o"),
				      cartInt(cart, "t"), NULL);
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
hti->rootName = cloneString(ct->tdb->tableName);
hti->isPos = TRUE;
hti->isSplit = FALSE;
hti->hasBin = FALSE;
hti->type = cloneString(ct->tdb->type);
if (ct->fieldCount >= 3)
    {
    strncpy(hti->chromField, "chrom", 32);
    strncpy(hti->startField, "chromStart", 32);
    strncpy(hti->endField, "chromEnd", 32);
    }
if (ct->fieldCount >= 4)
    {
    strncpy(hti->nameField, "name", 32);
    }
if (ct->fieldCount >= 5)
    {
    strncpy(hti->scoreField, "score", 32);
    }
if (ct->fieldCount >= 6)
    {
    strncpy(hti->strandField, "strand", 32);
    }
if (ct->fieldCount >= 8)
    {
    strncpy(hti->cdsStartField, "thickStart", 32);
    strncpy(hti->cdsEndField, "thickEnd", 32);
    hti->hasCDS = TRUE;
    }
if (ct->fieldCount >= 12)
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
hti->rootName = cloneString("hgUserPsl");
hti->isPos = TRUE;
hti->isSplit = FALSE;
hti->hasBin = FALSE;
hti->type = cloneString("psl");
strncpy(hti->chromField, "tName", 32);
strncpy(hti->startField, "tStart", 32);
strncpy(hti->endField, "tEnd", 32);
strncpy(hti->nameField, "qName", 32);
// psl can be scored... but strictly speaking, does not have a score field!
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
		// If query is protein, blockSizes are in aa units; fix 'em.
		bed->blockSizes[i] *= 3;
		}
	if (psl->strand[1] == '-')
	    {
	    // psl: if target strand is '-', flip the coords.
	    // (this is the target part of pslRcBoth from src/lib/psl.c)
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
	// translate absolute starts to relative starts (after handling 
	// target-strand coord-flipping)
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

sprintf(buf, "%s_%s", track, type);
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

void doGetDna3()
/* Fetch DNA in extended color format */
{
struct dnaSeq *seq;
struct cfm *cfm;
int i;
boolean isRc = cgiBoolean("hgc.dna.rc");
boolean defaultUpper = sameString(cartString(cart, "case"), "upper");
int winSize;
int lineWidth = cartInt(cart, "lineWidth");
struct rgbColor *colors;
struct trackDb *tdbList = hTrackDb(seqName), *tdb;
struct trackDb *ctdbList = tdbForCustomTracks();
struct trackDb *utdbList = tdbForUserPsl();
char *pos = NULL;
Bits *uBits;	/* Underline bits. */
Bits *iBits;    /* Italic bits. */
Bits *bBits;    /* Bold bits. */

if (NULL != (pos = cartOptionalString(cart, "getDnaPos")))
    hgParseChromRange(pos, &seqName, &winStart, &winEnd);

winSize = winEnd - winStart;
uBits = bitAlloc(winSize);	/* Underline bits. */
iBits = bitAlloc(winSize);	/* Italic bits. */
bBits = bitAlloc(winSize);	/* Bold bits. */

ctdbList = slCat(ctdbList, tdbList);
tdbList = slCat(utdbList, ctdbList);

cartWebStart(cart, "Extended DNA Output");
printf("<PRE><TT>");
printf(">%s:%d-%d %s\n", seqName, winStart+1, winEnd,
       (isRc ? "(reverse complement)" : ""));
seq = hDnaFromSeq(seqName, winStart, winEnd, dnaLower);
if (isRc)
    reverseComplement(seq->dna, seq->size);
if (defaultUpper)
    touppers(seq->dna);

AllocArray(colors, winSize);
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    char *track = tdb->tableName;
    struct featureBits *fbList = NULL, *fb;
    struct customTrack *ct = lookupCt(track);
    if (sameString("hgUserPsl", track) ||
	(ct != NULL) ||
	(fbUnderstandTrack(track) && !dnaIgnoreTrack(track)))
        {
	char buf[256];
	int r,g,b;

	if (sameString("hgUserPsl", track))
	    {
	    struct hTableInfo *hti = htiForUserPsl();
	    struct bedFilter *bf;
	    struct bed *bedList, *bedList2;
	    AllocVar(bf);
	    bedList = bedFromUserPsl();
	    bedList2 = bedFilterListInRange(bedList, bf, seqName, winStart,
					    winEnd);
	    fbList = fbFromBed(track, hti, bedList2, winStart, winEnd,
			       TRUE, FALSE);
	    bedFreeList(&bedList);
	    bedFreeList(&bedList2);
	    }
	else if (ct != NULL)
	    {
	    struct hTableInfo *hti = ctToHti(ct);
	    struct bedFilter *bf;
	    struct bed *bedList2;
	    AllocVar(bf);
	    bedList2 = bedFilterListInRange(ct->bedList, bf, seqName, winStart,
					    winEnd);
	    fbList = fbFromBed(track, hti, bedList2, winStart, winEnd,
			       TRUE, FALSE);
	    bedFreeList(&bedList2);
	    }
	else
	    fbList = fbGetRange(track, seqName, winStart, winEnd);

	/* Flip underline/italic/bold bits. */
	getDnaHandleBits(track, "u", uBits, winStart, winEnd, isRc, fbList);
	getDnaHandleBits(track, "b", bBits, winStart, winEnd, isRc, fbList);
	getDnaHandleBits(track, "i", iBits, winStart, winEnd, isRc, fbList);

	/* Toggle case if necessary. */
	sprintf(buf, "%s_case", track);
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
	sprintf(buf, "%s_red", track);
	r = cartInt(cart, buf);
	sprintf(buf, "%s_green", track);
	g = cartInt(cart, buf);
	sprintf(buf, "%s_blue", track);
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

void appendAuthor(struct dyString *dy, char *gbAuthor, int len)
/* Convert from  Kent,W.J. to Kent WJ and append to dy.
 * gbAuthor gets eaten in the process. 
 * Also strip web URLs since Entrez doesn't like those. */
{
char buf[2048];
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
char buf[256];
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

void printGeneLynxName(char *search)
/* Print link to GeneLynx search using gene name (WNT2, CFTR etc) */
{
printf("<B>GeneLynx</B> ");
printf("<A HREF=\"http://www.genelynx.org/cgi-bin/linklist?tableitem=GLID_NAME.name&IDlist=%s&dir=1\" TARGET=_blank>", search);
printf("%s</A><BR>\n", search);
}

void printGeneLynxAcc(char *search)
/* Print link to GeneLynx search using accession (X07876, BC001451 etc) */
{
printf("<B>GeneLynx</B> ");
printf("<A HREF=\"http://www.genelynx.org/cgi-bin/linklist?tableitem=GLYNX_INDEX.word&IDlist=%s&dir=1\" TARGET=_blank>", search);
printf("%s</A><BR>\n", search);
}

/* --- !!! Riken code is under development Fan. 4/16/02 */
void printRikenInfo(char *acc, struct sqlConnection *conn )
/* Print Riken annotation info */
{
struct sqlResult *sr;
char **row;
char qry[512];
char *seqid, *fantomid, *cloneid, *modified_time, *accession, *comment;
char *qualifier, *anntext, *datasrc, *srckey, *href, *evidence;   

accession = acc;
snprintf(qry, sizeof(qry), 
	 "select seqid from rikenaltid where altid='%s';", accession);
sr = sqlMustGetResult(conn, qry);
row = sqlNextRow(sr);

if (row != NULL)
    {
    seqid=strdup(row[0]);

    snprintf(qry, sizeof(qry), 
	     "select Qualifier, Anntext, Datasrc, Srckey, Href, Evidence "
	     "from rikenann where seqid='%s';", seqid);

    sqlFreeResult(&sr);
    sr = sqlMustGetResult(conn, qry);
    row = sqlNextRow(sr);

    while (row !=NULL)
	{
	qualifier = row[0];
	anntext   = row[1];
	datasrc   = row[2];
	srckey    = row[3];
	href      = row[4];
	evidence  = row[5];
	row = sqlNextRow(sr);		
	}

    snprintf(qry, sizeof(qry), 
	     "select comment from rikenseq where id='%s';", seqid);
    sqlFreeResult(&sr);
    sr = sqlMustGetResult(conn, qry);
    row = sqlNextRow(sr);

    if (row != NULL)
	{
	comment = row[0];
	printf("<B>Riken/comment:</B> %s<BR>\n",comment);
	}
    }  
}

void printStanSource(char *acc, char *type)
/* Print out a link to Stanford's SOURCE web resource. 
   Types known are: est,mrna,unigene,locusLink. */
{
char *stanSourceLink = "http://genome-www5.stanford.edu/cgi-bin/SMD/source/sourceResult?"; 
if(sameWord(type, "est"))
    {
    printf("<B>Stanford SOURCE:</B> %s <A HREF=\"%soption=Number&criteria=%s&choice=Gene\" TARGET=_blank>[Gene Info]</A> ",acc,stanSourceLink,acc);
    printf("<A HREF=\"%soption=Number&criteria=%s&choice=cDNA\" TARGET=_blank>[Clone Info]</A><BR>\n",stanSourceLink,acc);
    }
else if(sameWord(type,"unigene"))
    {
    printf("<B>Stanford SOURCE:</B> %s <A HREF=\"%soption=CLUSTER&criteria=%s&choice=Gene\" TARGET=_blank>[Gene Info]</A> ",acc,stanSourceLink,acc);
    printf("<A HREF=\"%soption=CLUSTER&criteria=%s&choice=cDNA\" TARGET=_blank>[Clone Info]</A><BR>\n",stanSourceLink,acc);
    }
else if(sameWord(type,"mrna"))
    printf("<B>Stanford SOURCE:</B> <A HREF=\"%soption=Number&criteria=%s&choice=Gene\" TARGET=_blank>%s</A><BR>\n",stanSourceLink,acc,acc);
else if(sameWord(type,"locusLink"))
    printf("<B>Stanford SOURCE Locus Link:</B> <A HREF=\"%soption=LLID&criteria=%s&choice=Gene\" TARGET=_blank>%s</A><BR>\n",stanSourceLink,acc,acc);
}

int getImageId(struct sqlConnection *conn, char *acc)
/* get the image id for a clone, or 0 if none */
{
int imageId = 0;
if (sqlTableExists(conn, "imageClone"))
    {
    struct sqlResult *sr;
    char **row;
    char query[128];
    safef(query, sizeof(query),
          "select imageId from imageClone where acc = '%s'", acc);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        imageId = sqlUnsigned(row[0]);
    sqlFreeResult(&sr);
    }
return imageId;
}

static char *mgcStatusDesc[][2] =
/* Table of MGC status codes to descriptions.  This is essentially duplicated
 * from MGC loading code, but we don't share the data to avoid creating some
 * dependencies.  Might want to move these to a shared file or just put these
 * in a table.  Although it is nice to be able to customize for the browser. */
{
    {"unpicked", "not picked"},
    {"picked", "picked"},
    {"notBack", "not back"},
    {"noDecision", "no decision yet"},
    {"fullLength", "full length"},
    {"fullLengthShort", "full length (short isoform)"},
    {"incomplete", "incomplete"},
    {"chimeric", "chimeric"},
    {"frameShift", "frame shifted"},
    {"contaminated", "contaminated"},
    {"retainedIntron", "retained intron"},
    {"mixedWells", "mixed wells"},
    {"noGrowth", "no growth"},
    {"noInsert", "no insert"},
    {"no5est", "no 5' EST match"},
    {"microDel", "no cloning site / microdeletion"},
    {"artifact", "library artifacts"},
    {"noPolyATail", "no polyA-tail"},
    {"cantSequence", "unable to sequence"},
    {NULL, NULL}
};

char *getMgcStatusDesc(struct sqlConnection *conn, int imageId)
/* get a description of the status of a MGC clone */
{
struct sqlResult *sr;
char **row;
char query[128];
char *desc = NULL;

/* mgcFullStatus is used on browsers with only the full-length (mgc genes)
 * track */
if (sqlTableExists(conn, "mgcStatus"))
    safef(query, sizeof(query),
          "SELECT status FROM mgcStatus WHERE (imageId = %d)", imageId);
else
    safef(query, sizeof(query),
          "SELECT status FROM mgcFullStatus WHERE (imageId = %d)", imageId);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL)
    desc = "no MGC status available; please report to browser maintainers";
else
    {
    int i;
    for (i = 0; (mgcStatusDesc[i][0] != NULL) && (desc == NULL); i++)
        {
        if (sameString(row[0], mgcStatusDesc[i][0]))
            desc = mgcStatusDesc[i][1];
        }
    if (desc == NULL)
        desc = "unknown MGC status; please report to browser maintainers";
    }

sqlFreeResult(&sr);
return desc;
}                   

void printMgcRnaSpecs(struct trackDb *tdb, char *acc, int imageId)
/* print status information for MGC mRNA or EST; must have imageId */
{
struct sqlConnection *conn = hgAllocConn();
char *mgcOrganism, *statusDesc;

/* link to MGC site only for full-length mRNAs */
if (sameString(tdb->tableName, "mgcGenes"))
    {
    if (startsWith("hg", database))
        mgcOrganism = "Hs";
    else if (startsWith("mm", database))
        mgcOrganism = "Mm";
    else
        errAbort("can't map database \"%s\" to a MGC organism", database);
    printf("<B>MGC clone information:</B> ");
    printf("<A href=\"http://mgc.nci.nih.gov/Reagents/CloneInfo?ORG=%s&IMAGE=%d\" TARGET=_blank>IMAGE:%d</A><BR>", 
           mgcOrganism, imageId, imageId);
    }

/* add status description */
statusDesc = getMgcStatusDesc(conn, imageId);
if (statusDesc != NULL)
    printf("<B>MGC status:</B> %s<BR>", statusDesc);
hFreeConn(&conn);
}

void printRnaSpecs(struct trackDb *tdb, char *acc)
/* Print auxiliarry info on RNA. */
{
struct dyString *dy = newDyString(1024);
struct sqlConnection *conn = hgAllocConn();
struct sqlResult *sr;
char **row;
char *type,*direction,*source,*orgFullName,*library,*clone,*sex,*tissue,
    *development,*cell,*cds,*description, *author,*geneName,
    *date,*productName;
boolean isMgcTrack = startsWith("mgc", tdb->tableName);
int imageId = (isMgcTrack ? getImageId(conn, acc) : 0);
int seqSize,fileSize;
long fileOffset;
char *ext_file;	
boolean hasVersion = hHasField("mrna", "version");
boolean haveGbSeq = sqlTableExists(conn, "gbSeq");
char *seqTbl = haveGbSeq ? "gbSeq" : "seq";
char *version = NULL;


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
dyStringAppend(dy,
               "select mrna.type,mrna.direction,"
               "source.name,organism.name,library.name,mrnaClone.name,"
               "sex.name,tissue.name,development.name,cell.name,cds.name,"
               "description.name,author.name,geneName.name,productName.name,");
if (haveGbSeq)
    dyStringAppend(dy,
                   "gbSeq.size,mrna.moddate,gbSeq.gbExtFile,gbSeq.file_offset,gbSeq.file_size ");
else
    dyStringAppend(dy,
		   "seq.size,seq.gb_date,seq.extFile,seq.file_offset,seq.file_size ");

/* If the mrna table has a "version" column then will show it */
if (hasVersion) 
    {
    dyStringAppend(dy,
                   ", mrna.version ");    
    } 

dyStringPrintf(dy,
               " from mrna,%s,source,organism,library,mrnaClone,sex,tissue,"
               "development,cell,cds,description,author,geneName,productName "
               " where mrna.acc = '%s' and mrna.id = %s.id ",
               seqTbl, acc, seqTbl);
dyStringAppend(dy,
               "and mrna.source = source.id and mrna.organism = organism.id "
               "and mrna.library = library.id and mrna.mrnaClone = mrnaClone.id "
               "and mrna.sex = sex.id and mrna.tissue = tissue.id "
               "and mrna.development = development.id and mrna.cell = cell.id "
               "and mrna.cds = cds.id and mrna.description = description.id "
               "and mrna.author = author.id and mrna.geneName = geneName.id "
               "and mrna.productName = productName.id");

sr = sqlMustGetResult(conn, dy->string);
row = sqlNextRow(sr);
if (row != NULL)
    {
    type=row[0];direction=row[1];source=row[2];orgFullName=row[3];library=row[4];clone=row[5];
    sex=row[6];tissue=row[7];development=row[8];cell=row[9];cds=row[10];description=row[11];
    author=row[12];geneName=row[13];productName=row[14];
    seqSize = sqlUnsigned(row[15]);
    date = row[16];
    ext_file = row[17];
    fileOffset=sqlUnsigned(row[18]);
    fileSize=sqlUnsigned(row[19]);

    if (hasVersion) 
        {
        version = row[20];
        }


    /* Now we have all the info out of the database and into nicely named
     * local variables.  There's still a few hoops to jump through to 
     * format this prettily on the web with hyperlinks to NCBI. */
    printf("<H2>Information on %s%s <A HREF=\"", 
           (isMgcTrack ? "MGC " : ""), type);
    printEntrezNucleotideUrl(stdout, acc);
    printf("\" TARGET=_blank>%s</A></H2>\n", acc);

    if (isMgcTrack && (imageId > 0))
        printMgcRnaSpecs(tdb, acc, imageId);
    printf("<B>Description:</B> %s<BR>\n", description);

    medlineLinkedLine("Gene", geneName, geneName);
    medlineLinkedLine("Product", productName, productName);
    dyStringClear(dy);
    gbToEntrezAuthor(author, dy);
    medlineLinkedLine("Author", author, dy->string);
    printf("<B>Organism:</B> ");
    printf("<A href=\"http://www.ncbi.nlm.nih.gov/Taxonomy/Browser/wwwtax.cgi?mode=Undef&name=%s&lvl=0&srchmode=1\" TARGET=_blank>", 
	   cgiEncode(orgFullName));
    printf("%s</A><BR>\n", orgFullName);
    printf("<B>Tissue:</B> %s<BR>\n", tissue);
    printf("<B>Development stage:</B> %s<BR>\n", development);
    printf("<B>Cell type:</B> %s<BR>\n", cell);
    printf("<B>Sex:</B> %s<BR>\n", sex);
    printf("<B>Library:</B> %s<BR>\n", library);
    printf("<B>Clone:</B> %s<BR>\n", clone);
    if (direction[0] != '0') printf("<B>Read direction:</B> %s'<BR>\n", direction);
    printf("<B>CDS:</B> %s<BR>\n", cds);
    printf("<B>Date:</B> %s<BR>\n", date);
    if (hasVersion) 
        {
        printf("<B>Version:</B> %s<BR>\n", version);
        }

    if (!startsWith("Worm", organism) && !startsWith("Fugu", organism))
    {
	/* Put up Gene Lynx */
	if (sameWord(type, "mrna"))
	    printGeneLynxAcc(acc);
    
	/* Put up Stanford Source link. */
	printStanSource(acc, type);
    }

    if ((strstr(hgGetDb(), "mm") != NULL) 
        && hTableExists("rikenaltid"))
	{
        sqlFreeResult(&sr);
//!	printRikenInfo(acc, conn);
	}

    }
else
    {
    warn("Couldn't find %s in mrna table", acc);
    }

sqlFreeResult(&sr);
freeDyString(&dy);
hgFreeConn(&conn);
}


void printAlignments(struct psl *pslList, 
		     int startFirst, char *hgcCommand, char *typeName, char *itemIn)
/* Print list of mRNA alignments. */
{
struct psl *psl;
int aliCount = slCount(pslList);
boolean same;
char otherString[512];

if (aliCount > 1)
    printf("The alignment you clicked on is first in the table below.<BR>\n");

printf("<PRE><TT>");
printf(" SIZE IDENTITY CHROMOSOME  STRAND    START     END              QUERY      START  END  TOTAL\n");
printf("--------------------------------------------------------------------------------------------\n");
for (same = 1; same >= 0; same -= 1)
    {
    for (psl = pslList; psl != NULL; psl = psl->next)
	{
	if (same ^ (psl->tStart != startFirst))
	    {
	    sprintf(otherString, "%d&aliTrack=%s", psl->tStart, typeName);
	    hgcAnchorSomewhere(hgcCommand, itemIn, otherString, psl->tName);
	    printf("%5d  %5.1f%%  %9s     %s %9d %9d  %20s %5d %5d %5d</A>",
		   psl->match + psl->misMatch + psl->repMatch,
		   100.0 - pslCalcMilliBad(psl, TRUE) * 0.1,
		   skipChr(psl->tName), psl->strand, psl->tStart + 1, psl->tEnd,
		   psl->qName, psl->qStart+1, psl->qEnd, psl->qSize);
	    printf("\n");
	    }
	}
    }
printf("</TT></PRE>");
}

struct psl *getAlignments(struct sqlConnection *conn, char *table, char *acc)
/* get the list of alignments for the specified acc */
{
struct sqlResult *sr = NULL;
char **row;
struct psl *psl, *pslList = NULL;
boolean hasBin;
char splitTable[64];
char query[256];
hFindSplitTable(seqName, table, splitTable, &hasBin);
safef(query, sizeof(query), "select * from %s where qName = '%s'", splitTable, acc);
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

struct psl *loadPslRangeT(char *table, char *qName, char *tName, int tStart, int tEnd)
/* Load a list of psls given qName tName tStart tEnd */
{
struct sqlResult *sr = NULL;
char **row;
struct psl *psl = NULL, *pslList = NULL;
boolean hasBin;
char splitTable[64];
char query[256];
struct sqlConnection *conn = hAllocConn();

hFindSplitTable(seqName, table, splitTable, &hasBin);
safef(query, sizeof(query), "select * from %s where qName = '%s' and tName = '%s' and tEnd > %d and tStart < %d", splitTable, qName, tName, tStart, tEnd);
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
char *track = tdb->tableName;
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
char *type;
char *table;
int start = cartInt(cart, "o");
struct psl *pslList = NULL;

if (sameString("xenoMrna", track) || sameString("xenoBestMrna", track) || sameString("xenoEst", track) || sameString("sim4", track) )
    {
    char temp[256];
    sprintf(temp, "non-%s RNA", organism);
    type = temp;
    table = track;
    }
else if (stringIn("est", track) || stringIn("Est", track) ||
         (stringIn("mgc", track) && stringIn("Picks", track)))
    {
    type = "EST";
    table = "all_est";
    }
else if (startsWith("psu", track))
    {
    type = "Pseudo & Real Genes";
    table = "psu";
    }
else if (sameWord("xenoBlastzMrna", track) )
    {
    type = "Blastz to foreign mRNA";
    table = "xenoBlastzMrna";
    }
else if (startsWith(track, "mrnaBlastz" ))
    {
    type = "mRNA";
    table = track;
    }
else if (startsWith(track,"pseudoMrna"))
    {
    type = "mRNA";
    table = "mrnaBlastz";
    }
else 
    {
    type = "mRNA";
    table = "all_mrna";
    }

/* Print non-sequence info. */
cartWebStart(cart, acc);

printRnaSpecs(tdb, acc);

/* Get alignment info. */
pslList = getAlignments(conn, table, acc);
htmlHorizontalLine();
printf("<H3>%s/Genomic Alignments</H3>", type);
printAlignments(pslList, start, "htcCdnaAli", table, acc);

printTrackHtml(tdb);
hFreeConn(&conn);
}

void doRikenRna(struct trackDb *tdb, char *item)
/* Put up Riken RNA stuff. */
{
char query[512];
struct sqlResult *sr;
char **row;
struct sqlConnection *conn = sqlConnect("mgsc");

genericHeader(tdb, item);
sprintf(query, "select * from rikenMrna where qName = '%s'", item);
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

void doUserPsl(char *track, char *item)
/* Process click on user-defined alignment. */
{
int start = cartInt(cart, "o");
struct lineFile *lf;
struct psl *pslList = NULL, *psl;
char *pslName, *faName, *qName;
char *encItem = cgiEncode(item);
enum gfType qt, tt;
char *words[4];
int wordCount;

cartWebStart(cart, "BLAT Search Alignments");
printf("<H2>BLAT Search Alignments</H2>\n");
printf("<H3>Click over a line to see detailed letter by letter display</H3>");
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
slReverse(&pslList);
lineFileClose(&lf);
printAlignments(pslList, start, "htcUserAli", "user", encItem);
pslFreeList(&pslList);
}

void doHgGold(struct trackDb *tdb, char *fragName)
/* Click on a fragment of golden path. */
{
char *track = tdb->tableName;
struct sqlConnection *conn = hAllocConn();
struct sqlConnection *conn2 = hAllocConn();
struct sqlConnection *conn3 = hAllocConn();
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
int start = cartInt(cart, "o");
boolean hasBin;
char splitTable[64];
char *chp;
char *accession1, *accession2, *spanner, *evaluation, *variation, *varEvidence, 
    *contact, *remark, *comment;
char *secondAcc, *secondAccVer;
char *tmpString;
int first;

cartWebStart(cart, fragName);
hFindSplitTable(seqName, track, splitTable, &hasBin);
sprintf(query, "select * from %s where frag = '%s' and chromStart = %d", 
	splitTable, fragName, start+1);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
agpFragStaticLoad(row+hasBin, &frag);

printf("<B>Clone Fragment ID:</B> %s<BR>\n", frag.frag);
printf("<B>Clone Fragment Type:</B> %s<BR>\n", frag.type);
printf("<B>Clone Bases:</B> %d-%d<BR>\n", frag.fragStart+1, frag.fragEnd);
printPos(frag.chrom, frag.chromStart, frag.chromEnd, frag.strand, FALSE);

if (hTableExists("certificate"))
    {
    first = 1;
    again:
    tmpString = strdup(frag.frag);
    chp = strstr(tmpString, ".");
    if (chp != NULL) *chp = '\0';

    if (first)
	{
    	sprintf(query2,"select * from certificate where accession1='%s';", tmpString);
	}
    else
	{
    	sprintf(query2,"select * from certificate where accession2='%s';", tmpString);
	}
    sr2 = sqlMustGetResult(conn2, query2);
    row2 = sqlNextRow(sr2);
    while (row2 != NULL)
	{
	printf("<HR>");
        accession1 	= row2[0];
        accession2 	= row2[1];
        spanner		= row2[2];
        evaluation      = row2[3];
        variation 	= row2[4];
        varEvidence 	= row2[5];
        contact  	= row2[6];
        remark 		= row2[7];
        comment  	= row2[8];
	
	if (first)
	    {
	    secondAcc = accession2;
	    }
	else
	    {
	    secondAcc = accession1;
	    }

	sprintf(query3, "select frag from %s where frag like '%s.%c';",
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
	//if (strcmp(evaluation, "") != 0) printf("<B>Evaluation: </B> %s<BR>\n", evaluation);
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
char *track = tdb->tableName;
struct sqlConnection *conn = hAllocConn();
char query[256];
struct sqlResult *sr;
char **row;
struct agpGap gap;
int start = cartInt(cart, "o");
boolean hasBin;
char splitTable[64];

cartWebStart(cart, "Gap in Sequence");
hFindSplitTable(seqName, track, splitTable, &hasBin);
sprintf(query, "select * from %s where chromStart = %d", 
	splitTable, start);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL)
    errAbort("Couldn't find gap at %s:%d", seqName, start);
agpGapStaticLoad(row+hasBin, &gap);

printf("<B>Gap Type:</B> %s<BR>\n", gap.type);
printf("<B>Bridged:</B> %s<BR>\n", gap.bridge);
printPos(gap.chrom, gap.chromStart, gap.chromEnd, NULL, FALSE);
printTrackHtml(tdb);

sqlFreeResult(&sr);
hFreeConn(&conn);
}


void selectOneRow(struct sqlConnection *conn, char *table, char *query, 
		  struct sqlResult **retSr, char ***retRow)
/* Do query and return one row offset by bin as needed. */
{
char fullTable[64];
boolean hasBin;
struct sqlResult *sr;
char **row;
if (!hFindSplitTable(seqName, table, fullTable, &hasBin))
    errAbort("Table %s doesn't exist in database", table);
*retSr = sqlGetResult(conn, query);
if ((row = sqlNextRow(*retSr)) == NULL)
    errAbort("No match to query '%s'", query);
*retRow = row + hasBin;
}


void doHgContig(struct trackDb *tdb, char *ctgName)
/* Click on a contig. */
{
char *track = tdb->tableName;
struct sqlConnection *conn = hAllocConn();
char query[256];
struct sqlResult *sr;
char **row;
struct ctgPos *ctg;
int cloneCount;

genericHeader(tdb, ctgName);
sprintf(query, "select * from %s where contig = '%s'", track, ctgName);
selectOneRow(conn, track, query, &sr, &row);
ctg = ctgPosLoad(row);
sqlFreeResult(&sr);
printf("<B>Name:</B> %s<BR>\n", ctgName);

if (hTableExists("clonePos"))
    {
    sprintf(query, 
	    "select count(*) from clonePos where chrom = '%s' and chromEnd >= %d and chromStart <= %d",
	    ctg->chrom, ctg->chromStart, ctg->chromEnd);
    cloneCount = sqlQuickNum(conn, query);
    printf("<B>Total Clones:</B> %d<BR>\n", cloneCount);
    }
printPos(ctg->chrom, ctg->chromStart, ctg->chromEnd, NULL, TRUE);
printTrackHtml(tdb);

hFreeConn(&conn);
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
char *track = tdb->tableName;
struct sqlConnection *conn = hAllocConn();
char query[256];
struct sqlResult *sr;
char **row;
struct clonePos *clone;
int fragCount;

cartWebStart(cart, cloneName);
sprintf(query, "select * from %s where name = '%s'", track, cloneName);
selectOneRow(conn, track, query, &sr, &row);
clone = clonePosLoad(row);
sqlFreeResult(&sr);

sprintf(query, 
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
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char *track = tdb->tableName;
char query[256];
char goldTable[16];
char ctgStartStr[16];
int ctgStart;

genericHeader(tdb, bactigName);
sprintf(query, "select * from %s where name = '%s'", track, bactigName);
selectOneRow(conn, track, query, &sr, &row);
bactig = bactigPosLoad(row);
sqlFreeResult(&sr);
printf("<B>Name:</B> %s<BR>\n", bactigName);

snprintf(goldTable, sizeof(goldTable), "%s_gold", seqName);

puts("<B>First contig:</B>");
if (hTableExists(goldTable))
    {
    snprintf(query, sizeof(query),
	     "select chromStart from %s where frag = \"%s\"",
	     goldTable, bactig->startContig);
    ctgStart = sqlQuickNum(conn, query);
    ctgStart -= 1;
    snprintf(ctgStartStr, sizeof(ctgStartStr), "%d", ctgStart);
    hgcAnchor("gold", bactig->startContig, ctgStartStr);
    }
printf("%s</A><BR>\n", bactig->startContig);

puts("<B>Last contig:</B>");
if (hTableExists(goldTable))
    {
    snprintf(query, sizeof(query),
	     "select chromStart from %s where frag = \"%s\"",
	     goldTable, bactig->endContig);
    ctgStart = sqlQuickNum(conn, query);
    ctgStart -= 1;
    snprintf(ctgStartStr, sizeof(ctgStartStr), "%d", ctgStart);
    hgcAnchor("gold", bactig->endContig, ctgStartStr);
    }
printf("%s</A><BR>\n", bactig->endContig);

printPos(bactig->chrom, bactig->chromStart, bactig->chromEnd, NULL, FALSE);
printTrackHtml(tdb);

hFreeConn(&conn);
}

int showGfAlignment(struct psl *psl, bioSeq *oSeq, FILE *f, 
		    enum gfType qType, int qStart, int qEnd, char *qName)
/* Show protein/DNA alignment or translated DNA alignment. */
{
struct dnaSeq *dnaSeq = NULL;
boolean tIsRc = (psl->strand[1] == '-');
boolean qIsRc = (psl->strand[0] == '-');
boolean isProt = (qType == gftProt);
int tStart = psl->tStart, tEnd = psl->tEnd;
int mulFactor = (isProt ? 3 : 1);
int dnaSize = 0;
DNA *dna = NULL;	/* Mixed case version of genomic DNA. */
int oSize = oSeq->size;
char *oLetters = cloneString(oSeq->dna);
//int cfmStart=0;
int qbafStart, qbafEnd, tbafStart, tbafEnd;
int qcfmStart, qcfmEnd, tcfmStart, tcfmEnd;

/* Load dna sequence. */
dnaSeq = hDnaFromSeq(seqName, tStart, tEnd, dnaLower);
freez(&dnaSeq->name);
dnaSeq->name = cloneString(psl->tName);
dnaSize = dnaSeq->size;

tbafStart = psl->tStart;
tbafEnd   = psl->tEnd;
tcfmStart = psl->tStart;
tcfmEnd   = psl->tEnd;

qbafStart = qStart;
qbafEnd   = qEnd;
qcfmStart = qStart;
qcfmEnd   = qEnd;

/* Deal with minus strand. */
if (tIsRc)
    {
    int temp;
    reverseComplement(dnaSeq->dna, dnaSize);
    temp = psl->tSize - tEnd;
    tEnd = psl->tSize - tStart;
    tStart = temp;
    
    tbafStart = psl->tEnd;
    tbafEnd   = psl->tStart;
    tcfmStart = psl->tEnd;
    tcfmEnd   = psl->tStart;
    }
if (qIsRc)
    {
    int temp;
    reverseComplement(oSeq->dna, oSeq->size);
    reverseComplement(oLetters, oSeq->size);

    qcfmStart = qEnd;
    qcfmEnd   = qStart;
    qbafStart = qEnd;
    qbafEnd   = qStart;
    
    temp = psl->qSize - qEnd;
    qEnd = psl->qSize - qStart;
    qStart = temp;
    }
dna = cloneString(dnaSeq->dna);

if (qName == NULL) 
    qName = psl->qName;


fprintf(f, "<H4><A NAME=cDNA></A>%s%s</H4>\n", qName, (qIsRc  ? " (reverse complemented)" : ""));
fprintf(f, "<PRE><TT>");
tolowers(oLetters);

/* Display query sequence. */
{
struct cfm *cfm;
char *colorFlags = needMem(oSeq->size);
int i,j;

for (i=0; i<psl->blockCount; ++i)
    {
    int qs = psl->qStarts[i] - qStart;
    int ts = psl->tStarts[i] - tStart;
    int sz = psl->blockSizes[i]-1;
    colorFlags[qs] = socBrightBlue;
    oLetters[qs] = toupper(oLetters[qs]);
    colorFlags[qs+sz] = socBrightBlue;
    oLetters[qs+sz] = toupper(oLetters[qs+sz]);
    if (isProt)
	{
	for (j=1; j<sz; ++j)
	    {
	    AA aa = oSeq->dna[qs+j];
	    DNA *codon = &dnaSeq->dna[ts + 3*j];
	    AA trans = lookupCodon(codon);
	    if (trans != 'X' && trans == aa)
		{
		colorFlags[qs+j] = socBlue;
		oLetters[qs+j] = toupper(oLetters[qs+j]);
		}
	    }
	}
    else
	{
	for (j=1; j<sz; ++j)
	    {
	    if (oSeq->dna[qs+j] == dnaSeq->dna[ts+j])
		{
		colorFlags[qs+j] = socBlue;
		oLetters[qs+j] = toupper(oLetters[qs+j]);
		}
	    }
	}
    }
cfm = cfmNew(10, 60, TRUE, qIsRc, f, qcfmStart);
for (i=0; i<oSize; ++i)
    cfmOut(cfm, oLetters[i], seqOutColorLookup[colorFlags[i]]);
cfmFree(&cfm);
freez(&colorFlags);
htmHorizontalLine(f);
}
fprintf(f, "</TT></PRE>\n");
fprintf(f, "<H4><A NAME=genomic></A>%s.%s %s:</H4>\n", 
	organism, psl->tName, (tIsRc ? "(reverse strand)" : ""));
fprintf(f, "<PRE><TT>");
/* Display DNA sequence. */
{
struct cfm *cfm;
char *colorFlags = needMem(dnaSeq->size);
int i,j;
int curBlock = 0;
int anchorCount = 0;

for (i=0; i<psl->blockCount; ++i)
    {
    int qs = psl->qStarts[i] - qStart;
    int ts = psl->tStarts[i] - tStart;
    int sz = psl->blockSizes[i];
    if (isProt)
	{
	for (j=0; j<sz; ++j)
	    {
	    AA aa = oSeq->dna[qs+j];
	    int codonStart = ts + 3*j;
	    DNA *codon = &dnaSeq->dna[codonStart];
	    AA trans = lookupCodon(codon);
	    if (trans != 'X' && trans == aa)
		{
		colorFlags[codonStart] = socBlue;
		colorFlags[codonStart+1] = socBlue;
		colorFlags[codonStart+2] = socBlue;
		toUpperN(dna+codonStart, 3);
		}
	    }
	}
    else
	{
	for (j=0; j<sz; ++j)
	    {
	    if (oSeq->dna[qs+j] == dnaSeq->dna[ts+j])
		{
		colorFlags[ts+j] = socBlue;
		dna[ts+j] = toupper(dna[ts+j]);
		}
	    }
	}
    colorFlags[ts] = socBrightBlue;
    colorFlags[ts+sz*mulFactor-1] = socBrightBlue;
    }

cfm = cfmNew(10, 60, TRUE, tIsRc, f, tcfmStart);
    
for (i=0; i<dnaSeq->size; ++i)
    {
    /* Put down "anchor" on first match position in haystack
     * so user can hop here with a click on the needle. */
    if (curBlock < psl->blockCount && psl->tStarts[curBlock] == (i + tStart) )
	{
	fprintf(f, "<A NAME=%d></A>", ++curBlock);
	}
    cfmOut(cfm, dna[i], seqOutColorLookup[colorFlags[i]]);
    }
cfmFree(&cfm);
freez(&colorFlags);
htmHorizontalLine(f);
}

/* Display side by side. */
fprintf(f, "</TT></PRE>\n");
fprintf(f, "<H4><A NAME=ali></A>Side by Side Alignment*</H4>\n");
fprintf(f, "<PRE><TT>");
{
struct baf baf;
int i,j;

bafInit(&baf, oSeq->dna, qbafStart, qIsRc,
	dnaSeq->dna, tbafStart, tIsRc, f, 60, isProt);
	    
if (isProt)
    {
    for (i=0; i<psl->blockCount; ++i)
	{
	int qs = psl->qStarts[i] - qStart;
	int ts = psl->tStarts[i] - tStart;
	int sz = psl->blockSizes[i];

	bafSetPos(&baf, qs, ts);
	bafStartLine(&baf);
	for (j=0; j<sz; ++j)
	    {
	    AA aa = oSeq->dna[qs+j];
	    int codonStart = ts + 3*j;
	    DNA *codon = &dnaSeq->dna[codonStart];
	    bafOut(&baf, ' ', codon[0]);
	    bafOut(&baf, aa, codon[1]);
	    bafOut(&baf, ' ', codon[2]);
	    }
	bafFlushLine(&baf);
	}
    }
else
    {
    int lastQe = psl->qStarts[0] - qStart;
    int lastTe = psl->tStarts[0] - tStart;
    int maxSkip = 20;
    bafSetPos(&baf, lastQe, lastTe);
    bafStartLine(&baf);
    for (i=0; i<psl->blockCount; ++i)
	{
	int qs = psl->qStarts[i] - qStart;
	int ts = psl->tStarts[i] - tStart;
	int sz = psl->blockSizes[i];
	boolean doBreak = TRUE;
	int qSkip = qs - lastQe;
	int tSkip = ts - lastTe;

	if (qSkip >= 0 && qSkip <= maxSkip && tSkip == 0)
	    {
	    for (j=0; j<qSkip; ++j)
		bafOut(&baf, oSeq->dna[lastQe+j], '-');
	    doBreak = FALSE;
	    }
	else if (tSkip > 0 && tSkip <= maxSkip && qSkip == 0)
	    {
	    for (j=0; j<tSkip; ++j)
		bafOut(&baf, '-', dnaSeq->dna[lastTe+j]);
	    doBreak = FALSE;
	    }
	if (doBreak)
	    {
	    bafFlushLine(&baf);
	    bafSetPos(&baf, qs, ts);
	    bafStartLine(&baf);
	    }
	for (j=0; j<sz; ++j)
	    bafOut(&baf, oSeq->dna[qs+j], dnaSeq->dna[ts+j]);
	lastQe = qs + sz;
	lastTe = ts + sz;
	}
    bafFlushLine(&baf);

    fprintf( f, "<I>*Aligned Blocks with gaps <= %d bases are merged for this display</I>\n", maxSkip);
    }
}
fprintf(f, "</TT></PRE>");
if (qIsRc)
reverseComplement(oSeq->dna, oSeq->size);
freeMem(dna);
freeMem(oLetters);
return psl->blockCount;
}
int showChesAlignment(struct psl *psl, bioSeq *oSeq, FILE *f, 
		    enum gfType qType, int qStart, int qEnd, char *qName)
/* Show protein/DNA alignment or translated DNA alignment. */
{
struct dnaSeq *dnaSeq = NULL;
boolean tIsRc = (psl->strand[1] == '-');
boolean qIsRc = (psl->strand[0] == '-');
boolean isProt = (qType == gftProt)|| (qType == gftProtChes);
int tStart = psl->tStart, tEnd = psl->tEnd;
int mulFactor = (isProt ? 3 : 1);
int dnaSize = 0;
DNA *dna = NULL;	/* Mixed case version of genomic DNA. */
int oSize = oSeq->size;
char *oLetters = cloneString(oSeq->dna);
//int cfmStart=0;
int qbafStart, qbafEnd, tbafStart, tbafEnd;
int qcfmStart, qcfmEnd, tcfmStart, tcfmEnd;
int protEnd;

/* Load dna sequence. */
/*
if (isProt)
{
    protEnd = tStart + mulFactor * oSize;
    if (tEnd < protEnd)
	tEnd = protEnd;
}
*/
dnaSeq = hDnaFromSeq(seqName, tStart, tEnd, dnaLower);
freez(&dnaSeq->name);
dnaSeq->name = cloneString(psl->tName);
dnaSize = dnaSeq->size;

tbafStart = psl->tStart;
tbafEnd   = psl->tEnd;
tcfmStart = psl->tStart;
tcfmEnd   = psl->tEnd;

qbafStart = qStart;
qbafEnd   = qEnd;
qcfmStart = qStart;
qcfmEnd   = qEnd;

/* Deal with minus strand. */
if (tIsRc)
    {
    int temp;
    reverseComplement(dnaSeq->dna, dnaSize);
    temp = psl->tSize - tEnd;
    tEnd = psl->tSize - tStart;
    tStart = temp;
    
    tbafStart = psl->tEnd;
    tbafEnd   = psl->tStart;
    tcfmStart = psl->tEnd;
    tcfmEnd   = psl->tStart;
    }
if (qIsRc)
    {
    int temp;
    reverseComplement(oSeq->dna, oSeq->size);
    reverseComplement(oLetters, oSeq->size);

    qcfmStart = qEnd;
    qcfmEnd   = qStart;
    qbafStart = qEnd;
    qbafEnd   = qStart;
    
    temp = psl->qSize - qEnd;
    qEnd = psl->qSize - qStart;
    qStart = temp;
    }
dna = cloneString(dnaSeq->dna);

if (qName == NULL) 
    qName = psl->qName;


fprintf(f, "<H4><A NAME=cDNA></A>%s%s</H4>\n", qName, (qIsRc  ? " (reverse complemented)" : ""));
fprintf(f, "<PRE><TT>");
tolowers(oLetters);

/* Display query sequence. */
{
struct cfm *cfm;
char *colorFlags = needMem(oSeq->size);
int i,j;

for (i=0; i<psl->blockCount; ++i)
    {
    int qs = psl->qStarts[i] - qStart;
    int ts = psl->tStarts[i] - tStart;
    int sz = psl->blockSizes[i]-1;
    colorFlags[qs] = socBrightBlue;
    oLetters[qs] = toupper(oLetters[qs]);
    colorFlags[qs+sz] = socBrightBlue;
    oLetters[qs+sz] = toupper(oLetters[qs+sz]);
    if (isProt)
	{
	for (j=1; j<sz; ++j)
	    {
	    AA aa = oSeq->dna[qs+j];
	    DNA *codon = &dnaSeq->dna[ts + 3*j];
	    AA trans = lookupCodon(codon);
	    if (trans != 'X' && trans == aa)
		{
		colorFlags[qs+j] = socBlue;
		oLetters[qs+j] = toupper(oLetters[qs+j]);
		}
	    }
	}
    else
	{
	for (j=1; j<sz; ++j)
	    {
	    if (oSeq->dna[qs+j] == dnaSeq->dna[ts+j])
		{
		colorFlags[qs+j] = socBlue;
		oLetters[qs+j] = toupper(oLetters[qs+j]);
		}
	    }
	}
    }
cfm = cfmNew(10, 60, TRUE, qIsRc, f, qcfmStart);
for (i=0; i<oSize; ++i)
    cfmOut(cfm, oLetters[i], seqOutColorLookup[colorFlags[i]]);
cfmFree(&cfm);
freez(&colorFlags);
htmHorizontalLine(f);
}
fprintf(f, "</TT></PRE>\n");
#if 0
fprintf(f, "<H4><A NAME=genomic></A>%s.%s %s:</H4>\n", 
	organism, psl->tName, (tIsRc ? "(reverse strand)" : ""));
fprintf(f, "<PRE><TT>");
/* Display DNA sequence. */
{
struct cfm *cfm;
char *colorFlags = needMem(dnaSeq->size);
int i,j;
int curBlock = 0;
int anchorCount = 0;

for (i=0; i<psl->blockCount; ++i)
    {
    int qs = psl->qStarts[i] - qStart;
    int ts = psl->tStarts[i] - tStart;
    int sz = psl->blockSizes[i];
    if (isProt)
	{
	for (j=0; j<sz; ++j)
	    {
	    AA aa = oSeq->dna[qs+j];
	    int codonStart = ts + 3*j;
	    DNA *codon = &dnaSeq->dna[codonStart];
	    AA trans = lookupCodon(codon);
	    if (trans != 'X' && trans == aa)
		{
		colorFlags[codonStart] = socBlue;
		colorFlags[codonStart+1] = socBlue;
		colorFlags[codonStart+2] = socBlue;
		toUpperN(dna+codonStart, 3);
		}
	    }
	}
    else
	{
	for (j=0; j<sz; ++j)
	    {
	    if (oSeq->dna[qs+j] == dnaSeq->dna[ts+j])
		{
		colorFlags[ts+j] = socBlue;
		dna[ts+j] = toupper(dna[ts+j]);
		}
	    }
	}
    colorFlags[ts] = socBrightBlue;
    colorFlags[ts+sz*mulFactor-1] = socBrightBlue;
    }

cfm = cfmNew(10, 60, TRUE, tIsRc, f, tcfmStart);
    
for (i=0; i<dnaSeq->size; ++i)
    {
    /* Put down "anchor" on first match position in haystack
     * so user can hop here with a click on the needle. */
    if (curBlock < psl->blockCount && psl->tStarts[curBlock] == (i + tStart) )
	{
	fprintf(f, "<A NAME=%d></A>", ++curBlock);
	}
    cfmOut(cfm, dna[i], seqOutColorLookup[colorFlags[i]]);
    }
cfmFree(&cfm);
//freez(&colorFlags);
htmHorizontalLine(f);
}
#endif

/* Display side by side. */
fprintf(f, "</TT></PRE>\n");
fprintf(f, "<H4><A NAME=ali></A>Side by Side Alignment*</H4>\n");
fprintf(f, "<PRE><TT>");
{
struct baf baf;
int i,j;

bafInit(&baf, oSeq->dna, qbafStart, qIsRc,
	dnaSeq->dna, tbafStart, tIsRc, f, 60, isProt);
	    
if (isProt)
    {
    for (i=0; i<psl->blockCount; ++i)
	{
	int qs = psl->qStarts[i] - qStart;
	int ts = psl->tStarts[i] - tStart;
	int sz = psl->blockSizes[i];

	bafSetPos(&baf, qs, ts);
	bafStartLine(&baf);
	for (j=0; j<sz; ++j)
	    {
	    AA aa = oSeq->dna[qs+j];
	    int codonStart = ts + 3*j;
	    DNA *codon = &dnaSeq->dna[codonStart];
	    bafOut(&baf, ' ', codon[0]);
	    bafOut(&baf, aa, codon[1]);
	    bafOut(&baf, ' ', codon[2]);
	    }
	bafFlushLine(&baf);
	}
    }
else
    {
    int lastQe = psl->qStarts[0] - qStart;
    int lastTe = psl->tStarts[0] - tStart;
    int maxSkip = 20;
    bafSetPos(&baf, lastQe, lastTe);
    bafStartLine(&baf);
    for (i=0; i<psl->blockCount; ++i)
	{
	int qs = psl->qStarts[i] - qStart;
	int ts = psl->tStarts[i] - tStart;
	int sz = psl->blockSizes[i];
	boolean doBreak = TRUE;
	int qSkip = qs - lastQe;
	int tSkip = ts - lastTe;

	if (qSkip >= 0 && qSkip <= maxSkip && tSkip == 0)
	    {
	    for (j=0; j<qSkip; ++j)
		bafOut(&baf, oSeq->dna[lastQe+j], '-');
	    doBreak = FALSE;
	    }
	else if (tSkip > 0 && tSkip <= maxSkip && qSkip == 0)
	    {
	    for (j=0; j<tSkip; ++j)
		bafOut(&baf, '-', dnaSeq->dna[lastTe+j]);
	    doBreak = FALSE;
	    }
	if (doBreak)
	    {
	    bafFlushLine(&baf);
	    bafSetPos(&baf, qs, ts);
	    bafStartLine(&baf);
	    }
	for (j=0; j<sz; ++j)
	    bafOut(&baf, oSeq->dna[qs+j], dnaSeq->dna[ts+j]);
	lastQe = qs + sz;
	lastTe = ts + sz;
	}
    bafFlushLine(&baf);

    fprintf( f, "<I>*Aligned Blocks with gaps <= %d bases are merged for this display</I>\n", maxSkip);
    }
}
fprintf(f, "</TT></PRE>");
if (qIsRc)
reverseComplement(oSeq->dna, oSeq->size);
freeMem(dna);
freeMem(oLetters);
return psl->blockCount;
}


int showDnaAlignment(struct psl *psl, struct dnaSeq *rnaSeq, FILE *body, int cdsS, int cdsE)
/* Show alignment for accession. */
{
struct dnaSeq *dnaSeq;
DNA *rna;
int dnaSize,rnaSize;
boolean isRc = FALSE;
struct ffAli *ffAli, *ff;
int tStart, tEnd, tRcAdjustedStart;
int lastEnd = 0;
int blockCount;
char title[256];


/* Get RNA and DNA sequence.  */
rna = rnaSeq->dna;
rnaSize = rnaSeq->size;
tStart = psl->tStart - 100;
if (tStart < 0) tStart = 0;
tEnd  = psl->tEnd + 100;
if (tEnd > psl->tSize) tEnd = psl->tSize;
dnaSeq = hDnaFromSeq(seqName, tStart, tEnd, dnaMixed);
freez(&dnaSeq->name);
dnaSeq->name = cloneString(psl->tName);

/* Write body heading info. */
fprintf(body, "<H2>Alignment of %s and %s:%d-%d</H2>\n", psl->qName, psl->tName, psl->tStart+1, psl->tEnd);
fprintf(body, "Click on links in the frame to left to navigate through alignment.\n");

/* Convert psl alignment to ffAli. */
tRcAdjustedStart = tStart;
if (psl->strand[0] == '-')
    {
    isRc = TRUE;
    reverseComplement(dnaSeq->dna, dnaSeq->size);
    pslRcBoth(psl);
    tRcAdjustedStart = psl->tSize - tEnd;
    /*if (cdsE != 0)
        {
        cdsS = psl->tSize - cdsS;
        cdsE = psl->tSize - cdsE;
        }*/
    }
ffAli = pslToFfAli(psl, rnaSeq, dnaSeq, tRcAdjustedStart);

blockCount = ffShAliPart(body, ffAli, psl->qName, rna, rnaSize, 0, 
			 dnaSeq->name, dnaSeq->dna, dnaSeq->size, tStart, 
			 8, FALSE, isRc, FALSE, TRUE, TRUE, TRUE, TRUE, cdsS, cdsE);
return blockCount;
}

void showSomeAlignment(struct psl *psl, bioSeq *oSeq, 
		       enum gfType qType, int qStart, int qEnd, char *qName, int cdsS, int cdsE)
/* Display protein or DNA alignment in a frame. */
{
int blockCount;
struct tempName indexTn, bodyTn;
FILE *index, *body;
char title[256];
int i;

makeTempName(&indexTn, "index", ".html");
makeTempName(&bodyTn, "body", ".html");

/* Writing body of alignment. */
body = mustOpen(bodyTn.forCgi, "w");
htmStart(body, psl->qName);
if (qType == gftRna || qType == gftDna)
    blockCount = showDnaAlignment(psl, oSeq, body, cdsS, cdsE);
else if (qType == gftProtChes)
    blockCount = showChesAlignment(psl, oSeq, body, qType, qStart, qEnd, qName);
else 
    blockCount = showGfAlignment(psl, oSeq, body, qType, qStart, qEnd, qName);
fclose(body);
chmod(bodyTn.forCgi, 0666);

/* Write index. */
index = mustOpen(indexTn.forCgi, "w");
if (qName == NULL)
    qName = psl->qName;
htmStart(index, qName);
fprintf(index, "<H3>Alignment of %s</H3>", qName);
fprintf(index, "<A HREF=\"%s#cDNA\" TARGET=\"body\">%s</A><BR>\n", bodyTn.forCgi, qName);
fprintf(index, "<A HREF=\"%s#genomic\" TARGET=\"body\">%s.%s</A><BR>\n", bodyTn.forCgi, hOrganism(hGetDb()), psl->tName);
for (i=1; i<=blockCount; ++i)
    {
    fprintf(index, "<A HREF=\"%s#%d\" TARGET=\"body\">block%d</A><BR>\n",
	    bodyTn.forCgi, i, i);
    }
fprintf(index, "<A HREF=\"%s#ali\" TARGET=\"body\">together</A><BR>\n", bodyTn.forCgi);
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


void htcCdnaAli(char *acc)
/* Show alignment for accession. */
{
char query[256];
char table[64];
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
struct psl *psl;
struct dnaSeq *rnaSeq;
char *type;
int start;
unsigned int cdsStart = 0, cdsEnd = 0;
boolean hasBin;

/* Print start of HTML. */
writeFramesetType();
puts("<HTML>");
printf("<HEAD>\n<TITLE>%s vs Genomic</TITLE>\n</HEAD>\n\n", acc);

/* Get some environment vars. */
type = cartString(cart, "aliTrack");
start = cartInt(cart, "o");

/* Get cds start and stop, if available */
conn = hAllocConn();
sprintf(query, "select cds from mrna where acc = '%s'", acc);
sr = sqlGetResult(conn, query); 
if ((row = sqlNextRow(sr)) != NULL)
    {
    sprintf(query, "select name from cds where id = '%d'", atoi(row[0]));
    sqlFreeResult(&sr);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	genbankParseCds(row[0], &cdsStart, &cdsEnd);
    }
sqlFreeResult(&sr);

/* Look up alignments in database */
hFindSplitTable(seqName, type, table, &hasBin);
sprintf(query, "select * from %s where qName = '%s' and tStart=%d",
	table, acc, start);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find alignment for %s at %d", acc, start);
psl = pslLoad(row+hasBin);
sqlFreeResult(&sr);
hFreeConn(&conn);

rnaSeq = hRnaSeq(acc);
if (startsWith("xeno", type))
    showSomeAlignment(psl, rnaSeq, gftDnaX, 0, rnaSeq->size, NULL, cdsStart, cdsEnd);
else
    showSomeAlignment(psl, rnaSeq, gftDna, 0, rnaSeq->size, NULL, cdsStart, cdsEnd);
}

void htcChainAli(char *item)
/* Draw detailed alignment representation of a chain. */
{
struct chain *chain;
struct psl *fatPsl, *psl = NULL;
int id = atoi(item);
char *track = cartString(cart, "o");
char *type = trackTypeInfo(track);
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
chain = chainLoadIdRange(database, track, seqName, winStart, winEnd, id);
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
    qSeq = hExtSeq(psl->qName);
    sprintf(name, "%s", psl->qName);
    }
else
    {
    qSeq = loadGenomePart(otherDb, psl->qName, psl->qStart, psl->qEnd);
    sprintf(name, "%s.%s", otherOrg, psl->qName);
    }
writeFramesetType();
puts("<HTML>");
printf("<HEAD>\n<TITLE>%s %s vs %s %s </TITLE>\n</HEAD>\n\n", 
       (otherOrg == NULL ? "" : otherOrg), psl->qName, org, psl->tName );
showSomeAlignment(psl, qSeq, gftDnaX, psl->qStart, psl->qEnd, name, 0, 0);
}

void htcChainTransAli(char *item)
/* Draw detailed alignment representation of a chain with translated protein */
{
struct chain *chain;
struct psl *fatPsl, *psl = NULL;
int id = atoi(item);
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
chain = chainLoadIdRange(database, track, seqName, winStart, winEnd, id);
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
    qSeq = hExtSeq(psl->qName);
    sprintf(name, "%s", psl->qName);
    }
else
    {
    qSeq = loadGenomePart(otherDb, psl->qName, psl->qStart, psl->qEnd);
    sprintf(name, "%s.%s", otherOrg, psl->qName);
    }
writeFramesetType();
puts("<HTML>");
printf("<HEAD>\n<TITLE>%s %s vs %s %s </TITLE>\n</HEAD>\n\n", 
       (otherOrg == NULL ? "" : otherOrg), psl->qName, org, psl->tName );
//showSomeAlignment(psl, qSeq, gftDnaX, psl->qStart, psl->qEnd, name, 0, 0);
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

/* Print start of HTML. */
writeFramesetType();
puts("<HTML>");
printf("<HEAD>\n<TITLE>User Sequence vs Genomic</TITLE>\n</HEAD>\n\n");

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
struct lineFile *lf;
struct psl *psl;
int start;
enum gfType tt = gftDnaX, qt = gftProt;
boolean isProt = 1;
struct sqlResult *sr;
struct sqlConnection *conn = hAllocConn();
struct dnaSeq *seq;
char query[256], **row;
char fullTable[64];
boolean hasBin;

/* Print start of HTML. */
writeFramesetType();
puts("<HTML>");
printf("<HEAD>\n<TITLE>Protein Sequence vs Genomic</TITLE>\n</HEAD>\n\n");

start = cartInt(cart, "o");
hFindSplitTable(seqName, table, fullTable, &hasBin);
sprintf(query, "select * from %s where qName = '%s' and tName = '%s' and tStart=%d",
	fullTable, readName, seqName, start);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find alignment for %s at %d", readName, start);
psl = pslLoad(row+hasBin);
sqlFreeResult(&sr);
hFreeConn(&conn);
seq = hPepSeq(readName);
showSomeAlignment(psl, seq, qt, 0, seq->size, NULL, 0, 0);
}

void htcBlatXeno(char *readName, char *table)
/* Show alignment for accession. */
{
char *pslName, *faName, *qName;
struct lineFile *lf;
bioSeq *oSeqList = NULL, *oSeq = NULL;
struct psl *psl;
int start;
enum gfType tt, qt;
boolean isProt;
struct sqlResult *sr;
struct sqlConnection *conn = hAllocConn();
struct dnaSeq *seq;
char query[256], **row;
char fullTable[64];
boolean hasBin;

/* Print start of HTML. */
writeFramesetType();
puts("<HTML>");
printf("<HEAD>\n<TITLE>Sequence %s</TITLE>\n</HEAD>\n\n", readName);

start = cartInt(cart, "o");
hFindSplitTable(seqName, table, fullTable, &hasBin);
sprintf(query, "select * from %s where qName = '%s' and tName = '%s' and tStart=%d",
	fullTable, readName, seqName, start);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find alignment for %s at %d", readName, start);
psl = pslLoad(row+hasBin);
sqlFreeResult(&sr);
hFreeConn(&conn);
seq = hExtSeq(readName);
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
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
int start = cartInt(cart, "o");
struct wabAli *wa = NULL;
int qOffset;
char strand = '+';

sprintf(query, "select * from %s where query = '%s' and chrom = '%s' and chromStart = %d",
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
cartWebStart(cart, "Fish Alignment");
printf("Alignment between fish sequence %s (above) and human chromosome %s (below)\n",
       name, skipChr(seqName));
fetchAndShowWaba("waba_tet", name);
}


void doHgCbr(struct trackDb *tdb, char *name)
/* Do thing with cbr track. */
{
cartWebStart(cart, "Worm Alignment");
printf("Alignment between C briggsae sequence %s (above) and C elegans chromosome %s (below)\n",
       name, skipChr(seqName));
fetchAndShowWaba("wabaCbr", name);
}

void doHgRepeat(struct trackDb *tdb, char *repeat)
/* Do click on a repeat track. */
{
char *track = tdb->tableName;
int offset = cartInt(cart, "o");
cartWebStart(cart, "Repeat");
if (offset >= 0)
    {
    struct sqlConnection *conn = hAllocConn();
 
    struct sqlResult *sr;
    char **row;
    struct rmskOut *ro;
    char query[256];
    char table[64];
    boolean hasBin;
    int start = cartInt(cart, "o");

    hFindSplitTable(seqName, track, table, &hasBin);
    sprintf(query, "select * from %s where  repName = '%s' and genoName = '%s' and genoStart = %d",
	    table, repeat, seqName, start);
    sr = sqlGetResult(conn, query);
    printf("<H3>RepeatMasker Information</H3>\n");
    while ((row = sqlNextRow(sr)) != NULL)
	{
	ro = rmskOutLoad(row+hasBin);
	printf("<B>Name:</B> %s<BR>\n", ro->repName);
	printf("<B>Family:</B> %s<BR>\n", ro->repFamily);
	printf("<B>Class:</B> %s<BR>\n", ro->repClass);
	printf("<B>SW Score:</B> %d<BR>\n", ro->swScore);
	printf("<B>Divergence:</B> %3.1f%%<BR>\n", 0.1 * ro->milliDiv);
	printf("<B>Deletions:</B>  %3.1f%%<BR>\n", 0.1 * ro->milliDel);
	printf("<B>Insertions:</B> %3.1f%%<BR>\n", 0.1 * ro->milliIns);
	printf("<B>Begin in repeat:</B> %d<BR>\n", ro->repStart);
	printf("<B>End in repeat:</B> %d<BR>\n", ro->repEnd);
	printf("<B>Left in repeat:</B> %d<BR>\n", ro->repLeft);
	printPos(seqName, ro->genoStart, ro->genoEnd, ro->strand, TRUE);
	}
    hFreeConn(&conn);
    }
else
    {
    if (sameString(repeat, "SINE"))
	printf("This track contains the small interspersed (SINE) class of repeats , which includes ALUs.<BR>\n");
    else if (sameString(repeat, "LINE"))
        printf("This track contains the long interspersed (LINE) class of repeats<BR>\n");
    else if (sameString(repeat, "LTR"))
        printf("This track contains the LTR (long terminal repeat) class of repeats which includes retroposons<BR>\n");
    else
        printf("This track contains the %s class of repeats<BR>\n", repeat);
    printf("Click on an individual repeat for more information on that repeat<BR>\n");
    }
printTrackHtml(tdb);
}

void doHgIsochore(struct trackDb *tdb, char *item)
/* do click on isochore track. */
{
char *track = tdb->tableName;
cartWebStart(cart, "Isochore Info");
printf("<H2>Isochore Information</H2>\n");
if (cgiVarExists("o"))
    {
    struct isochores *iso;
    struct sqlConnection *conn = hAllocConn();
    struct sqlResult *sr;
    char **row;
    char query[256];
    int start = cartInt(cart, "o");
    sprintf(query, "select * from %s where  name = '%s' and chrom = '%s' and chromStart = %d",
	    track, item, seqName, start);
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
char *track = tdb->tableName;
cartWebStart(cart, "Simple Repeat Info");
printf("<H2>Simple Tandem Repeat Information</H2>\n");
if (cgiVarExists("o"))
    {
    struct simpleRepeat *rep;
    struct sqlConnection *conn = hAllocConn();
    struct sqlResult *sr;
    char **row;
    char query[256];
    int start = cartInt(cart, "o");
    int rowOffset = hOffsetPastBin(seqName, track);
    sprintf(query, "select * from %s where  name = '%s' and chrom = '%s' and chromStart = %d",
	    track, item, seqName, start);
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
	printPos(seqName, rep->chromStart, rep->chromEnd, NULL, TRUE);
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
cartWebStart(cart, "Softberry TSSW Promoter");
printf("<H2>Softberry TSSW Promoter Prediction %s</H2>", item);

if (cgiVarExists("o"))
    {
    struct softPromoter *pro;
    struct sqlConnection *conn = hAllocConn();
    struct sqlResult *sr;
    char **row;
    char query[256];
    int start = cartInt(cart, "o");
    int rowOffset = hOffsetPastBin(seqName, track);
    sprintf(query, "select * from %s where  name = '%s' and chrom = '%s' and chromStart = %d",
	    track, item, seqName, start);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	pro = softPromoterLoad(row+rowOffset);
	bedPrintPos((struct bed *)pro, 3);
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
char *table = tdb->tableName;
cartWebStart(cart, "CpG Island Info");
printf("<H2>CpG Island Info</H2>\n");
if (cgiVarExists("o"))
    {
    struct cpgIsland *island;
    struct sqlConnection *conn = hAllocConn();
    struct sqlResult *sr;
    char **row;
    char query[256];
    int start = cartInt(cart, "o");
    int rowOffset = hOffsetPastBin(seqName, table);
    sprintf(query, "select * from %s where  name = '%s' and chrom = '%s' and chromStart = %d",
	    table, item, seqName, start);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	island = cpgIslandLoad(row+rowOffset);
	bedPrintPos((struct bed *)island, 3);
	printf("<B>Size:</B> %d<BR>\n", island->chromEnd - island->chromStart);
	printf("<B>CpG count:</B> %d<BR>\n", island->cpgNum);
	printf("<B>C count plus G count:</B> %d<BR>\n", island->gcNum);
	printf("<B>Percentage CpG:</B> %1.1f%%<BR>\n", island->perCpg);
	printf("<B>Percentage C or G:</B> %1.1f%%<BR>\n", island->perGc);
	printf("<BR>\n");
#ifdef OLD
	htmlHorizontalLine();
	printCappedSequence(island->chromStart, island->chromEnd, 100);
#endif /* OLD */
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

void printProbRow(char *label, float *p, int pCount)
/* Print one row of a probability profile. */
{
int i;
printf("%s ", label);
for (i=0; i < pCount; ++i)
    printf("%4.2f ", p[i]);
printf("\n");
}

#ifdef NEVER
char *motifConsensus(struct dnaMotif *motif)
/* Return consensus sequence for motif.  freeMem
 * result when done. */
{
int i, size = motif->columnCount;
char *consensus = needMem(size+1);
char c;
float best;
for (i=0; i<size; ++i)
    {
    c = 'a';
    best = motif->aProb[i];
    if (motif->cProb[i] > best)
	{
	best = motif->cProb[i];
	c = 'c';
	}
    if (motif->gProb[i] > best)
	{
	best = motif->gProb[i];
	c = 'g';
	}
    if (motif->tProb[i] > best)
	{
	best = motif->tProb[i];
	c = 't';
	}
    consensus[i] = c;
    }
return consensus;
}
#endif /* NEVER */

void printSpacedDna(char *dna, int size)
/* Print string with spaces between each letter. */
{
int i;
printf("  ");
for (i=0; i<size; ++i)
    printf(" %c   ", dna[i]);
}

void printConsensus(struct dnaMotif *motif)
/* Print motif - use bold caps for strong letters, then
 * caps, then small letters, then .'s */
{
int i, size = motif->columnCount;
char c;
float best;
printf("  ");
for (i=0; i<size; ++i)
    {
    c = 'a';
    best = motif->aProb[i];
    if (motif->cProb[i] > best)
	{
	best = motif->cProb[i];
	c = 'c';
	}
    if (motif->gProb[i] > best)
	{
	best = motif->gProb[i];
	c = 'g';
	}
    if (motif->tProb[i] > best)
	{
	best = motif->tProb[i];
	c = 't';
	}
    printf(" ");
    if (best >= 0.90)
	printf("<B>%c</B>", toupper(c));
    else if (best >= 0.75)
        printf("%c", toupper(c));
    else if (best >= 0.50)
        printf("%c", tolower(c));
    else if (best >= 0.40)
        printf("<FONT COLOR=\"#A0A0A0\">%c</FONT>", tolower(c));
    else
        printf("<FONT COLOR=\"#A0A0A0\">.</FONT>");
    printf("   ");
    }
}

void doTriangle(struct trackDb *tdb, char *item)
/* Display detailed info on a regulatory triangle item. */
{
struct sqlConnection *conn = hAllocConn();
int start = cartInt(cart, "o");
char query[256];
struct dnaSeq *seq = NULL;
char *table = tdb->tableName;
int rowOffset = hOffsetPastBin(seqName, table);
struct sqlResult *sr;
char **row;
struct bed *hit = NULL;
struct dnaMotif *motif;

cartWebStart(cart, "Regulatory Motif Info");
genericBedClick(conn, tdb, item, start, 6);

webNewSection("Motif:");
sprintf(query, 
	"select * from %s where  name = '%s' and chrom = '%s' and chromStart = %d",
	table, item, seqName, start);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    hit = bedLoadN(row + rowOffset, 6);
sqlFreeResult(&sr);

printf("<PRE>");
if (hit != NULL)
    {
    int i;
    seq = hDnaFromSeq(hit->chrom, hit->chromStart, hit->chromEnd, dnaLower);
    if (hit->strand[0] == '-')
	reverseComplement(seq->dna, seq->size);
    touppers(seq->dna);
    printSpacedDna(seq->dna, seq->size);
    printf("sequence here\n");
    }

sprintf(query, "name = '%s'", item);
motif = dnaMotifLoadWhere(conn, "dnaMotif", query);
if (motif != NULL)
    {
    printConsensus(motif);
    printf("motif consensus\n");
    printProbRow("A", motif->aProb, motif->columnCount);
    printProbRow("C", motif->cProb, motif->columnCount);
    printProbRow("G", motif->gProb, motif->columnCount);
    printProbRow("T", motif->tProb, motif->columnCount);
    }
printf("</PRE>");

}

void printLines(FILE *f, char *s, int lineSize)
/* Print s, lineSize characters (or less) per line. */
{
int len = strlen(s);
int start, left;
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
aaSeq *seq = hGenBankGetPep(pepName, table);
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
struct sqlConnection *conn = hAllocConn();
char query[256];
static char buf[256], *name;

sprintf(query, "select transId from %s where name = '%s'", table, hugoName);
name = sqlQuickQuery(conn, query, buf, sizeof(buf));
hFreeConn(&conn);
if (name == NULL)
    errAbort("Database inconsistency: couldn't find gene name %s in knownInfo",
	     hugoName);
return name;
}


void htcTranslatedProtein(char *geneName)
/* Display translated protein. */
{
hgcStart("Protein Translation");
showProteinPrediction(geneName, cartString(cart, "o"));
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
struct dnaSeq *genoSeq = hDnaFromSeq(gp->chrom, txStart, gp->txEnd,  dnaLower);
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

void htcGeneMrna(char *geneName)
/* Display associated cDNA. */
{
char *table = cartString(cart, "o");
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct genePred *gp;
struct dnaSeq *seq;
int cdsStart, cdsEnd;
int rowOffset = hOffsetPastBin(seqName, table);

hgcStart("DNA Near Gene");
sprintf(query, "select * from %s where name = '%s'", table, geneName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row+rowOffset);
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
sqlFreeResult(&sr);
}

void htcRefMrna(char *geneName)
/* Display mRNA associated with a refSeq gene. */
{
/* check both gbSeq and refMrna */
struct dnaSeq *seq = hGenBankGetMrna(geneName, "refMrna");
if (seq == NULL)
    errAbort("RefSeq mRNA sequence %s not found", geneName);

hgcStart("RefSeq mRNA");
printf("<PRE><TT>");
faWriteNext(stdout, seq->name, seq->dna, seq->size);
printf("</TT></PRE>");
dnaSeqFree(&seq);
}

void htcKnownGeneMrna(char *geneName)
/* Display mRNA associated with a knownGene gene. */
{
/* check both gbSeq and knowGeneMrna */
struct dnaSeq *seq = hGenBankGetMrna(geneName, "knownGeneMrna");
if (seq == NULL)
    errAbort("Known gene mRNA sequence %s not found", geneName);

hgcStart("Known Gene mRNA");
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
char hgTextTbl[128];

cartWebStart(cart, "Genomic Sequence Near Gene");
printf("<H2>Get Genomic Sequence Near Gene</H2>");

snprintf(hgTextTbl, sizeof(hgTextTbl), "chr1_%s", tbl);
if (hTableExists(hgTextTbl))
    snprintf(hgTextTbl, sizeof(hgTextTbl), "%s.chrN_%s", database, tbl);
else
    snprintf(hgTextTbl, sizeof(hgTextTbl), "%s.%s", database, tbl);
puts("<P>"
     "Note: if you would prefer to get DNA for more than one feature of "
     "this track at a time, try the ");
printf("<A HREF=\"%s?%s&db=%s&position=%s:%d-%d&table0=%s&phase=table\" TARGET=_BLANK>",
       hgTextName(), cartSidUrlString(cart), database,
       seqName, winStart+1, winEnd, hgTextTbl);
puts("Table Browser</A>: "
     "perform an Advanced Query and select FASTA as the output format.");

printf("<FORM ACTION=\"%s\">\n\n", hgcPath());
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

hgSeqOptions(tbl);
cgiMakeButton("submit", "submit");
printf("</FORM>");
}

void htcGeneAlignment(char *geneName)
/* Put up page that lets user display genomic sequence
 * associated with gene. */
{
cartWebStart(cart, "Aligned Annotated Genomic Sequence ");
printf("<H2>Align a gene prediction to another species or the same species and view codons and translated proteins.</H2>");
printf("<FORM ACTION=\"%s\">\n\n", hgcPath());
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
hgSeqOptions(cgiString("o"));
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

void htcDnaNearGene(char *geneName)
/* Fetch DNA near a gene. */
{
char *table    = cartString(cart, "o");
char constraints[256];
int itemCount;

snprintf(constraints, sizeof(constraints), "name = '%s'", geneName);
puts("<PRE>");
itemCount = hgSeqItemsInRange(table, seqName, winStart, winEnd, constraints);
if (itemCount == 0)
    printf("\n# No results returned from query.\n\n");
puts("</PRE>");
}

void doKnownGene(struct trackDb *tdb, char *geneName)
/* Handle click on known gene track. */
{
char *track = tdb->tableName;
char *transName = geneName;
struct knownMore *km = NULL;
boolean anyMore = FALSE;
boolean upgraded = FALSE;
char *knownTable = "knownInfo";
boolean knownMoreExists = FALSE;

cartWebStart(cart, "RefSeq Gene");
printf("<H2>RefSeq Gene %s</H2>\n", geneName);
if (hTableExists("knownMore"))
    {
    knownMoreExists = TRUE;
    knownTable = "knownMore";
    }
if (!isGenieGeneName(geneName))
    transName = hugoToGenieName(geneName, knownTable);
else
    geneName = NULL;
    
if (knownMoreExists)
    {
    char query[256];
    struct sqlResult *sr;
    char **row;
    struct sqlConnection *conn = hAllocConn();

    upgraded = TRUE;
    sprintf(query, "select * from knownMore where transId = '%s'", transName);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	km = knownMoreLoad(row);
	}
    sqlFreeResult(&sr);

    hFreeConn(&conn);
    }

if (km != NULL)
    {
    geneName = km->name;
    if (km->hugoName != NULL && km->hugoName[0] != 0)
	medlineLinkedLine("Name", km->hugoName, km->hugoName);
    if (km->aliases[0] != 0)
	printf("<B>Aliases:</B> %s<BR>\n", km->aliases);
    if (km->omimId != 0)
	{
	printf("<B>OMIM:</B> <A HREF=\"");
	printEntrezOMIMUrl(stdout, km->omimId);
	printf("\" TARGET=_blank>%d</A><BR>\n", km->omimId);
	}
    if (km->locusLinkId != 0)
        {
	printf("<B>LocusLink:</B> ");
	printf("<A HREF = \"http://www.ncbi.nlm.nih.gov/LocusLink/LocRpt.cgi?l=%d\" TARGET=_blank>",
	       km->locusLinkId);
	printf("%d</A><BR>\n", km->locusLinkId);
	}
    anyMore = TRUE;
    }
if (geneName != NULL) 
    {
    medlineLinkedLine("Symbol", geneName, geneName);
    printGeneLynxName(geneName);
    printf("<B>GeneCards:</B> ");
    printf("<A HREF = \"http://bioinfo.weizmann.ac.il/cards-bin/cardsearch.pl?search=%s\" TARGET=_blank>",
	   geneName);
    printf("%s</A><BR>\n", geneName);
    anyMore = TRUE;
    }

if (anyMore)
    htmlHorizontalLine();

geneShowCommon(transName, tdb, "genieKnownPep");
puts(
     "<P>Known genes are derived from the "
     "<A HREF = \"http://www.ncbi.nlm.nih.gov/LocusLink/\" TARGET=_blank>"
     "RefSeq</A> mRNA database supplemented by other mRNAs with annotated coding regions.  "
     "These mRNAs were mapped to the draft "
     "human genome using <A HREF = \"http://www.cse.ucsc.edu/~kent\" TARGET=_blank>"
     "Jim Kent's</A> PatSpace software, and further processed by "
     "<A HREF = \"http://www.affymetrix.com\" TARGET=_blank>Affymetrix</A> to cope "
     "with sequencing errors in the draft.</P>\n");
if (!upgraded)
    {
    puts(
	 "<P>The treatment of known genes in the browser is quite "
	 "preliminary.  We hope to provide links into OMIM and LocusLink "
	 "in the near future.</P>");
    }
puts(
     "<P>Additional information may be available by clicking on the "
     "mRNA associated with this gene in the main browser window.</P>");
}

void doViralProt(struct trackDb *tdb, char *geneName)
/* Handle click on known viral protein track. */
{
struct sqlConnection *conn = hAllocConn();
int start = cartInt(cart, "o");
char *track = tdb->tableName;
char *transName = geneName;
struct knownMore *km = NULL;
boolean anyMore = FALSE;
boolean upgraded = FALSE;
char *knownTable = "knownInfo";
boolean knownMoreExists = FALSE;
struct psl *pslList = NULL;

cartWebStart(cart, "Viral Gene");
printf("<H2>Viral Gene %s</H2>\n", geneName);
printCustomUrl(tdb, geneName, TRUE);

pslList = getAlignments(conn, "chr1_viralProt", geneName);
htmlHorizontalLine();
printf("<H3>Protein Alignments</H3>");
printAlignments(pslList, start, "htcProteinAli", "chr1_viralProt", geneName);
printTrackHtml(tdb);
}

void doPslDetailed(struct trackDb *tdb, char *item)
/* Fairly generic PSL handler -- print out some more details about the 
 * alignment. */
{
int start = cartInt(cart, "o");
int total = 0, i = 0;
char *track = tdb->tableName;
struct psl *pslList = NULL;
struct sqlConnection *conn = hAllocConn();

genericHeader(tdb, item);
printCustomUrl(tdb, item, TRUE);

puts("<P>");
puts("<B>Alignment Summary:</B><BR>\n");
pslList = getAlignments(conn, track, item);
printAlignments(pslList, start, "htcCdnaAli", track, item);

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
       pslList->match,
       pslList->misMatch,
       pslList->nCount,
       pslList->tBaseInsert, hOrganism(hGetDb()),
       pslList->qBaseInsert, item,
       pslScore(pslList));

printTrackHtml(tdb);
hFreeConn(&conn);
}

void doSPGene(struct trackDb *tdb, char *mrnaName)
/* Process click on a known gene. */
{
struct sqlConnection *conn  = hAllocConn();
struct sqlConnection *conn2 = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];

char cond_str[128];
char *refSeqName = NULL;
char *descID;
char *mrnaDesc;
char *proteinID;
char *proteinDesc;
char *pdbID;
char *freezeName;
char *mgiID;
struct refLink *rl;
char *seqType;
boolean dnaBased;
char *proteinAC;
char *pfamAC, *pfamID, *pfamDesc;
char *atlasUrl;
char *mapID, *locusID, *mapDescription;
char *geneID;
char *geneSymbol;
char cart_name[255];
boolean hasPathway, hasMedical;

sprintf(cond_str, "kgID='%s'", mrnaName);
geneSymbol = sqlGetField(conn, database, "kgXref", "geneSymbol", cond_str);
if (geneSymbol == NULL) geneSymbol = mrnaName;

sprintf(cart_name, "Known Gene: %s", geneSymbol);
cartWebStart(cart, cart_name);

dnaBased = FALSE;
if (hTableExists("knownGeneLink"))
    {
    sprintf(cond_str, "name='%s' and seqType='g'", mrnaName);
    seqType = sqlGetField(conn, database, "knownGeneLink", "seqType", cond_str);

    if (seqType != NULL)
        {
        dnaBased = TRUE;
        refSeqName = strdup(mrnaName);
        sprintf(cond_str, "name='%s'", refSeqName);
        proteinAC = sqlGetField(conn, database, "knownGeneLink", "proteinID", cond_str);
        }
    }
// Display mRNA or DNA info and NCBI link
if (dnaBased && refSeqName != NULL)
    {
    printf("This gene is based on RefSeq %s, which is derived from a DNA sequence.", refSeqName);
    printf("  Please follow the link below for further details.<br>\n");

    printf("<UL><LI>");
    printf("<B>NCBI: </B> <A HREF=\"");
    printEntrezNucleotideUrl(stdout, refSeqName);
    printf("\" TARGET=_blank>%s</A><BR>\n", refSeqName);
    printf("</UL>");
    }
else
    {
    printf("<B>mRNA:</B> ");
    sprintf(cond_str, "acc='%s'", mrnaName);
    descID = sqlGetField(conn, database, "mrna", "description", cond_str);
    sprintf(cond_str, "id=%s", descID);
    mrnaDesc = sqlGetField(conn, database, "description", "name", cond_str);
    if (mrnaDesc != NULL) printf("%s\n", mrnaDesc);

    printf("<UL><LI>");
    printf("<B>NCBI: </B> <A HREF=\"");
    printEntrezNucleotideUrl(stdout, mrnaName);
    printf("\" TARGET=_blank>%s</A><BR>\n", mrnaName);
    printf("</UL>");
    }

// Display protein description and links
printf("<B>Protein:</B> ");

sprintf(cond_str, "name='%s'", mrnaName);
proteinID = sqlGetField(conn, database, "knownGene", "proteinID", cond_str);
if (proteinID == NULL)
    {
    errAbort("Couldn't find corresponding protein for mRNA %s.", mrnaName);
    }

sprintf(cond_str, "displayID='%s'", proteinID);
proteinDesc = sqlGetField(conn, protDbName, "spXref3", "description", cond_str);
if (proteinDesc != NULL) printf("%s\n", proteinDesc);
sprintf(cond_str, "sp='%s'", proteinID);
pdbID= sqlGetField(conn, protDbName, "pdbSP", "pdb", cond_str);

printf("<UL>");

if (pdbID != NULL)
    {
    printf("<LI><B>PDB: </B>");
    sprintf(query, "select pdb from %s.pdbSP where sp = '%s'", 
	    protDbName, proteinID);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
    	{
    	pdbID  = row[0];
	printf("<A HREF=\"http://www.rcsb.org/pdb/cgi/explore.cgi?pdbId=%s\"", pdbID);
	printf("TARGET=_blank>%s</A>&nbsp\n", pdbID);
    	}
    printf("<BR>");
    sqlFreeResult(&sr);
    }

printf("<LI><B>SWISS-PROT/TrEMBL: </B>");
printf("<A HREF=\"http://www.expasy.org/cgi-bin/niceprot.pl?%s\" TARGET=_blank>%s</A>\n", 
       proteinID, proteinID);

// print more protein links if there are other proteins correspond to this mRNA
sprintf(query, "select dupProteinID from %s.dupSpMrna where mrnaID = '%s'", database, mrnaName);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL) printf(", ");
while (row != NULL)    
    {
    //spID  = row[0];
    printf("<A HREF=\"http://www.expasy.org/cgi-bin/niceprot.pl?%s\" TARGET=_blank>%s</A>\n", 
	   row[0], row[0]);
    row = sqlNextRow(sr);
    if (row != NULL) printf(", ");
    }
sqlFreeResult(&sr);
printf("<BR>");

// show Pfam links
if (hTableExistsDb(protDbName, "pfamXref"))
    {
    sprintf(cond_str, "swissDisplayID='%s'", proteinID);
    pfamAC = sqlGetField(conn, protDbName, "pfamXref", "pfamAC", cond_str);

    if (pfamAC != NULL)
    	{
        printf("<LI><B>Pfam-A Domains: </B><BR>");
    	sprintf(query, "select pfamAC from %s.pfamXref where swissDisplayID = '%s'", 
		protDbName, proteinID);
    	sr = sqlGetResult(conn, query);
    	while ((row = sqlNextRow(sr)) != NULL)
    	    {
    	    pfamAC  = row[0];
            sprintf(cond_str, "pfamAC='%s'", pfamAC);
            pfamID = sqlGetField(conn2, protDbName, "pfamDesc", "pfamID", cond_str);
	    printf("&nbsp&nbsp&nbsp");
	    printf("<A HREF=\"http://www.sanger.ac.uk/cgi-bin/Pfam/swisspfamget.pl?name=%s\"", 
		   proteinID);
	    printf("TARGET=_blank>%s</A>", pfamAC);
	    printf("&nbsp(%s)&nbsp", pfamID);
    	    sprintf(cond_str, "pfamAC='%s'", pfamAC);
            pfamDesc= sqlGetField(conn2, protDbName, "pfamDesc", "description", cond_str);
	    printf(" %s<BR>\n", pfamDesc);
    	    }
    	sqlFreeResult(&sr);
    	}
    }

//The following is disabled until UCSC Proteome Browser relased to public
goto skipPB;

// display link to UCSC Proteome Browser
printf("<LI><B>UCSC Proteome Browser: </B>");
printf("<A HREF=\"http:/cgi-bin/pb10?");
printf("proteinDB=SWISS&proteinID=%s&mrnaID=%s\" ", proteinID, mrnaName);
printf(" target=_blank>");
printf(" %s</A>", proteinID);

// display additional entries if they exist
sprintf(query, "select dupProteinID from %s.dupSpMrna where mrnaID = '%s'", database, mrnaName);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL) printf(", ");
while (row != NULL)    
    {
    printf("<A HREF=\"http:/cgi-bin/pb10?");
    printf("proteinDB=SWISS&proteinID=%s&mrnaID=%s\" ", row[0], mrnaName);
    printf(" target=_blank>");
    printf(" %s</A>", row[0]);
    row = sqlNextRow(sr);
    if (row != NULL) printf(", ");
    }
sqlFreeResult(&sr);

skipPB:
printf("</UL>");

// Display Gene Family Browser link
if (sqlTableExists(conn, "knownCanonical"))
    {
    printf("<B>UCSC Gene Family Browser:</B> ");
    printf("<A HREF=\"http:/cgi-bin/hgNear?near.id=%s\"", mrnaName);
    printf("TARGET=_blank>%s</A>&nbsp\n", geneSymbol);fflush(stdout);
    printf("<BR><BR>");
    }

// Show Pathway links if any exists
hasPathway = FALSE;

//Process KEGG Pathway link data
if (sqlTableExists(conn, "keggPathway"))
    {
    sprintf(query, "select * from %s.keggPathway where kgID = '%s'", database, mrnaName);
    sr = sqlGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
	{
	if (!hasPathway)
	    {
	    printf("<B>Pathways</B><UL>");
	    hasPathway = TRUE;
	    }
        while (row != NULL)
            {
            locusID = row[1];
	    mapID   = row[2];
	    printf("<LI><B>KEGG:&nbsp</B>");
	    sprintf(cond_str, "mapID=%c%s%c", '\'', mapID, '\'');
	    mapDescription = sqlGetField(conn2, database, "keggMapDesc", "description", cond_str);
	    printf("<A HREF = \"");
	    printf("http://www.genome.ad.jp/dbget-bin/show_pathway?%s+%s", mapID, locusID);
	    printf("\" TARGET=_blank>%s</A> %s </LI>\n",mapID, mapDescription);
            row = sqlNextRow(sr);
	    }
	}
    sqlFreeResult(&sr);
    }

// Process SRI BioCyc link data
if (sqlTableExists(conn, "bioCycPathway"))
    {
    sprintf(query, "select * from %s.bioCycPathway where kgID = '%s'", database, mrnaName);
    sr = sqlGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
	{
	if (!hasPathway)
	    {
	    printf("<BR><B>Pathways</B><UL>");
	    hasPathway = TRUE;
	    }
        while (row != NULL)
            {
            geneID = row[1];
	    mapID   = row[2];
	    printf("<LI><B>BioCyc:&nbsp</B>");
	    sprintf(cond_str, "mapID=%c%s%c", '\'', mapID, '\'');
	    mapDescription = sqlGetField(conn2, database, "bioCycMapDesc", "description", cond_str);
	    printf("<A HREF = \"");
	    printf("http://biocyc.org:1555/HUMAN/new-image?type=PATHWAY&object=%s&detail-level=2",
		   mapID);
	    printf("\" TARGET=_blank>%s</A> %s </LI>\n",mapID, mapDescription);
            row = sqlNextRow(sr);
	    }
	}
    sqlFreeResult(&sr);
    }

if (hasPathway)
    {
    printf("</UL>\n");
    }

// Process medical related links here
hasMedical = FALSE;

// process links to Atlas of Genetics and Cytogenetics in Oncology and Haematology 
if (sqlTableExists(conn, "atlasOncoGene"))
    {
    sprintf(cond_str, "locusSymbol=%c%s%c", '\'', geneSymbol, '\'');
    atlasUrl = sqlGetField(conn, database, "atlasOncoGene", "url", cond_str);
    if (atlasUrl != NULL)
	{
	if (!hasMedical)
	    {
    	    printf("<B>Medical Related Links:</B><UL>");
	    hasMedical = TRUE;
	    }

    	printf("<LI><B>Atlas of Genetics and Cytogenetics in Oncology and Haematology:&nbsp</B>");

	printf("<A HREF = \"%s%s\" TARGET=_blank>%s</A><BR></LI>\n", 
	   "http://www.infobiogen.fr/services/chromcancer/", atlasUrl, geneSymbol);fflush(stdout);
    	}
    }

if (hasMedical)
    {
    printf("</UL>\n");
    }

// Display RefSeq related info, if there is a corresponding RefSeq

    sprintf(query, "select refseq from %s.mrnaRefseq where mrna = '%s'",  database, mrnaName);
    sr = sqlGetResult(conn, query);
    row = sqlNextRow(sr);
    if ((row == NULL) && (!dnaBased))
	{
	printf("<B>RefSeq:</B>"); 
	printf(" No corresponding RefSeq entry found for %s.<br>\n", mrnaName);
	fflush(stdout);
	}
    else
	{
	if (dnaBased)
	    {
	    refSeqName = strdup(mrnaName);
	    }
	else
	    {
	    refSeqName = strdup(row[0]);
	    }
	sprintf(query, "select * from refLink where mrnaAcc = '%s'", refSeqName);

	sqlFreeResult(&sr);
	sr = sqlGetResult(conn, query);
	if ((row = sqlNextRow(sr)) == NULL) 
	    {
	    sqlFreeResult(&sr);
	    printf("<B>RefSeq:</B>"); 
	    printf(" A corresponding Reference Sequence ");
	    printf("<A HREF=\"");
	    printEntrezNucleotideUrl(stdout, refSeqName);
	    printf("\" TARGET=_blank>%s</A>", refSeqName);
	    printf(" is not found in our database for "); 
	    freezeName = hFreezeFromDb(database);
	    if(freezeName == NULL) freezeName = "Unknown";
	    printf("%s %s Freeze.<BR>\n",hOrganism(database),freezeName); 
	    fflush(stdout);
	    }
	else
	    {
	    rl = refLinkLoad(row);
	    sqlFreeResult(&sr);
    
	    htmlHorizontalLine();
	    printf("<B>RefSeq:</B> <A HREF=\"");
	    printEntrezNucleotideUrl(stdout, rl->mrnaAcc);
	    printf("\" TARGET=_blank>%s</A>", rl->mrnaAcc);

	    /* If refSeqStatus is available, report it: */
	    if (hTableExists("refSeqStatus"))
		{
		sprintf(query, "select status from refSeqStatus where mrnaAcc = '%s'",
			refSeqName);
		sr = sqlGetResult(conn, query);
		if ((row = sqlNextRow(sr)) != NULL)
		    {
		    printf("&nbsp;&nbsp; Status: <B>%s</B>", row[0]);
		    }
		sqlFreeResult(&sr);
		}
	    printf("<BR>");

	    printf("<B>Gene ID: %s<BR></B>\n", rl->name);
    
	    if (rl->omimId != 0)
		{
		printf("<B>OMIM:</B> <A HREF=\"");
		printEntrezOMIMUrl(stdout, rl->omimId);
		printf("\" TARGET=_blank>%d</A><BR>\n", rl->omimId);
		}
	    if (rl->locusLinkId != 0)
		{
		printf("<B>LocusLink:</B> ");
		printf("<A HREF = \"http://www.ncbi.nlm.nih.gov/LocusLink/LocRpt.cgi?");
		printf("l=%d\" TARGET=_blank>", rl->locusLinkId);
		printf("%d</A><BR>\n", rl->locusLinkId);
		if ((strstr(hgGetDb(), "mm") != NULL) && hTableExists("MGIid"))
		    {
		    sprintf(query, "select MGIid from MGIid where LLid = '%d';",
			    rl->locusLinkId);

		    sr = sqlGetResult(conn, query);
		    if ((row = sqlNextRow(sr)) != NULL)
			{
			printf("<B>Mouse Genome Informatics:</B> ");
			mgiID = strdup(row[0]);
			printf("<A HREF=\"http://www.informatics.jax.org/searches/accession_report.cgi?");
			printf("id=%s\">%s</A><BR>\n",mgiID, mgiID);
			}
		    else
			{
			// per Carol from Jackson Lab 4/12/02, JAX do not always agree
			// with Locuslink on seq to gene association.
			// Thus, not finding a MGIid even if a LocusLink ID
			// exists is always a possibility.
			}
		    sqlFreeResult(&sr);
		    }
		}
	    medlineLinkedLine("PubMed on Gene", rl->name, rl->name);
	    if (rl->product[0] != 0)
    		medlineLinkedLine("PubMed on Product", rl->product, rl->product);
	    printf("\n");
	    printGeneLynxName(rl->name);
	    printf("\n");
	    printf("<B>GeneCards:</B> ");
	    printf("<A HREF = \"http://bioinfo.weizmann.ac.il/cards-bin/cardsearch.pl?");
	    printf("search=%s\" TARGET=_blank>", rl->name);
	    printf("%s</A><BR>\n", rl->name);
	    if (hTableExists("jaxOrtholog"))
		{
		struct jaxOrtholog jo;
		sprintf(query, "select * from jaxOrtholog where humanSymbol='%s'", rl->name);
		sr = sqlGetResult(conn, query);
		while ((row = sqlNextRow(sr)) != NULL)
		    {
		    jaxOrthologStaticLoad(row, &jo);
		    printf("<B>MGI Mouse Ortholog:</B> ");
		    printf("<A HREF=\"http://www.informatics.jax.org/searches/accession_report.cgi?");
		    printf("id=%s\" target=_BLANK>", jo.mgiId);
		    printf("%s</A><BR>\n", jo.mouseSymbol);
		    }
		sqlFreeResult(&sr);
		}

	    if (startsWith("hg", hGetDb()))
		{
		printf("\n");
		printf("<B>AceView:</B> ");
		printf("<A HREF = \"http://www.ncbi.nih.gov/IEB/Research/Acembly/av.cgi?");
		printf("db=human&l=%s\" TARGET=_blank>", rl->name);
		printf("%s</A><BR>\n", rl->name);
		}
	    printStanSource(rl->mrnaAcc, "mrna");
	    }
	htmlHorizontalLine();
	if (dnaBased)
	    {
	    geneShowPosAndLinksDNARefseq(mrnaName, mrnaName, tdb, "knownGenePep", "htcTranslatedProtein",
					 "htcKnownGeneMrna", "htcGeneInGenome", "mRNA Sequence");
	    }
	else
	    {
	    geneShowPosAndLinks(mrnaName, mrnaName, tdb, "knownGenePep", "htcTranslatedProtein",
				"htcKnownGeneMrna", "htcGeneInGenome", "mRNA Sequence");
	    }
	}

    printTrackHtml(tdb);
    hFreeConn(&conn);
    hFreeConn(&conn2);
}

void printEnsemblCustomUrl(struct trackDb *tdb, char *itemName, boolean encode)
/* Print Ensembl Gene URL. */
{
char *shortItemName;
char *url = tdb->url;
if (url != NULL && url[0] != 0)
    {
    char supfamURL[512];
    char *genomeStr = "";
    char *genomeStrEnsembl = "";

    struct sqlConnection *conn = hAllocConn();
    char cond_str[256];
    char *proteinID;
    char *geneID;
    char *ans;

    char *chp;

    // shortItemName is the name without the "." + version 
    shortItemName = strdup(itemName);
    chp = strstr(shortItemName, ".");
    if (chp != NULL) *chp = '\0';

    genomeStrEnsembl = ensOrgNameFromScientificName(scientificName);
    if (genomeStrEnsembl == NULL)
	{
	warn("Organism %s not found!", organism); fflush(stdout);
	return;
	}

    printf("<B>Ensembl Gene Link: </B>");
    printf("<A HREF=\"http://www.ensembl.org/%s/geneview?transcript=%s\" target=_blank>", 
	   genomeStrEnsembl,shortItemName);
    printf("%s</A><br>", itemName);

    if (hTableExists("superfamily"))
	{
    	sprintf(cond_str, "transcript_name='%s'", shortItemName);    
	
        // this is necessary because Ensembl is changine their xref table definition
    	if (hTableExists("ensemblXref2"))
	    {
	    proteinID = sqlGetField(conn, database, "ensemblXref2", "translation_name", cond_str);
  	    }
	else
	    {
	    proteinID = sqlGetField(conn, database, "ensemblXref", "translation_name", cond_str);
  	    }

	if (proteinID != NULL)
	    { 
            printf("<B>Ensembl Protein: </B>");
            printf("<A HREF=\"http://www.ensembl.org/%s/protview?peptide=%s\" target=_blank>", 
		   genomeStrEnsembl,proteinID);
            printf("%s</A><BR>\n", proteinID);
	    }

    	// get genomeStr to be used in Superfamily URL */ 
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
	    	warn("Organism %s not found!", organism); 
	    	return;
	    	}
	    }
    	sprintf(cond_str, "name='%s'", itemName);    
    	ans = sqlGetField(conn, database, "superfamily", "name", cond_str);
    	if (ans != NULL)
	    {
            printf("<B>Superfamily Link: </B>");
            safef(supfamURL, sizeof(supfamURL), "<A HREF=\"%s%s;seqid=%s\" target=_blank>", 
	            "http://supfam.org/SUPERFAMILY/cgi-bin/gene.cgi?genome=", 
	    	    genomeStr, proteinID);
            printf("%s", supfamURL);
            printf("%s</A><BR><BR>\n", proteinID);
            }
    	}
    free(shortItemName);
    }
}

void doEnsemblGene(struct trackDb *tdb, char *item, char *itemForUrl)
/* Put up Ensembl Gene track info. */
{
char *dupe, *type, *words[16];
char title[256];
int wordCount;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn();

if (itemForUrl == NULL)
    itemForUrl = item;
dupe = cloneString(tdb->type);
genericHeader(tdb, item);
wordCount = chopLine(dupe, words);
printEnsemblCustomUrl(tdb, itemForUrl, item == itemForUrl);
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
    char cond_str[256];
    char *proteinID;
    char genomeStr[10];

    struct sqlConnection *conn = hAllocConn();
    char query[256];
    struct sqlResult *sr;
    char **row;

    printf("The corresponding protein %s has the following Superfamily domain(s):", itemName);
    printf("<UL>\n");
    
    sprintf(query,
            "select description from sfDescription where proteinID='%s';",
            itemName);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    while (row != NULL)
        {
        printf("<li>%s", row[0]);
        row = sqlNextRow(sr);
        }
    hFreeConn(&conn);
    sqlFreeResult(&sr);
    
    printf("</UL>"); 

    if (sameWord(organism, "human"))
	{
        strcpy(genomeStr, "hs");
	}
    else
	{
    	if (sameWord(organism, "mouse"))
	    {
	    strcpy(genomeStr, "mm");
	    }
	else
	    {
	    printf("<br>Organism %s not found!!!", organism); fflush(stdout);
	    return;
	    }
	}

    printf("<B>Superfamily Link: </B>");
    sprintf(supfamURL, "<A HREF=\"%s%s;seqid=%s\" target=_blank>", 
	    "http://supfam.org/SUPERFAMILY/cgi-bin/gene.cgi?genome=", 
	    genomeStr, itemName);
    printf("%s", supfamURL);
    printf("%s</A><BR><BR>\n", itemName);
    }
}

void doSuperfamily(struct trackDb *tdb, char *item, char *itemForUrl)
/* Put up Superfamily track info. */
{
char *dupe, *type, *words[16];
char title[256];
int wordCount;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn();

if (itemForUrl == NULL)
    itemForUrl = item;

genericHeader(tdb, item);

printSuperfamilyCustomUrl(tdb, itemForUrl, item == itemForUrl);

printTrackHtml(tdb);

freez(&dupe);
hFreeConn(&conn);
}

void doRefGene(struct trackDb *tdb, char *rnaName)
/* Process click on a known RefSeq gene. */
{
char *track = tdb->tableName;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
char *mgiID;
char *sqlRnaName = rnaName;
struct refLink *rl;
struct genePred *gp;
int start = cartInt(cart, "o");
struct psl *pslList = NULL;

/* Make sure to escape single quotes for DB parseability */
if (strchr(rnaName, '\''))
    {
    sqlRnaName = replaceChars(rnaName, "'", "''");
    }
cartWebStart(cart, "RefSeq Gene");
sprintf(query, "select * from refLink where mrnaAcc = '%s'", sqlRnaName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find %s in refLink table - database inconsistency.", rnaName);
rl = refLinkLoad(row);
sqlFreeResult(&sr);
printf("<H2>RefSeq Gene %s</H2>\n", rl->name);
    
printf("<B>RefSeq:</B> <A HREF=\"");
printEntrezNucleotideUrl(stdout, rl->mrnaAcc);
printf("\" TARGET=_blank>%s</A>", rl->mrnaAcc);
/* If refSeqStatus is available, report it: */
if (hTableExists("refSeqStatus"))
    {
    sprintf(query, "select status from refSeqStatus where mrnaAcc = '%s'",
	    sqlRnaName);
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
    printf("<B>LocusLink:</B> ");
    printf("<A HREF = \"http://www.ncbi.nlm.nih.gov/LocusLink/LocRpt.cgi?l=%d\" TARGET=_blank>",
	   rl->locusLinkId);
    printf("%d</A><BR>\n", rl->locusLinkId);

    if ( (strstr(hgGetDb(), "mm") != NULL) && hTableExists("MGIid"))
    	{
	sprintf(query, "select MGIid from MGIid where LLid = '%d';",
		rl->locusLinkId);

	sr = sqlGetResult(conn, query);
	if ((row = sqlNextRow(sr)) != NULL)
	    {
	    printf("<B>Mouse Genome Informatics:</B> ");
	    mgiID = strdup(row[0]);
		
	    printf("<A HREF=\"http://www.informatics.jax.org/searches/accession_report.cgi?id=%s\">%s</A><BR>\n",mgiID, mgiID);
	    }
	else
	    {
	    // per Carol from Jackson Lab 4/12/02, JAX do not always agree
	    // with Locuslink on seq to gene association.
	    // Thus, not finding a MGIid even if a LocusLink ID
	    // exists is always a possibility.
	    }
	sqlFreeResult(&sr);
	}
    } 
if (!startsWith("Worm", organism))
{
    medlineLinkedLine("PubMed on Gene", rl->name, rl->name);
    if (rl->product[0] != 0)
	medlineLinkedLine("PubMed on Product", rl->product, rl->product);
    printf("\n");
    printGeneLynxName(rl->name);
    printf("\n");
    printf("<B>GeneCards:</B> ");
    printf("<A HREF = \"http://bioinfo.weizmann.ac.il/cards-bin/cardsearch.pl?search=%s\" TARGET=_blank>",
       rl->name);
    printf("%s</A><BR>\n", rl->name);
}
if (hTableExists("jaxOrtholog"))
    {
    struct jaxOrtholog jo;
    char * sqlRlName = rl->name;

    /* Make sure to escape single quotes for DB parseability */
    if (strchr(rl->name, '\''))
        {
        sqlRlName = replaceChars(rl->name, "'", "''");
        }
    sprintf(query, "select * from jaxOrtholog where humanSymbol='%s'", sqlRlName);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	jaxOrthologStaticLoad(row, &jo);
	printf("<B>MGI Mouse Ortholog:</B> ");
	printf("<A HREF=\"http://www.informatics.jax.org/searches/accession_report.cgi?id=%s\" target=_BLANK>", jo.mgiId);
	printf("%s</A><BR>\n", jo.mouseSymbol);
	}
    sqlFreeResult(&sr);
    }
if (startsWith("hg", hGetDb()))
    {
    printf("\n");
    printf("<B>AceView:</B> ");
    printf("<A HREF = \"http://www.ncbi.nih.gov/IEB/Research/Acembly/av.cgi?db=human&l=%s\" TARGET=_blank>",
	   rl->name);
    printf("%s</A><BR>\n", rl->name);
    }
if (!startsWith("Worm", organism))
{
    printStanSource(rl->mrnaAcc, "mrna");
}

htmlHorizontalLine();

/* print alignments that track was based on */
{
char *refSeqAli = "refSeqAli";
char *xenoRefSeqAli = "xenoRefSeqAli";

if (!sqlTableExists(conn, refSeqAli))
    if (sqlTableExists(conn, xenoRefSeqAli))
	refSeqAli = xenoRefSeqAli;

pslList = getAlignments(conn, refSeqAli, rl->mrnaAcc);
printf("<H3>mRNA/Genomic Alignments</H3>");
printAlignments(pslList, start, "htcCdnaAli", refSeqAli, rl->mrnaAcc);
}

htmlHorizontalLine();

geneShowPosAndLinks(rl->mrnaAcc, rl->protAcc, tdb, "refPep", "htcTranslatedProtein",
		    "htcRefMrna", "htcGeneInGenome", "mRNA Sequence");

printTrackHtml(tdb);
hFreeConn(&conn);
}

void doMgcGenes(struct trackDb *tdb, char *acc)
/* Process click on a mgcGenes track. */
{
struct sqlConnection *conn = hAllocConn();
int start = cartInt(cart, "o");
struct psl *pslList = NULL;

/* Print non-sequence info. */
cartWebStart(cart, acc);

printRnaSpecs(tdb, acc);
htmlHorizontalLine();
geneShowPosAndLinks(acc, acc, tdb, NULL, NULL,
                    "htcGeneMrna", "htcGeneInGenome", "mRNA Sequence");

/* print alignments that track was based on */
pslList = getAlignments(conn, "all_mrna", acc);
htmlHorizontalLine();
printf("<H3>mRNA/Genomic Alignments</H3>");
printAlignments(pslList, start, "htcCdnaAli", "all_mrna", acc);

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
    struct sqlResult *sr2, *sr5;
    char **row2;
    struct sqlConnection *conn2 = hAllocConn();
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
    //printf("<A HREF=\"http://www.soe.ucsc.edu/~karplus/SARS/%s/summary.html#alignment", 
    printf("<A HREF=\"/SARS/%s/summary.html#alignment", 
	    itemName);
    printf("\" TARGET=_blank>%s</A><BR>\n", itemName);

    printf("<B>Secondary Structure Predictions:</B> ");
    //printf("<A HREF=\"http://www.soe.ucsc.edu/~karplus/SARS/%s/summary.html#secondary-structure", 
    printf("<A HREF=\"/SARS/%s/summary.html#secondary-structure", 
	    itemName);
    printf("\" TARGET=_blank>%s</A><BR>\n", itemName);

    printf("<B>3D Structure Prediction (PDB file):</B> ");
    gotPDBFile = 0;    
    sprintf(cond_str, "proteinID='%s' and evalue <1.0e-5;", itemName);
    if (sqlGetField(conn2, database, "protHomolog", "proteinID", cond_str) != NULL)
	{
	sprintf(cond_str, "proteinID='%s'", itemName);
	predFN = sqlGetField(conn2, database, "protPredFile", "predFileName", cond_str);
    	if (predFN != NULL)
	    {
            printf("<A HREF=\"/SARS/%s/", itemName);
            //printf("%s.t2k.undertaker-align.pdb\">%s</A><BR>\n", itemName,itemName);
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
    
    conn2= hAllocConn();
    sprintf(query2,
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
		{
		strcpy(goodSCOPdomain, SCOPdomain);
		}
	    else
		{
		if (strcmp(goodSCOPdomain,SCOPdomain) != 0)
			{
			goto skip;
			}
		else
			{
			if (eValue > 0.1) goto skip;
			}
		}
	    if (first)
		{
		first = 0;
		}
	    else
		{
		printf(", ");
		}

    	    printf("<A HREF=\"http://www.rcsb.org/pdb/cgi/explore.cgi?job=graphics&pdbId=%s", 
		   homologID);
    	    if (strlen(chain) >= 1) 
		{
   	        printf("\"TARGET=_blank>%s(chain %s)</A>", homologID, chain);
		}
	    else
		{
   	        printf("\"TARGET=_blank>%s</A>", homologID);
		}
	    homologCount++;

	    skip:
	    row2 = sqlNextRow(sr2);
	    }
	}
    hFreeConn(&conn2);
    sqlFreeResult(&sr2);
    if (homologCount == 0)
	{
	printf("None<BR>\n");
	}	
    
    printf("<BR><B>Details:</B> ");
    printf("<A HREF=\"/SARS/%s/summary.html", itemName);
    printf("\" TARGET=_blank>%s</A><BR>\n", itemName);
   
    htmlHorizontalLine();
    }

void showHomologies(char *geneName, char *table)
/* Show homology info. */
{
struct sqlConnection *conn = hAllocConn();
char query[256];
struct sqlResult *sr;
char **row;
boolean isFirst = TRUE, gotAny = FALSE;
char *gi;
struct softberryHom hom;

if (sqlTableExists(conn, table))
    {
    sprintf(query, "select * from %s where name = '%s'", table, geneName);
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
	sprintf(query, "%s", gi);
	printEntrezProteinUrl(stdout, query);
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
struct sqlConnection *conn = hAllocConn();
char query[256];
struct sqlResult *sr;
char **row;
boolean isFirst = TRUE, gotAny = FALSE;
char *gi;
struct borkPseudoHom hom;
char *parts[10];
int partCount;
char *clone;


if (sqlTableExists(conn, table))
    {
    sprintf(query, "select * from %s where name = '%s'", table, geneName);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	borkPseudoHomStaticLoad(row, &hom);
//	if ((gi = getGi(hom.giString)) == NULL)
//	    continue;
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
	    sprintf(query, "%s", parts[1]);
	    printSwissProtProteinUrl(stdout, query);
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
char *tbl = cgiUsualString("table", cgiString("g"));

printf("<p>");
printf("<B>%s PseudoGene:</B> %s:%d-%d   %d bp<BR>\n", hOrganism(database),  bed->chrom, bed->chromStart, bed->chromEnd, bed->chromEnd-bed->chromStart);
printf("Strand: %c",bed->strand[0]);
printf("<p>");
}
void pseudoPrintPos(char *chrom, int chromStart, int chromEnd, struct pseudoGeneLink *pg)
/*    print details of pseudogene record */ 
{
char *tbl = cgiUsualString("table", cgiString("g"));
char chainStr[32];
char query[256];
char chainTable[64];
struct sqlResult *sr;
char **row;
struct psl *pslList ;
struct sqlConnection *conn = hAllocConn();

if (sameString(pg->assembly , database))
    safef(chainTable,sizeof(chainTable), "selfChain");
else
    {
    char *org = hOrganism(pg->assembly);
    org[0] = tolower(org[0]);
    safef(chainTable,sizeof(chainTable), "%sChain", org);
    }
printf("<B>Score:</B> %d Gap Ratio: %d Intron Ratio: %d <B>PolyA tail length:</B> %d <B>Start:</B> %d <BR>\n", pg->score, pg->score2, pg->score3, pg->polyA, pg->polyAstart);
htmlHorizontalLine();
printf("<p>");
printf("<B>%s Gene:</B> %s %s in %s\n", hOrganism(pg->assembly), pg->gene, pg->geneTable, pg->assembly);
linkToOtherBrowser(pg->assembly, pg->chrom, pg->gStart, pg->gEnd);
printf("%s:%d-%d \n", pg->chrom, pg->gStart, pg->gEnd);
printf("</A>");

pslList = loadPslRangeT("mrnaBlastz", pg->name, pg->chrom, pg->gStart, pg->gEnd);
if (pslList != NULL)
    printAlignments(pslList, chromStart, "htcCdnaAli", "mrnaBlastz", pg->name);
htmlHorizontalLine();
/* lookup chain if not stored */
if (pg->chainId == 0 && pg->strand != NULL)
    {
    if (sameString(pg->strand,pg->pStrand))
        safef(query,sizeof(query),
            "select id, score from %s_%s where tEnd > %d and tStart < %d and qName = '%s' and qEnd > %d and qStart < %d order by score desc",
            chrom, chainTable,chromStart,chromEnd, pg->chrom, pg->gStart, pg->gEnd);
    else
        {
        safef(query,sizeof(query),
            "select id, score from %s_%s where tEnd > %d and tStart < %d and qName = '%s' and qEnd > %d and qStart < %d order by score desc",
            chrom, chainTable,chromStart,chromEnd, pg->chrom, hChromSize(pg->chrom)-(pg->gEnd), hChromSize(pg->chrom)-(pg->gStart));
        }
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
        int chainId, score;
        chainId = sqlUnsigned(row[0]);
        score = sqlUnsigned(row[1]);
        if (pg->chainId == 0) pg->chainId = chainId;
        puts("<LI>\n");
        printf("<B>Chain:</B> %d  \n",chainId);
        hgcAnchorTranslatedChain(chainId, chainTable, chrom, pg->gStart, pg->gEnd);
        printf("View details of parts of chain within browser window</A>. score %d %s:%d-%d<BR>\n",score, pg->chrom,pg->gStart,pg->gEnd);
        puts("</LI>\n");
        }
    sqlFreeResult(&sr);
    }
else
    {
    puts("<LI>\n");
    printf("<B>Chain:</B> %d  \n",pg->chainId);
    hgcAnchorTranslatedChain(pg->chainId, chainTable, chrom, pg->gStart, pg->gEnd);
    printf("View details of parts of chain within browser window</A>.<BR>\n");
    puts("</LI>\n");
    }
//printf("<p>");
if (pg->chainId > 0)
    {
    puts("<LI>\n");
    hgcAnchorPseudoGene(pg->gene, pg->geneTable, chrom, "startcodon", chromStart, chromEnd, pg->chrom, pg->gStart, pg->gEnd, pg->chainId, pg->assembly);
    printf("Show %s %s aligned to pseudogene</A>  to see frameshifts and in frame stops <BR>\n",hOrganism(pg->assembly), pg->geneTable);
    puts("</LI>\n");
    }
puts("<LI>\n");
linkToOtherBrowser(pg->assembly, pg->chrom, pg->gStart, pg->gEnd);
printf("Open %s browser</A> at position corresponding to the Gene.<BR>\n",hOrganism(pg->assembly) );
puts("</LI>\n");
}

void doPseudoPsl(struct trackDb *tdb, char *acc)
/* Click on an pseudogene based on mrna alignment. */
{
char *track = tdb->tableName;
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
char *type;
char *table = NULL;
boolean hasBin;
struct pseudoGeneLink *pg;
int start = cartInt(cart, "o");
int winStart = cartInt(cart, "l");
int winEnd = cartInt(cart, "r");
char *chrom = cartString(cart, "c");
struct psl *pslList = NULL, *psl = NULL;
char *tbl = cgiUsualString("table", cgiString("g"));

/* Get alignment info. */
pslList = loadPslRangeT(tbl, acc, chrom, winStart, winEnd);

/* print header */
genericHeader(tdb, acc);
//printf("<B>%s PseudoGene:</B> %s:%d-%d   %d bp<BR>\n", hOrganism(database),  psl->tName, psl->tStart, psl->tEnd, psl->tEnd-psl->tStart);
//printf("Strand: %c",psl->strand[0]);
if (startsWith(track,"pseudoMrna"))
    {
    type = "mRNA";
    table = "mrnaBlastz";
    }
else 
    {
    type = "mRNA";
    table = "all_mrna";
    }

/* Print non-sequence info. */
cartWebStart(cart, acc);

printf("<H4>PseudoGene/Genomic Alignment</H4>");
printAlignments(pslList, start, "htcCdnaAli", table, acc);

sprintf(query, "select * from pseudoGeneLink where name = '%s' and pchrom = '%s' and pStart = %d", acc, chrom, start);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    pg = pseudoGeneLinkLoad(row);
    if (hTableExists("axtInfo") && pslList != NULL)
        {
        pseudoPrintPos(pslList->tName, pslList->tStart, pslList->tEnd, pg);
        }
    }
//htmlHorizontalLine();
//printf("<p><A HREF=\"%s&o=%d&g=getDna&i=mixed&c=%s&l=%d&r=%d&strand=%s&table=%s\">"
//        "View DNA for this feature</A><BR>\n",  hgcPathAndSettings(),
//	psl->tStart, psl->tName, psl->tStart, psl->tEnd, "+", tbl);
printTrackHtml(tdb);
hFreeConn(&conn);
}


void doPseudoGene(struct trackDb *tdb, char *geneName)
/* Handle click on ucsc pseudogene gene track. */
{
char query[256];
struct sqlResult *sr;
char **row;
boolean isFirst = TRUE, gotAny = FALSE;
char *gi;
struct bed *bed = NULL;
struct pseudoGeneLink pg;
struct sqlConnection *conn = hAllocConn();
char *tbl = cgiUsualString("table", cgiString("g"));
char *start = cartString(cart, "o");
genericHeader(tdb, geneName);
if (sqlTableExists(conn, tdb->tableName))
    {
    sprintf(query, "select * from %s where name = '%s' and chromStart = '%s'", tbl, geneName,start);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
        bed = bedLoadN(row+1, 6);
        }
//    sqlFreeResult(&sr);
    pseudoPrintPosHeader(bed);
    sprintf(query, "select * from pseudoGeneLink where name = '%s'", geneName);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
        pseudoGeneLinkStaticLoad(row, &pg);
        if (hTableExists("axtInfo") && bed != NULL)
            {
            pseudoPrintPos(bed->chrom, bed->chromStart, bed->chromEnd, &pg);
            }
        }
    }
//showHomologies(geneName, "softberryHom");
//geneShowCommon(geneName, tdb, NULL);
printf("<p><A HREF=\"%s&o=%d&g=getDna&i=mixed&c=%s&l=%d&r=%d&strand=%s&table=%s\">"
	   "View DNA for this feature</A><BR>\n",  hgcPathAndSettings(),
	   bed->chromStart, bed->chrom, bed->chromStart, bed->chromEnd, "+", tbl);
printTrackHtml(tdb);
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
    sprintf(query, "select * from %s where name = '%s'", table, geneName);
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
	sprintf(query, "%s[gi]", gi);
	printEntrezProteinUrl(stdout, query);
	printf("\" TARGET=_blank>%s</A> %s<BR>", hom.giString, hom.description);
	}
    }
if (gotAny)
    htmlHorizontalLine();
sqlFreeResult(&sr);
}

struct hash *makeTrackHash(char *chrom)
/* Make hash of trackDb items for this chromosome. */
{
struct sqlConnection *conn = hAllocConn();
struct hash *trackHash = newHash(7);
struct sqlResult *sr;
char **row;
struct trackDb *tdb;
char *trackDb = hTrackDbName();
char query[256];
snprintf(query, sizeof(query), "select * from %s", trackDb);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    tdb = trackDbLoad(row);
    hLookupStringsInTdb(tdb, database);
    if (hTrackOnChrom(tdb, chrom))
	hashAdd(trackHash, tdb->tableName, tdb);
    else
        trackDbFree(&tdb);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return trackHash;
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
if (hTableExists(extraTable))
    {
    struct sanger22extra se;
    char query[256];
    struct sqlConnection *conn = hAllocConn();
    struct sqlResult *sr;
    char **row;

    sprintf(query, "select * from %s where name = '%s'", extraTable, geneName);
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


void doVegaGene(struct trackDb *tdb, char *geneName)
/* Handle click on Vega gene track. */
{
struct vegaInfo *vi = NULL;

if (hTableExists("vegaInfo"))
    {
    char query[256];
    struct sqlConnection *conn = hAllocConn();
    struct sqlResult *sr;
    char **row;

    safef(query, sizeof(query),
	  "select * from vegaInfo where transcriptId = '%s'", geneName);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
	AllocVar(vi);
	vegaInfoStaticLoad(row, vi);
	}
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }

genericHeader(tdb, geneName);
// printCustomUrl(tdb, ((vi != NULL) ? vi->otterId : geneName), TRUE);
if (vi != NULL)
    {
    printf("<B>VEGA Gene Name:</B> %s<BR>\n", vi->geneId);
    printf("<B>VEGA Gene Type:</B> %s<BR>\n", vi->method);
    printf("<B>VEGA Gene Description:</B> %s<BR>\n", vi->geneDesc);
    }
geneShowCommon(geneName, tdb, "vegaPep");
printTrackHtml(tdb);
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
char *track = tdb->tableName;
struct genomicDups dup;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char oChrom[64];
int oStart;

cartWebStart(cart, "Genomic Duplications");
printf("<H2>Genomic Duplication</H2>\n");
if (cgiVarExists("o"))
    {
    int start = cartInt(cart, "o");
    int rowOffset = hOffsetPastBin(seqName, track);
    parseChromPointPos(dupName, oChrom, &oStart);

    sprintf(query, "select * from %s where chrom = '%s' and chromStart = %d "
	    "and otherChrom = '%s' and otherStart = %d",
	    track, seqName, start, oChrom, oStart);
    sr = sqlGetResult(conn, query);
    while (row = sqlNextRow(sr))
	{
	genomicDupsStaticLoad(row+rowOffset, &dup);
	printf("<B>First Position:</B> %s:%d-%d<BR>\n",
	       dup.chrom, dup.chromStart, dup.chromEnd);
	printf("<B>Second Position:</B> %s:%d-%d<BR>\n",
	       dup.otherChrom, dup.otherStart, dup.otherEnd);
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
hgcStart(item);
seq = hExtSeq(item);
printf("<PRE><TT>");
faWriteNext(stdout, item, seq->dna, seq->size);
printf("</TT></PRE>");
freeDnaSeq(&seq);
}

void doBlatMouse(struct trackDb *tdb, char *itemName)
/* Handle click on blatMouse track. */
{
char *track = tdb->tableName;
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
int start = cartInt(cart, "o");
struct psl *pslList = NULL, *psl;
struct dnaSeq *seq;
char *tiNum = strrchr(itemName, '|');
boolean hasBin;
char table[64];

/* Print heading info including link to NCBI. */
if (tiNum != NULL) 
    ++tiNum;
cartWebStart(cart, itemName);
printf("<H1>Information on Mouse %s %s</H1>", 
       (tiNum == NULL ? "Contig" : "Read"), itemName);

/* Print links to NCBI and to sequence. */
if (tiNum != NULL)
    {
    printf("Link to ");
    printf("<A HREF=\"http://www.ncbi.nlm.nih.gov/Traces/trace.cgi?val=%s\" TARGET=_blank>", tiNum);
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
    sprintf(query, "select templateId from mouseTraceInfo where ti = '%s'", itemName);
    templateId = sqlQuickQuery(conn, query, buf, sizeof(buf));
    if (templateId != NULL)
        {
	sprintf(query, "select ti from mouseTraceInfo where templateId = '%s'", templateId);
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
hFindSplitTable(seqName, track, table, &hasBin);
sprintf(query, "select * from %s where qName = '%s'", table, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row+hasBin);
    slAddHead(&pslList, psl);
    }
sqlFreeResult(&sr);
slReverse(&pslList);
printAlignments(pslList, start, "htcBlatXeno", track, itemName);
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
struct dyString *dy = newDyString(1024);
struct sqlConnection *conn = hAllocConn();
char table[64];
boolean hasBin;
struct sqlResult *sr;
char **row;
struct psl *psl;

hFindSplitTable(tName, track, table, &hasBin);
dyStringPrintf(dy, "select * from %s ", table);
dyStringPrintf(dy, "where qStart = %d ", qStart);
dyStringPrintf(dy, "and qEnd = %d ", qEnd);
dyStringPrintf(dy, "and qName = '%s' ", qName);
dyStringPrintf(dy, "and tStart = %d ", tStart);
dyStringPrintf(dy, "and tEnd = %d ", tEnd);
dyStringPrintf(dy, "and tName = '%s'", tName);
sr = sqlGetResult(conn, dy->string);
row = sqlNextRow(sr);
if (row == NULL)
    errAbort("Couldn't loadPslAt %s:%d-%d", tName, tStart, tEnd);
psl = pslLoad(row + hasBin);
sqlFreeResult(&sr);
freeDyString(&dy);
hFreeConn(&conn);
return psl;
}

struct psl *loadPslFromRangePair(char *track, char *rangePair)
/* Load a specific psl given 'qName:qStart-qEnd tName:tStart-tEnd' in rangePair. */
{
char *qRange, *tRange;
char *qName, *tName;
int qStart, qEnd, tStart, tEnd;
qRange = nextWord(&rangePair);
tRange = nextWord(&rangePair);
if (tRange == NULL)
    errAbort("Expecting two ranges in loadPslFromRangePair");
mustParseRange(qRange, &qName, &qStart, &qEnd);
mustParseRange(tRange, &tName, &tStart, &tEnd);
return loadPslAt(track, qName, qStart, qEnd, tName, tStart, tEnd);
}

void longXenoPsl1Given(struct trackDb *tdb, char *item, 
		       char *otherOrg, char *otherChromTable, char *otherDb, struct psl *psl, char *pslTableName )
/* Put up cross-species alignment when the second species
 * sequence is in a nib file, AND psl record is given. */
{
int hgsid = cartSessionId(cart);
struct psl *trimmedPsl = NULL;
char otherString[256];
char *cgiItem = cgiEncode(item);
char *thisOrg = hOrganism(database);

cartWebStart(cart, tdb->longLabel);


printf("<B>%s position:</B> <a target=\"_blank\" href=\"/cgi-bin/hgTracks?db=%s&position=%s%%3A%d-%d\">%s:%d-%d</a><BR>\n",
       otherOrg, otherDb, psl->qName, psl->qStart+1, psl->qEnd, 
       psl->qName, psl->qStart+1, psl->qEnd);

/*superceded by above code*/
//    printf("<B>%s position:</B> %s:%d-%d<BR>\n", otherOrg,
//	    psl->qName, psl->qStart+1, psl->qEnd);


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
sprintf(otherString, "%d&pslTable=%s&otherOrg=%s&otherChromTable=%s&otherDb=%s", psl->tStart, 
	pslTableName, otherOrg, otherChromTable, otherDb);

if (pslTrimToTargetRange(psl, winStart, winEnd) != NULL)
    {
    hgcAnchorSomewhere("htcLongXenoPsl2", cgiItem, otherString, psl->tName);
    printf("<BR>View details of parts of alignment within browser window</A>.<BR>\n");
    }
freez(&cgiItem);
}

/* 
   Multipurpose function to show alignments in details pages where applicable
*/
void longXenoPsl1(struct trackDb *tdb, char *item, 
		  char *otherOrg, char *otherChromTable, char *otherDb)
/* Put up cross-species alignment when the second species
 * sequence is in a nib file. */
{
struct psl *psl = NULL, *trimmedPsl = NULL;
char otherString[256];
char *cgiItem = cgiEncode(item);
char *thisOrg = hOrganism(database);

cartWebStart(cart, tdb->longLabel);
psl = loadPslFromRangePair(tdb->tableName, item);
printf("<B>%s position:</B> <a target=\"_blank\" href=\"/cgi-bin/hgTracks?db=%s&position=%s%%3A%d-%d\">%s:%d-%d</a><BR>\n",
       otherOrg, otherDb, psl->qName, psl->qStart+1, psl->qEnd, 
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
sprintf(otherString, "%d&pslTable=%s&otherOrg=%s&otherChromTable=%s&otherDb=%s", psl->tStart, 
	tdb->tableName, otherOrg, otherChromTable, otherDb);
//joni
if (pslTrimToTargetRange(psl, winStart, winEnd) != NULL)
    {
    hgcAnchorSomewhere("htcLongXenoPsl2", cgiItem, otherString, psl->tName);
    printf("<BR>View details of parts of alignment within browser window</A>.<BR>\n");
    }

if (containsStringNoCase(otherDb, "zoo"))
    {
    printf("<P><A HREF='/cgi-bin/hgTracks?db=%s'>Go to the browser view of the %s</A><BR>\n", otherDb, otherOrg);
    }

printTrackHtml(tdb);
freez(&cgiItem);
}

/* 
   Multipurpose function to show alignments in details pages where applicable
   Show the URL from trackDb as well. 
   Only used for the Chimp tracks right now.
*/
void longXenoPsl1Chimp(struct trackDb *tdb, char *item, 
		       char *otherOrg, char *otherChromTable, char *otherDb)
/* Put up cross-species alignment when the second species
 * sequence is in a nib file. */
{
struct psl *psl = NULL, *trimmedPsl = NULL;
char otherString[256];
char *cgiItem = cgiEncode(item);
char *thisOrg = hOrganism(database);

cartWebStart(cart, tdb->longLabel);
psl = loadPslFromRangePair(tdb->tableName, item);
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
sprintf(otherString, "%d&pslTable=%s&otherOrg=%s&otherChromTable=%s&otherDb=%s", psl->tStart, 
	tdb->tableName, otherOrg, otherChromTable, otherDb);

printCustomUrl(tdb, item, TRUE);
printTrackHtml(tdb);
freez(&cgiItem);
}

void longXenoPsl1zoo2(struct trackDb *tdb, char *item, 
		      char *otherOrg, char *otherChromTable)
/* Put up cross-species alignment when the second species
 * sequence is in a nib file. */
{
struct psl *psl = NULL, *trimmedPsl = NULL;
char otherString[256];
char anotherString[256];
char *cgiItem = cgiEncode(item);
char *thisOrg = hOrganism(database);

cartWebStart(cart, tdb->longLabel);
psl = loadPslFromRangePair(tdb->tableName, item);
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

sprintf(anotherString, "%s",otherOrg);
toUpperN(anotherString,1);
printf("Link to <a href=\"http://hgwdev-tcbruen.cse.ucsc.edu/cgi-bin/hgTracks?db=zoo%s1&position=chr1:%d-%d\">%s database</a><BR>\n",anotherString,psl->qStart,psl->qEnd,otherOrg);


sprintf(otherString, "%d&pslTable=%s&otherOrg=%s&otherChromTable=%s", psl->tStart, 
	tdb->tableName, otherOrg, otherChromTable);
if (pslTrimToTargetRange(psl, winStart, winEnd) != NULL)
    {
    hgcAnchorSomewhere("htcLongXenoPsl2", cgiItem, otherString, psl->tName);
    printf("<BR>View details of parts of alignment within browser window</A>.<BR>\n");
    }
printTrackHtml(tdb);
freez(&cgiItem);
}

void doAlignmentOtherDb(struct trackDb *tdb, char *item)
/* Put up cross-species alignment when the second species
 * is another db, indicated by the 3rd word of tdb->type. */
{
char *otherOrg;
char *otherDb;
char *typeLine = cloneString(tdb->type);
char *words[8];
int wordCount = chopLine(typeLine, words);
if (words[0] == NULL || words[1] == NULL || words[2] == NULL ||
    ! (sameString(words[0], "psl") &&
       sameString(words[1], "xeno")))
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
//errAbort("(%s)\n", item );


// Check to see if name is one of zoo names
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
    sprintf( chromStr, "%sChrom" , otherName );
    longXenoPsl1zoo2(tdb, item, otherName, chromStr );
    }

}



void htcGenePsl(char *htcCommand, char *item)
/* Interface for selecting & displaying alignments from axtInfo 
 * for an item from a genePred table. */
{
struct genePred *gp = NULL;
struct axtInfo *aiList = NULL, *ai;
struct axt *axtList = NULL;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char *track = cartString(cart, "o");
char *chrom = cartString(cart, "c");
char *name = cartOptionalString(cart, "i");
char *alignment = cgiOptionalString("alignment");
char *db2 = cartString(cart, "db2");
int left = cgiInt("l");
int right = cgiInt("r");
char table[64];
char query[512];
char nibFile[512];
boolean hasBin; 
boolean alignmentOK;

cartWebStart(cart, "Alignment of %s in %s to %s using %s.",
	     name, hOrganism(database), hOrganism(db2), alignment);
hSetDb2(db2);

// get nibFile
sprintf(query, "select fileName from chromInfo where chrom = '%s'",  chrom);
if (sqlQuickQuery(conn, query, nibFile, sizeof(nibFile)) == NULL)
    errAbort("Sequence %s isn't in chromInfo", chrom);

// get gp
hFindSplitTable(chrom, track, table, &hasBin);
snprintf(query, sizeof(query),
	 "select * from %s where name = '%s' and chrom = '%s' and txStart < %d and txEnd > %d",
	 table, name, chrom, right, left);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row + hasBin);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
if (gp == NULL)
    errAbort("Could not locate gene prediction (db=%s, table=%s, name=%s, in range %s:%d-%d)",
	     database, table, name, chrom, left+1, right);

puts("<FORM ACTION=\"/cgi-bin/hgc\" NAME=\"orgForm\" METHOD=\"GET\">");
cartSaveSession(cart);
cgiContinueHiddenVar("g");
cgiContinueHiddenVar("i");
cgiContinueHiddenVar("o");
cgiContinueHiddenVar("c");
cgiContinueHiddenVar("l");
cgiContinueHiddenVar("r");
puts("\n<TABLE><tr>");
puts("<td align=center valign=baseline>genome/version</td>");
puts("<td align=center valign=baseline>alignment</td>");
puts("</tr>");
puts("<tr>");
puts("<td align=center>\n");
printOrgAssemblyListAxtInfo("db2", onChangeAssemblyText);
puts("</td>\n");
puts("<td align=center>\n");
printAlignmentListHtml(db2, "alignment", alignment);
printf("</td>\n");
printf("<td align=center>");
cgiMakeButton("Submit", "Submit");
printf("</td>\n");
puts("</tr></TABLE>");
puts("</FORM>");

// Make sure alignment is not just a leftover from previous db2.
aiList = hGetAxtAlignments(db2);
if (alignment != NULL)
    {
    alignmentOK = FALSE;
    for (ai=aiList;  ai != NULL;  ai=ai->next)
	{
	if (sameString(ai->alignment, alignment))
	    {
	    alignmentOK = TRUE;
	    break;
	    }
	}
    if (! alignmentOK)
	alignment = NULL;
    }
if ((alignment == NULL) && (aiList != NULL))
    {
    alignment = aiList->alignment;
    }

puts("<PRE><TT>");
axtList = getAxtListForGene(gp, nibFile, database, db2, alignment);
axtOneGeneOut(axtList, LINESIZE, stdout , gp, nibFile);
puts("</TT></PRE>");
axtInfoFreeList(&aiList);
webEnd();
}

void htcPseudoGene(char *htcCommand, char *item)
/* Interface for selecting & displaying alignments from axtInfo 
 * for an item from a genePred table. */
{
struct genePred *gp = NULL;
struct axtInfo *aiList = NULL, *ai;
struct axt *axtList = NULL;
//struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char *track = cartString(cart, "o");
char *chrom = cartString(cart, "c");
char *name = cartOptionalString(cart, "i");
char *alignment = cgiOptionalString("alignment");
char *db2 = cartString(cart, "db2");
int tStart = cgiInt("l");
int tEnd = cgiInt("r");
char *qChrom = cgiOptionalString("qc");
int chainId = cgiInt("ci");
int qStart = cgiInt("qs");
int qEnd = cgiInt("qe");
char table[64];
char query[512];
char nibFile[512];
char qNibFile[512];
char qNibDir[512];
char tNibDir[512];
char path[512];
boolean hasBin; 
boolean alignmentOK;
struct sqlConnection *conn = hAllocConn();
struct sqlConnection *conn2;
struct hash *qChromHash = hashNew(0);
struct cnFill *fill;
struct chain *chain, *subChain, *chainToFree;
struct dnaSeq *tChrom = NULL;


cartWebStart(cart, "Alignment of %s in %s to pseudogene in %s",
	     name, hOrganism(db2), hOrganism(database));
hSetDb2(db2); 
conn2 = hAllocConn2();

// get nibFile for pseudoGene
sprintf(query, "select fileName from chromInfo where chrom = '%s'",  chrom);
if (sqlQuickQuery(conn, query, nibFile, sizeof(nibFile)) == NULL)
    errAbort("Sequence %s isn't in chromInfo", chrom);

// get nibFile for Gene in other species
sprintf(query, "select fileName from chromInfo where chrom = 'chr1'");
if (sqlQuickQuery(conn2, query, qNibFile, sizeof(qNibFile)) == NULL)
    errAbort("Sequence chr1 isn't in chromInfo");
// get gp
//hFindSplitTable(chrom, track, table, &hasBin);
hFindSplitTableDb(db2, qChrom, track, table, &hasBin);
if (sameString(track, "mrna"))
    {
    struct psl *psl = NULL ;
    safef(query, sizeof(query),
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
else
    {
    safef(query, sizeof(query),
             "select * from %s where name = '%s' and chrom = '%s' ",
             //and txStart < %d and txEnd > %d",
             table, name, qChrom
             //, qEnd, qStart
             );
    sr = sqlGetResult(conn2, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
        gp = genePredLoad(row + hasBin);
        }
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

sprintf(path, "%s/%s.nib", tNibDir, chrom);

/*
if (tChrom->size != net->size)
    errAbort("Size mismatch on %s.  Net/nib out of sync or possibly nib dirs swapped?",
        tChrom->name);
*/


/* load chain */
if (sameString(database,db2))
    strcpy(track, "selfChain");
else
    sprintf(track, "%sChain",hOrganism(db2));
track[0] = tolower(track[0]);
if (chainId > 0)
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
    axtList = netFillToAxt(fill, tChrom, hChromSize(chrom), qChromHash, qNibDir, chain, TRUE);
    /*
    if (subChain == NULL)
        subChain = chain;
    axtList = chainToAxt(subChain, qSeq, qOffset, tChrom, fill->tStart, 200);
    */

    /* fill in gaps between axt blocks */
    /* allows display of aligned coding regions */
    axtFillGap(&axtList,qNibDir);

    if (gp->strand[0] == '-')
        axtListReverse(&axtList, database);
    if (axtList != NULL) 
        if (axtList->next != NULL)
            if ((axtList->next->tStart < axtList->tStart && gp->strand[0] == '+') ||
                (axtList->next->tStart > axtList->tStart && (gp->strand[0] == '-')))
                slReverse(&axtList);

    /* output fancy formatted alignment */
    puts("<PRE><TT>");
    axtOneGeneOut(axtList, LINESIZE, stdout , gp, qNibFile);
    puts("</TT></PRE>");
    }

axtInfoFreeList(&aiList);
webEnd();
hFreeConn2(&conn2);
}

void htcLongXenoPsl2(char *htcCommand, char *item)
/* Display alignment - loading sequence from nib file. */
{
char *pslTable = cgiString("pslTable");
char *otherChromTable = cgiString("otherChromTable");
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
hSetDb2(otherDb);
if (hChromSize2(qChrom) != psl->qSize)
    errAbort("Alignment's query size for %s is %d, but the size of %s in database %s is %d.  Incorrect database in trackDb.type?",
	     qChrom, psl->qSize, qChrom, otherDb, hChromSize2(qChrom));

psl = pslTrimToTargetRange(psl, winStart, winEnd);

qSeq = loadGenomePart(otherDb, qChrom, psl->qStart, psl->qEnd);
snprintf(name, sizeof(name), "%s.%s", otherOrg, qChrom);
writeFramesetType();
puts("<HTML>");
printf("<HEAD>\n<TITLE>%s %dk</TITLE>\n</HEAD>\n\n", name, psl->qStart/1000);
showSomeAlignment(psl, qSeq, gftDnaX, psl->qStart, psl->qEnd, name, 0, 0);
}

void doBlatCompGeno(struct trackDb *tdb, char *itemName, char *otherGenome)
    /* Handle click on blat track in a generic fashion */
    /* otherGenome is the text to display for genome name on details page */
{
char *track = tdb->tableName;
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
int start = cartInt(cart, "o");
struct psl *pslList = NULL, *psl;
struct dnaSeq *seq;
boolean hasBin;
char table[64];

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
cartWebStart(cart, itemName);
printf("<H1>Information on %s Sequence %s</H1>", otherGenome, itemName);

printf("Get ");
printf("<A HREF=\"%s&g=htcExtSeq&c=%s&l=%d&r=%d&i=%s\">",
               hgcPathAndSettings(), seqName, winStart, winEnd, itemName);
printf("%s DNA</A><BR>\n", otherGenome);

/* Get alignment info and print. */
printf("<H2>Alignments</H2>\n");
hFindSplitTable(seqName, track, table, &hasBin);
sprintf(query, "select * from %s where qName = '%s'", table, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row+hasBin);
    slAddHead(&pslList, psl);
    }
sqlFreeResult(&sr);
slReverse(&pslList);
printAlignments(pslList, start, "htcBlatXeno", track, itemName);
printTrackHtml(tdb);
}

void doTSS(struct trackDb *tdb, char *itemName)
/* Handle click on DBTSS track. */
{
char *track = tdb->tableName;
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row = NULL;
int start = cartInt(cart, "o");
struct psl *pslList = NULL, *psl = NULL;
struct dnaSeq *seq = NULL;
boolean hasBin = TRUE;
char *table = "refFullAli"; /* Table with the pertinent PSL data */

cartWebStart(cart, itemName);
printf("<H1>Information on DBTSS Sequence %s</H1>", itemName);
printf("Get ");
printf("<A HREF=\"%s&g=htcExtSeq&c=%s&l=%d&r=%d&i=%s\">",
       hgcPathAndSettings(), seqName, winStart, winEnd, itemName);
printf("Sequence</A><BR>\n");

/* Get alignment info and print. */
printf("<H2>Alignments</H2>\n");
sprintf(query, "select * from %s where qName = '%s'", table, itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row + hasBin);
    slAddHead(&pslList, psl);
    }

sqlFreeResult(&sr);
slReverse(&pslList);
printAlignments(pslList, start, "htcCdnaAli", track, itemName);
printTrackHtml(tdb);
}

void doEst3(char *itemName)
/* Handle click on EST 3' end track. */
{
struct est3 el;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset;

cartWebStart(cart, "EST 3' Ends");
printf("<H2>EST 3' Ends</H2>\n");

rowOffset = hOffsetPastBin(seqName, "est3");
sprintf(query, "select * from est3 where chrom = '%s' and chromStart = %d",
	seqName, start);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    est3StaticLoad(row+rowOffset, &el);
    printf("<B>EST 3' End Count:</B> %d<BR>\n", el.estCount);
    bedPrintPos((struct bed *)&el, 3);
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

void doRnaGene(struct trackDb *tdb, char *itemName)
/* Handle click on EST 3' end track. */
{
char *track = tdb->tableName;
struct rnaGene rna;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset;

genericHeader(tdb, itemName);
rowOffset = hOffsetPastBin(seqName, track);
sprintf(query, "select * from %s where chrom = '%s' and chromStart = %d and name = '%s'",
	track, seqName, start, itemName);
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
    bedPrintPos((struct bed *)&rna, 3);
    htmlHorizontalLine();
    }
puts(tdb->html);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doStsMarker(struct trackDb *tdb, char *marker)
/* Respond to click on an STS marker. */
{
char *table = tdb->tableName;
char query[256];
char title[256];
struct sqlConnection *conn = hAllocConn();
boolean stsInfo2Exists = sqlTableExists(conn, "stsInfo2");
boolean stsInfoExists = sqlTableExists(conn, "stsInfo");
boolean stsMapExists = sqlTableExists(conn, "stsMap");
struct sqlConnection *conn1 = hAllocConn();
struct sqlResult *sr = NULL, *sr1 = NULL;
char **row, **row1;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
struct stsMap stsRow;
struct stsInfo *infoRow = NULL;
struct stsInfo2 *info2Row = NULL;
char band[32], stsid[20];
int i;
struct psl *pslList = NULL, *psl;
int pslStart;
char *sqlMarker = marker;

/* Make sure to escpae single quotes for DB parseability */
if (strchr(marker, '\''))
    {
    sqlMarker = replaceChars(marker, "'", "''");
    }

/* Print out non-sequence info */
sprintf(title, "STS Marker %s", marker);
cartWebStart(cart, title);

/* Find the instance of the object in the bed table */ 
sprintf(query, "SELECT * FROM %s WHERE name = '%s' "
               "AND chrom = '%s' AND chromStart = %d "
               "AND chromEnd = %d",
	table, sqlMarker, seqName, start, end);  
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    if (stsMapExists)
	{
	stsMapStaticLoad(row, &stsRow);
	}
    else
        /* Load and convert from original bed format */ 
	{
	struct stsMarker oldStsRow;
	stsMarkerStaticLoad(row, &oldStsRow);
	stsMapFromStsMarker(&oldStsRow, &stsRow);
	}
    if (stsInfo2Exists)
        {
	/* Find the instance of the object in the stsInfo2 table */ 
	sqlFreeResult(&sr);
	sprintf(query, "SELECT * FROM stsInfo2 WHERE identNo = '%d'", stsRow.identNo);
	sr = sqlMustGetResult(conn, query);
	row = sqlNextRow(sr);
	if (row != NULL)
	    {
	    info2Row = stsInfo2Load(row);
	    infoRow = stsInfoLoad(row);	    
	    }
	}
    else if (stsInfoExists)
        {
	/* Find the instance of the object in the stsInfo table */ 
	sqlFreeResult(&sr);
	sprintf(query, "SELECT * FROM stsInfo WHERE identNo = '%d'", stsRow.identNo);
	sr = sqlMustGetResult(conn, query);
	row = sqlNextRow(sr);
	if (row != NULL)
	    {
	    infoRow = stsInfoLoad(row);
	    }
	}
    if (((stsInfo2Exists) || (stsInfoExists)) && (row != NULL)) 
	{
	printf("<TABLE>\n");
	printf("<TR><TH ALIGN=left>Chromosome:</TH><TD>%s</TD></TR>\n", seqName);
	printf("<TR><TH ALIGN=left>Start:</TH><TD>%d</TD></TR>\n",start+1);
	printf("<TR><TH ALIGN=left>End:</TH><TD>%d</TD></TR>\n",end);
	if (hChromBand(seqName, start, band))
	    printf("<TR><TH ALIGN=left>Band:</TH><TD>%s</TD></TR>\n",band);
	printf("</TABLE>\n");
	htmlHorizontalLine();

	/* Print out marker name and links to UniSTS, Genebank, GDB */
	if (infoRow->nameCount > 0)
	    {
	    printf("<TABLE>\n");
	    printf("<TR><TH>Other names:</TH><TD>%s",infoRow->otherNames[0]);
	    for (i = 1; i < infoRow->nameCount; i++) 
		{
		printf(", %s",infoRow->otherNames[i]);
		}
	    printf("</TR>\n</TABLE>\n");
	    htmlHorizontalLine();
	    }
	printf("<TABLE>\n");
	printf("<TR><TH ALIGN=left>UCSC STS id:</TH><TD>%d</TD></TR>\n", stsRow.identNo);
	printf("<TR><TH ALIGN=left>UniSTS id:</TH><TD><A HREF=");
	printUnistsUrl(stdout, infoRow->dbSTSid);
	printf(">%d</A></TD></TR>\n", infoRow->dbSTSid);
	if (infoRow->otherDbstsCount > 0) 
	    {
	    printf("<TR><TH ALIGN=left>Related UniSTS ids:</TH>");
	    for (i = 0; i < infoRow->otherDbstsCount; i++) 
		{
		printf("<TD><A HREF=");
		printUnistsUrl(stdout, infoRow->otherDbSTS[i]);
		printf(">%d</A></TD>", infoRow->otherDbSTS[i]);
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
		printf("\">%s</A></TD>", infoRow->genbank[i]);
		}
	    printf("</TR>\n");
	    } 
	if (infoRow->gdbCount > 0) 
	    {
	    printf("<TR><TH ALIGN=left>GDB:</TH>");
	    for (i = 0; i < infoRow->gdbCount; i++) 
		{
		printf("<TD><A HREF=");
		printGdbUrl(stdout, infoRow->gdb[i]);
		printf(">%s</A></TD>", infoRow->gdb[i]);
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
		{
		printf("<TH ALIGN=left>Genethon:</TH><TD WIDTH=150>%s</TD><TD WIDTH=150>%s</TD><TD WIDTH=150>%.2f</TD></TR>\n",
		       infoRow->genethonName, infoRow->genethonChr, infoRow->genethonPos);
		}
	    if (!sameString(infoRow->marshfieldName,""))
		{
		printf("<TH ALIGN=left>Marshfield:</TH><TD WIDTH=150>%s</TD><TD WIDTH=150>%s</TD><TD WIDTH=150>%.2f</TD></TR>\n",
		       infoRow->marshfieldName, infoRow->marshfieldChr,
		       infoRow->marshfieldPos);
		}
	    if ((stsInfo2Exists) && (!sameString(info2Row->decodeName,"")))
		{
		printf("<TH ALIGN=left>deCODE:</TH><TD WIDTH=150>%s</TD><TD WIDTH=150>%s</TD><TD WIDTH=150>%.2f</TD></TR>\n",
		       info2Row->decodeName, info2Row->decodeChr,
		       info2Row->decodePos);
		}
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
		{
		printf("<TH>&nbsp</TH><TH ALIGN=left WIDTH=150>Name</TH><TH ALIGN=left WIDTH=150>Chromosome</TH><TH ALIGN=left WIDTH=150>Position (LOD)</TH></TR>\n");
		}
	    else
		{
		printf("<TH>&nbsp</TH><TH ALIGN=left WIDTH=150>Name</TH><TH ALIGN=left WIDTH=150>Chromosome</TH><TH ALIGN=left WIDTH=150>Position</TH></TR>\n");
		}
	    if (!sameString(infoRow->gm99gb4Name,""))
		{
		printf("<TH ALIGN=left>GM99 Gb4:</TH><TD WIDTH=150>%s</TD><TD WIDTH=150>%s</TD><TD WIDTH=150>%.2f (%.2f)</TD></TR>\n",
		       infoRow->gm99gb4Name, infoRow->gm99gb4Chr, infoRow->gm99gb4Pos,
		       infoRow->gm99gb4LOD);
		}
	    if (!sameString(infoRow->gm99g3Name,""))
		{
		printf("<TH ALIGN=left>GM99 G3:</TH><TD WIDTH=150>%s</TD><TD WIDTH=150>%s</TD><TD WIDTH=150>%.2f (%.2f)</TD></TR>\n",
		       infoRow->gm99g3Name, infoRow->gm99g3Chr, infoRow->gm99g3Pos,
		       infoRow->gm99g3LOD);
		}
	    if (!sameString(infoRow->wirhName,""))
		{
		printf("<TH ALIGN=left>WI RH:</TH><TD WIDTH=150>%s</TD><TD WIDTH=150>%s</TD><TD WIDTH=150>%.2f (%.2f)</TD></TR>\n",
		       infoRow->wirhName, infoRow->wirhChr, infoRow->wirhPos,
		       infoRow->wirhLOD);
		}
	    if (!sameString(infoRow->tngName,""))
		{
		printf("<TH ALIGN=left>Stanford TNG:</TH><TD WIDTH=150>%s</TD><TD WIDTH=150>%s</TD><TD WIDTH=150>%.2f</TD></TR>\n",
		       infoRow->tngName, infoRow->tngChr, infoRow->tngPos);
		}
	    printf("</TABLE><P>\n");
	    }
	/* Print out alignment information - full sequence */
	webNewSection("Genomic Alignments:");
	sprintf(query, "SELECT * FROM all_sts_seq WHERE qName = '%d'", 
		infoRow->identNo);  
	sr1 = sqlGetResult(conn1, query);
	i = 0;
	pslStart = 0;
	while ((row = sqlNextRow(sr1)) != NULL)
	    {  
	    psl = pslLoad(row);
	    if ((sameString(psl->tName, seqName)) 
		&& (abs(psl->tStart - start) < 1000))
		{
		pslStart = psl->tStart;
		}
	    slAddHead(&pslList, psl);
	    i++;
	    }
	slReverse(&pslList);
	if (i > 0) 
	    {
	    printf("<H3>Full sequence:</H3>\n");
	    sprintf(stsid,"%d",infoRow->identNo);
	    printAlignments(pslList, pslStart, "htcCdnaAli", "all_sts_seq", stsid);
	    sqlFreeResult(&sr1);
	    htmlHorizontalLine();
	    }
	slFreeList(&pslList);
	/* Print out alignment information - primers */
	sprintf(stsid,"dbSTS_%d",infoRow->dbSTSid);
	sprintf(query, "SELECT * FROM all_sts_primer WHERE qName = '%s'", 
		stsid);  
	sr1 = sqlGetResult(conn1, query);
	i = 0;
	pslStart = 0;
	while ((row = sqlNextRow(sr1)) != NULL)
	    {  
	    psl = pslLoad(row);
	    if ((sameString(psl->tName, seqName)) 
		&& (abs(psl->tStart - start) < 1000))
		{
		pslStart = psl->tStart;
		}
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
	/* printf("<H2>STS Marker %s</H2>\n", marker);
	   htmlHorizontalLine(); */
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
	    {
	    printf("<TR><TH ALIGN=left>FISH:</TH><TD>%s.%s - %s.%s</TD></TR>\n", stsRow.fishChrom, 
		   stsRow.beginBand, stsRow.fishChrom, stsRow.endBand);
	    }
	printf("</TABLE>\n");
	htmlHorizontalLine();
	if (stsRow.score == 1000)
	    {
	    printf("<H3>This is the only location found for %s</H3>\n",marker);
	    }
	else
	    {
	    sqlFreeResult(&sr);
	    printf("<H4>Other locations found for %s in the genome:</H4>\n", marker);
	    printf("<TABLE>\n");
	    sprintf(query, "SELECT * FROM %s WHERE name = '%s' "
                           "AND (chrom != '%s' OR chromStart != %d OR chromEnd != %d)",
		    table, marker, seqName, start, end); 
	    sr = sqlGetResult(conn,query);
	    while ((row = sqlNextRow(sr)) != NULL)
		{
		if (stsMapExists)
		    {
		    stsMapStaticLoad(row, &stsRow);
		    }
		else
		    /* Load and convert from original bed format */ 
		    {
		    struct stsMarker oldStsRow;
		    stsMarkerStaticLoad(row, &oldStsRow);
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
puts(tdb->html);
sqlFreeResult(&sr);
hFreeConn(&conn);
hFreeConn(&conn1);
}

void doStsMapMouse(struct trackDb *tdb, char *marker)
/* Respond to click on an STS marker. */
{
char *table = tdb->tableName;
char title[256];
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlConnection *conn1 = hAllocConn();
struct sqlResult *sr = NULL, *sr1 = NULL;
char **row, **row1;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
int hgsid = cartSessionId(cart);
struct stsMapMouse stsRow;
struct stsInfoMouse *infoRow;
char stsid[20];
int i;
struct psl *pslList = NULL, *psl;
int pslStart;

/* Print out non-sequence info */
sprintf(title, "STS Marker %s", marker);
cartWebStart(cart, title);

/* Find the instance of the object in the bed table */ 
sprintf(query, "SELECT * FROM %s WHERE name = '%s' "
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
    sprintf(query, "SELECT * FROM stsInfoMouse WHERE identNo = '%d'", stsRow.identNo);
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
/*	printf("<TR><TH ALIGN=left>MGI Marker ID:</TH><TD>MGI:%d</TD></TR>\n", infoRow->MGIMarkerID);*/
	printf("<TR><TH ALIGN=left>MGI Marker ID:</TH><TD><B>MGI:</B>");	
	printf("<A HREF = \"http://www.informatics.jax.org/searches/accession_report.cgi?id=MGI:%d\" TARGET=_blank>%d</A></TD></TR>\n", infoRow->MGIMarkerID, infoRow->MGIMarkerID);
	/*printf("<TR><TH ALIGN=left>MGI Probe ID:</TH><TD>MGI:%d</TD></TR>\n", infoRow->MGIPrimerID);*/
	printf("<TR><TH ALIGN=left>MGI Probe ID:</TH><TD><B>MGI:</B>");	
	printf("<A HREF = \"http://www.informatics.jax.org/searches/accession_report.cgi?id=MGI:%d\" TARGET=_blank>%d</A></TD></TR>\n", infoRow->MGIPrimerID, infoRow->MGIPrimerID);
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
	sprintf(stsid,"%d",infoRow->MGIPrimerID);
	sprintf(query, "SELECT * FROM all_sts_primer WHERE  qName = '%s' AND  tStart = '%d' AND tEnd = '%d'",stsid, start, end); 
	sr1 = sqlGetResult(conn1, query);
	i = 0;
	pslStart = 0;
	while ((row = sqlNextRow(sr1)) != NULL)
	    {  
	    psl = pslLoad(row);
	    if ((sameString(psl->tName, seqName)) 
		&& (abs(psl->tStart - start) < 1000))
		{
		pslStart = psl->tStart;
		}
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
	{
	printf("<H3>This is the only location found for %s</H3>\n",marker);
	}
    else
	{
	sqlFreeResult(&sr);
	printf("<H4>Other locations found for %s in the genome:</H4>\n", marker);
	printf("<TABLE>\n");
	sprintf(query, "SELECT * FROM %s WHERE name = '%s' "
                       "AND (chrom != '%s' OR chromStart != %d OR chromEnd != %d)",
		table, marker, seqName, start, end); 
	sr = sqlGetResult(conn,query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    stsMapMouseStaticLoad(row, &stsRow);
		
		
	    printf("<TR><TD>%s:</TD><TD><A HREF = \"../cgi-bin/hgc?hgsid=%d&o=%u&t=%d&g=stsMapMouse&i=%s&c=%s\" target=_blank>%d</A></TD></TR>\n",
		   stsRow.chrom, hgsid, stsRow.chromStart,stsRow.chromEnd, stsRow.name, stsRow.chrom,(stsRow.chromStart+stsRow.chromEnd)>>1);
	    }
	printf("</TABLE>\n");
	}
    }
webNewSection("Notes:");
puts(tdb->html);
sqlFreeResult(&sr);
hFreeConn(&conn);
hFreeConn(&conn1);
}



void doStsMapMouseNew(struct trackDb *tdb, char *marker)
/* Respond to click on an STS marker. */
{
char *table = tdb->tableName;
char title[256];
char query[256];
char query1[256];
struct sqlConnection *conn = hAllocConn();
struct sqlConnection *conn1 = hAllocConn();
struct sqlResult *sr = NULL, *sr1 = NULL, *sr2 = NULL;
char **row, **row1;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
int hgsid = cartSessionId(cart);
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

sprintf(title, "STS Marker %s\n", marker);
/* sprintf(title, "STS Marker <A HREF=\"http://www.informatics.jax.org/searches/marker_report.cgi?string\%%3AmousemarkerID=%s\">%s</A>\n", marker, marker); */
cartWebStart(cart, title);

/* Find the instance of the object in the bed table */ 
sprintf(query, "SELECT * FROM %s WHERE name = '%s' "
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
    sprintf(query, "SELECT * FROM stsInfoMouseNew WHERE identNo = '%d'", stsRow.identNo);
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
	if( infoRow->UiStsId != 0)
	    {
	    printf("<TR><TH ALIGN=left>UniSts Marker ID:</TH><TD><A HREF=\"http://www.ncbi.nlm.nih.gov/genome/sts/sts.cgi?uid=%d\">%d</A></TD></TR>\n", infoRow->UiStsId, infoRow->UiStsId);
	    }
	if( infoRow->MGIId != 0)
	    {
	      printf("<TR><TH ALIGN=left>MGI Marker ID:</TH><TD><B><A HREF=\"http://www.informatics.jax.org/searches/marker_report.cgi?accID=MGI%c3A%d\">%d</A></TD></TR>\n",sChar,infoRow->MGIId,infoRow->MGIId ); 
	    }
	if( strcmp(infoRow->MGIName, "") )
	    {
	    printf("<TR><TH ALIGN=left>MGI Marker Name:</TH><TD>%s</TD></TR>\n", infoRow->MGIName);
	    }
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
	  {
	    printf("<H3>Map Position</H3>\n");  
	    printf("<TABLE>\n");
	  }
	if(strcmp(infoRow->wigName, ""))
	    {
	    printf("<TR><TH>&nbsp</TH><TH ALIGN=left WIDTH=150>Name</TH><TH ALIGN=left WIDTH=150>Chromosome</TH><TH ALIGN=left WIDTH=150>Position</TH></TR>\n");
	    printf("<TR><TH ALIGN=left>&nbsp</TH><TD WIDTH=150>%s</TD><TD WIDTH=150>%s</TD><TD WIDTH=150>%.2f</TD></TR>\n",
	       infoRow->wigName, infoRow->wigChr, infoRow->wigGeneticPos); 
	    }
	if(strcmp(infoRow->mgiName, ""))
	    {
	    printf("<TR><TH>&nbsp</TH><TH ALIGN=left WIDTH=150>Name</TH><TH ALIGN=left WIDTH=150>Chromosome</TH><TH ALIGN=left WIDTH=150>Position</TH></TR>\n");
	    printf("<TR><TH ALIGN=left>&nbsp</TH><TD WIDTH=150>%s</TD><TD WIDTH=150>%s</TD><TD WIDTH=150>%.2f</TD></TR>\n",
	       infoRow->mgiName, infoRow->mgiChr, infoRow->mgiGeneticPos); 
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
	sprintf(stsid,"%d",infoRow->identNo);
	sprintf(stsPrimer, "%d_%s", infoRow->identNo, infoRow->name);
	sprintf(stsClone, "%d_%s_clone", infoRow->identNo, infoRow->name);

	/* find sts in primer alignment info */
	sprintf(query, "SELECT * FROM all_sts_primer WHERE  qName = '%s' AND  tStart = '%d' AND tEnd = '%d'",stsPrimer, start, end); 
	sr1 = sqlGetResult(conn1, query);
	i = 0;
	pslStart = 0;
	while ((row = sqlNextRow(sr1)) != NULL )
	  {  
	    psl = pslLoad(row);
	    fflush(stdout);
	    if ((sameString(psl->tName, seqName)) 
		&& (abs(psl->tStart - start) < 1000))
	      {
		pslStart = psl->tStart;
	      }
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
        sprintf(query1, "SELECT * FROM all_sts_primer WHERE  qName = '%s' AND  tStart = '%d' AND tEnd = '%d'",stsClone, start, end);
	sr2 = sqlGetResult(conn1, query1);
	i = 0;
	pslStart = 0;
	while ((row = sqlNextRow(sr2)) != NULL )
	  {  
	    psl = pslLoad(row);
	    fflush(stdout);
	    if ((sameString(psl->tName, seqName)) 
		&& (abs(psl->tStart - start) < 1000))
	      {
		pslStart = psl->tStart;
	      }
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
	    {
	    printf("<H3>This is the only location found for %s</H3>\n",marker);
            }
        else
	    {
	    sqlFreeResult(&sr);
            printf("<H4>Other locations found for %s in the genome:</H4>\n", marker);
            printf("<TABLE>\n");
	    sprintf(query, "SELECT * FROM %s WHERE name = '%s' "
		"AND (chrom != '%s' OR chromStart != %d OR chromEnd != %d)",
	            table, marker, seqName, start, end); 
            sr = sqlGetResult(conn,query);
            while ((row = sqlNextRow(sr)) != NULL)
		{
		stsMapMouseNewStaticLoad(row, &stsRow);
		printf("<TR><TD>%s:</TD><TD><A HREF = \"../cgi-bin/hgc?hgsid=%d&o=%u&t=%d&g=stsMapMouseNew&i=%s&c=%s\" target=_blank>%d</A></TD></TR>\n",
		       stsRow.chrom, hgsid, stsRow.chromStart,stsRow.chromEnd, stsRow.name, stsRow.chrom,(stsRow.chromStart+stsRow.chromEnd)>>1);
		}
	    printf("</TABLE>\n");
	    }
    }
webNewSection("Notes:");
puts(tdb->html);
sqlFreeResult(&sr);
hFreeConn(&conn);
hFreeConn(&conn1);
}




void doStsMapRat(struct trackDb *tdb, char *marker)
/* Respond to click on an STS marker. */
{
char *table = tdb->tableName;
char title[256];
char query[256];
char query1[256];
struct sqlConnection *conn = hAllocConn();
struct sqlConnection *conn1 = hAllocConn();
struct sqlResult *sr = NULL, *sr1 = NULL, *sr2 = NULL;
char **row, **row1;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
int hgsid = cartSessionId(cart);
struct stsMapRat stsRow;
struct stsInfoRat *infoRow;
char stsid[20];
char stsPrimer[40];
char stsClone[45];
int i;
struct psl *pslList = NULL, *psl;
int pslStart;

/* Print out non-sequence info */
sprintf(title, "STS Marker %s", marker);
cartWebStart(cart, title);

/* Find the instance of the object in the bed table */ 
sprintf(query, "SELECT * FROM %s WHERE name = '%s' "
               "AND chrom = '%s' AND chromStart = %d "
               "AND chromEnd = %d",
	table, marker, seqName, start, end);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    stsMapRatStaticLoad(row, &stsRow);
    /* Find the instance of the object in the stsInfo table */ 
    sqlFreeResult(&sr);
    sprintf(query, "SELECT * FROM stsInfoRat WHERE identNo = '%d'", stsRow.identNo);
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
	if( infoRow->UiStsId != 0)
	    {
	    printf("<TR><TH ALIGN=left>UniSts Marker ID:</TH><TD><A HREF=\"http://www.ncbi.nlm.nih.gov/genome/sts/sts.cgi?uid=%d\">%d</A></TD></TR>\n", infoRow->UiStsId, infoRow->UiStsId);
	    }
	if( infoRow->RGDId != 0)
	    {
	      printf("<TR><TH ALIGN=left>RGD Marker ID:</TH><TD><B><A HREF=\"http://rgd.mcw.edu/tools/query/query.cgi?id=%d\">%d</A></TD></TR>\n",infoRow->RGDId,infoRow->RGDId ); 
	    }
	if( strcmp(infoRow->RGDName, "") )
	    {
	    printf("<TR><TH ALIGN=left>RGD Marker Name:</TH><TD>%s</TD></TR>\n", infoRow->RGDName);
	    }
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
	    {
	    printf("<H3>Map Position</H3>\n");  
	    printf("<TABLE>\n");
	    }
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
	sprintf(stsid,"%d",infoRow->identNo);
	sprintf(stsPrimer, "%d_%s", infoRow->identNo, infoRow->name);
	sprintf(stsClone, "%d_%s_clone", infoRow->identNo, infoRow->name);

	/* find sts in primer alignment info */
	sprintf(query, "SELECT * FROM all_sts_primer WHERE  qName = '%s' AND  tStart = '%d' AND tEnd = '%d'",stsPrimer, start, end); 
	sr1 = sqlGetResult(conn1, query);
	i = 0;
	pslStart = 0;
	while ((row = sqlNextRow(sr1)) != NULL )
	    {  
	    psl = pslLoad(row);
	    fflush(stdout);
	    if ((sameString(psl->tName, seqName)) 
		&& (abs(psl->tStart - start) < 1000))
		{
		pslStart = psl->tStart;
		}
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
        sprintf(query1, "SELECT * FROM all_sts_primer WHERE  qName = '%s' AND  tStart = '%d' AND tEnd = '%d'",stsClone, start, end);
	sr2 = sqlGetResult(conn1, query1);
	i = 0;
	pslStart = 0;
	while ((row = sqlNextRow(sr2)) != NULL )
	    {  
	    psl = pslLoad(row);
	    fflush(stdout);
	    if ((sameString(psl->tName, seqName)) 
		&& (abs(psl->tStart - start) < 1000))
		{
		pslStart = psl->tStart;
		}
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
	{
	printf("<H3>This is the only location found for %s</H3>\n",marker);
	}
    else
	{
	sqlFreeResult(&sr);
	printf("<H4>Other locations found for %s in the genome:</H4>\n", marker);
	printf("<TABLE>\n");
	sprintf(query, "SELECT * FROM %s WHERE name = '%s' "
                       "AND (chrom != '%s' OR chromStart != %d OR chromEnd != %d)",
		table, marker, seqName, start, end); 
	sr = sqlGetResult(conn,query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    stsMapRatStaticLoad(row, &stsRow);
	    printf("<TR><TD>%s:</TD><TD><A HREF = \"../cgi-bin/hgc?hgsid=%d&o=%u&t=%d&g=stsMapRat&i=%s&c=%s\" target=_blank>%d</A></TD></TR>\n",
		   stsRow.chrom, hgsid, stsRow.chromStart,stsRow.chromEnd, stsRow.name, stsRow.chrom,(stsRow.chromStart+stsRow.chromEnd)>>1);
	    }
	printf("</TABLE>\n");
	}
    }
webNewSection("Notes:");
puts(tdb->html);
sqlFreeResult(&sr);
hFreeConn(&conn);
hFreeConn(&conn1);
}





void doFishClones(struct trackDb *tdb, char *clone)
/* Handle click on the FISH clones track */
{
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
struct fishClones *fc;
char sband[32], eband[32];
int i;

/* Print out non-sequence info */
cartWebStart(cart, clone);


/* Find the instance of the object in the bed table */ 
sprintf(query, "SELECT * FROM fishClones WHERE name = '%s' "
               "AND chrom = '%s' AND chromStart = %d "
                "AND chromEnd = %d",
	clone, seqName, start, end);  
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    boolean gotS, gotB;
    fc = fishClonesLoad(row);
    /* Print out general sequence positional information */
    printf("<H2><A HREF=");
    printCloneRegUrl(stdout, clone);
    printf(">%s</A></H2>\n", clone);
    htmlHorizontalLine();
    printf("<TABLE>\n");
    printf("<TR><TH ALIGN=left>Chromosome:</TH><TD>%s</TD></TR>\n", seqName);
    printf("<TR><TH ALIGN=left>Start:</TH><TD>%d</TD></TR>\n",start+1);
    printf("<TR><TH ALIGN=left>End:</TH><TD>%d</TD></TR>\n",end);
    gotS = hChromBand(seqName, start, sband);
    gotB = hChromBand(seqName, end, eband);
    if (gotS && gotB)
	{
	if (sameString(sband,eband)) 
	    {
	    printf("<TR><TH ALIGN=left>Band:</TH><TD>%s</TD></TR>\n",sband);
	    }
	else
	    {
	    printf("<TR><TH ALIGN=left>Bands:</TH><TD>%s - %s</TD></TR>\n",sband, eband);
	    }
	}
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
	    printf("\">%s</A></TD>", fc->accNames[i]);	  
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
	    printf("\">%s</A></TD>", fc->beNames[i]);
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
	    printf("<TR><TD WIDTH=100 ALIGN=center>%s</TD><TD ALIGN=center>%s - %s</TD></TR>",
		   fc->labs[i], fc->bandStarts[i], fc->bandEnds[i]);
	    }
	}

    }
printf("</TABLE>\n"); 
webNewSection("Notes:");
puts(tdb->html);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doRecombRate(struct trackDb *tdb)
/* Handle click on the Recombination Rate track */
{
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
struct recombRate *rr;
char sband[32], eband[32];
int i;

/* Print out non-sequence info */
cartWebStart(cart, "Recombination Rates");


/* Find the instance of the object in the bed table */ 
sprintf(query, "SELECT * FROM recombRate WHERE "
               "chrom = '%s' AND chromStart = %d "
               "AND chromEnd = %d",
	seqName, start, end);  
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    boolean gotS, gotB;
    rr = recombRateLoad(row);
    /* Print out general sequence positional information */
    printf("<TABLE>\n");
    printf("<TR><TH ALIGN=left>Chromosome:</TH><TD>%s</TD></TR>\n", seqName);
    printf("<TR><TH ALIGN=left>Start:</TH><TD>%d</TD></TR>\n",start+1);
    printf("<TR><TH ALIGN=left>End:</TH><TD>%d</TD></TR>\n",end);
    gotS = hChromBand(seqName, start, sband);
    gotB = hChromBand(seqName, end, eband);
    if (gotS && gotB)
	{
	if (sameString(sband,eband)) 
	    {
	    printf("<TR><TH ALIGN=left>Band:</TH><TD>%s</TD></TR>\n",sband);
	    }
	else
	    {
	    printf("<TR><TH ALIGN=left>Bands:</TH><TD>%s - %s</TD></TR>\n",sband, eband);
	    }
	}
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
puts(tdb->html);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doGenMapDb(struct trackDb *tdb, char *clone)
/* Handle click on the GenMapDb clones track */
{
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
struct genMapDb *upc;
char sband[32], eband[32];
int i,size;

/* Print out non-sequence info */
cartWebStart(cart, "GenMapDB BAC Clones");

/* Find the instance of the object in the bed table */ 
sprintf(query, "SELECT * FROM genMapDb WHERE name = '%s' "
               "AND chrom = '%s' AND chromStart = %d "
               "AND chromEnd = %d",
	clone, seqName, start, end);  
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    boolean gotS, gotB;
    upc = genMapDbLoad(row);
    /* Print out general sequence positional information */
    printf("<H2><A HREF=");
    printGenMapDbUrl(stdout, clone);
    printf(">%s</A></H2>\n", clone);
    htmlHorizontalLine();
    printf("<TABLE>\n");
    printf("<TR><TH ALIGN=left>Chromosome:</TH><TD>%s</TD></TR>\n", seqName);
    printf("<TR><TH ALIGN=left>Start:</TH><TD>%d</TD></TR>\n",start+1);
    printf("<TR><TH ALIGN=left>End:</TH><TD>%d</TD></TR>\n",end);
    size = end - start + 1;
    printf("<TR><TH ALIGN=left>Size:</TH><TD>%d</TD></TR>\n",size);
    gotS = hChromBand(seqName, start, sband);
    gotB = hChromBand(seqName, end, eband);
    if (gotS && gotB)
	{
	if (sameString(sband,eband)) 
	    {
	    printf("<TR><TH ALIGN=left>Band:</TH><TD>%s</TD></TR>\n",sband);
	    }
	else
	    {
	    printf("<TR><TH ALIGN=left>Bands:</TH><TD>%s - %s</TD></TR>\n",sband, eband);
	    }
	}
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
	printf("\">%s</A></TD>", upc->accT7);
	printf("<TD>%s:</TD><TD ALIGN=right>%d</TD><TD ALIGN=LEFT> - %d</TD>", 
	       seqName, upc->startT7, upc->endT7);
	printf("</TR>\n");
	}
    if (upc->accSP6) 
	{
	printf("<TR><TH ALIGN=left>SP6 end sequence:</TH>");
	printf("<TD><A HREF=\"");
	printEntrezNucleotideUrl(stdout, upc->accSP6);
	printf("\">%s</A></TD>", upc->accSP6);
	printf("<TD>%s:</TD><TD ALIGN=right>%d</TD><TD ALIGN=LEFT> - %d</TD>", 
	       seqName, upc->startSP6, upc->endSP6);
	printf("</TR>\n");
	}
    if (upc->stsMarker) 
	{
	printf("<TR><TH ALIGN=left>STS Marker:</TH>");
	printf("<TD><A HREF=\"");
	printEntrezUniSTSUrl(stdout, upc->stsMarker);
	printf("\">%s</A></TD>", upc->stsMarker);
	printf("<TD>%s:</TD><TD ALIGN=right>%d</TD><TD ALIGN=LEFT> - %d</TD>", 
	       seqName, upc->stsStart, upc->stsEnd);
	printf("</TR>\n");
	}
    printf("</TABLE>\n");
    }
webNewSection("Notes:");
puts(tdb->html);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doMouseOrthoDetail(struct trackDb *tdb, char *itemName)
/* Handle click on mouse synteny track. */
{
char *track = tdb->tableName;
struct mouseSyn el;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset;

cartWebStart(cart, "Mouse Synteny");
printf("<H2>Mouse Synteny</H2>\n");

sprintf(query, "select * from %s where chrom = '%s' and chromStart = %d",
	track, seqName, start);
rowOffset = hOffsetPastBin(seqName, track);
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
char *track = tdb->tableName;
struct mouseSyn el;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset;

cartWebStart(cart, "Mouse Synteny");
printf("<H2>Mouse Synteny</H2>\n");

sprintf(query, "select * from %s where chrom = '%s' and chromStart = %d",
	track, seqName, start);
rowOffset = hOffsetPastBin(seqName, track);
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
char *track = tdb->tableName;
struct mouseSynWhd el;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset;

cartWebStart(cart, "Mouse Synteny (Whitehead)");
printf("<H2>Mouse Synteny (Whitehead)</H2>\n");

sprintf(query, "select * from %s where chrom = '%s' and chromStart = %d",
	track, seqName, start);
rowOffset = hOffsetPastBin(seqName, track);
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
char *track = tdb->tableName;
struct ensPhusionBlast el;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char *org = hOrganism(database);
char *tbl = cgiUsualString("table", cgiString("g"));
char *elname, *ptr, *xenoDb, *xenoOrg, *xenoChrom;
char query[256];
int rowOffset;

cartWebStart(cart, "%s", tdb->longLabel);
printf("<H2>%s</H2>\n", tdb->longLabel);

sprintf(query, "select * from %s where chrom = '%s' and chromStart = %d",
	track, seqName, start);
rowOffset = hOffsetPastBin(seqName, track);
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
	printf("<A HREF=\"%s?%s&db=%s&position=%s:%d-%d\" TARGET=_BLANK>%s Genome Browser</A> at %s:%d-%d <BR>\n",
	       hgTracksName(), cartSidUrlString(cart),
	       xenoDb, xenoChrom, el.xenoStart, el.xenoEnd,
	       xenoOrg, xenoChrom, el.xenoStart, el.xenoEnd);

	}
    printf("<A HREF=\"%s&o=%d&g=getDna&i=mixed&c=%s&l=%d&r=%d&strand=%s&table=%s\">"
	   "View DNA for this feature</A><BR>\n",  hgcPathAndSettings(),
	   el.chromStart, el.chrom, el.chromStart, el.chromEnd, el.strand, tbl);
    freez(&elname);
    }
printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doDbSnpRS(char *name)
/* print additional SNP details */
{
struct sqlConnection *conn = sqlConnect("hgFixed");
char query[256];
struct dbSnpRS *snp=NULL;
snprintf(query, sizeof(query),
	 "select rsId, "
	 "       avHet, "
	 "       avHetSE, "
	 "       valid, "
	 "       base1, "
	 "       base2, "
	 "       assembly, "
	 "       alternate "
	 "from   dbSnpRS "
	 "where  rsId = '%s'", name);
snp = dbSnpRSLoadByQuery(conn, query);
if (snp!=NULL)
    {
    printf("<BR>\n");
    if(snp->avHetSE>0)
	{
	printf("<B>Average Heterozygosity:</B> %f<BR>\n",snp->avHet);
	printf("<B>Standard Error of Average Heterozygosity:</B> %f<BR>\n",snp->avHetSE);
	}
    else
	{
	printf("<B>Average Heterozygosity:</B> Not Known<BR>\n");
	printf("<B>Standard Error of Average Heterozygosity:</B> Not Known<BR>\n");
	}
    printf("<B>Validation Status:</B> <font face=\"Courier\">%s<BR></font>\n",snp->valid);
    printf("<B>Allele1:          </B> <font face=\"Courier\">%s<BR></font>\n",snp->allele1);
    printf("<B>Allele2:          </B> <font face=\"Courier\">%s<BR>\n",snp->allele2);
    printf("Sequence in Assembly:&nbsp;%s<BR>\n",snp->assembly);
    printf("Alternate Sequence:&nbsp;&nbsp;&nbsp;%s<BR></font>\n",snp->alternate);
    }
/* else printf("<BR>%s<BR>\n",query);*/
dbSnpRSFree(&snp);
sqlDisconnect(&conn);
}

void doSnpLocusLink(struct trackDb *tdb, char *name)
/* print link to Locus Link for this SNP */
{
char *group = tdb->tableName;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[512];
int rowOffset;
snprintf(query, sizeof(query),
         "select distinct        "
         "       rl.locusLinkID, "
         "       rl.name,        "
         "       kg.name,        "
         "       kg.proteinID    "
         "from   knownGene  kg,  "
         "       refLink    rl,  "
         "       %s         snp, "
	 "       mrnaRefseq mrs  "
         "where  snp.chrom  = kg.chrom       "
	 "  and  kg.name    = mrs.mrna       "
         "  and  mrs.refSeq = rl.mrnaAcc     "
         "  and  kg.txStart < snp.chromStart "
         "  and  kg.txEnd   > snp.chromEnd   "
         "  and  snp.name   = '%s'", group, name);
rowOffset = hOffsetPastBin(seqName, group);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    printf("<P><A HREF=\"http://www.ncbi.nlm.nih.gov/SNP/snp_ref.cgi?");
    printf("locusId=%s\" TARGET=_blank>LocusLink for ", row[0]);
    printf("%s (%s; %s)</A></P>\n", row[1], row[2], row[3]);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doSnp(struct trackDb *tdb, char *itemName)
/* Put up info on a SNP. */
{
char *group = tdb->tableName;
char *ncbiName = itemName;
struct snp snp;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset;

ncbiName += 2;
cartWebStart(cart, "Single Nucleotide Polymorphism (SNP)");
printf("<H2>Single Nucleotide Polymorphism (SNP) %s</H2>\n", itemName);
sprintf(query, "select * "
	       "from   %s "
	       "where  chrom = '%s' "
	       "  and  chromStart = %d "
	       "  and name = '%s'",
        group, seqName, start, itemName);
rowOffset = hOffsetPastBin(seqName, group);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    snpStaticLoad(row+rowOffset, &snp);
    bedPrintPos((struct bed *)&snp, 3);
    }
doDbSnpRS(ncbiName);
printf("<P><A HREF=\"http://www.ncbi.nlm.nih.gov/SNP/snp_ref.cgi?");
printf("type=rs&rs=%s\" TARGET=_blank>dbSNP link</A></P>\n", snp.name);
doSnpLocusLink(tdb, itemName);
printTrackHtml(tdb);
sqlFreeResult(&sr);
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
sprintf(buf, "species=%s&tc=%s ", animal, id);
genericClickHandler(tdb, item, buf);
}


void doJaxQTL(struct trackDb *tdb, char *item)
/* Put up info on Quantitative Trait Locus from Jackson Labs. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char query[256];
char **row;
int start = cartInt(cart, "o");
struct jaxQTL *jaxQTL;
char band[64];

genericHeader(tdb, item);
sprintf(query, "select * from jaxQTL where name = '%s' and chrom = '%s' and chromStart = %d",
    	item, seqName, start);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    jaxQTL = jaxQTLLoad(row);
    printCustomUrl(tdb, jaxQTL->mgiID, FALSE);
    printf("<B>QTL:</B> %s<BR>\n", jaxQTL->name);
    printf("<B>Description:</B> %s <BR>\n", jaxQTL->description);
    printf("<B>cM position of marker associated with peak LOD score:</B> %3.1f<BR>\n", jaxQTL->cMscore);
    printf("<B>MIT SSLP marker with highest correlation:</B> %s<BR>",
	   jaxQTL->marker);
    printf("<B>Chromosome:</B> %s<BR>\n", skipChr(seqName));
    if (hChromBand(seqName, start, band))
	printf("<B>Band:</B> %s<BR>\n", band);
    printf("<B>Start of marker in chromosome:</B> %d<BR>\n", start+1);
    }
printTrackHtml(tdb);

sqlFreeResult(&sr);
hFreeConn(&conn);
}

void doGbProtAnn(struct trackDb *tdb, char *item)
/* Show extra info for GenBank Protein Annotations track. */
{
struct sqlConnection *conn  = hAllocConn();
struct sqlConnection *conn2 = hAllocConn();
struct sqlResult *sr;
char query[256];
char **row;

char query2[256];
struct sqlResult *sr2, *sr5;
char **row2;

char cond_str[256];
char *predFN;

int start = cartInt(cart, "o");
struct gbProtAnn *gbProtAnn;
char band[64];
char *homologID;
char *SCOPdomain;
char *chain;
char goodSCOPdomain[40];
int  first = 1;
float  eValue;
float ff;
char *chp;
int homologCount;

genericHeader(tdb, item);
sprintf(query, "select * from gbProtAnn where name = '%s' and chrom = '%s' and chromStart = %d",
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
    printf("<A HREF=\"http://www.ncbi.nlm.nih.gov/entrez/viewer.fcgi?val=%s\"", 
	    gbProtAnn->proteinId);
    printf(" TARGET=_blank>%s</A><BR>\n", gbProtAnn->proteinId);

    htmlHorizontalLine();
    showSAM_T02(gbProtAnn->proteinId);
    
    printPos(seqName, gbProtAnn->chromStart, gbProtAnn->chromEnd, "+", TRUE);
    }
printTrackHtml(tdb);

sqlFreeResult(&sr);
hFreeConn(&conn);
}

void printOtherLFS(char *clone, char *table, int start, int end)
/* Print out the other locations of this clone */
{
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
struct lfs *lfs;

printf("<H4>Other locations found for %s in the genome:</H4>\n", clone);
printf("<TABLE>\n");
sprintf(query, "SELECT * FROM %s WHERE name = '%s' "
               "AND (chrom != '%s' "
               "OR chromStart != %d OR chromEnd != %d)",
	table, clone, seqName, start, end); 
sr = sqlGetResult(conn,query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    lfs = lfsLoad(row+1);
    printf("<TR><TD>%s:</TD><TD>%d</TD><TD>-</TD><TD>%d</TD></TR>\n",
	   lfs->chrom, lfs->chromStart, lfs->chromEnd);
    lfsFree(&lfs);
    }
printf("</TABLE>\n"); 
sqlFreeResult(&sr);
hgFreeConn(&conn);
}

void doLinkedFeaturesSeries(char *track, char *clone, struct trackDb *tdb)
/* Create detail page for linked features series tracks */ 
{
char query[256];
char title[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL, *sr1 = NULL;
char **row, **row1;
char *type = NULL, *lfLabel = NULL;
char *table = NULL;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
int length = end - start;
int i;
struct lfs *lfs, *lfsList = NULL;
struct psl *pslList = NULL, *psl;
char sband[32], eband[32];
boolean gotS, gotB;

/* Determine type */
if (sameString("bacEndPairs", track)) 
    {
    sprintf(title, "Location of %s using BAC end sequences", clone);
    lfLabel = "BAC ends";
    table = track;
    }
if (sameString("bacEndPairsBad", track)) 
    {
    sprintf(title, "Location of %s using BAC end sequences", clone);
    lfLabel = "BAC ends";
    table = track;
    }
if (sameString("bacEndPairsLong", track)) 
    {
    sprintf(title, "Location of %s using BAC end sequences", clone);
    lfLabel = "BAC ends";
    table = track;
    }
if (sameString("fosEndPairs", track)) 
    {
    sprintf(title, "Location of %s using fosmid end sequences", clone);
    lfLabel = "Fosmid ends";
    table = track;
    }
if (sameString("fosEndPairsBad", track)) 
    {
    sprintf(title, "Location of %s using fosmid end sequences", clone);
    lfLabel = "Fosmid ends";
    table = track;
    }
if (sameString("fosEndPairsLong", track)) 
    {
    sprintf(title, "Location of %s using fosmid end sequences", clone);
    lfLabel = "Fosmid ends";
    table = track;
    }

/* Print out non-sequence info */
cartWebStart(cart, title);

/* Find the instance of the object in the bed table */ 
sprintf(query, "SELECT * FROM %s WHERE name = '%s' "
               "AND chrom = '%s' AND chromStart = %d "
               "AND chromEnd = %d",
	table, clone, seqName, start, end);  
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    lfs = lfsLoad(row+1);
    if (sameString("bacEndPairs", track)) 
	{
	printf("<H2><A HREF=");
	printCloneRegUrl(stdout, clone);
	printf(">%s</A></H2>\n", clone);
	}
    else 
	{
	printf("<B>%s</B>\n", clone);
	}
    /*printf("<H2>%s - %s</H2>\n", type, clone);*/
    printf("<P><HR ALIGN=\"CENTER\"></P>\n<TABLE>\n");
    printf("<TR><TH ALIGN=left>Chromosome:</TH><TD>%s</TD></TR>\n",seqName);
    printf("<TR><TH ALIGN=left>Start:</TH><TD>%d</TD></TR>\n",start+1);
    printf("<TR><TH ALIGN=left>End:</TH><TD>%d</TD></TR>\n",end);
    printf("<TR><TH ALIGN=left>Length:</TH><TD>%d</TD></TR>\n",length);
    printf("<TR><TH ALIGN=left>Strand:</TH><TD>%s</TD></TR>\n", lfs->strand);
    gotS = hChromBand(seqName, start, sband);
    gotB = hChromBand(seqName, end, eband);
    if (gotS && gotB)
	{
	if (sameString(sband,eband)) 
	    {
	    printf("<TR><TH ALIGN=left>Band:</TH><TD>%s</TD></TR>\n",sband);
	    }
	else
	    {
	    printf("<TR><TH ALIGN=left>Bands:</TH><TD>%s - %s</TD></TR>\n",sband, eband);
	    }
	}
    printf("</TABLE>\n");
    printf("<P><HR ALIGN=\"CENTER\"></P>\n");
    if (lfs->score == 1000)
        {
	printf("<H4>This is the only location found for %s</H4>\n",clone);
	}
    else
        {
	printOtherLFS(clone, table, start, end);
	}

    sprintf(title, "Genomic alignments of %s:", lfLabel);
    webNewSection(title);
    
    for (i = 0; i < lfs->lfCount; i++) 
	{
	sqlFreeResult(&sr);
	sprintf(query, "SELECT * FROM %s WHERE qName = '%s'", 
	        lfs->pslTable, lfs->lfNames[i]);  
	sr = sqlMustGetResult(conn, query);
	while ((row1 = sqlNextRow(sr)) != NULL)
	    {
	    psl = pslLoad(row1);
	    slAddHead(&pslList, psl);
	    }
	slReverse(&pslList);
	if (!sameString("fosEndPairs", track)) 
	    {
	    printf("<H3><A HREF=");
	    printEntrezNucleotideUrl(stdout, lfs->lfNames[i]);
	    printf(">%s</A></H3>\n",lfs->lfNames[i]);
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
webNewSection("Notes:");
puts(tdb->html);
hgFreeConn(&conn);
} 

void fillCghTable(int type, char *tissue, boolean bold)
/* Get the requested records from the database and print out HTML table */
{
char query[256];
char currName[64];
int rowOffset;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
struct cgh *cghRow;

if (tissue)
    {
    sprintf(query, "type = %d AND tissue = '%s' ORDER BY name, chromStart", type, tissue);
    }
else 
    {
    sprintf(query, "type = %d ORDER BY name, chromStart", type);
    }
     
sr = hRangeQuery(conn, "cgh", seqName, winStart, winEnd, query, &rowOffset);
while (row = sqlNextRow(sr)) 
    {
    cghRow = cghLoad(row);
    if (strcmp(currName,cghRow->name))
	{
	if (bold) 
	    {    
	    printf("</TR>\n<TR>\n<TH>%s</TH>\n",cghRow->name);
	    } 
	else
	    {    
	    printf("</TR>\n<TR>\n<TD>%s</TD>\n",cghRow->name);
	    } 
	strcpy(currName,cghRow->name);
	}
    if (bold)
	{
	printf("<TH ALIGN=right>%.6f</TH>\n",cghRow->score);
	}
    else
	{
	printf("<TD ALIGN=right>%.6f</TD>\n",cghRow->score);
	}
    }
sqlFreeResult(&sr);
}



/*Ewan's stuff */

void doCeleraDupPositive(struct trackDb *tdb, char *dupName)
/* Handle click on celeraDupPositive track. */
{
char *track = tdb->tableName;
struct celeraDupPositive dup;
char query[512];
char title[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
sprintf(title, "Segmental Duplication Database");
cartWebStart(cart, title);

printf("<H2>Segmental Duplication Database (SDD)</H2>\n");


if (cgiVarExists("o"))
    {
    int start = cgiInt("o");
    int rowOffset = hOffsetPastBin(seqName, track);



    sprintf(query, "select * from %s where chrom = '%s' and chromStart = %d and name= '%s'",
	    track, seqName, start ,dupName);
    sr = sqlGetResult(conn, query);
    while (row = sqlNextRow(sr))
	{
	celeraDupPositiveStaticLoad(row+rowOffset, &dup);
	printf("<B>Duplication Name:</B> %s<BR>\n",dup.name);
	printf("<B>Position:</B> %s:%d-%d<BR>\n",
	       dup.chrom, dup.chromStart, dup.chromEnd);
	printf("<B>Full Descriptive Name:</B> %s<BR>\n", dup.fullName);

	printf("<B>Fraction BP Match:</B> %3.4f<BR>\n", dup.fracMatch);
	printf("<B>Alignment Length:</B> %3.0f<BR>\n", dup.bpAlign);


	htmlHorizontalLine();
	printf("<A HREF=\"http://humanparalogy.cwru.edu/eichler/celera3/cgi-bin/celera3.pl?search=%s&type=pdf \" Target=%s_PDF><B>Clone Read Depth PDF:</B></A><BR>",dup.name,dup.name);
	printf("<A HREF=\"http://humanparalogy.cwru.edu/eichler/celera3/cgi-bin/celera3.pl?search=%s&type=jpg \" Target=%s_JPG><B>Clone Read Depth JPG:</B></A><BR>",dup.name,dup.name);
	htmlHorizontalLine();
	}
    }
else
    {
    puts("<P>Click directly on a repeat for specific information on that repeat</P>");
    }

puts("This region shows similarity > 90\% and >250 bp of repeatmasked sequence to sequences"
     "in the Segmental Duplication Database (SDD).");
hFreeConn(&conn);
webEnd();
}

void doCeleraCoverage()
/* Handle click on celeraCoverage track. */
{
//char *track = tdb->tableName;
//struct celeraDupPositive dup;
//char query[512];
//struct sqlConnection *conn = hAllocConn();
//struct sqlResult *sr;
//char **row;
char title[256];
sprintf(title, "CoverageSDD");
cartWebStart(cart, title);

printf("<H2>Coverage SDD</H2>\n");

puts("This track represents coverage of clones that were assayed for "
     "duplications with Celera reads. Absent regions were not assessed "
     "by this version of the SDD. ");
webEnd();
}



void parseSuperDupsChromPointPos(char *pos, char *retChrom, int *retPos, int *retID)
/* Parse out NNNN.chrN:123 into NNNN and chrN and 123. */
{
char *idStr, *periodPlaceHolder, *colonPlaceHolder,*chromStr, *posStr;

int idLen;
int chrLen;
periodPlaceHolder = strchr(pos, '.');
if (periodPlaceHolder == NULL)
    errAbort("No . in chromosome point position %s", pos);
idLen = periodPlaceHolder - pos;
idStr=malloc(idLen+1);
memcpy(idStr, pos, idLen);
idStr[idLen] = 0;
*retID=atoi(idStr);
free(idStr);



chromStr=periodPlaceHolder+1;

colonPlaceHolder=strchr(pos,':');
if (colonPlaceHolder == NULL)
    errAbort("No : in chromosome point position %s", pos);
chrLen=colonPlaceHolder-chromStr;



memcpy(retChrom,chromStr,chrLen);
retChrom[chrLen]=0;
posStr=colonPlaceHolder+1;
*retPos= atoi(posStr);

}


void doGenomicSuperDups(struct trackDb *tdb, char *dupName)
/* Handle click on genomic dup track. */
{
char *track = tdb->tableName;
struct genomicSuperDups dup;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char oChrom[64];

int oStart;
int dupId; /*Holds the duplication id*/
int rowOffset;
char title[256];
char *tbl = cgiUsualString("table", cgiString("g"));
sprintf(title, "Genome Duplications");
cartWebStart(cart, title);

printf("<H2>Genome Duplications</H2>\n");
if (cgiVarExists("o"))
    {
    int start = cgiInt("o");
    rowOffset = hOffsetPastBin(seqName, track);

    parseSuperDupsChromPointPos(dupName,oChrom,&oStart,&dupId);


    safef(query, sizeof(query), "select * from %s where chrom = '%s' and chromStart = %d "
	    "and uid = %d and otherStart = %d",
	    track, seqName, start, dupId, oStart);
    sr = sqlGetResult(conn, query);
    while (row = sqlNextRow(sr))
	{

	genomicSuperDupsStaticLoad(row+rowOffset, &dup);
	printf("<B>Current Position:</B> %s:%d-%d\n &nbsp;&nbsp;&nbsp;",
	       dup.chrom, dup.chromStart+1, dup.chromEnd);
	printf("<A HREF=\"%s&o=%d&t=%d&g=getDna&i=mixed&c=%s&l=%d&r=%d&strand=+&db=%s&table=%s\">"
	       "View DNA for this feature</A><BR>\n",
	       hgcPathAndSettings(), dup.chromStart, dup.chromEnd,
	       dup.chrom, dup.chromStart, dup.chromEnd, database, tbl);


	printf("<B>Other Position:</B> %s:%d-%d &nbsp;&nbsp;&nbsp;\n",
	       dup.otherChrom, dup.otherStart+1, dup.otherEnd);
	printf("<A HREF=\"%s&o=%d&t=%d&g=getDna&i=mixed&c=%s&l=%d&r=%d&strand=%s&db=%s&table=%s\">"
	       "View DNA for this feature</A><BR>\n",
	       hgcPathAndSettings(), dup.otherStart, dup.otherEnd,
	       dup.otherChrom, dup.otherStart, dup.otherEnd, dup.strand,
	       database, tbl);

	//printf("<B>Name:</B>%s<BR>\n",dup.name);
	//printf("<B>Score:</B>%d<BR>\n",dup.score);
	printf("<B>Other Position Relative Orientation:</B>%s<BR>\n",dup.strand);
	//printf("<B>Uid:</B>%d<BR>\n",dup.uid);
	printf("<B>Filter Verdict:</B>%s<BR>\n",dup.verdict);
	printf("&nbsp;&nbsp;&nbsp;<B>testResult:</B>%s<BR>\n",dup.testResult);
	printf("&nbsp;&nbsp;&nbsp;<B>chits:</B>%s<BR>\n",dup.chits);
	printf("&nbsp;&nbsp;&nbsp;<B>ccov:</B>%s<BR>\n",dup.ccov);
	printf("&nbsp;&nbsp;&nbsp;<B>posBasesHit:</B>%d<BR>\n",dup.posBasesHit);
	/*printf("<A HREF=/jab/der_oo33/%s TARGET=\"%s:%d-%d\">Optimal Global Alignment</A><BR>\n",dup.alignfile,dup.chrom, dup.chromStart, dup.chromEnd);*/
	printf("<A HREF=http://humanparalogy.cwru.edu/jab/der_oo33/%s TARGET=\"%s:%d-%d\">Optimal Global Alignment</A><BR>\n",dup.alignfile,dup.chrom, dup.chromStart, dup.chromEnd);
	printf("<B>Alignment Length:</B>%d<BR>\n",dup.alignL);
	printf("&nbsp;&nbsp;&nbsp;<B>Indels #:</B>%d<BR>\n",dup.indelN);
	printf("&nbsp;&nbsp;&nbsp;<B>Indels bp:</B>%d<BR>\n",dup.indelS);
	printf("&nbsp;&nbsp;&nbsp;<B>Aligned Bases:</B>%d<BR>\n",dup.alignB);
	printf("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<B>Matching bases:</B>%d<BR>\n",dup.matchB);
	printf("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<B>Mismatched bases:</B>%d<BR>\n",dup.mismatchB);
	printf("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<B>Transitions:</B>%d<BR>\n",dup.transitionsB);
	printf("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<B>Transverions:</B>%d<BR>\n",dup.transversionsB);
	printf("&nbsp;&nbsp;&nbsp;<B>Fraction Matching:</B>%3.4f<BR>\n",dup.fracMatch);
	printf("&nbsp;&nbsp;&nbsp;<B>Fraction Matching with Indels:</B>%3.4f<BR>\n",dup.fracMatchIndel);
	printf("&nbsp;&nbsp;&nbsp;<B>Jukes Cantor:</B>%3.4f<BR>\n",dup.jcK);

	}
    }
else
    {
    puts("<P>Click directly on a repeat for specific information on that repeat</P>");
    }

printTrackHtml(tdb);
sqlFreeResult(&sr);
hFreeConn(&conn);
webEnd();
}
/*Ewan's end*/



void doCgh(char *track, char *tissue, struct trackDb *tdb)
/* Create detail page for comparative genomic hybridization track */ 
{
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;

/* Print out non-sequence info */
cartWebStart(cart, tissue);

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
sprintf(query, "SELECT spot from cgh where chrom = '%s' AND "
               "chromStart <= '%d' AND chromEnd >= '%d' AND "
               "tissue = '%s' AND type = 3 GROUP BY spot ORDER BY chromStart",
	seqName, winEnd, winStart, tissue);
sr = sqlMustGetResult(conn, query);
while (row = sqlNextRow(sr)) {
printf("<TH>Spot %s</TH>",row[0]);
}  
printf("</TR>\n");
sqlFreeResult(&sr);

/* Find the relevant tissues type records in the range */ 
fillCghTable(3, tissue, FALSE);
printf("<TR><TD>&nbsp</TD></TR>\n");

/* Find the relevant tissue average records in the range */
fillCghTable(2, tissue, TRUE);
printf("<TR><TD>&nbsp</TD></TR>\n");

/* Find the all tissue average records in the range */
fillCghTable(1, NULL, TRUE);
printf("<TR><TD>&nbsp</TD></TR>\n");

printf("</TR>\n</TABLE>\n");

hgFreeConn(&conn);
}

void doMcnBreakpoints(char *track, char *name, struct trackDb *tdb)
/* Create detail page for MCN breakpoints track */ 
{
char query[256];
char title[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
char startBand[32]; 
char endBand[32]; 
char **row;
int rowOffset;
struct mcnBreakpoints *mcnRecord;

/* Print out non-sequence info */
sprintf(title, "MCN Breakpoints - %s",name);
cartWebStart(cart, title);

/* Print general range info */
/*printf("<H2>MCN Breakpoints - %s</H2>\n", name);
  printf("<P><HR ALIGN=\"CENTER\"></P>");*/
printf("<TABLE>\n");
printf("<TR><TH ALIGN=left>Chromosome:</TH><TD>%s</TD></TR>\n",seqName);
printf("<TR><TH ALIGN=left>Begin in Chromosome:</TH><TD>%d</TD></TR>\n",start);
printf("<TR><TH ALIGN=left>End in Chromosome:</TH><TD>%d</TD></TR>\n",end);
if (hChromBand(seqName, start, startBand) && hChromBand(seqName, end - 1, endBand))
    printf("<TR><TH Align=left>Chromosome Band Range:</TH><TD>%s - %s<TD></TR>\n",
	   startBand, endBand);	
printf("</TABLE>\n");

/* Find all of the breakpoints in this range for this name*/
sprintf(query, "SELECT * FROM mcnBreakpoints WHERE chrom = '%s' AND "
               "chromStart = %d and chromEnd = %d AND name = '%s'",
	seqName, start, end, name);
sr = sqlGetResult(conn, query);
while (row = sqlNextRow(sr)) 
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
hgFreeConn(&conn);
} 

void doMgcMrna(char *track, char *acc)
/* Redirects to genbank record */
{

printf("Content-Type: text/html\n\n<HTML><BODY><SCRIPT>\n");
printf("location.replace('");
printEntrezNucleotideUrl(stdout, acc);
puts("');"); 
printf("</SCRIPT> <NOSCRIPT> No JavaScript support. Click <B><A HREF=\"");
printEntrezNucleotideUrl(stdout, acc);
puts("\">continue</A></B> for the requested GenBank report. </NOSCRIPT>");}

void doProbeDetails(struct trackDb *tdb, char *item)
{
struct sqlConnection *conn = hAllocConn();
int start = cartInt(cart, "o");
struct dnaProbe *dp = NULL;
char buff[256];
genericHeader(tdb, item); 
snprintf(buff, sizeof(buff), "select * from dnaProbe where name='%s'",  item);
dp = dnaProbeLoadByQuery(conn, buff);
if(dp != NULL)
    {
    printf("<h3>Probe details:</h3>\n");
    printf("<b>Name:</b> %s  <font size=-2>[dbName genomeVersion strand coordinates]</font><br>\n",dp->name);
    printf("<b>Dna:</b> %s", dp->dna );
    printf("[<a href=\"hgBlat?type=DNA&genome=hg8&sort=&query,score&output=hyperlink&userSeq=%s\">blat (blast like alignment)</a>]<br>", dp->dna);
    printf("<b>Size:</b> %d<br>", dp->size );
    printf("<b>Chrom:</b> %s<br>", dp->chrom );
    printf("<b>ChromStart:</b> %d<br>", dp->start+1 );
    printf("<b>ChromEnd:</b> %d<br>", dp->end );
    printf("<b>Strand:</b> %s<br>", dp->strand );
    printf("<b>3' Dist:</b> %d<br>", dp->tpDist );
    printf("<b>Tm:</b> %f <font size=-2>[scores over 100 are allowed]</font><br>", dp->tm );
    printf("<b>%%GC:</b> %f<br>", dp->pGC );
    printf("<b>Affy:</b> %d <font size=-2>[1 passes, 0 doesn't pass Affy heuristic]</font><br>", dp->affyHeur );
    printf("<b>Sec Struct:</b> %f<br>", dp->secStruct);
    printf("<b>blatScore:</b> %d<br>", dp->blatScore );
    printf("<b>Comparison:</b> %f<br>", dp->comparison);
    }
//printf("<h3>Genomic Details:</h3>\n");
//genericBedClick(conn, tdb, item, start, 1);
printTrackHtml(tdb);
hFreeConn(&conn);
}

void perlegenDetails(struct trackDb *tdb, char *item)
{
char *dupe, *type, *words[16];
char title[256];
int wordCount;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn();
char table[64];
boolean hasBin;
struct bed *bed;
char query[512];
struct sqlResult *sr;
char **row;
boolean firstTime = TRUE;
char *itemForUrl = item;
int numSnpsReq = -1;
if(tdb == NULL)
    errAbort("TrackDb entry null for perlegen, item=%s\n", item);

dupe = cloneString(tdb->type);
genericHeader(tdb, item);
wordCount = chopLine(dupe, words);
printCustomUrl(tdb, item, TRUE);
hFindSplitTable(seqName, tdb->tableName, table, &hasBin);
sprintf(query, "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d",
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
    bedPrintPos(bed, 3);
    }
printTrackHtml(tdb);
hFreeConn(&conn);
}

void haplotypeDetails(struct trackDb *tdb, char *item)
{
char *dupe, *type, *words[16];
char title[256];
int wordCount;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn();
char table[64];
boolean hasBin;
struct bed *bed;
char query[512];
struct sqlResult *sr;
char **row;
boolean firstTime = TRUE;
char *itemForUrl = item;
int numSnpsReq = -1;
if(tdb == NULL)
    errAbort("TrackDb entry null for haplotype, item=%s\n", item);

dupe = cloneString(tdb->type);
genericHeader(tdb, item);
wordCount = chopLine(dupe, words);
printCustomUrl(tdb, item, TRUE);
hFindSplitTable(seqName, tdb->tableName, table, &hasBin);
sprintf(query, "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d",
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
    bedPrintPos(bed, 3);
    }
printTrackHtml(tdb);
hFreeConn(&conn);
}

void mitoDetails(struct trackDb *tdb, char *item)
{
char *dupe, *type, *words[16];
char title[256];
int wordCount;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn();
char table[64];
boolean hasBin;
struct bed *bed;
char query[512];
struct sqlResult *sr;
char **row;
boolean firstTime = TRUE;
char *itemForUrl = item;
int numSnpsReq = -1;
if(tdb == NULL)
    errAbort("TrackDb entry null for mitoSnps, item=%s\n", item);

dupe = cloneString(tdb->type);
genericHeader(tdb, item);
wordCount = chopLine(dupe, words);
printCustomUrl(tdb, item, TRUE);
hFindSplitTable(seqName, tdb->tableName, table, &hasBin);
sprintf(query, "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d",
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
    bedPrintPos(bed, 3);
    }
printTrackHtml(tdb);
hFreeConn(&conn);
}


void ancientRDetails(struct trackDb *tdb, char *item)
{
char *dupe, *type, *words[16];
char title[256];
int wordCount;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn();
char table[64];
boolean hasBin;
struct bed *bed = NULL;
char query[512];
struct sqlResult *sr = NULL;
char **row;
boolean firstTime = TRUE;
char *itemForUrl = item;
double ident = -1.0;

struct ancientRref *ar = NULL;

if(tdb == NULL)
    errAbort("TrackDb entry null for ancientR, item=%s\n", item);

dupe = cloneString(tdb->type);
genericHeader(tdb, item);
wordCount = chopLine(dupe, words);
printCustomUrl(tdb, item, TRUE);
hFindSplitTable(seqName, tdb->tableName, table, &hasBin);
sprintf(query, "select * from %s where name = '%s' and chrom = '%s'",
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
    bedPrintPos(bed, 3);

    }

/* look in associated table 'ancientRref' to get human/mouse alignment*/
sprintf(query, "select * from %sref where id = '%s'", table, item );
sr = sqlGetResult( conn, query );
while ((row = sqlNextRow(sr)) != NULL )
    {
    ar = ancientRrefLoad(row);

    printf("<h4><i>Repeat</i></h4>");
    printf("<B>Name:</B> %s<BR>\n", ar->name);
    printf("<B>Class:</B> %s<BR>\n", ar->class);
    printf("<B>Family:</B> %s<BR>\n", ar->family);

    //print the aligned sequences in html on multiple rows
    htmlHorizontalLine();
    printf("<i>human sequence on top, mouse on bottom</i><br><br>" );
    htmlPrintJointAlignment( ar->hseq, ar->mseq, 80,
			     bed->chromStart, bed->chromEnd, bed->strand );
    }

printTrackHtml(tdb);
hFreeConn(&conn);
}

void doGcDetails(struct trackDb *tdb, char *itemName) {
/* Show details for gc percent */
char *group = tdb->tableName;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
struct gcPercent *gc;
boolean hasBin; 
char table[64];

cartWebStart(cart, "Percentage GC in 20,000 Base Windows (GC)");

hFindSplitTable(seqName, group, table, &hasBin);
sprintf(query, "select * from %s where chrom = '%s' and chromStart = %d and name = '%s'",
	table, seqName, start, itemName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gc = gcPercentLoad(row + hasBin);
    printPos(gc->chrom, gc->chromStart, gc->chromEnd, NULL, FALSE);
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
printf("<LINK REL=STYLESHEET TYPE=\"text/css\" href=\"http://genome-test.cse.ucsc.edu/style/blueStyle.css\" title=\"Chuck Style\">\n");
printf("<title>%s</title>\n</head><body bgcolor=\"#f3f3ff\">",title);
}

void chuckHtmlContactInfo()
/* Writes out Chuck's email so people bother Chuck instead of Jim */
{
puts("<br><br><font size=-2><i>If you have comments and/or suggestions please email "
     "<a href=\"mailto:sugnet@cse.ucsc.edu\">sugnet@cse.ucsc.edu</a>.\n");
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
if(strstr(clickName,name)) 
    printf("<table border=0 cellspacing=0 cellpadding=0 bgcolor=\"D9E4F8\">\n");
else
    printf("<table border=0 cellspacing=0 cellpadding=0>\n");
for(i = 0; i < length; i++)
    {
    if(header[i] == ' ') 
	printf("<tr><td align=center>&nbsp</td></tr>\n");
    else
	{
	if(strstr(clickName,name)) 
	    printf("<tr><td align=center bgcolor=\"D9E4F8\">");
	else 
	    printf("<tr><td align=center>");
	
	/* if we have a url, create a reference */
	if(differentString(url,""))
	    printf("<a href=\"%s\">%c</a>", url, header[i]);
	else
	    printf("%c", header[i]);

	if(strstr(clickName,name)) 
	    {
	    printf("</font>");
	    }
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
char *user = cfgOption("db.user");
char *password = cfgOption("db.password");
struct sqlConnection *sc = NULL;
//struct sqlConnection *sc = sqlConnectRemote("localhost", user, password, "hgFixed");
char query[256];
struct sageExp *seList = NULL, *se=NULL;
char **row;
struct sqlResult *sr = NULL;
char *tmp= cloneString("select * from sageExp order by num");
if(hTableExists(tableName))
    sc = hAllocConn();
else
    sc = sqlConnectRemote("localhost", user, password, "hgFixed");

sprintf(query,"%s",tmp);
sr = sqlGetResult(sc,query);
while((row = sqlNextRow(sr)) != NULL)
    {
    se = sageExpLoad(row);
    slAddHead(&seList,se);
    }
freez(&tmp);
sqlFreeResult(&sr);
if(hTableExists(tableName))
    hFreeConn(&sc);
else
    sqlDisconnect(&sc);
slReverse(&seList);
return seList;
}

struct sage *loadSageData(char *table, struct bed* bedList)
/* load the sage data by constructing a query based on the qNames of the bedList
 */
{
char *user = cfgOption("db.user");
char *password = cfgOption("db.password");
struct sqlConnection *sc = NULL;
struct dyString *query = newDyString(2048);
struct sage *sgList = NULL, *sg=NULL;
struct bed *bed=NULL;
char **row;
int count=0;
struct sqlResult *sr = NULL;
if(hTableExists(table))
    sc = hAllocConn();
else
    sc = sqlConnectRemote("localhost", user, password, "hgFixed");
dyStringPrintf(query, "%s", "select * from sage where ");
for(bed=bedList;bed!=NULL;bed=bed->next)
    {
    if(count++) 
	{
	dyStringPrintf(query," or uni=%d ", atoi(bed->name + 3 ));
	}
    else 
	{
	dyStringPrintf(query," uni=%d ", atoi(bed->name + 3));
	}
    }
sr = sqlGetResult(sc,query->string);
while((row = sqlNextRow(sr)) != NULL)
    {
    sg = sageLoad(row);
    slAddHead(&sgList,sg);
    }
sqlFreeResult(&sr);
if(hTableExists(table))
    hFreeConn(&sc);
else
    sqlDisconnect(&sc);
slReverse(&sgList);
freeDyString(&query);
return sgList;
}

int sageBedWSListIndex(struct bed *bedList, int uni)
/* find the index of a bed by the unigene identifier in a bed list */
{
struct bed *bed;
int count =0;
char buff[128];
sprintf(buff,"Hs.%d",uni);
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
printf("Please click ");
printf("<a target=other href=\"../cgi-bin/sageVisCGI?");
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

/* puts( */
/*      "<table width=600 cellpadding=0 cellspacing=0><tr><td><p><b>Description: S</b>erial <b>A</b>nalysis of <b>G</b>ene <b>E</b>xpression (SAGE)" */
/*      "is a quantative measurement gene expression. Data is presented for every cluster contained " */
/*      "in the browser window and the selected cluster name is highlighted in red. All data is from " */
/*      "the repository at the <a href=\"http://www.ncbi.nlm.nih.gov/SAGE/\"> SageMap </a>" */
/*      "project downloaded Jul 26, 2002. Selecting the UniGene cluster name will display SageMap's page for that cluster."); */
/* printSageGraphUrl(sgList); */
/* puts( */
/*      "<p><b>Brief Methodology:</b> SAGE counts are produced " */
/*      "by sequencing small \"tags\" of DNA believed to be associated with a " */
/*      "gene. These tags are produced by attatching poly-A RNA to oligo-dT " */
/*      "beads. After synthesis of double stranded cDNA transcripts are " */
/*      "cleaved by an anchoring enzyme (usually NIaIII). Then small tags are " */
/*      "produced by ligation with a linker containing a type IIS restriction " */
/*      "enzyme site and cleavage with the tagging enzyme (usually BsmFI). The " */
/*      "tags are then concatenated together and sequenced. The frequency of each " */
/*      "tag is counted and used to infer expression level of transcripts that can " */
/*      "be matched to that tag. All SAGE data presented here was mapped to UniGene " */
/*      "transcripts by the <a href=\"http://www.ncbi.nlm.nih.gov/SAGE/\"> SageMap </a>" */
/*      "project at <a href=\"http://www.ncbi.nlm.nih.gov\"> NCBI </a>.</td></tr></table><br><br>" */
/* ); */
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
printSageGraphUrl(sgList);
printf("<BR>\n");
for(sg=sgList; sg != NULL; sg = sg->next)
    {
    char buff[256];
    sprintf(buff,"Hs.%d",sg->uni);
    printStanSource(buff, "unigene");
    }
featureCount= slCount(sgList); 
printf("<basefont size=-1>\n");
printf("<table cellspacing=0 border=1 bordercolor=\"black\">\n");
printf("<tr>\n");
printf("<th align=center>Sage Experiment</th>\n");
printf("<th align=center>Tissue</th>\n");
printf("<th align=center colspan=%d valign=top>Uni-Gene Clusters<br>(<b>Median</b> [Ave &plusmn Stdev])</th>\n",featureCount);
printf("</tr>\n<tr><td>&nbsp</td><td>&nbsp</td>\n");
for(sg = sgList; sg != NULL; sg = sg->next)
    {
    char buff[32];
    char url[256];
    sprintf(buff,"Hs.%d",sg->uni);
    printf("<td valign=top align=center>\n");
    sprintf(url, "http://www.ncbi.nlm.nih.gov/SAGE/SAGEcid.cgi?cid=%d&org=Hs",sg->uni);
    printTableHeaderName(buff, itemName, url);
    printf("</td>");
    }
printf("</tr>\n");
/* for each experiment write out the name and then all of the values */
for(se=seList;se!=NULL;se=se->next)
    {
    char *tmp;
    float mark = (float)10000.0/se->totalCount;
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
	if(sg->aves[se->num] == -1.0) 
	    printf("<td>N/A</td>");
	else 
	    printf("<td>  <b>%4.1f</b> <font size=-2>[%.2f &plusmn %.2f]</font></td>\n",
		   sg->meds[se->num],sg->aves[se->num],sg->stdevs[se->num]);
	}
    printf("</tr>\n");	   
    }
printf("</table>\n");
}


struct bed *bedWScoreLoadByChrom(char *table, char *chrom, int start, int end)
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
struct bed *bedWS, *bedWSList = NULL;
char **row;
int rowOffset;
char query[256];
struct hTableInfo *hti = hFindTableInfo(seqName, table);
if(hti == NULL)
    errAbort("Can't find table: %s", seqName);
else if(hti && sameString(hti->startField, "tStart"))
    snprintf(query, sizeof(query), "select qName,tStart,tEnd from %s where tName='%s' and tStart < %u and tEnd > %u", 
	     table, seqName, winEnd, winStart);
else if(hti && sameString(hti->startField, "chromStart"))
    snprintf(query, sizeof(query), "select name,chromStart,chromEnd from %s where chrom='%s' and chromStart < %u and chromEnd > %u", 
	     table, seqName, winEnd, winStart);
else
    errAbort("%s doesn't have tStart or chromStart");
//sr = hRangeQuery(conn,table,seqName,winStart,winEnd,NULL, &rowOffset);
sr = sqlGetResult(conn, query);
while((row = sqlNextRow(sr)) != NULL)
    {
    // bedWS = bedLoad12(row+rowOffset);
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

void llArrayInfo(struct trackDb *tdb, char *itemName) 
/* This one prints out a bunch of array information when a gene is clicked on.  
*  It reads a table if it exists.  */
{
char *track = tdb->tableName;
struct llaInfo *lla = NULL;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr, *sr2;
char **row, **row2;
char *infoName = strcat(track,"Info");
char query[256], query2[256];

if (hTableExists(infoName))
    {
    sprintf(query, "select * from %s where name = '%s'", infoName, itemName);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL) 
	{
	int i;
	lla = llaInfoLoad(row);
	printf("<B>Top %d correlated things:</B><BR><BR>\n", lla->numCorrs); 
	printf("<TABLE>\n");
	printf("<TR><TD><B>Rank</B></TD><TD><B>Thing</B></TD><TD><B>Distance (1-correlation)</B></TD></TR>\n");
	for (i = 0; i < lla->numCorrs; i++) 
	    {
	    printf("<TR><TD>%d</TD><TD>%s</TD><TD>%f</TD>\n",(i+1),lla->corrNames[i],lla->corrs[i]);
	    }
	printf("</TABLE>\n");
	htmlHorizontalLine();
	printf("<b>PCR product length:</b> %d<br>\n", lla->prodLen);
	printf("<b>Source ORF length: </b> %d<br>\n", lla->ORFLen);
	printf("<b>PCR melting temperature: </b> %.1f&#176;C<br>\n", lla->meltTm);
	printf("<b>Forward+reverse primer cross complementarity:</b> %.1f<br>\n", lla->frcc);
	printf("<b>3' forward+reverse primer cross complementarity:</b> %.1f<br>\n", lla->fr3pcc);
	printf("<u><b>Sense primer</b></u>\n<ul>\n");
	printf("  <li><b>Annealing temperature: </b>%.1f&#176;C</li>\n", lla->SnTm);
	printf("  <li><b>GC percent:</b> %.1f&#37;</li>\n", lla->SnGc);
	printf("  <li><b>Self-complementary score:</b> %.1f</li>\n", lla->SnSc);
	printf("  <li><b>3' self-complementary score:</b> %.1f</li>\n", lla->Sn3pSc);
	printf("  <li><b>Nucleotide sequence:</b>\n");
	printf("      <p><pre>%s</pre></p></li>\n", lla->SnSeq);
	printf("</ul>\n");
	printf("<u><b>Antisense primer</b></u>\n<ul>\n");
	printf("  <li><b>Annealing temperature:</b> %.1f&#176;C</li>\n", lla->AsnTm);
	printf("  <li><b>GC percent:</b> %.1f&#37;</li>\n", lla->AsnGc);
	printf("  <li><b>Self-complementary score:</b> %.1f</li>\n", lla->AsnSc);
	printf("  <li><b>3' self-complementary score:</b> %.1f</li>\n", lla->Asn3pSc);
	printf("  <li><b>Nucleotide sequence:</b>\n");
	printf("      <p><pre>%s</pre></p></li>\n", lla->AsnSeq);
	printf("</ul>\n");
	htmlHorizontalLine();    
	llaInfoFree(&lla);
	}
    puts(tdb->html);
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
}

void llArrayDetails(struct trackDb *tdb, char *expName) 
/* print out a page for the affy data from gnf based on ratio of
* measurements to the median of the measurements. */
{
char *itemName = cgiUsualString("i2","none");
genericHeader(tdb, itemName);
llArrayInfo(tdb, itemName);
}

void llDoCodingGenes(struct trackDb *tdb, char *item, 
		     char *pepTable, char *extraTable)
/* Handle click on gene track. */
{
struct minGeneInfo ginfo;
char query[256];
struct sqlResult *sr;
char **row;
char *dupe, *type, *words[16];
char title[256];
int wordCount;
int start = cartInt(cart, "o"), num = 0;
struct sqlConnection *conn = hAllocConn();

dupe = cloneString(tdb->type);
genericHeader(tdb, item);
wordCount = chopLine(dupe, words);
if (wordCount > 1)
    num = atoi(words[1]);
if (num < 3) num = 3;
genericBedClick(conn, tdb, item, start, num);
if (pepTable != NULL && hTableExists(pepTable))
    {
    char *pepNameCol = sameString(pepTable, "gbSeq") ? "acc" : "name";
    conn = hAllocConn();
    // simple query to see if pepName has a record in pepTable:
    safef(query, sizeof(query), "select 0 from %s where %s = '%s'",
	  pepTable, pepNameCol, item);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	hgcAnchorSomewhere("htcTranslatedProtein", item, pepTable, seqName);
	printf("Predicted Protein</A> <BR>\n"); 
	}
    sqlFreeResult(&sr);
    }
if (extraTable != NULL && hTableExists(extraTable)) 
    {
    conn = hAllocConn();
    sprintf(query, "select * from %s where name = '%s'", extraTable, item);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL) 
	{
	minGeneInfoStaticLoad(row, &ginfo);
	printf("<B>Product: </B>%s<BR>\n", ginfo.product);
	printf("<B>Note: </B>%s<BR>\n", ginfo.note);
	}
    sqlFreeResult(&sr);
    }
printTrackHtml(tdb);
hFreeConn(&conn);
}

void llDoTigrCmrGenes(struct trackDb *tdb, char *geneName)
/* Handle click on gene track. */
{
char *track = tdb->tableName;
struct tigrCmrGene *cmr = NULL;
char query[256];
struct sqlResult *sr;
char **row;
struct sqlConnection *conn = hAllocConn();

genericHeader(tdb,geneName);
/*
showGenePos(geneName,tdb);
*/
sprintf(query, "select * from %s where tigrLocus = '%s'", strcat(track,"Info"), geneName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    cmr = tigrCmrGeneLoad(row);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
if (cmr != NULL)
    {
    printf("<B>TIGR Common Name:</B> %s<BR>\n",cmr->tigrCommon);
    printf("<B>TIGR Gene Symbol: </B> %s<BR>\n",cmr->tigrGene);
    printf("<B>TIGR Enzyme Commission Number: </B> %s<BR>\n",cmr->tigrECN);
    printf("<B>Primary Locus Name: </B> %s<BR>\n",cmr->primLocus);
    printf("<B>TIGR Sequence Length: </B> %d<BR>\n",cmr->tigrLength);
    printf("<B>TIGR Protein Length: </B> %d<BR>\n",cmr->tigrPepLength);
    printf("<B>TIGR Main Role: </B> %s<BR>\n",cmr->tigrMainRole);
    printf("<B>TIGR Subrole: </B> %s<BR>\n",cmr->tigrSubRole);
    printf("<B>TIGR SwissProt/TrEmbl Accession: </B> %s<BR>\n",cmr->swissProt);
    printf("<B>TIGR Genbank ID: </B> %s<BR>\n",cmr->genbank);
    printf("<B>TIGR Molecular Weight: </B> %.2f<BR>\n",cmr->tigrMw);
    printf("<B>TIGR PI: </B> %.4f<BR>\n",cmr->tigrPi);
    printf("<B>TIGR GC Content: </B> %.1f<BR>\n",cmr->tigrGc);
    }
printTrackHtml(tdb);
}

void doSageDataDisp(char *tableName, char *itemName, struct trackDb *tdb) 
{
struct bed *sgList = NULL;
char buff[64];
char *s=NULL;
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
  sprintf(buff,"%d",winStart);
  cgiMakeHiddenVar("winStart", buff);
  zeroBytes(buff,64);
  sprintf(buff,"%d",winEnd);
  cgiMakeHiddenVar("winEnd", buff);
  cgiMakeHiddenVar("db",database); 
  printf("<br>\n");*/
chuckHtmlContactInfo();
}

int vgFindRgb(struct vGfx *vg, struct rgbColor *rgb)
/* Find color index corresponding to rgb color. */
{
return vgFindColorIx(vg, rgb->r, rgb->g, rgb->b);
}

void makeGrayShades(struct vGfx *vg)
/* Make eight shades of gray in display. */
{
int i;
for (i=0; i<=maxShade; ++i)
    {
    struct rgbColor rgb;
    int level = 255 - (255*i/maxShade);
    if (level < 0) level = 0;
    rgb.r = rgb.g = rgb.b = level;
    shadesOfGray[i] = vgFindRgb(vg, &rgb);
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
static struct rgbColor black = {0, 0, 0};
static struct rgbColor red = {255, 0, 0};
mgMakeColorGradient(mg, &black, &red, maxRGBShade+1, shadesOfRed);
exprBedColorsMade = TRUE;
}

boolean isExon(char *v, int i, int j)
/** Return TRUE if edge i-j is an exon, FALSE otherwise. */
{
if( (v[i] == ggHardStart || v[i] == ggSoftStart)  
    && (v[j] == ggHardEnd || v[j] == ggSoftEnd))
    return TRUE;
return FALSE;
}

boolean isIntron(char *v, int i, int j)
/** Return TRUE if edge i-j is an exon, FALSE otherwise. */
{
if( (v[j] == ggHardStart || v[j] == ggSoftStart)  
    && (v[i] == ggHardEnd || v[i] == ggSoftEnd))
    return TRUE;
return FALSE;
}


void doHumanEnlargeExons(struct altGraphX *ag)
/* Experimental, doesn't quite seem to work yet. Idea is to scale
 exons such that the smallest exon is no smaller that factor*largest
 intron.*/
{
bool **em = altGraphXCreateEdgeMatrix(ag);
int i,j,k;
int maxIntron=0;
char *vTypes = ag->vTypes;
int *vPos = ag->vPositions;
int minExon =BIGNUM;
int increment = 0;
double multFact = 1.2;
double minIntronFact = .1;
int vC = ag->vertexCount;
/* Find largest intron. */
for(i=0; i<vC; i++)
    {
    for(j=0; j<vC; j++)
	{
	if(em[i][j])
	    {
	    if(isIntron(vTypes, i, j))
		maxIntron = max(maxIntron, abs(vPos[i] - vPos[j]));
	    if(isExon(vTypes,i,j))
		minExon = min(minExon, abs(vPos[i] - vPos[j]));
	    }
	}
    }

/*minExon = minIntronFact * maxIntron;*/
multFact = (minIntronFact*maxIntron)/minExon;

/* Enlarge each exon by stretching one end. */
for(i=0; i<vC; i++)
    {
    for(j=0; j<vC; j++)
	{
	if(em[i][j])
	    {
	    if(isExon(vTypes, i, j))
		{
		int mark = vPos[j];
		int size = abs(vPos[i] - vPos[j]);
		increment = multFact*size - size;
		ag->tEnd += increment;
		for(k=0; k<vC; k++)
		    {
		    if(vPos[k] >= mark)
			vPos[k] += increment;
		    }
		}
	    }
	}
    }
}


char *altGraphXMakeImage(struct trackDb *tdb, struct altGraphX *ag)
/* create a drawing of splicing pattern */
{
MgFont *font = mgSmallFont();
int trackTabWidth = 11;
int fontHeight = mgFontLineHeight(font);
struct spaceSaver *ssList = NULL;
struct hash *heightHash = NULL;
int rowCount = 0;
struct tempName gifTn;
int pixWidth = atoi(cartUsualString(cart, "pix", "600" ));
int pixHeight = 0;
struct vGfx *vg;
int lineHeight = 0;
double scale = 0;

scale = (double)pixWidth/(ag->tEnd - ag->tStart);
lineHeight = 2 * fontHeight +1;
altGraphXLayout(ag, ag->tStart, pixWidth, pixWidth, scale, 100, &ssList, &heightHash, &rowCount);
pixHeight = rowCount * lineHeight;
makeTempName(&gifTn, "hgc", ".gif");
vg = vgOpenGif(pixWidth, pixHeight, gifTn.forCgi);
makeGrayShades(vg);
vgSetClip(vg, 0, 0, pixWidth, pixHeight);
altGraphXDrawPack(ag, ssList, 0, 0, pixWidth, lineHeight, lineHeight-1,
		  ag->tStart, ag->tEnd, scale, ag->tEnd-ag->tStart, vg, font, MG_BLACK, shadesOfGray, "Dummy", NULL);
vgUnclip(vg);
vgClose(&vg);
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
    }
return "NA";
}

void doAltGraphXDetails(struct trackDb *tdb, char *item)
/* do details page for an altGraphX */
{
int id = atoi(item);
char query[256];
int i,j;
struct altGraphX *ag = NULL;
struct altGraphX *orthoAg = NULL;
char buff[128];
struct sqlConnection *conn = hAllocConn();
char *image = NULL;
genericHeader(tdb, item);
snprintf(query, sizeof(query),"select * from %s where id=%d", tdb->tableName, id);
ag = altGraphXLoadByQuery(conn, query);
//doHumanEnlargeExons(ag);
if(ag == NULL)
    errAbort("hgc::doAltGraphXDetails() - couldn't find altGraphX with id=%d", id);
printf("<center>\n");
if(sameString(tdb->tableName, "altGraphXCon")) 
    printf("Common Splicing<br>");
image = altGraphXMakeImage(tdb,ag);
if(sameString(tdb->tableName, "altGraphXCon")) 
    {
    struct sqlConnection *orthoConn = NULL;
    struct altGraphX *origAg = NULL;
    hSetDb2("mm3");
    safef(query, sizeof(query), "select * from altGraphX where name='%s'", ag->name);
    origAg = altGraphXLoadByQuery(conn, query);
    //doHumanEnlargeExons(origAg);
    puts("<br><center>Human</center>\n");
    altGraphXMakeImage(tdb,origAg);
    orthoConn = hAllocConn2();
    safef(query, sizeof(query), "select orhtoAgName from orthoAgReport where agName='%s'", ag->name);
    sqlQuickQuery(conn, query, buff, sizeof(buff));
    safef(query, sizeof(query), "select * from altGraphX where name='%s'", buff);
    orthoAg = altGraphXLoadByQuery(orthoConn, query);
    //doHumanEnlargeExons(orthoAg);
    if(differentString(orthoAg->strand, origAg->strand))
	{
	altGraphXReverseComplement(orthoAg);
	puts("<br>Mouse (opposite strand)\n");
	}
    else 
	puts("<br>Mouse\n");
    printf("<a HREF=\"%s?db=%s&position=%s:%d-%d&mrna=squish&intronEst=squish&refGene=pack&altGraphX=full&%s\"",
	   hgTracksName(), "mm3", orthoAg->tName, orthoAg->tStart, orthoAg->tEnd, cartSidUrlString(cart));
    printf(" ALT=\"Zoom to browser coordinates of altGraphX\">");
    printf("<font size=-1>[%s.%s:%d-%d]</font></a><br><br>\n", "mm3", 
	   orthoAg->tName, orthoAg->tStart, orthoAg->tEnd);
    altGraphXMakeImage(tdb,orthoAg);
    }
printf("<br><a HREF=\"%s?position=%s:%d-%d&mrna=full&intronEst=full&refGene=full&altGraphX=full&%s\"",
       hgTracksName(), ag->tName, ag->tStart, ag->tEnd, cartSidUrlString(cart));
printf(" ALT=\"Zoom to browser coordinates of altGraphX\">");
printf("Jump to browser for %d</a><font size=-1>[%s:%d-%d]</font><br><br>\n", ag->id, ag->tName, ag->tStart, ag->tEnd);
printf("<table cellpadding=1 border=1>\n");
printf("<tr><th>Cassette Exon</th><th>Tissues Found</th></tr>\n");
for(i=0; i<ag->edgeCount; i++)
    {
    if(ag->edgeTypes[i] == -1)
	{
	char buff[512];
	int j=0;
	struct evidence *e =  slElementFromIx(ag->evidence, i);	
	printf("<tr><td>%d-%d</td><td>\n", ag->edgeStarts[i], ag->edgeEnds[i]);
	for(j=0; j<e->evCount; j++)
	    {
	    char *tmp = NULL;
	    snprintf(query, sizeof(query), "select name from tissue where id = %d",e->mrnaIds[j]);
	    tmp = sqlQuickQuery(conn, query, buff, sizeof(buff));
	    if(tmp != NULL)
		printf("%s,", buff);
	    }
	printf("</td></tr>\n");
	}
    }
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
    printf("<tr><td>%d</td><td>%d</td>", 	   ag->edgeStarts[i], ag->edgeEnds[i]);
    printf("<td><a href=%s?position=%s:%d-%d&mrna=full&intronEst=full&refGene=full&altGraphX=full\">%s</a></td><td>", 
	   hgTracksName(), 
	   ag->tName, 
	   ag->vPositions[ag->edgeStarts[i]], 
	   ag->vPositions[ag->edgeEnds[i]],
	   agXStringForEdge(ag, i));
    for(j=0; j<e->evCount; j++)
	printf("%s, ", ag->mrnaRefs[e->mrnaIds[j]]);
    printf("</td></tr>\n");
    }
printf("</table>\n");

printf("</td></tr>\n");
printf("</table>\n");
printf("</center>\n");
hFreeConn(&conn);
webEnd();
}


struct lineFile *openExtLineFile(unsigned int extFileId)
/* Open line file corresponding to id in extFile table. */
{
char *path = hExtFileName("extFile", extFileId);
struct lineFile *lf = lineFileOpen(path, TRUE);
freeMem(path);
return lf;
}

char *hgcNameAndSettings()
/* Return path to hgc with variables to store UI settings.
 */
{
static struct dyString *dy = NULL;
if (dy == NULL)
    {
    dy = newDyString(128);
    dyStringPrintf(dy, "%s?%s", hgcName(), cartSidUrlString(cart)); 
    } 
return dy->string; 
}

void printSampleWindow( struct psl *thisPsl, int thisWinStart, int
			thisWinEnd, char *winStr, char *otherOrg, char *otherDb, 
			char *pslTableName )
{
char otherString[256];
char pslItem[1024];
char *cgiPslItem;

sprintf( pslItem, "%s:%d-%d %s:%d-%d", thisPsl->qName, thisPsl->qStart, thisPsl->qEnd, thisPsl->tName, thisPsl->tStart, thisPsl->tEnd );
cgiPslItem = cgiEncode(pslItem);
sprintf(otherString, "%d&pslTable=%s&otherOrg=%s&otherChromTable=%s&otherDb=%s", thisPsl->tStart, 
	pslTableName, otherOrg, "chromInfo" , otherDb );
if (pslTrimToTargetRange(thisPsl, thisWinStart, thisWinEnd) != NULL)
    {
    hgcAnchorWindow("htcLongXenoPsl2", cgiPslItem, thisWinStart,
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

if( smp->chromStart + smp->samplePosition[i] -
    humMusWinSize / 2 + 1< left
    &&  smp->chromStart + smp->samplePosition[i] + humMusWinSize / 2 < left ) 
    return(0);

if( smp->chromStart + smp->samplePosition[i] -
    humMusWinSize / 2  + 1< thisStart 
    && smp->chromStart + smp->samplePosition[i] + humMusWinSize / 2 < thisStart  ) 
    return(0);

if( smp->chromStart + smp->samplePosition[i] -
    humMusWinSize / 2 + 1> right
    && smp->chromStart + smp->samplePosition[i] +
    humMusWinSize / 2  > right )
    return(0);


if( smp->chromStart + smp->samplePosition[i] -
    humMusWinSize / 2 + 1 > thisEnd 
    && smp->chromStart + smp->samplePosition[i] +
    humMusWinSize / 2  > thisEnd  ) 
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
int flag;
int i;
char table[64];
boolean hasBin;
struct sample *smp;
char query[512];
char istr[1024];
char tempTableName[1024];
struct sqlResult *sr;
char **row;
char **pslRow;
boolean firstTime = TRUE;
struct psl *psl;
struct psl *thisPsl;

char pslItem[1024];
char str[256];
char thisItem[256];
char *cgiItem;
char otherString[256] = "";


struct sqlResult *pslSr;
struct sqlConnection *conn2 = hAllocConn();

int thisStart, thisEnd;

int left = cartIntExp( cart, "l" );
int right = cartIntExp( cart, "r" );


char *winOn = cartUsualString( cart, "win", "F" );
//errAbort( "(%s), (%s)\n", pslTableName, pslTableName );

hFindSplitTable(seqName, tdb->tableName, table, &hasBin);
sprintf(query, "select * from %s where name = '%s' and chrom = '%s'",
	table, item, seqName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (firstTime)
	firstTime = FALSE;
    else
	htmlHorizontalLine();
    smp = sampleLoad(row+hasBin);

    sprintf( tempTableName, "%s_%s", smp->chrom, pslTableName );
    hFindSplitTable(seqName, pslTableName, table, &hasBin);
    sprintf(query, "select * from %s where tName = '%s' and tEnd >= %d and tStart <= %d" 
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

	    cgiItem = cgiEncode(thisItem);
	    longXenoPsl1Given(tdb, thisItem, otherOrg, "chromInfo",
			      otherDb, thisPsl, pslTableName );

	    sprintf(otherString, "%d&win=T", thisPsl->tStart );
	    hgcAnchorSomewhere( tdb->tableName, cgiEncode(item), otherString, thisPsl->tName );
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
		//0 to 8.0 is the fixed total L-score range for
		//all these conservation tracks. Scores outside 
		//this range are truncated.

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
int humMusWinSize = 50;
int flag;
int i;
char table[64];
boolean hasBin;
struct sample *smp;
char query[512];
char istr[1024];
char tempTableName[1024];
struct sqlResult *sr;
char **row;
char **pslRow;
boolean firstTime = TRUE;
struct psl *psl;
struct psl *thisPsl;

char pslItem[1024];
char str[256];
char thisItem[256];
char *cgiPslItem;
char filename[10000];

char pslTableName[128] = "blastzBestMouse";

struct sqlResult *pslSr;
struct sqlConnection *conn2 = hAllocConn();

int thisStart, thisEnd;
int offset;
int motifid;

int left = cartIntExp( cart, "l" );
int right = cartIntExp( cart, "r" );

hFindSplitTable(seqName, tdb->tableName, table, &hasBin);
sprintf(query, "select * from %s where name = '%s'",
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
    sprintf(filename,"../zoo_blanchem/new_raw2_offset%d.fa.main.html?motifID=%d",offset,motifid);

    sprintf( tempTableName, "%s_%s", smp->chrom, pslTableName );
    hFindSplitTable(seqName, pslTableName, table, &hasBin);
    sprintf(query, "select * from %s where tName = '%s' and tEnd >= %d and tStart <= %d" ,
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
char *dupe, *type, *words[16];
char title[256];
int num;
int wordCount;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn();

dupe = cloneString(tdb->type);
genericHeader(tdb, item);
wordCount = chopLine(dupe, words);
if (wordCount > 0)
    {
    type = words[0];

    num = 0;
    if (wordCount > 1)
	num = atoi(words[1]);
    if (num < 3) num = 3;

        //humMusSampleClick(conn, tdb, item, start, num, "Mouse", "mm2", "blastzBestMouse", printWindowFlag );
        humMusSampleClick( conn, tdb, item, start, num, targetName, targetDb, targetTable, printWindowFlag );
        //    "Human", "hg12", "blastzBestHuman_08_30", printWindowFlag );
    }
printTrackHtml(tdb);
freez(&dupe);
hFreeConn(&conn);
}

void footPrinterClickHandler(struct trackDb *tdb, char *item )
/* Put up generic track info. */
{  
char *dupe, *type, *words[16];
char title[256];
int num;
int wordCount;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn();
dupe = cloneString(tdb->type);
//genericHeader(tdb, item);
wordCount = chopLine(dupe, words);
if (wordCount > 0)
    {
    type = words[0];

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


void hgCustom(char *trackId, char *fileItem)
/* Process click on custom track. */
{
char *fileName, *itemName;
struct customTrack *ctList = getCtList();
struct customTrack *ct;
struct bed *bed;
int start = cartInt(cart, "o");
char *url;

cartWebStart(cart, "Custom Track");
fileName = nextWord(&fileItem);
itemName = skipLeadingSpaces(fileItem);
printf("<H2>Custom Track Item %s</H2>\n", itemName);
for (ct = ctList; ct != NULL; ct = ct->next)
    {
    if (sameString(trackId, ct->tdb->tableName))
	break;
    }
if (ct == NULL)
    errAbort("Couldn't find %s in %s", trackId, fileName);
for (bed = ct->bedList; bed != NULL; bed = bed->next)
    {
    if (bed->chromStart == start && sameString(seqName, bed->chrom))
	{
	if (bed->name == NULL || sameString(itemName, bed->name) )
	    {
	    break;
	    }
	}
    }
if (bed == NULL)
    errAbort("Couldn't find %s@%s:%d in %s", itemName, seqName, start, fileName);
printCustomUrl(ct->tdb, itemName, TRUE);
bedPrintPos(bed, ct->fieldCount);
}

void chesProtein(struct trackDb *tdb, char *itemName)
/* Show protein to translated dna alignment for accession. */
{
struct lineFile *lf;
struct psl *psl;
enum gfType tt = gftDnaX, qt = gftProt;
boolean isProt = 1;
struct sqlResult *sr;
struct sqlConnection *conn = hAllocConn();
struct dnaSeq *seq;
char query[256], **row;
char fullTable[64];
boolean hasBin;
int start;

/* Print start of HTML. */
writeFramesetType();
puts("<HTML>");
printf("<HEAD>\n<TITLE>Protein Sequence vs Genomic</TITLE>\n</HEAD>\n\n");

start = cartInt(cart, "o");
hFindSplitTable(seqName, tdb->tableName, fullTable, &hasBin);
sprintf(query, "select * from %s where qName = '%s' and tName = '%s' and tStart=%d",
	fullTable, itemName, seqName, start);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find alignment for %s", itemName);
psl = pslLoad(row+hasBin);
sqlFreeResult(&sr);
hFreeConn(&conn);
seq = hPepSeq(itemName);
showSomeAlignment(psl, seq, gftProtChes, 0, seq->size, NULL, 0, 0);
//showSomeAlignment(psl, seq, gftProt, psl->qStart, psl->qEnd, NULL, 0, 0);
}

void doMiddle()
/* Generate body of HTML. */
{
char *track = cartString(cart, "g");
char *item = cartOptionalString(cart, "i");
char title[256];
struct trackDb *tdb;

/*	database and organism are global variables used in many places	*/
database = cartUsualString(cart, "db", hGetDb());
organism = hOrganism(database);
scientificName = hScientificName(database);

hDefaultConnect(); 	/* set up default connection settings */
hSetDb(database);

protDbName = hPdbFromGdb(database);

seqName = cartString(cart, "c");
winStart = cartIntExp(cart, "l");
winEnd = cartIntExp(cart, "r");
trackHash = makeTrackHash(seqName);
tdb = hashFindVal(trackHash, track);
if (sameWord(track, "getDna"))
    {
    doGetDna1();
    }
else if (sameWord(track, "htcGetDna2"))
    {
    doGetDna2();
    }
else if (sameWord(track, "htcGetDna3"))
    {
    doGetDna3();
    }
else if (sameWord(track, "htcGetDnaExtended1"))
    {
    doGetDnaExtended1();
    }
else if (sameWord(track, "mrna") || sameWord(track, "mrna2") || 
	 sameWord(track, "all_mrna") ||
         sameWord(track, "est") || sameWord(track, "intronEst") || 
         sameWord(track, "xenoMrna") || sameWord(track, "xenoBestMrna") ||
         startsWith(track, "mrnaBlastz") || 
         sameWord(track, "xenoBlastzMrna") || sameWord(track, "sim4") ||
         sameWord(track, "xenoEst") || sameWord(track, "psu") ||
         sameWord(track, "tightMrna") || sameWord(track, "tightEst") ||
         sameWord(track, "mgcIncompleteMrna") ||
         sameWord(track, "mgcFailedEst") ||
         sameWord(track, "mgcPickedEst") ||
         sameWord(track, "mgcUnpickedEst") 
         )
    {
    doHgRna(tdb, item);
    }
else if (sameWord(track, "refFullAli"))
    {
    doTSS(tdb, item);
    }
else if (sameWord(track, "rikenMrna"))
    {
    doRikenRna(tdb, item);
    }
else if (sameWord(track, "ctgPos"))
    {
    doHgContig(tdb, item);
    }
else if (sameWord(track, "clonePos"))
    {
    doHgCover(tdb, item);
    }
else if (sameWord(track, "bactigPos"))
    {
    doBactigPos(tdb, item);
    }
else if (sameWord(track, "hgClone"))
    {
    tdb = hashFindVal(trackHash, "clonePos");
    doHgClone(tdb, item);
    }
else if (sameWord(track, "gold"))
    {
    doHgGold(tdb, item);
    }
else if (sameWord(track, "gap"))
    {
    doHgGap(tdb, item);
    }
else if (sameWord(track, "tet_waba"))
    {
    doHgTet(tdb, item);
    }
else if (sameWord(track, "wabaCbr"))
    {
    doHgCbr(tdb, item);
    }
else if (sameWord(track, "rmsk"))
    {
    doHgRepeat(tdb, item);
    }
else if (sameWord(track, "isochores"))
    {
    doHgIsochore(tdb, item);
    }
else if (sameWord(track, "chesSimpleRepeat"))
    {
    doSimpleRepeat(tdb, item);
    }
else if (sameWord(track, "simpleRepeat"))
    {
    doSimpleRepeat(tdb, item);
    }
else if (sameWord(track, "cpgIsland") || sameWord(track, "cpgIsland2"))
    {
    doCpgIsland(tdb, item);
    }
else if (sameWord(track, "knownGene"))
    {
    doSPGene(tdb, item);
    }
else if (sameWord(track, "superfamily"))
    {
    doSuperfamily(tdb, item, NULL);
    }
else if (sameWord(track, "ensGene"))
    {
    doEnsemblGene(tdb, item, NULL);
    }
else if (sameWord(track, "xenoRefGene"))
    {
    doRefGene(tdb, item);
    }
else if (sameWord(track, "refGene"))
    {
    doRefGene(tdb, item);
    }
else if (sameWord(track, "mgcGenes"))
    {
    doMgcGenes(tdb, item);
    }
else if (sameWord(track, "genieKnown"))
    {
    doKnownGene(tdb, item);
    }
else if (startsWith("viralProt", track))
    {
    doViralProt(tdb, item);
    }
else if (sameWord("otherSARS", track))
    {
    doPslDetailed(tdb, item);
    }
else if (sameWord(track, "softberryGene"))
    {
    doSoftberryPred(tdb, item);
    }
else if (startsWith(track, "pseudoMrna"))
    {
    doPseudoPsl(tdb, item);
    }
else if (sameWord(track, "pseudoUcsc") || sameWord(track, "pseudoUcscSelf"))
    {
    doPseudoGene(tdb, item);
    }
else if (sameWord(track, "borkPseudo"))
    {
    doPseudoPred(tdb, item);
    }
else if (sameWord(track, "borkPseudoBig"))
    {
    doPseudoPred(tdb, item);
    }
else if (sameWord(track, "sanger22"))
    {
    doSangerGene(tdb, item, "sanger22pep", "sanger22mrna", "sanger22extra");
    }
else if (sameWord(track, "sanger20"))
    {
    doSangerGene(tdb, item, "sanger20pep", "sanger20mrna", "sanger20extra");
    }
else if (sameWord(track, "vegaGene") || sameWord(track, "vegaPseudoGene"))
    {
    doVegaGene(tdb, item);
    }
else if (sameWord(track, "genomicDups"))
    {
    doGenomicDups(tdb, item);
    }
else if (sameWord(track, "blatMouse") || sameWord(track, "bestMouse")
	 || sameWord(track, "blastzTest") || sameWord(track, "blastzTest2"))
    {
    doBlatMouse(tdb, item);
    }
else if (startsWith("multAlignWebb", track))
    {
    doMultAlignZoo(tdb, item, &track[13] );
    }
/*
  Generalized code to show strict chain blastz alignments in the zoo browsers
*/
else if (containsStringNoCase(track, "blastzStrictChain")
         && containsStringNoCase(database, "zoo"))
    {
    int len = strlen("blastzStrictChain");
    char *orgName = &track[len];
    char dbName[32] = "zoo";
    strcpy(&dbName[3], orgName);
    len = strlen(orgName);
    strcpy(&dbName[3 + len], "3");
    longXenoPsl1(tdb, item, orgName, "chromInfo", dbName);
    }
else if (sameWord(track, "blatChimp") ||
         sameWord(track, "chimpBac") ||
         sameWord(track, "bacChimp"))
    { 
    longXenoPsl1Chimp(tdb, item, "Chimpanzee", "chromInfo", database);
    }
else if (sameWord(track, "htcLongXenoPsl2"))
    {
    htcLongXenoPsl2(track, item);
    }
else if (sameWord(track, "htcGenePsl"))
    {
    htcGenePsl(track, item);
    }
else if (sameWord(track, "htcPseudoGene"))
    {
    htcPseudoGene(track, item);
    }
else if (sameWord(track, "firstEF"))
    {
    firstEF(tdb, item);
    }
else if (sameWord(track, "chesChordataBlat"))
    {
    chesProtein(tdb, item);
    }
else if ( sameWord(track, "chesMammalBlat") )
    {
    chesProtein(tdb, item);
    }
else if (sameWord(track, "chesChordataPsl") || 
	 sameWord(track, "chesMammalPsl") )
    {
    chesProtein(tdb, item);
    }
else if (sameWord(track, "hg15PepPsl") )
    {
    chesProtein(tdb, item);
    }
else if (sameWord(track, "hg15repeats") )
    {
    doBlatCompGeno(tdb, item, "Human");
    }
/* generic handling of blat tracks */ 
else if (startsWith("blat", track))
    {
    char *blatLabel = &track[4];
    if (hDbExists(blatLabel))
        {
        /* handle tracks that include other database name 
         * in trackname; e.g. blatCe1, blatCb1, blatCi1, blatHg15, blatMm3... 
         * Uses genome column from database table as display text */
        blatLabel = hGenome(blatLabel);
        }
    doBlatCompGeno(tdb, item, blatLabel);
    }
else if (sameWord(track, "humanKnownGene")) 
    {
    doKnownGene(tdb, item);
    }
/* This is a catch-all for blastz/blat tracks -- any special cases must be 
 * above this point! */
else if (startsWith("blastz", track) || startsWith("blat", track) || (endsWith(track, "Blastz")))
    {
    doAlignmentOtherDb(tdb, item);
    }
else if (sameWord(track, "rnaGene"))
    {
    doRnaGene(tdb, item);
    }
else if (sameWord(track, "fishClones"))
    {
    doFishClones(tdb, item);
    }
else if (sameWord(track, "stsMarker"))
    {
    doStsMarker(tdb, item);
    }
else if (sameWord(track, "stsMapMouse"))
    {
    doStsMapMouse(tdb, item);
    }
else if (sameWord(track, "stsMapMouseNew")) /*steal map rat code for new mouse sts track. */
    {
    doStsMapMouseNew(tdb, item);
    }
else if(sameWord(track, "stsMapRat"))
    {
    doStsMapRat(tdb, item);
    }
else if (sameWord(track, "stsMap"))
    {
    doStsMarker(tdb, item);
    }
else if (sameWord(track, "recombRate"))
    {
    doRecombRate(tdb);
    }
else if (sameWord(track, "genMapDb"))
    {
    doGenMapDb(tdb, item);
    }
else if (sameWord(track, "mouseSynWhd"))
    {
    doMouseSynWhd(tdb, item);
    }
else if (sameWord(track, "ensRatMusHom"))
    {
    doEnsPhusionBlast(tdb, item);
    }
else if (sameWord(track, "mouseSyn"))
    {
    doMouseSyn(tdb, item);
    }
else if (sameWord(track, "mouseOrtho"))
    {
    doMouseOrtho(tdb, item);
    }
else if (sameWord(track, "hgUserPsl"))
    {
    doUserPsl(track, item);
    }
else if (sameWord(track, "softPromoter"))
    {
    hgSoftPromoter(track, item);
    }
else if (startsWith("ct_", track))
    {
    hgCustom(track, item);
    }
else if (sameWord(track, "snpTsc") || sameWord(track, "snpNih"))
    {
    doSnp(tdb, item);
    }
else if (sameWord(track, "uniGene_2") || sameWord(track, "uniGene"))
    {
    doSageDataDisp(track, item, tdb);
    }
else if (sameWord(track, "tigrGeneIndex"))
    {
    doTigrGeneIndex(tdb, item);
    }
else if (sameWord(track, "mgc_mrna"))
    {
    doMgcMrna(track, item);
    }
else if ((sameWord(track, "bacEndPairs")) || (sameWord(track, "bacEndPairsBad")) || (sameWord(track, "bacEndPairsLong")))
    {
    doLinkedFeaturesSeries(track, item, tdb);
    }
else if ((sameWord(track, "fosEndPairs")) || (sameWord(track, "fosEndPairsBad")) || (sameWord(track, "fosEndPairsLong")))
    {
    doLinkedFeaturesSeries(track, item, tdb);
    }
else if (sameWord(track, "cgh"))
    {
    doCgh(track, item, tdb);
    }
else if (sameWord(track, "mcnBreakpoints"))
    {
    doMcnBreakpoints(track, item, tdb);
    }
else if (sameWord(track, "htcChainAli"))
    {
    htcChainAli(item);
    }
else if (sameWord(track, "htcChainTransAli"))
    {
    htcChainTransAli(item);
    }
else if (sameWord(track, "htcCdnaAli"))
    {
    htcCdnaAli(item);
    }
else if (sameWord(track, "htcUserAli"))
    {
    htcUserAli(item);
    }
else if (sameWord(track, "htcProteinAli"))
    {
    htcProteinAli(item, cartString(cart, "aliTrack"));
    }
else if (sameWord(track, "htcBlatXeno"))
    {
    htcBlatXeno(item, cartString(cart, "aliTrack"));
    }
else if (sameWord(track, "htcExtSeq"))
    {
    htcExtSeq(item);
    }
else if (sameWord(track, "htcTranslatedProtein"))
    {
    htcTranslatedProtein(item);
    }
else if (sameWord(track, "htcGeneMrna"))
    {
    htcGeneMrna(item);
    }
else if (sameWord(track, "htcRefMrna"))
    {
    htcRefMrna(item);
    }
else if (sameWord(track, "htcKnownGeneMrna"))
    {
    htcKnownGeneMrna(item);
    }
else if (sameWord(track, "htcGeneInGenome"))
    {
    htcGeneInGenome(item);
    }
else if (sameWord(track, "htcDnaNearGene"))
    {
    htcDnaNearGene(item);
    }
else if (sameWord(track, "getMsBedAll"))
    {
    getMsBedExpDetails(tdb, item, TRUE);
    }
else if (sameWord(track, "getMsBedRange"))
    {
    getMsBedExpDetails(tdb, item, FALSE);
    }
else if (sameWord(track, "perlegen"))
    {
    perlegenDetails(tdb, item);
    }
else if (sameWord(track, "haplotype"))
    {
    haplotypeDetails(tdb, item);
    }
else if (sameWord(track, "mitoSnps"))
    {
    mitoDetails(tdb, item);
    }
else if(sameWord(track, "rosetta"))
    {
    rosettaDetails(tdb, item);
    }
else if (sameWord(track, "cghNci60"))
    {
    cghNci60Details(tdb, item);
    }
else if (sameWord(track, "nci60"))
    {
    nci60Details(tdb, item);
    }
else if(sameWord(track, "affy"))
    {
    affyDetails(tdb, item);
    }
else if ( sameWord(track, "affyRatio") || sameWord(track, "affyGnfU74A") 
	|| sameWord(track, "affyGnfU74B") || sameWord(track, "affyGnfU74C"))
    {
    gnfExpRatioDetails(tdb, item);
    }
else if(sameWord(track, "affyUcla"))
    {
    affyUclaDetails(tdb, item);
    }
else if(sameWord(track, "loweProbes"))
    {
    doProbeDetails(tdb, item);
    }
else if( sameWord(track, "ancientR"))
    {
    ancientRDetails(tdb, item);
    }
else if( sameWord(track, "gcPercent"))
    {
    doGcDetails(tdb, item);
    }
else if( sameWord(track, "altGraphX") || sameWord(track, "altGraphXCon"))
    {
    doAltGraphXDetails(tdb,item);
    }

/* Lowe Lab Stuff */

else if (sameWord(track, "gbProtCode"))
    {
    llDoCodingGenes(tdb, item,"gbProtCodePep","gbProtCodeXra");
    }
else if (sameWord(track, "tigrCmrORFs"))
    {
    llDoTigrCmrGenes(tdb,item);
    }
else if (startsWith("lla", track)) 
    {
    llArrayDetails(tdb,item);
    }

/*Evan's stuff*/
else if (sameWord(track, "genomicSuperDups"))
    {
    doGenomicSuperDups(tdb, item);
    }
else if (sameWord(track, "celeraCoverage"))
    {
    doCeleraCoverage();
    }
else if (sameWord(track, "celeraDupPositive"))
    {
    doCeleraDupPositive(tdb, item);
    }

else if (sameWord(track, "triangle") || sameWord(track, "triangleSelf") || sameWord(track, "transfacHit"))
    {
    doTriangle(tdb, item);
    }
else if( sameWord( track, "humMusL" ) || sameWord( track, "regpotent" ))
    {
    humMusClickHandler( tdb, item, "Mouse", "mm2", "blastzBestMouse", 0);
    }
else if( sameWord( track, "musHumL" ))
    {
    humMusClickHandler( tdb, item, "Human", "hg12", "blastzBestHuman_08_30" , 0);
    }
else if( sameWord( track, "mm3Rn2L" ))
    {
    humMusClickHandler( tdb, item, "Rat", "rn2", "blastzBestRat", 0 );
    }
else if( sameWord( track, "hg15Mm3L" ))
    {
    humMusClickHandler( tdb, item, "Mouse", "mm3", "blastzBestMm3", 0 );
    }
else if( sameWord( track, "mm3Hg15L" ))
    {
    humMusClickHandler( tdb, item, "Human", "hg15", "blastzNetHuman" , 0);
    }

else if( sameWord( track, "footPrinter" ))
    {
    footPrinterClickHandler( tdb, item );
    }
else if (sameWord(track, "jaxQTL"))
    {
    doJaxQTL(tdb, item);
    }
else if (sameWord(track, "gbProtAnn"))
    {
    doGbProtAnn(tdb, item);
    }
else if (tdb != NULL)
    {
    genericClickHandler(tdb, item, NULL);
    }
else
    {
    cartWebStart(cart, track);
    printf("Sorry, clicking there doesn't do anything yet (%s).", track);
    }
cartHtmlEnd();
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

char *excludeVars[] = {"bool.hcg.dna.rc", "Submit", "submit", "g", "i", "aliTrack", NULL};

int main(int argc, char *argv[])
{
cgiSpoof(&argc,argv);
cartEmptyShell(cartDoMiddle, hUserCookie(), excludeVars, NULL);
return 0;
}
