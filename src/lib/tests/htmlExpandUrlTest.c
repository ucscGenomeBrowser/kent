/* htmlPageTest - test some stuff in htmlPage module. */

/* Copyright (C) 2004 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "htmlPage.h"


static void testHtmlExpandUrl()
/* DO some testing of expandRelativeUrl. */
{
    {
    char *oldUrl = "genome.ucsc.edu";
    char *relUrl = "credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "genome.ucsc.edu";
    char *relUrl = "/credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "genome.ucsc.edu";
    char *relUrl = "../credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "genome.ucsc.edu";
    char *relUrl = "../news/chimp.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "genome.ucsc.edu";
    char *relUrl = "http://test.org/credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu";
    char *relUrl = "credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu";
    char *relUrl = "/credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu";
    char *relUrl = "../credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu";
    char *relUrl = "../news/chimp.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu";
    char *relUrl = "http://test.org/credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/index.html";
    char *relUrl = "credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/index.html";
    char *relUrl = "/credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/index.html";
    char *relUrl = "../credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/index.html";
    char *relUrl = "../news/chimp.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/index.html";
    char *relUrl = "http://test.org/credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/goldenPath/";
    char *relUrl = "credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/goldenPath/";
    char *relUrl = "/credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/goldenPath/";
    char *relUrl = "../credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/goldenPath/";
    char *relUrl = "../news/chimp.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/goldenPath/";
    char *relUrl = "http://test.org/credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/goldenPath/downloads.html";
    char *relUrl = "credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/goldenPath/downloads.html";
    char *relUrl = "/credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/goldenPath/downloads.html";
    char *relUrl = "../credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/goldenPath/downloads.html";
    char *relUrl = "../news/chimp.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/goldenPath/downloads.html";
    char *relUrl = "http://test.org/credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/goldenPath/hg16/";
    char *relUrl = "credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/goldenPath/hg16/";
    char *relUrl = "/credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/goldenPath/hg16/";
    char *relUrl = "../credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/goldenPath/hg16/";
    char *relUrl = "../news/chimp.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/goldenPath/hg16/";
    char *relUrl = "http://test.org/credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/goldenPath/hg16/index.html";
    char *relUrl = "credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/goldenPath/hg16/index.html";
    char *relUrl = "/credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/goldenPath/hg16/index.html";
    char *relUrl = "../credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/goldenPath/hg16/index.html";
    char *relUrl = "../news/chimp.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/goldenPath/hg16/index.html";
    char *relUrl = "http://test.org/credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/";
    char *relUrl = "credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/";
    char *relUrl = "/credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/";
    char *relUrl = "../credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/";
    char *relUrl = "../news/chimp.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
    {
    char *oldUrl = "http://genome.ucsc.edu/";
    char *relUrl = "http://test.org/credits.html";
    char *newUrl = htmlExpandUrl(oldUrl, relUrl);
    printf("%s %s -> %s\n", oldUrl, relUrl, newUrl);
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
testHtmlExpandUrl();
return 0;
}

