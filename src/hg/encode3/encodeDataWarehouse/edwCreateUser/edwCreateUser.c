/* edwCreateUser - Create a new user from email/password combo.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "dystring.h"
#include "edwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwCreateUser - Create a new user from email combo.\n"
  "usage:\n"
  "   edwCreateUser email\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void edwCreateUser(char *email)
/* edwCreateUser - Create a new user from email.  Since we use persona we rely on them
 * for password handling. */
{
verbose(2, "edwCreateUser(email=%s)\n", email);
edwCreateNewUser(email);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
edwCreateUser(argv[1]);
return 0;
}
