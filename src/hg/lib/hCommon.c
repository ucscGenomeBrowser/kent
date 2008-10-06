/* hCommon.c - routines used by many files in hgap project. */

#include "common.h"
#include "hCommon.h"
#include "chromInfo.h"
#include "portable.h"
#include "hgConfig.h"

static char const rcsid[] = "$Id: hCommon.c,v 1.35 2008/10/06 17:18:27 angie Exp $";

static char *_hgcName = "../cgi-bin/hgc";	/* Path to click processing program. */
static char *_hgTracksName = "../cgi-bin/hgTracks"; /* Path back to genome browser. */
static char *_hgTrackUiName = "../cgi-bin/hgTrackUi"; /* Path to extended ui program. */
static char *_hgTextName = "../cgi-bin/hgText"; /* Path back to the text browser. */
static char *_hgTablesName = "../cgi-bin/hgTables"; /* Path back to the table browser. */
static char *_hgCustomName = "../cgi-bin/hgCustom"; /* Path back to the custom tracks manager. */
static char *_hgSessionName = "../cgi-bin/hgSession";	/* Path to session manager. */
static char *_hgPalName = "../cgi-bin/hgPal"; /* Path back to the protein aligner */

char *hgPalName()
/* Relative URL to click processing program. */
{
return _hgPalName;
}

char *hgcName()
/* Relative URL to click processing program. */
{
return _hgcName;
}

char *hgTracksName()
/* Relative URL to browser. */
{
return _hgTracksName;
}

char *hgTrackUiName()
/* Relative URL to extended track UI. */
{
return _hgTrackUiName;
}

char *hgTextName()
/* Relative URL to old table browser. */
{
return _hgTextName;
}

char *hgTablesName()
/* Relative URL to table browser. */
{
return _hgTablesName;
}

char *hgCustomName()
/* Relative URL to custom tracks manager. */
{
return _hgCustomName;
}

char *hgSessionName()
/* Relative URL to session manager. */
{
return _hgSessionName;
}

static void finishCloneName(char *fragName, char *e, char cloneName[128])
/* Finish conversion from frag to clone or clone.ver name. */
{
int size;

if (e == NULL)
    e = fragName + strlen(fragName);
size = e - fragName;
if (size >= 128)
    errAbort("name too long %s\n", fragName);
memcpy(cloneName, fragName, size);
cloneName[size] = 0;
}

void fragToCloneName(char *fragName, char cloneName[128])
/* Convert fragment name to clone name. */
{
char *e = strchr(fragName, '.');
finishCloneName(fragName, e, cloneName);
}

void fragToCloneVerName(char *fragName, char cloneVerName[128])
/* Convert fragment name to clone.version name. */
{
char *e = strchr(fragName, '.');
if (e == NULL)
    errAbort("No . in fragName %s", fragName);
e = strchr(e, '_');
finishCloneName(fragName, e, cloneVerName);
}

void recNameToFileName(char *dir, char *recName, char *fileName, char *suffix)
/* Convert UCSC style fragment name to name of file for a clone. */
{
char *e;
char *d = fileName;
int size;


/* Start file name with directory if any. */
if (dir != NULL)
    {
    size = strlen(dir);
    memcpy(d, dir, size);
    d += size;
    if (dir[size-1] != '/')
	*d++ = '/';
    }
if (*recName == '>')
    ++recName;
recName = skipLeadingSpaces(recName);
e = strchr(recName, '.');
if (e == NULL)
    e = skipToSpaces(recName);
if (e == NULL)
    e = recName + strlen(recName);
size = e - recName;
memcpy(d, recName, size);
d += size;
strcpy(d, suffix);
}

void faRecNameToQacFileName(char *dir, char *recName, char *fileName)
/* Convert fa record name to file name. */
{
recNameToFileName(dir, recName, fileName, ".qac");
}

void faRecNameToFaFileName(char *dir, char *recName, char *fileName)
/* Convert fa record name to file name. */
{
recNameToFileName(dir, recName, fileName, ".fa");
}

void gsToUcsc(char *gsName, char *ucscName)
/* Convert from 
 *    AC020585.5~1.2 Fragment 2 of 29 (AC020585.5:1..1195)
 * to
 *    AC020585.5_1_2
 */
{
char *s, *e, *d;
int size;

/* Copy in accession and version. */
d = ucscName;
s = gsName;
e = strchr(s, '~');
if (e == NULL)
    errAbort("Expecting . in %s", gsName);
size = e - s;
memcpy(d, s, size);
d += size;

/* Skip over tilde and replace it with _ */
s = e+1;
*d++ = '_';

e = skipToSpaces(s);
if (e == NULL)
    e = s + strlen(s);
size = e - s;
memcpy(d, s, size);
d[size] = 0;
subChar(d, '.', '_');
return;
}

char *skipChr(char *s)
/* Skip leading 'chr' in string (to get the actual chromosome part). */
{
if (startsWith("chr", s))
    s += 3;
else if (startsWith("scaffold_", s))
    s += 9;
else if (startsWith("Scaffold_", s))
    s += 9;
return s;
}

int chromToInt(char *s)
/* converts a chrom name chrXX into an integer from 1 to 54. 
    X = 23 Y = 24 Un = 25 M = 26 random = chr + 26;*/
{
char *u;
int ret = 0;
char str[64];

if (!startsWith("chr", s))
    {
    return 0;
    }
s += 3;
safef(str, sizeof(str), "%s", s);
u = strchr(str,'_');
if (u != NULL)
    {
    ret = 26;
    *u = '\0';
    }
switch (str[0])
    {
    case 'X':
        ret += 23; 
        break;
    case 'Y':
        ret += 24; 
        break;
    case 'U':
        ret += 25; 
        break;
    case 'M':
        ret += 26; 
        break;
    default:
        ret += atoi(s);
    }
return ret;
}
boolean hIsMgcServer()
/* Is this the MGC-customized server? Change for config variable
 * mgc.server=yes */
{
static boolean mgcHost = FALSE;
static boolean haveChecked = FALSE;
if (!haveChecked)
    {
    char *serverOpt = cfgOption("mgc.server");
    mgcHost = (serverOpt != NULL) && sameString(serverOpt, "yes");
    haveChecked = TRUE;
    }
return mgcHost;
}

boolean hIsGsidServer()
/* Is this the GSID-customized server? Change for config variable
 * gsid.server=yes */
{
static boolean gsidHost = FALSE;
static boolean haveChecked = FALSE;
if (!haveChecked)
    {
    char *serverOpt = cfgOption("gsid.server");
    gsidHost = (serverOpt != NULL) && sameString(serverOpt, "yes");
    haveChecked = TRUE;
    }
return gsidHost;
}

void hTableStart()
/* Output a table with solid borders. */
/* For some reason BORDER=1 does not work in our web.c nested table scheme.
 * So use web.c's trick of using an enclosing table to provide a border.   */
{
puts("<!--hTableStart-->" "\n"
     "<TABLE BGCOLOR=\"#"HG_COL_BORDER"\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>");
puts("<TABLE BORDER=\"1\" BGCOLOR=\"#"HG_COL_INSIDE"\" CELLSPACING=\"0\">");
}

void hTableEnd()
/* Close out table started with hTableStart() */
{
puts("</TABLE>");
puts("</TD></TR></TABLE>");
puts("<!--hTableEnd-->");
}

