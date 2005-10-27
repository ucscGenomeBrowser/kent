/* sqlProg - functions for building command line programs to deal with
 * sql databases.*/
#ifndef SQLPROG_H
#define SQLPROG_H

void sqlExecProg(char *prog, char **progArgs, int userArgc, char *userArgv[]);
/* Exec one of the sql programs using user and password from ~/.hg.conf.
 * progArgs is NULL-terminate array of program-specific arguments to add,
 * which maybe NULL. userArgv are arguments passed in from the command line.
 * The program is execvp-ed, this function does not return. */

#endif
