
/* qaPushQ - Push Queue cgi */

/* we don't use the cart, we have our own table to store cart-like info */

#include <time.h>
#include <stdio.h>
#include <crypt.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include <sys/stat.h>

#include "common.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "jksql.h"
#include "hgConfig.h"
#include "obscure.h"
#include "portable.h"

#include "pushQ.h"
#include "formPushQ.h"

#include "versionInfo.h"

/* stuff to support outputting Release Log html */
#include "web.h"
#include "hui.h"
#include "dbDb.h"
#include "htmlPage.h"

static char const rcsid[] = "$Id: qaPushQ.c,v 1.124 2010/04/16 19:05:01 galt Exp $";

char msg[2048] = "";
char ** saveEnv;

#define BLSIZE 512000  /* size of strings for processing big lists of tables and files */
#define BUFMAX 512000
char html[BUFMAX];

char *action = NULL;   /* have to put declarations first */
boolean crossPost = FALSE;  /* are we doing cross-post from dev to beta (or vice versa)? */
			    /* support showSizes across machines */

char *database = NULL;
char *host     = NULL;
char *user     = NULL;
char *password = NULL;

struct sqlConnection *conn = NULL;
struct sqlConnection *conn2 = NULL;

char *qaUser = NULL;

#define SSSZ 256  /* MySql String Size 255 + 1 */
#define MAXBLOBSHOW 128


#define TITLE "Push Queue v"CGI_VERSION

time_t curtime;
struct tm *loctime;

struct utsname utsName;

struct users myUser;

char *showColumns = NULL;

char *defaultColumns =
    "pqid,qid,priority,importance,qadate,track,dbs,tbls,cgis,files,currLoc,makeDocYN,onlineHelp,ndxYN,stat,sponsor,reviewer,extSource,notes";

char *newRandState = NULL;
char *oldRandState = NULL;

/*
"qid,pqid,priority,rank,qadate,newYN,track,dbs,tbls,cgis,files,sizeMB,currLoc,"
"makeDocYN,onlineHelp,ndxYN,joinerYN,stat,sponsor,reviewer,extSource,openIssues,notes,"
pushState,initdate,bounces,lockUser,lockDateTime,releaseLog,featureBits,releaseLogUrl,importance";
*/

/* structural improvements suggested by MarkD:

static struct {
enum colEnum col;
char *name;
char *hdr;
} colTbl [] = {
{e_qid, "qid"},
{0,      NULL},
}

ArraySize(colTbl)

numWords = chopString(liststr, ",", NULL,  NULL);

*/


static char const *colName[] = {
 "qid"       ,
 "pqid"      ,
 "priority"  ,
 "rank"      ,
 "qadate"    ,
 "newYN"     ,
 "track"     ,
 "dbs"       ,
 "tbls"      ,
 "cgis"      ,
 "files"     ,
 "sizeMB"    ,
 "currLoc"   ,
 "makeDocYN" ,
 "onlineHelp",
 "ndxYN"     ,
 "joinerYN"  ,
 "stat"      ,
 "featureBits",
 "sponsor"   ,
 "reviewer"  ,
 "extSource" ,
 "openIssues",
 "notes"     ,
 "pushState" ,
 "initdate"  ,
 "lastdate"  ,
 "bounces"   ,
 "lockUser"  ,
 "lockDateTime",
 "releaseLog",
 "releaseLogUrl",
 "importance"
};


enum colEnum {
e_qid       ,
e_pqid      ,
e_priority  ,
e_rank      ,
e_qadate    ,
e_newYN     ,
e_track     ,
e_dbs       ,
e_tbls      ,
e_cgis      ,
e_files     ,
e_sizeMB    ,
e_currLoc   ,
e_makeDocYN ,
e_onlineHelp,
e_ndxYN     ,
e_joinerYN  ,
e_stat      ,
e_featureBits,
e_sponsor   ,
e_reviewer  ,
e_extSource ,
e_openIssues,
e_notes     ,
e_pushState ,
e_initdate  ,
e_lastdate  ,
e_bounces   ,
e_lockUser  ,
e_lockDateTime,
e_releaseLog,
e_releaseLogUrl,
e_importance,
e_NUMCOLS
};



char *colHdr[] = {
"Queue ID",
"Parent Queue ID",
"Priority",
"Rank",
"&nbsp;&nbsp;&nbsp;&nbsp;Date&nbsp;&nbsp;&nbsp;&nbsp;",
"New?",
"Track",
"Databases",
"Tables",
"CGIs",
"Files",
"Size MB",
"Current Location",
"MakeDoc",
"Online help",
"Index",
"All. Joiner",
"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Status&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;",
"Feature&nbsp;Bits",
"Sponsor (local)",
"Reviewer",
"External Source or Collaborator",
"Open Issues",
"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Notes&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;",
"PushState",
"Initial &nbsp;&nbsp;Submission&nbsp;&nbsp; Date",
"Last &nbsp;&nbsp;QA&nbsp;&nbsp; Date",
"Bounce Count",
"Lock User",
"Lock&nbsp;Date&nbsp;Time",
"Release&nbsp;Log",
"Release&nbsp;Log&nbsp;Url",
"Importance"
};

char *numberToMonth[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

char pushQtbl[256] = "pushQ";   /* default */

char month[256] = "";

enum colEnum colOrder[e_NUMCOLS];
int numColumns = 0;

int randInt(int N)
/* generate random number from 0 to N-1 */
{
return (int) N * (rand() / (RAND_MAX + 1.0));
}

char *randDigits(int length)
/* generate string of random digits 0-9. String will need to be free'd later. */
{
int i = 0;
char *s = needMem(length+1);
for(i=0;i<length;i++)
    {
    s[i]='0'+randInt(10);
    }
s[length]=0;
return s;
}


bool isDateValid(char *s)
/* check if date has valid format only YYYY-MM-DD tolerated. *
 * YYYY must be 1900-2100.  MM 01-12.  DD 01-31.
*/
{
int yyyy = 0, mm = 0, dd = 0;
char *y = NULL, *m = NULL, *d = NULL;
if (strlen(s)!=10) return FALSE;
if (s[4]!='-') return FALSE;
if (s[7]!='-') return FALSE;
y = cloneStringZ(&s[0],4);
m = cloneStringZ(&s[5],2);
d = cloneStringZ(&s[8],2);
if (sscanf(y,"%d",&yyyy) != 1) return FALSE;
if (sscanf(m,"%d",&mm)   != 1) return FALSE;
if (sscanf(d,"%d",&dd)   != 1) return FALSE;
if (yyyy < 1900) return FALSE;
if (yyyy > 2100) return FALSE;
if (  mm > 12  ) return FALSE;
if (  mm <  1  ) return FALSE;
if (  dd > 31  ) return FALSE;
if (  dd <  1  ) return FALSE;
return TRUE;
}


void encryptPWD(char *password, char *salt, char *buf, int bufsize)
/* encrypt a password */
{
/* encrypt user's password. */
safef(buf,bufsize,crypt(password, salt));
}


void encryptNewPWD(char *password, char *buf, int bufsize)
/* encrypt a new password */
{
unsigned long seed[2];
char salt[] = "$1$........";
const char *const seedchars =
"./0123456789ABCDEFGHIJKLMNOPQRST"
"UVWXYZabcdefghijklmnopqrstuvwxyz";
int i;
/* Generate a (not very) random seed. */
seed[0] = time(NULL);
seed[1] = getpid() ^ (seed[0] >> 14 & 0x30000);
/* Turn it into printable characters from `seedchars'. */
for (i = 0; i < 8; i++)
    salt[3+i] = seedchars[(seed[i/5] >> (i%5)*6) & 0x3f];
encryptPWD(password, salt, buf, bufsize);
}

bool checkPWD(char *password, char *encPassword)
/* check an encrypted password */
{
char encPwd[35] = "";
encryptPWD(password, encPassword, encPwd, sizeof(encPwd));
if (sameString(encPassword,encPwd))
    {
    return TRUE;
    }
else
    {
    return FALSE;
    }
}



void doMsg()
/* callable from htmShell */
{
printf("%s",msg);
}

bool mySqlGetLock(char *name, int timeout)
/* Tries to acquire (for 10 seconds) and set an advisory lock.
 *  note: mysql returns 1 if successful,
 *   0 if name already locked or NULL if error occurred
 *   blocks another client from obtaining a lock with the same name
 *   lock is automatically released by mysql when connection is closed or detected broken
 *   may even detect program crash and release lock.
 */
{
char query[256];
struct sqlResult *rs;
char **row = NULL;
bool result = FALSE;

safef(query, sizeof(query), "select get_lock('%s', %d)", name, timeout);
rs = sqlGetResult(conn, query);
row=sqlNextRow(rs);
if (row[0] == NULL)
    {
    safef(msg, sizeof(msg), "Attempt to GET_LOCK of %s caused an error\n",name);
    htmShell(TITLE, doMsg, NULL);
    exit(0);
    }
if (sameWord(row[0], "1"))
    result = TRUE;
else if (sameWord(row[0], "0"))
    result = FALSE;
sqlFreeResult(&rs);
return result;
}

void mySqlReleaseLock(char *name)
/* Releases an advisory lock created by GET_LOCK in mySqlGetLock */
{
char query[256];
safef(query, sizeof(query), "select release_lock('%s')", name);
sqlUpdate(conn, query);
}



void setLock()
/* set a lock to reduce concurrency problems */
{
mySqlGetLock("qapushq",10);    /* just an advisory semaphore, really */
}

void releaseLock()
/* release the advisory lock */
{
mySqlReleaseLock("qapushq");
}


enum colEnum mapFieldToEnum(char *f, bool must)
{
int i = 0;
for(i=0;i<e_NUMCOLS;i++)
    {
    if (sameString(colName[i],f))
	{
	return (enum colEnum) i;
	}
    }
if (must)
    {
    errAbort("Field not found in mapFieldToEnum: %s",f);
    }
return -1;
}



void replaceInStr(char *s, int ssize, char *t, char *r)
/* Strings: Replace an occurrence of t in s with r.
 * size is sizeof(s)
 * note: s must have room to hold any expansion, as it happens in-place in s */
{
char *temp = replaceChars(s,t,r);
if (strlen(temp) >= ssize)
    {
    errAbort("buf size exceeded. strlen(temp)=%ld, size of buf= %d",(unsigned long)strlen(temp),ssize);
    }
safef(s,ssize,"%s",temp);
freez(&temp);
}




bool parseList(char *s, char delim, int num, char *buf, int bufsize)
/* parse list to find nth element of delim-separated list in string
 * returns l if not found which points to the string end 0
 * otherwise returns offset in s where begins */
{
int n = 0;
int i = 0;
int l = strlen(s);
int j = 0;
char c;
if (bufsize==0)
    {
    return FALSE;
    }
while (TRUE)
    {
    if (n==num)
	{
	while (j<bufsize)
	    {
	    c = s[i+j];
	    if (c==delim)
		{
		c = 0;
		}
	    buf[j] = c;
	    if (c == 0)
		{
		return TRUE;
		}
	    j++;
	    }
	buf[bufsize-1] = 0;  /* add term in case */
	return TRUE;
	}
    while (s[i] != delim)
	{
	if (i >= l)
	    {
	    buf[0] = 0;
	    return FALSE;
	    }
	i++;
	}
    i++;
    n++;
    }
}


void replaceSelectOptions(char *varname, char *values, char *value)
/* Replace <!sel-name-val> tags in select option picklist.
 * This initializes the correct default selection.
 * Currently values is a list of single characters only,
 * picklists with string-values not supported. */
{
int i = 0;
char *p = NULL;
char tempTag[256] = "";
char tempVal[256];
while(TRUE)
    {
    parseList(values,',',i,tempVal,sizeof(tempVal));
    if (tempVal[0]==0)
	{
	break;
	}
    safef(tempTag,sizeof(tempTag),"<!sel-%s-%s>",varname,tempVal);
    if (sameString(value,tempVal))
	{p = "selected";}
    else
	{p = "";}
    replaceInStr(html, sizeof(html), tempTag, p);
    i++;
    }
}



void initColsFromString()
{
int i = 0, e=0;
char colName[256];
char sep[2]="";
struct dyString * s = NULL;

s = newDyString(2048);  /* need room */
numColumns=0;
while(parseList(showColumns,',',i,colName,sizeof(colName)))
    {
    e = mapFieldToEnum(colName,FALSE);
    if (e >= 0) /* tolerate old nonexistent colnames in pseudocart more gracefully */
	{
	colOrder[i] = e;
	dyStringPrintf(s, "%s%s", sep, colName);
	safef(sep,sizeof(sep),",");
	numColumns++;
	}
    i++;
    }
showColumns = cloneString(s->string);
freeDyString(&s);

}



/* -------------------- Push Queue ----------------------- */

void showSizesJavascript()
/* set showSizes for cross-posting to support file sizes */
{
char sizesButton[1024];
safef(sizesButton, sizeof(sizesButton),
    "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<input TYPE=SUBMIT NAME=\"showSizes\" VALUE=\"Show Sizes\""
    " ONCLICK=\"if (document.forms[0].currLoc.value!='%s') {"
		" document.forms[0]._action.value='xpost';"
		" document.forms[0].action='http://%s.cse.ucsc.edu/cgi-bin/qaPushQ';"
		"};return true;\">"
    , utsName.nodename
    , sameString(utsName.nodename, "hgwdev") ? "hgwbeta" : "hgwdev"
    );
replaceInStr(html, sizeof(html), "<!sizesbutton>", sizesButton);
}

void replacePushQFields(struct pushQ *ki, bool isNew)
/* fill in dummy html tags with values from the sql pushQ record */
{
char tempLink[256];
char tempSizeMB[256];
char tempMsg[256];
bool myLock = FALSE;

if (sameString(ki->lockUser,qaUser))
    {
    myLock = TRUE;
    }

safef(html,sizeof(html),"%s",formQ);

safef(tempSizeMB, sizeof(tempSizeMB), "%u", ki->sizeMB);
if (ki->sizeMB == 0)
    {
    safef(tempSizeMB,sizeof(tempSizeMB),"%s","");
    }


replaceInStr(html, sizeof(html) , "<!qid>"         , ki->qid       );
replaceSelectOptions("priority" , "A,B,C,D,L"      , ki->priority  );
replaceInStr(html, sizeof(html) , "<!qadate>"      , ki->qadate    );
replaceSelectOptions("newYN"    ,"Y,N,X"           , ki->newYN     );
replaceInStr(html, sizeof(html) , "<!track>"       , ki->track     );
replaceInStr(html, sizeof(html) , "<!dbs>"         , ki->dbs       );
replaceInStr(html, sizeof(html) , "<!tbls>"        , ki->tbls      );
replaceInStr(html, sizeof(html) , "<!cgis>"        , ki->cgis      );
replaceInStr(html, sizeof(html) , "<!files>"       , ki->files     );
replaceInStr(html, sizeof(html) , "<!sizeMB>"      , tempSizeMB    );
replaceInStr(html, sizeof(html) , "<!currLoc>"     , ki->currLoc   );
replaceSelectOptions("currLoc"  , "hgwdev,hgwbeta" , ki->currLoc   );
replaceSelectOptions("makeDocYN", "Y,N,X"          , ki->makeDocYN );
replaceInStr(html, sizeof(html) , "<!onlineHelp>"  , ki->onlineHelp);
replaceSelectOptions("ndxYN"    , "Y,N,X"          , ki->ndxYN     );
replaceSelectOptions("joinerYN" , "Y,N,X"          , ki->joinerYN  );
replaceInStr(html, sizeof(html) , "<!stat>"        , ki->stat      );
replaceInStr(html, sizeof(html) , "<!featureBits>" , ki->featureBits);
replaceInStr(html, sizeof(html) , "<!sponsor>"     , ki->sponsor   );
replaceInStr(html, sizeof(html) , "<!reviewer>"    , ki->reviewer  );
replaceInStr(html, sizeof(html) , "<!extSource>"   , ki->extSource );
replaceInStr(html, sizeof(html) , "<!openIssues>"  , ki->openIssues);
replaceInStr(html, sizeof(html) , "<!notes>"       , ki->notes     );
replaceInStr(html, sizeof(html) , "<!initdate>"    , ki->initdate  );
replaceInStr(html, sizeof(html) , "<!releaseLog>"  , ki->releaseLog);
replaceInStr(html, sizeof(html) , "<!releaseLogUrl>", ki->releaseLogUrl);
replaceSelectOptions("importance", " ,B,L,M,H,U"   , ki->importance  );

replaceInStr(html, sizeof(html) , "<!cb>"          , newRandState  );

if (isNew)
    {
    replaceInStr(html, sizeof(html), "<!DISABLED>", "");
    replaceInStr(html, sizeof(html), "<!READONLY>", "");
    replaceInStr(html, sizeof(html), "<!submitbutton>", "<input TYPE=SUBMIT NAME=\"submit\" VALUE=\"Submit\" >&nbsp;&nbsp;");
    replaceInStr(html, sizeof(html), "<!delbutton>", "");
    replaceInStr(html, sizeof(html), "<!pushbutton>", "");
    replaceInStr(html, sizeof(html), "<!clonebutton>", "");
    replaceInStr(html, sizeof(html), "<!bouncebutton>", "");
    replaceInStr(html, sizeof(html), "<!lockbutton>", "");
    replaceInStr(html, sizeof(html), "<!refreshlink>", "");
    replaceInStr(html, sizeof(html), "<!transferbutton>", "");

    safef(tempLink, sizeof(tempLink), "<a href=\"/cgi-bin/qaPushQ?cb=%s\">CANCEL</a>&nbsp;&nbsp;",newRandState);
    replaceInStr(html, sizeof(html), "<!cancellink>", tempLink );

    showSizesJavascript();

    }
else
    {
    if (myLock)
	{
	replaceInStr(html, sizeof(html), "<!DISABLED>", "");
    	replaceInStr(html, sizeof(html), "<!READONLY>", "");

        replaceInStr(html, sizeof(html), "<!submitbutton>", "<input TYPE=SUBMIT NAME=\"submit\" VALUE=\"Submit\" >&nbsp;&nbsp;");
	replaceInStr(html, sizeof(html),
	    "<!delbutton>" ,
	    "<input TYPE=SUBMIT NAME=\"delbutton\"  VALUE=\"delete\">&nbsp;&nbsp;"
	    );

	if (ki->priority[0]!='L')
	    {
	    replaceInStr(html, sizeof(html),
		"<!pushbutton>",
		"<input TYPE=SUBMIT NAME=\"pushbutton\" VALUE=\"push requested\">&nbsp;&nbsp;"
		);
	    }

	replaceInStr(html, sizeof(html),
	    "<!clonebutton>",
	    "<input TYPE=SUBMIT NAME=\"clonebutton\" VALUE=\"clone\">&nbsp;&nbsp;"
	    );

	if (ki->priority[0]=='A')
	    {
	    replaceInStr(html, sizeof(html),
		"<!bouncebutton>",
		"<input TYPE=SUBMIT NAME=\"bouncebutton\" VALUE=\"bounce\">&nbsp;&nbsp;"
		);
	    }
	else
	    {
	    replaceInStr(html, sizeof(html),
		"<!bouncebutton>",
		"<input TYPE=SUBMIT NAME=\"bouncebutton\" VALUE=\"unbounce\">&nbsp;&nbsp;"
		);
	    }

	replaceInStr(html, sizeof(html), "<!lockbutton>", "");

	replaceInStr(html, sizeof(html), "<!cancellink>",
	    "<input TYPE=SUBMIT NAME=\"cancelbutton\" VALUE=\"Cancel\" >&nbsp;&nbsp;");

	replaceInStr(html, sizeof(html), "<!refreshlink>", "");

	replaceInStr(html, sizeof(html),
	    "<!transferbutton>",
	    "<input TYPE=SUBMIT NAME=\"transfer\" VALUE=\"Transfer\">&nbsp;&nbsp;");

	showSizesJavascript();
	}
    else
	{ /* we don't have a lock yet, disable and readonly */
	replaceInStr(html, sizeof(html), "<!DISABLED>", "DISABLED");
    	replaceInStr(html, sizeof(html), "<!READONLY>", "READONLY");
	replaceInStr(html, sizeof(html), "<!submitbutton>", "");
	replaceInStr(html, sizeof(html), "<!delbutton>", "");
	replaceInStr(html, sizeof(html), "<!pushbutton>", "");
	replaceInStr(html, sizeof(html), "<!clonebutton>", "");
	replaceInStr(html, sizeof(html), "<!bouncebutton>", "");
	replaceInStr(html, sizeof(html), "<!lockbutton>",
	    "<input TYPE=SUBMIT NAME=\"lockbutton\" VALUE=\"Lock\" >&nbsp;&nbsp;");

	safef(tempLink, sizeof(tempLink),
	    "<a href=\"/cgi-bin/qaPushQ?cb=%s\">RETURN</a>&nbsp;&nbsp;",newRandState);
	replaceInStr(html, sizeof(html), "<!cancellink>", tempLink );

	safef(tempLink, sizeof(tempLink),
	    "<a href=\"/cgi-bin/qaPushQ?action=edit&qid=%s&cb=%s\">REFRESH</a>&nbsp;&nbsp;",
	    ki->qid,newRandState);
	replaceInStr(html, sizeof(html), "<!refreshlink>", tempLink );

	replaceInStr(html, sizeof(html), "<!transferbutton>", "");
	replaceInStr(html, sizeof(html), "<!sizesbutton>", "");
	if (sameString(ki->lockUser,""))
	    {
	    safef(tempMsg,sizeof(tempMsg),"%s %s", msg, "READONLY view. Press Lock to edit. Click RETURN for the main queue.");
	    }
	else
	    {
	    safef(tempMsg,sizeof(tempMsg),"%s User %s currently has lock on Queue Id %s since %s.",
		msg, ki->lockUser, ki->qid,ki->lockDateTime );
	    }
	safef(msg,sizeof(msg), "%s", tempMsg);
	}
    }

replaceInStr(html, sizeof(html) , "<!msg>"         , msg           );

safef(msg,sizeof(msg),"%s","");

printf("%s",html);

}


void doAdd()
/* handle setup add form for new pushQ record */
{

struct pushQ q;
ZeroVar(&q);

q.next = NULL;
safef(q.qid, sizeof(q.qid), "%s", "");
safef(q.priority, sizeof(q.priority), "%s", "A");  /* default priority */
strftime (q.qadate, sizeof(q.qadate), "%Y-%m-%d", loctime); /* default to today's date */
safef(q.newYN, sizeof(q.newYN), "%s", "N");  /* default to not new track */
q.track  = "";
q.dbs    = "";
q.tbls   = "";
q.cgis   = "";
q.files  = "";
q.sizeMB = 0;
safef(q.currLoc   , sizeof(q.currLoc)   , "%s", "hgwdev");  /* default loc */
safef(q.makeDocYN , sizeof(q.makeDocYN) , "%s", "N");
safef(q.onlineHelp, sizeof(q.onlineHelp), "%s", "" );
safef(q.ndxYN     , sizeof(q.ndxYN)     , "%s", "N");  /* default to not checked yet */
safef(q.joinerYN  , sizeof(q.joinerYN)  , "%s", "N");  /* default to all.joiner not checked yet */
q.stat   = "";
q.featureBits = "";

safef(q.sponsor   , sizeof(q.sponsor)   , "%s", "" );
safef(q.reviewer  , sizeof(q.reviewer)  , "%s", "" );
if (sameString(myUser.role,"qa"))
    {
    safef(q.reviewer, sizeof(q.reviewer), "%s", qaUser);   /* if role is qa, default reviewer to this user */
    }
if (sameString(myUser.role,"dev"))
    {
    safef(q.sponsor , sizeof(q.sponsor) , "%s", qaUser);   /* if role is dev, default sponsor to current user */
    }
safef(q.extSource , sizeof(q.extSource) , "%s", "" );
q.openIssues   = "";
q.notes   = "";
strftime (q.initdate, sizeof(q.initdate), "%Y-%m-%d", loctime); /* automatically use today date */
safef(q.lastdate, sizeof(q.lastdate), "%s", "" );
q.releaseLog = "";
q.releaseLogUrl = "";
safef(q.importance, sizeof(q.importance), "%s", " ");  /* default importance */

if (sameString(myUser.role,"dev"))
    {
    safef(msg, sizeof(msg), "%s",
	"Developer: Please leave priority and date alone. "
	"Do specify if the track is new. Enter the shortLabel for the track name. "
	"Be sure to fill out the database, tables,  and external files, if any. "
	"If relevant cgis have changed since last branch, please list them. "
	"Enter collaborator if applicable. "
	"Leave the other fields for QA. You may leave a note for QA staff in the notes field. "
	"Thanks very much!"
	);
    }

replacePushQFields(&q, TRUE);  /* new rec = true */

}

void dotdotdot(char *s, int l)
/* truncate a string for shorter display with ... at end */
{
if ((strlen(s)<l) || (strlen(s)<5))
    {
    return;
    }
s[l]=0;
s[l-1]='.';
s[l-2]='.';
s[l-3]='.';
s[l-4]=' ';
}

char *fixLineBreaks(char *s)
/* insert <br> before \n to have an effect inside html */
{
return replaceChars(s,"\n","<br>\n");
}


void drawDisplayLine(enum colEnum col, struct pushQ *ki)
{
char *temp = NULL;
switch(col)
    {
    case e_qid:
	printf("<td><A href=\"/cgi-bin/qaPushQ?action=edit&qid=%s&cb=%s\">%s</A>%s",
	    ki->qid, newRandState, ki->qid, sameString(ki->lockUser,"") ? "":"*" );
	if (ki->pushState[0]=='Y')
	    {
	    printf("<BR><A href=\"/cgi-bin/qaPushQ?action=pushDone&qid=%s&cb=%s\">Done!</A>",
		ki->qid, newRandState );
	    }
	printf("</td>\n");
	break;

    case e_pqid:
	printf("<td>%s</td>\n", ki->pqid   );
	break;


    case e_priority:
	if (ki->priority[0] == 'L')
	    {
	    printf("<td>%s</td>", ki->priority);
	    }
	else
	    {
	    printf(
		"<td><table><tr><td>%s</td><td>"
		"<A href=\"/cgi-bin/qaPushQ?action=promote&qid=%s&cb=%s\">^</A>&nbsp;&nbsp;"
		"<A href=\"/cgi-bin/qaPushQ?action=top&qid=%s&cb=%s\">T</A>&nbsp;&nbsp;"
		"</td></tr><tr><td>&nbsp</td><td>"
		"<A href=\"/cgi-bin/qaPushQ?action=demote&qid=%s&cb=%s\">v</A>&nbsp;&nbsp;"
		"<A href=\"/cgi-bin/qaPushQ?action=bottom&qid=%s&cb=%s\">B</A>"
		"</td></tr></table></td>\n",
		ki->priority,
		ki->qid, newRandState,
		ki->qid, newRandState,
		ki->qid, newRandState,
		ki->qid, newRandState
		);
	    }
	break;

    case e_rank:
	printf("<td>%d</td>\n", ki->rank      );
	break;

    case e_qadate:
	printf("<td>%s</td>\n", ki->qadate    );
	break;

    case e_newYN:
	printf("<td>%s</td>\n", ki->newYN     );
	break;

    case e_track:
	dotdotdot(ki->track,MAXBLOBSHOW);  /* chr(255) */
	printf("<td>%s</td>\n", ki->track     );
	break;

    case e_dbs:
	dotdotdot(ki->tbls ,MAXBLOBSHOW);  /* chr(255) */
	printf("<td>%s</td>\n", ki->dbs       );
	break;

    case e_tbls:
	dotdotdot(ki->tbls,MAXBLOBSHOW);  /* longblob */
	printf("<td>%s</td>\n", ki->tbls      );
	break;

    case e_cgis:
	dotdotdot(ki->cgis ,MAXBLOBSHOW);  /* chr(255) */
	printf("<td>%s</td>\n", ki->cgis      );
	break;

    case e_files:
	dotdotdot(ki->files,MAXBLOBSHOW);  /* chr(255) */
	printf("<td>%s</td>\n", fixLineBreaks(ki->files)  );
	break;

    case e_sizeMB:
	printf("<td>%u</td>\n", ki->sizeMB    );
	break;

    case e_currLoc:
	printf("<td>%s</td>\n", ki->currLoc   );
	break;

    case e_makeDocYN:
	printf("<td>%s</td>\n", ki->makeDocYN );
	break;

    case e_onlineHelp:
	printf("<td>%s</td>\n", ki->onlineHelp);
	break;

    case e_ndxYN:
	printf("<td>%s</td>\n", ki->ndxYN     );
	break;

    case e_joinerYN:
	printf("<td>%s</td>\n", ki->joinerYN  );
	break;

    case e_stat:
	dotdotdot(ki->stat ,MAXBLOBSHOW);  /* chr(255) */
	printf("<td>%s</td>\n", ki->stat      );
	break;

    case e_featureBits:
	dotdotdot(ki->featureBits,MAXBLOBSHOW);  /* longblob */
	printf("<td>%s</td>\n", ki->featureBits);
	break;

    case e_sponsor:
	printf("<td>%s</td>\n", ki->sponsor   );
	break;

    case e_reviewer:
	printf("<td>%s</td>\n", ki->reviewer  );
	break;

    case e_extSource:
	printf("<td>%s</td>\n", ki->extSource );
	break;

    case e_openIssues:
	dotdotdot(ki->openIssues,MAXBLOBSHOW);  /* longblob */
	printf("<td>%s</td>\n", ki->openIssues);
	break;

    case e_notes:
	dotdotdot(ki->notes,MAXBLOBSHOW);       /* longblob */
	printf("<td>%s</td>\n", ki->notes     );
	break;

    case e_pushState:
	printf("<td>%s</td>\n", ki->pushState);
	break;

    case e_initdate:
	printf("<td>%s</td>\n", ki->initdate  );
	break;

    case e_lastdate:
	printf("<td>%s</td>\n", ki->lastdate  );
	break;

    case e_bounces:
	printf("<td>%u</td>\n", ki->bounces   );
	break;

    case e_lockUser:
	printf("<td>%s</td>\n", ki->lockUser  );
	break;

    case e_lockDateTime:
	printf("<td>%s</td>\n", ki->lockDateTime );
	break;

    case e_releaseLog:
	dotdotdot(ki->releaseLog,MAXBLOBSHOW);  /* chr(255) */
	printf("<td>%s</td>\n", ki->releaseLog   );
	break;

    case e_releaseLogUrl:
	dotdotdot(ki->releaseLogUrl,MAXBLOBSHOW);  /* chr(255) */
	printf("<td>%s</td>\n", ki->releaseLogUrl  );
	break;

    case e_importance:
        temp = "";
        if (sameString(ki->importance, "") || sameString(ki->importance, " "))
	    temp = "Unprioritized";
        else if (sameString(ki->importance, "B"))
	    temp = "Background";
        else if (sameString(ki->importance, "L"))
	    temp = "Low";
        else if (sameString(ki->importance, "M"))
	    temp = "Medium";
        else if (sameString(ki->importance, "H"))
	    temp = "High";
        else if (sameString(ki->importance, "U"))
	    temp = "Urgent";
	printf("<td>%s</td>\n", temp );
	break;

    default:
	errAbort("drawDisplayLine: unexpected case enum %d.",col);


    }

}



void doDisplay()
/* handle display request, shows pushQ records, also this is the default action  */
{
struct pushQ *ki, *kiList = NULL;
struct sqlResult *sr;
char **row;
char query[256];
char lastP = ' ';
int c = 0;
char monthsql[256];
char comment[256];

/* initialize column display order */
initColsFromString();

safef(monthsql,sizeof(monthsql),"%s","");
if (!sameString(month,""))
    {
    safef(monthsql,sizeof(monthsql)," where priority='L' and qadate like '%s%%' ",month);
    }

/* Get a list of all (or in month). */
safef(query, sizeof(query), "select * from %s%s%s",
    pushQtbl,
    monthsql,
    " order by priority, rank, qadate desc, qid desc limit 200"
    );

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ki = pushQLoad(row);
    slAddHead(&kiList, ki);
    }
sqlFreeResult(&sr);
slReverse(&kiList);

/* #rows returned
slCount(kiList)
*/

if (sameString(utsName.nodename,"hgwdev"))
    {
    printf("<p style=\"color:red\">Machine: %s THIS IS NOT THE REAL PUSHQ- GO TO <a href=http://hgwbeta.cse.ucsc.edu/cgi-bin/qaPushQ>HGWBETA</a> </p>\n",utsName.nodename);
    }

if (!sameString(msg,""))
    {
    printf("<p style=\"color:red\">%s</p>\n",msg);
    }

if (sameString(month,""))
    {
    printf("&nbsp;<A href=/cgi-bin/qaPushQ?action=add&cb=%s>ADD</A>\n",newRandState);
    }
else
    {
    printf("&nbsp;<A href=/cgi-bin/qaPushQ?action=display&month=current&cb=%s>Current</A>\n",newRandState);
    }
printf("&nbsp;<A href=/cgi-bin/qaPushQ?action=reset&cb=%s>Logout</A>\n",newRandState);
printf("&nbsp;<A href=/cgi-bin/qaPushQ?action=showAllCol&cb=%s>All Columns</A>\n",newRandState);
printf("&nbsp;<A href=/cgi-bin/qaPushQ?action=showDefaultCol&cb=%s>Default Columns</A>\n",newRandState);
printf("&nbsp;<A href=/cgi-bin/qaPushQ?action=showMonths&cb=%s>Log by Month</A>\n",newRandState);
printf("&nbsp;<A href=/cgi-bin/qaPushQ?action=showGateway&cb=%s>Gateway</A>\n",newRandState);
printf("&nbsp;<A href=/cgi-bin/qaPushQ?action=showDisplayHelp target=\"_blank\">Help</A>\n");
printf("&nbsp;<A href=/cgi-bin/qaPushQ?action=releaseLog target=\"_blank\">Release Log</A>\n");
//printf("&nbsp;<A href=/cgi-bin/qaPushQ?action=releaseLogPush target=\"_blank\">Publish RL</A>\n");
printf("&nbsp;<A href=/cgi-bin/qaPushQ?cb=%s#priorityA>A</A>\n",newRandState);
printf("&nbsp;<A href=/cgi-bin/qaPushQ?cb=%s#priorityB>B</A>\n",newRandState);
printf("&nbsp;<A href=/cgi-bin/qaPushQ?cb=%s#priorityC>C</A>\n",newRandState);
printf("&nbsp;<A href=/cgi-bin/qaPushQ?cb=%s#priorityD>D</A>\n",newRandState);
printf("&nbsp;<A href=/cgi-bin/qaPushQ?cb=%s#priorityL>L</A>\n",newRandState);
printf("&nbsp;<A href=/cgi-bin/qaPushQ?cb=%s>Refresh</A>\n",newRandState);
//printf("&nbsp;newRandState=%s\n",newRandState);
//printf("&nbsp;oldRandState=%s\n",oldRandState);

/* draw table header */

printf("<H2>Track Push Queue");
if (!sameString(pushQtbl,"pushQ"))
    {
    printf(" for %s",pushQtbl);
    }
if (!sameString(month,""))
    {
    printf(" (%s)",month);
    }
printf("</H2>\n");

printf("<TABLE BORDER CELLSPACING=0 CELLPADDING=5>\n");
printf("  <TR>\n");

for (c=0; c<numColumns; c++)
    {
    printf("    <TH>"
	"<a href=qaPushQ?action=promoteColumn&col=%s&cb=%s>&lt;</a>&nbsp;"
	"<a href=qaPushQ?action=hideColumn&col=%s&cb=%s>!</a>&nbsp;"
	"<a href=qaPushQ?action=demoteColumn&col=%s&cb=%s>&gt;</a>"
	"<br></TH>\n",
	colName[colOrder[c]], newRandState,
	colName[colOrder[c]], newRandState,
	colName[colOrder[c]], newRandState
	);
    }
printf("  <TR>\n");
printf("  </TR>\n");

for (c=0; c<numColumns; c++)
    {
    printf("    <TH>%s</TH>\n",colHdr[colOrder[c]]);
    }

printf("  </TR>\n");


/* Print some info for each match */
for (ki = kiList; ki != NULL; ki = ki->next)
    {

    /* Major-priority section header */
    if (ki->priority[0] != lastP)
	{
	lastP = ki->priority[0];
	safef(comment,sizeof(comment),"%s","");
	switch (ki->priority[0])
	    {
	    case 'A':
		safef(comment,sizeof(comment),"%s","active");
		break;
	    case 'B':
		safef(comment,sizeof(comment),"%s","short hold");
		break;
	    case 'C':
		safef(comment,sizeof(comment),"%s","long hold");
		break;
	    case 'L':
		safef(comment,sizeof(comment),"%s","log");
		break;

	    }
	printf("<tr>");
	printf("<td><h1><A name=\"priority%s\">%s</A></h1></td><td><b>%s</b></td>\n", ki->priority, ki->priority, comment);
	printf("</tr>");

    }

    /* Regular row */
    printf("<tr>");

    for (c=0; c<numColumns; c++)
	{
	drawDisplayLine(colOrder[c],ki);
	}

    printf("</tr>");
    }


/* draw table footer */
printf("</table>");

pushQFreeList(&kiList);

}


struct pushQ *loadPushQ(char *qid)
/* Return pushQ struct loading q with existing values.
 * Use pushQFree() when done.*/
{
char **row;
struct sqlResult *sr;
char query[256];
struct pushQ *q = NULL;
safef(query, sizeof(query), "select * from %s where qid = '%s'",pushQtbl,qid);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row)
    q = pushQLoad(row);
sqlFreeResult(&sr);
return q;
}

struct pushQ *mustLoadPushQ(char *qid)
/* Load pushQ or die */
{
struct pushQ *q = loadPushQ(qid);
if (!q)
    errAbort("loadPushQ: Queue Id %s not found.",qid);
return q;
}



void doPushDone()
/* Mark record pushState=D, move priority to L for Log, and set rank=0  */
{

struct pushQ *q;
char query[256];

q=mustLoadPushQ(cgiString("qid"));
if (sameString(q->lockUser,"") && sameString(q->pushState,"Y"))
    { /* not already locked and pushState=Y */

    safef(q->lastdate, sizeof(q->lastdate), q->qadate);
    strftime (q->qadate  , sizeof(q->qadate  ), "%Y-%m-%d", loctime); /* today's date */

    safef(query, sizeof(query),
	"update %s set rank = 0, priority ='L', pushState='D', qadate='%s', lastdate='%s' where qid = '%s' ",
	pushQtbl, q->qadate, q->lastdate, q->qid);
    sqlUpdate(conn, query);


    /* first close the hole where it was */
    safef(query, sizeof(query),
	"update %s set rank = rank - 1 where priority ='%s' and rank > %d ",
	pushQtbl, q->priority, q->rank);
    sqlUpdate(conn, query);
    }
else
    {
    if (sameString(q->lockUser,""))
	{
    	safef(msg, sizeof(msg), "Unable to mark record %s done-> Record is locked by %s->",q->qid,q->lockUser);
	}
    else
	{
    	safef(msg, sizeof(msg), "Invalid operation for qid %s, pushState is not Y, = %s->",q->qid,q->pushState);
	}
    }

pushQFree(&q);

doDisplay();
}


void XdoPromote(int change)
/* Promote the ranking of this Q item
 * >0 means promote, <0 means demote */
{

struct pushQ *q;
char query[256];
char newQid[sizeof(q->qid)] = "";

safef(newQid, sizeof(newQid), cgiString("qid"));

q = mustLoadPushQ(newQid);

if ((q->rank > 1) && (change>0))
    {
    /* swap places with rank-1 */
    safef(query, sizeof(query),
    "update %s set rank = rank + 1 where priority ='%s' and rank = %d ",
    pushQtbl, q->priority, q->rank-1);
    sqlUpdate(conn, query);
    q->rank--;
    safef(query, sizeof(query),
    "update %s set rank = %d where qid ='%s'",
    pushQtbl, q->rank, q->qid);
    sqlUpdate(conn, query);
    }

if (change<0)
    {
    /* swap places with rank+1 */
    safef(query, sizeof(query),
    "update %s set rank = rank - 1 where priority ='%s' and rank = %d ",
    pushQtbl, q->priority, q->rank+1);
    if (sqlUpdateRows(conn, query, NULL)>0)
	{
	q->rank++;
	safef(query, sizeof(query),
	    "update %s set rank = %d where qid ='%s'",
	    pushQtbl, q->rank, q->qid);
	sqlUpdate(conn, query);
	}
    }

pushQFree(&q);

doDisplay();
}


void doPromote()
{
XdoPromote(1);
}

void doDemote()
{
XdoPromote(-1);
}




int getNextAvailQid()
/* adding new pushQ rec, get next available qid number */
{
struct pushQ q;
int newqid = 0;
char query[256];
char *quickres = NULL;
safef(query, sizeof(query), "select max(qid) from %s",pushQtbl);
quickres = sqlQuickString(conn, query);
if (quickres != NULL)
    {
    safef(q.qid, sizeof(q.qid), quickres);
    sscanf(q.qid,"%d",&newqid);
    freez(&quickres);
    }
newqid++;
return newqid;
}

int getNextAvailRank(char *priority)
/* get next available rank at end of priority section */
{
struct pushQ q;
char query[256];
char *quickres = NULL;
safef(query, sizeof(query),
    "select rank from %s where priority='%s' order by rank desc limit 1",
    pushQtbl, priority);
quickres = sqlQuickString(conn, query);
if (quickres == NULL)
    {
    q.rank = 0;
    }
else
    {
    sscanf(quickres,"%d",&q.rank);
    freez(&quickres);
    }
q.rank++;
return q.rank;
}


void doTop()
{

struct pushQ *q;
char query[256];
char newQid[sizeof(q->qid)] = "";

safef(newQid, sizeof(newQid), cgiString("qid"));

q = mustLoadPushQ(newQid);

/* first close the hole where it was */
safef(query, sizeof(query),
"update %s set rank = rank + 1 where priority ='%s' and rank < %d ",
pushQtbl, q->priority, q->rank);
sqlUpdate(conn, query);

q->rank = 1;
safef(query, sizeof(query),
"update %s set rank = %d where qid = '%s' ",
pushQtbl, q->rank, q->qid);
sqlUpdate(conn, query);

pushQFree(&q);

doDisplay();
}

void doBottom()
{

struct pushQ *q;
char query[256];
char newQid[sizeof(q->qid)] = "";

safef(newQid, sizeof(newQid), cgiString("qid"));

q = mustLoadPushQ(newQid);

/* first close the hole where it was */
safef(query, sizeof(query),
"update %s set rank = rank - 1 where priority ='%s' and rank > %d ",
pushQtbl, q->priority, q->rank);
sqlUpdate(conn, query);

q->rank = getNextAvailRank(q->priority);
safef(query, sizeof(query),
"update %s set rank = %d where qid = '%s' ",
pushQtbl, q->rank, q->qid);
sqlUpdate(conn, query);

pushQFree(&q);

doDisplay();
}


/* too bad this isn't part of autoSql's code generation */

void pushQUpdateEscaped(struct sqlConnection *conn, struct pushQ *el, char *tableName, int updateSize)
/* Update pushQ row to the table specified by tableName.
 * As blob fields may be arbitrary size updateSize specifies the approx size.
 * of a string that would contain the entire query. Automatically
 * escapes all simple strings (not arrays of string) but may be slower than pushQSaveToDb().
 * For example automatically copies and converts:
 * "autosql's features include" --> "autosql\'s features include"
 * before inserting into database. */
{
struct dyString *update = newDyString(updateSize);
char  *qid, *pqid, *priority, *qadate, *newYN, *track, *dbs, *tbls, *cgis, *files, *currLoc, *makeDocYN, *onlineHelp, *ndxYN, *joinerYN, *stat, *featureBits, *sponsor, *reviewer, *extSource, *openIssues, *notes, *pushState, *initdate, *lastdate, *lockUser, *lockDateTime, *releaseLog, *releaseLogUrl, *importance;
qid = sqlEscapeString(el->qid);
pqid = sqlEscapeString(el->pqid);
priority = sqlEscapeString(el->priority);
qadate = sqlEscapeString(el->qadate);
newYN = sqlEscapeString(el->newYN);
track = sqlEscapeString(el->track);
dbs = sqlEscapeString(el->dbs);
tbls = sqlEscapeString(el->tbls);
cgis = sqlEscapeString(el->cgis);
files = sqlEscapeString(el->files);
currLoc = sqlEscapeString(el->currLoc);
makeDocYN = sqlEscapeString(el->makeDocYN);
onlineHelp = sqlEscapeString(el->onlineHelp);
ndxYN = sqlEscapeString(el->ndxYN);
joinerYN = sqlEscapeString(el->joinerYN);
stat = sqlEscapeString(el->stat);
featureBits = sqlEscapeString(el->featureBits);
sponsor = sqlEscapeString(el->sponsor);
reviewer = sqlEscapeString(el->reviewer);
extSource = sqlEscapeString(el->extSource);
openIssues = sqlEscapeString(el->openIssues);
notes = sqlEscapeString(el->notes);
pushState = sqlEscapeString(el->pushState);
initdate = sqlEscapeString(el->initdate);
lastdate = sqlEscapeString(el->lastdate);
lockUser = sqlEscapeString(el->lockUser);
lockDateTime = sqlEscapeString(el->lockDateTime);
releaseLog = sqlEscapeString(el->releaseLog);
releaseLogUrl = sqlEscapeString(el->releaseLogUrl);
importance = sqlEscapeString(el->importance);

/* had to split this up because dyStringPrintf only up to 4000 chars at one time */
dyStringPrintf(update,
    "update %s set "
    "pqid='%s',priority='%s',rank=%u,qadate='%s',newYN='%s',track='%s',",
    tableName,  pqid,  priority, el->rank,  qadate, newYN, track);
dyStringPrintf(update, "dbs='%s',",dbs);
dyStringPrintf(update, "tbls='%s',",tbls);
dyStringPrintf(update, "cgis='%s',",cgis);
dyStringPrintf(update, "files='%s',",files);
dyStringPrintf(update, "sizeMB=%u,currLoc='%s',"
    "makeDocYN='%s',onlineHelp='%s',ndxYN='%s',joinerYN='%s',stat='%s',"
    "sponsor='%s',reviewer='%s',extSource='%s',",
    el->sizeMB ,  currLoc,  makeDocYN,
    onlineHelp,  ndxYN,  joinerYN,  stat,
    sponsor,  reviewer,  extSource);
dyStringPrintf(update, "openIssues='%s',",openIssues);
dyStringPrintf(update, "notes='%s',",notes);
dyStringPrintf(update, "pushState='%s', initdate='%s', lastdate='%s', bounces='%u',lockUser='%s',lockDateTime='%s',releaseLog='%s',featureBits='%s',releaseLogUrl='%s',importance='%s' "
	"where qid='%s'",
	pushState, initdate, lastdate, el->bounces, lockUser, lockDateTime, releaseLog, featureBits, releaseLogUrl, importance,
	qid
	);

sqlUpdate(conn, update->string);
freeDyString(&update);
freez(&qid);
freez(&pqid);
freez(&priority);
freez(&qadate);
freez(&newYN);
freez(&track);
freez(&dbs);
freez(&tbls);
freez(&cgis);
freez(&files);
freez(&currLoc);
freez(&makeDocYN);
freez(&onlineHelp);
freez(&ndxYN);
freez(&joinerYN);
freez(&stat);
freez(&sponsor);
freez(&reviewer);
freez(&extSource);
freez(&openIssues);
freez(&notes);
freez(&pushState);
freez(&initdate);
freez(&lastdate);
freez(&lockUser);
freez(&lockDateTime);
freez(&releaseLog);
freez(&releaseLogUrl);
freez(&importance);
}

void getCgiData(bool *isOK, bool isPtr, void *ptr, int size, char *name)
/* get data, truncate to fit in field to prevent safef buf overflows */
{
int l = 0;
char **pfld = NULL;
char *fld = NULL;
char *cgi = NULL;
cgi = cgiString(name);
l = strlen(cgi);
if (isPtr)
    {
    pfld = (char **) ptr;
    }
else
    {
    fld = (char *) ptr;
    }
if (size != -1)  /* -1 for blob, has no length */
    {
    if (l>(size-1))
	{
	*isOK = FALSE;
	safef(msg,sizeof(msg),"%s: too large, max. %d chars.",name,size-1);
	}
    }
if (isPtr)
    {
    *pfld = cloneString(cgi);  /* set pointer to a copy of the whole thing */
    }
else
    {
    safef(fld, size, cloneStringZ(cgi,size-1));  /* for non-ptr strings, copy into existing buffer */
    }
}



void doTransfer();    /* forward reference needed */

void doShowSizes();  /* forward reference needed */

void doEdit();       /* forward reference needed */

void doPost()
/* handle the  post (really just a get for now) from Add or Edit of a pushQ record */
{


int updateSize = 2456;
int newqid = 0;

char query[256];
char *submitbutton = cgiUsualString("submit"   ,"");
char *delbutton    = cgiUsualString("delbutton"   ,"");
char *pushbutton   = cgiUsualString("pushbutton"  ,"");
char *clonebutton  = cgiUsualString("clonebutton" ,"");
char *bouncebutton = cgiUsualString("bouncebutton","");
char *lockbutton   = cgiUsualString("lockbutton"  ,"");
char *cancelbutton   = cgiUsualString("cancelbutton"  ,"");
char *showSizes    = cgiUsualString("showSizes"   ,"");
char *transfer     = cgiUsualString("transfer"   ,"");

struct pushQ *q;

bool isNew  = FALSE;   /* new rec */
bool isRedo = FALSE;   /* need to return to edit form with error msg */
bool isOK   = TRUE;    /* is data valid length (not too large) */
bool lockOK = TRUE;    /* assume for now lock state OK */

char newQid     [sizeof(q->qid)]      = "";
char newPriority[sizeof(q->priority)] = "";

safef(newQid, sizeof(newQid), cgiString("qid"));

if (sameString(newQid,""))
    {
    isNew = TRUE;
    }
else
    {
    isNew = FALSE;
    }

if (!isNew)
    {
    /* we need to preload q with existing values
     * because some fields like rank are not carried in form
    */
    q = mustLoadPushQ(newQid);
	/* true means optional, it was asked if we could tolerate this,
	 *  e.g. delete, then hit back-button
	* user is trying to use back button to recover deleted rec
	safef(newQid, sizeof(newQid), "");
	isNew = TRUE;
	*/

    /* check lock status */

    if (sameString(cancelbutton,"Cancel"))  /* user cancelled */
	{  /* unlock record */
	safef(q->lockUser, sizeof(q->lockUser), "%s", "");
	safef(q->lockDateTime, sizeof(q->lockDateTime), "%s", "");
	pushQUpdateEscaped(conn, q, pushQtbl, updateSize);
	lockOK = FALSE;
	}
    else if (sameString(lockbutton,"Lock"))  /* try to lock the record for editing */
	{
	if (sameString(q->lockUser,""))  /* q->lockUser blank if nobody has lock */
	    {
	    safef(q->lockUser, sizeof(q->lockUser), qaUser);
	    strftime(q->lockDateTime, sizeof(q->lockDateTime), "%Y-%m-%d %H:%M", loctime);
	    pushQUpdateEscaped(conn, q, pushQtbl, updateSize);
	    lockOK = FALSE;
	    }
	else
	    { /* somebody else has lock-> */
	    lockOK = FALSE;
	    }
	}
    else if (!sameString(q->lockUser,qaUser))  /* User supposed to already have lock, verify. */
	{ /* if lock was lost, what do we do now? */
	if (sameString(q->lockUser,""))
	    {
	    safef(msg,sizeof(msg),"Lost lock-> Must refresh data->");
	    }
	else
	    {
	    safef(msg,sizeof(msg),"Lost lock-> User %s currently has lock on Queue Id %s since %s->",
		q->lockUser,q->qid,q->lockDateTime);
	    }
	lockOK = FALSE;
	}

    if (!lockOK)
	{
	doEdit();
	pushQFree(&q);
	return;
	}

    }

if (isNew)
    {
    AllocVar(q);
    newqid = getNextAvailQid();
    safef(q->pqid, sizeof(q->pqid), "%s", "");
    safef(q->pushState,sizeof(q->pushState),"N");  /* default to: push not done yet */
    }


safef(newPriority, sizeof(newPriority), cgiString("priority"));


/* dates */
getCgiData(&isOK, FALSE, q->qadate    , sizeof(q->qadate    ), "qadate"    );
getCgiData(&isOK, FALSE, q->initdate  , sizeof(q->initdate  ), "initdate"  );

/* YN select listboxes */
getCgiData(&isOK, FALSE, q->newYN     , sizeof(q->newYN     ), "newYN"     );
getCgiData(&isOK, FALSE, q->makeDocYN , sizeof(q->makeDocYN ), "makeDocYN" );
getCgiData(&isOK, FALSE, q->ndxYN     , sizeof(q->ndxYN     ), "ndxYN"     );
getCgiData(&isOK, FALSE, q->joinerYN  , sizeof(q->joinerYN  ), "joinerYN"  );
getCgiData(&isOK, FALSE, q->importance, sizeof(q->importance), "importance"  );

/* chr(255) strings */
getCgiData(&isOK, TRUE ,&q->track     , 256                 , "track"     );
getCgiData(&isOK, TRUE ,&q->dbs       , 256                 , "dbs"       );
getCgiData(&isOK, TRUE ,&q->cgis      , 256                 , "cgis"      );
getCgiData(&isOK, TRUE ,&q->stat      , 256                 , "stat"      );

/* integers */
if (sscanf(cgiString("sizeMB"),"%u",&q->sizeMB) != 1)
    {
    q->sizeMB = 0;
    }

/* strings of various sizes */
getCgiData(&isOK, FALSE, q->currLoc   , sizeof(q->currLoc   ), "currLoc"   );
getCgiData(&isOK, FALSE, q->onlineHelp, sizeof(q->onlineHelp), "onlineHelp");
getCgiData(&isOK, FALSE, q->sponsor   , sizeof(q->sponsor   ), "sponsor"   );
getCgiData(&isOK, FALSE, q->reviewer  , sizeof(q->reviewer  ), "reviewer"  );
getCgiData(&isOK, FALSE, q->extSource , sizeof(q->extSource ), "extSource" );

/* blobs */
getCgiData(&isOK, TRUE ,&q->tbls      , -1                  , "tbls"      );
getCgiData(&isOK, TRUE ,&q->files     , -1                  , "files"     );
getCgiData(&isOK, TRUE ,&q->featureBits, -1                 , "featureBits");
getCgiData(&isOK, TRUE ,&q->openIssues, -1                  , "openIssues");
getCgiData(&isOK, TRUE ,&q->notes     , -1                  , "notes"     );
getCgiData(&isOK, TRUE ,&q->releaseLog, -1                  , "releaseLog");
getCgiData(&isOK, TRUE ,&q->releaseLogUrl, -1               , "releaseLogUrl");


/* check for things too big  */
if (!isOK)
    {
    isRedo = TRUE;
    }
if ((q->sizeMB < 0) || (q->sizeMB > 100000000))
    {
    safef(msg,sizeof(msg),"Size(MB): invalid size. <br>\n");
    isRedo = TRUE;
    }
if (!isDateValid(cgiString("qadate")))
    {
    safef(msg,sizeof(msg),"Date format invalid, should be YYYY-MM-DD. <br>\n");
    isRedo = TRUE;
    }
if (strlen(cgiString("track"))>255)
    {
    safef(msg,sizeof(msg),"Track: too long for field, 255 char max. <br>\n");
    isRedo = TRUE;
    }
if (strlen(cgiString("dbs"))>255)
    {
    safef(msg,sizeof(msg),"Database: too long for field, 255 char max. <br>\n");
    isRedo = TRUE;
    }
if (strlen(cgiString("cgis"))>255)
    {
    safef(msg,sizeof(msg),"CGIs: too long for field, 255 char max. <br>\n");
    isRedo = TRUE;
    }
if (strlen(cgiString("stat"))>255)
    {
    safef(msg,sizeof(msg),"Status: too long for field, 255 char max. <br>\n");
    isRedo = TRUE;
    }


/* need to do this before delete or will lose the record */
if ((sameString(pushbutton,"push requested"))&&(q->sizeMB==0))
    {
    safef(msg,sizeof(msg),"Size (MB) should not be zero. <br>\n");
    isRedo = TRUE;
    }

if ((sameString(pushbutton,"push requested"))&&(!sameString(q->currLoc,"hgwbeta")))
    {
    safef(msg,sizeof(msg),"Current Location should be hgwbeta. <br>\n");
    isRedo = TRUE;
    }

if ((sameString(pushbutton,"push requested"))&&(sameString(q->makeDocYN,"N")))
    {
    safef(msg,sizeof(msg),"MakeDoc not verified. <br>\n");
    isRedo = TRUE;
    }

if ((sameString(pushbutton,"push requested"))&&(sameString(q->ndxYN,"N")))
    {
    safef(msg,sizeof(msg),"Index not verified. <br>\n");
    isRedo = TRUE;
    }

if ((sameString(pushbutton,"push requested"))&&(sameString(q->joinerYN,"N")))
    {
    safef(msg,sizeof(msg),"All->Joiner not verified. <br>\n");
    isRedo = TRUE;
    }

if ((sameString(bouncebutton,"bounce"))&&(!sameString(q->priority,"A")))
    {
    safef(msg,sizeof(msg),"Only priority A records should be bounced. <br>\n");
    isRedo = TRUE;
    }
if ((sameString(bouncebutton,"unbounce"))&&(sameString(q->priority,"A")))
    {
    safef(msg,sizeof(msg),"Priority A records should not be unbounced. <br>\n");
    isRedo = TRUE;
    }


if (isRedo)
    {
    replacePushQFields(q, isNew);
    pushQFree(&q);
    return;
    }


if (sameString(bouncebutton,"bounce"))
    {
    safef(newPriority, sizeof(newPriority), "B");
    safef(q->lastdate, sizeof(q->lastdate), q->qadate);
    strftime (q->qadate, sizeof(q->qadate), "%Y-%m-%d", loctime); /* set to today's date */
    q->bounces++;
    }
if (sameString(bouncebutton,"unbounce"))
    {
    safef(newPriority, sizeof(newPriority), "A");
    safef(q->lastdate, sizeof(q->lastdate), q->qadate);
    strftime (q->qadate, sizeof(q->qadate), "%Y-%m-%d", loctime); /* set to today's date */
    }


/* check if priority class has changed, or deleted, then close ranks */
if ( (!sameString(newPriority,q->priority)) || (sameString(delbutton,"delete")) )
    {
    /* first close the hole where it was */
    safef(query, sizeof(query),
    "update %s set rank = rank - 1 where priority ='%s' and rank > %d ",
    pushQtbl, q->priority, q->rank);
    sqlUpdate(conn, query);
    }

/* if not deleted, then if new or priority class change, then take last rank */
if (!sameString(delbutton,"delete"))
    {
    if ((!sameString(newPriority,q->priority)) || isNew)
	{
	q->rank = getNextAvailRank(newPriority);
	safef(q->priority, sizeof(q->priority), newPriority);
	}
    }

if (q->priority[0]=='L')
    {
    q->rank = 0;
    }


if (sameString(pushbutton,"push requested"))
    {
    /* reset pushState in case was prev-> a log already */
    safef(q->pushState,sizeof(q->pushState),"Y");
    }

if (sameString(delbutton,"delete"))
    {
    /* delete old record */
    safef(query, sizeof(query), "delete from %s where qid ='%s'", pushQtbl, q->qid);
    sqlUpdate(conn, query);
    }
else
    {
    if (sameString(showSizes,"Show Sizes") || sameString(transfer,"Transfer"))
	{ /* mark record as locked */
	safef(q->lockUser, sizeof(q->lockUser), qaUser);
	strftime(q->lockDateTime, sizeof(q->lockDateTime), "%Y-%m-%d %H:%M", loctime);
	}
    else
	{ /* unlock record */
	safef(q->lockUser, sizeof(q->lockUser), "%s", "");
	safef(q->lockDateTime, sizeof(q->lockDateTime), "%s", "");
	}
    if (isNew)
	{
	/* save new record */
	safef(msg, sizeof(msg), "%%0%dd", (int)sizeof(q->qid)-1);
	safef(newQid,sizeof(newQid),msg,newqid);
	safef(q->qid, sizeof(q->qid), newQid);
    	safef(msg, sizeof(msg), "%s", "");
	pushQSaveToDbEscaped(conn, q, pushQtbl, updateSize);
	}
    else
	{
	/* update existing record */
	pushQUpdateEscaped(conn, q, pushQtbl, updateSize);
	}
    }

if (sameString(clonebutton,"clone"))
    {
    /* save new clone */
    safef(q->pqid,sizeof(q->pqid), q->qid);  /* daughter will point to parent */
    newqid = getNextAvailQid();
    safef(msg, sizeof(msg), "%%0%dd", (int)sizeof(q->qid)-1);
    safef(newQid,sizeof(newQid),msg,newqid);
    safef(q->qid, sizeof(q->qid), newQid);
    safef(msg, sizeof(msg), "%s", "");
    if (q->priority[0]=='L')
	{
	q->rank = 0;
	}
    else
	{
	q->rank = getNextAvailRank(q->priority);
	}
    safef(q->pushState,sizeof(q->pushState),"N");  /* default to: push not done yet */
    pushQSaveToDbEscaped(conn, q, pushQtbl, updateSize);
    }


if (sameString(showSizes,"Show Sizes"))
    {
    cgiVarSet("qid", q->qid); /* for new rec */
    doShowSizes();
    }

else if (sameString(transfer,"Transfer"))
    {
    cgiVarSet("qid", q->qid); /* for new rec */
    doTransfer();
    }

else if (sameString(submitbutton,"Submit"))
    { /* if submit button, saved data, now return to readonly view->  */

    safef(msg, sizeof(msg), "Data saved->");

    cgiVarSet("qid", q->qid); /* for new rec */
    doEdit();
    }
else
    {
    doDisplay();
    }

pushQFree(&q);


}



void doEdit()
/* Handle edit request for a pushQ entry */
{
int updateSize = 2456;
struct pushQ *q;
q = loadPushQ(cgiString("qid"));
if (!q)
    {
    printf("Queue Id %s not found.", cgiString("qid"));
    return;
    }

if ( sameString(qaUser,"kuhn") ||
     sameString(qaUser,"kuhn2") ||
     sameString(qaUser,"mary") ||
     sameString(qaUser,"ann") ||
     sameString(qaUser,"antonio")
     )  /* for users that want to automatically try to lock record immediately */
    {
    if (sameString(action,"edit") || sameString(action,"setSize"))
	{
	if (sameString(q->lockUser,""))  /* q->lockUser blank if nobody has lock */
	    {
	    safef(q->lockUser, sizeof(q->lockUser), qaUser);
	    strftime(q->lockDateTime, sizeof(q->lockDateTime), "%Y-%m-%d %H:%M", loctime);
	    pushQUpdateEscaped(conn, q, pushQtbl, updateSize);
	    }
	}
    else /* we are coming back from a post? so return to display automatically */
	{
	doDisplay();
	return;  /* this is needed? */
	}
    }

replacePushQFields(q, FALSE);  /* new rec = false */
pushQFree(&q);
}

void doSetSize()
/* save sizeMB */
{
struct pushQ *q;
char tempSizeMB[10];
int updateSize=2456;
q = loadPushQ(cgiString("qid"));
if (!q)
    {
    printf("Queue Id %s not found.", cgiString("qid"));
    return;
    }
safef(tempSizeMB,sizeof(tempSizeMB), cgiUsualString("sizeMB",""));
if (!sameString(tempSizeMB,""))
    {
    if (sscanf(tempSizeMB,"%u",&q->sizeMB) != 1)
	{
	q->sizeMB = 0;
	}
    }
pushQUpdateEscaped(conn, q, pushQtbl, updateSize);
doEdit();
pushQFree(&q);
}


void doLogin()
/* make form for login */
{
printf("<h4>Login</h4>\n");

printf("<FORM ACTION=\"/cgi-bin/qaPushQ\" NAME=\"loginForm\" METHOD=\"POST\">\n");

printf("<input TYPE=\"hidden\" NAME=\"action\" VALUE=\"postLogin\"  >\n");

printf("<TABLE cellpadding=6>\n");

printf("<TR>\n");
printf("<TD align=right>\n");
printf("User:\n");
printf("</TD>\n");
printf("<TD>\n");
printf("<INPUT TYPE=text NAME=user size = 8 value=\"\" >\n");
printf("</TD>\n");
printf("</TR>\n");

printf("<TR>\n");
printf("<TD align=right>\n");
printf("Password:\n");
printf("</TD>\n");
printf("<TD>\n");
printf("<INPUT TYPE=password NAME=password size=8 value=\"\" > <br>\n");
printf("</TD>\n");
printf("</TR>\n");

printf("<TR>\n");
printf("<TD align=right>\n");
printf("&nbsp;\n");
printf("</TD>\n");
printf("<TD>\n");
printf("<input TYPE=SUBMIT NAME=\"submit\" VALUE=\"Log In\">\n");
printf("</TD>\n");
printf("</TR>\n");

printf("</TABLE>\n");

if (sameString(utsName.nodename,"hgwdev"))
    {
    printf("<br><p style=\"color:red\">Machine: %s THIS IS NOT THE REAL PUSHQ- GO TO "
           "<a href=http://hgwbeta.cse.ucsc.edu/cgi-bin/qaPushQ>HGWBETA</a> </p>\n",utsName.nodename);
    }

printf("</FORM>\n");

printf("<br>\n");
printf("%s<br>\n",msg);
}

void doLogoutMsg()
/* let user know logged-out ok */
{
printf("<h4>Logged Out</h4>\n");
printf("<FORM ACTION=\"/cgi-bin/qaPushQ\" NAME=\"logoutForm\" METHOD=\"POST\">\n");
printf("<input TYPE=\"hidden\" NAME=\"action\" VALUE=\"login\"  >\n");
printf("<TABLE cellpadding=6>\n");
printf("<TR>\n");
printf("<TD align=right>\n");
printf("<input TYPE=SUBMIT NAME=\"submit\" VALUE=\"Re-Login\">\n");
printf("</TD>\n");
printf("</TR>\n");
printf("</TABLE>\n");
printf("</FORM>\n");
}


void doCookieReset()
/* reset cookie, will cause new login next time */
{
htmlSetCookie("qapushq", "", NULL, NULL, ".cse.ucsc.edu", FALSE);
htmShell(TITLE, doLogoutMsg, NULL);
}




bool readAUser(struct users *u, bool optional)
/* read data for my user */
{
char query[256];
char **row;
struct sqlResult *sr;

safef(query, sizeof(query), "select * from users where user = '%s'",u->user);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL)
    {
    if (optional)
	{
	sqlFreeResult(&sr);
	return FALSE;
	}
    else
	{
	errAbort("%s not found.",u->user);
	}
    }
else
    {
    usersStaticLoad(row,u);
    }
sqlFreeResult(&sr);
return TRUE;
}


void readMyUser()
/* read data for my user */
{
int i = 1;  /* because it should start right off with ? */
char tempVar[2048];
char tempVarName[256];
char tempVal[2048];

if ((qaUser == NULL) || (sameString(qaUser,"")))
    {
    return;
    }
safef(myUser.user,sizeof(myUser.user),qaUser);
readAUser(&myUser, FALSE);

while(parseList(myUser.contents,'?',i,tempVar,sizeof(tempVar)))
    {

    parseList(tempVar,'=',0,tempVarName,sizeof(tempVarName));

    parseList(tempVar,'=',1,tempVal,sizeof(tempVal));

    if (sameString(tempVarName,"showColumns"))
	{
	showColumns = cloneString(tempVal);
	}

    if (sameString(tempVarName,"org"))
	{
	safef(pushQtbl,sizeof(pushQtbl),tempVal);
	}

    if (sameString(tempVarName,"month"))
	{
	safef(month,sizeof(month),tempVal);
	}

    if (sameString(tempVarName,"oldRandState"))
	{
	oldRandState = cloneString(tempVal);
	}

    i++;
    }
}

void saveMyUser();  /* forward declaration */

void doPostLogin()
/* process Login post */
{

char *tbl = "users";
char query[256];

struct users u;

char *userPassword = NULL;
bool loginOK = FALSE;
char *meta = ""
"<head>"
"<title>"
"Meta Redirect Code"
"</title>"
"<meta http-equiv=\"refresh\" content=\"0;url=/cgi-bin/qaPushQ\">"
"</head>";

userPassword = cgiString("password");

ZeroVar(&u);

u.next = NULL;
safef(u.user,  sizeof(u.user), cgiString("user"));

conn = sqlConnectRemote(host, user, password, database);  /* do db conn here special for login */

if (!readAUser(&u, TRUE))
    {
    /* unknown user not allowed */
    safef(msg,sizeof(msg),"Invalid user or password.");
    }
else
    {
    if (strlen(u.password)==0)
	{ /* if pwd in db is blank, use this as their new password and encrypt it and save in db. */
	if (strlen(userPassword) < 6)
	    { /* bad pwd */
	    safef(msg,sizeof(msg),"Invalid password. Password must be at least 6 characters long.");
	    }
	else
	    {
	    encryptNewPWD(userPassword, u.password, sizeof(u.password));
	    safef(query, sizeof(query),
		"update %s set password = '%s' where user = '%s' ",
		tbl, u.password, u.user);
	    sqlUpdate(conn, query);
	    loginOK = TRUE;
	    }
	}
    else
	{ /* verify password matches db */
	if (checkPWD(userPassword, u.password))
	    { /* good pwd, save user in cookie */
	    loginOK = TRUE;
	    }
	else
	    { /* bad pwd */
	    safef(msg,sizeof(msg),"Invalid user or password.");
	    }
	}
    }

if (loginOK)
    {
    /* try to make same cookie work with both hgwdev and hgwbeta to obviate need for double-login */
    /* note: for permanent cookie, set NULL to expire in format "Wdy, DD-Mon-YYYY HH:MM:SS GMT" (must be GMT) */
    htmlSetCookie("qapushq", u.user, NULL, NULL, ".cse.ucsc.edu", FALSE);

    qaUser=u.user;
    oldRandState="";
    showColumns=cloneString(defaultColumns);
    readMyUser();
    oldRandState="";
    saveMyUser();
    safef(msg, sizeof(msg),"Login successful.");
    htmShellWithHead(TITLE, meta, doMsg ,NULL);  /* this is now the only redirect */
    }
else
    {
    htmShell(TITLE, doLogin, NULL);
    }

sqlDisconnect(&conn);
}



void saveMyUser()
/* read data for my user */
{
char *tbl = "users";
struct dyString * query = NULL;
query = newDyString(2048);
if ((qaUser == NULL) || (sameString(qaUser,"")))
    {
    return;
    }
dyStringPrintf(query,
    "update %s set contents = '?showColumns=%s?org=%s?month=%s?oldRandState=%s' where user = '%s'",
    tbl, showColumns, pushQtbl, month, oldRandState, myUser.user);
sqlUpdate(conn, query->string);
freeDyString(&query);
}




void XdoPromoteColumn(int change)
/* Promote the column
 * 1 = promote, 0 = hide, -1 = demote */
{
char target[256] = "";

int i = 0;
char tempBefore[256] = "";
char tempVal   [256] = "";
char tempAfter [256] = "";
char tempSwap  [256] = "";
struct dyString * s = NULL;
s = newDyString(2048);  /* need room */

safef(target, sizeof(target), cgiString("col"));

while(TRUE)
    {
    parseList(showColumns,',',i,tempAfter,sizeof(tempAfter));
    if ((tempBefore[0]==0) && (tempVal[0]==0) && (tempAfter[0]==0))
	{
	break;
	}
    if (sameString(target,tempVal))
	{

	if (change==1)
	    {
	    /*  swap places with Before */
	    safef(tempSwap  , sizeof(tempSwap  ), tempBefore);
	    safef(tempBefore, sizeof(tempBefore), tempVal   );
	    safef(tempVal   , sizeof(tempVal   ), tempSwap  );
	    }
	if (change==0)
	    {
	    /* remove */
	    tempVal[0]=0;  /* set to empty string, output will be skipped */
	    }
	if (change==-1)
	    {
	    /* swap places with After */
	    safef(tempSwap  , sizeof(tempSwap  ), tempAfter );
	    safef(tempAfter , sizeof(tempAfter ), tempVal   );
	    safef(tempVal   , sizeof(tempVal   ), tempSwap  );
	    }

	change = 99;  /* just suppress any more changes */

	}
    if (!sameString(tempBefore,""))
	{
	dyStringPrintf(s, "%s,", tempBefore);
	}
    /* roll 'em! */
    safef(tempBefore, sizeof(tempBefore), tempVal  );
    safef(tempVal   , sizeof(tempVal)   , tempAfter);
    i++;
    }

showColumns = cloneString(s->string);
freeDyString(&s);

showColumns[strlen(showColumns)-1]=0;  /* chop off trailing comma */

doDisplay();

}


void doPromoteColumn()
{
XdoPromoteColumn(1);
}

void doHideColumn()
{
XdoPromoteColumn(0);
}

void doDemoteColumn()
{
XdoPromoteColumn(-1);
}




void doShowAllColumns()
/* Display hidden columns available for resurrection */
{
int c = 0;
char templist[512];
char tempe[64];

printf("<h4>Show Hidden Columns</h4>\n");
printf("<br>\n");
printf("<a href=\"/cgi-bin/qaPushQ?cb=%s\">RETURN</a><br>", newRandState);
printf("<br>\n");
printf("Click on any column below to un-hide it.<br>\n");
printf("<br>\n");

safef(templist,sizeof(templist),",%s,",showColumns);  /* add sentinel comma values to the ends of the col list */

for (c=0; c<e_NUMCOLS; c++)
    {
    safef(tempe,sizeof(tempe),",%s,",colName[c]);  /* add sentinel comma values to the ends of the col element */
    if (strstr(templist,tempe)==NULL)
	{
	printf("<a href=\"/cgi-bin/qaPushQ?action=showColumn&colName=%s&cb=%s\">%s</a><br><br>",
	    colName[c], newRandState, colName[c]);
	}
    }

printf("<br>\n");
printf("<a href=\"/cgi-bin/qaPushQ?cb=%s\">RETURN</a><br>", newRandState);
}


void doShowColumn()
/* Make column visible again */
{
struct dyString * s = NULL;
char *colName = NULL;
char templist[512];
char tempe[64];

s = newDyString(2048);  /* need room */

colName = cgiString("colName");

mapFieldToEnum(colName,TRUE);  /* this will make sure it exists or errAbort */

safef(templist,sizeof(templist),",%s,",showColumns);  /* add sentinel comma values to the ends of the col list */
safef(tempe,sizeof(tempe),",%s,",colName);  /* add sentinel comma values to the ends of the col element */
if (strstr(templist,tempe)==NULL)  /* make sure not already in list */
    {
    dyStringAppend(s, showColumns);
    dyStringPrintf(s, ",%s", colName);
    showColumns = cloneString(s->string);
    }

freeDyString(&s);

doDisplay();
}


void doShowDefaultColumns()
/* Show Default Columns in default order */
{
showColumns = cloneString(defaultColumns);
doDisplay();
}



void doShowMonths()
/* This gives the user a choice of months to filter on */
{

struct sqlResult *sr;
char **row;
char query[256];

printf("<h4>Logs for Month</h4>\n");
printf("<br>\n");
printf("<A href=qaPushQ?action=display&month=current&cb=%s>Current</A><br>\n", newRandState);
printf("<br>\n");

safef(query, sizeof(query), "select distinct substring(qadate,1,7) from %s where priority='L' order by qadate desc",pushQtbl);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    printf("<A href=qaPushQ?action=display&month=%s&cb=%s>%s</A><br>\n",row[0],newRandState,row[0]);
    }
sqlFreeResult(&sr);
printf("<br>\n");
printf("<a href=\"/cgi-bin/qaPushQ?cb=%s\">RETURN</a><br>", newRandState);
}



void getIndexes(struct sqlConnection *conn, char *tbl, char *s, int ssize)
/* Get indexes with show index on table command. Return -1 if err. Will match multiple if "%" used in tbl */
{
char query[256];
char **row;
struct sqlResult *sr;
char *fld = NULL;
int f = 0, i = 0, n = 0, c = 0;
char lastKeyName[256]="";


safef(query, sizeof(query), "show index from %s",tbl);
sr = sqlGetResult(conn, query);
f = 0;
i = 0;
n = 0;
if (ssize > 0) s[0]=0;
while ((fld = sqlFieldName(sr)) != NULL)
    {
    if (sameString(fld,"Key_name"))
	{
	n = f;
	}
    if (sameString(fld,"Column_name"))
	{
	i = f;
	}
    f++;
    }
while ((row = sqlNextRow(sr)) != NULL)
    {
    c++;
    if (sameString(row[n],lastKeyName))
    	{
	strcat(s,"+");
	strcat(s,row[i]);
	}
    else
	{
	if (c > 1)
	    {
	    strcat(s,", ");
	    }
	strcat(s, row[i]);
	safef(lastKeyName,sizeof(lastKeyName),row[n]);
	}
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
}



void mySprintWithCommas(char *s, int slength, long long size)
/* the one in obscure.c was overflowing, so here's mine  */
{
char *temp=NULL;
char sep[2]="";
s[0]=0;
temp = needMem(slength);
while (size >= 1000)
    {
    safef(temp,slength,s);
    safef(s,slength,"%03d%s%s",(int)(size%1000),sep,temp);
    size/=1000;
    safef(sep,sizeof(sep),",");
    }
if (size > 0)
    {
    safef(temp,slength,s);
    safef(s,slength,"%3d%s%s",(int)size,sep,temp);
    }
else
    {
    safef(s,slength,"0");  /* special case zero*/
    }
freez(&temp);
}



long long pq_getTableSize(char *rhost, char *db, char *tbl, int *errCount)  /* added extension pq_ to supress name conflict in hdb.c */
/* Get table size via show table status command. Return -1 if err. Will match multiple if "%" used in tbl */
{
char query[256];
char **row;
struct sqlResult *sr;
char *fld = NULL;
int f = 0, d = 0, i = 0, n = 0, c = 0;
unsigned long size = 0;
long long totalsize = 0;
char nicenumber[256]="";
char  indexlist[256]="";

char *host     = NULL;
char *user     = NULL;
char *password = NULL;

struct sqlConnection *conn = NULL;

host     = cfgOption("db.host"    );
user     = cfgOption("db.user"    );
password = cfgOption("db.password");

if ((sameString(utsName.nodename,"hgwbeta")) && (sameString(rhost,"hgwdev")))
    {
    host     = "hgwdev";
    user     = cfgOption("db.user"    );
    password = cfgOption("db.password");
    }

if ((sameString(utsName.nodename,"hgwdev")) && (sameString(rhost,"hgwbeta")))
    {  // inaccurate but doesn't matter since we only use it to test qaPushQ cgi on dev.
    host     = cfgOption("central.host"    );
    user     = cfgOption("central.user"    );
    password = cfgOption("central.password");
    }

conn = sqlConnectRemote(host, user, password, db);

safef(query, sizeof(query), "show table status like '%s'",tbl);
sr = sqlGetResult(conn, query);
f = 0;
d = 0;
i = 0;
n = 0;
while ((fld = sqlFieldName(sr)) != NULL)
    {
    if (sameString(fld,"Name"))
	{
	n = f;
	}
    if (sameString(fld,"Data_length"))
	{
	d = f;
	}
    if (sameString(fld,"Index_length"))
	{
	i = f;
	}
    f++;
    }
while ((row = sqlNextRow(sr)) != NULL)
    {
    c++;
    printf("<tr>");
    printf("<td>%s</td>",row[n]);

    sscanf(row[d],"%lu",&size);
    totalsize+=size;
    mySprintWithCommas(nicenumber, sizeof(nicenumber), size);
    printf("<td align=right>%s</td>",nicenumber);

    sscanf(row[i],"%lu",&size);
    totalsize+=size;
    mySprintWithCommas(nicenumber, sizeof(nicenumber), size);
    printf("<td align=right>%s</td>",nicenumber);

    getIndexes( sqlConnectRemote(host, user, password, db), row[n], indexlist, sizeof(indexlist));
    printf("<td>%s</td>",indexlist);

    printf("</tr>\n");
    }
sqlFreeResult(&sr);

if (c == 0)
    {
    printf("<tr><td style=\"color:red\">%s</td><td style=\"color:red\">%s</td></tr>\n",tbl,"error fetching");
    ++(*errCount);
    }

sqlDisconnect(&conn);
return totalsize;
}

void cutParens(char *s)
/* internally modify string to remove parens and anything they contain.  nested ok, but must be balanced. */
{
int i=0,j=0,l=0,c=0;
l=strlen(s);
for(i=0;i<=l;i++)
    {
    switch (s[i])
	{
	case '(': c++; break;
	case ')': c--; break;
	case 0: c = 0;
	default:
	    if (c==0)
		{
		s[j++]=s[i];
		}
	}
    }

}

void whiteSpace(char *s)
/* internally modify string to convert whitespace to space chars. */
{
char *ss = NULL;
int i=0,ii=0,l=0;
char c=' ';
l=strlen(s);
ss =needMem(l+1);
for(i=0;i<=l;i++)
    {
    c = s[i];
    switch (c)
	{
	case '\t' :
	case '\n' :
	case '\r' :
	case '\f' :
	  c = ' ';
	default:
	    ss[ii++]=c;
	}
    }
safef(s, l+1, "%s", ss);
}


void doShowSizes()
/* show the sizes of all the track tables, cgis, and general files in separate window target= _blank  */
{
char tbl[BLSIZE] = "";
char  db[1024] = "";
off_t size = 0;
off_t totalsize = 0;
unsigned long sizeMB = 0;
int errCount = 0;
off_t totalTable = 0;
off_t totalGbdb = 0;
off_t totalGoldenPath = 0;
int tableCount = 0;
int gbdbCount = 0;
int goldenCount = 0;
int i = 0, ii = 0, iii = 0;
int j = 0, jj = 0, jjj = 0;
int g = 0, gg = 0, ggg = 0;
char nicenumber[1024]="";
struct pushQ *q;
char newQid[sizeof(q->qid)] = "";

char dbsComma[1024];
char dbsSpace[1024];
char dbsVal[1024];
char d;

char tempComma[BLSIZE];
char tempSpace[BLSIZE];
char tempVal[BLSIZE];
char c;

char gComma[BLSIZE];
char gSpace[BLSIZE];
char gVal[BLSIZE];
char gc;
char cgiPath[1024];
char pathName[1024];
char filePath[1024];
char fileName[1024];
char *found=NULL;
struct fileInfo *fi = NULL;
char *crossUrl = "";

safef(newQid, sizeof(newQid), cgiString("qid"));

printf("<H2>Show File Sizes </H2>\n");

q = mustLoadPushQ(newQid);

if (crossPost)  // support showSizes across machines
    {
    crossUrl = sameString(utsName.nodename, "hgwdev") ? "http://hgwbeta.cse.ucsc.edu" : "http://hgwdev.cse.ucsc.edu";
    }

printf("<a href=\"%s/cgi-bin/qaPushQ?action=showSizesHelp&qid=%s&cb=%s\" target=\"_blank\">HELP</a> \n",
    crossUrl,q->qid,newRandState);
printf("<a href=\"%s/cgi-bin/qaPushQ?action=edit&qid=%s&cb=%s\">RETURN</a> \n",crossUrl,newQid,newRandState);
printf(" <br>\n");
printf("Location: %s <br>\n",q->currLoc);
printf("Database: %s <br>\n",q->dbs    );
/* deemed too verbose: */
//printf("  Tables: %s <br>\n",q->tbls   );
//printf("    CGIs: %s <br>\n",q->cgis   );
//printf("   Files: %s <br>\n",q->files  );

cutParens(q->dbs);
cutParens(q->tbls);
cutParens(q->cgis);
cutParens(q->files);

whiteSpace(q->dbs);
whiteSpace(q->tbls);
whiteSpace(q->cgis);
whiteSpace(q->files);

q->tbls = replaceChars(q->tbls,"chrN_","chr*_");
q->tbls = replaceChars(q->tbls,"\\","\\\\");
q->tbls = replaceChars(q->tbls,"%","\\%");
q->tbls = replaceChars(q->tbls,"_","\\_");

for(j=0;parseList(q->dbs, ',' ,j,dbsComma,sizeof(dbsComma));j++)
    {
    if (dbsComma[0]==0)
	{
	continue;
	}
    for(jj=0;parseList(dbsComma, ' ' ,jj,dbsSpace,sizeof(dbsSpace));jj++)
	{
	if (dbsSpace[0]==0)
	    {
	    continue;
	    }
	dbsVal[0]=0;
	for (jjj=0;jjj<=strlen(dbsSpace);jjj++)
	    {
	    d = dbsSpace[jjj];
	    if (
		((d>='A')&&(d<='Z'))
	     || ((d>='a')&&(d<='z'))
	     || ((d>='0')&&(d<='9'))
	     )
		{
		dbsVal[jjj]=d;
		}
	    else
		{
		dbsVal[jjj]=0;
		break;
		}
	    }
	if (dbsVal[0]!=0)
	    {
	    safef(db,sizeof(db),"%s",dbsVal);

	    printf(" <br>\n");
	    printf("<h4>%s on %s:</h4>\n",db,q->currLoc);
	    printf("<table cellpadding=5 >");
	    printf("<th>Table</th>"
	           "<th>data size</th>"
	           "<th>index size</th>"
	           "<th>index keys</th>"
	    );

	    /* we parsed the db multiples, now parse the tbl mutiples  */
	    for(i=0;parseList(q->tbls, ',' ,i,tempComma,sizeof(tempComma));i++)
		{
		if (tempComma[0]==0)
		    {
		    continue;
		    }
		for(ii=0;parseList(tempComma, ' ' ,ii,tempSpace,sizeof(tempSpace));ii++)
		    {
		    if (tempSpace[0]==0)
			{
			continue;
			}
		    tempVal[0]=0;
		    for (iii=0;iii<=strlen(tempSpace);iii++)
			{
			c = tempSpace[iii];
			if (c=='*') c = '%';
			if (
			    ((c>='A')&&(c<='Z'))
			 || ((c>='a')&&(c<='z'))
			 || ((c>='0')&&(c<='9'))
			 || (c=='_')
			 || (c=='%')
			 || (c=='\\')
			)
			    {
			    tempVal[iii]=c;
			    }
			else
			    {
			    tempVal[iii]=0;
			    break;
			    }
			}
		    if (tempVal[0]!=0)
			{
			safef(tbl,sizeof(tbl),"%s",tempVal);
			long long tableSize = pq_getTableSize(q->currLoc,db,tbl,&errCount);
			totalsize += tableSize;
			totalTable += tableSize;
			++tableCount;
			}
		    }
		}

	    printf("</table>");
	    }
	 }
     }


if (!sameString(q->cgis,""))
    {
    printf(" <br>\n");
    printf("<h4>CGIs on %s:</h4>\n",utsName.nodename);
    printf("<table cellpadding=5 >");
    printf("<th>cgi</th><th># bytes</th>");
    for(g=0;parseList(q->cgis, ',' ,g,gComma,sizeof(gComma));g++)
	{
	if (gComma[0]==0)
	    {
	    continue;
	    }
	for(gg=0;parseList(gComma, ' ' ,gg,gSpace,sizeof(gSpace));gg++)
	    {
	    if (gSpace[0]==0)
		{
		continue;
		}
	    gVal[0]=0;
	    for (ggg=0;ggg<=strlen(gSpace);ggg++)
		{
		gc = gSpace[ggg];
		if (
		    ((gc>='A')&&(gc<='Z'))
		 || ((gc>='a')&&(gc<='z'))
		 || ((gc>='0')&&(gc<='9'))
		 || (gc=='.')
		 )
		    {
		    gVal[ggg]=gc;
		    }
		else
		    {
		    gVal[ggg]=0;
		    break;
		    }
		}
	    if (gVal[0]!=0)
		{
		safef(cgiPath,sizeof(cgiPath),"%s%s","./",gVal);
		size=fileSize(cgiPath);
		if (size == -1)
		    {
		    printf("<tr><td style=\"color:red\">%s<td/><td style=\"color:red\">not found</td></tr>\n",gVal);
		    ++errCount;
		    }
		else
		    {
		    totalsize+=size;
		    sprintLongWithCommas(nicenumber, size);
		    printf("<tr><td>%s<td/><td>%s</td></tr>\n",gVal,nicenumber);
		    }
		}
	     }
	 }
    printf("</table>");
    }

if (!sameString(q->files,""))
    {
    printf(" <br>\n");
    printf("<h4>Files on %s:</h4>\n",utsName.nodename);
    printf("<table cellpadding=5 >");
    printf("<th>file</th><th># bytes</th>");
    for(g=0;parseList(q->files, ',' ,g,gComma,sizeof(gComma));g++)
	{
	if (gComma[0]==0)
	    {
	    continue;
	    }
	for(gg=0;parseList(gComma, ' ' ,gg,gSpace,sizeof(gSpace));gg++)
	    {
	    if (gSpace[0]==0)
		{
		continue;
		}
	    gVal[0]=0;
	    for (ggg=0;ggg<=strlen(gSpace);ggg++)
		{
		gc = gSpace[ggg];
		if (
		    ((gc>='A')&&(gc<='Z'))
		 || ((gc>='a')&&(gc<='z'))
		 || ((gc>='0')&&(gc<='9'))
		 || (gc=='.')
		 || (gc=='/')
		 || (gc=='-')
		 || (gc=='_')
		 || (gc=='*')
		 )
		    {
		    gVal[ggg]=gc;
		    }
		else
		    {
		    gVal[ggg]=0;
		    break;
		    }
		}

	    if (gVal[0]!=0)
		{

		if (strrchr(gVal, '*') == NULL)
		    { /* no wildcards in filename, do it the normal way */
		    safef(pathName,sizeof(pathName),"%s",gVal);
		    size=fileSize(pathName);
		    if (size == -1)
			{
			printf("<tr><td style=\"color:red\">%s<td/><td style=\"color:red\">not found</td></tr>\n",gVal);
			++errCount;
			}
		    else
			{
			totalsize+=size;
			sprintLongWithCommas(nicenumber, size);
			printf("<tr><td>%s<td/><td>%s</td></tr>\n",gVal,nicenumber);
			if (startsWith("/gbdb/", pathName))
			    {
			    totalGbdb+=size;
			    ++gbdbCount;
			    }
			if (stringIn("/goldenPath/", pathName))
			    {
    			    totalGoldenPath+=size;
			    ++goldenCount;
			    }
			}
		    }
		else
		    { /* wildcards found in name, use listDirX */
		    printf("<tr><td>expansion for %s<td/></tr>\n",gVal);
		    found = strrchr(gVal, '/');
		    if (found == NULL)
			{
			filePath[0]=0;
			safef(fileName,sizeof(fileName),"%s",gVal);
			}
		    else
			{
			*found = 0;
			safef(filePath,sizeof(filePath),"%s",gVal);
			found++;
			safef(fileName,sizeof(fileName),"%s",found);
			}
		    for (fi = listDirXExt(filePath,fileName,FALSE,TRUE);fi!=NULL;fi=fi->next)
			{
			if (fi->statErrno > 0)  // usually due to bad symlink
			    {
			    char *errStr = htmlEncode(strerror(fi->statErrno));
    			    printf("<tr><td style=\"color:red\">%s<td/>"
				"<td style=\"color:red\">stat() failed: %s</td></tr>\n"
				, fi->name
				, errStr);
			    freeMem(errStr);
			    ++errCount;
			    }
			else if (fi->isDir)
			    {
			    printf("<tr><td style=\"color:red\">error: %s is a directory<td/></tr>\n",fi->name);
			    ++errCount;
			    }
			else
			    {
    			    totalsize+=fi->size;
			    if (startsWith("/gbdb/", filePath))
				{
				++gbdbCount;
				totalGbdb+=fi->size;
				}
    			    if (stringIn("/goldenPath/", filePath))
				{
				++goldenCount;
				totalGoldenPath+=fi->size;
				}
    			    sprintLongWithCommas(nicenumber, fi->size);
    			    printf("<tr><td>%s<td/><td>%s</td></tr>\n",fi->name,nicenumber);
			    }
			}
		    printf("<tr><td>&nbsp;<td/></tr>\n"); /* spacer */
		    }

		}
	     }
	 }
    printf("</table>");
    }

if (totalTable > 0)
    {
    printf(" <br>\n");
    mySprintWithCommas(nicenumber, sizeof(nicenumber), totalTable);
    printf(" Total size of tables: %s ",nicenumber);
    sprintWithGreekByte(nicenumber, sizeof(nicenumber), totalTable);
    printf("&nbsp; ( %s ) ",nicenumber);
    mySprintWithCommas(nicenumber, sizeof(nicenumber), tableCount);
    printf("&nbsp; ( %s tables ) <br>\n", nicenumber);
    }

if (totalGbdb > 0)
    {
    printf(" <br>\n");
    mySprintWithCommas(nicenumber, sizeof(nicenumber), totalGbdb);
    printf(" Total size of /gbdb/ files: %s ",nicenumber);
    sprintWithGreekByte(nicenumber, sizeof(nicenumber), totalGbdb);
    printf("&nbsp; ( %s ) ",nicenumber);
    mySprintWithCommas(nicenumber, sizeof(nicenumber), gbdbCount);
    printf("&nbsp; ( %s gbdb files ) <br>\n", nicenumber);
    }

if (totalGoldenPath > 0)
    {
    printf(" <br>\n");
    mySprintWithCommas(nicenumber, sizeof(nicenumber), totalGoldenPath);
    printf(" Total size of .../goldenPath/ files: %s ",nicenumber);
    sprintWithGreekByte(nicenumber, sizeof(nicenumber), totalGoldenPath);
    printf("&nbsp; ( %s ) ",nicenumber);
    mySprintWithCommas(nicenumber, sizeof(nicenumber), goldenCount);
    printf("&nbsp; ( %s goldenPath files ) <br>\n", nicenumber);
    }

printf(" <br>\n");
mySprintWithCommas(nicenumber, sizeof(nicenumber), totalsize);
printf(" Total size of all: %s ",nicenumber);
sprintWithGreekByte(nicenumber, sizeof(nicenumber), totalsize);
printf("&nbsp; ( %s ) <br>\n",nicenumber);


printf(" <br>\n");
sizeMB = (((totalsize * 1.0) / (1024 * 1024)) + 0.5);
if ((sizeMB == 0) && (totalsize > 0))
    {
    sizeMB = 1;
    }
sprintLongWithCommas(nicenumber, sizeMB );
printf("<p style=\"color:red\">Total: %s MB",nicenumber);
if (errCount)
    {
    printf("&nbsp;&nbsp; Errors: %d",errCount);
    }
printf("</p>\n");


printf(" <br>\n");
printf("<a href=\"%s/cgi-bin/qaPushQ?action=setSize&qid=%s&sizeMB=%lu&cb=%s\">"
       "Set Size as %s MB</a> <br>\n",crossUrl,newQid,sizeMB,newRandState,nicenumber);
printf(" <br>\n");
printf("<a href=\"%s/cgi-bin/qaPushQ?action=edit&qid=%s&cb=%s\">RETURN</a> <br>\n",crossUrl,newQid,newRandState);

pushQFree(&q);

}


void listQueues(char *action, boolean isTransfer);  /* forward decl */

void doTransfer()
/* Present choice of queues to which the selected and locked record may be transferred */
{
struct pushQ *q;
char newQid[sizeof(q->qid)] = "";
char tempUrl[256];

ZeroVar(&q);

safef(newQid, sizeof(newQid), cgiString("qid"));

printf("<H2>Transfer Queue Entry %s:%s to Another Queue </H2>\n", pushQtbl, newQid);

q=mustLoadPushQ(newQid);

printf("<a href=\"/cgi-bin/qaPushQ?action=edit&qid=%s&cb=%s\">RETURN</a> \n",newQid,newRandState);
printf("<br>\n");
printf("<br>\n");

safef(tempUrl, sizeof(tempUrl), "action=transferTo&qid=%s&toOrg", newQid);
listQueues(tempUrl, TRUE);

printf("<a href=\"/cgi-bin/qaPushQ?action=edit&qid=%s&cb=%s\">RETURN</a> <br>\n",newQid,newRandState);
pushQFree(&q);
}


void doTransferTo()
/* Execute transfer of qid from current pushQtbl to named target Q table */
{
struct pushQ *q;
int updateSize = 2456; /* almost anything works here */
char *toOrg = cgiString("toOrg");  /* required cgi var */
char *origQid = NULL;
int newqid = 0;
char newQid     [sizeof(q->qid)]      = "";
char *savePushQtbl = cloneString(pushQtbl);
char query[256];

origQid = cloneString(cgiString("qid"));  /* required cgi var */

/* get the data from the record */
q = mustLoadPushQ(origQid);

/* first close the hole where it was */
safef(query, sizeof(query),
"update %s set rank = rank - 1 where priority ='%s' and rank > %d ",
pushQtbl, q->priority, q->rank);
sqlUpdate(conn, query);

/* delete old record */
safef(query, sizeof(query), "delete from %s where qid ='%s'", pushQtbl, origQid);
sqlUpdate(conn, query);

/* temporarily set pushQtbl to target */
safef(pushQtbl, sizeof(pushQtbl), "%s",toOrg);

/* unlock record - don't want it to keep the lock status after xfer */
safef(q->lockUser, sizeof(q->lockUser), "%s","");
safef(q->lockDateTime, sizeof(q->lockDateTime), "%s","");

/* get the maxQid from the target Q tbl */
newqid = getNextAvailQid();
safef(msg, sizeof(msg), "%%0%dd", (int)sizeof(q->qid)-1);
safef(newQid,sizeof(newQid),msg,newqid);
safef(q->qid, sizeof(q->qid), newQid);
safef(msg, sizeof(msg), "%s", "");

/* get the maxrank from the target Q tbl for given priority */
if (q->priority[0]=='L')
    {
    q->rank = 0;
    }
else
    {
    q->rank = getNextAvailRank(q->priority);
    }

/* clear parent link since can not be maintained */
safef(q->pqid, sizeof(q->pqid), "%s", "");

/* save record into new target Q */
pushQSaveToDbEscaped(conn, q, pushQtbl, updateSize);

/* restore pushQtbl */
safef(pushQtbl, sizeof(pushQtbl), "%s", savePushQtbl);

/* set msg to display the new Qid to the user, visible on return */
safef(msg,sizeof(msg),"Transferred %s Qid %s to %s Qid %s. <br>\n", pushQtbl, origQid, toOrg, q->qid);

doDisplay();

freez(&origQid);
freez(&savePushQtbl);
pushQFree(&q);
}




void doShowDisplayHelp()
/* show the sizes of all the track tables, cgis, and general files in separate window target= _blank  */
{
printf("<h4>Display Help</h4>\n");
printf("<br>\n");
printf("NOTE: DO NOT USE BACK-BUTTON OR REFRESH ON YOUR BROWSER. <br>\n");
printf("<br>\n");
printf(" &nbsp;&nbsp;&nbsp;   NAVIGATION BUTTONS AND LINKS ARE PROVIDED. <br>\n");
printf("<br>\n");
printf("ADD - add a new Push Queue record.<br>\n");
printf("Logout - clears your cookie and logs out.<br>\n");
printf("All Columns - allows you to bring back hidden columns.<br>\n");
printf("Default Columns - reset your column preferences to default columns and order.<br>\n");
printf("Log by Month - view old log records by month<br>\n");
printf("Gateway - click to select alternate push queues, if any, e.g. new track/org<br>\n");
printf("HELP - click to see this help.<br>\n");
printf("Refresh - click to see if others have made changes.<br>\n");
printf("<br>\n");
printf("! - click to hide a column you do not wish to see.<br>\n");
printf("< - click to move the column to the left.<br>\n");
printf("> - click to move the column to the right.<br>\n");
printf("<br>\n");
printf("^ - click to raise the priority of the record higher within the priority-class.<br>\n");
printf("v - click to lower the priority.<br>\n");
printf("T - click to raise to top priority.<br>\n");
printf("B - click to lower to bottom priority.<br>\n");
printf("<br>\n");
printf("Queue Id - click to edit or see the details page for the record.<br>\n");
printf("<br>\n");
printf("<a href=\"javascript:window.close();\" >CLOSE</a> <br>\n");
}


void doShowEditHelp()
/* show the sizes of all the track tables, cgis, and general files in separate window target= _blank  */
{
struct pushQ q;
ZeroVar(&q);
safef(q.qid,sizeof(q.qid),cgiString("qid"));
printf("<h4>Details/Edit Help</h4>\n");
printf("<br>\n");
printf("CANCEL - click to return to main display without saving changes.<br>\n");
printf("HELP - click to see this help.<br>\n");
printf("<br>\n");
printf("Initial submission - displays date automatically generated when push queue record is created.<br>\n");
printf("Importance - from Redmine.<br>\n");
printf("Date Opened - date QA (re)opened. (YYYY-MM-DD) Defaults originally to current date to save typing.<br>\n");
printf("New track? - choose Y if this is a new track (i.e. has never before appeared on beta).<br>\n");
printf("Track - enter the track name as it will appear in the genome browser (use the shortLabel).<br>\n");
printf("Release Log- enter the short Label (usually) followed by notes in parentheses if any. This appears in the release log unless empty.<br>\n");
printf("Databases - enter db name. May be comma-separated list if more than one organism, etc.<br>\n");
printf("Tables - enter as comma-separated list all tables that apply. They must exist in the database specified. Wildcard * supported. (Put comments in parentheses).<br>\n");
printf("CGIs - enter names of any new cgis that are applicable. Must be found on hgwbeta.<br>\n");
printf("Files - enter pathnames of any additional files if needed.<br>\n");
printf("Size(MB) - enter the size of the total push in megabytes (MB).<br>\n");
printf("Show Sizes button - click to see a complete list of sizes of all tables and cgis.  Tables are relative to Current Location specified.<br>\n");
printf("Current Location - chooose the current location of the files.  Should default to hgwdev at start, after sudo mypush to hgwbeta, change this to hgwbeta.<br>\n");
printf("Makedoc verified? - choose Y if you have verified the MakeAssembly.doc in kent/src/hg/makeDb.<br>\n");
printf("Online help - enter status of online help. Verify <a href=http://hgwbeta.cse.ucsc.edu/goldenPath/help/hgTracksHelp.html#IndivTracks>hgTracksHelp</a><br>\n");
printf("Index verified? - choose Y if the index has been verified. Use the ShowSizes button for a quick view.<br>\n");
printf("All.joiner verified? - choose Y if the all.joiner in /hg/makeDb/schema has been verified.<br>\n");
printf("Status - enter current status (255 char max). Put long notes in Open Issues or Notes.<br>\n");
printf("Sponsor - usually the developer.<br>\n");
printf("Reviewer - usually the QA person handling the push queue for the track.<br>\n");
printf("External Source or Collaborator - external contact outside our staff that may be involved.<br>\n");
printf("Open Issues - Record any remaining open issues that are not completely resolved (no size limit here).<br>\n");
printf("Notes - Any other notes you would like to make (no size limit here).<br>\n");
printf("<br>\n");
printf("Submit button - save changes and return to main display.<br>\n");
printf("delete button - delete this push queue record and return to main display.<br>\n");
printf("push requested button - press only if you are QA staff and about to submit the push-request. It will try to verify that required entries are present.<br>\n");
printf("clone button - press if you wish to split the original push queue record into multiple parts. Saves typing, used rarely.<br>\n");
printf("bounce button - press to bounce from priority A, the QA queue, to B, the developer queue if it needs developer attention.<br>\n");
printf("transfer button - press to transfer the pushQ entry to another queue.<br>\n");
printf("lock - press lock to lock the record and edit it.  When in edit mode, make your changes and submit.  Do not leave the record locked.<br>\n");
printf("<br>\n");
printf("<a href=\"javascript:window.close();\">CLOSE</a> <br>\n");
}


void doShowSizesHelp()
/* show the sizes of all the track tables, cgis, and general files in separate window target= _blank  */
{
struct pushQ q;
ZeroVar(&q);
safef(q.qid,sizeof(q.qid),cgiString("qid"));
printf("<h4>Show File Sizes Help</h4>\n");
printf("<br>\n");
printf("Tables: Shows sizes of database data and indexes.<br>\n");
printf("Expands wildcard * in table names list. <br>\n");
printf("Shows total index size, and the key expression of each index.<br>\n");
printf("Location of tables is relative to the Current Location setting in the record.<br>\n");
printf("<br>\n");
printf("CGIs: shows files specified. Currently limited to checking localhost (hgwbeta in this case).<br>\n");
printf("<br>\n");
printf("Total size of all:  total size of all files found in bytes.<br>\n");
printf("Total: size in megabytes(MB) which is what should be entered into the size(MB) field of the push queue record.<br>\n");
printf("<br>\n");
printf("RETURN - click to return to the details/edit page.<br>\n");
printf("Set Size As - click to set size to that found, and return to the details/edit page. Saves typing. Be sure to press submit to save changes.<br>\n");
printf("<br>\n");
printf("<br>\n");
printf("<a href=\"javascript:window.close();\">CLOSE</a> <br>\n");
}

void checkConn2()
/* get 2nd conn, if not already done */
{
if (!conn2)
    conn2 = sqlConnectRemote(host, user, password, database);
}

boolean verifyTableIsQueue(char *table)
/* Return TRUE if table is a push Q */
{
boolean result = TRUE;
char query[256];
char *field = NULL;
safef(query, sizeof(query), "desc %s",table);
checkConn2();
field = sqlQuickString(conn2, query);
result = sameString(field,"qid");
freez(&field);
return result;
}

void listQueues(char *action, boolean isTransfer)
/* list the available queues other than self */
{
struct sqlResult *sr;
char **row;
char query[256];
char *monthChange = isTransfer ? "" : "&month=current";
if (!(isTransfer && sameString(pushQtbl,"pushQ")))
    {
    char *extra = (sameString(pushQtbl,"pushQ") ? " (you are here)" : "");
    printf("<A href=qaPushQ?%s=%s%s&cb=%s>Main Push Queue</A>%s<br>\n",action,"pushQ",monthChange,newRandState,extra);
    printf("<br>\n");
    }
safef(query, sizeof(query), "show tables");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (!(isTransfer && sameString(row[0],pushQtbl)) &&
	!sameString(row[0],"pushQ") &&
	!sameString(row[0],"users") &&
	!sameString(row[0],"gbjunk"))
	{
	if (verifyTableIsQueue(row[0]))
	    {
	    char *displayQ = row[0];
	    if (sameString(displayQ,"pushQ"))
		displayQ = "Main Push Queue";
	    char *extra = (sameString(row[0],pushQtbl) ? " (you are here)" : "");
    	    printf("<A href=qaPushQ?%s=%s%s&cb=%s>%s</A>%s<br>\n",action,row[0],monthChange,newRandState,displayQ,extra);
	    }
	}
    }
sqlFreeResult(&sr);
printf("<br>\n");
}

void doShowGateway()
/* This gives the user a choice of queues to use */
{

printf("<h2>Gateway - Choose Queue</h2>\n");
printf("<br>\n");

listQueues("org", FALSE);

printf("<a href=\"/cgi-bin/qaPushQ?cb=%s\">RETURN</a> <br>\n",newRandState);
}

void doUnlock()
/* currently a backdoor for logged-in users to unlock a record */
{
struct pushQ *q;
int updateSize = 2456; /* almost anything works here */

q = mustLoadPushQ(cgiString("qid")); /* required cgi var */

/* unlock record */
safef(q->lockUser, sizeof(q->lockUser), "%s","");
safef(q->lockDateTime, sizeof(q->lockDateTime), "%s","");

/* update existing record */
pushQUpdateEscaped(conn, q, pushQtbl, updateSize);

pushQFree(&q);

doDisplay();

}


/* ======================================================== */


void doDrawReleaseLog(boolean isEncode)
/* Test - draw the release log using log data in pushQ  */
{

char *centraldb  = NULL;
char *chost       = NULL;
char *cuser       = NULL;
char *cpassword   = NULL;

struct sqlConnection *betaconn = NULL;

struct dbDb *ki=NULL, *kiList=NULL, *dbDbTemp=NULL;
struct sqlResult *sr;
char **row;
char query[256];
char tempName[256];
char now[256];

int m=0,d=0;
int topCount=0;

char *encodeClause = "";

if (isEncode)
    encodeClause = " and releaseLog like '%ENCODE%'";

ZeroVar(&dbDbTemp);


chost     = cfgOption("rrcentral.host"    );
cuser     = cfgOption("rrcentral.user"    );
cpassword = cfgOption("rrcentral.password");
centraldb = cfgOption("rrcentral.db");

webStart(NULL, NULL, "Track and Table Releases");



// only allowed one connection at a time?
sqlDisconnect(&conn);

betaconn = sqlConnectRemote(chost, cuser, cpassword, centraldb);


printf(" This page contains track and table release information for the following genome assemblies:<br>\n");

safef(query,sizeof(query),
    "select * from dbDb "
    "where active=1 "
    "order by orderKey, name desc");
sr = sqlGetResult(betaconn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ki = dbDbLoad(row);
    slAddHead(&kiList, ki);
    }
sqlFreeResult(&sr);

slReverse(&kiList);
sqlDisconnect(&betaconn);

// are we really only allowed one remoteconn at a time?
conn = sqlConnectRemote(host, user, password, database);

/* filter the db list to make sure we actually have data */
struct dbDb *newList=NULL, *kiNext;
for (ki = kiList; ki != NULL; ki = kiNext)
    {
    kiNext = ki->next;
    safef(query,sizeof(query),
	"select count(*) from pushQ "
	"where priority='L' and releaseLog != '' and dbs like '%%%s%%' %s"
	"order by qadate desc, qid desc",
	ki->name,
	encodeClause
	);
    if (sqlQuickNum(conn, query) > 0)
	{
    	slAddHead(&newList, ki);
	}
    }
slReverse(&newList);
kiList = newList;

/* 10 Latest Changes */
printf("<ul>\n");
printf("<li><a CLASS=\"toc\" HREF=\"#recent\" ><b>10 Latest Changes (all assemblies)</b></a></li>");

/* regular log index #links */
for (ki = kiList; ki != NULL; ki = ki->next)
    {
    safef(tempName,sizeof(tempName),ki->organism);
    if (!sameString(ki->organism, ki->genome))
	{
	safef(tempName,sizeof(tempName),"<em>%s</em>",ki->genome);
	}
    printf("<li><a CLASS=\"toc\" HREF=\"#%s\">%s %s (%s)</a></li>\n",
	ki->name,tempName,ki->description,ki->name);
    }

printf("</ul>\n");
printf("<p>\n");
printf(" For more information about the tracks and tables listed on this page, refer to the "
"<a href=/goldenPath/help/hgTracksHelp.html#IndivTracks>User's Guide</a>.<p>\n");

strftime (now, sizeof(now), "%02d %b %Y", loctime); /* default to today's date */
printf("<em>Last updated %s. <a HREF=\"/contacts.html\">Inquiries and feedback welcome</a>.</em>\n",now);
/* 10 LATEST CHANGES */
webNewSection("<A NAME=recent></A> 10 Latest Changes (all assemblies)");

printf("<TABLE CELLPADDING=4 style='border:1px solid #aaaaaa; width:100%%;'>\n"
    "<TR>\n"
    "<TD nowrap><B style='color:#006666;'>Track/Table Name</B></TD>\n"
    "<TD nowrap><B style='color:#006666;'>Assembly</B></TD>\n"
    "<TD nowrap><B style='color:#006666;'>Release Date</B></TD>\n"
    "</TR>\n"
    );
safef(query,sizeof(query),
    "select releaseLog, dbs, qadate, releaseLogUrl from pushQ "
    "where priority='L' and releaseLog != '' and dbs != '' %s"
    "order by qadate desc, qid desc ", encodeClause
    );
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    sscanf(cloneStringZ(&row[2][5],2),"%d",&m);
    sscanf(cloneStringZ(&row[2][8],2),"%d",&d);
	{  /* parse dblist and make sure it's kosher and active=1 good */
	char* dbs = cloneString(row[1]);
	char dbsComma[1024];
	char dbsSpace[1024];
	struct dyString* dbList = newDyString(1024);
	int j = 0, jj = 0;
	char* sep = "";
	boolean found = FALSE;
	cutParens(dbs);
	whiteSpace(dbs);
	for(j=0;parseList(dbs, ',' ,j,dbsComma,sizeof(dbsComma));j++)
	    {
	    if (dbsComma[0]==0)
		{ continue; }
	    for(jj=0;parseList(dbsComma, ' ' ,jj,dbsSpace,sizeof(dbsSpace));jj++)
		{
		if (dbsSpace[0]==0)
		    { continue; }
		for (ki = kiList; ki != NULL; ki = ki->next)
		    {
		    if (sameWord(ki->name,dbsSpace))
			{
			found = TRUE;
			dyStringPrintf(dbList,"%s%s",sep,dbsSpace);
			sep=",";
			}
		    }
		}
	    }

	if (found)
	    {
	    topCount++;
	    printf("<TR valign=top><TD align=left>\n");
	    if (sameOk(row[3], ""))
		{
    		printf("%s", row[0]);
		}
	    else
		{
    		printf("<a href=\"%s\" target=_blank>%s</a>", row[3], row[0]);
		}
	    printf("</td>\n");
	    printf("<td>%s</td>\n"
		"<td>%02d %s %s</td>\n"
		"</tr>\n",
		row[1], d, numberToMonth[m-1], cloneStringZ(row[2],4) );
	    }

	freez(&dbs);
	freeDyString(&dbList);

	if (topCount>=10)
	    {break;}

	}

    }
sqlFreeResult(&sr);
printf("</table>\n");

/* REGULAR LOG */
for (ki = kiList; ki != NULL; ki = ki->next)
    {
    safef(tempName,sizeof(tempName),ki->organism);
    if (!sameString(ki->organism, ki->genome))
	{
	safef(tempName,sizeof(tempName),"<em>%s</em>",ki->genome);
	}

    webNewSection("<A NAME=%s></A>%s %s (%s, %s)",
	ki->name, tempName, ki->description, ki->name, ki->sourceName);
    printf("<TABLE CELLPADDING=4 style='border:1px solid #aaaaaa; width:100%%;'>\n"
	"<TR><TD nowrap><B style='color:#006666;'>Track/Table Name</B></TD>\n"
	"    <TD nowrap><B style='color:#006666;'>Release Date</B>\n"
	"</TD></TR>\n"
	);

    safef(query,sizeof(query),
	"select releaseLog, qadate, releaseLogUrl from pushQ "
	"where priority='L' and releaseLog != '' and dbs like '%%%s%%' %s"
	"order by qadate desc, qid desc",
	ki->name,
	encodeClause
	);

    //printf("query=%s\n",query);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	sscanf(cloneStringZ(&row[1][5],2),"%d",&m);
	sscanf(cloneStringZ(&row[1][8],2),"%d",&d);
	printf("<TR valign=top><TD align=left>\n");
	if (sameOk(row[2], ""))
	    {
	    printf("%s", row[0]);
	    }
	else
	    {
	    printf("<a href=\"%s\" target=_blank>%s</a>", row[2], row[0]);
	    }
	printf("</td>\n");
	printf(
	    "<td>%02d %s %s</td>\n"
	    "</tr>\n",
	    d, numberToMonth[m-1], cloneStringZ(row[1],4) );
	}
    sqlFreeResult(&sr);
    printf("</table>\n");

    }

dbDbFreeList(&kiList);

webEnd();
}

void doReleaseLogPush()
/* fetch and write releaseLog and display cut-and-pastable push-request */
{
//char *rlPath = "/goldenPath/releaseLogNew.html";
//char *apache = "/usr/local/apache/htdocs";
char *rlPath = "/trash/releaseLogNew.html";
char *apache = "/usr/local/apache";
char url[256] = "";
struct htmlPage *page = NULL;
char filePath[256] = "";
FILE *f=NULL;

safef(url, sizeof(url), "http://%s/cgi-bin/qaPushQ?action=releaseLog",utsName.nodename);
page = htmlPageGet(url);
if (page->status->status == 200)
    {
    safef(filePath,sizeof(filePath),"%s%s",apache,rlPath);
    f=mustOpen(filePath, "w");
    mustWrite(f, page->htmlText, strlen(page->htmlText));
    carefulClose(&f);
    printf("<br>\n");
    printf("Updated release log html %s<br>\n",rlPath);
    printf("<br>\n");
    printf("push-request:<br>\n");
    printf("<br>\n");
    printf("Please push from beta to RR,MGC: <br>\n");
    printf("&nbsp;&nbsp;&nbsp;%s<br>\n",filePath);
    printf("<br>\n");
    printf("Thanks!<br>\n");
    printf("<br>\n");
    printf("<br>\n");
    printf("See <a href=%s>Release Log</a><br>\n",rlPath);
    }
else
    {
    printf("Error reading %s: %d<br>\n",rlPath,page->status->status);
    }
printf("<br>\n");
printf("<a href=\"javascript:window.close();\">CLOSE</a> <br>\n");

htmlPageFree(&page);

}


/* ======================================================== */

/* ------------------------------------------------------- */


void doMiddle()
/* dispatch events */
{

char *org = NULL;      /* changes pushQtbl */
char *newmonth = NULL; /* changes month */
char *temp = NULL;
char *reqRandState = NULL;

/* debug *
safef(msg,sizeof(msg),"db='%s', host='%s', user='%s', password='%s' <br>\n",database,host,user,password);
htmShell("Push Queue debug", doMsg, NULL);
exit(0);
*/

if (crossPost)  // support showSizes across machines
    {
    //host = sameString(utsName.nodename, "hgwdev") ? "hgwbeta.cse.ucsc.edu" : "hgwdev.cse.ucsc.edu";
    host = cfgOption("pq.crossHost");
    }

newRandState = randDigits(20);

conn = sqlConnectRemote(host, user, password, database);

setLock();

/* default columns */
showColumns = cloneString(defaultColumns);

readMyUser();

org = cgiUsualString("org","");  /* get org, defaults to display of main push queue */
if (!sameString(org,""))
    {
    safef(pushQtbl,sizeof(pushQtbl),org);
    }
if (!sqlTableExists(conn, pushQtbl))  /* if pushQtbl no longer exists, switch to main "pushQ" and set action to "display" */
    {
    safef(pushQtbl,sizeof(pushQtbl),"pushQ");
    action=cloneString("display");    /* do not need to free action because it just points to a cgi-var hash element */
    }

newmonth = cgiUsualString("month","");  /* get month, if =current then resets to normal */
if (!sameString(newmonth,""))
    {
    if (sameString(newmonth,"current"))
	{
	safef(month, sizeof(month), "%s", "");
	}
    else
	{
	temp = needMem(strlen(newmonth)+1+3);
	safef(temp, strlen(newmonth)+1+3, "%s-01",newmonth);
	if (isDateValid(temp))
	    {
	    safef(month,sizeof(month),newmonth);
	    }
	}
    }



if (sameString(action,"unlock"))
    {
    /* user probably didnt type in the right cachebuster cb= parm, we will supply it. */
    cgiVarSet("cb",oldRandState);
    }

reqRandState = cgiUsualString("cb","");  /* get cb (cache-buster), ignores request, defaults to main display page */
if (!sameString(reqRandState,oldRandState))
    {
    printf("req != old. \n\n req=%s,  old=%s \n\n",reqRandState,oldRandState);
    action = cloneString("display");
    }



/* ---- Push Queue  ----- */

if (sameString(action,"display"))
    {
    doDisplay();
    }

else if (sameString(action,"add"))
    {
    doAdd();
    }

else if (sameString(action,"edit"))
    {
    doEdit();
    }

else if (sameString(action,"post"))
    {
    doPost();
    }

else if (sameString(action,"promote"))
    {
    doPromote();
    }

else if (sameString(action,"demote"))
    {
    doDemote();
    }

else if (sameString(action,"top"))
    {
    doTop();
    }

else if (sameString(action,"bottom"))
    {
    doBottom();
    }

else if (sameString(action,"pushDone"))
    {
    doPushDone();
    }

else if (sameString(action,"demoteColumn" ))
    {
    doDemoteColumn();
    }

else if (sameString(action,"hideColumn"   ))
    {
    doHideColumn();
    }

else if (sameString(action,"promoteColumn"))
    {
    doPromoteColumn();
    }

else if (sameString(action,"showAllCol" ))
    {
    doShowAllColumns();
    }

else if (sameString(action,"showColumn"   ))
    {
    doShowColumn();
    }

else if (sameString(action,"showDefaultCol" ))
    {
    doShowDefaultColumns();
    }

else if (sameString(action,"showMonths" ))
    {
    doShowMonths();
    }

else if (sameString(action,"showSizes" ))
    {
    doShowSizes();
    }

else if (sameString(action,"transfer" ))
    {
    doTransfer();
    }

else if (sameString(action,"transferTo" ))
    {
    doTransferTo();
    }

else if (sameString(action,"showGateway" ))
    {
    doShowGateway();
    }

else if (sameString(action,"unlock" ))
    {
    doUnlock();
    }

else if (sameString(action,"setSize" ))
    {
    doSetSize();
    }

else
    {
    safef(msg,sizeof(msg),"action='%s' is invalid! <br>\n",action);
    doMsg();
    }

oldRandState=newRandState;

saveMyUser();
releaseLock();
sqlDisconnect(&conn);
sqlDisconnect(&conn2);

}


int main(int argc, char *argv[], char *env[])
/* Process command line if any. */
{

assert(e_NUMCOLS == ArraySize(colName));
assert(e_NUMCOLS == ArraySize(colHdr));

if (!cgiIsOnWeb())
    {
    warn("This is a CGI script - attempting to fake environment from command line");
    cgiSpoof(&argc, argv);
    }

saveEnv = env;

curtime = time (NULL);           /* Get the current time. */
loctime = localtime (&curtime);  /* Convert it to local time representation. */

srand( (unsigned)time( NULL ) );  /* Set seed (initial seed) off clock (not the best quality random input ;) */

uname(&utsName);

ZeroVar(&myUser);

database = cfgOption("pq.db"      );
host     = cfgOption("pq.host"    );
user     = cfgOption("pq.user"    );
password = cfgOption("pq.password");

/* workaround for name-collision on form.action now as form._action on form */
action = cgiUsualString("_action","display");  /* get action, defaults to display of push queue */
action = cgiUsualString("action",action);

if (sameString(action,"xpost"))
    {
    crossPost = TRUE;  /* are we doing cross-post from dev to beta (or vice versa)? */
    action = "post";
    }

/* initCgiInput() is not exported in cheapcgi.h, but it should get called by cgiUsualString
So it will find all input regardless of Get/Put/Post/etc and make available as cgivars */


if (sameString(action,"releaseLog"))
    {
    doDrawReleaseLog(FALSE);
    return 0;
    }
if (sameString(action,"encodeReleaseLog"))
    {
    doDrawReleaseLog(TRUE);
    return 0;
    }
if (sameString(action,"releaseLogPush"))
    {
    htmShell(TITLE, doReleaseLogPush, NULL);
    return 0;
    }



qaUser = findCookieData("qapushq");  /* will also cause internal structures to load cookie data */
if ((qaUser == NULL) || (sameString(qaUser,"")))
    {
    if (!sameString(action,"postLogin"))
    	action = cloneString("login");
    }

if (sameString(action,"login"))
    {
    htmShell(TITLE, doLogin, NULL);
    }

else if (sameString(action,"postLogin"))
    {
    doPostLogin();        /* cant start htmShell until cookie is set */
    }
else if (sameString(action,"reset"))
    {
    doCookieReset();      /* cant start htmShell until cookie is re-set */
    }

/* The help screens open in a separate window and don't hurt anything. Only displays text. Ignore cb. */
else if (sameString(action,"showDisplayHelp" ))
    {
    htmShell(TITLE, doShowDisplayHelp , NULL);
    }
else if (sameString(action,"showEditHelp"    ))
    {
    htmShell(TITLE, doShowEditHelp    , NULL);
    }
else if (sameString(action,"showSizesHelp"   ))
    {
    htmShell(TITLE, doShowSizesHelp   , NULL);
    }

else
    {
    htmShell(TITLE, doMiddle, NULL);
    }

return 0;

}

