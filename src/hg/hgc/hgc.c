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
#include "trackDb.h"
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
#include "stsInfo.h"
#include "mouseSyn.h"
#include "cytoBand.h"
#include "knownMore.h"
#include "snp.h"
#include "softberryHom.h"
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
#include "jaxOrtholog.h"
#include "expRecord.h"
#include "dnaProbe.h"
#include "ancientRref.h"
#include "jointalign.h"
#include "gcPercent.h"
#include "uPennClones.h"
#include "geneGraph.h"
#include "altGraphX.h"
#include "stsMapMouse.h"
#include "stsInfoMouse.h"
#include "dnaMotif.h"

#define ROGIC_CODE 1
#define FUREY_CODE 1

struct cart *cart;	/* User's settings. */

char *seqName;		/* Name of sequence we're working on. */
int winStart, winEnd;   /* Bounds of sequence. */
char *database;		/* Name of mySQL database. */

int maxShade = 9;	/* Highest shade in a color gradient. */
Color shadesOfGray[10+1];	/* 10 shades of gray from white to black */

Color shadesOfRed[16];
boolean exprBedColorsMade = FALSE; /* Have the shades of red been made? */
int maxRGBShade = 16;

struct pslWScore *sageExpList = NULL;
char *entrezScript = "http://www.ncbi.nlm.nih.gov/htbin-post/Entrez/query?form=4";
char *unistsnameScript = "http://www.ncbi.nlm.nih.gov:80/entrez/query.fcgi?db=unists";
char *unistsScript = "http://www.ncbi.nlm.nih.gov/genome/sts/sts.cgi?uid=";
char *gdbScript = "http://www.gdb.org/gdb-bin/genera/accno?accessionNum=";
char *cloneRegScript = "http://www.ncbi.nlm.nih.gov/genome/clone/clname.cgi?stype=Name&list=";

static void hgcStart(char *title)
/* Print out header of web page with title.  Set
 * error handler to normal html error handler. */
{
cartHtmlStart(title);
}

void printEntrezNucleotideUrl(FILE *f, char *accession)
/* Print URL for Entrez browser on a nucleotide. */
{
fprintf(f, "\"%s&db=n&term=%s\"", entrezScript, accession);
}

void printEntrezProteinUrl(FILE *f, char *accession)
/* Print URL for Entrez browser on a nucleotide. */
{
fprintf(f, "\"%s&db=p&term=%s\"", entrezScript, accession);
}

void printEntrezUniSTSUrl(FILE *f, char *name)
/* Print URL for Entrez browser on a STS name. */
{
fprintf(f, "\"%s&term=%s\"", unistsnameScript, name);
}

void printUnistsUrl(FILE *f, int id)
/* Print URL for UniSTS record for an id. */
{
fprintf(f, "\"%s%d\"", unistsScript, id);
}

void printGdbUrl(FILE *f, char *id)
/* Print URL for GDB browser for an id */
{
fprintf(f, "\"%s%s\"", gdbScript, id);
}

void printCloneRegUrl(FILE *f, char *clone)
/* Print URL for Clone Registry at NCBI for a clone */
{
fprintf(f, "\"%s%s\"", cloneRegScript, clone);
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
/* Generate an anchor that calls click processing program with item and other parameters. */
{
printf("<A HREF=\"%s&g=%s&i=%s&c=%s&l=%d&r=%d&o=%s\">",
	hgcPathAndSettings(), group, item, chrom, winStart, winEnd, other);
}

void hgcAnchor(char *group, char *item, char *other)
/* Generate an anchor that calls click processing program with item and other parameters. */
{
hgcAnchorSomewhere(group, item, other, seqName);
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

printf("<P>Here is the sequence around this feature: bases %d to %d of %s.  The bases "
       "that contain the feature itself are in upper case.</P>\n", s, e, seqName);
seq = hDnaFromSeq(seqName, s, e, dnaLower);
toUpperN(seq->dna + (start-s), end - start);
printf("<TT><PRE>");
cfm = cfmNew(10, 50, TRUE, FALSE, stdout, s);
for (i=0; i<seq->size; ++i)
   {
   cfmOut(cfm, seq->dna[i], 0);
   }
cfmFree(&cfm);
printf("</PRE></TT>");
}

boolean chromBand(char *chrom, int pos, char *retBand)
/* Return text string that says what band pos is on. 
 * Return FALSE if not on any band, or table missing. */
{
struct sqlConnection *conn = hAllocConn();
char query[256];
char buf[64];
char *s;
/*static char band[64];*/

if (!hTableExists("cytoBand"))
    return FALSE;
sprintf(query, 
	"select name from cytoBand where chrom = '%s' and chromStart <= %d and chromEnd > %d", 
	chrom, pos, pos);
buf[0] = 0;
s = sqlQuickQuery(conn, query, buf, sizeof(buf));
sprintf(retBand, "%s%s", skipChr(chrom), buf);
hFreeConn(&conn);
return s != NULL;
}

void printPos(char *chrom, int start, int end, char *strand, boolean featDna)
/* Print position lines.  'strand' argument may be null. */
{
char band[64];

printf("<B>Chromosome:</B> %s<BR>\n", skipChr(chrom));
if (chromBand(chrom, (start + end)/2, band))
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
    printf("<A HREF=\"%s&o=%d&g=getDna&i=mixed&c=%s&l=%d&r=%d&strand=%s\">"
	  "View DNA for this feature</A><BR>\n",  hgcPathAndSettings(),
	  start, chrom, start, end, strand);
    }
}

void bedPrintPos(struct bed *bed)
/* Print first three fields of a bed type structure in
 * standard format. */
{
printPos(bed->chrom, bed->chromStart, bed->chromEnd, NULL, TRUE);
}

void genericHeader(struct trackDb *tdb, char *item)
/* Put up generic track info. */
{
char buf[256];
/* sprintf(buf, "%s (%s)", tdb->shortLabel, item);
   hgcStart(buf);
   printf("<H2>%s (%s)</H2>\n", tdb->longLabel, item);*/
sprintf(buf, "%s (%s)\n", tdb->longLabel, item);
cartWebStart(buf);
}

struct dyString *subMulti(char *orig, int subCount, char *in[], char *out[])
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
    printf("%s</A><BR>\n", uUrl->string);
    freeMem(eItem);
    freeDyString(&uUrl);
    freeDyString(&eUrl);
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
    if (bedSize > 3)
	printf("<B>Item:</B> %s<BR>\n", bed->name);
    if (bedSize > 4)
        printf("<B>Score:</B> %d<BR>\n", bed->score);
    if (bedSize > 5)
        printf("<B>Strand:</B> %s<BR>\n", bed->strand);
    bedPrintPos(bed);
    }
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

void geneShowPosAndLinks(char *geneName, char *pepName, struct trackDb *tdb, 
	char *pepTable, char *pepClick, 
	char *mrnaClick, char *genomicClick, char *mrnaDescription)
/* Show parts of gene common to everything */
{
char *geneTable = tdb->tableName;
char other[256];
showGenePos(geneName, tdb);
printf("<H3>Links to sequence:</H3>\n");
printf("<UL>\n");
if (pepTable != NULL && hTableExists(pepTable))
    {
    hgcAnchorSomewhere(pepClick, pepName, pepTable, seqName);
    printf("<LI>Translated Protein</A>\n"); 
    }
hgcAnchorSomewhere(mrnaClick, geneName, geneTable, seqName);
printf("<LI>%s</A>\n", mrnaDescription);
hgcAnchorSomewhere(genomicClick, geneName, geneTable, seqName);
printf("<LI>Genomic Sequence</A>\n");
printf("</UL>\n");
}

void geneShowCommon(char *geneName, struct trackDb *tdb, char *pepTable)
/* Show parts of gene common to everything */
{
geneShowPosAndLinks(geneName, geneName, tdb, pepTable, "htcTranslatedProtein",
	"htcGeneMrna", "htcGeneInGenome", "Predicted mRNA");
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
char table[64];
boolean hasBin;
char query[512];
struct sqlResult *sr;
char **row;
hFindSplitTable(seqName, tdb->tableName, table, &hasBin);
sprintf(query, "select * from %s where qName = '%s'", table, item);
sr = sqlGetResult(conn, query);
printf("<PRE><TT>\n");
printf("#match\tmisMatches\trepMatches\tnCount\tqNumInsert\tqBaseInsert\tstrand\tqName\tqSize\tqStart\tqEnd\ttName\ttSize\ttStart\ttEnd\tblockCount\tblockSizes\tqStarts\ttStarts\n");
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct psl *psl = pslLoad(row+hasBin);
    pslTabOut(psl, stdout);
    }
printf("</PRE></TT>\n");
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
    }
printTrackHtml(tdb);
freez(&dupe);
hFreeConn(&conn);
}

void savePosInHidden()
/* Save basic position/database info in hidden vars. */
{
cgiContinueHiddenVar("c");
cgiContinueHiddenVar("l");
cgiContinueHiddenVar("r");
cgiContinueHiddenVar("db");
}

void doGetDna1()
/* Do first get DNA dialog. */
{
char *var = "hgc.dna.out1";
char *oldVal = cartUsualString(cart, var, "lcRepeats");

cartWebStart("Get DNA in Window");
printf("<H2>Get DNA for %s:%d-%d</H2>\n", seqName, winStart+1, winEnd);
printf("<FORM ACTION=\"%s\">\n\n", hgcPath());
cartSaveSession(cart);
cgiMakeHiddenVar("g", "htcGetDna2");
savePosInHidden();
cgiMakeRadioButton(var, "lcRepeats", sameString(oldVal, "lcRepeats"));
printf(" lower case repeats<BR>\n");
cgiMakeRadioButton(var, "nRepeats", sameString(oldVal, "nRepeats"));
printf(" mask repeats<BR>\n");
cgiMakeRadioButton(var, "lc", sameString(oldVal, "lc"));
printf(" all lower case<BR>\n");
cgiMakeRadioButton(var, "uc", sameString(oldVal, "uc"));
printf(" all upper case<BR>\n");
cgiMakeRadioButton(var, "extended", sameString(oldVal, "extended"));
printf(" extended case/color options<BR>\n");
printf(" reverse complement ");
cgiMakeCheckBox("hgc.dna.rc", FALSE);
printf(" ");
cgiMakeButton("Submit", "Submit");
printf("</FORM>\n");

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
    sameString("mouseSyn", track));
}


void doGetDnaExtended1()
/* Do extended case/color get DNA options. */
{
struct trackDb *tdbList = hTrackDb(seqName), *tdb;

cartWebStart("Extended DNA Case/Color");
printf("<H1>Extended DNA Case/Color Options</H1>\n");
puts(
"Use this page to highlight features in genomic DNA text. "
"DNA covered by a particular track can be hilighted by "
"case, underline, bold, italic, or color.  See below for "
"details about color, and for examples.");

printf("<FORM ACTION=\"%s\" METHOD=\"POST\">\n\n", hgcPath());
cartSaveSession(cart);
cgiMakeHiddenVar("g", "htcGetDna3");
printf("Chromosome ");
cgiMakeTextVar("c", seqName, 6);
printf(" Start ");
cgiMakeIntVar("l", winStart, 9);
printf(" End ");
cgiMakeIntVar("r", winEnd, 9);
cgiContinueHiddenVar("db");
printf(" Reverse complement ");
cgiMakeCheckBox("hgc.dna.rc", cartUsualBoolean(cart, "hgc.dna.rc", FALSE));
printf("<BR>\n");
printf("Letters per line ");
cgiMakeIntVar("lineWidth", 60, 3);
printf(" Default case: ");
cgiMakeRadioButton("case", "upper", FALSE);
printf(" Upper ");
cgiMakeRadioButton("case", "lower", TRUE);
printf(" Lower ");
cgiMakeButton("Submit", "Submit");
printf("<BR>\n");
printf("<TABLE BORDER=1 COL=8>\n");
printf("<TR><TD>Track<BR>Name</TD><TD>Toggle<BR>Case</TD><TD>Under-<BR>line</TD><TD>Bold</TD><TD>Italic</TD><TD>Red</TD><TD>Green</TD><TD>Blue</TD></TR>\n");
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    if (fbUnderstandTrack(tdb->tableName) && !dnaIgnoreTrack(tdb->tableName))
	{
	char buf[128];
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
printf("</TABLE>\n");
printf("</FORM>\n");
printf("<H3>Coloring Information and Examples</H3>\n");
puts("The color values range from 0 (darkest) to 255 (lightest) and are additive.\n");
puts("The examples below show a few ways to hilight individual tracks, "
  "and their interplay. It's good to keep it simple at first.  It's easy "
  "to make pretty but completely cryptic displays with this feature.");
puts(
  "<UL>"
  "<LI>To put exons from Known Genes in upper case red text, check the "
  "appropriate box in the Toggle Case column and set the color to pure "
  "red, RGB (255,0,0). Upon submitting, any Known Gene within the "
  "designated chromosomal interval will now appear in red capital letters.\n"
  "<LI>To see the overlap between Known Genes and Genscan predictions try "
  "setting the Known Genes to red (255,0,0) and Genscan to green (0,255,0). "
  "Places where the Known Genes and Genscan overlap will be painted yellow "
  "(255,255,0).\n"
  "<LI>To get a level-of-coverage effect for tracks like Spliced Ests with "
  "multiple overlapping items, initially select a darker color such as deep "
  "green, RGB (0,64,0). Nucleotides covered by a single EST will appear dark "
  "green, while regions covered with more ESTs get progressively brighter -- "
  "saturating at 4 ESTs."
  "<LI>Another track can be used to mask unwanted features. Setting the "
  "RepeatMasker track to RGB (255,255,255) will white-out Genscan predictions "
  "of LINEs but not mainstream host genes; masking with Known Genes will show "
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
char *outType = cartString(cart, "hgc.dna.out1");
if (sameString(outType, "extended"))
    {
    doGetDnaExtended1();
    }
else
    {
    struct dnaSeq *seq;
    char name[256];
    boolean isRc = cartBoolean(cart, "hgc.dna.rc");

    sprintf(name, "%s:%d-%d %s", seqName, winStart+1, winEnd, 
    	(isRc ? "(reverse complement)" : ""));
    hgcStart(name);
    seq = hDnaFromSeq(seqName, winStart, winEnd, dnaLower);
    if (sameString(outType, "uc"))
        touppers(seq->dna);
    else if (sameString(outType, "lcRepeats"))
        {
	touppers(seq->dna);
	maskRepeats(seqName, winStart, winEnd, seq->dna, winStart, TRUE);
	}
    else if (sameString(outType, "nRepeats"))
        {
	touppers(seq->dna);
	maskRepeats(seqName, winStart, winEnd, seq->dna, winStart, FALSE);
	}
    if (isRc)
	reverseComplement(seq->dna, seq->size);
    printf("<TT><PRE>");
    faWriteNext(stdout, name, seq->dna, seq->size);
    printf("</TT></PRE>");
    freeDnaSeq(&seq);
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
	struct featureBits **pList)
/* See if track_type variable exists, and if so set corresponding bits. */
{
char buf[256];
struct featureBits *fb;
int s,e;
int winSize = winEnd - winStart;

sprintf(buf, "%s_%s", track, type);
if (cgiBoolean(buf))
    {
    if (*pList == NULL)
	*pList = fbGetRange(track, seqName, winStart, winEnd);
    for (fb = *pList; fb != NULL; fb = fb->next)
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
int winSize = winEnd - winStart;
int lineWidth = cartInt(cart, "lineWidth");
struct rgbColor *colors;
struct trackDb *tdbList = hTrackDb(seqName), *tdb;
Bits *uBits = bitAlloc(winSize);	/* Underline bits. */
Bits *iBits = bitAlloc(winSize);	/* Italic bits. */
Bits *bBits = bitAlloc(winSize);	/* Bold bits. */


cartWebStart("Extended DNA Output");
printf("<TT><PRE>");
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
    if (fbUnderstandTrack(tdb->tableName) && !dnaIgnoreTrack(tdb->tableName))
        {
	char buf[256];
	int r,g,b;

	/* Flip underline/italic/bold bits. */
	getDnaHandleBits(track, "u", uBits, winStart, winEnd, isRc, &fbList);
	getDnaHandleBits(track, "b", bBits, winStart, winEnd, isRc, &fbList);
	getDnaHandleBits(track, "i", iBits, winStart, winEnd, isRc, &fbList);

	/* Toggle case if necessary. */
	sprintf(buf, "%s_case", track);
	if (cgiBoolean(buf))
	    {
	    if (fbList == NULL)
		fbList = fbGetRange(track, seqName, winStart, winEnd);
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
	    if (fbList == NULL)
		fbList = fbGetRange(track, seqName, winStart, winEnd);
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
    printf("<A HREF=\"%s&db=m&term=%s\" TARGET=_blank>%s</A><BR>", entrezScript, 
        encoded, text);
freeMem(encoded);
}

void appendAuthor(struct dyString *dy, char *gbAuthor, int len)
/* Convert from  Kent,W.J. to Kent WJ and append to dy.
 * gbAuthor gets eaten in the process. */
{
char buf[512];

if (len >= sizeof(buf))
    warn("author %s too long to process", gbAuthor);
else
    {
    memcpy(buf, gbAuthor, len);
    buf[len] = 0;
    stripChar(buf, '.');
    subChar(buf, ',' , ' ');
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

void printGeneLynx(char *search)
/* Print link to GeneLynx search */
{
printf("<B>Gene Lynx</B> ");
printf("<A HREF=\"http://www.genelynx.org/cgi-bin/linklist?tableitem=GLID_NAME.name&IDlist=%s&dir=1\" TARGET=_blank>", search);
printf("%s</A><BR>\n", search);
}

/* --- */
void printRikenInfo(char *acc, struct sqlConnection *conn )
/* Print Riken annotation info */
{
struct sqlResult *sr;
char **row;
char qry[512];
char *seqid, *fantomid, *cloneid, *modified_time, *accession, *comment;
char *qualifier, *anntext, *datasrc, *srckey, *href, *evidence;   

accession = acc;

	//!!! uncomment the following line, if you want to test Riken annotation 
	// before the new genbank data get loaded into the mouse genome database.  
	//    Fan 3/28/02
	//accession = strdup("AK002809");

	snprintf(qry, sizeof(qry), "select seqid from rikenaltid where altid='%s';", accession);
	sr = sqlMustGetResult(conn, qry);
	row = sqlNextRow(sr);

	if (row != NULL)
		{
		seqid=strdup(row[0]);

		snprintf(qry, sizeof(qry), "select Qualifier, Anntext, Datasrc, Srckey, Href, Evidence from rikenann where seqid='%s';", seqid);

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
		
			printf("<B>Riken/%s link:</B> ",datasrc);
	        	printf("<A HREF=\"%s\">", href);	
	        	printf("%s",anntext);
			printf("</A><BR>\n");

			row = sqlNextRow(sr);		
			}
	
		snprintf(qry, sizeof(qry), "select comment from rikenseq where id='%s';", seqid);
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

void printRnaSpecs(char *acc)
/* Print auxiliarry info on RNA. */
{
struct dyString *dy = newDyString(1024);
struct sqlConnection *conn = hgAllocConn();
struct sqlResult *sr;
char **row;
char *type,*direction,*source,*organism,*library,*clone,*sex,*tissue,
     *development,*cell,*cds,*description, *author,*geneName,
     *date,*productName;
int seqSize,fileSize;
long fileOffset;
char *ext_file;		    
/* This sort of query and having to keep things in sync between
 * the first clause of the select, the from clause, the where
 * clause, and the results in the row ... is really tedious.
 * One of my main motivations for going to a more object
 * based rather than pure relational approach in general,
 * and writing 'autoSql' to help support this.  However
 * the pure relational approach wins for pure search speed,
 * and these RNA fields are searched.  So it looks like
 * the code below stays.  Be really careful when you modify
 * it. */
dyStringAppend(dy,
  "select mrna.type,mrna.direction,"
  	 "source.name,organism.name,library.name,mrnaClone.name,"
         "sex.name,tissue.name,development.name,cell.name,cds.name,"
	 "description.name,author.name,geneName.name,productName.name,"
	 "seq.size,seq.gb_date,seq.extFile,seq.file_offset,seq.file_size "
  "from mrna,seq,source,organism,library,mrnaClone,sex,tissue,"
        "development,cell,cds,description,author,geneName,productName ");
dyStringPrintf(dy,
  "where mrna.acc = '%s' and mrna.id = seq.id ", acc);
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
    type=row[0];direction=row[1];source=row[2];organism=row[3];library=row[4];clone=row[5];
    sex=row[6];tissue=row[7];development=row[8];cell=row[9];cds=row[10];description=row[11];
    author=row[12];geneName=row[13];productName=row[14];
    seqSize = sqlUnsigned(row[15]);
    date = row[16];
    ext_file = row[17];
    fileOffset=sqlUnsigned(row[18]);
    fileSize=sqlUnsigned(row[19]);

    /* Now we have all the info out of the database and into nicely named
     * local variables.  There's still a few hoops to jump through to 
     * format this prettily on the web with hyperlinks to NCBI. */
    printf("<H2>Information on %s <A HREF=", type);
    printEntrezNucleotideUrl(stdout, acc);
    printf(" TARGET=_blank>%s</A></H2>\n", acc);

    printf("<B>description:</B> %s<BR>\n", description);

    medlineLinkedLine("gene", geneName, geneName);
    medlineLinkedLine("product", productName, productName);
    dyStringClear(dy);
    gbToEntrezAuthor(author, dy);
    medlineLinkedLine("author", author, dy->string);
    printf("<B>organism:</B> ");
    printf("<A href=\"http://www.ncbi.nlm.nih.gov/htbin-post/Taxonomy/wgetorg?mode=Undef&name=%s&lvl=0&srchmode=1\" TARGET=_blank>", 
    	cgiEncode(organism));
    printf("%s</A><BR>\n", organism);
    printf("<B>tissue:</B> %s<BR>\n", tissue);
    printf("<B>development stage:</B> %s<BR>\n", development);
    printf("<B>cell type:</B> %s<BR>\n", cell);
    printf("<B>sex:</B> %s<BR>\n", sex);
    printf("<B>library:</B> %s<BR>\n", library);
    printf("<B>clone:</B> %s<BR>\n", clone);
    if (direction[0] != '0') printf("<B>read direction:</B> %s'<BR>\n", direction);
    printf("<B>cds:</B> %s<BR>\n", cds);
    printf("<B>date:</B> %s<BR>\n", date);

    /* Put up Gene Lynx */
    if (sameWord(type, "mrna"))
        printGeneLynx(acc);

    if ((strstr(hgGetDb(), "mm") != NULL) 
        && hTableExists("rikenaltid"))
	{
        sqlFreeResult(&sr);
	printRikenInfo(acc, conn);
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



#ifdef ROGIC_CODE

void printEstPairInfo(char *track, char *name)
/* print information about 5' - 3' EST pairs */
{
struct sqlConnection *conn = hgAllocConn();
struct sqlResult *sr;
char query[256]; 
char **row;
struct estPair *ep = NULL;

sprintf(query, "select * from %s where mrnaClone='%s'", track, name);
sr = sqlGetResult(conn, query);
 if((row = sqlNextRow(sr)) != NULL)
   {
     ep = estPairLoad(row);
     printf("<H2>Information on 5' - 3' EST pair from the clone %s </H2>\n\n", name);
     printf("<B>chromosome:</B> %s<BR>\n", ep->chrom); 
     printf("<B>Start position in chromosome :</B> %u<BR>\n", ep->chromStart); 
     printf("<B>End position in chromosome :</B> %u<BR>\n", ep->chromEnd);
     printf("<B>5' accession:</B> <A HREF=\"../cgi-bin/hgc?o=%u&t=%u&g=est&i=%s&c=%s&l=%d&r=%d&db=%s\"> %s</A><BR>\n", ep->start5, ep->end5, ep->acc5, ep->chrom, winStart, winEnd, database, ep->acc5); 
     printf("<B>Start position of 5' est in chromosome :</B> %u<BR>\n", ep->start5); 
     printf("<B>End position of 5' est in chromosome :</B> %u<BR>\n", ep->end5); 
     printf("<B>3' accession:</B> <A HREF=\"../cgi-bin/hgc?o=%u&t=%u&g=est&i=%s&c=%s&l=%d&r=%d&db=%s\"> %s</A><BR>\n", ep->start3, ep->end3, ep->acc3, ep->chrom, winStart, winEnd, database, ep->acc3);  
     printf("<B>Start position of 3' est in chromosome :</B> %u<BR>\n", ep->start3); 
     printf("<B>End position of 3' est in chromosome :</B> %u<BR>\n", ep->end3);
   }
 else
   {
     warn("Couldn't find %s in mrna table", name);
   }   
sqlFreeResult(&sr);
hgFreeConn(&conn);
}
#endif /* ROGIC_CODE */

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

printf("<TT><PRE>");
printf(" SIZE IDENTITY CHROMOSOME STRAND  START     END       cDNA   START  END  TOTAL\n");
printf("------------------------------------------------------------------------------\n");
for (same = 1; same >= 0; same -= 1)
    {
    for (psl = pslList; psl != NULL; psl = psl->next)
	{
	if (same ^ (psl->tStart != startFirst))
	    {
	    sprintf(otherString, "%d&aliTrack=%s", psl->tStart, typeName);
	    hgcAnchorSomewhere(hgcCommand, itemIn, otherString, psl->tName);
	    printf("%5d  %5.1f%%  %9s     %s %9d %9d  %8s %5d %5d %5d</A>",
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

#ifdef ROGIC_CODE

void doHgEstPair(char *track, char *name)
/* Click on EST pair */
{
cartWebStart(name);
printEstPairInfo(track, name);
}

#endif /* ROGIC_CODE */

void doHgRna(struct trackDb *tdb, char *acc)
/* Click on an individual RNA. */
{
char *track = tdb->tableName;
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
char *type;
boolean hasBin;
char splitTable[64];
char *table;
int start = cartInt(cart, "o");
struct psl *pslList = NULL, *psl;

if (sameString("xenoMrna", track) || sameString("xenoBestMrna", track) || sameString("xenoEst", track))
    {
    type = "non-Human RNA";
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
else 
    {
    type = "mRNA";
    table = "all_mrna";
    }

/* Print non-sequence info. */
cartWebStart(acc);

printRnaSpecs(acc);

/* Get alignment info. */
hFindSplitTable(seqName, table, splitTable, &hasBin);

sprintf(query, "select * from %s where qName = '%s'", table, acc);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row+hasBin);
    slAddHead(&pslList, psl);
    }
sqlFreeResult(&sr);
slReverse(&pslList);

htmlHorizontalLine();
printf("<H3>%s/Genomic Alignments</H3>", type);
printAlignments(pslList, start, "htcCdnaAli", table, acc);

printTrackHtml(tdb);
}

void parseSs(char *ss, char **retPslName, char **retFaName, char **retQName)
/* Parse space separated 'ss' item. */
{
static char buf[512*2];
int wordCount;
char *words[4];
strcpy(buf, ss);
wordCount = chopLine(buf, words);
if (wordCount != 3)
    errAbort("Expecting 3 words in ss item");
*retPslName = words[0];
*retFaName = words[1];
*retQName = words[2];
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

cartWebStart("BLAT Search Alignments");
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
char query[256];
struct sqlResult *sr;
char **row;
struct agpFrag frag;
int start = cartInt(cart, "o");
boolean hasBin;
char splitTable[64];

cartWebStart(fragName);
hFindSplitTable(seqName, track, splitTable, &hasBin);
sprintf(query, "select * from %s where frag = '%s' and chromStart = %d", 
	splitTable, fragName, start+1);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
agpFragStaticLoad(row+hasBin, &frag);

printf("<B>Clone Fragment ID:</B> %s<BR>\n", frag.frag);
printf("<B>Clone Bases:</B> %d-%d\n", frag.fragStart+1, frag.fragEnd);
printPos(frag.chrom, frag.chromStart, frag.chromEnd, frag.strand, TRUE);
printTrackHtml(tdb);

sqlFreeResult(&sr);
hFreeConn(&conn);
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

cartWebStart("Gap in Sequence");
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
sprintf(query, 
    "select count(*) from clonePos where chrom = '%s' and chromEnd >= %d and chromStart <= %d",
    ctg->chrom, ctg->chromStart, ctg->chromEnd);
cloneCount = sqlQuickNum(conn, query);

printf("<B>Name:</B> %s<BR>\n", ctgName);
printf("<B>Total Clones:</B> %d<BR>\n", cloneCount);
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

cartWebStart(cloneName);
sprintf(query, "select * from %s where name = '%s'", track, cloneName);
selectOneRow(conn, track, query, &sr, &row);
clone = clonePosLoad(row);
sqlFreeResult(&sr);

sprintf(query, 
    "select count(*) from %s_gl where end >= %d and start <= %d and frag like '%s%%'",
    clone->chrom, clone->chromStart, clone->chromEnd, clone->name);
fragCount = sqlQuickNum(conn, query);

printf("<H2>Information on <A HREF=");
printEntrezNucleotideUrl(stdout, cloneName);
printf(" TARGET=_blank>%s</A></H2>\n", cloneName);
printf("<B>status:</B> %s<BR>\n", cloneStageName(clone->stage));
printf("<B>fragments:</B> %d<BR>\n", fragCount);
printf("<B>size:</B> %d bases<BR>\n", clone->seqSize);
printf("<B>chromosome:</B> %s<BR>\n", skipChr(clone->chrom));
printf("<BR>\n");

hgcAnchor("htcCloneSeq", cloneName, "");
printf("Fasta format sequence</A><BR>\n");
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

void htcCloneSeq(char *cloneName)
/* Fetch sequence as fasta file. */
{
struct sqlConnection *conn = hAllocConn();
char query[256];
struct sqlResult *sr;
char **row;
struct clonePos *clone;
struct dnaSeq *seqList, *seq;

hgcStart(cloneName);
sprintf(query, "select * from clonePos where name = '%s'", cloneName);
selectOneRow(conn, "clonePos", query, &sr, &row);
clone = clonePosLoad(row);
sqlFreeResult(&sr);

seqList = faReadAllDna(clone->faFile);
printf("<PRE><TT>");
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    faWriteNext(stdout, seq->name, seq->dna, seq->size);
    }
hFreeConn(&conn);
}

int showGfAlignment(struct psl *psl, bioSeq *oSeq, FILE *f, enum gfType qType, int qStart, 
	int qEnd, char *qName)
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
fprintf(f, "<TT><PRE>");
fprintf(f, "<H4><A NAME=cDNA></A>%s%s</H4>\n", qName, (qIsRc  ? " (reverse complemented)" : ""));
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

fprintf(f, "<H4><A NAME=genomic></A>Genomic %s %s:</H4>\n", 
    psl->tName, (tIsRc ? "(reverse strand)" : ""));
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
	    for (j=1; j<sz; ++j)
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
fprintf(f, "<H4><A NAME=ali></A>Side by Side Alignment</H4>\n");
    {
    struct baf baf;
    int i,j;

    bafInit(&baf, oSeq->dna, qbafStart, qIsRc,
            dnaSeq->dna, tbafStart, tIsRc, f, 60, isProt);
	    
    for (i=0; i<psl->blockCount; ++i)
	{
	int qs = psl->qStarts[i] - qStart;
	int ts = psl->tStarts[i] - tStart;
	int sz = psl->blockSizes[i];

	bafSetPos(&baf, qs, ts);
	bafStartLine(&baf);
	if (isProt)
	    {
	    for (j=0; j<sz; ++j)
		{
		AA aa = oSeq->dna[qs+j];
		int codonStart = ts + 3*j;
		DNA *codon = &dnaSeq->dna[codonStart];
		bafOut(&baf, ' ', codon[0]);
		bafOut(&baf, aa, codon[1]);
		bafOut(&baf, ' ', codon[2]);
		}
	    }
	else
	    {
	    for (j=0; j<sz; ++j)
		{
		bafOut(&baf, oSeq->dna[qs+j], dnaSeq->dna[ts+j]);
		}
	    }
	bafFlushLine(&baf);
	}
    }
fprintf(f, "</TT></PRE>");
if (qIsRc)
    reverseComplement(oSeq->dna, oSeq->size);
freeMem(dna);
freeMem(oLetters);
return psl->blockCount;
}


int showDnaAlignment(struct psl *psl, struct dnaSeq *rnaSeq, FILE *body)
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
    }
ffAli = pslToFfAli(psl, rnaSeq, dnaSeq, tRcAdjustedStart);

blockCount = ffShAliPart(body, ffAli, psl->qName, rna, rnaSize, 0, 
	dnaSeq->name, dnaSeq->dna, dnaSeq->size, tStart, 
	8, FALSE, isRc, FALSE, TRUE, TRUE, TRUE, TRUE);
return blockCount;
}

void showSomeAlignment(struct psl *psl, bioSeq *oSeq, enum gfType qType, int qStart, int qEnd,
	char *qName)
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
    blockCount = showDnaAlignment(psl, oSeq, body);
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
fprintf(index, "<A HREF=\"%s#genomic\" TARGET=\"body\">genomic</A><BR>\n", bodyTn.forCgi);
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
printf("  <FRAME SRC=\"%s\" NAME=\"index\" RESIZE>\n", indexTn.forCgi);
printf("  <FRAME SRC=\"%s\" NAME=\"body\" RESIZE>\n", bodyTn.forCgi);
puts("</FRAMESET>");
puts("<NOFRAMES><BODY></BODY></NOFRAMES>");
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
boolean hasBin;

/* Print start of HTML. */
printf("<HEAD>\n<TITLE>%s vs Genomic</TITLE>\n</HEAD>\n\n", acc);
puts("<HTML>");

/* Get some environment vars. */
type = cartString(cart, "aliTrack");
start = cartInt(cart, "o");

/* Look up alignments in database */
conn = hAllocConn();
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
    showSomeAlignment(psl, rnaSeq, gftDnaX, 0, rnaSeq->size, NULL);
else
    showSomeAlignment(psl, rnaSeq, gftDna, 0, rnaSeq->size, NULL);
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
printf("<HEAD>\n<TITLE>User Sequence vs Genomic</TITLE>\n</HEAD>\n\n");
puts("<HTML>");

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
showSomeAlignment(psl, oSeq, qt, 0, oSeq->size, NULL);
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
printf("<HEAD>\n<TITLE>Sequence %s</TITLE>\n</HEAD>\n\n", readName);
puts("<HTML>");

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
showSomeAlignment(psl, seq, gftDnaX, 0, seq->size, NULL);
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
printf("</PRE></TT>");

wabAliFree(&wa);
hFreeConn(&conn);
}

void doHgTet(struct trackDb *tdb, char *name)
/* Do thing with tet track. */
{
cartWebStart("Tetraodon Alignment");
printf("Alignment between tetraodon sequence %s (above) and human chromosome %s (below)\n",
    name, skipChr(seqName));
fetchAndShowWaba("waba_tet", name);
}

void doHgRepeat(struct trackDb *tdb, char *repeat)
/* Do click on a repeat track. */
{
char *track = tdb->tableName;
int offset = cartInt(cart, "o");
cartWebStart("Repeat");
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
    printf("Click right on top of an individual repeat for more information on that repeat<BR>\n");
    }
printTrackHtml(tdb);
}

void doHgIsochore(struct trackDb *tdb, char *item)
/* do click on isochore track. */
{
char *track = tdb->tableName;
cartWebStart("Isochore Info");
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
cartWebStart("Simple Repeat Info");
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
	printf("<B>Chromosome:</B> %s<BR>\n", skipChr(rep->chrom));
	printf("<B>Begin in chromosome:</B> %d<BR>\n", rep->chromStart);
	printf("<B>End in chromosome:</B> %d<BR>\n", rep->chromEnd);
	printf("<B>Size:</B> %d<BR>\n", rep->chromEnd - rep->chromStart);
	printf("<B>Period:</B> %d<BR>\n", rep->period);
	printf("<B>Copies:</B> %4.1f<BR>\n", rep->copyNum);
	printf("<B>Consensus size:</B> %d<BR>\n", rep->consensusSize);
	printf("<B>Match Percentage:</B> %d%%<BR>\n", rep->perMatch);
	printf("<B>Insert/Delete Percentage:</B> %d%%<BR>\n", rep->perIndel);
	printf("<B>Score:</B> %d<BR>\n", rep->score);
	printf("<B>Entropy:</B> %4.3f<BR>\n", rep->entropy);
	printf("<B>Sequence:</B> %s<BR>\n", rep->sequence);
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
cartWebStart("Softberry TSSW Promoter");
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
	bedPrintPos((struct bed *)pro);
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
cartWebStart("CpG Island Info");
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
	bedPrintPos((struct bed *)island);
	printf("<B>Size:</B> %d<BR>\n", island->chromEnd - island->chromStart);
	printf("<B>CpG count:</B> %d<BR>\n", island->cpgNum);
	printf("<B>C count plus G count:</B> %d<BR>\n", island->gcNum);
	printf("<B>Percentage CpG:</B> %1.1f%%<BR>\n", island->perCpg);
	printf("<B>Percentage C or G:</B> %1.1f%%<BR>\n", island->perGc);
	printf("<BR>\n");
	htmlHorizontalLine();
	printCappedSequence(island->chromStart, island->chromEnd, 100);
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

cartWebStart("Regulatory Triangle Info");
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

void showProteinPrediction(char *geneName, char *table)
/* Fetch and display protein prediction. */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct pepPred *pp = NULL;

if (!sqlTableExists(conn, table))
    {
    warn("Predicted peptide not yet available");
    }
else
    {
    sprintf(query, "select * from %s where name = '%s'", table, geneName);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	pp = pepPredLoad(row);
	}
    sqlFreeResult(&sr);
    if (pp != NULL)
	{
	printf("<TT><PRE>");
	printf(">%s\n", geneName);
	printLines(stdout, pp->seq, 50);
	printf("</PRE></TT>");
	}
    else
        {
	warn("Sorry, currently there is no protein prediction for %s", geneName);
	}
    }
hFreeConn(&conn);
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
    missingStart += exonSize - rangeIntersection(exonStart, exonEnd, gp->cdsStart, exonEnd);
    missingEnd += exonSize - rangeIntersection(exonStart, exonEnd, exonStart, gp->cdsEnd);
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
    printf("<TT><PRE>");
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
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];

hgcStart("RefSeq mRNA");
sprintf(query, "select name,seq from refMrna where name = '%s'", geneName);
sr = sqlGetResult(conn, query);
printf("<PRE><TT>");
while ((row = sqlNextRow(sr)) != NULL)
    {
    faWriteNext(stdout, row[0], row[1], strlen(row[1]));
    }
sqlFreeResult(&sr);
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
cartWebStart("Genomic Sequence Near Gene");
printf("<H2>Get Genomic Sequence Near Gene</H2>");
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
cartContinueRadio("hgc.gene.how", "tx", "tx");
printf("Transcript<BR>");
cartContinueRadio("hgc.gene.how", "cds", "tx");
printf("Coding Region Only<BR>");
cartContinueRadio("hgc.gene.how", "txPlus", "tx");
printf("Transcript + Promoter<BR>");
cartContinueRadio("hgc.gene.how", "promoter", "tx");
printf("Promoter Only<BR>");
printf("Promoter Size: ");
cgiMakeIntVar("hgc.gene.promoterSize", 1000, 6);
printf("<BR>");
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
char *table = cartString(cart, "o");
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row = NULL;
struct genePred *gp = NULL;
struct dnaSeq *seq = NULL;
char *how = cartString(cart, "hgc.gene.how");
int start, end, promoSize;
boolean isRev;
char faLine[256];
int rowOffset = hOffsetPastBin(seqName, table);

hgcStart("Predicted mRNA");
sprintf(query, "select * from %s where name = '%s'", table, geneName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row+rowOffset);
    isRev = (gp->strand[0] == '-');
    start = gp->txStart;
    end = gp->txEnd;
    promoSize = cartInt(cart, "hgc.gene.promoterSize");
    if (sameString(how, "cds"))
        {
	start = gp->cdsStart;
	end = gp->cdsEnd;
	}
    else if (sameString(how, "txPlus"))
        {
	if (isRev)
	    {
	    end += promoSize;
	    }
	else
	    {
	    start -= promoSize;
	    if (start < 0) start = 0;
	    }
	}
    else if (sameString(how, "promoter"))
        {
	if (isRev)
	    {
	    start = gp->txEnd;
	    end = start + promoSize;
	    }
	else
	    {
	    end = gp->txStart;
	    start = end - promoSize;
	    if (start < 0) start = 0;
	    }
	}
    seq = hDnaFromSeq(gp->chrom, start, end, dnaLower);
    toUpperExons(start, seq, gp);
    if (isRev)
        reverseComplement(seq->dna, seq->size);
    printf("<TT><PRE>");
    sprintf(faLine, "%s:%d-%d %s exons in upper case",
    	gp->chrom, start+1, end,
	(isRev ? "(reverse complemented)" : "") );
    faWriteNext(stdout, faLine, seq->dna, seq->size);
    printf("</TT></PRE>");
    genePredFree(&gp);
    freeDnaSeq(&seq);
    }
sqlFreeResult(&sr);
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

cartWebStart("Known Gene");
printf("<H2>Known Gene %s</H2>\n", geneName);
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
	printf("<B>OMIM:</B> ");
	printf(
	   "<A HREF = \"http://www.ncbi.nlm.nih.gov/entrez/dispomim.cgi?id=%d\" TARGET=_blank>%d</A><BR>\n", 
	    km->omimId, km->omimId);
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
    printGeneLynx(geneName);
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

void doRefGene(struct trackDb *tdb, char *rnaName)
/* Process click on a known RefSeq gene. */
{
char *track = tdb->tableName;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
struct refLink *rl;
struct genePred *gp;

cartWebStart("Known Gene");
sprintf(query, "select * from refLink where mrnaAcc = '%s'", rnaName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find %s in refLink table - database inconsistency.", rnaName);
rl = refLinkLoad(row);
sqlFreeResult(&sr);
printf("<H2>Known Gene %s</H2>\n", rl->name);
    
printf("<B>RefSeq:</B> <A HREF=");
printEntrezNucleotideUrl(stdout, rl->mrnaAcc);
printf(" TARGET=_blank>%s</A><BR>", rl->mrnaAcc);
if (rl->omimId != 0)
    {
    printf("<B>OMIM:</B> ");
    printf(
       "<A HREF = \"http://www.ncbi.nlm.nih.gov/entrez/dispomim.cgi?id=%d\" TARGET=_blank>%d</A><BR>\n
", 
	rl->omimId, rl->omimId);
    }
if (rl->locusLinkId != 0)
    {
    printf("<B>LocusLink:</B> ");
    printf("<A HREF = \"http://www.ncbi.nlm.nih.gov/LocusLink/LocRpt.cgi?l=%d\" TARGET=_blank>",
	    rl->locusLinkId);
    printf("%d</A><BR>\n", rl->locusLinkId);
    }

medlineLinkedLine("PubMed on Gene", rl->name, rl->name);
if (rl->product[0] != 0)
    medlineLinkedLine("PubMed on Product", rl->product, rl->product);
printf("\n");
printGeneLynx(rl->name);
printf("\n");
printf("<B>GeneCards:</B> ");
printf("<A HREF = \"http://bioinfo.weizmann.ac.il/cards-bin/cardsearch.pl?search=%s\" TARGET=_blank>",
	rl->name);
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
	printf("<A HREF=\"http://www.informatics.jax.org/searches/accession_report.cgi?id=%s\" target=_BLANK>", jo.mgiId);
	printf("%s</A><BR>\n", jo.mouseSymbol);
	}
    sqlFreeResult(&sr);
    }
htmlHorizontalLine();

geneShowPosAndLinks(rl->mrnaAcc, rl->protAcc, tdb, "refPep", "htcTranslatedProtein",
	"htcRefMrna", "htcGeneInGenome", "mRNA Sequence");

puts(
   "<P>Known genes are derived from the "
   "<A HREF = \"http://www.ncbi.nlm.nih.gov/LocusLink/refseq.html\" TARGET=_blank>"
   "RefSeq</A> mRNA database. "
   "These mRNAs were mapped to the draft "
   "human genome using <A HREF = \"http://www.cse.ucsc.edu/~kent\" TARGET=_blank>"
   "Jim Kent's</A> BLAT software.</P>\n");
puts(
   "<P>Additional information may be available by clicking on the "
   "mRNA associated with this gene in the main browser window.</P>");
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
	printf("<A HREF=");
	sprintf(query, "%s[gi]", gi);
	printEntrezProteinUrl(stdout, query);
	printf(" TARGET=_blank>%s</A> %s<BR>", hom.giString, hom.description);
	}
    }
if (gotAny)
    htmlHorizontalLine();
hFreeConn(&conn);
}

void doSoftberryPred(struct trackDb *tdb, char *geneName)
/* Handle click on Softberry gene track. */
{
genericHeader(tdb, geneName);
showHomologies(geneName, "softberryHom");
geneShowCommon(geneName, tdb, "softberryPep");
printTrackHtml(tdb);
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

cartWebStart("Genomic Duplications");
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
printf("</PRE></TT>");
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
cartWebStart(itemName);
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

void pslRecalcBounds(struct psl *psl)
/* Calculate qStart/qEnd tStart/tEnd at top level to be consistent
 * with blocks. */
{
int qStart, qEnd, tStart, tEnd, size;
int last = psl->blockCount - 1;
qStart = psl->qStarts[0];
tStart = psl->tStarts[0];
size = psl->blockSizes[last];
qEnd = psl->qStarts[last] + size;
tEnd = psl->tStarts[last] + size;
if (psl->strand[0] == '-')
    reverseIntRange(&qStart, &qEnd, psl->qSize);
if (psl->strand[1] == '-')
    reverseIntRange(&tStart, &tEnd, psl->tSize);
psl->qStart = qStart;
psl->qEnd = qEnd;
psl->tStart = tStart;
psl->tEnd = tEnd;
}

struct psl *trimPsl(struct psl *oldPsl, int tMin, int tMax)
/* Return psl trimmed to fit inside tMin/tMax.  Note this does not
 * update the match/misMatch and related fields. */
{
int newSize;
int oldBlockCount = oldPsl->blockCount;
boolean tIsRc = (oldPsl->strand[1] == '-');
boolean qIsRc = (oldPsl->strand[0] == '-');
int newBlockCount = 0, completeBlockCount = 0;
int i, newI = 0;
struct psl *newPsl = NULL;
int tMn = tMin, tMx = tMax;   /* tMin/tMax adjusted for strand. */

/* Deal with case where we're completely trimmed out quickly. */
newSize = rangeIntersection(oldPsl->tStart, oldPsl->tEnd, tMin, tMax);
if (newSize <= 0)
    return NULL;

if (tIsRc)
    reverseIntRange(&tMn, &tMx, oldPsl->tSize);

/* Count how many blocks will survive trimming. */
oldBlockCount = oldPsl->blockCount;
for (i=0; i<oldBlockCount; ++i)
    {
    int s = oldPsl->tStarts[i];
    int e = s + oldPsl->blockSizes[i];
    int sz = e - s;
    int overlap;
    if ((overlap = rangeIntersection(s, e, tMn, tMx)) > 0)
        ++newBlockCount;
    if (overlap == sz)
        ++completeBlockCount;
    }

if (newBlockCount == 0)
    return NULL;

/* Allocate new psl and fill in what we already know. */
AllocVar(newPsl);
strcpy(newPsl->strand, oldPsl->strand);
newPsl->qName = cloneString(oldPsl->qName);
newPsl->qSize = oldPsl->qSize;
newPsl->tName = cloneString(oldPsl->tName);
newPsl->tSize = oldPsl->tSize;
newPsl->blockCount = newBlockCount;
AllocArray(newPsl->blockSizes, newBlockCount);
AllocArray(newPsl->qStarts, newBlockCount);
AllocArray(newPsl->tStarts, newBlockCount);

/* Fill in blockSizes, qStarts, tStarts with real data. */
newBlockCount = completeBlockCount = 0;
for (i=0; i<oldBlockCount; ++i)
    {
    int oldSz = oldPsl->blockSizes[i];
    int sz = oldSz;
    int tS = oldPsl->tStarts[i];
    int tE = tS + sz;
    int qS = oldPsl->qStarts[i];
    int qE = qS + sz;
    if (rangeIntersection(tS, tE, tMn, tMx) > 0)
        {
	int diff;
	if ((diff = (tMn - tS)) > 0)
	    {
	    tS += diff;
	    qS += diff;
	    sz -= diff;
	    }
	if ((diff = (tE - tMx)) > 0)
	    {
	    tE -= diff;
	    qE -= diff;
	    sz -= diff;
	    }
	newPsl->qStarts[newBlockCount] = qS;
	newPsl->tStarts[newBlockCount] = tS;
	newPsl->blockSizes[newBlockCount] = sz;
	++newBlockCount;
	if (sz == oldSz)
	    ++completeBlockCount;
	}
    }
pslRecalcBounds(newPsl);
return newPsl;
}

void longXenoPsl1(struct trackDb *tdb, char *item, 
	char *otherOrg, char *otherChromTable)
/* Put up cross-species alignment when the second species
 * sequence is in a nib file. */
{
struct psl *psl = NULL, *trimmedPsl = NULL;
char otherString[256];
char *cgiItem = cgiEncode(item);
char *thisOrg = hOrganism(database);

cartWebStart(tdb->longLabel);
psl = loadPslFromRangePair(tdb->tableName, item);
printf("<B>%s position:</B> %s:%d-%d<BR>\n", otherOrg,
	psl->qName, psl->qStart, psl->qEnd);
printf("<B>%s size:</B> %d<BR>\n", otherOrg, psl->qEnd - psl->qStart);
printf("<B>%s position:</B> %s:%d-%d<BR>\n", thisOrg,
	psl->tName, psl->tStart, psl->tEnd);
printf("<B>%s size:</B> %d<BR>\n", thisOrg,
	psl->tEnd - psl->tStart);
printf("<B>Bases in aligning blocks:</B> %d<BR>\n", psl->match + psl->repMatch);
printf("<B>Percent identity within aligning blocks:</B> %3.1f%%<BR>\n", 0.1*(1000 - pslCalcMilliBad(psl, FALSE)));
printf("<B>Browser window position:</B> %s:%d-%d<BR>\n", seqName, winStart, winEnd);
printf("<B>Browser window size:</B> %d<BR>\n", winEnd - winStart);
sprintf(otherString, "%d&pslTable=%s&otherOrg=%s&otherChromTable=%s", psl->tStart, 
	tdb->tableName, otherOrg, otherChromTable);
if (trimPsl(psl, winStart, winEnd) != NULL)
    {
    hgcAnchorSomewhere("htcLongXenoPsl2", cgiItem, otherString, psl->tName);
    printf("View details of parts of alignment within browser window.</A><BR>\n");
    }
printTrackHtml(tdb);
freez(&cgiItem);
}

void doBlatMus(struct trackDb *tdb, char *item)
/* Put up cross-species alignment when the second species
 * sequence is in a nib file. */
{
longXenoPsl1(tdb, item, "Mouse", "mouseChrom");
}

void doBlatHuman(struct trackDb *tdb, char *item)
/* Put up cross-species alignment when the second species
 * sequence is in a nib file. */
{
longXenoPsl1(tdb, item, "Human", "humanChrom");
}



void htcLongXenoPsl2(char *htcCommand, char *item)
/* Display alignment - loading sequence from nib file. */
{
char *pslTable = cgiString("pslTable");
char *otherChromTable = cgiString("otherChromTable");
char *otherOrg = cgiString("otherOrg");
struct psl *psl = loadPslFromRangePair(pslTable,  item);
char nibFile[512];
char query[256];
char name[256];
struct dnaSeq *musSeq = NULL;
struct sqlConnection *conn = hAllocConn();

psl = trimPsl(psl, winStart, winEnd);
sprintf(query, "select fileName from %s where chrom = '%s'", 
	otherChromTable, psl->qName);
if (sqlQuickQuery(conn, query, nibFile, sizeof(nibFile)) == NULL)
    errAbort("Sequence %s isn't in mouseChrom", psl->qName);
musSeq = nibLoadPart(nibFile, psl->qStart, psl->qEnd - psl->qStart);
snprintf(name, sizeof(name), "%s.%s", otherOrg, psl->qName);
showSomeAlignment(psl, musSeq, gftDnaX, psl->qStart, psl->qEnd, name);
hFreeConn(&conn);
}

void doBlatFish(struct trackDb *tdb, char *itemName)
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
boolean hasBin;
char table[64];

cartWebStart(itemName);
printf("<H1>Information on Tetraodon Sequence %s</H1>", itemName);

printf("Get ");
printf("<A HREF=\"%s&g=htcExtSeq&c=%s&l=%d&r=%d&i=%s\">",
      hgcPathAndSettings(), seqName, winStart, winEnd, itemName);
printf("Fish DNA</A><BR>\n");

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

cartWebStart("EST 3' Ends");
printf("<H2>EST 3' Ends</H2>\n");

rowOffset = hOffsetPastBin(seqName, "est3");
sprintf(query, "select * from est3 where chrom = '%s' and chromStart = %d",
    seqName, start);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    est3StaticLoad(row+rowOffset, &el);
    printf("<B>EST 3' End Count:</B> %d<BR>\n", el.estCount);
    bedPrintPos((struct bed *)&el);
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
    bedPrintPos((struct bed *)&rna);
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
boolean stsInfoExists = sqlTableExists(conn, "stsInfo");
boolean stsMapExists = sqlTableExists(conn, "stsMap");
struct sqlConnection *conn1 = hAllocConn();
struct sqlResult *sr = NULL, *sr1 = NULL;
char **row, **row1;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
struct stsMap stsRow;
struct stsInfo *infoRow;
char band[32], stsid[20];
int i;
struct psl *pslList = NULL, *psl;
int pslStart;

/* Print out non-sequence info */
sprintf(title, "STS Marker %s", marker);
cartWebStart(title);

/* Find the instance of the object in the bed table */ 
sprintf(query, "SELECT * FROM %s WHERE name = '%s' 
                AND chrom = '%s' AND chromStart = %d
                AND chromEnd = %d",
	        table, marker, seqName, start, end);  
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
    if (stsInfoExists)
        {
	/* Find the instance of the object in the stsInfo table */ 
	sqlFreeResult(&sr);
	sprintf(query, "SELECT * FROM stsInfo WHERE identNo = '%d'", stsRow.identNo);
	sr = sqlMustGetResult(conn, query);
	row = sqlNextRow(sr);
	if (row != NULL)
	    {
	    infoRow = stsInfoLoad(row);
	    /* Print out general sequence positional information */
            /* printf("<H2>STS Marker %s</H2>\n", infoRow->name);
	       htmlHorizontalLine(); */
	    printf("<TABLE>\n");
	    printf("<TR><TH ALIGN=left>Chromosome:</TH><TD>%s</TD></TR>\n", seqName);
	    printf("<TR><TH ALIGN=left>Start:</TH><TD>%d</TD></TR>\n",start);
	    printf("<TR><TH ALIGN=left>End:</TH><TD>%d</TD></TR>\n",end);
	    if (chromBand(seqName, start, band))
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
		    printf("<TD><A HREF=");
		    printEntrezNucleotideUrl(stdout, infoRow->genbank[i]);
		    printf(">%s</A></TD>", infoRow->genbank[i]);
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
		|| (!sameString(infoRow->marshfieldName,"")))
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
	    sprintf(query, "SELECT * FROM %s WHERE name = '%s' 
                           AND (chrom != '%s' OR chromStart != %d OR chromEnd != %d)",
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
struct stsMapMouse stsRow;
struct stsInfoMouse *infoRow;
char stsid[20];
int i;
struct psl *pslList = NULL, *psl;
int pslStart;

/* Print out non-sequence info */
sprintf(title, "STS Marker %s", marker);
cartWebStart(title);

/* Find the instance of the object in the bed table */ 
sprintf(query, "SELECT * FROM %s WHERE name = '%s' 
                AND chrom = '%s' AND chromStart = %d
                AND chromEnd = %d",
	        table, marker, seqName, start, end);  
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    stsMapMouseStaticLoad(row, &stsRow);
    /* Find the instance of the object in the stsInfo table */ 
    sqlFreeResult(&sr);
    sprintf(query, "SELECT * FROM stsInfoMouse WHERE MGIMarkerID = '%d'", stsRow.identNo);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
	{
	infoRow = stsInfoMouseLoad(row);
	printf("<TABLE>\n");
	printf("<TR><TH ALIGN=left>Chromosome:</TH><TD>%s</TD></TR>\n", seqName);
	printf("<TR><TH ALIGN=left>Start:</TH><TD>%d</TD></TR>\n",start);
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
	sprintf(query, "SELECT * FROM all_sts_primer WHERE qName = '%s'", stsid);  
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
	    sprintf(query, "SELECT * FROM %s WHERE name = '%s' 
                           AND (chrom != '%s' OR chromStart != %d OR chromEnd != %d)",
	            table, marker, seqName, start, end); 
            sr = sqlGetResult(conn,query);
            while ((row = sqlNextRow(sr)) != NULL)
		{
		stsMapMouseStaticLoad(row, &stsRow);
		printf("<TR><TD>%s:</TD><TD>%d</TD></TR>\n",
		       stsRow.chrom, (stsRow.chromStart+stsRow.chromEnd)>>1);
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
cartWebStart(clone);


/* Find the instance of the object in the bed table */ 
sprintf(query, "SELECT * FROM fishClones WHERE name = '%s' 
                AND chrom = '%s' AND chromStart = %d
                AND chromEnd = %d",
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
    printf("<TR><TH ALIGN=left>Start:</TH><TD>%d</TD></TR>\n",start);
    printf("<TR><TH ALIGN=left>End:</TH><TD>%d</TD></TR>\n",end);
    gotS = chromBand(seqName, start, sband);
    gotB = chromBand(seqName, end, eband);
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
	    printf("<TD><A HREF=");
	    printEntrezNucleotideUrl(stdout, fc->accNames[i]);
	    printf(">%s</A></TD>", fc->accNames[i]);	  
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
	    printf("<TD><A HREF=");
	    printEntrezNucleotideUrl(stdout, fc->beNames[i]);
	    printf(">%s</A></TD>", fc->beNames[i]);
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

void doUPennClones(struct trackDb *tdb, char *clone)
/* Handle click on the Univ of Penn clones track */
{
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
struct uPennClones *upc;
char sband[32], eband[32];
int i;

/* Print out non-sequence info */
cartWebStart("University of Penn BAC Clones");

/* Find the instance of the object in the bed table */ 
sprintf(query, "SELECT * FROM uPennClones WHERE name = '%s' 
                AND chrom = '%s' AND chromStart = %d
                AND chromEnd = %d",
	        clone, seqName, start, end);  
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    boolean gotS, gotB;
    upc = uPennClonesLoad(row);
    /* Print out general sequence positional information */
    printf("<H2><A HREF=");
    printCloneRegUrl(stdout, clone);
    printf(">%s</A></H2>\n", clone);
    htmlHorizontalLine();
    printf("<TABLE>\n");
    printf("<TR><TH ALIGN=left>Chromosome:</TH><TD>%s</TD></TR>\n", seqName);
    printf("<TR><TH ALIGN=left>Start:</TH><TD>%d</TD></TR>\n",start);
    printf("<TR><TH ALIGN=left>End:</TH><TD>%d</TD></TR>\n",end);
    gotS = chromBand(seqName, start, sband);
    gotB = chromBand(seqName, end, eband);
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
	printf("<TD><A HREF=");
	printEntrezNucleotideUrl(stdout, upc->accT7);
	printf(">%s</A></TD>", upc->accT7);
	printf("<TD>%s:</TD><TD ALIGN=right>%d</TD><TD ALIGN=LEFT> - %d</TD>", 
	       seqName, upc->startT7, upc->endT7);
	printf("</TR>\n");
	}
    if (upc->accSP6) 
	{
	printf("<TR><TH ALIGN=left>SP6 end sequence:</TH>");
	printf("<TD><A HREF=");
	printEntrezNucleotideUrl(stdout, upc->accSP6);
	printf(">%s</A></TD>", upc->accSP6);
	printf("<TD>%s:</TD><TD ALIGN=right>%d</TD><TD ALIGN=LEFT> - %d</TD>", 
	       seqName, upc->startSP6, upc->endSP6);
	printf("</TR>\n");
	}
    if (upc->stsMarker) 
	{
	printf("<TR><TH ALIGN=left>STS Marker:</TH>");
	printf("<TD><A HREF=");
	printEntrezUniSTSUrl(stdout, upc->stsMarker);
	printf(">%s</A></TD>", upc->stsMarker);
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

void doMouseSyn(char *track, char *itemName)
/* Handle click on mouse synteny track. */
{
struct mouseSyn el;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset;

cartWebStart("Mouse Synteny");
printf("<H2>Mouse Synteny</H2>\n");

sprintf(query, "select * from %s where chrom = '%s' and chromStart = %d",
    track, seqName, start);
rowOffset = hOffsetPastBin(seqName, track);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    mouseSynStaticLoad(row+rowOffset, &el);
    printf("<B>mouse chromosome:</B> %s<BR>\n", el.name+6);
    printf("<B>human chromosome:</B> %s<BR>\n", skipChr(el.chrom));
    printf("<B>human starting base:</B> %d<BR>\n", el.chromStart);
    printf("<B>human ending base:</B> %d<BR>\n", el.chromEnd);
    printf("<B>size:</B> %d<BR>\n", el.chromEnd - el.chromStart);
    htmlHorizontalLine();
    }
sqlFreeResult(&sr);
hFreeConn(&conn);

puts("<P>This track syntenous (corresponding) regions between human "
     "and mouse chromosomes.  This track was created by looking for "
     "homology to known mouse genes in the draft assembly.  The data "
     "for this track was kindly provided by "
     "<A HREF=\"mailto:church@ncbi.nlm.nih.gov\">Deanna Church</A> at NCBI. Please "
     "visit <A HREF=\"http://www.ncbi.nlm.nih.gov/Homology/\" TARGET = _blank>"
     "http://www.ncbi.nlm.nih.gov/Homology/</A> for more details.");
}

void doSnp(struct trackDb *tdb, char *itemName)
/* Put up info on a SNP. */
{
char *group = tdb->tableName;
struct snp snp;
int start = cartInt(cart, "o");
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
int rowOffset;

cartWebStart("Single Nucleotide Polymorphism (SNP)");
printf("<H2>Single Nucleotide Polymorphism (SNP) rs%s</H2>\n", itemName);

sprintf(query, "select * from %s where chrom = '%s' and chromStart = %d and name = '%s'",
    group, seqName, start, itemName);
rowOffset = hOffsetPastBin(seqName, group);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    snpStaticLoad(row+rowOffset, &snp);
    bedPrintPos((struct bed *)&snp);
    }
printf(
  "<P><A HREF=\"http://www.ncbi.nlm.nih.gov/SNP/snp_ref.cgi?type=rs&rs=%s\" TARGET=_blank>", 
  snp.name);
printf("dbSNP link</A></P>\n");
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


#ifdef FUREY_CODE

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
sprintf(query, "SELECT * FROM %s WHERE name = '%s' 
                AND (chrom != '%s'
                OR chromStart != %d OR chromEnd != %d)",
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
int i;
struct lfs *lfs, *lfsList = NULL;
struct psl *pslList = NULL, *psl;


/* Determine type */
if (sameString("bacEndPairs", track)) 
    {
    sprintf(title, "Location of %s using BAC end sequences", clone);
    lfLabel = "BAC ends";
    table = track;
    }
if (sameString("uPennBacEndPairs", track)) 
    {
    sprintf(title, "Location of %s using BAC end sequences", clone);
    lfLabel = "BAC ends";
    table = track;
    }

/* Print out non-sequence info */
cartWebStart(title);

/* Find the instance of the object in the bed table */ 
sprintf(query, "SELECT * FROM %s WHERE name = '%s' 
                AND chrom = '%s' AND chromStart = %d
                AND chromEnd = %d",
	        table, clone, seqName, start, end);  
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    lfs = lfsLoad(row+1);
    if ((sameString("bacEndPairs", track)) || (sameString("uPennBacEndPairs", track))) 
    {
    printf("<H2><A HREF=");
    printCloneRegUrl(stdout, clone);
    printf(">%s</A></H2>\n", clone);
    }
    /*printf("<H2>%s - %s</H2>\n", type, clone);*/
    printf("<P><HR ALIGN=\"CENTER\"></P>\n<TABLE>\n");
    printf("<TR><TH ALIGN=left>Chromosome:</TH><TD>%s</TD></TR>\n",seqName);
    printf("<TR><TH ALIGN=left>Start:</TH><TD>%d</TD></TR>\n",start);
    printf("<TR><TH ALIGN=left>End:</TH><TD>%d</TD></TR>\n",end);
    printf("<TR><TH ALIGN=left>Strand:</TH><TD>%s</TD></TR>\n", lfs->strand);
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
      printf("<H3><A HREF=");
      printEntrezNucleotideUrl(stdout, lfs->lfNames[i]);
      printf(" >%s</A></H3>\n", lfs->lfNames[i]);
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

void doCgh(char *track, char *tissue, struct trackDb *tdb)
/* Create detail page for comparative genomic hybridization track */ 
{
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;

/* Print out non-sequence info */
cartWebStart(tissue);

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
sprintf(query, "SELECT spot from cgh where chrom = '%s' AND
                chromStart <= '%d' AND chromEnd >= '%d' AND
                tissue = '%s' AND type = 3 GROUP BY spot ORDER BY chromStart",
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
cartWebStart(title);

/* Print general range info */
/*printf("<H2>MCN Breakpoints - %s</H2>\n", name);
printf("<P><HR ALIGN=\"CENTER\"></P>");*/
printf("<TABLE>\n");
printf("<TR><TH ALIGN=left>Chromosome:</TH><TD>%s</TD></TR>\n",seqName);
printf("<TR><TH ALIGN=left>Begin in Chromosome:</TH><TD>%d</TD></TR>\n",start);
printf("<TR><TH ALIGN=left>End in Chromosome:</TH><TD>%d</TD></TR>\n",end);
if (chromBand(seqName, start, startBand) && chromBand(seqName, end - 1, endBand))
    printf("<TR><TH Align=left>Chromosome Band Range:</TH><TD>%s - %s<TD></TR>\n",
	   startBand, endBand);	
printf("</TABLE>\n");

/* Find all of the breakpoints in this range for this name*/
sprintf(query, "SELECT * FROM mcnBreakpoints WHERE chrom = '%s' AND
                chromStart = %d and chromEnd = %d AND name = '%s'",
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

#endif /* FUREY_CODE */


#ifdef ROGIC_CODE

void doMgcMrna(char *track, char *acc)
/* Redirects to genbank record */
{

  printf("Content-Type: text/html\n\n<HTML><BODY><SCRIPT>\n");
  printf("location.replace('http://www.ncbi.nlm.nih.gov/htbin-post/Entrez/query?form=4&db=n&term=%s');\n",acc); 
  printf("</SCRIPT> <NOSCRIPT> No JavaScript support. Click <b><a href=\"http://www.ncbi.nlm.nih.gov/htbin-post/Entrez/query?form=4&db=n&term=%s\">continue</a></b> for the requested GenBank report. </NOSCRIPT>\n",acc); 
}

#endif /* ROGIC_CODE */

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
    printf("<b>Name:</b> %s<br>\n",dp->name);
    printf("<b>Dna:</b> %s", dp->dna );
    printf("[<a href=\"hgBlat?type=DNA&genome=hg8&sort=&query,score&output=hyperlink&userSeq=%s\">blat</a>]<br>", dp->dna);
    printf("<b>Size:</b> %d<br>", dp->size );
    printf("<b>Chrom:</b> %s<br>", dp->chrom );
    printf("<b>ChromStart:</b> %d<br>", dp->start );
    printf("<b>ChromEnd:</b> %d<br>", dp->end );
    printf("<b>Strand:</b> %s<br>", dp->strand );
    printf("<b>3' Dist:</b> %d<br>", dp->tpDist );
    printf("<b>Tm:</b> %f<br>", dp->tm );
    printf("<b>%%GC:</b> %f<br>", dp->pGC );
    printf("<b>Affy:</b> %d<br>", dp->affyHeur );
    printf("<b>Sec Struct:</b> %f<br>", dp->secStruct);
    printf("<b>blatScore:</b> %d<br>", dp->blatScore );
    printf("<b>Comparison:</b> %f<br>", dp->comparison);
    printf("<hr>\n");
    }
printf("<h3>Genomic Details:</h3>\n");
genericBedClick(conn, tdb, item, start, 1);
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
    bedPrintPos(bed);
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
    bedPrintPos(bed);

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

cartWebStart("Percentage GC in 20,000 Base Windows (GC)");

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

void makeCheckBox(char *name, boolean isChecked)
/* Create a checkbox with the given name in the given state. */
{
printf("<INPUT TYPE=CHECKBOX NAME=\"%s\" VALUE=on%s>", name,
    (isChecked ? " CHECKED" : "") );
}


struct rgbColor getColorForExprBed(float val, float max)
/* Return the correct color for a given score */
{
char *colorScheme = cartUsualString(cart, "exprssn.color", "rg");
boolean redGreen = sameString(colorScheme, "rg");
float absVal = fabs(val);
struct rgbColor color; 
int colorIndex = 0;
/* if log score is -10000 data is missing */
if(val == -10000) 
    {
    color.g = color.r = color.b = 128;
    return(color);
    }

if(absVal > max) 
    absVal = max;
if (max == 0) 
    errAbort("ERROR: hgc::getColorForExprBed() maxDeviation can't be zero\n"); 
colorIndex = (int)(absVal * 255/max);
if(redGreen) 
    {
    if(val > 0) 
	{
	color.r = colorIndex; 
	color.g = 0;
	color.b = 0;
	}
    else 
	{
	color.r = 0;
	color.g = colorIndex;
	color.b = 0;
	}
    }
else
    {
    if(val > 0) 
	{
	color.r = colorIndex; 
	color.g = 0;
	color.b = 0;
	}
    else 
	{
	color.r = 0;
	color.g = 0;
	color.b = colorIndex;
	}
    }
return color;
}

struct rgbColor getColorForAffyBed(float val, float max)
/* Return the correct color for a given score */
{
struct rgbColor color; 
int colorIndex = 0;
int offset = 0;

/* if log score is -10000 data is missing */
if(val == -10000) 
    {
    color.g = color.r = color.b = 128;
    return(color);
    }

val = fabs(val);
/* take the log for visualization */
if(val > 0)
    val = logBase2(val);
else
    val = 0;

/* scale offset down to 0 */
if(val > offset) 
    val -= offset;
else
    val = 0;

if (max <= 0) 
    errAbort("ERROR: hgc::getColorForAffyBed() maxDeviation can't be zero\n"); 
max = logBase2(max);
max -= offset;
if(max < 0)
    errAbort("hgc::getColorForAffyBed() - Max val should be greater than 0 but it is: %g", max);
    
if(val > max) 
    val = max;

colorIndex = (int)(val * 255/max);
color.r = 0;
color.g = 0;
color.b = colorIndex;
return color;
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

char *abbrevExprBedName(char *name)
/* chops part off rosetta exon identifiers, returns pointer to 
local static char* */
{
static char abbrev[32];
char *ret;
strncpy(abbrev, name, sizeof(abbrev));
abbr(abbrev, "LINK_Em:");
return abbrev;
}

void printTableHeaderName(char *name, char *clickName, char *url) 
/* creates a table to display a name vertically,
 * basically creates a column of letters 
 */
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

void msBedPrintTableHeader(struct bed *bedList, struct hash *erHash, char *itemName, 
			    char **headerNames, int headerCount, char *scoresHeader)
/* print out a bed with multiple scores header for a table.
   headerNames contain titles of columns up to the scores columns. scoresHeader
   is a single string that will span as many columns as there are beds.*/
{
struct bed *bed;
int featureCount = slCount(bedList);
int i=0;
printf("<tr>");
for(i=0;i<headerCount; i++)
    printf("<th align=center>%s</th>\n",headerNames[i]);
printf("<th align=center colspan=%d valign=top>%s</th>\n",featureCount, scoresHeader);
printf("</tr>\n<tr>");
for(i=0;i<headerCount; i++)
    printf("<td>&nbsp</td>\n");
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    printf("<td valign=top align=center>\n");
    printTableHeaderName(bed->name, itemName, NULL);
    printf("</td>");
    }
printf("</tr>\n");
}

void msBedDefaultPrintHeader(struct bed *bedList, struct hash *erHash, char *itemName)
/* print out a header with names for each bed with itemName highlighted */
{
char *headerNames[] = {"Experiment"};
char *scoresHeader = "Item Name";
msBedPrintTableHeader(bedList, erHash, itemName, headerNames, ArraySize(headerNames), scoresHeader);
}

void rosettaPrintHeader(struct bed *bedList, struct hash *erHash, char *itemName)
/* print out the header for the rosetta details table */
{
char *headerNames[] = {"&nbsp", "Hybridization"};
char *scoresHeader = "Exon Number";
msBedPrintTableHeader(bedList, erHash, itemName, headerNames, ArraySize(headerNames), scoresHeader);
}

void cghNci60PrintHeader(struct bed *bedList, struct hash *erHash, char *itemName)
/* print out the header for the CGH NCI 60 details table */
{
char *headerNames[] = {"Cell Line", "Tissue"};
char *scoresHeader ="CGH Log Ratio";
msBedPrintTableHeader(bedList, erHash, itemName, headerNames, ArraySize(headerNames), scoresHeader);
}

void printExprssnColorKey(float minVal, float maxVal, float stepSize,
			  struct rgbColor(*getColor)(float val, float maxVal))
/* print out a little table which provides a color->score key */
{
float currentVal = -1 * maxVal;
int square = 15;
int numColumns;
assert(stepSize != 0);

numColumns = maxVal/stepSize *2+1;
printf("<TABLE  BGCOLOR=\"#000000\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>");
printf("<TABLE  BGCOLOR=\"#fffee8\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR>");
printf("<th colspan=%d>False Color Key, all values log base 2</th></tr><tr>\n",numColumns);
/* have to add the stepSize/2 to account for the ability to 
   absolutely represent some numbers as floating points */
for(currentVal = minVal; currentVal <= maxVal + (stepSize/2); currentVal += stepSize)
    {
    printf("<th><b>%.2f</b></th>", currentVal);
    }
printf("</tr><tr>\n");
for(currentVal = minVal; currentVal <= maxVal + (stepSize/2); currentVal += stepSize)
    {
    struct rgbColor rgb = getColor(currentVal, maxVal);
    printf("<td bgcolor=\"#%.2X%.2X%.2X\">&nbsp</td>\n", rgb.r, rgb.g, rgb.b);
    }
printf("</tr></table>\n");
printf("</td></tr></table>\n");
}

void printAffyExprssnColorKey(float minVal, float maxVal, float stepSize,
			  struct rgbColor(*getColor)(float val, float maxVal))
/* print out a little table which provides a color->score key */
{
float currentVal = -1 * maxVal;
int square = 15;
int numColumns;

assert(maxVal != 0);
if(minVal != 0)
    numColumns = logBase2(maxVal) - logBase2(minVal);
else 
    numColumns = logBase2(maxVal);
printf("<TABLE  BGCOLOR=\"#000000\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>\n");
printf("<TABLE  BGCOLOR=\"#fffee8\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\">\n<tr>");
printf("<th colspan=%d>False Color Key</th></tr>\n<tr>",numColumns);
printf("<th width=55><b> NA </b></th>");
for(currentVal = minVal; currentVal <= maxVal; currentVal = (2*currentVal))
    {
    printf("<th width=55><b> %7.2g </b></th>", currentVal);
    }
printf("</tr>\n<tr>");
printf("<td bgcolor=\"#%.2X%.2X%.2X\">&nbsp</td>\n", 128,128,128);
for(currentVal = minVal; currentVal <= maxVal; currentVal = (2*currentVal))
    {
    struct rgbColor rgb = getColor(currentVal, maxVal);
    printf("<td bgcolor=\"#%.2X%.2X%.2X\">&nbsp</td>\n", rgb.r, rgb.g, rgb.b);
    }
printf("</tr></table>\n");
printf("</td></tr></table>\n");
}

void msBedExpressionPrintRow(struct bed *bedList, struct hash *erHash, int expIndex, char *expName, float maxScore)
/* print the name of the experiment and color the 
   background of individual cells using the score to 
   create false two color display */
{
char buff[32];
struct bed *bed = bedList;
struct expRecord *er = NULL;
int square = 10;
snprintf(buff, sizeof(buff), "%d", expIndex);
er = hashMustFindVal(erHash, buff);

printf("<tr>\n");
if(strstr(er->name, expName))
    printf("<td align=left bgcolor=\"D9E4F8\"> %s</td>\n",er->name);
else
    printf("<td align=left> %s</td>\n", er->name);

for(bed = bedList;bed != NULL; bed = bed->next)
    {
	/* use the background colors to creat patterns */
	struct rgbColor rgb = getColorForExprBed(bed->expScores[expIndex], maxScore);
	printf("<td height=%d width=%d bgcolor=\"#%.2X%.2X%.2X\">&nbsp</td>\n", square, square, rgb.r, rgb.g, rgb.b);
	}
printf("</tr>\n");
}

void msBedAffyPrintRow(struct bed *bedList, struct hash *erHash, int expIndex, char *expName, float maxScore)
/* print the name of the experiment and color the 
   background of individual cells using the score to 
   create false two color display */
{
char buff[32];
struct bed *bed = bedList;
struct expRecord *er = NULL;
int square = 10;
snprintf(buff, sizeof(buff), "%d", expIndex);
er = hashMustFindVal(erHash, buff);

printf("<tr>\n");
if(strstr(er->name, expName))
    printf("<td align=left bgcolor=\"D9E4F8\"> %s</td>\n",er->name);
else
    printf("<td align=left> %s</td>\n", er->name);

for(bed = bedList;bed != NULL; bed = bed->next)
    {
	/* use the background colors to creat patterns */
	struct rgbColor rgb = getColorForAffyBed(bed->expScores[expIndex], maxScore);
	printf("<td height=%d width=%d bgcolor=\"#%.2X%.2X%.2X\">&nbsp</td>\n", square, square, rgb.r, rgb.g, rgb.b);
	}
printf("</tr>\n");
}


void msBedPrintTable(struct bed *bedList, struct hash *erHash, char *itemName, 
		     char *expName, float minScore, float maxScore, float stepSize,
		     void(*printHeader)(struct bed *bedList, struct hash *erHash, char *item),
		     void(*printRow)(struct bed *bedList,struct hash *erHash, int expIndex, char *expName, float maxScore),
		     void(*printKey)(float minVal, float maxVal, float size, struct rgbColor(*getColor)(float val, float max)),
		     struct rgbColor(*getColor)(float val, float max))
/* prints out a table from the data present in the bedList */
{
int i,featureCount=0, currnetRow=0, square=10;
struct bed *bed = NULL;
char buff[32];
if(bedList == NULL)
    errAbort("hgc::msBedPrintTable() - bedList is NULL");

featureCount = slCount(bedList);
/* time to write out some html, first the table and header */
if(printKey != NULL)
    printKey(minScore, maxScore, stepSize, getColor);
printf("<p>\n");
printf("<basefont size=-1>\n");
printf("<table  bgcolor=\"#000000\" border=\"0\" cellspacing=\"0\" cellpadding=\"1\"><tr><td>");
printf("<table  bgcolor=\"#fffee8\" border=\"0\" cellspacing=\"0\" cellpadding=\"1\">");
printHeader(bedList, erHash, itemName);
for(i=0; i<bedList->expCount; i++)
    {
    printRow(bedList, erHash, i, expName, maxScore);
    }
printf("</table>");
printf("</td></tr></table>");
printf("</basefont>");
}

struct bed * loadMsBed(char *table, char *chrom, uint start, uint end)
/* load every thing from a bed 15 table in the given range */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;
struct bed *bedList = NULL, *bed;
sr = hRangeQuery(conn, table, chrom, start, end, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row+rowOffset, 15);
    slAddHead(&bedList, bed);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&bedList);
return bedList;
}

struct bed * loadMsBedAll(char *table)
/* load every thing from a bed 15 table */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct bed *bedList = NULL, *bed;
char query[512];
sprintf(query, "select * from %s", table); 
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row, 15);
    slAddHead(&bedList, bed);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&bedList);
return bedList;
}

struct expRecord * loadExpRecord(char *table, char *database)
/* load everything from an expRecord table in the
   specified database, usually hgFixed instead of hg7, hg8, etc. */
{
struct sqlConnection *conn = sqlConnect(database);
char query[256];
struct expRecord *erList = NULL;
snprintf(query, sizeof(query), "select * from %s", table);
erList = expRecordLoadByQuery(conn, query);
sqlDisconnect(&conn);
return erList;
}

void msBedGetExpDetailsLink(char *expName, struct trackDb *tdb, char *expTable)
/* Create link to download data from a MsBed track */
{
char *msBedTable = tdb->tableName;

printf("<H3>Download raw data for experiments:</H3>\n");
printf("<UL>\n");
hgcAnchorSomewhere("getMsBedAll", expTable, msBedTable, seqName);
printf("<LI>All data</A>\n");
hgcAnchorSomewhere("getMsBedRange", expTable, msBedTable, seqName);
printf("<LI>Data in range</A>\n");
printf("</UL>\n");
}

void getMsBedExpDetails(struct trackDb *tdb, char *expName, boolean all)
/* Create tab-delimited output to download */
{
char *expTable = cartString(cart, "i");
char *bedTable = cartString(cart, "o");
struct expRecord *er, *erList=NULL;
struct bed *b, *bedList=NULL;
char line[1024];
int i;

/* Get all of the expression record details */
erList = loadExpRecord(expTable, "hgFixed");

/* Get either all of the data, or only that data in the range */
if (all) 
    bedList = loadMsBedAll(bedTable);
else 
    bedList = loadMsBed(bedTable, seqName, winStart, winEnd); 

/* Print out a header row */
printf("<HTML><BODY><PRE>\n");
printf("Name\tChr\tChrStart\tChrEnd\tTallChrStart\tTallChrEnd");
for (er = erList; er != NULL; er = er->next)
    if (sameString(bedTable, "cghNci60"))
	printf("\t%s(%s)",er->name, er->extras[1]);
    else 
	printf("\t%s",er->name);
printf("\n");

/* Print out a row for each of the record in the bedList */
for (b = bedList; b != NULL; b = b->next)
    {
    printf("%s\t%s\t%d\t%d\t%d\t%d",b->name, b->chrom, b->chromStart, b->chromEnd, b->thickStart, b->thickEnd);
    for (i = 0; i < b->expCount; i++)
	if (i == b->expIds[i])
	    printf("\t%f",b->expScores[i]);
	else
	    printf("\t");
    printf("\n");
    }
printf("</PRE>");
}

struct bed *rosettaFilterByExonType(struct bed *bedList)
/* remove beds from list depending on user preference for 
   seeing confirmed and/or predicted exons */
{
struct bed *bed=NULL, *tmp=NULL, *tmpList=NULL;
char *exonTypes = cartUsualString(cart, "rosetta.et", rosettaExonEnumToString(0));
enum rosettaExonOptEnum et = rosettaStringToExonEnum(exonTypes);

if(et == rosettaAllEx)
    return bedList;
/* go through and remove appropriate beds */
for(bed = bedList; bed != NULL; )
    {
    if(et == rosettaConfEx)
	{
	tmp = bed->next;
	if(bed->name[strlen(bed->name) -2] == 't')
	    slSafeAddHead(&tmpList, bed);
	else
	    bedFree(&bed);
	bed = tmp;
	}
    else if(et == rosettaPredEx)
	{
	tmp = bed->next;
	if(bed->name[strlen(bed->name) -2] == 'p')
	    slSafeAddHead(&tmpList, bed);
	else
	    bedFree(&bed);
	bed = tmp;
	}
    }
slReverse(&tmpList);
return tmpList;
}

void rosettaPrintRow(struct bed *bedList, struct hash *erHash, int expIndex, char *expName, float maxScore)
/* print a row in the details table for rosetta track, designed for
   use msBedPrintTable */
{
char buff[32];
struct bed *bed = bedList;
struct expRecord *er = NULL;
int square = 10;
snprintf(buff, sizeof(buff), "%d", expIndex);
er = hashMustFindVal(erHash, buff);

sprintf(buff,"e%d",er->id);
printf("<tr>\n");
printf("<td align=left>");
makeCheckBox(buff,FALSE);
printf("</td>");
if(strstr(er->name, expName))
    printf("<td align=left bgcolor=\"D9E4F8\"> %s</td>\n",er->name);
else
    printf("<td align=left> %s</td>\n", er->name);
for(bed = bedList;bed != NULL; bed = bed->next)
    {
	/* use the background colors to creat patterns */
	struct rgbColor rgb = getColorForExprBed(bed->expScores[expIndex], maxScore);
	printf("<td height=%d width=%d bgcolor=\"#%.2X%.2X%.2X\">&nbsp</td>\n", square, square, rgb.r, rgb.g, rgb.b);
	}
printf("</tr>\n");
}

void rosettaPrintDataTable(struct bed *bedList, char *itemName, char *expName, float maxScore, char *tableName)
/* creates a false color table of the data in the bedList */
{
struct expRecord *erList = NULL, *er;
struct hash *erHash;
float stepSize = 0.2;
char buff[32];
if(bedList == NULL)
    printf("<b>No Expression Data in this Range.</b>\n");
else 
    {
    erHash = newHash(2);
    erList = loadExpRecord(tableName, "hgFixed");
    for(er = erList; er != NULL; er=er->next)
	{
	snprintf(buff, sizeof(buff), "%d", er->id);
	hashAddUnique(erHash, buff, er);
	}
    msBedPrintTable(bedList, erHash, itemName, expName, -1*maxScore, maxScore, stepSize, 
		    rosettaPrintHeader, rosettaPrintRow, printExprssnColorKey, getColorForExprBed);
    expRecordFreeList(&erList);
    hashFree(&erHash);
    bedFreeList(&bedList);
    }
}

void rosettaDetails(struct trackDb *tdb, char *expName)
/* print out a page for the rosetta data track */
{
struct bed *bedList, *bed=NULL;
char *tableName = "rosettaExps";
char *itemName = cgiUsualString("i2","none");
char *nameTmp=NULL;
char buff[256];
char *plotType = NULL;
float maxScore = 2.0;
char *maxIntensity[] = { "100", "20", "15", "10", "5" ,"4","3","2","1" };
char *exonTypes = cartUsualString(cart, "rosetta.et", rosettaExonEnumToString(0));
enum rosettaExonOptEnum et = rosettaStringToExonEnum(exonTypes);

/* get data from database and filter it */
bedList = loadMsBed(tdb->tableName, seqName, winStart, winEnd);
bedList = rosettaFilterByExonType(bedList);


/* abbreviate the names */
for(bed=bedList; bed != NULL; bed = bed->next)
    {
    nameTmp = abbrevExprBedName(bed->name);
    freez(&bed->name);
    bed->name = cloneString(nameTmp);
    }

/* start html */
snprintf(buff, sizeof(buff), "Rosetta Expression Data For: %s %d-%d</h2>", seqName, winStart, winEnd);
cartWebStart(buff);
printf("%s", tdb->html);
printf("<br><br>");
printf("<form action=\"../cgi-bin/rosChr22VisCGI\" method=get>\n");
rosettaPrintDataTable(bedList, itemName, expName, maxScore, tableName);

/* other info needed for plotting program */
cgiMakeHiddenVar("table", tdb->tableName);
cgiMakeHiddenVar("db", database);
sprintf(buff,"%d",winStart);
cgiMakeHiddenVar("winStart", buff);
zeroBytes(buff,64);
sprintf(buff,"%d",winEnd);

/* plot type is passed to graphing program to tell it which exons to use */
if(et == rosettaConfEx)
    plotType = "te";
else if(et == rosettaPredEx)
    plotType = "pe";
else if(et == rosettaAllEx) 
    plotType = "e";
else 
    errAbort("hgc::rosettaDetails() - don't recognize rosettaExonOptEnum %d", et);
cgiMakeHiddenVar("t",plotType);
cgiMakeHiddenVar("winEnd", buff);
printf("<br>\n");
printf("<table width=\"100%%\" cellpadding=0 cellspacing=0>\n");
printf("<tr><th align=left><h3>Plot Options:</h3></th></tr><tr><td><p><br>");
cgiMakeDropList("mi",maxIntensity, 9, "20");
printf(" Maximum Intensity value to allow.\n");
printf("</td></tr><tr><td align=center><br>\n");
printf("<b>Press Here to View Detailed Plots</b><br><input type=submit name=Submit value=submit>\n");
printf("<br><br><br><b>Clear Values</b><br><input type=reset name=Reset></form>\n");
printf("</td></tr></table>");
}

void nci60Details(struct trackDb *tdb, char *expName) 
/* print out a page for the nci60 data from stanford */
{
struct bed *bedList;
char *tableName = "nci60Exps";
char *itemName = cgiUsualString("i2","none");
struct expRecord *erList = NULL, *er;
char buff[32];
struct hash *erHash;
float stepSize = 0.2;
float maxScore = 2.0;
bedList = loadMsBed(tdb->tableName, seqName, winStart, winEnd);
genericHeader(tdb, itemName);

printf("%s", tdb->html);
printf("<br><br>");
if(bedList == NULL)
    printf("<b>No Expression Data in this Range.</b>\n");
else 
    {
    erHash = newHash(2);
    erList = loadExpRecord(tableName, "hgFixed");
    for(er = erList; er != NULL; er=er->next)
	{
	snprintf(buff, sizeof(buff), "%d", er->id);
	hashAddUnique(erHash, buff, er);
	}
    msBedPrintTable(bedList, erHash, itemName, expName, -1*maxScore, maxScore, stepSize,
		    msBedDefaultPrintHeader, msBedExpressionPrintRow, printExprssnColorKey, getColorForExprBed);
    expRecordFreeList(&erList);
    hashFree(&erHash);
    bedFreeList(&bedList);
    }
}

void printAffyLinks(char *name)
/* print out links to affymetrix's netaffx website */
{
char *netaffx = "https://www.netaffx.com/LinkServlet?array=U95&probeset=";
char *netaffxDisp = "https://www.netaffx.com/svghtml?query=";
char *gnfDetailed = "http://expression.gnf.org/cgi-bin/index.cgi?text=";
/* char *gnf = "http://expression.gnf.org/promoter/tissue/images/"; */
if(name != NULL)
    {
    printf("<p>More information about individual probes and probe sets is available ");
    printf("at Affymetrix's <a href=\"https://www.netaffx.com/index2.jsp\">netaffx.com</a> website. [registration required]\n");
    printf("<ul>\n");
    printf("<li> Information about probe sequences is <a href=\"%s%s\">available there</a></li>\n",
	   netaffx, name);
    printf("<li> A graphical representation is also <a href=\"%s%s\">available</a> ",netaffxDisp, name);
    printf("<basefont size=-2>[svg viewer required]</basefont></li>\n");
    printf("</ul>\n");
    printf("<p>A <a href=\"%s%s\">histogram</a> of the data for the probe set selected (%s) over all ",gnfDetailed, name, name);
    printf("tissues is available at the<a href=\"http://expression.gnf.org/cgi-bin/index.cgi\"> GNF web supplement</a>.\n");
    }
}

void affyDetails(struct trackDb *tdb, char *expName) 
/* print out a page for the affy data from gnf */
{
struct bed *bedList;
char *tableName = "affyExps";
char *itemName = cgiUsualString("i2","none");
int stepSize = 1;
struct expRecord *erList = NULL, *er;
char buff[32];
struct hash *erHash;
float maxScore = 262144/16; /* 2^18/2^4 */
float minScore = 2;
bedList = loadMsBed(tdb->tableName, seqName, winStart, winEnd);
genericHeader(tdb, itemName);
printf("<h2></h2><p>\n");
printf("%s", tdb->html);

printAffyLinks(itemName);
if(bedList == NULL)
    printf("<b>No Expression Data in this Range.</b>\n");
else 
    {
    erHash = newHash(2);
    erList = loadExpRecord(tableName, "hgFixed");
    for(er = erList; er != NULL; er=er->next)
	{
	snprintf(buff, sizeof(buff), "%d", er->id);
	hashAddUnique(erHash, buff, er);
	}
    printf("<h2></h2><p>\n");
    msBedPrintTable(bedList, erHash, itemName, expName, minScore, maxScore, stepSize,
		    msBedDefaultPrintHeader, msBedAffyPrintRow, printAffyExprssnColorKey, getColorForAffyBed);
    expRecordFreeList(&erList);
    hashFree(&erHash);
    bedFreeList(&bedList);
    }
}

void affyRatioDetails(struct trackDb *tdb, char *expName) 
/* print out a page for the affy data from gnf based on ratio of
* measurements to the median of the measurements. */
{
struct bed *bedList;
char *tableName = "affyExps";
char *itemName = cgiUsualString("i2","none");
struct expRecord *erList = NULL, *er;
char buff[32];
struct hash *erHash;
float stepSize = 0.5;
float maxScore = 3.0;

bedList = loadMsBed(tdb->tableName, seqName, winStart, winEnd);
genericHeader(tdb, itemName);
printf("<h2></h2><p>\n");
printf("%s", tdb->html);

printAffyLinks(itemName);
if(bedList == NULL)
    printf("<b>No Expression Data in this Range.</b>\n");
else 
    {
    erHash = newHash(2);
    erList = loadExpRecord(tableName, "hgFixed");
    for(er = erList; er != NULL; er=er->next)
	{
	snprintf(buff, sizeof(buff), "%d", er->id);
	hashAddUnique(erHash, buff, er);
	}
    printf("<h2></h2><p>\n");
    msBedPrintTable(bedList, erHash, itemName, expName, -1*maxScore, maxScore, stepSize,
		    msBedDefaultPrintHeader, msBedExpressionPrintRow, printExprssnColorKey, getColorForExprBed);
    expRecordFreeList(&erList);
    hashFree(&erHash);
    bedFreeList(&bedList);
    }
}


struct rgbColor getColorForCghBed(float val, float max)
/* Return the correct color for a given score */
{
char *colorScheme = cartUsualString(cart, "cghNci60.color", "gr");
boolean redColor = sameString(colorScheme, "rg");
float absVal = fabs(val);
struct rgbColor color; 
int colorIndex = 0;
/* if log score is -10000 data is missing */
if(val == -10000) 
    {
    color.g = color.r = color.b = 128;
    return(color);
    }

if(absVal > max) 
    absVal = max;
if (max == 0) 
    errAbort("ERROR: hgc::getColorForCghBed() maxDeviation can't be zero\n"); 
colorIndex = (int)(absVal * 255/max);
if(sameString(colorScheme, "gr")) 
    {
    if(val < 0) 
	{
	color.r = colorIndex; 
	color.g = 0;
	color.b = 0;
	}
    else 
	{
	color.r = 0;
	color.g = colorIndex;
	color.b = 0;
	}
    } 
else if(sameString(colorScheme, "rg")) 
    {
    if(val > 0) 
	{
	color.r = colorIndex; 
	color.g = 0;
	color.b = 0;
	}
    else 
	{
	color.r = 0;
	color.g = colorIndex;
	color.b = 0;
	}
    }
else
    {
    if(val > 0) 
	{
	color.r = colorIndex; 
	color.g = 0;
	color.b = 0;
	}
    else 
	{
	color.r = 0;
	color.g = 0;
	color.b = colorIndex;
	}
    }
return color;
}

void msBedCghPrintRow(struct bed *bedList, struct hash *erHash, int expIndex, char *expName, float maxScore)
/* print the name of the experiment and color the 
   background of individual cells using the score to 
   create false two color display */
{
char buff[32];
struct bed *bed = bedList;
struct expRecord *er = NULL;
int square = 10;
snprintf(buff, sizeof(buff), "%d", expIndex);
er = hashMustFindVal(erHash, buff);

printf("<tr>\n");
if(strstr(er->name, expName))
    printf("<td align=left bgcolor=\"D9E4F8\"> %s</td>\n",er->name);
else
    printf("<td align=left> %s</td>\n", er->name);

printf("<td align=left>  %s</td>\n", er->extras[1]);
for(bed = bedList;bed != NULL; bed = bed->next)
    {
	/* use the background colors to creat patterns */
	struct rgbColor rgb = getColorForCghBed(bed->expScores[expIndex], maxScore);
	printf("<td height=%d width=%d bgcolor=\"#%.2X%.2X%.2X\">&nbsp</td>\n", square, square, rgb.r, rgb.g, rgb.b);
	}
printf("</tr>\n");
}

void cghNci60Details(struct trackDb *tdb, char *expName) 
/* print out a page for the nci60 data from stanford */
{
struct bed *bedList;
char *tableName = "cghNci60Exps";
char *itemName = cgiUsualString("i2","none");
struct expRecord *erList = NULL, *er;
char buff[32];
struct hash *erHash;
float stepSize = 0.2;
float maxScore = 1.6;
bedList = loadMsBed(tdb->tableName, seqName, winStart, winEnd);
genericHeader(tdb, itemName);

printf("%s", tdb->html);
printf("<br><br>");
msBedGetExpDetailsLink(expName, tdb, tableName);
printf("<br><br>");
if(bedList == NULL)
    printf("<b>No CGH Data in this Range.</b>\n");
else 
    {
    erHash = newHash(2);
    erList = loadExpRecord(tableName, "hgFixed");
    for(er = erList; er != NULL; er=er->next)
	{
	snprintf(buff, sizeof(buff), "%d", er->id);
	hashAddUnique(erHash, buff, er);
	}
    msBedPrintTable(bedList, erHash, itemName, expName, -1 * maxScore,  maxScore, stepSize,
		    cghNci60PrintHeader, msBedCghPrintRow, printExprssnColorKey, getColorForCghBed);
    expRecordFreeList(&erList);
    hashFree(&erHash);
    bedFreeList(&bedList);
    }
}

struct sageExp *loadSageExps(char *tableName, struct pslWScore  *psList)
/* load the sage experiment data. */
{
char *user = cfgOption("db.user");
char *password = cfgOption("db.password");
struct sqlConnection *sc = sqlConnectRemote("localhost", user, password, "hgFixed");
char query[256];
struct sageExp *seList = NULL, *se=NULL;
char **row;
struct sqlResult *sr = NULL;
char *tmp= cloneString("select * from sageExp order by num");

sprintf(query,"%s",tmp);
sr = sqlGetResult(sc,query);
while((row = sqlNextRow(sr)) != NULL)
    {
    se = sageExpLoad(row);
    slAddHead(&seList,se);
    }
freez(&tmp);
sqlFreeResult(&sr);
sqlDisconnect(&sc);
slReverse(&seList);
return seList;
}

struct sage *loadSageData(char *table, struct pslWScore* pslList)
/* load the sage data by constructing a query based on the qNames of the pslList
 */
{
char *user = cfgOption("db.user");
char *password = cfgOption("db.password");
struct sqlConnection *sc = sqlConnectRemote("localhost", user, password, "hgFixed");
struct dyString *query = newDyString(2048);
struct sage *sgList = NULL, *sg=NULL;
struct pslWScore *psl=NULL;
char **row;
int count=0;
struct sqlResult *sr = NULL;
dyStringPrintf(query, "%s", "select * from sage where ");
for(psl=pslList;psl!=NULL;psl=psl->next)
    {
    if(count++) 
	{
	dyStringPrintf(query," or uni=%d ", atoi(psl->qName + 3 ));
	}
    else 
	{
	dyStringPrintf(query," uni=%d ", atoi(psl->qName + 3));
	}
    }
sr = sqlGetResult(sc,query->string);
while((row = sqlNextRow(sr)) != NULL)
    {
    sg = sageLoad(row);
    slAddHead(&sgList,sg);
    }
sqlFreeResult(&sr);
sqlDisconnect(&sc);
slReverse(&sgList);
freeDyString(&query);
return sgList;
}

int sagePslWSListIndex(struct pslWScore *pslList, int uni)
/* find the index of a psl by the unigene identifier in a psl list */
{
struct pslWScore *psl;
int count =0;
char buff[128];
sprintf(buff,"Hs.%d",uni);
for(psl = pslList; psl != NULL; psl = psl->next)
    {
    if(sameString(psl->qName,buff))
	return count;
    count++;
    }
errAbort("Didn't find the unigene tag %s",buff);
return 0;
}

int sortSageByPslOrder(const void *e1, const void *e2)
/* used by slSort to sort the sage experiment data using the order of the psls */
{
const struct sage *s1 = *((struct sage**)e1);
const struct sage *s2 = *((struct sage**)e2);
return(sagePslWSListIndex(sageExpList,s1->uni) - sagePslWSListIndex(sageExpList,s2->uni));
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
printf("\">here</a>");
printf(" to see the data as a graph.\n");
}

void printSageReference(struct sage *sgList)
{
puts(
     "<table width=600 cellpadding=0 cellspacing=0><tr><td><p><b>Description: S</b>erial <b>A</b>nalysis of <b>G</b>ene <b>E</b>xpression (SAGE)"
     "is a quantative measurement gene expression. Data is presented for every cluster contained "
     "in the browser window and the selected cluster name is highlighted in red. All data is from "
     "the repository at the <a href=\"http://www.ncbi.nlm.nih.gov/SAGE/\"> SageMap </a>"
     "project downloaded Jul 26, 2001. Selecting the UniGene cluster name will display SageMap's page for that cluster.");
printSageGraphUrl(sgList);
puts(
     "<p><b>Brief Methodology:</b> SAGE counts are produced "
     "by sequencing small \"tags\" of DNA believed to be associated with a "
     "gene. These tags are produced by attatching poly-A RNA to oligo-dT "
     "beads. After synthesis of double stranded cDNA transcripts are "
     "cleaved by an anchoring enzyme (usually NIaIII). Then small tags are "
     "produced by ligation with a linker containing a type IIS restriction "
     "enzyme site and cleavage with the tagging enzyme (usually BsmFI). The "
     "tags are then concatenated together and sequenced. The frequency of each "
     "tag is counted and used to infer expression level of transcripts that can "
     "be matched to that tag. All SAGE data presented here was mapped to UniGene "
     "transcripts by the <a href=\"http://www.ncbi.nlm.nih.gov/SAGE/\"> SageMap </a>"
     "project at <a href=\"http://www.ncbi.nlm.nih.gov\"> NCBI </a>.</td></tr></table><br><br>"
);
}

void sagePrintTable(struct pslWScore *pslList, char *itemName) 
/* load up the sage experiment data using psl->qNames and display it as a table */
{
struct sageExp *seList = NULL, *se =NULL;
struct sage *sgList=NULL, *sg=NULL;
int featureCount;
int count=0;
seList=loadSageExps("sageExp",pslList);
sgList = loadSageData("sage", pslList);
slSort(&sgList,sortSageByPslOrder);

printSageReference(sgList);
printSageGraphUrl(sgList);
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


struct pslWScore *pslWScoreLoadByChrom(char *table, char *chrom, int start, int end)
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
struct pslWScore *pslWS, *pslWSList = NULL;
char **row;
int rowOffset;
sr = hRangeQuery(conn,table,seqName,winStart,winEnd,NULL, &rowOffset);
while((row = sqlNextRow(sr)) != NULL)
    {
    pslWS = pslWScoreLoad(row);
    slAddHead(&pslWSList, pslWS);
    }
slReverse(&pslWSList);
sqlFreeResult(&sr);
hFreeConn(&conn);
return pslWSList;
}


void doSageDataDisp(char *tableName, char *itemName)
{
struct pslWScore *sgList = NULL;
char buff[64];
char *s=NULL;
int sgCount=0;
chuckHtmlStart("Sage Data Requested");
printf("<h2>Sage Data for: %s %d-%d</h2>\n", seqName, winStart, winEnd);
puts("<table cellpadding=0 cellspacing=0><tr><td>\n");

sgList = pslWScoreLoadByChrom("uniGene", seqName, winStart, winEnd);

sgCount = slCount(sgList);
if(sgCount > 50)
    printf("<hr><p>That will create too big of a table, try creating a window with less than 50 elements.<hr>\n");
else 
    {
    sageExpList = sgList;
    sagePrintTable(sgList, itemName);
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

void makeGrayShades(struct memGfx *mg)
/* Make eight shades of gray in display. */
{
int i;
for (i=0; i<=maxShade; ++i)
    {
    struct rgbColor rgb;
    int level = 255 - (255*i/maxShade);
    if (level < 0) level = 0;
    rgb.r = rgb.g = rgb.b = level;
    shadesOfGray[i] = mgFindColor(mg, rgb.r, rgb.g, rgb.b);
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



boolean altGraphXEdgeSeen(struct altGraphX *ag, int *seen, int *seenCount, int mrnaIx)
/* is the mrnaIx already in seen? */
{
int i=0;
boolean result = FALSE;
for(i=0; i<*seenCount; i++)
    {
    if(ag->mrnaTissues[seen[i]] == ag->mrnaTissues[mrnaIx] ||
       ag->mrnaLibs[seen[i]] == ag->mrnaLibs[mrnaIx])
	{
	result = TRUE;
	break;
	}
    }
if(!result)
    {
    seen[*seenCount++] = mrnaIx;
    }
return result;
}

int altGraphConfidenceForEdge(struct altGraphX *ag, int eIx)
/* count how many unique libraries or tissues contain a given edge */
{
struct evidence *ev = slElementFromIx(ag->evidence, eIx);
int *seen = NULL;
int seenCount = 0,i;
int conf = 0;
AllocArray(seen, ag->edgeCount);
for(i=0; i<ag->edgeCount; i++)
    seen[i] = -1;
for(i=0; i<ev->evCount; i++)
    if(!altGraphXEdgeSeen(ag, seen, &seenCount, ev->mrnaIds[i]))
	conf++;
freez(&seen);
return conf;
}

boolean altGraphXInEdges(struct ggEdge *edges, int v1, int v2)
/* Return TRUE if a v1-v2 edge is in the list FALSE otherwise. */
{
struct ggEdge *e = NULL;
for(e = edges; e != NULL; e = e->next)
    {
    if(e->vertex1 == v1 && e->vertex2 == v2)
	return TRUE;
    }
return FALSE;
}

Color altGraphXColorForEdge(struct memGfx *mg, struct altGraphX *ag, int eIx)
/* Return the color of an edge given by confidence */
{
int confidence = altGraphConfidenceForEdge(ag, eIx);
Color c = shadesOfGray[maxShade/4];
struct geneGraph *gg = NULL;
struct ggEdge *edges = NULL;

if(ag->vTypes[ag->edgeStarts[eIx]] == ggHardStart && ag->vTypes[ag->edgeEnds[eIx]] == ggHardEnd)
    {
    gg = altGraphXToGG(ag);
    edges = ggFindCassetteExons(gg);
    if(altGraphXInEdges(edges, ag->edgeStarts[eIx], ag->edgeEnds[eIx]))
	{
	if(!exprBedColorsMade)
	    makeRedGreenShades(mg);
	if(confidence == 1) c = shadesOfRed[(maxRGBShade - 6 > 0) ? maxRGBShade - 6 : 0];
	else if(confidence == 2) c = shadesOfRed[(maxRGBShade - 4 > 0) ? maxRGBShade - 4: 0];
	else if(confidence >= 3) c = shadesOfRed[maxRGBShade];
	}
    else 
	{
	if(confidence == 1) c = shadesOfGray[maxShade/3];
	else if(confidence == 2) c = shadesOfGray[2*maxShade/3];
	else if(confidence >= 3) c = shadesOfGray[maxShade];
	}    
    freeGeneGraph(&gg);
    slFreeList(&edges);
    return c;
    }
else
    {
    if(confidence == 1) c = shadesOfGray[maxShade/3];
    else if(confidence == 2) c = shadesOfGray[2*maxShade/3];
    else if(confidence >= 3) c = shadesOfGray[maxShade];
    }
return c;
}


static void altGraphXDraw(struct altGraphX *ag, struct memGfx *mg, int xOff, int yOff, 
			  int width,  MgFont *font)
/* Draws the blocks for an alt-spliced gene and the connections */
{
int start = ag->tStart;
int end = ag->tEnd;
int baseWidth = ag->tEnd - ag->tStart;
int y = yOff;
int heightPer = 2 * mgFontLineHeight(font);
int lineHeight = mgFontLineHeight(font);
int x1,x2;
double scale = width/(double)baseWidth;
int i;
double y1, y2;
int midLineOff = heightPer/2;
int s =0, e=0;

for(i= ag->edgeCount -1; i >= 0; i--)   // for(i=0; i< ag->edgeCount; i++)
    {
    char buff[16];
    int textWidth;
    int sx1 = 0;
    int sx2 = 0;
    int sw = 0;
    s = ag->vPositions[ag->edgeStarts[i]];
    e = ag->vPositions[ag->edgeEnds[i]];
    x1 = round((double)((int)s-start)*scale) + xOff;
    x2 = round((double)((int)e-start)*scale) + xOff;
    sx1 = roundingScale(s-start, width, baseWidth)+xOff;
    sx2 = roundingScale(e-start, width, baseWidth)+xOff;
    sw = sx2 - sx1;
    snprintf(buff, sizeof(buff), "%d-%d", ag->edgeStarts[i], ag->edgeEnds[i]); 
    /* if it is an exon draw a box */
    if( (ag->vTypes[ag->edgeStarts[i]] == ggHardStart || ag->vTypes[ag->edgeStarts[i]] == ggSoftStart)  
	&& (ag->vTypes[ag->edgeEnds[i]] == ggHardEnd || ag->vTypes[ag->edgeEnds[i]] == ggSoftEnd)) 
	{
	Color color2 = altGraphXColorForEdge(mg, ag, i);
	mgDrawBox(mg, x1, y+(heightPer/2), (x2-x1), heightPer/2, color2);
	textWidth = mgFontStringWidth(font, buff);
	if (textWidth <= sw + 2 )
	    mgTextCentered(mg, sx2-textWidth-2, y+(heightPer/2), textWidth+2, heightPer/2, MG_WHITE, font, buff);
	}
    /* if it is an intron draw an arc */
    if( (ag->vTypes[ag->edgeStarts[i]] == ggHardEnd || ag->vTypes[ag->edgeStarts[i]] == ggSoftEnd) 
	&& (ag->vTypes[ag->edgeEnds[i]] == ggHardStart || ag->vTypes[ag->edgeEnds[i]] == ggSoftStart))
	{
	Color color2 = altGraphXColorForEdge(mg, ag, i);
	int midX;   
	int midY = y + heightPer/2;
	midX = (x1+x2)/2;
	mgDrawLine(mg, x1, midY, midX, y, color2);
	mgDrawLine(mg, midX, y, x2, midY, color2);
	textWidth = mgFontStringWidth(font, buff);
	if (textWidth <= sw )
	    mgTextCentered(mg, sx1, y+(heightPer/2), sw, heightPer/2, MG_BLACK, font, buff);
	}
    }
}

char *altGraphXMakeImage(struct trackDb *tdb, struct altGraphX *ag)
/* create a drawing of splicing pattern */
{
struct memGfx *mg;
MgFont *font = mgSmallFont();
int trackTabWidth = 11;
int fontHeight = mgFontLineHeight(font);
struct tempName gifTn;
int pixWidth = atoi(cartUsualString(cart, "pix", "600" ));
int pixHeight = 2 * mgFontLineHeight(font)+2;
mg = mgNew(pixWidth, pixHeight);
mgClearPixels(mg);
makeGrayShades(mg);
altGraphXDraw(ag, mg, 0, 1, pixWidth, font);
makeTempName(&gifTn, "hgc", ".gif");
mgSaveGif(mg, gifTn.forCgi);
printf(
    "<IMG SRC = \"%s\" BORDER=1 WIDTH=%d HEIGHT=%d><BR>\n",
    gifTn.forHtml, pixWidth, pixHeight);
mgFree(&mg);
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
struct sqlConnection *conn = hAllocConn();
char *image = NULL;
genericHeader(tdb, item);
snprintf(query, sizeof(query),"select * from %s where id=%d", tdb->tableName, id);
ag = altGraphXLoadByQuery(conn, query);
if(ag == NULL)
    errAbort("hgc::doAltGraphXDetails() - couldn't find altGraphX with id=%d", id);
printf("<center>\n");
image = altGraphXMakeImage(tdb,ag);
printf("<br><a HREF=\"%s?position=%s:%d-%d&mrna=full&intronEst=full&refGene=full&altGraphX=full&%s\"",
       hgTracksName(), ag->tName, ag->tStart, ag->tEnd, cartSidUrlString(cart));
printf(" ALT=\"Zoom to browser coordinates of altGraphX\">");
printf("Jump to browser for %d</a><font size=-1>[%s:%d-%d]</font><br><br>\n", ag->id, ag->tName, ag->tStart, ag->tEnd);
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



void hgCustom(char *trackId, char *fileItem)
/* Process click on custom track. */
{
char *fileName, *itemName;
struct customTrack *ctList, *ct;
struct customTrack *customTracksFromFile(char *text);
struct bed *bed;
int start = cartInt(cart, "o");
char *url;

cartWebStart("Custom Track");
fileName = nextWord(&fileItem);
itemName = skipLeadingSpaces(fileItem);
printf("<H2>Custom Track Item %s</H2>\n", itemName);
if (fileName == NULL || itemName == NULL)
    errAbort("Misformed fileItem in hgCustom");
ctList = customTracksFromFile(fileName);
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
bedPrintPos(bed);
}

struct hash *makeTrackHash(char *chrom)
/* Make hash of trackDb items for this chromosome. */
{
struct sqlConnection *conn = hAllocConn();
struct hash *trackHash = newHash(7);
struct sqlResult *sr;
char **row;
struct trackDb *tdb;

sr = sqlGetResult(conn, "select * from trackDb");
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

void doMiddle()
/* Generate body of HTML. */
{
char *track = cartString(cart, "g");
char *item = cartOptionalString(cart, "i");
char title[256];
struct hash *trackHash;
struct trackDb *tdb;

database = cartUsualString(cart, "db", hGetDb());

hDefaultConnect(); 	/* set up default connection settings */
hSetDb(database);
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
else if (sameWord(track, "mrna") || sameWord(track, "mrna2") || 
	sameWord(track, "est") || sameWord(track, "intronEst") || 
	sameWord(track, "xenoMrna") || sameWord(track, "xenoBestMrna") ||
	sameWord(track, "xenoEst") || sameWord(track, "psu") ||
	sameWord(track, "tightMrna") || sameWord(track, "tightEst") ||
	sameWord(track, "mgcNcbiPicks") ||
        sameWord(track, "mgcNcbiSplicedPicks") ||
	sameWord(track, "mgcUcscPicks"))
    {
    doHgRna(tdb, item);
    }
#ifdef ROGIC_CODE
else if (sameWord(track, "estPair"))
    {
    doHgEstPair(track, item);
    }
#endif /*ROGIC_CODE*/
else if (sameWord(track, "ctgPos"))
    {
    doHgContig(tdb, item);
    }
else if (sameWord(track, "clonePos"))
    {
    doHgCover(tdb, item);
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
else if (sameWord(track, "rmsk"))
    {
    doHgRepeat(tdb, item);
    }
else if (sameWord(track, "isochores"))
    {
    doHgIsochore(tdb, item);
    }
else if (sameWord(track, "simpleRepeat"))
    {
    doSimpleRepeat(tdb, item);
    }
else if (sameWord(track, "cpgIsland") || sameWord(track, "cpgIsland2"))
    {
    doCpgIsland(tdb, item);
    }
else if (sameWord(track, "refGene"))
    {
    doRefGene(tdb, item);
    }
else if (sameWord(track, "genieKnown"))
    {
    doKnownGene(tdb, item);
    }
else if (sameWord(track, "softberryGene"))
    {
    doSoftberryPred(tdb, item);
    }
else if (sameWord(track, "sanger22"))
    {
    doSangerGene(tdb, item, "sanger22pep", "sanger22mrna", "sanger22extra");
    }
else if (sameWord(track, "sanger20"))
    {
    doSangerGene(tdb, item, "sanger20pep", "sanger20mrna", "sanger20extra");
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
else if (sameWord(track, "blatMus"))
    {
    doBlatMus(tdb, item);
    }
else if (sameWord(track, "blatHuman"))
    {
    doBlatHuman(tdb, item);
    }
else if (sameWord(track, "htcLongXenoPsl2"))
    {
    htcLongXenoPsl2(track, item);
    }
else if (sameWord(track, "blatFish"))
    {
    doBlatFish(tdb, item);
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
else if (sameWord(track, "stsMap"))
    {
    doStsMarker(tdb, item);
    }
else if (sameWord(track, "uPennClones"))
    {
    doUPennClones(tdb, item);
    }
else if (sameWord(track, "mouseSyn"))
    {
    doMouseSyn(track, item);
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
else if (sameWord(track, "uniGene"))
    {
    doSageDataDisp(track, item);
    }
else if (sameWord(track, "tigrGeneIndex"))
    {
    doTigrGeneIndex(tdb, item);
    }
#ifdef ROGIC_CODE
 else if (sameWord(track, "mgc_mrna"))
   {
     doMgcMrna(track, item);
   }
#endif /*ROGIC_CODE*/
#ifdef FUREY_CODE
 else if (sameWord(track, "bacEndPairs"))
   {
     doLinkedFeaturesSeries(track, item, tdb);
   }
 else if (sameWord(track, "uPennBacEndPairs"))
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
#endif /*FUREY_CODE*/
else if (sameWord(track, "htcCloneSeq"))
    {
    htcCloneSeq(item);
    }
else if (sameWord(track, "htcCdnaAli"))
   {
   htcCdnaAli(item);
   }
else if (sameWord(track, "htcUserAli"))
   {
   htcUserAli(item);
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
else if (sameWord(track, "cghNci60"))
    {
    cghNci60Details(tdb, item);
    }
else if (sameWord(track, "nci60"))
    {
    nci60Details(tdb, item);
    }
else if (sameWord(track, "perlegen"))
    {
    perlegenDetails(tdb, item);
    }
else if(sameWord(track, "rosetta"))
    {
    rosettaDetails(tdb, item);
    }
else if(sameWord(track, "affy"))
    {
    affyDetails(tdb, item);
    }
else if(sameWord(track, "affyRatio"))
    {
    affyRatioDetails(tdb, item);
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
else if( sameWord(track, "altGraphX"))
    {
    doAltGraphXDetails(tdb,item);
    }
else if (sameWord(track, "triangle"))
    {
    doTriangle(tdb, item);
    }
else if (tdb != NULL)
   {
   genericClickHandler(tdb, item, NULL);
   }
else
   {
   cartWebStart(track);
   printf("Sorry, clicking there doesn't do anything yet (%s).", track);
   }
cartHtmlEnd();
}

void cartDoMiddle(struct cart *theCart)
/* Save cart and do main middle handler. */
{
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
