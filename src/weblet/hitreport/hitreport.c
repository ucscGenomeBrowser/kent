/* hitreport.c - Summarize hit count data. */
#include "common.h"
#include <time.h>
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"

boolean readWhence = FALSE;    /* Include where user came from? */
char *fileName;				   /* Name of file hit data stored in. */
char title[256];			   /* .html title. */

struct user
/* One of these gets made for each "user" (actually for
 * each IP address. */
    {
    struct user *next;	  /* Link to next user on list. */
    char *ip;			  /* Something like soe.ucsc.edu. */
    char *whence;		  /* http address we came from. */
    time_t lastTime;	  /* Time of hits in seconds since 1970s */
    int lastDay;		  /* Day of last hit (since 1970s) */
    int lastHour;		  /* Hour of last hit (since 1970s) */
    int hits;			  /* Total number of hits from user. */
    int days;			  /* # of distinct days user has hit page */
    int hours;			  /* # of distinct hours user has hit page */
    };

int hitCmp(const void *va, const void *vb)
/* Number of hits of two users for sorting purposes. */
{
const struct user *a = *((struct user **)va);
const struct user *b = *((struct user **)vb);
int dif = b->hits - a->hits;
if (dif == 0)
    dif = b->lastTime - a->lastTime;
return dif;
}

int timeCmp(const void *va, const void *vb)
/* Compare last time of hit of two users for sorting purposes. */
{
const struct user *a = *((struct user **)va);
const struct user *b = *((struct user **)vb);
return b->lastTime - a->lastTime;
}

void readStringz(FILE *f, char *s, int sLen)
/* Read in a zero terminated string from file into a
 * character array of given size. */
{
mustGetLine(f, s, sLen);
}

void printList(struct user *userList, int topCount)
/* Print user list. */
{
int i;
struct user *user;

printf("IP Address                                 #Days #Hours #Hits  %s     Last-Hit           \n",
    (readWhence ?  "           Referrer                            " : ""));
printf("---------------------------------------------------------------------------------------%s\n",
	   (readWhence ? "-------------------------------------------------" : ""));
for (user = userList, i=0; user != NULL && i < topCount; user = user->next, i++)
    {
    printf("%-41s %5d %6d %5d", user->ip, user->days, user->hours, user->hits);
    if (readWhence)
        printf("          %-40s ", user->whence);
    printf("    %s",         ctime(&user->lastTime));     
    }
}

void doMiddle()
/* Write middle part of .html. (Everything between <BODY> and </BODY>) */
{
FILE *f = mustOpen(fileName, "rb");
long count;
time_t time;
char ipNameBuf[80];
char whenceNameBuf[256];
long i;
struct hashEl *hel;
struct hash *hash;
struct hash *whenceHash;
struct user *user = NULL;
int day, hour;
struct user *userList = NULL;
int topCount;

hash = newHash(14);
whenceHash = newHash(14);
mustReadOne(f,count);
for (i=0; i<count; ++i)
    {
    if (!readOne(f, time))
        break;
    readStringz(f, ipNameBuf, sizeof(ipNameBuf));
    if ((hel = hashLookup(hash, ipNameBuf)) == NULL)
        {
        AllocVar(user);
        slAddHead(&userList, user);        
        hel = hashAdd(hash, ipNameBuf, user);
        user->ip = hel->name;
        }
    if (readWhence)
        {
        struct hashEl *wHel;
        readStringz(f, whenceNameBuf, sizeof(whenceNameBuf));
        if ((wHel = hashLookup(whenceHash, whenceNameBuf)) == NULL)
            {
            wHel = hashAdd(whenceHash, whenceNameBuf, NULL);
            }
        user->whence = wHel->name;
        }
    user = hel->val;
    user->lastTime = time;
    user->hits += 1;
    hour = time / (60*60);
    if (hour != user->lastHour)
        {
        user->lastHour = hour;
        user->hours += 1;
        }
    day = hour/24;
    if (day != user->lastDay)
        {
        user->lastDay = day;
        user->days += 1;
        }
    }
slReverse(userList);

printf("<H2>%s</H2>", title);
printf("<TT><PRE>\n");
printf("Total hits: %ld\n", count); 
topCount = slCount(userList);
printf("Total IP addresses %d\n\n", topCount);
if (topCount > 100)
    topCount = 100;

slSort(&userList, timeCmp);
printf("Most recent %d users:\n\n", topCount);
printList(userList, topCount);

slSort(&userList, hitCmp);
printf("\n\nTop %d users:\n\n", topCount);
printList(userList, topCount);
}

int main(int argc, char **argv)
{
fileName = cgiString("file");
readWhence = cgiBoolean("whence");
sprintf(title, "Hit statistics in %s", fileName);
htmShell(title, doMiddle, "QUERY");
return 0;
}
