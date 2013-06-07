// Code to create hgTracks menu bar

#include "common.h"
#include "dystring.h"
#include "ensFace.h"
#include "agpFrag.h"
#include "ctgPos.h"
#include "hCommon.h"
#include "htmshell.h"
#include "hash.h"
#include "liftOver.h"
#include "wikiLink.h"
#include "web.h"
#include "geoMirror.h"
#include "hgTracks.h"

/* list of links to display in a menu */
struct hotLink
    {
    struct hotLink *next;
    char *name;
    char *url;
    char *id;
    boolean external;
    };

static void appendLink(struct hotLink **links, char *url, char *name, char *id, boolean external)
{
// append to list of links for later printing and/or communication with javascript client
struct hotLink *link;
AllocVar(link);
link->name = cloneString(name);
link->url = cloneString(url);
link->id = cloneString(id);
link->external = external;
slAddTail(links, link);
}

static void printEnsemblAnchor(char *database, char* archive,
                               char *chrName, int start, int end, struct hotLink **links)
/* Print anchor to Ensembl display on same window. */
{
char *scientificName = hScientificName(database);
char *dir = ensOrgNameFromScientificName(scientificName);
struct dyString *ensUrl;
char *name;
int localStart, localEnd;

name = chrName;

if (sameWord(scientificName, "Takifugu rubripes"))
    {
    /* for Fugu, must give scaffold, not chr coordinates */
    /* Also, must give "chrom" as "scaffold_N", name below. */
    if (differentWord(chromName,"chrM") &&
    !hScaffoldPos(database, chromName, winStart, winEnd,
                        &name, &localStart, &localEnd))
        /* position doesn't appear on Ensembl browser.
         * Ensembl doesn't show scaffolds < 2K */
        return;
    }
else if (sameWord(scientificName, "Gasterosteus aculeatus"))
    {
    if (differentWord("chrM", chrName))
    {
    char *fixupName = replaceChars(chrName, "chr", "group");
    name = fixupName;
    }
    }
else if (sameWord(scientificName, "Ciona intestinalis"))
    {
    if (stringIn("chr0", chrName))
	{
	char *fixupName = replaceChars(chrName, "chr0", "chr");
	name = fixupName;
	}
    }
else if (sameWord(scientificName, "Saccharomyces cerevisiae"))
    {
    if (stringIn("2micron", chrName))
	{
	char *fixupName = replaceChars(chrName, "2micron", "2-micron");
	name = fixupName;
	}
    }

if (sameWord(chrName, "chrM"))
    name = "chrMt";
ensUrl = ensContigViewUrl(database, dir, name, seqBaseCount, start+1, end, archive);
appendLink(links, ensUrl->string, "Ensembl", "ensemblLink", TRUE);
/* NOTE: you can not freeMem(dir) because sometimes it is a literal
 * constant */
freeMem(scientificName);
dyStringFree(&ensUrl);
}

static void fr2ScaffoldEnsemblLink(char *archive, struct hotLink **links)
/* print out Ensembl link to appropriate scaffold there */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row = NULL;
char query[256];
sqlSafef(query, sizeof(query),
"select * from chrUn_gold where chrom = '%s' and chromStart<%u and chromEnd>%u",
chromName, winEnd, winStart);
sr = sqlGetResult(conn, query);

int itemCount = 0;
struct agpFrag *agpItem = NULL;
while ((row = sqlNextRow(sr)) != NULL)
    {
    agpFragFree(&agpItem);  // if there is a second one
    agpItem = agpFragLoad(row+1);
    ++itemCount;
    if (itemCount > 1)
	break;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
if (1 == itemCount)
    {   // verify *entirely* within single contig
    if ((winEnd <= agpItem->chromEnd) &&
	(winStart >= agpItem->chromStart))
	{
	int agpStart = winStart - agpItem->chromStart;
	int agpEnd = agpStart + winEnd - winStart;
	printEnsemblAnchor(database, archive, agpItem->frag,
                           agpStart, agpEnd, links);
	}
    }
agpFragFree(&agpItem);  // the one we maybe used
}

void printMenuBar()
/* Put up the menu bar. */
{
struct hotLink *link, *links = NULL;
int i, len;
char *menuStr, buf[4096], uiVars[1024];
safef(uiVars, sizeof(uiVars), "%s=%u", cartSessionVarName(), cartSessionId(cart));

menuStr = menuBar(cart);

// Create top items in view menu
safef(buf, sizeof(buf), "../cgi-bin/hgTracks?%s&hgt.psOutput=on", uiVars);
appendLink(&links, buf, "PDF/PS", "pdfLink", FALSE);
safef(buf, sizeof(buf), "%s&o=%d&g=getDna&i=mixed&c=%s&l=%d&r=%d&db=%s&%s",
      hgcNameAndSettings(), winStart, chromName, winStart, winEnd, database, uiVars);
appendLink(&links, buf, "DNA", "dnaLink", FALSE);
safef(buf, sizeof(buf), "../cgi-bin/hgConvert?hgsid=%d&db=%s", cartSessionId(cart), database);
appendLink(&links, buf, "in other Genomes (Convert)", "convertMenuLink", FALSE);

// Add link-outs to other dbs as appropriate for this assembly
if (differentWord(database,"susScr2"))
    {
    /* Print Ensembl anchor for latest assembly of organisms we have
     * supported by Ensembl == if versionString from trackVersion exists */
    char ensVersionString[256], ensDateReference[256];
    ensGeneTrackVersion(database, ensVersionString, ensDateReference, sizeof(ensVersionString));

    if (sameWord(database,"hg19"))
        {
        printEnsemblAnchor(database, NULL, chromName, winStart, winEnd, &links);
        }
    else if (sameWord(database,"hg18"))
        {
        printEnsemblAnchor(database, "ncbi36", chromName, winStart, winEnd, &links);
        }
    else if (sameWord(database,"oryCun2") || sameWord(database,"anoCar2") || sameWord(database,"calJac3"))
        {
        printEnsemblAnchor(database, NULL, chromName, winStart, winEnd, &links);
        }
    else if (ensVersionString[0])
        {
        char *archive = NULL;
        if (ensDateReference[0] && differentWord("current", ensDateReference))
            archive = cloneString(ensDateReference);
        /*  Can we perhaps map from a UCSC random chrom to an Ensembl contig ? */
        if (isUnknownChrom(database, chromName))
            {
            //	which table to check
            char *ctgPos = "ctgPos";

            if (sameWord(database,"fr2"))
                fr2ScaffoldEnsemblLink(archive, &links);
            else if (hTableExists(database, UCSC_TO_ENSEMBL))
                printEnsemblAnchor(database, archive, chromName, winStart, winEnd, &links);
            else if (hTableExists(database, ctgPos))
                /* see if we are entirely within a single contig */
                {
                struct sqlConnection *conn = hAllocConn(database);
                struct sqlResult *sr = NULL;
                char **row = NULL;
                char query[256];
                sqlSafef(query, sizeof(query),
                      "select * from %s where chrom = '%s' and chromStart<%u and chromEnd>%u",
                      ctgPos, chromName, winEnd, winStart);
                sr = sqlGetResult(conn, query);

                int itemCount = 0;
                struct ctgPos *ctgItem = NULL;
                while ((row = sqlNextRow(sr)) != NULL)
                    {
                    ctgPosFree(&ctgItem);   // if there is a second one
                    ctgItem = ctgPosLoad(row);
                    ++itemCount;
                    if (itemCount > 1)
                        break;
                    }
                sqlFreeResult(&sr);
                hFreeConn(&conn);
                if (1 == itemCount)
                    {   // verify *entirely* within single contig
                    if ((winEnd <= ctgItem->chromEnd) &&
                        (winStart >= ctgItem->chromStart))
                        {
                        int ctgStart = winStart - ctgItem->chromStart;
                        int ctgEnd = ctgStart + winEnd - winStart;
                        printEnsemblAnchor(database, archive, ctgItem->contig,
                                           ctgStart, ctgEnd, &links);
                        }
                    }
                ctgPosFree(&ctgItem);   // the one we maybe used
                }
            }
        else
            {
            printEnsemblAnchor(database, archive, chromName, winStart, winEnd, &links);
            }
        }
    }

if (sameString(database, "hg18"))
    {
    safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9606&build=previous&CHR=%s&BEG=%d&END=%d",
          skipChr(chromName), winStart+1, winEnd);
    appendLink(&links, buf, "NCBI", "ncbiLink", TRUE);
    }
else if (sameString(database, "hg19"))
    {
    safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9606&CHR=%s&BEG=%d&END=%d",
          skipChr(chromName), winStart+1, winEnd);
    appendLink(&links, buf, "NCBI", "ncbiLink", TRUE);
    }
else if (sameString(database, "mm8"))
    {
    safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=10090&CHR=%s&BEG=%d&END=%d",
          skipChr(chromName), winStart+1, winEnd);
    appendLink(&links, buf, "NCBI", "ncbiLink", TRUE);
    }
else if (sameString(database, "danRer2"))
    {
    safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=7955&CHR=%s&BEG=%d&END=%d",
          skipChr(chromName), winStart+1, winEnd);
    appendLink(&links, buf, "NCBI", "ncbiLink", TRUE);
    }
else if (sameString(database, "galGal3"))
    {
    safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9031&CHR=%s&BEG=%d&END=%d",
          skipChr(chromName), winStart+1, winEnd);
    appendLink(&links, buf, "NCBI", "ncbiLink", TRUE);
    }
else if (sameString(database, "canFam2"))
    {
    safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9615&CHR=%s&BEG=%d&END=%d",
          skipChr(chromName), winStart+1, winEnd);
    appendLink(&links, buf, "NCBI", "ncbiLink", TRUE);
    }
else if (sameString(database, "rheMac2"))
    {
    safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9544&CHR=%s&BEG=%d&END=%d",
          skipChr(chromName), winStart+1, winEnd);
    appendLink(&links, buf, "NCBI", "ncbiLink", TRUE);
    }
else if (sameString(database, "panTro2"))
    {
    safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9598&CHR=%s&BEG=%d&END=%d",
          skipChr(chromName), winStart+1, winEnd);
    appendLink(&links, buf, "NCBI", "ncbiLink", TRUE);
    }
else if (sameString(database, "anoGam1"))
    {
    safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=7165&CHR=%s&BEG=%d&END=%d",
          skipChr(chromName), winStart+1, winEnd);
    appendLink(&links, buf, "NCBI", "ncbiLink", TRUE);
    }
else if (sameString(database, "bosTau6"))
    {
    safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9913&CHR=%s&BEG=%d&END=%d",
          skipChr(chromName), winStart+1, winEnd);
    appendLink(&links, buf, "NCBI", "ncbiLink", TRUE);
    }
else if (startsWith("oryLat", database))
    {
    safef(buf, sizeof(buf), "http://medaka.utgenome.org/browser_ens_jump.php?revision=version1.0&chr=chromosome%s&start=%d&end=%d",
          skipChr(chromName), winStart+1, winEnd);
    appendLink(&links, buf, "UTGB", "medakaLink", TRUE);
    }
else if (sameString(database, "cb3"))
    {
    safef(buf, sizeof(buf), "http://www.wormbase.org/db/seq/gbrowse/briggsae?name=%s:%d-%d",
          skipChr(chromName), winStart+1, winEnd);
    appendLink(&links, buf, "WormBase", "wormbaseLink", TRUE);
    }
else if (sameString(database, "cb4"))
    {
    safef(buf, sizeof(buf), "http://www.wormbase.org/db/gb2/gbrowse/c_briggsae?name=%s:%d-%d",
          chromName, winStart+1, winEnd);
    appendLink(&links, buf, "WormBase", "wormbaseLink", TRUE);
    }
else if (sameString(database, "ce10"))
    {
    safef(buf, sizeof(buf), "http://www.wormbase.org/db/gb2/gbrowse/c_elegans?name=%s:%d-%d",
          skipChr(chromName), winStart+1, winEnd);
    appendLink(&links, buf, "WormBase", "wormbaseLink", TRUE);
    }
else if (sameString(database, "ce4"))
    {
    safef(buf, sizeof(buf), "http://ws170.wormbase.org/db/seq/gbrowse/wormbase?name=%s:%d-%d",
          skipChr(chromName), winStart+1, winEnd);
    appendLink(&links, buf, "WormBase", "wormbaseLink", TRUE);
    }
else if (sameString(database, "ce2"))
    {
    safef(buf, sizeof(buf), "http://ws120.wormbase.org/db/seq/gbrowse/wormbase?name=%s:%d-%d",
          skipChr(chromName), winStart+1, winEnd);
    appendLink(&links, buf, "WormBase", "wormbaseLink", TRUE);
    }

// finish View menu
safef(buf, sizeof(buf), "../cgi-bin/hgTracks?%s&hgTracksConfigPage=configure", uiVars);
appendLink(&links, buf, "Configure Browser", "configureMenuLink", FALSE);
safef(buf, sizeof(buf), "../cgi-bin/hgTracks?%s&hgt.reset=on", uiVars);
appendLink(&links, buf, "Default Tracks", "defaultTracksMenuLink", FALSE);
safef(buf, sizeof(buf), "../cgi-bin/hgTracks?%s&hgt.defaultImgOrder=on", uiVars);
appendLink(&links, buf, "Default Track Order", "defaultTrackOrderMenuLink", FALSE);
appendLink(&links, "../cgi-bin/cartReset", "Reset all user settings", "cartResetMenuLink", FALSE);

struct dyString *viewMenu = dyStringCreate("<li class='menuparent' id='view'><span>View</span>\n<ul style='display: none; visibility: hidden;'>\n");
len = slCount(links);
for(i = 0, link = links; link != NULL; i++, link = link->next)
    {
    char class[100];
    if(i == 0)
        safef(class, sizeof(class), "first");
    else if (i + 1 == len)
        safef(class, sizeof(class), "last");
    else
        class[0] = 0;
    char *encodedName = htmlEncode(link->name);
    dyStringAppend(viewMenu, "<li");
    if(*class)
        dyStringPrintf(viewMenu, " class='%s'", class);
    dyStringPrintf(viewMenu, "><a href='%s' id='%s'%s>%s</a></li>\n", link->url, link->id,
                   link->external ? " TARGET='_blank'" : "", encodedName);
    freez(&encodedName);
    }
dyStringAppend(viewMenu, "</ul>\n</li>\n");

menuStr = replaceChars(menuStr, "<!-- OPTIONAL_VIEW_MENU -->", dyStringCannibalize(&viewMenu));
hPuts(menuStr);
freez(&menuStr);
}
