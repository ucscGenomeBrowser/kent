/* pbTracks - UCSC Proteome Browser main cgi script. */
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

//static char const rcsid[] = "$Id: pbTracks.c,v 1.1 2003/11/27 01:31:24 fanhsu Exp $";

boolean hgDebug = FALSE;      /* Activate debugging code. Set to true by hgDebug=on in command line*/

struct cart *cart;	/* The cart where we keep persistent variables. */

/* These variables persist from one incarnation of this program to the
 * next - living mostly in the cart. */
char *database;			/* Name of database we're using. */
char *organism;			/* Name of organism we're working on. */

int gfxBorder = 1;		/* Width of graphics border. */
int insideWidth;		/* Width of area to draw tracks in in pixels. */

char *protDbName;               /* Name of proteome database for this genome. */
boolean suppressHtml = FALSE;	/* If doing PostScript output we'll suppress most
         			 * of HTML output. */
char *proteinID;
char *protDisplayID;
char *mrnaID;
char *description;

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
int currentYoffset;
int pbScale = {1};

double tx[100000], ty[100000];

double xScale = 50.0;
double yScale = 100.0;

struct pbStampPict *stampPictPtr;

double avg[20];
double stddev[20];

int exStart[500], exEnd[500];
int exCount;
int aaStart[500], aaEnd[500];

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

char * convertEpsToPdf(char *epsFile)
/* Convert EPS to PDF and return filename, or NULL if failure. */
{
char *pdfTmpName = NULL, *pdfName=NULL;
char cmdBuffer[2048];
int sysVal = 0;
struct lineFile *lf = NULL;
char *line;
int lineSize=0;
float width=0, height=0;
pdfTmpName = cloneString(epsFile);

/* Get the dimensions of bounding box. */
lf = lineFileOpen(epsFile, TRUE);
while(lineFileNext(lf, &line, &lineSize))
    {
    if(strstr( line, "BoundingBox:"))
        {
        char *words[5];
        chopLine(line, words);
        width = atof(words[3]);
        height = atof(words[4]);
        break;
        }
    }
lineFileClose(&lf);

/* Do conversion. */
chopSuffix(pdfTmpName);
pdfName = addSuffix(pdfTmpName, ".pdf");
safef(cmdBuffer, sizeof(cmdBuffer), "ps2pdf -dDEVICEWIDTHPOINTS=%d -dDEVICEHEIGHTPOINTS=%d %s %s",
      round(width), round(height), epsFile, pdfName);
sysVal = system(cmdBuffer);
if(sysVal != 0)
    freez(&pdfName);
freez(&pdfTmpName);
return pdfName;
}

void makeActiveImagePB(char *psOutput)
/* Make image and image map. */
{
struct vGfx *vg;
struct tempName gifTn;
char *mapName = "map";
int pixWidth, pixHeight;

Color bkgColor;

char cond_str[255];
struct sqlConnection *conn   = hAllocConn();
char query[256];
struct sqlResult *sr;
char **row;
char *chp;
int  i,l;
int  ii = 0;
int  iypos;

hPrintf("<CENTER>");
hPrintf("<br><font size=4>");
hPrintf("<A HREF=\"http://www.expasy.org/cgi-bin/niceprot.pl?%s\"><B>%s</B></A>\n", 
	proteinID, proteinID);
if (strcmp(proteinID, protDisplayID) != 0)hPrintf(" (aka %s)", protDisplayID);

hPrintf("<br>%s\n", description);
hPrintf("</font>");
fflush(stdout);

protSeq = strdup(getAA(proteinID));
protSeqLen = strlen(protSeq);
 
if (cgiOptionalString("pbScale") != NULL)
	{
	if (strcmp(cgiOptionalString("pbScale"), "3X") == 0) pbScale = 3;
	if (strcmp(cgiOptionalString("pbScale"), "6X") == 0) pbScale = 6;
	}

pixWidth = 160+ protSeqLen*pbScale;
if (pixWidth < 550) pixWidth = 550;
insideWidth = pixWidth-gfxBorder;

pixHeight = 420;
//make room for individual residues display
if (pbScale >=6) pixHeight = pixHeight + 20;

makeTempName(&gifTn, "hgt", ".gif");
vg = vgOpenGif(pixWidth, pixHeight, gifTn.forCgi);

bkgColor = vgFindColorIx(vg, 255, 254, 232);
vgBox(vg, 0, 0, insideWidth, pixHeight, bkgColor);

/* Start up client side map. */
hPrintf("<MAP Name=%s>\n", mapName);

vgSetClip(vg, 0, gfxBorder, insideWidth, pixHeight - 2*gfxBorder);

iypos = 15;
/* Draw tracks. */
doTracks(proteinID, mrnaID, protSeq, vg, &iypos);

/* Draw stamps. */
doStamps(proteinID, mrnaID, protSeq, vg, &iypos);

/* Finish map. */
hPrintf("</MAP>\n");

/* Save out picture and tell html file about it. */

vgClose(&vg);

//smallBreak();
hPrintf("<P>");
//smallBreak();

if (cgiOptionalString("exonNum") == NULL)
    {
    hPrintf("<IMG SRC=\"%s\" BORDER=1 WIDTH=%d HEIGHT=%d USEMAP=#%s onMouseOut=\"javascript:popupoff();\"><BR>",
            gifTn.forCgi, pixWidth, pixHeight, mapName);

    hPrintf("<br>Current scale: %dX ", pbScale);
    hPrintf("&nbsp&nbsp&nbsp Rescale to ");
    hPrintf("<INPUT TYPE=SUBMIT NAME=\"pbScale\" VALUE=\"1X\">\n");
    hPrintf("<INPUT TYPE=SUBMIT NAME=\"pbScale\" VALUE=\"3X\">\n");
    hPrintf("<INPUT TYPE=SUBMIT NAME=\"pbScale\" VALUE=\"6X\">\n");
    }
hPrintf("</CENTER><P>");
//spConn = sqlConnect("swissProt");

domainsPrint(conn, proteinID);

if (cgiOptionalString("exonNum") == NULL)printExonAA(proteinID, protSeq, -1);
doGenomeBrowserLink(proteinID, mrnaID);
}

void doTrackForm(char *psOutput)
/* Make the tracks display form */
{
/* Tell browser where to go when they click on image. */
hPrintf("<FORM ACTION=\"%s\" NAME=\"TrackForm\" METHOD=POST>\n\n", "../cgi-bin/pbTracks");
cartSaveSession(cart);

hPrintf("<TABLE WIDTH=\"100%%\" BGCOLOR=\"#"HG_COL_HOTLINKS"\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"2\"><TR>\n");
hPrintf("<TD ALIGN=LEFT><A HREF=\"/index.html\">%s</A></TD>", wrapWhiteFont("Home"));
hPrintf("<TD ALIGN=CENTER><FONT COLOR=\"#FFFFFF\" SIZE=4>%s</FONT></TD>", 
	"UCSC Proteome Browser (V1.0)");
hPrintf("<TD ALIGN=Right><A HREF=\"../goldenPath/help/%s\">%s</A></TD>",
        "hgTracksHelp.html", wrapWhiteFont("Help"));
hPrintf("</TR></TABLE>");
fflush(stdout);

//hPrintf("<CENTER>");

/* Make clickable image and map. */
makeActiveImagePB(psOutput);

//hPrintf("</CENTER>\n");
hPrintf("</FORM>");
}


void handlePostscript()
/* Deal with Postscript output. */
{
struct tempName psTn;
char *pdfFile = NULL;
makeTempName(&psTn, "hgt", ".eps");
hPrintf("<H1>PostScript/PDF Output</H1>\n");
hPrintf("PostScript images can be printed at high resolution "
       "and edited by many drawing programs such as Adobe "
       "Illustrator.<BR>");
doTrackForm(psTn.forCgi);
hPrintf("<A HREF=\"%s\">Click here to download</A> "
       "the current browser graphic in PostScript.  ", psTn.forCgi);
pdfFile = convertEpsToPdf(psTn.forCgi);
if(pdfFile != NULL) 
    {
    hPrintf("<BR><BR>PDF can be viewed with Adobe Acrobat Reader.<BR>\n");
    hPrintf("<A HREF=\"%s\">Click here to download</A> "
	   "the current browser graphic in PDF", pdfFile);
    }
else
    hPrintf("<BR><BR>PDF format not available");
freez(&pdfFile);
}

void doMiddle(struct cart *theCart)
/* Print the body of an html file.   */
{
char cond_str[255];
struct sqlConnection *conn;
char query[256];
struct sqlResult *sr;
char **row;
char *proteinAC;

char *debugTmp = NULL;
struct dyString *state = NULL;

/* Initialize layout and database. */
cart = theCart;

/* Uncomment this to see parameters for debugging. */
/* Be careful though, it breaks if custom track
 * is more than 4k */
//state = cgiUrlString();
//hPrintf("State: %s\n", state->string);   

getDbAndGenome(cart, &database, &organism);
hSetDb(database);
protDbName = hPdbFromGdb(database);
spConn = sqlConnect("swissProt");
debugTmp = cartUsualString(cart, "hgDebug", "off");
if(sameString(debugTmp, "on"))
    hgDebug = TRUE;
else
    hgDebug = FALSE;

hDefaultConnect();
conn  = hAllocConn();

proteinID = cartOptionalString(cart, "proteinID");

// check proteinID to see if it is a valid SWISS-PROT/TrEMBL accession or display ID
// then assign the accession number to global variable proteinID
sprintf(cond_str, "accession='%s'", proteinID);
proteinAC = sqlGetField(conn, protDbName, "spXref3", "accession", cond_str);
if (proteinAC == NULL)
    {
    sprintf(cond_str, "displayID='%s'", proteinID);
    proteinAC = sqlGetField(conn, protDbName, "spXref3", "accession", cond_str);
    if (proteinAC == NULL)
	{
	errAbort("Protein %s not found at SWISS-PROT/TrEMBL", proteinID);
	}
    else
	{
	protDisplayID = proteinID;
	proteinID = proteinAC;
	}
    }
else
    {
    sprintf(cond_str, "accession='%s'", proteinID);
    protDisplayID = sqlGetField(conn, protDbName, "spXref3", "displayID", cond_str);
    }

sprintf(cond_str, "proteinID='%s'", protDisplayID);
mrnaID = sqlGetField(conn, database, "knownGene", "name", cond_str);

sprintf(cond_str, "accession='%s'", proteinID);
description = sqlGetField(NULL, protDbName, "spXref3", "description", cond_str);

/* Do main display. */
doTrackForm(NULL);
}

void doDown(struct cart *cart)
{
hPrintf("<H2>The Browser is Being Updated</H2>\n");
hPrintf("The browser is currently unavailable.  We are in the process of\n");
hPrintf("updating the database and the display software.\n");
hPrintf("Please try again later.\n");
}

/* Other than submit and Submit all these vars should start with hgt.
 * to avoid weeding things out of other program's namespaces.
 * Because the browser is a central program, most of it's cart 
 * variables are not hgt. qualified.  It's a good idea if other
 * program's unique variables be qualified with a prefix though. */
char *excludeVars[] = { "submit", "Submit", "hgt.reset",
			"hgt.in1", "hgt.in2", "hgt.in3", "hgt.inBase",
			"hgt.out1", "hgt.out2", "hgt.out3",
			"hgt.left1", "hgt.left2", "hgt.left3", 
			"hgt.right1", "hgt.right2", "hgt.right3", 
			"hgt.dinkLL", "hgt.dinkLR", "hgt.dinkRL", "hgt.dinkRR",
			"hgt.tui", "hgt.hideAll", "hgt.psOutput", "hideControls",
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
htmlSetBackground("../images/floret.jpg");
if (cgiVarExists("hgt.reset"))
    resetVars();
cartHtmlShell("UCSC Proteome Browser V1.0", doMiddle, hUserCookie(), excludeVars, NULL);
return 0;
}
