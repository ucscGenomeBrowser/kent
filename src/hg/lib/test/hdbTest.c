/* Test functions in hdb.c */

#include "common.h"
#include "hdb.h"

int main(int argc, char *argv[])
{
    hSetDb("hg15");
    printf("hg15 organism ID = %d\n", hOrganismID("hg15"));
    printf("hg15 scientific name = %s\n", hScientificName("hg15"));
    return 0;
}
