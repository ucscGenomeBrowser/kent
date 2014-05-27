/* edwChangeFormat - Change format and force a revalidation for a file.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

char *tagToChange="format";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwChangeFormat - Change format or other tag and force a revalidation for a file.\n"
  "usage:\n"
  "   edwChangeFormat newValue id1 id2 ... idN\n"
  "change format of files with given ids to new format. File will keep it's license plate but end\n"
  "up with an error message if there's a problem validating it in the new format.  This is mostly\n"
  "used in practice on things submitted as 'unknown' where we now have validators for the format.\n"
  "Also this has been used to correct glitches in load from ENCODE2.\n"
  "options:\n"
  "   -tagToChange=tagName What tag to update with a new value - default is format.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"tagToChange", OPTION_STRING},
   {NULL, 0},
};

void changeFormat(struct sqlConnection *conn, struct edwValidFile *vf, char *format)
/* Set up vf to change format. */
{
struct edwFile *ef = edwFileFromId(conn, vf->fileId);
char *newTags = cgiStringNewValForVar(ef->tags, tagToChange, format);
edwFileResetTags(conn, ef, newTags, TRUE);
edwFileFree(&ef);
}

void edwChangeFormat(char *format, int idCount, char *idStrings[])
/* edwChangeFormat - Change format and force a revalidation for a file.. */
{
struct sqlConnection *conn = edwConnectReadWrite();

/* Convert ascii id's to valid file ids so we catch errors early. */
long long ids[idCount];
struct edwValidFile *vfs[idCount];
int i;
for (i=0; i<idCount; ++i)
    {
    long long id = ids[i] = sqlLongLong(idStrings[i]);
    struct edwValidFile *vf = vfs[i] = edwValidFileFromFileId(conn, id);
    if (vf == NULL)
        errAbort("%lld is not a fileId in the edwValidFile table", id);
    }

/* Loop through each file and change format. */
for (i=0; i<idCount; ++i)
    {
    changeFormat(conn, vfs[i], format);
    }
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
tagToChange = optionVal("tagToChange", tagToChange);
if (argc < 3)
    usage();
edwChangeFormat(argv[1], argc-2, argv+2);
return 0;
}
