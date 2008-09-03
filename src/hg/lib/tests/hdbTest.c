/* Test functions in hdb.c */

#include "common.h"
#include "hdb.h"

int main(int argc, char *argv[])
{
char *db="hg15";
printf("%s organism ID = %d\n", db, hOrganismID(db));
printf("%s scientific name = %s\n", db, hScientificName(db));
return 0;
}
