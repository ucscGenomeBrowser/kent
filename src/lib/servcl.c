/* "Web Server" for command line execution. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "portable.h"
#include "portimpl.h"
#include "obscure.h"


static char *_trashDir()
/* Return the path for temporary files: default is "." but env var JKTRASH can override. */
{
static char *__trashDir = NULL;
if (__trashDir == NULL)
    {
    char *jktrashDir = getenv("JKTRASH");
    __trashDir = jktrashDir ? jktrashDir : ".";
    }
return __trashDir;
}

static void _makeTempName(struct tempName *tn, char *base, char *suffix)
/* Figure out a temp name, and how CGI and HTML will access it. */
{
char *tname = rTempName(_trashDir(), base, suffix);
strcpy(tn->forCgi, tname);
strcpy(tn->forHtml, tn->forCgi);
}

static char *_cgiDir()
/* Return the path for CGI executables: default is "" but env var JKWEB can override. */
{
char *jkwebDir;
if ((jkwebDir = getenv("JKWEB")) == NULL)
    return "";
else
    return jkwebDir;
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
