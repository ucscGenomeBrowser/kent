/* hgLogin - Administer UCSC Genome Browser membership - signup, lost password, etc. */

#include "common.h"
#include "hash.h"
#include "obscure.h"
#include "hgConfig.h"
#include "cheapcgi.h"
#include "memalloc.h"
#include "jksql.h"
#include "htmshell.h"
#include "cart.h"
#include "hPrint.h"
#include "hdb.h"
#include "hui.h"
#include "web.h"
#include "ra.h"
#include "hgColors.h"
#include <crypt.h>

#include "net.h"

#include "hgLogin.h"
#include "gbMembers.h"

#include "versionInfo.h"
char msg[2048] = "";


char *excludeVars[] = { "submit", "Submit", "debug", "fixMembers", "update", "hgLogin_password", NULL };
/* The excludeVars are not saved to the cart. (We also exclude
 * any variables that start "near.do.") */

/* ---- Global variables. ---- */
struct cart *cart;	/* This holds cgi and other variables between clicks. */
char *database;		/* Name of genome database - hg15, mm3, or the like. */
struct hash *oldCart;	/* Old cart hash. */
char *errMsg;           /* Error message to show user when form data rejected */

/* -------- password functions ---- */

void encryptPWD(char *password, char *salt, char *buf, int bufsize)
/* encrypt a password */
{
/* encrypt user's password. */
safef(buf,bufsize,crypt(password, salt));
}


void encryptNewPwd(char *password, char *buf, int bufsize)
/* encrypt a new password */
/* XXXX TODO: use md5 in linked SSL */
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

bool checkPwd(char *password, char *encPassword)
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


boolean checkPwdCharClasses(char *password)
/* check that password uses at least 2 character classes */
{
/* [A-Z] [a-z] [0-9] [!@#$%^&*()] */
int classes[4]={0,0,0,0};
char c;
while ((c=*password++))
    {
    if (c >= 'A' && c <= 'Z') classes[0] = 1;
    if (c >= 'a' && c <= 'z') classes[1] = 1;
    if (c >= '0' && c <= '9') classes[2] = 1;
    if (strchr("!@#$%^&*()",c)) classes[3] = 1;
    }
return ((classes[0]+classes[1]+classes[2]+classes[3])>=2);
}

unsigned int randInt(unsigned int n)
/* little randome number helper returns 0 to n-1 */
{
return (unsigned int) n * (rand() / (RAND_MAX + 1.0));
}

char *generateRandomPassword()
/* Generate valid random password for users who have lost their old one.
 * Free the returned value.*/
{
char boundary[256];
char punc[] = "!@#$%^&*()";
/* choose a new string for the boundary */
/* Set initial seed */
int i = 0;
int r = 0;
char c = ' ';
boundary[0]=0;
srand( (unsigned)time( NULL ) );
for(i=0;i<8;++i)
    {
    r = randInt(4);
    switch (r)
	{
	case 0 :
    	    c = 'A' + randInt(26);
	    break;
	case 1 :
    	    c = 'a' + randInt(26);
	    break;
	case 2 :
    	    c = '0' + randInt(10);
	    break;
	default:
    	    c = punc[randInt(10)];
	    break;
    	}
    boundary[i] = c;
    }
boundary[i]=0;
return cloneString(boundary);
}


/* --- update passwords file ----- */

void updatePasswordsFile(struct sqlConnection *conn)
/* update the passwords file containing email:encryptedPassword */
{
struct sqlResult *sr;
char **row;

FILE *out = mustOpen("../conf/passwords", "w");

sr = sqlGetResult(conn,
"select email,password from gbMembers where activated='Y'"
" and (expireDate='' or (current_date() < expireDate))");
while ((row = sqlNextRow(sr)) != NULL)
    {
    fprintf(out,"%s:%s\n",row[0],row[1]);
    }
sqlFreeResult(&sr);

carefulClose(&out);

}




/* ---------- reverse DNS function --------- */

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

char *reverseDns(char *ip)
/* do reverse dns lookup on ip using getnamebyaddr,
 *  and then return a string to be freed that is the host */
{
struct hostent *hp;
struct sockaddr_in sock;
if (inet_aton(ip,&sock.sin_addr) == 0) return NULL;
hp = gethostbyaddr(&sock.sin_addr,sizeof(sock.sin_addr),AF_INET);
if (!hp) return NULL;
return cloneString(hp->h_name);
}


/* -------- functions ---- */

void debugShowAllMembers(struct sqlConnection *conn)
/* display all gbMembers */
{
struct sqlResult *sr;
char **row;

hPrintf("<h1>Members</h1>");
hPrintf("<table>");
hPrintf("<th>email</th><th>password</th>");

sr = sqlGetResult(conn, "select * from members");
while ((row = sqlNextRow(sr)) != NULL)
    {
    hPrintf("<tr><td>%s</td><td>%s</td></tr>",row[0],row[1]);
    }
sqlFreeResult(&sr);

hPrintf("</table>");
}



void lostPasswordPage(struct sqlConnection *conn)
/* draw the lost password page */
{
hPrintf(
"<h2>UCSC Genome Browser</h2>"
"<p align=\"left\">"
"</p>"
"<span style='color:red;'>%s</span>"
"<h3>Send Me A New Password</h3>"
"<form method=post action=\"hgLogin\" name=lostPasswordForm >"
"<table>"
"<tr><td>E-mail</td><td><input type=text name=hgLogin_email size=20> "
  "(your e-mail is also your user-id)</td></tr>"
"<tr><td>&nbsp;</td><td><input type=submit name=hgLogin.do.lostPassword value=submit>"
"&nbsp;<input type=submit name=hgLogin.do.signupPage value=cancel></td></tr>"
"</table>"
"<br>"
, errMsg ? errMsg : ""
);

cartSaveSession(cart);

hPrintf("</FORM>");

}


void lostPassword(struct sqlConnection *conn)
/* process the lost password form */
{
char query[256];
char cmd[256];
char *email = cartUsualString(cart, "hgLogin_email", "");
if (!email || sameString(email,""))
    {
    freez(&errMsg);
    errMsg = cloneString("Email cannot be blank.");
    lostPasswordPage(conn);
    return;
    }
safef(query,sizeof(query), "select password from gbMembers where email='%s'", email);
char *password = sqlQuickString(conn, query);
if (!password)
    {
    freez(&errMsg);
    errMsg = cloneString("Email not found.");
    lostPasswordPage(conn);
    return;
    }
freez(&password);
password = generateRandomPassword();
char encPwd[35] = "";
encryptNewPwd(password, encPwd, sizeof(encPwd));

safef(query,sizeof(query), "update gbMembers set password='%s' where email='%s'", sqlEscapeString(encPwd), sqlEscapeString(email));
sqlUpdate(conn, query);

updatePasswordsFile(conn);

safef(cmd,sizeof(cmd),
"echo 'Your new password is: %s' | mail -s \"Lost GSID HIV password\" %s"
, password, email);
int result = system(cmd);
if (result == -1)
    {
    hPrintf(
    "<h2>UCSC Genome Browser</h2>"
    "<p align=\"left\">"
    "</p>"
    "<h3>Error emailing password to: %s</h3>"
    "Click <a href=hgLogin?hgLogin.do.signupPage=1>here</a> to return.<br>"
    , email
    );
    }
else
    {
    hPrintf(
    "<h2>UCSC Genome Browser</h2>"
    "<p align=\"left\">"
    "</p>"
    "<h3>Password has been emailed to: %s</h3>"
    "Click <a href=hgLogin?hgLogin.do.signupPage=1>here</a> to return.<br>"
    , email
    );
    }

freez(&password);
}

void changePasswordPage(struct sqlConnection *conn)
/* change password page */
{
hPrintf(
"<h2>UCSC Genome Browser</h2>"
"<p align=\"left\">"
"</p>"
"<span style='color:red;'>%s</span>"
"<h3>Change Password</h3>"
"<form method=post action=\"hgLogin\" name=changePasswordForm >"
"<table>"
"<tr><td>E-mail</td><td><input type=text name=hgLogin_email size=20 value=\"%s\"> "
  "(your e-mail is also your user-id)</td></tr>"
"<tr><td>Current Password</td><td><input type=password name=hgLogin_password value=\"\" size=10></td></tr>\n"
"<tr><td>New Password</td><td><input type=password name=hgLogin_newPassword value=\"\" size=10></td></tr>\n"
"<tr><td>&nbsp;</td><td><input type=submit name=hgLogin.do.changePassword value=submit>"
"&nbsp;<input type=submit name=hgLogin.do.signupPage value=cancel></td></tr>"
"</table>"
"<br>"
, errMsg ? errMsg : ""
, cartUsualString(cart, "hgLogin_email", "")
);

cartSaveSession(cart);

hPrintf("</FORM>");

}

void changePassword(struct sqlConnection *conn)
/* process the change password form */
{
char query[256];
char *email = cartUsualString(cart, "hgLogin_email", "");
char *currentPassword = cartUsualString(cart, "hgLogin_password", "");
char *newPassword = cartUsualString(cart, "hgLogin_newPassword", "");
if (!email || sameString(email,""))
    {
    freez(&errMsg);
    errMsg = cloneString("Email cannot be blank.");
    changePasswordPage(conn);
    return;
    }
if (!currentPassword || sameString(currentPassword,""))
    {
    freez(&errMsg);
    errMsg = cloneString("Current password cannot be blank.");
    changePasswordPage(conn);
    return;
    }
if (!newPassword || sameString(newPassword,""))
    {
    freez(&errMsg);
    errMsg = cloneString("New password cannot be blank.");
    changePasswordPage(conn);
    return;
    }
safef(query,sizeof(query), "select password from gbMembers where email='%s'", email);
char *password = sqlQuickString(conn, query);
if (!password)
    {
    freez(&errMsg);
    errMsg = cloneString("Email not found.");
    changePasswordPage(conn);
    return;
    }
if (!checkPwd(currentPassword, password))
    {
    freez(&errMsg);
    errMsg = cloneString("Invalid current password.");
    changePasswordPage(conn);
    return;
    }
freez(&password);
if (!newPassword || sameString(newPassword,"") || (strlen(newPassword)<8))
    {
    freez(&errMsg);
    errMsg = cloneString("New password must be at least 8 characters long.");
    changePasswordPage(conn);
    return;
    }
if (!checkPwdCharClasses(newPassword))
    {
    freez(&errMsg);
    errMsg = cloneString(
	"Password must contain characters from 2 of the following 4 classes: "
	"[A-Z] [a-z] [0-9] [!@#$%^&*()].");
    changePasswordPage(conn);
    return;
    }
char encPwd[35] = "";
encryptNewPwd(newPassword, encPwd, sizeof(encPwd));
safef(query,sizeof(query), "update gbMembers set password='%s' where email='%s'", sqlEscapeString(encPwd), sqlEscapeString(email));
sqlUpdate(conn, query);

hPrintf
    (
    "<h2>UCSC Genome Browser</h2>"
    "<p align=\"left\">"
    "</p>"
    "<h3>Password has been changed.</h3>"
    "Click <a href=hgLogin?hgLogin.do.signupPage=1>here</a> to return.<br>"
    , email
    );

updatePasswordsFile(conn);

cartRemove(cart, "hgLogin_password");
cartRemove(cart, "hgLogin_newPassword");
}




void signupPage(struct sqlConnection *conn)
/* draw the signup page */
/* XXXX TODO: 
  cornfirm password, password help 
  like Required. 30 characters or fewer. Letters, digits and @/./+/-/_ only.
optional real name */

{
hPrintf(
"<h2>UCSC Genome Browser</h2>\n"
"<p align=\"left\">"
"</p>"
"GSID provides access to data from the 2003 VaxGen HIV vaccine phase III clinical trials on a yearly access-fee basis.<br>\n"
"Academic and non-profit researchers get a substantial discount. <br>\n"
"<br>\n"
"If you are already a member, click <a href=https://%s/>here</a> to access UCSC Genome Browser.<br>\n"
"To view your existing account, click <a href=\"hgLogin?hgLogin.do.displayUserInfo=1\">here</a>.<br>\n"
"To change your password, click <a href=\"hgLogin?hgLogin.do.changePasswordPage=1\">here</a>.<br>\n"
"Lost your password? Click <a href=\"hgLogin?hgLogin.do.lostPasswordPage=1\">here</a>.<br>\n"
"<span style='color:red;'>%s</span>"
"<h3>Sign up</h3>\n"
"<form method=post action=\"hgLogin\" name=mainForm >\n"
"NOTE: Your e-mail is also your user-id.\n"
"<table>\n"
"<tr><td>Name</td><td><input type=text name=hgLogin_user value=\"%s\" size=20></td></tr>\n"
"<tr><td>E-mail</td><td><input type=text name=hgLogin_email value=\"%s\"size=20>\n"
"<tr><td>Password</td><td><input type=password name=hgLogin_password value=\"%s\" size=10></td></tr>\n"
"<tr><td>Real name (optional)</td><td><input type=text name=hgLogin_realName value=\"%s\" size=20></td></tr>\n"
"<tr><td>&nbsp;</td><td><input type=submit name=hgLogin.do.signup value=submit></td></tr>\n"
"</table>\n"
"<br>\n"
, getenv("HTTP_HOST")
, errMsg ? errMsg : ""
, cartUsualString(cart, "hgLogin_user", "")
, cartUsualString(cart, "hgLogin_email", "")
, cartUsualString(cart, "hgLogin_password", "")
, cartUsualString(cart, "hgLogin_realName", "")
);


cartSaveSession(cart);

hPrintf("</FORM>");

}


void signup(struct sqlConnection *conn)
/* process the signup form */
{
char query[256];
char *user = cartUsualString(cart, "hgLogin_user", "");
if (!user || sameString(user,""))
    {
    freez(&errMsg);
    errMsg = cloneString("User name cannot be blank.");
    signupPage(conn);
    return;
    }
safef(query,sizeof(query), "select password from gbMembers where userName='%s'", user);
char *password = sqlQuickString(conn, query);
if (password)
    {
    freez(&errMsg);
    errMsg = cloneString("A user with this name already exists.");
    signupPage(conn);
    freez(&user);
    return;
    }

char *email = cartUsualString(cart, "hgLogin_email", "");
if (!email || sameString(email,""))
    {
    freez(&errMsg);
    errMsg = cloneString("Email cannot be blank.");
    signupPage(conn);
    return;
    }

password = cartUsualString(cart, "hgLogin_password", "");
if (!password || sameString(password,"") || (strlen(password)<8))
    {
    freez(&errMsg);
    errMsg = cloneString("Password must be at least 8 characters long.");
    signupPage(conn);
    return;
    }
if (!checkPwdCharClasses(password))
    {
    freez(&errMsg);
    errMsg = cloneString("Password must contain characters from 2 of the following 4 classes: [A-Z] [a-z] [0-9] [!@#$%^&*()].");
    signupPage(conn);
    return;
    }

char *realName = cartUsualString(cart, "hgLogin_realName", "");
if (!realName || sameString(realName,""))
    {
    realName = " ";
    }

char encPwd[35] = "";
encryptNewPwd(password, encPwd, sizeof(encPwd));
safef(query,sizeof(query), "insert into gbMembers set "
    "userName='%s',realName='%s',password='%s',email='%s', "
    "lastUse=NOW(),activated='N',dateAuthenticated='9999-12-31 23:59:59'",
    sqlEscapeString(user),sqlEscapeString(realName),sqlEscapeString(encPwd),sqlEscapeString(email));
sqlUpdate(conn, query);


hPrintf(
"<h2>UCSC Genome Browser</h2>\n"
"<p align=\"left\">\n"
"</p>\n"
"<h3>User %s successfully added.</h3>\n"
, email
);


hPrintf(
"Click <a href=hgLogin?hgLogin.do.signupPage=1>here</a> to return.<br>\n"
);


}


/* ----- account login/display functions ---- */


void displayLoginPage(struct sqlConnection *conn)
/* draw the account login page */
{
char *email = cartUsualString(cart, "hgLogin_email", "");
/* for password security, use cgi hash instead of cart */
char *password = cgiUsualString("hgLogin_password", "");
hPrintf(
"<h2>UCSC Genome Browser</h2>"
"<p align=\"left\">"
"</p>"
"<span style='color:red;'>%s</span>"
"<h3>Account Login</h3>"
"<form method=post action=\"hgLogin\" name=accountLoginForm >"
"<input TYPE=\"hidden\" NAME=\"hgLogin_click\" VALUE=\"postLogin\" >"
"<table>"
"<tr><td>User Name</td><td><input type=text name=hgLogin_userName value=\"%s\" size=20> <BR> "
"<tr><td>Password</td><td><input type=password name=hgLogin_password value=\"%s\" size=10></td></tr><BR>"
"<tr><td>&nbsp;</td><td><input type=submit name=hgLogin.do.displayLogin value=submit>"
"&nbsp;<input type=submit name=hgLogin.do.signupPage value=cancel></td></tr>"
"</table>"
"<BR>"
, errMsg ? errMsg : ""
, email
, password
);

cartSaveSession(cart);

hPrintf("</FORM>");

}


/******* BEGIN dispalyLogin *************************/
void displayLogin(struct sqlConnection *conn)
/* display user account info */
{
struct sqlResult *sr;
char **row;
char query[256];
char *userName = cartUsualString(cart, "hgLogin_userName", "");
if (sameString(userName,""))
    {
    freez(&errMsg);
    errMsg = cloneString("User name cannot be blank.");
    displayLoginPage(conn);
    return;
    }
/*************************************************************
char *email = cartUsualString(cart, "hgLogin_email", "");
if (sameString(email,""))
    {
    freez(&errMsg);
    errMsg = cloneString("Email cannot be blank.");
    displayLoginPage(conn);
    return;
    }
****************************************************************/
/* for password security, use cgi hash instead of cart */
char *password = cgiUsualString("hgLogin_password", "");
if (sameString(password,""))
    {
    freez(&errMsg);
    errMsg = cloneString("Password cannot be blank.");
    displayLoginPage(conn);
    return;
    }

safef(query,sizeof(query),"select * from gbMembers where userName='%s'", userName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    {
    freez(&errMsg);
    char temp[256];
    safef(temp,sizeof(temp),"User name %s not found.",userName);
    errMsg = cloneString(temp);
    displayLoginPage(conn);
    return;
    }
struct gbMembers *m = gbMembersLoad(row);
sqlFreeResult(&sr);

/* TODO: check user name exist and activated */
/* ..... */

if (checkPwd(password,m->password))
    {
hPrintf("<h1>Login succesful !!!! calling displayLoginSuccess now.</h1>\n");
      displayLoginSuccess();
// htmlSetCookie("hgLogin_User", "chinhli", NULL, NULL, ".cse.ucsc.edu", FALSE);
// hPrintf("<meta http-equiv=\"set-cookie\" content=\"hgLogin_User=chinhli;expires=Fri, 30 Dec 2015 12:00:00 GMT; path=cse.ucsc.edu\">");
      return;
//htmlSetCookie("hgLogin_User", "Chin Li", NULL, NULL,
//".cse.ucsc.edu", FALSE);
//    hPrintf("<h1>Login Information for %s:</h1>\n",m->email);
//    hPrintf("<table>\n");
//    hPrintf("<tr><td align=right>name:</td><td>%s</td><tr>\n",m->userName);
//    hPrintf("<tr><td align=right>activated:</td><td>%s</td><tr>\n",m->activated);
//    hPrintf("</table>\n");
//    hPrintf("<br>\n");
//    hPrintf("Return to <a href=\"hgSession\">Session</A>.<br>\n");
//    hPrintf("Return to <a href=\"hgLogin\">signup</A>.<br>\n");
//
//    hPrintf("Go to <a href=\"/\">UCSC Genome Browser</A>.<br>\n");
    }
else
    {
    hPrintf("<h1>Invalid User/Password</h1>\n",m->email);
    hPrintf("Return to <a href=\"hgLogin\">signup</A>.<br>\n");
    }

gbMembersFree(&m);

}
/******* end old dispalyLogin *************************/



void  displayLoginSuccess()
/* display login success msg, and set cookie */
{
hPrintf(
"<h2>UCSC Genome Browser</h2>"
"<p align=\"left\">"
"</p>"
"<span style='color:red;'>%s</span>"
"\n"
//"<script type=\"text/javascript\">\n"
//"function setCookie(c_name,value,exdays) "
//"{"
//"var exdate=new Date();"
//"exdate.setDate(exdate.getDate() + exdays);"
//"var c_value=escape(value) + \" 30 Dec 2015 12:00:00 GMT\";"
//"document.cookie=c_name + \"=\" + c_value;"
//"} <\/script> \n");
//hPrintf("setCookie(\"hgLogin_User\",\"chinhl\", 180);");
);
//hPrintf("document.cookie = \"hgLogin_User=chinhli\";");
//hPrintf("<body onload=\"setCookie()\"> </body>");

hPrintf(
"<script language=\"JavaScript\">"
" document.write(\"Hello World!\");"
"<\/script><script language=\"JavaScript\">"
"\n"
"document.write(\"Hello, Cookie Monster!\");"
"document.cookie =  \"hgLogin_UserName=hgLogin001; domain=cse.ucsc.edu; expires=Mon, 30 Apr 2012 20:47:11 UTC; path=/\"; "
"\n"
"document.cookie =  \"hgLogin_UserID=3043; domain=cse.ucsc.edu; expires=Mon, 30 Apr 2012 20:47:11 UTC; path=/\";"
" </script>"
"\n"
);
hPrintf(
"<script type=\"text/javascript\">\n"
"<!-- "
"\n"
"window.location =\"http://hgwdev-chinhli.cse.ucsc.edu/cgi-bin/hgSession?hgS_doMainPage=1\";"
"\n"
"//-->"
"\n"
"</script>"
);
}



void displayUserInfo(struct sqlConnection *conn)
/* display user account info */
{
struct sqlResult *sr;
char **row;
char query[256];
char *user = cartUsualString(cart, "hgLogin_user", "");
/*************************************/
if (sameString(user,""))
    {
    freez(&errMsg);
    errMsg = cloneString("User name cannot be blank.");
    displayUserInfo(conn);
    return;
    }
/* for password security, use cgi hash instead of cart */
char *password = cgiUsualString("hgLogin_password", "");
if (sameString(password,""))
    {
    freez(&errMsg);
    errMsg = cloneString("Password cannot be blank.");
    displayUserInfo(conn);
    return;
    }
safef(query,sizeof(query),"select * from gbMembers where username='%s'", user);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    {
    freez(&errMsg);
    char temp[256];
    safef(temp,sizeof(temp),"User %s not found.",user);
    errMsg = cloneString(temp);
    displayUserInfo(conn);
    return;
    }
struct gbMembers *m = gbMembersLoad(row);
sqlFreeResult(&sr);

if (checkPwd(password,m->password))
    {

    hPrintf("<h1>UCSC Genome Browser User Information:</h1>\n");
    hPrintf("<table>\n");
    hPrintf("<tr><td align=right>User name:</td><td>%s</td><tr>\n",m->userName);

    hPrintf("<tr><td align=right>Real name:</td><td>%s</td><tr>\n",m->realName);
    hPrintf("<tr><td align=right>E-mail:</td><td>%s</td><tr>\n",m->email);
    hPrintf("</table>\n");
    hPrintf("<br>\n");


    hPrintf("Return to <a href=\"hgLogin\">signup</A>.<br>\n");
    hPrintf("Go to <a href=\"/\">UCSC Genome Browser</A>.<br>\n");
    }
else
    {
    hPrintf("<h1>Invalid User/Password</h1>\n",m->userName);
    hPrintf("Return to <a href=\"hgLogin\">signup</A>.<br>\n");
    }
/**************************************************/
gbMembersFree(&m);

}

/*
void upgradeMembersTable(struct sqlConnection* conn)
/ * one-time upgrade of gbMembers table to store encrypted passwords * /
{
char query[256];

safef(query,sizeof(query),"select email from members");
struct slName *email=NULL,*list = sqlQuickList(conn,query);
for(email=list;email;email=email->next)
    {

    uglyf("email=%s<br>\n",email->name);

    safef(query,sizeof(query),"select password from gbMembers where email='%s'", email->name);
    char *password = sqlQuickString(conn,query);

    uglyf("password=%s<br>\n",password);

    if (password)
	{
	if (!startsWith("$1$",password)) / * upgrade has not already been done * /
	    {
	    uglyf("does not start with $1$<br>\n");
	    char encPwd[35] = "";
    	    encryptNewPwd(password, encPwd, sizeof(encPwd));
	    safef(query,sizeof(query),"update gbMembers set password = '%s' where email='%s'",
		sqlEscapeString(encPwd), sqlEscapeString(email->name));
	    uglyf("query: %s<br>\n",query);
	    sqlUpdate(conn,query);
	    }
	freez(&password);
	}

    uglyf("<br>\n");

    }
slFreeList(&list);
}
*/


void doMiddle(struct cart *theCart)
/* Write the middle parts of the HTML page.
 * This routine sets up some globals and then
 * dispatches to the appropriate page-maker. */
{
struct sqlConnection *conn = hConnectCentral();
cart = theCart;


if (cartVarExists(cart, "debug"))
    debugShowAllMembers(conn);
/* remove after a while when it is no longer needed
else if (cartVarExists(cart, "fixMembers"))
    {
    upgradeMembersTable(conn);
    updatePasswordsFile(conn);
    hPrintf(
    "<h2>UCSC Genome Browser</h2>"
    "<p align=\"left\">"
    "</p>"
    "<h3>Successfully updated the gbMembers table to store hashed passwords.</h3>"
    "Click <a href=hgLogin?hgLogin.do.signupPage=1>here</a> to return.<br>"
    );
    }
*/
else if (cartVarExists(cart, "update"))
    {
    updatePasswordsFile(conn);
    hPrintf(
    "<h2>UCSC Genome Browser</h2>"
    "<p align=\"left\">"
    "</p>"
    "<h3>Successfully updated the authentication file.</h3>"
    "Click <a href=hgLogin?hgLogin.do.signupPage=1>here</a> to return.<br>"
    );
    }
else if (cartVarExists(cart, "hgLogin.do.lostPasswordPage"))
    lostPasswordPage(conn);
else if (cartVarExists(cart, "hgLogin.do.lostPassword"))
    lostPassword(conn);
else if (cartVarExists(cart, "hgLogin.do.changePasswordPage"))
    changePasswordPage(conn);
else if (cartVarExists(cart, "hgLogin.do.changePassword"))
    changePassword(conn);
else if (cartVarExists(cart, "hgLogin.do.displayUserInfo"))
    displayUserInfo(conn);
else if (cartVarExists(cart, "hgLogin.do.displayLoginPage"))
    displayLoginPage(conn);
else if (cartVarExists(cart, "hgLogin.do.displayLogin"))
    displayLogin(conn);
else if (cartVarExists(cart, "hgLogin.do.signup"))
    signup(conn);
else
    signupPage(conn);


hDisconnectCentral(&conn);
cartRemovePrefix(cart, "hgLogin.do.");

}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLogin - administer gsid hiv membership functions - a cgi script\n"
  "usage:\n"
  "   hgLogin\n"
  );
}

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(100000000);
cgiSpoof(&argc, argv);
htmlSetStyle(htmlStyleUndecoratedLink);
htmlSetBgColor(HG_CL_OUTSIDE);
oldCart = hashNew(10);
cartHtmlShell("UCSC Genome Browser Signup", doMiddle, hUserCookie(), excludeVars, oldCart);
return 0;
}
