/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* "Web Server" for command line execution. */
#include "common.h"
#include "portable.h"
#include "portimpl.h"
#include "obscure.h"

static void _makeTempName(struct tempName *tn, char *base, char *suffix)
/* Figure out a temp name, and how CGI and HTML will access it. */
{
char *tname = rTempName(".", base, suffix);
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

static char *_cgiSuffix()
{
return "";
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
    _cgiSuffix,
    _speed,
    };
