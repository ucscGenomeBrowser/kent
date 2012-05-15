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
safef(query, sizeof(query),
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
boolean gotBlat = hIsBlatIndexedDatabase(database);
struct dyString *uiVars = uiStateUrlPart(NULL);
char *orgEnc = cgiEncode(organism);
boolean psOutput = cgiVarExists("hgt.psOutput");
struct hotLink *link, *links = NULL;

hPrintf("<TABLE WIDTH=\"100%%\" BGCOLOR=\"#000000\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>\n");
hPrintf("<TABLE WIDTH=\"100%%\" BGCOLOR=\"#2636D1\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\"><TR>\n");
hPrintf("<TD><TABLE BORDER=\"0\"><TR>\n");
hPrintf("<TD ALIGN=CENTER><A HREF=\"../index.html?org=%s&db=%s&%s=%u\" class=\"topbar\">Home</A>&nbsp;&nbsp;</TD>",
    orgEnc, database, cartSessionVarName(), cartSessionId(cart));

if (hIsGisaidServer())
    {
    /* disable hgGateway for gisaid for now */
    //hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../cgi-bin/hgGateway?org=%s&db=%s\" class=\"topbar\">Sequence View Gateway</A>&nbsp;&nbsp;</TD>", orgEnc, database);
    hPrintf(
    "<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../cgi-bin/gisaidTable?gisaidTable.do.advFilter=filter+%c28now+on%c29&fromProg=hgTracks&%s=%u\" class=\"topbar\">%s</A>&nbsp;&nbsp;</TD>",
    '%', '%',
    cartSessionVarName(),
    cartSessionId(cart),
    "Select Subjects");
    }
else
if (hIsGsidServer())
    {
    hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../cgi-bin/hgGateway?org=%s&db=%s\" class=\"topbar\">Sequence View Gateway</A>&nbsp;&nbsp;</TD>", orgEnc, database);
    hPrintf(
    "<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../cgi-bin/gsidTable?gsidTable.do.advFilter=filter+%c28now+on%c29&fromProg=hgTracks\" class=\"topbar\">%s</A>&nbsp;&nbsp;</TD>",
    '%', '%', "Select Subjects");
    }
else
    {
    hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../cgi-bin/hgGateway?org=%s&db=%s&%s=%u\" class=\"topbar\">Genomes</A>&nbsp;&nbsp;</TD>", orgEnc, database, cartSessionVarName(), cartSessionId(cart));
    }
if (psOutput)
    {
    hPrintf("<TD ALIGN=CENTER nowrap>&nbsp;&nbsp;<A HREF=\"../cgi-bin/hgTracks?hgTracksConfigPage=notSetorg=%s&db=%s&%s=%u\" class='topbar'>Genome Browser</A>&nbsp;&nbsp;</TD>", orgEnc, database, cartSessionVarName(), cartSessionId(cart));
    }
if (gotBlat)
    {
    hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../cgi-bin/hgBlat?%s\" class=\"topbar\">Blat</A>&nbsp;&nbsp;</TD>", uiVars->string);
    }
if (hIsGisaidServer())
    {
    hPrintf("<TD ALIGN=CENTER nowrap>&nbsp;&nbsp;<A HREF=\"../cgi-bin/gisaidTable?db=%s&%s=%u\" class=\"topbar\">%s</A>&nbsp;&nbsp;</TD>",
       database,
       cartSessionVarName(),
       cartSessionId(cart),
       "Table View");
    }
else if (hIsGsidServer())
    {
    hPrintf("<TD ALIGN=CENTER nowrap>&nbsp;&nbsp;<A HREF=\"../cgi-bin/gsidTable?db=%s\" class=\"topbar\">%s</A>&nbsp;&nbsp;</TD>",
       database, "Table View");
    }
else
    {
    /* disable TB for CGB servers */
    if (!hIsCgbServer())
	{
	    hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../cgi-bin/hgTables?db=%s&%s=%u\" "
		    "class=\"topbar\">%s</A>&nbsp;&nbsp;</TD>",
		    database, cartSessionVarName(), cartSessionId(cart),
	"Tables");
	}
    }

if (hgNearOk(database))
    {
    hPrintf("<TD ALIGN=CENTER nowrap>&nbsp;&nbsp;<A HREF=\"../cgi-bin/hgNear?%s\" class=\"topbar\">%s</A>&nbsp;&nbsp;</TD>",
                 uiVars->string, "Gene Sorter");
    }
if (hgPcrOk(database))
    {
    hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../cgi-bin/hgPcr?%s\" class=\"topbar\">PCR</A>&nbsp;&nbsp;</TD>", uiVars->string);
    }
if (!psOutput)
    {
    hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"%s&o=%d&g=getDna&i=mixed&c=%s&l=%d&r=%d&db=%s&%s\" class=\"topbar\" id='dnaLink'>"
        "%s</A>&nbsp;&nbsp;</TD>",  hgcNameAndSettings(),
        winStart, chromName, winStart, winEnd, database, uiVars->string, "DNA");
    }

if (!psOutput)
    {
    /* disable Convert function for CGB servers for the time being */
    if (!hIsCgbServer())
    if (liftOverChainForDb(database) != NULL)
        {
        hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../cgi-bin/hgConvert?%s&db=%s",
		uiVars->string, database);
        hPrintf("\" class=\"topbar\">Convert</A>&nbsp;&nbsp;</TD>");
        }
    }

if (!psOutput)
    {
    hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../cgi-bin/hgTracks?%s=%u&hgt.psOutput=on\" id='pdfLink' class=\"topbar\">%s</A>&nbsp;&nbsp;</TD>",cartSessionVarName(),
        cartSessionId(cart), "PS/PDF");
    }

if (!psOutput)
    {
    if (wikiLinkEnabled())
        {
        printf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../cgi-bin/hgSession?%s=%u"
        "&hgS_doMainPage=1\" class=\"topbar\">Session</A>&nbsp;&nbsp;</TD>",
        cartSessionVarName(), cartSessionId(cart));
        }
    }

char ensVersionString[256];
char ensDateReference[256];
ensGeneTrackVersion(database, ensVersionString, ensDateReference,
    sizeof(ensVersionString));

if (!psOutput)
    {
    if (differentWord(database,"susScr2"))
        {
        /* Print Ensembl anchor for latest assembly of organisms we have
        * supported by Ensembl == if versionString from trackVersion exists */
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
                    safef(query, sizeof(query),
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
    }

if (!psOutput)
    {
    char buf[2056];
    /* Print NCBI MapView anchor */
    if (sameString(database, "hg18"))
        {
        safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9606&build=previous&CHR=%s&BEG=%d&END=%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "NCBI", "ncbiLink", TRUE);
        }
    if (sameString(database, "hg19"))
        {
        safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9606&CHR=%s&BEG=%d&END=%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "NCBI", "ncbiLink", TRUE);
        }
    if (sameString(database, "mm8"))
        {
        safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=10090&CHR=%s&BEG=%d&END=%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "NCBI", "ncbiLink", TRUE);
        }
    if (sameString(database, "danRer2"))
        {
        safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=7955&CHR=%s&BEG=%d&END=%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "NCBI", "ncbiLink", TRUE);
        }
    if (sameString(database, "galGal3"))
        {
        safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9031&CHR=%s&BEG=%d&END=%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "NCBI", "ncbiLink", TRUE);
        }
    if (sameString(database, "canFam2"))
        {
        safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9615&CHR=%s&BEG=%d&END=%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "NCBI", "ncbiLink", TRUE);
        }
    if (sameString(database, "rheMac2"))
        {
        safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9544&CHR=%s&BEG=%d&END=%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "NCBI", "ncbiLink", TRUE);
        }
    if (sameString(database, "panTro2"))
        {
        safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9598&CHR=%s&BEG=%d&END=%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "NCBI", "ncbiLink", TRUE);
        }
    if (sameString(database, "anoGam1"))
        {
        safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=7165&CHR=%s&BEG=%d&END=%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "NCBI", "ncbiLink", TRUE);
        }
    if (sameString(database, "bosTau6"))
        {
        safef(buf, sizeof(buf), "http://www.ncbi.nlm.nih.gov/mapview/maps.cgi?taxid=9913&CHR=%s&BEG=%d&END=%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "NCBI", "ncbiLink", TRUE);
        }
    if (startsWith("oryLat", database))
        {
        safef(buf, sizeof(buf), "http://medaka.utgenome.org/browser_ens_jump.php?revision=version1.0&chr=chromosome%s&start=%d&end=%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "UTGB", "medakaLink", TRUE);
        }
    if (sameString(database, "cb3"))
        {
        safef(buf, sizeof(buf), "http://www.wormbase.org/db/seq/gbrowse/briggsae?name=%s:%d-%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "WormBase", "wormbaseLink", TRUE);
        }
    if (sameString(database, "cb4"))
        {
        safef(buf, sizeof(buf), "http://www.wormbase.org/db/gb2/gbrowse/c_briggsae?name=%s:%d-%d",
            chromName, winStart+1, winEnd);
        appendLink(&links, buf, "WormBase", "wormbaseLink", TRUE);
        }
    if (sameString(database, "ce10"))
        {
        safef(buf, sizeof(buf), "http://www.wormbase.org/db/gb2/gbrowse/c_elegans?name=%s:%d-%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "WormBase", "wormbaseLink", TRUE);
        }
    if (sameString(database, "ce4"))
        {
        safef(buf, sizeof(buf), "http://ws170.wormbase.org/db/seq/gbrowse/wormbase?name=%s:%d-%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "WormBase", "wormbaseLink", TRUE);
        }
    if (sameString(database, "ce2"))
        {
        safef(buf, sizeof(buf), "http://ws120.wormbase.org/db/seq/gbrowse/wormbase?name=%s:%d-%d",
            skipChr(chromName), winStart+1, winEnd);
        appendLink(&links, buf, "WormBase", "wormbaseLink", TRUE);
        }
    }

for(link = links; link != NULL; link = link->next)
    hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"%s\" TARGET=\"_blank\" class=\"topbar\" id=\"%s\">%s</A>&nbsp;&nbsp;</TD>\n", link->url, link->id, link->name);

if (hIsGisaidServer())
    {
    //hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"/goldenPath/help/gisaidTutorial.html#SequenceView\" TARGET=_blank class=\"topbar\">%s</A>&nbsp;&nbsp;</TD>\n", "Help");
    hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../cgi-bin/hgNotYet\" TARGET=_blank class=\"topbar\">%s</A>&nbsp;&nbsp;</TD>\n", "Help");
    }
else
if (hIsGsidServer())
    {
    hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"/goldenPath/help/gsidTutorial.html#SequenceView\" TARGET=_blank class=\"topbar\">%s</A>&nbsp;&nbsp;</TD>\n", "Help");
    }
else
    {
    hPrintf("<TD ALIGN=CENTER>&nbsp;&nbsp;<A HREF=\"../goldenPath/help/hgTracksHelp.html\" TARGET=_blank class=\"topbar\">%s</A>&nbsp;&nbsp;</TD>\n", "Help");
    }

hPuts("<TD colspan=20>&nbsp;</TD></TR></TABLE></TD>");
hPuts("</TR></TABLE>");
hPuts("</TD></TR></TABLE>\n");
}
