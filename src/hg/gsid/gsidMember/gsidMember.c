/* gsidMember - Administer GSID HIV membership - signup, paypal, lost password, etc. */

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

#include "gsidMember.h"
#include "members.h"
#include "bio.h"
#include "paypalSignEncrypt.h"
#include "versionInfo.h"

static char const rcsid[] = "$Id: gsidMember.c,v 1.35 2008/09/08 16:05:04 fanhsu Exp $";

char *excludeVars[] = { "submit", "Submit", "debug", "fixMembers", "update", "gsidM_password", NULL }; 
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
"select email,password from members where activated='Y'"
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


/* -------- paypal functions ------ */

void appendSqlField(struct dyString* dy, char *varName, struct cgiVar *cgiVars)
/* append the next field to the sql insert statement */
{
boolean isFirstField = dy->stringSize == 0;
if (isFirstField)
    dyStringAppend(dy,"insert into transactions set ");
dyStringPrintf(dy,"%s%s='%s'",
    isFirstField ? "" : ", ",
    sqlEscapeString(varName),
    sqlEscapeString(cgiUsualString(varName,"")));
struct cgiVar *this = NULL;
for(this=cgiVars;this;this=this->next)
    {
    if (sameString(this->name,varName))
	this->saved = TRUE;
    }
}


void processIpn(struct sqlConnection *conn)
/* process Instant Payment Notification 
 *  Steps: 
 *   verify server source name
 *   write to log
 *   compose post pack to paypal.com or sandbox
 *     todo: optimize this with https
 *   verify response
 *   write transaction to ipn table
 *   mark user as paid if transaction is completed
 *    (matching on email)
 *   activate user account if completed by updating "passwords" file.
 */
{

/* save the ipn post variables received to the log regardless */
struct cgiVar *this=NULL, *cgiVars = cgiVarList();
FILE *f=mustOpen("ipn.log","a");
struct dyString *dy=newDyString(256);
struct dyString *dyVerify = NULL;
char *paypalServer = cfgOption("paypalServer");
dyStringAppend(dy,"../cgi-bin/webscr?cmd=_notify-validate");
for(this=cgiVars;this;this=this->next)
    {
    fprintf(f,"%s=%s\n", this->name, this->val);
    char *encodedVal = cgiEncode(this->val);
    dyStringPrintf(dy,"&%s=%s", this->name, encodedVal);
    freeMem(encodedVal);
    this->saved = FALSE; /* clear now, use later */ 
    }
fflush(f);

/* verify ipn sender ip */
char *remoteAddr=getenv("REMOTE_ADDR");
//66.135.197.164 ipn.sandbox.paypal.com
fprintf(f,"REMOTE_ADDR=%s\n", remoteAddr);
fflush(f);

char *paypalIpn = reverseDns(remoteAddr);

if (!sameString(paypalIpn,cfgOption("paypalIpnServer")))
    {
    fprintf(f,"Error: invalid REMOTE_ADDR %s=%s is not paypal ipn %s\n", remoteAddr, paypalIpn, cfgOption("paypalIpnServer"));
    fflush(f);
    goto cleanup;
    }

/* verify via post back to paypal */
fprintf(f,"about to attempt POST to [%s]:\n",dy->string);
fflush(f);

dyVerify = bio(paypalServer, dy->string, "all.pem", NULL);  /* certs to verify OK */
if (!dyVerify)
    {
    fprintf(f,"Error: unable to post verification to %s\n",dy->string);
    fflush(f);
    goto cleanup;
    }

//struct lineFile *lf = netLineFileMayOpen(dy->string);

struct lineFile *lf = lineFileOnString("response",TRUE,cloneString(dyVerify->string));
fprintf(f,"POST verification response:\n");
fflush(f);
char *line = NULL;
boolean verified = FALSE;
/* skip http response header */
while (lineFileNext(lf, &line, NULL))  
    {
    if (sameString(line,""))
	break;
    }
while (lineFileNext(lf, &line, NULL))
    {
    fprintf(f,"%s\n",line);
    fflush(f);
    if (sameString(line,"VERIFIED"))
	verified = TRUE;
    }
lineFileClose(&lf);
if (!verified)
    {
    fprintf(f,"NOT VERIFIED (txn_id=%s)\n",cgiOptionalString("txn_id"));
    fflush(f);
    goto cleanup;
    }
fprintf(f,"VERIFIED (txn_id=%s)\n",cgiOptionalString("txn_id"));
fflush(f);

/* append to transactions table */
dyStringClear(dy);
appendSqlField(dy,"invoice",cgiVars);
appendSqlField(dy,"receiver_email",cgiVars);
appendSqlField(dy,"item_name",cgiVars);
appendSqlField(dy,"item_number",cgiVars);
appendSqlField(dy,"quantity",cgiVars);
appendSqlField(dy,"payment_status",cgiVars);
appendSqlField(dy,"pending_reason",cgiVars);
appendSqlField(dy,"payment_date",cgiVars);
appendSqlField(dy,"mc_gross",cgiVars);
appendSqlField(dy,"mc_fee",cgiVars);
appendSqlField(dy,"shipping",cgiVars);
appendSqlField(dy,"tax",cgiVars);
appendSqlField(dy,"mc_currency",cgiVars);
appendSqlField(dy,"txn_id",cgiVars);
appendSqlField(dy,"txn_type",cgiVars);
appendSqlField(dy,"first_name",cgiVars);
appendSqlField(dy,"last_name",cgiVars);
appendSqlField(dy,"address_street",cgiVars);
appendSqlField(dy,"address_city",cgiVars);
appendSqlField(dy,"address_state",cgiVars);
appendSqlField(dy,"address_zip",cgiVars);
appendSqlField(dy,"address_country",cgiVars);
appendSqlField(dy,"address_status",cgiVars);
appendSqlField(dy,"residence_country",cgiVars);
appendSqlField(dy,"payer_email",cgiVars);
appendSqlField(dy,"payer_id",cgiVars);
appendSqlField(dy,"payer_status",cgiVars);
appendSqlField(dy,"payment_type",cgiVars);
appendSqlField(dy,"payment_gross",cgiVars);
appendSqlField(dy,"payment_fee",cgiVars);
appendSqlField(dy,"business",cgiVars);
appendSqlField(dy,"referrer_id",cgiVars);
appendSqlField(dy,"receiver_id",cgiVars);
appendSqlField(dy,"charset",cgiVars);
appendSqlField(dy,"custom",cgiVars);
appendSqlField(dy,"notify_version",cgiVars);
appendSqlField(dy,"verify_sign",cgiVars);
/* catchall for fields we did not anticipate, or future fields */
dyStringPrintf(dy,", otherFields='");
for(this=cgiVars;this;this=this->next)
    {
    if (!this->saved)
	dyStringPrintf(dy,"%s=%s\\n",sqlEscapeString(this->name),sqlEscapeString(this->val));
    /* remove these vars from the cart for better security/privacy */
    cartRemove(cart, this->name);	
    }
dyStringPrintf(dy,"'");

//debug  TODO: clean that out of trash
//writeGulp("../trash/debug.sql", dy->string, dy->stringSize);

sqlUpdate(conn,dy->string);


char *invoice = cgiUsualString("invoice","");

char *paymentDate = cgiUsualString("payment_date","");

/* handle expiration date */

/* Could use <time.h> function strptime, 
but perhaps getdate with the external file datemsk 
is more flexible for GSID in the long run.
struct tm *tm;
AllocVar(tm);
char *dateOk=strptime(paymentDate, "%H:%M:%S %b %d, %Y PST", tm);
if (!dateOk) strptime(paymentDate, "%H:%M:%S %b %d, %Y PDT", tm);
if (!dateOk)
*/
setenv("DATEMSK","./datemsk", TRUE);  /* required by getdate() */
struct tm *tm=getdate(paymentDate);
if (!tm)
    {
    fprintf(f,"error: getdate(%s) returned null struct tm*\n", paymentDate);
    fflush(f);
    goto cleanup;
    }
/* set expiration date to one year ahead */
char expireDate[11]; /* note: tm returns year rel 1900, mon and day are 0 based */ 
safef(expireDate,sizeof(expireDate),"%4d-%02d-%02d",1900+tm->tm_year+1,tm->tm_mon+1,tm->tm_mday+1);

/* use invoice# to map back to user's email */
char query[256];
safef(query,sizeof(query), "select email from invoices where id=%s", invoice);
char *email = sqlQuickString(conn, query);
if (!email)
    {
    fprintf(f,"error: unable to use invoice# %s to map back to user's email.\n", invoice);
    fflush(f);
    goto cleanup;
    return;
    }

/* see if payment_status is completed */
char *paymentStatus = cgiUsualString("payment_status","");
if (!sameString("Completed",paymentStatus))
    {
    fprintf(f,"Note: payment status not 'Completed' %s\n",dy->string);
    fflush(f);
    /* send payer an email confirming */
    char cmd[256];
    safef(cmd,sizeof(cmd), 
    "echo \"We received your payment through PayPal. However your account is not yet activated.\nPayment status is %s %s. When your payment status is completed your account will be activated and you will receive another email.  Thank you.\" | mail -s \"Payment received for GSID HIV Data Browser access.\" %s"
    , paymentStatus
    , cgiUsualString("payment_reason","") 
    , email);
    int result = system(cmd);
    if (result == -1)
	{
	fprintf(f,"Note: sending email notice of non-activated account to %s failed\n", email);
	fflush(f);
	goto cleanup;
	}
    goto cleanup;
    }


/* Write payment info to the members table 
 *  email field has been stored in the invoice field */
dyStringClear(dy);
dyStringPrintf(dy,"update members set "
"activated='Y',"
//P means paied but not activated until GA
//"activated='P',"
"amountPaid='%s',"
"datePaid='%s',"
"expireDate='%s'"
" where email='%s'"
, cgiUsualString("payment_gross","")
, paymentDate
, expireDate
, sqlEscapeString(email)
);

//debug  TODO: clean that out of trash
//writeGulp("../trash/debug.sql", dy->string, dy->stringSize);

sqlUpdate(conn,dy->string);


updatePasswordsFile(conn);

/* send payer an email confirming */
char cmd[256];
safef(cmd,sizeof(cmd), 
"echo \"We received your payment through Paypal. Your account is now activated.\nPlease go to http://%s/ to access the site. \" | mail -s \"Payment received for GSID HIV Data Browser access.\" %s"
, getenv("HTTP_HOST"), email);
int result = system(cmd);
if (result == -1)
    {
    fprintf(f,"Note: sending email notice of activated account to %s failed\n", email);
    fflush(f);
    goto cleanup;
    }

cleanup:
freez(&paypalIpn);
fprintf(f,"\n");
carefulClose(&f);
dyStringFree(&dy);
dyStringFree(&dyVerify);
}

/* -------- functions ---- */

void debugShowAllMembers(struct sqlConnection *conn)
/* display all members */
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
"<h2>GSID HIV Data Browser</h2>"
"<p align=\"left\">"
"</p>"
"<font color=red>%s</font>"
"<h3>Send Me A New Password</h3>"
"<form method=post action=\"gsidMember\" name=lostPasswordForm >"
"<table>"
"<tr><td>E-mail</td><td><input type=text name=gsidM_email size=20> "
  "(your e-mail is also your user-id)</td></tr>"
"<tr><td>&nbsp;</td><td><input type=submit name=gsidMember.do.lostPassword value=submit>"
"&nbsp;<input type=submit name=gsidMember.do.signupPage value=cancel></td></tr>"
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
char *email = cartUsualString(cart, "gsidM_email", "");
if (!email || sameString(email,""))
    {
    freez(&errMsg);
    errMsg = cloneString("Email cannot be blank.");
    lostPasswordPage(conn);
    return;
    }
safef(query,sizeof(query), "select password from members where email='%s'", email);
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

safef(query,sizeof(query), "update members set password='%s' where email='%s'", sqlEscapeString(encPwd), sqlEscapeString(email));
sqlUpdate(conn, query);

updatePasswordsFile(conn);

safef(cmd,sizeof(cmd), 
"echo 'Your new password is: %s' | mail -s \"Lost GSID HIV password\" %s"
, password, email);
int result = system(cmd);
if (result == -1)
    {
    hPrintf(
    "<h2>GSID HIV Data Browser</h2>"
    "<p align=\"left\">"
    "</p>"
    "<h3>Error emailing password to: %s</h3>"
    "Click <a href=gsidMember?gsidMember.do.signupPage=1>here</a> to return.<br>"
    , email
    );
    }
else
    {
    hPrintf(
    "<h2>GSID HIV Data Browser</h2>"
    "<p align=\"left\">"
    "</p>"
    "<h3>Password has been emailed to: %s</h3>"
    "Click <a href=gsidMember?gsidMember.do.signupPage=1>here</a> to return.<br>"
    , email
    );
    }

freez(&password);
}

void changePasswordPage(struct sqlConnection *conn)
/* change password page */
{
hPrintf(
"<h2>GSID HIV Data Browser</h2>"
"<p align=\"left\">"
"</p>"
"<font color=red>%s</font>"
"<h3>Change Password</h3>"
"<form method=post action=\"gsidMember\" name=changePasswordForm >"
"<table>"
"<tr><td>E-mail</td><td><input type=text name=gsidM_email size=20 value=\"%s\"> "
  "(your e-mail is also your user-id)</td></tr>"
"<tr><td>Current Password</td><td><input type=password name=gsidM_password value=\"\" size=10></td></tr>\n"
"<tr><td>New Password</td><td><input type=password name=gsidM_newPassword value=\"\" size=10></td></tr>\n"
"<tr><td>&nbsp;</td><td><input type=submit name=gsidMember.do.changePassword value=submit>"
"&nbsp;<input type=submit name=gsidMember.do.signupPage value=cancel></td></tr>"
"</table>"
"<br>"
, errMsg ? errMsg : ""
, cartUsualString(cart, "gsidM_email", "")
);

cartSaveSession(cart);

hPrintf("</FORM>");

}

void changePassword(struct sqlConnection *conn)
/* process the change password form */
{
char query[256];
char *email = cartUsualString(cart, "gsidM_email", "");
char *currentPassword = cartUsualString(cart, "gsidM_password", "");
char *newPassword = cartUsualString(cart, "gsidM_newPassword", "");
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
safef(query,sizeof(query), "select password from members where email='%s'", email);
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
safef(query,sizeof(query), "update members set password='%s' where email='%s'", sqlEscapeString(encPwd), sqlEscapeString(email));
sqlUpdate(conn, query);

hPrintf
    (
    "<h2>GSID HIV Data Browser</h2>"
    "<p align=\"left\">"
    "</p>"
    "<h3>Password has been changed.</h3>"
    "Click <a href=gsidMember?gsidMember.do.signupPage=1>here</a> to return.<br>"
    , email
    );
    
updatePasswordsFile(conn);

cartRemove(cart, "gsidM_password");
cartRemove(cart, "gsidM_newPassword");
}




void signupPage(struct sqlConnection *conn)
/* draw the signup page */
{
hPrintf(
"<h2>GSID HIV Data Browser</h2>\n"
"<p align=\"left\">"
"</p>"
"GSID provides access to data from the 2003 VaxGen HIV vaccine phase III clinical trials on a yearly access-fee basis.<br>\n"
"Academic and non-profit researchers get a substantial discount. <br>\n"
"<br>\n"
"If you are already a member, click <a href=https://%s/>here</a> to access GSID HIV Data Browser.<br>\n"
"To view your existing account, click <a href=\"gsidMember?gsidMember.do.displayAccountPage=1\">here</a>.<br>\n"
"To change your password, click <a href=\"gsidMember?gsidMember.do.changePasswordPage=1\">here</a>.<br>\n"
"Lost your password? Click <a href=\"gsidMember?gsidMember.do.lostPasswordPage=1\">here</a>.<br>\n"
"<font color=red>%s</font>"
"<h3>Sign up</h3>\n"
"<form method=post action=\"gsidMember\" name=mainForm >\n"
"NOTE: Your e-mail is also your user-id.\n"
"<table>\n"
"<tr><td>E-mail</td><td><input type=text name=gsidM_email value=\"%s\"size=20>\n"
"<tr><td>Password</td><td><input type=password name=gsidM_password value=\"%s\" size=10></td></tr>\n"
"<tr><td>Name</td><td><input type=text name=gsidM_name value=\"%s\" size=20></td></tr>\n"
"<tr><td>Phone</td><td><input type=text name=gsidM_phone value=\"%s\" size=20></td></tr>\n"
"<tr><td>Institution</td><td><input type=text name=gsidM_institution value=\"%s\" size=40></td></tr>\n"
"<tr><td>Type</td><td><input type=radio name=gsidM_type value=commercial%s>Commercial $%s.00 USD</td></tr>\n"
"<tr><td>&nbsp;</td><td><input type=radio name=gsidM_type value=academic%s>Academic $%s.00 USD</td></tr>\n"
"<tr><td>&nbsp;</td><td><input type=submit name=gsidMember.do.signup value=submit></td></tr>\n"
"</table>\n"
"<br>\n"
"Questions? Call (650) 228-7900.<br>\n"
, getenv("HTTP_HOST")
, errMsg ? errMsg : ""
, cartUsualString(cart, "gsidM_email", "")
, cartUsualString(cart, "gsidM_password", "")
, cartUsualString(cart, "gsidM_name", "")
, cartUsualString(cart, "gsidM_phone", "")
, cartUsualString(cart, "gsidM_institution", "")
, sameString("commercial",cartUsualString(cart, "gsidM_type", "")) ? " checked" : ""
, cfgOption("paypalCommercialFee")
, sameString("academic",cartUsualString(cart, "gsidM_type", "")) ? " checked" : ""
, cfgOption("paypalAcademicFee")
);


cartSaveSession(cart);

hPrintf("</FORM>");

}


void drawPaymentButton(struct sqlConnection *conn, char *type)
{
char query[256];
char *email = cartUsualString(cart, "gsidM_email", "");

safef(query, sizeof(query), "insert into invoices values(default, '%s')", sqlEscapeString(email));
sqlUpdate(conn, query);
int id = sqlLastAutoId(conn);
char invoice[20];
safef(invoice, sizeof(invoice), "%d", id);

//char buttonFile[256];
//safef(buttonFile,sizeof(buttonFile),"./%s.button",type); // TODO may move the button file later
char buttonHtml[4096];
//readInGulp(buttonFile, &buttonHtml, NULL);
char buttonData[4096];

char *paypalServer = cfgOption("paypalServer");
char *httpHost=getenv("HTTP_HOST");
char *paypalEmail = cfgOption("paypalEmail");

safef(buttonData, sizeof(buttonData), 
"cmd=_xclick\n"
"business=%s\n"
"invoice=%s\n"
"item_name=GSID HIV Yearly %s Access Fee\n"
"item_number=%s\n"
"amount=%s.00\n"
"no_shipping=2\n"
"return=https://%s/cgi-bin-signup/gsidMember?gsidMember.do.paypalThanks=1\n"
"cancel_return=https://%s/cgi-bin-signup/gsidMember?gsidMember.do.paypalCancel=1\n"
"notify_url=https://%s/cgi-bin-signup/gsidMember\n"
"no_note=1\n"
"currency_code=USD\n"
"lc=US\n"
"bn=PP-BuyNowBF\n"
"cert_id=%s\n"

, paypalEmail
, invoice

, sameString("commercial",type) 
  ? "Commercial"
  : "Academic"

, sameString("commercial",type) 
  ? "001"
  : "002"

, sameString("commercial",type) 
  ? cfgOption("paypalCommercialFee") 
  : cfgOption("paypalAcademicFee")

, httpHost
, httpHost
, httpHost
, cfgOption("gsidCertId")
);


//debug  TODO: clean that out of trash
//writeGulp("../trash/debug.buttonData", buttonData, strlen(buttonData));
//fprintf(stderr, "debug: about to encrypt buttonData=[%s]\n", buttonData);fflush(stderr);

char *buttonEncrypted = sign_and_encryptFromFiles(buttonData, "gsid_key.pem", "gsid_cert.pem", cfgOption("paypalCert"), FALSE);

if (buttonEncrypted)
    {
    //eraseWhiteSpace(buttonEncrypted);
    //debug  TODO: clean that out of trash
    //writeGulp("../trash/debug.buttonEnc", buttonEncrypted, strlen(buttonEncrypted));
    }
else
    {
    buttonEncrypted = cloneString("");
    fprintf(stderr, "error: sign_and_encrypt failed on buttonData=[%s]\n", buttonData);
    }

safef(buttonHtml,sizeof(buttonHtml),
"<form action=\"https://%s/cgi-bin/webscr\" method=\"post\">\n"
"<input type=\"hidden\" name=\"cmd\" value=\"_s-xclick\">\n"
"<input type=\"image\" src=\"https://%s/en_US/i/btn/x-click-but23.gif\" border=\"0\" name=\"submit\" alt=\"Make payments with PayPal - it's fast, free and secure!\">\n"
"<img alt=\"\" border=\"0\" src=\"https://%s/en_US/i/scr/pixel.gif\" width=\"1\" height=\"1\">\n"
"<input type=\"hidden\" name=\"encrypted\" value=\"%s\">\n"
"</form>\n"
, paypalServer
, paypalServer
, paypalServer
, buttonEncrypted
);

freez(&buttonEncrypted);

//debug
//fprintf(stderr, "encrypted button form: [%s]\n", buttonHtml);

hPrintf(
"Pay yearly %s access fee to activate your account.<br>\n"
"%s\n"
"Your payment will be processed by PayPal.<br>\n"
"You can use a variety of payment methods including credit card.<br>\n"
"You do not need to be a PayPal member.<br>\n"
"Be sure to click the \"Return to Merchant\" link when done paying.<br>\n"
"<br>\n"
, type
, buttonHtml
);

}


void signup(struct sqlConnection *conn)
/* process the signup form */
{
char query[256];
char *email = cartUsualString(cart, "gsidM_email", "");
if (!email || sameString(email,""))
    {
    freez(&errMsg);
    errMsg = cloneString("Email cannot be blank.");
    signupPage(conn);
    return;
    }
safef(query,sizeof(query), "select password from members where email='%s'", email);
char *password = sqlQuickString(conn, query);
if (password)
    {
    freez(&errMsg);
    errMsg = cloneString("A user with this email already exists.");
    signupPage(conn);
    freez(&password);
    return;
    }

password = cartUsualString(cart, "gsidM_password", "");
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

char *name = cartUsualString(cart, "gsidM_name", "");
if (!name || sameString(name,""))
    {
    freez(&errMsg);
    errMsg = cloneString("Name cannot be blank.");
    signupPage(conn);
    return;
    }

char *phone = cartUsualString(cart, "gsidM_phone", "");
if (!phone || sameString(phone,""))
    {
    freez(&errMsg);
    errMsg = cloneString("Phone cannot be blank.");
    signupPage(conn);
    return;
    }

char *institution = cartUsualString(cart, "gsidM_institution", "");
if (!institution || sameString(institution,""))
    {
    freez(&errMsg);
    errMsg = cloneString("Institution cannot be blank.");
    signupPage(conn);
    return;
    }

char *type = cartUsualString(cart, "gsidM_type", "");
if (!type || sameString(type,""))
    {
    freez(&errMsg);
    errMsg = cloneString("Type cannot be blank.");
    signupPage(conn);
    return;
    }

char encPwd[35] = "";
encryptNewPwd(password, encPwd, sizeof(encPwd));
safef(query,sizeof(query), "insert into members set "
    "email='%s',password='%s',activated='%s',name='%s',phone='%s',institution='%s',type='%s'", 
    sqlEscapeString(email), sqlEscapeString(encPwd), "N", sqlEscapeString(name), sqlEscapeString(phone), sqlEscapeString(institution), type);
sqlUpdate(conn, query);


hPrintf(
"<h2>GSID HIV Data Browser</h2>\n"
"<p align=\"left\">\n"
"</p>\n"
"<h3>User %s successfully added.</h3>\n"
, email
);

drawPaymentButton(conn, type);

hPrintf(
"Click <a href=gsidMember?gsidMember.do.signupPage=1>here</a> to return.<br>\n"
);


}


void paypalThanks()
/* thank the user for their payment and welcome them */
{
char *paypalServer = cfgOption("paypalServer");
char *status = cgiUsualString("st", "");
if (sameString(status,"Completed"))
    {
    hPrintf(
    "<p>\n"
    "<CENTER><H1>Thanks For signing up for GSID HIV Data Browser</H1></CENTER>\n"
    "<br>\n"
//    );

    "Your account is now activated and ready to use.<br>\n"
    
// disable the following statement with temporary Beta release message
/* 
    hPrintf(" We received your payment.  Your account is now created.  \n");
    hPrintf("<br><br><B>At the moment, the system is available only to our authorized Beta test users.</B>");
    hPrintf(" We will notify you as soon as our Beta test phase is completed. ");
    hPrintf(" Upon its official release,");
    hPrintf(" we will activate your account to enable you to log in.<br>");

    hPrintf(
*/

    "<br>\n"
    "Thank you for your payment. "
    "Your transaction has been completed, and a receipt for your purchase has been emailed to you.<br>\n"
    "You may log into PayPal at http://%s/us to view details of this transaction.\n"
    "<br>\n"
    "<br>\n"
    "<big>\n"
    "Go to <a href=\"/\">GSID HIV Data Browser</A>\n"
    "</big>\n"
    "<br>\n"
    "<br>\n"
    , paypalServer
    );
    }
else
    {
    hPrintf(
    "<p>\n"
    "<CENTER><H1>Thanks For signing up for GSID HIV Data Browser</H1></CENTER>\n"
    "<br>\n"
    "Thank you for your payment. "
    "However, your account is not activated yet (status=%s).<br>\n"
    "<br>\n"
    "When your transaction has been completed, a notice will be emailed to you.<br>\n"
    "You may log into PayPal at http://%s/us to view details of this transaction.\n"
    "<br>\n"
    "<br>\n"
    "<big>\n"
    "When your transaction is completed, you may go to <a href=\"/\">GSID HIV Data Browser</A>\n"
    "</big>\n"
    "<br>\n"
    "<br>\n"
    , status
    , paypalServer
    );
    }
}

void paypalCancel()
/* the user has cancelled their payment before completion */
{
hPrintf(
"<p>\n"
"<CENTER><H1>GSID HIV Data Browser - Payment Cancelled</H1></CENTER>\n"
"<br>\n"
"Because you cancelled your PayPal payment, your account has not been activated.<br>\n"
"<br>\n"
"<big>\n"
"Go to <a href=\"/hiv-signup-html/\">GSID HIV Data Browser</A> to sign up.\n"
"</big>\n"
"<br>\n"
"<br>\n"
);
}

/* ----- account login/display functions ---- */

void displayAccountPage(struct sqlConnection *conn)
/* draw the account login page */
{
char *email = cartUsualString(cart, "gsidM_email", "");
/* for password security, use cgi hash instead of cart */
char *password = cgiUsualString("gsidM_password", ""); 
hPrintf(
"<h2>GSID HIV Data Browser</h2>"
"<p align=\"left\">"
"</p>"
"<font color=red>%s</font>"
"<h3>Account Login</h3>"
"<form method=post action=\"gsidMember\" name=accountLoginForm >"
"<table>"
"<tr><td>E-mail</td><td><input type=text name=gsidM_email value=\"%s\" size=20> "
"<tr><td>Password</td><td><input type=password name=gsidM_password value=\"%s\" size=10></td></tr>\n"
  "(your e-mail is also your user-id)</td></tr>"
"<tr><td>&nbsp;</td><td><input type=submit name=gsidMember.do.displayAccount value=submit>"
"&nbsp;<input type=submit name=gsidMember.do.signupPage value=cancel></td></tr>"
"</table>"
"<br>"
, errMsg ? errMsg : ""
, email
, password
);

cartSaveSession(cart);

hPrintf("</FORM>");

}



void displayAccount(struct sqlConnection *conn)
/* display user account info */
{
struct sqlResult *sr;
char **row;
char query[256];
char *email = cartUsualString(cart, "gsidM_email", "");
if (sameString(email,""))
    {
    freez(&errMsg);
    errMsg = cloneString("Email cannot be blank.");
    displayAccountPage(conn);
    return;
    }
/* for password security, use cgi hash instead of cart */
char *password = cgiUsualString("gsidM_password", ""); 
if (sameString(password,""))
    {
    freez(&errMsg);
    errMsg = cloneString("Password cannot be blank.");
    displayAccountPage(conn);
    return;
    }

safef(query,sizeof(query),"select * from members where email='%s'", email);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    {
    freez(&errMsg);
    char temp[256];
    safef(temp,sizeof(temp),"Email %s not found.",email);
    errMsg = cloneString(temp);
    displayAccountPage(conn);
    return;
    }
struct members *m = membersLoad(row);
sqlFreeResult(&sr);

if (checkPwd(password,m->password))
    {

    hPrintf("<h1>Account Information for %s:</h1>\n",m->email);
    hPrintf("<table>\n");
    hPrintf("<tr><td align=right>name:</td><td>%s</td><tr>\n",m->name);
    hPrintf("<tr><td align=right>phone:</td><td>%s</td><tr>\n",m->phone);
    hPrintf("<tr><td align=right>institution:</td><td>%s</td><tr>\n",m->institution);
    hPrintf("<tr><td align=right>type:</td><td>%s</td><tr>\n",m->type);
    hPrintf("<tr><td align=right>amount paid:</td><td>$%8.2f</td><tr>\n",m->amountPaid);
    hPrintf("<tr><td align=right>expiration:</td><td>%s</td><tr>\n",m->expireDate);
    hPrintf("<tr><td align=right>activated:</td><td>%s</td><tr>\n",m->activated);
    hPrintf("</table>\n");
    hPrintf("<br>\n");


    /* add payment button if needed */
    char *currentDate=sqlQuickString(conn, "select current_date()");
    if (!sameString(m->activated,"Y") || strcmp(currentDate,m->expireDate)>0)
	{
	drawPaymentButton(conn, m->type);
	}
    freez(&currentDate);

    hPrintf("Return to <a href=\"gsidMember\">signup</A>.<br>\n");
    hPrintf("Go to <a href=\"/\">GSID HIV Data Browser</A>.<br>\n");
    }
else
    {
    hPrintf("<h1>Invalid User/Password</h1>\n",m->email);
    hPrintf("Return to <a href=\"gsidMember\">signup</A>.<br>\n");
    }

membersFree(&m);

}

/*
void upgradeMembersTable(struct sqlConnection* conn)
/ * one-time upgrade of members table to store encrypted passwords * /
{
char query[256];

safef(query,sizeof(query),"select email from members");
struct slName *email=NULL,*list = sqlQuickList(conn,query);
for(email=list;email;email=email->next)
    {

    uglyf("email=%s<br>\n",email->name);
    
    safef(query,sizeof(query),"select password from members where email='%s'", email->name);
    char *password = sqlQuickString(conn,query);

    uglyf("password=%s<br>\n",password);
    
    if (password)
	{
	if (!startsWith("$1$",password)) / * upgrade has not already been done * /
	    {
	    uglyf("does not start with $1$<br>\n");
	    char encPwd[35] = "";
    	    encryptNewPwd(password, encPwd, sizeof(encPwd));
	    safef(query,sizeof(query),"update members set password = '%s' where email='%s'", 
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
struct sqlConnection *conn;
cart = theCart;

//hSetDb("membership");
conn = hAllocConn("membership");


if (cartVarExists(cart, "debug"))
    debugShowAllMembers(conn);
/* remove after a while when it is no longer needed
else if (cartVarExists(cart, "fixMembers"))
    {
    upgradeMembersTable(conn);
    updatePasswordsFile(conn);
    hPrintf(
    "<h2>GSID HIV Data Browser</h2>"
    "<p align=\"left\">"
    "</p>"
    "<h3>Successfully updated the members table to store hashed passwords.</h3>"
    "Click <a href=gsidMember?gsidMember.do.signupPage=1>here</a> to return.<br>"
    );
    }
*/
else if (cartVarExists(cart, "update"))
    {
    updatePasswordsFile(conn);
    hPrintf(
    "<h2>GSID HIV Data Browser</h2>"
    "<p align=\"left\">"
    "</p>"
    "<h3>Successfully updated the authentication file.</h3>"
    "Click <a href=gsidMember?gsidMember.do.signupPage=1>here</a> to return.<br>"
    );
    }
else if (cgiVarExists("verify_sign"))
    processIpn(conn);
else if (cartVarExists(cart, "gsidMember.do.paypalThanks"))
    paypalThanks();
else if (cartVarExists(cart, "gsidMember.do.paypalCancel"))
    paypalCancel();
else if (cartVarExists(cart, "gsidMember.do.lostPasswordPage"))
    lostPasswordPage(conn);
else if (cartVarExists(cart, "gsidMember.do.lostPassword"))
    lostPassword(conn);
else if (cartVarExists(cart, "gsidMember.do.changePasswordPage"))
    changePasswordPage(conn);
else if (cartVarExists(cart, "gsidMember.do.changePassword"))
    changePassword(conn);
else if (cartVarExists(cart, "gsidMember.do.displayAccountPage"))
    displayAccountPage(conn);
else if (cartVarExists(cart, "gsidMember.do.displayAccount"))
    displayAccount(conn);
else if (cartVarExists(cart, "gsidMember.do.signup"))
    signup(conn);
else
    signupPage(conn);
    

hFreeConn(&conn);
cartRemovePrefix(cart, "gsidMember.do.");

}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gsidMember - administer gsid hiv membership functions - a cgi script\n"
  "usage:\n"
  "   gsidMember\n"
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
cartHtmlShell("GSID HIV Data Browser Signup", doMiddle, hUserCookie(), excludeVars, oldCart);
return 0;
}
