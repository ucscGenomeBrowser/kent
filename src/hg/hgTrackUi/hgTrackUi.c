#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "web.h"
#include "cart.h"
#include "jksql.h"
#include "trackDb.h"
#include "hdb.h"
#include "hCommon.h"
#include "hui.h"
#include "ldUi.h"
#include "snpUi.h"
#include "snp125Ui.h"
#include "sample.h"
#include "cdsColors.h"
#include "wiggle.h"
#include "hgMaf.h"
#include "obscure.h"
#include "chainCart.h"
#include "chainDb.h"
#include "phyloTree.h"
#include "humanPhenotypeUi.h"
#include "hgMutUi.h"
#include "gvUi.h"
#include "landmarkUi.h"
#include "chromGraph.h"
#include "hgConfig.h"
#include "dbRIP.h"

#define CDS_HELP_PAGE "/goldenPath/help/hgCodonColoring.html"
#define CDS_MRNA_HELP_PAGE "/goldenPath/help/hgCodonColoringMrna.html"
#define CDS_BASE_HELP_PAGE "/goldenPath/help/hgBaseLabel.html"
#define WIGGLE_HELP_PAGE  "/goldenPath/help/hgWiggleTrackHelp.html"

static char const rcsid[] = "$Id: hgTrackUi.c,v 1.287 2006/07/19 22:00:02 hiram Exp $";

struct cart *cart = NULL;	/* Cookie cart with UI settings */
char *database = NULL;		/* Current database. */
char *chromosome = NULL;	/* Chromosome. */

void radioButton(char *var, char *val, char *ourVal)
/* Print one radio button */
{
cgiMakeRadioButton(var, ourVal, sameString(ourVal, val));
printf("%s ", ourVal);
}

void filterButtons(char *filterTypeVar, char *filterTypeVal, boolean none)
/* Put up some filter buttons. */
{
printf("<B>Filter:</B> ");
radioButton(filterTypeVar, filterTypeVal, "red");
radioButton(filterTypeVar, filterTypeVal, "green");
radioButton(filterTypeVar, filterTypeVal, "blue");
radioButton(filterTypeVar, filterTypeVal, "exclude");
radioButton(filterTypeVar, filterTypeVal, "include");
if (none)
    radioButton(filterTypeVar, filterTypeVal, "none");
}

void stsMapUi(struct trackDb *tdb)
/* Put up UI stsMarkers. */
{
char *stsMapFilter = cartUsualString(cart, "stsMap.filter", "blue");
char *stsMapMap = cartUsualString(cart, "stsMap.type", smoeEnumToString(0));
filterButtons("stsMap.filter", stsMapFilter, TRUE);
printf(" ");
smoeDropDown("stsMap.type", stsMapMap);
}

void stsMapMouseUi(struct trackDb *tdb)
/* Put up UI stsMapMouse. */
{
char *stsMapMouseFilter = cartUsualString(cart, "stsMapMouse.filter", "blue");
char *stsMapMouseMap = cartUsualString(cart, "stsMapMouse.type", smmoeEnumToString(0));
filterButtons("stsMapMouse.filter", stsMapMouseFilter, TRUE);
printf(" ");
smmoeDropDown("stsMapMouse.type", stsMapMouseMap);
}

void stsMapRatUi(struct trackDb *tdb)
/* Put up UI stsMapRat. */
{
char *stsMapRatFilter = cartUsualString(cart, "stsMapRat.filter", "blue");
char *stsMapRatMap = cartUsualString(cart, "stsMapRat.type", smroeEnumToString(0));
filterButtons("stsMapRat.filter", stsMapRatFilter, TRUE);
printf(" ");
smroeDropDown("stsMapRat.type", stsMapRatMap);
}

void snpMapTypeFilterButtons(char *filterTypeVar, char *filterTypeVal)
/* Put up some filter buttons. */
{
radioButton(filterTypeVar, filterTypeVal, "include");
radioButton(filterTypeVar, filterTypeVal, "exclude");
}

void snpColorFilterButtons(char *filterTypeVar, char *filterTypeVal)
/* Put up some filter buttons. */
{
int i;
for (i=0; i<snpColorSourceLabelsSize; i++)
    {
    cgiMakeRadioButton(filterTypeVar, snpColorSourceStrings[i], 
		       sameString(snpColorSourceStrings[i], filterTypeVal));
    printf("%s &nbsp;", snpColorSourceLabels[i]);
    }
printf("<BR>\n");
}

void snpFilterButtons(char *filterTypeVar, char *filterTypeVal)
/* Put up some filter buttons. */
{
printf("&nbsp;&nbsp;&nbsp;&nbsp;");
cgiMakeRadioButton(filterTypeVar, "exclude", sameString("exclude", filterTypeVal));
printf("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<B>|</B>&nbsp;&nbsp;");
radioButton(filterTypeVar, filterTypeVal, "red");
radioButton(filterTypeVar, filterTypeVal, "green");
radioButton(filterTypeVar, filterTypeVal, "blue");
radioButton(filterTypeVar, filterTypeVal, "black");
}

void snp125FilterButtons(char *filterTypeVar, char *filterTypeVal)
/* Put up some filter buttons. */
{
printf("&nbsp;&nbsp;&nbsp;&nbsp;");
radioButton(filterTypeVar, filterTypeVal, "red");
radioButton(filterTypeVar, filterTypeVal, "green");
radioButton(filterTypeVar, filterTypeVal, "blue");
radioButton(filterTypeVar, filterTypeVal, "black");
}

void snpMapUi(struct trackDb *tdb)
/* Put up UI snpMap data. */
{
int   snpMapSource = 0;
int   snpMapType = 0;

if (strncmp(database,"hg",2))
    return;
printf("<BR><B>Variant Sources:</B><BR>\n");
for (snpMapSource=0; snpMapSource<snpMapSourceCartSize; snpMapSource++)
    {
    snpMapSourceCart[snpMapSource] = 
	cartUsualString(cart, snpMapSourceStrings[snpMapSource], snpMapSourceDefault[snpMapSource]);
    snpFilterButtons(snpMapSourceStrings[snpMapSource], snpMapSourceCart[snpMapSource]);
    printf(" - <B>%s</B><BR>\n", snpMapSourceLabels[snpMapSource]);
    }
printf("<BR><B>Variant Types:</B><BR>\n");
for (snpMapType=0; snpMapType<snpMapTypeCartSize; snpMapType++)
    {
    snpMapTypeCart[snpMapType] = 
	cartUsualString(cart, snpMapTypeStrings[snpMapType], snpMapTypeDefault[snpMapType]);
    snpMapTypeFilterButtons(snpMapTypeStrings[snpMapType], snpMapTypeCart[snpMapType]);
    printf(" - <B>%s</B><BR>\n", snpMapTypeLabels[snpMapType]);
    }
}

void snp125Ui(struct trackDb *tdb)
{
/* It would be nice to add a 'reset' button here to reset the snp
 * variables to their defaults. */
int i = 0;
char *autoSubmit = "onchange=\"document.snp125UiForm.submit();\"";

snp125AvHetCutoff = atof(cartUsualString(cart, "snp125AvHetCutoff", "0"));
printf("<BR><B>Minimum <A HREF=\"#AvHet\">Average Heterozygosity</A>:</B>&nbsp;");
cgiMakeDoubleVar("snp125AvHetCutoff",snp125AvHetCutoff,6);

snp125WeightCutoff = atoi(cartUsualString(cart, "snp125WeightCutoff", "3"));
printf("<BR><B>Maximum <A HREF=\"#Weight\">Weight</A>:</B>&nbsp;");
cgiMakeIntVar("snp125WeightCutoff",snp125WeightCutoff,4);

printf("<BR><BR>\n");
printf("Any type of data can be excluded from view by deselecting the checkbox below.\n");
printf("<BR><BR>\n");

printf("<B>Location Type</B>: ");
printf("<BR>\n");
for (i=0; i < snp125LocTypeLabelsSize; i++)
    {
    snp125LocTypeIncludeCart[i] = 
        cartUsualBoolean(cart, snp125LocTypeIncludeStrings[i], snp125LocTypeIncludeDefault[i]);
    cgiMakeCheckBox(snp125LocTypeIncludeStrings[i], snp125LocTypeIncludeCart[i]);
    printf(" %s", snp125LocTypeLabels[i]);
    }
printf("<BR>\n");


printf("<B>Class</B>: ");
printf("<BR>\n");
for (i=0; i < snp125ClassLabelsSize; i++)
    {
    snp125ClassIncludeCart[i] = cartUsualBoolean(cart, snp125ClassIncludeStrings[i], snp125ClassIncludeDefault[i]);
    cgiMakeCheckBox(snp125ClassIncludeStrings[i], snp125ClassIncludeCart[i]);
    printf(" %s", snp125ClassLabels[i]);
    }
printf("<BR>\n");

printf("<B>Validation</B>: ");
printf("<BR>\n");
for (i=0; i < snp125ValidLabelsSize; i++)
    {
    snp125ValidIncludeCart[i] = cartUsualBoolean(cart, snp125ValidIncludeStrings[i], snp125ValidIncludeDefault[i]);
    cgiMakeCheckBox(snp125ValidIncludeStrings[i], snp125ValidIncludeCart[i]);
    printf(" %s", snp125ValidLabels[i]);
    }
printf("<BR>\n");

printf("<B>Function</B>: ");
printf("<BR>\n");
for (i=0; i < snp125FuncLabelsSize; i++)
    {
    snp125FuncIncludeCart[i] = cartUsualBoolean(cart, snp125FuncIncludeStrings[i], snp125FuncIncludeDefault[i]);
    cgiMakeCheckBox(snp125FuncIncludeStrings[i], snp125FuncIncludeCart[i]);
    printf(" %s", snp125FuncLabels[i]);
    }
printf("<BR>\n");

printf("<B>Molecule Type</B>: ");
printf("<BR>\n");
for (i=0; i < snp125MolTypeLabelsSize; i++)
    {
    snp125MolTypeIncludeCart[i] = cartUsualBoolean(cart, snp125MolTypeIncludeStrings[i], snp125MolTypeIncludeDefault[i]);
    cgiMakeCheckBox(snp125MolTypeIncludeStrings[i], snp125MolTypeIncludeCart[i]);
    printf(" %s", snp125MolTypeLabels[i]);
    }
printf("<BR>\n");

/* prematurely close the hgTracks FORM because forms cannot be nested */
/* we will open another FORM here, and allow it to be closed later */
printf("</FORM>\n");
puts("<FORM ACTION=\"/cgi-bin/hgTrackUi\" NAME=\"snp125UiForm\" METHOD=\"GET\">");
cartSaveSession(cart);
cgiContinueHiddenVar("g");
cgiContinueHiddenVar("c");

printf("<HR>\n");
printf("<B>SNP Feature for Color Specification:</B>\n");
snp125ColorSourceCart[0] = cartUsualString(cart, snp125ColorSourceDataName[0], snp125ColorSourceDefault[0]);
cgiMakeDropListFull(snp125ColorSourceDataName[0], snp125ColorSourceLabels, snp125ColorSourceLabels, 
                    snp125ColorSourceLabelsSize, snp125ColorSourceCart[0], autoSubmit);
printf("<BR><BR>\n");
printf("The selected feature above has the following values below.  \n");
printf("For each value, a selection of colors is available.<BR><BR>\n");

snp125ColorSourceCart[0] = cartUsualString(cart, snp125ColorSourceDataName[0], snp125ColorSourceDefault[0]);
if (sameString(snp125ColorSourceCart[0], "Location Type"))
    {
    for (i=0; i<snp125LocTypeLabelsSize; i++)
        {
	printf("<B>%s</B>: ", snp125LocTypeLabels[i]);
	snp125LocTypeCart[i] = cartUsualString(cart, snp125LocTypeStrings[i], snp125LocTypeDefault[i]);
        cgiMakeDropListFull(snp125LocTypeStrings[i], snp125ColorLabel, snp125ColorLabel, 
                            snp125ColorLabelSize, snp125LocTypeCart[i], autoSubmit);
        }
    }
else if (sameString(snp125ColorSourceCart[0], "Class"))
    {
    for (i=0; i<snp125ClassLabelsSize; i++)
        {
	printf("<B>%s</B>: ", snp125ClassLabels[i]);
	snp125ClassCart[i] = cartUsualString(cart, snp125ClassStrings[i], snp125ClassDefault[i]);
        cgiMakeDropListFull(snp125ClassStrings[i], snp125ColorLabel, snp125ColorLabel, 
                            snp125ColorLabelSize, snp125ClassCart[i], autoSubmit);
        }
    }
else if (sameString(snp125ColorSourceCart[0], "Validation"))
    {
    for (i=0; i<snp125ValidLabelsSize; i++)
        {
	printf("<B>%s</B>: ", snp125ValidLabels[i]);
	snp125ValidCart[i] = cartUsualString(cart, snp125ValidStrings[i], snp125ValidDefault[i]);
        cgiMakeDropListFull(snp125ValidStrings[i], snp125ColorLabel, snp125ColorLabel, 
                            snp125ColorLabelSize, snp125ValidCart[i], autoSubmit);
        }
    }
else if (sameString(snp125ColorSourceCart[0], "Function"))
    {
    for (i=0; i<snp125FuncLabelsSize; i++)
        {
	printf("<B>%s</B>: ", snp125FuncLabels[i]);
	snp125FuncCart[i] = cartUsualString(cart, snp125FuncStrings[i], snp125FuncDefault[i]);
        cgiMakeDropListFull(snp125FuncStrings[i], snp125ColorLabel, snp125ColorLabel, 
                            snp125ColorLabelSize, snp125FuncCart[i], autoSubmit);
        }
    }
else if (sameString(snp125ColorSourceCart[0], "Molecule Type"))
    {
    for (i=0; i<snp125MolTypeLabelsSize; i++)
        {
	printf("<B>%s</B>: ", snp125MolTypeLabels[i]);
	snp125MolTypeCart[i] = cartUsualString(cart, snp125MolTypeStrings[i], snp125MolTypeDefault[i]);
        cgiMakeDropListFull(snp125MolTypeStrings[i], snp125ColorLabel, snp125ColorLabel, 
                            snp125ColorLabelSize, snp125MolTypeCart[i], autoSubmit);
        }
    }

printf("<BR>\n");
printf("<HR>\n");


}

void snpUi(struct trackDb *tdb)
/* Put up UI snp data. */
{
int   snpSource  = 0;
int   snpMolType = 0;
int   snpClass   = 0;
int   snpValid   = 0;
int   snpFunc    = 0;
int   snpLocType = 0;

/* It would be nice to add a 'reset' button here to reset the snp
 * variables to their defaults.
 * I'd also like to see 'submit' buttons at several places along the
 * page, as the page is very tall and scrolling is tedious. */

printf("<BR><B>Colors and Filters:</B>\n");
printf("<BR>Use the &quot;Color Specification&quot; buttons to specify a group to direct coloring for the track display.\n");
printf("<BR>Variants can optionally be excluded based on their values in each of the subsequent categories by choosing &quot;exclude&quot;.\n");
printf("<BR>\n");

printf("<BR><B>Color Specification:</B><BR>\n");
snpColorSourceCart[0] = cartUsualString(cart, snpColorSourceDataName[0], snpColorSourceDefault[0]);
snpColorFilterButtons(snpColorSourceDataName[0], snpColorSourceCart[0]);

snpAvHetCutoff = atof(cartUsualString(cart, "snpAvHetCutoff", "0"));
printf("<BR><B>Minimum <A HREF=\"#AvHet\">Average Heterozygosity</A>:</B>&nbsp;");
cgiMakeDoubleVar("snpAvHetCutoff",snpAvHetCutoff,6);

printf("<BR><BR><B>Exclude&nbsp;&nbsp;|&nbsp;&nbsp;<A HREF=\"#Source\">Sources</A>:</B><BR>\n");
for (snpSource=0; snpSource < snpSourceCartSize; snpSource++)
    {
    snpSourceCart[snpSource] = cartUsualString(cart, snpSourceStrings[snpSource], snpSourceDefault[snpSource]);
    snpFilterButtons(snpSourceStrings[snpSource], snpSourceCart[snpSource]);
    printf(" - <B>%s</B><BR>\n", snpSourceLabels[snpSource]);
    }
printf("<BR><B>Exclude&nbsp;&nbsp;|&nbsp;&nbsp;<A HREF=\"#MolType\">Molecule Types</A>:</B><BR>\n");
for (snpMolType=0; snpMolType<snpMolTypeCartSize; snpMolType++)
    {
    snpMolTypeCart[snpMolType] = cartUsualString(cart, snpMolTypeStrings[snpMolType], snpMolTypeDefault[snpMolType]);
    snpFilterButtons(snpMolTypeStrings[snpMolType], snpMolTypeCart[snpMolType]);
    printf(" - <B>%s</B><BR>\n", snpMolTypeLabels[snpMolType]);
    }
printf("<BR><B>Exclude&nbsp;&nbsp;|&nbsp;&nbsp;<A HREF=\"#Class\">Variant Classes</A>:</B><BR>\n");
for (snpClass=0; snpClass<snpClassCartSize; snpClass++)
    {
    snpClassCart[snpClass] = cartUsualString(cart, snpClassStrings[snpClass], snpClassDefault[snpClass]);
    snpFilterButtons(snpClassStrings[snpClass], snpClassCart[snpClass]);
    printf(" - <B>%s</B><BR>\n", snpClassLabels[snpClass]);
    }
printf("<BR><B>Exclude&nbsp;&nbsp;|&nbsp;&nbsp;<A HREF=\"#Valid\">Validation Status</A>:</B><BR>\n");
for (snpValid=0; snpValid<snpValidCartSize; snpValid++)
    {
    snpValidCart[snpValid] = cartUsualString(cart, snpValidStrings[snpValid], snpValidDefault[snpValid]);
    snpFilterButtons(snpValidStrings[snpValid], snpValidCart[snpValid]);
    printf(" - <B>%s</B><BR>\n",snpValidLabels[snpValid]);
    }
printf("<BR><B>Exclude&nbsp;&nbsp;|&nbsp;&nbsp;<A HREF=\"#Func\">Functional Classes<A>:</B><BR>\n");
for (snpFunc=0; snpFunc<snpFuncCartSize; snpFunc++)
    {
    snpFuncCart[snpFunc] = cartUsualString(cart, snpFuncStrings[snpFunc], snpFuncDefault[snpFunc]);
    snpFilterButtons(snpFuncStrings[snpFunc], snpFuncCart[snpFunc]);
    printf(" - <B>%s</B><BR>\n",snpFuncLabels[snpFunc]);
    }
printf("<BR><B>Exclude&nbsp;&nbsp;|&nbsp;&nbsp;<A HREF=\"#LocType\">Location Type</A>:</B><BR>\n");
for (snpLocType=0; snpLocType<snpLocTypeCartSize; snpLocType++)
    {
    snpLocTypeCart[snpLocType] = cartUsualString(cart, snpLocTypeStrings[snpLocType], snpLocTypeDefault[snpLocType]);
    snpFilterButtons(snpLocTypeStrings[snpLocType], snpLocTypeCart[snpLocType]);
    printf(" - <B>%s</B><BR>\n",snpLocTypeLabels[snpLocType]);
    }
}

void ldUi(struct trackDb *tdb)
/* Put up UI snp data. */
{
struct dyString *dsLdVal = newDyString(32);
struct dyString *dsLdPos = newDyString(32);
struct dyString *dsLdOut = newDyString(32);
struct dyString *dsLdTrm = newDyString(32);
struct dyString *dsLdInv = newDyString(32);
char    *ldVal;
char    *ldPos;
char    *ldOut;
boolean  ldTrm;
boolean  ldInv;
/* these subTrack-specific variables are needed for special case composite tracks */
boolean  hapmapLdCeu_inv    = cartUsualBoolean(cart, "hapmapLdCeu_inv", ldInvDefault);
boolean  hapmapLdChb_inv    = cartUsualBoolean(cart, "hapmapLdChb_inv", ldInvDefault);
boolean  hapmapLdChbJpt_inv = cartUsualBoolean(cart, "hapmapLdChbJpt_inv", ldInvDefault);
boolean  hapmapLdJpt_inv    = cartUsualBoolean(cart, "hapmapLdJpt_inv", ldInvDefault);
boolean  hapmapLdYri_inv    = cartUsualBoolean(cart, "hapmapLdYri_inv", ldInvDefault);
if (startsWith("hapmapLd",tdb->tableName))
    {
    dyStringPrintf(dsLdVal, "hapmapLd_val");
    dyStringPrintf(dsLdPos, "hapmapLd_pos");
    dyStringPrintf(dsLdOut, "hapmapLd_out");
    dyStringPrintf(dsLdTrm, "hapmapLd_trm");
    dyStringPrintf(dsLdInv, "hapmapLd_inv");
    }
else
    {
    dyStringPrintf(dsLdVal, "%s_val", tdb->tableName);
    dyStringPrintf(dsLdPos, "%s_pos", tdb->tableName);
    dyStringPrintf(dsLdOut, "%s_out", tdb->tableName);
    dyStringPrintf(dsLdTrm, "%s_trm", tdb->tableName);
    dyStringPrintf(dsLdInv, "%s_inv", tdb->tableName);
    }
ldVal = cartUsualString(cart,  dsLdVal->string, ldValDefault);
ldPos = cartUsualString(cart,  dsLdPos->string, ldPosDefault);
ldOut = cartUsualString(cart,  dsLdOut->string, ldOutDefault);
ldTrm = cartUsualBoolean(cart, dsLdTrm->string, ldTrmDefault);
ldInv = cartUsualBoolean(cart, dsLdInv->string, ldInvDefault);

/* It would be nice to add a 'reset' button to reset the ld variables to their defaults. */

printf("<BR><B>LD Values:</B><BR>&nbsp;&nbsp;\n");

cgiMakeRadioButton(dsLdVal->string, "rsquared", sameString("rsquared", ldVal));
printf("&nbsp;r<sup>2</sup>&nbsp;&nbsp;");
cgiMakeRadioButton(dsLdVal->string, "dprime",   sameString("dprime",   ldVal));
printf("&nbsp;D'&nbsp;&nbsp;");
cgiMakeRadioButton(dsLdVal->string, "lod",      sameString("lod",      ldVal));
printf("&nbsp;LOD<BR>");

printf("<BR><B>Track Geometry:</B><BR>&nbsp;&nbsp;\n");

cgiMakeCheckBox(dsLdTrm->string, ldTrm); 
printf("&nbsp;Trim to triangle<BR>\n");

if (startsWith("hapmapLd", tdb->tableName))
    {
    printf("<BR>&nbsp;&nbsp;&nbsp;");
    if (hTableExists("hapmapLdYri"))
	{
	cgiMakeCheckBox("hapmapLdYri_inv", hapmapLdYri_inv); 
	printf("&nbsp;Invert display for the YRI sample - Yoruba in Ibadan, Nigeria <BR>&nbsp;&nbsp;\n");
	}
    if (hTableExists("hapmapLdCeu"))
	{
	cgiMakeCheckBox("hapmapLdCeu_inv", hapmapLdCeu_inv); 
	printf("&nbsp;Invert display for the CEU sample - CEPH (Utah residents with ancestry from northern and western Europe) <BR>&nbsp;&nbsp;\n");
	}
    if (hTableExists("hapmapLdChb"))
	{
	cgiMakeCheckBox("hapmapLdChb_inv", hapmapLdChb_inv); 
	printf("&nbsp;Invert display for the CHB sample - Han Chinese in Beijing, China <BR>&nbsp;&nbsp;\n");
	}
    if (hTableExists("hapmapLdJpt"))
	{
	cgiMakeCheckBox("hapmapLdJpt_inv", hapmapLdJpt_inv); 
	printf("&nbsp;Invert display for the JPT sample - Japanese in Tokyo, Japan <BR>&nbsp;&nbsp;\n");
	}
    if (hTableExists("hapmapLdChbJpt"))
	{
	cgiMakeCheckBox("hapmapLdChbJpt_inv", hapmapLdChbJpt_inv); 
	printf("&nbsp;Invert display for the combined JPT+CHB sample - Japanese in Tokyo, Japan and Han Chinese in Beijing, China <BR>&nbsp;&nbsp;\n");
	}
    }
else 
    {
	printf("&nbsp;&nbsp;&nbsp;");
	cgiMakeCheckBox(dsLdInv->string, ldInv); 
	printf("&nbsp;Invert the display<BR>&nbsp;&nbsp;\n");
    }
printf("<BR><B>Colors:</B>\n");

printf("<TABLE>\n ");
printf("<TR>\n  <TD>&nbsp;&nbsp;LD values&nbsp;&nbsp;</TD>\n  <TD>- ");
radioButton(dsLdPos->string, ldPos, "red");
printf("</TD>\n  <TD>");
radioButton(dsLdPos->string, ldPos, "green");
printf("</TD>\n  <TD>");
radioButton(dsLdPos->string, ldPos, "blue");
printf("</TD>\n </TR>\n ");

printf("<TR>\n  <TD>&nbsp;&nbsp;Outlines&nbsp;&nbsp;</TD>\n  <TD>- ");
radioButton(dsLdOut->string, ldOut, "red");
printf("</TD>\n  <TD>");
radioButton(dsLdOut->string, ldOut, "green");
printf("</TD>\n  <TD>");
radioButton(dsLdOut->string, ldOut, "blue");
printf("</TD>\n  <TD>");
radioButton(dsLdOut->string, ldOut, "yellow");
printf("</TD>\n  <TD>");
radioButton(dsLdOut->string, ldOut, "black");
printf("</TD>\n  <TD>");
radioButton(dsLdOut->string, ldOut, "white");
printf("</TD>\n  <TD>");
radioButton(dsLdOut->string, ldOut, "none");
printf("</TD>\n </TR>\n ");
printf("</TABLE>\n");

if (startsWith("hapmapLd", tdb->tableName))
    printf("<BR><B>Populations:</B>\n");
}

void landmarkUi (struct trackDb *tdb)
/* print the controls */
{
int i = 0; /* variable to walk through array */

printf("<BR /><B>Exclude region type</B><BR />");
for (i = 0; i < landmarkTypeSize; i++)
    {
    cartMakeCheckBox(cart, landmarkTypeString[i], FALSE);
    printf (" %s<BR />", landmarkTypeLabel[i]);
    }
}

void gvIdControls (struct trackDb *tdb)
/* print the controls for the label choice */
{
char varName[64];
boolean option = FALSE;

printf("<B>Label:</B> ");
safef(varName, sizeof(varName), "%s.label.hgvs", tdb->tableName);
option = cartUsualBoolean(cart, varName, FALSE);
cgiMakeCheckBox(varName, option);
/* need to change HGVS to reflect different species */
printf(" %s&nbsp;&nbsp;&nbsp;", "HGVS name");

safef(varName, sizeof(varName), "%s.label.common", tdb->tableName);
option = cartUsualBoolean(cart, varName, FALSE);
cgiMakeCheckBox(varName, option);
printf(" %s&nbsp;&nbsp;&nbsp;", "Common name");

safef(varName, sizeof(varName), "%s.label.dbid", tdb->tableName);
option = cartUsualBoolean(cart, varName, FALSE);
cgiMakeCheckBox(varName, option);
printf(" %s&nbsp;&nbsp;&nbsp;", "ID");
printf("<BR />\n");
}

void hgMutIdControls (struct trackDb *tdb) 
/* print the controls for the label choice */
{
char varName[64];
boolean option = FALSE;

printf("<B>Label:</B> ");
safef(varName, sizeof(varName), "%s.label.hgvs", tdb->tableName);
option = cartUsualBoolean(cart, varName, FALSE);
cgiMakeCheckBox(varName, option);
printf(" %s&nbsp;&nbsp;&nbsp;", "HGVS name");

safef(varName, sizeof(varName), "%s.label.common", tdb->tableName);
option = cartUsualBoolean(cart, varName, FALSE);
cgiMakeCheckBox(varName, option);
printf(" %s&nbsp;&nbsp;&nbsp;", "Common name");

safef(varName, sizeof(varName), "%s.label.dbid", tdb->tableName);
option = cartUsualBoolean(cart, varName, FALSE);
cgiMakeCheckBox(varName, option);
printf(" %s&nbsp;&nbsp;&nbsp;", "ID");
printf("<BR />\n");
}

void gvUi(struct trackDb *tdb)
/* print UI for human mutation filters */
{
int i = 0; /* variable to walk through arrays */

gvIdControls(tdb);
printf("<BR /><B>Exclude</B><BR />");
for (i = 0; i < gvAccuracySize; i++)
    {
    cartMakeCheckBox(cart, gvAccuracyString[i], FALSE);
    printf (" %s<BR />", gvAccuracyLabel[i]);
    }

printf("<BR /><B>Exclude mutation type</B><BR />");
for (i = 0; i < gvTypeSize; i++)
    {
    cartMakeCheckBox(cart, gvTypeString[i], FALSE);
    printf (" %s<BR />", gvTypeLabel[i]);
    }

printf("<BR /><B>Exclude mutation location</B><BR />");
for (i = 0; i < gvLocationSize; i++)
    {
    cartMakeCheckBox(cart, gvLocationString[i], FALSE);
    printf (" %s<BR />", gvLocationLabel[i]);
    }

/* exclude Swiss-Prot data by default, can be misleading */
printf("<BR /><B>Exclude data source</B><BR />");
for (i = 0; i < gvSrcSize; i++)
    {
    if (differentString(gvSrcDbValue[i], "LSDB"))
        {
        cartMakeCheckBox(cart, gvSrcString[i], TRUE);
        printf (" %s<BR />", gvSrcDbValue[i]); /* label with db value */
        }
    else 
        {
        cartMakeCheckBox(cart, gvSrcString[i], FALSE);
        printf (" Locus Specific Databases<BR />");
        }
    }

printf("<BR /><B>Color mutations by type</B><BR />");
for (i = 0; i < gvColorTypeSize; i++)
    {
    char *defaultVal = cartUsualString(cart, gvColorTypeStrings[i], gvColorTypeDefault[i]);
    printf (" %s ", gvColorTypeLabels[i]);
    cgiMakeDropList(gvColorTypeStrings[i], gvColorLabels, gvColorLabelSize, defaultVal);
    }
printf("<BR />");
}

void hgMutUi(struct trackDb *tdb)
/* print UI for human mutation filters */
{
int i = 0; /* variable to walk through arrays */
char **row;
struct sqlResult *sr;
struct sqlConnection *conn = hAllocConn();
char srcButton[128];

hgMutIdControls(tdb);
printf("<BR /><B>Exclude mutation type</B><BR />");
for (i = 0; i < variantTypeSize; i++)
    {
    cartMakeCheckBox(cart, variantTypeString[i], FALSE);
    printf (" %s<BR />", variantTypeLabel[i]);
    }

printf("<BR /><B>Exclude mutation location</B><BR />");
for (i = 0; i < variantLocationSize; i++)
    {
    cartMakeCheckBox(cart, variantLocationString[i], FALSE);
    printf (" %s<BR />", variantLocationLabel[i]);
    }

printf("<BR /><B>Exclude data source</B><BR />");
sr = sqlGetResult(conn, "select distinct(src) from hgMutSrc order by src");
while ((row = sqlNextRow(sr)) != NULL)
    {
    safef(srcButton, sizeof(srcButton), "hgMut.filter.src.%s", row[0]);
    if (differentString(row[0], "LSDB")) 
        {
        cartMakeCheckBox(cart, srcButton, TRUE);
        printf (" %s<BR />", row[0]);
        }
    else 
        {
        cartMakeCheckBox(cart, srcButton, FALSE);
        printf (" Locus Specific Databases<BR />");
        }

    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void humanPhenotypeUi(struct trackDb *tdb) 
/* print UI for human phenotype filters */
{
int i = 0; /* variable to walk through arrays */
printf("<BR /><B>Exclude mutation type</B><BR />");
for (i = 0; i < variantTypeSize; i++)
    {
    cartMakeCheckBox(cart, variantTypeString[i], FALSE);
    printf (" %s<BR />", variantTypeLabel[i]);
    }

printf("<BR /><B>Exclude mutation location</B><BR />");
for (i = 0; i < variantLocationSize; i++)
    {
    cartMakeCheckBox(cart, variantLocationString[i], FALSE);
    printf (" %s<BR />", variantLocationLabel[i]);
    }

/* print key for colors */
printf("<BR /><B>Color key (by mutation type)</B><BR />");
printf("substitution = purple<BR />");
printf("insertion = green<BR />");
printf("deletion = blue<BR />");
printf("duplication = orange<BR />");
printf("complex = red<BR />");
printf("unknown = black<BR />\n");
printf("Darker shades of the colors indicate that there is a link to clinical data available.<BR />\n");

printf("<BR /><B>Include subtracks</B>");
}

void retroposonsUi(struct trackDb *tdb) 
{
struct sqlConnection *conn = hAllocConn();
char query[256];
char **row;
struct sqlResult *sr;
struct slName *sList = NULL, *item;
int menuSize = 0;
char **menu;
int i;
char *tableList[3];

i = 0;
tableList[i++] = "dbRIP_Alu";
tableList[i++] = "dbRIP_L1";
tableList[i++] = "dbRIP_SVA";

safef(query, sizeof(query),
"SELECT genoRegion FROM dbRIP_Alu GROUP BY genoRegion ORDER BY genoRegion DESC");
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    slNameAddHead(&sList, row[0]);
    }
sqlFreeResult(&sr);

menuSize = slCount(sList) + 1;
menu = needMem((size_t)(menuSize * sizeof(char *)));

i = 0;
menu[i++] = GENO_REGION_DEFAULT;
for (item = sList; item != NULL; item = item->next)
    menu[i++] = item->name;

puts("<P><B>Add filters:</B><BR><BR>\n" );
puts("<B>Gene Context:</B>&nbsp;" );
cgiMakeDropList(GENO_REGION, menu, menuSize,
    cartCgiUsualString(cart, GENO_REGION, menu[0]));
slFreeList(&sList);
freez(&menu);

menuSize = 3;
menu = needMem((size_t)(menuSize * sizeof(char *)));
i = 0;
menu[i++] = POLY_SOURCE_DEFAULT;
menu[i++] = "yes";
menu[i++] = "no";

puts("<BR><BR>\n<B>Insertion found in reference sequence:</B>&nbsp;");
cgiMakeDropList(POLY_SOURCE, menu, menuSize,
    cartCgiUsualString(cart, POLY_SOURCE, menu[0]));
freez(&menu);

for (i = 0; i < 3; ++i)
    {
    safef(query, sizeof(query),
    "SELECT polySubfamily FROM %s GROUP BY polySubfamily ORDER BY polySubfamily DESC", tableList[i]);
    sr = sqlGetResult(conn, query);

    while ((row = sqlNextRow(sr)) != NULL)
	{
	slNameStore(&sList, row[0]);
	}
    sqlFreeResult(&sr);
    }
slNameSortCase(&sList);

menuSize = slCount(sList) + 1;
menu = needMem((size_t)(menuSize * sizeof(char *)));

i = 0;
menu[i++] = POLY_SUBFAMILY_DEFAULT;
for (item = sList; item != NULL; item = item->next)
    menu[i++] = item->name;

puts("<BR><BR>\n<B>Insertion identified in sub-family:</B>&nbsp;");
cgiMakeDropList(POLY_SUBFAMILY, menu, menuSize,
    cartCgiUsualString(cart, POLY_SUBFAMILY, menu[0]));
slFreeList(&sList);
freez(&menu);

safef(query, sizeof(query),
"SELECT ethnicGroup FROM polyGenotype GROUP BY ethnicGroup ORDER BY ethnicGroup DESC");
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
{
    slNameStore(&sList, row[0]);
    }
sqlFreeResult(&sr);

menuSize = slCount(sList) + 1;
menu = needMem((size_t)(menuSize * sizeof(char *)));

i = 0;
menu[i++] = ETHNIC_GROUP_DEFAULT;
for (item = sList; item != NULL; item = item->next)
    menu[i++] = item->name;

puts("<BR><BR>\n<B>Ethnic group:</B>&nbsp;");
cgiMakeDropList(ETHNIC_GROUP, menu, menuSize,
    cartCgiUsualString(cart, ETHNIC_GROUP, menu[0]));
slFreeList(&sList);
freez(&menu);
puts("&nbsp;");

radioButton(ETHNIC_GROUP_EXCINC,
	cartUsualString(cart, ETHNIC_GROUP_EXCINC, ETHNIC_NOT_DEFAULT),
	"include");
radioButton(ETHNIC_GROUP_EXCINC,
	cartUsualString(cart, ETHNIC_GROUP_EXCINC, ETHNIC_NOT_DEFAULT),
	"exclude");
puts("<B>this ethnic group</B><BR>\n");

menuSize = 10;
menu = needMem((size_t)(menuSize * sizeof(char *)));
i = 0;
menu[i++] = "0.0"; menu[i++] = "0.1"; menu[i++] = "0.2"; menu[i++] = "0.3";
menu[i++] = "0.4"; menu[i++] = "0.5"; menu[i++] = "0.6"; menu[i++] = "0.7";
menu[i++] = "0.8"; menu[i++] = "0.9";

/*	safety check on bad user input, they may have set them illegally
 *	in which case reset them to defaults 0.0 <= f <= 1.0
 */
double freqLow = sqlFloat(cartCgiUsualString(cart, ALLELE_FREQ_LOW, menu[0]));
double freqHi = sqlFloat(cartCgiUsualString(cart, ALLELE_FREQ_HI, menu[9]));

puts("<BR>\n<B>Restrict polymorphism frequency to:</B>&nbsp;");
if (freqLow < freqHi)
    cgiMakeDropList(ALLELE_FREQ_LOW, menu, menuSize,
	cartCgiUsualString(cart, ALLELE_FREQ_LOW, menu[0]));
else
    cgiMakeDropList(ALLELE_FREQ_LOW, menu, menuSize, menu[0]);


i = 0;
menu[i++] = "0.1"; menu[i++] = "0.2"; menu[i++] = "0.3"; menu[i++] = "0.4";
menu[i++] = "0.5"; menu[i++] = "0.6"; menu[i++] = "0.7"; menu[i++] = "0.8";
menu[i++] = "0.9"; menu[i++] = "1.0";

puts("&nbsp;&lt;=&nbsp;f&nbsp;&lt;=&nbsp;");
if (freqLow < freqHi)
    cgiMakeDropList(ALLELE_FREQ_HI, menu, menuSize,
	cartCgiUsualString(cart, ALLELE_FREQ_HI, menu[9]));
else
    cgiMakeDropList(ALLELE_FREQ_HI, menu, menuSize, menu[9]);
freez(&menu);

puts("\n</P>\n");
hFreeConn(&conn);
}

void cbrWabaUi(struct trackDb *tdb)
/* Put up UI cbrWaba. */
{
/*   This link is disabled in the external browser
char *cbrWabaFilter = cartUsualString(cart, "cbrWaba.filter", "red");
char *cbrWabaMap = cartUsualString(cart, "cbrWaba.type", fcoeEnumToString(0));
int start = cartInt(cart, "cbrWaba.start");
int end = cartInt(cart, "cbrWaba.end");
printf(
"<P><A HREF=\"http://genome-test.cse.ucsc.edu/cgi-bin/tracks.exe?where=%s%%3A%d-%d\"> Temporary Intronerator link: %s:%d-%d</A> <I>for testing purposes only</I> \n</P>", chromosome+3, start, end, chromosome+3, start, end );
*/
}

void fishClonesUi(struct trackDb *tdb)
/* Put up UI fishClones. */
{
char *fishClonesFilter = cartUsualString(cart, "fishClones.filter", "red");
char *fishClonesMap = cartUsualString(cart, "fishClones.type", fcoeEnumToString(0));
filterButtons("fishClones.filter", fishClonesFilter, TRUE);
printf(" ");
fcoeDropDown("fishClones.type", fishClonesMap);
}

void recombRateUi(struct trackDb *tdb)
/* Put up UI recombRate. */
{
char *recombRateMap = cartUsualString(cart, "recombRate.type", rroeEnumToString(0));
printf("<b>Map Distances: </b>");
rroeDropDown("recombRate.type", recombRateMap);
}

void recombRateRatUi(struct trackDb *tdb)
/* Put up UI recombRateRat. */
{
char *recombRateRatMap = cartUsualString(cart, "recombRateRat.type", rrroeEnumToString(0));
printf("<b>Map Distances: </b>");
rrroeDropDown("recombRateRat.type", recombRateRatMap);
}

void recombRateMouseUi(struct trackDb *tdb)
/* Put up UI recombRateMouse. */
{
char *recombRateMouseMap = cartUsualString(cart, "recombRateMouse.type", rrmoeEnumToString(0));
printf("<b>Map Distances: </b>");
rrmoeDropDown("recombRateMouse.type", recombRateMouseMap);
}

void cghNci60Ui(struct trackDb *tdb)
/* Put up UI cghNci60. */
{
char *cghNci60Map = cartUsualString(cart, "cghNci60.type", cghoeEnumToString(0));
char *col = cartUsualString(cart, "cghNci60.color", "gr");
printf(" <b>Cell Lines: </b> ");
cghoeDropDown("cghNci60.type", cghNci60Map);
printf(" ");
printf(" <b>Color Scheme</b>: ");
cgiMakeRadioButton("cghNci60.color", "gr", sameString(col, "gr"));
printf(" green/red ");
cgiMakeRadioButton("cghNci60.color", "rg", sameString(col, "rg"));
printf(" red/green ");
cgiMakeRadioButton("cghNci60.color", "rb", sameString(col, "rb"));
printf(" red/blue ");
}

void nci60Ui(struct trackDb *tdb)
/* put up UI for the nci60 track from stanford track */
{
char *nci60Map = cartUsualString(cart, "nci60.type", nci60EnumToString(0));
char *col = cartUsualString(cart, "exprssn.color", "rg");
printf("<p><b>Cell Lines: </b> ");
nci60DropDown("nci60.type", nci60Map);
printf(" ");
printf(" <b>Color Scheme</b>: ");
cgiMakeRadioButton("exprssn.color", "rg", sameString(col, "rg"));
printf(" red/green ");
cgiMakeRadioButton("exprssn.color", "rb", sameString(col, "rb"));
printf(" red/blue ");
}

void affyUi(struct trackDb *tdb)
/* put up UI for the affy track from stanford track */
{
char *affyMap;
char *col;
char varName[128];

safef(varName, sizeof(varName), "%s.%s", tdb->tableName, "type");
affyMap = cartUsualString(cart, varName, affyEnumToString(affyTissue));
col = cartUsualString(cart, "exprssn.color", "rg");
printf("<p><b>Experiment Display: </b> ");
affyDropDown(varName, affyMap);
printf(" <b>Color Scheme</b>: ");
cgiMakeRadioButton("exprssn.color", "rg", sameString(col, "rg"));
printf(" red/green ");
cgiMakeRadioButton("exprssn.color", "rb", sameString(col, "rb"));
printf(" red/blue ");
}

void affyAllExonUi(struct trackDb *tdb)
/* put up UI for the affy all exon tracks. */
{
char *affyAllExonMap;
char *col;
char varName[128];

safef(varName, sizeof(varName), "%s.%s", tdb->tableName, "type");
affyAllExonMap = cartUsualString(cart, varName, affyAllExonEnumToString(affyAllExonTissue));
col = cartUsualString(cart, "exprssn.color", "rg");
printf("<p><b>Experiment Display: </b> ");
affyAllExonDropDown(varName, affyAllExonMap);
printf(" <b>Color Scheme</b>: ");
cgiMakeRadioButton("exprssn.color", "rg", sameString(col, "rg"));
printf(" red/green ");
cgiMakeRadioButton("exprssn.color", "rb", sameString(col, "rb"));
printf(" red/blue ");
}

void rosettaUi(struct trackDb *tdb)
/* put up UI for the rosetta track */
{
char *rosettaMap = cartUsualString(cart, "rosetta.type", rosettaEnumToString(0));
char *col = cartUsualString(cart, "exprssn.color", "rg");
char *exonTypes = cartUsualString(cart, "rosetta.et",  rosettaExonEnumToString(0));

printf("<p><b>Reference Sample: </b> ");
rosettaDropDown("rosetta.type", rosettaMap);
printf("  ");
printf("<b>Exons Shown:</b> ");
rosettaExonDropDown("rosetta.et", exonTypes);
printf(" <b>Color Scheme</b>: ");
cgiMakeRadioButton("exprssn.color", "rg", sameString(col, "rg"));
printf(" red/green ");
cgiMakeRadioButton("exprssn.color", "rb", sameString(col, "rb"));
printf(" red/blue ");
}

void baseColorOptions(struct trackDb *tdb)
/*base coloring options*/
{
char *drawOption;
char *baseDrawDefault;
char baseColorVar[128];

printf("<p><b>Show sequence for&nbsp;</b>");
safef(baseColorVar, 128, "%s.%s", tdb->tableName, PSL_SEQUENCE_BASES );
baseDrawDefault = trackDbSettingOrDefault(tdb, PSL_SEQUENCE_BASES, PSL_SEQUENCE_DEFAULT);
drawOption = cartUsualString(cart, baseColorVar, baseDrawDefault);
baseColorDropDown(baseColorVar, drawOption);
printf("&nbsp;<b>bases.</b>\n");
printf("<BR><BR><a href=%s>Help on EST base labeling</a><br>",CDS_BASE_HELP_PAGE);
}

void nmdFilterOptions(struct trackDb *tdb)
/* Filter out NMD targets. */
{
char buff[256];
boolean nmdDefault = FALSE;
/* Skip if this track doesn't implement this filter. */
if(differentString(trackDbSettingOrDefault(tdb, "nmdFilter", "off"), "on"))
    return;
safef(buff, sizeof(buff), "hgt.%s.nmdFilter", tdb->tableName);
nmdDefault = cartUsualBoolean(cart, buff, FALSE);
printf("<p><b>Filter out NMD targets.</b>");
cgiMakeCheckBox(buff, nmdDefault);
}

void cdsColorOptions(struct trackDb *tdb, int numValues)
/*Codon coloring options*/
{
    char *drawOption;
    char *drawOptionsDefault;
    char *cdsDrawDefault;
    char cdsColorVar[128];

    boolean isGenePred = (numValues >= 0);
    
    if (isGenePred)
        drawOptionsDefault = "enabled";
    else
        drawOptionsDefault = "disabled";
    if(sameString(trackDbSettingOrDefault(tdb,
                  "cdsDrawOptions", drawOptionsDefault), "disabled"))
        return;
    
    printf("<p><b>Color track by codons: </b>");
    safef(cdsColorVar, 128, "%s.cds.draw", tdb->tableName );
    cdsDrawDefault = trackDbSettingOrDefault(tdb, "cdsDrawDefault", CDS_DRAW_DEFAULT);
    drawOption = cartUsualString(cart, cdsColorVar, cdsDrawDefault);
    cdsColorDropDown(cdsColorVar, drawOption, numValues);
    if(numValues > 0)
        printf("<BR><BR><a href=%s>Help on codon coloring</a><br>",CDS_HELP_PAGE);
    else
        printf("<BR><BR><a href=%s>Help on mRNA coloring</a><br>",CDS_MRNA_HELP_PAGE);
}

void blastSGUi(struct trackDb *tdb)
{
char geneName[64];
char accName[64];
char sprotName[64];
char posName[64];
char cModeStr[64];
boolean useGene, useAcc, useSprot, usePos;
int cMode;
char *cModes[3] = {"0", "1", "2"};

safef(geneName, sizeof(geneName), "%s.geneLabel", tdb->tableName);
safef(accName, sizeof(accName), "%s.accLabel", tdb->tableName);
safef(sprotName, sizeof(sprotName), "%s.sprotLabel", tdb->tableName);
safef(posName, sizeof(posName), "%s.posLabel", tdb->tableName);
useGene= cartUsualBoolean(cart, geneName, TRUE);
useAcc= cartUsualBoolean(cart, accName, FALSE);
useSprot= cartUsualBoolean(cart, sprotName, FALSE);
usePos= cartUsualBoolean(cart, posName, FALSE);

safef(cModeStr, sizeof(cModeStr), "%s.cmode", tdb->tableName);
cMode = cartUsualInt(cart, cModeStr, 0);

printf("<P><B>Color elements: </B> ");
cgiMakeRadioButton(cModeStr, cModes[0], cMode == 0);
printf("by score ");
cgiMakeRadioButton(cModeStr, cModes[1], cMode == 1);
printf("by yeast chromosome ");
cgiMakeRadioButton(cModeStr, cModes[2], cMode == 2);
printf("black");

printf("<P><B>Label elements by: </B> ");
cgiMakeCheckBox(geneName, useGene);
printf("Yeast Gene ");
cgiMakeCheckBox(accName, useAcc);
printf("Yeast mRNA ");
cgiMakeCheckBox(sprotName, useSprot);
printf("SwissProt ID ");
cgiMakeCheckBox(posName, usePos);
printf("Yeast Position");

cdsColorOptions(tdb, 2);
}
void blastFBUi(struct trackDb *tdb)
{
char geneName[64];
char accName[64];
char sprotName[64];
char posName[64];
char cModeStr[64];
boolean useGene, useAcc, usePos;
int cMode;
char *cModes[3] = {"0", "1", "2"};

safef(geneName, sizeof(geneName), "%s.geneLabel", tdb->tableName);
safef(accName, sizeof(accName), "%s.accLabel", tdb->tableName);
safef(sprotName, sizeof(sprotName), "%s.sprotLabel", tdb->tableName);
safef(posName, sizeof(posName), "%s.posLabel", tdb->tableName);
useGene= cartUsualBoolean(cart, geneName, TRUE);
useAcc= cartUsualBoolean(cart, accName, FALSE);
usePos= cartUsualBoolean(cart, posName, FALSE);

safef(cModeStr, sizeof(cModeStr), "%s.cmode", tdb->tableName);
cMode = cartUsualInt(cart, cModeStr, 0);

printf("<P><B>Color elements: </B> ");
cgiMakeRadioButton(cModeStr, cModes[0], cMode == 0);
printf("by score ");
cgiMakeRadioButton(cModeStr, cModes[1], cMode == 1);
printf("by D. mel. chromosome ");
cgiMakeRadioButton(cModeStr, cModes[2], cMode == 2);
printf("black");
printf("<P><B>Label elements by: </B> ");
cgiMakeCheckBox(geneName, useGene);
printf("FlyBase Gene ");
cgiMakeCheckBox(accName, useAcc);
printf("D. melanogaster mRNA ");
cgiMakeCheckBox(posName, usePos);
printf("D. melanogaster Position");

cdsColorOptions(tdb, 2);
}
void blastUi(struct trackDb *tdb)
{
char geneName[64];
char accName[64];
char sprotName[64];
char posName[64];
char cModeStr[64];
boolean useGene, useAcc, useSprot, usePos;
int cMode;
char *cModes[3] = {"0", "1", "2"};

safef(geneName, sizeof(geneName), "%s.geneLabel", tdb->tableName);
safef(accName, sizeof(accName), "%s.accLabel", tdb->tableName);
safef(sprotName, sizeof(sprotName), "%s.sprotLabel", tdb->tableName);
safef(posName, sizeof(posName), "%s.posLabel", tdb->tableName);
useGene= cartUsualBoolean(cart, geneName, TRUE);
useAcc= cartUsualBoolean(cart, accName, FALSE);
useSprot= cartUsualBoolean(cart, sprotName, FALSE);
usePos= cartUsualBoolean(cart, posName, FALSE);

safef(cModeStr, sizeof(cModeStr), "%s.cmode", tdb->tableName);
cMode = cartUsualInt(cart, cModeStr, 0);

printf("<P><B>Color elements: </B> ");
cgiMakeRadioButton(cModeStr, cModes[0], cMode == 0);
printf("by score ");
cgiMakeRadioButton(cModeStr, cModes[1], cMode == 1);
printf("by human chromosome ");
cgiMakeRadioButton(cModeStr, cModes[2], cMode == 2);
printf("black");

printf("<P><B>Label elements by: </B> ");
cgiMakeCheckBox(geneName, useGene);
printf("Human Gene ");
cgiMakeCheckBox(accName, useAcc);
printf("Human mRNA ");
cgiMakeCheckBox(sprotName, useSprot);
printf("UniProt(Swiss-Prot/TrEMBL) ID ");
cgiMakeCheckBox(posName, usePos);
printf("Human Position");

if (startsWith("mrna", tdb->tableName))
    cdsColorOptions(tdb, 5);
else
    cdsColorOptions(tdb, 2);
}

void hg17KgIdConfig(struct trackDb *tdb)
/* Put up gene ID track controls */
{
char varName[64];
char *geneLabel;
safef(varName, sizeof(varName), "%s.label", tdb->tableName);
geneLabel = cartUsualString(cart, varName, "gene symbol");
printf("<B>Label:</B> ");
radioButton(varName, geneLabel, "gene symbol");
radioButton(varName, geneLabel, "UCSC Known Gene ID");
radioButton(varName, geneLabel, "all");
radioButton(varName, geneLabel, "none");
}

void hg17KgUI(struct trackDb *tdb)
/* Put up refGene-specifc controls */
{
hg17KgIdConfig(tdb);
cdsColorOptions(tdb, 2);
}

void knownGeneIdConfig(struct trackDb *tdb)
/* Put up gene ID track controls */
{
char varName[64];
struct sqlConnection *conn = hAllocConn();
char query[256];
char *omimAvail = NULL;
boolean option = FALSE;
safef(query, sizeof(query), "select kgXref.kgID from kgXref,refLink where kgXref.refseq = refLink.mrnaAcc and refLink.omimId != 0 limit 1");
omimAvail = sqlQuickString(conn, query);
hFreeConn(&conn);

printf("<B>Label:</B> ");
safef(varName, sizeof(varName), "%s.label.gene", tdb->tableName);
option = cartUsualBoolean(cart, varName, FALSE);
cgiMakeCheckBox(varName, option);
printf(" %s&nbsp;&nbsp;&nbsp;", "gene symbol");

safef(varName, sizeof(varName), "%s.label.kgId", tdb->tableName);
option = cartUsualBoolean(cart, varName, FALSE);
cgiMakeCheckBox(varName, option);
printf(" %s&nbsp;&nbsp;&nbsp;", "UCSC Known Gene ID");

safef(varName, sizeof(varName), "%s.label.prot", tdb->tableName);
option = cartUsualBoolean(cart, varName, FALSE);
cgiMakeCheckBox(varName, option);
printf(" %s&nbsp;&nbsp;&nbsp;", "UniProt Display ID");

if (omimAvail != NULL)
    {
    safef(varName, sizeof(varName), "%s.label.omim%s", tdb->tableName, cartString(cart, "db"));
    option = cartUsualBoolean(cart, varName, FALSE);
    cgiMakeCheckBox(varName, option);
    printf(" %s&nbsp;&nbsp;&nbsp;", "OMIM ID");
    }
}

void knownGeneUI(struct trackDb *tdb)
/* Put up refGene-specifc controls */
{
/* This is incompatible with adding Protein ID to lf->extra */
knownGeneIdConfig(tdb); 
cdsColorOptions(tdb, 2);
}

void geneIdConfig(struct trackDb *tdb)
/* Put up gene ID track controls */
{
char varName[64];
char *geneLabel;
struct sqlConnection *conn = hAllocConn();
int omimAvail = 0;
char query[128];
if (sameString(tdb->tableName, "refGene"))
    {
    safef(query, sizeof(query), "select refLink.omimId from refLink, refGene where refLink.mrnaAcc = refGene.name and refLink.omimId != 0 limit 1");
    omimAvail = sqlQuickNum(conn, query);
    }
hFreeConn(&conn);

safef(varName, sizeof(varName), "%s.label", tdb->tableName);
geneLabel = cartUsualString(cart, varName, "gene");
printf("<B>Label:</B> ");
radioButton(varName, geneLabel, "gene");
radioButton(varName, geneLabel, "accession");
if (omimAvail != 0) 
    {
    radioButton(varName, geneLabel, "OMIM ID");
    radioButton(varName, geneLabel, "all");
    }
else 
    radioButton(varName, geneLabel, "both");
radioButton(varName, geneLabel, "none");
}

void geneIdConfig2(struct trackDb *tdb)
/* Put up gene ID track controls, with checkboxes */
{
char varName[64];
boolean option;
struct sqlConnection *conn = hAllocConn();
int omimAvail = 0;
char query[128];
if (sameString(tdb->tableName, "refGene"))
    {
    safef(query, sizeof(query), "select refLink.omimId from refLink, refGene where refLink.mrnaAcc = refGene.name and refLink.omimId != 0 limit 1");
    omimAvail = sqlQuickNum(conn, query);
    }
hFreeConn(&conn);

printf("<B>Label:</B> ");
safef(varName, sizeof(varName), "%s.label.gene", tdb->tableName);
option = cartUsualBoolean(cart, varName, FALSE);
cgiMakeCheckBox(varName, option);
printf(" %s&nbsp;&nbsp;&nbsp;", "gene");

safef(varName, sizeof(varName), "%s.label.acc", tdb->tableName);
option = cartUsualBoolean(cart, varName, FALSE);
cgiMakeCheckBox(varName, option);
printf(" %s&nbsp;&nbsp;&nbsp;", "accession");

if (omimAvail != 0)
    {
    safef(varName, sizeof(varName), "%s.label.omim%s", tdb->tableName, cartString(cart, "db"));
    option = cartUsualBoolean(cart, varName, FALSE);
    cgiMakeCheckBox(varName, option);
    printf(" %s&nbsp;&nbsp;&nbsp;", "OMIM ID");
    }
}

void refGeneUI(struct trackDb *tdb)
/* Put up refGene-specifc controls */
{
geneIdConfig2(tdb);
cdsColorOptions(tdb, 2);
}

void retroGeneUI(struct trackDb *tdb)
/* Put up retroGene-specifc controls */
{
geneIdConfig(tdb);
cdsColorOptions(tdb, 2);
}

void gencodeUI(struct trackDb *tdb)
/* Put up gencode-specifc controls */
{
geneIdConfig(tdb);
}

void oneMrnaFilterUi(struct controlGrid *cg, char *text, char *var)
/* Print out user interface for one type of mrna filter. */
{
controlGridStartCell(cg);
printf("%s:<BR>", text);
cgiMakeTextVar(var, cartUsualString(cart, var, ""), 19);
controlGridEndCell(cg);
}

void mrnaUi(struct trackDb *tdb, boolean isXeno)
/* Put up UI for an mRNA (or EST) track. */
{
struct mrnaUiData *mud = newMrnaUiData(tdb->tableName, isXeno);
struct mrnaFilter *fil;
struct controlGrid *cg = NULL;
char *filterTypeVar = mud->filterTypeVar;
char *filterTypeVal = cartUsualString(cart, filterTypeVar, "red");
char *logicTypeVar = mud->logicTypeVar;
char *logicTypeVal = cartUsualString(cart, logicTypeVar, "and");
boolean pslSequenceBases = (cartVarExists(cart, PSL_SEQUENCE_BASES) ||
    ((char *) NULL != trackDbSetting(tdb, PSL_SEQUENCE_BASES)));

/* Define type of filter. */
filterButtons(filterTypeVar, filterTypeVal, FALSE);
printf("  <B>Combination Logic:</B> ");
radioButton(logicTypeVar, logicTypeVal, "and");
radioButton(logicTypeVar, logicTypeVal, "or");
printf("<BR>\n");

/* List various fields you can filter on. */
printf("<table border=0 cellspacing=1 cellpadding=1 width=%d>\n", CONTROL_TABLE_WIDTH);
cg = startControlGrid(4, NULL);
for (fil = mud->filterList; fil != NULL; fil = fil->next)
     oneMrnaFilterUi(cg, fil->label, fil->key);
endControlGrid(&cg);
if (pslSequenceBases)
    baseColorOptions(tdb);
else
    cdsColorOptions(tdb, -1);
}

void bedUi(struct trackDb *tdb)
/* Put up UI for an mRNA (or EST) track. */
{
struct mrnaUiData *mud = newBedUiData(tdb->tableName);
struct mrnaFilter *fil;
struct controlGrid *cg = NULL;
char *filterTypeVar = mud->filterTypeVar;
char *filterTypeVal = cartUsualString(cart, filterTypeVar, "red");
char *logicTypeVar = mud->logicTypeVar;
char *logicTypeVal = cartUsualString(cart, logicTypeVar, "and");

/* Define type of filter. */
filterButtons(filterTypeVar, filterTypeVal, FALSE);
printf("  <B>Combination Logic:</B> ");
radioButton(logicTypeVar, logicTypeVal, "and");
radioButton(logicTypeVar, logicTypeVal, "or");
printf("<BR>\n");

/* List various fields you can filter on. */
printf("<table border=0 cellspacing=1 cellpadding=1 width=%d>\n", CONTROL_TABLE_WIDTH);
cg = startControlGrid(4, NULL);
for (fil = mud->filterList; fil != NULL; fil = fil->next)
     oneMrnaFilterUi(cg, fil->label, fil->key);
endControlGrid(&cg);
}

static void filterByChrom(struct trackDb *tdb)
{
char *filterSetting;
char filterVar[256];
char *filterVal = "";

printf("<p><b>Filter by chromosome (eg. chr10):</b> ");
snprintf(filterVar, sizeof(filterVar), "%s.chromFilter", tdb->tableName);
filterSetting = cartUsualString(cart, filterVar, filterVal);
cgiMakeTextVar(filterVar, cartUsualString(cart, filterVar, ""), 5);
}

void scoreUi(struct trackDb *tdb, int maxScore)
/* Put up UI for filtering bed track based on a score */
{
char option[256];

char *scoreValString = trackDbSetting(tdb, "scoreFilter");
int scoreSetting;
int scoreVal = 0;
char tempScore[256];

/* initial value of score theshold is 0, unless
 * overridden by the scoreFilter setting in the track */
if (scoreValString != NULL)
    scoreVal = atoi(scoreValString);
printf("<p><b>Show only items with unnormalized score at or above:</b> ");
snprintf(option, sizeof(option), "%s.scoreFilter", tdb->tableName);
scoreSetting = cartUsualInt(cart,  option,  scoreVal);
safef(tempScore, sizeof(tempScore), "%d",scoreSetting);
cgiMakeTextVar( option, tempScore, 11);
printf("&nbsp;&nbsp;(range: 0&nbsp;to&nbsp;%d)", maxScore);
}

void crossSpeciesUi(struct trackDb *tdb)
/* Put up UI for selecting rainbow chromosome color or intensity score. */
{
char colorVar[256];
char *colorSetting;
/* initial value of chromosome coloring option is "on", unless
 * overridden by the colorChromDefault setting in the track */
char *colorDefault = trackDbSettingOrDefault(tdb, "colorChromDefault", "on");

printf("<p><b>Color track based on chromosome:</b> ");
snprintf(colorVar, sizeof(colorVar), "%s.color", tdb->tableName);
colorSetting = cartUsualString(cart, colorVar, colorDefault);
cgiMakeRadioButton(colorVar, "on", sameString(colorSetting, "on"));
printf(" on ");
cgiMakeRadioButton(colorVar, "off", sameString(colorSetting, "off"));
printf(" off ");
printf("<br><br>");
filterByChrom(tdb);
}

void transRegCodeUi(struct trackDb *tdb)
/* Put up UI for transcriptional regulatory code - not
 * much more than score UI. */
{
scoreUi(tdb, 1000);
printf("%s",
	"<P>The scoring ranges from 0 to 1000 and is based on the number of lines "
	"of evidence that support the motif being active.  Each of the two "
	"<I>sensu stricto</I> species in which the motif was conserved counts "
	"as a line of evidence.  If the CHIP/CHIP data showed good (P 0.001) "
	"evidence of binding to the transcription factor associated with the "
	"motif, that counts as two lines of evidence.  If the CHIP/CHIP data "
	"showed weaker (P 0.005) evidence of binding, that counts as just one line "
	"of evidence.  The following table shows the relationship between lines "
	"of evidence and score:");
printf("<P>");
hTableStart();
printf("%s",
   "<BLOCKQUOTE>\n"
   "<TR><TH>Evidence</TH><TH>Score</TH></TR>\n"
   "<TR><TD>4</TD><TD>1000</TD></TR>\n"
   "<TR><TD>3</TD><TD>500</TD></TR>\n"
   "<TR><TD>2</TD><TD>333</TD></TR>\n"
   "<TR><TD>1</TD><TD>250</TD></TR>\n"
   "<TR><TD>0</TD><TD>200</TD></TR>\n"
   "</BLOCKQUOTE>\n"
   );
hTableEnd();
}


void zooWiggleUi(struct trackDb *tdb )
/* put up UI for zoo track with one species on each line
 * and checkboxes to toggle each of them on/off*/
{
char options[7][256];
int thisHeightPer, thisLineGap;
float thisMinYRange, thisMaxYRange;
char *interpolate, *fill;

char **row;
int rowOffset;
struct sample *sample;
struct sqlResult *sr;

char option[64];
struct sqlConnection *conn = hAllocConn();

char newRow = 0;

snprintf( &options[0][0], 256, "%s.heightPer", tdb->tableName );
snprintf( &options[1][0], 256, "%s.linear.interp", tdb->tableName );
snprintf( &options[3][0], 256, "%s.fill", tdb->tableName );
snprintf( &options[4][0], 256, "%s.min.cutoff", tdb->tableName );
snprintf( &options[5][0], 256, "%s.max.cutoff", tdb->tableName );
snprintf( &options[6][0], 256, "%s.interp.gap", tdb->tableName );

thisHeightPer = atoi(cartUsualString(cart, &options[0][0], "50"));
interpolate = cartUsualString(cart, &options[1][0], "Linear Interpolation");
fill = cartUsualString(cart, &options[3][0], "1");
thisMinYRange = atof(cartUsualString(cart, &options[4][0], "0.0"));
thisMaxYRange = atof(cartUsualString(cart, &options[5][0], "1000.0"));
thisLineGap = atoi(cartUsualString(cart, &options[6][0], "200"));

printf("<p><b>Interpolation: </b> ");
wiggleDropDown(&options[1][0], interpolate );
printf(" ");

printf("<br><br>");
printf(" <b>Fill Blocks</b>: ");
cgiMakeRadioButton(&options[3][0], "1", sameString(fill, "1"));
printf(" on ");

cgiMakeRadioButton(&options[3][0], "0", sameString(fill, "0"));
printf(" off ");

printf("<p><b>Track Height</b>:&nbsp;&nbsp;");
cgiMakeIntVar(&options[0][0], thisHeightPer, 5 );
printf("&nbsp;pixels");

printf("<p><b>Vertical Range</b>:&nbsp;&nbsp;\nmin:");
cgiMakeDoubleVar(&options[4][0], thisMinYRange, 6 );
printf("&nbsp;&nbsp;&nbsp;&nbsp;max:");
cgiMakeDoubleVar(&options[5][0], thisMaxYRange, 6 );

printf("<p><b>Toggle Species on/off</b><br>" );
sr = hRangeQuery(conn, tdb->tableName, chromosome, 0, 1877426, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    sample = sampleLoad(row + rowOffset);
    snprintf( option, sizeof(option), "zooSpecies.%s", sample->name );
    if( cartUsualBoolean(cart, option, TRUE ) )
	cgiMakeCheckBox(option, TRUE );
    else 
	cgiMakeCheckBox(option, FALSE );
    printf("%s&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;", sample->name );

    newRow++;
    if( newRow % 5 == 0 ) printf("<br>");
		    
    sampleFree(&sample);
    }

}

void chainColorUi(struct trackDb *tdb)
/* UI for the chain tracks */
{
if (chainDbNormScoreAvailable(chromosome, tdb->tableName, NULL))
    {
    char options[1][256];	/*	our option strings here	*/
    char *colorOpt;
    (void) chainFetchColorOption(tdb, &colorOpt);
    snprintf( &options[0][0], 256, "%s.%s", tdb->tableName, OPT_CHROM_COLORS );
    printf("<p><b>Color chains by:&nbsp;</b>");
    chainColorDropDown(&options[0][0], colorOpt);

    freeMem (colorOpt);
    filterByChrom(tdb);
    scoreUi(tdb, 2000000000);
    }
else
    crossSpeciesUi(tdb);

}

#define NET_OPT_TOP  "netTopOnly"

void netUi(struct trackDb *tdb)
/* UI for net track */
{
char option[64];
safef(option, sizeof option, "%s.%s", tdb->tableName, NET_OPT_TOP);
puts("<P>");
cgiMakeCheckBox(option, cartUsualBoolean(cart, option, FALSE));
puts("<B>&nbsp; Show top-level only</B>");
}

void wigUi(struct trackDb *tdb)
/* UI for the wiggle track */
{
char *typeLine = NULL;	/*	to parse the trackDb type line	*/
char *words[8];		/*	to parse the trackDb type line	*/
int wordCount = 0;	/*	to parse the trackDb type line	*/
char options[14][256];	/*	our option strings here	*/
double minY;		/*	from trackDb or cart	*/
double maxY;		/*	from trackDb or cart	*/
double tDbMinY;		/*	data range limits from trackDb type line */
double tDbMaxY;		/*	data range limits from trackDb type line */
int defaultHeight;	/*	pixels per item	*/
char *horizontalGrid = NULL;	/*	Grid lines, off by default */
char *lineBar;	/*	Line or Bar graph */
char *autoScale;	/*	Auto scaling on or off */
char *windowingFunction;	/*	Maximum, Mean, or Minimum */
char *smoothingWindow;	/*	OFF or [2:16] */
char *yLineMarkOnOff;	/*	user defined Y marker line to draw */
double yLineMark;		/*	from trackDb or cart	*/
int maxHeightPixels = atoi(DEFAULT_HEIGHT_PER);
int minHeightPixels = MIN_HEIGHT_PER;
boolean bedGraph = FALSE;	/*	working on bedGraph type ? */

typeLine = cloneString(tdb->type);
wordCount = chopLine(typeLine,words);

if (sameString(words[0],"bedGraph"))
    bedGraph = TRUE;

snprintf( &options[0][0], 256, "%s.%s", tdb->tableName, HEIGHTPER );
snprintf( &options[4][0], 256, "%s.%s", tdb->tableName, MIN_Y );
snprintf( &options[5][0], 256, "%s.%s", tdb->tableName, MAX_Y );
snprintf( &options[7][0], 256, "%s.%s", tdb->tableName, HORIZGRID );
snprintf( &options[8][0], 256, "%s.%s", tdb->tableName, LINEBAR );
snprintf( &options[9][0], 256, "%s.%s", tdb->tableName, AUTOSCALE );
snprintf( &options[10][0], 256, "%s.%s", tdb->tableName, WINDOWINGFUNCTION );
snprintf( &options[11][0], 256, "%s.%s", tdb->tableName, SMOOTHINGWINDOW );
snprintf( &options[12][0], 256, "%s.%s", tdb->tableName, YLINEMARK );
snprintf( &options[13][0], 256, "%s.%s", tdb->tableName, YLINEONOFF );

wigFetchMinMaxPixels(tdb, &minHeightPixels, &maxHeightPixels, &defaultHeight);
if (bedGraph)
    wigFetchMinMaxLimits(tdb, &minY, &maxY, &tDbMinY, &tDbMaxY);
else
    wigFetchMinMaxY(tdb, &minY, &maxY, &tDbMinY, &tDbMaxY, wordCount, words);
(void) wigFetchHorizontalGrid(tdb, &horizontalGrid);
(void) wigFetchAutoScale(tdb, &autoScale);
(void) wigFetchGraphType(tdb, &lineBar);
(void) wigFetchWindowingFunction(tdb, &windowingFunction);
(void) wigFetchSmoothingWindow(tdb, &smoothingWindow);
(void) wigFetchYLineMark(tdb, &yLineMarkOnOff);
wigFetchYLineMarkValue(tdb, &yLineMark);

printf("<TABLE BORDER=0><TR><TD ALIGN=LEFT>\n");

if (bedGraph)
printf("<b>Type of graph:&nbsp;</b>");
else
printf("<b>Type of graph:&nbsp;</b>");
wiggleGraphDropDown(&options[8][0], lineBar);
printf("</TD></TR><TR><TD ALIGN=LEFT COLSPAN=2>\n");

printf("<b>y&nbsp;=&nbsp;0.0 line:&nbsp;</b>");
wiggleGridDropDown(&options[7][0], horizontalGrid);
printf(" </TD></TR><TR><TD ALIGN=LEFT COLSPAN=2>\n");

printf("<b>Track height:&nbsp;</b>");
cgiMakeIntVar(&options[0][0], defaultHeight, 5);
printf("&nbsp;pixels&nbsp;&nbsp;(range:&nbsp;%d&nbsp;to&nbsp;%d)",
	minHeightPixels, maxHeightPixels);
printf("</TD></TR><TR><TD ALIGN=LEFT COLSPAN=2>\n");

printf("<b>Vertical viewing range</b>:&nbsp;&nbsp;\n<b>min:&nbsp;</b>");
cgiMakeDoubleVar(&options[4][0], minY, 6);
printf("&nbsp;&nbsp;&nbsp;&nbsp;<b>max:&nbsp;</b>");
cgiMakeDoubleVar(&options[5][0], maxY, 6);
printf("\n&nbsp; &nbsp;(range: &nbsp;%g&nbsp;to&nbsp;%g)",
    tDbMinY, tDbMaxY);
printf("</TD></TR><TR><TD ALIGN=LEFT COLSPAN=2>\n");

printf("<b>Data view scaling:&nbsp;</b>");
wiggleScaleDropDown(&options[9][0], autoScale);
printf("</TD></TR><TR><TD ALIGN=LEFT COLSPAN=2>\n");

printf("<b>Windowing function:&nbsp;</b>");
wiggleWindowingDropDown(&options[10][0], windowingFunction);
printf("</TD></TR><TR><TD ALIGN=LEFT COLSPAN=2>\n");

printf("<b>Smoothing window:&nbsp;</b>");
wiggleSmoothingDropDown(&options[11][0], smoothingWindow);
printf("&nbsp;pixels");
printf("</TD></TR><TR><TD ALIGN=LEFT COLSPAN=2>\n");

printf("<b>Draw indicator line at y&nbsp;=&nbsp;</b>&nbsp;\n");
cgiMakeDoubleVar(&options[12][0], yLineMark, 6);
printf("&nbsp;&nbsp;");
wiggleYLineMarkDropDown(&options[13][0], yLineMarkOnOff);
printf("</TD></TR></TABLE>\n");
printf("<BR><A HREF=\"%s\" TARGET=_blank>Graph configuration help</A>",
	WIGGLE_HELP_PAGE);

freeMem(typeLine);

}

void chromGraphUi(struct trackDb *tdb)
/* UI for the wiggle track */
{
char varName[chromGraphVarNameMaxSize];
struct sqlConnection *conn = hAllocConn();
char *track = tdb->tableName;
double minVal,maxVal;
struct chromGraphSettings *cgs = chromGraphSettingsGet(track,
	conn, tdb, cart);

printf("<b>Track height:&nbsp;</b>");
chromGraphVarName(track, "pixels", varName);
cgiMakeIntVar(varName, cgs->pixels, 3);
printf("&nbsp;pixels&nbsp;&nbsp;(range:&nbsp;%d&nbsp;to&nbsp;%d)<BR>",
	cgs->minPixels, cgs->maxPixels);

printf("<b>Vertical viewing range</b>:&nbsp;&nbsp;\n<b>min:&nbsp;</b>");
chromGraphVarName(track, "minVal", varName);
cgiMakeDoubleVar(varName, cgs->minVal, 6);
printf("&nbsp;&nbsp;&nbsp;&nbsp;<b>max:&nbsp;</b>");
chromGraphVarName(track, "maxVal", varName);
cgiMakeDoubleVar(varName, cgs->maxVal, 6);
chromGraphDataRange(track, conn, &minVal, &maxVal);
printf("\n&nbsp; &nbsp;(range: &nbsp;%g&nbsp;to&nbsp;%g)<BR>",
    minVal, maxVal);

hFreeConn(&conn);
}

void rulerUi(struct trackDb *tdb)
/* UI for base position (ruler) */
{
boolean complementsToo = cartUsualBoolean(cart, MOTIF_COMPLEMENT, FALSE);
boolean showPos = cartUsualBoolean(cart, BASE_SHOWPOS, FALSE);
boolean showAsm = cartUsualBoolean(cart, BASE_SHOWASM, FALSE);
/* title var is assembly-specific */
char titleVar[256];
char *title = NULL;
/* Configure zoom when click occurs */
char *currentZoom = cartCgiUsualString(cart, RULER_BASE_ZOOM_VAR, ZOOM_3X);
char *motifString = cartCgiUsualString(cart, BASE_MOTIFS, "");
safef(titleVar,sizeof(titleVar),"%s_%s",BASE_TITLE,database);
title = cartUsualString(cart, titleVar, "");
puts("<P><B>Zoom in:&nbsp;</B>");
zoomRadioButtons(RULER_BASE_ZOOM_VAR, currentZoom);
puts("<P><B>Motifs to highlight:&nbsp;</B>");
cgiMakeTextVar(BASE_MOTIFS, motifString, 20);
puts("&nbsp;(Comma separated list, i.e.: GT,AG for splice sites)");
puts("<P>");
cgiMakeCheckBox(MOTIF_COMPLEMENT, complementsToo);
puts("&nbsp;<B>Show reverse complements of motifs also</B>");
puts("<P>Options for slides and presentations:");
puts("<P><B>Title:&nbsp;</B>");
cgiMakeTextVar(titleVar, title, 30);
puts("<P><B>Display:&nbsp;</B>");
cgiMakeCheckBox(BASE_SHOWASM, showAsm);
puts("&nbsp;<B>assembly&nbsp;</B>");
cgiMakeCheckBox(BASE_SHOWPOS, showPos);
puts("&nbsp;<B>position</B>");

}

void oligoMatchUi(struct trackDb *tdb)
/* UI for oligo match track */
{
char *oligo = cartUsualString(cart, oligoMatchVar, oligoMatchDefault);
puts("<P><B>Short (2-30 base) sequence:</B>");
cgiMakeTextVar(oligoMatchVar, oligo, 45);
}

void cutterUi(struct trackDb *tdb)
/* UI for restriction enzyme track */
{
char *enz = cartUsualString(cart, cutterVar, cutterDefault);
puts("<P><B>Enzymes (separate with commas):</B><BR>");
cgiMakeTextVar(cutterVar, enz, 100);
}

struct wigMafSpecies 
    {
    struct wigMafSpecies *next;
    char *name;
    int group;
    };

void wigMafUi(struct trackDb *tdb)
/* UI for maf/wiggle track
 * NOTE: calls wigUi */
{
char *defaultCodonSpecies = trackDbSetting(tdb, SPECIES_CODON_DEFAULT);
char *speciesTarget = trackDbSetting(tdb, SPECIES_TARGET_VAR);
char *speciesTree = trackDbSetting(tdb, SPECIES_TREE_VAR);
char *speciesOrder = trackDbSetting(tdb, SPECIES_ORDER_VAR);
char *speciesGroup = trackDbSetting(tdb, SPECIES_GROUP_VAR);
char *framesTable = trackDbSetting(tdb, "frames");
char *species[100];
char *groups[20];
char sGroup[24];
char *treeImage = NULL;
struct wigMafSpecies *wmSpecies, *wmSpeciesList = NULL;
int group, prevGroup;
int speciesCt = 0, groupCt = 1;
int i, j;
char option[64];
int fieldCt;
char *fields[20];
struct phyloTree *tree;
char *consNames[10];
char *consWiggle = trackDbSetting(tdb, CONS_WIGGLE);

puts("<TABLE><TR><TD>");

/* check for alternate conservation wiggles -- create dropbox */
if (consWiggle)
    {
    safef(option, sizeof(option), "%s.%s", tdb->tableName, CONS_WIGGLE);
    fieldCt = chopLine(consWiggle, fields);
    if (fieldCt > 2)
        {
        puts("<P><B>Conservation:</B>" );
        for (i=1, j=0; i < fieldCt; i+=2, j++)
            consNames[j] = fields[i];
        cgiMakeDropList(option, consNames, fieldCt/2,
            cartCgiUsualString(cart, option, consNames[0]));
        }
    }

/* determine species and groups for pairwise -- create checkboxes */
if (speciesOrder == NULL && speciesGroup == NULL)
    errAbort(
      "Track %s missing required trackDb setting: speciesOrder or speciesGroup",
                tdb->tableName);
if (speciesGroup)
    groupCt = chopLine(speciesGroup, groups);

for (group = 0; group < groupCt; group++)
    {
    if (groupCt != 1 || !speciesOrder)
        {
        safef(sGroup, sizeof sGroup, "%s%s", 
                                SPECIES_GROUP_PREFIX, groups[group]);
        speciesOrder = trackDbRequiredSetting(tdb, sGroup);
        }
    speciesCt = chopLine(speciesOrder, species);
    for (i = 0; i < speciesCt; i++)
        {
        AllocVar(wmSpecies);
        wmSpecies->name = cloneString(species[i]);
        wmSpecies->group = group;
        slAddHead(&wmSpeciesList, wmSpecies);
        }
    }
slReverse(&wmSpeciesList);

//#define CODON_HIGHLIGHT
#ifdef CODON_HIGHLIGHT
char *currentCodonMode;
#endif


puts("<P><B>Pairwise alignments:</B><BR>" );

if ((speciesTree != NULL) && ((tree = phyloParseString(speciesTree)) != NULL))
{
    char buffer[128];
    char *nodeNames[512];
    int numNodes = 0;
    char *path, *orgName;
    int ii;

    safef(buffer, sizeof(buffer), "%s.vis",tdb->tableName);
    cartMakeRadioButton(cart, buffer,"useTarg", "useTarg");
    printf("Show shortest path to target species:  ");
    path = phyloNodeNames(tree);
    numNodes = chopLine(path, nodeNames);
    for(ii=0; ii < numNodes; ii++)
	{
	if ((orgName = hOrganism(nodeNames[ii])) != NULL)
	    nodeNames[ii] = orgName;
	nodeNames[ii][0] = toupper(nodeNames[ii][0]);
	}

    cgiMakeDropList(SPECIES_HTML_TARGET, nodeNames, numNodes,
	cartUsualString(cart, SPECIES_HTML_TARGET, speciesTarget));
    puts("<br>");
    cartMakeRadioButton(cart,buffer,"useCheck", "useTarg");
    printf("Show all species checked : ");
}

if (groupCt == 1)
    puts("<TABLE><TR>");
group = -1;
for (wmSpecies = wmSpeciesList, i = 0; wmSpecies != NULL; 
		    wmSpecies = wmSpecies->next, i++)
    {
    char *label;
    prevGroup = group;
    group = wmSpecies->group;
    if (groupCt != 1 && group != prevGroup)
	{
	i = 0;
	if (group != 0)
	    puts("</TR></TABLE>");
	printf("<P>&nbsp;&nbsp;<B><EM>%s</EM></B>", groups[group]);
	puts("<TABLE><TR>");
	}
    if (i != 0 && (i % 5) == 0)
	puts("</TR><TR>");
    puts("<TD>");
    safef(option, sizeof(option), "%s.%s", tdb->tableName, wmSpecies->name);
    cgiMakeCheckBox(option, cartUsualBoolean(cart, option, TRUE));
    label = hOrganism(wmSpecies->name);
    if (label == NULL)
	label = wmSpecies->name;
    *label = tolower(*label);
    printf ("%s<BR>", label);
    puts("</TD>");
    }
puts("</TR></TABLE><BR>");

puts("<P><B>Multiple alignment base-level:</B><BR>" );
safef(option, sizeof option, "%s.%s", tdb->tableName, MAF_DOT_VAR);
cgiMakeCheckBox(option, cartCgiUsualBoolean(cart, option, FALSE));
puts("Display bases identical to reference as dots<BR>" );
safef(option, sizeof option, "%s.%s", tdb->tableName, MAF_CHAIN_VAR);
cgiMakeCheckBox(option, cartCgiUsualBoolean(cart, option, TRUE));
if (trackDbSetting(tdb, "irows") != NULL)
    puts("Display chains between alignments<BR>");
else
    puts("Display unaligned bases with spanning chain as 'o's<BR>");
safef(option, sizeof option, "%s.%s", tdb->tableName, "codons");
if (framesTable)
    {
    char *nodeNames[512];
    char buffer[128];

    printf("<BR><B>Codon Translation:</B><BR>");
    printf("Default species to establish reading frame: ");
    nodeNames[0] = database;
    for (wmSpecies = wmSpeciesList, i = 1; wmSpecies != NULL; 
			wmSpecies = wmSpecies->next, i++)
	{
	nodeNames[i] = wmSpecies->name;
	}
    cgiMakeDropList(SPECIES_CODON_DEFAULT, nodeNames, i,
	cartUsualString(cart, SPECIES_CODON_DEFAULT, defaultCodonSpecies));
    puts("<br>");
    safef(buffer, sizeof(buffer), "%s.codons",tdb->tableName);
    cartMakeRadioButton(cart, buffer,"codonNone", "codonDefault");
    printf("No codon translation<BR>");
    cartMakeRadioButton(cart, buffer,"codonDefault", "codonDefault");
    printf("Use default species reading frames for translation<BR>");
    cartMakeRadioButton(cart, buffer,"codonFrameNone", "codonDefault");
    printf("Use reading frames for species if available, otherwise no translation<BR>");
    cartMakeRadioButton(cart, buffer,"codonFrameDef", "codonDefault");
    printf("Use reading frames for species if available, otherwise use default species<BR>");
    }
else
    {
    puts("<P><B>Codon highlighting:</B><BR>" );

#ifdef GENE_FRAMING

    safef(option, sizeof(option), "%s.%s", tdb->tableName, MAF_FRAME_VAR);
    currentCodonMode = cartCgiUsualString(cart, option, MAF_FRAME_GENE);

    /* Disable codon highlighting */
    cgiMakeRadioButton(option, MAF_FRAME_NONE, 
		    sameString(MAF_FRAME_NONE, currentCodonMode));
    puts("None &nbsp;");

    /* Use gene pred */
    cgiMakeRadioButton(option, MAF_FRAME_GENE, 
			    sameString(MAF_FRAME_GENE, currentCodonMode));
    puts("CDS-annotated frame based on");
    safef(option, sizeof(option), "%s.%s", tdb->tableName, MAF_GENEPRED_VAR);
    genePredDropDown(cart, makeTrackHash(database, chromosome), NULL, option);

#else
    snprintf(option, sizeof(option), "%s.%s", tdb->tableName, BASE_COLORS_VAR);
    puts ("&nbsp; Alternate colors every");
    cgiMakeIntVar(option, cartCgiUsualInt(cart, option, 0), 1);
    puts ("bases<BR>");
    snprintf(option, sizeof(option), "%s.%s", tdb->tableName, 
			    BASE_COLORS_OFFSET_VAR);
    puts ("&nbsp; Offset alternate colors by");
    cgiMakeIntVar(option, cartCgiUsualInt(cart, option, 0), 1);
    puts ("bases<BR>");
#endif
    }

treeImage = trackDbSetting(tdb, "treeImage");
if (treeImage)
    printf("</TD><TD><IMG ALIGN=TOP SRC=\"/images/%s\"></TD></TR></TABLE>", treeImage);
else
    puts("</TD></TR></TABLE");

if (trackDbSetting(tdb, CONS_WIGGLE) != NULL)
    {
    puts("<P><B>Conservation graph:</B>" );
    wigUi(tdb);
    }
}

void genericWiggleUi(struct trackDb *tdb, int optionNum )
/* put up UI for any standard wiggle track (a.k.a. sample track)*/
{

	char options[7][256];
	int thisHeightPer, thisLineGap;
	float thisMinYRange, thisMaxYRange;
	char *interpolate, *fill;

snprintf( &options[0][0], 256, "%s.heightPer", tdb->tableName );
snprintf( &options[1][0], 256, "%s.linear.interp", tdb->tableName );
snprintf( &options[3][0], 256, "%s.fill", tdb->tableName );
snprintf( &options[4][0], 256, "%s.min.cutoff", tdb->tableName );
snprintf( &options[5][0], 256, "%s.max.cutoff", tdb->tableName );
snprintf( &options[6][0], 256, "%s.interp.gap", tdb->tableName );

thisHeightPer = atoi(cartUsualString(cart, &options[0][0], "50"));
interpolate = cartUsualString(cart, &options[1][0], "Linear Interpolation");
fill = cartUsualString(cart, &options[3][0], "1");
thisMinYRange = atof(cartUsualString(cart, &options[4][0], "0.0"));
thisMaxYRange = atof(cartUsualString(cart, &options[5][0], "1000.0"));
thisLineGap = atoi(cartUsualString(cart, &options[6][0], "200"));

printf("<p><b>Interpolation: </b> ");
wiggleDropDown(&options[1][0], interpolate );
printf(" ");

printf("<br><br>");
printf(" <b>Fill Blocks</b>: ");
cgiMakeRadioButton(&options[3][0], "1", sameString(fill, "1"));
printf(" on ");

cgiMakeRadioButton(&options[3][0], "0", sameString(fill, "0"));
printf(" off ");

printf("<p><b>Track Height</b>:&nbsp;&nbsp;");
cgiMakeIntVar(&options[0][0], thisHeightPer, 5 );
printf("&nbsp;pixels");

printf("<p><b>Vertical Range</b>:&nbsp;&nbsp;\nmin:");
cgiMakeDoubleVar(&options[4][0], thisMinYRange, 6 );
printf("&nbsp;&nbsp;&nbsp;&nbsp;max:");
cgiMakeDoubleVar(&options[5][0], thisMaxYRange, 6 );

if( optionNum >= 7 )
    {
    printf("<p><b>Maximum Interval to Interpolate Across</b>:&nbsp;&nbsp;");
    cgiMakeIntVar(&options[6][0], thisLineGap, 10 );
    printf("&nbsp;bases");
    }
}

void affyTxnPhase2Ui(struct trackDb *tdb) 
/* Ui for affymetrix transcriptome phase 2 data. */
{
enum trackVisibility tnfgVis = tvHide;
char *visString = cartUsualString(cart, "hgt.affyPhase2.tnfg", "hide");
tnfgVis = hTvFromString(visString);
printf("<b>Transfrags Display Mode: </b>");
hTvDropDown("hgt.affyPhase2.tnfg", tnfgVis, TRUE);

printf("<p><b><u>Graph Plotting options:</u></b><br>");
wigUi(tdb);
printf("<p><b><u>View/Hide individual cell lines:</u></b>");
}

void humMusUi(struct trackDb *tdb, int optionNum )
/* put up UI for human/mouse conservation sample tracks (humMusL and musHumL)*/
{

	char options[7][256];
	int thisHeightPer, thisLineGap;
	float thisMinYRange, thisMaxYRange;
	char *interpolate, *fill;

snprintf( &options[0][0], 256, "%s.heightPer", tdb->tableName );
snprintf( &options[1][0], 256, "%s.linear.interp", tdb->tableName );
snprintf( &options[3][0], 256, "%s.fill", tdb->tableName );
snprintf( &options[4][0], 256, "%s.min.cutoff", tdb->tableName );
snprintf( &options[5][0], 256, "%s.max.cutoff", tdb->tableName );
snprintf( &options[6][0], 256, "%s.interp.gap", tdb->tableName );

thisHeightPer = atoi(cartUsualString(cart, &options[0][0], "50"));
interpolate = cartUsualString(cart, &options[1][0], "Linear Interpolation");
fill = cartUsualString(cart, &options[3][0], "1");
thisMinYRange = atof(cartUsualString(cart, &options[4][0], "0.0"));
thisMaxYRange = atof(cartUsualString(cart, &options[5][0], "8.0"));
thisLineGap = atoi(cartUsualString(cart, &options[6][0], "200"));

printf("<p><b>Interpolation: </b> ");
wiggleDropDown(&options[1][0], interpolate );
printf(" ");

printf("<br><br>");
printf(" <b>Fill Blocks</b>: ");
cgiMakeRadioButton(&options[3][0], "1", sameString(fill, "1"));
printf(" on ");

cgiMakeRadioButton(&options[3][0], "0", sameString(fill, "0"));
printf(" off ");

printf("<p><b>Track Height</b>:&nbsp;&nbsp;");
cgiMakeIntVar(&options[0][0], thisHeightPer, 5 );
printf("&nbsp;pixels");

printf("<p><b>Vertical Range</b>:&nbsp;&nbsp;\nmin:");
cgiMakeDoubleVar(&options[4][0], thisMinYRange, 6 );
printf("&nbsp;&nbsp;&nbsp;&nbsp;max:");
cgiMakeDoubleVar(&options[5][0], thisMaxYRange, 6 );

if( optionNum >= 7 )
{
	printf("<p><b>Maximum Interval to Interpolate Across</b>:&nbsp;&nbsp;");
	cgiMakeIntVar(&options[6][0], thisLineGap, 10 );
	printf("&nbsp;bases");
}
}




void affyTranscriptomeUi(struct trackDb *tdb)
	/* put up UI for the GC percent track (a sample track)*/
{
	int affyTranscriptomeHeightPer = atoi(cartUsualString(cart, "affyTranscriptome.heightPer", "100"));
	char *fill = cartUsualString(cart, "affyTranscriptome.fill", "1");

	printf("<br><br>");
	printf(" <b>Fill Blocks</b>: ");
	cgiMakeRadioButton("affyTranscriptome.fill", "1", sameString(fill, "1"));
	printf(" on ");

	cgiMakeRadioButton("affyTranscriptome.fill", "0", sameString(fill, "0"));
	printf(" off ");

	printf("<p><b>Track Height</b>:&nbsp;&nbsp;");
	cgiMakeIntVar("affyTranscriptome.heightPer", affyTranscriptomeHeightPer, 5 );
	printf("&nbsp;pixels");

}


void ancientRUi(struct trackDb *tdb)
	/* put up UI for the ancient repeats track to let user enter an
	 * integer to filter out those repeats with less aligned bases.*/
{
	int ancientRMinLength = atoi(cartUsualString(cart, "ancientR.minLength", "50"));
	printf("<p><b>Length Filter</b><br>Exclude aligned repeats with less than ");
	cgiMakeIntVar("ancientR.minLength", ancientRMinLength, 4 );
	printf("aligned bases (not necessarily identical). Enter 0 for no filtering.");
}


void affyTransfragUi(struct trackDb *tdb) 
/* Options for filtering affymetrix transfrag track based on score. */
{
boolean skipPseudos = cartUsualBoolean(cart, "affyTransfrags.skipPseudos", TRUE);
boolean skipDups = cartUsualBoolean(cart, "affyTransfrags.skipDups", FALSE);
printf("<br>");
cgiMakeCheckBox("affyTransfrags.skipPseudos", skipPseudos);
printf(" Remove transfrags that overlap pseudogenes from display.<br>");
cgiMakeCheckBox("affyTransfrags.skipDups", skipDups);
printf(" Remove transfrags that have a BLAT match elsewhere in the genome from display.<br>");
}

void acemblyUi(struct trackDb *tdb)
/* Options for filtering the acembly track based on gene class */
{
char *acemblyClass = cartUsualString(cart, "acembly.type", acemblyEnumToString(0));
printf("<p><b>Gene Class: </b>");
acemblyDropDown("acembly.type", acemblyClass);
printf("  ");
}

void specificUi(struct trackDb *tdb)
	/* Draw track specific parts of UI. */
{
char *track = tdb->tableName;


if (sameString(track, "stsMap"))
        stsMapUi(tdb);
else if (sameString(track, "affyTxnPhase2"))
    affyTxnPhase2Ui(tdb);
else if (sameString(track, "stsMapMouseNew"))
        stsMapMouseUi(tdb);
else if (sameString(track, "stsMapRat"))
        stsMapRatUi(tdb);
else if (sameString(track, "snpMap"))
        snpMapUi(tdb);
else if (sameString(track, "snp"))
        snpUi(tdb);
else if (sameString(track, "snp125"))
	snp125Ui(tdb);
else if (sameString(track, "snp126"))
	snp125Ui(tdb);
else if (sameString(track, "rertyHumanDiversityLd"))
        ldUi(tdb);
else if (sameString(track, "hapmapLd"))
        ldUi(tdb);
else if (sameString(track, "cbr_waba"))
        cbrWabaUi(tdb);
else if (sameString(track, "fishClones"))
        fishClonesUi(tdb);
else if (sameString(track, "recombRate"))
        recombRateUi(tdb);
else if (sameString(track, "recombRateRat"))
        recombRateRatUi(tdb);
else if (sameString(track, "recombRateMouse"))
        recombRateMouseUi(tdb);
else if (sameString(track, "nci60"))
        nci60Ui(tdb);
else if (sameString(track, "cghNci60"))
        cghNci60Ui(tdb);
else if (sameString(track, "xenoRefGene"))
        refGeneUI(tdb);
else if (sameString(track, "refGene"))
        refGeneUI(tdb);
else if (sameString(track, "knownGene"))
        knownGeneUI(tdb);
else if (sameString(track, "hg17Kg"))
        hg17KgUI(tdb);
else if (sameString(track, "pseudoGeneLink"))
        retroGeneUI(tdb);
else if (sameString(track, "all_mrna"))
    mrnaUi(tdb, FALSE);
else if (sameString(track, "mrna"))
    mrnaUi(tdb, FALSE);
else if (sameString(track, "splicesP"))
    bedUi(tdb);
else if (sameString(track, "all_est"))
        mrnaUi(tdb, FALSE);
else if (sameString(track, "est"))
        mrnaUi(tdb, FALSE);
else if (sameString(track, "tightMrna"))
        mrnaUi(tdb, FALSE);
else if (sameString(track, "tightEst"))
        mrnaUi(tdb, FALSE);
else if (sameString(track, "intronEst"))
        mrnaUi(tdb, FALSE);
else if (sameString(track, "xenoMrna"))
        mrnaUi(tdb, TRUE);
else if (sameString(track, "xenoEst"))
        mrnaUi(tdb, TRUE);
else if (sameString(track, "rosetta"))
        rosettaUi(tdb);
else if (startsWith("blastDm", track))
        blastFBUi(tdb);
else if (sameString(track, "blastSacCer1SG"))
        blastSGUi(tdb);
else if (sameString(track, "blastHg17KG") || sameString(track, "blastHg16KG") 
        || sameString(track, "blastHg18KG")
        || sameString(track, "blatzHg17KG")|| startsWith("mrnaMap", track)|| startsWith("mrnaXeno", track))
        blastUi(tdb);
else if (startsWith("netAlign", tdb->type))
        netUi(tdb);
else if (startsWith("bedGraph", tdb->type))
	wigUi(tdb);
else if (startsWith("wig", tdb->type))
        {
        if (startsWith("wigMaf", tdb->type))
            wigMafUi(tdb);
        else
            wigUi(tdb);
        }
else if (startsWith("chromGraph", tdb->type))
        chromGraphUi(tdb);
else if (sameString(track, "affyRatio") || sameString(track, "gnfAtlas2") )
        affyUi(tdb);
else if (sameString(track, "affyHumanExon"))
        affyAllExonUi(tdb);
else if (sameString(track, "ancientR"))
        ancientRUi(tdb);
else if (sameString(track, "zoo") || sameString(track, "zooNew" ))
         zooWiggleUi(tdb);
else if (sameString(track, "humMusL") ||
         sameString( track, "musHumL") ||
         sameString( track, "regpotent") ||
         sameString( track, "mm3Rn2L" )	 ||
         sameString( track, "mm3Hg15L" ) ||
         sameString( track, "hg15Mm3L" ))
            humMusUi(tdb,7);
/* NOTE: type psl xeno <otherDb> tracks use crossSpeciesUi, so
 * add explicitly here only if track has another type (bed, chain).
 * For crossSpeciesUi, the
 * default for chrom coloring is "on", unless track setting
 * colorChromDefault is set to "off" */
else if (endsWith("chainSelf", track))
    chainColorUi(tdb);
else if (sameString(track, "orthoTop4"))
    /* still used ?? */
    crossSpeciesUi(tdb);
else if (sameString(track, "mouseOrtho"))
    crossSpeciesUi(tdb);
else if (sameString(track, "mouseSyn"))
    crossSpeciesUi(tdb);
else if (sameString(track, "affyTranscriptome"))
    affyTranscriptomeUi(tdb);

else if (startsWith("sample", tdb->type))
    genericWiggleUi(tdb,7);
else if (sameString(track, RULER_TRACK_NAME))
    rulerUi(tdb);
else if (sameString(track, OLIGO_MATCH_TRACK_NAME))
    oligoMatchUi(tdb);
else if (sameString(track, CUTTERS_TRACK_NAME))
    cutterUi(tdb);
else if(sameString(track, "affyTransfrags"))
    affyTransfragUi(tdb);
else if (sameString(track, "transRegCode"))
    transRegCodeUi(tdb);
else if (sameString(track, "hgMut"))
    hgMutUi(tdb);
else if (sameString(track, "gvPos"))
    gvUi(tdb);
else if (sameString(track, "landmark"))
    landmarkUi(tdb);
else if (sameString(track, "humanPhenotype"))
    humanPhenotypeUi(tdb);
else if (startsWith("retroposons", track))
    retroposonsUi(tdb);
else if (tdb->type != NULL)
    {
    /* handle all tracks with type genePred or bed or "psl xeno <otherDb>" */
    char *typeLine = cloneString(tdb->type);
    char *words[8];
    int wordCount = 0;
    wordCount = chopLine(typeLine, words);
    if (wordCount > 0)
	{
	if (sameWord(words[0], "genePred"))
            {
            if (sameString(track, "acembly"))
                acemblyUi(tdb);
            else if (startsWith("encodeGencode", track))
                gencodeUI(tdb);
            nmdFilterOptions(tdb);
            if (! sameString(track, "tigrGeneIndex"))
	        cdsColorOptions(tdb, 2);
            }
	
	/* if bed has score then show optional filter based on score */
	if (sameWord(words[0], "bed") && wordCount == 3)
	    {
	    /* Note: jaxQTL3 is a bed 8 format track because of thickStart/thickStart, 
	      	     but there is no valid score.
	             Similarly, the score field for wgRna track is no long used either.
		     It originally was usd to depict different RNA types.  But the new 
		     wgRna table has a new field 'type', which is used to store RNA type info
		     and from which to determine the display color of each entry.
	    */
	    if ((atoi(words[1])>4) && !sameString(track,"jaxQTL3") && !sameString(track, "wgRna") && !startsWith("encodeGencodeIntron", track))
		scoreUi(tdb, 1000);
	    }
	else if (sameWord(words[0], "psl"))
	    {
	    if (wordCount == 3)
		if (sameWord(words[1], "xeno"))
		    crossSpeciesUi(tdb);
	    cdsColorOptions(tdb, -1);
	    }
	}
    freeMem(typeLine);
    }

if (trackDbSetting(tdb, "compositeTrack"))
    hCompositeUi(cart, tdb, NULL, NULL, "mainForm");
}

void trackUi(struct trackDb *tdb)
/* Put up track-specific user interface. */
{
char *vis = hStringFromTv(tdb->visibility);
printf("<FORM ACTION=\"%s\" NAME=\"mainForm\" METHOD=%s>\n\n",
       hgTracksName(), cartUsualString(cart, "formMethod", "POST"));
cartSaveSession(cart);
printf("<H1>%s</H1>\n", tdb->longLabel);
printf("<B>Display mode: </B>");
hTvDropDown(tdb->tableName, 
    hTvFromString(cartUsualString(cart,tdb->tableName, vis)), tdb->canPack);
printf(" ");
cgiMakeButton("Submit", "Submit");
printf("<BR>\n");

specificUi(tdb);

puts("</FORM>");

/* Make link to TB schema -- unless this is an on-the-fly (tableless) track. */
if (hTableOrSplitExists(tdb->tableName))
    {
    char *tableName = tdb->tableName;
    if (sameString(tableName, "mrna"))
	tableName = "all_mrna";
    printf("<P><A HREF=\"/cgi-bin/hgTables?db=%s&hgta_group=%s&hgta_track=%s"
	   "&hgta_table=%s&hgta_doSchema=describe+table+schema\" "
	   "TARGET=_BLANK>"
	   "View table schema</A></P>\n",
	   database, tdb->grp, tableName, tableName);
    }
else if (tdb->subtracks != NULL)
    {
    /* handle multi-word subTrack settings: */
    char *words[2];
    if ((chopLine(cloneString(tdb->subtracks->tableName), words) > 0) &&
	hTableOrSplitExists(words[0]))
	printf("<P><A HREF=\"/cgi-bin/hgTables?db=%s&hgta_group=%s&hgta_track=%s"
	       "&hgta_table=%s&hgta_doSchema=describe+table+schema\" "
	       "TARGET=_BLANK>"
	       "View table schema</A></P>\n",
	       database, tdb->grp, tdb->tableName, tdb->subtracks->tableName);
    }

if (tdb->html != NULL && tdb->html[0] != 0)
    {
    htmlHorizontalLine();
    puts(tdb->html);
    }
}

struct trackDb *trackDbForPseudoTrack(char *tableName, char *shortLabel, 
	char *longLabel, int defaultVis, boolean canPack)
/* Create trackDb for a track without a corresponding table. */
{
struct trackDb *tdb;
char htmlFile[256];
char *buf;
size_t size;
char *helpdir;

AllocVar(tdb);
tdb->tableName = tableName;
tdb->shortLabel = shortLabel;
tdb->longLabel = longLabel;
tdb->visibility = defaultVis;
tdb->priority = 1.0;

 helpdir = cfgOption("help.html");
 if (helpdir != NULL)
   safef(htmlFile, 256, "%s/%s.html", helpdir, tableName);
 else
   safef(htmlFile, 256, "%s%s/%s.html", hDocumentRoot(), HELP_DIR, tableName);
 
readInGulp(htmlFile, &buf, &size);
tdb->html = buf;
tdb->type = "none";
tdb->grp = "map";
tdb->canPack = canPack;
return tdb;
}

struct trackDb *trackDbForRuler()
/* Create a trackDb entry for the base position ruler.
   It is not (yet?) a real track, so doesn't appear in trackDb */
{
return trackDbForPseudoTrack(RULER_TRACK_NAME,
	RULER_TRACK_LABEL, RULER_TRACK_LONGLABEL, tvFull, FALSE);
}

struct trackDb *trackDbForOligoMatch()
/* Create a trackDb entry for the oligo matcher pseudo-track. */
{
return trackDbForPseudoTrack(OLIGO_MATCH_TRACK_NAME,
	OLIGO_MATCH_TRACK_LABEL, OLIGO_MATCH_TRACK_LONGLABEL, tvHide, TRUE);
}

void doMiddle(struct cart *theCart)
/* Write body of web page. */
{
struct trackDb *tdb;
char *track;
cart = theCart;
track = cartString(cart, "g");
database = cartUsualString(cart, "db", hGetDb());
hSetDb(database);
chromosome = cartUsualString(cart, "c", hDefaultChrom());
if (sameWord(track, RULER_TRACK_NAME))
    /* special handling -- it's not a full-fledged track */
    tdb = trackDbForRuler();
else if (sameWord(track, OLIGO_MATCH_TRACK_NAME))
    tdb = trackDbForOligoMatch();
else if (sameWord(track, CUTTERS_TRACK_NAME))
    tdb = trackDbForPseudoTrack(CUTTERS_TRACK_NAME, CUTTERS_TRACK_LABEL, CUTTERS_TRACK_LONGLABEL, tvHide, TRUE);
else
    tdb = hTrackDbForTrack(track);
if (tdb == NULL)
   errAbort("Can't find %s in track database %s chromosome %s",
	    track, database, chromosome);
cartWebStart(cart, "%s Track Settings", tdb->shortLabel);
trackUi(tdb);
webEnd();
}

char *excludeVars[] = { "submit", "Submit", "g", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
htmlSetBackground(hBackgroundImage());
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, NULL);
return 0;
}
