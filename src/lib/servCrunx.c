/* Stuff that's specific for local linux server goes here. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "portable.h"
#include "portimpl.h"
#include "obscure.h"


static char *__trashDir = "/home/httpd/html/trash";

static void _makeTempName(struct tempName *tn, char *base, char *suffix)
/* Figure out a temp name, and how CGI and HTML will access it. */
{
char *tname;
char *tempDirCgi = __trashDir;
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

static char *_trashDir()
{
return __trashDir;
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
    _speed,
    _trashDir,
    };
