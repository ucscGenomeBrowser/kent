/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* Stuff that's specific for the MS II Web Server goes here. */
#include "common.h"
#include "portable.h"
#include "portimpl.h"
#include "obscure.h"


static void _makeTempName(struct tempName *tn, char *base, char *suffix)
/* Figure out a temp name, and how CGI and HTML will access it. */
{
long tempIx = incCounterFile("tcounter");
sprintf(tn->forCgi, "..\\trash\\%s%ld%s", base, tempIx, suffix);
sprintf(tn->forHtml, "..\\trash\\%s%ld%s", base, tempIx, suffix);
}

static char *_cgiDir()
{
return "";
}

static char *_cgiSuffix()
{
return ".exe";
}

static double _speed()
{
return 2.5;
}

    
struct webServerSpecific wssMicrosoftII =
    {
    "Microsoft-IIS",
    _makeTempName,
    _cgiDir,
    _cgiSuffix,
    _speed,
    };
