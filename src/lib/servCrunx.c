/* Stuff that's specific for local linux server goes here. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "portable.h"
#include "portimpl.h"
#include "obscure.h"

static char const rcsid[] = "$Id: servCrunx.c,v 1.5 2003/05/06 07:33:44 kate Exp $";

static void _makeTempName(struct tempName *tn, char *base, char *suffix)
/* Figure out a temp name, and how CGI and HTML will access it. */
{
char *tname;
char *tempDirCgi = "/home/httpd/html/trash";
char *tempDirHtml = "/trash";
int tlcLen = strlen(tempDirCgi);
int tlhLen = strlen(tempDirHtml);

tname = rTempName(tempDirCgi, base, suffix);
strcpy(tn->forCgi, tname);
memcpy(tn->forHtml, tempDirHtml, tlhLen);
strcpy(tn->forHtml+tlhLen, tn->forCgi+tlcLen);
}

static char *_cgiDir()
{
return "../cgi-bin/";
}

static char *_cgiSuffix()
{
return ".cgi";
}

static double _speed()
{
return 3.0;
}
    
struct webServerSpecific wssLinux =
    {
    "linux",
    _makeTempName,
    _cgiDir,
    _cgiSuffix,
    _speed,
    };
