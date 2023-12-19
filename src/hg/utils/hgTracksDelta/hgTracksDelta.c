/* hgTracksDelta - find differences in the images created by hgTracks. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "md5.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgTracksDelta - find differences in the images created by hgTracks\n"
  "usage:\n"
  "   hgTracksDelta sessions.txt machine1 machine2 pngDir logFile\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct session
{
struct session *next;
char *userName;
char *sessionName;
};

static struct session *readSessions(char *sessionsFile)
{
struct lineFile *lf = lineFileOpen(sessionsFile, TRUE);
struct session *sessionList = NULL, *session;

char *row[2];
while (lineFileRow(lf, row))
    {
    AllocVar(session);
    slAddHead(&sessionList, session);
    session->userName = cloneString(row[0]);
    session->sessionName = cloneString(row[1]);
    }

slReverse(&sessionList);
lineFileClose(&lf);

return sessionList;
}

#define URL_TEMPLATE "wget 'http://%s/cgi-bin/hgRenderTracks?hgS_doOtherUser=submit&hgS_otherUserName=%s&hgS_otherUserSessionName=%s' -O %s"


static void digDown(struct session *session, char *machine1, char *machine2, char *pngDir, FILE *logF)
{
exit(1);
}

static boolean fileCompare(char *file1, char *file2)
{
char *one = md5HexForFile(file1);
char *two = md5HexForFile(file2);

if (differentString(one, two))
    return TRUE;

return FALSE;
}

static void compareSession(struct session *session, char *machine1, char *machine2, char *pngDir, FILE *logF)
{
struct dyString *outFile1 = dyStringNew(50);
dyStringPrintf(outFile1, "%s/%s_%s.%s.png", pngDir, session->userName, session->sessionName, machine1);
struct dyString *dyUrl1 = dyStringNew(50);
dyStringPrintf(dyUrl1, URL_TEMPLATE,  machine1, session->userName, session->sessionName, outFile1->string);

system(dyUrl1->string);
sleep(1);

struct dyString *outFile2 = dyStringNew(50);
dyStringPrintf(outFile2, "%s/%s_%s.%s.png", pngDir, session->userName, session->sessionName, machine2);
struct dyString *dyUrl2 = dyStringNew(50);
dyStringPrintf(dyUrl2, URL_TEMPLATE,  machine1, session->userName, session->sessionName, outFile2->string);

system(dyUrl2->string);
sleep(1);

if (fileCompare(outFile1->string, outFile2->string))
    {
    fprintf(logF, "Difference %s %s\n", outFile1->string, outFile2->string);
    digDown(session, machine1, machine2, pngDir, logF);
    }
}

void hgTracksDelta(char *sessionsFile, char *machine1, char *machine2, char *pngDir, char *logFileName)
/* hgTracksDelta - find differences in the images created by hgTracks. */
{
struct session *sessions = readSessions(sessionsFile);
FILE *logF = mustOpen(logFileName, "w");

makeDirs(pngDir);
for(; sessions; sessions = sessions->next)
    compareSession(sessions, machine1, machine2, pngDir, logF);
fclose(logF);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
hgTracksDelta(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
