/* Test functions in hdb.c */

/* Copyright (C) 2008 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hdb.h"

int main(int argc, char *argv[])
{
char *db="hg15";
printf("%s organism ID = %d\n", db, hOrganismID(db));
printf("%s scientific name = %s\n", db, hScientificName(db));
return 0;
}
