/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

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
#include "dupTrack.h"
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
#include "trackVersion.h"
#include "gtexUi.h"
#include "genbank.h"
#include "botDelay.h"
#include "customComposite.h"
#include "hicUi.h"
#include "decoratorUi.h"
    
#ifdef USE_HAL 
#include "halBlockViz.h"
#endif 

#define MAIN_FORM "mainForm"
#define WIGGLE_HELP_PAGE  "../goldenPath/help/hgWiggleTrackHelp.html"

/* for earlyBotCheck() function at the beginning of main() */
#define delayFraction   0.25    /* standard penalty is 1.0 for most CGIs */
                                /* this one is 0.25 */
static boolean issueBotWarning = FALSE;

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
struct trackDb *geneTdbList = NULL, *gTdb;
geneTdbList = snp125FetchGeneTracks(database, cart);
if (geneTdbList)
    {
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
	if (gTdb->parent != NULL)
	    {
	    struct dyString *dy = dyStringNew(0);
	    if (gTdb->parent->parent != NULL &&
		!startsWith(gTdb->parent->parent->shortLabel, gTdb->parent->shortLabel))
		dyStringPrintf(dy, "%s: ", gTdb->parent->parent->shortLabel);
	    if (!startsWith(gTdb->parent->shortLabel, gTdb->shortLabel))
		dyStringPrintf(dy, "%s: ", gTdb->parent->shortLabel);
	    dyStringPrintf(dy, "%s", gTdb->shortLabel);
	    labels[i] = dyStringCannibalize(&dy);
	    }
	else
	    labels[i] = gTdb->shortLabel;
	}
    cgiMakeCheckboxGroupWithVals(cartVar, labels, values, menuSize, selectedGeneTracks, numCols);
    jsEndCollapsibleSection();
    }
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

static char *commaSepFromSqlEnumOrSetTypeDecl(struct sqlFieldInfo *fi, char *type, char *table)
/* Destructively prepare fi->type for chopCommas. type is either "enum" or "set".
 * Strip the initial "set(" or "enum(" and final ")", informatively errAborting if not found,
 * and strip the single-quote characters that mysql puts around each field. */
{
if (sameString(type, "enum"))
    {
    if (!startsWith("enum(", fi->type))
	errAbort("Expected %s.%s's type to begin with 'enum(' but got '%s'",
		 table, fi->field, fi->type);
    }
else if (sameString(type, "set"))
    {
    if (!startsWith("set(", fi->type))
	errAbort("Expected %s.%s's type to begin with 'set(' but got '%s'",
		 table, fi->field, fi->type);
    }
char *vals = fi->type + strlen(type) + 1;
char *rightParen = strrchr(vals, ')');
if (rightParen == NULL || rightParen[1] != '\0')
    errAbort("Expected %s.%s's type to end with ')' but got '%s'",
	     table, fi->field, fi->type);
else
    *rightParen = '\0';
stripChar(vals, '\'');
return vals;
}

static struct slName *snp125FixClassGlobals(struct trackDb *tdb)
/* Fix snp125Class* global variables to contain only the classes that are present
 * in the SQL enum type definition. Return a list of classes that are not present. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlFieldInfo *fi, *fiList = sqlFieldInfoGet(conn, tdb->table);
hFreeConn(&conn);
struct slName *unusedList = NULL;
boolean foundClass = FALSE;
for (fi = fiList;  fi != NULL;  fi = fi->next)
    {
    if (sameString(fi->field, "class"))
	{
	foundClass = TRUE;
	char *vals = commaSepFromSqlEnumOrSetTypeDecl(fi, "enum", tdb->table);
	char *values[64]; // max 11 in older tables
	int valCount = chopCommas(vals, values);
	char *labels[valCount];
	char *defaults[valCount];
	char *oldVars[valCount];
	// Use labels from old static array
	int i;
	for (i = 0;  i < valCount;  i++)
	    {
	    int oldIx = stringArrayIx(values[i], snp125ClassDataName, snp125ClassArraySize);
	    labels[i] = snp125ClassLabels[oldIx];
	    defaults[i] = snp125ClassDefault[oldIx];
	    oldVars[i] = snp125ClassOldColorVars[oldIx];
	    }
	// Make a list of unused values;
	for (i = 0;  i < snp125ClassArraySize;  i++)
	    {
	    if (stringArrayIx(snp125ClassDataName[i], values, valCount) < 0)
		slAddHead(&unusedList, slNameNew(snp125ClassDataName[i]));
	    }
	// Now overwrite old globals with the correct contents.
	snp125ClassArraySize = valCount;
	for (i = 0;  i < valCount;  i++)
	    {
	    snp125ClassDataName[i] = cloneString(values[i]);
	    snp125ClassLabels[i] = cloneString(labels[i]);
	    snp125ClassDefault[i] = cloneString(defaults[i]);
	    snp125ClassOldColorVars[i] = cloneString(oldVars[i]);
	    }
	}
    }
if (! foundClass)
    errAbort("Didn't find definition of class field in %s", tdb->table);
return unusedList;
}

static void snp125MakeHiddenInputsForUnused(char *cartVar, struct slName *unusedList)
/* If this db's snpNNN table uses only a small subset of the global arrays, but some
 * other db's snpNNN table uses a larger subset, we don't want to have the effect of
 * turning off the checkboxes that aren't used in this db's snpNNN.  So make hidden
 * inputs to pretend that all unused values' checkboxes are checked. */
{
struct slName *unused;
for (unused = unusedList;  unused != NULL;  unused = unused->next)
    cgiMakeHiddenVar(cartVar, unused->name);
}

static void snp137PrintFunctionFilterControls(struct trackDb *tdb)
/* As of snp137, show func filter choices based on sql field set
 * values and Sequence Ontology (SO) terms so we won't have to
 * hardcode menus as new functional categories are added. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlFieldInfo *fi, *fiList = sqlFieldInfoGet(conn, tdb->table);
hFreeConn(&conn);
for (fi = fiList;  fi != NULL;  fi = fi->next)
    {
    if (sameString(fi->field, "func"))
	{
	char *vals = commaSepFromSqlEnumOrSetTypeDecl(fi, "set", tdb->table);
	char *values[128]; // 22 values as of snp137
	int valCount = chopCommas(vals, values);
	char *labels[valCount];
	int i;
	for (i = 0;  i < valCount;  i++)
	    {
	    if (sameString(values[i], "unknown"))
		labels[i] = "Unknown";
	    else
		labels[i] = snpMisoLinkFromFunc(values[i]);
	    }
	snp125PrintFilterControls(tdb->track, "Function", "func", labels, values, valCount);
	return;
	}
    }
errAbort("Didn't find definition of func field in %s", tdb->table);
}

int snp125ValidArraySize(int version)
/* Figure out how many validation options are applicable to this database and version. */
{
// Cache result since it costs a mysql query and won't change
static int size = 0;
if (size == 0)
    {
    size = snp125ValidArraySizeNonHuman;
    if (sameString(hOrganism(database), "Human"))
	{
	size = snp125ValidArraySizeHuman;
	if (version < 130)
	    size--; // no by-1000genomes
	}
    }
return size;
}

static void snp125MakeHiddenInputsForValid(char *cartVar, int version)
/* Non-human dbs' snpNNN tables use only a subset of the validation codes, but human dbs'
 * snpNNN tables use all of them.  When making options for non-human dbs, we don't want
 * to have the effect of turning off the checkboxes that aren't used (but would be for human).
 * So make hidden inputs to pretend that all unused values' checkboxes are checked. */
{
int i;
for (i = snp125ValidArraySize(version);  i < snp125ValidArraySizeHuman;  i++)
    cgiMakeHiddenVar(cartVar, snp125ValidDataName[i]);
}

static void snp125PrintFilterControlSection(struct trackDb *tdb, int version,
					    boolean molTypeHasMito,
					    struct slName *snp125UnusedClasses)
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
safef(cartVar, sizeof(cartVar), "%s.include_%s", tdb->track, "class");
snp125MakeHiddenInputsForUnused(cartVar, snp125UnusedClasses);
snp125PrintFilterControls(tdb->track, "Validation", "valid", snp125ValidLabels,
			  snp125ValidDataName, snp125ValidArraySize(version));
safef(cartVar, sizeof(cartVar), "%s.include_%s", tdb->track, "valid");
snp125MakeHiddenInputsForValid(cartVar, version);
if (version < 137)
    {
    int funcArraySize = (version < 130) ? snp125FuncArraySize : (snp125FuncArraySize - 1);
    snp125PrintFilterControls(tdb->track, "Function", "func", snp125FuncLabels,
			      snp125FuncDataName, funcArraySize);
    }
else
    snp137PrintFunctionFilterControls(tdb);
int molTypeArraySize = snp125MolTypeArraySize;
if (! molTypeHasMito)
    molTypeArraySize--;
snp125PrintFilterControls(tdb->track, "Molecule Type", "molType", snp125MolTypeLabels,
			  snp125MolTypeDataName, molTypeArraySize);
if (version >= 132)
    {
    int excArraySize = snp132ExceptionArraySize;
    if (version < 135)
	excArraySize -= 2;
    snp125PrintFilterControls(tdb->track, "Unusual Conditions (UCSC)", "exceptions",
		      snp132ExceptionLabels, snp132ExceptionVarName, excArraySize);
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
    snp125RemoveColorVars(cart, snp125ValidOldColorVars, TRUE, snp125ValidArraySizeHuman,
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

void snp125PrintColorControlSection(struct trackDb *tdb, int version, boolean molTypeHasMito)
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
safef(autoSubmit, sizeof(autoSubmit), ""
      "document."MAIN_FORM".action = '%s'; %s"
      "document."MAIN_FORM".submit();",
      cgiScriptName(), jsSetVerticalPosition(MAIN_FORM));
cgiContinueHiddenVar("g");
cgiContinueHiddenVar("c");

char cartVar[512];
safef(cartVar, sizeof(cartVar), "%s.colorSource", tdb->track);
enum snp125ColorSource colorSourceCart = snp125ColorSourceFromCart(cart, tdb);
char *colorSourceSelected = snp125ColorSourceToLabel(tdb, colorSourceCart);
cgiMakeDropListFull(cartVar, labels, labels, arraySize, colorSourceSelected, "change", autoSubmit);
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
    int funcArraySize, excArraySize, molTypeArraySize;
    case snp125ColorSourceLocType:
                snp125PrintColorSpec(tdb->track, "locType", snp125LocTypeOldColorVars, TRUE,
                                     snp125LocTypeLabels, snp125LocTypeDefault,
                                     snp125LocTypeArraySize);
                break;
    case snp125ColorSourceClass:
                snp125PrintColorSpec(tdb->track, "class", snp125ClassOldColorVars, TRUE,
                                     snp125ClassLabels, snp125ClassDefault,
				     snp125ClassArraySize);
                break;
    case snp125ColorSourceValid:
                snp125PrintColorSpec(tdb->track, "valid", snp125ValidOldColorVars, TRUE,
                                     snp125ValidLabels, snp125ValidDefault,
				     snp125ValidArraySize(version));
                break;
    case snp125ColorSourceFunc:
                funcArraySize = (version < 130) ? snp125FuncArraySize : (snp125FuncArraySize - 1);
                snp125PrintColorSpec(tdb->track, "func", snp125FuncOldColorVars, TRUE,
                                     snp125FuncLabels, snp125FuncDefault, funcArraySize);
                break;
    case snp125ColorSourceMolType:
		molTypeArraySize = snp125MolTypeArraySize;
		if (! molTypeHasMito)
		    molTypeArraySize--;
                snp125PrintColorSpec(tdb->track, "molType", snp125MolTypeOldColorVars, TRUE,
				     snp125MolTypeLabels, snp125MolTypeDefault, molTypeArraySize);
                break;
    case snp125ColorSourceExceptions:
		excArraySize = snp132ExceptionArraySize;
		if (version < 135)
		    excArraySize -= 2;
                snp125PrintColorSpec(tdb->track, "exceptions", snp132ExceptionVarName, FALSE,
                                     snp132ExceptionLabels, snp132ExceptionDefault,
                                     excArraySize);
                break;
    case snp125ColorSourceBitfields:
                snp125PrintColorSpec(tdb->track, "bitfields", snp132BitfieldVarName, FALSE,
                                     snp132BitfieldLabels, snp132BitfieldDefault,
                                     snp132BitfieldArraySize);
                break;
    case snp125ColorSourceAlleleFreq:
                printf("<P>Items are colored by allele frequency on a red-blue spectrum, "
                       "with red representing rare alleles and blue representing common alleles. "
                       "Items with no allele frequency data are colored black.</P>");
                break;
    default:
                errAbort("Unrecognized value of enum snp125ColorSource (%d)", colorSourceCart);
    }
jsEndCollapsibleSection();
}

boolean snp125CheckMolTypeForMito(struct trackDb *tdb)
/* Can't use version to determine whether the molType enum includes "mito" --
 * check SQL column def. */
{
boolean gotMito = FALSE;
struct sqlConnection *conn = hAllocConn(database);
char **enumVals = sqlGetEnumDef(conn, tdb->table, "molType");
while (*enumVals != NULL && !gotMito)
    {
    if (sameString(*enumVals, "mito"))
	gotMito = TRUE;
    enumVals++;
    }
hFreeConn(&conn);
return gotMito;
}

void snp125Ui(struct trackDb *tdb)
/* UI for dbSNP version 125 and later. */
{
char *orthoTable = snp125OrthoTable(tdb, NULL);
int version = snpVersion(tdb->track);
char cartVar[512];
jsInit();
struct slName *snp125UnusedClasses = snp125FixClassGlobals(tdb);

if (isNotEmpty(orthoTable) && hTableExists(database, orthoTable))
    {
    printf("<BR><B>Include Chimp state and observed human alleles in name: </B>&nbsp;");
    safef(cartVar, sizeof(cartVar), "%s.extendedNames", tdb->track);
    snp125ExtendedNames = cartUsualBoolean(cart, cartVar,
			  // Check old cart var name for backwards compatibility w/ old sessions:
					   cartUsualBoolean(cart, "snp125ExtendedNames", FALSE));
    cgiMakeCheckBox(cartVar, snp125ExtendedNames);
    printf("<BR>(If enabled, chimp allele is displayed first, then '>', then human alleles).");
    }
else
    {
    printf("<BR><B>Include observed alleles in name: </B>&nbsp;");
    safef(cartVar, sizeof(cartVar), "%s.extendedNames", tdb->track);
    snp125ExtendedNames = cartUsualBoolean(cart, cartVar,
			  // Check old cart var name for backwards compatibility w/ old sessions:
					   cartUsualBoolean(cart, "snp125ExtendedNames", FALSE));
    cgiMakeCheckBox(cartVar, snp125ExtendedNames);
    }
puts("<BR>");

printf("&nbsp;&nbsp;<B>Show alleles on strand of reference genome reported by dbSNP: </B>&nbsp;");
safef(cartVar, sizeof(cartVar), "%s.allelesDbSnpStrand", tdb->track);
boolean useDbSnpStrand = cartUsualBoolean(cart, cartVar, FALSE);
cgiMakeCheckBox(cartVar, useDbSnpStrand);
puts("<BR><BR>");

// Make wrapper table for collapsible sections:
puts("<TABLE border=0 cellspacing=0 cellpadding=0>");

snp125OfferGeneTracksForFunction(tdb);

boolean molTypeHasMito = snp125CheckMolTypeForMito(tdb);

puts("<TR><TD colspan=2><BR></TD></TR>");
snp125PrintFilterControlSection(tdb, version, molTypeHasMito, snp125UnusedClasses);
puts("<TR><TD colspan=2><BR></TD></TR>");

snp125PrintColorControlSection(tdb, version, molTypeHasMito);
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
    slSort(&tdbRefList, trackDbRefCmp);
    struct slRef *tdbRef;
    for (tdbRef = tdbRefList; tdbRef != NULL; tdbRef = tdbRef->next)
	{
	struct trackDb *subTdb = tdbRef->val;
	if (hTableExists(database, subTdb->table))
	    {
	    safef(var, sizeof(var), "%s_inv", subTdb->track);
	    cgiMakeCheckBoxMore(var, cartUsualBoolean(cart, var, ldInvDefault),
			      "class='subtrackInCompositeUi'");
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

void gvIdControls (struct trackDb *tdb)
/* print the controls for the label choice */
{
printf("<B>Label:</B> ");
labelMakeCheckBox(cart, tdb, "hgvs", "HGVS name", FALSE);
labelMakeCheckBox(cart, tdb, "common", "Common name", FALSE);
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

sqlSafef(query, sizeof(query),
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
    sqlSafef(query, sizeof(query),
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

sqlSafef(query, sizeof(query),
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
"<P><A HREF=\"http://genome-test.gi.ucsc.edu/cgi-bin/tracks.exe?where=%s%%3A%d-%d\"> Temporary Intronerator link: %s:%d-%d</A> <I>for testing purposes only</I> \n</P>", chromosome+3, start, end, chromosome+3, start, end );
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
safef(varName, sizeof(varName), "%s.label", tdb->track);
printf("<BR><B>Include Entries of:</B> ");
printf("<UL>\n");
printf("<LI>");
labelMakeCheckBox(cart, tdb, "class1", "Phenotype map key 1: the disorder has been placed on the map based on its association with a gene, but the underlying defect is not known.", TRUE);
printf("<LI>");
labelMakeCheckBox(cart, tdb, "class2", "Phenotype map key 2: the disorder has been placed on the map by linkage; no mutation has been found.", TRUE);
printf("<LI>");
labelMakeCheckBox(cart, tdb, "class3", "Phenotype map key 3: the molecular basis for the disorder is known; a mutation has been found in the gene.", TRUE);
printf("<LI>");
labelMakeCheckBox(cart, tdb, "class4", "Phenotype map key 4: a contiguous gene deletion or duplication syndrome; multiple genes are deleted or duplicated causing the phenotype.", TRUE);

// removed the "others" option for the time being
//printf("<LI>");
//labelMakeCheckBox(cart, tdb, "others", "Others: no associated OMIM phenotype map key info available.", TRUE);
printf("</UL>");
}

void omimGene2Config(struct trackDb *tdb)
/* Put up OMIM Genes track controls */
{
char varName[64];
safef(varName, sizeof(varName), "%s.label", tdb->track);
printf("<BR><B>Include Entries of:</B> ");
printf("<UL>\n");
printf("<LI>");
labelMakeCheckBox(cart, tdb, "class1", "Phenotype map key 1: the disorder has been placed on the map based on its association with a gene, but the underlying defect is not known.", TRUE);
printf("<LI>");
labelMakeCheckBox(cart, tdb, "class2", "Phenotype map key 2: the disorder has been placed on the map by linkage; no mutation has been found.", TRUE);
printf("<LI>");
labelMakeCheckBox(cart, tdb, "class3", "Phenotype map key 3: the molecular basis for the disorder is known; a mutation has been found in the gene.", TRUE);
printf("<LI>");
labelMakeCheckBox(cart, tdb, "class4", "Phenotype map key 4: a contiguous gene deletion or duplication syndrome; multiple genes are deleted or duplicated causing the phenotype.", TRUE);
printf("<LI>");
labelMakeCheckBox(cart, tdb, "others", "Others: no associated OMIM phenotype map key info available.", TRUE);
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
sqlSafef(query, sizeof(query), "select kgXref.kgID from kgXref,%s r where kgXref.refseq = r.mrnaAcc and r.omimId != 0 limit 1", refLinkTable);
omimAvail = sqlQuickString(conn, query);
hFreeConn(&conn);
boolean isGencode = trackDbSettingOn(tdb, "isGencode") || trackDbSettingOn(tdb, "isGencode2");

printf("<B>Label:</B> ");
labelMakeCheckBox(cart, tdb, "gene", "gene symbol", FALSE);
if (isGencode)
    labelMakeCheckBox(cart, tdb, "gencodeId", "GENCODE Transcript ID", FALSE);
labelMakeCheckBox(cart, tdb, "kgId", "UCSC Known Gene ID", FALSE);
labelMakeCheckBox(cart, tdb, "prot", "UniProt Display ID", FALSE);

if (omimAvail != NULL)
    {
    char sym[32];
    safef(sym, sizeof(sym), "omim%s", cartString(cart, "db"));
    labelMakeCheckBox(cart, tdb, sym, "OMIM ID", FALSE);
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
boolean isGencode = trackDbSettingOn(tdb, "isGencode");
boolean isGencode2 = trackDbSettingOn(tdb, "isGencode2");
if (isGencode || isGencode2)
    {
    safef(varName, sizeof(varName), "%s.show.comprehensive", tdb->track);
    option = cartUsualBoolean(cart, varName, FALSE);
    cgiMakeCheckBox(varName, option);
    printf(" %s&nbsp;&nbsp;&nbsp;", "show comprehensive set");

    if (isGencode2)
        {
        safef(varName, sizeof(varName), "%s.show.pseudo", tdb->track);
        option = cartUsualBoolean(cart, varName, FALSE);
        cgiMakeCheckBox(varName, option);
        printf(" %s&nbsp;&nbsp;&nbsp;", "show pseudogenes");
        }
    }
printf("<BR>\n");
}

void knownGeneUI(struct trackDb *tdb)
/* Put up refGene-specific controls */
{
/* This is incompatible with adding Protein ID to lf->extra */
knownGeneIdConfig(tdb);
knownGeneShowWhatUi(tdb);
baseColorDrawOptDropDown(cart, tdb);
wigOption(cart, "knownGene", "UCSC Genes", tdb);
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
labelMakeCheckBox(cart, tdb, "omimId", "OMIM ID", FALSE);
labelMakeCheckBox(cart, tdb, "gene", "gene symbol", FALSE);

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

void ensGeneIdConfig(struct trackDb *tdb)
/* Put up gene ID track controls */
{
char varName[64];
char *geneLabel;
safef(varName, sizeof(varName), "%s.label", tdb->track);
geneLabel = cartUsualString(cart, varName, "ENST* identifier");
printf("<B>Label:</B> ");
radioButton(varName, geneLabel, "ENST* identifier");
radioButton(varName, geneLabel, "ENSG* identifier");
radioButton(varName, geneLabel, "gene symbol");
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
labelMakeCheckBox(cart, tdb, "gene", "gene", FALSE);
labelMakeCheckBox(cart, tdb, "acc", "accession", FALSE);
printf("<BR>\n");

baseColorDrawOptDropDown(cart, tdb);
}

void refGeneUI(struct trackDb *tdb)
/* Put up refGene or xenoRefGene gene ID track controls, with checkboxes */
{
// Show label options only if top-level track; ncbiRefSeqUI (for refSeqComposite) shows
// label options for all subtracks.
if (tdb->parent == NULL)
    {
    /* Figure out if OMIM database is available. */
    int omimAvail = 0;
    if (sameString(tdb->track, "refGene"))
        {
        struct sqlConnection *conn = hAllocConn(database);
        char query[128];
        sqlSafef(query, sizeof(query), "select r.omimId from %s r, refGene where r.mrnaAcc = refGene.name and r.omimId != 0 limit 1", refLinkTable);
        omimAvail = sqlQuickNum(conn, query);
        hFreeConn(&conn);
        }

    /* Put up label line  - boxes for gene, accession or maybe OMIM. */
    printf("<BR><B>Label:</B> ");
    labelMakeCheckBox(cart, tdb, "gene", "gene", TRUE);
    labelMakeCheckBox(cart, tdb, "acc", "accession", FALSE);
    if (omimAvail != 0)
        {
        char sym[32];
        safef(sym, sizeof(sym), "omim%s", cartString(cart, "db"));
        labelMakeCheckBox(cart, tdb, sym, "OMIM ID", FALSE);
        }
    printf("<BR>\n");
    }

/* Put up noncoding option and codon coloring stuff. */
hideNoncodingOpt(tdb);
baseColorDrawOptDropDown(cart, tdb);
printf("<BR>\n");

// let the user choose to see the track in wiggle mode
wigOption(cart, tdb->track, tdb->shortLabel, tdb);
}

void transMapUI(struct trackDb *tdb)
/* Put up transMap-specific controls for table-based transMap */
{
// FIXME: this can be deleted once table-based transMap is no longer supported.
printf("<B>Label:</B> ");
labelMakeCheckBox(cart, tdb, "orgCommon", "common name", FALSE);
labelMakeCheckBox(cart, tdb, "orgAbbrv", "organism abbreviation", FALSE);
labelMakeCheckBox(cart, tdb, "db", "assembly database", FALSE);
labelMakeCheckBox(cart, tdb, "gene", "gene", FALSE);
labelMakeCheckBox(cart, tdb, "acc", "accession", FALSE);

baseColorDrawOptDropDown(cart, tdb);
indelShowOptions(cart, tdb);
}

void retroGeneUI(struct trackDb *tdb)
/* Put up retroGene-specific controls */
{
printf("<B>Label:</B> ");
labelMakeCheckBox(cart, tdb, "gene", "gene", FALSE);
labelMakeCheckBox(cart, tdb, "acc", "accession", FALSE);

baseColorDrawOptDropDown(cart, tdb);
}

void ncbiRefSeqUI(struct trackDb *tdb)
/* Put up gene ID track controls */
{
char varName[256];
safef(varName, sizeof(varName), "%s.label", tdb->track);
printf("<br><b>Label:</b> ");
labelMakeCheckBox(cart, tdb, "gene", "gene symbol", TRUE);
labelMakeCheckBox(cart, tdb, "acc", "accession", TRUE);
struct sqlConnection *conn = hAllocConn(database);
char query[1024];
sqlSafef(query, sizeof query, "select 1 from ncbiRefSeqLink where omimId != 0 limit 1");
boolean omimAvail = sqlQuickNum(conn, query);
if (omimAvail)
    {
    char sym[32];
    safef(sym, sizeof(sym), "omim%s", cartString(cart, "db"));
    labelMakeCheckBox(cart, tdb, sym, "OMIM ID", FALSE);
    }
hFreeConn(&conn);
}

void ensGeneUI(struct trackDb *tdb)
/* Put up Ensembl Gene track-specific controls */
{
ensGeneIdConfig(tdb);
printf("<BR>\n");

/* Put up codon coloring stuff. */
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
int thisHeightPer;
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
boolean compositeTrack = tdbIsComposite(tdb);

if (compositeTrack)
    return;	// configuration taken care of by hCompositeUi() later
else
    chainCfgUi(database, cart, tdb, tdb->track, NULL, FALSE, chromosome);
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

puts("<P>");
cgiMakeCheckBox(BASE_SHOWASM_SCALEBAR, cartUsualBoolean(cart, BASE_SHOWASM_SCALEBAR, TRUE));
puts("&nbsp;<B>Show assembly next to scale bar</B>");
puts("</P>");

puts("<P>");
cgiMakeCheckBox(BASE_SHOWCODONS, cartUsualBoolean(cart, BASE_SHOWCODONS, FALSE));
puts("&nbsp;<B>Show amino acids for all three possible reading frames</B>");
puts("</P>");

puts("<P><B>Zoom factor:&nbsp;</B>");
zoomRadioButtons(RULER_BASE_ZOOM_VAR, currentZoom);
puts("<P><B>Motifs to highlight:&nbsp;</B>");
cgiMakeTextVar(BASE_MOTIFS, motifString, 20);
puts("&nbsp;(Comma separated list, e.g.: GT,AG for splice sites)");
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

void pubsUi(struct trackDb *tdb)
/* UI for pubs match track */
{
// bing tracks need no config
if (endsWith(tdb->track, "BingBlat"))
    return;

#define NUM_YEARS 15  // similar to google scholar, which goes back to 20 years

#define PUBS_KEYWORDS_TAG "pubsFilterKeywords"
#define PUBS_YEAR_TAG     "pubsFilterYear"
#define PUBS_COLORBY_TAG    "pubsColorBy"
#define PUBS_PUBFILT_TAG    "pubsFilterPublisher"

// get current set filters from cart
char *keywords   = cartUsualStringClosestToHome(cart, tdb, FALSE, PUBS_KEYWORDS_TAG, "");
char *yearFilter = cartUsualStringClosestToHome(cart, tdb, FALSE, PUBS_YEAR_TAG, "anytime");
char *colorBy    = cartUsualStringClosestToHome(cart, tdb, FALSE, PUBS_COLORBY_TAG, "topic");
char *pubFilter  = cartUsualStringClosestToHome(cart, tdb, FALSE, PUBS_PUBFILT_TAG, "all");

// print keyword input box
puts("<P><B>Filter articles by keywords in abstract, title or authors:</B>");
char cgiVar[128];
safef(cgiVar,sizeof(cgiVar),"%s.%s",tdb->track,PUBS_KEYWORDS_TAG);
cgiMakeTextVar(cgiVar, keywords, 45);

// generate strings like "since <year>" for last 15 years
char *text[NUM_YEARS + 1];
char *values[NUM_YEARS + 1];
text[0] = "anytime";
values[0] = "anytime";
time_t nowTime = time(NULL);
struct tm *tm = localtime(&nowTime);
int nowYear = 1900 + tm->tm_year;

int i;
for(i = 0; i < NUM_YEARS; i++)
    {
    char buf[20];
    safef(buf, sizeof(buf), "since %d", nowYear - i);
    text[i + 1] = cloneString(buf);
    safef(buf, sizeof(buf), "%d", nowYear - i);
    values[i + 1] = cloneString(buf);
    }

// print dropdown box with "since <year>" lines
puts("</P><P>\n");
printf("<B>Show articles published </B>");
safef(cgiVar,sizeof(cgiVar),"%s.%s",tdb->track,PUBS_YEAR_TAG);
cgiDropDownWithTextValsAndExtra(cgiVar, text, values, NUM_YEARS + 1, yearFilter, NULL);

// print dropdown box with "filter by publisher" lines
puts("</P><P>\n");
printf("<B>Only articles published by </B>");
char *publText[5] = {"all publishers", "Elsevier", "PubmedCentral", "Nature Publ. Group", "FASEB"};
char *publVals[5] = {"all", "elsevier", "pmc", "npg", "faseb"};
safef(cgiVar,sizeof(cgiVar),"%s.%s",tdb->track,PUBS_PUBFILT_TAG);
cgiDropDownWithTextValsAndExtra(cgiVar, publText, publVals, 5, pubFilter, NULL);
puts("</P>\n");

// print dropdown box with "color matches" lines
puts("</P><P>\n");
printf("<B>Color sequence matches by </B>");
char *colorText[3] = {"topic", "impact of journal", "year"};
char *colorVals[3] = {"topic", "impact", "year"};
safef(cgiVar,sizeof(cgiVar),"%s.%s",tdb->track,PUBS_COLORBY_TAG);
cgiDropDownWithTextValsAndExtra(cgiVar, colorText, colorVals, 3, colorBy, NULL);
puts("</P>\n");

wigOption(cart, tdb->track, tdb->shortLabel, tdb);
}

void oligoMatchUi(struct trackDb *tdb)
/* UI for oligo match track */
{
char *oligo = cartUsualString(cart, oligoMatchVar, oligoMatchDefault);
puts("<P><B>Short (2-30 base) sequence:</B>");
jsInline(
"function packTrack()\n"
"{\n"
"var box = jQuery('select[name$=oligoMatch]');\n"
"if (box.val()=='hide')\n"
"    box.val('pack');\n"
"}\n");
printf("<input name='%s' id='%s' size=\"%d\" value=\"%s\" type=\"TEXT\">", 
    oligoMatchVar, oligoMatchVar, 45, oligo);
puts("<br>Examples: TATAWAAR, AAAAA");
jsOnEventById("input", oligoMatchVar, "packTrack();");
}

void cutterUi(struct trackDb *tdb)
/* UI for restriction enzyme track */
{
char *enz = cartUsualString(cart, cutterVar, cutterDefault);
puts("<P><B>Filter display by enzymes (separate with commas):</B><BR>");
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

wigCfgUi(cart,tdb,tdb->track,
         "<span style='text-decoration:underline;'>Graph Plotting options:</span>",FALSE);
printf("<p><b><span style='text-decoration:underline;'>"
       "View/Hide individual cell lines:</span></b>");
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
    {
      "no filter",
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
sqlSafef(query, sizeof(query),
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
	  "<A HREF=\"https://www.ncbi.nlm.nih.gov/entrez/query.fcgi?cmd=Retrieve&db=PubMed"
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

static void factorSourceUi(char *db, struct trackDb *tdb)
{
// Multi-select filter on factors
// NOTE: this UI code doesn't currently support the use of factorSource tracks
// as subtracks in a composite (would require moving to hui.c, using newer cfgByType approach)
filterBy_t *filters = filterBySetGet(tdb, cart, tdb->track);
if (filters != NULL)
    {
    puts("<p>");
    filterBySetCfgUi(cart, tdb, filters, TRUE, tdb->track);
    filterBySetFree(&filters);
    }
char varName[64];
if (trackDbSetting(tdb, "motifTable") != NULL)
    {
    printf("<BR><B>Highlight motifs: </B> ");
    safef(varName, sizeof(varName), "%s.highlightMotifs", tdb->track);
    cartMakeCheckBox(cart, varName, trackDbSettingClosestToHomeOn(tdb, "motifDrawDefault"));
    printf("<BR>");
    }

printf("<BR><B>Cluster right label: </B>");

safef(varName, sizeof(varName), "%s.showExpCounts", tdb->track);
cartMakeCheckBox(cart, varName, TRUE);
printf("cell count (detected/assayed)&nbsp;&nbsp;");

safef(varName, sizeof(varName), "%s.showCellAbbrevs", tdb->track);
cartMakeCheckBox(cart, varName, TRUE);
printf("cell abbreviations");

puts("<p><table>");
jsBeginCollapsibleSectionFontSize(cart, tdb->track, "cellSources", "Cell Abbreviations", FALSE,
                                        "medium");
struct sqlConnection *conn = hAllocConn(db);
hPrintFactorSourceAbbrevTable(conn, tdb);
jsEndCollapsibleSection();
puts("</table>");
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
jsIncludeFile("hui.js",NULL);
printf("\n<P><TABLE CELLPADDING=2>");
tdbRefSortPrioritiesFromCart(cart, &superTdb->children);
struct slRef *childRef;
char javascript[1024];
for (childRef = superTdb->children; childRef != NULL; childRef = childRef->next)
    {
    struct trackDb *tdb = childRef->val;
    if (childRef == superTdb->children) // first time through
        {
        printf("\n<TR><TD NOWRAP colspan=2>");
	printf("<IMG height=18 width=18 id='btn_plus_all' src='../images/add_sm.gif'>");
	jsOnEventById("click", "btn_plus_all", "superT.plusMinus(true);");
	printf("<IMG height=18 width=18 id='btn_minus_all' src='../images/remove_sm.gif'>");
	jsOnEventById("click", "btn_minus_all", "superT.plusMinus(false);");
        printf("&nbsp;<B>All</B><BR>");
        printf("</TD></TR>\n");
        }
    printf("<TR><TD NOWRAP>");
    if (!tdbIsDownloadsOnly(tdb))
        {
	char id[256];
        enum trackVisibility tv =
                hTvFromString(cartUsualString(cart, tdb->track,hStringFromTv(tdb->visibility)));
        // Don't use cheapCgi code... no name and no boolshad... just js
	safef(id, sizeof id, "%s_check", tdb->track);
        printf("<INPUT TYPE=CHECKBOX id='%s'%s>",
               id, (tv != tvHide?" CHECKED":""));
	jsOnEventById("change", id, "superT.childChecked(this);");

        safef(javascript, sizeof(javascript), "superT.selChanged(this)");
        struct slPair *event = slPairNew("change", cloneString(javascript));
        hTvDropDownClassVisOnlyAndExtra(tdb->track, tv, tdb->canPack,
                                        (tv == tvHide ? "hiddenText":"normalText"),
                                        trackDbSetting(tdb, "onlyVisibility"),
                                        event);

        printf("</TD>\n<TD>");
        hPrintPennantIcon(tdb);
	safef(id, sizeof id, "%s_link", tdb->track);
        printf("<A HREF='%s?%s=%s&c=%s&g=%s' id='%s'>%s</A>&nbsp;", 
                    tdbIsDownloadsOnly(tdb) ? hgFileUiName(): hTrackUiForTrack(tdb->track),
                    cartSessionVarName(), cartSessionId(cart), chromosome, cgiEncode(tdb->track), 
                    id, tdb->shortLabel);
	jsOnEventById("click", id, "superT.submitAndLink(this);");
        }
    else
        {
        printf("<A HREF='%s?%s=%s&g=%s'>Downloads</A>",
               hgFileUiName(),cartSessionVarName(), cartSessionId(cart), cgiEncode(tdb->track));
        printf("</TD>\n<TD>");
        printf("%s&nbsp;",tdb->shortLabel);
        }
    printf("</TD>\n");
    printf("<TD>%s", tdb->longLabel);

    printf("&nbsp;&nbsp;");
    printDataVersion(database, tdb);
    //printf("&nbsp;&nbsp;<EM style='color:#666666; font-size:smaller;'>%s</EM>", dataVersion);
    printf("</TD></TR>");
    }
printf("</TABLE>");
}

#ifdef USE_HAL
static void cfgHalSnake(struct trackDb *tdb, char *name)
{
boolean parentLevel = isNameAtParentLevel(tdb, name);
if (parentLevel)
    return;
char *fileName = trackDbSetting(tdb, "bigDataUrl");
if (fileName == NULL)
     return;
char *errString;
int handle = halOpenLOD(fileName, &errString);
if (handle < 0)
    errAbort("can't open HAL file: %s", fileName);
struct hal_species_t *speciesList, *sp;
char *otherSpecies = trackDbSetting(tdb, "otherSpecies");
extern char *database;

speciesList = halGetPossibleCoalescenceLimits(handle, otherSpecies, 
    trackHubSkipHubName(database), &errString);

int count = 0;
for(sp=speciesList; sp; sp = sp->next)
    count++;

if (count == 0)
    return;
char codeVarName[1024];
safef(codeVarName, sizeof codeVarName, "%s.coalescent", tdb->track);
char **ancestors;
AllocArray(ancestors, count);
count = 0;
for(sp=speciesList; sp; sp = sp->next)
    {
    ancestors[count] = sp->name;
    count++;
    }
char *coalescent = cartOptionalString(cart, codeVarName);
printf("<B>Set Coalescent Ancestor to:</B>");
cgiMakeDropListFull(codeVarName, ancestors, ancestors,
    count, coalescent, NULL, NULL);
}
#endif

void specificUi(struct trackDb *tdb, struct trackDb *tdbList, struct customTrack *ct, boolean ajax)
/* Draw track specific parts of UI. */
{
char *track = tdb->track;
char *db = database;
char *liftDb = cloneString(trackDbSetting(tdb, "quickLiftDb"));
if (liftDb != NULL) 
    db = liftDb;
// Ideally check cfgTypeFromTdb()/cfgByCfgType() first, but with all these special cases already in
//    place, lets be cautious at this time.
// NOTE: Developer, please try to use cfgTypeFromTdb()/cfgByCfgType().

boolean boxed = trackDbSettingClosestToHomeOn(tdb, "boxedCfg");
boolean isGencode3 = trackDbSettingOn(tdb, "isGencode3");
// UI precedence:
// 1) supers to get them out of the way: they have no controls
// 2) special cases based upon track name (developer please avoid)
// 3) cfgTypeFromTdb()/cfgByCfgType() <== prefered method
// 4) special cases falling through the cracks but based upon type
if (tdbIsSuperTrack(tdb))
    superTrackUi(tdb, tdbList);
else if (sameString(track, "stsMap"))
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
else if (sameString(track, "rertyHumanDiversityLd")
     ||  startsWith("hapmapLd", track)
     ||  sameString(tdb->type, "ld2"))
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
else if (sameString(track, "xenoRefGene")
     ||  sameString(track, "ncbiGene")
     ||  sameString(track, "refGene"))
    refGeneUI(tdb);
else if (startsWith("transMapAln", track) && (trackDbSetting(tdb, "bigDataUrl") == NULL))
    transMapUI(tdb);
else if (sameString(track, "rgdGene2"))
    rgdGene2UI(tdb);
else if (sameString(track, "knownGene") && !isGencode3)
    knownGeneUI(tdb);
else if (sameString(track, "omimLocation"))
    omimLocationUI(tdb);
else if (sameString(track, "omimGene2"))
    omimGene2UI(tdb);
else if (sameString(track, "omimGene"))
    omimGeneUI(tdb);
else if (sameString(track, "hg17Kg"))
    hg17KgUI(tdb);
else if (startsWith("ucscRetro", track)
     ||  startsWith("retroMrnaInfo", track))
    retroGeneUI(tdb);
else if (sameString(track, "ensGeneNonCoding"))
    ensemblNonCodingUI(tdb);
else if (startsWith("refSeqComposite", track))
    ncbiRefSeqUI(tdb);
else if (sameString(track, "ensGene"))
    ensGeneUI(tdb);
else if (sameString(track, "vegaGeneComposite"))
    vegaGeneUI(tdb);
else if (sameString(track, "rosetta"))
    rosettaUi(tdb);
else if (startsWith("pubs", track))
    pubsUi(tdb);
else if (startsWith("blastDm", track))
    blastFBUi(tdb);
else if (sameString(track, "blastSacCer1SG"))
    blastSGUi(tdb);
else if (sameString(track, "blastHg17KG")
     ||  sameString(track, "blastHg16KG")
     ||  sameString(track, "blastCe3WB")
     ||  sameString(track, "blastHg18KG")
     ||  sameString(track, "blatzHg17KG")
     ||  startsWith("mrnaMap", track)
     ||  startsWith("mrnaXeno", track))
    blastUi(tdb);
else if (sameString(track, "hgPcrResult"))
    pcrResultUi(tdb);
else if (sameString(track, "ancientR"))
    ancientRUi(tdb);
else if (sameString(track, "zoo")
     ||  sameString(track, "zooNew" ))
    zooWiggleUi(tdb);
else if (sameString(track, "humMusL")
     ||  sameString(track, "musHumL")
     ||  sameString(track, "regpotent")
     ||  sameString(track, "mm3Rn2L" )
     ||  sameString(track, "mm3Hg15L" )
     ||  sameString(track, "hg15Mm3L" ))
    humMusUi(tdb,7);
else if (startsWith("chain", track)
     || endsWith("chainSelf", track))
    chainColorUi(tdb);
else if (sameString(track, "orthoTop4") // still used ??
     ||  sameString(track, "mouseOrtho")
     ||  sameString(track, "mouseSyn"))
    // NOTE: type psl xeno <otherDb> tracks use crossSpeciesCfgUi, so
    // add explicitly here only if track has another type (bed, chain).
    // For crossSpeciesCfgUi, the
    // default for chrom coloring is "on", unless track setting
    // colorChromDefault is set to "off"
    crossSpeciesCfgUi(cart,tdb);
else if (sameString(track, "affyTranscriptome"))
    affyTranscriptomeUi(tdb);
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
else if (sameString(track, "dgv")
     || (startsWith("dgvV", track) && isdigit(track[4])))
    dgvUi(tdb);
else if (sameString(track, "all_mrna")
     ||  sameString(track, "mrna")
     ||  sameString(track, "all_est")
     ||  sameString(track, "est")
     ||  sameString(track, "tightMrna")
     ||  sameString(track, "tightEst")
     ||  sameString(track, "intronEst")
     ||  sameString(track, "xenoMrna")
     ||  sameString(track, "xenoEst"))
    mrnaCfgUi(cart, tdb, tdb->track, NULL, boxed);
else if (sameString(track, "lrg"))
    lrgCfgUi(cart, tdb, tdb->track, NULL, boxed);
else if (startsWithWord("hic", tdb->type) && tdbIsComposite(tdb))
    hicCfgUiComposite(cart, tdb, tdb->track, NULL, boxed); // Some hic options aren't available at this level
else if (tdb->type != NULL)
    {   // NOTE for developers: please avoid special cases and use cfgTypeFromTdb//cfgByCfgType()
        //  When you do, then multi-view cfg and subtrack cfg will work.
    eCfgType cType = cfgTypeFromTdb(tdb,FALSE);
    if (cType != cfgNone)
        {
        cfgByCfgType(cType,db, cart, tdb,tdb->track, NULL, boxed);
        if (startsWith("gtexGene", track))
            gtexGeneUi(cart, tdb, tdb->track, NULL, TRUE);
        else if (startsWith("gtexEqtlCluster", track))
            gtexEqtlClusterUi(cart, tdb, tdb->track, NULL, TRUE);
#ifdef USE_HAL
	if (cType == cfgSnake)
	    cfgHalSnake(tdb, tdb->track);
#endif
        }
    // NOTE: these cases that fall through the cracks should probably get folded into cfgByCfgType()
    else if (startsWithWord("expRatio", tdb->type))
        expRatioUi(tdb);
    else if (startsWith("chromGraph", tdb->type))
        chromGraphUi(tdb);
    else if (startsWith("sample", tdb->type))
        genericWiggleUi(tdb,7);
    else if (startsWithWord("array",tdb->type)) // not quite the same as "expRatio" (custom tracks)
        expRatioCtUi(tdb);
    else if (startsWithWord("factorSource",tdb->type))
        factorSourceUi(db,tdb);
    else if (startsWithWord("bigBed",tdb->type))
        labelCfgUi(db, cart, tdb, tdb->track);
    }

if (!ajax) // ajax asks for a simple cfg dialog for right-click popup or hgTrackUi subtrack cfg
    {
    // Composites *might* have had their top level controls just printed,
    // but almost certainly have additional controls
    boolean isLogo = (trackDbSetting(tdb, "logo") != NULL);
    if (tdbIsComposite(tdb) && !isLogo)  // for the moment generalizing this to include other containers...
        hCompositeUi(db, cart, tdb, NULL, NULL, MAIN_FORM);

    // Additional special case navigation links may be added
    extraUiLinks(db, tdb, cart);
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

struct jsonElement *jsonGlobalsHash = NULL; 

void jsonPrintGlobals()
// prints out the "common" globals json hash
// This hash is the one utils.js and therefore all CGIs know about
{
if (jsonGlobalsHash != NULL)
    {
    struct dyString *dy = dyStringNew(1024);
    jsonDyStringPrint(dy, jsonGlobalsHash, "common", 0);
    jsInline(dy->string);
    dyStringFree(&dy);
    }
}

void jsonObjectAddGlobal(char *name, struct jsonElement *ele)
/* Add json object to global hash */
{
if (jsonGlobalsHash == NULL)
    jsonGlobalsHash = newJsonObject(newHash(5));
jsonObjectAdd(jsonGlobalsHash, name, ele);
}

void showSupertrackInfo(struct trackDb *tdb)
{
// A bit of context when we're in hierarchy: parent description and sibling track list

if (!tdb->parent)
    return;

// show super-track info
struct trackDb *tdbParent = tdb->parent;

printf("<b>Track collection: "
           "<img height=12 src='../images/ab_up.gif'>"
            "<a href='%s?%s=%s&c=%s&g=%s'>%s </a></b>",
            hgTrackUiName(), cartSessionVarName(), cartSessionId(cart),
            chromosome, cgiEncode(tdbParent->track), tdbParent->longLabel);
printf("<p>");

if (tdbParent->html)
    {
    // collapsed panel for Description
    printf("<p><table>");  // required by jsCollapsible
    jsBeginCollapsibleSectionFontSize(cart, tdb->track, "superDescription", "Description", FALSE,
                                            "medium");
    // TODO: better done with regex
    char *html = replaceChars(tdbParent->html, "<H", "<h");
    html = replaceChars(html, "</H", "</h");

    // remove Description header
    html = replaceChars(html, "<h2>Description</h2>", "");
    html = replaceChars(html, "<h3>Description</h3>", "");
    html = replaceChars(html, "<h1>Description</h1>", "");

    // remove everything after Description text
    char *end = stringIn("<h2>", html);
    if (!end)
        end = stringIn("<h1>", html);
    if (!end)
        end = stringIn("<h3>", html);
    if (end)
        *end = '\0';
    printf("%s", html);
    printf("<p><i>To view the full description, click "
                "<a target='_blank' href='%s?%s=%s&c=%s&g=%s#TRACK_HTML'>here.</a></i>\n",
                        hgTrackUiName(), cartSessionVarName(), cartSessionId(cart),
                        chromosome, cgiEncode(tdbParent->track));
    jsEndCollapsibleSection();
    printf("</table>\n"); // required by jsCollapsible
    }

// collapsed panel for list of other tracks in the supertrack

char listTitle[1000];
safef(listTitle, sizeof listTitle, "All tracks in this collection (%d)", 
                    slCount(tdbParent->children));
printf("<table>");  // required by jsCollapsible
jsBeginCollapsibleSectionFontSize(cart, tdb->track, "superMembers", listTitle, FALSE, "medium");
printf("<table cellpadding='2' style='margin-left: 50px';>");
struct slRef *childRef;
tdbRefSortPrioritiesFromCart(cart, &tdbParent->children);
for (childRef = tdbParent->children; childRef != NULL; childRef = childRef->next)
    {
    struct trackDb *sibTdb = childRef->val;
    if (sameString(sibTdb->track, tdb->track))
        {
        printf("<tr><td><b>%s</b></td>\n", sibTdb->shortLabel);
        printf("<td>%s</td></tr>\n", sibTdb->longLabel);
        continue;
        }
    printf("<tr>");
    printf("<td><a href='%s?%s=%s&c=%s&g=%s'>%s</a>&nbsp;</td>", 
                tdbIsDownloadsOnly(sibTdb) ? hgFileUiName(): hTrackUiForTrack(sibTdb->track),
                cartSessionVarName(), cartSessionId(cart), chromosome, cgiEncode(sibTdb->track), 
                sibTdb->shortLabel);
    printf("<td>%s</td></tr>\n", sibTdb->longLabel);
    }
printf("</table>");
jsEndCollapsibleSection();
printf("</table>"); // required by jsCollapsible
printf("<hr>");
printf("</p>");
}

boolean tdbIsDupable(struct trackDb *tdb)
/* Return TRUE if a track is duplicatable */
{
if (!dupTrackEnabled())
    return FALSE;

/* Can't handle container tracks yet at least */
if (!tdbIsDataTrack(tdb))
    return FALSE;
/* A few other special case we can't handle */
if (startsWith("hub_", tdb->track))
    return FALSE;
if (startsWith("ct_", tdb->track))
    return FALSE;
if (sameString(tdb->track, "hgPcrResult"))
    return FALSE;
if (sameString(tdb->track, "ruler"))
    return FALSE;
if (sameString(tdb->track, "cutters"))
    return FALSE;
if (sameString(tdb->track, "oligoMatch"))
    return FALSE;
return TRUE;
}

void trackUi(struct trackDb *tdb, struct trackDb *tdbList, struct customTrack *ct, boolean ajax)
/* Put up track-specific user interface. */
{
if (!ajax)
    {
    jsIncludeFile("jquery.js", NULL);
    webIncludeResourceFile("jquery-ui.css");
    jsIncludeFile("jquery-ui.js", NULL);
    jsIncludeFile("utils.js",NULL);
    webIncludeResourceFile("spectrum.min.css");
    jsIncludeFile("spectrum.min.js",NULL);
    jsIncludeFile("jquery.tablednd.js", NULL);
    jsonObjectAddGlobal("track", newJsonString(tdb->track));
    jsonObjectAddGlobal("db", newJsonString(database));
    jsIncludeFile("hui.js",NULL);
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
        {
        printRelatedTracks(database,trackHash,tdb,cart);
        puts(tdb->html);
        }
    else
        {
        struct trackDb *tdbParent = tdb->parent;
        for (;tdbParent && (tdbParent->html == NULL || tdbParent->html[0] == 0);
              tdbParent = tdbParent->parent )
            ; // Get the first parent that has html
        if (tdbParent != NULL && tdbParent->html != NULL && tdbParent->html[0])
            {
            printf("<h2 style='color:%s'>Retrieved from %s Track...</h2>\n",
                   COLOR_DARKGREEN,tdbParent->shortLabel);
            printRelatedTracks(database,trackHash,tdb,cart);
            puts(tdbParent->html);
            }
        else
            printf("<h2>No description found for: %s.</h2>",tdbParent?tdbParent->track:tdb->track);
        }
    cartRemove(cart,"descriptionOnly"); // This is a once only request and should be deleted
    return;
    }
if (tdbIsContainer(tdb))
    {
    safef(setting,sizeof(setting),"%s.%s",tdb->track,RESET_TO_DEFAULTS);
    // NOTE: if you want track vis to not be reset, move to after vis dropdown
    if (1 == cartUsualInt(cart, setting, 0))
        cartRemoveAllForTdbAndChildren(cart,tdb);
    else if (!ajax) // Overkill on !ajax, because ajax shouldn't be called for a composite
        cartTdbTreeReshapeIfNeeded(cart,tdb);
    }

/* track configuration form */

printf("<FORM ACTION=\"%s?hgsid=%s&db=%s\" NAME=\""MAIN_FORM"\" METHOD=%s>\n\n",
       hgTracksName(), cartSessionId(cart), database, cartUsualString(cart, "formMethod", "POST"));
cartSaveSession(cart);
if (sameWord(tdb->track,"ensGene"))
    {
    char longLabel[256];
    struct trackVersion *trackVersion = getTrackVersion(database, tdb->track);
    if ((trackVersion != NULL) && !isEmpty(trackVersion->version))
        {
        if (!isEmpty(trackVersion->dateReference) && differentWord("current", trackVersion->dateReference))
            safef(longLabel, sizeof(longLabel), "Ensembl Gene Predictions - archive %s - %s", trackVersion->version, trackVersion->dateReference);
        else
            safef(longLabel, sizeof(longLabel), "Ensembl Gene Predictions - %s", trackVersion->version);
        }
    else
        safef(longLabel, sizeof(longLabel), "%s", tdb->longLabel);

    printf("<B style='font-size:200%%;'>%s%s</B>\n", longLabel, tdbIsSuper(tdb) ? " Tracks" : "");
    }
else if (sameWord(tdb->track, "refSeqComposite"))
    {
    char longLabel[1024];
    char *version = checkDataVersion(database, tdb);

    if (version)
	{
	safef(longLabel, sizeof(longLabel), "%s - Annotation Release %s", tdb->longLabel, version);
	}
    else
	{
	struct trackVersion *trackVersion = getTrackVersion(database, "ncbiRefSeq");
	if ((trackVersion != NULL) && !isEmpty(trackVersion->version))
	    {
	    safef(longLabel, sizeof(longLabel), "%s - Annotation Release %s", tdb->longLabel, trackVersion->version);
	    }
	else
	    safef(longLabel, sizeof(longLabel), "%s", tdb->longLabel);
	}
    printf("<B style='font-size:200%%;'>%s%s</B>\n", longLabel, tdbIsSuper(tdb) ? " Tracks" : "");
    }
else
    {
    if (trackDbSetting(tdb, "wgEncode"))
        printf("<A HREF='/ENCODE/index.html'><IMG style='vertical-align:middle;' "
               "width=100 src='/images/ENCODE_scaleup_logo.png'><A>");
    // set large title font size, but less so for long labels to minimize wrap
    printf("<B style='font-size:%d%%;'>%s%s</B>\n", strlen(tdb->longLabel) > 30 ? 133 : 200,
                tdb->longLabel, tdbIsSuper(tdb) ? " tracks" : "");
    }


/* Print link for parent track */
if (!ajax)
    {
    if (!tdb->parent)
        {
        // show group info
        struct grp *grp, *grps = hLoadGrps(database);
        for (grp = grps; grp != NULL; grp = grp->next)
            {
            if (sameString(grp->name,tdb->grp))
                {
                printf("&nbsp;&nbsp;<B style='font-size:100%%;'>"
                       "(<A HREF=\"%s?%s=%s&c=%s&hgTracksConfigPage=configure"
                       "&hgtgroup_%s_close=0#%sGroup\" title='%s tracks in track configuration "
                       "page'><IMG height=12 src='../images/ab_up.gif'>All %s%s</A>)</B>",
                       hgTracksName(), cartSessionVarName(), cartSessionId(cart),chromosome,
                       tdb->grp,tdb->grp,grp->label,grp->label,
                       endsWith(grp->label," Tracks")?"":" tracks");
                break;
                }
            }
        grpFreeList(&grps);
        }

    // incoming links from Google searches can go directly to a composite child trackUi page: tell users 
    // that they're inside a container now and can go back up the hierarchy
    if (tdbGetComposite(tdb)) {
        printf("<p>This track is a subtrack of the composite container track \"%s\".<br>", tdb->parent->shortLabel);
        printf("<a href='hgTrackUi?db=%s&c=%s&g=%s'>Click here</a> to display the \"%s\" container configuration page.", database, chromosome, tdb->parent->track, tdb->parent->shortLabel);
    }

    }

puts("<BR><BR>");

if (tdbIsSuperTrackChild(tdb))
    showSupertrackInfo(tdb);

if (ct && sameString(tdb->type, "maf"))
    tdb->canPack = TRUE;
else if (sameString(tdb->track, WIKI_TRACK_TABLE))
    // special case wikiTrack (there's no trackDb entry); fixes redmine 2395
    tdb->canPack = TRUE;
else if (sameString(tdb->type, "halSnake"))
    tdb->canPack = TRUE;
else if (!startsWith("bigWig", tdb->type) && startsWith("big", tdb->type))
    tdb->canPack = TRUE;
else if (sameString(tdb->type, "bigNarrowPeak"))
    tdb->canPack = TRUE;
else if (sameString(tdb->type, "hic"))
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
        boolean canPack = rTdbTreeCanPack(tdb);
        if (ajax)
            {
            // ajax popups should show currently inherited visability
            vis = tdbVisLimitedByAncestry(cart, tdb, TRUE);
            // composite children may inherit squish/pack vis so allow it.
            if (canPack == FALSE && tdbIsCompositeChild(tdb))
                canPack = rTdbTreeCanPack(tdbGetComposite(tdb));
            }
        else                               // But hgTrackUi page should show local vis
            vis = hTvFromString(cartUsualString(cart,tdb->track, hStringFromTv(vis)));

        if (tdbIsSuperTrackChild(tdb))
            {
	    char javascript[1024];
	    safef(javascript, sizeof(javascript), "visTriggersHiddenSelect(this);");
	    struct slPair *event = slPairNew("change", cloneString(javascript));

            hTvDropDownClassVisOnlyAndExtra(tdb->track,vis,canPack,"normalText superChild visDD",
                                            trackDbSetting(tdb, "onlyVisibility"), event);
            }
        else
            hTvDropDownClassVisOnlyAndExtra(tdb->track,vis,canPack,"normalText visDD",
                                            trackDbSetting(tdb, "onlyVisibility"),NULL);
        }

    if (!ajax)
        {
        printf("&nbsp;");
        cgiMakeButton("Submit", "Submit");
        // Offer cancel button always?    // composites and multiTracks (not standAlones or supers)
        if (tdbIsContainer(tdb))
            {
            printf("&nbsp;");
            cgiMakeOnClickButton("htui_cancel", "window.history.back();","Cancel");
            }

        if (tdbIsComposite(tdb))
	    {
            printf("\n&nbsp;&nbsp;<a href='#' id='htui_reset'>Reset to defaults</a>\n");
	    jsOnEventByIdF("click", "htui_reset",
                   "setVarAndPostForm('%s','1','mainForm'); return false;", setting);
	    }
        if ( isCustomComposite(tdb))
            {
            printf("\n&nbsp;&nbsp;<a href='%s' >Go to Track Collection Builder</a>\n", hgCollectionName());
            }
	/* Offer to dupe the non-containery tracks including composite and supertrack elements */
	if (tdbIsDupable(tdb))
	    {
	    printf("\n&nbsp;&nbsp;<a href='%s?%s=%s&c=%s&g=%s&hgTrackUi_op=dupe' >Duplicate track</a>\n", 
		hgTrackUiName(), cartSessionVarName(), cartSessionId(cart),
		chromosome, cgiEncode(tdb->track));
	    if (isDupTrack(tdb->track))
		{
		/* Offer to undupe */
		printf("\n&nbsp;&nbsp;<a href='%s?%s=%s&c=%s&g=%s&hgTrackUi_op=undupe' >Remove duplicate</a>\n", 
		    hgTrackUiName(), cartSessionVarName(), cartSessionId(cart),
		    chromosome, cgiEncode(tdb->track));
		}

	    }
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
            if (ajax)
                // reference to a separate form doesn't work in modal dialog,
                // so change window.location directly.
                safef(buf, sizeof(buf), "window.location='%s?hgsid=%s&%s=%s';return false;",
                      hgCustomName(), cartSessionId(cart), CT_SELECTED_TABLE_VAR, tdb->track);
            else
                safef(buf, sizeof(buf), "document.customTrackForm.submit();return false;");
            cgiMakeOnClickButton("htui_updtCustTrk", buf, "Update custom track");
            }
        }
    }

if (!tdbIsSuper(tdb) && !tdbIsDownloadsOnly(tdb) && !ajax)
    {
    // NAVLINKS - For pages w/ matrix, add Description, Subtracks and Downloads links
    if (trackDbSetting(tdb, "dimensions")
    ||  (trackDbSetting(tdb, "wgEncode") && tdbIsComposite(tdb)))
        {
        printf("\n&nbsp;&nbsp;<span id='navDown' style='float:right; display:none;'>");
        if (trackDbSetting(tdb, "wgEncode") && isEncode2(database, tdb->track))
            {
            printf("<A TARGET=_BLANK HREF='../ENCODE/index.html' TITLE='ENCODE Portal'>ENCODE at UCSC</A>");
            printf("&nbsp;&nbsp;");
            makeDownloadsLink(database, tdb);
            }
        char *downArrow = "&dArr;";
        enum browserType browser = cgiBrowser();
        if (browser == btIE || browser == btFF)
            downArrow = "&darr;";
        printf("&nbsp;&nbsp;<A HREF='#DISPLAY_SUBTRACKS' TITLE='Jump to subtrack list section of "
               "page'>Subtracks%s</A>", downArrow);
        printf("&nbsp;&nbsp;<A HREF='#TRACK_HTML' TITLE='Jump to description section of page'>"
               "Description%s</A>", downArrow);
        if (trackDbSetting(tdb, "wgEncode") && isEncode2(database, tdb->track))
            {
            printf("&nbsp;&nbsp;<A HREF='#TRACK_CREDITS' TITLE='Jump to ENCODE lab contacts for this data'>"
               "Contact%s</A>", downArrow);
            }
        printf("&nbsp;</span>");
        }
    }
if (!tdbIsSuperTrack(tdb) && !tdbIsComposite(tdb))
    puts("<BR>");

if (tdbIsDownloadsOnly(tdb))             // Composites without tracks but with files to download
    filesDownloadUi(database,cart,tdb);  // are tdb->type: downloadsOnly
else
    specificUi(tdb, tdbList, ct, ajax);

// Decorator UI
struct slName *decoratorSettings = trackDbSettingsWildMatch(tdb, "decorator.*");
if (decoratorSettings)
    {
    char *browserVersion;
    if (btIE == cgiClientBrowser(&browserVersion, NULL, NULL) && *browserVersion < '8')
        htmlHorizontalLine();
    else
        printf("<HR ALIGN='bottom' style='position:relative; top:1em;'>");
    decoratorUi(tdb, cart, decoratorSettings);
    }

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
    /* Print data version setting, if any */
    cgiDown(0.7);
    printRelatedTracks(database,trackHash,tdb,cart);
    printDataVersion(database, tdb);

    char *genome = hGenome(database);
    char *desc = hFreezeDateOpt(database);
    printf("<b>Assembly:</b> %s %s<br>", genome, desc);

    /* Print lift information from trackDb, if any */
    trackDbPrintOrigAssembly(tdb, database);

    printUpdateTime(database, tdb, NULL);
    }

char *liftDb = cloneString(trackDbSetting(tdb, "quickLiftDb"));
if (liftDb)
    tdb->html = getTrackHtml(liftDb, tdb->table);
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

    char *html = tdb->html;
    if (trackDbSetting(tdb, "wgEncode"))
        {
        // add anchor to Credits section of ENCODE HTML page so lab contacts are easily found (on top menu)
        html = replaceChars(tdb->html, "2>Credits", "2></H2><A NAME='TRACK_CREDITS'></A>\n<H2>Credits</H2>");
        }
    puts(html);

    printf("</td><td nowrap>");
    cgiDown(0.7); // positions top link below line
    makeTopLink(tdb);
    printf("&nbsp;</td></tr><tr valign='bottom'><td nowrap>");
    makeTopLink(tdb);
    printf("&nbsp;</td></tr></table>");
    }
}       /*      void trackUi(struct trackDb *tdb)       */

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

static char *handleDupOp(char *track, struct hash *trackHash)
/* Handle the duplication operation in the URL if any.  Return dupe name if
 * a dupe has happened. The trackHash is keyed by track name and has 
 * struct trackDb values. */
{
char *newTrack = NULL;

/* Handle duplicate, and possible in the future other operations on tracks. */
char *opVar = "hgTrackUi_op";
char *operation = cartUsualString(cart, opVar, NULL);
if (operation != NULL)
   {
   if (sameString(operation, "dupe"))
       {
       struct trackDb *tdb = hashFindVal(trackHash, dupTrackSkipToSourceName(track));
       newTrack = dupTrackInCartAndTrash(track, cart, tdb);
       }
   else if (sameString(operation, "undupe"))
       {
       newTrack = dupTrackSkipToSourceName(track);
       undupTrackInCartAndTrash(track, cart);
       }
   else
       {
       internalErr();
       }
   cartRemove(cart, opVar);
   }
return newTrack;
}

void doMiddle(struct cart *theCart)
/* Write body of web page. */
{
struct trackDb *tdbList = NULL;
struct trackDb *tdb = NULL;
char *track;
struct customTrack *ct = NULL, *ctList = NULL;
char *ignored;

/* used to have hgBotDelayFrac(0.25) here, replaced with earlyBotCheck()
 * at the beginning of main() to output message here if in delay time
 * 2021-06-21 - Hiram
 */
if (issueBotWarning)
    {
    char *ip = getenv("REMOTE_ADDR");
    botDelayMessage(ip, botDelayMillis);
    }

cart = theCart;
track = cartString(cart, "g");
getDbAndGenome(cart, &database, &ignored, NULL);
initGenbankTableNames(database);
chromosome = cartUsualString(cart, "c", hDefaultChrom(database));
trackHash = trackHashMakeWithComposites(database,chromosome,&tdbList,FALSE); 

/* Handle dup of track related stuff */
char *dupeName = handleDupOp(track, trackHash);
if (dupeName != NULL)
    track = dupeName;
struct dupTrack *dupList = dupTrackListFromCart(cart);
char *dupWholeName = NULL;
boolean isDup = isDupTrack(track);
if (isDup)
    {
    dupWholeName = track;
    track = dupTrackSkipToSourceName(track);
    }


/* Add in duplicate tracks. */
struct dupTrack *dup;
for (dup = dupList; dup != NULL; dup = dup->next)
    {
    struct trackDb *sourceTdb = hashFindVal(trackHash, dup->source);
    if (sourceTdb != NULL)
	{
	struct trackDb *dupTdb = dupTdbFrom(sourceTdb, dup);
        hashAdd(trackHash, dupTdb->track, dupTdb);
	if (sourceTdb->parent != NULL)
	    {
	    struct trackDb *parent = sourceTdb->parent;
	    // Add to parent here depending on whether composite or something else?
	    if (tdbIsFolder(parent))
		{
		refAdd(&parent->children, dupTdb);
		}
	    else
		{
		slAddHead(&parent->subtracks, dupTdb);
		dupTdb = NULL;  /* We use it! */
		}
	    }
	if (dupTdb != NULL)
	    slAddHead(&tdbList, dupTdb);
	}
    }

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

// Do  little more dupe handling - make a tdb for dupe if any 
if (isDup)
    {
    struct dupTrack *dup = dupTrackFindInList(dupList, dupWholeName);
    tdb = dupTdbFrom(tdb, dup);
    }


if(cartOptionalString(cart, "ajax"))
    {
    // html is going to be used w/n a dialog in hgTracks.js so serve up stripped down html
    // still need CSP2 header for security
    printf("%s", getCspMetaHeader());
    trackUi(tdb, tdbList, ct, TRUE);
    cartRemove(cart,"ajax");
    jsInlineFinish();
    }
else
    {
    char title[1000];
    if (tdb->parent)
        {
        safef(title, sizeof title, 
                        // TODO: replace in-line styling with class
                "<span style='background-color: #c3d4f4; "
                    "padding-left: 10px; padding-right: 10px;"
                    "margin-right: 10px; margin-left: -8px;'>"
                       "%s</span> %s", 
                tdb->parent->shortLabel, tdb->shortLabel);
        }
    else
        safef(title, sizeof title, "%s", tdb->shortLabel);
    char *titleEnd = (tdbIsSuper(tdb) ? "Tracks" :
               tdbIsDownloadsOnly(tdb) ? DOWNLOADS_ONLY_TITLE : "Track Settings");
    htmlNoEscape();     // allow HTML tags to format title blue bar (using short label)
    cartWebStart(cart, database, "%s %s", title, titleEnd);
    htmlDoEscape();
    trackUi(tdb, tdbList, ct, FALSE);
    printf("<BR>\n");
    jsonPrintGlobals();
    webEnd();
    }
}

char *excludeVars[] = { "submit", "Submit", "g", NULL, "ajax", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
long enteredMainTime = clock1000();
/* 0, 0, == use default 10 second for warning, 20 second for immediate exit */
issueBotWarning = earlyBotCheck(enteredMainTime, "hgTrackUi", delayFraction, 0, 0, "html");
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, NULL);
cgiExitTime("hgTrackUi", enteredMainTime);
return 0;
}
