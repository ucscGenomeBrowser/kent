
/* qaPushQ - An test Push Queue cgi program that uses mySQL. */

/* if we use the cart, we will need to link jkhgap.a */

#include "common.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "jksql.h"
#include "hgConfig.h"

#include <time.h>
#include <stdio.h>
#include <crypt.h>
#include <fcntl.h>
#include <sys/utsname.h>

#include "pushQ.h"
#include "formPushQ.h"

#include "versionInfo.h"

static char const rcsid[] = "$Id: qaPushQ.c,v 1.7 2004/05/05 19:06:56 galt Exp $";

char msg[2048] = "";
char ** saveEnv;

#define BUFMAX 65536
char html[BUFMAX];

#define LOCKFILE "../trash/qaPushQ.lock"

char *database = NULL;
char *host     = NULL;
char *user     = NULL;
char *password = NULL;

struct sqlConnection *conn = NULL;

char *qaUser = NULL;

#define SSSZ 256  /* MySql String Size 255 + 1 */
#define MAXBLOBSHOW 128

#define MAXCOLS 25

#define TITLE "Push Queue v"VERSION

time_t curtime;
struct tm *loctime;

struct utsname utsName;

struct users myUser;

char *showColumns = NULL;

char *defaultColumns =
    "pqid,qid,priority,qadate,track,dbs,tbls,cgis,files,currLoc,makeDocYN,onlineHelp,ndxYN,stat,sponsor,reviewer,extSource,notes";
/*
"qid,pqid,priority,rank,qadate,newYN,track,dbs,tbls,cgis,files,sizeMB,currLoc,makeDocYN,onlineHelp,ndxYN,joinerYN,stat,sponsor,reviewer,extSource,openIssues,notes,reqdate,pushYN";
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
 "sponsor"   ,
 "reviewer"  ,
 "extSource" ,
 "openIssues",
 "notes"     ,
 "reqdate"   ,
 "pushYN"    
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
e_sponsor   ,
e_reviewer  ,
e_extSource ,
e_openIssues,
e_notes     ,
e_reqdate   ,
e_pushedYN    
};

char *colHdr[] = {
"Queue ID",
"Parent Queue ID",
"Priority",
"Rank",
"&nbsp;&nbsp;Submission&nbsp;&nbsp; Date",
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
"Sponsor (local)",
"Reviewer",
"External Source or Collaborator",
"Open Issues",
"Notes",
"Req&nbsp;Date",
"Pushed?"
};



enum colEnum colOrder[MAXCOLS];
int numColumns = 0;



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

boolean checkPWD(char *password, char *encPassword)
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



char * errno_string(int e)
/* convert errno to string */
{
switch (e)
    {
    case EPERM    : return "Not owner";
    case ENOENT   : return "No such file or directory";
    case ESRCH    : return "No such process";
    case ENXIO    : return "No such device or address";
    case ENOEXEC  : return "Exec format error";
    case EBADF    : return "Bad file number";
    case ECHILD   : return "No children";
    case ENOMEM   : return "Not enough core";
    case EACCES   : return "Permission denied";
    case EFAULT   : return "Bad address";
    case ENOTBLK  : return "Block device required";
    case EBUSY    : return "Mount device busy";
    case EEXIST   : return "File exists";
    case EXDEV    : return "Cross-device link";
    case ENODEV   : return "No such device";
    case ENOTDIR  : return "Not a directory";
    case EISDIR   : return "Is a directory";
    case EINVAL   : return "Invalid argument";
    case ENFILE   : return "File table overflow";
    case EMFILE   : return "Too many open files";
    case ENOTTY   : return "Not a typewriter";
    case ETXTBSY  : return "Text file busy";
    case EFBIG    : return "File too large";
    case ENOSPC   : return "No space left on device";
    case ESPIPE   : return "Illegal seek";
    case EROFS    : return "Read-only file system";
    case EMLINK   : return "Too many links";
    case EPIPE    : return "Broken pipe";
    case ELOOP    : return "Too many levels of symbolic links";
    case ENAMETOOLONG: return "File name too long";
    }
}



void setLock()
/* set a lock to reduce concurrency problems */
{

/* creat may be set to fail if the file exists, allowing a simple lock mechanism */
int retries = 0;
int fd = 0;

/* note: only retry 5 times with sleep 5 secs between, then assume failure and push ahead */
while (retries < 5)
    {
    fd = open( LOCKFILE, O_CREAT|O_WRONLY|O_TRUNC|O_EXCL, 0660);

    /* 
    if (fd < 0) 
	{
	safef(msg, sizeof(msg),
	    "error creating lockfile, fd==%d, errno=%d, err_string=%s",
	    fd, errno, errno_string (errno));
	    htmShell(TITLE, doMsg, NULL);
	return;
	}
    */
    
    if (fd >= 0) 
	{
	close(fd);
	return;
	}
    sleep(5);
    retries++;
    }
}

void releaseLock()
/* release the simple lock */
{
unlink(LOCKFILE);
}


void cleanUp();  /* needed for forward reference */

enum colEnum mapFieldToEnum(char *f)
{
int i = 0;
for(i=0;i<MAXCOLS;i++)
    {
    if (sameString(colName[i],f))
	{
	return (enum colEnum) i;
	}
    }
errAbort("Field not found in mapFieldToEnum: %s",f);
}



void replaceInStr(char *s, int ssize, char *t, char *r) 
/* Strings: Replace an occurrence of t in s with r.
 * size is sizeof(s)
 * note: s must have room to hold any expansion, as it happens in-place in s */
{
char *temp = replaceChars(s,t,r);
if (strlen(temp) >= ssize)
    {
    errAbort("html page buf size exceeded. strlen(temp)=%s, size of html= %s",strlen(temp),ssize);
    }
safef(s,ssize,"%s",temp);
freez(&temp);
}



void doMsg()
/* callable from htmShell */
{
printf("%s",msg);
cleanUp();
}



void parseList(char *s, char delim, int num, char *buf, int bufsize)
/* parse list to find nth element of delim-separated list in string
 * returns l if not found which points to the string end 0
 * otherwise returns offset in s where begins */
{
int n = 0;
int i = 0;
int l = strlen(s);
int j = 0;
char c;
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
		return;
		}
	    j++;
	    }
	buf[bufsize-1] = 0;  /* add term in case */
	return;
	}
    if (n>num)  /* should not happen */
	{
	buf[0] = 0;
	return;
	}
    while (s[i] != delim)
	{
	if (i > l)
	    {
	    buf[0] = 0;
	    return;
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



void initColsFromString(char *s)
{
int i = 0;
char tempVal[256];
while(TRUE)
    { 
    parseList(s,',',i,tempVal,sizeof(tempVal));
    if (tempVal[0]==0) 
	{
	break;
	}
    colOrder[i] = mapFieldToEnum(tempVal);
    i++;
    }
numColumns=i;
}



/* -------------------- Push Queue ----------------------- */

void replacePushQFields(struct pushQ *ki)
/* fill in dummy html tags with values from the sql pushQ record */
{

char tempSizeMB[256];

safef(tempSizeMB, sizeof(tempSizeMB), "%u",ki->sizeMB);
if (ki->sizeMB == 0) 
    {
    strcpy(tempSizeMB,"");
    }

replaceInStr(html, sizeof(html) , "<!qid>"         , ki->qid       ); 
replaceSelectOptions("priority" , "A,B,C,D,L"      , ki->priority  );
replaceInStr(html, sizeof(html) , "<!qadate>"      , ki->qadate    ); 
replaceSelectOptions("newYN"    ,"Y,N"             , ki->newYN     );
replaceInStr(html, sizeof(html) , "<!track>"       , ki->track     ); 
replaceInStr(html, sizeof(html) , "<!dbs>"         , ki->dbs       ); 
replaceInStr(html, sizeof(html) , "<!tbls>"        , ki->tbls      ); 
replaceInStr(html, sizeof(html) , "<!cgis>"        , ki->cgis      ); 
replaceInStr(html, sizeof(html) , "<!files>"       , ki->files     ); 
replaceInStr(html, sizeof(html) , "<!sizeMB>"      , tempSizeMB    ); 
replaceInStr(html, sizeof(html) , "<!currLoc>"     , ki->currLoc   ); 
replaceSelectOptions("currLoc"  , "hgwdev,hgwbeta" , ki->currLoc   );
replaceSelectOptions("makeDocYN", "Y,N"            , ki->makeDocYN );
replaceInStr(html, sizeof(html) , "<!onlineHelp>"  , ki->onlineHelp); 
replaceSelectOptions("ndxYN"    , "Y,N"            , ki->ndxYN     );
replaceSelectOptions("joinerYN" , "Y,N"            , ki->joinerYN  );
replaceInStr(html, sizeof(html) , "<!stat>"        , ki->stat      ); 
replaceInStr(html, sizeof(html) , "<!sponsor>"     , ki->sponsor   ); 
replaceInStr(html, sizeof(html) , "<!reviewer>"    , ki->reviewer  ); 
replaceInStr(html, sizeof(html) , "<!extSource>"   , ki->extSource ); 
replaceInStr(html, sizeof(html) , "<!openIssues>"  , ki->openIssues); 
replaceInStr(html, sizeof(html) , "<!notes>"       , ki->notes     );
/*
replaceInStr(html, sizeof(html), "<!reqdate>"   , ki->reqdate); 
replaceSelectOptions("pushedYN", "Y,N",  ki->pushedYN);
*/
}


void doAdd()
/* handle setup add form for new pushQ record */
{

struct pushQ q;
ZeroVar(&q);

q.next = NULL;
strcpy(q.qid       ,"");
strcpy(q.priority  ,"A");  /* default priority */
strftime (q.qadate, sizeof(q.qadate), "%Y-%m-%d", loctime); /* default to today's date */
strcpy(q.newYN     ,"N");  /* default to not new track */
q.track  = ""; 
q.dbs    = ""; 
q.tbls   = "";
q.cgis   = "";
q.files  = "";
q.sizeMB = 0;
strcpy(q.currLoc   ,"hgwdev");  /* default loc */
strcpy(q.makeDocYN ,"N");
strcpy(q.onlineHelp,"");
strcpy(q.ndxYN     ,"N");  /* default to not checked yet */
strcpy(q.joinerYN  ,"N");  /* default to all.joiner not checked yet */
q.stat   = "";
strcpy(q.sponsor   ,"");
strcpy(q.reviewer  ,qaUser);   /* default to this user */
strcpy(q.extSource ,"");
q.openIssues   = "";
q.notes   = "";
 
safef(html,BUFMAX,formQ); 
replacePushQFields(&q);

replaceInStr(html, sizeof(html), "<!delbutton>", ""); 
replaceInStr(html, sizeof(html), "<!pushbutton>", ""); 
replaceInStr(html, sizeof(html), "<!clonebutton>", ""); 
 
printf("%s",html);
cleanUp();
 
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


void drawDisplayLine(enum colEnum col, struct pushQ *ki)
{
char url[256];


switch(col)
    {
    case e_qid:
	printf("<td><A href=\"/cgi-bin/qaPushQ?action=edit&qid=%s\">%s</A>",
	    ki->qid, ki->qid);
	if ((!sameString(ki->reqdate,"")) && (ki->pushedYN[0]=='N'))
	    {
	    printf("<BR><A href=\"/cgi-bin/qaPushQ?action=pushDone&qid=%s\">Done!</A>",
		ki->qid );
	    }
	printf("</td>\n");
	break;

    case e_pqid:
	printf("<td>%s</td>\n", ki->pqid   );
	break;
	
	
    case e_priority:
	printf("<td>%s",ki->priority);
	if (ki->priority[0] != 'L')
	{
	printf("&nbsp;&nbsp;"
	    "<A href=\"/cgi-bin/qaPushQ?action=promote&qid=%s\">^</A>&nbsp;&nbsp;"
	    "<A href=\"/cgi-bin/qaPushQ?action=demote&qid=%s\" >v</A>", 
	    ki->qid, ki->qid);
	}
	printf("</td>\n");
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
	printf("<td>%s</td>\n", ki->track     );
	break;
	
    case e_dbs:
	printf("<td>%s</td>\n", ki->dbs       );
	break;
	
    case e_tbls:
	dotdotdot(ki->tbls,MAXBLOBSHOW);  /* longblob */
	printf("<td>%s</td>\n", ki->tbls      );
	break;
	
    case e_cgis:
	printf("<td>%s</td>\n", ki->cgis      );
	break;
	
    case e_files:
	printf("<td>%s</td>\n", ki->files     );
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
	printf("<td>%s</td>\n", ki->stat      );
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
	
    case e_reqdate:
	printf("<td>%s</td>\n", ki->reqdate   );
	break;
	
    case e_pushedYN:
	safef(url, sizeof(url), ki->pushedYN); 
	if ((!sameString(ki->reqdate,"")) && (ki->pushedYN[0]=='N'))
	    {
	    safef(url, sizeof(url), 
		"<A href=\"/cgi-bin/qaPushQ?action=pushDone&qid=%s\">%s</A>", 
		ki->qid, ki->pushedYN );
	    }
	printf("<td>%s</td>\n", url);
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
char *month = cgiUsualString("month","");
char monthsql[256];

/* initialize column display order */
initColsFromString(showColumns);

safef(monthsql,sizeof(monthsql),"");
if (!sameString(month,""))
    {
    safef(monthsql,sizeof(monthsql),"where priority='L' and qadate like '%s%%' ",month);
    }

/* Get a list of all (or in month). */
safef(query, sizeof(query), "%s%s%s", 
    "select * from pushQ ",
    monthsql,
    "order by priority,rank,qid desc limit 100"
    );

//debug printf("query=%s",query);

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
    printf("<p style=\"color:red\">Machine: %s THIS IS NOT THE REAL PUSHQ- GO TO HGWBETA </p>\n",utsName.nodename);
    }

if (sameString(month,""))
    {
    printf("&nbsp;<A href=/cgi-bin/qaPushQ?action=add>ADD</A>\n");
    }
else
    {
    printf("&nbsp;<A href=/cgi-bin/qaPushQ?action=display>Current</A>\n");
    }
printf("&nbsp;<A href=/cgi-bin/qaPushQ?action=reset>Logout</A>\n");
printf("&nbsp;<A href=/cgi-bin/qaPushQ?action=showAllCol>All Columns</A>\n");
printf("&nbsp;<A href=/cgi-bin/qaPushQ?action=showDefaultCol>Default Columns</A>\n");
printf("&nbsp;<A href=/cgi-bin/qaPushQ?action=showMonths>Log by Month</A>\n");

/* draw table header */


printf("<H2>Track Push Queue</H2>\n");

printf("<TABLE BORDER CELLSPACING=0 CELLPADDING=5>\n");
printf("  <TR>\n");

for (c=0; c<numColumns; c++)
    {
    printf("    <TH>"
	"<a href=qaPushQ?action=promoteColumn&col=%s>&lt;</a>&nbsp;"
	"<a href=qaPushQ?action=hideColumn&col=%s>!</a>&nbsp;"
	"<a href=qaPushQ?action=demoteColumn&col=%s>&gt;</a>"
	"<br></TH>\n",
	colName[colOrder[c]], 
	colName[colOrder[c]], 
	colName[colOrder[c]] 
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
	printf("<tr>");
	printf("<td><h1>%s</h1></td>\n", ki->priority );
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
cleanUp();

}


boolean loadPushQ(char *qid, struct pushQ *q, boolean optional)
/* we need to load q with existing values */
{
char **row;
struct sqlResult *sr;
char query[256];

safef(query, sizeof(query), "select * from pushQ where qid = '%s'",qid);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL)
    {
    if (!optional)
	{
	errAbort("%s not found.",qid);
	}
    else 
	{
	return FALSE;
	}
    }
else
    {
    pushQStaticLoad(row,q);
    }
sqlFreeResult(&sr);
return TRUE;
}


void doPushDone()
/* Mark record pushedYN=Y, move priority to L for Log, and set rank=0  */
{

struct pushQ q; /* just so we get the size of q.qid */
char query[256];
char *tbl = "pushQ";
char *meta = ""
"<head>"
"<title>Meta Redirect Code</title>"
"<meta http-equiv=\"refresh\" content=\"0;url=/cgi-bin/qaPushQ?action=display\">"
"</head>";


safef(q.qid, sizeof(q.qid), cgiString("qid"));

loadPushQ(q.qid, &q, FALSE);

safef(query, sizeof(query), 
    "update %s set rank = 0, priority ='L', pushedYN='Y' where qid = '%s' ", 
    tbl, q.qid);
sqlUpdate(conn, query);


/* first close the hole where it was */
safef(query, sizeof(query), 
    "update %s set rank = rank - 1 where priority ='%s' and rank > %d ", 
tbl, q.priority, q.rank);
sqlUpdate(conn, query);


sprintf(msg,"Record completed, qid %s moved to log.<br>\n",q.qid);

htmShellWithHead(TITLE, meta, doMsg, NULL);

}


void doPromote(int change)
/* Promote the ranking of this Q item 
 * >0 means promote, <0 means demote */
{

char *tbl = "pushQ";
char **row;
struct sqlResult *sr;

struct pushQ q;
char query[256];
char newQid[sizeof(q.qid)] = "";

char *meta = ""
"<head>"
"<title>Meta Redirect Code</title>"
"<meta http-equiv=\"refresh\" content=\"0;url=/cgi-bin/qaPushQ?action=display\">"
"</head>";

safef(newQid, sizeof(newQid), cgiString("qid"));

loadPushQ(newQid, &q,FALSE);

if ((q.rank > 1) && (change>0))
    {
    /* swap places with rank-1 */
    safef(query, sizeof(query), 
    "update %s set rank = rank + 1 where priority ='%s' and rank = %d ", 
    tbl, q.priority, q.rank-1);
    sqlUpdate(conn, query);
    q.rank--;
    safef(query, sizeof(query), 
    "update %s set rank = %d where qid ='%s'", 
    tbl, q.rank, q.qid);
    sqlUpdate(conn, query);
    }

if (change<0)
    {
    /* swap places with rank+1 */
    safef(query, sizeof(query), 
    "update %s set rank = rank - 1 where priority ='%s' and rank = %d ", 
    tbl, q.priority, q.rank+1);
    if (sqlUpdateRows(conn, query, NULL)>0)
    {
    q.rank++;
    safef(query, sizeof(query), 
	"update %s set rank = %d where qid ='%s'", 
	tbl, q.rank, q.qid);
    sqlUpdate(conn, query);
    }
    }

sprintf(msg,"Record moved qid = %s.<br>\n",q.qid);
htmShellWithHead(TITLE, meta, doMsg, NULL);

}



int getNextAvailQid()
/* adding new pushQ rec, get next available qid number */
{
struct pushQ q;
int newqid = 0;
char query[256];
char *tbl = "pushQ";
char *quickres = NULL;
safef(query, sizeof(query), "select qid from %s order by qid desc limit 1",tbl);
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
int newqid = 0;
char query[256];
char *tbl = "pushQ";
char *quickres = NULL;
safef(query, sizeof(query), 
    "select rank from %s where priority='%s' order by rank desc limit 1",
    tbl, priority);
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
char  *qid, *pqid, *priority, *qadate, *newYN, *track, *dbs, *tbls, *cgis, *files, *currLoc, *makeDocYN, *onlineHelp, *ndxYN, *joinerYN, *stat, *sponsor, *reviewer, *extSource, *openIssues, *notes, *reqdate, *pushedYN;
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
sponsor = sqlEscapeString(el->sponsor);
reviewer = sqlEscapeString(el->reviewer);
extSource = sqlEscapeString(el->extSource);
openIssues = sqlEscapeString(el->openIssues);
notes = sqlEscapeString(el->notes);
reqdate = sqlEscapeString(el->reqdate);
pushedYN = sqlEscapeString(el->pushedYN);

dyStringPrintf(update, 
"update %s set "
"pqid='%s',priority='%s',rank=%u,qadate='%s',newYN='%s',"
"track='%s',dbs='%s',tbls='%s',cgis='%s',files='%s',sizeMB=%u,currLoc='%s',"
"makeDocYN='%s',onlineHelp='%s',ndxYN='%s',joinerYN='%s',stat='%s',"
"sponsor='%s',reviewer='%s',extSource='%s',"
"openIssues='%s',notes='%s',reqdate='%s',pushedYN='%s' "
"where qid='%s'", 
	tableName,  
	pqid,  priority, el->rank,  qadate, newYN, track, dbs, 
	tbls,  cgis,  files, el->sizeMB ,  currLoc,  makeDocYN,  
	onlineHelp,  ndxYN,  joinerYN,  stat,  
	sponsor,  reviewer,  extSource,  
	openIssues,  notes,  reqdate,  pushedYN,
	qid
	);

//debug
//safef(msg, sizeof(msg), update->string);
//htmShell(TITLE, doMsg, NULL);
//exit(0);

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
freez(&reqdate);
freez(&pushedYN);
}




void doPost()
/* handle the  post (really just a get for now) from Add or Edit of a pushQ record */
{


char *tbl = "pushQ";
int updateSize = 2456;
int newqid = 0;

char query[256];
char *delbutton   = cgiUsualString("delbutton"  ,"");
char *pushbutton  = cgiUsualString("pushbutton" ,"");
char *clonebutton = cgiUsualString("clonebutton","");

char *meta = ""
"<head>"
"<title>Meta Redirect Code</title>"
"<meta http-equiv=\"refresh\" content=\"0;url={url}\">"
"</head>";

char **row;
struct sqlResult *sr;

struct pushQ q;

char newQid     [sizeof(q.qid)]      = "";
char newPriority[sizeof(q.priority)] = "";

struct dyString *url = NULL;

ZeroVar(&q);

/* example ALTERNATIVE method
struct pushQ *q;
AllocVar(q);
 ....
freez(&q);

*/

q.next = NULL;


/* check for things too big- this could be improved  */
if (strlen(cgiString("track"))>255)
    {
    safef(msg,sizeof(msg),"Track: too long for field, 255 char max. <br>\n");
    htmShell(TITLE, doMsg, NULL);
    return;
    }
if (strlen(cgiString("dbs"))>255)
    {
    safef(msg,sizeof(msg),"Database: too long for field, 255 char max. <br>\n");
    htmShell(TITLE, doMsg, NULL);
    return;
    }
if (strlen(cgiString("cgis"))>255)
    {
    safef(msg,sizeof(msg),"CGIs: too long for field, 255 char max. <br>\n");
    htmShell(TITLE, doMsg, NULL);
    return;
    }
if (strlen(cgiString("files"))>255)
    {
    safef(msg,sizeof(msg),"Files: too long for field, 255 char max. <br>\n");
    htmShell(TITLE, doMsg, NULL);
    return;
    }
if (strlen(cgiString("stat"))>255)
    {
    safef(msg,sizeof(msg),"Status: too long for field, 255 char max. <br>\n");
    htmShell(TITLE, doMsg, NULL);
    return;
    }


safef(newQid,      sizeof(newQid)     , cgiString("qid"));

safef(newPriority, sizeof(newPriority), cgiString("priority"));


if (sameString(newQid,"")) 
    {
    newqid = getNextAvailQid();
    safef(q.pqid, sizeof(q.pqid), "");
    strcpy(q.reqdate   ,"" ); 
    strcpy(q.pushedYN  ,"N");  /* default to: push not done yet */
    }
else
    {
    /* we need to preload q with existing values 
     * because some fields like rank are not carried in form */
    if (!loadPushQ(newQid, &q,TRUE))  
	/* true means optional, it was asked if we could tolerate this, 
	 *  e.g. delete, then hit back-button */
	{
	safef(q.qid, sizeof(q.qid), newQid);
	}
    }

    
/* check if priority class has changed, or deleted, then close ranks */
if ( (!sameString(newPriority,q.priority)) || (sameString(delbutton,"delete")) )
    {
    /* first close the hole where it was */
    safef(query, sizeof(query), 
    "update %s set rank = rank - 1 where priority ='%s' and rank > %d ", 
    tbl, q.priority, q.rank);
    sqlUpdate(conn, query);
    }

/* if not deleted, then if new or priority class change, then take last rank */
if (!sameString(delbutton,"delete")) 
    {
    if ((!sameString(newPriority,q.priority)) || (sameString(newQid,"")))
	{
	q.rank = getNextAvailRank(newPriority);
	safef(q.priority, sizeof(q.priority), newPriority);
	safef(newQid,     sizeof(newQid)    , cgiString("qid"));
	}
    }

if (q.priority[0]=='L') 
    {
    q.rank = 0;
    }


safef(q.qadate    , sizeof(q.qadate    ), cgiString("qadate"    ));
safef(q.newYN     , sizeof(q.newYN     ), cgiString("newYN"     ));
q.track = needMem(SSSZ);
safef(q.track     , SSSZ                , cgiString("track"     ));
q.dbs   = needMem(SSSZ);
safef(q.dbs       , SSSZ                , cgiString("dbs"       ));

q.tbls  = cgiString("tbls");   /* blob, just leave alone for now! */

q.cgis  = needMem(SSSZ);
safef(q.cgis      , SSSZ                , cgiString("cgis"      ));
q.files = needMem(SSSZ);
safef(q.files     , SSSZ                , cgiString("files"     ));
if (sscanf(cgiString("sizeMB"),"%u",&q.sizeMB) != 1) 
    {
    q.sizeMB = 0;
    }
safef(q.currLoc   , sizeof(q.currLoc   ), cgiString("currLoc"   ));
safef(q.makeDocYN , sizeof(q.makeDocYN ), cgiString("makeDocYN" ));
safef(q.onlineHelp, sizeof(q.onlineHelp), cgiString("onlineHelp"));
safef(q.ndxYN     , sizeof(q.ndxYN     ), cgiString("ndxYN"     ));
safef(q.joinerYN  , sizeof(q.joinerYN  ), cgiString("joinerYN"  ));
q.stat  = needMem(SSSZ);
safef(q.stat      , SSSZ                , cgiString("stat"      ));
safef(q.sponsor   , sizeof(q.sponsor   ), cgiString("sponsor"   ));
safef(q.reviewer  , sizeof(q.reviewer  ), cgiString("reviewer"  ));
safef(q.extSource , sizeof(q.extSource ), cgiString("extSource" ));

q.openIssues = cgiString("openIssues");
q.notes      = cgiString("notes"     );

/*
safef(q.reqdate   , sizeof(q.reqdate   ), cgiString("reqdate"   ));
safef(q.pushedYN  , sizeof(q.pushedYN  ), cgiString("pushedYN"  ));
*/

/* debug!
safef(msg, sizeof(msg), "Got to past loading q.qid priority data. %d %d <br>\n",newqid,sizeof(newQid));
htmShell(TITLE, doMsg, NULL);
return;
*/


/* need to do this before delete or will lose the record */
if ((sameString(pushbutton,"push requested"))&&(q.sizeMB==0))
    {
    safef(msg,sizeof(msg),"Size (MB) should not be zero. <br>\n");
    htmShell(TITLE, doMsg, NULL);
    return;
    }

if ((sameString(pushbutton,"push requested"))&&(!sameString(q.currLoc,"hgwbeta")))
    {
    safef(msg,sizeof(msg),"Current Location should be hgwbeta. <br>\n");
    htmShell(TITLE, doMsg, NULL);
    return;
    }

if ((sameString(pushbutton,"push requested"))&&(!sameString(q.makeDocYN,"Y")))
    {
    safef(msg,sizeof(msg),"MakeDoc not verified. <br>\n");
    htmShell(TITLE, doMsg, NULL);
    return;
    }

if ((sameString(pushbutton,"push requested"))&&(!sameString(q.ndxYN,"Y")))
    {
    safef(msg,sizeof(msg),"Index not verified. <br>\n");
    htmShell(TITLE, doMsg, NULL);
    return;
    }

if ((sameString(pushbutton,"push requested"))&&(!sameString(q.joinerYN,"Y")))
    {
    safef(msg,sizeof(msg),"All.Joiner not verified. <br>\n");
    htmShell(TITLE, doMsg, NULL);
    return;
    }

if ((sameString(clonebutton,"clone"))&&(sameString(q.priority,"L")))
    {
    safef(msg,sizeof(msg),"Log records should not be cloned. <br>\n");
    htmShell(TITLE, doMsg, NULL);
    return;
    }

if (sameString(pushbutton,"push requested")) 
    {
    /* set to today's date */
    strftime (q.reqdate, sizeof(q.reqdate), "%Y-%m-%d", loctime); 
    /* reset pushedYN in case was prev. a log already */
    safef(q.pushedYN,sizeof(q.pushedYN),"N");
    }

if (sameString(delbutton,"delete")) 
    {
    /* delete old record */
    safef(query, sizeof(query), "delete from %s where qid ='%s'", tbl, q.qid);
    sqlUpdate(conn, query);
    }
else if (sameString(newQid,""))
    {
    /* save new record */
    safef(msg, sizeof(msg), "%%0%dd", sizeof(q.qid)-1);
    safef(newQid,sizeof(newQid),msg,newqid);
    safef(q.qid, sizeof(q.qid), newQid);
    pushQSaveToDbEscaped(conn, &q, tbl, updateSize);
    }
else
    {  
    /* update existing record */
    pushQUpdateEscaped(conn, &q, tbl, updateSize);
    }


if (sameString(clonebutton,"clone")) 
    {  
    /* save new clone */
    safef(q.pqid,sizeof(q.pqid), q.qid);  /* daughter will point to parent */
    newqid = getNextAvailQid();
    safef(msg, sizeof(msg), "%%0%dd", sizeof(q.qid)-1);
    safef(newQid,sizeof(newQid),msg,newqid);
    safef(q.qid, sizeof(q.qid), newQid);
    q.rank = getNextAvailRank(q.priority);
    strcpy(q.reqdate   ,"" ); 
    strcpy(q.pushedYN  ,"N");  /* default to: push not done yet */
    pushQSaveToDbEscaped(conn, &q, tbl, updateSize);
    }


url = newDyString(2048);  /* need room for 5 fields cgi encoded */ 

dyStringAppend(url, "/cgi-bin/qaPushQ");  

meta = replaceChars(meta, "{url}", url->string);

freeDyString(&url);

safef(msg,sizeof(msg),"Record updated qid = %s.<br>\n",q.qid);
htmShellWithHead(TITLE, meta, doMsg, NULL);

/* free memory */
freez(&q.track);
freez(&q.dbs  );
freez(&q.tbls );
freez(&q.stat );

freez(&meta);

}



void doEdit()
/* Handle edit request for a pushQ entry */
{

struct pushQ targ;
struct pushQ *ki, *kiList = NULL;
char query[256];

ZeroVar(&targ);

safef(targ.qid, sizeof(targ.qid), cgiString("qid"));

/* Get a list of all that have that ID. */
safef(query, sizeof(query), "select * from pushQ where qid = '%s'",targ.qid);
kiList=pushQLoadByQuery(conn, query);

if (kiList == NULL) 
    {
    printf("%s not found.", targ.qid);
    return;
    }

if (kiList->next != NULL) 
    {
    printf("More than one record with queue id = %s found, but it should be unique.",targ.qid);
    return;
    }

ki = kiList;

strcpy(html,formQ); 
replacePushQFields(ki);

replaceInStr(html, sizeof(html), 
    "<!delbutton>" , 
    "<input TYPE=SUBMIT NAME=\"delbutton\"  VALUE=\"delete\">"
    );
    
if (ki->priority[0]!='L')
    {
    replaceInStr(html, sizeof(html), 
    "<!pushbutton>", 
    "<input TYPE=SUBMIT NAME=\"pushbutton\" VALUE=\"push requested\">"
    ); 
    }
    
if (ki->priority[0]!='L')
    {
    replaceInStr(html, sizeof(html), 
    "<!clonebutton>", 
    "<input TYPE=SUBMIT NAME=\"clonebutton\" VALUE=\"clone\">"
    ); 
    }
    
printf("%s",html);

pushQFreeList(&kiList);
cleanUp();

}


void doCookieReset()
/* reset cookie, will cause new login next time */
{
char *meta = ""
"<head>"
"<title>Meta Redirect Code</title>"
"<meta http-equiv=\"refresh\" content=\"0;url=/cgi-bin/qaPushQ?action=display\">"
"</head>";

htmlSetCookie("qapushq", "", NULL, NULL, NULL, FALSE);

safef(msg,sizeof(msg),"Cookie reset.<br>\n");
htmShellWithHead(TITLE, meta, doMsg, NULL);

}


void doLogin()
/* make form for login */
{
printf("<FORM ACTION=\"/cgi-bin/qaPushQ\" NAME=\"loginForm\" METHOD=\"GET\">\n");

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
printf("<INPUT TYPE=password NAME=password size=8 value=\"\" >\n");
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
printf("</FORM>\n");
cleanUp();
}




boolean readAUser(struct users *u, boolean optional)
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



void doPostLogin()
/* process Login post */
{

char *tbl = "users";
char query[256];
char *meta = ""
"<head>"
"<title>Meta Redirect Code</title>"
"<meta http-equiv=\"refresh\" content=\"0;url={url}\">"
"</head>";

char **row;
struct sqlResult *sr;
struct users u;

char *password = cgiString("password");

int updateSize = 2456;  /* this is probably a bit large but ok, is hint to alloc ram */

struct dyString *url = newDyString(2048);  /* need room for 5 fields cgi encoded */ 

ZeroVar(&u);

dyStringAppend(url, "/cgi-bin/qaPushQ");  

u.next = NULL;
safef(u.user,      sizeof(u.user)     , cgiString("user"));

if (!readAUser(&u, TRUE))
    {
    /* unknown user not allowed */
    safef(msg,sizeof(msg),"Invalid user or password.");
    dyStringAppend(url, "?action=login");  
    }
else
    {
    if (strlen(u.password)==0) 
	{ /* if pwd in db is blank, use this as their new password and encrypt it and save in db. */
	if (strlen(password) < 6)
	    { /* bad pwd */
	    safef(msg,sizeof(msg),"Invalid password. Password must be at least 6 characters long.");
	    dyStringAppend(url, "?action=login");  
	    }
	else
	    {
	    encryptNewPWD(password, u.password, sizeof(u.password));  
	    safef(query, sizeof(query), 
		"update %s set password = '%s' where user = '%s' ", 
		tbl, u.password, u.user);
	    sqlUpdate(conn, query);
	    htmlSetCookie("qapushq", u.user, NULL, NULL, NULL, FALSE);
	    }
	}
    else
	{ /* verify password matches db */
	if (checkPWD(password, u.password)) 
	    { /* good pwd, save user in cookie */
	    htmlSetCookie("qapushq", u.user, NULL, NULL, NULL, FALSE);
	    safef(msg,sizeof(msg),"Login successful.<br>\n");
	    }
	else
	    { /* bad pwd */
	    safef(msg,sizeof(msg),"Invalid user or password.");
	    dyStringAppend(url, "?action=login");  
	    }
	}
    }

meta = replaceChars(meta, "{url}", url->string);

freeDyString(&url);

htmShellWithHead(TITLE, meta, doMsg, NULL);

/* free memory */

freez(&meta);

}



void readMyUser()
/* read data for my user */
{
int i = 1;  /* because it should start right off with ? */
char tempVar[2048];
char tempVarName[256];
char tempVal[2048];

safef(myUser.user,sizeof(myUser.user),qaUser);
readAUser(&myUser, FALSE);

while(TRUE)
    { 
    parseList(myUser.contents,'?',i,tempVar,sizeof(tempVar));
    if (tempVar[0]==0) 
	{
	break;
	}

    parseList(tempVar,'=',0,tempVarName,sizeof(tempVarName));
   
    parseList(tempVar,'=',1,tempVal,sizeof(tempVal));
   
    if (sameString(tempVarName,"showColumns"))
	{
	freeMem(showColumns);
	showColumns = needMem(strlen(tempVal)+1);
	safef(showColumns, strlen(tempVal)+1, tempVal);
	}
    // debug
    //safef(msg,sizeof(msg),"tempVar=%s<br>\n tempVarName=%s<br>\n tempVal=%s<br>\n showColumns=%s<br>\n",tempVar,tempVarName,tempVal,showColumns);
    //htmShell(TITLE, doMsg, NULL);
    //exit(0);

    i++;
    }
}


void saveMyUser()
/* read data for my user */
{
char *tbl = "users";
char query[256];
struct dyString * newcontents = newDyString(2048);  /* need room */

dyStringPrintf(newcontents, "?showColumns=%s", showColumns);

freeMem(&myUser.contents);
myUser.contents = newcontents->string;
safef(query, sizeof(query), 
    "update %s set contents = '%s' where user = '%s' ", 
    tbl, myUser.contents, myUser.user);
sqlUpdate(conn, query);
freeDyString(&newcontents);
}




void doPromoteColumn(int change)
/* Promote the column 
 * 1 = promote, 0 = hide, -1 = demote */
{
char target[256] = "";

char *meta = ""
"<head>"
"<title>Meta Redirect Code</title>"
"<meta http-equiv=\"refresh\" content=\"0;url=/cgi-bin/qaPushQ?action=display\">"
"</head>";

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

safef(showColumns, 2048, "%s", s->string);
freeDyString(&s);

showColumns[strlen(showColumns)-1]=0;  /* chop off trailing comma */

//saveMyUser();

safef(msg,sizeof(msg),"Column changed = %s.<br>\n",target);
htmShellWithHead(TITLE, meta, doMsg, NULL);

}


void doShowAllColumns()
/* Promote the ranking of this Q item 
 * >0 means promote, <0 means demote */
{
int c = 0;
struct dyString * s = newDyString(2048);  /* need room */
char *meta = ""
"<head>"
"<title>Meta Redirect Code</title>"
"<meta http-equiv=\"refresh\" content=\"0;url=/cgi-bin/qaPushQ?action=display\">"
"</head>";


dyStringAppend(s, showColumns);

for (c=0; c<MAXCOLS; c++)
    {
    if (strstr(showColumns,colName[c])==NULL)
	{
	dyStringPrintf(s, ",%s", colName[c]);
	}
    }

/* Note: I was getting an error until I reallocated a new string, 
 *  but it makes no sense unless memory corruption was being detected ? */
showColumns = needMem(2048);
safef(showColumns, 2048, "%s", s->string);
freeDyString(&s);

//saveMyUser();

safef(msg,sizeof(msg),"All Columns now visible.<br>\n");
htmShellWithHead(TITLE, meta, doMsg, NULL);

}



void doShowDefaultColumns()
/* Promote the ranking of this Q item 
 * >0 means promote, <0 means demote */
{
int c = 0;
struct dyString * s = newDyString(2048);  /* need room */
char *meta = ""
"<head>"
"<title>Meta Redirect Code</title>"
"<meta http-equiv=\"refresh\" content=\"0;url=/cgi-bin/qaPushQ?action=display\">"
"</head>";


dyStringAppend(s, defaultColumns);

showColumns = needMem(2048);
safef(showColumns, 2048, "%s", s->string);
freeDyString(&s);

//saveMyUser();

safef(msg,sizeof(msg),"Default columns now visible.<br>\n");
htmShellWithHead(TITLE, meta, doMsg, NULL);

}



void doShowMonths()
/* This gives the user a choice of months to filter on */
{

struct sqlResult *sr;
char **row;
char query[256];

printf("<A href=qaPushQ?action=display>Current</A><br>\n");

safef(query, sizeof(query), "select distinct substring(qadate,1,7) from pushQ where priority='L' order by qadate desc");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    printf("<A href=qaPushQ?action=display&month=%s>%s</A><br>\n",row[0],row[0]);
    }
sqlFreeResult(&sr);
cleanUp();
}



void cleanUp()
/* save anything needing it, release resources */
{
saveMyUser();
sqlDisconnect(&conn);
releaseLock();
}


/* ------------------------------------------------------- */





int main(int argc, char *argv[], char *env[])
/* Process command line if any. */
{

char *action = NULL;   /* have to put declarations first */

if (!cgiIsOnWeb())
    {
    warn("This is a CGI script - attempting to fake environment from command line");
    cgiSpoof(&argc, argv);
    }

ZeroVar(&myUser);

saveEnv = env;

/* Get the current time. */
curtime = time (NULL);
/* Convert it to local time representation. */
loctime = localtime (&curtime);

uname(&utsName);

database = cfgOption("pq.db"      );
host     = cfgOption("pq.host"    );
user     = cfgOption("pq.user"    );
password = cfgOption("pq.password");


setLock();

/* debug 
safef(msg,sizeof(msg),"db='%s', host='%s', user='%s', password='%s' <br>\n",database,host,user,password);
htmShell("Push Queue debug", doMsg, NULL);
return 0;
*/

conn = sqlConnectRemote(host, user, password, database);


qaUser = findCookieData("qapushq");  /* will also cause internal structures to load cookie data */


action = cgiUsualString("action","display");  /* get action, defaults to display of push queue */
/* initCgiInput() is not exported in cheapcgi.h, but it should get called by cgiUsualString
So it will find all input regardless of Get/Put/Post/etc and make available as cgivars */


if (sameString(action,"postLogin")) 
    {
    doPostLogin();  /* will redirect back to display */
    return;
    }
    

if ((qaUser == NULL) || (sameString(qaUser,"")))
    {
    action = needMem(6);
    safef(action,6,"login");
    }

if (sameString(action,"login"))
    {
    htmShell(TITLE, doLogin, NULL);
    return;
    }


if (sameString(action,"reset")) 
    {
    doCookieReset();  /* will redirect back to display */
    return;
    }


/* default columns */
showColumns = needMem(2048);
safef(showColumns, 2048, defaultColumns);
    

readMyUser();


/*
htmShell(TITLE,dumpCookieList, NULL);
return 0;
*/



/* saveMyUser(); */

/* ---- Push Queue  ----- */

if (sameString(action,"display")) 
    {
    htmShell(TITLE, doDisplay, NULL);
    }

else if (sameString(action,"add")) 
    {
    htmShell(TITLE, doAdd, NULL);
    }

else if (sameString(action,"edit")) 
    {
    htmShell(TITLE, doEdit, NULL);
    }

else if (sameString(action,"post")) 
    {
    doPost(); /* suppress htmShell here because may redirect */
    }

else if (sameString(action,"promote")) 
    {
    doPromote(1);  /* htmShell not used here, will redirect back to display  */
    }

else if (sameString(action,"demote")) 
    {
    doPromote(-1);  /* htmShell not used here, will redirect back to display  */
    }

else if (sameString(action,"pushDone")) 
    {
    doPushDone();  /* htmShell not used here, will redirect back to display  */
    }
    
else if (sameString(action,"promoteColumn")) 
    {
    doPromoteColumn( 1 );  /* htmShell not used here, will redirect back to display  */
    }

else if (sameString(action,"hideColumn"   )) 
    {
    doPromoteColumn( 0 );  /* htmShell not used here, will redirect back to display  */
    }

else if (sameString(action,"demoteColumn" )) 
    {
    doPromoteColumn(-1 );  /* htmShell not used here, will redirect back to display  */
    }

else if (sameString(action,"showAllCol" )) 
    {
    doShowAllColumns();  /* htmShell not used here, will redirect back to display  */
    }

else if (sameString(action,"showDefaultCol" )) 
    {
    doShowDefaultColumns();  /* htmShell not used here, will redirect back to display  */
    }

else if (sameString(action,"showMonths" )) 
    {
    htmShell(TITLE, doShowMonths, NULL);
    }

else
    {
    safef(msg,sizeof(msg),"action='%s' is invalid! <br>\n",action);
    htmShell(TITLE, doMsg, NULL);
    }

/* it will never reach here since htmShell never returns :( */

return 0;

}

