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
#include "mdb.h"
#include "web.h"
#include "hPrint.h"
#include "fileUi.h"
#include "bigBed.h"
#include "bigWig.h"
#include "vcfUi.h"

static char const rcsid[] = "$Id: hui.c,v 1.297 2010/06/02 19:27:51 tdreszer Exp $";

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

//#define SUBTRACK_CFG_POPUP

struct trackDb *wgEncodeDownloadDirKeeper(char *db, struct trackDb *tdb, struct hash *trackHash)
/* Look up through self and parents, looking for someone responsible for handling
 * where the downloads are. */
{
if (!tdbIsDownloadsOnly(tdb) && !sameString(tdb->table, tdb->track) && trackHash)
    {
    tdb = hashFindVal(trackHash, tdb->table);
    if (tdb == NULL)
        errAbort("Can't find track for table %s in wgEncodeDownloadDirKeeper", tdb->table);
    }
return trackDbTopLevelSelfOrParent(tdb);
}

static char *htmlStringForDownloadsLink(char *database, struct trackDb *tdb,char *name,boolean nameIsFile,
        struct hash *trackHash)
// Returns an HTML string for a downloads link
{
// If has fileSortOrder, then link to new hgFileUi
if (!nameIsFile && trackDbSetting(tdb, FILE_SORT_ORDER) != NULL)
    {
    char * link = needMem(PATH_LEN); // 512 should be enough
    safef(link,PATH_LEN,"<A HREF='%s?db=%s&g=%s' title='Downloadable Files...'>%s</A>", //  NOTE: TARGET=ucscDownloads   ??
        hgFileUiName(), database, /*cartSessionVarName(), cartSessionId(cart),*/ tdb->track, name); // Note the hgsid would be needed if downloads page ever saved fileSortOrder to cart.
    return link;
    }
else if(trackDbSetting(tdb, "wgEncode") != NULL)  // Downloads directory if this is ENCODE
    {
    struct trackDb *dirKeeper = wgEncodeDownloadDirKeeper(database, tdb, trackHash);
    char *compositeDir = (sameWord(dirKeeper->type,"downloadsOnly")?dirKeeper->track:dirKeeper->table);
    struct dyString *dyLink = dyStringCreate("<A HREF=\"http://%s/goldenPath/%s/%s/%s/%s\" title='Download %s' TARGET=ucscDownloads>%s</A>",
            hDownloadsServer(),
            trackDbSettingOrDefault(dirKeeper, "origAssembly",database),  // This may not be wise!!!
            ENCODE_DCC_DOWNLOADS, compositeDir, (nameIsFile?name:""), nameIsFile?"file":"files",name);
    return dyStringCannibalize(&dyLink);
    }
return NULL;
}

static boolean makeNamedDownloadsLink(char *database, struct trackDb *tdb,char *name,
	struct hash *trackHash)
// Make a downloads link (if appropriate and then returns TRUE)
{
char *htmlString = htmlStringForDownloadsLink(database,trackDbTopLevelSelfOrParent(tdb),name,FALSE,trackHash);
if (htmlString == NULL)
    return FALSE;

printf("%s", htmlString);
freeMem(htmlString);
return TRUE;
}

boolean makeDownloadsLink(char *database, struct trackDb *tdb, struct hash *trackHash)
// Make a downloads link (if appropriate and then returns TRUE)
{
return makeNamedDownloadsLink(database, tdb,"Downloads", trackHash);
}

void makeTopLink(struct trackDb *tdb)
/* Link to top of UI page */
{
if (trackDbSetting(tdb, "dimensions"))
    {
    char *upArrow = "&uArr;";
    enum browserType browser = cgiBrowser();
    if (browser == btIE || browser == btFF)
        upArrow = "&uarr;";
    // Note: the nested spans are so that javascript can determine position and selectively display the link when appropriate
    printf("<span class='navUp' style='float:right; display:none'>&nbsp;&nbsp;<A HREF='#' TITLE='Return to top of page'>Top%s</A></span>",upArrow);
    }
}

boolean makeSchemaLink(char *db,struct trackDb *tdb,char *label)
// Make a table schema link (if appropriate and then returns TRUE)
{
#define SCHEMA_LINKED "<A HREF=\"../cgi-bin/hgTables?db=%s&hgta_group=%s&hgta_track=%s&hgta_table=%s&hgta_doSchema=describe+table+schema\" TARGET=ucscSchema%s>%s</A>"
if (hTableOrSplitExists(db, tdb->table))
    {
    char *tbOff = trackDbSetting(tdb, "tableBrowser");
    if (isNotEmpty(tbOff) && sameString(nextWord(&tbOff), "off"))
	return FALSE;
    char *hint = " title='Open table schema in new window'";
    if( label == NULL)
        label = " View table schema";
    struct trackDb *topLevel = trackDbTopLevelSelfOrParent(tdb);
    printf(SCHEMA_LINKED, db, topLevel->grp, topLevel->track, tdb->table, hint, label);
    return TRUE;
    }
return FALSE;
}

char *controlledVocabLink(char *file,char *term,char *value,char *title, char *label,char *suffix)
// returns allocated string of HTML link to controlled vocabulary term
{
#define VOCAB_LINK_WITH_FILE "<A HREF='hgEncodeVocab?ra=%s&%s=\"%s\"' title='%s details' TARGET=ucscVocab>%s</A>"
#define VOCAB_LINK "<A HREF='hgEncodeVocab?%s=\"%s\"' title='%s details' TARGET=ucscVocab>%s</A>"
struct dyString *dyLink = NULL;
char *encTerm = cgiEncode(term);
char *encValue = cgiEncode(value);
if (file != NULL)
    {
    char *encFile = cgiEncode(file);
    dyLink = dyStringCreate(VOCAB_LINK_WITH_FILE,encFile,encTerm,encValue,title,label);
    freeMem(encFile);
    }
else
    dyLink = dyStringCreate(VOCAB_LINK,encTerm,encValue,title,label);
if (suffix != NULL)
    dyStringAppend(dyLink,suffix);  // Don't encode since this may contain HTML

freeMem(encTerm);
freeMem(encValue);
return dyStringCannibalize(&dyLink);
}

char *metadataAsHtmlTable(char *db,struct trackDb *tdb,boolean showLongLabel,boolean showShortLabel, struct hash *trackHash)
/* If metadata from metaDb exists, return string of html with table definition */
{
const struct mdbObj *safeObj = metadataForTable(db,tdb,NULL);
if (safeObj == NULL || safeObj->vars == NULL)
return NULL;

//struct dyString *dyTable = dyStringCreate("<table id='mdb_%s'>",tdb->table);
struct dyString *dyTable = dyStringCreate("<table style='display:inline-table;'>");
if(showLongLabel)
    dyStringPrintf(dyTable,"<tr valign='bottom'><td colspan=2 nowrap>%s</td></tr>",tdb->longLabel);
if(showShortLabel)
    dyStringPrintf(dyTable,"<tr valign='bottom'><td align='right' nowrap><i>shortLabel:</i></td><td nowrap>%s</td></tr>",tdb->shortLabel);

// Get the hash of mdb and cv term types
struct hash *cvTermTypes = (struct hash *)cvTermTypeHash();

struct mdbObj *mdbObj = mdbObjClone(safeObj); // Important if we are going to remove vars!
mdbObjRemoveVars(mdbObj,MDB_OBJ_TYPE_COMPOSITE " " MDB_VAR_PROJECT " " MDB_OBJ_TYPE " " MDB_VAR_MD5SUM); // Don't bother showing these (NOTE: composite,objType should be added to cv.ra typeOfTerms as hidden)
mdbObjRemoveHiddenVars(mdbObj);
mdbObjReorderByCv(mdbObj,FALSE);// Use cv defined order for visible vars
struct mdbVar *mdbVar;
for (mdbVar=mdbObj->vars;mdbVar!=NULL;mdbVar=mdbVar->next)
    {
    if ((sameString(mdbVar->var,MDB_VAR_FILENAME) || sameString(mdbVar->var,MDB_VAR_FILEINDEX) )
    && trackDbSettingClosestToHome(tdb,MDB_VAL_ENCODE_PROJECT) != NULL)
        {
        dyStringPrintf(dyTable,"<tr valign='top'><td align='right' nowrap><i>%s:</i></td><td nowrap>",mdbVar->var);
//#define NO_FILENAME_LISTS
#ifdef NO_FILENAME_LISTS
        dyStringAppend(dyTable,htmlStringForDownloadsLink(db, tdb, mdbVar->val, TRUE, trackHash));
#else///ifndef NO_FILENAME_LISTS

        struct slName *fileSet = slNameListFromComma(mdbVar->val);
        while (fileSet != NULL)
            {
            struct slName *file = slPopHead(&fileSet);
            dyStringAppend(dyTable,htmlStringForDownloadsLink(db, tdb, file->name, TRUE, trackHash));
            if (fileSet != NULL)
                dyStringAppend(dyTable,"<BR>");
            slNameFree(&file);
            }
#endif///ndef NO_FILENAME_LISTS
        dyStringAppend(dyTable,"</td></tr>");
        }
    else
        {
        if (cvTermTypes && differentString(mdbVar->var,MDB_VAR_TABLENAME)) // Don't bother with tableName
            {
            struct hash *cvTerm = hashFindVal(cvTermTypes,mdbVar->var);
            if (cvTerm != NULL) // even if cvTerm isn't used, it proves that it exists and a link is desirable
                {
                if(!cvTermIsHidden(mdbVar->var))
                    {
                    char *label = (char *)cvLabel(NULL,mdbVar->var);
                    char *linkOfType = controlledVocabLink(NULL,CV_TYPE,mdbVar->var,label,label,NULL);
                    if (cvTermIsCvDefined(mdbVar->var))
                        {
                        label = (char *)cvLabel(mdbVar->var,mdbVar->val);
                        char *linkOfTerm = controlledVocabLink(NULL,CV_TERM,mdbVar->val,label,label,NULL);
                        dyStringPrintf(dyTable,"<tr valign='bottom'><td align='right' nowrap><i>%s:</i></td><td nowrap>%s</td></tr>",linkOfType,linkOfTerm);
                        freeMem(linkOfTerm);
                        }
                    else
                        dyStringPrintf(dyTable,"<tr valign='bottom'><td align='right' nowrap><i>%s:</i></td><td nowrap>%s</td></tr>",linkOfType,mdbVar->val);
                    freeMem(linkOfType);
                    continue;
                    }
                }
            }
        dyStringPrintf(dyTable,"<tr valign='bottom'><td align='right' nowrap><i>%s:</i></td><td nowrap>%s</td></tr>",mdbVar->var,mdbVar->val);
        }
    }
dyStringAppend(dyTable,"</table>");
//mdbObjsFree(&mdbObj); // spill some memory
return dyStringCannibalize(&dyTable);
}

boolean compositeMetadataToggle(char *db,struct trackDb *tdb,char *title,
        boolean embeddedInText,boolean showLongLabel, struct hash *trackHash)
/* If metadata from metaTbl exists, create a link that will allow toggling it's display */
{
const struct mdbObj *safeObj = metadataForTable(db,tdb,NULL);
if(safeObj == NULL || safeObj->vars == NULL)
return FALSE;

printf("%s<A HREF='#a_meta_%s' onclick='return metadataShowHide(\"%s\",%s,true);' title='Show metadata details...'>%s<img src='../images/downBlue.png'/></A>",
        (embeddedInText?"&nbsp;":"<P>"),tdb->track,tdb->track, showLongLabel?"true":"false", (title?title:""));
if (!sameString(tdb->table, tdb->track) && trackHash != NULL) // If trackHash is needed, then can't fill this in with ajax
    {
    printf("<DIV id='div_%s_meta' style='display:none;'>%s</div>",tdb->track,
        metadataAsHtmlTable(db,tdb,showLongLabel,TRUE,trackHash) );
    }
else
    printf("<DIV id='div_%s_meta' style='display:none;'></div>",tdb->track);
return TRUE;
}

void extraUiLinks(char *db,struct trackDb *tdb, struct hash *trackHash)
/* Show downlaods, schema and metadata links where appropriate */
{
boolean schemaLink = (!tdbIsDownloadsOnly(tdb)
                  && isCustomTrack(tdb->table) == FALSE)
                  && (hTableOrSplitExists(db, tdb->table));
boolean metadataLink = (!tdbIsComposite(tdb)
                  && metadataForTable(db, tdb, NULL) != NULL);
boolean downloadLink = (trackDbSetting(tdb, "wgEncode") != NULL && !tdbIsSuperTrack(tdb));
int links = 0;
if (schemaLink)
    links++;
if (metadataLink)
    links++;
if (downloadLink)
    links++;

if(links > 0)
    cgiDown(0.7);
if(links > 1)
    printf("<table><tr><td nowrap>View table: ");

if(schemaLink)
    {
    makeSchemaLink(db,tdb,(links > 1 ? "schema":"View table schema"));
    if(downloadLink || metadataLink)
        printf(", ");
    }
if(downloadLink)
    {
    // special case exception (hg18:NHGRI BiPs are in 7 different dbs but only hg18 has downloads):
    char *targetDb = trackDbSetting(tdb, "compareGenomeLinks");
    if (targetDb != NULL)
        {
        targetDb = cloneFirstWordByDelimiter(targetDb,'=');
        if (!startsWith("hg",targetDb))
            freez(&targetDb);
        }
    if (targetDb == NULL)
        targetDb = cloneString(db);

    makeNamedDownloadsLink(targetDb, tdb, (links > 1 ? "downloads":"Downloads"), trackHash);
    freez(&targetDb);
    if(metadataLink)
        printf(",");
    }
if (metadataLink)
    compositeMetadataToggle(db,tdb,"metadata", TRUE, TRUE, trackHash);

if(links > 1)
    printf("</td></tr></table>");
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
safef(buf, sizeof(buf), "<span style='color:#FFFFFF;'>%s</span>", s);
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
#define hTvStringShowSameAsFull "show"

enum trackVisibility hTvFromStringNoAbort(char *s)
/* Given a string representation of track visibility, return as
 * equivalent enum. */
{
int vis = stringArrayIx(s, hTvStrings, ArraySize(hTvStrings));
if (vis < 0)
    {
    if (sameString(hTvStringShowSameAsFull,s))
        return tvShow;  // Show is the same as full!
    vis = 0;  // don't generate bogus value on invalid input
    }
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

void hTvDropDownClassVisOnlyAndExtra(char *varName, enum trackVisibility vis,
	boolean canPack, char *class, char *visOnly,char *extra)
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
	cgiMakeDropListClassWithStyleAndJavascript(varName, denseOnly, ArraySize(denseOnly),
		denseOnly[visIx], class, TV_DROPDOWN_STYLE,extra);
    else if (sameWord(visOnly,"squish"))
	cgiMakeDropListClassWithStyleAndJavascript(varName, squishOnly,
                ArraySize(squishOnly), squishOnly[visIx],
                class, TV_DROPDOWN_STYLE,extra);
    else if (sameWord(visOnly,"pack"))
	cgiMakeDropListClassWithStyleAndJavascript(varName, packOnly, ArraySize(packOnly),
		packOnly[visIx], class, TV_DROPDOWN_STYLE,extra);
    else if (sameWord(visOnly,"full"))
	cgiMakeDropListClassWithStyleAndJavascript(varName, fullOnly, ArraySize(fullOnly),
		fullOnly[visIx], class, TV_DROPDOWN_STYLE,extra);
    else			/* default when not recognized */
	cgiMakeDropListClassWithStyleAndJavascript(varName, denseOnly, ArraySize(denseOnly),
		denseOnly[visIx], class, TV_DROPDOWN_STYLE,extra);
    }
    else
    {
    if (canPack)
	cgiMakeDropListClassWithStyleAndJavascript(varName, pack, ArraySize(pack),
                            pack[packIx[vis]], class, TV_DROPDOWN_STYLE,extra);
    else
	cgiMakeDropListClassWithStyleAndJavascript(varName, noPack, ArraySize(noPack),
                            noPack[vis], class, TV_DROPDOWN_STYLE,extra);
    }
}

void hideShowDropDownWithClassAndExtra(char *varName, boolean show, char *class, char *extra)
/* Make hide/show dropdown for varName */
{
static char *hideShow[] =
    {
    "hide",
    "show"
    };
cgiMakeDropListClassWithStyleAndJavascript(varName, hideShow, ArraySize(hideShow),
                            hideShow[show], class, TV_DROPDOWN_STYLE,extra);
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
    BASE_COLOR_DRAW_ITEM_BASES_CDS_LABEL,
    BASE_COLOR_DRAW_DIFF_BASES_CDS_LABEL,
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
    BASE_COLOR_DRAW_ITEM_BASES_NC_LABEL,
    BASE_COLOR_DRAW_DIFF_BASES_NC_LABEL,
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
		 tdb->track, BASE_COLOR_USE_CDS, setting);
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
		 tdb->track, BASE_COLOR_USE_SEQUENCE, setting);
    }
return gotIt;
}

static void baseColorDropLists(struct cart *cart, struct trackDb *tdb, char *name)
/* draw the baseColor drop list options */
{
enum baseColorDrawOpt curOpt = baseColorDrawOptEnabled(cart, tdb);
char *curValue = baseColorDrawAllOptionValues[curOpt];
char var[512];
safef(var, sizeof(var), "%s." BASE_COLOR_VAR_SUFFIX, name);
boolean gotCds = baseColorGotCds(tdb);
boolean gotSeq = baseColorGotSequence(tdb);
if (gotCds && gotSeq)
    {
    puts("<P><B>Color track by codons or bases:</B>");
    cgiMakeDropListFull(var, baseColorDrawAllOptionLabels,
			baseColorDrawAllOptionValues,
			ArraySize(baseColorDrawAllOptionLabels),
			curValue, NULL);
    printf("<A HREF=\"%s\">Help on mRNA coloring</A><BR>",
	   CDS_MRNA_HELP_PAGE);
    }
else if (gotCds)
    {
    char buf[256];
    char *disabled = NULL;
    safef(buf, sizeof(buf), "onchange='codonColoringChanged(\"%s\")'", name);
    puts("<P><B>Color track by codons:</B>");
    cgiMakeDropListFull(var, baseColorDrawGenomicOptionLabels,
			baseColorDrawGenomicOptionValues,
			ArraySize(baseColorDrawGenomicOptionLabels),
			curValue, buf);
    printf("<A HREF=\"%s\">Help on codon coloring</A><BR>",
	   CDS_HELP_PAGE);
    safef(buf, sizeof(buf), "%s.%s", name, CODON_NUMBERING_SUFFIX);
    puts("<br /><b>Show codon numbering</b>:\n");
    if(curOpt == baseColorDrawOff)
        disabled = "disabled";
    cgiMakeCheckBoxJS(buf, cartUsualBooleanClosestToHome(cart, tdb, FALSE, CODON_NUMBERING_SUFFIX, FALSE), disabled);
    }
else if (gotSeq)
    {
    puts("<P><B>Color track by bases:</B>");
    cgiMakeDropListFull(var, baseColorDrawItemOptionLabels,
			baseColorDrawItemOptionValues,
			ArraySize(baseColorDrawItemOptionLabels),
			curValue, NULL);
    printf("<A HREF=\"%s\">Help on base coloring</A><BR>",
	   CDS_BASE_HELP_PAGE);
    }
}

void baseColorDrawOptDropDown(struct cart *cart, struct trackDb *tdb)
/* Make appropriately labeled drop down of options if any are applicable.*/
{
baseColorDropLists(cart, tdb, tdb->track);
}

enum baseColorDrawOpt baseColorDrawOptEnabled(struct cart *cart,
					      struct trackDb *tdb)
/* Query cart & trackDb to determine what drawing mode (if any) is enabled. */
{
char *stringVal = NULL;
assert(cart);
assert(tdb);

/* trackDb can override default of OFF; cart can override trackDb. */
stringVal = trackDbSettingClosestToHomeOrDefault(tdb, BASE_COLOR_DEFAULT,
				    BASE_COLOR_DRAW_OFF);
stringVal = cartUsualStringClosestToHome(cart, tdb, FALSE, BASE_COLOR_VAR_SUFFIX,stringVal);

return baseColorDrawOptStringToEnum(stringVal);
}


/*** Control of fancy indel display code: ***/

static boolean tdbOrCartBoolean(struct cart *cart, struct trackDb *tdb,
                                char *settingName, char *defaultOnOff)
/* Query cart & trackDb to determine if a boolean variable is set. */
{
boolean alreadySet;
alreadySet = !sameString("off",trackDbSettingOrDefault(tdb, settingName, defaultOnOff));
alreadySet = cartUsualBooleanClosestToHome(cart, tdb, FALSE, settingName, alreadySet); // NOTE: compositeLevel=FALSE because tdb param already is at appropriate level
return alreadySet;
}

static boolean indelAppropriate(struct trackDb *tdb)
/* Return true if it makes sense to offer indel display options for tdb. */
{
return (tdb && (startsWith("psl", tdb->type) || sameString("bam", tdb->type)) &&
	(cfgOptionDefault("browser.indelOptions", NULL) != NULL));
}

static void indelEnabledByName(struct cart *cart, struct trackDb *tdb, char *name,
                  float basesPerPixel, boolean *retDoubleInsert, boolean *retQueryInsert,
                  boolean *retPolyA)
/* Query cart & trackDb to determine what indel display (if any) is enabled. Set
 * basesPerPixel to 0.0 to disable check for zoom level.  */
{
struct trackDb *tdbLevel = tdb;
if (differentString(tdb->track, name) && tdb->parent != NULL)
    tdbLevel = tdb->parent;

boolean apropos = indelAppropriate(tdb);
if (apropos && (basesPerPixel > 0.0))
    {
    // check indel max zoom
    float showIndelMaxZoom = trackDbFloatSettingOrDefault(tdbLevel, "showIndelMaxZoom", -1.0);
    if ((showIndelMaxZoom >= 0)
        && ((basesPerPixel > showIndelMaxZoom) || (showIndelMaxZoom == 0.0)))
        apropos = FALSE;
    }

if (retDoubleInsert)
    *retDoubleInsert = apropos &&
        tdbOrCartBoolean(cart, tdbLevel, INDEL_DOUBLE_INSERT, "off");
if (retQueryInsert)
    *retQueryInsert = apropos &&
        tdbOrCartBoolean(cart, tdbLevel, INDEL_QUERY_INSERT, "off");
if (retPolyA)
    *retPolyA = apropos &&
        tdbOrCartBoolean(cart, tdbLevel, INDEL_POLY_A, "off");
}

void indelEnabled(struct cart *cart, struct trackDb *tdb, float basesPerPixel,
                  boolean *retDoubleInsert, boolean *retQueryInsert,
                  boolean *retPolyA)
/* Query cart & trackDb to determine what indel display (if any) is enabled. Set
 * basesPerPixel to 0.0 to disable check for zoom level.  */
{
indelEnabledByName(cart,tdb,tdb->track,basesPerPixel,retDoubleInsert,retQueryInsert,retPolyA);
}

static void indelShowOptionsWithName(struct cart *cart, struct trackDb *tdb,char *name)
/* Make HTML inputs for indel display options if any are applicable. */
{
if (indelAppropriate(tdb))
    {
    boolean showDoubleInsert, showQueryInsert, showPolyA;
    char var[512];
    indelEnabledByName(cart, tdb, name, 0.0, &showDoubleInsert, &showQueryInsert, &showPolyA);
    printf("<TABLE><TR><TD colspan=2><B>Alignment Gap/Insertion Display Options</B>");
    printf("&nbsp;<A HREF=\"%s\">Help on display options</A>\n<TR valign='top'><TD>",
           INDEL_HELP_PAGE);
    safef(var, sizeof(var), "%s.%s", name, INDEL_DOUBLE_INSERT);
    cgiMakeCheckBox(var, showDoubleInsert);
    printf("</TD><TD>Draw double horizontal lines when both genome and query have "
           "an insertion</TD></TR>\n<TR valign='top'><TD>");
    safef(var, sizeof(var), "%s.%s", name, INDEL_QUERY_INSERT);
    cgiMakeCheckBox(var, showQueryInsert);
    printf("</TD><TD>Draw a vertical purple line for an insertion at the beginning or "
           "end of the <BR>query, orange for insertion in the middle of the query</TD></TR>\n<TR valign='top'><TD>");
    safef(var, sizeof(var), "%s.%s", name, INDEL_POLY_A);
    /* We can highlight valid polyA's only if we have query sequence --
     * so indelPolyA code piggiebacks on baseColor code: */
    if (baseColorGotSequence(tdb))
        {
        cgiMakeCheckBox(var, showPolyA);
        printf("</TD><TD>Draw a vertical green line where query has a polyA tail "
               "insertion</TD></TR>\n");
        }

    printf("</TABLE>\n");
    }
}

void indelShowOptions(struct cart *cart, struct trackDb *tdb)
/* Make HTML inputs for indel display options if any are applicable. */
{
indelShowOptionsWithName(cart, tdb, tdb->track);
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

static char *aggregateLabels[] =
    {
    "none",
    "transparent",
    "solid",
    };

static char *aggregateValues[] =
    {
    WIG_AGGREGATE_NONE,
    WIG_AGGREGATE_TRANSPARENT,
    WIG_AGGREGATE_SOLID,
    };

void aggregateDropDown(char *var, char *curVal)
/* Make drop down menu for aggregate strategy */
{
cgiMakeDropListFull(var, aggregateLabels, aggregateValues,
	ArraySize(aggregateValues), curVal, NULL);
}

static char *wiggleTransformFuncOptions[] = {
    "NONE",
    "LOG"
    };

static char *wiggleTransformFuncLabels[] = {
    "NONE",
    "LOG (ln(1+x))"
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
cgiMakeDropListFull(var, wiggleTransformFuncLabels, wiggleTransformFuncOptions,
    ArraySize(wiggleTransformFuncOptions), curVal, NULL);
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
    if ((sameString(type, "genePred")) && (!sameString(tdb->table, "tigrGeneIndex") && !tdbIsComposite(tdb)))
	{
	AllocVar(name);
	name->name = tdb->track;
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

void rAddTrackListToHash(struct hash *trackHash, struct trackDb *tdbList, char *chrom,
	boolean leafOnly)
/* Recursively add trackList to trackHash */
{
struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    if (hTrackOnChrom(tdb, chrom))
	{
	if (tdb->subtracks == NULL || !leafOnly)
	    hashAdd(trackHash, tdb->track, tdb);
	}
    rAddTrackListToHash(trackHash, tdb->subtracks, chrom, leafOnly);
    }
}

struct hash *trackHashMakeWithComposites(char *db,char *chrom,struct trackDb **tdbList,bool withComposites)
// Make hash of trackDb items for this chromosome, including composites, not just the subtracks.
// May pass in prepopulated trackDb list, or may receive the trackDb list as an inout.
{
struct trackDb *theTdbs = NULL;
if (tdbList == NULL || *tdbList == NULL)
    {
    theTdbs = hTrackDb(db);
    if (tdbList != NULL)
        *tdbList = theTdbs;
    }
else
    theTdbs = *tdbList;
struct hash *trackHash = newHash(7);
rAddTrackListToHash(trackHash, theTdbs, chrom, !withComposites);
return trackHash;
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

static boolean parseAssignment(char *words, char **name, char **value)
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
char *type = NULL;
if (primarySubtrack)
    {
    struct slRef *tdbRef, *tdbRefList = trackDbListGetRefsToDescendants(tdb->subtracks);
    for (tdbRef = tdbRefList; tdbRef != NULL; tdbRef = tdbRef->next)
	{
	struct trackDb *subtrack = tdbRef->val;
	if (sameString(subtrack->track, primarySubtrack))
	    {
	    type = subtrack->type;
	    break;
	    }
	}
    slFreeList(&tdbRefList);
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

// Four State checkboxes can be checked/unchecked by enable/disabled
// NOTE: fourState is not a bitmap because it is manipulated in javascript and int seemed easier at the time
#define FOUR_STATE_EMPTY             TDB_EXTRAS_EMPTY_STATE
//#define FOUR_STATE_UNCHECKED         0
//#define FOUR_STATE_CHECKED           1
//#define FOUR_STATE_CHECKED_DISABLED  -1
#define FOUR_STATE_DISABLE(val)      {while((val) >= 0) (val) -= 2;}
#define FOUR_STATE_ENABLE(val)       {while((val) < 0) (val) += 2;}

int subtrackFourStateChecked(struct trackDb *subtrack, struct cart *cart)
/* Returns the four state checked state of the subtrack */
{
char * setting = NULL;
char objName[SMALLBUF];
int fourState = (int)tdbExtrasFourState(subtrack);
if(fourState != FOUR_STATE_EMPTY)
    return fourState;

fourState = FOUR_STATE_UNCHECKED;  // default to unchecked, enabled
if ((setting = trackDbLocalSetting(subtrack, "parent")) != NULL)
    {
    if(findWordByDelimiter("off",' ',setting) == NULL)
        fourState = FOUR_STATE_CHECKED;
    }

// Now check visibility
enum trackVisibility vis = tdbLocalVisibility(cart, subtrack, NULL);
if (vis == tvHide)
    {
    if(tdbIsCompositeView(subtrack->parent))
        {
        if(tdbLocalVisibility(cart, subtrack->parent, NULL) == tvHide)
            FOUR_STATE_DISABLE(fourState);
        }
    }

safef(objName, sizeof(objName), "%s_sel", subtrack->track);
fourState = cartUsualInt(cart, objName, fourState);
tdbExtrasFourStateSet(subtrack,fourState);
return fourState;
}

void subtrackFourStateCheckedSet(struct trackDb *subtrack, struct cart *cart,boolean checked, boolean enabled)
/* Sets the fourState Checked in the cart and updates cached state */
{
int fourState = ( checked ? FOUR_STATE_CHECKED : FOUR_STATE_UNCHECKED );
if (!enabled)
    FOUR_STATE_DISABLE(fourState);

char objName[SMALLBUF];
char objVal[5];
safef(objName, sizeof(objName), "%s_sel", subtrack->track);
safef(objVal, sizeof(objVal), "%d", fourState);
cartSetString(cart, objName, objVal);
tdbExtrasFourStateSet(subtrack,fourState);
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

#define FILTER_COMPOSITE_ONLYONE
#ifdef FILTER_COMPOSITE_ONLYONE
// FIXME: do we even support anything but multi???  If not, this is a boolean
enum filterCompositeType
/* How to look at a track. */
    {
    fctNone=0,      // do not offer filter for this dimension
    fctOne=1,       // filter composite by one or all
    fctOneOnly=2,   // filter composite by only one
    fctMulti=3,     // filter composite by multiselect: all, one or many
    };
#endif///def FILTER_COMPOSITE_ONLYONE

typedef struct _members {
    int count;
    char * groupTag;
    char * groupTitle;
    char **tags;
    char **titles;
    boolean *selected;
    char * setting;
    int *subtrackCount;              // count of subtracks
    int *currentlyVisible;           // count of visible subtracks
    struct slRef **subtrackList;     // set of subtracks belonging to each subgroup member
#ifdef FILTER_COMPOSITE_ONLYONE
    enum filterCompositeType fcType; // fctNone,fctOne,fctMulti
#endif///def FILTER_COMPOSITE_ONLYONE
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
int ix,count;
char *setting = subgroupSettingByTagOrName(parentTdb, groupNameOrTag);
if(setting == NULL)
    return NULL;
members_t *members = needMem(sizeof(members_t));
members->setting = cloneString(setting);
char *words[SMALLBUF];
count = chopLine(members->setting, words);
assert(count <= ArraySize(words));
if(count <= 1)
    {
    freeMem(members->setting);
    freeMem(members);
    return NULL;
    }
members->groupTag   = words[0];
members->groupTitle = strSwapChar(words[1],'_',' '); // Titles replace '_' with space
members->tags       = needMem(count*sizeof(char*));
members->titles     = needMem(count*sizeof(char*));
for (ix = 2,members->count=0; ix < count; ix++)
    {
    char *name,*value;
    if (parseAssignment(words[ix], &name, &value))
        {
        members->tags[members->count]  = name;
        members->titles[members->count] = strSwapChar(value,'_',' ');
        members->count++;
        }
    }
return members;
}

// No longer used, but it could come back!
//static members_t *subgroupMembersGetByDimension(struct trackDb *parentTdb, char dimension)
///* Finds the dimension requested and return its associated
// * subgroupMembership, returning the count of members or 0 */
//{
//int ix;
//dimensions_t *dimensions = dimensionSettingsGet(parentTdb);
//if(dimensions!=NULL)
//    {
//    for(ix=0;ix<dimensions->count;ix++)
//        {
//        if(lastChar(dimensions->names[ix]) != dimension)
//            continue;
//        if((strlen(dimensions->names[ix]) == 10 && startsWith("dimension",dimensions->names[ix]))
//        || (strlen(dimensions->names[ix]) == 4  && startsWith("dim",dimensions->names[ix])))
//            {
//            members_t *members = subgroupMembersGet(parentTdb, dimensions->subgroups[ix]);
//            dimensionsFree(&dimensions);
//            return members;
//            }
//        }
//    dimensionsFree(&dimensions);
//    }
//return NULL;
//}

static int membersSubGroupIx(members_t* members, char *tag)
{ // Returns the index of the subgroup within the members struct (or -1)
int ix = 0;
for(ix=0;ix<members->count;ix++)
    {
    if (members->tags[ix] != NULL && sameString(members->tags[ix],tag))
        return ix;
    }
return -1;
}


static void subgroupMembersFree(members_t **members)
/* frees memory for subgroupMembers lists */
{
if(members && *members)
    {
    // This should only get set through membersForAll which should not be freed.
    if((*members)->selected != NULL || (*members)->subtrackList != NULL)
    	return;
    freeMem((*members)->setting);
    freeMem((*members)->tags);
    freeMem((*members)->titles);
    freez(members);
    }
}

static members_t *subgroupMembersWeedOutEmpties(struct trackDb *parentTdb, members_t *members, struct cart *cart)
// Weed out members of a subgroup without any subtracks, alters memory in place!
{
// First tally all subtrack counts
int ixIn=0;
struct slRef *subtrackRef, *subtrackRefList = trackDbListGetRefsToDescendantLeaves(parentTdb->subtracks);
struct trackDb *subtrack;
members->subtrackCount    = needMem(members->count * sizeof(int));
members->currentlyVisible = needMem(members->count * sizeof(int));
members->subtrackList     = needMem(members->count * sizeof(struct slRef *));
for (subtrackRef = subtrackRefList; subtrackRef != NULL; subtrackRef = subtrackRef->next)
    {
    subtrack = subtrackRef->val;
    char *belongsTo =NULL;
    if(subgroupFind(subtrack,members->groupTag,&belongsTo))
        {
        for(ixIn=0;ixIn<members->count;ixIn++)
            {
            if(sameString(members->tags[ixIn],belongsTo))
                {
                members->subtrackCount[ixIn]++;
                if(cart && fourStateVisible(subtrackFourStateChecked(subtrack,cart)))
                    members->currentlyVisible[ixIn]++;
                refAdd(&(members->subtrackList[ixIn]), subtrack);
                break;
                }
            }
        }
    }

// Now weed out empty subgroup tags.  Can do this in place since new count <= old count
// NOTE: Don't I wish I had made these as an slList ages ago! (tim)
int ixOut=0;
for(ixIn=ixOut;ixIn<members->count;ixIn++)
    {
    if(members->subtrackCount[ixIn] > 0)
        {
        if(ixOut < ixIn)
        {
            members->tags[ixOut]             = members->tags[ixIn];
            members->titles[ixOut]           = members->titles[ixIn];
            members->subtrackCount[ixOut]    = members->subtrackCount[ixIn];
            members->currentlyVisible[ixOut] = members->currentlyVisible[ixIn];
            members->subtrackList[ixOut]     = members->subtrackList[ixIn];
            if(members->selected != NULL)
                members->selected[ixOut]     = members->selected[ixIn];
            }
        ixOut++;
        }
    else
        {
        members->tags[ixIn]             = NULL;
        members->titles[ixIn]           = NULL;
        members->subtrackCount[ixIn]    = 0;
        members->currentlyVisible[ixIn] = 0;
        //slFreeList(&members->subtrackList[ixIn]);  // No freeing at this moment
        members->subtrackList[ixIn]     = NULL;
        if(members->selected != NULL)
            members->selected[ixIn]     = FALSE;
        }
    }
members->count = ixOut;

if(members->count == 0) // No members of this subgroup had a subtrack
    {
    subgroupMembersFree(&members);
    return NULL;
    }

return members;
}

enum
{
    dimV=0, // View first
    dimX=1, // X & Y next
    dimY=2,
    dimA=3, // dimA is start of first of the optional non-matrix, non-view dimensions
};

typedef struct _membersForAll {
    int abcCount;
    int dimMax;               // Arrays of "members" structs will be ordered as [view][dimX][dimY][dimA]... with first 3 in fixed spots and rest as found (and non-empty)
    boolean filters;          // ABCs use filterComp boxes (as upposed to check boxes
    dimensions_t *dimensions; // One struct describing "deimensions" setting"  (e.g. dimX:cell dimY:antibody dimA:treatment)
    members_t* members[27];   // One struct for each dimension describing groups in dimension (e.g. cell: GM12878,K562)
    char* checkedTags[27];  // FIXME: Should move checkedTags into membersForAll->members[ix]->selected;
    char letters[27];
} membersForAll_t;

static char* abcMembersChecked(struct trackDb *parentTdb, struct cart *cart, members_t* members, char letter)
/* returns a string of subGroup tags which are currently checked */
{
char settingName[SMALLBUF];
int mIx;
if (members->selected == NULL)
    members->selected = needMem(members->count * sizeof(boolean));
safef(settingName, sizeof(settingName), "%s.filterComp.%s",parentTdb->track,members->groupTag);
struct slName *options = cartOptionalSlNameList(cart,settingName);
if(options != NULL)
    {
    if (sameWord(options->name,"All")) // filterComp returns "All" which means every option selected
        {
        slNameFreeList(&options);
        options = slNameListFromStringArray(members->tags, members->count);
        assert(options != NULL);
        }
    struct slName *option;
    for(option=options;option!=NULL;option=option->next)
        {
        mIx = membersSubGroupIx(members, option->name);
        if(mIx >= 0)
            members->selected[mIx] = TRUE;
        }
    return slNameListToString(options,',');
    }
struct dyString *currentlyCheckedTags = NULL;
// Need a string of subGroup tags which are currently checked
safef(settingName,sizeof(settingName),"dimension%cchecked",letter);
char *dimCheckedDefaults = trackDbSettingOrDefault(parentTdb,settingName,"All");
for(mIx=0;mIx<members->count;mIx++)
    {
    safef(settingName, sizeof(settingName), "%s.mat_%s_dim%c_cb",parentTdb->track,members->tags[mIx],letter);
    members->selected[mIx] = TRUE;
    if (differentWord(dimCheckedDefaults,"All") && differentWord(dimCheckedDefaults,"Any"))
        members->selected[mIx] = (NULL!=findWordByDelimiter(members->tags[mIx],',',dimCheckedDefaults));
    members->selected[mIx] = cartUsualBoolean(cart,settingName,members->selected[mIx]);
    if(members->selected[mIx])
        {
        if(currentlyCheckedTags == NULL)
            currentlyCheckedTags = dyStringCreate(members->tags[mIx]);
        else
            dyStringPrintf(currentlyCheckedTags,",%s",members->tags[mIx]);
        }
    }
if(currentlyCheckedTags)
    return dyStringCannibalize(&currentlyCheckedTags);
return NULL;
}

static membersForAll_t *membersForAllSubGroupsWeedOutEmpties(struct trackDb *parentTdb, membersForAll_t *membersForAll, struct cart *cart)
// Weed through members, tossing those without subtracks
{
// View is always first
if (membersForAll->members[dimV] != NULL)
    membersForAll->members[dimV] = subgroupMembersWeedOutEmpties(parentTdb, membersForAll->members[dimV],cart);

// X and Y are special
if (membersForAll->members[dimX] != NULL)
    membersForAll->members[dimX] = subgroupMembersWeedOutEmpties(parentTdb, membersForAll->members[dimX],cart);
if (membersForAll->members[dimY] != NULL)
    membersForAll->members[dimY] = subgroupMembersWeedOutEmpties(parentTdb, membersForAll->members[dimY],cart);

// Handle the ABC dimensions
int ixIn,ixOut=dimA;
for(ixIn=ixOut;ixIn<membersForAll->dimMax;ixIn++)
    {
    if(membersForAll->members[ixIn] != NULL)
        membersForAll->members[ixIn] = subgroupMembersWeedOutEmpties(parentTdb, membersForAll->members[ixIn],cart);
    if(membersForAll->members[ixIn] == NULL)
        membersForAll->checkedTags[ixOut] = NULL;
    else
        {
        if(ixOut < ixIn)  // Collapse if necessary
            { // NOTE: Don't I wish I had made these as an slList ages ago! (tim)
            membersForAll->members[ixOut]     = membersForAll->members[ixIn];
            membersForAll->checkedTags[ixOut] = membersForAll->checkedTags[ixIn];
            membersForAll->letters[ixOut]     = membersForAll->letters[ixIn];
            }
        ixOut++;
        }
    }
membersForAll->dimMax   = ixOut;
membersForAll->abcCount = membersForAll->dimMax - dimA;

return membersForAll;
}

static membersForAll_t* membersForAllSubGroupsGet(struct trackDb *parentTdb, struct cart *cart)
/* Returns all the parents subGroups and members */
{
membersForAll_t *membersForAll = tdbExtrasMembersForAll(parentTdb);
if(membersForAll != NULL)
    return membersForAll;  // Already retrieved, so don't do it again

int ix;
membersForAll = needMem(sizeof(membersForAll_t));
membersForAll->members[dimV]=subgroupMembersGet(parentTdb,"view");
membersForAll->letters[dimV]='V';
membersForAll->dimMax=dimA;  // This can expand, depending upon ABC dimensions
membersForAll->dimensions = dimensionSettingsGet(parentTdb);
if(membersForAll->dimensions != NULL)
    {
    for(ix=0;ix<membersForAll->dimensions->count;ix++)
        {
        char letter = lastChar(membersForAll->dimensions->names[ix]);
        if(letter != 'X' && letter != 'Y')
            {
            membersForAll->members[membersForAll->dimMax]=subgroupMembersGet(parentTdb, membersForAll->dimensions->subgroups[ix]);
            membersForAll->letters[membersForAll->dimMax]=letter;
            if(cart != NULL)
                membersForAll->checkedTags[membersForAll->dimMax] = abcMembersChecked(parentTdb,cart,membersForAll->members[membersForAll->dimMax],letter);
            membersForAll->dimMax++;
            }
        else if(letter == 'X')
            {
            membersForAll->members[dimX]=subgroupMembersGet(parentTdb, membersForAll->dimensions->subgroups[ix]);
            membersForAll->letters[dimX]=letter;
            }
        else
            {
            membersForAll->members[dimY]=subgroupMembersGet(parentTdb, membersForAll->dimensions->subgroups[ix]);
            membersForAll->letters[dimY]=letter;
            }
        }
    }
membersForAll->abcCount = membersForAll->dimMax - dimA;

membersForAll = membersForAllSubGroupsWeedOutEmpties(parentTdb, membersForAll, cart);

// NOTE: Dimensions must be defined for filterComposite.  Filter dimensioms are all and only ABCs.  Use dimensionAchecked to define selected
char *filtering = trackDbSettingOrDefault(parentTdb,"filterComposite",NULL);
if(filtering && !sameWord(filtering,"off"))
    {
    if(membersForAll->dimensions == NULL)
        errAbort("If 'filterComposite' defined, must define 'dimensions' also.");

    membersForAll->filters = TRUE;
#ifdef FILTER_COMPOSITE_ONLYONE
    // Default all to multi
    for(ix=dimA;ix<membersForAll->dimMax;ix++)
        {
        if(membersForAll->members[ix] != NULL)
            membersForAll->members[ix]->fcType = fctMulti;
        }
    if(!sameWord(filtering,"on"))
        {
        // Example tdb setting: "filterComposite on" OR "filterComposite dimA=one dimB=multi dimC=onlyOne"
        // FIXME: do we even support anything but multi???
        char *filterGroups[27];
        int count = chopLine(filtering,filterGroups);
        for(ix=0;ix<count;ix++)
            {
            char *dim = cloneNextWordByDelimiter(&filterGroups[ix],'=');
            char letter = lastChar(dim);
            int abcIx = dimA;
            for (;abcIx < membersForAll->dimMax && membersForAll->letters[abcIx] != letter;abcIx++) ; // Advance to correct letter
            if (abcIx >= membersForAll->dimMax)
                errAbort("Invalid 'filterComposite' trackDb setting. Dimension '%s' not found.",dim);
            if (sameWord(filterGroups[ix],"one"))
                membersForAll->members[abcIx]->fcType = fctOne;
            else if (sameWord(filterGroups[ix],"onlyOne") || sameWord(filterGroups[ix],"oneOnly"))
                membersForAll->members[abcIx]->fcType = fctOneOnly;
            }
        }
#endif///def FILTER_COMPOSITE_ONLYONE
    }

if(cart != NULL) // Only save this if it is fully populated!
    tdbExtrasMembersForAllSet(parentTdb,membersForAll);

return membersForAll;
}

static int membersForAllFindSubGroupIx(membersForAll_t* membersForAll, char *tag)
{ // Returns the index of the subgroups member struct within membersForAll (or -1)
int ix = 0;
for(ix=0;ix<membersForAll->dimMax;ix++)
    {
    if (membersForAll->members[ix] != NULL && sameString(membersForAll->members[ix]->groupTag,tag))
        return ix;
    }
return -1;
}

const members_t*membersFindByTag(struct trackDb *parentTdb, char *tag)
{ // Uses membersForAll which may be in tdbExtraCache.  Do not free
membersForAll_t* membersForAll = membersForAllSubGroupsGet(parentTdb,NULL);
if(membersForAll == NULL)
    return NULL;

int ix = membersForAllFindSubGroupIx(membersForAll,tag);
if(ix >= 0)
    return membersForAll->members[ix];
return NULL;
}

static void membersForAllSubGroupsFree(struct trackDb *parentTdb, membersForAll_t** membersForAllPtr)
/* frees memory for membersForAllSubGroups struct */
{
if(membersForAllPtr && *membersForAllPtr)
    {
    if(parentTdb != NULL)
        {
        if(*membersForAllPtr == tdbExtrasMembersForAll(parentTdb))
            return;  // Don't free something saved to the tdbExtras!
        }
    membersForAll_t* membersForAll = *membersForAllPtr;
    subgroupMembersFree(&(membersForAll->members[dimX]));
    subgroupMembersFree(&(membersForAll->members[dimY]));
    subgroupMembersFree(&(membersForAll->members[dimV]));
    int ix;
    for(ix=dimA;ix<membersForAll->dimMax;ix++)
        {
        //ASSERT(membersForAll->members[ix] != NULL);
        subgroupMembersFree(&(membersForAll->members[ix]));
        if(membersForAll->checkedTags[ix])
            freeMem(membersForAll->checkedTags[ix]);
        }
    dimensionsFree(&(membersForAll->dimensions));
    freez(membersForAllPtr);
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
membership_t *membership = tdbExtrasMembership(childTdb);
if(membership != NULL)
    return membership;  // Already retrieved, so don't do it again

membership = needMem(sizeof(membership_t));
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
            int ix2 = stringArrayIx(value,members->tags,members->count);
            if(ix2 != -1)
                membership->titles[membership->count] = strSwapChar(cloneString(members->titles[ix2]),'_',' ');
            subgroupMembersFree(&members);
            }
        membership->count++;
        }
    }
tdbExtrasMembershipSet(childTdb,membership);
return membership;
}


static boolean membershipInAllCurrentABCs(membership_t *membership,membersForAll_t*membersForAll)
/* looks for a match between a membership set and ABC dimensions currently checked */
{
int mIx,aIx,tIx;
for (aIx = dimA; aIx < membersForAll->dimMax; aIx++)  // for each ABC subGroup
    {
    assert(membersForAll->members[aIx]->selected);

    // must find atleast one selected tag that we have membership in
    boolean matched = FALSE;
    for (mIx = 0; mIx <membersForAll->members[aIx]->count;mIx++) // for each tag of that subgroup
        {
        if(membersForAll->members[aIx]->selected[mIx])  // The particular subgroup tag is selected
            {
            for(tIx=0;tIx<membership->count;tIx++)  // what we are members of
                {
                if(sameString(membersForAll->members[aIx]->groupTag, membership->subgroups[tIx])   // subTrack belongs to subGroup
                &&   sameWord(membersForAll->members[aIx]->tags[mIx],membership->membership[tIx])) // and tags match
                    {
                    matched = TRUE;
                    break;
                    }
                }
            }
        }
        if(!matched)
            return FALSE;
    }
return TRUE; // passed all tests so must be on all
}

char *compositeGroupLabel(struct trackDb *tdb, char *group, char *id)
/* Given ID from group, return corresponding label,  looking through parent's subGroupN's */
/* Given group ID, return group label,  looking through parent's subGroupN's */
{
members_t *members = subgroupMembersGet(tdb, group);
char *result = NULL;
int i;
for (i=0; i<members->count; ++i)
    {
    if (sameString(members->tags[i], id))
	result = cloneString(members->titles[i]);
    }
subgroupMembersFree(&members);
return result;
}

char *compositeGroupId(struct trackDb *tdb, char *group, char *label)
/* Given label, return id,  looking through parent's subGroupN's */
{
members_t *members = subgroupMembersGet(tdb, group);
char *result = NULL;
int i;
for (i=0; i<members->count; ++i)
    {
    if (sameString(members->titles[i], label))
	result = cloneString(members->tags[i]);
    }
subgroupMembersFree(&members);
return result;
}


static boolean subtrackInAllCurrentABCs(struct trackDb *childTdb,membersForAll_t*membersForAll)
/* looks for a match between a membership set and ABC dimensions currently checked */
{
membership_t *membership = subgroupMembershipGet(childTdb);
if(membership == NULL)
    return FALSE;
boolean found = membershipInAllCurrentABCs(membership,membersForAll);
return found;
}


boolean subgroupFind(struct trackDb *childTdb, char *name,char **value)
/* looks for a single tag in a child track's subGroups setting */
{
if(value != NULL)
    *value = NULL;
membership_t *membership = subgroupMembershipGet(childTdb);
if(membership != NULL)
    {
    int ix;
    for(ix=0;ix<membership->count;ix++)
        {
        if(sameString(name,membership->subgroups[ix]))
            {
            if(value != NULL)
                *value = cloneString(membership->membership[ix]);
            return TRUE;
            }
        }
    }
return FALSE;
}

boolean subgroupFindTitle(struct trackDb *parentTdb, char *name,char **value)
/* looks for a a subgroup matching the name and returns the title if found */
{
if(value != (void*)NULL)
    *value = NULL;
members_t*members=subgroupMembersGet(parentTdb, name);
//const members_t *members = membersFindByTag(parentTdb,name); // Can't use because of dimension dependence
if(members==NULL)
    return FALSE;
*value = cloneString(members->groupTitle);
//subgroupMembersFree(&members);
return TRUE;
}

void subgroupFree(char **value)
/* frees subgroup memory */
{
if(value && *value)
    freez(value);
}
#define SORT_ON_TRACK_NAME "trackName"
#define SORT_ON_RESTRICTED "dateUnrestricted"

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
sortOrder->htmlId = needMem(strlen(parentTdb->track)+15);
safef(sortOrder->htmlId, (strlen(parentTdb->track)+15), "%s.sortOrder", parentTdb->track);
char *cartSetting = NULL;
if(cart != NULL)
    cartSetting = cartCgiUsualString(cart, sortOrder->htmlId, setting);
if(cart != NULL && strlen(cartSetting) > strlen(setting)) // If setting is bigger, then it may be due to a trackDb change
    sortOrder->sortOrder = cloneString(cartSetting);  // cart order
else
    sortOrder->sortOrder = cloneString(setting);      // old cart value is abandoned!

sortOrder->setting = cloneString(setting);
sortOrder->count   = chopByWhite(sortOrder->setting,NULL,0);  // Get size
if (cart && !stringIn(SORT_ON_TRACK_NAME,setting))
    sortOrder->count += 1;
if (cart && !stringIn(SORT_ON_RESTRICTED,setting))
    sortOrder->count += 1;
sortOrder->column  = needMem(sortOrder->count*sizeof(char*));
int foundColumns = chopByWhite(sortOrder->setting, sortOrder->column,sortOrder->count);
sortOrder->title   = needMem(sortOrder->count*sizeof(char*));
sortOrder->forward = needMem(sortOrder->count*sizeof(boolean));
sortOrder->order   = needMem(sortOrder->count*sizeof(int));
if (cart && foundColumns < sortOrder->count)
    {
    int columnCount = foundColumns;
    int size = 0;
    char *moreOrder = NULL;
    if (cart && columnCount < sortOrder->count && !stringIn(SORT_ON_TRACK_NAME,setting))
        {
        assert(sortOrder->column[columnCount] == NULL);
        sortOrder->column[columnCount] = cloneString(SORT_ON_TRACK_NAME "=+");
        if (!stringIn(SORT_ON_TRACK_NAME,sortOrder->sortOrder))
            {
            size = strlen(sortOrder->sortOrder) + strlen(sortOrder->column[columnCount]) + 5; // little bit more
            moreOrder = needMem(size);
            safef(moreOrder,size,"%s %s",sortOrder->sortOrder, sortOrder->column[columnCount]);
            freeMem(sortOrder->sortOrder);
            sortOrder->sortOrder = moreOrder;
            }
        columnCount++;
        }
    if (cart && columnCount < sortOrder->count && !stringIn(SORT_ON_RESTRICTED,setting))
        {
        assert(sortOrder->column[columnCount] == NULL);
        sortOrder->column[columnCount] = cloneString(SORT_ON_RESTRICTED "=+");
        if (!stringIn(SORT_ON_RESTRICTED,sortOrder->sortOrder))
            {
            size = strlen(sortOrder->sortOrder) + strlen(sortOrder->column[columnCount]) + 5; // little bit more
            moreOrder = needMem(size);
            safef(moreOrder,size,"%s %s",sortOrder->sortOrder, sortOrder->column[columnCount]);
            freeMem(sortOrder->sortOrder);
            sortOrder->sortOrder = moreOrder;
            }
        columnCount++;
        }
    }
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
    if (ix < foundColumns)
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

return strcasecmp(a->tdb->shortLabel, b->tdb->shortLabel); // Last chance
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
    name = tdb->track;

setting = cloneString(setting);
char *filters[10];
// multiple filterBys are delimited by space but spaces inside filter can be protected "by quotes"
int filterCount = chopByWhiteRespectDoubleQuotes(setting, filters, ArraySize(filters));
int ix;
for(ix=0;ix<filterCount;ix++)
    {
    char *filter = cloneString(filters[ix]);
    filterBy_t *filterBy;
    AllocVar(filterBy);
    char *first = strchr(filter,':');
    if (first != NULL)
        *first = '\0';
    else
        errAbort("filterBySetGet() expected ':' divider between table column and label.");
    filterBy->column = filter;
    filter += strlen(filter) + 1;
    first = strchr(filter,'=');
    if (first != NULL)
        *first = '\0';
    else
        errAbort("filterBySetGet() expected '=' divider between table column and options list.");
    filterBy->title = strSwapChar(filter,'_',' '); // Title does not have underscores
    filter += strlen(filter) + 1;

    // Are values indexes to the string titles?
    if(filter[0] == '+')
        {
        filter += 1;
        filterBy->useIndex = TRUE;
        }

    // Now set up each of the values which may have 1-3 parts (value|label{style})
    // the slName list will have the 3 parts delimited by null value\0label\0style\0
    stripString(filter, "\"");  // Remove any double quotes now and chop by commmas
    filterBy->slValues = slNameListFromComma(filter);
    struct slName *val = filterBy->slValues;
    for(;val!=NULL;val=val->next)
        {
        // chip the style off the end of value or value|label
        char *chipper = strrchr(val->name,'{');
        if (chipper != NULL)
            {
            if (val == filterBy->slValues) // First one
                {
                filterBy->styleFollows = (lastChar(chipper) == '}');
                if (filterBy->styleFollows == FALSE) // Must be closed at the end of the string or
                    filterBy->styleFollows = (*(chipper + 1) == '#'); // Legacy: color only
                }
            if (filterBy->styleFollows == FALSE)
                errAbort("filterBy values either all end in {CSS style} or none do.");
            *chipper++ = 0;  // delimit by null
            char *end = chipper + (strlen(chipper) - 1);
            if (*end == '}')
                *end = 0;
            else if (*(chipper + 1) != '#') // Legacy: Could be color only definition
                errAbort("filterBy values ending in style must be enclosed in {curly brackets}.");
            }
        else if (filterBy->styleFollows)
            errAbort("filterBy values either all end in {CSS style} or none do.");

        if (filterBy->useIndex)
            strSwapChar(val->name,'_',' '); // value is a label so swap underscores
        else
            {
            // now chip the label off the end of value name
            chipper =strchr(val->name,'|');
            if (chipper != NULL)
                {
                if (val == filterBy->slValues) // First one
                    filterBy->valueAndLabel = TRUE;
                if (filterBy->valueAndLabel == FALSE)
                    errAbort("filterBy values either all have labels (as value|label) or none do.");
                *chipper++ = 0;  // The label is found inside the filters->svValues as the next string
                strSwapChar(chipper,'_',' '); // Title does not have underscores
                }
            else if (filterBy->valueAndLabel)
                errAbort("filterBy values either all have labels in form of value|label or none do.");
        }
        }

    slAddTail(&filterBySet,filterBy); // Keep them in order (only a few)

    if(cart != NULL)
        {
        char suffix[256];
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

static char *filterByClauseStd(filterBy_t *filterBy)
/* returns the SQL where clause for a single filterBy struct in
 * the standard cases */
{
int count = slCount(filterBy->slChoices);
struct dyString *dyClause = newDyString(256);
dyStringAppend(dyClause, filterBy->column);
if(count == 1)
    dyStringPrintf(dyClause, " = ");
else
    dyStringPrintf(dyClause, " in (");

struct slName *slChoice = NULL;
boolean first = TRUE;
for(slChoice = filterBy->slChoices;slChoice != NULL;slChoice=slChoice->next)
    {
    if(!first)
        dyStringAppend(dyClause, ",");
    first = FALSE;
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

char *filterByClause(filterBy_t *filterBy)
/* returns the SQL where clause for a single filterBy struct */
{
if ((filterBy->slChoices == NULL) || (slNameInList(filterBy->slChoices,"All")))
    return NULL;
else
    return filterByClauseStd(filterBy);
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

void filterBySetCfgUi(struct trackDb *tdb, filterBy_t *filterBySet, boolean onOneLine)
/* Does the UI for a list of filterBy structure */
{
if(filterBySet == NULL)
    return;

#define FILTERBY_HELP_LINK  "<A HREF=\"../goldenPath/help/multiView.html\" TARGET=ucscHelp>help</A>"
int count = slCount(filterBySet);
if(count == 1)
    puts("<TABLE cellpadding=3><TR valign='top'>");
else
    printf("<B>Filter items by:</B> (select multiple categories and items - %s)<TABLE cellpadding=3><TR valign='top'>\n",FILTERBY_HELP_LINK);

filterBy_t *filterBy = NULL;
webIncludeResourceFile("ui.dropdownchecklist.css");
jsIncludeFile("ui.dropdownchecklist.js",NULL);
#ifdef NEW_JQUERY
jsIncludeFile("ddcl.js",NULL);
#endif///def NEW_JQUERY

int ix=0;
for(filterBy = filterBySet;filterBy != NULL; filterBy = filterBy->next)
    {
    puts("<TD>");
    if(count == 1)
        printf("<B>Filter by %s</B> (select multiple items - %s)",filterBy->title,FILTERBY_HELP_LINK);
    else
        printf("<B>%s</B>",filterBy->title);
    printf("<BR>\n");

    // TODO: columnCount (Number of filterBoxes per row) should be configurable through tdb setting
    #ifdef NEW_JQUERY
        #define FILTER_BY_FORMAT "<SELECT id='fbc%d' name='%s.filterBy.%s' multiple style='display: none; font-size:.9em;' class='filterBy'><BR>\n"
    #else///ifndef NEW_JQUERY
        #define FILTER_BY_FORMAT "<SELECT id='fbc%d' name='%s.filterBy.%s' multiple style='display: none;' class='filterBy'><BR>\n"
    #endif///ndef NEW_JQUERY
    printf(FILTER_BY_FORMAT,ix,tdb->track,filterBy->column);
    ix++;
    printf("<OPTION%s>All</OPTION>\n",(filterBy->slChoices == NULL || slNameInList(filterBy->slChoices,"All")?" SELECTED":""));
    struct slName *slValue;

    int ix=1;
    for(slValue=filterBy->slValues;slValue!=NULL;slValue=slValue->next,ix++)
        {
        char varName[32];
        char *label = NULL;
        char *name = NULL;
        if (filterBy->useIndex)
            {
            safef(varName, sizeof(varName), "%d",ix);
            name = varName;
            label = slValue->name;
            }
        else
            {
            label = (filterBy->valueAndLabel? slValue->name + strlen(slValue->name)+1: slValue->name);
            name = slValue->name;
            }
        printf("<OPTION");
        if (filterBy->slChoices != NULL && slNameInList(filterBy->slChoices,name))
            printf(" SELECTED");
        if (filterBy->useIndex || filterBy->valueAndLabel)
            printf(" value='%s'",name);
        if (filterBy->styleFollows)
            {
            char *styler = label + strlen(label)+1;
            if (*styler != '\0')
                {
                if (*styler == '#') // Legacy: just the color that follows
                    printf(" style='color: %s;'",styler);
                else
                    printf(" style='%s'",styler);
                }
            }
        printf(">%s</OPTION>\n",label);
        }
    }
    printf("</SELECT>\n");

#ifndef NEW_JQUERY
    // The following is needed to make msie scroll to selected option.
    printf("<script type='text/javascript'>onload=function(){ if( $.browser.msie ) { $(\"select[name^='%s.filterBy.']\").children('option[selected]').each( function(i) { $(this).attr('selected',true); }); }}</script>\n",tdb->track);
#endif///ndef NEW_JQUERY
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
dyStringPrintf(id,"cb_%s_", tdb->track);
int dimIx=1; // Skips over view dim for now
for(;dimIx<dimMax;dimIx++)
    {
    if(dims[dimIx] != NULL)
        {
        ix = stringArrayIx(dims[dimIx]->groupTag, membership->subgroups, membership->count);
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

// FIXME FIXME Should be able to use membersForAll struct to set default sort order from subGroups
// FIXME FIXME This should be done in hgTrackDb at load time and should change tag values to ensure js still works
boolean tdbAddPrioritiesFromCart(struct cart *cart, struct trackDb *tdbList)
/* Updates the tdb->priority from cart for all tracks in list and their
 * descendents. */
{
char htmlIdentifier[128];
struct trackDb *tdb;
boolean cartPriorities = FALSE;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    safef(htmlIdentifier, sizeof(htmlIdentifier), "%s.priority", tdb->track);
    char *cartHas = cartOptionalString(cart,htmlIdentifier);
    if(cartHas != NULL)
	{
	tdb->priority = atof(cartHas);
	cartPriorities = TRUE;
	}
    if (tdbAddPrioritiesFromCart(cart, tdb->subtracks))
        cartPriorities = TRUE;
    }
return cartPriorities;
}

boolean tdbSortPrioritiesFromCart(struct cart *cart, struct trackDb **tdbList)
/* Updates the tdb->priority from cart then sorts the list anew.
   Returns TRUE if priorities obtained from cart */
{
boolean cartPriorities = tdbAddPrioritiesFromCart(cart, *tdbList);
slSort(tdbList, trackDbCmp);
return cartPriorities;
}

boolean tdbRefSortPrioritiesFromCart(struct cart *cart, struct slRef **tdbRefList)
/* Updates the tdb->priority from cart then sorts the list anew.
   Returns TRUE if priorities obtained from cart */
{
char htmlIdentifier[128];
struct slRef *tdbRef;
boolean cartPriorities = FALSE;
for (tdbRef = *tdbRefList; tdbRef != NULL; tdbRef = tdbRef->next)
    {
    struct trackDb *tdb = tdbRef->val;
    safef(htmlIdentifier, sizeof(htmlIdentifier), "%s.priority", tdb->track);
    char *cartHas = cartOptionalString(cart,htmlIdentifier);
    if(cartHas != NULL)
	{
	tdb->priority = atof(cartHas);
	cartPriorities = TRUE;
	}
    }
slSort(tdbRefList, trackDbRefCmp);
return cartPriorities;
}

void cfgByCfgType(eCfgType cType,char *db, struct cart *cart, struct trackDb *tdb,char *prefix, char *title, boolean boxed)
// Methods for putting up type specific cfgs used by composites/subtracks in hui.c and exported for common use
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
#ifdef USE_BAM
    case cfgBam:        bamCfgUi(cart, tdb, prefix, title, boxed);
			break;
#endif
    case cfgVcf:	vcfCfgUi(cart, tdb, prefix, title, boxed);
			break;
    case cfgPsl:	pslCfgUi(db,cart,tdb,prefix,title,boxed);
                        break;
    default:            warn("Track type is not known to multi-view composites. type is: %d ", cType);
                        break;
    }
}

char *encodeRestrictionDate(char *db,struct trackDb *trackDb,boolean excludePast)
/* Create a string for ENCODE restriction date of this track
   if return is not null, then free it after use */
{
if (!trackDb)
    return NULL;

char *date = NULL;

if(metadataForTable(db,trackDb,NULL) != NULL)
    {
    date = cloneString((char *)metadataFindValue(trackDb,"dateUnrestricted"));
    if (date != NULL)
        date = strSwapChar(date, ' ', 0);   // Truncate time (not expected, but just in case)

    if (excludePast && !isEmpty(date) && dateIsOld(date,"%F"))
        freez(&date);
    }
return date;
}

static void cfgLinkToDependentCfgs(struct cart *cart, struct trackDb *tdb,char *prefix)
/* Link composite or view level controls to all associateled lower level controls */
{
if (!cartVarExists(cart, "ajax") && tdbIsComposite(tdb))
#ifdef SUBTRACK_CFG_POPUP
    printf("<script type='text/javascript'>registerViewOnchangeAction('%s')</script>\n",prefix);
#else///ifndef SUBTRACK_CFG_POPUP
    printf("<script type='text/javascript'>compositeCfgRegisterOnchangeAction(\"%s\")</script>\n",prefix);
#endif///ndef SUBTRACK_CFG_POPUP
}

static void compositeUiSubtracks(char *db, struct cart *cart, struct trackDb *parentTdb,struct hash *trackHash)
/* Display list of subtracks and descriptions with checkboxes to control visibility and possibly other
 * nice things including links to schema and metadata and a release date. */
{
struct trackDb *subtrack;
struct dyString *dyHtml = newDyString(SMALLBUF);
//char *colors[2]   = { COLOR_BG_DEFAULT,
//                      COLOR_BG_ALTDEFAULT };
char *colors[2]   = { "bgLevel1",
                      "bgLevel1" };
int colorIx = COLOR_BG_DEFAULT_IX; // Start with non-default allows alternation
#ifndef SUBTRACK_CFG_POPUP
boolean dependentCfgsNeedBinding = FALSE;
#endif///ndef SUBTRACK_CFG_POPUP

// Get list of leaf subtracks to work with
struct slRef *subtrackRef, *subtrackRefList = trackDbListGetRefsToDescendantLeaves(parentTdb->subtracks);

// Look for dividers, heirarchy, dimensions, sort and dragAndDrop!
char **lastDivide = NULL;
dividers_t *dividers = dividersSettingGet(parentTdb);
if (dividers)
    lastDivide = needMem(sizeof(char*)*dividers->count);
hierarchy_t *hierarchy = hierarchySettingGet(parentTdb);

membersForAll_t* membersForAll = membersForAllSubGroupsGet(parentTdb,NULL);
int dimCount=0,di;
for(di=0;di<membersForAll->dimMax;di++) { if (membersForAll->members[di]) dimCount++; }
sortOrder_t* sortOrder = sortOrderGet(cart,parentTdb);
boolean preSorted = FALSE;
boolean useDragAndDrop = sameOk("subTracks",trackDbSetting(parentTdb, "dragAndDrop"));
boolean displayAll = sameString(cartUsualString(cart, "displaySubtracks", "all"), "all");

// Determine whether there is a restricted until date column
boolean restrictions = FALSE;
for (subtrackRef = subtrackRefList; subtrackRef != NULL; subtrackRef = subtrackRef->next)
    {
    subtrack = subtrackRef->val;
    (void)metadataForTable(db,subtrack,NULL);
    if (NULL != metadataFindValue(subtrack,"dateUnrestricted"))
        {
        restrictions = TRUE;
        break;
        }
    }

// Table wraps around entire list so that "Top" link can float to the correct place.
cgiDown(0.7);
printf("<table><tr><td class='windowSize'>");
printf("<A NAME='DISPLAY_SUBTRACKS'></A>");
if (sortOrder != NULL)
    {
    // First table row contains the display "selected/visible" or "all" radio buttons
    // NOTE: list subtrack radio buttons are inside tracklist table header if there are no sort columns
    //       The reason is to ensure spacing of lines column headers when the only column header is "Restricted Until"
    printf("<B>List subtracks:&nbsp;");
    char javascript[JBUFSIZE];
    safef(javascript, sizeof(javascript), "onclick=\"showOrHideSelectedSubtracks(true);\"");
    cgiMakeOnClickRadioButton("displaySubtracks", "selected", !displayAll,javascript);
    puts("only selected/visible &nbsp;&nbsp;");
    safef(javascript, sizeof(javascript), "onclick=\"showOrHideSelectedSubtracks(false);\"");
    cgiMakeOnClickRadioButton("displaySubtracks", "all", displayAll,javascript);
    printf("all</B>");
    if (slCount(subtrackRefList) > 5)
        printf("&nbsp;&nbsp;&nbsp;&nbsp;(<span class='subCBcount'></span>)");
    makeTopLink(parentTdb);
    printf("</td></tr></table>");
    }
else
    makeTopLink(parentTdb);

// Now we can start in on the table of subtracks  It may be sortable and/or dragAndDroppable
printf("\n<TABLE CELLSPACING='2' CELLPADDING='0' border='0'");
dyStringClear(dyHtml);
if (sortOrder != NULL)
    dyStringPrintf(dyHtml, "sortable");
if (useDragAndDrop)
    {
    if (dyStringLen(dyHtml) > 0)
        dyStringAppendC(dyHtml,' ');
    dyStringPrintf(dyHtml, "tableWithDragAndDrop");
    }
printf(" class='subtracks");
if (dyStringLen(dyHtml) > 0)
    {
    printf(" bglevel1 %s'",dyStringContents(dyHtml));
    colorIx = COLOR_BG_ALTDEFAULT_IX;
    }
if (sortOrder != NULL)
    puts("'><THEAD class=sortable>");
else
    puts("'><THEAD>");

boolean doColorPatch = trackDbSettingOn(parentTdb, "showSubtrackColorOnUi");
int colspan = 3;
if (sortOrder != NULL)
    colspan = sortOrder->count+2;
if (doColorPatch)
    colspan += 1;
int columnCount = 0;
if (sortOrder != NULL)
    printf("<TR id=\"subtracksHeader\" class='sortable%s'>\n",useDragAndDrop?" nodrop nodrag":"");
else
    {
    printf("<TR%s>",useDragAndDrop?" id='noDrag' class='nodrop nodrag'":"");
    // First table row contains the display "selected/visible" or "all" radio buttons
    // NOTE: list subtrack radio buttons are inside tracklist table header if there are no sort columns
    //       The reason is to ensure spacing of lines column headers when the only column header is "Restricted Until"
    printf("<TD colspan='%d'><B>List subtracks:&nbsp;", colspan);
    char javascript[JBUFSIZE];
    safef(javascript, sizeof(javascript), "onclick=\"showOrHideSelectedSubtracks(true);\"");
    cgiMakeOnClickRadioButton("displaySubtracks", "selected", !displayAll,javascript);
    puts("only selected/visible &nbsp;&nbsp;");
    safef(javascript, sizeof(javascript), "onclick=\"showOrHideSelectedSubtracks(false);\"");
    cgiMakeOnClickRadioButton("displaySubtracks", "all", displayAll,javascript);
    printf("all</B>");
    if (slCount(subtrackRefList) > 5)
        printf("&nbsp;&nbsp;&nbsp;&nbsp;(<span class='subCBcount'></span>)");
    puts("</TD>");
    columnCount = colspan;
    }

// Add column headers which are sort button links
if (sortOrder != NULL)
    {
    printf("<TH>&nbsp;<INPUT TYPE=HIDDEN NAME='%s' class='sortOrder' VALUE='%s'></TH>\n", sortOrder->htmlId, sortOrder->sortOrder); // keeing track of sortOrder
    columnCount++;
    // Columns in tdb order (unchanging), sort in cart order (changed by user action)
    int sIx=0;
    for(sIx=0;sIx<sortOrder->count;sIx++)
        {
        if (sameString(SORT_ON_TRACK_NAME,sortOrder->column[sIx]))
            break; // All wrangler requested sort orders have been done.
        if (sameString(SORT_ON_RESTRICTED,sortOrder->column[sIx]))
            break; // All wrangler requested sort orders have been done.
        printf("<TH id='%s' class='sortable%s sort%d' abbr='use' onclick='tableSortAtButtonPress(this);'>%s",
            sortOrder->column[sIx],(sortOrder->forward[sIx]?"":" sortRev"),sortOrder->order[sIx],sortOrder->title[sIx]);
        printf("<sup>%s",(sortOrder->forward[sIx]?"&darr;":"&uarr;"));
        if (sortOrder->count > 1)
            printf ("%d",sortOrder->order[sIx]);
        printf("</sup>");
        puts ("</TH>");
        columnCount++;
        }

    // longLabel column
    assert(sameString(SORT_ON_TRACK_NAME,sortOrder->column[sIx]));
    printf("<TH id='%s' class='sortable%s sort%d' onclick='tableSortAtButtonPress(this);' align='left'>&nbsp;&nbsp;Track Name",
           sortOrder->column[sIx],(sortOrder->forward[sIx]?"":" sortRev"),sortOrder->order[sIx]);
    printf("<sup>%s%d</sup>",(sortOrder->forward[sIx]?"&darr;":"&uarr;"),sortOrder->order[sIx]);
    puts ("</TH>");
    columnCount++;
    }
puts("<TH>&nbsp;</TH>"); // schema column
columnCount++;

// Finally there may be a restricted until column
if (restrictions)
    {
    if (sortOrder != NULL)
        {
        int sIx=sortOrder->count-1;
        assert(sameString(SORT_ON_RESTRICTED,sortOrder->column[sIx]));
        printf("<TH id='%s' class='sortable%s sort%d' onclick='tableSortAtButtonPress(this);' align='left'>&nbsp;Restricted Until",
            sortOrder->column[sIx],(sortOrder->forward[sIx]?"":" sortRev"),sortOrder->order[sIx]);
        printf("<sup>%s%d</sup>",(sortOrder->forward[sIx]?"&darr;":"&uarr;"),sortOrder->order[sIx]);
        //printf("<span class='bgLevel1' style='height:100%%;'><A HREF=\'%s\' TARGET=BLANK>&nbsp;?&nbsp;</A></span>", ENCODE_DATA_RELEASE_POLICY);
        puts ("</TH>");
        }
    else
        {
        printf("<TH align='center'>&nbsp;");
        printf("<A HREF=\'%s\' TARGET=BLANK>Restricted Until</A>", ENCODE_DATA_RELEASE_POLICY);
        puts("&nbsp;</TH>");
        }
    columnCount++;
    }
puts("</TR></THEAD>"); // The end of the header section.

// The subtracks need to be sorted by priority but only sortable and dragable will have non-default (cart) priorities to sort on
if (sortOrder != NULL || useDragAndDrop)
    {
    preSorted = tdbRefSortPrioritiesFromCart(cart, &subtrackRefList); // preserves user's prev sort/drags
    printf("<TBODY class='%saltColors'>\n",(sortOrder != NULL ? "sortable " : "") );
    }
else
    {
    slSort(&subtrackRefList, trackDbRefCmp);  // straight from trackDb.ra
    preSorted = TRUE;
    puts("<TBODY>");
    }

// Finally the big "for loop" to list each subtrack as a table row.
for (subtrackRef = subtrackRefList; subtrackRef != NULL; subtrackRef = subtrackRef->next)
    {
    subtrack = subtrackRef->val;
    int ix;

    // Determine whether subtrack is checked, visible, configurable, has group membership, etc.
    int fourState = subtrackFourStateChecked(subtrack,cart);
    boolean checkedCB = fourStateChecked(fourState);
    boolean enabledCB = fourStateEnabled(fourState);
    eCfgType cType = cfgTypeFromTdb(subtrack,FALSE);
#ifdef SUBTRACK_CFG_POPUP
    // Turn this off only if configurable explicitly set to off
    if (trackDbSettingClosestToHome(subtrack, "configurable") && trackDbSettingClosestToHomeOn(subtrack, "configurable") == FALSE)
#else///ifndef SUBTRACK_CFG_POPUP
    if (trackDbSettingClosestToHomeOn(subtrack, "configurable") == FALSE)
#endif///ndef SUBTRACK_CFG_POPUP
        cType = cfgNone;
    membership_t *membership = subgroupMembershipGet(subtrack);

    if (sortOrder == NULL && !useDragAndDrop)
        {
        if ( divisionIfNeeded(lastDivide,dividers,membership) )
            colorIx = (colorIx == COLOR_BG_DEFAULT_IX ? COLOR_BG_ALTDEFAULT_IX : COLOR_BG_DEFAULT_IX);
        }

    // Start the TR which must have an id that is directly related to the checkBox id
    char *id = checkBoxIdMakeForTrack(subtrack,membersForAll->members,membersForAll->dimMax,membership); // view is known tag
    printf("<TR valign='top' class='%s%s'",colors[colorIx],(useDragAndDrop?" trDraggable":""));
    printf(" id=tr_%s p%s>\n",id,(!displayAll?" style='display:none'":""));

    // Now the TD that holds the checkbox
    printf("<TD%s%s>",
           (enabledCB?"":" title='view is hidden'"),
           (useDragAndDrop?" class='dragHandle' title='Drag to reorder'":""));

    // The checkbox has identifying classes including subCB and the tag for each dimension (e.g. class='subCB GM12878 CTCF Peak')
    dyStringClear(dyHtml);
    dyStringAppend(dyHtml, "subCB"); // always first
    for(di=dimX;di<membersForAll->dimMax;di++)
        {
        if (membersForAll->members[di] && -1 != (ix = stringArrayIx(membersForAll->members[di]->groupTag, membership->subgroups, membership->count)))
            dyStringPrintf(dyHtml," %s",membership->membership[ix]);
        }
    if (membersForAll->members[dimV] && -1 != (ix = stringArrayIx(membersForAll->members[dimV]->groupTag, membership->subgroups, membership->count)))
        dyStringPrintf(dyHtml, " %s",membership->membership[ix]);  // Saved view for last

    // And finally the checkBox is made!
    char htmlIdentifier[SMALLBUF];
    safef(htmlIdentifier, sizeof(htmlIdentifier), "%s_sel", subtrack->track);
    cgiMakeCheckBoxFourWay(htmlIdentifier,checkedCB,enabledCB,id,dyStringContents(dyHtml),"onclick='matSubCbClick(this);' style='cursor:pointer'");
    if (useDragAndDrop)
        printf("&nbsp;");

    // TODO: make a "view" dropdown (fake to save rendering time) and a configurable wrench here right after the checkbox

    // A hidden field to keep track of subtrack order if it could change
    if (sortOrder != NULL || useDragAndDrop)
        {
        safef(htmlIdentifier, sizeof(htmlIdentifier), "%s.priority", subtrack->track);
        float priority = (float)cartUsualDouble(cart, htmlIdentifier, subtrack->priority);
        printf("<INPUT TYPE=HIDDEN NAME='%s' class='trPos' VALUE=\"%.0f\">", htmlIdentifier, priority); // keeing track of priority
        }

    // A color patch which helps distinguish subtracks in some types of composites
    if (doColorPatch)
        {
        printf("<TD BGCOLOR='#%02X%02X%02X'>&nbsp;&nbsp;&nbsp;&nbsp;</TD>",
                subtrack->colorR, subtrack->colorG, subtrack->colorB);
        }

    // Subtrack configuration requires a field that is a link to an embedded (or popup) dialog
#ifdef SUBTRACK_CFG_POPUP
    #define CFG_SUBTRACK_LINK  "<A HREF='#a_cfg_%s' onclick='return popUpSubtrackCfg(\"%s\",\"%s\");' title='%s'>%s</A>"
    #define MAKE_CFG_SUBTRACK_LINK(table,label,title) printf(CFG_SUBTRACK_LINK, (table),(table),(label),(label),(title))
    struct dyString *dyLabel = newDyString(128);
#else///ifndef SUBTRACK_CFG_POPUP
    #define CFG_SUBTRACK_LINK  "<A HREF='#a_cfg_%s' onclick='return subtrackCfgShow(\"%s\");' title='Subtrack Configuration'>%s</A>"
    #define MAKE_CFG_SUBTRACK_LINK(table,title) printf(CFG_SUBTRACK_LINK, (table),(table),(title))
#endif///ndef SUBTRACK_CFG_POPUP

    // If sortable, then there must be a column per sortable dimension
    if (sortOrder != NULL)
        {
        int sIx=0;
        for(sIx=0;sIx<sortOrder->count;sIx++)
            {
            ix = stringArrayIx(sortOrder->column[sIx], membership->subgroups, membership->count); // TODO: Sort needs to expand from subGroups to labels as well
            if (ix >= 0)
                {
                char *titleRoot=NULL;
                if (cvTermIsEmpty(sortOrder->column[sIx],membership->titles[ix]))
                    titleRoot = cloneString(" &nbsp;");
                else
                    titleRoot = labelRoot(membership->titles[ix],NULL);
                // Each sortable column requires hidden goop (in the "abbr" field currently) which is the actual sort on value
                printf ("<TD id='%s_%s' abbr='%s' align='left'>&nbsp;",subtrack->track,sortOrder->column[sIx],membership->membership[ix]);
            #ifdef SUBTRACK_CFG_POPUP
                dyStringPrintf(dyLabel,"%s ",titleRoot);
                if (cType != cfgNone && sameString("view",sortOrder->column[sIx])) // configure link is on view currenntly  TODO: make a wrench next to check box/view
                    {
                    dyStringAppend(dyLabel,"Configuration");
                    MAKE_CFG_SUBTRACK_LINK(subtrack->track,dyStringContents(dyLabel),titleRoot);
                    }
            #else///ifndef SUBTRACK_CFG_POPUP
                if (cType != cfgNone && sameString("view",sortOrder->column[sIx]))
                    MAKE_CFG_SUBTRACK_LINK(subtrack->track,titleRoot);  // FIXME: Currently configurable under sort only supported when multiview
            #endif///ndef SUBTRACK_CFG_POPUP
                else
                    printf("%s",titleRoot);
                puts ("</TD>");
                freeMem(titleRoot);
                }
            }
        }
    else  // Non-sortable tables do not have sort by columns but will display a short label (which may be a configurable link)
        {
        printf ("<TD>&nbsp;");
        indentIfNeeded(hierarchy,membership);
    #ifdef SUBTRACK_CFG_POPUP
        if (cType != cfgNone && cType != cfgWigMaf)  // FIXME: wigMaf restriction is temporary until configureByPopup off is set
            MAKE_CFG_SUBTRACK_LINK(subtrack->track,subtrack->shortLabel,subtrack->shortLabel);
    #else///ifndef SUBTRACK_CFG_POPUP
        if (cType != cfgNone)
            MAKE_CFG_SUBTRACK_LINK(subtrack->track,subtrack->shortLabel);
    #endif///ndef SUBTRACK_CFG_POPUP
        else
            printf("%s",subtrack->shortLabel);
        puts ("</TD>");
        }
#ifdef SUBTRACK_CFG_POPUP
    dyStringFree(&dyLabel);
#endif///def SUBTRACK_CFG_POPUP

    // The long label column (note that it may have a "..." that allows getting at all the metadata)
    printf ("<TD title='select to copy'>&nbsp;%s", subtrack->longLabel);
    if (trackDbSetting(parentTdb, "wgEncode") && trackDbSetting(subtrack, "accession"))
        printf (" [GEO:%s]", trackDbSetting(subtrack, "accession"));
    compositeMetadataToggle(db,subtrack,NULL,TRUE,FALSE, trackHash);
    printf("&nbsp;");

#ifndef SUBTRACK_CFG_POPUP
    // Embedded cfg dialogs are within the TD that contains the longLabel.  This allows a wide item to be embedded in the table
    if (cType != cfgNone)
        {
        dependentCfgsNeedBinding = TRUE; // configurable subtrack needs to be bound to composite settings
    #define CFG_SUBTRACK_DIV "<DIV id='div_%s_cfg'%s><INPUT TYPE=HIDDEN NAME='%s' value='%s'>\n"
    #define MAKE_CFG_SUBTRACK_DIV(table,cfgVar,open) printf(CFG_SUBTRACK_DIV,(table),((open)?"":" style='display:none'"),(cfgVar),((open)?"on":"off"))
        safef(htmlIdentifier,sizeof(htmlIdentifier),"%s.childShowCfg",subtrack->track);
        boolean open = cartUsualBoolean(cart, htmlIdentifier,FALSE);
        MAKE_CFG_SUBTRACK_DIV(subtrack->track,htmlIdentifier,open);
        safef(htmlIdentifier,sizeof(htmlIdentifier),"%s",subtrack->track);
        cfgByCfgType(cType,db,cart,subtrack,htmlIdentifier,"Subtrack",TRUE);
        printf("</DIV>");
        }
#endif///ndef SUBTRACK_CFG_POPUP

    // A schema link for each track
    printf("</td>\n<TD>&nbsp;");
    makeSchemaLink(db,subtrack,"schema");
    printf("&nbsp;");

    // Do we have a restricted until date?
    if (restrictions)
        {
        char *dateDisplay = encodeRestrictionDate(db,subtrack,FALSE); // includes dates in the past
        if (dateDisplay)
            {
            if (dateIsOld(dateDisplay,"%F"))
                printf("</TD>\n<TD align='center' nowrap style='color: #BBBBBB;'>&nbsp;%s&nbsp;", dateDisplay);
            else
                printf("</TD>\n<TD align='center'>&nbsp;%s&nbsp;", dateDisplay);
            }
        }

    // End of row and free ourselves of this subtrack
    puts("</TD></TR>\n");
    checkBoxIdFree(&id);
    }

// End of the table
puts("</TBODY>");
if (slCount(subtrackRefList) > 5 || (restrictions && sortOrder != NULL))
    {
    printf("<TFOOT style='background-color:%s;'><TR valign='top'><TD colspan=%d>&nbsp;&nbsp;&nbsp;&nbsp;",
           COLOR_BG_DEFAULT_DARKER,columnCount-1);

    // Count of subtracks is filled in by javascript.
    if (slCount(subtrackRefList) > 5)
        printf("<span class='subCBcount'></span>\n");

    // Restruction policy needs a link
    if (restrictions && sortOrder != NULL)
        printf("</TD><TH><A HREF='%s' TARGET=BLANK style='font-size:.9em;'>Restriction Policy</A>", ENCODE_DATA_RELEASE_POLICY);

    printf("</TD></TR></TFOOT>\n");
    }
puts("</TABLE>");
if (sortOrder == NULL)
    printf("</td></tr></table>");

// Tying subtracks with matrix and subtrack cfgs with views requires javascript help
puts("<script type='text/javascript'>matInitializeMatrix();</script>");
#ifndef SUBTRACK_CFG_POPUP
if (dependentCfgsNeedBinding)
    cfgLinkToDependentCfgs(cart,parentTdb,parentTdb->track);
#endif//ndef SUBTRACK_CFG_POPUP

// Finally we are free of all this
membersForAllSubGroupsFree(parentTdb,&membersForAll);
dyStringFree(&dyHtml)
sortOrderFree(&sortOrder);
dividersFree(&dividers);
hierarchyFree(&hierarchy);
}

static void compositeUiSubtracksMatchingPrimary(char *db, struct cart *cart, struct trackDb *parentTdb,char *primarySubtrack)
/* Display list of subtracks associated with a primary subtrack for the hgTables merge function */
{
assert(primarySubtrack != NULL);
char *primaryType = getPrimaryType(primarySubtrack, parentTdb);
char htmlIdentifier[SMALLBUF];

// Get list of leaf subtracks to work with and sort them
struct slRef *subtrackRef, *subtrackRefList = trackDbListGetRefsToDescendantLeaves(parentTdb->subtracks);
if (NULL != trackDbSetting(parentTdb, "sortOrder") || NULL != trackDbSetting(parentTdb, "dragAndDrop"))
    tdbRefSortPrioritiesFromCart(cart, &subtrackRefList); // preserves user's prev sort/drags
else
    slSort(&subtrackRefList, trackDbRefCmp);  // straight from trackDb.ra

// Now we can start in on the table of subtracks
printf("\n<TABLE CELLSPACING='2' CELLPADDING='0' border='0' id='subtracks.%s'><THEAD>\n</TR></THEAD><TBODY>\n",parentTdb->track);

for (subtrackRef = subtrackRefList; subtrackRef != NULL; subtrackRef = subtrackRef->next)
    {
    struct trackDb *subtrack = subtrackRef->val;
    int fourState = subtrackFourStateChecked(subtrack,cart);
    boolean checkedCB = fourStateChecked(fourState);
    boolean enabledCB = fourStateEnabled(fourState);
    safef(htmlIdentifier, sizeof(htmlIdentifier), "%s_sel", subtrack->track);

    if (sameString(subtrack->track, primarySubtrack))
        {
        puts("<TR><TD>");
        cgiMakeHiddenBoolean(htmlIdentifier, TRUE);
        puts("[on] ");
        printf ("</TD><TD>%s [selected on main page]</TD></TR>\n",
            subtrack->longLabel);
        }
    else if (hSameTrackDbType(primaryType, subtrack->type))
        {
        puts("<TR><TD>");
        cgiMakeCheckBox(htmlIdentifier, checkedCB && enabledCB);
        printf ("</TD><TD>%s</TD></TR>\n", subtrack->longLabel);
        }
    }
puts("</TBODY><TFOOT></TFOOT>");
puts("</TABLE>");
if (slCount(subtrackRefList) > 5)
    puts("&nbsp;&nbsp;&nbsp;&nbsp;<span class='subCBcount'></span>");
puts("<P>");
if (!primarySubtrack)
    puts("<script type='text/javascript'>matInitializeMatrix();</script>");
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

boolean cfgBeginBoxAndTitle(struct trackDb *tdb, boolean boxed, char *title)
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
    printf("<TABLE class='blueBox");
    char *view = tdbGetViewName(tdb);
    if(view != NULL)
        printf(" %s",view);
    printf("' style='background-color:%s;'><TR><TD>", COLOR_BG_ALTDEFAULT);
    if (title)
        printf("<CENTER><B>%s Configuration</B></CENTER>\n", title);
    }
else if (title)
    printf("<p><B>%s &nbsp;</b>", title );
else
    printf("<p>");
return boxed;
}

void cfgEndBox(boolean boxed)
/* Handle end of box and title for individual track type settings */
{
if (boxed)
    puts("</td></tr></table>");
}

void wigCfgUi(struct cart *cart, struct trackDb *tdb, char *name, char *title, boolean boxed)
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

wigFetchMinMaxYWithCart(cart, tdb, name, &minY, &maxY, &tDbMinY, &tDbMaxY, wordCount, words);
freeMem(typeLine);

wigFetchTransformFuncWithCart(cart,tdb,name, &transformFunc);
wigFetchAlwaysZeroWithCart(cart,tdb,name, &alwaysZero);
wigFetchHorizontalGridWithCart(cart,tdb,name, &horizontalGrid);
wigFetchAutoScaleWithCart(cart,tdb,name, &autoScale);
wigFetchGraphTypeWithCart(cart,tdb,name, &lineBar);
wigFetchWindowingFunctionWithCart(cart,tdb,name, &windowingFunction);
wigFetchSmoothingWindowWithCart(cart,tdb,name, &smoothingWindow);
wigFetchYLineMarkWithCart(cart,tdb,name, &yLineMarkOnOff);
wigFetchYLineMarkValueWithCart(cart,tdb,name, &yLineMark);

printf("<TABLE BORDER=0>");

char *aggregate = trackDbSetting(tdb, "aggregate");
if (aggregate != NULL && tdb->subtracks)
    {
    char *aggregateVal = cartOrTdbString(cart, tdb, "aggregate", NULL);
    printf("<TR valign=center><th align=right>Overlay method:</th><td align=left>");
    safef(option, sizeof(option), "%s.%s", name, AGGREGATE);
    aggregateDropDown(option, aggregateVal);
    puts("</td></TR>");
    }

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
cgiMakeDoubleVarWithLimits(option, minY, "Range min", 0, NO_VALUE, NO_VALUE);
printf("</td><td align=leftv colspan=2>max:&nbsp;");
snprintf(option, sizeof(option), "%s.%s", name, MAX_Y );
cgiMakeDoubleVarWithLimits(option, maxY, "Range max", 0, NO_VALUE, NO_VALUE);
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
snprintf(option, sizeof(option), "%s.%s", name, YLINEONOFF );
wiggleYLineMarkDropDown(option, yLineMarkOnOff);
printf("</td>");
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
struct mrnaUiData *mud = newBedUiData(tdb->track);
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
    puts("\n<B>Shade of lowest-scoring items: </B>");
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
    printf("</SELECT>\n");
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
// Defaulting min and max within limits.  Sorry for the horizontal ifs, but stacking the group makes them easier to follow
if (min && limitMin && *limitMin != NO_VALUE && (*min == NO_VALUE || *min < *limitMin)) *min = *limitMin;
if (min && limitMax && *limitMax != NO_VALUE &&                      *min > *limitMax)  *min = *limitMax;
if (max && limitMax && *limitMax != NO_VALUE && (*max == NO_VALUE || *max > *limitMax)) *max = *limitMax;
if (max && limitMin && *limitMin != NO_VALUE &&                      *max < *limitMin)  *max = *limitMin;
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
// Defaulting min and max within limits.  Sorry for the horizontal ifs, but stacking the group makes them easier to follow
if (min && limitMin && (int)(*limitMin) != NO_VALUE && ((int)(*min) == NO_VALUE || *min < *limitMin)) *min = *limitMin;
if (min && limitMax && (int)(*limitMax) != NO_VALUE &&                             *min > *limitMax)  *min = *limitMax;
if (max && limitMax && (int)(*limitMax) != NO_VALUE && ((int)(*max) == NO_VALUE || *max > *limitMax)) *max = *limitMax;
if (max && limitMin && (int)(*limitMin) != NO_VALUE &&                             *max < *limitMin)  *max = *limitMin;
}

static boolean showScoreFilter(struct cart *cart, struct trackDb *tdb, boolean *opened, boolean boxed,
                               boolean compositeLevel,char *name, char *title, char *label,
                               char *scoreName, boolean isFloat)
/* Shows a score filter control with minimum value and optional range */
{
char *setting = trackDbSetting(tdb, scoreName);
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
    // NOTE: could determine isFloat = (strchr(setting,'.') != NULL);  However, historical trackDb settings of pValueFilter did not always contain '.'
    if (isFloat)
        {
        double minLimit=NO_VALUE,maxLimit=NO_VALUE;
        double minVal=minLimit,maxVal=maxLimit;
        colonPairToDoubles(setting,&minVal,&maxVal);
        getScoreFloatRangeFromCart(cart,tdb,scoreName,&minLimit,&maxLimit,&minVal,&maxVal);
        safef(varName, sizeof(varName), "%s.%s%s", name, scoreName, _MIN);
        safef(altLabel, sizeof(altLabel), "%s%s", (filterByRange?"Minimum ":""), htmlEncodeText(htmlTextStripTags(label),FALSE));
        cgiMakeDoubleVarWithLimits(varName,minVal, altLabel, 0,minLimit, maxLimit);
        if(filterByRange)
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
        }
    else
        {
        int minLimit=NO_VALUE,maxLimit=NO_VALUE;
        int minVal=minLimit,maxVal=maxLimit;
        colonPairToInts(setting,&minVal,&maxVal);
        getScoreIntRangeFromCart(cart,tdb,scoreName,&minLimit,&maxLimit,&minVal,&maxVal);
        safef(varName, sizeof(varName), "%s.%s%s", name, scoreName, filterByRange ? _MIN:"");
        safef(altLabel, sizeof(altLabel), "%s%s", (filterByRange?"Minimum ":""), label);
        cgiMakeIntVarWithLimits(varName,minVal, altLabel, 0,minLimit, maxLimit);
        if(filterByRange)
            {
            printf("<TD align='left'>to<TD align='left'>");
            safef(varName, sizeof(varName), "%s.%s%s", name, scoreName, _MAX);
            safef(altLabel, sizeof(altLabel), "%s%s", (filterByRange?"Maximum ":""), label);
            cgiMakeIntVarWithLimits(varName,maxVal, altLabel, 0,minLimit, maxLimit);
            }
        safef(altLabel, sizeof(altLabel), "%s", (filterByRange?"": "colspan=3"));
        if(minLimit != NO_VALUE && maxLimit != NO_VALUE)
            printf("<TD align='left'%s> (%d to %d)",altLabel,minLimit, maxLimit);
        else if(minLimit != NO_VALUE)
            printf("<TD align='left'%s> (minimum %d)",altLabel,minLimit);
        else if(maxLimit != NO_VALUE)
            printf("<TD align='left'%s> (maximum %d)",altLabel,maxLimit);
        else
            printf("<TD align='left'%s",altLabel);
        }
    puts("</TR>");
    return TRUE;
    }
return FALSE;
}


#ifdef ALL_SCORE_FILTERS_LOGIC
static int numericFiltersShowAll(struct cart *cart, struct trackDb *tdb, boolean *opened, boolean boxed,
                               boolean compositeLevel,char *name, char *title)
// Shows all *Filter style filters.  Note that these are in random order and have no graceful title
{
int count = 0;
struct slName *filterSettings = trackDbSettingsWildMatch(tdb, "*Filter");
if (filterSettings)
    {
    puts("<BR>");
    struct slName *filter = NULL;
    struct slPair *extras = NULL;
    char *extraFields = trackDbSetting(tdb, "extraFields");  // TODo: seems like there should be a cleaner way
    if (extraFields != NULL)
        extras = slPairListFromString(extraFields,TRUE); // Quoted strings may be okay

    while ((filter = slPopHead(&filterSettings)) != NULL)
        {
        if (differentString(filter->name,"noScoreFilter") && differentString(filter->name,"scoreFilter")) // TODO: scoreFilter could be included
            {
            char *field = cloneString(filter->name);
            int ix = strlen(field) - strlen("Filter");
            assert(ix > 0);
            field[ix] = '\0';
            // Could lookup extraFields  // TODO: Should we be using extra fields?  Could this be sorted by the order in extraFields?
            if (extras != NULL)
                {
                char *foundLabel = slPairFindVal(extras, field);
                if (foundLabel != NULL)
                    { // Found label so replace field
                    freeMem(field);
                    field = strchr(foundLabel,']');
                    if (field == NULL)
                        field = cloneString(foundLabel);
                    else
                        field = cloneString(field + 1);
                    strSwapChar(field,'_',' ');
                    }
                }
            char label[128];
            safef(label,sizeof(label),"Minimum %s",field);
            freeMem(field);
            // Determine floating point or integer
            char *setting = trackDbSetting(tdb, filter->name);
            boolean isFloat = (strchr(setting,'.') != NULL);
            showScoreFilter(cart,tdb,opened,boxed,compositeLevel,name,title,label,filter->name,isFloat);
            count++;
            }
        slNameFree(&filter);
        }
    }
if (count > 0)
    puts("</TABLE>");
return count;
}
#endif///def ALL_SCORE_FILTERS_LOGIC


void scoreCfgUi(char *db, struct cart *cart, struct trackDb *tdb, char *name, char *title,  int maxScore, boolean boxed)
/* Put up UI for filtering bed track based on a score */
{
char option[256];
boolean compositeLevel = isNameAtCompositeLevel(tdb,name);
boolean skipScoreFilter = FALSE;
filterBy_t *filterBySet = filterBySetGet(tdb,cart,name);

#ifdef ALL_SCORE_FILTERS_LOGIC
// Numeric filters are first
boolean isBoxOpened = FALSE;
if (numericFiltersShowAll(cart, tdb, &isBoxOpened, boxed, compositeLevel, name, title) > 0)
    skipScoreFilter = TRUE;
#endif///def ALL_SCORE_FILTERS_LOGIC

// Add any multi-selects next
if(filterBySet != NULL)
    {
    if(!tdbIsComposite(tdb) && cartOptionalString(cart, "ajax") == NULL)
        jsIncludeFile("hui.js",NULL);

    if (!isBoxOpened)   // Note filterBy boxes are not double "boxed", if there are no other filters
        printf("<BR>");
    filterBySetCfgUi(tdb,filterBySet,TRUE);
    filterBySetFree(&filterBySet);
    skipScoreFilter = TRUE;
    }

// For no good reason scoreFilter is incompatible with filterBy and or numericFilters
// FIXME scoreFilter should be implemented inside numericFilters and is currently specificly excluded to avoid unexpected changes
if (skipScoreFilter)
    {
    #ifdef ALL_SCORE_FILTERS_LOGIC
    if (isBoxOpened)
        cfgEndBox(boxed);
    #endif///def ALL_SCORE_FILTERS_LOGIC

    return; // Cannot have both '*filter' and 'scoreFilter'
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
    if ( !compositeLevel && hTableExists(db, tdb->table))
        printf("&nbsp; (range: 1 to 100,000 total items: %d)\n",getTableSize(db, tdb->table));
    else
        printf("&nbsp; (range: 1 to 100,000)\n");
    }
cfgEndBox(boxed);
}

void pslCfgUi(char *db, struct cart *cart, struct trackDb *tdb, char *name, char *title, boolean boxed)
/* Put up UI for psl tracks */
{
boxed = cfgBeginBoxAndTitle(tdb, boxed, title);
baseColorDropLists(cart, tdb, name);
indelShowOptionsWithName(cart, tdb, name);
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
            warn("invalid filter by %s: %s %s for track %s", field, value, limits, tdb->track);
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
        }
    }
    //if(dyStringLen(extraWhere)) warn("SELECT FROM %s WHERE %s",tdb->table,dyStringContents(extraWhere));
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
            warn("invalid filter by %s: %s %s for track %s", field, value, limits, tdb->track);
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
    //if(dyStringLen(extraWhere)) warn("SELECT FROM %s WHERE %s",tdb->table,dyStringContents(extraWhere));
return extraWhere;
}

#ifdef ALL_SCORE_FILTERS_LOGIC
struct dyString *dyAddAllScoreFilters(struct cart *cart, struct trackDb *tdb, struct dyString *extraWhere,boolean *and)
/* creates the where clause condition to gather together all random double filters
   Filters are expected to follow
        {fiterName}: trackDb min or min:max - default value(s);
        {filterName}Min or {filterName}: min (user supplied) cart variable;
        {filterName}Max: max (user supplied) cart variable;
        {filterName}Limits: trackDb allowed range "0.0:10.0" Optional
            uses:  defaultLimits: function param if no tdb limits settings found)
   The 'and' param and dyString in/out allows stringing multiple where clauses together */
{
struct slName *filterSettings = trackDbSettingsWildMatch(tdb, "*Filter");
if (filterSettings)
    {
    struct slName *filter = NULL;
    while ((filter = slPopHead(&filterSettings)) != NULL)
        {
        if (differentString(filter->name,"noScoreFilter") && differentString(filter->name,"scoreFilter")) // TODO: scoreFilter could be included
            {
            char *field = cloneString(filter->name);
            int ix = strlen(field) - strlen("filter");
            assert(ix > 0);
            field[ix] = '\0';
            char *setting = trackDbSetting(tdb, filter->name);
            // How to determine float or int ?  If actual tracDb setting has decimal places, then float!
            if (strchr(setting,'.') == NULL)
                extraWhere = dyAddFilterAsInt(cart,tdb,extraWhere,filter->name,"0:1000",field,and);
            else
                extraWhere = dyAddFilterAsDouble(cart,tdb,extraWhere,filter->name,NULL,field,and);
            }
        slNameFree(&filter);
        }
    }
return extraWhere;
}
#endif///def ALL_SCORE_FILTERS_LOGIC

void encodePeakCfgUi(struct cart *cart, struct trackDb *tdb, char *name, char *title, boolean boxed)
/* Put up UI for filtering wgEnocde peaks based on score, Pval and Qval */
{
boolean compositeLevel = isNameAtCompositeLevel(tdb,name);
boolean opened = FALSE;
showScoreFilter(cart,tdb,&opened,boxed,compositeLevel,name,title,"Minimum Signal value",     SIGNAL_FILTER,TRUE);
showScoreFilter(cart,tdb,&opened,boxed,compositeLevel,name,title,"Minimum P-Value (<code>-log<sub>10</sub></code>)",PVALUE_FILTER,TRUE);
showScoreFilter(cart,tdb,&opened,boxed,compositeLevel,name,title,"Minimum Q-Value (<code>-log<sub>10</sub></code>)",QVALUE_FILTER,TRUE);

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
else if(startsWith("wgEncodeGencode", name)
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

if(!sameString(tdb->track, "tigrGeneIndex")
&& !sameString(tdb->track, "ensGeneNonCoding")
&& !sameString(tdb->track, "encodeGencodeRaceFrags"))
    baseColorDropLists(cart, tdb, name);

if (cartOptionalString(cart, "ajax") == NULL)
    {
    filterBy_t *filterBySet = filterBySetGet(tdb,cart,name);
    if(filterBySet != NULL)
        {
        printf("<BR>");
        filterBySetCfgUi(tdb,filterBySet,FALSE);
        filterBySetFree(&filterBySet);
        }
    }
cfgEndBox(boxed);
}

static boolean isSpeciesOn(struct cart *cart, struct trackDb *tdb, char *species, char *option, int optionSize, boolean defaultState)
/* check the cart to see if species is turned off or on (default is defaultState) */
{
boolean ret = defaultState;
safef(option, optionSize, "%s.%s", tdb->track, species);

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
		tdb->parent->track, viewString,  species);
	    ret = cartUsualBoolean(cart, option, ret);
	    }
	}
    }

return ret;
}

char **wigMafGetSpecies(struct cart *cart, struct trackDb *tdb, char *db, struct wigMafSpecies **list, int *groupCt)
{
int speciesCt = 0;
char *speciesGroup = trackDbSettingClosestToHome(tdb, SPECIES_GROUP_VAR);
char *speciesUseFile = trackDbSettingClosestToHome(tdb, SPECIES_USE_FILE);
char *speciesOrder = trackDbSettingClosestToHome(tdb, SPECIES_ORDER_VAR);
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
    if (isCustomTrack(tdb->track))
	return NULL;
    errAbort("Track %s missing required trackDb setting: speciesOrder, speciesGroups, or speciesUseFile", tdb->track);
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

if(cartOptionalString(cart, "ajax") == NULL)
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
        strcpy(trackName, tdb->track);

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
char *viewString = NULL;

subgroupFind(tdb, "view", &viewString);

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

if (viewString != NULL)
    safef(option, sizeof option, "%s.%s.%s", name, viewString, MAF_DOT_VAR);
else
    safef(option, sizeof option, "%s.%s", name, MAF_DOT_VAR);
cgiMakeCheckBox(option, cartCgiUsualBoolean(cart, option, FALSE));

if (isWigMafProt)
    puts("Display amino acids identical to reference as dots<BR>" );
else
    puts("Display bases identical to reference as dots<BR>" );

if (viewString != NULL)
    safef(option, sizeof option, "%s.%s.%s", name, viewString, MAF_CHAIN_VAR);
else
    safef(option, sizeof option, "%s.%s", name, MAF_CHAIN_VAR);
cgiMakeCheckBox(option, cartCgiUsualBoolean(cart, option, TRUE));

char *irowStr = trackDbSetting(tdb, "irows");
boolean doIrows = (irowStr == NULL) || !sameString(irowStr, "off");
if (isCustomTrack(tdb->track) || doIrows)
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
    if (viewString != NULL)
	safef(buffer, sizeof(buffer), "%s.%s.codons",name, viewString);
    else
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

#ifdef USE_BAM
static char *grayLabels[] =
    { "alignment quality",
      "base qualities",
      "unpaired ends",
    };
static char *grayValues[] =
    { BAM_GRAY_MODE_ALI_QUAL,
      BAM_GRAY_MODE_BASE_QUAL,
      BAM_GRAY_MODE_UNPAIRED,
    };

// When a child input of a radio set is changed, click its radio button:
#define UPDATE_RADIO_FORMAT "%s=\"\
    var inputs = document.getElementsByName('%s'); \
    if (inputs) { \
      for (var i=0; i < inputs.length; i++) { \
        if (inputs[i].type == 'radio') { \
          inputs[i].checked = (inputs[i].value == '%s'); \
        } \
      } \
    }\""

void bamCfgUi(struct cart *cart, struct trackDb *tdb, char *name, char *title, boolean boxed)
/* BAM: short-read-oriented alignment file format. */
{
boxed = cfgBeginBoxAndTitle(tdb, boxed, title);
char cartVarName[1024];

printf("<TABLE%s><TR><TD>",boxed?" width='100%'":"");
char *tdbShowNames = trackDbSetting(tdb, BAM_SHOW_NAMES);
safef(cartVarName, sizeof(cartVarName), "%s.%s", name, BAM_SHOW_NAMES);
cartMakeCheckBox(cart, cartVarName, !sameOk(tdbShowNames, "off"));
printf("</TD><TD>Display read names</TD>");
if (boxed && fileExists(hHelpFile("hgBamTrackHelp")))
    printf("<TD style='text-align:right'><A HREF=\"../goldenPath/help/hgBamTrackHelp.html\" TARGET=_BLANK>BAM "
           "configuration help</A></TD>");
printf("</TR>\n");
boolean canPair = (trackDbSetting(tdb, BAM_PAIR_ENDS_BY_NAME) != NULL);
if (canPair)
    {
    printf("<TR><TD>");
    safef(cartVarName, sizeof(cartVarName), "%s." BAM_PAIR_ENDS_BY_NAME, name);
    cartMakeCheckBox(cart, cartVarName, TRUE);
    printf("</TD><TD>Attempt to join paired end reads by name\n");
    //puts("<BR>");
    }
printf("<TR><TD colspan=2>Minimum alignment quality:\n");
safef(cartVarName, sizeof(cartVarName), "%s." BAM_MIN_ALI_QUAL, name);
cartMakeIntVar(cart, cartVarName,
               atoi(trackDbSettingOrDefault(tdb, BAM_MIN_ALI_QUAL, BAM_MIN_ALI_QUAL_DEFAULT)), 4);
printf("</TD></TR></TABLE>");


if (isCustomTrack(name))
    {
    // Auto-magic baseColor defaults for BAM, same as in hgTracks.c newCustomTrack
    hashAdd(tdb->settingsHash, BASE_COLOR_USE_SEQUENCE, cloneString("lfExtra"));
    hashAdd(tdb->settingsHash, BASE_COLOR_DEFAULT, cloneString("diffBases"));
    hashAdd(tdb->settingsHash, SHOW_DIFF_BASES_ALL_SCALES, cloneString("."));
    hashAdd(tdb->settingsHash, "showDiffBasesMaxZoom", cloneString("100"));
    }
baseColorDropLists(cart, tdb, name);
puts("<BR>");
indelShowOptionsWithName(cart, tdb, name);
printf("<BR>\n");
printf("<B>Additional coloring modes:</B><BR>\n");
safef(cartVarName, sizeof(cartVarName), "%s." BAM_COLOR_MODE, name);
char *selected = cartUsualString(cart, cartVarName,
				 trackDbSettingOrDefault(tdb, BAM_COLOR_MODE, BAM_COLOR_MODE_DEFAULT));
cgiMakeRadioButton(cartVarName, BAM_COLOR_MODE_STRAND, sameString(selected, BAM_COLOR_MODE_STRAND));
printf("Color by strand (blue for +, red for -)<BR>\n");
cgiMakeRadioButton(cartVarName, BAM_COLOR_MODE_GRAY, sameString(selected, BAM_COLOR_MODE_GRAY));
printf("Use gray for\n");
char cartVarName2[1024];
safef(cartVarName2, sizeof(cartVarName2), "%s." BAM_GRAY_MODE, name);
int grayMenuSize = canPair ? ArraySize(grayLabels) : ArraySize(grayLabels)-1;
char *sel2 = cartUsualString(cart, cartVarName2,
			     trackDbSettingOrDefault(tdb, BAM_GRAY_MODE, BAM_GRAY_MODE_DEFAULT));
char onChange[2048];
safef(onChange, sizeof(onChange), UPDATE_RADIO_FORMAT,
      "onChange", cartVarName, BAM_COLOR_MODE_GRAY);
cgiMakeDropListFull(cartVarName2, grayLabels, grayValues, grayMenuSize, sel2, onChange);
printf("<BR>\n");
if (trackDbSetting(tdb, "noColorTag") == NULL)
    {
    cgiMakeRadioButton(cartVarName, BAM_COLOR_MODE_TAG, sameString(selected, BAM_COLOR_MODE_TAG));
    printf("Use R,G,B colors specified in user-defined tag ");
    safef(cartVarName2, sizeof(cartVarName2), "%s." BAM_COLOR_TAG, name);
    sel2 = cartUsualString(cart, cartVarName2,
			   trackDbSettingOrDefault(tdb, BAM_COLOR_TAG, BAM_COLOR_TAG_DEFAULT));
    safef(onChange, sizeof(onChange), UPDATE_RADIO_FORMAT,
	  "onkeypress", cartVarName, BAM_COLOR_MODE_TAG);
    cgiMakeTextVarWithExtraHtml(cartVarName2, sel2, 30, onChange);
    printf("<BR>\n");
    }
cgiMakeRadioButton(cartVarName, BAM_COLOR_MODE_OFF, sameString(selected, BAM_COLOR_MODE_OFF));
printf("No additional coloring");

//TODO: include / exclude flags

if (!boxed && fileExists(hHelpFile("hgBamTrackHelp")))
    printf("<P><A HREF=\"../goldenPath/help/hgBamTrackHelp.html\" TARGET=_BLANK>BAM "
	   "configuration help</A></P>");

cfgEndBox(boxed);
}
#endif//def USE_BAM

struct trackDb *rFindViewInList(struct trackDb *tdbList, char *view)
/* Return the trackDb on the list (or on any children of the list) that has matching view tag. */
{
struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    char *viewSetting = trackDbSetting(tdb, "view");
    if (sameOk(viewSetting, view))
        return tdb;
    }
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    struct trackDb *viewTdb = rFindViewInList(tdb->subtracks, view);
    if (viewTdb != NULL)
        return viewTdb;
    }
return NULL;
}

static boolean compositeViewCfgExpandedByDefault(struct trackDb *parentTdb,char *view,
	char **retVisibility)
/* returns true if the view cfg is expanded by default.  Optionally allocates string of view
 * setting (eg 'dense') */
{
boolean expanded = FALSE;
if ( retVisibility != NULL )
    *retVisibility = cloneString(hStringFromTv(parentTdb->visibility));
struct trackDb *viewTdb = rFindViewInList(parentTdb->subtracks, view);
if (viewTdb == NULL)
    return FALSE;
if (retVisibility != NULL)
    *retVisibility = cloneString(hStringFromTv(viewTdb->visibility));
if (trackDbSetting(viewTdb, "viewUi"))
    expanded = TRUE;
return expanded;
}

enum trackVisibility visCompositeViewDefault(struct trackDb *parentTdb,char *view)
/* returns the default track visibility of particular view within a composite track */
{
char *visibility = NULL;
compositeViewCfgExpandedByDefault(parentTdb,view,&visibility);
enum trackVisibility vis = hTvFromString(visibility);
freeMem(visibility);
return vis;
}

struct trackDb *rFindView(struct trackDb *forest, char *viewName)
/* Find a descendent with given view. */
{
struct trackDb *tdb;
for (tdb = forest; tdb != NULL; tdb = tdb->next)
    {
    char *viewSetting = trackDbSetting(tdb, "view");
    if (sameOk(viewSetting, viewName))
        return tdb;
    struct trackDb *view = rFindView(tdb->subtracks, viewName);
    if (view)
        return view;
    }
return NULL;
}

static boolean hCompositeDisplayViewDropDowns(char *db, struct cart *cart, struct trackDb *parentTdb)
/* UI for composite view drop down selections. */
{
int ix;
char varName[SMALLBUF];
char classes[SMALLBUF];
char javascript[JBUFSIZE];
#define CFG_LINK  "<B><A HREF=\"#a_cfg_%s\" onclick=\"return (showConfigControls('%s') == false);\" title=\"%s Configuration\">%s</A><INPUT TYPE=HIDDEN NAME='%s.%s.showCfg' value='%s'></B>"
#define MAKE_CFG_LINK(name,title,tbl,open) printf(CFG_LINK, (name),(name),(title),(title),(tbl),(name),((open)?"on":"off"))

members_t *membersOfView = subgroupMembersGet(parentTdb,"view");
if(membersOfView == NULL)
    return FALSE;

char configurable[membersOfView->count];
memset(configurable,cfgNone,sizeof(configurable));
int firstOpened = -1;
boolean makeCfgRows = FALSE;
struct trackDb **matchedSubtracks = needMem(sizeof(struct trackDb *)*membersOfView->count);

for (ix = 0; ix < membersOfView->count; ix++)
    {
    char *viewName = membersOfView->tags[ix];
    struct trackDb *view = rFindView(parentTdb->subtracks, viewName);
    if (view != NULL)
        {
        matchedSubtracks[ix] = view;
        configurable[ix] = (char)cfgTypeFromTdb(view->subtracks, TRUE);
        if(configurable[ix] != cfgNone)
            {
            if(firstOpened == -1)
                {
                safef(varName, sizeof(varName), "%s.%s.showCfg", parentTdb->track, viewName);
                if(cartUsualBoolean(cart,varName,FALSE))
                    firstOpened = ix;
                }
            makeCfgRows = TRUE;
            }
        }
    }

toLowerN(membersOfView->groupTitle, 1);
printf("<B>Select %s</B> (<A HREF='../goldenPath/help/multiView.html' title='Help on views' TARGET=_BLANK>help</A>):\n", membersOfView->groupTitle);
printf("<TABLE><TR style='text-align:left;'>\n");
// Make row of vis drop downs
for (ix = 0; ix < membersOfView->count; ix++)
    {
    struct trackDb *view = matchedSubtracks[ix];
    char *viewName = membersOfView->tags[ix];
    if (view != NULL)
        {
        printf("<TD>");
        if(configurable[ix] != cfgNone)
            {
            MAKE_CFG_LINK(membersOfView->tags[ix],membersOfView->titles[ix],parentTdb->track,(firstOpened == ix));
            }
        else
            printf("<B>%s</B>",membersOfView->titles[ix]);
        puts("</TD>");

        safef(varName, sizeof(varName), "%s.%s.vis", parentTdb->track, viewName);
        enum trackVisibility tv =
            hTvFromString(cartUsualString(cart, varName,hStringFromTv(visCompositeViewDefault(parentTdb,viewName))));

        safef(javascript, sizeof(javascript), "onchange=\"matSelectViewForSubTracks(this,'%s');\" onfocus='this.lastIndex=this.selectedIndex;'", viewName);

        printf("<TD>");
        safef(classes, sizeof(classes), "viewDD normalText %s", membersOfView->tags[ix]);
        hTvDropDownClassWithJavascript(varName, tv, parentTdb->canPack,classes,javascript);
        puts(" &nbsp; &nbsp; &nbsp;</TD>");
        }
    }
puts("</TR>");

// Make row of cfg boxes if needed
if(makeCfgRows)
    {
    puts("</TABLE><TABLE>");
    for (ix = 0; ix < membersOfView->count; ix++)
        {
        struct trackDb *view = matchedSubtracks[ix];
        if (view != NULL)
            {
            char *viewName = membersOfView->tags[ix];
            printf("<TR id=\"tr_cfg_%s\"", viewName);
            if((firstOpened == -1 && !compositeViewCfgExpandedByDefault(parentTdb,membersOfView->tags[ix],NULL))
                || (firstOpened != -1 && firstOpened != ix))
                printf(" style=\"display:none\"");
            printf("><TD width=10>&nbsp;</TD>");
            int ix2=ix;
            while(0 < ix2--)
                printf("<TD width=100>&nbsp;</TD>");
            printf("<TD colspan=%d>",membersOfView->count+1);
            safef(varName, sizeof(varName), "%s", view->track);
            if(configurable[ix] != cfgNone)
                {
                cfgByCfgType(configurable[ix],db,cart,view->subtracks,varName,
                        membersOfView->titles[ix],TRUE);
                cfgLinkToDependentCfgs(cart,parentTdb,varName);
                }
            }
        }
    }
puts("</TABLE>");
subgroupMembersFree(&membersOfView);
freeMem(matchedSubtracks);
return TRUE;
}

char *compositeLabelWithVocabLink(char *db,struct trackDb *parentTdb, struct trackDb *childTdb,
	char *vocabType, char *label)
/* If the parentTdb has a controlledVocabulary setting and the vocabType is found,
   then label will be wrapped with the link to display it.  Return string is cloned. */
{
char *vocab = trackDbSetting(parentTdb, "controlledVocabulary");
(void)metadataForTable(db,childTdb,NULL);
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
    if(sameString(vocabType,words[ix])) // controlledVocabulary setting matches tag so all labels are linked
        {
        char *link = controlledVocabLink(words[0],"term",words[ix],rootLabel,rootLabel,suffix);
        return link;
        }
    else if(countChars(words[ix],'=') == 1 && childTdb != NULL) // The name of a trackDb setting follows and will be the controlled vocab term
        {
        strSwapChar(words[ix],'=',0);
        if(sameString(vocabType,words[ix]))  // tags match, but search for term
            {
            char * cvSetting = words[ix] + strlen(words[ix]) + 1;
            const char * cvTerm = metadataFindValue(childTdb,cvSetting);
            if(cvTerm != NULL)
                {
                char *link = controlledVocabLink(words[0],(sameWord(cvSetting,"antibody")?"target":"term"),(char *)cvTerm,(char *)cvTerm,rootLabel,suffix);
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
#define MATRIX_RIGHT_BUTTONS_AFTER 8
#define MATRIX_BOTTOM_BUTTONS_AFTER 20

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

static void matrixXheadingsRow1(char *db,struct trackDb *parentTdb, membersForAll_t* membersForAll,boolean top)
/* prints the top row of a matrix: 'All' buttons; X titles; buttons 'All' */
{
members_t *dimensionX = membersForAll->members[dimX];
members_t *dimensionY = membersForAll->members[dimY];

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
        printf("<TH align=RIGHT><B><EM>%s</EM></B></TH>", dimensionX->groupTitle);
    else
        printf("<TH ALIGN=RIGHT valign=%s>&nbsp;&nbsp;<B><EM>%s</EM></B></TH>",(top?"TOP":"BOTTOM"), dimensionX->groupTitle);

    for (ixX = 0; ixX < dimensionX->count; ixX++)
        {
        if(dimensionX->subtrackList && dimensionX->subtrackList[ixX] && dimensionX->subtrackList[ixX]->val)
            {
            char *label =replaceChars(dimensionX->titles[ixX]," (","<BR>(");
            printf("<TH WIDTH='60'>&nbsp;%s&nbsp;</TH>",compositeLabelWithVocabLink(db,parentTdb,dimensionX->subtrackList[ixX]->val,dimensionX->groupTag,label));
            freeMem(label);
            cntX++;
            }
        }
    // If dimension is big enough, then add Y buttons to right as well
    if(cntX>MATRIX_RIGHT_BUTTONS_AFTER)
        {
        if(dimensionY)
            {
            printf("<TH align=LEFT><B><EM>%s</EM></B></TH>", dimensionX->groupTitle);
            printf("<TH ALIGN=RIGHT valign=%s>All&nbsp;",top?"TOP":"BOTTOM");
            buttonsForAll();
            puts("</TH>");
            }
        else
            printf("<TH ALIGN=LEFT valign=%s><B><EM>%s</EM></B>&nbsp;&nbsp;</TH>",top?"TOP":"BOTTOM", dimensionX->groupTitle);
        }
    }
else if(dimensionY)
    {
    printf("<TH ALIGN=RIGHT WIDTH=100 nowrap>");
    printf("<B><EM>%s</EM></B>", dimensionY->groupTitle);
    printf("</TH><TH ALIGN=CENTER WIDTH=60>");
    buttonsForAll();
    puts("</TH>");
    }
puts("</TR>\n");
}

static void matrixXheadingsRow2(struct trackDb *parentTdb, membersForAll_t* membersForAll)
/* prints the 2nd row of a matrix: Y title; X buttons; title Y */
{
members_t *dimensionX = membersForAll->members[dimX];
members_t *dimensionY = membersForAll->members[dimY];

// If there are both X and Y dimensions, then there is a row of buttons in X
if(dimensionX && dimensionY)
    {
    int ixX,cntX=0;
    printf("<TR ALIGN=CENTER BGCOLOR=\"%s\"><TH ALIGN=CENTER colspan=2><B><EM>%s</EM></B></TH>",COLOR_BG_ALTDEFAULT, dimensionY->groupTitle);
    for (ixX = 0; ixX < dimensionX->count; ixX++)    // Special row of +- +- +-
        {
        if(dimensionX->subtrackList && dimensionX->subtrackList[ixX] && dimensionX->subtrackList[ixX]->val)
            {
            char objName[SMALLBUF];
            puts("<TD>");
            safef(objName, sizeof(objName), "plus_%s_all", dimensionX->tags[ixX]);
            buttonsForOne( objName, dimensionX->tags[ixX] );
            puts("</TD>");
            cntX++;
            }
        }
    // If dimension is big enough, then add Y buttons to right as well
    if(cntX>MATRIX_RIGHT_BUTTONS_AFTER)
        printf("<TH ALIGN=CENTER colspan=2><B><EM>%s</EM></B></TH>", dimensionY->groupTitle);
    puts("</TR>\n");
    }
}

static void matrixXheadings(char *db,struct trackDb *parentTdb, membersForAll_t* membersForAll,boolean top)
/* UI for X headings in matrix */
{
if(top)
    matrixXheadingsRow1(db,parentTdb,membersForAll,top);

    matrixXheadingsRow2(parentTdb,membersForAll);

if(!top)
    matrixXheadingsRow1(db,parentTdb,membersForAll,top);
}

static void matrixYheadings(char *db,struct trackDb *parentTdb, membersForAll_t* membersForAll,int ixY,boolean left)
/* prints the column of Y labels and buttons */
{
members_t *dimensionX = membersForAll->members[dimX];
members_t *dimensionY = membersForAll->members[dimY];

struct trackDb *childTdb = NULL;
if(dimensionY && dimensionY->subtrackList && dimensionY->subtrackList[ixY] && dimensionY->subtrackList[ixY]->val)
    childTdb = dimensionY->subtrackList[ixY]->val;

if(dimensionX && dimensionY && childTdb != NULL) // Both X and Y, then column of buttons
    {
    char objName[SMALLBUF];
    printf("<TH ALIGN=%s nowrap colspan=2>",left?"RIGHT":"LEFT");
    if(left)
        printf("%s&nbsp;",compositeLabelWithVocabLink(db,parentTdb,childTdb,dimensionY->groupTag,dimensionY->titles[ixY]));
    safef(objName, sizeof(objName), "plus_all_%s", dimensionY->tags[ixY]);
    buttonsForOne( objName, dimensionY->tags[ixY] );
    if(!left)
        printf("&nbsp;%s",compositeLabelWithVocabLink(db,parentTdb,childTdb,dimensionY->groupTag,dimensionY->titles[ixY]));
    puts("</TH>");
    }
else if (dimensionX)
    {
    printf("<TH ALIGN=%s>",left?"RIGHT":"LEFT");
    buttonsForAll();
    puts("</TH>");
    }
else if (left && dimensionY && childTdb != NULL)
    printf("<TH ALIGN=RIGHT nowrap>%s</TH>\n",compositeLabelWithVocabLink(db,parentTdb,childTdb,dimensionY->groupTag,dimensionY->titles[ixY]));
}

static int displayABCdimensions(char *db,struct cart *cart, struct trackDb *parentTdb, struct slRef *subtrackRefList, membersForAll_t* membersForAll)
/* This will walk through all declared nonX&Y dimensions (X and Y is the 2D matrix of CBs.
   NOTE: ABC dims are only supported if there are X & Y both.  Also expected number should be passed in */
{
int count=0,ix;
for(ix=dimA;ix<membersForAll->dimMax;ix++)
    {
    if(membersForAll->members[ix]==NULL)
        continue;
    if(membersForAll->members[ix]->count<1)
        continue;
    count++;

    if(count==1) // First time set up a table
        puts("<BR><TABLE>");
    printf("<TR><TH valign=top align='right'>&nbsp;&nbsp;<B><EM>%s</EM></B>:</TH>",membersForAll->members[ix]->groupTitle);
    int aIx;
    for(aIx=0;aIx<membersForAll->members[ix]->count;aIx++)
        {
        if(membersForAll->members[ix]->tags[aIx] != NULL)
            {
            assert(membersForAll->members[ix]->subtrackList[aIx]->val != NULL);
            printf("<TH align=left nowrap>");
            char objName[SMALLBUF];
            char javascript[JBUFSIZE];
            boolean alreadySet=FALSE;
            if(membersForAll->members[ix]->selected != NULL)
                alreadySet = membersForAll->members[ix]->selected[aIx];
            safef(objName, sizeof(objName), "%s.mat_%s_dim%c_cb",parentTdb->track,membersForAll->members[ix]->tags[aIx],membersForAll->letters[ix]);
            safef(javascript,sizeof(javascript),"onclick='matCbClick(this);' class=\"matCB abc %s\"",membersForAll->members[ix]->tags[aIx]);
            // TODO Set classes properly (if needed!!!)  The class abc works but what about a b or c?
            cgiMakeCheckBoxJS(objName,alreadySet,javascript);
            printf("%s",compositeLabelWithVocabLink(db,parentTdb,membersForAll->members[ix]->subtrackList[aIx]->val,
                   membersForAll->members[ix]->groupTag,membersForAll->members[ix]->titles[aIx]));
            puts("</TH>");
            }
        }
    puts("</TR>");
    }
if(count>0)
    puts("</TABLE>");
return count;
}

#ifdef DEBUG
static void dumpDimension(members_t *dimension, char *name, FILE *f)
/* Dump out information on dimension. */
{
int count = dimension->count;
fprintf(f, "%s: count=%d tag=%s title=%s setting=%s<BR>\n", name, count, dimension->tag, dimension->title, dimension->setting);
int i;
for (i=0; i<count; ++i)
    fprintf(f, "%s=%s ", dimension->names[i], dimension->values[i]);
fprintf(f, "<BR>\n");
}
#endif /* DEBUG */

static char *labelWithVocabLinkForMultiples(char *db,struct trackDb *parentTdb, members_t* members)
/* If the parentTdb has a controlledVocabulary setting and the vocabType is found,
   then label will be wrapped with the link to all relevent terms.  Return string is cloned. */
{
assert(members->subtrackList != NULL);
char *vocab = cloneString(trackDbSetting(parentTdb, "controlledVocabulary"));
if(vocab == NULL)
    return cloneString(members->groupTitle); // No link wrapping!

char *words[15];
int count,ix;
boolean found=FALSE;
if((count = chopByWhite(vocab, words,15)) <= 1) // vocab now contains just the file name
    return cloneString(members->groupTitle);

char *mdbVar = NULL;

// Find mdb var to look up based upon the groupTag and cv setting
for(ix=1;ix<count && !found;ix++)
    {
    if(sameString(members->groupTag,words[ix])) // controlledVocabulary setting matches tag so all labels are linked
        {
        mdbVar = members->groupTag;
        break;
        }
    else if(startsWithWordByDelimiter(members->groupTag,'=',words[ix]))
        {
        mdbVar = words[ix] + strlen(members->groupTag) + 1;
        break;
        }
    }
if(mdbVar == NULL)
    {
    freeMem(vocab);
    return cloneString(members->groupTitle);
    }

#define VOCAB_MULTILINK_BEG "<A HREF='hgEncodeVocab?ra=%s&%s=\""
#define VOCAB_MULTILINK_END "\"' title='Click for details of each %s' TARGET=ucscVocab>%s</A>"
struct dyString *dyLink = dyStringCreate(VOCAB_MULTILINK_BEG,vocab,(sameWord(mdbVar,"antibody")?"target":"term"));

// Now build the comma delimited string of mdb vals (all have same mdb var)
boolean first = TRUE;
for(ix=0;ix<members->count;ix++)
    {
    if(members->subtrackList[ix] != NULL && members->subtrackList[ix]->val != NULL)
        {
        struct trackDb *childTdb = members->subtrackList[ix]->val;
        (void)metadataForTable(db,childTdb,NULL); // Makes sure this has been populated
        const char * mdbVal = metadataFindValue(childTdb,mdbVar); // one for each is enough
        if(mdbVal != NULL)
            {
            if(!first)
                dyStringAppendC(dyLink,',');
            dyStringAppend(dyLink,(char *)mdbVal);
            first = FALSE;
            }
        }
    }
dyStringPrintf(dyLink,VOCAB_MULTILINK_END,members->groupTitle,members->groupTitle);
freeMem(vocab);
return dyStringCannibalize(&dyLink);
}

static boolean compositeUiByFilter(char *db, struct cart *cart, struct trackDb *parentTdb, char *formName)
/* UI for composite tracks: filter subgroups by multiselects to select subtracks. */
{
membersForAll_t* membersForAll = membersForAllSubGroupsGet(parentTdb,cart);
if(membersForAll == NULL || membersForAll->filters == FALSE) // Not Matrix or filters
    return FALSE;
webIncludeResourceFile("ui.dropdownchecklist.css");
jsIncludeFile("ui.dropdownchecklist.js",NULL);
#ifdef NEW_JQUERY
jsIncludeFile("ddcl.js",NULL);
#endif///def NEW_JQUERY

cgiDown(0.7);
printf("<B>Filter subtracks %sby:</B> (select multiple %sitems - %s)<BR>\n",
       (membersForAll->members[dimX] != NULL || membersForAll->members[dimY] != NULL ? "further ":""),
       (membersForAll->dimMax == dimA?"":"categories and "),FILTERBY_HELP_LINK);
printf("<TABLE><TR valign='top'>\n");

// Do All [+][-] buttons
if(membersForAll->members[dimX] == NULL && membersForAll->members[dimY] == NULL) // No matrix
    {
    #define PM_BUTTON_FILTER_COMP "<input type='button' class='inOutButton' onclick=\"waitOnFunction(filterCompositeSet,this,%s); return false;\" id='btn_%s' value='%c'>"
    printf("<TD align='left' width='50px'><B>All:</B><BR>");
    printf(PM_BUTTON_FILTER_COMP,"true",  "plus_fc",'+');
    printf(PM_BUTTON_FILTER_COMP,"false","minus_fc",'-');
    //#define PM_BUTTON2_FILTER_COMP "<IMG height=18 width=18 onclick=\"filterCompositeSet(%s);\" id='btn_%s' src='../images/%s'>"
    //printf(PM_BUTTON2_FILTER_COMP,"true",  "plus_fc",   "add_sm.gif");
    //printf(PM_BUTTON2_FILTER_COMP,"false","minus_fc","remove_sm.gif");
    printf("</TD>\n");
    }

// Now make a filterComp box for each ABC dimension
int dimIx=dimA;
for(dimIx=dimA;dimIx<membersForAll->dimMax;dimIx++)
    {
  //printf("<TD align='right'><B>%s:</B></TD><TD align='left'>\n",labelWithVocabLinkForMultiples(db,parentTdb,membersForAll->members[dimIx]));
    printf("<TD align='left'><B>%s:</B><BR>\n",labelWithVocabLinkForMultiples(db,parentTdb,membersForAll->members[dimIx]));

    #ifdef FILTER_COMPOSITE_OPEN_SIZE
    int fullSize = membersForAll->members[dimIx]->count;
    #ifdef FILTER_COMPOSITE_ONLYONE
    if(membersForAll->members[dimIx]->fcType != fctOneOnly)
    #endif///def FILTER_COMPOSITE_ONLYONE
        fullSize++; // Room for "All"
    #endif///def FILTER_COMPOSITE_OPEN_SIZE

#ifdef NEW_JQUERY
    #define FILTER_COMPOSITE_FORMAT "<SELECT id='fc%d' name='%s.filterComp.%s' %s onchange='filterCompositeSelectionChanged(this);' style='display: none; font-size:.8em;' class='filterComp'><BR>\n"
#else///ifndef NEW_JQUERY
    #define FILTER_COMPOSITE_FORMAT "<SELECT id='fc%d' name='%s.filterComp.%s' %s onchange='filterCompositeSelectionChanged(this);' style='display: none;' class='filterComp'><BR>\n"
#endif///ndef NEW_JQUERY
    printf(FILTER_COMPOSITE_FORMAT,dimIx,parentTdb->track,membersForAll->members[dimIx]->groupTag,"multiple");

    #ifdef FILTER_COMPOSITE_ONLYONE
    // DO we support anything besides multi? (membersForAll->members[dimIx]->fcType == fctMulti?"multiple ":""));
    if(membersForAll->members[dimIx]->fcType != fctOneOnly)
    #endif///def FILTER_COMPOSITE_ONLYONE
        printf("<OPTION%s>All</OPTION>\n",(sameWord("All",membersForAll->checkedTags[dimIx])?" SELECTED":"") );

    int ix=0;
    for(ix=0;ix<membersForAll->members[dimIx]->count; ix++)
        {
        boolean alreadySet = membersForAll->members[dimIx]->selected[ix];
        printf("<OPTION%s value=%s>%s</OPTION>\n",(alreadySet?" SELECTED":""),
               membersForAll->members[dimIx]->tags[ix],membersForAll->members[dimIx]->titles[ix]);
        }
    printf("</SELECT>");

    #ifdef FILTER_COMPOSITE_ONLYONE
    if(membersForAll->members[dimIx]->fcType == fctOneOnly)
        printf(" (select only one)");
    #endif///def FILTER_COMPOSITE_ONLYONE

    printf("</TD><TD width='20'></TD>\n");
    }
printf("</TR></TABLE>\n");

puts("<BR>\n");

return TRUE;
}

static boolean compositeUiByMatrix(char *db, struct cart *cart, struct trackDb *parentTdb, char *formName)
/* UI for composite tracks: matrix of checkboxes. */
{
//int ix;
char objName[SMALLBUF];

membersForAll_t* membersForAll = membersForAllSubGroupsGet(parentTdb,cart);
if(membersForAll == NULL || membersForAll->dimensions == NULL) // Not Matrix!
    return FALSE;

int ixX,ixY;
members_t *dimensionX = membersForAll->members[dimX];
members_t *dimensionY = membersForAll->members[dimY];

// use array of char determine all the cells (in X,Y,Z dimensions) that are actually populated
char *value;
int sizeOfX = dimensionX?dimensionX->count:1;
int sizeOfY = dimensionY?dimensionY->count:1;
int cells[sizeOfX][sizeOfY]; // There needs to be atleast one element in dimension
int chked[sizeOfX][sizeOfY]; // How many subCBs are checked per matCB?
int enabd[sizeOfX][sizeOfY]; // How many subCBs are enabled per matCB?
memset(cells, 0, sizeof(cells));
memset(chked, 0, sizeof(chked));
memset(enabd, 0, sizeof(chked));

struct slRef *subtrackRef, *subtrackRefList = trackDbListGetRefsToDescendantLeaves(parentTdb->subtracks);
struct trackDb *subtrack;
if (dimensionX || dimensionY) // Must be an X or Y dimension
    {
    // Fill the cells based upon subtrack membership
    for (subtrackRef = subtrackRefList; subtrackRef != NULL; subtrackRef = subtrackRef->next)
        {
        subtrack = subtrackRef->val;
        ixX = (dimensionX ? -1 : 0 );
        ixY = (dimensionY ? -1 : 0 );
        if(dimensionX && subgroupFind(subtrack,dimensionX->groupTag,&value))
            {
            ixX = stringArrayIx(value,dimensionX->tags,dimensionX->count);
            subgroupFree(&value);
            }
        if(dimensionY && subgroupFind(subtrack,dimensionY->groupTag,&value))
            {
            ixY = stringArrayIx(value,dimensionY->tags,dimensionY->count);
            subgroupFree(&value);
            }
        if(ixX > -1 && ixY > -1)
            {
            cells[ixX][ixY]++;
            int fourState = subtrackFourStateChecked(subtrack,cart);
            if(fourStateEnabled(fourState))  // hidden views are handled by 4-way CBs: only count enabled
                {
                if(subtrackInAllCurrentABCs(subtrack,membersForAll))  // Only bother if the subtrack is found in all ABC dims checked
                    {
                    enabd[ixX][ixY]++;
                    if(fourStateChecked(fourState) == 1)
                        chked[ixX][ixY]++;
                    }
                }
            }
        }
    }

// If there is no matrix and if there is a filterComposite, then were are done.
if(dimensionX == NULL && dimensionY == NULL)
    {
    if (compositeUiByFilter(db, cart, parentTdb, formName))
        return FALSE;
    }

// Tell the user what to do:
char javascript[JBUFSIZE];
//puts("<B>Select subtracks by characterization:</B><BR>");
printf("<B>Select subtracks by ");
if(dimensionX && !dimensionY)
    safef(javascript, sizeof(javascript), "%s:</B>",dimensionX->groupTitle);
else if(!dimensionX && dimensionY)
    safef(javascript, sizeof(javascript), "%s:</B>",dimensionY->groupTitle);
else if(dimensionX && dimensionY)
    safef(javascript, sizeof(javascript), "%s and %s:</B>",dimensionX->groupTitle,dimensionY->groupTitle);
else
    safef(javascript, sizeof(javascript), "multiple variables:</B>");
puts(strLower(javascript));

if(!subgroupingExists(parentTdb,"view"))
    puts("(<A HREF=\"../goldenPath/help/multiView.html\" title='Help on subtrack selection' TARGET=_BLANK>help</A>)\n");

puts("<BR>\n");

if(membersForAll->abcCount > 0 && membersForAll->filters == FALSE)
    {
    displayABCdimensions(db,cart,parentTdb,subtrackRefList,membersForAll);
    }

if(dimensionX == NULL && dimensionY == NULL) // Could have been just filterComposite. Must be an X or Y dimension
    return FALSE;

printf("<TABLE class='greenBox' style='background-color:%s;'>\n",COLOR_BG_DEFAULT);

matrixXheadings(db,parentTdb,membersForAll,TRUE);

// Now the Y by X matrix
int cntX=0,cntY=0;
for (ixY = 0; ixY < sizeOfY; ixY++)
    {
    if(dimensionY == NULL || (dimensionY->tags[ixY]))
        {
        cntY++;
        assert(!dimensionY || ixY < dimensionY->count);
        printf("<TR ALIGN=CENTER BGCOLOR=\"#FFF9D2\">");

        matrixYheadings(db,parentTdb, membersForAll,ixY,TRUE);

#define MAT_CB_SETUP "<INPUT TYPE=CHECKBOX NAME='%s' VALUE=on %s>"
#define MAT_CB(name,js) printf(MAT_CB_SETUP,(name),(js));
        for (ixX = 0; ixX < sizeOfX; ixX++)
            {
            if(dimensionX == NULL || (dimensionX->tags[ixX]))
                {
                assert(!dimensionX || ixX < dimensionX->count);

                if(cntY==1) // Only do this on the first good Y
                    cntX++;

                if(dimensionX && ixX == dimensionX->count)
                    break;
                char *ttlX = NULL;
                char *ttlY = NULL;
                if(dimensionX)
                    {
                    ttlX = cloneString(dimensionX->titles[ixX]);
                    stripString(ttlX,"<i>");
                    stripString(ttlX,"</i>");
                    }
                if(dimensionY != NULL)
                    {
                    ttlY = cloneString(dimensionY->titles[ixY]);
                    stripString(ttlY,"<i>");
                    stripString(ttlY,"</i>");
                    }
                if(cells[ixX][ixY] > 0)
                    {
                    boolean halfChecked = (chked[ixX][ixY] > 0 && chked[ixX][ixY] < enabd[ixX][ixY]);

                    struct dyString *dyJS = dyStringCreate("onclick='matCbClick(this);'");
                    if(dimensionX && dimensionY)
                        {
                        safef(objName, sizeof(objName), "mat_%s_%s_cb", dimensionX->tags[ixX],dimensionY->tags[ixY]);
                        }
                    else
                        {
                        safef(objName, sizeof(objName), "mat_%s_cb", (dimensionX ? dimensionX->tags[ixX] : dimensionY->tags[ixY]));
                        }
                    //printf("<TD title='subCBs:%d  checked:%d enabled:%d'>\n",cells[ixX][ixY],chked[ixX][ixY],enabd[ixX][ixY]);
                    if(ttlX && ttlY)
                        printf("<TD title='%s and %s'>\n",ttlX,ttlY);
                    else
                        printf("<TD title='%s'>\n",(ttlX ? ttlX : ttlY));
                    dyStringPrintf(dyJS, " class=\"matCB");
                    if(halfChecked)
                        dyStringPrintf(dyJS, " halfVis");  // needed for later js identification!
                    if(dimensionX)
                        dyStringPrintf(dyJS, " %s",dimensionX->tags[ixX]);
                    if(dimensionY)
                        dyStringPrintf(dyJS, " %s",dimensionY->tags[ixY]);
                    dyStringAppendC(dyJS,'"');
                    if(chked[ixX][ixY] > 0)
                        dyStringAppend(dyJS," CHECKED");
                    if(halfChecked)
                        {
                        //dyStringAppend(dyJS," style='filter:alpha(opacity=50)'"); // Doesn't get set overkill with class=halfVis but IE doesn't cooperate!
                        dyStringAppend(dyJS," title='Not all associated subtracks have been selected'"); // overkill with class=halfVis but IE doesn't cooperate!
                        }
                    MAT_CB(objName,dyStringCannibalize(&dyJS)); // X&Y are set by javascript page load
                    puts("</TD>");
                    }
                else
                    {
                    if(ttlX && ttlY)
                        printf("<TD title='%s and %s'></TD>\n",ttlX,ttlY);
                    else
                        printf("<TD title='%s'></TD>\n",(ttlX ? ttlX : ttlY));
                    //puts("<TD>&nbsp;</TD>");
                    }
                }
            }
        if(dimensionX && cntX>MATRIX_RIGHT_BUTTONS_AFTER)
            matrixYheadings(db,parentTdb, membersForAll,ixY,FALSE);
        puts("</TR>\n");
        }
    }
if(dimensionY && cntY>MATRIX_BOTTOM_BUTTONS_AFTER)
    matrixXheadings(db,parentTdb,membersForAll,FALSE);

puts("</TD></TR></TABLE>");

// If any filter additional filter composites, they can be added at the end.
compositeUiByFilter(db, cart, parentTdb, formName);

return TRUE;
}

static boolean compositeUiAllButtons(char *db, struct cart *cart, struct trackDb *parentTdb, char *formName)
/* UI for composite tracks: all/none buttons only (as opposed to matrix or lots of buttons */
{
if (trackDbCountDescendantLeaves(parentTdb) <= 1)
    return FALSE;

if(dimensionsExist(parentTdb))
    return FALSE;

#define PM_BUTTON_GLOBAL "<IMG height=18 width=18 onclick=\"matSubCBsCheck(%s);\" id='btn_%s' src='../images/%s'>"
#define    BUTTON_PLUS_ALL_GLOBAL()  printf(PM_BUTTON_GLOBAL,"true",  "plus_all",   "add_sm.gif")
#define    BUTTON_MINUS_ALL_GLOBAL() printf(PM_BUTTON_GLOBAL,"false","minus_all","remove_sm.gif")
BUTTON_PLUS_ALL_GLOBAL();
BUTTON_MINUS_ALL_GLOBAL();
puts("&nbsp;<B>Select all subtracks</B><BR>");
return TRUE;
}

static boolean compositeUiNoMatrix(char *db, struct cart *cart, struct trackDb *parentTdb,
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
    puts("<TR><TD><B><EM>&nbsp; &nbsp; All</EM></B>&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; </TD><TD>");
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
    struct slRef *tdbRefList = trackDbListGetRefsToDescendantLeaves(parentTdb->subtracks);
    struct slRef *tdbRef;
    for (tdbRef = tdbRefList; tdbRef != NULL; tdbRef = tdbRef->next)
        {
	subtrack = tdbRef->val;
        boolean newVal = FALSE;
        safef(option, sizeof(option), "%s_sel", subtrack->track);
        newVal = sameString(button, ADD_BUTTON_LABEL);
        if (primarySubtrack)
            {
            if (sameString(subtrack->track, primarySubtrack))
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
    printf("<TR><TD><B><EM>&nbsp; &nbsp; %s</EM></B></TD></TR>", words[1]);
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
	struct slRef *tdbRefList = trackDbListGetRefsToDescendantLeaves(parentTdb->subtracks);
	struct slRef *tdbRef;
	for (tdbRef = tdbRefList; tdbRef != NULL; tdbRef = tdbRef->next)
            {
	    subtrack = tdbRef->val;
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
                    safef(option, sizeof(option),"%s_sel", subtrack->track);
                    newVal = sameString(button, ADD_BUTTON_LABEL);
                    if (primarySubtrack)
                        {
                        if (sameString(subtrack->track, primarySubtrack))
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
		  char *primarySubtrack, char *fakeSubmit, char *formName, struct hash *trackHash)
/* UI for composite tracks: subtrack selection.  If primarySubtrack is
 * non-NULL, don't allow it to be cleared and only offer subtracks
 * that have the same type.  If fakeSubmit is non-NULL, add a hidden
 * var with that name so it looks like it was pressed. */
{
bool hasSubgroups = (trackDbSetting(tdb, "subGroup1") != NULL);
boolean isMatrix = dimensionsExist(tdb);
boolean viewsOnly = FALSE;

if (primarySubtrack == NULL)
    {
    if (!cartVarExists(cart, "ajax"))
        {
        if(trackDbSetting(tdb, "dragAndDrop") != NULL)
            jsIncludeFile("jquery.tablednd.js", NULL);
        jsIncludeFile("ajax.js",NULL);
        #ifdef TABLE_SCROLL
        jsIncludeFile("jquery.fixedtable.js",NULL);
        #endif//def TABLE_SCROLL
        }
    jsIncludeFile("hui.js",NULL);
    }

#ifdef SUBTRACK_CFG_POPUP
printf("<div id='popit' style='display: none'></div>");
cgiMakeHiddenVar("db", db);
printf("<input type=HIDDEN id='track' value='%s';</input>\n",tdb->track);
#endif
cgiDown(0.7);
if (trackDbCountDescendantLeaves(tdb) < MANY_SUBTRACKS && !hasSubgroups)
    {
    if(primarySubtrack)
        compositeUiSubtracksMatchingPrimary(db, cart, tdb,primarySubtrack);
    else
        compositeUiSubtracks(db, cart, tdb, trackHash);
    return;
    }
if (fakeSubmit)
    cgiMakeHiddenVar(fakeSubmit, "submit");

if(primarySubtrack == NULL)
    {
    if(subgroupingExists(tdb,"view"))
        {
        hCompositeDisplayViewDropDowns(db, cart,tdb);
        if(subgroupCount(tdb) <= 1)
            viewsOnly = TRUE;
        }
    if(!viewsOnly)
        {
        cgiDown(0.7);
        if(trackDbSettingOn(tdb, "allButtonPair"))
	    {
            compositeUiAllButtons(db, cart, tdb, formName);
	    }
        else if (!hasSubgroups || !isMatrix || primarySubtrack)
	    {
            compositeUiNoMatrix(db, cart,tdb,primarySubtrack,formName);
	    }
        else
	    {
            compositeUiByMatrix(db, cart, tdb, formName);
	    }
        }
#ifdef SUBTRACK_CFG_POPUP
    if(primarySubtrack == NULL)
        cfgLinkToDependentCfgs(cart,tdb,tdb->track);  // Must be after views are set up to get view vis
    printf("<script type='text/javascript'>registerFormSubmit('mainForm');</script>\n");
#endif
    }

cartSaveSession(cart);
cgiContinueHiddenVar("g");

if(primarySubtrack)
    compositeUiSubtracksMatchingPrimary(db, cart, tdb,primarySubtrack);
else
    compositeUiSubtracks(db, cart, tdb, trackHash);

if (primarySubtrack == NULL)  // primarySubtrack is set for tableBrowser but not hgTrackUi
    {
    if (trackDbCountDescendantLeaves(tdb) > 5)
        {
        cgiDown(0.7);
        cgiMakeButton("Submit", "Submit");
        }
    }
}

boolean superTrackDropDownWithExtra(struct cart *cart, struct trackDb *tdb,
                                int visibleChild,char *extra)
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
        cartUsualString(cart, tdb->track, tdb->isShow ? "show" : "hide");
if (sameString("show", setting))
    show = TRUE;

/* Determine if any tracks in supertrack are visible; if not,
 * the 'show' is grayed out */
if (show && (visibleChild == -1))
    {
    visibleChild = 0;
    struct slRef *childRef;
    for ( childRef = tdb->children; childRef != NULL; childRef = childRef->next)
        {
	struct trackDb *cTdb = childRef->val;
        cTdb->visibility =
                hTvFromString(cartUsualString(cart, cTdb->track,
                                      hStringFromTv(cTdb->visibility)));
        if (cTdb->visibility != tvHide)
            visibleChild = 1;
        }
    }
hideShowDropDownWithClassAndExtra(tdb->track, show, (show && visibleChild) ?
                            "normalText visDD" : "hiddenText visDD",extra);
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

enum trackVisibility tdbLocalVisibility(struct cart *cart, struct trackDb *tdb,boolean *subtrackOverride)
// returns visibility NOT limited by ancestry.  Fills optional boolean if subtrack specific vis is found
// If not NULL cart will be examined without ClosestToHome.  Folders/supertracks resolve to hide/full
{
if (subtrackOverride != NULL)
*subtrackOverride = FALSE; // default

// tdb->visibility should reflect local trackDb setting
enum trackVisibility vis = tdb->visibility;
if (tdbIsSuperTrack(tdb))
    vis = (tdb->isShow ? tvFull : tvHide);

if (cart != NULL) // cart is optional
    {
    char *cartVis = NULL;
    if (tdbIsCompositeView(tdb))
        {
        char *view = trackDbLocalSetting(tdb,"view"); // views have funky cart setting
        assert(view != NULL);
        char setting[512];
        safef(setting,sizeof(setting),"%s.%s.vis",tdb->parent->track,view);
        cartVis = cartOptionalString(cart, setting);
        }
    else
        cartVis = cartOptionalString(cart, tdb->track);
    if (cartVis != NULL)
        {
        vis = hTvFromString(cartVis);
        if (subtrackOverride != NULL && tdbIsContainerChild(tdb))
            *subtrackOverride = TRUE;
        }
    }
return vis;
}

enum trackVisibility tdbVisLimitedByAncestors(struct cart *cart, struct trackDb *tdb, boolean checkBoxToo, boolean foldersToo)
// returns visibility limited by ancestry.  This includes subtrack vis override and parents limit maximum.
// cart may be null, in which case, only trackDb settings (default state) are examined
// checkBoxToo means ensure subtrack checkbox state is visible
// foldersToo means limit by folders (aka superTracks) as well.
{
boolean subtrackOverride = FALSE;
enum trackVisibility vis = tdbLocalVisibility(cart,tdb,&subtrackOverride);
if (subtrackOverride)
    return vis;

// subtracks without explicit (cart) vis but are selected, should get inherited vis
if (tdbIsContainerChild(tdb))
    {
    if (checkBoxToo && fourStateVisible(subtrackFourStateChecked(tdb,cart)))
        vis = tvFull; // to be limited by ancestry
    }

if (vis == tvHide || tdb->parent == NULL || (!foldersToo && tdbIsFolder(tdb->parent)))  // aka superTrack
    return vis; // end of line

return tvMin(vis,tdbVisLimitedByAncestors(cart,tdb->parent,checkBoxToo,foldersToo));
}

char *compositeViewControlNameFromTdb(struct trackDb *tdb)
/* Returns a string with the composite view control name if one exists */
{
char *stView   = NULL;
char *name     = NULL;
char *rootName = NULL;
// This routine should give these results: compositeName.viewName or else subtrackName.viewName or else compositeName or else subtrackName
if(tdbIsCompositeChild(tdb) == TRUE && trackDbLocalSetting(tdb, "parent") != NULL)
    {
    if(trackDbSettingClosestToHomeOn(tdb, "configurable"))
        rootName = tdb->track;  // subtrackName
    else
        rootName = firstWordInLine(cloneString(trackDbLocalSetting(tdb, "parent"))); // compositeName
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
    name = cloneString(tdb->track);
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
struct trackDb *parent;
for (parent = tdb->parent; parent != NULL; parent = parent->parent)
    if (startsWithWordByDelimiter(parent->track, '.', name))
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

void hPrintAbbreviationTable(struct sqlConnection *conn, char *sourceTable, char *label)
/* Print out table of abbreviations. */
{
char query[256];
safef(query, sizeof(query), "select name,description from %s order by name", sourceTable);
struct sqlResult *sr = sqlGetResult(conn, query);
webPrintLinkTableStart();
webPrintLabelCell("Symbol");
webPrintLabelCell(label);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    printf("</TR><TR>\n");
    char *name = row[0];
    char *description = row[1];
    webPrintLinkCell(name);
    webPrintLinkCell(description);
    }
sqlFreeResult(&sr);
webPrintLinkTableEnd();
}


boolean printPennantIconNote(struct trackDb *tdb)
// Returns TRUE and prints out the "pennantIcon" and note when found.
//This is used by hgTrackUi and hgc before printing out trackDb "html"
{
char * setting = trackDbSetting(tdb, "pennantIcon");
if (setting != NULL)
    {
    setting = cloneString(setting);
    char *icon = htmlEncodeText(nextWord(&setting),FALSE);
    char *url = nextWord(&setting);
    char *hint = htmlEncodeText(stripEnclosingDoubleQuotes(setting),FALSE);

    if (strlen(url) > 0)
        {
        printf("<P><a title='%s' href='%s' TARGET=ucscHelp><img height='16' width='16' src='../images/%s'></a>",hint,url,icon);

        // Special case for liftOver from hg17 or hg18, but this should probably be generalized.
        if (sameString(icon,"18.jpg") && startsWithWord("lifted",hint))
            printf("&nbsp;Note: these data have been converted via liftOver from the Mar. 2006 (NCBI36/hg18) version of the track.");
        else if (sameString(icon,"17.jpg") && startsWithWord("lifted",hint))
            printf("&nbsp;Note: these data have been converted via liftOver from the May 2004 (NCBI35/hg17) version of the track.");
        else if (strlen(hint) > 0)
            printf("&nbsp;Note: %s.",hint);
        printf("</P>\n");
        }
    else
        printf("<img height='16' width='16' src='%s'>\n",icon);
    return TRUE;
    }
return FALSE;
}

boolean hPrintPennantIcon(struct trackDb *tdb)
// Returns TRUE and prints out the "pennantIcon" when found.  Example: ENCODE tracks in hgTracks config list.
{
char *setting = trackDbSetting(tdb, "pennantIcon");
if(setting != NULL)
    {
    setting = cloneString(setting);
    char *icon = htmlEncodeText(nextWord(&setting),FALSE);
    if (setting)
        {
        char *url = nextWord(&setting);
        if (setting)
            {
            char *hint = htmlEncodeText(stripEnclosingDoubleQuotes(setting),FALSE);
            hPrintf("<a title='%s' href='%s' TARGET=ucscHelp><img height='16' width='16' src='../images/%s'></a>\n",hint,url,icon);
            freeMem(hint);
            }
        else
            hPrintf("<a href='%s' TARGET=ucscHelp><img height='16' width='16' src='../images/%s'></a>\n",url,icon);
        }
    else
        hPrintf("<img height='16' width='16' src='%s'>\n",icon);
    freeMem(icon);
    return TRUE;
    }
else if(trackDbSetting(tdb, "wgEncode") != NULL)
    {
    hPrintf("<a title='encode project' href='../ENCODE'><img height='16' width='16' src='../images/encodeThumbnail.jpg'></a>\n");
    return TRUE;
    }
return FALSE;
}

void printUpdateTime(char *database, struct trackDb *tdb,
    struct customTrack *ct)
/* display table update time */
{
/* have not decided what to do for a composite container */
if (tdbIsComposite(tdb))
    return;
struct sqlConnection *conn = NULL;
char *tableName = NULL;
if (isCustomTrack(tdb->track))
    {
    if (ct)
	{
	conn =  hAllocConn(CUSTOM_TRASH);
	tableName = ct->dbTableName;
	}
    }
else if (startsWith("big", tdb->type))
    {
    char *tableName = hTableForTrack(database, tdb->table);
    struct sqlConnection *conn =  hAllocConnTrack(database, tdb);
    char *bbiFileName = bbiNameFromSettingOrTable(tdb, conn, tableName);
    hFreeConn(&conn);
    struct bbiFile *bbi = NULL;
    if (startsWith("bigBed", tdb->type))
	bbi = bigBedFileOpen(bbiFileName);
    if (startsWith("bigWig", tdb->type))
	bbi = bigWigFileOpen(bbiFileName);
    time_t timep = 0;
    if (bbi)
	{
	timep = bbiUpdateTime(bbi);
	bbiFileClose(&bbi);
	}
    printBbiUpdateTime(&timep);
    }
else
    {
    tableName = hTableForTrack(database, tdb->table);
    conn = hAllocConnTrack(database, tdb);
    }
if (tableName)
    {
    char *date = firstWordInLine(sqlTableUpdate(conn, tableName));
    if (date != NULL)
	printf("<B>Data last updated:&nbsp;</B>%s<BR>\n", date);
    }
hFreeConn(&conn);
}

void printBbiUpdateTime(time_t *timep)
/* for bbi files, print out the timep value */
{
    printf ("<B>Data last updated:&nbsp;</B>%s<BR>\n",
	sqlUnixTimeToDate(timep, FALSE));
}
