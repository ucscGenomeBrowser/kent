/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* Stuff that's specific for Comp Science dept. web server goes here. */
#include "common.h"
#include "portable.h"
#include "portimpl.h"
#include "obscure.h"


static void _makeTempName(struct tempName *tn, char *base, char *suffix)
/* Figure out a temp name, and how CGI and HTML will access it. */
{
char *tname;
char buf[64];

sprintf(buf, "%s_%s", getHost(), base);
#ifdef WIN32
tname = _tempnam("../trash", buf);
#else
tname = tempnam("../trash", buf);
#endif
sprintf(tn->forCgi, "%s%s", tname, suffix);
strcpy(tn->forHtml, tn->forCgi);
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
return 1.0;
}

    
struct webServerSpecific wssDefault =
    {
    "default",
    _makeTempName,
    _cgiDir,
    _cgiSuffix,
    _speed,
    };
