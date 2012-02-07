/* "Web Server" for command line execution. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "portable.h"
#include "portimpl.h"
#include "obscure.h"


static char *__trashDir = ".";

static void _makeTempName(struct tempName *tn, char *base, char *suffix)
/* Figure out a temp name, and how CGI and HTML will access it. */
{
char *tname = rTempName(__trashDir, base, suffix);
strcpy(tn->forCgi, tname);
strcpy(tn->forHtml, tn->forCgi);
}

static char *_cgiDir()
{
char *jkwebDir;
if ((jkwebDir = getenv("JKWEB")) == NULL)
    return "";
else
    return jkwebDir;
}

static char *_trashDir()
{
return __trashDir;
}

static double _speed()
{
return 1.0;
}
    
    
struct webServerSpecific wssCommandLine =
    {
    "commandLine",
    _makeTempName,
    _cgiDir,
    _speed,
    _trashDir,
    };
