/* Stuff that's specific for .brc.mcw.edu server goes here. 
 *
 * This file is copyright 2004 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "portable.h"
#include "portimpl.h"
#include "obscure.h"
#include "hash.h"

static char const rcsid[] = "$Id: servBrcMcw.c,v 1.1 2004/02/09 20:21:15 kent Exp $";


static void _makeTempName(struct tempName *tn, char *base, char *suffix)
/* Figure out a temp name, and how CGI and HTML will access it. */
{
char *tname;
char buf[64];

tname = rTempName("/trash", base, suffix);
strcpy(tn->forCgi, tname);
strcpy(tn->forHtml, tname);
}

static char *_cgiDir()
{
return "/cgi-bin/";
}

static double _speed()
{
return 3.0;
}
    
struct webServerSpecific wssBrcMcw =
    {
    "default",
    _makeTempName,
    _cgiDir,
    _speed,
    };
