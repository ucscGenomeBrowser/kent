#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "jksql.h"
#include "jsHelper.h"
#include "trackDb.h"
#include "hgTrackUi.h"
#include "hdb.h"
#include "hCommon.h"
#include "hui.h"
#include "fileUi.h"
#include "ldUi.h"
#include "snpUi.h"
#include "snp125Ui.h"
#include "snp125.h"
#include "sample.h"
#include "wiggle.h"
#include "hgMaf.h"
#include "obscure.h"
#include "chainCart.h"
#include "chainDb.h"
#include "gvUi.h"
#include "grp.h"
#include "oregannoUi.h"
#include "chromGraph.h"
#include "hgConfig.h"
#include "customTrack.h"
#include "dbRIP.h"
#include "tfbsConsSites.h"
#include "hapmapSnps.h"
#include "nonCodingUi.h"
#include "expRecord.h"
#include "wikiTrack.h"
#include "hubConnect.h"
#include "trackHub.h"
#include "pcrResult.h"
#include "dgv.h"
#include "transMapStuff.h"
#include "vcfUi.h"
#include "bbiFile.h"
#include "ensFace.h"
#include "microarray.h"

#define MAIN_FORM "mainForm"
#define WIGGLE_HELP_PAGE  "../goldenPath/help/hgWiggleTrackHelp.html"

static char const rcsid[] = "$Id: hgTrackUi.c,v 1.527 2010/06/04 21:54:56 angie Exp $";

struct cart *cart = NULL;	/* Cookie cart with UI settings */
char *database = NULL;		/* Current database. */
char *chromosome = NULL;        /* Chromosome. */
struct hash *trackHash = NULL;	/* Hash of all tracks in database. */

void tfbsConsSitesUi(struct trackDb *tdb)
{
float tfbsConsSitesCutoff =
    sqlFloat(cartUsualString(cart,TFBS_SITES_CUTOFF,TFBS_SITES_CUTOFF_DEFAULT));
printf("<BR><B>Z score cutoff (default %s, minimum %s):&nbsp;</B>",
	TFBS_SITES_CUTOFF_DEFAULT, TFBS_SITES_CUTOFF_MINIMUM);
cgiMakeDoubleVar(TFBS_SITES_CUTOFF,tfbsConsSitesCutoff,5);
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

void snp125OfferGeneTracksForFunction(struct trackDb *tdb)
/* Get a list of genePred tracks and make checkboxes to enable hgc's functional
 * annotations. */
{
struct sqlConnection *conn = hAllocConn(database);
struct slName *genePredTables = hTrackTablesOfType(conn, "genePred%%"), *gt;
if (genePredTables != NULL)
    {
    struct trackDb *geneTdbList = NULL, *gTdb;
    for (gt = genePredTables;  gt != NULL;  gt = gt->next)
	{
	gTdb = hashFindVal(trackHash, gt->name);
	if (gTdb && sameString(gTdb->grp, "genes"))
	    {
	    // We are going to overwrite gTdb's next pointer and possibly its priority,
	    // so make a shallow copy:
	    gTdb = CloneVar(gTdb);
	    if (gTdb->parent)
		gTdb->priority = (gTdb->parent->priority + gTdb->priority/1000);
	    slAddHead(&geneTdbList, gTdb);
	    }
	}
    slSort(&geneTdbList, trackDbCmp);
    jsBeginCollapsibleSection(cart, tdb->track, "geneTracks",
			      "Use Gene Tracks for Functional Annotation", FALSE);
    printf("<BR><B>On details page, show function and coding differences relative to: </B>\n");
    char cartVar[256];
    safef(cartVar, sizeof(cartVar), "%s_geneTrack", tdb->track);
    jsMakeCheckboxGroupSetClearButton(cartVar, TRUE);
    jsMakeCheckboxGroupSetClearButton(cartVar, FALSE);
    struct slName *selectedGeneTracks = cartOptionalSlNameList(cart, cartVar);
    if (!cartListVarExists(cart, cartVar))
	{
	char *defaultGeneTracks = trackDbSetting(tdb, "defaultGeneTracks");
	if (isNotEmpty(defaultGeneTracks))
	    selectedGeneTracks = slNameListFromComma(defaultGeneTracks);
	}
    int numCols = 4, i;
    int menuSize = slCount(geneTdbList);
    char **values = needMem(menuSize*sizeof(char *));
    char **labels = needMem(menuSize*sizeof(char *));
    for (i = 0, gTdb = geneTdbList;  i < menuSize && gTdb != NULL;  i++, gTdb = gTdb->next)
	{
	values[i] = gTdb->track;
	labels[i] = gTdb->shortLabel;
	}
    cgiMakeCheckboxGroupWithVals(cartVar, labels, values, menuSize, selectedGeneTracks, numCols);
    jsEndCollapsibleSection();
    }
hFreeConn(&conn);
}

#define SNP125_FILTER_COLUMNS 4
#define SNP125_DEFAULTS "snp125Defaults"

void snp125PrintFilterControls(char *track, char *attributeLabel, char *attributeVar,
			       char *labels[], char *values[], int menuSize)
/* Print two or more rows (attribute name header and row(s) of checkboxes)
 * of a table displaying snp125 attribute filter checkboxes. */
{
char anchor[256];
safecpy(anchor, sizeof(anchor), attributeVar);
anchor[0] = toupper(anchor[0]);
printf("<TR><TD><B><A HREF=\"#%s\">%s</A>:</B>&nbsp;\n", anchor, attributeLabel);
char cartVar[256];
safef(cartVar, sizeof(cartVar), "%s.include_%s", track, attributeVar);
jsMakeCheckboxGroupSetClearButton(cartVar, TRUE);
puts("&nbsp;");
jsMakeCheckboxGroupSetClearButton(cartVar, FALSE);
printf("</TD></TR>\n<TR><TD>\n");
boolean foundInCart = FALSE;
struct slName *selectedAttributes = snp125FilterFromCart(cart, track, attributeVar, &foundInCart);
// Include all by default:
if (! foundInCart)
    selectedAttributes = slNameListFromStringArray(values, menuSize);
cgiMakeCheckboxGroupWithVals(cartVar, labels, values, menuSize, selectedAttributes,
			     SNP125_FILTER_COLUMNS);
printf("</TD></TR>\n");
}

static void snp125PrintFilterControlSection(struct trackDb *tdb, int version)
/* Print a collapsible section of filtering controls on SNP properties, first numeric
 * and then enum/set. */
{
char cartVar[512];
printf("<TR><TD colspan=2><A name=\"filterControls\"></TD></TR>\n");
jsBeginCollapsibleSection(cart, tdb->track, "filterByAttribute", "Filtering Options", FALSE);

printf("<BR>\n");
safef(cartVar, sizeof(cartVar), "%s.minAvHet", tdb->track);
double minAvHet = cartUsualDouble(cart, cartVar,
			     // Check old cart var name:
			     cartUsualDouble(cart, "snp125AvHetCutoff", SNP125_DEFAULT_MIN_AVHET));
printf("<B>Minimum <A HREF=\"#AvHet\">Average Heterozygosity</A>:</B>&nbsp;");
cgiMakeDoubleVar(cartVar, minAvHet, 6);
printf("<BR>\n");

safef(cartVar, sizeof(cartVar), "%s.maxWeight", tdb->track);
int defaultMaxWeight = SNP125_DEFAULT_MAX_WEIGHT;
char *setting = trackDbSetting(tdb, "defaultMaxWeight");
if (isNotEmpty(setting))
    defaultMaxWeight = atoi(setting);
int maxWeight = cartUsualInt(cart, cartVar,
			     // Check old cart var name:
			     cartUsualInt(cart, "snp125WeightCutoff", defaultMaxWeight));
printf("<B>Maximum <A HREF=\"#Weight\">Weight</A>:</B>&nbsp;");
cgiMakeIntVar(cartVar, maxWeight, 4);
printf("&nbsp;<EM>Range: 1, 2 or 3; SNPs with higher weights are less reliable</EM><BR>\n");

if (version >= 132)
    {
    printf("<B>Minimum number of distinct "
	   "<A HREF=\"#Submitters\">Submitters</A>:</B>&nbsp;");
    safef(cartVar, sizeof(cartVar), "%s.minSubmitters", tdb->track);
    cgiMakeIntVar(cartVar, cartUsualInt(cart, cartVar, SNP132_DEFAULT_MIN_SUBMITTERS), 4);
    printf("<BR>\n");
    printf("<B><A HREF=\"#AlleleFreq\">Minor Allele Frequency</A> range:</B>&nbsp;");
    safef(cartVar, sizeof(cartVar), "%s.minMinorAlFreq", tdb->track);
    float maf = cartUsualDouble(cart, cartVar, SNP132_DEFAULT_MIN_MINOR_AL_FREQ);
    cgiMakeDoubleVarInRange(cartVar, maf, NULL, 4, "0.0", "0.5");
    printf(" to ");
    safef(cartVar, sizeof(cartVar), "%s.maxMinorAlFreq", tdb->track);
    maf = cartUsualDouble(cart, cartVar, SNP132_DEFAULT_MAX_MINOR_AL_FREQ);
    cgiMakeDoubleVarInRange(cartVar, maf, NULL, 4, "0.0", "0.5");
    printf(" <em>Range: 0.0 - 0.5</em>\n");
    printf("<BR>\n");
    printf("<B>Minimum chromosome sample count (2N) for "
	   "<A HREF=\"#AlleleFreq\">Allele Frequency</A> data:</B>&nbsp;");
    safef(cartVar, sizeof(cartVar), "%s.minAlFreq2N", tdb->track);
    cgiMakeIntVar(cartVar, cartUsualInt(cart, cartVar, SNP132_DEFAULT_MIN_AL_FREQ_2N), 4);
    printf("<BR>\n");
    }

printf("<BR>\n");

printf("<B>Filter by attribute:</B><BR>\n");
printf("Check the boxes below to include SNPs with those attributes.  "
       "In order to be displayed, a SNP must pass the filter for each "
       "category.  \n"
       "Some assemblies may not contain any SNPs that have some of the "
       "listed attributes.\n"
       "<BR><BR>\n");

printf("<TABLE border=0 cellspacing=0 cellpadding=0>\n");
if (version <= 127)
    snp125PrintFilterControls(tdb->track, "Location Type", "locType", snp125LocTypeLabels,
			 snp125LocTypeDataName, snp125LocTypeArraySize);
snp125PrintFilterControls(tdb->track, "Class", "class", snp125ClassLabels,
			  snp125ClassDataName, snp125ClassArraySize);
snp125PrintFilterControls(tdb->track, "Validation", "valid", snp125ValidLabels,
			  snp125ValidDataName, snp125ValidArraySize);
int funcArraySize = (version < 130) ? snp125FuncArraySize : (snp125FuncArraySize - 1);
snp125PrintFilterControls(tdb->track, "Function", "func", snp125FuncLabels,
			  snp125FuncDataName, funcArraySize);
snp125PrintFilterControls(tdb->track, "Molecule Type", "molType", snp125MolTypeLabels,
			  snp125MolTypeDataName, snp125MolTypeArraySize);
if (version >= 132)
    {
    snp125PrintFilterControls(tdb->track, "Unusual Conditions (UCSC)", "exceptions",
		      snp132ExceptionLabels, snp132ExceptionVarName, snp132ExceptionArraySize);
    snp125PrintFilterControls(tdb->track, "Miscellaneous Attributes (dbSNP)", "bitfields",
		      snp132BitfieldLabels, snp132BitfieldDataName, snp132BitfieldArraySize);
    }
printf("</TABLE>\n");
jsEndCollapsibleSection();
}

static void snp125PrintColorSpec(char *track, char *attribute, char *vars[], boolean varsAreOld,
				 char *labels[], char *defaults[], int varCount)
/* Print a table displaying snp125 attribute color selects. */
{
int i;
printf("If a SNP has more than one of these attributes, the stronger color will override the\n"
       "weaker color. The order of colors, from strongest to weakest, is red,\n"
       "green, blue, gray, and black.\n"
       "<BR><BR>\n");
printf("<TABLE border=0 cellspacing=0 cellpadding=1>\n");
for (i=0; i < varCount; i++)
    {
    if (i % SNP125_FILTER_COLUMNS == 0)
	{
	if (i > 0)
	    printf("</TR>\n");
	printf("<TR>");
	}
    printf("<TD align=right>%s</TD><TD>", labels[i]);
    char cartVar[512];
    safef(cartVar, sizeof(cartVar), "%s.%s%s", track, attribute,
	  (varsAreOld ? snp125OldColorVarToNew(vars[i], attribute) : vars[i]));
    char *defaultCol = defaults[i];
    if (varsAreOld)
	defaultCol = cartUsualString(cart, vars[i], defaultCol);
    char *selected = cartUsualString(cart, cartVar, defaultCol);
    cgiMakeDropListWithVals(cartVar, snp125ColorLabel, snp125ColorLabel,
			    snp125ColorArraySize, selected);
    printf("</TD><TD>&nbsp;&nbsp;&nbsp;</TD>");
    }
printf("</TABLE>\n");
}

static void snp125RemoveColorVars(struct cart *cart, char *vars[], boolean varsAreOld,
				  int varCount, char *track, char *attribute)
/* Remove each cart variable in vars[], as well as the new cart vars that begin with
 * the track name if varsAreOld. */
{
int i;
for (i = 0;  i < varCount;  i++)
    {
    if (varsAreOld)
	cartRemove(cart, vars[i]);
    char cartVar[512];
    safef(cartVar, sizeof(cartVar), "%s.%s%s", track, attribute,
	  (varsAreOld ? snp125OldColorVarToNew(vars[i], attribute) : vars[i]));
    cartRemove(cart, cartVar);
    }
}

static void snp125ResetColorVarsIfNecessary(struct trackDb *tdb, char *buttonVar, int version)
/* If the 'Set defaults' button has been clicked, remove all color-control cart variables. */
{
// Note we use CGI, not cart, to detect a click:
if (isNotEmpty(cgiOptionalString(buttonVar)))
    {
    char cartVar[512];
    safef(cartVar, sizeof(cartVar), "%s.colorSource", tdb->track);
    cartRemove(cart, cartVar);
    cartRemove(cart, snp125ColorSourceOldVar);
    snp125RemoveColorVars(cart, snp125LocTypeOldColorVars,  TRUE, snp125LocTypeArraySize,
			  tdb->track, "locType");
    snp125RemoveColorVars(cart, snp125ClassOldColorVars, TRUE, snp125ClassArraySize,
			  tdb->track, "class");
    snp125RemoveColorVars(cart, snp125ValidOldColorVars, TRUE, snp125ValidArraySize,
			  tdb->track, "valid");
    int funcArraySize = (version < 130) ? snp125FuncArraySize : (snp125FuncArraySize - 1);
    snp125RemoveColorVars(cart, snp125FuncOldColorVars, TRUE, funcArraySize,
			  tdb->track, "func");
    snp125RemoveColorVars(cart, snp125MolTypeOldColorVars, TRUE, snp125MolTypeArraySize,
			  tdb->track, "molType");
    snp125RemoveColorVars(cart, snp132ExceptionVarName, FALSE, snp132ExceptionArraySize,
			  tdb->track, "exceptions");
    snp125RemoveColorVars(cart, snp132BitfieldVarName, FALSE, snp132BitfieldArraySize,
			  tdb->track, "bitfields");
    }
}

void snp125PrintColorControlSection(struct trackDb *tdb, int version)
/* Print a collapsible section of color controls: user selects an attribute to color by,
 * and then a color for each possible value of the selected attribute. */
{
printf("<TR><TD colspan=2><A name=\"colorSpec\"></TD></TR>\n");
jsBeginCollapsibleSection(cart, tdb->track, "colorByAttribute", "Coloring Options", FALSE);

char defaultButtonVar[512];
safef(defaultButtonVar, sizeof(defaultButtonVar), "%s_coloring", SNP125_DEFAULTS);
stripChar(defaultButtonVar, ' ');
snp125ResetColorVarsIfNecessary(tdb, defaultButtonVar, version);

printf("<BR><B>SNP Feature for Color Specification:</B>\n");
char **labels = snp132ColorSourceLabels;
int arraySize = snp132ColorSourceArraySize;
if (version <= 127)
    {
    labels = snp125ColorSourceLabels;
    arraySize = snp125ColorSourceArraySize;
    }
else if (version <= 131)
    {
    labels = snp128ColorSourceLabels;
    arraySize = snp128ColorSourceArraySize;
    }

// It would be preferable for Javascript to handle changing the color selection
// menus when the color source selection changes, but for now we do a submit that
// returns to the current vertical position:
char autoSubmit[2048];
safef(autoSubmit, sizeof(autoSubmit), "onchange=\""
      "document."MAIN_FORM".action = '%s'; %s"
      "document."MAIN_FORM".submit();\"",
      cgiScriptName(), jsSetVerticalPosition(MAIN_FORM));
cgiContinueHiddenVar("g");
cgiContinueHiddenVar("c");

char cartVar[512];
safef(cartVar, sizeof(cartVar), "%s.colorSource", tdb->track);
enum snp125ColorSource colorSourceCart = snp125ColorSourceFromCart(cart, tdb);
char *colorSourceSelected = snp125ColorSourceToLabel(tdb, colorSourceCart);
cgiMakeDropListFull(cartVar, labels, labels, arraySize, colorSourceSelected, autoSubmit);
printf("&nbsp;\n");
char javascript[2048];
safef(javascript, sizeof(javascript),
      "document."MAIN_FORM".action='%s'; %s document."MAIN_FORM".submit();",
      cgiScriptName(), jsSetVerticalPosition(MAIN_FORM));
cgiMakeOnClickSubmitButton(javascript, defaultButtonVar, JS_DEFAULTS_BUTTON_LABEL);
printf("<BR><BR>\n"
       "The selected &quot;Feature for Color Specification&quot; above has the\n"
       "selection of colors below for each attribute. Only the color\n"
       "options for the feature selected above will be used to color items;\n"
       "color options for other features will not be shown.\n");

if (version > 127 && colorSourceCart == snp125ColorSourceLocType)
    colorSourceCart = SNP125_DEFAULT_COLOR_SOURCE;
switch (colorSourceCart)
    {
    int funcArraySize;
    case snp125ColorSourceLocType:
	snp125PrintColorSpec(tdb->track, "locType", snp125LocTypeOldColorVars, TRUE,
			     snp125LocTypeLabels, snp125LocTypeDefault, snp125LocTypeArraySize);
	break;
    case snp125ColorSourceClass:
    snp125PrintColorSpec(tdb->track, "class", snp125ClassOldColorVars, TRUE,
			 snp125ClassLabels, snp125ClassDefault, snp125ClassArraySize);
	break;
    case snp125ColorSourceValid:
    snp125PrintColorSpec(tdb->track, "valid", snp125ValidOldColorVars, TRUE,
			 snp125ValidLabels, snp125ValidDefault, snp125ValidArraySize);
	break;
    case snp125ColorSourceFunc:
	funcArraySize = (version < 130) ? snp125FuncArraySize : (snp125FuncArraySize - 1);
    snp125PrintColorSpec(tdb->track, "func", snp125FuncOldColorVars, TRUE,
			 snp125FuncLabels, snp125FuncDefault, funcArraySize);
	break;
    case snp125ColorSourceMolType:
    snp125PrintColorSpec(tdb->track, "molType", snp125MolTypeOldColorVars, TRUE,
			 snp125MolTypeLabels, snp125MolTypeDefault, snp125MolTypeArraySize);
	break;
    case snp125ColorSourceExceptions:
    snp125PrintColorSpec(tdb->track, "exceptions", snp132ExceptionVarName, FALSE,
			 snp132ExceptionLabels, snp132ExceptionDefault, snp132ExceptionArraySize);
	break;
    case snp125ColorSourceBitfields:
    snp125PrintColorSpec(tdb->track, "bitfields", snp132BitfieldVarName, FALSE,
			 snp132BitfieldLabels, snp132BitfieldDefault, snp132BitfieldArraySize);
	break;
    case snp125ColorSourceAlleleFreq:
	printf("<P>Items are be colored by allele frequency on a red-blue spectrum, "
	       "with red representing rare alleles and blue representing common alleles. "
	       "Items with no allele frequency data are colored black.</P>");
	break;
    default:
	errAbort("Unrecognized value of enum snp125ColorSource (%d)", colorSourceCart);
    }
jsEndCollapsibleSection();
}

void snp125Ui(struct trackDb *tdb)
/* UI for dbSNP version 125 and later. */
{
char *orthoTable = snp125OrthoTable(tdb, NULL);
int version = snpVersion(tdb->track);
char cartVar[512];
jsInit();

if (version < 130)
    snp125ValidArraySize--; // no by-1000genomes

if (isNotEmpty(orthoTable) && hTableExists(database, orthoTable))
    {
    printf("<BR><B>Include Chimp state and observed human alleles in name: </B>&nbsp;");
    safef(cartVar, sizeof(cartVar), "%s.extendedNames", tdb->track);
    snp125ExtendedNames = cartUsualBoolean(cart, cartVar,
			  // Check old cart var name for backwards compatibility w/ old sessions:
					   cartUsualBoolean(cart, "snp125ExtendedNames", FALSE));
    cgiMakeCheckBox(cartVar, snp125ExtendedNames);
    printf("<BR>(If enabled, chimp allele is displayed first, then '>', then human alleles).");
    printf("<BR><BR>\n");
    }
else
    puts("<BR>");

// Make wrapper table for collapsible sections:
puts("<TABLE border=0 cellspacing=0 cellpadding=0>");

snp125OfferGeneTracksForFunction(tdb);

puts("<TR><TD colspan=2><BR></TD></TR>");
snp125PrintFilterControlSection(tdb, version);
puts("<TR><TD colspan=2><BR></TD></TR>");

snp125PrintColorControlSection(tdb, version);
// End wrapper table for collapsible sections:
puts("</TABLE>");
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
char var[512];
char *val;

/* It would be nice to add a 'reset' button to reset the ld variables to their defaults. */

printf("<BR><B>LD Values:</B><BR>&nbsp;&nbsp;\n");

safef(var, sizeof(var), "%s_val", tdb->track);
val = cartUsualString(cart,  var, ldValDefault);
cgiMakeRadioButton(var, "rsquared", sameString("rsquared", val));
printf("&nbsp;r<sup>2</sup>&nbsp;&nbsp;");
cgiMakeRadioButton(var, "dprime",   sameString("dprime",   val));
printf("&nbsp;D'&nbsp;&nbsp;");
cgiMakeRadioButton(var, "lod",      sameString("lod",      val));
printf("&nbsp;LOD<BR>");

printf("<BR><B>Track Geometry:</B><BR>&nbsp;&nbsp;\n");

safef(var, sizeof(var), "%s_trm", tdb->track);
cgiMakeCheckBox(var, cartUsualBoolean(cart, var, ldTrmDefault));
printf("&nbsp;Trim to triangle<BR>\n");

if (tdbIsComposite(tdb))
    {
    printf("<BR>&nbsp;&nbsp;&nbsp;");
    struct slRef *tdbRefList = trackDbListGetRefsToDescendantLeaves(tdb->subtracks);
    slSort(tdbRefList, trackDbRefCmp);
    struct slRef *tdbRef;
    for (tdbRef = tdbRefList; tdbRef != NULL; tdbRef = tdbRef->next)
	{
	struct trackDb *subTdb = tdbRef->val;
	if (hTableExists(database, subTdb->table))
	    {
	    safef(var, sizeof(var), "%s_inv", subTdb->track);
	    cgiMakeCheckBox(var, cartUsualBoolean(cart, var, ldInvDefault));
	    printf("&nbsp;Invert display for %s<BR>&nbsp;&nbsp;\n",
		   subTdb->longLabel);
	    }
	}
    slFreeList(&tdbRefList);
    }
else
    {
    safef(var, sizeof(var), "%s_inv", tdb->track);
    printf("&nbsp;&nbsp;&nbsp;");
    cgiMakeCheckBox(var, cartUsualBoolean(cart, var, ldInvDefault));
    printf("&nbsp;Invert the display<BR>&nbsp;&nbsp;\n");
    }
printf("<BR><B>Colors:</B>\n");

safef(var, sizeof(var), "%s_pos", tdb->track);
val = cartUsualString(cart, var, ldPosDefault);
printf("<TABLE>\n ");
printf("<TR>\n  <TD>&nbsp;&nbsp;LD values&nbsp;&nbsp;</TD>\n  <TD>- ");
radioButton(var, val, "red");
printf("</TD>\n  <TD>");
radioButton(var, val, "green");
printf("</TD>\n  <TD>");
radioButton(var, val, "blue");
printf("</TD>\n </TR>\n ");

safef(var, sizeof(var), "%s_out", tdb->track);
val = cartUsualString(cart, var, ldOutDefault);
printf("<TR>\n  <TD>&nbsp;&nbsp;Outlines&nbsp;&nbsp;</TD>\n  <TD>- ");
radioButton(var, val, "red");
printf("</TD>\n  <TD>");
radioButton(var, val, "green");
printf("</TD>\n  <TD>");
radioButton(var, val, "blue");
printf("</TD>\n  <TD>");
radioButton(var, val, "yellow");
printf("</TD>\n  <TD>");
radioButton(var, val, "black");
printf("</TD>\n  <TD>");
radioButton(var, val, "white");
printf("</TD>\n  <TD>");
radioButton(var, val, "none");
printf("</TD>\n </TR>\n ");
printf("</TABLE>\n");

if (tdb->type && sameString(tdb->type, "ld2"))
    {
    safef(var, sizeof(var), "%s_gap", tdb->track);
    printf("&nbsp;&nbsp;");
    cgiMakeCheckBox(var, cartUsualBoolean(cart, var, ldGapDefault));
    printf("&nbsp;In dense mode, shade gaps between markers by T-int<BR>\n");
    }

if (tdbIsComposite(tdb))
    printf("<BR><B>Populations:</B>\n");
}

void oregannoUi (struct trackDb *tdb)
/* print the controls */
{
int i = 0; /* variable to walk through array */

printf("<BR><B>Type of region to display: </B> ");
printf("&nbsp;(Click <A HREF=\"http://www.oreganno.org/oregano/help/records.html\" target=\"_blank\">here</A> for detailed information on these element types)<BR>");
for (i = 0; i < oregannoTypeSize; i++)
    {
    cartMakeCheckBox(cart, oregannoTypeString[i], TRUE);
    printf (" %s<BR>", oregannoTypeLabel[i]);
    }
}

void labelMakeCheckBox(struct trackDb *tdb, char *sym, char *desc, boolean dflt)
/* add a checkbox use to choose labels to enable. */
{
char varName[64];
safef(varName, sizeof(varName), "%s.label.%s", tdb->track, sym);
boolean option = cartUsualBoolean(cart, varName, dflt);
cgiMakeCheckBox(varName, option);
printf(" %s&nbsp;&nbsp;&nbsp;", desc);
}

void gvIdControls (struct trackDb *tdb)
/* print the controls for the label choice */
{
printf("<B>Label:</B> ");
labelMakeCheckBox(tdb, "hgvs", "HGVS name", FALSE);
labelMakeCheckBox(tdb, "common", "Common name", FALSE);
printf("<BR>\n");
}

void gvUi(struct trackDb *tdb)
/* print UI for human mutation filters */
{
int i = 0; /* variable to walk through arrays */
//char *def;

gvIdControls(tdb);

/*
printf("<BR><B>Exclude data source</B><BR>");
for (i = 0; i < gvSrcSize; i++)
    {
    if (differentString(gvSrcDbValue[i], "LSDB"))
        {
        if (sameString(gvSrcDbValue[i], "UniProt (Swiss-Prot/TrEMBL)"))
            {
            // exclude Swiss-Prot data by default, can be misleading
            cartMakeCheckBox(cart, gvSrcString[i], TRUE);
            }
        else
            {
            cartMakeCheckBox(cart, gvSrcString[i], FALSE);
            }
        printf (" %s<BR>", gvSrcDbValue[i]); // label with db value
        }
    else
        {
        cartMakeCheckBox(cart, gvSrcString[i], FALSE);
        printf (" Locus Specific Databases<BR>");
        }
    }
*/
printf("<BR><B>Exclude</B><BR>");
for (i = 0; i < gvAccuracySize; i++)
    {
    cartMakeCheckBox(cart, gvAccuracyString[i], FALSE);
    printf (" %s<BR>", gvAccuracyLabel[i]);
    }

printf("<BR><B>Exclude mutation type</B><BR>");
for (i = 0; i < gvTypeSize; i++)
    {
    cartMakeCheckBox(cart, gvTypeString[i], FALSE);
    printf (" %s<BR>", gvTypeLabel[i]);
    }

printf("<BR><B>Exclude mutation location</B><BR>");
for (i = 0; i < gvLocationSize; i++)
    {
    cartMakeCheckBox(cart, gvLocationString[i], FALSE);
    printf (" %s<BR>", gvLocationLabel[i]);
    }

/*
printf("<BR><B>Exclude phenotype association</B><BR>");
for (i = 0; i < gvFilterDASize; i++)
    {
    cartMakeCheckBox(cart, gvFilterDAString[i], FALSE);
    printf (" %s<BR>", gvFilterDALabel[i]);
    }
*/
printf("<BR>");
cartMakeRadioButton(cart, "gvPos.filter.colorby", "type", "type");
printf("<B>Color mutations by type</B><BR>");
for (i = 0; i < gvColorTypeSize; i++)
    {
    char *defaultVal = cartUsualString(cart, gvColorTypeStrings[i], gvColorTypeDefault[i]);
    printf (" %s ", gvColorTypeLabels[i]);
    cgiMakeDropList(gvColorTypeStrings[i], gvColorLabels, gvColorLabelSize, defaultVal);
    }

printf("<BR>");
/*
cartMakeRadioButton(cart, "gvPos.filter.colorby", "disease", "type");
printf("<B>Color mutations by phenotype association</B> (determination is described <A HREF=#phenoassoc>below</A>)<BR>");
for (i = 0; i < gvColorDASize; i++)
    {
    char *defaultVal = cartUsualString(cart, gvColorDAStrings[i], gvColorDADefault[i]);
    printf (" %s ", gvColorDALabels[i]);
    cgiMakeDropList(gvColorDAStrings[i], gvColorLabels, gvColorLabelSize, defaultVal);
    }
printf("<BR>");
*/
/* only on development page for now, but need to check in other changes */
/*
if (startsWith("hgwdev-giardine", cgiServerName()))
    {
    cartMakeRadioButton(cart, "gvPos.filter.colorby", "count", "type");
    printf("<B>Color mutations by count of positions associated with a mutation</B><BR>");
    def = cartUsualString(cart, "gvColorCountSingle", "blue");
    printf (" Single position ");
    cgiMakeDropList("gvColorCountSingle", gvColorLabels, gvColorLabelSize, def);
    def = cartUsualString(cart, "gvColorCountMult", "green");
    printf (" Multiple positions ");
    cgiMakeDropList("gvColorCountMult", gvColorLabels, gvColorLabelSize, def);
    printf("<BR>");
    }
*/
}

void retroposonsUi(struct trackDb *tdb)
{
struct sqlConnection *conn = hAllocConn(database);
char query[256];
char **row;
struct sqlResult *sr;
struct slName *sList = NULL, *item;
int menuSize = 0;
char **menu;
int i;
char *tableList[3];

i = 0;
tableList[i++] = "dbRIPAlu";
tableList[i++] = "dbRIPL1";
tableList[i++] = "dbRIPSVA";

safef(query, sizeof(query),
"SELECT genoRegion FROM dbRIPAlu GROUP BY genoRegion ORDER BY genoRegion DESC");
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

#ifdef HAS_NO_MEANING
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
#endif	/*	HAS_NO_MEANING	*/

menuSize = 3;
menu = needMem((size_t)(menuSize * sizeof(char *)));
i = 0;
menu[i++] = DISEASE_DEFAULT;
menu[i++] = "yes";
menu[i++] = "no";

puts("<BR><BR>\n<B>Associated disease state known:</B>&nbsp;");
cgiMakeDropList(dbRIP_DISEASE, menu, menuSize,
    cartCgiUsualString(cart, dbRIP_DISEASE, menu[0]));
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

void affyUi(struct trackDb *tdb)
/* put up UI for the affy track from stanford track */
{
char *affyMap;
char *col;
char varName[128];

safef(varName, sizeof(varName), "%s.%s", tdb->track, "type");
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

void expRatioCombineDropDown(struct trackDb *tdb, struct microarrayGroups *groups)
/* Make a drop-down of all the main combinations. */
{
struct maGrouping *comb;
int size = 0;
int i;
char **menuArray;
char **valArray;
char *dropDownName = expRatioCombineDLName(tdb->track);
char *defaultSelection = NULL;
char *cartSetting = NULL;
if (!groups->allArrays)
    errAbort("The \'all\' stanza must be set in the microarrayGroup settings for track %s", tdb->track);
if (groups->defaultCombine)
    defaultSelection = groups->defaultCombine->name;
else
    defaultSelection = groups->allArrays->name;
size = groups->numCombinations + 1;
AllocArray(menuArray, size);
AllocArray(valArray, size);
menuArray[0] = groups->allArrays->description;
valArray[0] = groups->allArrays->name;
for (i = 1, comb = groups->combineSettings; (i < size) && (comb != NULL); i++, comb = comb->next)
    {
    menuArray[i] = cloneString(comb->description);
    valArray[i] = cloneString(comb->name);
    }
cartSetting = cartUsualString(cart, dropDownName, defaultSelection);
printf(" <b>Combine arrays</b>: ");
cgiMakeDropListWithVals(dropDownName, menuArray, valArray,
                         size, cartSetting);
}

void expRatioDrawExonOption(struct trackDb *tdb)
/* Add option to show exons if possible. */
{
char checkBoxName[512];
char *drawExons = trackDbSetting(tdb, "expDrawExons");
boolean checked = FALSE;
if (!drawExons || differentWord(drawExons, "on"))
    return;
safef(checkBoxName, sizeof(checkBoxName), "%s.expDrawExons", tdb->track);
checked = cartCgiUsualBoolean(cart, checkBoxName, FALSE);
puts("<B>Draw intron lines/arrows and exons: </B> ");
cgiMakeCheckBox(checkBoxName, checked);
puts("<BR>\n");
}

void expRatioColorOption(struct trackDb *tdb)
/* Radio button for red/green or blue/yellow */
{
char radioName[256];
char *colorSetting = NULL;
char *tdbSetting = trackDbSettingOrDefault(tdb, "expColor", "redGreen");
safef(radioName, sizeof(radioName), "%s.color", tdb->track);
colorSetting = cartUsualString(cart, radioName, tdbSetting);
puts("<BR><B>Color: </B><BR> ");
cgiMakeRadioButton(radioName, "redGreen", sameString(colorSetting, "redGreen"));
puts("red/green<BR>");
cgiMakeRadioButton(radioName, "redBlue", sameString(colorSetting, "redBlue"));
puts("red/blue<BR>");
cgiMakeRadioButton(radioName, "yellowBlue", sameString(colorSetting, "yellowBlue"));
puts("yellow/blue<BR>\n");
cgiMakeRadioButton(radioName, "redBlueOnWhite", sameString(colorSetting, "redBlueOnWhite"));
puts("red/blue on white background<BR>");
cgiMakeRadioButton(radioName, "redBlueOnYellow", sameString(colorSetting, "redBlueOnYellow"));
puts("red/blue on yellow background<BR>");
}

void expRatioSubsetDropDown(struct maGrouping *ss, char *varName, char *offset)
/* because setting up the droplist is a bit involved... this is just called */
/* from expRatioSubsetOptions() */
{
char **menu;
char **values;
int i;
AllocArray(menu, ss->numGroups);
AllocArray(values, ss->numGroups);
for (i = 0; i < ss->numGroups; i++)
    {
    char num[4];
    safef(num, sizeof(num), "%d", i);
    menu[i] = cloneString(ss->names[i]);
    values[i] = cloneString(num);
    }
cgiMakeDropListWithVals(varName, menu, values, ss->numGroups, offset);
}

void expRatioSubsetOptions(struct trackDb *tdb, struct microarrayGroups *groups)
/* subsetting options for a microarray track */
{
char *radioVarName = expRatioSubsetRadioName(tdb->track, groups);
char *subSetting = NULL;
struct maGrouping *subsets = groups->subsetSettings;
struct maGrouping *ss;
subSetting = cartUsualString(cart, radioVarName, NULL);
puts("<BR><B>Subset:</B><BR>");
cgiMakeRadioButton(radioVarName, "none", (subSetting == NULL) || sameString(subSetting, "none"));
puts("no subset<BR>\n");
for (ss = subsets; ss != NULL; ss = ss->next)
    {
    char *dropVarName = expRatioSubsetDLName(tdb->track, ss);
    char *offS = NULL;
    offS = cartUsualString(cart, dropVarName, "-1");
    cgiMakeRadioButton(radioVarName, ss->name, (subSetting) && sameString(ss->name, subSetting));
    printf("%s \n", ss->description);
    expRatioSubsetDropDown(ss, dropVarName, offS);
    printf("<BR>\n");
    }
}

void expRatioUi(struct trackDb *tdb)
/* UI options for the expRatio tracks. */
{
struct microarrayGroups *groups = maGetTrackGroupings(database, tdb);
if (groups == NULL)
    errAbort("Could not get group settings for track.");
expRatioDrawExonOption(tdb);
if (groups->numCombinations > 0)
    expRatioCombineDropDown(tdb, groups);
if (groups->numSubsets > 0)
    expRatioSubsetOptions(tdb, groups);
expRatioColorOption(tdb);
}

void expRatioCtUi(struct trackDb *tdb)
/* UI options for array custom tracks. */
{
expRatioColorOption(tdb);
}

void affyAllExonUi(struct trackDb *tdb)
/* put up UI for the affy all exon tracks. */
{
char *affyAllExonMap;
char *col;
char varName[128];

safef(varName, sizeof(varName), "%s.%s", tdb->track, "type");
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

void switchDbScoreUi(struct trackDb *tdb)
/* Put up UI for filtering switchDb track based on a score */
/* The scores use a drop-box to set scoreFilter at several */
/* thresholds. */
{
char *option = "switchDbTss.scoreFilter";
char *pseudo = "switchDbTss.pseudo";
char *menu[3] = {"All TSSs (no filter)", "Lower stringency (score >= 10)", "Higher stringency (score >= 20)"};
char *values[3] = {"0", "10", "20"};
char *scoreValString = trackDbSetting(tdb, "scoreFilter");
int scoreSetting;
int scoreVal = SWITCHDBTSS_FILTER;
char tempScore[256];
if (scoreValString != NULL)
    scoreVal = atoi(scoreValString);
printf("<p><b>Filter TSSs by score:</b> ");
scoreSetting = cartUsualInt(cart,  option,  scoreVal);
safef(tempScore, sizeof(tempScore), "%d",scoreSetting);
cgiMakeDropListWithVals(option, menu, values,
			ArraySize(menu), tempScore);
printf("<p><b>Include TSSs for predicted pseudogenes</b> ");
cartMakeCheckBox(cart, pseudo, FALSE);
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

safef(geneName, sizeof(geneName), "%s.geneLabel", tdb->track);
safef(accName, sizeof(accName), "%s.accLabel", tdb->track);
safef(sprotName, sizeof(sprotName), "%s.sprotLabel", tdb->track);
safef(posName, sizeof(posName), "%s.posLabel", tdb->track);
useGene= cartUsualBoolean(cart, geneName, TRUE);
useAcc= cartUsualBoolean(cart, accName, FALSE);
useSprot= cartUsualBoolean(cart, sprotName, FALSE);
usePos= cartUsualBoolean(cart, posName, FALSE);

safef(cModeStr, sizeof(cModeStr), "%s.cmode", tdb->track);
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

baseColorDrawOptDropDown(cart, tdb);
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

safef(geneName, sizeof(geneName), "%s.geneLabel", tdb->track);
safef(accName, sizeof(accName), "%s.accLabel", tdb->track);
safef(sprotName, sizeof(sprotName), "%s.sprotLabel", tdb->track);
safef(posName, sizeof(posName), "%s.posLabel", tdb->track);
useGene= cartUsualBoolean(cart, geneName, TRUE);
useAcc= cartUsualBoolean(cart, accName, FALSE);
usePos= cartUsualBoolean(cart, posName, FALSE);

safef(cModeStr, sizeof(cModeStr), "%s.cmode", tdb->track);
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

baseColorDrawOptDropDown(cart, tdb);
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

safef(geneName, sizeof(geneName), "%s.geneLabel", tdb->track);
safef(accName, sizeof(accName), "%s.accLabel", tdb->track);
safef(sprotName, sizeof(sprotName), "%s.sprotLabel", tdb->track);
safef(posName, sizeof(posName), "%s.posLabel", tdb->track);
useGene= cartUsualBoolean(cart, geneName, TRUE);
useAcc= cartUsualBoolean(cart, accName, FALSE);
useSprot= cartUsualBoolean(cart, sprotName, FALSE);
usePos= cartUsualBoolean(cart, posName, FALSE);

safef(cModeStr, sizeof(cModeStr), "%s.cmode", tdb->track);
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

baseColorDrawOptDropDown(cart, tdb);
}

void hg17KgIdConfig(struct trackDb *tdb)
/* Put up gene ID track controls */
{
char varName[64];
char *geneLabel;
safef(varName, sizeof(varName), "%s.label", tdb->track);
geneLabel = cartUsualString(cart, varName, "gene symbol");
printf("<B>Label:</B> ");
radioButton(varName, geneLabel, "gene symbol");
radioButton(varName, geneLabel, "UCSC Known Gene ID");
radioButton(varName, geneLabel, "all");
radioButton(varName, geneLabel, "none");
}

void hg17KgUI(struct trackDb *tdb)
/* Put up refGene-specific controls */
{
hg17KgIdConfig(tdb);
baseColorDrawOptDropDown(cart, tdb);
}

void omimLocationConfig(struct trackDb *tdb)
/* Put up OMIM Location track controls */
{
char varName[64];
char *geneLabel;
safef(varName, sizeof(varName), "%s.label", tdb->track);
geneLabel = cartUsualString(cart, varName, "OMIM ID");
printf("<BR><B>Include Entries of:</B> ");
printf("<UL>\n");
printf("<LI>");
labelMakeCheckBox(tdb, "class1", "Phenotype map key 1: the disorder has been placed on the map based on its association with a gene, but the underlying defect is not known.", TRUE);
printf("<LI>");
labelMakeCheckBox(tdb, "class2", "Phenotype map key 2: the disorder has been placed on the map by linkage; no mutation has been found.", TRUE);
printf("<LI>");
labelMakeCheckBox(tdb, "class3", "Phenotype map key 3: the molecular basis for the disorder is known; a mutation has been found in the gene.", TRUE);
printf("<LI>");
labelMakeCheckBox(tdb, "class4", "Phenotype map key 4: a contiguous gene deletion or duplication syndrome; multiple genes are deleted or duplicated causing the phenotype.", TRUE);

// removed the "others" option for the time being
//printf("<LI>");
//labelMakeCheckBox(tdb, "others", "Others: no associated OMIM phenotype map key info available.", TRUE);
printf("</UL>");
}

void omimGene2Config(struct trackDb *tdb)
/* Put up OMIM Genes track controls */
{
char varName[64];
char *geneLabel;
safef(varName, sizeof(varName), "%s.label", tdb->track);
geneLabel = cartUsualString(cart, varName, "OMIM ID");
printf("<BR><B>Include Entries of:</B> ");
printf("<UL>\n");
printf("<LI>");
labelMakeCheckBox(tdb, "class1", "Phenotype map key 1: the disorder has been placed on the map based on its association with a gene, but the underlying defect is not known.", TRUE);
printf("<LI>");
labelMakeCheckBox(tdb, "class2", "Phenotype map key 2: the disorder has been placed on the map by linkage; no mutation has been found.", TRUE);
printf("<LI>");
labelMakeCheckBox(tdb, "class3", "Phenotype map key 3: the molecular basis for the disorder is known; a mutation has been found in the gene.", TRUE);
printf("<LI>");
labelMakeCheckBox(tdb, "class4", "Phenotype map key 4: a contiguous gene deletion or duplication syndrome; multiple genes are deleted or duplicated causing the phenotype.", TRUE);
printf("<LI>");
labelMakeCheckBox(tdb, "others", "Others: no associated OMIM phenotype map key info available.", TRUE);
printf("</UL>");
}

void omimGeneIdConfig(struct trackDb *tdb)
/* Put up gene ID track controls */
{
char varName[64];
char *geneLabel;
safef(varName, sizeof(varName), "%s.label", tdb->track);
geneLabel = cartUsualString(cart, varName, "OMIM ID");
printf("<BR><B>Label:</B> ");
radioButton(varName, geneLabel, "OMIM ID");
radioButton(varName, geneLabel, "OMIM gene or syndrome");
radioButton(varName, geneLabel, "UCSC gene symbol");
}

void knownGeneIdConfig(struct trackDb *tdb)
/* Put up gene ID track controls */
{
struct sqlConnection *conn = hAllocConn(database);
char query[256];
char *omimAvail = NULL;
safef(query, sizeof(query), "select kgXref.kgID from kgXref,refLink where kgXref.refseq = refLink.mrnaAcc and refLink.omimId != 0 limit 1");
omimAvail = sqlQuickString(conn, query);
hFreeConn(&conn);

printf("<B>Label:</B> ");
labelMakeCheckBox(tdb, "gene", "gene symbol", FALSE);
labelMakeCheckBox(tdb, "kgId", "UCSC Known Gene ID", FALSE);
labelMakeCheckBox(tdb, "prot", "UniProt Display ID", FALSE);

if (omimAvail != NULL)
    {
    char sym[32];
    safef(sym, sizeof(sym), "omim%s", cartString(cart, "db"));
    labelMakeCheckBox(tdb, sym, "OMIM ID", FALSE);
    }
printf("<BR>\n");
}

void knownGeneShowWhatUi(struct trackDb *tdb)
/* Put up line of controls that describe what parts to show. */
{
char varName[64];
printf("<B>Show:</B> ");
safef(varName, sizeof(varName), "%s.show.noncoding", tdb->track);
boolean option = cartUsualBoolean(cart, varName, TRUE);
cgiMakeCheckBox(varName, option);
printf(" %s&nbsp;&nbsp;&nbsp;", "non-coding genes");
safef(varName, sizeof(varName), "%s.show.spliceVariants", tdb->track);
option = cartUsualBoolean(cart, varName, TRUE);
cgiMakeCheckBox(varName, option);
printf(" %s&nbsp;&nbsp;&nbsp;", "splice variants");
printf("<BR>\n");
}

void knownGeneUI(struct trackDb *tdb)
/* Put up refGene-specific controls */
{
/* This is incompatible with adding Protein ID to lf->extra */
knownGeneIdConfig(tdb);
knownGeneShowWhatUi(tdb);
baseColorDrawOptDropDown(cart, tdb);
}

void omimLocationUI(struct trackDb *tdb)
/* Put up omimLcation-specific controls */
{
omimLocationConfig(tdb);
}

void omimGene2IdConfig(struct trackDb *tdb)
/* Put up gene ID track controls */
{
printf("<B>Label:</B> ");
labelMakeCheckBox(tdb, "omimId", "OMIM ID", FALSE);
labelMakeCheckBox(tdb, "gene", "gene symbol", FALSE);

printf("<BR>\n");
}
void omimGene2UI(struct trackDb *tdb)
/* Put up omimGene2-specific controls */
{
omimGene2IdConfig(tdb);
omimGene2Config(tdb);
}

void omimGeneUI(struct trackDb *tdb)
/* Put up omimGene-specific controls */
{
omimGeneIdConfig(tdb);
}

void geneIdConfig(struct trackDb *tdb)
/* Put up gene ID track controls */
{
char varName[64];
char *geneLabel;
safef(varName, sizeof(varName), "%s.label", tdb->track);
geneLabel = cartUsualString(cart, varName, "gene");
printf("<B>Label:</B> ");
radioButton(varName, geneLabel, "gene");
radioButton(varName, geneLabel, "accession");
radioButton(varName, geneLabel, "both");
radioButton(varName, geneLabel, "none");
}

static void hideNoncodingOpt(struct trackDb *tdb)
/* Put up hide-noncoding options. */
{
/* Put up option to hide non-coding elements. */
printf("<B>Hide non-coding genes:</B> ");
char varName[64];
safef(varName, sizeof(varName), "%s.%s", tdb->track, HIDE_NONCODING_SUFFIX);
cartMakeCheckBox(cart, varName, HIDE_NONCODING_DEFAULT);
}

void rgdGene2UI(struct trackDb *tdb)
/* Put up rgdGene2 gene ID track controls, with checkboxes */
{
/* Put up label line  - boxes for gene and accession. */
printf("<B>Label:</B> ");
labelMakeCheckBox(tdb, "gene", "gene", FALSE);
labelMakeCheckBox(tdb, "acc", "accession", FALSE);
printf("<BR>\n");

baseColorDrawOptDropDown(cart, tdb);
}

void refGeneUI(struct trackDb *tdb)
/* Put up refGene or xenoRefGene gene ID track controls, with checkboxes */
{
/* Figure out if OMIM database is available. */
int omimAvail = 0;
if (sameString(tdb->track, "refGene"))
    {
    struct sqlConnection *conn = hAllocConn(database);
    char query[128];
    safef(query, sizeof(query), "select refLink.omimId from refLink, refGene where refLink.mrnaAcc = refGene.name and refLink.omimId != 0 limit 1");
    omimAvail = sqlQuickNum(conn, query);
    hFreeConn(&conn);
    }

/* Put up label line  - boxes for gene, accession or maybe OMIM. */
printf("<B>Label:</B> ");
labelMakeCheckBox(tdb, "gene", "gene", FALSE);
labelMakeCheckBox(tdb, "acc", "accession", FALSE);
if (omimAvail != 0)
    {
    char sym[32];
    safef(sym, sizeof(sym), "omim%s", cartString(cart, "db"));
    labelMakeCheckBox(tdb, sym, "OMIM ID", FALSE);
    }
printf("<BR>\n");

/* Put up noncoding option and codon coloring stuff. */
hideNoncodingOpt(tdb);
baseColorDrawOptDropDown(cart, tdb);
}

void transMapUI(struct trackDb *tdb)
/* Put up transMap-specific controls */
{
printf("<B>Label:</B> ");
labelMakeCheckBox(tdb, "orgCommon", "common name", FALSE);
labelMakeCheckBox(tdb, "orgAbbrv", "organism abbreviation", FALSE);
labelMakeCheckBox(tdb, "db", "assembly database", FALSE);
labelMakeCheckBox(tdb, "gene", "gene", FALSE);
labelMakeCheckBox(tdb, "acc", "accession", FALSE);

baseColorDrawOptDropDown(cart, tdb);
indelShowOptions(cart, tdb);
}

void retroGeneUI(struct trackDb *tdb)
/* Put up retroGene-specific controls */
{
printf("<B>Label:</B> ");
labelMakeCheckBox(tdb, "gene", "gene", FALSE);
labelMakeCheckBox(tdb, "acc", "accession", FALSE);

baseColorDrawOptDropDown(cart, tdb);
}

void vegaGeneUI(struct trackDb *tdb)
/* Put up Vega Gene track-specific controls */
{
geneIdConfig(tdb);
printf("<BR>\n");

/* Put up codon coloring stuff. */
baseColorDrawOptDropDown(cart, tdb);
}

void ensemblNonCodingTypeConfig(struct trackDb *tdb)
{
int i = 0;

printf("<BR><B>Non-coding RNA type:</B> ");
printf("<BR>\n");

for (i=0; i < nonCodingTypeLabelsSize; i++)
    {
    nonCodingTypeIncludeCart[i] = cartUsualBoolean(cart, nonCodingTypeIncludeStrings[i], nonCodingTypeIncludeDefault[i]);
    cgiMakeCheckBox(nonCodingTypeIncludeStrings[i], nonCodingTypeIncludeCart[i]);
    printf(" %s", nonCodingTypeLabels[i]);
    }
}

void ensemblNonCodingUI(struct trackDb *tdb)
/* Put up Ensembl Non-Coding genes-specific controls */
{
ensemblNonCodingTypeConfig(tdb);
}

void mrnaUi(struct trackDb *tdb, boolean isXeno)
/* Put up UI for an mRNA (or EST) track. */
{
struct mrnaUiData *mud = newMrnaUiData(tdb->track, isXeno);
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
    oneMrnaFilterUi(cg, fil->label, fil->key, cart);
endControlGrid(&cg);
baseColorDrawOptDropDown(cart, tdb);
indelShowOptions(cart, tdb);
}



static void filterByChrom(struct trackDb *tdb)
{
char *filterSetting;
char filterVar[256];
char *filterVal = "";

printf("<p><b>Filter by chromosome (e.g. chr10):</b> ");
snprintf(filterVar, sizeof(filterVar), "%s.chromFilter", tdb->track);
filterSetting = cartUsualString(cart, filterVar, filterVal);
cgiMakeTextVar(filterVar, cartUsualString(cart, filterVar, ""), 15);
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
snprintf(colorVar, sizeof(colorVar), "%s.color", tdb->track);
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
printf("%s",
	"<P>The scoring ranges from 0 to 1000 and is based on the number of lines "
	"of evidence that support the motif being active.  Each of the two "
	"<I>sensu stricto</I> species in which the motif was conserved counts "
	"as a line of evidence.  If the ChIP-chip data showed good (P &le; 0.001) "
	"evidence of binding to the transcription factor associated with the "
	"motif, that counts as two lines of evidence.  If the ChIP-chip data "
	"showed weaker (P &le; 0.005) evidence of binding, that counts as just one line "
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
struct sqlConnection *conn = hAllocConn(database);

char newRow = 0;

snprintf( &options[0][0], 256, "%s.heightPer", tdb->track );
snprintf( &options[1][0], 256, "%s.linear.interp", tdb->track );
snprintf( &options[3][0], 256, "%s.fill", tdb->track );
snprintf( &options[4][0], 256, "%s.min.cutoff", tdb->track );
snprintf( &options[5][0], 256, "%s.max.cutoff", tdb->track );
snprintf( &options[6][0], 256, "%s.interp.gap", tdb->track );

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
sr = hRangeQuery(conn, tdb->table, chromosome, 0, 1877426, NULL, &rowOffset);
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
boolean normScoreAvailable = chainDbNormScoreAvailable(tdb);
boolean compositeTrack = tdbIsComposite(tdb);

if (compositeTrack)
    return;	// configuration taken care of by hCompositeUi() later
else if (normScoreAvailable)
    chainCfgUi(database, cart, tdb, tdb->track, NULL, FALSE, chromosome);
else
    crossSpeciesUi(tdb);
}

void chromGraphUi(struct trackDb *tdb)
/* UI for the wiggle track */
{
char varName[chromGraphVarNameMaxSize];
struct sqlConnection *conn = NULL;
char *track = tdb->track;
if (!isCustomTrack(track))
    conn = hAllocConn(database);
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
if (conn)
    chromGraphDataRange(track, conn, &minVal, &maxVal);
else
    {
    char *fileName = trackDbRequiredSetting(tdb, "binaryFile");
    struct chromGraphBin *cgb = chromGraphBinOpen(fileName);
    minVal = cgb->minVal;
    maxVal = cgb->maxVal;
    chromGraphBinFree(&cgb);
    }
printf("\n&nbsp; &nbsp;(range: &nbsp;%g&nbsp;to&nbsp;%g)<BR>",
    minVal, maxVal);

hFreeConn(&conn);
}

void wikiTrackUi(struct trackDb *tdb)
/* UI for wiki track user annotations */
{
return;	// currently empty
}

void rulerUi(struct trackDb *tdb)
/* UI for base position (ruler) */
{
boolean showScaleBar = cartUsualBoolean(cart, BASE_SCALE_BAR, TRUE);
boolean showRuler = cartUsualBoolean(cart, BASE_SHOWRULER, TRUE);
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
puts("<P>");
cgiMakeCheckBox(BASE_SHOWRULER, showRuler);
puts("&nbsp;<B>Show ruler</B></P>");
puts("<P>");
cgiMakeCheckBox(BASE_SCALE_BAR, showScaleBar);
puts("&nbsp;<B>Show scale bar</B>");
puts("<P><B>Zoom factor:&nbsp;</B>");
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

void genericWiggleUi(struct trackDb *tdb, int optionNum )
/* put up UI for any standard wiggle track (a.k.a. sample track)*/
{

char options[7][256];
int thisHeightPer, thisLineGap;
float thisMinYRange, thisMaxYRange;
char *interpolate, *fill;

snprintf( &options[0][0], 256, "%s.heightPer", tdb->track );
snprintf( &options[1][0], 256, "%s.linear.interp", tdb->track );
snprintf( &options[3][0], 256, "%s.fill", tdb->track );
snprintf( &options[4][0], 256, "%s.min.cutoff", tdb->track );
snprintf( &options[5][0], 256, "%s.max.cutoff", tdb->track );
snprintf( &options[6][0], 256, "%s.interp.gap", tdb->track );

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

wigCfgUi(cart,tdb,tdb->track,"<span style='text-decoration:underline;'>Graph Plotting options:</span>",FALSE);
printf("<p><b><span style='text-decoration:underline;'>View/Hide individual cell lines:</span></b>");
}

void humMusUi(struct trackDb *tdb, int optionNum )
/* put up UI for human/mouse conservation sample tracks (humMusL and musHumL)*/
{

char options[7][256];
int thisHeightPer, thisLineGap;
float thisMinYRange, thisMaxYRange;
char *interpolate, *fill;

snprintf( &options[0][0], 256, "%s.heightPer", tdb->track );
snprintf( &options[1][0], 256, "%s.linear.interp", tdb->track );
snprintf( &options[3][0], 256, "%s.fill", tdb->track );
snprintf( &options[4][0], 256, "%s.min.cutoff", tdb->track );
snprintf( &options[5][0], 256, "%s.max.cutoff", tdb->track );
snprintf( &options[6][0], 256, "%s.interp.gap", tdb->track );

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

void ucsfdemoUi(struct trackDb *tdb)
{
char **menu;
int menuSize = 0;
int menuPos = 0;

puts("<BR><B>ER filter:</B>&nbsp;");
menuSize = 3;
menu = needMem((size_t)(menuSize * sizeof(char *)));
menuPos = 0;
menu[menuPos++] = "no filter";
menu[menuPos++] = "pos";
menu[menuPos++] = "neg";
cgiMakeDropList(UCSF_DEMO_ER, menu, menuSize,
    cartCgiUsualString(cart, UCSF_DEMO_ER, UCSF_DEMO_ER_DEFAULT));
freez(&menu);

puts("<BR><B>PR filter:</B>&nbsp;");
menuSize = 3;
menu = needMem((size_t)(menuSize * sizeof(char *)));
menuPos = 0;
menu[menuPos++] = "no filter";
menu[menuPos++] = "pos";
menu[menuPos++] = "neg";
cgiMakeDropList(UCSF_DEMO_PR, menu, menuSize,
    cartCgiUsualString(cart, UCSF_DEMO_PR, UCSF_DEMO_PR_DEFAULT));
freez(&menu);

}

void hapmapSnpsUi(struct trackDb *tdb)
/* Options for filtering hapmap snps */
/* Default is always to not filter (include all data) */
/* snp track puts the menu options in kent/src/hg/lib/snp125Ui.h */
/* snp track also sets global variables, don't actually need this? */
/* Consider using radio buttons */
{
struct sqlConnection *conn = hAllocConn(database);
boolean isPhaseIII = sameString(trackDbSettingOrDefault(tdb, "hapmapPhase", "II"), "III");

if ((isPhaseIII && !sqlTableExists(conn, "hapmapPhaseIIISummary")) ||
    (!isPhaseIII & !sqlTableExists(conn, "hapmapAllelesSummary")))
    return;

puts("<P>");
puts("<B>Display filters (applied to all subtracks):</B>");
puts("<BR>\n");

puts("<BR><B>Population availability:</B>&nbsp;");
static char *popAvailMenuPhaseIII[] =
    { "no filter",
      "all 11 Phase III populations",
      "all 4 Phase II populations" };
static char *popAvailMenuPhaseII[] =
    { "no filter",
      "all 4 populations",
      "1-3 populations" };
char **menu = isPhaseIII ? popAvailMenuPhaseIII : popAvailMenuPhaseII;
cgiMakeDropList(HAP_POP_COUNT, menu, 3,
		cartUsualString(cart, HAP_POP_COUNT, HAP_FILTER_DEFAULT));

puts("<BR><B>Major allele mixture between populations:</B>&nbsp;");
static char *mixedMenu[] = { "no filter", "mixed", "not mixed" };
cgiMakeDropList(HAP_POP_MIXED, mixedMenu, 3,
		cartUsualString(cart, HAP_POP_MIXED, HAP_FILTER_DEFAULT));

puts("<BR><B>Monomorphism:</B><BR>");
static char *noYesNoMenu[] = { "no filter", "yes", "no" };
char **pops = isPhaseIII ? hapmapPhaseIIIPops : hapmapPhaseIIPops;
int popCount = isPhaseIII ? HAP_PHASEIII_POPCOUNT : HAP_PHASEII_POPCOUNT;
puts("<TABLE BORDERWITH=0>");
int cellCount = 0, i;
char cartVar[128];
for (i = 0;  i < popCount;  i++)
    {
    char table[HDB_MAX_TABLE_STRING];
    if (endsWith(tdb->track, "PhaseII"))
	safef(table, sizeof(table), "hapmapSnps%sPhaseII", pops[i]);
    else
	safef(table, sizeof(table), "hapmapSnps%s", pops[i]);
    if (sqlTableExists(conn, table))
	{
	if (cellCount == 0)
	    puts("<TR>");
	printf("<TD align=right><B>%s:</B></TD><TD>", pops[i]);
	safef(cartVar, sizeof(cartVar), "%s_%s", HAP_MONO_PREFIX, pops[i]);
	cgiMakeDropList(cartVar, noYesNoMenu, 3,
			cartUsualString(cart, cartVar, HAP_FILTER_DEFAULT));
	printf("</TD>\n");
	cellCount += 2;
	if (cellCount == 12)
	    {
	    puts("</TR>");
	    cellCount = 0;
	    }
	}
    }
if (cellCount != 0)
    {
    while (cellCount++ < 12)
	puts("<TD></TD>");
    puts("</TR>");
    }
puts("</TABLE>");

puts("<BR><B>Polymorphism type:</B>&nbsp;");
static char *typeMenu[] = { "no filter", "bi-allelic", "transition", "transversion", "complex" };
cgiMakeDropList(HAP_TYPE, typeMenu, 5,
		cartUsualString(cart, HAP_TYPE, HAP_FILTER_DEFAULT));

puts("<BR><BR><B>Minor allele frequency in any population:  min:</B>&nbsp;");
float minFreq = atof(cartUsualString(cart, HAP_MIN_FREQ, HAP_MIN_FREQ_DEFAULT));
cgiMakeDoubleVar(HAP_MIN_FREQ, minFreq, 6);

puts("<B>max:</B>&nbsp;");
float maxFreq = atof(cartUsualString(cart, HAP_MAX_FREQ, HAP_MAX_FREQ_DEFAULT));
cgiMakeDoubleVar(HAP_MAX_FREQ, maxFreq, 6);
puts("&nbsp;(range: 0.0 to 0.5)\n");

if (isPhaseIII)
    puts("<BR><B>Average of populations' observed heterozygosities: </B>");
else
    puts("<BR><B>Expected heterozygosity (from total allele frequencies): </B>");
puts("<B>min:</B>&nbsp;");
float minHet = atof(cartUsualString(cart, HAP_MIN_HET, HAP_MIN_HET_DEFAULT));
cgiMakeDoubleVar(HAP_MIN_HET, minHet, 6);

puts("<B>max:</B>&nbsp;");
if (isPhaseIII)
    {
    float maxHet = atof(cartUsualString(cart, HAP_MAX_OBSERVED_HET, HAP_MAX_OBSERVED_HET_DEFAULT));
    cgiMakeDoubleVar(HAP_MAX_OBSERVED_HET, maxHet, 6);
    puts("&nbsp;(range: 0.0 to 1.0)\n");
    }
else
    {
    float maxHet = atof(cartUsualString(cart, HAP_MAX_EXPECTED_HET, HAP_MAX_EXPECTED_HET_DEFAULT));
    cgiMakeDoubleVar(HAP_MAX_EXPECTED_HET, maxHet, 6);
    puts("&nbsp;(range: 0.0 to 0.5)\n");
    }

static char *orthoMenu[] =
    { "no filter",
      "available",
      "matches major human allele",
      "matches minor human allele",
      "matches neither human allele" };

puts("<P><TABLE>");
for (i = 0;  hapmapOrthoSpecies[i] != NULL;  i++)
    {
    printf("<TR><TD><B>%s allele:</B></TD>\n<TD>", hapmapOrthoSpecies[i]);
    safef(cartVar, sizeof(cartVar), "%s_%s", HAP_ORTHO_PREFIX, hapmapOrthoSpecies[i]);
    cgiMakeDropList(cartVar, orthoMenu, 5,
		    cartUsualString(cart, cartVar, HAP_FILTER_DEFAULT));
    puts("</TD>");
    safef(cartVar, sizeof(cartVar), "%s_%s", HAP_ORTHO_QUAL_PREFIX, hapmapOrthoSpecies[i]);
    int minQual = atoi(cartUsualString(cart, cartVar, HAP_ORTHO_QUAL_DEFAULT));
    puts("<TD><B>Minimum quality score:</B></TD>\n<TD>");
    cgiMakeIntVar(cartVar, minQual, 4);
    puts("&nbsp;(range: 0 to 100)</TD></TR>\n");
    }
puts("</TABLE>");
puts("</P>\n");
printf("<B>Select subtracks to display:</B><BR>\n");
hFreeConn(&conn);
}

void pcrResultUi(struct trackDb *tdb)
/* Result from hgPcr query. */
{
struct targetDb *target;
if (! pcrResultParseCart(database, cart, NULL, NULL, &target))
    return;
if (target != NULL)
    {
    char *chosen = cartUsualString(cart, PCR_RESULT_TARGET_STYLE,
				   PCR_RESULT_TARGET_STYLE_DEFAULT);
    cgiMakeRadioButton(PCR_RESULT_TARGET_STYLE, PCR_RESULT_TARGET_STYLE_TRIM,
		       sameString(chosen, PCR_RESULT_TARGET_STYLE_TRIM));
    printf("Show only the amplified part of %s item&nbsp;&nbsp;&nbsp;\n",
	   target->description);
    cgiMakeRadioButton(PCR_RESULT_TARGET_STYLE, PCR_RESULT_TARGET_STYLE_TALL,
		       sameString(chosen, PCR_RESULT_TARGET_STYLE_TALL));
    printf("Show the whole %s item with amplified part tall",
	   target->description);
    }
baseColorDrawOptDropDown(cart, tdb);
}

void dgvUi(struct trackDb *tdb)
/* Database of Genomic Variants: filter by publication. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
safef(query, sizeof(query),
      "select reference,pubMedId from %s group by pubMedId order by reference;", tdb->table);
sr = sqlGetResult(conn, query);
printf("<BR><B>Filter by publication reference:</B>\n");
char cartVarName[256];
safef (cartVarName, sizeof(cartVarName), "hgt_%s_filterType", tdb->track);
boolean isInclude = sameString("include", cartUsualString(cart, cartVarName, "include"));
cgiMakeRadioButton(cartVarName, "include", isInclude);
printf("include\n");
cgiMakeRadioButton(cartVarName, "exclude", !isInclude);
printf("exclude<BR>\n");
safef (cartVarName, sizeof(cartVarName), "hgt_%s_filterPmId", tdb->track);
boolean filterPmIdInCart = cartListVarExists(cart, cartVarName);
struct slName *checked = NULL;
if (filterPmIdInCart)
    checked = cartOptionalSlNameList(cart, cartVarName);
#define MAX_DGV_REFS 128
char *labelArr[MAX_DGV_REFS], *valueArr[MAX_DGV_REFS];
int refCount = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *ref = row[0];
    char *pmId = row[1];
    char label[512];
    safef(label, sizeof(label),
	  "<A HREF=\"http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?cmd=Retrieve&db=PubMed"
	  "&list_uids=%s&dopt=Abstract&tool=genome.ucsc.edu\" TARGET=_BLANK>%s</A>", pmId, ref);
    labelArr[refCount] = cloneString(label);
    valueArr[refCount++] = cloneString(pmId);
    if (! filterPmIdInCart)
	slNameAddHead(&checked, pmId);
    if (refCount >= MAX_DGV_REFS)
	errAbort("dgvUi: %s has too many references (max %d)", tdb->track, MAX_DGV_REFS);
    }
sqlFreeResult(&sr);
jsMakeCheckboxGroupSetClearButton(cartVarName, TRUE);
jsMakeCheckboxGroupSetClearButton(cartVarName, FALSE);
cgiMakeCheckboxGroupWithVals(cartVarName, labelArr, valueArr, refCount, checked, 4);
hFreeConn(&conn);
}

#ifdef UNUSED
static boolean isInTrackList(struct trackDb *tdbList, struct trackDb *target)
/* Return TRUE if target is in tdbList. */
{
struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    if (tdb == target)
        return TRUE;
return FALSE;
}
#endif /* UNUSED */

void superTrackUi(struct trackDb *superTdb, struct trackDb *tdbList)
/* List tracks in this collection, with visibility controls and UI links */
{
printf("<P><TABLE CELLPADDING=2>");
tdbRefSortPrioritiesFromCart(cart, &superTdb->children);
struct slRef *childRef;
for (childRef = superTdb->children; childRef != NULL; childRef = childRef->next)
    {
    struct trackDb *tdb = childRef->val;
    printf("<TR><TD NOWRAP>");
    if (tdbIsDownloadsOnly(tdb))
        printf("%s&nbsp;",tdb->shortLabel);
    else
        printf("<A HREF='%s?%s=%u&c=%s&g=%s'>%s</A>&nbsp;",
                (tdbIsDownloadsOnly(tdb)? hgFileUiName(): hgTrackUiName()),
                cartSessionVarName(), cartSessionId(cart),
                chromosome, cgiEncode(tdb->track), tdb->shortLabel);
    printf("</TD><TD>");
    if (tdbIsDownloadsOnly(tdb))
        {
        printf("<A HREF='%s?%s=%u&g=%s'>Downloads</A>",
                hgFileUiName(),cartSessionVarName(), cartSessionId(cart), cgiEncode(tdb->track));
        }
    else
        {
        enum trackVisibility tv =
                        hTvFromString(cartUsualString(cart, tdb->track,
                                                hStringFromTv(tdb->visibility)));
        hTvDropDownClassVisOnly(tdb->track, tv, tdb->canPack,
                                tv == tvHide ?  "hiddenText" : "normalText",
                                trackDbSetting(tdb, "onlyVisibility"));
        }
    printf("<TD>%s", tdb->longLabel);
    char *dataVersion = trackDbSetting(tdb, "dataVersion");
    if (dataVersion)
        printf("&nbsp&nbsp;<EM style='color:#666666; font-size:smaller;'>%s</EM>", dataVersion);
    printf("</TD></TR>");
    }
printf("</TABLE>");
}

void previewLinks(char *db, struct trackDb *tdb)
/* Informational messages about preview browser (ENCODE tracks only) */
{
if (trackDbSetting(tdb, "wgEncode") != NULL)
    {
    if (hIsPreviewHost())
        {
        printf("<p><b>WARNING</b>: This data is provided for early access via the Preview Browser -- it is unreviewed and subject to change. For high quality reviewed annotations, see the <a target=_blank href='http://%s/cgi-bin/hgTracks?db=%s'>Genome Browser</a>.",
            "genome.ucsc.edu", db);
        }
    else
        {
        // TODO: use hTrackUiName()
        printf("<p><b>NOTE</b>: Early access to additional track data may be available on the <a target=_blank href='http://%s/cgi-bin/hgTrackUi?db=%s&g=%s'>Preview Browser</A>.",
            "genome-preview.ucsc.edu", db, tdb->track);
        }
    }
}

void specificUi(struct trackDb *tdb, struct trackDb *tdbList, struct customTrack *ct, boolean ajax)
/* Draw track specific parts of UI. */
{
char *track = tdb->track;

if (sameString(track, "stsMap"))
        stsMapUi(tdb);
else if (sameString(track, "affyTxnPhase2"))
    affyTxnPhase2Ui(tdb);
else if (sameString(track, "cgapSage"))
    cgapSageUi(tdb);
else if (sameString(track, "stsMapMouseNew"))
        stsMapMouseUi(tdb);
else if (sameString(track, "stsMapRat"))
        stsMapRatUi(tdb);
else if (sameString(track, "snpMap"))
        snpMapUi(tdb);
else if (sameString(track, "snp"))
        snpUi(tdb);
else if (snpVersion(track) >= 125)
	snp125Ui(tdb);
else if (sameString(track, "rertyHumanDiversityLd") ||
	 startsWith("hapmapLd", track) ||
	 sameString(tdb->type, "ld2"))
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
else if (sameString(track, "cghNci60"))
        cghNci60Ui(tdb);
else if (sameString(track, "xenoRefGene"))
        refGeneUI(tdb);
else if (startsWith("transMapAln", track))
        transMapUI(tdb);
else if (sameString(track, "refGene"))
        refGeneUI(tdb);
else if (sameString(track, "rgdGene2"))
        rgdGene2UI(tdb);
else if (sameString(track, "knownGene"))
        knownGeneUI(tdb);
else if (sameString(track, "omimLocation"))
        omimLocationUI(tdb);
else if (sameString(track, "omimGene2"))
        omimGene2UI(tdb);
else if (sameString(track, "omimGene"))
        omimGeneUI(tdb);
else if (sameString(track, "hg17Kg"))
        hg17KgUI(tdb);
else if (startsWith("ucscRetro", track) || startsWith("retroMrnaInfo", track))
        retroGeneUI(tdb);
else if (sameString(track, "ensGeneNonCoding"))
        ensemblNonCodingUI(tdb);
else if (sameString(track, "vegaGeneComposite"))
        vegaGeneUI(tdb);
else if (sameString(track, "all_mrna"))
    mrnaUi(tdb, FALSE);
else if (sameString(track, "mrna"))
    mrnaUi(tdb, FALSE);
else if (sameString(track, "splicesP"))
    bedUi(tdb, cart, "", FALSE);
else if(sameString(track,"FantomCageBedForward") || sameString(track,"FantomCageBedReverse") ||
	sameString(track,"FantomCageReadForward") || sameString(track,"FantomCageReadReverse"))
    bedUi(tdb, cart, "", FALSE);
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
        || sameString(track, "blastCe3WB") || sameString(track, "blastHg18KG")
        || sameString(track, "blatzHg17KG")|| startsWith("mrnaMap", track)|| startsWith("mrnaXeno", track))
        blastUi(tdb);
else if (sameString(track, "hgPcrResult"))
    pcrResultUi(tdb);
else if (startsWith("bedGraph", tdb->type) || startsWith("bigWig", tdb->type))
    wigCfgUi(cart,tdb,tdb->track,NULL, FALSE);
else if (startsWith("bamWig", tdb->type))
    wigCfgUi(cart,tdb,tdb->track,NULL, FALSE);
else if (startsWith("wig", tdb->type))
        {
        if (startsWith("wigMaf", tdb->type))
            wigMafCfgUi(cart, tdb, tdb->track, NULL, FALSE, database);
        else
            wigCfgUi(cart,tdb,tdb->track, NULL, FALSE);
        }
else if (startsWith("chromGraph", tdb->type))
        chromGraphUi(tdb);
/* else if (sameString(track, "affyHumanExon")) */
/*         affyAllExonUi(tdb); */
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
else if (startsWith("net", track))
    netAlignCfgUi(database, cart, tdb, tdb->track, NULL, FALSE);
else if (startsWith("chain", track) || endsWith("chainSelf", track))
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
else if (sameString(track, WIKI_TRACK_TABLE))
    wikiTrackUi(tdb);
else if (sameString(track, RULER_TRACK_NAME))
    rulerUi(tdb);
else if (sameString(track, OLIGO_MATCH_TRACK_NAME))
    oligoMatchUi(tdb);
else if (sameString(track, CUTTERS_TRACK_NAME))
    cutterUi(tdb);
else if(sameString(track, "affyTransfrags"))
    affyTransfragUi(tdb);
else if (sameString(track, "gvPos"))
    gvUi(tdb);
else if (sameString(track, "oreganno"))
    oregannoUi(tdb);
else if (startsWith("retroposons", track))
    retroposonsUi(tdb);
else if (sameString(track, "tfbsConsSites"))
    tfbsConsSitesUi(tdb);
else if (sameString(track, "CGHBreastCancerUCSF"))
    ucsfdemoUi(tdb);
else if (startsWith("hapmapSnps", track))
    hapmapSnpsUi(tdb);
else if (sameString(track, "switchDbTss"))
    switchDbScoreUi(tdb);
else if (sameString(track, "dgv") || (startsWith("dgvV", track) && isdigit(track[4])))
    dgvUi(tdb);
#ifdef USE_BAM
else if (sameString(tdb->type, "bam"))
    bamCfgUi(cart, tdb, track, NULL, FALSE);
#endif
else if (sameString(tdb->type, "vcfTabix"))
    vcfCfgUi(cart, tdb, track, NULL, FALSE);
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
            genePredCfgUi(cart,tdb,tdb->track,NULL,FALSE);
            }
        else if(sameWord(words[0], "encodePeak") || sameWord(words[0], "narrowPeak")
             || sameWord(words[0], "broadPeak")  || sameWord(words[0], "gappedPeak"))
	       {
	       encodePeakUi(tdb, ct);
	       }
        else if (sameWord(words[0], "expRatio"))
	       {
	       expRatioUi(tdb);
	       }
        else if (sameWord(words[0], "array")) /* not quite the same as an "expRatio" type (custom tracks) */
            {
            expRatioCtUi(tdb);
            }
        /* if bed has score then show optional filter based on score */
        else if (sameWord(words[0], "bed") && wordCount == 3)
            {
            /* Note: jaxQTL3 is a bed 8 format track because of
                thickStart/thickStart, but there is no valid score.
                Similarly, the score field for wgRna track is no long used either.
                It originally was usd to depict different RNA types.  But the new
                wgRna table has a new field 'type', which is used to store RNA
                type info and from which to determine the display color of each entry.
            */
            int bedFieldCount = atoi(words[1]);
            if ((bedFieldCount >= 5 || trackDbSetting(tdb, "scoreMin") != NULL)
            &&  !sameString(track, "jaxQTL3") && !sameString(track, "wgRna")
            &&  !startsWith("encodeGencodeIntron", track))
                {
                cfgByCfgType(cfgBedScore,database, cart, tdb,tdb->track, NULL, trackDbSettingClosestToHomeOn(tdb, "boxedCfg"));
                }
            }
        else if (sameWord(words[0], "bed5FloatScore") || sameWord(words[0], "bed5FloatScoreWithFdr"))
            scoreCfgUi(database, cart,tdb,tdb->track,NULL,1000,FALSE);
        else if (sameWord(words[0], "psl"))
            {
            if (wordCount == 3)
            if (sameWord(words[1], "xeno"))
                crossSpeciesUi(tdb);
            baseColorDrawOptDropDown(cart, tdb);
	    indelShowOptions(cart, tdb);
            }
	else if (sameWord(words[0], "factorSource"))
	    {
	    printf("<BR><B>Cell Abbreviations:</B><BR>\n");
	    char *sourceTable = trackDbRequiredSetting(tdb, "sourceTable");
	    struct sqlConnection *conn = hAllocConn(database);
	    hPrintAbbreviationTable(conn, sourceTable, "Cell Type");
	    hFreeConn(&conn);
	    }
        }
        freeMem(typeLine);
    }
if (tdbIsSuperTrack(tdb))
    {
    superTrackUi(tdb, tdbList);
    }
else if (tdbIsComposite(tdb))  // for the moment generalizing this to include other containers...
    {
    hCompositeUi(database, cart, tdb, NULL, NULL, MAIN_FORM, trackHash);
    }
if (!ajax)
    {
    previewLinks(database, tdb);
    extraUiLinks(database,tdb, trackHash);
    }
}

#ifdef UNUSED
static void findSuperChildrenAndSettings(struct trackDb *tdbList, struct trackDb *super)
/* Find the tracks that have super as a parent and stuff references to them on
 * super's children list. Also do some visibility and parentName futzing. */
{
struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    if (tdb->parent == super)
        {
	trackDbSuperMemberSettings(tdb);  /* This adds tdb to tdb->parent->children. */
	}
    }
}
#endif /* UNUSED */


void trackUi(struct trackDb *tdb, struct trackDb *tdbList, struct customTrack *ct, boolean ajax)
/* Put up track-specific user interface. */
{
if (!ajax)
    {
    jsIncludeFile("jquery.js", NULL);
    webIncludeResourceFile("jquery-ui.css");
    jsIncludeFile("jquery-ui.js", NULL);
    jsIncludeFile("utils.js",NULL);
#ifdef NEW_JQUERY
    printf("<script type='text/javascript'>var newJQuery=true;</script>\n");
#else///ifndef NEW_JQUERY
    printf("<script type='text/javascript'>var newJQuery=false;</script>\n");
#endif///ndef NEW_JQUERY
    }
#define RESET_TO_DEFAULTS "defaults"
char setting[128];

// NOTE: Currently only composite multi-view tracks because
// reset relies upon all cart vars following naming convention:
//   {track}.{varName}...  ( One exception supported: {track}_sel ).

if (trackDbLocalSetting(tdb, "container"))
    {
    /* For the moment, be a composite... */
    tdbMarkAsComposite(tdb);
    }
if (ajax && cartOptionalString(cart, "descriptionOnly"))
    {
    //struct trackDb *tdbParent = tdbFillInAncestry(cartString(cart, "db"),tdb);
    if (tdb->html != NULL && tdb->html[0] != 0)
        puts(tdb->html);
    else
        {
        struct trackDb *tdbParent = tdb->parent;
        for (;tdbParent && (tdbParent->html == NULL || tdbParent->html[0] == 0); tdbParent = tdbParent->parent )
            ; // Get the first parent that has html
        if (tdbParent != NULL && tdbParent->html != NULL && tdbParent->html[0])
            {
            printf("<h2 style='color:%s'>Retrieved from %s Track...</h2>\n",COLOR_DARKGREEN,tdbParent->shortLabel);
            puts(tdbParent->html);
            }
        else
            printf("<h2>No description found for: %s.</h2>",tdbParent?tdbParent->track:tdb->track);
        }
    cartRemove(cart,"descriptionOnly"); // This is a once only request and should be deleted
    return;
    }
if(tdbIsContainer(tdb))
    {
    safef(setting,sizeof(setting),"%s.%s",tdb->track,RESET_TO_DEFAULTS);
    // NOTE: if you want track vis to not be reset, move to after vis dropdown
    if (1 == cartUsualInt(cart, setting, 0))
        cartRemoveAllForTdbAndChildren(cart,tdb);
    else if (!ajax) // Overkill on !ajax, because ajax shouldn't be called for a composite
        cartTdbTreeReshapeIfNeeded(cart,tdb);
    }

printf("<FORM ACTION=\"%s\" NAME=\""MAIN_FORM"\" METHOD=%s>\n\n",
       hgTracksName(), cartUsualString(cart, "formMethod", "POST"));
cartSaveSession(cart);
if (sameWord(tdb->track,"ensGene"))
    {
    char ensVersionString[256];
    char ensDateReference[256];
    char longLabel[256];
    ensGeneTrackVersion(database, ensVersionString, ensDateReference,
        sizeof(ensVersionString));
    if (ensVersionString[0])
        {
        if (ensDateReference[0] && differentWord("current", ensDateReference))
            safef(longLabel, sizeof(longLabel), "Ensembl Gene Predictions - archive %s - %s", ensVersionString, ensDateReference);
        else
            safef(longLabel, sizeof(longLabel), "Ensembl Gene Predictions - %s", ensVersionString);
        }
    else
        safef(longLabel, sizeof(longLabel), "%s", tdb->longLabel);

    printf("<B style='font-family:serif; font-size:200%%;'>%s%s</B>\n", longLabel, tdbIsSuper(tdb) ? " Tracks" : "");
    }
else
    {
    if (trackDbSetting(tdb, "wgEncode"))
        printf("<A HREF='/ENCODE/index.html'><IMG style='vertical-align:middle;' width=100 src='/images/ENCODE_scaleup_logo.png'><A>");
    printf("<B style='font-family:serif; font-size:200%%;'>%s%s</B>\n", tdb->longLabel, tdbIsSuper(tdb) ? " Tracks" : "");

    }
/* Print link for parent track */
if (!ajax)
    {
    if (tdb->parent)
        {
        char *encodedMapName = cgiEncode(tdb->parent->track);
        printf("&nbsp;&nbsp;<B style='font-family:serif; font-size:100%%;'>(<A HREF=\"%s?%s=%u&c=%s&g=%s\" title='Link to parent track'><IMG height=12 src='../images/ab_up.gif'>%s</A>)</B>",
                    hgTrackUiName(), cartSessionVarName(), cartSessionId(cart),
                    chromosome, encodedMapName, tdb->parent->shortLabel);
        freeMem(encodedMapName);
        }
    else
        {
        struct grp *grp, *grps = hLoadGrps(database);
        for (grp = grps; grp != NULL; grp = grp->next)
            {
            if (sameString(grp->name,tdb->grp))
                {
                printf("&nbsp;&nbsp;<B style='font-family:serif; font-size:100%%;'>(<A HREF=\"%s?%s=%u&c=%s&hgTracksConfigPage=configure&hgtgroup_%s_close=0#%sGroup\" title='%s tracks in track configuration page'><IMG height=12 src='../images/ab_up.gif'>All %s%s</A>)</B>",
                        hgTracksName(), cartSessionVarName(), cartSessionId(cart),chromosome,tdb->grp,tdb->grp,grp->label,grp->label,endsWith(grp->label," Tracks")?"":" tracks");
                break;
                }
            }
        grpFreeList(&grps);
        }
    }
    puts("<BR><BR>");

if (ct && sameString(tdb->type, "maf"))
    tdb->canPack = TRUE;
else if (sameString(tdb->track, WIKI_TRACK_TABLE))
    // special case wikiTrack (there's no trackDb entry); fixes redmine 2395
    tdb->canPack = TRUE;

// Don't bother with vis controls for downloadsOnly
if (!tdbIsDownloadsOnly(tdb))
    {
    /* Display visibility menu */
        if (tdbIsComposite(tdb) && multViewCount(tdb) > 0)
            printf("<B>Maximum&nbsp;display&nbsp;mode:&nbsp;</B>");
        else
            printf("<B>Display&nbsp;mode:&nbsp;</B>");
        if (tdbIsSuper(tdb))
            {
            superTrackDropDown(cart, tdb, 1);
            }
        else
            {
            /* normal visibility control dropdown */
            enum trackVisibility vis = tdb->visibility;
            boolean canPack = tdb->canPack;
            if (ajax)
                {
                vis = tdbVisLimitedByAncestry(cart, tdb, TRUE);  // ajax popups should show currently inherited visability
                if (tdbIsCompositeChild(tdb))
                    canPack = TRUE;
                }
            else
                vis = hTvFromString(cartUsualString(cart,tdb->track, hStringFromTv(vis))); // But hgTrackUi page should show local vis
            hTvDropDownClassVisOnlyAndExtra(tdb->track,vis,
                canPack, "normalText visDD", trackDbSetting(tdb, "onlyVisibility"),
                                (tdb->parent != NULL ?"onchange='return visTriggersHiddenSelect(this);'":NULL));
            }
    if (!ajax)
        {
        printf("&nbsp;");
        cgiMakeButton("Submit", "Submit");

        if(tdbIsComposite(tdb))
            printf("\n&nbsp;&nbsp;<a href='#' onclick='setVarAndPostForm(\"%s\",\"1\",\"mainForm\"); return false;'>Reset to defaults</a>\n",setting);
        }

    if (ct)
        {
        puts("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;");
        cgiMakeButton(CT_DO_REMOVE_VAR, "Remove custom track");
        cgiMakeHiddenVar(CT_SELECTED_TABLE_VAR, tdb->track);
        puts("&nbsp;");
        if (differentString(tdb->type, "chromGraph"))
            {
            char buf[256];
            if(ajax)
                // reference to a separate form doesn't work in modal dialog, so change window.location directly.
                safef(buf, sizeof(buf), "window.location='%s?hgsid=%d&%s=%s';return false;", hgCustomName(), cartSessionId(cart), CT_SELECTED_TABLE_VAR, tdb->track);
            else
                safef(buf, sizeof(buf), "document.customTrackForm.submit();return false;");
            cgiMakeOnClickButton(buf, "Update custom track");
            }
        }
    }

if (!tdbIsSuper(tdb) && !tdbIsDownloadsOnly(tdb) && !ajax)
    {
    // NAVLINKS - For pages w/ matrix, add Description, Subtracks and Downloads links
    if (trackDbSetting(tdb, "dimensions") || (trackDbSetting(tdb, "wgEncode") && tdbIsComposite(tdb)))
        {
        printf("\n&nbsp;&nbsp;<span id='navDown' style='float:right; display:none;'>");
        if (trackDbSetting(tdb, "wgEncode"))
            {
            if (!hIsPreviewHost())
                {
                // TODO: get from hui.c
                printf("<A TARGET=_BLANK HREF='http://%s/cgi-bin/hgTrackUi?db=%s&g=%s' TITLE='Early access to unreviewed new data on the Preview Browser...'>Preview</A>",
                    "genome-preview.ucsc.edu", database, tdb->track);
                }
            printf("&nbsp;&nbsp;");
            makeDownloadsLink(database, tdb, trackHash);
            }
        char *downArrow = "&dArr;";
        enum browserType browser = cgiBrowser();
        if (browser == btIE || browser == btFF)
            downArrow = "&darr;";
        printf("&nbsp;&nbsp;<A HREF='#DISPLAY_SUBTRACKS' TITLE='Jump to subtracks section of page'>Subtracks%s</A>",downArrow);
        printf("&nbsp;&nbsp;<A HREF='#TRACK_HTML' TITLE='Jump to description section of page'>Description%s</A>",downArrow);
        printf("&nbsp;</span>");
        }
    }
if (!tdbIsSuperTrack(tdb) && !tdbIsComposite(tdb))
    puts("<BR>");

if (tdbIsDownloadsOnly(tdb))
    filesDownloadUi(database,cart,tdb);  // Composites without tracks but with files to download are tdb->type: downloadsOnly
else
    specificUi(tdb, tdbList, ct, ajax);
puts("</FORM>");

if (ajax)
    return;

if (ct)
    {
    /* hidden form for custom tracks CGI */
    printf("<FORM ACTION='%s' NAME='customTrackForm'>", hgCustomName());
    cartSaveSession(cart);
    cgiMakeHiddenVar(CT_SELECTED_TABLE_VAR, tdb->track);
    puts("</FORM>\n");
    if (ct->bbiFile)
	{
	time_t timep = bbiUpdateTime(ct->bbiFile);
	printBbiUpdateTime(&timep);
	}
    else
	printUpdateTime(CUSTOM_TRASH, ct->tdb, ct);
    }

if (!ct)
    {
    /* Print data version trackDB setting, if any */
    char *version = trackDbSetting(tdb, "dataVersion");
    if (version)
        {
        cgiDown(0.7);
        printf("<B>Data version:</B> %s\n", version);
        }

   /* Print lift information from trackDb, if any */
   trackDbPrintOrigAssembly(tdb, database);

    printUpdateTime(database, tdb, NULL);
    }

if (tdb->html != NULL && tdb->html[0] != 0)
    {
    char *browserVersion;
    if (btIE == cgiClientBrowser(&browserVersion, NULL, NULL) && *browserVersion < '8')
        htmlHorizontalLine();
    else // Move line down, since <H2>Description (in ->html) is proceded by too much space
        printf("<HR ALIGN='bottom' style='position:relative; top:1em;'>");

    printf("<table class='windowSize'><tr valign='top'><td rowspan=2>");
    puts("<A NAME='TRACK_HTML'></A>");    // include anchor for Description link

    // Add pennantIcon
    printPennantIconNote(tdb);

    puts(tdb->html);
    printf("</td><td nowrap>");
    cgiDown(0.7); // positions top link below line
    makeTopLink(tdb);
    printf("&nbsp</td></tr><tr valign='bottom'><td nowrap>");
    makeTopLink(tdb);
    printf("&nbsp</td></tr></table>");
    }
}	/*	void trackUi(struct trackDb *tdb)	*/

struct trackDb *trackDbForPseudoTrack(char *tableName, char *shortLabel,
	char *longLabel, int defaultVis, boolean canPack)
/* Create trackDb for a track without a corresponding table. */
{
struct trackDb *tdb;

AllocVar(tdb);
tdb->track = tableName;
tdb->table = tableName;
tdb->shortLabel = shortLabel;
tdb->longLabel = longLabel;
tdb->visibility = defaultVis;
tdb->priority = 1.0;

tdb->html = hFileContentsOrWarning(hHelpFile(tableName));
tdb->type = "none";
tdb->grp = "map";
tdb->canPack = canPack;
return tdb;
}

struct trackDb *trackDbForWikiTrack()
/* Create a trackDb entry for the wikiTrack.
   It is not a real track, so doesn't appear in trackDb */
{
return trackDbForPseudoTrack(WIKI_TRACK_TABLE,
	WIKI_TRACK_LABEL, WIKI_TRACK_LONGLABEL, tvFull, FALSE);
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
struct trackDb *tdbList = NULL;
struct trackDb *tdb = NULL;
char *track;
struct customTrack *ct = NULL, *ctList = NULL;
char *ignored;
cart = theCart;
track = cartString(cart, "g");
getDbAndGenome(cart, &database, &ignored, NULL);
chromosome = cartUsualString(cart, "c", hDefaultChrom(database));

trackHash = trackHashMakeWithComposites(database,chromosome,&tdbList,FALSE);
if (sameWord(track, WIKI_TRACK_TABLE))
    tdb = trackDbForWikiTrack();
else if (sameWord(track, RULER_TRACK_NAME))
    /* special handling -- it's not a full-fledged track */
    tdb = trackDbForRuler();
else if (sameWord(track, OLIGO_MATCH_TRACK_NAME))
    tdb = trackDbForOligoMatch();
else if (sameWord(track, CUTTERS_TRACK_NAME))
    tdb = trackDbForPseudoTrack(CUTTERS_TRACK_NAME, CUTTERS_TRACK_LABEL, CUTTERS_TRACK_LONGLABEL, tvHide, TRUE);
else if (isCustomTrack(track))
    {
    ctList = customTracksParseCart(database, cart, NULL, NULL);
    for (ct = ctList; ct != NULL; ct = ct->next)
        {
        if (sameString(track, ct->tdb->track))
            {
            tdb = ct->tdb;
            break;
            }
        }
    }
else if (isHubTrack(track))
    {
    tdb = hubConnectAddHubForTrackAndFindTdb(database, track, &tdbList, trackHash);
    }
else if (sameString(track, "hgPcrResult"))
    tdb = pcrResultFakeTdb();
else
    {
    tdb = tdbForTrack(database, track,&tdbList);
    }
if (tdb == NULL)
   {
   errAbort("Can't find %s in track database %s chromosome %s",
	    track, database, chromosome);
   }
char *super = trackDbGetSupertrackName(tdb);
if (super)
    {
    /* configured as a supertrack member in trackDb */
    if (tdb->parent)
        {
        /* the supertrack is also configured, so use supertrack defaults */
        tdbMarkAsSuperTrack(tdb->parent);
        trackDbSuperMemberSettings(tdb);
        }
    }
char *title = (tdbIsSuper(tdb) ? "Super-track Settings" :
                tdbIsDownloadsOnly(tdb) ? DOWNLOADS_ONLY_TITLE : "Track Settings");
if(cartOptionalString(cart, "ajax"))
    {
    // html is going to be used w/n a dialog in hgTracks.js so serve up stripped down html
    trackUi(tdb, tdbList, ct, TRUE);
    cartRemove(cart,"ajax");
    }
else
    {
    cartWebStart(cart, database, "%s %s", tdb->shortLabel, title);
    trackUi(tdb, tdbList, ct, FALSE);
    printf("<BR>\n");
    webEnd();
    }
}

char *excludeVars[] = { "submit", "Submit", "g", NULL, "ajax", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
htmlSetBackground(hBackgroundImage());
setUdcCacheDir();
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, NULL);
return 0;
}
