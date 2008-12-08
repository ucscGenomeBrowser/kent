/* hui - human genome user interface common controls. */

#include "common.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "jksql.h"
#include "jsHelper.h"
#include "sqlNum.h"
#include "cart.h"
#include "hdb.h"
#include "hui.h"
#include "hCommon.h"
#include "hgConfig.h"
#include "chainCart.h"
#include "obscure.h"
#include "wiggle.h"
#include "phyloTree.h"
#include "hgMaf.h"
#include "customTrack.h"

static char const rcsid[] = "$Id: hui.c,v 1.139 2008/12/08 19:07:06 angie Exp $";

#define SMALLBUF 128
#define MAX_SUBGROUP 9
#define ADD_BUTTON_LABEL        "add"
#define CLEAR_BUTTON_LABEL      "clear"
#define JBUFSIZE 2048

#define PM_BUTTON "<A NAME=\"%s\"></A><A HREF=\"#%s\"><IMG height=18 width=18 onclick=\"return (setCheckBoxesThatContain('%s',%s,true,'%s','%s') == false);\" id=\"btn_%s\" src=\"../images/%s\" alt=\"%s\"></A>\n"
#define DEF_BUTTON "<A NAME=\"%s\"></A><A HREF=\"#%s\"><IMG onclick=\"setCheckBoxesThatContain('%s',true,false,'%s','%s'); return (setCheckBoxesThatContain('%s',false,false,'%s','%s','_defOff') == false);\" id=\"btn_%s\" src=\"../images/%s\" alt=\"%s\"></A>\n"
#define DEFAULT_BUTTON(nameOrId,anc,str1,str2) printf(DEF_BUTTON, (anc),(anc),(nameOrId),(str1),(str2),(nameOrId),(str1),(str2),(anc),"defaults_sm.png","default")
#define    PLUS_BUTTON(nameOrId,anc,str1,str2) printf(PM_BUTTON, (anc),(anc),(nameOrId),"true",(str1),(str2),(anc),"add_sm.gif","+")
#define   MINUS_BUTTON(nameOrId,anc,str1,str2) printf(PM_BUTTON, (anc),(anc),(nameOrId),"false",(str1),(str2),(anc),"remove_sm.gif","-")

char *hUserCookie()
/* Return our cookie name. */
{
if (hIsMgcServer())
    return "mgcuid";
else
    return cfgOptionDefault("central.cookie", "hguid");
}

char *wrapWhiteFont(char *s)
/* Write white font around s */
{
static char buf[256];
safef(buf, sizeof(buf), "<FONT COLOR=\"#FFFFFF\">%s</FONT>", s);
return buf;
}

char *hDocumentRoot()
/* get the path to the DocumentRoot, or the default */
{
return cfgOptionDefault("browser.documentRoot", DOCUMENT_ROOT);
}

char *hHelpFile(char *fileRoot)
/* Given a help file root name (e.g. "hgPcrResult" or "cutters"),
 * prepend the complete help directory path and add .html suffix.
 * Do not free the statically allocated result. */
{
static char helpName[PATH_LEN];
/* This cfgOption comes from Todd Lowe's hgTrackUi.c addition (r1.230): */
char *helpDir = cfgOption("help.html");
if (helpDir != NULL)
    safef(helpName, sizeof(helpName), "%s/%s.html", helpDir, fileRoot);
else
    safef(helpName, sizeof(helpName), "%s%s/%s.html", hDocumentRoot(),
	  HELP_DIR, fileRoot);
return helpName;
}

char *hFileContentsOrWarning(char *file)
/* Return the contents of the html file, or a warning message.
 * The file path may begin with hDocumentRoot(); if it doesn't, it is
 * assumed to be relative and hDocumentRoot() will be prepended. */
{
if (isEmpty(file))
    return cloneString("<BR>Program Error: Empty file name for include file"
		       "<BR>\n");
char path[PATH_LEN];
char *docRoot = hDocumentRoot();
if (startsWith(docRoot, file))
    safecpy(path, sizeof path, file);
else
    safef(path, sizeof path, "%s/%s", docRoot, file);
if (! fileExists(path))
    {
    char message[1024];
    safef(message, sizeof(message), "<BR>Program Error: Missing file %s</BR>",
	  path);
    return cloneString(message);
    }
/* If the file is there but not readable, readInGulp will errAbort,
 * but I think that is serious enough that errAbort is OK. */
char *result;
readInGulp(path, &result, NULL);
return result;
}

char *hCgiRoot()
/* get the path to the CGI directory.
 * Returns NULL when not running as a CGI (unless specified by browser.cgiRoot) */
{
static char defaultDir[PATH_LEN];
char *scriptFilename = getenv("SCRIPT_FILENAME");
if(scriptFilename)
    {
    char dir[PATH_LEN], name[FILENAME_LEN], extension[FILEEXT_LEN];
    dir[0] = 0;
    splitPath(scriptFilename, dir, name, extension);
    safef(defaultDir, sizeof(defaultDir), "%s", dir);
    int len = strlen(defaultDir);
    // Get rid of trailing slash to be consistent with hDocumentRoot
    if(defaultDir[len-1] == '/')
        defaultDir[len-1] = 0;
    }
else
    {
    defaultDir[0] = 0;
    }
return cfgOptionDefault("browser.cgiRoot", defaultDir);
}

char *hBackgroundImage()
/* get the path to the configured background image to use, or the default */
{
return cfgOptionDefault("browser.background", DEFAULT_BACKGROUND);
}

/******  Some stuff for tables of controls ******/

struct controlGrid *startControlGrid(int columns, char *align)
/* Start up a control grid. */
{
struct controlGrid *cg;
AllocVar(cg);
cg->columns = columns;
cg->align = cloneString(align);
cg->rowOpen = FALSE;
return cg;
}

void controlGridEndRow(struct controlGrid *cg)
/* Force end of row. */
{
printf("</tr>");
cg->rowOpen = FALSE;
cg->columnIx = 0;
}

void controlGridStartCell(struct controlGrid *cg)
/* Start a new cell in control grid. */
{
if (cg->columnIx == cg->columns)
    controlGridEndRow(cg);
if (!cg->rowOpen)
    {
    printf("<tr>");
    cg->rowOpen = TRUE;
    }
if (cg->align)
    printf("<td align=%s>", cg->align);
else
    printf("<td>");
}

void controlGridEndCell(struct controlGrid *cg)
/* End cell in control grid. */
{
printf("</td>");
++cg->columnIx;
}

void endControlGrid(struct controlGrid **pCg)
/* Finish up a control grid. */
{
struct controlGrid *cg = *pCg;
if (cg != NULL)
    {
    int i;
    if (cg->columnIx != 0 && cg->columnIx < cg->columns)
	for( i = cg->columnIx; i <= cg->columns; i++)
	    printf("<td>&nbsp;</td>\n");
    if (cg->rowOpen)
	printf("</tr>\n");
    printf("</table>\n");
    freeMem(cg->align);
    freez(pCg);
    }
}

/******  Some stuff for hide/dense/full controls ******/

static char *hTvStrings[] =
/* User interface strings for track visibility controls. */
    {
    "hide",
    "dense",
    "full",
    "pack",
    "squish"
    };

enum trackVisibility hTvFromStringNoAbort(char *s)
/* Given a string representation of track visibility, return as
 * equivalent enum. */
{
int vis = stringArrayIx(s, hTvStrings, ArraySize(hTvStrings));
if (vis < 0)
    vis = 0;  // don't generate bogus value on invalid input
return vis;
}

enum trackVisibility hTvFromString(char *s)
/* Given a string representation of track visibility, return as
 * equivalent enum. */
{
enum trackVisibility vis = hTvFromStringNoAbort(s);
if ((int)vis < 0)
   errAbort("Unknown visibility %s", s);
return vis;
}

char *hStringFromTv(enum trackVisibility vis)
/* Given enum representation convert to string. */
{
return hTvStrings[vis];
}

void hTvDropDownClassWithJavascript(char *varName, enum trackVisibility vis, boolean canPack, char *class,char * javascript)
/* Make track visibility drop down for varName with style class */
{
static char *noPack[] =
    {
    "hide",
    "dense",
    "full",
    };
static char *pack[] =
    {
    "hide",
    "dense",
    "squish",
    "pack",
    "full",
    };
static int packIx[] = {tvHide,tvDense,tvSquish,tvPack,tvFull};
if (canPack)
    cgiMakeDropListClassWithStyleAndJavascript(varName, pack, ArraySize(pack),
                            pack[packIx[vis]], class, TV_DROPDOWN_STYLE,javascript);
else
    cgiMakeDropListClassWithStyleAndJavascript(varName, noPack, ArraySize(noPack),
                            noPack[vis], class, TV_DROPDOWN_STYLE,javascript);
}

void hTvDropDownClassVisOnly(char *varName, enum trackVisibility vis,
	boolean canPack, char *class, char *visOnly)
/* Make track visibility drop down for varName with style class,
	and potentially limited to visOnly */
{
static char *denseOnly[] =
    {
    "hide",
    "dense",
    };
static char *squishOnly[] =
    {
    "hide",
    "squish",
    };
static char *packOnly[] =
    {
    "hide",
    "pack",
    };
static char *fullOnly[] =
    {
    "hide",
    "full",
    };
static char *noPack[] =
    {
    "hide",
    "dense",
    "full",
    };
static char *pack[] =
    {
    "hide",
    "dense",
    "squish",
    "pack",
    "full",
    };
static int packIx[] = {tvHide,tvDense,tvSquish,tvPack,tvFull};
if (visOnly != NULL)
    {
    int visIx = (vis > 0) ? 1 : 0;
    if (sameWord(visOnly,"dense"))
	cgiMakeDropListClassWithStyle(varName, denseOnly, ArraySize(denseOnly),
		denseOnly[visIx], class, TV_DROPDOWN_STYLE);
    else if (sameWord(visOnly,"squish"))
	cgiMakeDropListClassWithStyle(varName, squishOnly,
                ArraySize(squishOnly), squishOnly[visIx],
                class, TV_DROPDOWN_STYLE);
    else if (sameWord(visOnly,"pack"))
	cgiMakeDropListClassWithStyle(varName, packOnly, ArraySize(packOnly),
		packOnly[visIx], class, TV_DROPDOWN_STYLE);
    else if (sameWord(visOnly,"full"))
	cgiMakeDropListClassWithStyle(varName, fullOnly, ArraySize(fullOnly),
		fullOnly[visIx], class, TV_DROPDOWN_STYLE);
    else			/* default when not recognized */
	cgiMakeDropListClassWithStyle(varName, denseOnly, ArraySize(denseOnly),
		denseOnly[visIx], class, TV_DROPDOWN_STYLE);
    }
    else
    {
    if (canPack)
	cgiMakeDropListClassWithStyle(varName, pack, ArraySize(pack),
                            pack[packIx[vis]], class, TV_DROPDOWN_STYLE);
    else
	cgiMakeDropListClassWithStyle(varName, noPack, ArraySize(noPack),
                            noPack[vis], class, TV_DROPDOWN_STYLE);
    }
}

void hideShowDropDown(char *varName, boolean show, char *class)
/* Make hide/show dropdown for varName */
{
static char *hideShow[] =
    {
    "hide",
    "show"
    };
cgiMakeDropListClassWithStyle(varName, hideShow, ArraySize(hideShow),
                    hideShow[show], class, TV_DROPDOWN_STYLE);
}


/****** Some stuff for stsMap related controls *******/

static char *stsMapOptions[] = {
    "All Genetic",
    "Genethon",
    "Marshfield",
    "deCODE",
    "GeneMap 99",
    "Whitehead YAC",
    "Whitehead RH",
    "Stanford TNG",
};

enum stsMapOptEnum smoeStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, stsMapOptions);
if (x < 0)
   errAbort("Unknown option %s", string);
return x;
}

char *smoeEnumToString(enum stsMapOptEnum x)
/* Convert from enum to string representation. */
{
return stsMapOptions[x];
}

void smoeDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, stsMapOptions, ArraySize(stsMapOptions),
	curVal);
}

/****** Some stuff for stsMapMouseNew related controls *******/

static char *stsMapMouseOptions[] = {
    "All Genetic",
    "WICGR Genetic Map",
    "MGD Genetic Map",
    "RH",
};

enum stsMapMouseOptEnum smmoeStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, stsMapMouseOptions);
if (x < 0)
   errAbort("Unknown option %s", string);
return x;
}

char *smmoeEnumToString(enum stsMapMouseOptEnum x)
/* Convert from enum to string representation. */
{
return stsMapMouseOptions[x];
}

void smmoeDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, stsMapMouseOptions, ArraySize(stsMapMouseOptions),
	curVal);
}

/****** Some stuff for stsMapRat related controls *******/

static char *stsMapRatOptions[] = {
    "All Genetic",
    "FHHxACI",
    "SHRSPxBN",
    "RH",
};

enum stsMapRatOptEnum smroeStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, stsMapRatOptions);
if (x < 0)
   errAbort("Unknown option %s", string);
return x;
}

char *smroeEnumToString(enum stsMapRatOptEnum x)
/* Convert from enum to string representation. */
{
return stsMapRatOptions[x];
}

void smroeDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, stsMapRatOptions, ArraySize(stsMapRatOptions),
	curVal);
}

/****** Some stuff for fishClones related controls *******/

static char *fishClonesOptions[] = {
    "Fred Hutchinson CRC",
    "National Cancer Institute",
    "Sanger Centre",
    "Roswell Park Cancer Institute",
    "Cedars-Sinai Medical Center",
    "Los Alamos National Lab",
    "UC San Francisco",
};

enum fishClonesOptEnum fcoeStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, fishClonesOptions);
if (x < 0)
   errAbort("Unknown option %s", string);
return x;
}

char *fcoeEnumToString(enum fishClonesOptEnum x)
/* Convert from enum to string representation. */
{
return fishClonesOptions[x];
}

void fcoeDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, fishClonesOptions, ArraySize(fishClonesOptions),
	curVal);
}

/****** Some stuff for recombRate related controls *******/

static char *recombRateOptions[] = {
    "deCODE Sex Averaged Distances",
    "deCODE Female Distances",
    "deCODE Male Distances",
    "Marshfield Sex Averaged Distances",
    "Marshfield Female Distances",
    "Marshfield Male Distances",
    "Genethon Sex Averaged Distances",
    "Genethon Female Distances",
    "Genethon Male Distances",
};

enum recombRateOptEnum rroeStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, recombRateOptions);
if (x < 0)
   errAbort("Unknown option %s", string);
return x;
}

char *rroeEnumToString(enum recombRateOptEnum x)
/* Convert from enum to string representation. */
{
return recombRateOptions[x];
}

void rroeDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, recombRateOptions, ArraySize(recombRateOptions),
	curVal);
}

/****** Some stuff for recombRateRat related controls *******/

static char *recombRateRatOptions[] = {
    "SHRSPxBN Sex Averaged Distances",
    "FHHxACI Sex Averaged Distances",
};

enum recombRateRatOptEnum rrroeStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, recombRateRatOptions);
if (x < 0)
   errAbort("Unknown option %s", string);
return x;
}

char *rrroeEnumToString(enum recombRateRatOptEnum x)
/* Convert from enum to string representation. */
{
return recombRateRatOptions[x];
}

void rrroeDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, recombRateRatOptions, ArraySize(recombRateRatOptions),
	curVal);
}

/****** Some stuff for recombRateMouse related controls *******/

static char *recombRateMouseOptions[] = {
    "WI Genetic Map Sex Averaged Distances",
    "MGD Genetic Map Sex Averaged Distances",
};

enum recombRateMouseOptEnum rrmoeStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, recombRateMouseOptions);
if (x < 0)
   errAbort("Unknown option %s", string);
return x;
}

char *rrmoeEnumToString(enum recombRateMouseOptEnum x)
/* Convert from enum to string representation. */
{
return recombRateMouseOptions[x];
}

void rrmoeDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, recombRateMouseOptions, ArraySize(recombRateMouseOptions),
	curVal);
}

/****** Some stuff for CGH NCI60 related controls *******/

static char *cghNci60Options[] = {
    "Tissue Averages",
    "BREAST",
    "CNS",
    "COLON",
    "LEUKEMIA",
    "LUNG",
    "MELANOMA",
    "OVARY",
    "PROSTATE",
    "RENAL",
    "All Cell Lines",
};

enum cghNci60OptEnum cghoeStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, cghNci60Options);
if (x < 0)
   errAbort("Unknown option %s", string);
return x;
}

char *cghoeEnumToString(enum cghNci60OptEnum x)
/* Convert from enum to string representation. */
{
return cghNci60Options[x];
}

void cghoeDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, cghNci60Options, ArraySize(cghNci60Options),
	curVal);
}

/****** Some stuff for nci60 related controls *******/

static char *nci60Options[] = {
    "Tissue Averages",
    "All Cell Lines",
    "BREAST",
    "CNS",
    "COLON",
    "LEUKEMIA",
    "MELANOMA",
    "OVARIAN",
    "PROSTATE",
    "RENAL",
    "NSCLC",
    "DUPLICATE",
    "UNKNOWN"
    };

enum nci60OptEnum nci60StringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, nci60Options);
if (x < 0)
   errAbort("hui::nci60StringToEnum() - Unknown option %s", string);
return x;
}

char *nci60EnumToString(enum nci60OptEnum x)
/* Convert from enum to string representation. */
{
return nci60Options[x];
}

void nci60DropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, nci60Options, ArraySize(nci60Options),
	curVal);
}


/*** Control of base/codon coloring code: ***/

/* All options (parallel to enum baseColorDrawOpt): */
static char *baseColorDrawAllOptionLabels[] =
    {
    BASE_COLOR_DRAW_OFF_LABEL,
    BASE_COLOR_DRAW_GENOMIC_CODONS_LABEL,
    BASE_COLOR_DRAW_ITEM_CODONS_LABEL,
    BASE_COLOR_DRAW_DIFF_CODONS_LABEL,
    BASE_COLOR_DRAW_ITEM_BASES_LABEL,
    BASE_COLOR_DRAW_DIFF_BASES_LABEL,
    };
static char *baseColorDrawAllOptionValues[] =
    {
    BASE_COLOR_DRAW_OFF,
    BASE_COLOR_DRAW_GENOMIC_CODONS,
    BASE_COLOR_DRAW_ITEM_CODONS,
    BASE_COLOR_DRAW_DIFF_CODONS,
    BASE_COLOR_DRAW_ITEM_BASES,
    BASE_COLOR_DRAW_DIFF_BASES,
    };

/* Subset of options for tracks with CDS info but not item sequence: */
static char *baseColorDrawGenomicOptionLabels[] =
    {
    BASE_COLOR_DRAW_OFF_LABEL,
    BASE_COLOR_DRAW_GENOMIC_CODONS_LABEL,
    };
static char *baseColorDrawGenomicOptionValues[] =
    {
    BASE_COLOR_DRAW_OFF,
    BASE_COLOR_DRAW_GENOMIC_CODONS,
    };

/* Subset of options for tracks with aligned item sequence but not CDS: */
static char *baseColorDrawItemOptionLabels[] =
    {
    BASE_COLOR_DRAW_OFF_LABEL,
    BASE_COLOR_DRAW_ITEM_BASES_LABEL,
    BASE_COLOR_DRAW_DIFF_BASES_LABEL,
    };
static char *baseColorDrawItemOptionValues[] =
    {
    BASE_COLOR_DRAW_OFF,
    BASE_COLOR_DRAW_ITEM_BASES,
    BASE_COLOR_DRAW_DIFF_BASES,
    };

enum baseColorDrawOpt baseColorDrawOptStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, baseColorDrawAllOptionValues);
if (x < 0)
   errAbort("hui::baseColorDrawOptStringToEnum() - Unknown option %s", string);
return x;
}

static boolean baseColorGotCds(struct trackDb *tdb)
/* Return true if this track has CDS info according to tdb (or is genePred). */
{
boolean gotIt = FALSE;
char *setting = trackDbSetting(tdb, BASE_COLOR_USE_CDS);
if (isNotEmpty(setting))
    {
    if (sameString(setting, "all") || sameString(setting, "given") ||
	sameString(setting, "genbank") || startsWith("table", setting))
	gotIt = TRUE;
    else if (! sameString(setting, "none"))
	errAbort("trackDb for %s, setting %s: unrecognized value \"%s\".  "
		 "must be one of {none,all,given,genbank,table}.",
		 tdb->tableName, BASE_COLOR_USE_CDS, setting);
    }
else if (startsWith("genePred", tdb->type))
    gotIt = TRUE;
return gotIt;
}

static boolean baseColorGotSequence(struct trackDb *tdb)
/* Return true if this track has aligned sequence according to tdb. */
{
boolean gotIt = FALSE;
char *setting = trackDbSetting(tdb, BASE_COLOR_USE_SEQUENCE);
if (isNotEmpty(setting))
    {
    if (sameString(setting, "genbank") || sameString(setting, "seq") ||
	sameString(setting, "ss") || startsWith("extFile", setting) ||
	sameString(setting, "hgPcrResult"))
	gotIt = TRUE;
    else if (sameString(setting, "none"))
	errAbort("trackDb for %s, setting %s: unrecognized value \"%s\".  "
		 "must be one of {none,genbank,seq,extFile}.",
		 tdb->tableName, BASE_COLOR_USE_SEQUENCE, setting);
    }
return gotIt;
}

void baseColorDrawOptDropDown(struct cart *cart, struct trackDb *tdb)
/* Make appropriately labeled drop down of options if any are applicable.*/
{
enum baseColorDrawOpt curOpt = baseColorDrawOptEnabled(cart, tdb);
char *curValue = baseColorDrawAllOptionValues[curOpt];
char var[512];
boolean gotCds = baseColorGotCds(tdb);
boolean gotSeq = baseColorGotSequence(tdb);

safef(var, sizeof(var), "%s." BASE_COLOR_VAR_SUFFIX, tdb->tableName);
if (gotCds && gotSeq)
    {
    puts("<P><B>Color track by codons or bases:</B>");
    cgiMakeDropListFull(var, baseColorDrawAllOptionLabels,
			baseColorDrawAllOptionValues,
			ArraySize(baseColorDrawAllOptionLabels),
			curValue, NULL);
    printf("<BR><A HREF=\"%s\">Help on mRNA coloring</A><BR>",
	   CDS_MRNA_HELP_PAGE);
    }
else if (gotCds)
    {
    puts("<P><B>Color track by codons:</B>");
    cgiMakeDropListFull(var, baseColorDrawGenomicOptionLabels,
			baseColorDrawGenomicOptionValues,
			ArraySize(baseColorDrawGenomicOptionLabels),
			curValue, NULL);
    printf("<BR><A HREF=\"%s\">Help on codon coloring</A><BR>",
	   CDS_HELP_PAGE);
    }
else if (gotSeq)
    {
    puts("<P><B>Color track by bases:</B>");
    cgiMakeDropListFull(var, baseColorDrawItemOptionLabels,
			baseColorDrawItemOptionValues,
			ArraySize(baseColorDrawItemOptionLabels),
			curValue, NULL);
    printf("<BR><A HREF=\"%s\">Help on mRNA coloring</A><BR>",
	   CDS_MRNA_HELP_PAGE);
    }
}

enum baseColorDrawOpt baseColorDrawOptEnabled(struct cart *cart,
					      struct trackDb *tdb)
/* Query cart & trackDb to determine what drawing mode (if any) is enabled. */
{
char *stringVal = NULL;
char optionStr[256];
assert(cart);
assert(tdb);

/* trackDb can override default of OFF; cart can override trackDb. */
stringVal = trackDbSettingOrDefault(tdb, BASE_COLOR_DEFAULT,
				    BASE_COLOR_DRAW_OFF);
safef(optionStr, sizeof(optionStr), "%s." BASE_COLOR_VAR_SUFFIX,
      tdb->tableName);
stringVal = cartUsualString(cart, optionStr, stringVal);

return baseColorDrawOptStringToEnum(stringVal);
}


/*** Control of fancy indel display code: ***/

static boolean indelAppropriate(struct trackDb *tdb)
/* Return true if it makes sense to offer indel display options for tdb. */
{
return (tdb && startsWith("psl", tdb->type) &&
	(cfgOptionDefault("browser.indelOptions", NULL) != NULL));
}

void indelShowOptions(struct cart *cart, struct trackDb *tdb)
/* Make HTML inputs for indel display options if any are applicable. */
{
if (indelAppropriate(tdb))
    {
    boolean showDoubleInsert, showQueryInsert, showPolyA;
    char var[512];
    indelEnabled(cart, tdb, 0.0, &showDoubleInsert, &showQueryInsert, &showPolyA);
    printf("<P><B>Alignment Gap/Insertion Display Options</B><BR>\n");
    safef(var, sizeof(var), "%s_%s", INDEL_DOUBLE_INSERT, tdb->tableName);
    cgiMakeCheckBox(var, showDoubleInsert);
    printf("Draw double horizontal lines when both genome and query have "
	   "an insertion "
	   "<BR>\n");
    safef(var, sizeof(var), "%s_%s", INDEL_QUERY_INSERT, tdb->tableName);
    cgiMakeCheckBox(var, showQueryInsert);
    printf("Draw a vertical blue line for an insertion at the beginning or "
	   "end of the query, orange for insertion in the middle of the query"
	   "<BR>\n");
    safef(var, sizeof(var), "%s_%s", INDEL_POLY_A, tdb->tableName);
    /* We can highlight valid polyA's only if we have query sequence --
     * so indelPolyA code piggiebacks on baseColor code: */
    if (baseColorGotSequence(tdb))
	{
	cgiMakeCheckBox(var, showPolyA);
	printf("Draw a vertical green line where query has a polyA tail "
	       "insertion"
	       "<BR>\n");
	}

    printf("<A HREF=\"%s\">Help on alignment gap/insertion display options</A>"
	   "<BR>\n",
	   INDEL_HELP_PAGE);
    }
}

static boolean tdbOrCartBoolean(struct cart *cart, struct trackDb *tdb,
				char *settingName, char *defaultOnOff)
/* Query cart & trackDb to determine if a boolean variable is set. */
{
boolean alreadySet;
char optionStr[512];
alreadySet = !sameString("off",
		trackDbSettingOrDefault(tdb, settingName, defaultOnOff));
safef(optionStr, sizeof(optionStr), "%s_%s",
      settingName, tdb->tableName);
alreadySet = cartUsualBoolean(cart, optionStr, alreadySet);
return alreadySet;
}

void indelEnabled(struct cart *cart, struct trackDb *tdb, float basesPerPixel,
		  boolean *retDoubleInsert, boolean *retQueryInsert,
		  boolean *retPolyA)
/* Query cart & trackDb to determine what indel display (if any) is enabled. Set
 * basesPerPixel to 0.0 to disable check for zoom level.  */
{
boolean apropos = indelAppropriate(tdb);
if (apropos && (basesPerPixel > 0.0))
    {
    // check indel max zoom
    float showIndelMaxZoom = trackDbFloatSettingOrDefault(tdb, "showIndelMaxZoom", -1.0);
    if ((showIndelMaxZoom >= 0)
        && ((basesPerPixel > showIndelMaxZoom) || (showIndelMaxZoom == 0.0)))
        apropos = FALSE;
    }

if (retDoubleInsert)
    *retDoubleInsert = apropos &&
	tdbOrCartBoolean(cart, tdb, INDEL_DOUBLE_INSERT, "off");
if (retQueryInsert)
    *retQueryInsert = apropos &&
	tdbOrCartBoolean(cart, tdb, INDEL_QUERY_INSERT, "off");
if (retPolyA)
    *retPolyA = apropos &&
	tdbOrCartBoolean(cart, tdb, INDEL_POLY_A, "off");
}


/****** base position (ruler) controls *******/

static char *zoomOptions[] = {
    ZOOM_1PT5X,
    ZOOM_3X,
    ZOOM_10X,
    ZOOM_BASE
    };

void zoomRadioButtons(char *var, char *curVal)
/* Make a list of radio buttons for all zoom options */
{
int i;
int size = ArraySize(zoomOptions);
for (i = 0; i < size; i++)
    {
    char *s = zoomOptions[i];
    cgiMakeRadioButton(var, s, sameString(s, curVal));
    printf(" %s &nbsp;&nbsp;", s);
    }
}

/****** Some stuff for affy related controls *******/

static char *affyOptions[] = {
    "Chip Type",
    "Chip ID",
    "Tissue Averages"
    };

enum affyOptEnum affyStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, affyOptions);
if (x < 0)
   errAbort("hui::affyStringToEnum() - Unknown option %s", string);
return x;
}

char *affyEnumToString(enum affyOptEnum x)
/* Convert from enum to string representation. */
{
return affyOptions[x];
}

void affyDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, affyOptions, ArraySize(affyOptions),
	curVal);
}

/****** Some stuff for affy all exon related controls *******/

static char *affyAllExonOptions[] = {
    "Chip",
    "Tissue Averages"
    };

enum affyAllExonOptEnum affyAllExonStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, affyAllExonOptions);
if (x < 0)
   errAbort("hui::affyAllExonStringToEnum() - Unknown option %s", string);
return x;
}

char *affyAllExonEnumToString(enum affyAllExonOptEnum x)
/* Convert from enum to string representation. */
{
return affyAllExonOptions[x];
}

void affyAllExonDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, affyAllExonOptions, ArraySize(affyAllExonOptions),
	curVal);
}

/****** Some stuff for Rosetta related controls *******/

static char *rosettaOptions[] = {
    "All Experiments",
    "Common Reference and Other",
    "Common Reference",
    "Other Exps"
};

static char *rosettaExonOptions[] = {
    "Confirmed Only",
    "Predicted Only",
    "All",
};

enum rosettaOptEnum rosettaStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, rosettaOptions);
if (x < 0)
   errAbort("hui::rosettaStringToEnum() - Unknown option %s", string);
return x;
}

char *rosettaEnumToString(enum rosettaOptEnum x)
/* Convert from enum to string representation. */
{
return rosettaOptions[x];
}

void rosettaDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, rosettaOptions, ArraySize(rosettaOptions),
	curVal);
}

enum rosettaExonOptEnum rosettaStringToExonEnum(char *string)
/* Convert from string to enum representation of exon types. */
{
int x = stringIx(string, rosettaExonOptions);
if (x < 0)
   errAbort("hui::rosettaStringToExonEnum() - Unknown option %s", string);
return x;
}

char *rosettaExonEnumToString(enum rosettaExonOptEnum x)
/* Convert from enum to string representation of exon types. */
{
return rosettaExonOptions[x];
}

void rosettaExonDropDown(char *var, char *curVal)
/* Make drop down of exon type options. */
{
cgiMakeDropList(var, rosettaExonOptions, ArraySize(rosettaExonOptions), curVal);
}

/****** Options for the chain track color options *******/
static char *chainColorOptions[] = {
    CHROM_COLORS,
    SCORE_COLORS,
    NO_COLORS
    };

enum chainColorEnum chainColorStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, chainColorOptions);
if (x < 0)
   errAbort("hui::chainColorStringToEnum() - Unknown option %s", string);
return x;
}

char *chainColorEnumToString(enum chainColorEnum x)
/* Convert from enum to string representation. */
{
return chainColorOptions[x];
}

void chainColorDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, chainColorOptions, ArraySize(chainColorOptions), curVal);
}

/****** Options for the wiggle track Windowing *******/

static char *wiggleWindowingOptions[] = {
    "maximum",
    "mean",
    "minimum"
    };

enum wiggleWindowingEnum wiggleWindowingStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, wiggleWindowingOptions);
if (x < 0)
   errAbort("hui::wiggleWindowingStringToEnum() - Unknown option %s", string);
return x;
}

char *wiggleWindowingEnumToString(enum wiggleWindowingEnum x)
/* Convert from enum to string representation. */
{
return wiggleWindowingOptions[x];
}

void wiggleWindowingDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, wiggleWindowingOptions, ArraySize(wiggleWindowingOptions),
	curVal);
}

/****** Options for the wiggle track Smoothing *******/

static char *wiggleSmoothingOptions[] = {
    "OFF", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11",
    "12", "13", "14", "15", "16"
    };

enum wiggleSmoothingEnum wiggleSmoothingStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, wiggleSmoothingOptions);
if (x < 0)
   errAbort("hui::wiggleSmoothingStringToEnum() - Unknown option %s", string);
return x;
}

char *wiggleSmoothingEnumToString(enum wiggleSmoothingEnum x)
/* Convert from enum to string representation. */
{
return wiggleSmoothingOptions[x];
}

void wiggleSmoothingDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, wiggleSmoothingOptions, ArraySize(wiggleSmoothingOptions),
	curVal);
}

/****** Options for the wiggle track y Line Mark On/Off *******/

static char *wiggleYLineMarkOptions[] = {
    "OFF",
    "ON"
    };

enum wiggleYLineMarkEnum wiggleYLineMarkStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, wiggleYLineMarkOptions);
if (x < 0)
   errAbort("hui::wiggleYLineMarkStringToEnum() - Unknown option %s", string);
return x;
}

char *wiggleYLineMarkEnumToString(enum wiggleYLineMarkEnum x)
/* Convert from enum to string representation. */
{
return wiggleYLineMarkOptions[x];
}

void wiggleYLineMarkDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, wiggleYLineMarkOptions, ArraySize(wiggleYLineMarkOptions),
	curVal);
}

/****** Options for the wiggle track AutoScale *******/

static char *wiggleScaleOptions[] = {
    "use vertical viewing range setting",
    "auto-scale to data view"
    };

enum wiggleScaleOptEnum wiggleScaleStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, wiggleScaleOptions);
if (x < 0)
   errAbort("hui::wiggleScaleStringToEnum() - Unknown option %s", string);
return x;
}

char *wiggleScaleEnumToString(enum wiggleScaleOptEnum x)
/* Convert from enum to string representation. */
{
return wiggleScaleOptions[x];
}

void wiggleScaleDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, wiggleScaleOptions, ArraySize(wiggleScaleOptions),
	curVal);
}

/****** Options for the wiggle track type of graph *******/

static char *wiggleGraphOptions[] = {
    "points",
    "bar"
    };

enum wiggleGraphOptEnum wiggleGraphStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, wiggleGraphOptions);
if (x < 0)
   errAbort("hui::wiggleGraphStringToEnum() - Unknown option %s", string);
return x;
}

char *wiggleGraphEnumToString(enum wiggleGraphOptEnum x)
/* Convert from enum to string representation. */
{
return wiggleGraphOptions[x];
}

void wiggleGraphDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, wiggleGraphOptions, ArraySize(wiggleGraphOptions),
	curVal);
}

/****** Options for the wiggle track horizontal grid lines *******/

static char *wiggleGridOptions[] = {
    "ON",
    "OFF"
    };

enum wiggleGridOptEnum wiggleGridStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, wiggleGridOptions);
if (x < 0)
   errAbort("hui::wiggleGridStringToEnum() - Unknown option %s", string);
return x;
}

char *wiggleGridEnumToString(enum wiggleGridOptEnum x)
/* Convert from enum to string representation. */
{
return wiggleGridOptions[x];
}

void wiggleGridDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, wiggleGridOptions, ArraySize(wiggleGridOptions),
	curVal);
}

/****** Some stuff for wiggle track related controls *******/

static char *wiggleOptions[] = {
    "samples only",
    "linear interpolation"
    };

enum wiggleOptEnum wiggleStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, wiggleOptions);
if (x < 0)
   errAbort("hui::wiggleStringToEnum() - Unknown option %s", string);
return x;
}

char *wiggleEnumToString(enum wiggleOptEnum x)
/* Convert from enum to string representation. */
{
return wiggleOptions[x];
}

void wiggleDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, wiggleOptions, ArraySize(wiggleOptions),
	curVal);
}


/****** Some stuff for GCwiggle track related controls *******/

static char *GCwiggleOptions[] = {
    "samples only",
    "linear interpolation"
    };

enum GCwiggleOptEnum GCwiggleStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, GCwiggleOptions);
if (x < 0)
   errAbort("hui::GCwiggleStringToEnum() - Unknown option %s", string);
return x;
}

char *GCwiggleEnumToString(enum GCwiggleOptEnum x)
/* Convert from enum to string representation. */
{
return GCwiggleOptions[x];
}

void GCwiggleDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, GCwiggleOptions, ArraySize(GCwiggleOptions),
	curVal);
}

/****** Some stuff for chimp track related controls *******/

static char *chimpOptions[] = {
    "samples only",
    "linear interpolation"
    };

enum chimpOptEnum chimpStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, chimpOptions);
if (x < 0)
   errAbort("hui::chimpStringToEnum() - Unknown option %s", string);
return x;
}

char *chimpEnumToString(enum chimpOptEnum x)
/* Convert from enum to string representation. */
{
return chimpOptions[x];
}

void chimpDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, chimpOptions, ArraySize(chimpOptions),
	curVal);
}



/****** Some stuff for mRNA and EST related controls *******/

static void addMrnaFilter(struct mrnaUiData *mud, char *track, char *label, char *key, char *table)
/* Add an mrna filter */
{
struct mrnaFilter *fil;
char buf[128];
AllocVar(fil);
fil->label = label;
safef(buf, sizeof(buf), "%s_%s", track, key);
fil->key = cloneString(buf);
fil->table = table;
slAddTail(&mud->filterList, fil);
}

struct mrnaUiData *newBedUiData(char *track)
/* Make a new  in extra-ui data structure for a bed. */
{
struct mrnaUiData *mud;
char buf[128];  /* Expand me here */
AllocVar(mud);
safef(buf, sizeof(buf), "%sFt", track);
mud->filterTypeVar = cloneString(buf);
safef(buf, sizeof(buf), "%sLt", track);
mud->logicTypeVar = cloneString(buf);
addMrnaFilter(mud, track, "name", "name",track);
return mud;
}

struct mrnaUiData *newMrnaUiData(char *track, boolean isXeno)
/* Make a new  in extra-ui data structure for mRNA. */
{
struct mrnaUiData *mud;
char buf[128];
AllocVar(mud);
safef(buf, sizeof(buf), "%sFt", track);
mud->filterTypeVar = cloneString(buf);
safef(buf, sizeof(buf), "%sLt", track);
mud->logicTypeVar = cloneString(buf);
if (isXeno)
    addMrnaFilter(mud, track, "organism", "org", "organism");
addMrnaFilter(mud, track, "accession", "acc", "acc");
addMrnaFilter(mud, track, "author", "aut", "author");
addMrnaFilter(mud, track, "library", "lib", "library");
addMrnaFilter(mud, track, "tissue", "tis", "tissue");
addMrnaFilter(mud, track, "cell", "cel", "cell");
addMrnaFilter(mud, track, "keyword", "key", "keyword");
addMrnaFilter(mud, track, "gene", "gen", "geneName");
addMrnaFilter(mud, track, "product", "pro", "productName");
addMrnaFilter(mud, track, "description", "des", "description");
return mud;
}

int trackNameAndLabelCmp(const void *va, const void *vb)
/* Compare to sort on label. */
{
const struct trackNameAndLabel *a = *((struct trackNameAndLabel **)va);
const struct trackNameAndLabel *b = *((struct trackNameAndLabel **)vb);
return strcmp(a->label, b->label);
}

char *trackFindLabel(struct trackNameAndLabel *list, char *label)
/* Try to find label in list. Return NULL if it's
 * not there. */
{
struct trackNameAndLabel *el;
for (el = list; el != NULL; el = el->next)
    {
    if (sameString(el->label, label))
        return label;
    }
return NULL;
}

char *genePredDropDown(struct cart *cart, struct hash *trackHash,
                                        char *formName, char *varName)
/* Make gene-prediction drop-down().  Return track name of
 * currently selected one.  Return NULL if no gene tracks.
 * If formName isn't NULL, it's the form for auto submit (onchange attr).
 * If formName is NULL, no submit occurs when menu is changed */
{
char *cartTrack = cartOptionalString(cart, varName);
struct hashEl *trackList, *trackEl;
char *selectedName = NULL;
struct trackNameAndLabel *nameList = NULL, *name;
char *trackName = NULL;

/* Make alphabetized list of all genePred track names. */
trackList = hashElListHash(trackHash);
for (trackEl = trackList; trackEl != NULL; trackEl = trackEl->next)
    {
    struct trackDb *tdb = trackEl->val;
    char *dupe = cloneString(tdb->type);
    char *type = firstWordInLine(dupe);
    if ((sameString(type, "genePred")) && (!sameString(tdb->tableName, "tigrGeneIndex") && !tdbIsComposite(tdb)))
	{
	AllocVar(name);
	name->name = tdb->tableName;
	name->label = tdb->shortLabel;
	slAddHead(&nameList, name);
	}
    freez(&dupe);
    }
slSort(&nameList, trackNameAndLabelCmp);

/* No gene tracks - not much we can do. */
if (nameList == NULL)
    {
    slFreeList(&trackList);
    return NULL;
    }

/* Try to find current track - from cart first, then
 * knownGenes, then refGenes. */
if (cartTrack != NULL)
    selectedName = trackFindLabel(nameList, cartTrack);
if (selectedName == NULL)
    selectedName = trackFindLabel(nameList, "Known Genes");
if (selectedName == NULL)
    selectedName = trackFindLabel(nameList, "SGD Genes");
if (selectedName == NULL)
    selectedName = trackFindLabel(nameList, "BDGP Genes");
if (selectedName == NULL)
    selectedName = trackFindLabel(nameList, "WormBase Genes");
if (selectedName == NULL)
    selectedName = trackFindLabel(nameList, "RefSeq Genes");
if (selectedName == NULL)
    selectedName = nameList->name;

/* Make drop-down list. */
    {
    char javascript[SMALLBUF], *autoSubmit;
    int nameCount = slCount(nameList);
    char **menu;
    int i;

    AllocArray(menu, nameCount);
    for (name = nameList, i=0; name != NULL; name = name->next, ++i)
	{
	menu[i] = name->label;
	}
    if (formName == NULL)
        autoSubmit = NULL;
    else
        {
        safef(javascript, sizeof(javascript),
                "onchange=\"document.%s.submit();\"", formName);
        autoSubmit = javascript;
        }
    cgiMakeDropListFull(varName, menu, menu,
    	                        nameCount, selectedName, autoSubmit);
    freez(&menu);
    }

/* Convert to track name */
for (name = nameList; name != NULL; name = name->next)
    {
    if (sameString(selectedName, name->label))
        trackName = name->name;
    }

/* Clean up and return. */
slFreeList(&nameList);
slFreeList(&trackList);
return trackName;
}

struct hash *makeTrackHashWithComposites(char *database, char *chrom,
                                        bool withComposites)
/* Make hash of trackDb items for this chromosome, including composites,
   not just the subtracks. */
{
struct trackDb *tdbs = hTrackDb(database, chrom);
struct hash *trackHash = newHash(7);

while (tdbs != NULL)
    {
    struct trackDb *tdb = slPopHead(&tdbs);
    hLookupStringsInTdb(tdb, database);
    if (hTrackOnChrom(tdb, chrom))
        {
        if (tdb->subtracks)
            {
            struct trackDb *subtrack;
            for (subtrack = tdb->subtracks; subtrack != NULL;
                        subtrack = subtrack->next)
                {
                /* need track description for hgc */
                subtrack->html = cloneString(tdb->html);
                hashAdd(trackHash, subtrack->tableName, subtrack);
                }
            if (withComposites)
                hashAdd(trackHash, tdb->tableName, tdb);
            }
        else
            hashAdd(trackHash, tdb->tableName, tdb);
        }
    else
        trackDbFree(&tdb);
    }

return trackHash;
}

struct hash *makeTrackHash(char *database, char *chrom)
/* Make hash of trackDb items for this chromosome. */
{
    return makeTrackHashWithComposites(database, chrom, FALSE);
}

/****** Stuff for acembly related options *******/

static char *acemblyOptions[] = {
    "all genes",
    "main",
    "putative",
};

enum acemblyOptEnum acemblyStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, acemblyOptions);
if (x < 0)
   errAbort("hui::acemblyStringToEnum() - Unknown option %s", string);
return x;
}

char *acemblyEnumToString(enum acemblyOptEnum x)
/* Convert from enum to string representation. */
{
return acemblyOptions[x];
}

void acemblyDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, acemblyOptions, ArraySize(acemblyOptions),
	curVal);
}

boolean parseAssignment(char *words, char **name, char **value)
/* parse <name>=<value>, destroying input words in the process */
{
char *p;
if ((p = index(words, '=')) == NULL)
    return FALSE;
*p++ = 0;
if (name)
    *name = words;
if (value)
    *value = p;
return TRUE;
}

static char *getPrimaryType(char *primarySubtrack, struct trackDb *tdb)
/* Do not free when done. */
{
struct trackDb *subtrack = NULL;
char *type = NULL;
if (primarySubtrack)
    for (subtrack = tdb->subtracks; subtrack != NULL; subtrack = subtrack->next)
	{
	if (sameString(subtrack->tableName, primarySubtrack))
	    {
	    type = subtrack->type;
	    break;
	    }
	}
return type;
}

typedef struct _dividers {
    int count;
    char**subgroups;
    char* setting;
} dividers_t;

static dividers_t *dividersSettingGet(struct trackDb *parentTdb)
/* Parses any dividers setting in parent of subtracks */
{
dividers_t *dividers = needMem(sizeof(dividers_t));
dividers->setting    = cloneString(trackDbSetting(parentTdb, "dividers"));
if(dividers->setting == NULL)
    {
    freeMem(dividers);
    return NULL;
    }
dividers->subgroups  = needMem(24*sizeof(char*));
dividers->count      = chopByWhite(dividers->setting, dividers->subgroups,24);
return dividers;
}
static void dividersFree(dividers_t **dividers)
/* frees any previously obtained dividers setting */
{
if(dividers && *dividers)
    {
    freeMem((*dividers)->subgroups);
    freeMem((*dividers)->setting);
    freez(dividers);
    }
}

typedef struct _hierarchy {
    int count;
    char* subgroup;
    char**membership;
    int*  indents;
    char* setting;
} hierarchy_t;

static hierarchy_t *hierarchySettingGet(struct trackDb *parentTdb)
/* Parses any list hierachy instructions setting in parent of subtracks */
{
hierarchy_t *hierarchy = needMem(sizeof(hierarchy_t));
hierarchy->setting     = cloneString(trackDbSetting(parentTdb, "hierarchy"));  // To be freed later
if(hierarchy->setting == NULL)
    {
    freeMem(hierarchy);
    return NULL;
    }
int cnt,ix;
char *words[SMALLBUF];
cnt = chopLine(hierarchy->setting, words);
assert(cnt<=ArraySize(words));
if(cnt <= 1)
    {
    freeMem(hierarchy->setting);
    freeMem(hierarchy);
    return NULL;
    }

hierarchy->membership  = needMem(cnt*sizeof(char*));
hierarchy->indents     = needMem(cnt*sizeof(int));
hierarchy->subgroup    = words[0];
char *name,*value;
for (ix = 1,hierarchy->count=0; ix < cnt; ix++)
    {
    if (parseAssignment(words[ix], &name, &value))
        {
        hierarchy->membership[hierarchy->count]  = name;
        hierarchy->indents[hierarchy->count] = sqlUnsigned(value);
        hierarchy->count++;
        }
    }
return hierarchy;
}
static void hierarchyFree(hierarchy_t **hierarchy)
/* frees any previously obtained hierachy settings */
{
if(hierarchy && *hierarchy)
    {
    freeMem((*hierarchy)->setting);
    freeMem((*hierarchy)->membership);
    freeMem((*hierarchy)->indents);
    freez(hierarchy);
    }
}

typedef struct _dimensions {
    int count;
    char**names;
    char**subgroups;
    char* setting;
} dimensions_t;

boolean dimensionsExist(struct trackDb *parentTdb)
/* Does this parent track contain dimensions? */
{
    return (trackDbSetting(parentTdb, "dimensions") != NULL);
}
//static boolean dimensionsSubtrackOf(struct trackDb *childTdb)
///* Does this child belong to a parent  with dimensions? */
//{
//    return (tdbIsCompositeChild(childTdb) && dimensionsExist(childTdb->parent));
//}

static dimensions_t *dimensionSettingsGet(struct trackDb *parentTdb)
/* Parses any dimemnions setting in parent of subtracks */
{
dimensions_t *dimensions = needMem(sizeof(dimensions_t));
dimensions->setting = cloneString(trackDbSetting(parentTdb, "dimensions"));  // To be freed later
if(dimensions->setting == NULL)
    {
    freeMem(dimensions);
    return NULL;
    }
int cnt,ix;
char *words[SMALLBUF];
cnt = chopLine(dimensions->setting,words);
assert(cnt<=ArraySize(words));
if(cnt <= 0)
    {
    freeMem(dimensions->setting);
    freeMem(dimensions);
    return NULL;
    }

dimensions->names     = needMem(cnt*sizeof(char*));
dimensions->subgroups = needMem(cnt*sizeof(char*));
char *name,*value;
for (ix = 0,dimensions->count=0; ix < cnt; ix++)
    {
    if (parseAssignment(words[ix], &name, &value))
        {
        dimensions->names[dimensions->count]     = name;
        dimensions->subgroups[dimensions->count] = value;
        dimensions->count++;
        }
    }
return dimensions;
}
static void dimensionsFree(dimensions_t **dimensions)
/* frees any previously obtained dividers setting */
{
if(dimensions && *dimensions)
    {
    freeMem((*dimensions)->setting);
    freeMem((*dimensions)->names);
    freeMem((*dimensions)->subgroups);
    freez(dimensions);
    }
}

#define SUBGROUP_MAX 9

typedef struct _members {
    int count;
    char * tag;
    char * title;
    char **names;
    char **values;
    char * setting;
} members_t;

int subgroupCount(struct trackDb *parentTdb)
/* How many subGroup setting does this parent have? */
{
int ix;
int count = 0;
for(ix=1;ix<=SUBGROUP_MAX;ix++)
    {
    char subGrp[16];
    safef(subGrp, ArraySize(subGrp), "subGroup%d",ix);
    if(trackDbSetting(parentTdb, subGrp) != NULL)
        count++;
    }
return count;
}

char * subgroupSettingByTagOrName(struct trackDb *parentTdb, char *groupNameOrTag)
/* look for a subGroup by name (ie subGroup1) or tag (ie view) and return an unallocated char* */
{
int ix;
char *setting = NULL;
if(startsWith("subGroup",groupNameOrTag))
    {
    setting = trackDbSetting(parentTdb, groupNameOrTag);
    if(setting != NULL)
        return setting;
    }
for(ix=1;ix<=SUBGROUP_MAX;ix++) // How many do we support?
    {
    char subGrp[16];
    safef(subGrp, ArraySize(subGrp), "subGroup%d",ix);
    setting = trackDbSetting(parentTdb, subGrp);
    if(setting != NULL)  // Doesn't require consecutive subgroups
        {
        if(startsWithWord(groupNameOrTag,setting))
            return setting;
        }
    }
return NULL;
}

boolean subgroupingExists(struct trackDb *parentTdb, char *groupNameOrTag)
/* Does this parent track contain a particular subgrouping? */
{
    return (subgroupSettingByTagOrName(parentTdb,groupNameOrTag) != NULL);
}

static members_t *subgroupMembersGet(struct trackDb *parentTdb, char *groupNameOrTag)
/* Parse a subGroup setting line into tag,title, names(optional) and values(optional), returning the count of members or 0 */
{
int ix,cnt;
char *setting = subgroupSettingByTagOrName(parentTdb, groupNameOrTag);
if(setting == NULL)
    return NULL;
members_t *members = needMem(sizeof(members_t));
members->setting = cloneString(setting);
char *words[SMALLBUF];
cnt = chopLine(members->setting, words);
assert(cnt <= ArraySize(words));
if(cnt <= 1)
    {
    freeMem(members->setting);
    freeMem(members);
    return NULL;
    }
members->tag   = words[0];
members->title = strSwapChar(words[1],'_',' '); // Titles replace '_' with space
members->names = needMem(cnt*sizeof(char*));
members->values = needMem(cnt*sizeof(char*));
for (ix = 2,members->count=0; ix < cnt; ix++)
    {
    char *name,*value;
    if (parseAssignment(words[ix], &name, &value))
        {
        members->names[members->count]  = name;
        members->values[members->count] = strSwapChar(value,'_',' ');
        members->count++;
        }
    }
return members;
}
static members_t *subgroupMembersGetByDimension(struct trackDb *parentTdb, char dimension)
/* Finds the dimension requested and return its associated
 * subgroupMembership, returning the count of members or 0 */
{
int ix;
dimensions_t *dimensions = dimensionSettingsGet(parentTdb);
if(dimensions!=NULL)
    {
    for(ix=0;ix<dimensions->count;ix++)
        {
        if(startsWith("dimension",dimensions->names[ix])
        && strlen(dimensions->names[ix]) == 10
        && lastChar(dimensions->names[ix]) == dimension)
            {
            members_t *members = subgroupMembersGet(parentTdb, dimensions->subgroups[ix]);
            dimensionsFree(&dimensions);
            return members;
            }
        }
    dimensionsFree(&dimensions);
    }
return NULL;
}

static void subgroupMembersFree(members_t **members)
/* frees memory for subgroupMembers lists */
{
if(members && *members)
    {
    freeMem((*members)->setting);
    freeMem((*members)->names);
    freeMem((*members)->values);
    freez(members);
    }
}

typedef struct _membership {
    int count;
    char **subgroups;    // Ary of Tags in parentTdb->subGroupN and in childTdb->subGroups (ie view)
    char **membership;   // Ary of Tags of subGroups that child belongs to (ie PK)
    char **titles;       // Ary of Titles of subGroups a child belongs to (ie Peak)
    char * setting;
} membership_t;

static membership_t *subgroupMembershipGet(struct trackDb *childTdb)
/* gets all the subgroup membership for a child track */
{
membership_t *membership = needMem(sizeof(membership_t));
membership->setting = cloneString(trackDbSetting(childTdb, "subGroups"));
if(membership->setting == NULL)
    {
    freeMem(membership);
    return NULL;
    }

int ix,cnt;
char *words[SMALLBUF];
cnt = chopLine(membership->setting, words);
assert(cnt <= ArraySize(words));
if(cnt <= 0)
    {
    freeMem(membership->setting);
    freeMem(membership);
    return NULL;
    }
membership->subgroups  = needMem(cnt*sizeof(char*));
membership->membership = needMem(cnt*sizeof(char*));
membership->titles     = needMem(cnt*sizeof(char*));
for (ix = 0,membership->count=0; ix < cnt; ix++)
    {
    char *name,*value;
    if (parseAssignment(words[ix], &name, &value))
        {
        membership->subgroups[membership->count]  = name;
        membership->membership[membership->count] = value;//strSwapChar(value,'_',' ');
        members_t* members = subgroupMembersGet(childTdb->parent, name);
        membership->titles[membership->count] = NULL; // default
        if(members != NULL)
            {
            int ix2 = stringArrayIx(value,members->names,members->count);
            if(ix2 != -1)
                membership->titles[membership->count] = strSwapChar(cloneString(members->values[ix2]),'_',' ');
            subgroupMembersFree(&members);
            }
        membership->count++;
        }
    }
return membership;
}
static void subgroupMembershipFree(membership_t **membership)
/* frees subgroupMembership memory */
{
if(membership && *membership)
    {
    int ix;
    for(ix=0;ix<(*membership)->count;ix++) { freeMem((*membership)->titles[ix]); }
    freeMem((*membership)->titles);
    freeMem((*membership)->setting);
    freeMem((*membership)->subgroups);
    freeMem((*membership)->membership);
    freez(membership);
    }
}

boolean subgroupFind(struct trackDb *childTdb, char *name,char **value)
/* looks for a single tag in a child track's subGroups setting */
{
if(value != NULL)
    *value = NULL;
char *subGroups = trackDbSetting(childTdb, "subGroups");
if(subGroups == (void*)NULL)
    return FALSE;
char *found = stringIn(name, subGroups);
if(found == (void*)NULL)
    return FALSE;
if(found[strlen(name)] != '=')
    return FALSE;
if(value != (void*)NULL)
    {
    *value = cloneFirstWordInLine(found+strlen(name)+1);
    if(*value == NULL)
        return FALSE;
    }
return TRUE;
}
boolean subgroupFindTitle(struct trackDb *parentTdb, char *name,char **value)
/* looks for a a subgroup matching the name and returns the title if found */
{
if(value != (void*)NULL)
    *value = NULL;
members_t*members=subgroupMembersGet(parentTdb, name);
if(members==NULL)
    return FALSE;
*value = cloneString(members->title);
subgroupMembersFree(&members);
return TRUE;
}
void subgroupFree(char **value)
/* frees subgroup memory */
{
if(value && *value)
    freez(value);
}

typedef struct _sortOrder {
    int count;
    char*sortOrder;      // from cart (eg: CEL=+ FAC=- view=-)
    char*htmlId;         // {tableName}.sortOrder
    char**column;        // Always order in trackDb.ra (eg: FAC,CEL,view) TAG
    char**title;         // Always order in trackDb.ra (eg: Factor,Cell Line,View)
    boolean* forward;    // Always order in trackDb.ra but value of cart! (eg: -,+,-)
    int*  order;  // 1 based
    char *setting;
} sortOrder_t;

static sortOrder_t *sortOrderGet(struct cart *cart,struct trackDb *parentTdb)
/* Parses any list sort order instructions for parent of subtracks (from cart or trackDb) */
// Some trickiness here.  sortOrder->sortOrder is from cart (changed by user action), as is sortOrder->order,
//                        But columns are in original tdb order (unchanging)!
{
int ix;
char *setting = trackDbSetting(parentTdb, "sortOrder");
if(setting == NULL) // Must be in trackDb or not a sortable table
    return NULL;

sortOrder_t *sortOrder = needMem(sizeof(sortOrder_t));
sortOrder->htmlId = needMem(strlen(parentTdb->tableName)+15);
safef(sortOrder->htmlId, (strlen(parentTdb->tableName)+15), "%s.sortOrder", parentTdb->tableName);
char *cartSetting = cartCgiUsualString(cart, sortOrder->htmlId, setting);
if(strlen(cartSetting) == strlen(setting))
    sortOrder->sortOrder = cloneString(cartSetting);  // cart order
else
    sortOrder->sortOrder = cloneString(setting);      // old cart value is abandoned!
sortOrder->column  = needMem(12*sizeof(char*)); // There aren't going to be more than 3 or 4!
sortOrder->setting = cloneString(setting);
sortOrder->count   = chopByWhite(sortOrder->setting, sortOrder->column,12);
sortOrder->title   = needMem(sortOrder->count*sizeof(char*));
sortOrder->forward = needMem(sortOrder->count*sizeof(boolean));
sortOrder->order   = needMem(sortOrder->count*sizeof(int));
for (ix = 0; ix<sortOrder->count; ix++)
    {
    strSwapChar(sortOrder->column[ix],'=',0);  // Don't want 'CEL=+' but 'CEL' and '+'
    char *pos = stringIn(sortOrder->column[ix], sortOrder->sortOrder);// find tdb substr in cart current order string
    //assert(pos != NULL && pos[strlen(sortOrder->column[ix])] == '=');
    if(pos != NULL && pos[strlen(sortOrder->column[ix])] == '=')
        {
        int ord=1;
        char* pos2 = sortOrder->sortOrder;
        for(;*pos2 && pos2 < pos;pos2++)
            {
            if(*pos2 == '=') // Discovering sort order in cart
                ord++;
            }
        sortOrder->forward[ix] = (pos[strlen(sortOrder->column[ix]) + 1] == '+');
        sortOrder->order[ix] = ord;
        }
    else  // give up on cartSetting
        {
        sortOrder->forward[ix] = TRUE;
        sortOrder->order[ix] = ix+1;
        }
    subgroupFindTitle(parentTdb,sortOrder->column[ix],&(sortOrder->title[ix]));
    }
return sortOrder;  // NOTE cloneString:words[0]==*sortOrder->column[0] and will be freed when sortOrder is freed
}
static void sortOrderFree(sortOrder_t **sortOrder)
/* frees any previously obtained sortOrder settings */
{
if(sortOrder && *sortOrder)
    {
    int ix;
    for(ix=0;ix<(*sortOrder)->count;ix++) { subgroupFree(&((*sortOrder)->title[ix])); }
    freeMem((*sortOrder)->sortOrder);
    freeMem((*sortOrder)->htmlId);
    freeMem((*sortOrder)->column);
    freeMem((*sortOrder)->forward);
    freeMem((*sortOrder)->order);
    freeMem((*sortOrder)->setting);
    freez(sortOrder);
    }
}



#define COLOR_BG_DEFAULT        "#FFFEE8"
#define COLOR_BG_ALTDEFAULT     "#FFF9D2"
#define COLOR_BG_GHOST          "#EEEEEE"
#define COLOR_BG_PALE           "#F8F8F8"
#define COLOR_BG_DEFAULT_IX     0
#define COLOR_BG_ALTDEFAULT_IX  1
#define COLOR_DARKGREEN         "#008800"
#define COLOR_DARKBLUE          "#000088"
#define DIVIDING_LINE "<TR valign=\"CENTER\" line-height=\"1\" BGCOLOR=\"%s\"><TH colspan=\"5\" align=\"CENTER\"><hr noshade color=\"%s\" width=\"100%%\"></TD></TR>\n"
#define DIVIDER_PRINT(color) printf(DIVIDING_LINE,COLOR_BG_DEFAULT,(color))

static char *checkBoxIdMakeForTrack(struct trackDb *tdb,char *tagX,char *tagY,char *tagZ,membership_t *membership)
/* Creates an 'id' string for subtrack checkbox in style that matrix understand: "cb_dimX_dimY_view_cb" */
{
int ix;
#define CHECKBOX_ID_SZ 128
char *id = needMem(CHECKBOX_ID_SZ);
// What is wanted: id="cb_ES_K4_SIG_cb"
safef(id, CHECKBOX_ID_SZ, "cb_%s_", tdb->tableName);
//safecpy(id,CHECKBOX_ID_SZ,"cb_");
if(tagX != NULL)
    {
    ix = stringArrayIx(tagX, membership->subgroups, membership->count);
    if(ix >= 0)
        safef(id+strlen(id), CHECKBOX_ID_SZ-strlen(id), "%s_", membership->membership[ix]);
    }
if(tagY != NULL)
    {
    ix = stringArrayIx(tagY, membership->subgroups, membership->count);
    if(ix >= 0)
        safef(id+strlen(id), CHECKBOX_ID_SZ-strlen(id), "%s_", membership->membership[ix]);
    }
if(tagZ != NULL)
    {
    ix = stringArrayIx(tagZ, membership->subgroups, membership->count);
    if(ix >= 0)
        safef(id+strlen(id), CHECKBOX_ID_SZ-strlen(id), "%s_", membership->membership[ix]);
    }
if(membership != NULL)
    {
    ix = stringArrayIx("view", membership->subgroups, membership->count);   // view is a known tagname
    if(ix >= 0)
        safef(id+strlen(id), CHECKBOX_ID_SZ-strlen(id), "%s_", membership->membership[ix]);
    }
safecat(id+strlen(id), CHECKBOX_ID_SZ-strlen(id), "cb");
// If all else fails:
//if(strlen(id) <= 5)
//    safef(id, CHECKBOX_ID_SZ, "cb_%s", tdb->tableName);
return id;
}
static void checkBoxIdFree(char**id)
/* Frees 'id' string */
{
if(id && *id)
    freez(id);
}

static boolean divisionIfNeeded(char **lastDivide,dividers_t *dividers,membership_t *membership)
/* Keeps track of location within subtracks in order to provide requested division lines */
{
boolean division = FALSE;
if(dividers)
    {
    if(lastDivide != NULL)
        {
        int ix;
        for(ix=0;ix<dividers->count;ix++)
            {
            int sIx = stringArrayIx(dividers->subgroups[ix],membership->subgroups, membership->count);
            if((lastDivide[ix] == (void*)NULL && sIx >= 0)
            || (lastDivide[ix] != (void*)NULL && sIx <  0)
            || (strcmp(lastDivide[ix],membership->membership[sIx]) != 0) )
                {
                division = TRUE;
                if(lastDivide[ix] != (void*)NULL)
                    freeMem(lastDivide[ix]);
                lastDivide[ix] = ( sIx<0 ? (void*)NULL : cloneString(membership->membership[sIx]) );
                }
            }
        }
    //if(division)
    //    DIVIDER_PRINT(COLOR_DARKGREEN);
    }
return division;
}

static void indentIfNeeded(hierarchy_t*hierarchy,membership_t *membership)
/* inserts any needed indents */
{
int indent = 0;
if(hierarchy && hierarchy->count>0)
    {
    int ix;
    for(ix=0;ix<membership->count;ix++)
        {
        int iIx = stringArrayIx(membership->membership[ix], hierarchy->membership, hierarchy->count);
        if(iIx >= 0)
            {
            indent = hierarchy->indents[iIx];
            break;  // Only one
            }
        }
    }
for(;indent>0;indent--)
    puts ("&nbsp;&nbsp;&nbsp;");
}

static int daysOfMonth(struct tm *tp)
{
int days=0;
switch(tp->tm_mon)
    {
    case 3:
    case 5:
    case 8:
    case 10:    days = 30;   break;
    case 1:     days = 28;
                if( (tp->tm_year % 4) == 0
                && ((tp->tm_year % 20) != 0 || (tp->tm_year % 100) == 0) )
                    days = 29;
                break;
    default:    days = 31;   break;
    }
return days;
}

static void dateAdd(struct tm *tp,int addYears,int addMonths,int addDays)
/* Add years,months,days to a date */
{
tp->tm_mday  += addDays;
tp->tm_mon   += addMonths;
tp->tm_year  += addYears;
int dom=28;
while( (tp->tm_mon >11  || tp->tm_mon <0)
    || (tp->tm_mday>dom || tp->tm_mday<1) )
    {
    if(tp->tm_mon>11)   // First month: tm.tm_mon is 0-11 range
        {
        tp->tm_year += (tp->tm_mon / 12);
        tp->tm_mon  = (tp->tm_mon % 12);
        }
    else if(tp->tm_mon<0)
        {
        tp->tm_year += (tp->tm_mon / 12) - 1;
        tp->tm_mon  =  (tp->tm_mon % 12) + 12;
        }
    else
        {
        dom = daysOfMonth(tp);
        if(tp->tm_mday>dom)
            {
            tp->tm_mday -= dom;
            tp->tm_mon  += 1;
            dom = daysOfMonth(tp);
            }
        else if(tp->tm_mday < 1)
            {
            tp->tm_mon  -= 1;
            dom = daysOfMonth(tp);
            tp->tm_mday += dom;
            }
        }
    }
}
static char *dateAddToAndFormat(char *date,char *format,int addYears,int addMonths,int addDays)
/* Add years,months,days to a formatted date and returns the new date as a string on the stack
*  format is a strptime/strftime format: %F = yyyy-mm-dd */
{
char *newDate = needMem(12);
struct tm tp;
if(strptime (date,format, &tp))
    {
    dateAdd(&tp,addYears,addMonths,addDays); // tp.tm_year only contains years since 1900
    strftime(newDate,12,format,&tp);
    }
return newDate;  // newDate is never freed!
}

void tdbSortPrioritiesFromCart(struct cart *cart, struct trackDb **tdbList)
/* Updates the tdb_>priority from cart then sorts the list anew */
{
char htmlIdentifier[128];
struct trackDb *tdb;
for (tdb = *tdbList; tdb != NULL; tdb = tdb->next)
    {
        safef(htmlIdentifier, sizeof(htmlIdentifier), "%s.priority", tdb->tableName);
        tdb->priority = (float)cartUsualDouble(cart, htmlIdentifier, tdb->priority);
    }
slSort(tdbList, trackDbCmp);
}

// Not all track types have separate configuration
enum cfgType
{
    cfgNone    =0,
    cfgBedScore=1,
    cfgWig     =2,
    cfgWigMaf  =3,
    cfgPeak    =4
};

static enum cfgType cfgTypeFromTdb(struct trackDb *tdb)
/* determine what kind of track specific configuration is needed */
{
enum cfgType cType = cfgNone;
if(startsWith("wigMaf", tdb->type))
    cType = cfgWigMaf;
else if(startsWith("wig", tdb->type))
    cType = cfgWig;
else if(startsWith("bedGraph", tdb->type))
    cType = cfgWig;
else if(sameWord("bed5FloatScore",       tdb->type)
     || sameWord("bed5FloatScoreWithFdr",tdb->type))
    cType = cfgBedScore;
else if(sameWord("narrowPeak",tdb->type)
     || sameWord("broadPeak", tdb->type))
    cType = cfgPeak;
else if(startsWith("bed ", tdb->type)) // TODO: Only these are configurable so far
    {
    char *words[3];
    chopLine(cloneString( tdb->type), words);
    if (atoi(words[1]) >= 5 && trackDbSetting(tdb, "noScoreFilter") == NULL)
        cType = cfgBedScore;
    }

return cType;
}

static void cfgByCfgType(enum cfgType cType,char *db, struct cart *cart, struct trackDb *tdb,char *prefix, char *title, boolean boxed)
{
char *scoreMax;
int maxScore;
switch(cType)
    {
    case cfgBedScore:
    case cfgPeak:       scoreMax = trackDbSetting(tdb, "scoreFilterMax");
                        maxScore = (scoreMax ? sqlUnsigned(scoreMax):1000);
                        scoreCfgUi(db, cart,tdb->parent,prefix,title,maxScore,TRUE);
                        break;
    case cfgWig:        wigCfgUi(cart,tdb,prefix,title,TRUE);
                        break;
    case cfgWigMaf:     wigMafCfgUi(cart,tdb,prefix,title,TRUE, db);
                        break;
    default:            break;
    }
}

char *encodeRestrictionDateDisplay(struct trackDb *trackDb)
/* Create a string for ENCODE restriction date of this track */
{
if (!trackDb)
    return NULL;
char *date = trackDbSetting(trackDb, "dateReleased");
if (date)
    return strSwapChar(cloneString(date), ' ', 0);   // Truncate time
date = trackDbSetting(trackDb, "dateSubmitted");
if (date)
    return dateAddToAndFormat(strSwapChar(cloneString(date), ' ', 0), "%F", 0, 9, 0);
return NULL;
}

static void compositeUiSubtracks(char *db, struct cart *cart, struct trackDb *parentTdb,
                 boolean selectedOnly, char *primarySubtrack)
/* Show checkboxes for subtracks. */
{
struct trackDb *subtrack;
char *primaryType = getPrimaryType(primarySubtrack, parentTdb);
char htmlIdentifier[128];
char *words[5];
char *colors[2]   = { COLOR_BG_DEFAULT,
                      COLOR_BG_ALTDEFAULT };
int colorIx = COLOR_BG_DEFAULT_IX; // Start with non-default allows alternation

// Look for dividers, heirarchy, dimensions, sort and dragAndDrop!
char **lastDivide = NULL;
dividers_t *dividers = dividersSettingGet(parentTdb);
if(dividers)
    lastDivide = needMem(sizeof(char*)*dividers->count);
hierarchy_t *hierarchy = hierarchySettingGet(parentTdb);

enum
{
    dimX=0,
    dimY=1,
    dimZ=2,
    dimV=3,
    dimMax=4
};
members_t* dimensions[dimMax];
dimensions[dimX]=subgroupMembersGetByDimension(parentTdb,'X');
dimensions[dimY]=subgroupMembersGetByDimension(parentTdb,'Y');
dimensions[dimZ]=subgroupMembersGetByDimension(parentTdb,'Z');
dimensions[dimV]=subgroupMembersGet(parentTdb,"view");
int dimCount=0,di;
for(di=0;di<dimMax;di++) { if(dimensions[di]) dimCount++; }
sortOrder_t* sortOrder = sortOrderGet(cart,parentTdb);
boolean useDragAndDrop = sameOk("subTracks",trackDbSetting(parentTdb, "dragAndDrop"));

// Now we can start in on the table of subtracks
printf("\n<TABLE CELLSPACING='2' CELLPADDING='0' border='0'");
if(sortOrder != NULL)
    {
    printf(" id='subtracks.%s.sortable'",parentTdb->tableName);
    colorIx = COLOR_BG_ALTDEFAULT_IX;
    }
else
    printf(" id='subtracks.%s'",parentTdb->tableName);
if(useDragAndDrop)
    {
    printf(" class='tableWithDragAndDrop'");
    colorIx = COLOR_BG_ALTDEFAULT_IX;
    }
puts("><THEAD>");

if (!primarySubtrack)
    {
    char javascript[JBUFSIZE];
    boolean displayAll = sameString(cartUsualString(cart, "displaySubtracks", "all"), "all");
    boolean restrictions = FALSE;
    for (subtrack = parentTdb->subtracks; subtrack != NULL; subtrack = subtrack->next)
        {
        if(trackDbSetting(subtrack, "dateReleased")
        || trackDbSetting(subtrack, "dateSubmitted"))
            {
            restrictions = TRUE;
            break;
            }
        }

    printf("<TR");
    if(useDragAndDrop)
        printf(" id=\"noDrag\" class='nodrop nodrag'");
    printf("><TD colspan='%d'><B>Show checkboxes for:</B>&nbsp;",(sortOrder != NULL?sortOrder->count+2:3));
    safef(javascript, sizeof(javascript), "onclick=\"showOrHideSelectedSubtracks(true);\"");
    cgiMakeOnClickRadioButton("displaySubtracks", "selected", !displayAll,javascript);
    puts("Only selected subtracks &nbsp;");
    safef(javascript, sizeof(javascript), "onclick=\"showOrHideSelectedSubtracks(false);\"");
    cgiMakeOnClickRadioButton("displaySubtracks", "all", displayAll,javascript);
    puts("All subtracks&nbsp;&nbsp;&nbsp;&nbsp;</TD>");

    if(sortOrder != NULL)   // Add some sort buttons
        {
        puts("<TD colspan=5>&nbsp;</TD></TR>");
        printf("<TR id=\"%s.sortTr\" class='nodrop nodrag'>\n",parentTdb->tableName);     // class='nodrop nodrag'
        printf("<TD>&nbsp;<INPUT TYPE=HIDDEN NAME='%s' id='%s' VALUE=\"%s\"></TD>", sortOrder->htmlId, sortOrder->htmlId,sortOrder->sortOrder); // keeing track of priority
        // Columns in tdb order (unchanging), sort in cart order (changed by user action)
        int sIx=0;
        for(sIx=0;sIx<sortOrder->count;sIx++)
            {
            printf ("<TH id='%s.%s.sortTh' abbr='%c' onMouseOver=\"hintOverSortableColumnHeader(this)\" nowrap><A HREF='#nowhere' onclick=\"tableSortAtButtonPress(this,'%s');return false;\">%s</A><sup>%s",
                parentTdb->tableName,sortOrder->column[sIx],(sortOrder->forward[sIx]?'-':'+'),sortOrder->column[sIx],sortOrder->title[sIx],(sortOrder->forward[sIx]?"&darr;":"&uarr;"));
            if (sortOrder->count > 1)
                printf ("%d",sortOrder->order[sIx]);
            puts ("</sup></TH>");
            }
        puts("<TD>&nbsp;</TD>");
        }

    puts("<TH>&nbsp;</TH>");
    //puts("<TH align=\"center\" nowrap>&nbsp;Table&nbsp;</TH>");
    if(restrictions)
        {
        printf("<TH align=\"center\" nowrap>&nbsp;");
        printf("<A HREF=\'%s\' TARGET=BLANK>Restricted Until</A>", ENCODE_DATA_RELEASE_POLICY);
        puts("&nbsp;</TH>");
        }
    }

if(sortOrder != NULL || useDragAndDrop)
    {
    tdbSortPrioritiesFromCart(cart, &(parentTdb->subtracks)); // preserves user's prev sort/drags
    puts("</TR></THEAD><TBODY id='sortable_tbody'>");
    }
else
    {
    slSort(&(parentTdb->subtracks), trackDbCmp);  // straight from trackDb.ra
    puts("</TR></THEAD><TBODY>");
    }

for (subtrack = parentTdb->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    boolean alreadySet = TRUE;
    boolean isPrimary = FALSE;
    char *setting;
    int ix;

    safef(htmlIdentifier, sizeof(htmlIdentifier), "%s_sel", subtrack->tableName);
    if ((setting = trackDbSetting(subtrack, "subTrack")) != NULL)
        {
        if (chopLine(cloneString(setting), words) >= 2)
            alreadySet = differentString(words[1], "off");
        }
    alreadySet = cartUsualBoolean(cart, htmlIdentifier, alreadySet);
    isPrimary = (primarySubtrack &&
         sameString(subtrack->tableName, primarySubtrack));

    if (primarySubtrack)
        {
        if (isPrimary)
            {
            puts("<TR><TD>");
            cgiMakeHiddenBoolean(htmlIdentifier, TRUE);
            puts("[on] ");
            printf ("</TD><TD>%s [selected on main page]</TD></TR>\n",
                subtrack->longLabel);
            }
        else if (sameString(primaryType, subtrack->type) && hTableExists(db, subtrack->tableName))
            {
            puts("<TR><TD>");
            cgiMakeCheckBox(htmlIdentifier, alreadySet);
            printf ("</TD><TD>%s</TD></TR>\n", subtrack->longLabel);
            }
        }
    else
        {
        enum cfgType cType = cfgTypeFromTdb(subtrack);
        if(trackDbSettingOn(subtrack, "configurable") == FALSE)
            cType = cfgNone;
        membership_t *membership = subgroupMembershipGet(subtrack);
        if (hTableExists(db, subtrack->tableName))
            {
            if(sortOrder == NULL && !useDragAndDrop)
                {
                if( divisionIfNeeded(lastDivide,dividers,membership) )
                    colorIx = (colorIx == COLOR_BG_DEFAULT_IX ? COLOR_BG_ALTDEFAULT_IX : COLOR_BG_DEFAULT_IX);
                }

            char *id = checkBoxIdMakeForTrack(subtrack,
                        (dimensions[dimX]?dimensions[dimX]->tag:NULL),
                        (dimensions[dimY]?dimensions[dimY]->tag:NULL),
                        (dimensions[dimZ]?dimensions[dimZ]->tag:NULL),membership); // view is known tag
            printf("<TR valign='top' BGCOLOR=\"%s\"",colors[colorIx]);
            if(useDragAndDrop)
                printf(" class='trDraggable' title='Drag to Reorder' onmouseover=\"hintForDraggableRow(this)\"");

            printf(" id=\"tr_%s\" nowrap>\n<TD>",id);
            cgiMakeCheckBoxIdAndJS(htmlIdentifier,alreadySet,id,"onclick=\"matSubtrackCbClick(this);\" onmouseover=\"this.style.cursor='default';\"");
            if(sortOrder != NULL || useDragAndDrop)
                {
                safef(htmlIdentifier, sizeof(htmlIdentifier), "%s.priority", subtrack->tableName);
                float priority = (float)cartUsualDouble(cart, htmlIdentifier, subtrack->priority);
                printf("<INPUT TYPE=HIDDEN NAME='%s' id='%s' VALUE=\"%.0f\">", htmlIdentifier, htmlIdentifier, priority); // keeing track of priority
                }

            if(sortOrder != NULL)
                {
                int sIx=0;
                for(sIx=0;sIx<sortOrder->count;sIx++)
                    {
                    ix = stringArrayIx(sortOrder->column[sIx], membership->subgroups, membership->count);
                    if(ix >= 0)
                        {
#define CFG_SUBTRACK_LINK  "<A NAME=\"a_cfg_%s\"></A><A HREF=\"#a_cfg_%s\" onclick=\"return (subtrackCfgShow(this) == false);\" title=\"Configure Subtrack Settings\">%s</A>\n"
#define MAKE_CFG_SUBTRACK_LINK(name,title) printf(CFG_SUBTRACK_LINK, (name),(name),(title))
                        printf ("<TD id='%s' abbr='%s' align='left'>&nbsp;",sortOrder->column[sIx],membership->membership[ix]);
                        if(cType != cfgNone && sameString("view",sortOrder->column[sIx]))
                            MAKE_CFG_SUBTRACK_LINK(subtrack->tableName,membership->titles[ix]);
                        else
                            printf("%s\n",membership->titles[ix]);
                        puts ("</TD>");
                        }
                    }
                }
            else
                {
                printf ("<TD nowrap='true'>&nbsp;");
                indentIfNeeded(hierarchy,membership);
                if(cType != cfgNone)
                    MAKE_CFG_SUBTRACK_LINK(subtrack->tableName,subtrack->shortLabel);
                else
                    printf("%s\n",subtrack->shortLabel);
                puts ("</TD>");
                }
            printf ("<TD nowrap='true'>&nbsp;%s", subtrack->longLabel);
            if(trackDbSetting(parentTdb, "wgEncode") && trackDbSetting(subtrack, "accession"))
                printf (" [GEO:%s]", trackDbSetting(subtrack, "accession"));

            if(cType != cfgNone)
                {
                ix = stringArrayIx("view", membership->subgroups, membership->count);
                safef(htmlIdentifier,sizeof(htmlIdentifier),"%s.%s",subtrack->tableName,membership->membership[ix]);
                printf("<DIV id=\"div.%s.cfg\" style=\"display:none\">\n",subtrack->tableName);
                cfgByCfgType(cType,db,cart,subtrack,htmlIdentifier,"Subtrack",TRUE);
                puts("</DIV>\n");
                }
#define SCHEMA_LINK "<TD>&nbsp;<A HREF=\"../cgi-bin/hgTables?db=%s&hgta_group=%s&hgta_track=%s&hgta_table=%s&hgta_doSchema=describe+table+schema\" TARGET=_BLANK title='View table schema'>schema</A>&nbsp;\n"
            printf(SCHEMA_LINK, db, parentTdb->grp, parentTdb->tableName, subtrack->tableName);

            char *dateDisplay = encodeRestrictionDateDisplay(subtrack);
            if (dateDisplay)
                printf("</TD><TD align=\"CENTER\">&nbsp;%s&nbsp;", dateDisplay);

            puts("</TD></TR>");
            checkBoxIdFree(&id);
            }
        subgroupMembershipFree(&membership);
        }
    }
puts("</TBODY><TFOOT></TFOOT>");
puts("</TABLE>");
puts("<P>");
if (!primarySubtrack)
    puts("<script type='text/javascript'>matInitializeMatrix();</script>");
for(di=dimX;di<dimMax;di++)
    subgroupMembersFree(&dimensions[di]);
sortOrderFree(&sortOrder);
dividersFree(&dividers);
hierarchyFree(&hierarchy);
}


static void compositeUiAllSubtracks(char *db, struct cart *cart, struct trackDb *tdb,
				    char *primarySubtrack)
/* Show checkboxes for all subtracks, not just selected ones. */
{
compositeUiSubtracks(db, cart, tdb, FALSE, primarySubtrack);
}

static void compositeUiSelectedSubtracks(char *db, struct cart *cart, struct trackDb *tdb,
					 char *primarySubtrack)
/* Show checkboxes only for selected subtracks. */
{
compositeUiSubtracks(db, cart, tdb, TRUE, primarySubtrack);
}

static void makeAddClearSubmitTweak(char javascript[JBUFSIZE], char *formName,
				    char *buttonVar, char *label)
/* safef into javascript a sequence of commands that will force a refresh
 * of this same form, updating the values of whatever variables are necessary
 * to say what we want to do. */
{
safef(javascript, JBUFSIZE*sizeof(char),
      "document.%s.action = '%s'; document.%s.%s.value='%s'; "
      "document.%s.submit();",
      formName, cgiScriptName(), formName, buttonVar, label,
      formName);
}

#define MANY_SUBTRACKS  8

#define WIGGLE_HELP_PAGE  "../goldenPath/help/hgWiggleTrackHelp.html"

static void cfgBeginBoxAndTitle(boolean boxed, char *title)
/* Handle start of box and title for individual track type settings */
{
if (boxed)
    {
    printf("<TABLE class='blueBox' bgcolor=\"%s\" borderColor=\"%s\"><TR><TD>", COLOR_BG_ALTDEFAULT, COLOR_BG_ALTDEFAULT);
    if (title)
        printf("<CENTER><B>%s Configuration</B></CENTER>\n", title);
    }
else if (title)
    printf("<p><B>%s &nbsp;</b>", title );
else
    printf("<p>");
}

static void cfgEndBox(boolean boxed)
/* Handle end of box and title for individual track type settings */
{
if (boxed)
    puts("</td></tr></table>");
}

void wigCfgUi(struct cart *cart, struct trackDb *tdb,char *name,char *title,boolean boxed)
/* UI for the wiggle track */
{
char *typeLine = NULL;  /*  to parse the trackDb type line  */
char *words[8];     /*  to parse the trackDb type line  */
int wordCount = 0;  /*  to parse the trackDb type line  */
char option[256];
double minY;        /*  from trackDb or cart    */
double maxY;        /*  from trackDb or cart    */
double tDbMinY;     /*  data range limits from trackDb type line */
double tDbMaxY;     /*  data range limits from trackDb type line */
int defaultHeight;  /*  pixels per item */
char *horizontalGrid = NULL;    /*  Grid lines, off by default */
char *lineBar;  /*  Line or Bar graph */
char *autoScale;    /*  Auto scaling on or off */
char *windowingFunction;    /*  Maximum, Mean, or Minimum */
char *smoothingWindow;  /*  OFF or [2:16] */
char *yLineMarkOnOff;   /*  user defined Y marker line to draw */
double yLineMark;       /*  from trackDb or cart    */
int maxHeightPixels = atoi(DEFAULT_HEIGHT_PER);
int minHeightPixels = MIN_HEIGHT_PER;

cfgBeginBoxAndTitle(boxed, title);

wigFetchMinMaxPixelsWithCart(cart,tdb,name,&minHeightPixels, &maxHeightPixels, &defaultHeight);
typeLine = cloneString(tdb->type);
wordCount = chopLine(typeLine,words);

wigFetchMinMaxYWithCart(cart,tdb,name, &minY, &maxY, &tDbMinY, &tDbMaxY, wordCount, words);
freeMem(typeLine);

(void) wigFetchHorizontalGridWithCart(cart,tdb,name, &horizontalGrid);
(void) wigFetchAutoScaleWithCart(cart,tdb,name, &autoScale);
(void) wigFetchGraphTypeWithCart(cart,tdb,name, &lineBar);
(void) wigFetchWindowingFunctionWithCart(cart,tdb,name, &windowingFunction);
(void) wigFetchSmoothingWindowWithCart(cart,tdb,name, &smoothingWindow);
(void) wigFetchYLineMarkWithCart(cart,tdb,name, &yLineMarkOnOff);
wigFetchYLineMarkValueWithCart(cart,tdb,name, &yLineMark);

puts("<TABLE BORDER=0><TR><TD ALIGN=LEFT>");
printf("<b>Type of graph:&nbsp;</b>");
snprintf( option, sizeof(option), "%s.%s", name, LINEBAR );
wiggleGraphDropDown(option, lineBar);
puts("</TD></TR><TR><TD ALIGN=LEFT COLSPAN=2>");

puts("<b>y&nbsp;=&nbsp;0.0 line:&nbsp;</b>");
snprintf(option, sizeof(option), "%s.%s", name, HORIZGRID );
wiggleGridDropDown(option, horizontalGrid);
puts(" </TD></TR><TR><TD ALIGN=LEFT COLSPAN=2>");

printf("<b>Track height:&nbsp;</b>");
snprintf(option, sizeof(option), "%s.%s", name, HEIGHTPER );
cgiMakeIntVar(option, defaultHeight, 5);
printf("&nbsp;pixels&nbsp;&nbsp;(range: %d to %d)",
    minHeightPixels, maxHeightPixels);
puts("</TD></TR><TR><TD ALIGN=LEFT COLSPAN=2>");

printf("<b>Vertical viewing range</b>:&nbsp;&nbsp;\n<b>min:&nbsp;</b>");
snprintf(option, sizeof(option), "%s.%s", name, MIN_Y );
cgiMakeDoubleVar(option, minY, 6);
printf("&nbsp;&nbsp;&nbsp;<b>max:&nbsp;</b>");
snprintf(option, sizeof(option), "%s.%s", name, MAX_Y );
cgiMakeDoubleVar(option, maxY, 6);
printf("&nbsp;(range: %g to %g)",
    tDbMinY, tDbMaxY);
puts("</TD></TR><TR><TD ALIGN=LEFT COLSPAN=2>");

printf("<b>Data view scaling:&nbsp;</b>");
snprintf(option, sizeof(option), "%s.%s", name, AUTOSCALE );
wiggleScaleDropDown(option, autoScale);
puts("</TD></TR><TR><TD ALIGN=LEFT COLSPAN=2>");

printf("<b>Windowing function:&nbsp;</b>");
snprintf(option, sizeof(option), "%s.%s", name, WINDOWINGFUNCTION );
wiggleWindowingDropDown(option, windowingFunction);
puts("</TD></TR><TR><TD ALIGN=LEFT COLSPAN=2>");

printf("<b>Smoothing window:&nbsp;</b>");
snprintf(option, sizeof(option), "%s.%s", name, SMOOTHINGWINDOW );
wiggleSmoothingDropDown(option, smoothingWindow);
puts("&nbsp;pixels</TD></TR><TR><TD ALIGN=LEFT COLSPAN=1>");

puts("<b>Draw indicator line at y&nbsp;=&nbsp;</b>&nbsp;");
snprintf(option, sizeof(option), "%s.%s", name, YLINEMARK );
cgiMakeDoubleVar(option, yLineMark, 6);
printf("&nbsp;&nbsp;");
snprintf(option, sizeof(option), "%s.%s", name, YLINEONOFF );
wiggleYLineMarkDropDown(option, yLineMarkOnOff);
if(boxed)
    printf("</TD><TD align=\"RIGHT\">");
else
    puts("</TD></TR></TABLE>");
printf("<A HREF=\"%s\" TARGET=_blank>Graph configuration help</A>",WIGGLE_HELP_PAGE);
if(boxed)
    puts("</TD></TR></TABLE>");

cfgEndBox(boxed);
}

void scoreGrayLevelCfgUi(struct cart *cart, struct trackDb *tdb, int scoreMax)
/* If scoreMin has been set, let user select the shade of gray for that score, in case 
 * the default is too light to see or darker than necessary. */
{
char *scoreMinStr = trackDbSetting(tdb, "scoreMin");
if (scoreMinStr != NULL)
    {
    int scoreMin = atoi(scoreMinStr);
    // maxShade=9 taken from hgTracks/simpleTracks.c.  Ignore the 10 in shadesOfGray[10+1] --
    // maxShade is used to access the array.
    int maxShade = 9;  
    int scoreMinGrayLevel = scoreMin * maxShade/scoreMax;
    if (scoreMinGrayLevel <= 0) scoreMinGrayLevel = 1;
    char setting[256];
    safef(setting, sizeof(setting), "%s_minGrayLevel", tdb->tableName);
    int minGrayLevel = cartUsualInt(cart, setting, scoreMinGrayLevel);
    if (minGrayLevel <= 0) minGrayLevel = 1;
    if (minGrayLevel > maxShade) minGrayLevel = maxShade;
    puts("\n<P><B>Shade of lowest-scoring items: </B>");
    // Add class and javascript to select so that its color is consistent with option colors:
    printf("<SELECT NAME=\"%s\" class=grayShade%d onchange='", setting, minGrayLevel);
    int i, level;
    for (i = 1;  i < maxShade;  i++)
	{
	level = 255 - (255*i / maxShade);
	if (i > 1)
	    printf("else ");
	printf ("if (this.value == \"%d\") {this.style.color = \"#%02x%02x%02x\";} ",
		i, level, level, level);
	}
    level = 255 - (255*i / maxShade);
    printf("else {this.style.color = \"#%02x%02x%02x\";}'>\n", level, level, level);
    // Use class to set color of each option:
    for (i = 1;  i <= maxShade;  i++)
	{
	printf("<OPTION%s CLASS=grayShade%d VALUE=%d>",
	       (minGrayLevel == i ? " SELECTED" : ""), i, i);
	if (i == maxShade)
	    printf("&bull; black</OPTION>\n");
	else
	    printf("&bull; gray (%d%%)</OPTION>\n", i * (100/maxShade));
	}
    printf("</SELECT>\n");
    }
}

void scoreCfgUi(char *db, struct cart *cart, struct trackDb *parentTdb, char *name, char *title,  int maxScore, boolean boxed)
/* Put up UI for filtering bed track based on a score */
{
char option[256];

char *scoreValString = trackDbSetting(parentTdb, "scoreFilter");
int scoreSetting;
int scoreVal = 0;
char *words[2];

/* filter top-scoring N items in track */
char *scoreCtString = trackDbSetting(parentTdb, "filterTopScorers");
char *scoreFilterCt = NULL;
bool doScoreCtFilter = FALSE;

cfgBeginBoxAndTitle(boxed, title);

/* initial value of score theshold is 0, unless
 * overridden by the scoreFilter setting in the track */
if (scoreValString != NULL)
    scoreVal = atoi(scoreValString);
printf("<b>Show only items with score at or above:</b> ");
snprintf(option, sizeof(option), "%s.scoreFilter", name);
scoreSetting = cartUsualInt(cart,  option,  scoreVal);
cgiMakeIntVar(option, scoreSetting, 11);
printf("&nbsp;&nbsp;(range: 0&nbsp;to&nbsp;%d)", maxScore);

scoreGrayLevelCfgUi(cart, parentTdb, maxScore);

if (scoreCtString != NULL)
    {
    /* show only top-scoring items. This option only displayed if trackDb
     * setting exists.  Format:  filterTopScorers <on|off> <count> <table> */
    chopLine(cloneString(scoreCtString), words);
    safef(option, sizeof(option), "%s.filterTopScorersOn", name);
    doScoreCtFilter =
        cartCgiUsualBoolean(cart, option, sameString(words[0], "on"));
    puts("<P>");
    cgiMakeCheckBox(option, cartCgiUsualBoolean(cart, option, doScoreCtFilter));
    safef(option, sizeof(option), "%s.filterTopScorersCt", name);
    scoreFilterCt = cartCgiUsualString(cart, option, words[1]);

    puts("&nbsp; <B> Show only items in top-scoring </B>");
    cgiMakeTextVar(option, scoreFilterCt, 5);
    /* Only check size of table if track does not have subtracks */
    if (!tdbIsComposite(parentTdb))
        printf("&nbsp; (range: 1 to 100000, total items: %d)",
               getTableSize(db, parentTdb->tableName));
    }
cfgEndBox(boxed);
}


char **wigMafGetSpecies(struct cart *cart, struct trackDb *tdb, char *db, struct wigMafSpecies **list, int *groupCt)
{
int speciesCt = 0;
char *speciesGroup = trackDbSetting(tdb, SPECIES_GROUP_VAR);
char *speciesUseFile = trackDbSetting(tdb, SPECIES_USE_FILE);
char *speciesOrder = trackDbSetting(tdb, SPECIES_ORDER_VAR);
char sGroup[24];
//Ochar *groups[20];
struct wigMafSpecies *wmSpecies, *wmSpeciesList = NULL;
int group;
int i;
#define MAX_SP_SIZE 2000
#define MAX_GROUPS 20
char *species[MAX_SP_SIZE];
char option[MAX_SP_SIZE];

/* determine species and groups for pairwise -- create checkboxes */
if (speciesOrder == NULL && speciesGroup == NULL && speciesUseFile == NULL)
    {
    if (isCustomTrack(tdb->tableName))
	return NULL;
    errAbort("Track %s missing required trackDb setting: speciesOrder, speciesGroups, or speciesUseFile", tdb->tableName);
    }

char **groups = needMem(MAX_GROUPS * sizeof (char *));
*groupCt = 1;
if (speciesGroup)
    *groupCt = chopByWhite(speciesGroup, groups, MAX_GROUPS);

if (speciesUseFile)
    {
    if ((speciesGroup != NULL) || (speciesOrder != NULL))
	errAbort("Can't specify speciesUseFile and speciesGroup or speciesOrder");
    speciesOrder = cartGetOrderFromFile(db, cart, speciesUseFile);
    }

for (group = 0; group < *groupCt; group++)
    {
    if (*groupCt != 1 || !speciesOrder)
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
    	safef(option, sizeof(option), "%s.%s", tdb->tableName, wmSpecies->name);
	wmSpecies->on = cartUsualBoolean(cart, option, TRUE);
	//printf("checking %s and is %d\n",option,wmSpecies->on);
        wmSpecies->group = group;
        slAddHead(&wmSpeciesList, wmSpecies);
        }
    }
slReverse(&wmSpeciesList);
*list = wmSpeciesList;

return groups;
}

struct wigMafSpecies * wigMafSpeciesTable(struct cart *cart,
    struct trackDb *tdb, char *name, char *db)
{
int groupCt;
#define MAX_SP_SIZE 2000
char option[MAX_SP_SIZE];
int group, prevGroup;
int i,j;

bool lowerFirstChar = TRUE;
char *speciesTarget = trackDbSetting(tdb, SPECIES_TARGET_VAR);
char *speciesTree = trackDbSetting(tdb, SPECIES_TREE_VAR);

struct wigMafSpecies *wmSpeciesList;
char **groups = wigMafGetSpecies(cart, tdb, db, &wmSpeciesList, &groupCt);
struct wigMafSpecies *wmSpecies = wmSpeciesList;
struct slName *speciesList = NULL;

for(; wmSpecies; wmSpecies = wmSpecies->next)
    {
    struct slName *newName = slNameNew(wmSpecies->name);
    slAddHead(&speciesList, newName);
    //printf("%s<BR>\n",speciesList->name);
    }
slReverse(&speciesList);

int numberPerRow;
struct phyloTree *tree;
boolean lineBreakJustPrinted;
char trackName[255];
char query[256];
char **row;
struct sqlConnection *conn;
struct sqlResult *sr;
char *words[MAX_SP_SIZE];
int defaultOffSpeciesCnt = 0;

jsIncludeFile("utils.js",NULL);
//jsInit();
puts("\n<P><B>Species selection:</B>&nbsp;");

PLUS_BUTTON( "id", "plus_pw","cb_maf_","_maf_");
MINUS_BUTTON("id","minus_pw","cb_maf_","_maf_");
cgiContinueHiddenVar("g");

char prefix[512];
safef(prefix, sizeof prefix, "%s.", name);
char *defaultOffSpecies = trackDbSetting(tdb, "speciesDefaultOff");
if (defaultOffSpecies)
    {
    DEFAULT_BUTTON( "id", "default_pw","cb_maf_","_maf_");
    int wordCt = chopLine(defaultOffSpecies, words);
    defaultOffSpeciesCnt = wordCt;
    }

if ((speciesTree != NULL) && ((tree = phyloParseString(speciesTree)) != NULL))
    {
    char buffer[128];
    char *nodeNames[512];
    int numNodes = 0;
    char *path, *orgName;
    int ii;

    safef(buffer, sizeof(buffer), "%s.vis",name);
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
    puts("\n<TABLE><TR>");
group = -1;
lineBreakJustPrinted = FALSE;
for (wmSpecies = wmSpeciesList, i = 0, j = 0; wmSpecies != NULL;
		    wmSpecies = wmSpecies->next, i++)
    {
    char *label;
    prevGroup = group;
    group = wmSpecies->group;
    if (groupCt != 1 && group != prevGroup)
	{
	i = 0;
	j = 0;
	if (group != 0)
	    puts("</TR></TABLE>\n");
        /* replace underscores in group names */
        subChar(groups[group], '_', ' ');
	printf("<P>&nbsp;&nbsp;<B><EM>%s</EM></B>", groups[group]);
    printf("&nbsp;&nbsp;");
    safef(option, sizeof(option), "plus_%s", groups[group]);
    PLUS_BUTTON( "id",option,"cb_maf_",groups[group]);
    safef(option, sizeof(option),"minus_%s", groups[group]);
    MINUS_BUTTON("id",option,"cb_maf_",groups[group]);

	puts("\n<TABLE><TR>");
	}
    if (hIsGsidServer())
	numberPerRow = 6;
    else
	numberPerRow = 5;

    /* new logic to decide if line break should be displayed here */
    if ((j != 0 && (j % numberPerRow) == 0) && (lineBreakJustPrinted == FALSE))
        {
        puts("</TR><TR>");
        lineBreakJustPrinted = TRUE;
        }

    char id[MAX_SP_SIZE];
    boolean checked = TRUE;
    if(defaultOffSpeciesCnt > 0)
        {
        if(stringArrayIx(wmSpecies->name,words,defaultOffSpeciesCnt) == -1)
            safef(id, sizeof(id), "cb_maf_%s_%s", groups[group], wmSpecies->name);
        else
            {
            safef(id, sizeof(id), "cb_maf_%s_%s_defOff", groups[group], wmSpecies->name);
            checked = FALSE;
            }
        }
    else
        safef(id, sizeof(id), "cb_maf_%s_%s", groups[group], wmSpecies->name);

    if (hIsGsidServer())
        {
        char *chp;
        /* for GSID maf, display only entries belong to the specific MSA selected */
    	safef(option, sizeof(option), "%s.%s", name, wmSpecies->name);
    	label = hOrganism(wmSpecies->name);
    	if (label == NULL)
            label = wmSpecies->name;
        strcpy(trackName, tdb->tableName);

        /* try AaMaf first */
        chp = strstr(trackName, "AaMaf");
        /* if it is not a AaMaf track, try Maf next */
        if (chp == NULL) chp = strstr(trackName, "Maf");

        /* test if the entry actually is part of the specific maf track data */
        if (chp != NULL)
            {
            *chp = '\0';
            safef(query, sizeof(query),
            "select id from %sMsa where id = 'ss.%s'", trackName, label);

            conn = hAllocConn(db);
            sr = sqlGetResult(conn, query);
            row = sqlNextRow(sr);

            /* offer it only if the entry is found in current maf data set */
            if (row != NULL)
                {
                puts("<TD>");
                cgiMakeCheckBoxWithId(option, cartUsualBoolean(cart, option, checked),id);
                printf ("%s", label);
                puts("</TD>");
                fflush(stdout);
                lineBreakJustPrinted = FALSE;
                j++;
                }
            sqlFreeResult(&sr);
            hFreeConn(&conn);
            }
        }
    else
    	{
    	puts("<TD>");
    	safef(option, sizeof(option), "%s.%s", name, wmSpecies->name);
        wmSpecies->on = cartUsualBoolean(cart, option, checked);
        cgiMakeCheckBoxWithId(option, wmSpecies->on,id);
    	label = hOrganism(wmSpecies->name);
    	if (label == NULL)
		label = wmSpecies->name;
        if (lowerFirstChar)
            *label = tolower(*label);
    	printf ("%s<BR>", label);
    	puts("</TD>");
        lineBreakJustPrinted = FALSE;
        j++;
        }
    }
puts("</TR></TABLE><BR>\n");
return wmSpeciesList;
}

void wigMafCfgUi(struct cart *cart, struct trackDb *tdb,char *name, char *title, boolean boxed, char *db)
/* UI for maf/wiggle track
 * NOTE: calls wigCfgUi */
{
bool lowerFirstChar = TRUE;
int i;
char option[MAX_SP_SIZE];

cfgBeginBoxAndTitle(boxed, title);

char *defaultCodonSpecies = trackDbSetting(tdb, SPECIES_CODON_DEFAULT);
char *framesTable = trackDbSetting(tdb, "frames");
char *firstCase = trackDbSetting(tdb, ITEM_FIRST_CHAR_CASE);
if (firstCase != NULL)
    {
    if (sameWord(firstCase, "noChange")) lowerFirstChar = FALSE;
    }
char *treeImage = NULL;
struct consWiggle *consWig, *consWiggles = wigMafWiggles(db, tdb);


boolean isWigMafProt = FALSE;

if (strstr(tdb->type, "wigMafProt")) isWigMafProt = TRUE;

puts("<TABLE><TR><TD VALIGN=\"TOP\">");

if (consWiggles && consWiggles->next)
    {
    /* check for alternate conservation wiggles -- create checkboxes */
    puts("<P STYLE=\"margin-top:10;\"><B>Conservation:</B>" );
    boolean first = TRUE;
    for (consWig = consWiggles; consWig != NULL; consWig = consWig->next)
        {
        char *wigVar = wigMafWiggleVar(tdb, consWig);
        cgiMakeCheckBox(wigVar, cartUsualBoolean(cart, wigVar, first));
        first = FALSE;
        subChar(consWig->uiLabel, '_', ' ');
        printf ("%s&nbsp;", consWig->uiLabel);
        }
    }

struct wigMafSpecies *wmSpeciesList = wigMafSpeciesTable(cart, tdb, name, db);
struct wigMafSpecies *wmSpecies;

if (isWigMafProt)
    puts("<B>Multiple alignment amino acid-level:</B><BR>" );
else
    puts("<B>Multiple alignment base-level:</B><BR>" );
safef(option, sizeof option, "%s.%s", name, MAF_DOT_VAR);
cgiMakeCheckBox(option, cartCgiUsualBoolean(cart, option, FALSE));

if (isWigMafProt)
    puts("Display amino acids identical to reference as dots<BR>" );
else
    puts("Display bases identical to reference as dots<BR>" );

safef(option, sizeof option, "%s.%s", name, MAF_CHAIN_VAR);
cgiMakeCheckBox(option, cartCgiUsualBoolean(cart, option, TRUE));

char *irowStr = trackDbSetting(tdb, "irows");
boolean doIrows = (irowStr == NULL) || !sameString(irowStr, "off");
if (isCustomTrack(tdb->tableName) || doIrows)
    puts("Display chains between alignments<BR>");
else
    {
    if (isWigMafProt)
	puts("Display unaligned amino acids with spanning chain as 'o's<BR>");
    else
	puts("Display unaligned bases with spanning chain as 'o's<BR>");
    }
safef(option, sizeof option, "%s.%s", name, "codons");
if (framesTable)
    {
    char *nodeNames[512];
    char buffer[128];

    printf("<BR><B>Codon Translation:</B><BR>");
    printf("Default species to establish reading frame: ");
    nodeNames[0] = db;
    for (wmSpecies = wmSpeciesList, i = 1; wmSpecies != NULL;
			wmSpecies = wmSpecies->next, i++)
	{
	nodeNames[i] = wmSpecies->name;
	}
    cgiMakeDropList(SPECIES_CODON_DEFAULT, nodeNames, i,
	cartUsualString(cart, SPECIES_CODON_DEFAULT, defaultCodonSpecies));
    puts("<br>");
    safef(buffer, sizeof(buffer), "%s.codons",name);
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
    /* Codon highlighting does not apply to wigMafProt type */
    if (!strstr(tdb->type, "wigMafProt"))
	{
    	puts("<P><B>Codon highlighting:</B><BR>" );

#ifdef GENE_FRAMING

    	safef(option, sizeof(option), "%s.%s", name, MAF_FRAME_VAR);
    	char *currentCodonMode = cartCgiUsualString(cart, option, MAF_FRAME_GENE);

    	/* Disable codon highlighting */
   	 cgiMakeRadioButton(option, MAF_FRAME_NONE,
		    sameString(MAF_FRAME_NONE, currentCodonMode));
    	puts("None &nbsp;");

    	/* Use gene pred */
    	cgiMakeRadioButton(option, MAF_FRAME_GENE,
			    sameString(MAF_FRAME_GENE, currentCodonMode));
    	puts("CDS-annotated frame based on");
    	safef(option, sizeof(option), "%s.%s", name, MAF_GENEPRED_VAR);
    	genePredDropDown(cart, makeTrackHash(db, chromosome), NULL, option);

#else
    	snprintf(option, sizeof(option), "%s.%s", name, BASE_COLORS_VAR);
    	puts ("&nbsp; Alternate colors every");
    	cgiMakeIntVar(option, cartCgiUsualInt(cart, option, 0), 1);
    	puts ("bases<BR>");
    	snprintf(option, sizeof(option), "%s.%s", name,
			    BASE_COLORS_OFFSET_VAR);
    	puts ("&nbsp; Offset alternate colors by");
    	cgiMakeIntVar(option, cartCgiUsualInt(cart, option, 0), 1);
    	puts ("bases<BR>");
#endif
	}
    }

treeImage = trackDbSetting(tdb, "treeImage");
if (treeImage)
    printf("</TD><TD VALIGN=\"TOP\"><IMG SRC=\"../images/%s\"></TD></TR></TABLE>", treeImage);
else
    puts("</TD></TR></TABLE>");

if (trackDbSetting(tdb, CONS_WIGGLE) != NULL)
    {
    wigCfgUi(cart,tdb,name,"Conservation graph:",FALSE);
    }
cfgEndBox(boxed);
}

static boolean compositeViewCfgExpandedByDefault(struct trackDb *parentTdb,char *view,char **visibility)
/* returns true if the view cfg is expanded by default.  Optionally allocates string of view setting (eg 'dense') */
{
int cnt,ix;
boolean expanded = FALSE;
if ( visibility != NULL )
    *visibility = cloneString(hStringFromTv(parentTdb->visibility));
char *setting = trackDbSetting(parentTdb, "visibilityViewDefaults");
if(setting == NULL)
    return FALSE;

char *target = cloneString(setting);
char *words[SMALLBUF];
cnt = chopLine(target, words);
for(ix=0;ix<cnt;ix++)
    {
    if(startsWith(view,words[ix]) && words[ix][strlen(view)] == '=')
        {
        if (words[ix][strlen(words[ix]) - 1] == '+')
            {
            expanded = TRUE;
            if ( visibility != NULL )
                words[ix][strlen(words[ix]) - 1] = 0;
            }
        if ( visibility != NULL )
            *visibility = cloneString(words[ix] + strlen(view) + 1);
        break;
        }
    }
freeMem(target);
return expanded;
}

enum trackVisibility visCompositeViewDefault(struct trackDb *parentTdb,char *view)
/* returns the default track visibility of particular view within a composite track */
{
char *visibility = NULL;
(void)compositeViewCfgExpandedByDefault(parentTdb,view,&visibility);
enum trackVisibility vis = hTvFromString(visibility);
freeMem(visibility);
return vis;
}

static boolean hCompositeDisplayViewDropDowns(char *db, struct cart *cart, struct trackDb *parentTdb)
/* UI for composite view drop down selections. */
{
int ix;
struct trackDb *subtrack;
char objName[SMALLBUF];
char javascript[JBUFSIZE];
#define CFG_LINK  "<B><A NAME=\"a_cfg_%s\"></A><A HREF=\"#a_cfg_%s\" onclick=\"return (showConfigControls('%s') == false);\" title=\"Configure View Settings\">%s</A></B>\n"
#define MAKE_CFG_LINK(name,title) printf(CFG_LINK, (name),(name),(name),(title))

members_t *membersOfView = subgroupMembersGet(parentTdb,"view");
if(membersOfView == NULL)
    return FALSE;

char configurable[membersOfView->count];
memset(configurable,cfgNone,sizeof(configurable));
boolean makeCfgRows = FALSE;
struct trackDb **matchedSubtracks = needMem(sizeof(struct trackDb *)*membersOfView->count);
for (ix = 0; ix < membersOfView->count; ix++)
    {
    for (subtrack = parentTdb->subtracks; subtrack != NULL; subtrack = subtrack->next)
        {
        char *stView;
        if(!subgroupFind(subtrack,"view",&stView))
            continue;
        if(differentString(stView,membersOfView->names[ix]))
            continue;
        matchedSubtracks[ix] = subtrack;
        configurable[ix] = (char)cfgTypeFromTdb(subtrack);
        // Need to find maximum wig range
        if(configurable[ix] != cfgNone)
            makeCfgRows = TRUE;
        break;
        }
    }

printf("<EM><B>Select %s:</EM></B><BR>\n", membersOfView->title);
puts("<TABLE><TR align=\"LEFT\">");
for (ix = 0; ix < membersOfView->count; ix++)
    {
    printf("<TD>");
    if(configurable[ix] != cfgNone)
        MAKE_CFG_LINK(membersOfView->names[ix],membersOfView->values[ix]);
    else
        printf("<B>%s</B>\n",membersOfView->values[ix]);
    puts("</TD>");

    safef(objName, sizeof(objName), "%s_dd_%s", parentTdb->tableName,membersOfView->names[ix]);
    enum trackVisibility tv =
        hTvFromString(cartUsualString(cart, objName,hStringFromTv(visCompositeViewDefault(parentTdb,membersOfView->names[ix]))));

    safef(javascript, sizeof(javascript), "onchange=\"matSelectViewForSubTracks(this,'_%s');\"", membersOfView->names[ix]);

    printf("<TD>");
    hTvDropDownWithJavascript(objName, tv, parentTdb->canPack,javascript);
    puts(" &nbsp; &nbsp; &nbsp;</TD>");
    }
puts("</TR>");
if(makeCfgRows)
    {
    puts("</TABLE><TABLE>");
    for (ix = 0; ix < membersOfView->count; ix++)
        {
        if(matchedSubtracks[ix] != NULL)
            {
            printf("<TR id=\"tr_cfg_%s\"",membersOfView->names[ix]);
            if(!compositeViewCfgExpandedByDefault(parentTdb,membersOfView->names[ix],NULL))
                printf(" style=\"display:none\"");
            printf("><TD>&nbsp;&nbsp;&nbsp;&nbsp;</TD><TD>");
            safef(objName, sizeof(objName), "%s.%s", parentTdb->tableName,membersOfView->names[ix]);
            cfgByCfgType(configurable[ix],db,cart,matchedSubtracks[ix],objName,membersOfView->values[ix],TRUE);
            if(configurable[ix] != cfgNone)
                printf("<script type='text/javascript'>compositeCfgRegisterOnchangeAction(\"%s\")</script>",objName);
            }
        }
    }
puts("</TABLE><BR>");
subgroupMembersFree(&membersOfView);
freeMem(matchedSubtracks);
return TRUE;
}

static char *labelWithControlledVocabLink(struct trackDb *parentTdb, char *vocabType, char *label)
/* If the parentTdb has a controlledVocabulary setting and the vocabType is found,
   then label will be wrapped with the link to display it.
   NOTE: returned string is on the stack so use it or loose it. */
{
char *vocab = trackDbSetting(parentTdb, "controlledVocabulary");
if(vocab == NULL)
    return cloneString(label); // No wrapping!

char *words[15];
char *prefix=NULL;
char *suffix=NULL;
int count,ix;
char buffer[128];
buffer[0] = 0;
if((count = chopByWhite(cloneString(vocab), words,15)) <= 1)
    return cloneString(label);
for(ix=1;ix<count;ix++)
    {
    if(sameString(vocabType,words[ix]))
       break;
    else if(countChars(words[ix],'=') == 1)
        {
            strSwapChar(words[ix],'=',0);
            if(sameString(vocabType,words[ix]))
                {
                char * lookForSet = words[ix] + strlen(words[ix]) + 1;
                char * lookFor = NULL;
                boolean found = FALSE;
                while(!found && (lookFor = cloneNextWordByDelimiter(&lookForSet,',')))
                    {
                    if(sameString(label,lookFor))
                        found = TRUE;
                    else if(startsWith(lookFor,label) && label[strlen(lookFor)] == ' ')
                        {
                        suffix = buffer;
                        strcpy(suffix,label+strlen(lookFor));
                        label = lookFor;
                        found = TRUE;
                        }
                    else if(endsWith(label,lookFor) && label[strlen(label) - strlen(lookFor) - 1] == ' ')
                        {
                        prefix = buffer;
                        strcpy(prefix,label);
                        prefix[strlen(label) - strlen(lookFor)] = 0;
                        label = lookFor;
                        found = TRUE;
                        }
                    }
                if(found)
                    break;
                }
        }
    }
if(ix==count)
    {
    freeMem(words[0]);
    return cloneString(label);
    }

#define VOCAB_LINK "%s<A HREF='hgEncodeVocab?ra=/usr/local/apache/cgi-bin/%s&term=\"%s\"' TARGET=_BLANK>%s</A>%s\n"
int sz=strlen(VOCAB_LINK)+strlen(words[0])+strlen(vocabType)+2*strlen(label) + strlen(buffer) + 2;
char *link=needMem(sz);
char *term = strSwapChar(cloneString(label),' ','_');
if(prefix)
    safef(link,sz,VOCAB_LINK,prefix,words[0],term,label,"");
else if(suffix)
    safef(link,sz,VOCAB_LINK,"",words[0],term,label,suffix);
else
    safef(link,sz,VOCAB_LINK,"",words[0],term,label,"");
freeMem(words[0]);
return link;
}

static boolean hCompositeUiByMatrix(char *db, struct cart *cart, struct trackDb *parentTdb, char *formName)
/* UI for composite tracks: matrix of checkboxes. */
{
//int ix;
char objName[SMALLBUF];
char javascript[JBUFSIZE];
char option[SMALLBUF];
boolean alreadySet = TRUE;
struct trackDb *subtrack;

if(!dimensionsExist(parentTdb))
    return FALSE;

hCompositeDisplayViewDropDowns(db, cart,parentTdb);  // If there is a view dimension, it is at top

int ixX,ixY,ixZ;
members_t *dimensionX = subgroupMembersGetByDimension(parentTdb,'X');
members_t *dimensionY = subgroupMembersGetByDimension(parentTdb,'Y');
members_t *dimensionZ = subgroupMembersGetByDimension(parentTdb,'Z');
if(dimensionX == NULL && dimensionY == NULL && dimensionZ == NULL) // Must be an X, Y or Z dimension
    return FALSE;

// use array of char determine all the cells (in X,Y,Z dimensions) that are actually populated
char *value;
int sizeOfX = dimensionX?dimensionX->count:1;
int sizeOfY = dimensionY?dimensionY->count:1;
int sizeOfZ = dimensionZ?dimensionZ->count:1;
char cells[sizeOfX][sizeOfY]; // There needs to be atleast one element in dimension
char cellsZ[sizeOfX];         // The Z dimension is a separate 1D matrix
memset(cells, 0, sizeof(cells));
memset(cellsZ, 0, sizeof(cellsZ));
for (subtrack = parentTdb->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    ixX = (dimensionX ? -1 : 0 );
    ixY = (dimensionY ? -1 : 0 );
    ixZ = (dimensionZ ? -1 : 0 );
    if(dimensionX && subgroupFind(subtrack,dimensionX->tag,&value))
        {
        ixX = stringArrayIx(value,dimensionX->names,dimensionX->count);
        subgroupFree(&value);
        }
    if(dimensionY && subgroupFind(subtrack,dimensionY->tag,&value))
        {
        ixY = stringArrayIx(value,dimensionY->names,dimensionY->count);
        subgroupFree(&value);
        }
    if(dimensionZ && subgroupFind(subtrack,dimensionZ->tag,&value))
        {
        ixZ = stringArrayIx(value,dimensionZ->names,dimensionZ->count);
        subgroupFree(&value);
        }
    if(ixX > -1 && ixY > -1)
        cells[ixX][ixY]++;
    if(ixZ > -1)
        cellsZ[ixZ]++;
    }

    // Regardless of whether there is a dimension X or Y, there will be 'all' buttons
    puts("<B>Select subtracks:</B><BR>");
    printf("<TABLE class='greenBox' bgcolor='%s' borderColor='%s'}>\n",COLOR_BG_DEFAULT,COLOR_BG_DEFAULT);

    printf("<TR ALIGN=CENTER BGCOLOR='%s'>\n",COLOR_BG_ALTDEFAULT);
    if(dimensionX && dimensionY)
        {
        printf("<TH ALIGN=LEFT WIDTH='100'>All&nbsp;&nbsp;");
        PLUS_BUTTON( "name", "plus_all","mat_","_cb");
        MINUS_BUTTON("name","minus_all","mat_","_cb");
        puts("</TH>");
        }
    else if(dimensionX)
        printf("<TH WIDTH=\"100\"><EM><B>%s</EM></B></TH>", dimensionX->title);
    else if(dimensionY)
        printf("<TH ALIGN=RIGHT WIDTH=\"100\"><EM><B>%s</EM></B></TH>", dimensionY->title);

    // If there is an X dimension, then titles go across the top
    if(dimensionX)
        {
        if(dimensionY)
            printf("<TH align='right' WIDTH=\"100\"><EM><B>%s</EM></B>:</TH>", dimensionX->title);
        for (ixX = 0; ixX < dimensionX->count; ixX++)
            printf("<TH WIDTH=\"100\">%s</TH>",labelWithControlledVocabLink(parentTdb,dimensionX->tag,dimensionX->values[ixX]));
        }
    else if(dimensionY)
        {
        printf("<TH ALIGN=CENTER WIDTH=\"100\">");
        PLUS_BUTTON( "name", "plus_all","mat_","_cb");
        MINUS_BUTTON("name","minus_all","mat_","_cb");
        puts("</TH>");
        }
    puts("</TR>\n");

    // If there are both X and Y dimensions, then there is a row of buttons in X
    if(dimensionX && dimensionY)
        {
        printf("<TR ALIGN=CENTER BGCOLOR=\"%s\"><TH ALIGN=RIGHT><EM><B>%s</EM></B></TH><TD>&nbsp;</TD>",COLOR_BG_ALTDEFAULT, dimensionY->title);
        for (ixX = 0; ixX < dimensionX->count; ixX++)    // Special row of +- +- +-
            {
            puts("<TD>");
            safef(option, sizeof(option), "mat_%s_", dimensionX->names[ixX]);
            safef(objName, sizeof(objName), "plus_%s_all", dimensionX->names[ixX]);
            PLUS_BUTTON( "name",objName,option,"_cb");
            safef(objName, sizeof(objName), "minus_%s_all", dimensionX->names[ixX]);
            MINUS_BUTTON("name",objName,option,"_cb");
            puts("</TD>");
            }
        puts("</TR>\n");
        }

    // Now the Y by X matrix
    for (ixY = 0; ixY < sizeOfY; ixY++)
        {
        assert(!dimensionY || ixY < dimensionY->count);
        printf("<TR ALIGN=CENTER BGCOLOR=\"#FFF9D2\">");
        if(dimensionY == NULL) // 'All' buttons go here if no Y dimension
            {
            printf("<TH ALIGN=CENTER WIDTH=\"100\">");
            PLUS_BUTTON( "name", "plus_all","mat_","_cb");
            MINUS_BUTTON("name","minus_all","mat_","_cb");
            puts("</TH>");
            }
        else if(ixY < dimensionY->count)
            printf("<TH ALIGN=RIGHT nowrap>%s</TH>\n",labelWithControlledVocabLink(parentTdb,dimensionY->tag,dimensionY->values[ixY]));
        else
            break;

        if(dimensionX && dimensionY) // Both X and Y, then column of buttons
            {
            puts("<TD>");
            safef(option, sizeof(option), "_%s_cb", dimensionY->names[ixY]);
            safef(objName, sizeof(objName), "plus_all_%s", dimensionY->names[ixY]);
            PLUS_BUTTON( "name",objName,"mat_",option);
            safef(objName, sizeof(objName), "minus_all_%s", dimensionY->names[ixY]);
            MINUS_BUTTON("name",objName,"mat_",option);
            puts("</TD>");
            }
        for (ixX = 0; ixX < sizeOfX; ixX++)
            {
            assert(!dimensionX || ixX < dimensionX->count);
            if(dimensionX && ixX == dimensionX->count)
                break;
            if(cells[ixX][ixY] > 0)
                {
                if(dimensionX && dimensionY)
                    {
                    safef(objName, sizeof(objName), "mat_%s_%s_cb", dimensionX->names[ixX],dimensionY->names[ixY]);
                    safef(javascript, sizeof(javascript), "onclick=\"matSetCheckBoxesThatContain('id',this.checked,true,'cb_','_%s_%s_','_cb');\"",
                          dimensionX->names[ixX],dimensionY->names[ixY]);
                    }
                else
                    {
                    safef(objName, sizeof(objName), "mat_%s_cb", (dimensionX ? dimensionX->names[ixX] : dimensionY->names[ixY]));
                    safef(javascript, sizeof(javascript), "onclick=\"matSetCheckBoxesThatContain('id',this.checked,true,'cb_','_%s_','_cb');\"",
                          (dimensionX ? dimensionX->names[ixX] : dimensionY->names[ixY]));
                    }
                alreadySet = cartUsualBoolean(cart, objName, alreadySet);
                puts("<TD>");
                cgiMakeCheckBoxJS(objName,alreadySet,javascript);
                puts("</TD>");
                }
            else
                puts("<TD>&nbsp;</TD>");
            }
        puts("</TR>\n");
        }
    if(dimensionZ)
        {
        printf("<TR align='center' valign='bottom' BGCOLOR='%s'>",COLOR_BG_ALTDEFAULT);
        printf("<TH class='greenRoof' colspan=50><EM><B>%s</EM></B>:",dimensionZ->title);
        //PLUS_BUTTON( "name","plus_all_dimZ", "mat_","_dimZ_cb");
        //MINUS_BUTTON("name","minus_all_dimZ","mat_","_dimZ_cb");
        for(ixZ=0;ixZ<sizeOfZ;ixZ++)
            if(cellsZ[ixZ]>0)
                {
                printf("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;");
                safef(objName, sizeof(objName), "mat_%s_dimZ_cb",dimensionZ->names[ixZ]);
                safef(javascript, sizeof(javascript), "onclick=\"matSetCheckBoxesThatContain('id',this.checked,true,'cb_','_%s_','_cb');\"",
                      dimensionZ->names[ixZ]);
                alreadySet = cartUsualBoolean(cart, objName, alreadySet);
                cgiMakeCheckBoxJS(objName,alreadySet,javascript);
                printf("%s",labelWithControlledVocabLink(parentTdb,dimensionZ->tag,dimensionZ->values[ixZ]));
                }
        }
    puts("</TD></TR></TABLE>");
    subgroupMembersFree(&dimensionX);
    subgroupMembersFree(&dimensionY);
    subgroupMembersFree(&dimensionZ);
    puts("<BR>\n");
    return TRUE;
}

static boolean hCompositeUiNoMatrix(char *db, struct cart *cart, struct trackDb *parentTdb,
          char *primarySubtrack, char *formName)
/* UI for composite tracks: subtrack selection.  This is the default UI
without matrix controls. */
{
int i, j, k;
char *words[SMALLBUF];
char option[SMALLBUF];
int wordCnt;
char javascript[JBUFSIZE];
char *primaryType = getPrimaryType(primarySubtrack, parentTdb);
char *name, *value;
char buttonVar[32];
char setting[] = "subGroupN";
char *button;
struct trackDb *subtrack;
bool hasSubgroups = (trackDbSetting(parentTdb, "subGroup1") != NULL);

if(dimensionsExist(parentTdb))
    return FALSE;

if(subgroupingExists(parentTdb,"view"))
    {
    hCompositeDisplayViewDropDowns(db, cart,parentTdb);
    if(subgroupCount(parentTdb) <= 1)
        return TRUE;
    }

puts ("<TABLE>");
if (hasSubgroups)
    {
    puts("<TR><B>Select subtracks:</B></TR>");
    puts("<TR><TD><EM><B>&nbsp; &nbsp; All</B></EM>&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; </TD><TD>");
    }
else
    {
    puts("<TR><TD><B>All subtracks:</B></TD><TD>");
    }
safef(buttonVar, sizeof buttonVar, "%s", "button_all");
if (formName)
    {
    cgiMakeHiddenVar(buttonVar, "");
    makeAddClearSubmitTweak(javascript, formName, buttonVar,
                ADD_BUTTON_LABEL);
    cgiMakeOnClickButton(javascript, ADD_BUTTON_LABEL);
    puts("</TD><TD>");
    makeAddClearSubmitTweak(javascript, formName, buttonVar,
                CLEAR_BUTTON_LABEL);
    cgiMakeOnClickButton(javascript, CLEAR_BUTTON_LABEL);
    }
else
    {
    cgiMakeButton(buttonVar, ADD_BUTTON_LABEL);
    puts("</TD><TD>");
    cgiMakeButton(buttonVar, CLEAR_BUTTON_LABEL);
    }
button = cgiOptionalString(buttonVar);
if (isNotEmpty(button))
    {
    for (subtrack = parentTdb->subtracks; subtrack != NULL; subtrack = subtrack->next)
        {
        boolean newVal = FALSE;
        safef(option, sizeof(option), "%s_sel", subtrack->tableName);
        newVal = sameString(button, ADD_BUTTON_LABEL);
        if (primarySubtrack)
            {
            if (sameString(subtrack->tableName, primarySubtrack))
                newVal = TRUE;
            if (sameString(subtrack->type, primaryType))
                cartSetBoolean(cart, option, newVal);
            }
        else
            cartSetBoolean(cart, option, newVal);
        }
    }
puts("</TD></TR>");
puts ("</TABLE>");
/* generate set & clear buttons for subgroups */
for (i = 0; i < MAX_SUBGROUP; i++)
    {
    char *subGroup;
    safef(setting, sizeof setting, "subGroup%d", i+1);
    if (trackDbSetting(parentTdb, setting) == NULL)
        break;
    wordCnt = chopLine(cloneString(trackDbSetting(parentTdb, setting)), words);
    if (wordCnt < 2)
        continue;
    subGroup = cloneString(words[0]);
    puts ("<TABLE>");
    printf("<TR><TD><EM><B>&nbsp; &nbsp; %s</EM></B></TD></TR>", words[1]);
    for (j = 2; j < wordCnt; j++)
        {
        if (!parseAssignment(words[j], &name, &value))
            continue;
        printf("<TR><TD>&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; %s</TD><TD>",
           value);
        safef(buttonVar, sizeof buttonVar, "%s_%s", subGroup, name);
        if (formName)
            {
            cgiMakeHiddenVar(buttonVar, "");
            makeAddClearSubmitTweak(javascript, formName, buttonVar,
                        ADD_BUTTON_LABEL);
            cgiMakeOnClickButton(javascript, ADD_BUTTON_LABEL);
            puts("</TD><TD>");
            makeAddClearSubmitTweak(javascript, formName, buttonVar,
                        CLEAR_BUTTON_LABEL);
            cgiMakeOnClickButton(javascript, CLEAR_BUTTON_LABEL);
            }
        else
            {
            cgiMakeButton(buttonVar, ADD_BUTTON_LABEL);
            puts("</TD><TD>");
            cgiMakeButton(buttonVar, CLEAR_BUTTON_LABEL);
            }
        puts("</TD></TR>");
        button = cgiOptionalString(buttonVar);
        if (isEmpty(button))
            continue;
        for (subtrack = parentTdb->subtracks; subtrack != NULL;
                subtrack = subtrack->next)
            {
            char *p;
            int n;
            if ((p = trackDbSetting(subtrack, "subGroups")) == NULL)
                continue;
            n = chopLine(cloneString(p), words);
            for (k = 0; k < n; k++)
                {
                char *subName, *subValue;
                if (!parseAssignment(words[k], &subName, &subValue))
                    continue;
                if (sameString(subName, subGroup) && sameString(subValue, name))
                    {
                    boolean newVal = FALSE;
                            safef(option, sizeof(option),
                                    "%s_sel", subtrack->tableName);
                    newVal = sameString(button, ADD_BUTTON_LABEL);
                    if (primarySubtrack)
                        {
                        if (sameString(subtrack->tableName, primarySubtrack))
                            newVal = TRUE;
                        if (sameString(subtrack->type, primaryType))
                            cartSetBoolean(cart, option, newVal);
                        }
                    else
                        cartSetBoolean(cart, option, newVal);
                    }
                }
            }
        }
    puts ("</TABLE>");
    }
    return TRUE;
}
static void commonCssStyles()
/* Defines a few common styles to use through CSS */
{
    printf("<style type='text/css'>");
    //printf(".tDnD_whileDrag {background-color:%s;}",COLOR_BG_GHOST);
    printf(".trDrag {background-color:%s;} .pale {background-color:%s;}",COLOR_BG_GHOST,COLOR_BG_PALE);
    printf(".greenRoof {border-top: 3px groove %s;}",COLOR_DARKGREEN);
    printf(".greenBox {border: 5px outset %s;}",COLOR_DARKGREEN);
    printf(".blueBox {border: 4px inset %s;}",COLOR_DARKBLUE);
    puts("</style>");
}

void hCompositeUi(char *db, struct cart *cart, struct trackDb *tdb,
		  char *primarySubtrack, char *fakeSubmit, char *formName)
/* UI for composite tracks: subtrack selection.  If primarySubtrack is
 * non-NULL, don't allow it to be cleared and only offer subtracks
 * that have the same type.  If fakeSubmit is non-NULL, add a hidden
 * var with that name so it looks like it was pressed. */
{
bool hasSubgroups = (trackDbSetting(tdb, "subGroup1") != NULL);
boolean displayAll =
    sameString(cartUsualString(cart, "displaySubtracks", "all"), "all");
boolean isMatrix = dimensionsExist(tdb);

jsIncludeFile("utils.js",NULL);
if(sameOk("subTracks",trackDbSetting(tdb, "dragAndDrop")))
    {
    jsIncludeFile("jquery.js", NULL);
    jsIncludeFile("jquery.tablednd.js", NULL);
    }
    commonCssStyles();
jsIncludeFile("hui.js",NULL);

puts("<P>");
if (slCount(tdb->subtracks) < MANY_SUBTRACKS && !hasSubgroups)
    {
    compositeUiAllSubtracks(db, cart, tdb, primarySubtrack);
    return;
    }
if (fakeSubmit)
    cgiMakeHiddenVar(fakeSubmit, "submit");

if (!hasSubgroups || !isMatrix || primarySubtrack)
    hCompositeUiNoMatrix(db, cart,tdb,primarySubtrack,formName);
else
    hCompositeUiByMatrix(db, cart,tdb,formName);

cartSaveSession(cart);
cgiContinueHiddenVar("g");
if (displayAll)
    compositeUiAllSubtracks(db, cart, tdb, primarySubtrack);
else
    compositeUiSelectedSubtracks(db, cart, tdb, primarySubtrack);

// Downloads directory if this is ENCODE
if(trackDbSetting(tdb, "wgEncode") != NULL)
    {
#define DOWNLOADS_LINK "<A HREF=\"../goldenPath/%s/%s/\" TARGET=_BLANK>Downloads</A>\n"
    printf(DOWNLOADS_LINK,(trackDbSetting(tdb, "origAssembly") != NULL ?
                           trackDbSetting(tdb, "origAssembly"):"hg18"),tdb->tableName);
    }
}

boolean superTrackDropDown(struct cart *cart, struct trackDb *tdb,
                                int visibleChild)
/* Displays hide/show dropdown for supertrack.
 * Set visibleChild to indicate whether 'show' should be grayed
 * out to indicate that no supertrack members are visible:
 *    0 to gray out (no visible children)
 *    1 don't gray out (there are visible children)
 *   -1 don't know (this function should determine)
 * If -1,i the subtracks field must be populated with the child trackDbs.
 * Returns false if not a supertrack */
{
if (!tdbIsSuperTrack(tdb))
    return FALSE;

/* determine if supertrack is show/hide */
boolean show = FALSE;
char *setting =
        cartUsualString(cart, tdb->tableName, tdb->isShow ? "show" : "hide");
if (sameString("show", setting))
    show = TRUE;

/* Determine if any tracks in supertrack are visible; if not,
 * the 'show' is grayed out */
if (show && (visibleChild == -1))
    {
    visibleChild = 0;
    struct trackDb *cTdb;
    for (cTdb = tdb->subtracks; cTdb != NULL; cTdb = tdb->next)
        {
        cTdb->visibility =
                hTvFromString(cartUsualString(cart, cTdb->tableName,
                                      hStringFromTv(cTdb->visibility)));
        if (cTdb->visibility != tvHide)
            visibleChild = 1;
        }
    }
hideShowDropDown(tdb->tableName, show, (show && visibleChild) ?
                            "normalText" : "hiddenText");
return TRUE;
}

int tvConvertToNumericOrder(enum trackVisibility v)
{
return ((v) == tvFull   ? 4 : \
        (v) == tvPack   ? 3 : \
        (v) == tvSquish ? 2 : \
        (v) == tvDense  ? 1 : 0);
}
int tvCompare(enum trackVisibility a, enum trackVisibility b)
/* enum trackVis isn't in numeric order by visibility, so compare
 * symbolically: */
{
return (tvConvertToNumericOrder(b) - tvConvertToNumericOrder(a));
}

enum trackVisibility tvMin(enum trackVisibility a, enum trackVisibility b)
/* Return the less visible of a and b. */
{
if (tvCompare(a, b) >= 0)
    return a;
else
    return b;
}

char *compositeViewControlNameFromTdb(struct trackDb *tdb)
/* Returns a string with the composite view control name if one exists */
{
char *stView   = NULL;
char *name     = NULL;
char *rootName = NULL;
if(tdbIsCompositeChild(tdb) == TRUE && subgroupFind(tdb,"view",&stView) && trackDbSetting(tdb, "subTrack") != NULL)
    {
    if(trackDbSettingOn(tdb, "configurable"))
        rootName = tdb->tableName;
    else
        rootName = firstWordInLine(cloneString(trackDbSetting(tdb, "subTrack")));
    }
if(rootName != NULL)
    {
    int len = strlen(rootName) + strlen(stView) + 3;
    name = needMem(len);
    safef(name,len,"%s.%s",rootName,stView);
    }
else
    name = cloneString(tdb->tableName);
subgroupFree(&stView);
return name;
}
void compositeViewControlNameFree(char **name)
/* frees a string allocated by compositeViewControlNameFromTdb */
{
if(name && *name)
    freez(name);
}
