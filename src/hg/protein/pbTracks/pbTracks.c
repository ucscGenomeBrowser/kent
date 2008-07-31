/* pbTracks.c - UCSC Proteome Browser main cgi script. */
#include "common.h"
#include "hCommon.h"
#include "portable.h"
#include "jksql.h"
#include "vGfx.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "cart.h"
#include "hdb.h"
#include "web.h"
#include "hgColors.h"
#include "hui.h"
#include "pbStamp.h"
#include "pbStampPict.h"
#include "pbTracks.h"
#include "trashDir.h"
#include "psGfx.h"

static char const rcsid[] = "$Id: pbTracks.c,v 1.51.50.1 2008/07/31 02:24:52 markd Exp $";

boolean hgDebug = FALSE;      /* Activate debugging code. Set to true by hgDebug=on in command line*/

boolean IAmPbTracks = TRUE;

int kgVersion = KG_UNKNOWN;

boolean proteinInSupportedGenome=TRUE;  /* The protein is in supported genome DB */
                                        /* This is a new variable necessary for PB V1.0
					   and PB V1.1 sharing the same PB library */

struct cart *cart;	/* The cart where we keep persistent variables. */
struct hash *oldVars = NULL; /* The cart vars before new cgi stuff added. */

/* These variables persist from one incarnation of this program to the
 * next - living mostly in the cart. */
char *database;			/* Name of database we're using. */
char *organism;			/* Name of organism we're working on. */
char *hgsid;
char hgsidStr[50];

int gfxBorder = 1;		/* Width of graphics border. */
int insideWidth;		/* Width of area to draw tracks in in pixels. */

char *protDbName;               /* Name of proteome database for this genome. */

struct tempName gifTn, gifTn2;  /* gifTn for tracks image and gifTn2 for stamps image */
boolean hideControls = FALSE;   /* Hide all controls? */
boolean suppressHtml = FALSE;	/* If doing PostScript output we'll suppress most
         			 * of HTML output. */
char *proteinID;
char *protDisplayID;
char *mrnaID;
char *description;

char *positionStr;

char *prevGBChrom;		/* chrom          previously chosen by Genome Browser */
int  prevGBStartPos;		/* start position previously chosen by Genome Browser */
int  prevGBEndPos;		/* end   position previously chosen by Genome Browser */
int  prevExonStartPos;		/* start position chosen by Genome Browser, converted to exon pos */
int  prevExonEndPos;		/* end   position chosen by Genome Browser, converted to exon pos */

char *ensPepName;		/* Ensembl peptide name, used for Superfamily track */
int sfCount;			/* count of Superfamily domains */

struct sqlConnection *spConn;   /* Connection to SwissProt database. */

char aaAlphabet[30] = {"WCMHYNFIDQKRTVPGEASLXZB"};
char *protSeq;
int  protSeqLen;

char aaChar[20];
float aa_hydro[255];
int aa_attrib[255];
char aa[100000];

struct vGfx *g_vg;
MgFont *g_font;

Color pbRed, pbBlue;

int  currentYoffset;
int  pbScale = {6};
char pbScaleStr[5];

boolean scaleButtonPushed;
double tx[100000], ty[100000];

double xScale = 50.0;
double yScale = 100.0;

struct pbStampPict *stampPictPtr;

double avg[20];
double stddev[20];

int exStart[500], exEnd[500];
int exCount;
int aaStart[500], aaEnd[500];

char kgProtMapTableName[20] = {"kgProtMap"}; 
int blockSize[500], blockSizePositive[500];
int blockStart[500], blockStartPositive[500];
int blockEnd[500],   blockEndPositive[500];
int blockGenomeStart[500], blockGenomeStartPositive[500];
int blockGenomeEnd[500], blockGenomeEndPositive[500];

int trackOrigOffset = 0;	/* current track display origin offset */
int aaOrigOffset = 0;		/* current track AA base origin offset */
boolean initialWindow = TRUE;
struct vGfx *vg, *vg2;
Color bkgColor;

Color abnormalColor;
Color normalColor;

void dvPrintf(char *format, va_list args)
/* Suppressable variable args hPrintf. */
{
char temp[1000];
safef(temp, sizeof(temp), "<br>%s", format);

vprintf(temp, args);
fflush(stdout);
}

/* used for debugging only */
/* #include "dPrint.c" */

void hvPrintf(char *format, va_list args)
/* Suppressable variable args hPrintf. */
{
if (!suppressHtml)
    vprintf(format, args);
}

void hPrintf(char *format, ...)
/* Printf that can be suppressed if not making
 * html. */
{
va_list(args);
va_start(args, format);
hvPrintf(format, args);
va_end(args);
}

void smallBreak()
/* Draw small horizontal break */
{
hPrintf("<FONT SIZE=1><BR></FONT>\n");
}

void makeActiveImagePB(char *psOutput, char *psOutput2)
/* Make image and image map. */
{
char *mapName = "map";
int pixWidth, pixHeight;

struct sqlConnection *conn;
int  iypos;
char *spDisplayId;

hPrintf("<br><font size=4>");
hPrintf("%s protein: ", organism);
hPrintf("<A HREF=\"http://www.expasy.org/cgi-bin/niceprot.pl?%s\" TARGET=_blank><B>%s</B></A>\n", 
	proteinID, proteinID);

/* show SWISS-PROT display ID if it is different than the accession ID */
/* but, if display name is like: Q03399 | Q03399_HUMAN, then don't show display name */
spDisplayId = spAccToId(spConn, proteinID);
if (strstr(spDisplayId, proteinID) == NULL)
   {
   hPrintf(" (aka %s", spDisplayId);
   /* don't show 2nd aka if the new and old displayId are the same */
   if (oldSpDisplayId(spDisplayId) != NULL)
	{
   	if (!sameWord(spDisplayId, oldSpDisplayId(spDisplayId)))
	    {
	    hPrintf(" or %s", oldSpDisplayId(spDisplayId));
	    }
	}
   hPrintf(") ", oldSpDisplayId(spDisplayId));
   }
hPrintf(" %s\n", description);
hPrintf("</font><br><br>");

protSeq = getAA(proteinID);
if (protSeq == NULL)
    {
    errAbort("%s is not a current valid entry in SWISS-PROT/TrEMBL\n", proteinID);
    }
protSeqLen = strlen(protSeq);

iypos = 15; 
doTracks(proteinID, mrnaID, protSeq, &iypos, psOutput);
if (!hTableExists(database, "pbStamp")) goto histDone; 

pbScale = 3;
pixWidth = 765;
insideWidth = pixWidth-gfxBorder;

pixHeight = 350;

if (psOutput2)
    {
    vg2 = vgOpenPostScript(pixWidth, pixHeight, psOutput2);
    }
else
    {
    trashDirFile(&gifTn2, "pbt", "pbt", ".gif");
    vg2 = vgOpenGif(pixWidth, pixHeight, gifTn2.forCgi);
    }

g_vg = vg2;

pbRed    = vgFindColorIx(vg2, 0xf9, 0x51, 0x59);
pbBlue   = vgFindColorIx(g_vg, 0x00, 0x00, 0xd0);

normalColor   = pbBlue;
abnormalColor = pbRed;

bkgColor = vgFindColorIx(vg2, 255, 254, 232);
vgBox(vg2, 0, 0, insideWidth, pixHeight, bkgColor);

/* Start up client side map. */
mapName=cloneString("pbStamps");
hPrintf("\n<MAP Name=%s>\n", mapName);

vgSetClip(vg2, 0, gfxBorder, insideWidth, pixHeight - 2*gfxBorder);
iypos = 15;

/* Draw stamps. */
doStamps(proteinID, mrnaID, protSeq, vg2, &iypos);

/* Finish map. */
hPrintf("</MAP>\n");

/* Save out picture and tell html file about it. */
vgClose(&vg2);
hPrintf("<P>");

hPrintf("\n<IMG SRC=\"%s\" BORDER=1 WIDTH=%d HEIGHT=%d USEMAP=#%s><BR>",
            gifTn2.forCgi, pixWidth, pixHeight, mapName);
hPrintf("\n<A HREF=\"../goldenPath/help/pbTracksHelpFiles/pbTracksHelp.shtml#histograms\" TARGET=_blank>");
hPrintf("Explanation of Protein Property Histograms</A><BR>");

hPrintf("<P>");

histDone:

hPrintf("\n<B>UCSC Links:</B><BR>\n ");
hPrintf("<UL>\n");

doGenomeBrowserLink(proteinID, mrnaID, hgsidStr);
doGeneDetailsLink(proteinID, mrnaID, hgsidStr);

/* show Gene Sorter link only if it is valid for this genome */
if (hgNearOk(database))
    {
    doGeneSorterLink(protDisplayID, mrnaID, hgsidStr);
    }

hPrintf("</UL><P>");

conn = sqlConnect(UNIPROT_DB_NAME);
domainsPrint(conn, proteinID);

hPrintf("<P>");
doPathwayLinks(protDisplayID, mrnaID); 

printFASTA(proteinID, protSeq);
}

void doTrackForm(char *psOutput, char *psOutput2)
/* Make the tracks display form */
{
if (psOutput != NULL)
    {
    suppressHtml = TRUE;
    hideControls = TRUE;
    }

/* Tell browser where to go when they click on image. */
hPrintf("<FORM ACTION=\"%s\" NAME=\"TrackForm\" METHOD=POST>\n\n", "../cgi-bin/pbTracks");
cartSaveSession(cart);

hPrintf("<TABLE WIDTH=\"100%%\" BGCOLOR=\"#"HG_COL_HOTLINKS"\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"2\"><TR>\n");
hPrintf("<TD ALIGN=LEFT><A HREF=\"../index.html\">%s</A></TD>", wrapWhiteFont("Home"));
hPrintf("<TD ALIGN=CENTER><FONT COLOR=\"#FFFFFF\" SIZE=4>%s</FONT></TD>",
        "UCSC Proteome Browser");
hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/pbTracks?%s=%u&pbt.psOutput=on\">%s</A></TD>\n",
        cartSessionVarName(), cartSessionId(cart), wrapWhiteFont("PDF/PS"));
hPrintf("<TD ALIGN=Right><A HREF=\"../goldenPath/help/pbTracksHelpFiles/pbTracksHelp.shtml\" TARGET=_blank>%s</A></TD>",
        wrapWhiteFont("Help"));
hPrintf("</TR></TABLE>");
fflush(stdout);

/* Make clickable image and map. */
makeActiveImagePB(psOutput, psOutput2);

if (psOutput == NULL) hPrintf("</FORM>");
}

void handlePostscript()
/* Deal with Postscript output. */
{
struct tempName psTn;
struct tempName psTn2;
char *pdfFile = NULL;

trashDirFile(&psTn, "pbt", "pbt", ".eps");
trashDirFile(&psTn2, "pbt", "pbt2", ".eps");

printf("<H1>PostScript/PDF Output</H1>\n");
printf("PostScript images can be printed at high resolution "
       "and edited by many drawing programs such as Adobe Illustrator.");
printf("  PDF can be viewed with Adobe Acrobat Reader.<BR><BR>\n");

doTrackForm(psTn.forCgi, psTn2.forCgi);

printf("<A HREF=\"%s\">Click here to download</A> "
       "the current protein tracks graphic in PostScript.  ", psTn.forCgi);
pdfFile = convertEpsToPdf(psTn.forCgi);
if(pdfFile != NULL) 
    {
    printf("<BR><A HREF=\"%s\">Click here to download</A> "
	   "the current protein tracks graphic in PDF", pdfFile);
    }
else
    printf("<BR><BR>PDF format not available");

printf("<BR><BR><A HREF=\"%s\">Click here to download</A> "
       "the current protein histograms graphic in PostScript.  ", psTn2.forCgi);
pdfFile = convertEpsToPdf(psTn2.forCgi);
if(pdfFile != NULL) 
    {
    printf("<BR><A HREF=\"%s\">Click here to download</A> "
	   "the current protein histograms graphic in PDF", pdfFile);
    }
else
    printf("<BR><BR>PDF format not available");

freez(&pdfFile);
}

void doMiddle(struct cart *theCart)
/* Print the body of an html file.   */
{
char cond_str[255];
struct sqlConnection *conn;
char *proteinAC;
char *chp, *chp1, *chp9;
char *debugTmp = NULL;
char *answer;

/* Initialize layout and database. */
cart = theCart;

/* Uncomment this to see parameters for debugging. */
/* Be careful though, it breaks if custom track is more than 4k */
/*
{ struct dyString *state = cgiUrlString(); 
  hPrintf("State: %s\n", state->string); }
*/

getDbAndGenome(cart, &database, &organism, oldVars);

/* if kgProtMap2 table exists, this means we are doing KG III */
if (hTableExists(database, "kgProtMap2")) 
    {
    kgVersion = KG_III;
    strcpy(kgProtMapTableName, "kgProtMap2");
    }

protDbName = hPdbFromGdb(database);
spConn = sqlConnect(UNIPROT_DB_NAME);
debugTmp = cartUsualString(cart, "hgDebug", "off");
if(sameString(debugTmp, "on"))
    hgDebug = TRUE;
else
    hgDebug = FALSE;

hDefaultConnect();
conn  = hAllocConn(database);

hgsid     = cartOptionalString(cart, "hgsid");
if (hgsid != NULL)
    {
    safef(hgsidStr, sizeof(hgsidStr), "&hgsid=%s", hgsid);
    }
else
    {
    strcpy(hgsidStr, "");
    }

proteinID = cartOptionalString(cart, "proteinID");

/* check proteinID to see if it is a valid SWISS-PROT/TrEMBL accession or display ID */
/* then assign the accession number to global variable proteinID */
safef(cond_str, sizeof(cond_str), "accession='%s'", proteinID);
proteinAC = sqlGetField(protDbName, "spXref3", "accession", cond_str);
if (proteinAC == NULL)
    {
    safef(cond_str, sizeof(cond_str), "displayID='%s'", proteinID);
    proteinAC = sqlGetField(protDbName, "spXref3", "accession", cond_str);
    if (proteinAC == NULL)
	{
	errAbort("'%s' does not seem to be a valid SWISS-PROT/TrEMBL protein ID.", proteinID);
	}
    else
	{
	protDisplayID = proteinID;
	proteinID = proteinAC;
	}
    }
else
    {
    safef(cond_str, sizeof(cond_str), "accession='%s'", proteinID);
    protDisplayID = sqlGetField(protDbName, "spXref3", "displayID", cond_str);
    }

if (spFindAcc(spConn, proteinID) == NULL)
    {
    safef(cond_str, sizeof(cond_str), "accession='%s'", proteinID);
    answer = sqlGetField(protDbName, "spXref3", "biodatabaseID", cond_str);
    if (sameWord(answer, "3"))
        {
        errAbort("The corresponding protein %s is no longer available from Swiss-Prot/TrEMBL.", 
		 proteinID);
        }
    else
        {
        errAbort("%s seems not to be a valid protein ID.", proteinID);
        }
    } 
else
   {
   if (!sameWord(proteinID, spFindAcc(spConn, proteinID)))
    {
    hPrintf("%s seems to be an outdated protein ID.", proteinID);
    printf("<BR><BR>Please try ");
    hPrintf("<A HREF=\"../cgi-bin/pbGlobal?proteinID=%s\">  %s</A> instead.\n",
	     spFindAcc(spConn, proteinID), spFindAcc(spConn, proteinID));fflush(stdout);
    errAbort(" ");
    }
  }
if (kgVersion == KG_III)
    {
    safef(cond_str, sizeof(cond_str), "spID='%s'", proteinID);
    mrnaID = sqlGetField(database, "kgXref", "kgId", cond_str);
    }
else
    {
    safef(cond_str, sizeof(cond_str), "proteinID='%s'", protDisplayID);
    mrnaID = sqlGetField(database, "knownGene", "name", cond_str);
    }

safef(cond_str, sizeof(cond_str), "accession='%s'", proteinID);
description = sqlGetField(protDbName, "spXref3", "description", cond_str);

/* obtain previous genome position range selected by the Genome Browser user */
positionStr = cloneString(cartOptionalString(cart, "position"));
if (positionStr != NULL)
    {
    chp = strstr(positionStr, ":");
    *chp = '\0';
    prevGBChrom = cloneString(positionStr);
    
    chp1 = chp + 1;
    chp9 = strstr(chp1, "-");
    *chp9 = '\0';
    prevGBStartPos = atoi(chp1);
    chp1 = chp9 + 1;
    prevGBEndPos   = atoi(chp1);
    }
else
    {
    prevGBChrom    = NULL;
    prevGBStartPos = -1;
    prevGBEndPos   = -1;
    }

/* Do main display. */
if (cgiVarExists("pbt.psOutput"))
    handlePostscript();
else
    doTrackForm(NULL, NULL);
}

void doDown(struct cart *cart)
{
hPrintf("<H2>The Browser is Being Updated</H2>\n");
hPrintf("The browser is currently unavailable.  We are in the process of\n");
hPrintf("updating the database and the display software.\n");
hPrintf("Please try again later.\n");
}

/* Other than submit and Submit all these vars should start with pbt.
 * to avoid weeding things out of other program's namespaces.
 * Because the browser is a central program, most of it's cart 
 * variables are not pbt. qualified.  It's a good idea if other
 * program's unique variables be qualified with a prefix though. */
char *excludeVars[] = { "submit", "Submit", "pbt.reset",
			"pbt.in1", "pbt.in2", "pbt.in3", "pbt.inBase",
			"pbt.out1", "pbt.out2", "pbt.out3",
			"pbt.left1", "pbt.left2", "pbt.left3", 
			"pbt.right1", "pbt.right2", "pbt.right3", 
			"pbt.dinkLL", "pbt.dinkLR", "pbt.dinkRL", "pbt.dinkRR",
			"pbt.tui", "pbt.hideAll", "pbt.psOutput", "hideControls",
			NULL };

void resetVars()
/* Reset vars except for position and database. */
{
static char *except[] = {"db", "position", NULL};
char *cookieName = hUserCookie();
int sessionId = cgiUsualInt(cartSessionVarName(), 0);
char *hguidString = findCookieData(cookieName);
int userId = (hguidString == NULL ? 0 : atoi(hguidString));
struct cart *oldCart = cartNew(userId, sessionId, NULL, NULL);
cartRemoveExcept(oldCart, except);
cartCheckout(&oldCart);
cgiVarExcludeExcept(except);
}

int main(int argc, char *argv[])
{
cgiSpoof(&argc, argv);
oldVars = hashNew(10);
htmlSetBackground(hBackgroundImage());
if (cgiVarExists("pbt.reset"))
    resetVars();
cartHtmlShellPB("UCSC Proteome Browser", doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
