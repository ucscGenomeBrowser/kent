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
#include "chainDb.h"
#include "netCart.h"
#include "obscure.h"
#include "wiggle.h"
#include "phyloTree.h"
#include "hgMaf.h"
#include "udc.h"
#include "customTrack.h"
#include "encode/encodePeak.h"

static char const rcsid[] = "$Id: hui.c,v 1.251.2.1 2009/12/11 01:57:57 kent Exp $";

#define SMALLBUF 128
#define MAX_SUBGROUP 9
#define ADD_BUTTON_LABEL        "add"
#define CLEAR_BUTTON_LABEL      "clear"
#define JBUFSIZE 2048

//#define PM_BUTTON "<A NAME=\"%s\"></A><A HREF=\"#%s\"><IMG height=18 width=18 onclick=\"return (setCheckBoxesThatContain('%s',%s,true,'%s','','%s') == false);\" id=\"btn_%s\" src=\"../images/%s\" alt=\"%s\"></A>\n"
//#define DEF_BUTTON "<A NAME=\"%s\"></A><A HREF=\"#%s\"><IMG onclick=\"setCheckBoxesThatContain('%s',true,false,'%s','','%s'); return (setCheckBoxesThatContain('%s',false,false,'%s','_defOff','%s') == false);\" id=\"btn_%s\" src=\"../images/%s\" alt=\"%s\"></A>\n"
//#define DEFAULT_BUTTON(nameOrId,anc,beg,contains) printf(DEF_BUTTON,(anc),(anc),(nameOrId),        (beg),(contains),(nameOrId),(beg),(contains),(anc),"defaults_sm.png","default")
//#define    PLUS_BUTTON(nameOrId,anc,beg,contains) printf(PM_BUTTON, (anc),(anc),(nameOrId),"true", (beg),(contains),(anc),"add_sm.gif",   "+")
//#define   MINUS_BUTTON(nameOrId,anc,beg,contains) printf(PM_BUTTON, (anc),(anc),(nameOrId),"false",(beg),(contains),(anc),"remove_sm.gif","-")
#define PM_BUTTON  "<IMG height=18 width=18 onclick=\"setCheckBoxesThatContain('%s',%s,true,'%s','','%s');\" id=\"btn_%s\" src=\"../images/%s\" alt=\"%s\">\n"
#define DEF_BUTTON "<IMG onclick=\"setCheckBoxesThatContain('%s',true,false,'%s','','%s'); setCheckBoxesThatContain('%s',false,false,'%s','_defOff','%s');\" id=\"btn_%s\" src=\"../images/%s\" alt=\"%s\">\n"
#define DEFAULT_BUTTON(nameOrId,anc,beg,contains) printf(DEF_BUTTON,(nameOrId),        (beg),(contains),(nameOrId),(beg),(contains),(anc),"defaults_sm.png","default")
#define    PLUS_BUTTON(nameOrId,anc,beg,contains) printf(PM_BUTTON, (nameOrId),"true", (beg),(contains),(anc),"add_sm.gif",   "+")
#define   MINUS_BUTTON(nameOrId,anc,beg,contains) printf(PM_BUTTON, (nameOrId),"false",(beg),(contains),(anc),"remove_sm.gif","-")

#define ENCODE_DCC_DOWNLOADS "encodeDCC"



static boolean makeNamedDownloadsLink(struct trackDb *tdb,char *name)
// Make a downloads link (if appropriate and then returns TRUE)
{
// Downloads directory if this is ENCODE
if(trackDbSetting(tdb, "wgEncode") != NULL)
    {
    printf("<A HREF=\"http://%s/goldenPath/%s/%s/%s/\" title='Open dowloads directory in a new window' TARGET=ucscDownloads>%s</A>",
            hDownloadsServer(),
            trackDbSettingOrDefault(tdb, "origAssembly","hg18"),
            ENCODE_DCC_DOWNLOADS,
            tdb->tableName,name);
    return TRUE;
    }
return FALSE;
}

boolean makeDownloadsLink(struct trackDb *tdb)
// Make a downloads link (if appropriate and then returns TRUE)
{
return makeNamedDownloadsLink(tdb,"Downloads");
}

boolean makeSchemaLink(char *db,struct trackDb *tdb,char *label)
// Make a table schema link (if appropriate and then returns TRUE)
{
#define SCHEMA_LINKED "<A HREF=\"../cgi-bin/hgTables?db=%s&hgta_group=%s&hgta_track=%s&hgta_table=%s&hgta_doSchema=describe+table+schema\" TARGET=ucscSchema%s>%s</A>"
if (hTableOrSplitExists(db, tdb->tableName))
    {
	char *tableName  = tdb->tableName;
	if (sameString(tableName, "mrna"))
	    tableName = "all_mrna";
    char *hint = " title='Open table schema in new window'";
    if( label == NULL)
        label = " View table schema";

    if(tdbIsCompositeChild(tdb))
        printf(SCHEMA_LINKED, db, tdb->parent->grp, tdb->parent->tableName,tableName,hint,label);
    else
        printf(SCHEMA_LINKED, db, tdb->grp, tdb->tableName,tableName,hint,label);

    return TRUE;
    }
return FALSE;
}

boolean metadataToggle(struct trackDb *tdb,char *title,boolean embeddedInText,boolean showLongLabel)
/* If metadata exists, create a link that will allow toggling it's display */
{
metadata_t *metadata = metadataSettingGet(tdb);
if(metadata != NULL)
    {
    printf("%s<A HREF='#a_meta_%s' onclick='return metadataShowHide(\"%s\");' title='Show metadata details...'>%s</A>",
           (embeddedInText?"&nbsp;":"<P>"),tdb->tableName,tdb->tableName, title);
    printf("<DIV id='div_%s_meta' style='display:none;'><!--<table>",tdb->tableName);
    if(showLongLabel)
        printf("<tr onmouseover=\"this.style.cursor='text';\"><td colspan=2>%s</td></tr>",tdb->longLabel);
    printf("<tr onmouseover=\"this.style.cursor='text';\"><td align=right><i>shortLabel:</i></td><td nowrap>%s</td></tr>",tdb->shortLabel);
    int ix = (sameString(metadata->values[0],"wgEncode")?1:0); // first should be project.
    for(;ix<metadata->count;ix++)
        {
        if(sameString(metadata->tags[ix],"fileName"))
            {
            printf("<tr onmouseover=\"this.style.cursor='text';\"><td align=right><i>%s:</i></td><td nowrap>",metadata->tags[ix]);
            makeNamedDownloadsLink(tdb->parent != NULL? tdb->parent :tdb ,metadata->values[ix]);
            printf("</td></tr>");
            }
        else
            if(!sameString(metadata->tags[ix],"subId")
                && !sameString(metadata->tags[ix],"composite"))
            printf("<tr onmouseover=\"this.style.cursor='text';\"><td align=right><i>%s:</i></td><td nowrap>%s</td></tr>",metadata->tags[ix],metadata->values[ix]);
        }
    printf("</table>--></div>");
    metadataFree(&metadata);
    return TRUE;
    }
return FALSE;
}

void extraUiLinks(char *db,struct trackDb *tdb)
/* Show downlaods, schema and metadata links where appropriate */
{
boolean schemaLink = (isCustomTrack(tdb->tableName) == FALSE)
                  && (hTableOrSplitExists(db, tdb->tableName));
boolean metadataLink = (!tdbIsComposite(tdb))
                  && trackDbSetting(tdb, "metadata");
boolean downloadLink = (trackDbSetting(tdb, "wgEncode") != NULL);
boolean moreThanOne = (schemaLink && metadataLink)
                   || (schemaLink && downloadLink)
                   || (downloadLink && metadataLink);

printf("<P>");
if(moreThanOne)
    printf("<table><tr><td nowrap>View table: ");

if(schemaLink)
    {
    makeSchemaLink(db,tdb,(moreThanOne ? "schema":"View table schema"));
    if(downloadLink || metadataLink)
        printf(", ");
    }
if(downloadLink)
    {
    struct trackDb *trueTdb = tdbIsCompositeChild(tdb)? tdb->parent: tdb;
    makeNamedDownloadsLink(trueTdb,(moreThanOne ? "downloads":"Downloads"));
    if(metadataLink)
        printf(",");
    }
if (metadataLink)
    metadataToggle(tdb,"metadata", TRUE, TRUE);

if(moreThanOne)
    printf("</td></tr></table>");
puts("</P>");
}


char *hUserCookie()
/* Return our cookie name. */
{
return cfgOptionDefault("central.cookie", "hguid");
}

char *hDownloadsServer()
/* get the downloads server from hg.conf or the default */
{
return cfgOptionDefault("downloads.server", "hgdownload.cse.ucsc.edu");
}

void setUdcCacheDir()
/* set the path to the udc cache dir */
{
udcSetDefaultDir(cfgOptionDefault("udc.cacheDir", udcDefaultDir()));
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
	sameString(setting, "hgPcrResult") || sameString(setting, "nameIsSequence") ||
	sameString(setting, "seq1Seq2") || sameString(setting, "lfExtra"))
	gotIt = TRUE;
    else if (differentString(setting, "none"))
	errAbort("trackDb for %s, setting %s: unrecognized value \"%s\".  "
		 "must be one of {none,genbank,seq,ss,extFile,nameIsSequence,seq1Seq2,"
		 "hgPcrResult,lfExtra}.",
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
assert(cart);
assert(tdb);

/* trackDb can override default of OFF; cart can override trackDb. */
stringVal = trackDbSettingOrDefault(tdb, BASE_COLOR_DEFAULT,
				    BASE_COLOR_DRAW_OFF);
stringVal = cartUsualStringClosestToHome(cart, tdb, FALSE, BASE_COLOR_VAR_SUFFIX,stringVal);

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

/****** Options for the net track level display options *******/
static char *netLevelOptions[] = {
    NET_LEVEL_0,
    NET_LEVEL_1,
    NET_LEVEL_2,
    NET_LEVEL_3,
    NET_LEVEL_4,
    NET_LEVEL_5,
    NET_LEVEL_6
    };

enum netLevelEnum netLevelStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, netLevelOptions);
if (x < 0)
   errAbort("hui::netLevelStringToEnum() - Unknown option %s", string);
return x;
}

char *netLevelEnumToString(enum netLevelEnum x)
/* Convert from enum to string representation. */
{
return netLevelOptions[x];
}

void netLevelDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, netLevelOptions, ArraySize(netLevelOptions), curVal);
}

/****** Options for the net track color options *******/
static char *netColorOptions[] = {
    CHROM_COLORS,
    GRAY_SCALE
    };

enum netColorEnum netColorStringToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, netColorOptions);
if (x < 0)
   errAbort("hui::netColorStringToEnum() - Unknown option %s", string);
return x;
}

char *netColorEnumToString(enum netColorEnum x)
/* Convert from enum to string representation. */
{
return netColorOptions[x];
}

void netColorDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, netColorOptions, ArraySize(netColorOptions), curVal);
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
    "mean+whiskers",
    "maximum",
    "mean",
    "minimum",
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
    "bar",
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

static char *wiggleTransformFuncOptions[] = {
    "NONE",
    "LOG"
    };

enum wiggleTransformFuncEnum wiggleTransformFuncToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, wiggleTransformFuncOptions);
if (x < 0)
   errAbort("hui::wiggleTransformFuncToEnum() - Unknown option %s", string);
return x;
}

void wiggleTransformFuncDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, wiggleTransformFuncOptions,
    ArraySize(wiggleTransformFuncOptions), curVal);
}

static char *wiggleAlwaysZeroOptions[] = {
    "OFF",
    "ON"
    };

enum wiggleAlwaysZeroEnum wiggleAlwaysZeroToEnum(char *string)
/* Convert from string to enum representation. */
{
int x = stringIx(string, wiggleAlwaysZeroOptions);
if (x < 0)
   errAbort("hui::wiggleAlwaysZeroToEnum() - Unknown option %s", string);
return x;
}

void wiggleAlwaysZeroDropDown(char *var, char *curVal)
/* Make drop down of options. */
{
cgiMakeDropList(var, wiggleAlwaysZeroOptions,
    ArraySize(wiggleAlwaysZeroOptions), curVal);
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

boolean hSameTrackDbType(char *type1, char *type2)
/* Compare type strings: require same string unless both are wig tracks. */
{
return (sameString(type1, type2) ||
	(startsWith("wig ", type1) && startsWith("wig ", type2)));
}

static char *labelRoot(char *label, char** suffix)
/* Parses a label which may be split with a &nbsp; into root and suffix
   Always free labelRoot.  suffix, which may be null does not need to be freed. */
{
char *root = cloneString(label);
char *extra=strstrNoCase(root,"&nbsp;"); // &nbsp; mean don't include the reset as part of the link
if ((long)(extra)==-1)
    extra=NULL;
if (extra!=NULL)
    {
    *extra='\0';
    if(suffix != NULL)
        {
        extra+=5;
        *extra=' '; // Converts the &nbsp; to ' ' and include the ' '
        *suffix = extra;
        }
    }
return root;
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
struct trackDb *ancestor;
for (ancestor = parentTdb; ancestor != NULL; ancestor = ancestor->parent)
    {
    int ix;
    char *setting = NULL;
    if(startsWith("subGroup",groupNameOrTag))
	{
	setting = trackDbSetting(ancestor, groupNameOrTag);
	if(setting != NULL)
	    return setting;
	}
    for(ix=1;ix<=SUBGROUP_MAX;ix++) // How many do we support?
	{
	char subGrp[16];
	safef(subGrp, ArraySize(subGrp), "subGroup%d",ix);
	setting = trackDbSetting(ancestor, subGrp);
	if(setting != NULL)  // Doesn't require consecutive subgroups
	    {
	    if(startsWithWord(groupNameOrTag,setting))
		return setting;
	    }
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
        if(lastChar(dimensions->names[ix]) != dimension)
            continue;
        if((strlen(dimensions->names[ix]) == 10 && startsWith("dimension",dimensions->names[ix]))
        || (strlen(dimensions->names[ix]) == 4  && startsWith("dim",dimensions->names[ix])))
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

int multViewCount(struct trackDb *parentTdb)
/* returns the number of multiView views declared */
{
char *setting = subgroupSettingByTagOrName(parentTdb,"view");
if(setting == NULL)
    return 0;

setting = cloneString(setting);
int cnt;
char *words[32];
cnt = chopLine(setting, words);
freeMem(setting);
return (cnt - 1);
}

#ifdef ADD_MULT_SELECT_DIMENSIONS
// This is the beginning of work on allowing subtrack selection by multi-select drop downs
typedef struct _selectables {
    int count;
    members_t **subgroups;
    boolean**multiple;
} selectables_t;

boolean selectablesExist(struct trackDb *parentTdb)
/* Does this parent track contain selectables option? */
{
    return (trackDbSetting(parentTdb, "selectableBy") != NULL);
}
#endif//def ADD_MULT_SELECT_DIMENSIONS

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

sortOrder_t *sortOrderGet(struct cart *cart,struct trackDb *parentTdb)
/* Parses any list sort order instructions for parent of subtracks (from cart or trackDb)
   Some trickiness here.  sortOrder->sortOrder is from cart (changed by user action), as is sortOrder->order,
   But columns are in original tdb order (unchanging)!  However, if cart is null, all is from trackDb.ra */
{
int ix;
char *setting = trackDbSetting(parentTdb, "sortOrder");
if(setting == NULL) // Must be in trackDb or not a sortable table
    return NULL;

sortOrder_t *sortOrder = needMem(sizeof(sortOrder_t));
sortOrder->htmlId = needMem(strlen(parentTdb->tableName)+15);
safef(sortOrder->htmlId, (strlen(parentTdb->tableName)+15), "%s.sortOrder", parentTdb->tableName);
char *cartSetting = NULL;
if(cart != NULL)
    cartSetting = cartCgiUsualString(cart, sortOrder->htmlId, setting);
if(cart != NULL && strlen(cartSetting) == strlen(setting))
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
void sortOrderFree(sortOrder_t **sortOrder)
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


sortableTdbItem *sortableTdbItemCreate(struct trackDb *tdbChild,sortOrder_t *sortOrder)
// creates a sortable tdb item struct, given a child tdb and its parent's sort table
// Errors in interpreting a passed in sortOrder will return NULL
{
sortableTdbItem *item = NULL;
if(tdbChild == NULL || tdbChild->shortLabel == NULL)
    return NULL;
AllocVar(item);
item->tdb = tdbChild;
if(sortOrder != NULL)   // Add some sort buttons
    {
    int sIx=0;
    for(sIx=sortOrder->count - 1;sIx>=0;sIx--) // walk backwards to ensure sort order in columns
        {
        sortColumn *column = NULL;
        AllocVar(column);
        column->fwd = sortOrder->forward[sIx];
        if(!subgroupFind(item->tdb,sortOrder->column[sIx],&(column->value)))
            {
            char *setting = trackDbSetting(item->tdb,sortOrder->column[sIx]);
            if(setting != NULL)
                column->value = cloneString(setting);
            // No subgroup, assume there is a matching setting (eg longLabel)
            }
        if(column->value != NULL)
            slAddHead(&(item->columns), column);
        else
            {
            freez(&column);
            if(item->columns != NULL)
                slFreeList(&(item->columns));
            freeMem(item);
            return NULL; // sortOrder setting doesn't match items to be sorted.
            }
        }
    }
return item;
}

static int sortableTdbItemsCmp(const void *va, const void *vb)
// Compare two sortable tdb items based upon sort columns.
{
const sortableTdbItem *a = *((sortableTdbItem **)va);
const sortableTdbItem *b = *((sortableTdbItem **)vb);
sortColumn *colA = a->columns;
sortColumn *colB = b->columns;
int compared = 0;
for (;compared==0 && colA!=NULL && colB!=NULL;colA=colA->next,colB=colB->next)
    {
    if(colA->value != NULL && colB->value != NULL)
        compared = strcmp(colA->value, colB->value) * (colA->fwd? 1: -1);
    }
if(compared != 0)
    return compared;

return strcmp(a->tdb->shortLabel, b->tdb->shortLabel); // Last chance
}

void sortTdbItemsAndUpdatePriorities(sortableTdbItem **items)
// sort items in list and then update priorities of item tdbs
{
if(items != NULL && *items != NULL)
    {
    slSort(items, sortableTdbItemsCmp);
    int priority=10001; // Setting priorities high allows new subtracks without cart entries to fall after existing subtracks
    sortableTdbItem *item;
    for (item = *items; item != NULL; item = item->next)
        item->tdb->priority = (float)priority++;
    }
}

void sortableTdbItemsFree(sortableTdbItem **items)
// Frees all memory associated with a list of sortable tdb items
{
if(items != NULL && *items != NULL)
    {
    sortableTdbItem *item;
    for (item = *items; item != NULL; item = item->next)
        {
        sortColumn *column;
        for (column = item->columns; column != NULL; column = column->next)
            freeMem(column->value);
        slFreeList(&(item->columns));
        }
    slFreeList(items);
    }
}

static boolean colonPairToStrings(char * colonPair,char **first,char **second)
{ // Will set *first and *second to NULL.  Must free any string returned!  No colon: value goes to *first
if(first)
    *first =NULL; // default to NULL !
if(second)
    *second=NULL;
if(colonPair != NULL)
    {
    if(strchr(colonPair,':'))
        {
        if(second)
            *second = cloneString(strchr(colonPair,':') + 1);
        if(first)
            *first = strSwapChar(cloneString(colonPair),':',0);
        }
    else if(first)
        *first = cloneString(colonPair);
    return (*first != NULL || *second != NULL);
    }
return FALSE;
}
static boolean colonPairToInts(char * colonPair,int *first,int *second)
{ // Non-destructive. Only sets values if found. No colon: value goes to *first
char *a=NULL;
char *b=NULL;
if(colonPairToStrings(colonPair,&a,&b))
    {
    if(a!=NULL)
        {
        if(first)
            *first = atoi(a);
        freeMem(a);
        }
    if(b!=NULL)
        {
        if(second)
            *second = atoi(b);
        freeMem(b);
        }
    return TRUE;
    }
return FALSE;
}
static boolean colonPairToDoubles(char * colonPair,double *first,double *second)
{ // Non-destructive. Only sets values if found. No colon: value goes to *first
char *a=NULL;
char *b=NULL;
if(colonPairToStrings(colonPair,&a,&b))
    {
    if(a!=NULL)
        {
        if(first)
            *first = strtod(a,NULL);
        freeMem(a);
        }
    if(b!=NULL)
        {
        if(second)
            *second = strtod(b,NULL);
        freeMem(b);
        }
    return TRUE;
    }
return FALSE;
}

filterBy_t *filterBySetGet(struct trackDb *tdb, struct cart *cart, char *name)
/* Gets one or more "filterBy" settings (ClosestToHome).  returns NULL if not found */
{
filterBy_t *filterBySet = NULL;

char *setting = trackDbSettingClosestToHome(tdb, FILTER_BY);
if(setting == NULL)
    return NULL;
if( name == NULL )
    name = tdb->tableName;

setting = cloneString(setting);
char *filters[10];
int filterCount = chopLine(setting, filters);
int ix;
for(ix=0;ix<filterCount;ix++)
    {
    char *filter = cloneString(filters[ix]);
    filterBy_t *filterBy;
    AllocVar(filterBy);
    strSwapChar(filter,':',0);
    filterBy->column = filter;
    filter += strlen(filter) + 1;
    strSwapChar(filter,'=',0);
    filterBy->title = strSwapChar(filter,'_',' '); // Title does not have underscores
    filter += strlen(filter) + 1;
    if(filter[0] == '+') // values are indexes to the string titles
        {
        filter += 1;
        filterBy->useIndex = TRUE;
        }
    filterBy->valueAndLabel = (strchr(filter,'|') != NULL);
    filterBy->slValues = slNameListFromComma(filter);
    if(filterBy->valueAndLabel)
        {
        struct slName *val = filterBy->slValues;
        for(;val!=NULL;val=val->next)
            {
            char * lab =strchr(val->name,'|');
            if(lab == NULL)
                {
                warn("Using filterBy but only some values contain labels in form of value|label.");
                filterBy->valueAndLabel = FALSE;
                break;
                }
            *lab++ = 0;
            strSwapChar(lab,'_',' '); // Title does not have underscores
            }
        }
    slAddTail(&filterBySet,filterBy); // Keep them in order (only a few)

    if(cart != NULL)
        {
        char suffix[64];
        safef(suffix, sizeof(suffix), "filterBy.%s", filterBy->column);
        boolean compositeLevel = isNameAtCompositeLevel(tdb,name);
        if(cartLookUpVariableClosestToHome(cart,tdb,compositeLevel,suffix,&(filterBy->htmlName)))
            filterBy->slChoices = cartOptionalSlNameList(cart,filterBy->htmlName);
        }
    if(filterBy->htmlName == NULL)
        {
        int len = strlen(name) + strlen(filterBy->column) + 15;
        filterBy->htmlName = needMem(len);
        safef(filterBy->htmlName, len, "%s.filterBy.%s", name,filterBy->column);
        }
    }
freeMem(setting);

return filterBySet;
}

void filterBySetFree(filterBy_t **filterBySet)
/* Free a set of filterBy structs */
{
if(filterBySet != NULL)
    {
    while(*filterBySet != NULL)
        {
        filterBy_t *filterBy = slPopHead(filterBySet);
        if(filterBy->slValues != NULL)
            slNameFreeList(filterBy->slValues);
        if(filterBy->slChoices != NULL)
            slNameFreeList(filterBy->slChoices);
        if(filterBy->htmlName != NULL)
            freeMem(filterBy->htmlName);
        freeMem(filterBy->column);
        freeMem(filterBy);
        }
    }
}

static char *filterByClause(filterBy_t *filterBy)
/* returns the "column in (...)" clause for a single filterBy struct */
{
int count = slCount(filterBy->slChoices);
if(count == 0)
    return NULL;
if(slNameInList(filterBy->slChoices,"All"))
    return NULL;

struct dyString *dyClause = newDyString(256);
dyStringAppend(dyClause, filterBy->column);
if(count == 1)
    dyStringPrintf(dyClause, " = ");
else
    dyStringPrintf(dyClause, " in (");

struct slName *slChoice = NULL;
boolean notFirst = FALSE;
for(slChoice = filterBy->slChoices;slChoice != NULL;slChoice=slChoice->next)
    {
    if(notFirst)
        dyStringPrintf(dyClause, ",");
    notFirst = TRUE;
    if(filterBy->useIndex)
        dyStringAppend(dyClause, slChoice->name);
    else
        dyStringPrintf(dyClause, "\"%s\"",slChoice->name);
    }
if(dyStringLen(dyClause) == 0)
    {
    dyStringFree(&dyClause);
    return NULL;
    }
if(count > 1)
    dyStringPrintf(dyClause, ")");

return dyStringCannibalize(&dyClause);
}

struct dyString *dyAddFilterByClause(struct cart *cart, struct trackDb *tdb,
       struct dyString *extraWhere,char *column, boolean *and)
/* creates the where clause condition to support a filterBy setting.
   Format: filterBy column:Title=value,value [column:Title=value|label,value|label,value|label])
   filterBy filters are multiselect's so could have multiple values selected.
   thus returns the "column1 in (...) and column2 in (...)" clause.
   if 'column' is provided, and there are multiple filterBy columns, only the named column's clause is returned.
   The 'and' param and dyString in/out allows stringing multiple where clauses together
*/
{
filterBy_t *filterBySet = filterBySetGet(tdb, cart,NULL);
if(filterBySet== NULL)
    return extraWhere;

filterBy_t *filterBy = filterBySet;
for(;filterBy != NULL; filterBy = filterBy->next)
    {
    if(column != NULL && differentString(column,filterBy->column))
        continue;

    char *clause = filterByClause(filterBy);
    if(clause != NULL)
        {
        if(*and)
            dyStringPrintf(extraWhere, " AND ");
        dyStringAppend(extraWhere, clause);
        freeMem(clause);
        *and = TRUE;
        }
    }
filterBySetFree(&filterBySet);
return extraWhere;
}

char *filterBySetClause(filterBy_t *filterBySet)
/* returns the "column1 in (...) and column2 in (...)" clause for a set of filterBy structs */
{
struct dyString *dyClause = newDyString(256);
boolean notFirst = FALSE;
filterBy_t *filterBy = NULL;

for(filterBy = filterBySet;filterBy != NULL; filterBy = filterBy->next)
    {
    char *clause = filterByClause(filterBy);
    if(clause != NULL)
        {
        if(notFirst)
            dyStringPrintf(dyClause, " AND ");
        dyStringAppend(dyClause, clause);
        freeMem(clause);
        notFirst = TRUE;
        }
    }
if(dyStringLen(dyClause) == 0)
    {
    dyStringFree(&dyClause);
    return NULL;
    }
return dyStringCannibalize(&dyClause);
}

void filterBySetCfgUi(struct trackDb *tdb, filterBy_t *filterBySet)
/* Does the UI for a list of filterBy structure */
{
if(filterBySet == NULL)
    return;

#define FILTERBY_HELP_LINK  "<A HREF=\"../goldenPath/help/multiView.html\" TARGET=ucscHelp>help</A>"
int count = slCount(filterBySet);
if(count == 1)
    puts("<BR><TABLE cellpadding=3><TR valign='top'>");
else
    printf("<BR><B>Filter by</B> (select multiple categories and items - %s)<TABLE cellpadding=3><TR valign='top'>\n",FILTERBY_HELP_LINK);
filterBy_t *filterBy = NULL;
for(filterBy = filterBySet;filterBy != NULL; filterBy = filterBy->next)
    {
    puts("<TD>");
    if(count == 1)
        printf("<B>Filter by %s</B> (select multiple items - %s)<BR>\n",filterBy->title,FILTERBY_HELP_LINK);
    else
        printf("<B>%s</B><BR>\n",filterBy->title);
    int fullSize = slCount(filterBy->slValues)+1;
    int openSize = min(20,fullSize);
    int closedSize = (filterBy->slChoices == NULL || slCount(filterBy->slChoices) == 1 ? 1 : openSize);  //slCount(filterBy->slValues)+1);   // slChoice ??
//#define MULTI_SELECT_WITH_JS "<div class='multiSelectContainer'><SELECT name='%s.filterBy.%s' multiple=true size=%d openSize=%d style='display: none' onclick='multiSelectClick(this,%d);' onblur='multiSelectBlur(this,%d);' class='normalText filterBy'></div><BR>\n"
//    printf(MULTI_SELECT_WITH_JS,tdb->tableName,filterBy->column,closedSize,openSize,openSize,openSize);
#define MULTI_SELECT_WITH_JS "<SELECT name='%s.filterBy.%s' multiple=true size=%d onkeydown='this.size=%d' onclick='multiSelectClick(this,%d);' onblur='multiSelectBlur(this);' class='filterBy'><BR>\n"
    printf(MULTI_SELECT_WITH_JS,tdb->tableName,filterBy->column,closedSize,openSize,openSize);
    printf("<OPTION%s>All</OPTION>\n",(filterBy->slChoices == NULL || slNameInList(filterBy->slChoices,"All")?" SELECTED":"") );
    struct slName *slValue;
    if(filterBy->useIndex)
        {
        int ix=1;
        for(slValue=filterBy->slValues;slValue!=NULL;slValue=slValue->next,ix++)
            {
            char varName[32];
            safef(varName, sizeof(varName), "%d",ix);
            char *name = strSwapChar(cloneString(slValue->name),'_',' ');
                printf("<OPTION%s value=%s>%s</OPTION>\n",(filterBy->slChoices != NULL && slNameInList(filterBy->slChoices,varName)?" SELECTED":""),varName,name);
            freeMem(name);
            }
        }
    else if(filterBy->valueAndLabel)
        {
        for(slValue=filterBy->slValues;slValue!=NULL;slValue=slValue->next)
            printf("<OPTION%s value=%s>%s</OPTION>\n",(filterBy->slChoices != NULL && slNameInList(filterBy->slChoices,slValue->name)?" SELECTED":""),slValue->name,slValue->name+strlen(slValue->name)+1);
        }
    else
        {
        for(slValue=filterBy->slValues;slValue!=NULL;slValue=slValue->next)
            printf("<OPTION%s>%s</OPTION>\n",(filterBy->slChoices != NULL && slNameInList(filterBy->slChoices,slValue->name)?" SELECTED":""),slValue->name);
        }
    }
    // The following is needed to make msie scroll to selected option.
    printf("<script type='text/javascript'>onload=function(){ if( $.browser.msie ) { $(\"select[name^='%s.filterBy.']\").children('option[selected]').each( function(i) { $(this).attr('selected',true); }); }}</script>\n",tdb->tableName);
puts("</TR></TABLE>");

return;
}

#define COLOR_BG_DEFAULT_IX     0
#define COLOR_BG_ALTDEFAULT_IX  1
#define DIVIDING_LINE "<TR valign=\"CENTER\" line-height=\"1\" BGCOLOR=\"%s\"><TH colspan=\"5\" align=\"CENTER\"><hr noshade color=\"%s\" width=\"100%%\"></TD></TR>\n"
#define DIVIDER_PRINT(color) printf(DIVIDING_LINE,COLOR_BG_DEFAULT,(color))

static char *checkBoxIdMakeForTrack(struct trackDb *tdb,members_t** dims,int dimMax,membership_t *membership)
/* Creates an 'id' string for subtrack checkbox in style that matrix understand: "cb_dimX_dimY_view_cb" */
{
int ix;
#define CHECKBOX_ID_SZ 128
// What is wanted: id="cb_ES_K4_SIG_cb"
struct dyString *id = newDyString(CHECKBOX_ID_SZ);
dyStringPrintf(id,"cb_%s_", tdb->tableName);
int dimIx=1; // Skips over view dim for now
for(;dimIx<dimMax;dimIx++)
    {
    if(dims[dimIx] != NULL)
        {
        ix = stringArrayIx(dims[dimIx]->tag, membership->subgroups, membership->count);
        if(ix >= 0)
            dyStringPrintf(id,"%s_", membership->membership[ix]);
        }
    }
if(dims[0] != NULL) // The view is saved for last
    {
    ix = stringArrayIx("view", membership->subgroups, membership->count);   // view is a known tagname
    if(ix >= 0)
        dyStringPrintf(id,"%s_", membership->membership[ix]);
    }
dyStringAppend(id,"cb");
return dyStringCannibalize(&id);
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

boolean tdbSortPrioritiesFromCart(struct cart *cart, struct trackDb **tdbList)
/* Updates the tdb->priority from cart then sorts the list anew.
   Returns TRUE if priorities obtained from cart */
{
char htmlIdentifier[128];
struct trackDb *tdb;
boolean cartPriorities = FALSE;
for (tdb = *tdbList; tdb != NULL; tdb = tdb->next)
    {
        safef(htmlIdentifier, sizeof(htmlIdentifier), "%s.priority", tdb->tableName);
        char *cartHas = cartOptionalString(cart,htmlIdentifier);
        if(cartHas != NULL)
            {
            tdb->priority = atof(cartHas);
            cartPriorities = TRUE;
            }
    }
    slSort(tdbList, trackDbCmp);
return cartPriorities;
}

static void cfgByCfgType(eCfgType cType,char *db, struct cart *cart, struct trackDb *tdb,char *prefix, char *title, boolean boxed)
{
switch(cType)
    {
    case cfgBedScore:
	{
	char *scoreMax = trackDbSettingClosestToHome(tdb, SCORE_FILTER _MAX);
	int maxScore = (scoreMax ? sqlUnsigned(scoreMax):1000);
	scoreCfgUi(db, cart,tdb,prefix,title,maxScore,boxed);
	}
	break;
    case cfgPeak:
                        encodePeakCfgUi(cart,tdb,prefix,title,boxed);
                        break;
    case cfgWig:        wigCfgUi(cart,tdb,prefix,title,boxed);
                        break;
    case cfgWigMaf:     wigMafCfgUi(cart,tdb,prefix,title,boxed, db);
                        break;
    case cfgGenePred:   genePredCfgUi(cart,tdb,prefix,title,boxed);
                        break;
    case cfgChain:      chainCfgUi(db,cart,tdb,prefix,title,boxed, NULL);
                        break;
    case cfgNetAlign:	netAlignCfgUi(db,cart,tdb,prefix,title,boxed);
                        break;
    case cfgBedFilt:    bedUi(tdb,cart,title, boxed);
                 	break;
    default:            warn("Track type is not known to multi-view composites. type is: %d ", cType);
                        break;
    }
}

char *encodeRestrictionDateDisplay(struct trackDb *trackDb)
/* Create a string for ENCODE restriction date of this track
   if return is not null, then free it after use */
{
if (!trackDb)
    return NULL;
boolean addMonths = FALSE;
char *date = metadataSettingFind(trackDb,"dateUnrestricted");
if(date == NULL)
    {
    date = metadataSettingFind(trackDb,"dateSubmitted");
    addMonths = TRUE;
    }
if(date == NULL)
    {
    addMonths = FALSE;
    date = trackDbSetting(trackDb, "dateUnrestricted");
    if(date)
        date = cloneString(date); // all returns should be freeable memory
    }
if(date == NULL)
    {
    date = trackDbSetting(trackDb, "dateSubmitted");
    if(date)
        {
        addMonths = TRUE;
        date = cloneString(date); // all returns should be freeable memory
        }
    }
if (date != NULL)
    {
    date = strSwapChar(date, ' ', 0);   // Truncate time
    if(addMonths)
        date = dateAddToAndFormat(date, "%F", 0, 9, 0);
    }
return date;
}

static void cfgLinkToDependentCfgs(struct trackDb *tdb,char *prefix)
/* Link composite or view level controls to all associateled lower level controls */
{
if(tdbIsComposite(tdb))
    printf("<script type='text/javascript'>compositeCfgRegisterOnchangeAction(\"%s\")</script>\n",prefix);
}

static void compositeUiSubtracks(char *db, struct cart *cart, struct trackDb *parentTdb,
                 boolean selectedOnly, char *primarySubtrack)
/* Show checkboxes for subtracks. */
{
struct trackDb *subtrack;
char *primaryType = getPrimaryType(primarySubtrack, parentTdb);
char htmlIdentifier[SMALLBUF];
struct dyString *dyHtml = newDyString(SMALLBUF);
char *words[5];
char *colors[2]   = { COLOR_BG_DEFAULT,
                      COLOR_BG_ALTDEFAULT };
int colorIx = COLOR_BG_DEFAULT_IX; // Start with non-default allows alternation
boolean dependentCfgsNeedBinding = FALSE;

// Look for dividers, heirarchy, dimensions, sort and dragAndDrop!
char **lastDivide = NULL;
dividers_t *dividers = dividersSettingGet(parentTdb);
if(dividers)
    lastDivide = needMem(sizeof(char*)*dividers->count);
hierarchy_t *hierarchy = hierarchySettingGet(parentTdb);

enum
{
    dimV=0, // View first
    dimX=1, // X & Y next
    dimY=2,
    dimA=3, // dimA is start of first of the optional non-matrix, non-view dimensions
};
int dimMax=dimA;  // This can expand, depending upon ABC dimensions
members_t* dimensions[27]; // Just pointers, so make a bunch!
memset((char *)dimensions,0,sizeof(dimensions));
dimensions_t *dims = dimensionSettingsGet(parentTdb);
if(dims != NULL)
    {
    int ix;
    for(ix=0;ix<dims->count;ix++)
        {
        char letter = lastChar(dims->names[ix]);
        if(letter != 'X' && letter != 'Y')
            {
            dimensions[dimMax]=subgroupMembersGet(parentTdb, dims->subgroups[ix]);
            dimMax++;
            }
        else if(letter == 'X')
            dimensions[dimX]=subgroupMembersGet(parentTdb, dims->subgroups[ix]);
        else
            dimensions[dimY]=subgroupMembersGet(parentTdb, dims->subgroups[ix]);
        }
    dimensionsFree(&dims);
    }
dimensions[dimV]=subgroupMembersGet(parentTdb,"view");
int dimCount=0,di;
for(di=0;di<dimMax;di++) { if(dimensions[di]) dimCount++; }
sortOrder_t* sortOrder = sortOrderGet(cart,parentTdb);
boolean preSorted = FALSE;
boolean useDragAndDrop = sameOk("subTracks",trackDbSetting(parentTdb, "dragAndDrop"));

// Now we can start in on the table of subtracks
printf("\n<TABLE CELLSPACING='2' CELLPADDING='0' border='0'");
if(sortOrder != NULL)
    {
    printf(" id='subtracks.%s.sortable'",parentTdb->tableName);
    dyStringClear(dyHtml);
    dyStringPrintf(dyHtml, "tableSortable");
    colorIx = COLOR_BG_ALTDEFAULT_IX;
    }
else
    printf(" id='subtracks.%s'",parentTdb->tableName);
if(useDragAndDrop)
    {
    if(dyStringLen(dyHtml) > 0)
        dyStringAppendC(dyHtml,' ');
    dyStringPrintf(dyHtml, "tableWithDragAndDrop");
    printf(" class='%s'",dyStringContents(dyHtml));
    dyStringClear(dyHtml);
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
        char *date = metadataSettingFind(subtrack,"dateUnrestricted");
        if(date != NULL)
            {
            freeMem(date);
            restrictions = TRUE;
            break;
            }
        }

    printf("<TR");
    if(useDragAndDrop)
        printf(" id=\"noDrag\" class='nodrop nodrag'");
    printf("><TD colspan='%d'><B>List subtracks:&nbsp;",(sortOrder != NULL?sortOrder->count+2:3));
    safef(javascript, sizeof(javascript), "onclick=\"showOrHideSelectedSubtracks(true);\"");
    cgiMakeOnClickRadioButton("displaySubtracks", "selected", !displayAll,javascript);
    puts("only selected/visible &nbsp;&nbsp;");
    safef(javascript, sizeof(javascript), "onclick=\"showOrHideSelectedSubtracks(false);\"");
    cgiMakeOnClickRadioButton("displaySubtracks", "all", displayAll,javascript);
    puts("all</B></TD>");

    if(sortOrder != NULL)   // Add some sort buttons
        {
        puts("<TD colspan=5>&nbsp;</TD></TR>");
        printf("<TR id=\"%s.sortTr\" class='nodrop nodrag'>\n",parentTdb->tableName);     // class='nodrop nodrag'
        printf("<TD>&nbsp;<INPUT TYPE=HIDDEN NAME='%s' id='%s' VALUE=\"%s\"></TD>", sortOrder->htmlId, sortOrder->htmlId,sortOrder->sortOrder); // keeing track of priority
        // Columns in tdb order (unchanging), sort in cart order (changed by user action)
        int sIx=0;
        for(sIx=0;sIx<sortOrder->count;sIx++)
            {
            printf ("<TH id='%s.%s.sortTh' abbr='%c' nowrap><A HREF='#nowhere' onclick=\"tableSortAtButtonPress(this,'%s');return false;\">%s</A><sup>%s",
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
    preSorted = tdbSortPrioritiesFromCart(cart, &(parentTdb->subtracks)); // preserves user's prev sort/drags
    puts("</TR></THEAD><TBODY id='sortable_tbody'>");
    }
else
    {
    slSort(&(parentTdb->subtracks), trackDbCmp);  // straight from trackDb.ra
    preSorted = TRUE;
    puts("</TR></THEAD><TBODY>");
    }

for (subtrack = parentTdb->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    boolean checkedCB = TRUE;
    boolean enabledCB = TRUE;
    boolean isPrimary = FALSE;
    char *setting;
    int ix;

    if ((setting = trackDbSetting(subtrack, "subTrack")) != NULL)
        {
        if (chopLine(cloneString(setting), words) >= 2)
            checkedCB = differentString(words[1], "off");
        }
    safef(htmlIdentifier, sizeof(htmlIdentifier), "%s_sel", subtrack->tableName);
    setting = cartOptionalString(cart, htmlIdentifier);
    if(setting != NULL)
        {
        int state = atoi(setting);
        checkedCB = (state == 1 || state == -1);  // checked/eanbled:1 unchecked/enabled:0 checked/disabled:-1 unchecked/disabled:-2
        enabledCB = (state >= 0);
        }
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
        else if (hSameTrackDbType(primaryType, subtrack->type))
            // && hTableExists(db, subtrack->tableName))  REMOVED because using parentTdbAbandonTablelessChildren before we are here
            {
            puts("<TR><TD>");
            cgiMakeCheckBox(htmlIdentifier, checkedCB && enabledCB);
            printf ("</TD><TD>%s</TD></TR>\n", subtrack->longLabel);
            }
        }
    else
        {
        eCfgType cType = cfgTypeFromTdb(subtrack,FALSE);
        if(trackDbSettingClosestToHomeOn(subtrack, "configurable") == FALSE)
            cType = cfgNone;
        membership_t *membership = subgroupMembershipGet(subtrack);
        //if (hTableExists(db, subtrack->tableName))  REMOVED because using parentTdbAbandonTablelessChildren before we are here
            {
            if(sortOrder == NULL && !useDragAndDrop)
                {
                if( divisionIfNeeded(lastDivide,dividers,membership) )
                    colorIx = (colorIx == COLOR_BG_DEFAULT_IX ? COLOR_BG_ALTDEFAULT_IX : COLOR_BG_DEFAULT_IX);
                }

            char *id = checkBoxIdMakeForTrack(subtrack,dimensions,dimMax,membership); // view is known tag
            printf("<TR valign='top' BGCOLOR=\"%s\"",colors[colorIx]);
            if(useDragAndDrop)
                printf(" class='trDraggable' title='Drag to Reorder'");

            printf(" id=\"tr_%s\" nowrap%s>\n<TD>",id,(selectedOnly?" style='display:none'":""));
            dyStringClear(dyHtml);
            dyStringPrintf(dyHtml, "onclick='matSubCbClick(this);' onmouseover=\"this.style.cursor='default';\" class=\"subCB");
            for(di=dimX;di<dimMax;di++)
                {
                if(dimensions[di] && -1 != (ix = stringArrayIx(dimensions[di]->tag, membership->subgroups, membership->count)))
                    dyStringPrintf(dyHtml, " %s",membership->membership[ix]);
                }
            // Save view for last
            if(dimensions[dimV] && -1 != (ix = stringArrayIx(dimensions[dimV]->tag, membership->subgroups, membership->count)))
                dyStringPrintf(dyHtml, " %s",membership->membership[ix]);
            dyStringAppendC(dyHtml,'"');
            cgiMakeCheckBox2BoolWithIdAndJS(htmlIdentifier,checkedCB,enabledCB,id,dyStringContents(dyHtml));
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
                    ix = stringArrayIx(sortOrder->column[sIx], membership->subgroups, membership->count);                        // TODO: Sort needs to expand from subGroups to labels as well
                    if(ix >= 0)
                        {
                        char *titleRoot=labelRoot(membership->titles[ix],NULL);

#define CFG_SUBTRACK_LINK  "<A HREF='#a_cfg_%s' onclick='return subtrackCfgShow(\"%s\");' title='Subtrack Configuration'>%s</A>\n"
#define MAKE_CFG_SUBTRACK_LINK(table,title) printf(CFG_SUBTRACK_LINK, (table),(table),(title))
                        printf ("<TD id='%s' nowrap abbr='%s' align='left'>&nbsp;",sortOrder->column[sIx],membership->membership[ix]);
                        if(cType != cfgNone && sameString("view",sortOrder->column[sIx]))
                            MAKE_CFG_SUBTRACK_LINK(subtrack->tableName,titleRoot);  // FIXME: Currently configurable under sort only supported when multiview
                        else
                            printf("%s\n",titleRoot);
                        puts ("</TD>");
                        freeMem(titleRoot);
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
            printf ("<TD nowrap='true' title='select to copy' onmouseover=\"this.style.cursor='text';\"><div>&nbsp;%s", subtrack->longLabel);
            if(trackDbSetting(parentTdb, "wgEncode") && trackDbSetting(subtrack, "accession"))
                printf (" [GEO:%s]", trackDbSetting(subtrack, "accession"));
            metadataToggle(subtrack,"...",TRUE,FALSE);
            printf("</div>");

            if(cType != cfgNone)
                {
                dependentCfgsNeedBinding = TRUE; // configurable subtrack needs to be bound to composite settings
                if(membership)
                    ix = stringArrayIx("view", membership->subgroups, membership->count);
                else
                    ix = -1;
#define CFG_SUBTRACK_DIV "<DIV id='div_%s_cfg'%s><INPUT TYPE=HIDDEN NAME='%s' value='%s'>\n"
#define MAKE_CFG_SUBTRACK_DIV(table,cfgVar,open) printf(CFG_SUBTRACK_DIV,(table),((open)?"":" style='display:none'"),(cfgVar),((open)?"on":"off"))
                safef(htmlIdentifier,sizeof(htmlIdentifier),"%s.childShowCfg",subtrack->tableName);
                boolean open = cartUsualBoolean(cart, htmlIdentifier,FALSE);
                MAKE_CFG_SUBTRACK_DIV(subtrack->tableName,htmlIdentifier,open);
                if(ix >= 0)
                    safef(htmlIdentifier,sizeof(htmlIdentifier),"%s.%s",subtrack->tableName,membership->membership[ix]);
                else
                    safef(htmlIdentifier,sizeof(htmlIdentifier),"%s",subtrack->tableName);
                cfgByCfgType(cType,db,cart,subtrack,htmlIdentifier,"Subtrack",TRUE);
                puts("</DIV>\n");
                }
            printf("<TD nowrap>&nbsp;");
            makeSchemaLink(db,subtrack,"schema");
            puts("&nbsp;");

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
if(slCount(parentTdb->subtracks) > 5)
    puts("&nbsp;&nbsp;&nbsp;&nbsp;<FONT id='subCBcount'></font>");
puts("<P>");
//if (!preSorted && sortOrder != NULL)  // No longer need to do this since hgTrackDb should sort composites with sortOrder and set priorities
//    puts("<script type='text/javascript'>tableSortAtStartup();</script>");
if (!primarySubtrack)
    puts("<script type='text/javascript'>matInitializeMatrix();</script>");
if(dependentCfgsNeedBinding)
    cfgLinkToDependentCfgs(parentTdb,parentTdb->tableName);
for(di=0;di<dimMax;di++)
    subgroupMembersFree(&dimensions[di]);
dyStringFree(&dyHtml)
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

static boolean cfgBeginBoxAndTitle(struct trackDb *tdb, boolean boxed, char *title)
/* Handle start of box and title for individual track type settings */
{
if(!boxed)
    {
    boxed = trackDbSettingOn(tdb,"boxedCfg");
    if(boxed)
        printf("<BR>");
    }
if (boxed)
    {
    printf("<TABLE class='blueBox' bgcolor=\"%s\" borderColor=\"%s\"><TR><TD align='RIGHT'>", COLOR_BG_ALTDEFAULT, COLOR_BG_ALTDEFAULT);
    if (title)
        printf("<CENTER><B>%s Configuration</B></CENTER>\n", title);
    }
else if (title)
    printf("<p><B>%s &nbsp;</b>", title );
else
    printf("<p>");
return boxed;
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
char *transformFunc = NULL;    /* function to transform data points */
char *alwaysZero = NULL;    /* Always include 0 in range */
char *lineBar;  /*  Line or Bar graph */
char *autoScale;    /*  Auto scaling on or off */
char *windowingFunction;    /*  Maximum, Mean, or Minimum */
char *smoothingWindow;  /*  OFF or [2:16] */
char *yLineMarkOnOff;   /*  user defined Y marker line to draw */
double yLineMark;       /*  from trackDb or cart    */
int maxHeightPixels = atoi(DEFAULT_HEIGHT_PER);
int minHeightPixels = MIN_HEIGHT_PER;

boxed = cfgBeginBoxAndTitle(tdb, boxed, title);

wigFetchMinMaxPixelsWithCart(cart,tdb,name,&minHeightPixels, &maxHeightPixels, &defaultHeight);
typeLine = cloneString(tdb->type);
wordCount = chopLine(typeLine,words);

wigFetchMinMaxYWithCart(cart,tdb,name, &minY, &maxY, &tDbMinY, &tDbMaxY, wordCount, words);
freeMem(typeLine);

(void) wigFetchTransformFuncWithCart(cart,tdb,name, &transformFunc);
(void) wigFetchAlwaysZeroWithCart(cart,tdb,name, &alwaysZero);
(void) wigFetchHorizontalGridWithCart(cart,tdb,name, &horizontalGrid);
(void) wigFetchAutoScaleWithCart(cart,tdb,name, &autoScale);
(void) wigFetchGraphTypeWithCart(cart,tdb,name, &lineBar);
(void) wigFetchWindowingFunctionWithCart(cart,tdb,name, &windowingFunction);
(void) wigFetchSmoothingWindowWithCart(cart,tdb,name, &smoothingWindow);
(void) wigFetchYLineMarkWithCart(cart,tdb,name, &yLineMarkOnOff);
wigFetchYLineMarkValueWithCart(cart,tdb,name, &yLineMark);

printf("<TABLE BORDER=0>");

printf("<TR valign=center><th align=right>Type of graph:</th><td align=left>");
snprintf( option, sizeof(option), "%s.%s", name, LINEBAR );
wiggleGraphDropDown(option, lineBar);
if(boxed)
    {
    printf("</td><td align=right colspan=2>");
    printf("<A HREF=\"%s\" TARGET=_blank>Graph configuration help</A>",WIGGLE_HELP_PAGE);
    }
puts("</td></TR>");

printf("<TR valign=center><th align=right>Track height:</th><td align=left colspan=3>");
snprintf(option, sizeof(option), "%s.%s", name, HEIGHTPER );
cgiMakeIntVarWithLimits(option, defaultHeight, "Track height",0, minHeightPixels, maxHeightPixels);
printf("pixels&nbsp;(range: %d to %d)",
    minHeightPixels, maxHeightPixels);
puts("</TD></TR>");

printf("<TR valign=center><th align=right>Vertical viewing range:</th><td align=left>&nbsp;min:&nbsp;");
snprintf(option, sizeof(option), "%s.%s", name, MIN_Y );
cgiMakeDoubleVarWithLimits(option, minY, "Range min", 0, NO_VALUE, NO_VALUE);//tDbMinY, tDbMaxY);  // Jim requests this does not enforce limits
printf("</td><td align=leftv colspan=2>max:&nbsp;");
snprintf(option, sizeof(option), "%s.%s", name, MAX_Y );
cgiMakeDoubleVarWithLimits(option, maxY, "Range max", 0, NO_VALUE, NO_VALUE);//tDbMinY, tDbMaxY);
printf("&nbsp;(range: %g to %g)",
    tDbMinY, tDbMaxY);
puts("</TD></TR>");

printf("<TR valign=center><th align=right>Data view scaling:</th><td align=left colspan=3>");
snprintf(option, sizeof(option), "%s.%s", name, AUTOSCALE );
wiggleScaleDropDown(option, autoScale);
snprintf(option, sizeof(option), "%s.%s", name, ALWAYSZERO);
printf("Always include zero:&nbsp");
wiggleAlwaysZeroDropDown(option, alwaysZero);
puts("</TD></TR>");

printf("<TR valign=center><th align=right>Transform function:</th><td align=left>");
snprintf(option, sizeof(option), "%s.%s", name, TRANSFORMFUNC);
printf("Transform data points by:&nbsp");
wiggleTransformFuncDropDown(option, transformFunc);

printf("<TR valign=center><th align=right>Windowing function:</th><td align=left>");
snprintf(option, sizeof(option), "%s.%s", name, WINDOWINGFUNCTION );
wiggleWindowingDropDown(option, windowingFunction);

printf("<th align=right>Smoothing window:</th><td align=left>");
snprintf(option, sizeof(option), "%s.%s", name, SMOOTHINGWINDOW );
wiggleSmoothingDropDown(option, smoothingWindow);
puts("&nbsp;pixels</TD></TR>");

printf("<TR valign=center><td align=right><b>Draw y indicator lines:</b><td align=left colspan=2>");
printf("at y = 0.0:");
snprintf(option, sizeof(option), "%s.%s", name, HORIZGRID );
wiggleGridDropDown(option, horizontalGrid);
printf("&nbsp;&nbsp;&nbsp;at y =");
snprintf(option, sizeof(option), "%s.%s", name, YLINEMARK );
cgiMakeDoubleVarWithLimits(option, yLineMark, "Indicator at Y", 0, tDbMinY, tDbMaxY);
printf("</td><td align=left>");
snprintf(option, sizeof(option), "%s.%s", name, YLINEONOFF );
wiggleYLineMarkDropDown(option, yLineMarkOnOff);
if(boxed)
    puts("</TD></TR></TABLE>");
else
    {
    puts("</TD></TR></TABLE>");
    printf("<A HREF=\"%s\" TARGET=_blank>Graph configuration help</A>",WIGGLE_HELP_PAGE);
    }

cfgEndBox(boxed);
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

void radioButton(char *var, char *val, char *ourVal)
/* Print one radio button */
{
cgiMakeRadioButton(var, ourVal, sameString(ourVal, val));
printf("%s ", ourVal);
}

void oneMrnaFilterUi(struct controlGrid *cg, char *text, char *var, struct cart *cart)
/* Print out user interface for one type of mrna filter. */
{
controlGridStartCell(cg);
printf("%s:<BR>", text);
cgiMakeTextVar(var, cartUsualString(cart, var, ""), 19);
controlGridEndCell(cg);
}

void bedUi(struct trackDb *tdb, struct cart *cart, char *title, boolean boxed)
/* Put up UI for an mRNA (or EST) track. */
{
struct mrnaUiData *mud = newBedUiData(tdb->tableName);
struct mrnaFilter *fil;
struct controlGrid *cg = NULL;
char *filterTypeVar = mud->filterTypeVar;
char *filterTypeVal = cartUsualString(cart, filterTypeVar, "red");
boxed = cfgBeginBoxAndTitle(tdb, boxed, title);
/* Define type of filter. */
printf("<table width=400><tr><td align='left'>\n");
filterButtons(filterTypeVar, filterTypeVal, FALSE);
printf("</br>");
/* List various fields you can filter on. */
cg = startControlGrid(4, NULL);
for (fil = mud->filterList; fil != NULL; fil = fil->next)
    oneMrnaFilterUi(cg, fil->label, fil->key, cart);
endControlGrid(&cg);
cfgEndBox(boxed);
}


void scoreGrayLevelCfgUi(struct cart *cart, struct trackDb *tdb, char *prefix, int scoreMax)
/* If scoreMin has been set, let user select the shade of gray for that score, in case
 * the default is too light to see or darker than necessary. */
{
boolean compositeLevel = isNameAtCompositeLevel(tdb,prefix);
char *scoreMinStr = trackDbSettingClosestToHome(tdb, GRAY_LEVEL_SCORE_MIN);
if (scoreMinStr != NULL)
    {
    int scoreMin = atoi(scoreMinStr);
    // maxShade=9 taken from hgTracks/simpleTracks.c.  Ignore the 10 in shadesOfGray[10+1] --
    // maxShade is used to access the array.
    int maxShade = 9;
    int scoreMinGrayLevel = scoreMin * maxShade/scoreMax;
    if (scoreMinGrayLevel <= 0) scoreMinGrayLevel = 1;
    char *setting = trackDbSettingClosestToHome(tdb, MIN_GRAY_LEVEL);
    int minGrayLevel = cartUsualIntClosestToHome(cart, tdb, compositeLevel, MIN_GRAY_LEVEL,
                        setting ? atoi(setting) : scoreMinGrayLevel);
    if (minGrayLevel <= 0) minGrayLevel = 1;
    if (minGrayLevel > maxShade) minGrayLevel = maxShade;
    puts("\n<P><B>Shade of lowest-scoring items: </B>");
    // Add javascript to select so that its color is consistent with option colors:
    int level = 255 - (255*minGrayLevel / maxShade);
    printf("<SELECT NAME=\"%s.%s\" STYLE='color: #%02x%02x%02x' class='normalText'",
	   prefix, MIN_GRAY_LEVEL, level, level, level);
    int i;
#ifdef OMIT
    // IE works without this code and FF doesn't work with it.
    printf(" onchange=\"switch(this.value) {");
    for (i = 1;  i < maxShade;  i++)
        {
        level = 255 - (255*i / maxShade);
        printf ("case '%d': $(this).css('color','#%02x%02x%02x'); break; ",i, level, level, level);
        }
    level = 255 - (255*i / maxShade);
    printf("default: $(this).css('color','#%02x%02x%02x'); }\"", level, level, level);
#endif//def OMIT
    puts(">\n");
    // Use class to set color of each option:
    for (i = 1;  i <= maxShade;  i++)
        {
        level = 255 - (255*i / maxShade);
        printf("<OPTION%s STYLE='color: #%02x%02x%02x' VALUE=%d>",
            (minGrayLevel == i ? " SELECTED" : ""), level, level, level, i);
        if (i == maxShade)
            printf("&bull; black</OPTION>\n");
        else
            printf("&bull; gray (%d%%)</OPTION>\n", i * (100/maxShade));
        }
    printf("</SELECT></P>\n");
    }
}

static boolean getScoreDefaultsFromTdb(struct trackDb *tdb, char *scoreName,char *defaults,char**min,char**max)
/* returns TRUE if defaults exist and sets the string pointer (because they may be float or int)
   if min or max are set, then they should be freed */
{
if(min)
    *min = NULL; // default these outs!
if(max)
    *max = NULL;
char *setting = trackDbSettingClosestToHome(tdb, scoreName);
if(setting)
    {
    if(strchr(setting,':') != NULL)
        return colonPairToStrings(setting,min,max);
    else if(min)
        *min = cloneString(setting);
    return TRUE;
    }
return FALSE;
}

static boolean getScoreLimitsFromTdb(struct trackDb *tdb, char *scoreName,char *defaults,char**min,char**max)
/* returns TRUE if limits exist and sets the string pointer (because they may be float or int)
   if min or max are set, then they should be freed */
{
if(min)
    *min = NULL; // default these outs!
if(max)
    *max = NULL;
char scoreLimitName[128];
safef(scoreLimitName, sizeof(scoreLimitName), "%s%s", scoreName, _LIMITS);
char *setting = trackDbSettingClosestToHome(tdb, scoreLimitName);
if(setting)
    {
    return colonPairToStrings(setting,min,max);
    }
else
    {
    if(min)
        {
        safef(scoreLimitName, sizeof(scoreLimitName), "%s%s", scoreName, _MIN);
        setting = trackDbSettingClosestToHome(tdb, scoreLimitName);
        if(setting)
            *min = cloneString(setting);
        }
    if(max)
        {
        safef(scoreLimitName, sizeof(scoreLimitName), "%s%s", scoreName, _MAX);
        setting = trackDbSettingClosestToHome(tdb, scoreLimitName);
        if(setting)
            *max = cloneString(setting);
        }
    return TRUE;
    }
if(defaults != NULL && ((min && *min == NULL) || (max && *max == NULL)))
    {
    char *minLoc=NULL;
    char *maxLoc=NULL;
    if(colonPairToStrings(defaults,&minLoc,&maxLoc))
        {
        if(min && *min == NULL && minLoc != NULL)
            *min=minLoc;
        else
            freeMem(minLoc);
        if(max && *max == NULL && maxLoc != NULL)
            *max=maxLoc;
        else
            freeMem(maxLoc);
        return TRUE;
        }
    }
return FALSE;
}

static void getScoreIntRangeFromCart(struct cart *cart, struct trackDb *tdb, char *scoreName,
                                 int *limitMin, int *limitMax,int *min,int *max)
/* gets an integer score range from the cart, but the limits from trackDb
   for any of the pointers provided, will return a value found, if found, else it's contents
   are undisturbed (use NO_VALUE to recognize unavaliable values) */
{
char scoreLimitName[128];
char *deMin=NULL,*deMax=NULL;
if((limitMin || limitMax) && getScoreLimitsFromTdb(tdb,scoreName,NULL,&deMin,&deMax))
    {
    if(deMin != NULL && limitMin)
        *limitMin = atoi(deMin);
    if(deMax != NULL && limitMax)
        *limitMax = atoi(deMax);
    freeMem(deMin);
    freeMem(deMax);
    }
if((min || max) && getScoreDefaultsFromTdb(tdb,scoreName,NULL,&deMin,&deMax))
    {
    if(deMin != NULL && min)
        *min = atoi(deMin);
    if(deMax != NULL && max)
        *max =atoi(deMax);
    freeMem(deMin);
    freeMem(deMax);
    }
if(max)
    {
    safef(scoreLimitName, sizeof(scoreLimitName), "%s%s", scoreName, _MAX);
    deMax = cartOptionalStringClosestToHome(cart, tdb,FALSE,scoreLimitName);
    if(deMax != NULL)
        *max = atoi(deMax);
    }
if(min)
    {
    safef(scoreLimitName, sizeof(scoreLimitName), "%s%s", scoreName, (max && deMax? _MIN:"")); // Warning: name changes if max!
    deMin = cartOptionalStringClosestToHome(cart, tdb,FALSE,scoreLimitName);
    if(deMin != NULL)
        *min = atoi(deMin);
    }
if(min && limitMin && *min != NO_VALUE && *min < *limitMin) *min = *limitMin; // defaults within range
if(min && limitMax && *min != NO_VALUE && *min > *limitMax) *min = *limitMax;
if(max && limitMax && *max != NO_VALUE && *max > *limitMax) *max = *limitMax;
if(max && limitMin && *max != NO_VALUE && *max < *limitMin) *max = *limitMin;
}

static void getScoreFloatRangeFromCart(struct cart *cart, struct trackDb *tdb, char *scoreName,
                                   double *limitMin,double *limitMax,double*min,double*max)
/* gets an double score range from the cart, but the limits from trackDb
   for any of the pointers provided, will return a value found, if found, else it's contents
   are undisturbed (use NO_VALUE to recognize unavaliable values) */
{
char scoreLimitName[128];
char *deMin=NULL,*deMax=NULL;
if((limitMin || limitMax) && getScoreLimitsFromTdb(tdb,scoreName,NULL,&deMin,&deMax))
    {
    if(deMin != NULL && limitMin)
        *limitMin = strtod(deMin,NULL);
    if(deMax != NULL && limitMax)
        *limitMax =strtod(deMax,NULL);
    freeMem(deMin);
    freeMem(deMax);
    }
if((min || max) && getScoreDefaultsFromTdb(tdb,scoreName,NULL,&deMin,&deMax))
    {
    if(deMin != NULL && min)
        *min = strtod(deMin,NULL);
    if(deMax != NULL && max)
        *max =strtod(deMax,NULL);
    freeMem(deMin);
    freeMem(deMax);
    }
if(max)
    {
    safef(scoreLimitName, sizeof(scoreLimitName), "%s%s", scoreName, _MAX);
    deMax = cartOptionalStringClosestToHome(cart, tdb,FALSE,scoreLimitName);
    if(deMax != NULL)
        *max = strtod(deMax,NULL);
    }
if(min)
    {
    safef(scoreLimitName, sizeof(scoreLimitName), "%s%s", scoreName, _MIN); // name is always {filterName}Min
    deMin = cartOptionalStringClosestToHome(cart, tdb,FALSE,scoreLimitName);
    if(deMin != NULL)
        *min = strtod(deMin,NULL);
    }
if(min && limitMin && (int)(*min) != NO_VALUE && *min < *limitMin) *min = *limitMin; // defaults within range
if(min && limitMax && (int)(*min) != NO_VALUE && *min > *limitMax) *min = *limitMax;
if(max && limitMax && (int)(*max) != NO_VALUE && *max > *limitMax) *max = *limitMax;
if(max && limitMin && (int)(*max) != NO_VALUE && *max < *limitMin) *max = *limitMin;
}

void scoreCfgUi(char *db, struct cart *cart, struct trackDb *tdb, char *name, char *title,  int maxScore, boolean boxed)
/* Put up UI for filtering bed track based on a score */
{
char option[256];
boolean compositeLevel = isNameAtCompositeLevel(tdb,name);

filterBy_t *filterBySet = filterBySetGet(tdb,cart,name);
if(filterBySet != NULL)
    {
    if(!tdbIsComposite(tdb) && !tdbIsCompositeChild(tdb))
        jsIncludeFile("hui.js",NULL);

    filterBySetCfgUi(tdb,filterBySet);
    filterBySetFree(&filterBySet);
    return; // Cannot have both 'filterBy' score and 'scoreFilter'
    }


boolean scoreFilterOk = (trackDbSettingClosestToHome(tdb, NO_SCORE_FILTER) == NULL);
boolean glvlScoreMin = (trackDbSettingClosestToHome(tdb, GRAY_LEVEL_SCORE_MIN) != NULL);
if (! (scoreFilterOk || glvlScoreMin))
    return;

boxed = cfgBeginBoxAndTitle(tdb, boxed, title);

if (scoreFilterOk)
    {
    int minLimit=0,maxLimit=maxScore,minVal=0,maxVal=maxScore;
    getScoreIntRangeFromCart(cart,tdb,SCORE_FILTER,&minLimit,&maxLimit,&minVal,&maxVal);

    boolean filterByRange = trackDbSettingClosestToHomeOn(tdb, SCORE_FILTER _BY_RANGE);
    if (filterByRange)
        {
        puts("<B>Filter score range:  min:</B>");
        snprintf(option, sizeof(option), "%s.%s", name,SCORE_FILTER _MIN);
        cgiMakeIntVarWithLimits(option, minVal, "Minimum score",0, minLimit,maxLimit);
        puts("<B>max:</B>");
        snprintf(option, sizeof(option), "%s.%s", name,SCORE_FILTER _MAX);
        cgiMakeIntVarWithLimits(option, maxVal, "Maximum score",0,minLimit,maxLimit);
        printf("(%d to %d)\n",minLimit,maxLimit);
        }
    else
        {
        printf("<b>Show only items with score at or above:</b> ");
        snprintf(option, sizeof(option), "%s.%s", name,SCORE_FILTER);
        cgiMakeIntVarWithLimits(option, minVal, "Minimum score",0, minLimit,maxLimit);
        printf("&nbsp;&nbsp;(range: %d to %d)", minLimit, maxLimit);
        }
    }

if (glvlScoreMin)
    scoreGrayLevelCfgUi(cart, tdb, name, maxScore);

/* filter top-scoring N items in track */
char *scoreCtString = trackDbSettingClosestToHome(tdb, "filterTopScorers");
if (scoreCtString != NULL)
    {
    /* show only top-scoring items. This option only displayed if trackDb
     * setting exists.  Format:  filterTopScorers <on|off> <count> <table> */
    char *words[2];
    char *scoreFilterCt = NULL;
    chopLine(cloneString(scoreCtString), words);
    safef(option, sizeof(option), "%s.filterTopScorersOn", name);
    bool doScoreCtFilter =
        cartUsualBooleanClosestToHome(cart, tdb, compositeLevel, "filterTopScorersOn", sameString(words[0], "on"));
    puts("<P>");
    cgiMakeCheckBox(option, doScoreCtFilter);
    safef(option, sizeof(option), "%s.filterTopScorersCt", name);
    scoreFilterCt = cartUsualStringClosestToHome(cart, tdb, compositeLevel, "filterTopScorersCt", words[1]);

    puts("&nbsp; <B> Show only items in top-scoring </B>");
    cgiMakeIntVarWithLimits(option,atoi(scoreFilterCt),"Top-scoring count",0,1,100000);
    /* Only check size of table if track does not have subtracks */
    if ( !compositeLevel && hTableExists(db, tdb->tableName))
        printf("&nbsp; (range: 1 to 100,000 total items: %d)\n",getTableSize(db, tdb->tableName));
    else
        printf("&nbsp; (range: 1 to 100,000)\n");
    }
cfgEndBox(boxed);
}

void netAlignCfgUi(char *db, struct cart *cart, struct trackDb *tdb, char *prefix, char *title, boolean boxed)
/* Put up UI for net tracks */
{
boxed = cfgBeginBoxAndTitle(tdb, boxed, title);

boolean compositeLevel = isNameAtCompositeLevel(tdb,prefix);

enum netColorEnum netColor = netFetchColorOption(cart, tdb, compositeLevel);

char optString[256];	/*	our option strings here	*/
safef(optString, ArraySize(optString), "%s.%s", prefix, NET_COLOR );
printf("<p><b>Color nets by:&nbsp;</b>");
netColorDropDown(optString, netColorEnumToString(netColor));

#ifdef NOT_YET
enum netLevelEnum netLevel = netFetchLevelOption(cart, tdb, compositeLevel);

safef( optString, ArraySize(optString), "%s.%s", prefix, NET_LEVEL );
printf("<p><b>Limit display of nets to:&nbsp;</b>");
netLevelDropDown(optString, netLevelEnumToString(netLevel));
#endif

cfgEndBox(boxed);
}

void chainCfgUi(char *db, struct cart *cart, struct trackDb *tdb, char *prefix, char *title, boolean boxed, char *chromosome)
/* Put up UI for chain tracks */
{
boxed = cfgBeginBoxAndTitle(tdb, boxed, title);

boolean compositeLevel = isNameAtCompositeLevel(tdb,prefix);

enum chainColorEnum chainColor =
	chainFetchColorOption(cart, tdb, compositeLevel);

/* check if we have normalized scores available */
boolean normScoreAvailable = chainDbNormScoreAvailable(tdb);

char optString[256];
if (normScoreAvailable)
    {
    safef(optString, ArraySize(optString), "%s.%s", prefix, OPT_CHROM_COLORS );
    printf("<p><b>Color chains by:&nbsp;</b>");
    chainColorDropDown(optString, chainColorEnumToString(chainColor));
    }
else
    {
    printf("<p><b>Color track based on chromosome:</b>&nbsp;");

    char optString[256];
    /* initial value of chromosome coloring option is "on", unless
     * overridden by the colorChromDefault setting in the track */
    char *binaryColorDefault =
	    trackDbSettingClosestToHomeOrDefault(tdb, "colorChromDefault", "on");
    /* allow cart to override trackDb setting */
    safef(optString, sizeof(optString), "%s.color", prefix);
    char * colorSetting = cartUsualStringClosestToHome(cart, tdb,
	compositeLevel, "color", binaryColorDefault);
    cgiMakeRadioButton(optString, "on", sameString(colorSetting, "on"));
    printf(" on ");
    cgiMakeRadioButton(optString, "off", sameString(colorSetting, "off"));
    printf(" off ");
    printf("<br>\n");
    }

printf("<p><b>Filter by chromosome (e.g. chr10):</b> ");
safef(optString, ArraySize(optString), "%s.%s", prefix, OPT_CHROM_FILTER);
cgiMakeTextVar(optString,
    cartUsualStringClosestToHome(cart, tdb, compositeLevel,
	OPT_CHROM_FILTER, ""), 15);

if (normScoreAvailable)
    scoreCfgUi(db, cart,tdb,prefix,NULL,CHAIN_SCORE_MAXIMUM,FALSE);

cfgEndBox(boxed);
}

static boolean showScoreFilter(struct cart *cart, struct trackDb *tdb, boolean *opened, boolean boxed,
                               boolean compositeLevel,char *name, char *title, char *label,
                               char *scoreName,char *defaults,char *limitsDefault)
/* Shows a score filter control with minimum value and optional range */
{
char *setting = trackDbSettingClosestToHomeOrDefault(tdb, scoreName,defaults);//"0.0");
if(setting)
    {
    if(*opened == FALSE)
        {
        boxed = cfgBeginBoxAndTitle(tdb, boxed, title);
        puts("<TABLE>");
        *opened = TRUE;
        }
    printf("<TR><TD align='right'><B>%s:</B><TD align='left'>",label);
    char varName[256];
    char altLabel[256];
    safef(varName, sizeof(varName), "%s%s", scoreName, _BY_RANGE);
    boolean filterByRange = trackDbSettingClosestToHomeOn(tdb, varName);
    double minLimit=NO_VALUE,maxLimit=NO_VALUE;
    colonPairToDoubles(limitsDefault,&minLimit,&maxLimit);
    double minVal=minLimit,maxVal=maxLimit;
    colonPairToDoubles(setting,&minVal,&maxVal);
    getScoreFloatRangeFromCart(cart,tdb,scoreName,&minLimit,&maxLimit,&minVal,&maxVal);
    safef(varName, sizeof(varName), "%s.%s%s", name, scoreName, _MIN);
    safef(altLabel, sizeof(altLabel), "%s%s", (filterByRange?"Minimum ":""), label);
    cgiMakeDoubleVarWithLimits(varName,minVal, altLabel, 0,minLimit, maxLimit);
    if(filterByRange) // TODO: Test this range stuff which is not yet used
        {
        printf("<TD align='left'>to<TD align='left'>");
        safef(varName, sizeof(varName), "%s.%s%s", name, scoreName, _MAX);
        safef(altLabel, sizeof(altLabel), "%s%s", (filterByRange?"Maximum ":""), label);
        cgiMakeDoubleVarWithLimits(varName,maxVal, altLabel, 0,minLimit, maxLimit);
        }
    safef(altLabel, sizeof(altLabel), "%s", (filterByRange?"": "colspan=3"));
    if(minLimit != NO_VALUE && maxLimit != NO_VALUE)
        printf("<TD align='left'%s> (%g to %g)",altLabel,minLimit, maxLimit);
    else if(minLimit != NO_VALUE)
        printf("<TD align='left'%s> (minimum %g)",altLabel,minLimit);
    else if(maxLimit != NO_VALUE)
        printf("<TD align='left'%s> (maximum %g)",altLabel,maxLimit);
    else
        printf("<TD align='left'%s",altLabel);
    puts("</TR>");
    return TRUE;
    }
return FALSE;
}
struct dyString *dyAddFilterAsInt(struct cart *cart, struct trackDb *tdb,
       struct dyString *extraWhere,char *filter,char *defaultLimits, char*field, boolean *and)
/* creates the where clause condition to support numeric int filter range.
   Filters are expected to follow
        {fiterName}: trackDb min or min:max - default value(s);
        {filterName}Min or {filterName}: min (user supplied) cart variable;
        {filterName}Max: max (user supplied) cart variable;
        {filterName}Limits: trackDb allowed range "0:1000" Optional
           uses:{filterName}Min: old trackDb value if {filterName}Limits not found
                {filterName}Max: old trackDb value if {filterName}Limits not found
                defaultLimits: function param if no tdb limits settings found)
   The 'and' param and dyString in/out allows stringing multiple where clauses together */
{
char filterLimitName[64];
if(sameWord(filter,NO_SCORE_FILTER))
    safef(filterLimitName, sizeof(filterLimitName), "%s", NO_SCORE_FILTER);
else
    safef(filterLimitName, sizeof(filterLimitName), "%s%s", filter,_NO);
if(trackDbSettingClosestToHome(tdb, filterLimitName) != NULL)
    return extraWhere;

char *setting = NULL;
if(differentWord(filter,SCORE_FILTER))
    setting = trackDbSettingClosestToHome(tdb, filter);
else
    setting = trackDbSettingClosestToHomeOrDefault(tdb, filter,"0:1000");
if(setting || sameWord(filter,NO_SCORE_FILTER))
    {
    boolean invalid = FALSE;
    int minValueTdb = 0,maxValueTdb = NO_VALUE;
    colonPairToInts(setting,&minValueTdb,&maxValueTdb);
    int minLimit=NO_VALUE,maxLimit=NO_VALUE,min=minValueTdb,max=maxValueTdb;
    colonPairToInts(defaultLimits,&minLimit,&maxLimit);
    getScoreIntRangeFromCart(cart,tdb,filter,&minLimit,&maxLimit,&min,&max);
    if(minLimit != NO_VALUE || maxLimit != NO_VALUE)
        {
        // assume tdb default values within range! (don't give user errors that have no consequence)
        if((min != minValueTdb && ((minLimit != NO_VALUE && min < minLimit)
                                || (maxLimit != NO_VALUE && min > maxLimit)))
        || (max != maxValueTdb && ((minLimit != NO_VALUE && max < minLimit)
                                || (maxLimit != NO_VALUE && max > maxLimit))))
            {
            invalid = TRUE;
            char value[64];
            if(max == NO_VALUE) // min only is allowed, but max only is not
                safef(value, sizeof(value), "entered minimum (%d)", min);
            else
                safef(value, sizeof(value), "entered range (min:%d and max:%d)", min, max);
            char limits[64];
            if(minLimit != NO_VALUE && maxLimit != NO_VALUE)
                safef(limits, sizeof(limits), "violates limits (%d to %d)", minLimit, maxLimit);
            else if(minLimit != NO_VALUE)
                safef(limits, sizeof(limits), "violates lower limit (%d)", minLimit);
            else //if(maxLimit != NO_VALUE)
                safef(limits, sizeof(limits), "violates uppper limit (%d)", maxLimit);
            warn("invalid filter by %s: %s %s for track %s", field, value, limits, tdb->tableName);
            }
        }
    // else no default limits!
    if(invalid)
        {
        safef(filterLimitName, sizeof(filterLimitName), "%s%s", filter, (max!=NO_VALUE?_MIN:""));
        cartRemoveVariableClosestToHome(cart,tdb,FALSE,filterLimitName);
        safef(filterLimitName, sizeof(filterLimitName), "%s%s", filter, _MAX);
        cartRemoveVariableClosestToHome(cart,tdb,FALSE,filterLimitName);
        }
//#define FILTER_ASSUMES_RANGE_AT_LIMITS_IS_VALID_FILTER
#ifdef FILTER_ASSUMES_RANGE_AT_LIMITS_IS_VALID_FILTER
    else if((min != 0 && (int)min != NO_VALUE) || (int)max != NO_VALUE) // Assumes min==0 is no filter!
        {
        if((min != 0 && min != NO_VALUE) && max != NO_VALUE)
            dyStringPrintf(extraWhere, "%s(%s BETWEEN %d and %d)", (*and?" and ":""),field,min,max); // both min and max
        else if(min != 0 && min != NO_VALUE)
            dyStringPrintf(extraWhere, "%s(%s >= %d)", (*and?" and ":""),field,min);  // min only
        else //if(max != NO_VALUE)
            dyStringPrintf(extraWhere, "%s(%s <= %d)", (*and?" and ":""),field,max);  // max only
#else//ifndef FILTER_ASSUMES_RANGE_AT_LIMITS_IS_VALID_FILTER
    else if((min != NO_VALUE && (minLimit == NO_VALUE || minLimit != min))  // Assumes min==NO_VALUE or min==minLimit is no filter
         || (max != NO_VALUE && (maxLimit == NO_VALUE || maxLimit != max))) // Assumes max==NO_VALUE or max==maxLimit is no filter!
        {
        if(max == NO_VALUE || (maxLimit != NO_VALUE && maxLimit == max))
            dyStringPrintf(extraWhere, "%s(%s >= %d)", (*and?" and ":""),field,min);  // min only
        else if(min == NO_VALUE || (minLimit != NO_VALUE && minLimit == min))
            dyStringPrintf(extraWhere, "%s(%s <= %d)", (*and?" and ":""),field,max);  // max only
        else
            dyStringPrintf(extraWhere, "%s(%s BETWEEN %d and %d)", (*and?" and ":""),field,min,max); // both min and max
#endif//ndef FILTER_ASSUMES_RANGE_AT_LIMITS_IS_VALID_FILTER
        *and=TRUE;
        //warn("%s: %s",tdb->tableName,extraWhere->string);
        }
    }
return extraWhere;
}

struct dyString *dyAddFilterAsDouble(struct cart *cart, struct trackDb *tdb,
       struct dyString *extraWhere,char *filter,char *defaultLimits, char*field, boolean *and)
/* creates the where clause condition to support numeric double filters.
   Filters are expected to follow
        {fiterName}: trackDb min or min:max - default value(s);
        {filterName}Min or {filterName}: min (user supplied) cart variable;
        {filterName}Max: max (user supplied) cart variable;
        {filterName}Limits: trackDb allowed range "0.0:10.0" Optional
            uses:  defaultLimits: function param if no tdb limits settings found)
   The 'and' param and dyString in/out allows stringing multiple where clauses together */
{
char *setting = trackDbSettingClosestToHome(tdb, filter);
if(setting)
    {
    boolean invalid = FALSE;
    double minValueTdb = 0,maxValueTdb = NO_VALUE;
    colonPairToDoubles(setting,&minValueTdb,&maxValueTdb);
    double minLimit=NO_VALUE,maxLimit=NO_VALUE,min=minValueTdb,max=maxValueTdb;
    colonPairToDoubles(defaultLimits,&minLimit,&maxLimit);
    getScoreFloatRangeFromCart(cart,tdb,filter,&minLimit,&maxLimit,&min,&max);
    if((int)minLimit != NO_VALUE || (int)maxLimit != NO_VALUE)
        {
        // assume tdb default values within range! (don't give user errors that have no consequence)
        if((min != minValueTdb && (((int)minLimit != NO_VALUE && min < minLimit)
                                || ((int)maxLimit != NO_VALUE && min > maxLimit)))
        || (max != maxValueTdb && (((int)minLimit != NO_VALUE && max < minLimit)
                                || ((int)maxLimit != NO_VALUE && max > maxLimit))))
            {
            invalid = TRUE;
            char value[64];
            if((int)max == NO_VALUE) // min only is allowed, but max only is not
                safef(value, sizeof(value), "entered minimum (%g)", min);
            else
                safef(value, sizeof(value), "entered range (min:%g and max:%g)", min, max);
            char limits[64];
            if((int)minLimit != NO_VALUE && (int)maxLimit != NO_VALUE)
                safef(limits, sizeof(limits), "violates limits (%g to %g)", minLimit, maxLimit);
            else if((int)minLimit != NO_VALUE)
                safef(limits, sizeof(limits), "violates lower limit (%g)", minLimit);
            else //if((int)maxLimit != NO_VALUE)
                safef(limits, sizeof(limits), "violates uppper limit (%g)", maxLimit);
            warn("invalid filter by %s: %s %s for track %s", field, value, limits, tdb->tableName);
            }
        }
    if(invalid)
        {
        char filterLimitName[64];
        safef(filterLimitName, sizeof(filterLimitName), "%s%s", filter, _MIN);
        cartRemoveVariableClosestToHome(cart,tdb,FALSE,filterLimitName);
        safef(filterLimitName, sizeof(filterLimitName), "%s%s", filter, _MAX);
        cartRemoveVariableClosestToHome(cart,tdb,FALSE,filterLimitName);
        }
#ifdef FILTER_ASSUMES_RANGE_AT_LIMITS_IS_VALID_FILTER
    else if((min != 0 && (int)min != NO_VALUE) || (int)max != NO_VALUE) // Assumes min==0 is no filter!
        {
        if((min != 0 && (int)min != NO_VALUE) && (int)max != NO_VALUE)
            dyStringPrintf(extraWhere, "%s(%s BETWEEN %g and %g)", (*and?" and ":""),field,min,max); // both min and max
        else if(min != 0 && (int)min != NO_VALUE)
            dyStringPrintf(extraWhere, "%s(%s >= %g)", (*and?" and ":""),field,min);  // min only
        else //if((int)max != NO_VALUE)
            dyStringPrintf(extraWhere, "%s(%s <= %g)", (*and?" and ":""),field,max);  // max only
#else//ifndef FILTER_ASSUMES_RANGE_AT_LIMITS_IS_VALID_FILTER
    else if(((int)min != NO_VALUE && ((int)minLimit == NO_VALUE || minLimit != min))  // Assumes min==NO_VALUE or min==minLimit is no filter
         || ((int)max != NO_VALUE && ((int)maxLimit == NO_VALUE || maxLimit != max))) // Assumes max==NO_VALUE or max==maxLimit is no filter!
        {
        if((int)max == NO_VALUE || ((int)maxLimit != NO_VALUE && maxLimit == max))
            dyStringPrintf(extraWhere, "%s(%s >= %g)", (*and?" and ":""),field,min);  // min only
        else if((int)min == NO_VALUE || ((int)minLimit != NO_VALUE && minLimit == min))
            dyStringPrintf(extraWhere, "%s(%s <= %g)", (*and?" and ":""),field,max);  // max only
        else
            dyStringPrintf(extraWhere, "%s(%s BETWEEN %g and %g)", (*and?" and ":""),field,min,max); // both min and max
#endif//ndef FILTER_ASSUMES_RANGE_AT_LIMITS_IS_VALID_FILTER
        *and=TRUE;
        }
    }
return extraWhere;
}

void encodePeakCfgUi(struct cart *cart, struct trackDb *tdb, char *name, char *title, boolean boxed)
/* Put up UI for filtering wgEnocde peaks based on score, Pval and Qval */
{
boolean compositeLevel = isNameAtCompositeLevel(tdb,name);
boolean opened = FALSE;
showScoreFilter(cart,tdb,&opened,boxed,compositeLevel,name,title,"Minimum Q-Value (-log 10)",QVALUE_FILTER,NULL,NULL);//,"0.0",NULL);
showScoreFilter(cart,tdb,&opened,boxed,compositeLevel,name,title,"Minimum P-Value (-log 10)",PVALUE_FILTER,NULL,NULL);//,"0.0",NULL);
showScoreFilter(cart,tdb,&opened,boxed,compositeLevel,name,title,"Minimum Signal value",     SIGNAL_FILTER,NULL,NULL);//,"0.0",NULL);

char *setting = trackDbSettingClosestToHomeOrDefault(tdb, SCORE_FILTER,NULL);//"0:1000");
if(setting)
    {
    if(!opened)
        {
        boxed = cfgBeginBoxAndTitle(tdb, boxed, title);
        puts("<TABLE>");
        opened = TRUE;
        }
    char varName[256];
    int minLimit=0,maxLimit=1000,minVal=0,maxVal=NO_VALUE;
    colonPairToInts(setting,&minVal,&maxVal);
    getScoreIntRangeFromCart(cart,tdb,SCORE_FILTER,&minLimit,&maxLimit,&minVal,&maxVal);
    if(maxVal != NO_VALUE)
        puts("<TR><TD align='right'><B>Score range: min:</B><TD align='left'>");
    else
        puts("<TR><TD align='right'><B>Minimum score:</B><TD align='left'>");
    safef(varName, sizeof(varName), "%s%s", SCORE_FILTER, _BY_RANGE);
    boolean filterByRange = trackDbSettingClosestToHomeOn(tdb, varName);
    safef(varName, sizeof(varName), "%s.%s%s", name, SCORE_FILTER, (filterByRange?_MIN:""));
    cgiMakeIntVarWithLimits(varName, minVal, "Minimum score", 0, minLimit, maxLimit);
    if(filterByRange)
        {
        if(maxVal == NO_VALUE)
            maxVal = maxLimit;
        puts("<TD align='right'>to<TD align='left'>");
        safef(varName, sizeof(varName), "%s.%s%s", name, SCORE_FILTER,_MAX);
        cgiMakeIntVarWithLimits(varName, maxVal, "Maximum score", 0, minLimit, maxLimit);
        }
    printf("<TD align='left'%s> (%d to %d)",(filterByRange?"":" colspan=3"),minLimit, maxLimit);
    if(trackDbSettingClosestToHome(tdb, GRAY_LEVEL_SCORE_MIN) != NULL)
        {
        printf("<TR><TD align='right'colspan=5>");
        scoreGrayLevelCfgUi(cart, tdb, name, 1000);
        puts("</TR>");
        }
    }
if(opened)
    {
    puts("</TABLE>");
    cfgEndBox(boxed);
    }
}

void genePredCfgUi(struct cart *cart, struct trackDb *tdb, char *name, char *title, boolean boxed)
/* Put up gencode-specific controls */
{
char varName[64];
boolean compositeLevel = isNameAtCompositeLevel(tdb,name);
char *geneLabel = cartUsualStringClosestToHome(cart, tdb,compositeLevel, "label", "gene");

boxed = cfgBeginBoxAndTitle(tdb, boxed, title);

if (sameString(name, "acembly"))
    {
    char *acemblyClass = cartUsualStringClosestToHome(cart,tdb,compositeLevel,"type", acemblyEnumToString(0));
    printf("<p><b>Gene Class: </b>");
    acemblyDropDown("acembly.type", acemblyClass);
    printf("  ");
    }
else if(sameString("wgEncodeGencode", name)
     || sameString("wgEncodeSangerGencode", name)
     || (startsWith("encodeGencode", name) && !sameString("encodeGencodeRaceFrags", name)))
    {
    printf("<B>Label:</B> ");
    safef(varName, sizeof(varName), "%s.label", name);
    cgiMakeRadioButton(varName, "gene", sameString("gene", geneLabel));
    printf("%s ", "gene");
    cgiMakeRadioButton(varName, "accession", sameString("accession", geneLabel));
    printf("%s ", "accession");
    cgiMakeRadioButton(varName, "both", sameString("both", geneLabel));
    printf("%s ", "both");
    cgiMakeRadioButton(varName, "none", sameString("none", geneLabel));
    printf("%s ", "none");
    }

if(trackDbSettingClosestToHomeOn(tdb, "nmdFilter"))
    {
    boolean nmdDefault = FALSE;
    safef(varName, sizeof(varName), "hgt.%s.nmdFilter", name);
    nmdDefault = cartUsualBoolean(cart,varName, FALSE);  // TODO: var name (hgt prefix) needs changing before ClosesToHome can be used
    printf("<p><b>Filter out NMD targets.</b>");
    cgiMakeCheckBox(varName, nmdDefault);
    }

if(!sameString(tdb->tableName, "tigrGeneIndex")
&& !sameString(tdb->tableName, "ensGeneNonCoding")
&& !sameString(tdb->tableName, "encodeGencodeRaceFrags"))
    baseColorDrawOptDropDown(cart, tdb);

filterBy_t *filterBySet = filterBySetGet(tdb,cart,name);
if(filterBySet != NULL)
    {
    filterBySetCfgUi(tdb,filterBySet);
    filterBySetFree(&filterBySet);
    }
cfgEndBox(boxed);
}

static boolean isSpeciesOn(struct cart *cart, struct trackDb *tdb, char *species, char *option, int optionSize, boolean defaultState)
/* check the cart to see if species is turned off or on (default is defaultState) */
{
boolean ret = defaultState;
safef(option, optionSize, "%s.%s", tdb->tableName, species);

/* see if this is a simple multiz (not composite track) */
char *s = cartOptionalString(cart, option);
if (s != NULL)
    ret =  (sameString(s, "on") || atoi(s) > 0);
else
    {
    /* check parent to see if it has these variables */
    if (tdb->parent != NULL)
	{
	char *viewString;
	if (subgroupFind(tdb, "view", &viewString))
	    {
	    safef(option, optionSize, "%s.%s.%s",
		tdb->parent->tableName, viewString,  species);
	    ret = cartUsualBoolean(cart, option, ret);
	    }
	}
    }

return ret;
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
	wmSpecies->on = isSpeciesOn(cart, tdb, wmSpecies->name, option, sizeof option, TRUE);
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
struct hash *offHash = NULL;
if (defaultOffSpecies)
    {
    offHash = newHash(5);
    DEFAULT_BUTTON( "id", "default_pw","cb_maf_","_maf_");
    int wordCt = chopLine(defaultOffSpecies, words);
    defaultOffSpeciesCnt = wordCt;

    /* build hash of species that should be off */
    int ii;
    for(ii=0; ii < wordCt; ii++)
        hashAdd(offHash, words[ii], NULL);
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
	boolean defaultState = TRUE;
	if (offHash != NULL)
	    defaultState = (hashLookup(offHash, wmSpecies->name) == NULL);
        wmSpecies->on = isSpeciesOn(cart, tdb, wmSpecies->name, option, sizeof option, defaultState );
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

boxed = cfgBeginBoxAndTitle(tdb, boxed, title);

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
// At this point we need to search the cart to see if any others are already expanded.
// cart var of style "wgEncodeYaleChIPseq.Peaks.showCfg" {parentTable}.{view}.showCfg value='on'
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

struct trackDb *rFindSubtrackWithView(struct trackDb *parentTdb, char *viewName)
/* Find a descendent with given view. */
{
struct trackDb *subtrack;
for (subtrack = parentTdb->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    char *stView;
    if(subgroupFind(subtrack,"view",&stView))
        {
	if (sameString(stView, viewName))
	    return subtrack;
	}
    struct trackDb *subSub = rFindSubtrackWithView(subtrack, viewName);
    if (subSub != NULL)
        return subSub;
    }
return NULL;
}

static boolean hCompositeDisplayViewDropDowns(char *db, struct cart *cart, struct trackDb *parentTdb)
/* UI for composite view drop down selections. */
{
uglyf("hCompositeDisplayViewDropDowns(db=%s, cart=%p, parentTdb=%s)<BR>\n", db, cart, parentTdb->tableName);
uglyf("parentTdb=%s of %d kids<BR>\n", parentTdb->tableName, slCount(parentTdb->subtracks));
int ix;
struct trackDb *subtrack;
char objName[SMALLBUF];
char classes[SMALLBUF];
char javascript[JBUFSIZE];
#define CFG_LINK  "<B><A NAME=\"a_cfg_%s\"></A><A id='a_cfg_%s' HREF=\"#a_cfg_%s\" onclick=\"return (showConfigControls('%s') == false);\" title=\"%s Configuration\">%s</A><INPUT TYPE=HIDDEN NAME='%s.%s.showCfg' value='%s'></B>\n"
#define MAKE_CFG_LINK(name,title,tbl,open) printf(CFG_LINK, (name),(name),(name),(name),(title),(title),(tbl),(name),((open)?"on":"off"))

members_t *membersOfView = subgroupMembersGet(parentTdb,"view");
uglyf("membersOfView = %p<BR>\n", membersOfView);
if(membersOfView == NULL)
    return FALSE;
uglyf("membersOfView->count = %d<BR>\n", membersOfView->count);

char configurable[membersOfView->count];
memset(configurable,cfgNone,sizeof(configurable));
int firstOpened = -1;
boolean makeCfgRows = FALSE;
struct trackDb **matchedSubtracks = needMem(sizeof(struct trackDb *)*membersOfView->count);
// char *setting = trackDbSetting(parentTdb,"settingsByView");
// boolean blockCfgs = (setting != NULL && (sameWord(setting,"off") || sameWord(setting,"disabled") || sameWord(setting,"false")));

for (ix = 0; ix < membersOfView->count; ix++)
    {
    char *viewName = membersOfView->names[ix];
    uglyf("parentTdb=%s with %d children looking for view %s<BR>\n", parentTdb->tableName, slCount(parentTdb->subtracks), viewName);
    subtrack = rFindSubtrackWithView(parentTdb, viewName);
    if (subtrack != NULL)
	{
	uglyf("subtrack %s matches view %s<BR>\n", subtrack->tableName, viewName);
	matchedSubtracks[ix] = subtrack;
	configurable[ix] = (char)cfgTypeFromTdb(subtrack,TRUE); // Warns if not multi-view compatible
	if(configurable[ix] != cfgNone)
	    {
	    if(firstOpened == -1)
		{
		safef(objName, sizeof(objName), "%s.%s.showCfg", parentTdb->tableName,membersOfView->names[ix]);
		if(cartUsualBoolean(cart,objName,FALSE))
		    firstOpened = ix;
		}
	    makeCfgRows = TRUE;
	    }
	}
    }

toLowerN(membersOfView->title, 1);
printf("<B>Select %s </B>(<A HREF=\"../goldenPath/help/multiView.html\" title='Help on views' TARGET=_BLANK>help</A>):<BR>\n", membersOfView->title);
puts("<TABLE><TR align=\"LEFT\">");
for (ix = 0; ix < membersOfView->count; ix++)
    {
    if(matchedSubtracks[ix] != NULL)
	{
	printf("<TD>");
	if(configurable[ix] != cfgNone)
	    {
	    MAKE_CFG_LINK(membersOfView->names[ix],membersOfView->values[ix],parentTdb->tableName,(firstOpened == ix));
	    }
	else
	    printf("<B>%s</B>\n",membersOfView->values[ix]);
	puts("</TD>");

	safef(objName, sizeof(objName), "%s.%s.vis", parentTdb->tableName,membersOfView->names[ix]);
	enum trackVisibility tv =
	    hTvFromString(cartUsualString(cart, objName,hStringFromTv(visCompositeViewDefault(parentTdb,membersOfView->names[ix]))));

	safef(javascript, sizeof(javascript), "onchange=\"matSelectViewForSubTracks(this,'%s');\"", membersOfView->names[ix]);

	printf("<TD>");
	safef(classes, sizeof(classes), "viewDD normalText %s", membersOfView->names[ix]);
	hTvDropDownClassWithJavascript(objName, tv, parentTdb->canPack,classes,javascript);
	puts(" &nbsp; &nbsp; &nbsp;</TD>");
	// Until the cfg boxes are inserted here, this divorces the relationship
	//if(membersOfView->count > 6 && ix == ((membersOfView->count+1)/2)-1)  // An attempt at 2 rows of cfg's No good!
	//    puts("</tr><tr><td>&nbsp;</td></tr><tr>");
	}
    }
// Need to do the same for ENCODE Gencode 'filterBy's
puts("</TR>");
if(makeCfgRows)
    {
    puts("</TABLE><TABLE>");
    for (ix = 0; ix < membersOfView->count; ix++)
	{
	if(matchedSubtracks[ix] != NULL)
	    {
	    printf("<TR id=\"tr_cfg_%s\"",membersOfView->names[ix]);
	    if((firstOpened == -1 && !compositeViewCfgExpandedByDefault(parentTdb,membersOfView->names[ix],NULL))
	    || (firstOpened != -1 && firstOpened != ix))
		printf(" style=\"display:none\"");
	    printf("><TD width=10>&nbsp;</TD>");
	    int ix2=ix;
	    while(0 < ix2--)
		printf("<TD width=100>&nbsp;</TD>");
	    printf("<TD colspan=%d>",membersOfView->count+1);
	    safef(objName, sizeof(objName), "%s.%s", parentTdb->tableName,membersOfView->names[ix]);
	    if(configurable[ix] != cfgNone)
		{
		cfgByCfgType(configurable[ix],db,cart,matchedSubtracks[ix],objName,membersOfView->values[ix],TRUE);
		cfgLinkToDependentCfgs(parentTdb,objName);
		}
	    }
	}
    }
puts("</TABLE><BR>");
subgroupMembersFree(&membersOfView);
freeMem(matchedSubtracks);
return TRUE;
}

static char *labelWithVocabLink(struct trackDb *parentTdb, struct trackDb *childTdb, char *vocabType, char *label)
/* If the parentTdb has a controlledVocabulary setting and the vocabType is found,
   then label will be wrapped with the link to display it.  Return string is cloned. */
{
char *vocab = trackDbSetting(parentTdb, "controlledVocabulary");
if(vocab == NULL)
    return cloneString(label); // No wrapping!

char *words[15];
int count,ix;
boolean found=FALSE;
if((count = chopByWhite(cloneString(vocab), words,15)) <= 1)
    return cloneString(label);

char *suffix=NULL;
char *rootLabel = labelRoot(label,&suffix);

for(ix=1;ix<count && !found;ix++)
    {
#define VOCAB_LINK "<A HREF='hgEncodeVocab?ra=/usr/local/apache/cgi-bin/%s&term=\"%s\"' title='%s details' TARGET=ucscVocab>%s</A>"
    if(sameString(vocabType,words[ix])) // controlledVocabulary setting matches tag so all labels are linked
        {
        int sz=strlen(VOCAB_LINK)+strlen(words[0])+strlen(words[ix])+2*strlen(label) + 2;
        char *link=needMem(sz);
        safef(link,sz,VOCAB_LINK,words[0],words[ix],rootLabel,rootLabel);
        if(suffix)
            safecat(link,sz,suffix);
        freeMem(words[0]);
        return link;
        }
    else if(countChars(words[ix],'=') == 1 && childTdb != NULL) // The name of a trackDb setting follows and will be the controlled vocab term
        {
        strSwapChar(words[ix],'=',0);
        if(sameString(vocabType,words[ix]))  // tags match, but search for term
            {
            char * cvSetting = words[ix] + strlen(words[ix]) + 1;
            char * cvTerm = metadataSettingFind(childTdb, cvSetting);
            if(cvTerm != NULL)
                {
                char *encodedTerm = cgiEncode(cvTerm);
                int sz=strlen(VOCAB_LINK)+strlen(words[0])+strlen(encodedTerm)+2*strlen(label) + 2;
                char *link=needMem(sz);
                safef(link,sz,VOCAB_LINK,words[0],encodedTerm,cvTerm,rootLabel);
                if(suffix)
                    safecat(link,sz,suffix);
                freeMem(words[0]);
                freeMem(cvTerm);
                freeMem(encodedTerm);
                return link;
                }
            }
        }
    }
freeMem(words[0]);
freeMem(rootLabel);
return cloneString(label);
}

#define PM_BUTTON_UC "<IMG height=18 width=18 onclick=\"return (matSetMatrixCheckBoxes(%s%s%s%s%s%s) == false);\" id='btn_%s' src='../images/%s'>"
static void buttonsForAll()
{
printf(PM_BUTTON_UC,"true", "", "", "", "", "",  "plus_all",    "add_sm.gif");
printf(PM_BUTTON_UC,"false","", "", "", "", "", "minus_all", "remove_sm.gif");
}
static void buttonsForOne(char *name,char *class)
{
printf(PM_BUTTON_UC, "true",  ",'", class, "'", "", "", name,    "add_sm.gif");
printf(PM_BUTTON_UC, "false", ",'", class, "'", "", "", name, "remove_sm.gif");
}

#define MATRIX_RIGHT_BUTTONS_AFTER 8
#define MATRIX_BOTTOM_BUTTONS_AFTER 20
static void matrixXheadingsRow1(struct trackDb *parentTdb, members_t *dimensionX,members_t *dimensionY,struct trackDb **tdbsX,boolean top)
/* prints the top row of a matrix: 'All' buttons; X titles; buttons 'All' */
{
printf("<TR ALIGN=CENTER BGCOLOR='%s' valign=%s>\n",COLOR_BG_ALTDEFAULT,top?"BOTTOM":"TOP");
if(dimensionX && dimensionY)
    {
    printf("<TH ALIGN=LEFT valign=%s>",top?"TOP":"BOTTOM");
    buttonsForAll();
    puts("&nbsp;All</TH>");
    }

// If there is an X dimension, then titles go across the top
if(dimensionX)
    {
    int ixX,cntX=0;
    if(dimensionY)
        printf("<TH align=RIGHT><EM><B>%s</EM></B></TH>", dimensionX->title);
    else
        printf("<TH ALIGN=RIGHT valign=%s>&nbsp;&nbsp;<EM><B>%s</EM></B></TH>",top?"TOP":"BOTTOM", dimensionX->title);
    for (ixX = 0; ixX < dimensionX->count; ixX++)
        {
        if(tdbsX[ixX] != NULL)
            {
            char *label = replaceChars(dimensionX->values[ixX]," (","<BR>(");//
            printf("<TH WIDTH='60'>&nbsp;%s&nbsp;</TH>",labelWithVocabLink(parentTdb,tdbsX[ixX],dimensionX->tag,label));
            freeMem(label);
            cntX++;
            }
        }
    // If dimension is big enough, then add Y buttons to righ as well
    if(cntX>MATRIX_RIGHT_BUTTONS_AFTER)
        {
        if(dimensionY)
            {
            printf("<TH align=LEFT><EM><B>%s</EM></B></TH>", dimensionX->title);
            printf("<TH ALIGN=RIGHT valign=%s>All&nbsp;",top?"TOP":"BOTTOM");
            buttonsForAll();
            puts("</TH>");
            }
        else
            printf("<TH ALIGN=LEFT valign=%s><EM><B>%s</EM></B>&nbsp;&nbsp;</TH>",top?"TOP":"BOTTOM", dimensionX->title);
        }
    }
else if(dimensionY)
    {
    printf("<TH ALIGN=RIGHT WIDTH=100 nowrap>");
    printf("<EM><B>%s</EM></B>", dimensionY->title);
    printf("</TH><TH ALIGN=CENTER WIDTH=60>");
    buttonsForAll();
    puts("</TH>");
    }
puts("</TR>\n");
}

static void matrixXheadingsRow2(struct trackDb *parentTdb, members_t *dimensionX,members_t *dimensionY,struct trackDb **tdbsX)
/* prints the 2nd row of a matrix: Y title; X buttons; title Y */
{
// If there are both X and Y dimensions, then there is a row of buttons in X
if(dimensionX && dimensionY)
    {
    int ixX,cntX=0;
    printf("<TR ALIGN=CENTER BGCOLOR=\"%s\"><TH ALIGN=CENTER colspan=2><EM><B>%s</EM></B></TH>",COLOR_BG_ALTDEFAULT, dimensionY->title);
    for (ixX = 0; ixX < dimensionX->count; ixX++)    // Special row of +- +- +-
        {
        if(tdbsX[ixX] != NULL)
            {
            char objName[SMALLBUF];
            puts("<TD>");
            safef(objName, sizeof(objName), "plus_%s_all", dimensionX->names[ixX]);
            buttonsForOne( objName, dimensionX->names[ixX] );
            puts("</TD>");
            cntX++;
            }
        }
    // If dimension is big enough, then add Y buttons to righ as well
    if(cntX>MATRIX_RIGHT_BUTTONS_AFTER)
        printf("<TH ALIGN=CENTER colspan=2><EM><B>%s</EM></B></TH>", dimensionY->title);
    puts("</TR>\n");
    }
}

static void matrixXheadings(struct trackDb *parentTdb, members_t *dimensionX,members_t *dimensionY,struct trackDb **tdbsX,boolean top)
/* UI for X headings in matrix */
{
if(top)
    matrixXheadingsRow1(parentTdb,dimensionX,dimensionY,tdbsX,top);

    matrixXheadingsRow2(parentTdb,dimensionX,dimensionY,tdbsX);

if(!top)
    matrixXheadingsRow1(parentTdb,dimensionX,dimensionY,tdbsX,top);
}

static void matrixYheadings(struct trackDb *parentTdb, members_t *dimensionX,members_t *dimensionY,int ixY,struct trackDb *childTdb,boolean left)
/* prints the top row of a matrix: 'All' buttons; X titles; buttons 'All' */
{
if(dimensionX && dimensionY && childTdb != NULL) // Both X and Y, then column of buttons
    {
    char objName[SMALLBUF];
    printf("<TH ALIGN=%s nowrap colspan=2>",left?"RIGHT":"LEFT");
    if(left)
        printf("%s&nbsp;",labelWithVocabLink(parentTdb,childTdb,dimensionY->tag,dimensionY->values[ixY]));
    safef(objName, sizeof(objName), "plus_all_%s", dimensionY->names[ixY]);
    buttonsForOne( objName, dimensionY->names[ixY] );
    if(!left)
        printf("&nbsp;%s",labelWithVocabLink(parentTdb,childTdb,dimensionY->tag,dimensionY->values[ixY]));
    puts("</TH>");
    }
else if (dimensionX)
    {
    printf("<TH ALIGN=%s>",left?"RIGHT":"LEFT");
    buttonsForAll();
    puts("</TH>");
    }
else if (left && dimensionY && childTdb != NULL)
    printf("<TH ALIGN=RIGHT nowrap>%s</TH>\n",labelWithVocabLink(parentTdb,childTdb,dimensionY->tag,dimensionY->values[ixY]));
}

static int displayABCdimensions(struct cart *cart, struct trackDb *parentTdb,int expected)
/* This will walk through all declared nonX&Y dimensions (X and Y is the 2D matrix of CBs.
   NOTE: ABC dims are only supported if there are X & Y both.  Also expected number should be passed in */
{
int count=0,ix;
for(ix=0;ix<26;ix++)
    {
    char dimLetter = 'A' + ix;
    if(dimLetter == 'X' || dimLetter == 'Y')
        continue;

    members_t *dim = subgroupMembersGetByDimension(parentTdb,dimLetter);
    if(dim==NULL)
        continue;
    if(dim->count<1)
        {
        subgroupMembersFree(&dim);
        continue;
        }
    count++;
    int cells[dim->count];         // The Z dimension is a separate 1D matrix
    struct trackDb *tdbs[dim->count];
    memset(cells, 0, sizeof(cells));
    memset(tdbs,  0, sizeof(tdbs));

    int aIx;
    struct trackDb *subtrack;
    for (subtrack = parentTdb->subtracks; subtrack != NULL; subtrack = subtrack->next)
        {
        char *value;
        if(subgroupFind(subtrack,dim->tag,&value))
            {
            aIx = stringArrayIx(value,dim->names,dim->count);
            cells[aIx]++;
            tdbs[aIx] = subtrack;
            subgroupFree(&value);
            }
        }

    if(count==1) // First time set up a table
        puts("<BR><TABLE>");
    printf("<TR><TH valign=top align='right'>&nbsp;&nbsp;<B><EM>%s</EM></B>:</TH>",dim->title);
    char settingName[32];
    safef(settingName,sizeof(settingName),"dimension%cchecked",dimLetter);
    char *dimCheckedDefaults = trackDbSettingOrDefault(parentTdb,settingName,"");
    boolean alreadySet=FALSE;
    for(aIx=0;aIx<dim->count;aIx++)
        {
        if(tdbs[aIx] != NULL && cells[aIx]>0)
            {
            printf("<TH align=left nowrap>");
            char objName[SMALLBUF];
            char javascript[JBUFSIZE];
            safef(objName, sizeof(objName), "%s.mat_%s_dim%c_cb",parentTdb->tableName,dim->names[aIx],dimLetter);
            alreadySet=(NULL!=findWordByDelimiter(dim->names[aIx],',',dimCheckedDefaults));
            alreadySet=cartUsualBoolean(cart,objName,alreadySet);
            safef(javascript,sizeof(javascript),"onclick='matCbClick(this);' class=\"matCB abc %s\"",dim->names[aIx]);
            // TODO Set classes properly (if needed!!!)  The classes could be
            // a) matCB to keep the list of all
            // b) dimZ or some other designator but the current dimZ would work
            // c) a new one (subgroup tag?) to break dimZs into subsets.  But how to recognize the subsets?  Will js need to pick by name?
            cgiMakeCheckBoxJS(objName,alreadySet,javascript);
            printf("%s",labelWithVocabLink(parentTdb,tdbs[aIx],dim->tag,dim->values[aIx]));
            puts("</TH>");
            }
        }
    puts("</TR>");
    subgroupMembersFree(&dim);
    if(count==expected)
        break;
    }
if(count>0)
    puts("</TABLE>");
return count;
}


static boolean hCompositeUiByMatrix(char *db, struct cart *cart, struct trackDb *parentTdb, char *formName)
/* UI for composite tracks: matrix of checkboxes. */
{
//int ix;
char objName[SMALLBUF];
char javascript[JBUFSIZE];
struct trackDb *subtrack;

dimensions_t *dims = dimensionSettingsGet(parentTdb);
if(dims == NULL)
    return FALSE;

int ixX,ixY;
members_t *dimensionX = subgroupMembersGetByDimension(parentTdb,'X');
members_t *dimensionY = subgroupMembersGetByDimension(parentTdb,'Y');
if(dimensionX == NULL && dimensionY == NULL) // Must be an X or Y dimension
    return FALSE;

// use array of char determine all the cells (in X,Y,Z dimensions) that are actually populated
char *value;
int sizeOfX = dimensionX?dimensionX->count:1;
int sizeOfY = dimensionY?dimensionY->count:1;
int cells[sizeOfX][sizeOfY]; // There needs to be atleast one element in dimension
struct trackDb *tdbsX[sizeOfX]; // Representative subtracks
struct trackDb *tdbsY[sizeOfY];
memset(cells, 0, sizeof(cells));
memset(tdbsX, 0, sizeof(tdbsX));
memset(tdbsY, 0, sizeof(tdbsY));
for (subtrack = parentTdb->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    ixX = (dimensionX ? -1 : 0 );
    ixY = (dimensionY ? -1 : 0 );
    if(dimensionX && subgroupFind(subtrack,dimensionX->tag,&value))
        {
        ixX = stringArrayIx(value,dimensionX->names,dimensionX->count);
        tdbsX[ixX] = subtrack;
        subgroupFree(&value);
        }
    if(dimensionY && subgroupFind(subtrack,dimensionY->tag,&value))
        {
        ixY = stringArrayIx(value,dimensionY->names,dimensionY->count);
        tdbsY[ixY] = subtrack;
        subgroupFree(&value);
        }
    if(ixX > -1 && ixY > -1)
        cells[ixX][ixY]++;
    }

//puts("<B>Select subtracks by characterization:</B><BR>");
printf("<B>Select subtracks by ");
if(dimensionX && !dimensionY)
    safef(javascript, sizeof(javascript), "%s:</B>",dimensionX->title);
else if(!dimensionX && dimensionY)
    safef(javascript, sizeof(javascript), "%s:</B>",dimensionY->title);
else if(dims->count == 2)
    safef(javascript, sizeof(javascript), "%s and %s:</B>",dimensionX->title,dimensionY->title);
else
    safef(javascript, sizeof(javascript), "multiple variables:</B>");
puts(strLower(javascript));

if(!subgroupingExists(parentTdb,"view"))
    puts("(<A HREF=\"../goldenPath/help/multiView.html\" title='Help on views' TARGET=_BLANK>help</A>)\n");

puts("<BR>\n");

if(dims->count > 2)
    displayABCdimensions(cart,parentTdb,(dims->count - 2));  // No dimABCs without X & Y both
dimensionsFree(&dims);

printf("<TABLE class='greenBox' bgcolor='%s' borderColor='%s'>\n",COLOR_BG_DEFAULT,COLOR_BG_DEFAULT);

matrixXheadings(parentTdb,dimensionX,dimensionY,tdbsX,TRUE);

// Now the Y by X matrix
int cntX=0,cntY=0;
for (ixY = 0; ixY < sizeOfY; ixY++)
    {
    if(tdbsY[ixY] != NULL || dimensionY == NULL)
        {
        cntY++;
        assert(!dimensionY || ixY < dimensionY->count);
        printf("<TR ALIGN=CENTER BGCOLOR=\"#FFF9D2\">");

        matrixYheadings(parentTdb, dimensionX,dimensionY,ixY,tdbsY[ixY],TRUE);

#define MAT_CB_SETUP "<INPUT TYPE=CHECKBOX NAME='%s' VALUE=on %s>"
#define MAT_CB(name,js) printf(MAT_CB_SETUP,(name),(js));
        for (ixX = 0; ixX < sizeOfX; ixX++)
            {
            if(tdbsX[ixX] != NULL || dimensionX == NULL)
                {
                assert(!dimensionX || ixX < dimensionX->count);
                if(cntY==1) // Only do this on the first good Y
                    cntX++;

                if(dimensionX && ixX == dimensionX->count)
                    break;
                if(cells[ixX][ixY] > 0)
                    {
                    struct dyString *dyJS = dyStringCreate("onclick='matCbClick(this);'");
                    if(dimensionX && dimensionY)
                        {
                        safef(objName, sizeof(objName), "mat_%s_%s_cb", dimensionX->names[ixX],dimensionY->names[ixY]);
                        }
                    else
                        {
                        safef(objName, sizeof(objName), "mat_%s_cb", (dimensionX ? dimensionX->names[ixX] : dimensionY->names[ixY]));
                        }
                    puts("<TD>");
                    dyStringPrintf(dyJS, " class=\"matCB");
                    if(dimensionX)
                        dyStringPrintf(dyJS, " %s",dimensionX->names[ixX]);
                    if(dimensionY)
                        dyStringPrintf(dyJS, " %s",dimensionY->names[ixY]);
                    dyStringAppendC(dyJS,'"');
                    MAT_CB(objName,dyStringCannibalize(&dyJS)); // X&Y are set by javascript page load
                    puts("</TD>");
                    }
                else
                    puts("<TD>&nbsp;</TD>");
                }
            }
        if(dimensionX && cntX>MATRIX_RIGHT_BUTTONS_AFTER)
            matrixYheadings(parentTdb, dimensionX,dimensionY,ixY,tdbsY[ixY],FALSE);
        puts("</TR>\n");
        }
    }
if(dimensionY && cntY>MATRIX_BOTTOM_BUTTONS_AFTER)
    matrixXheadings(parentTdb,dimensionX,dimensionY,tdbsX,FALSE);

puts("</TD></TR></TABLE>");

subgroupMembersFree(&dimensionX);
subgroupMembersFree(&dimensionY);
puts("<BR>\n");

return TRUE;
}

static boolean hCompositeUiAllButtons(char *db, struct cart *cart, struct trackDb *parentTdb, char *formName)
/* UI for composite tracks: all/none buttons only (as opposed to matrix or lots of buttons */
{
if(slCount(parentTdb->subtracks) <= 1)
    return FALSE;

if(dimensionsExist(parentTdb))
    return FALSE;

#define PM_BUTTON_GLOBAL "<IMG height=18 width=18 onclick=\"matSubCBsCheck(%s);\" id='btn_%s' src='../images/%s'>"
#define    BUTTON_PLUS_ALL_GLOBAL()  printf(PM_BUTTON_GLOBAL,"true",  "plus_all",   "add_sm.gif")
#define    BUTTON_MINUS_ALL_GLOBAL() printf(PM_BUTTON_GLOBAL,"false","minus_all","remove_sm.gif")
printf("<P><B>Select subtracks:</B><P>All:&nbsp;");
BUTTON_PLUS_ALL_GLOBAL();
BUTTON_MINUS_ALL_GLOBAL();
puts("</P>");
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
            if (hSameTrackDbType(primaryType, subtrack->type))
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
    if(sameWord(subGroup,"view"))
        continue;  // Multi-view should have taken care of "view" subgroup already
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
                    safef(option, sizeof(option),"%s_sel", subtrack->tableName);
                    newVal = sameString(button, ADD_BUTTON_LABEL);
                    if (primarySubtrack)
                        {
                        if (sameString(subtrack->tableName, primarySubtrack))
                            newVal = TRUE;
                        if (hSameTrackDbType(primaryType, subtrack->type))
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
boolean viewsOnly = FALSE;

// (void)parentTdbAbandonTablelessChildren(db,tdb);

if(trackDbSetting(tdb, "dragAndDrop") != NULL)
    jsIncludeFile("jquery.tablednd.js", NULL);
jsIncludeFile("hui.js",NULL);

puts("<P>");
if (slCount(tdb->subtracks) < MANY_SUBTRACKS && !hasSubgroups)
    {
    compositeUiAllSubtracks(db, cart, tdb, primarySubtrack);
    return;
    }
uglyf("OK 2<BR>\n");
if (fakeSubmit)
    cgiMakeHiddenVar(fakeSubmit, "submit");

if(primarySubtrack == NULL)
    {
    uglyf("OK 2.1 primarySubtrack branch<BR>\n");
    if(subgroupingExists(tdb,"view"))
        {
	uglyf("OK 2.2 view branch<BR>\n");
        hCompositeDisplayViewDropDowns(db, cart,tdb);
        if(subgroupCount(tdb) <= 1)
            viewsOnly = TRUE;
        }
    if(!viewsOnly)
        {
	uglyf("OK 2.3 viewsOnly branch<BR>\n");
        if(trackDbSettingOn(tdb, "allButtonPair"))
	    {
	    uglyf("OK 2.3.1 allButtonPair<BR>\n");
            hCompositeUiAllButtons(db, cart, tdb, formName);
	    }
        else if (!hasSubgroups || !isMatrix || primarySubtrack)
	    {
	    uglyf("OK 2.3.2 !hasSubroups || !isMatrix || primarySubtrack<BR>\n");
            hCompositeUiNoMatrix(db, cart,tdb,primarySubtrack,formName);
	    }
        else
	    {
	    uglyf("OK 2.3.3 hCompositeUiByMatrix<BR>\n");
            hCompositeUiByMatrix(db, cart, tdb, formName);
	    }
        }
    }
uglyf("OK 3<BR>\n");

cartSaveSession(cart);
cgiContinueHiddenVar("g");

if(displayAll)
    compositeUiAllSubtracks(db, cart, tdb, primarySubtrack);
else
    compositeUiSelectedSubtracks(db, cart, tdb, primarySubtrack);

uglyf("OK 4<BR>\n");
if (primarySubtrack == NULL)  // primarySubtrack is set for tableBrowser but not hgTrackUi
    {
        if (slCount(tdb->subtracks) > 5)
        {
        cgiMakeButton("Submit", "Submit");
        puts("<P>");
        }
    }
uglyf("OK 5<BR>\n");
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
// This routine should give these results: compositeName.viewName or else subtrackName.viewName or else compositeName or else subtrackName
if(tdbIsCompositeChild(tdb) == TRUE && trackDbSetting(tdb, "subTrack") != NULL)
    {
    if(trackDbSettingClosestToHomeOn(tdb, "configurable"))
        rootName = tdb->tableName;  // subtrackName
    else
        rootName = firstWordInLine(cloneString(trackDbSetting(tdb, "subTrack"))); // compositeName
    }
if(rootName != NULL)
    {
    if(subgroupFind(tdb,"view",&stView))
        {
        int len = strlen(rootName) + strlen(stView) + 3;
        name = needMem(len);
        safef(name,len,"%s.%s",rootName,stView);
        subgroupFree(&stView);
        }
    else
        name = cloneString(rootName);
    }
else
    name = cloneString(tdb->tableName);
return name;
}
void compositeViewControlNameFree(char **name)
/* frees a string allocated by compositeViewControlNameFromTdb */
{
if(name && *name)
    freez(name);
}

boolean isNameAtCompositeLevel(struct trackDb *tdb,char *name)
/* cfgUi controls are passed a prefix name that may be at the composite or at the subtrack level
   returns TRUE for composite level name */
{
if(tdbIsCompositeChild(tdb)
&& startsWith(tdb->parent->tableName,name)
&& name[strlen(tdb->parent->tableName)] == '.')  // Cfg name at composite view level
    return TRUE;
return FALSE;
}

boolean chainDbNormScoreAvailable(struct trackDb *tdb)
/*	check if normScore column is specified in trackDb as available */
{
boolean normScoreAvailable = FALSE;
char * normScoreTest =
     trackDbSettingClosestToHomeOrDefault(tdb, "chainNormScoreAvailable", "no");
if (differentWord(normScoreTest, "no"))
        normScoreAvailable = TRUE;

return normScoreAvailable;
}
