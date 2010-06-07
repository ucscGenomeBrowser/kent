/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* Implement portable stuff.... */

/* There is one of the following structures for each web server
 * we support.  During run time looking at the environment variable
 * SERVER_SOFTWARE we decide which of these to use. */
struct webServerSpecific
    {
    char *name;

    /* Make a good name for a temp file. */
    void (*makeTempName)(struct tempName *tn, char *base, char *suffix);

    /* Return directory to look for cgi in. */
    char * (*cgiDir)();

#ifdef NEVER
    /* Return cgi suffix. */
    char * (*cgiSuffix)();
#endif /* NEVER */
    
    /* Return relative speed of CPU. (UCSC CSE 1999 FTP machine is 1.0) */
    double (*speed)();

    /* The relative path to trash directory for CGI binaries */
    char * (*trashDir)();

    };


extern struct webServerSpecific wssMicrosoftII, wssMicrosoftPWS, wssDefault,
	wssLinux, wssCommandLine, wssBrcMcw;

char *rTempName(char *dir, char *base, char *suffix);
/* Make a temp name that's almost certainly unique. */
