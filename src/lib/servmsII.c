/* Stuff that's specific for the MS II Web Server goes here. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "portable.h"
#include "portimpl.h"
#include "obscure.h"

static char const rcsid[] = "$Id: servmsII.c,v 1.5 2004/02/09 20:21:15 kent Exp $";


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

static double _speed()
{
return 2.5;
}

    
struct webServerSpecific wssMicrosoftII =
    {
    "Microsoft-IIS",
    _makeTempName,
    _cgiDir,
    _speed,
    };
