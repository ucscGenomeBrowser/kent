/* gensatFixFull - Fix a problem that arose from image magic convert not handling .full in filename 
 * during tiling.  Should be a one-time fix.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "jksql.h"
#include "dystring.h"


char *database = "visiGene";
char *thumbDir = "/cluster/store11/visiGene/gbdb/200/inSitu/Mouse/gensat";
char *fullDir = "/cluster/store11/visiGene/gbdb/full/inSitu/Mouse/gensat";
char *downloadDir = "/cluster/store11/visiGene/offline/gensat/Institutions";
int gensatId = 706;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gensatFixFull - Fix a problem that arose from image magic convert not handling .full "
  "in filename during tiling.  Should be a one-time fix.\n"
  "usage:\n"
  "   gensatFixFull caption.txt\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};


void gensatFixFull(char *captionFile)
/* Fix missing captions. */
{
struct lineFile *lf = lineFileOpen(captionFile, TRUE);
char *row[2];
struct dyString *sql = dyStringNew(0);
struct sqlConnection *conn = sqlConnect(database);
struct hash *capHash = newHash(16);
while (lineFileRowTab(lf, row))
    {
    int captionId;
    char *submitId = row[0];
    char *caption = row[1];
    captionId = hashIntValDefault(capHash, caption, 0);
    if (captionId == 0)
        {
	dyStringClear(sql);
	sqlDyStringPrintf(sql, "insert into caption values(default, '%s')", caption);
	sqlUpdate(conn, sql->string);
	verbose(1, "%s\n", sql->string);
	captionId = sqlLastAutoId(conn);
	hashAddInt(capHash, caption, captionId);
	}
    dyStringClear(sql);
    sqlDyStringPrintf(sql, "update imageFile set caption=%d ", captionId);
    sqlDyStringPrintf(sql, "where submissionSet=%d ", gensatId);
    sqlDyStringPrintf(sql, "and submitId = '%s'", submitId);
    sqlUpdate(conn, sql->string);
    verbose(1, "%s\n", sql->string);
    }
dyStringFree(&sql);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
gensatFixFull(argv[1]);
return 0;
}
