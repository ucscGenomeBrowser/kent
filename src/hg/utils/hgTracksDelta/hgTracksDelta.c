/* hgTracksDelta - find differences in the images created by hgTracks. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "md5.h"
#include "obscure.h"
#include "cart.h"
#include "trackHub.h"

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

#define URL_IMAGE "wget -q 'http://%s/cgi-bin/hgRenderTracks?hgS_doOtherUser=submit&hgS_otherUserName=%s&hgS_otherUserSessionName=%s' -O %s -o /dev/null"
#define URL_TRACKLIST "wget -q 'http://%s/cgi-bin/hgRenderTracks?dumpTracks=1&hgS_doOtherUser=submit&hgS_otherUserName=%s&hgS_otherUserSessionName=%s' -O %s -o /dev/null"


static void digDown(struct session *session, char *machine1, char *machine2, char *pngDir, FILE *logF)
{
//exit(1);
}

static boolean fileCompare(char *file1, char *file2)
{
char *one = md5HexForFile(file1);
char *two = md5HexForFile(file2);

if (differentString(one, two))
    return TRUE;

return FALSE;
}

static struct slName *getTrackList(struct session *session, char *machine, char *pngDir, FILE *logF)
{
struct dyString *outList = newDyString(50);
dyStringPrintf(outList, "%s/%s_%s.%s.tracks.txt", pngDir, session->userName, session->sessionName, machine);

struct dyString *dyUrl1 = newDyString(50);
dyStringPrintf(dyUrl1, URL_TRACKLIST,  machine, session->userName, session->sessionName, outList->string);
system(dyUrl1->string);
sleep(1);

char *string;
size_t size;
readInGulp(outList->string, &string, &size);

struct slName *list =  slNameListFromCommaEscaped(string);
slNameSort(&list);
//struct slName *tmp = list;
//for(; tmp; tmp = tmp->next)
    //printf("%s\n", tmp->name);
return list;
}

static boolean compareList(struct slName *list1,struct slName *list2, FILE *logF)
{
struct slName *tmp1 = list1;
struct slName *tmp2 = list2;
for(; (tmp1 != NULL) && (tmp2 != NULL); tmp1 = tmp1->next, tmp2 = tmp2->next)
    {
    if (differentString(trackHubSkipHubName(tmp1->name), trackHubSkipHubName(tmp2->name)))
        {
        //if (!(stringIn("trash/hgComposite/", tmp1->name) && stringIn("trash/hgComposite/", tmp2->name) ))
            {
            fprintf(logF, "Track list diff %s %s\n", tmp1->name, tmp2->name);

            return TRUE;
            }
        }
    }
if ((tmp1 != NULL) || (tmp2 != NULL))
    {
    fprintf(logF, "Track list different length\n");
    return TRUE;
    }
return FALSE;
}

static void compareSession(struct session *session, char *machine1, char *machine2, char *pngDir, FILE *logF)
{
<<<<<<< Updated upstream
struct dyString *outFile1 = dyStringNew(50);
dyStringPrintf(outFile1, "%s/%s_%s.%s.png", pngDir, session->userName, session->sessionName, machine1);
struct dyString *dyUrl1 = dyStringNew(50);
dyStringPrintf(dyUrl1, URL_TEMPLATE,  machine1, session->userName, session->sessionName, outFile1->string);
=======
struct slName *trackList1 = getTrackList(session, machine1, pngDir, logF);
struct slName *trackList2 = getTrackList(session, machine2, pngDir, logF);
if (compareList(trackList1, trackList2, logF))
    {
    return;
    }

struct dyString *outPng1 = newDyString(50);
dyStringPrintf(outPng1, "%s/%s_%s.%s.png", pngDir, session->userName, session->sessionName, machine1);
struct dyString *dyUrl1 = newDyString(50);
dyStringPrintf(dyUrl1, URL_IMAGE,  machine1, session->userName, session->sessionName, outPng1->string);

system(dyUrl1->string);
sleep(1);

struct dyString *outFile2 = dyStringNew(50);
dyStringPrintf(outFile2, "%s/%s_%s.%s.png", pngDir, session->userName, session->sessionName, machine2);
struct dyString *dyUrl2 = dyStringNew(50);
dyStringPrintf(dyUrl2, URL_TEMPLATE,  machine1, session->userName, session->sessionName, outFile2->string);
struct dyString *outPng2 = newDyString(50);
dyStringPrintf(outPng2, "%s/%s_%s.%s.png", pngDir, session->userName, session->sessionName, machine2);
struct dyString *dyUrl2 = newDyString(50);
dyStringPrintf(dyUrl2, URL_IMAGE,  machine1, session->userName, session->sessionName, outPng2->string);

system(dyUrl2->string);
sleep(1);

if (fileCompare(outPng1->string, outPng2->string))
    {
    fprintf(logF, "Image difference %s %s\n", outPng1->string, outPng2->string);
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
