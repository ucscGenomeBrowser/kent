/* test - test out something. */
#include "common.h"
#include "portable.h"
#include "jksql.h"
#include "hash.h"
#include "dystring.h"
#include "linefile.h"
#include "fa.h"
#include "ens.h"
#include "unfin.h"

static char const rcsid[] = "$Id: test.c,v 1.3 2003/05/06 07:22:35 kate Exp $";


char *database = "ensdev";


void usage()
{
errAbort("usage: test \n");
}


int test()
{
}

int main(int argc, char *argv[])
{
test();
return 0;
}
