/* Handle details pages for rna fold (evofold) tracks. */

#include "common.h"
#include "jksql.h"
#include "hgMaf.h"
#include "maf.h"
#include "cart.h"
#include "hgc.h"
#include "hCommon.h"
#include "hgColors.h"
#include "obscure.h"
#include "customTrack.h"
#include "htmshell.h"
#include "rnautil.h"
#include "rnaSecStr.h"
#include "memalloc.h"

static char const rcsid[] = "$Id: rnaFoldClick.c,v 1.6.44.1 2008/07/31 02:24:21 markd Exp $";

/* Taken from hgc.c (should probably be in hgc.h)*/
#define RED 0xFF0000
#define GREEN 0x00FF00
#define BLUE 0x0000FF
#define BLACK 0x000000
#define CYAN 0x00FFFF
#define GRAY 0xcccccc
#define LTGRAY 0x999999
#define ORANGE 0xDD6600
#define MAGENTA 0xFF00FF

#define LTPURPLE 0x9966CC
#define SCORE_SHADES_COUNT 10

void printRfamUrl(char *itemName)
/* Print a link to the Rfam entry corresponding to the item. */
{
char *query = cloneString(itemName);
char *end   = strchr(query, '.');
if (!end)
    return;
else
    *end = 0;
printf("<p><A HREF=\"");
printf("http://www.sanger.ac.uk/cgi-bin/Rfam/seqget.pl?name=%s", query);
printf("\" TARGET=_blank>%s</A></p>", itemName);
freeMem(query);
}

void doubleArray2binArray(double *scores, int size, double minScore, double maxScore, int *binArray, int binCount)
/* Will assign a bin to each score in 'scores' and store it in 'binArray' */
{
int i;
for (i = 0; i < size; i++)
    binArray[i] = assignBin(scores[i], minScore, maxScore, binCount);
}

void htmlColorPrintString(FILE *f, char *s, int *colorFormat, int *colors, int L)
/* Prints s to f wrapping characters in html tags for coloring
 * according to colorFormat and colors. colorFormat must be of same
 * length as s. Each position of colorFormat defines which of the
 * colors to use. Use 0 for default color. If L is non-NULL it
 * defines a mximum length to print.*/
{
int i;

if (!L || (L && strlen(s)<L) )
    L = strlen(s);

fprintf(f, "<FONT COLOR=\"%06X\">", colors[0]); /* default color */
for (i = 0; i < L; i++)
    {
    if (colorFormat[i] == 0)
	fprintf(f, "%c", s[i]);
    else
	fprintf(f, "<FONT COLOR=\"%06X\">%c</FONT>", colors[ colorFormat[i] ], s[i]);
    }
fprintf(f, "</FONT>");
}

void mafAndFoldHeader(FILE *f)
/* Print header for maf and fold the section*/
{
fprintf(f, "<center><h2> Multiple alignment and RNA secondary structure annotation </h2></center>");
}

void mkPosString(char *s, int size)
/* Make a string-ruler with a decimal every tenth position */
{
int i, d;
for (i = 0, d = 0; i < size; i++)
    if (i%10 == 0) 
	{
	s[i] = '0' + d;
	d++;
	if (d == 10) 
	  d = 0;
	}
    else
	s[i] = ' ';
s[size] = '\0';
}

char *mkScoreString(double *scores, int L, char *referenceText)
    /* allocate string of score symbols, adjust for gaps in reference sequene */
{
char *s = (char *) needMem(L+1);
char *scoreString = NULL;
int i, n;
for (i=0;i<L;i++) 
    {
    n    = floor(10 *scores[i]);
    if (n == 10) 
	n = 9;
    if (n < 0 || n > 9)
	errAbort( "Score not between 0 and 1\n" );
    s[i] = '0' + n;
    }
s[L] = 0;
scoreString = gapAdjustFold(s, referenceText);
freeMem(s);
return scoreString;
}

int *mkScoreColorFormat(double *scores, int L, char *referenceText)
  /* allocate string of score colors (gray shades), adjust for gaps in reference sequene */
{
int *v = NULL;
int *scoreColorFormat = NULL;
v = (int *) needMem(L *sizeof(int) );
doubleArray2binArray(scores, L, 0.0, 1.0, v, SCORE_SHADES_COUNT);
scoreColorFormat = gapIntArrayAdjust(v, referenceText);
freeMem(v);
return scoreColorFormat;
}

void defineMafSubstColors(int **p2mafColors)
/* Defines the colors used for maf coloring. Must corrspsond to the
 * markCompensatoryMutations function.*/
{
int *mafColors;
AllocArray(*p2mafColors, 8);
mafColors = *p2mafColors;
mafColors[0]    = LTGRAY;   /* not pairing */
mafColors[1]    = LTPURPLE; /* not pairing, substitution */
mafColors[2]    = BLACK;    /* compatible with pairing */
mafColors[3]    = BLUE;     /* compatible with pairing, single substitution  */
mafColors[4]    = GREEN;    /* compensatory change */
mafColors[5]    = RED;      /* not compatible with fold, single subs */
mafColors[6]    = ORANGE;   /* not compatible with fold, double subs */
mafColors[7]    = MAGENTA;  /* not compatible with fold, involves indel */
}

void htmlPrintMafAndFold(FILE *f, struct mafAli *maf, char *fold, double *scores, int lineSize)
/* HTML pretty print maf and fold to f. If scores is non-null then
 * scores are indicated below alignemnt.*/
{
struct mafComp *mc;
int  i, N, lineStart, lineEnd;
int  *pairList = NULL;
int  *mafColors = NULL;
int  **mafColorFormats = NULL;
int  *scoreColorFormat = NULL;
int  scoreColors[]  = {0x999999,0x888888,0x777777,0x666666,0x555555,0x444444,0x333333,0x222222,0x111111,0x000000};
char *scoreString = 0;
char *pairSymbols;
char *posString, *adjPosString;
char *adjFold;
char foldTag[]       = "SS anno";
char scoresTag[]     = "score  ";
char positionTag[]   = "offset";
char pairSymbolsTag[]= "pair symbol";
char *referenceText  = maf->components->text; /* first sequence in the alignment */
int  srcChars        = max( strlen(foldTag), strlen(pairSymbolsTag));
int  refLength       = strlen(referenceText);
int  foldLength      = strlen(fold);


/* Adjust fold to gap structure in referenceText */
adjFold = gapAdjustFold(fold, referenceText);

/* make arrays for color formatting */
N = slCount(maf->components);
AllocArray(mafColorFormats, N);
for (i = 0; i < N; i++)
    mafColorFormats[i] = AllocArray(mafColorFormats[i], refLength);
fold2pairingList(adjFold, refLength, &pairList);

for (mc = maf->components, i = 0; mc != NULL; mc = mc->next, i++)
    markCompensatoryMutations(mc->text, referenceText, pairList, mafColorFormats[i]);
defineMafSubstColors(&mafColors);

/* Make pos string */
posString = needMem(foldLength + 1);
mkPosString(posString, foldLength);
adjPosString = gapAdjustFold(posString, referenceText);

/* Make a symbol string indicating pairing partner */
pairSymbols = (char *) needMem(refLength + 1);
mkPairPartnerSymbols(pairList, pairSymbols, refLength);

/* Make the score string and its colorFormat */
if (scores)
     {
     scoreString      = mkScoreString(scores, foldLength, referenceText);
     scoreColorFormat = mkScoreColorFormat(scores, foldLength, referenceText);
     }

/* Find max. length of source (species) field. */
for (mc = maf->components; mc != NULL; mc = mc->next)
    {
    int len = strlen(mc->src);
    if (srcChars < len)
        srcChars = len;
    }

/* Pretty print maf and fold */
fprintf(f, "<PRE><TT>");
for (lineStart = 0; lineStart < maf->textSize; lineStart = lineEnd)
    {
    int size;
    lineEnd = lineStart + lineSize;
    if (lineEnd >= maf->textSize)
        lineEnd = maf->textSize;
    size = lineEnd - lineStart;
    fprintf(f, "%-*s %.*s\n", srcChars, positionTag, lineSize, adjPosString + lineStart);
    for (mc = maf->components, i = 0; mc != NULL; mc = mc->next, i++)
        {
	fprintf(f, "%-*s ", srcChars, mc->src);
	htmlColorPrintString(f, mc->text + lineStart, mafColorFormats[i] + lineStart, mafColors, lineSize);
	fprintf(f, "\n");
	}
    fprintf(f, "%-*s %.*s\n", srcChars, foldTag, lineSize, adjFold + lineStart);
    fprintf(f, "%-*s %.*s\n", srcChars, pairSymbolsTag, lineSize, pairSymbols + lineStart);
    if (scores)
	{
	fprintf(f, "%-*s ", srcChars, scoresTag);
	htmlColorPrintString(f, scoreString + lineStart, scoreColorFormat + lineStart, scoreColors, lineSize);
	fprintf(f, "\n");
	}
    fprintf(f, "\n");
    }
fprintf(f, "</PRE></TT>");

/* clean up*/
freeMem(posString);
freeMem(adjPosString);
freeMem(adjFold);
freeMem(pairSymbols);
if (scores)
    {
    freeMem(scoreString);
    freeMem(scoreColorFormat);
    }
for (i = 0; i < N; i++)
    freeMem(mafColorFormats[i]);
freeMem(mafColorFormats);
}

void mafAndFoldLegend(FILE *f)
/* Print legend for the maf and fold section */
{
char *s          = "0123456789";
int colorFormat[] = {0,1,2,3,4,5,6,7,8,9};
int colors[]      = {0x999999,0x888888,0x777777,0x666666,0x555555,0x444444,0x333333,0x222222,0x111111,0x000000};
fprintf(f, "<h3> Color legend </h3>");
fprintf(f, "<TABLE BORDER=0>");
fprintf(f, "<TR> <TD> <FONT COLOR=\"%06X\">GRAY:       </TD> <TD> Not part of annotated pair, no substitution. <BR>             </TD> </TR>", LTGRAY);
fprintf(f, "<TR> <TD> <FONT COLOR=\"%06X\">LT. PURPLE: </TD> <TD> Not part of annotated pair, substitution. <BR>                </TD> </TR>", LTPURPLE);
fprintf(f, "<TR> <TD> <FONT COLOR=\"%06X\">BLACK:      </TD> <TD> Compatible with annotated pair, no substitutions.<BR>         </TD> </TR>", BLACK);
fprintf(f, "<TR> <TD> <FONT COLOR=\"%06X\">BLUE:       </TD> <TD> Compatible with annotated pair, single substitution.<BR>      </TD> </TR>", BLUE);
fprintf(f, "<TR> <TD> <FONT COLOR=\"%06X\">GREEN:      </TD> <TD> Compatible with annotated pair, double substitution.<BR>      </TD> </TR>", GREEN);
fprintf(f, "<TR> <TD> <FONT COLOR=\"%06X\">RED:        </TD> <TD> Not compatible with annotated pair, single substitution. <BR> </TD> </TR>", RED);
fprintf(f, "<TR> <TD> <FONT COLOR=\"%06X\">ORANGE:     </TD> <TD> Not compatible with annotated pair, double substitution. <BR> </TD> </TR>", ORANGE);
fprintf(f, "<TR> <TD> <FONT COLOR=\"%06X\">MAGENTA:    </TD> <TD> Not compatible with annotated pair, involves gap. <BR>        </TD> </TR>", MAGENTA);
fprintf(f, "</TABLE>");
/* Score legend */
fprintf(f, "<BR>SCORE:   min ");
htmlColorPrintString(f, s, colorFormat, colors, 0);
fprintf(f, " max");
}

struct mafAli *mafFromRnaSecStrItem(char *mafTrack, struct rnaSecStr *item)
{
struct mafAli *maf;
maf = hgMafFrag(database, mafTrack, item->chrom, item->chromStart, item->chromEnd, item->strand[0], NULL, NULL);
return maf;
}

void mafSortBySpeciesOrder(struct mafAli *maf, char *speciesOrder)
/* Sort maf components by species. Excised from mafClick.c. */
{
int speciesCt;
char *species[256];
struct mafComp **newOrder, *mcThis;
int i;
int mcCount;
   
mcCount = 0;
speciesCt = chopLine(cloneString(speciesOrder), species);
newOrder = needMem((speciesCt + 1) * sizeof (struct mafComp *));
newOrder[mcCount++] = maf->components;
   
for (i = 0; i < speciesCt; i++)
  {
    if ((mcThis = mafMayFindCompSpecies(maf, species[i], '.')) == NULL)
	continue;
    newOrder[mcCount++] = mcThis;
  }
   
maf->components = NULL;
for (i = 0; i < mcCount; i++)
  {
    newOrder[i]->next = 0;
    slAddHead(&maf->components, newOrder[i]);
  }
   
slReverse(&maf->components);
}

void doRnaSecStr(struct trackDb *tdb, char *itemName)
/* Handle click on rnaSecStr type elements. */
{
char *table = tdb->tableName;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
struct rnaSecStr *item;
char extraWhere[256];
char **row;
int  rowOffset = 0;
char *mafTrack = trackDbRequiredSetting(tdb, "mafTrack");
int start = cartInt(cart, "o");
struct mafAli *maf;
char option[128];
char *speciesOrder = NULL;

/* print header */
genericHeader(tdb, itemName);
/* printRfamUrl(itemName); */
genericBedClick(conn, tdb, itemName, start, 6);
htmlHorizontalLine();

/* get the rnaSecStr and maf from db */
sprintf(extraWhere, "chromStart = %d and name = '%s'", start, itemName);
sr   = hExtendedChromQuery(conn, table, seqName, extraWhere,  FALSE, NULL, &rowOffset);
row  = sqlNextRow(sr);
item = rnaSecStrLoad(row + rowOffset);
maf  = mafFromRnaSecStrItem(mafTrack, item);

/* order maf by species */
safef(option, sizeof(option), "%s.speciesOrder", tdb->tableName);
speciesOrder = cartUsualString(cart, option, NULL);
if (speciesOrder == NULL)
  speciesOrder = trackDbSetting(tdb, "speciesOrder");
if (speciesOrder)
  mafSortBySpeciesOrder(maf, speciesOrder);

mafAndFoldHeader(stdout);
htmlPrintMafAndFold(stdout, maf, item->secStr, item->conf, 100);

mafAndFoldLegend(stdout);
/* track specific html */
printTrackHtml(tdb);

/* clean up */
mafAliFree(&maf);
rnaSecStrFree(&item);
sqlFreeResult(&sr);
hFreeConn(&conn);
}
