/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* Stuff that's specific for local linux server goes here. */
#include "common.h"
#include "portable.h"
#include "portimpl.h"
#include "obscure.h"

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
