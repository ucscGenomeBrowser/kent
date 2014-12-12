/* cdwCreateUser - Create a new user from email/password combo.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "dystring.h"
#include "cdwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwCreateUser - Create a new user from email combo.\n"
  "usage:\n"
  "   cdwCreateUser email\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void cdwCreateUser(char *email)
/* cdwCreateUser - Create a new user from email.  Since we use persona we rely on them
 * for password handling. */
{
verbose(2, "cdwCreateUser(email=%s)\n", email);
cdwCreateNewUser(email);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
cdwCreateUser(argv[1]);
return 0;
}
