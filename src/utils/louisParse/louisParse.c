/* louisParse - program to parse Apache error-log to get what Louis needs. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "louisParse - program to parse Apache error-log to get what Louis needs\n"
  "usage:\n"
  "   louisParse logFile\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

#define VERSION 1

struct instanceData
{
time_t date;
char *db;
int elapsedTime;
int hubTracks;
int nativeTracks;
int customTracks;
char *pid;
int version;
int lineNo;
};


static void countTracks(struct instanceData *data, char *tracks)
{
// printf("tracks %s\n", tracks);
char *words[1024];
int wordCount = chopString(tracks, ",", words, ArraySize(words));
int ii;

for(ii = 0; ii < wordCount; ii++)
    {
    if(startsWith("hub_", words[ii]))
        data->hubTracks++;
    else if(startsWith("ct_", words[ii]))
        data->customTracks++;
    else
        data->nativeTracks++;
    }

}

static void outData(struct instanceData *data)
{
//printf("%ld\t%s\t%u\t%u\t%u\t%u\t%s\n", data->date, data->db, data->elapsedTime, data->hubTracks, data->nativeTracks, data->customTracks, data->pid);
int hubDb = 0;
if (startsWith("hub_", data->db))
    hubDb = 1;
printf("%ld\t%u\t%u\t%u\t%u\t%u\t%s\t%d\t%d\n", data->date,  data->elapsedTime, hubDb, data->hubTracks, data->nativeTracks, data->customTracks, data->db, data->version, data->lineNo);
}

void louisParse(char *logFile)
/* louisParse - program to parse Apache error-log to get what Louis needs. */
{
struct lineFile *lf = lineFileOpen(logFile, TRUE);
char *words[100];
int wordCount;
struct instanceData *data;
struct hash *pidHash = newHash(5);
struct hashEl *hel;
time_t t = time(NULL);
struct tm *tm = localtime(&t);
char date[64];
strftime(date, sizeof(date), "%c", tm);

printf("# louisParse ver%d %s %s\n",VERSION, date,  logFile);
printf("# date\telapsedTime\tisHub\t#HubTracks\t#NativeTracks\t#CustomTracks\tassembly\tversion\tline#\n");
while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    //printf("pid %s\n",words[7]);
    if (sameString(words[11], "trackLog") && differentString(words[12], "position"))
        {
        if ((hel = hashLookup(pidHash, words[7])) == NULL)
            {
            struct tm result;
            memset(&result, 0, sizeof(result));
            char buffer[1024];
            AllocVar(data);
            char *dot = strchr(words[3], '.');
            *dot = 0;
            dot = strchr(words[4], ']');
            *dot = 0;
            sprintf(buffer, "%s %s %s %s", words[1],words[2],words[3],words[4]);
            //printf("date %s\n",buffer);
            //strptime(buffer, "%m %d %T %y",&result);
            data->date= dateToSeconds(buffer, "%b %d %T %Y");
            data->pid= cloneString(words[7]);
            sprintf(buffer, "%s_%s", words[7],words[14]);
            //data->pid= cloneString(words[14]);
            //data->pid= cloneString(buffer);
            //printf("epoch %lu\n",data->date);
            //data->date =
            hashAdd(pidHash, words[7], data);
            data->db = cloneString(words[13]);
            }
        else
            data = hel->val;

        countTracks(data, words[15]);

            //{
            //printf("pid: %d\n", atoi(words[7]));
            //printf("database: %s\n", words[13]);
            //printf("tracks: %s\n", words[15]);
            //}
        }
    if (sameString(words[11], "CGI_TIME:"))
        {
        if ((hel = hashLookup(pidHash, words[7])) != NULL)
            {
            data = hel->val;
            data->lineNo = lf->lineIx;
            data->elapsedTime = atoi(words[16]);
            outData(data);
            hashRemove(pidHash, words[7]);
            }

            //printf("pid: %d\n", atoi(words[7]));
            //printf("time %s\n", words[16]);
        }
    //printf("%s %s %s %s\n", words[6], words[7], words[11], words[12]);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
louisParse(argv[1]);
return 0;
}
