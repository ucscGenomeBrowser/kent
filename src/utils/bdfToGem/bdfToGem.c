/* bdfToGem - convert font bdf files to Gem C source font definitions */

#include	"common.h"
#include	"options.h"
#include	"linefile.h"

static char const rcsid[] = "$Id: bdfToGem.c,v 1.1 2005/02/16 18:16:47 hiram Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

static void usage()
{
errAbort(
"bdfToGem - convert font bdf files to Gem C source font definitions\n"
"usage: bdfToGem <file.bdf> <gem_definition.c>\n"
"options: <none yet>"
);
}

int main( int argc, char *argv[] )
{
optionInit(&argc, argv, optionSpecs);

if (argc < 2)
    usage();

return(0);
}
