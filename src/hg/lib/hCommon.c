/* hCommon.c - routines used by many files in hgap project. */

#include "common.h"
#include "hCommon.h"
#include "chromInfo.h"
#include "portable.h"
#include "hgConfig.h"

static char const rcsid[] = "$Id: hCommon.c,v 1.20 2003/06/24 07:06:08 kent Exp $";

static char *_hgcName = "../cgi-bin/hgc";	/* Path to click processing program. */
static char *_hgTracksName = "../cgi-bin/hgTracks"; /* Path back to self. */
static char *_hgTrackUiName = "../cgi-bin/hgTrackUi"; /* Path to extended ui program. */
static char *_hgcFullName = "http://genome.ucsc.edu/cgi-bin/hgc";	/* Path to click processing program. */
static char *_hgTracksFullName = "http://genome.ucsc.edu/cgi-bin/hgTracks"; /* Path back to self. */
static char *_hgTrackUiFullName = "http://genome.ucsc.edu/cgi-bin/hgTrackUi"; /* Path back to extended ui program. */

static char *_hgTextName = "/cgi-bin/hgText"; /* Path back to the text browser. */
static char *_hgTextFullName = "http://genome.ucsc.edu/cgi-bin/hgText"; /* Path back to the text browser. */

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

char *hgcFullName()
/* Absolute URL to click processing program. */
{
return _hgcFullName;
}

char *hgTracksFullName()
/* Absolute URL to browser. */
{
return _hgTracksFullName;
}

char *hgTextName()
/* Text broswer relative URL to browser. */
{
return _hgTextName;
}

char *hgTextFullName()
/* Absolute URL to browser. */
{
return _hgTextFullName;
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
return s;
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
