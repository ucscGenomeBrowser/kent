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
#include "wikiLink.h"
#include "hgLogin.h"
#include "gbMembers.h"
#include "versionInfo.h"

/* ---- Global variables. ---- */
char msg[4096] = "";
/* The excludeVars are not saved to the cart. */
char *excludeVars[] = { "submit", "Submit", "debug", "fixMembers", "update", 
     "hgLogin_password", "hgLogin_password2", "hgLogin_newPassword1",
     "hgLogin_newPassword2", NULL };
struct cart *cart;	/* This holds cgi and other variables between clicks. */
char *database;		/* Name of genome database - hg15, mm3, or the like. */
struct hash *oldCart;	/* Old cart hash. */
char *errMsg;           /* Error message to show user when form data rejected */
char signature[256]="\nUCSC Genome Browser\nhttp://www.genome.ucsc.edu ";

/* -------- password functions depend on optionally installed openssl lib ---- */
#ifdef USE_SSL
#include <openssl/md5.h>


void cryptWikiWay(char *password, char *salt, char* result)
/* encrypt password in mediawiki format - 
   ':B:'.$salt.':'. md5($salt.'-'.md5($password ) */
{
int i;
unsigned char result1[MD5_DIGEST_LENGTH];
unsigned char result2[MD5_DIGEST_LENGTH];
char firstMD5[MD5_DIGEST_LENGTH*2 + 1];
char secondMD5[MD5_DIGEST_LENGTH*2 + 1];
i = MD5_DIGEST_LENGTH;
MD5((unsigned char *)password, strlen(password), result1);
for(i = 0; i < MD5_DIGEST_LENGTH; i++)
    printf("%02x", result1[i]);
for(i = 0; i < MD5_DIGEST_LENGTH; i++)
    {
    sprintf(&firstMD5[i*2], "%02x", result1[i]);
    }   
// add the salt with "-" 
char saltDashMD5[256];
strcpy(saltDashMD5,salt);
strcat(saltDashMD5,"-");
strcat(saltDashMD5,firstMD5);
MD5((unsigned char *) saltDashMD5, strlen(saltDashMD5), result2);
for(i = 0; i < MD5_DIGEST_LENGTH; i++)
    {
    sprintf(&secondMD5[i*2], "%02x", result2[i]);
    }
strcpy(result, secondMD5);
}

void encryptPWD(char *password, char *salt, char *buf, int bufsize)
/* encrypt a password in mediawiki way */
{
char md5Returned[100];
cryptWikiWay(password, salt, md5Returned);
safecat(buf,bufsize,":B:");
safecat(buf,bufsize,salt);
safecat(buf,bufsize,":");
safecat(buf,bufsize,md5Returned);
}

void encryptNewPwd(char *password, char *buf, int bufsize)
/* encrypt a new password */
{
unsigned long seed[2];
char salt[] = "........";
const char *const seedchars =
"0123456789ABCDEFGHIJKLMNOPQRST"
"UVWXYZabcdefghijklmnopqrstuvwxyz";
int i;
/* Generate a (not very) random seed. */
seed[0] = time(NULL);
seed[1] = getpid() ^ (seed[0] >> 14 & 0x30000);
/* Turn it into printable characters from 'seedchars'. */
for (i = 0; i < 8; i++)
    salt[i] = seedchars[(seed[i/5] >> (i%5)*6) & 0x3f];
encryptPWD(password, salt, buf, bufsize);
}

char *generateTokenMD5(char *token)
/* Generate an unsalted MD5 string from token. */
{
unsigned char result[MD5_DIGEST_LENGTH];
char tokenMD5[MD5_DIGEST_LENGTH*2 + 1];
int i = MD5_DIGEST_LENGTH;
MD5((unsigned char *) token, strlen(token), result);
// Convert the tokenMD5 value to string
for(i = 0; i < MD5_DIGEST_LENGTH; i++)
    {
    sprintf(&tokenMD5[i*2], "%02x", result[i]);
    }
return cloneString(tokenMD5);
}

#else // --------- no USE_SSL ==> errAbort with message that openssl is required --------------

#define NEED_OPENSSL "kent/src must be recompiled with openssl libs and USE_SSL=1 in order for this to work."

void encryptPWD(char *password, char *salt, char *buf, int bufsize)
/* This is just a warning that appears in the absence of USE_SSL. Real implementation is above! */
{
errAbort(NEED_OPENSSL);
}

void encryptNewPwd(char *password, char *buf, int bufsize)
/* This is just a warning that appears in the absence of USE_SSL. Real implementation is above! */
{
errAbort(NEED_OPENSSL);
}

char *generateTokenMD5(char *token)
/* This is just a warning that appears in the absence of USE_SSL. Real implementation is above! */
{
errAbort(NEED_OPENSSL);
return NULL; // Compiler doesn't know that we never get here.
}

#endif//ndef USE_SSL

void findSalt(char *encPassword, char *salt, int saltSize)
/* find the salt part from the password field */
{
char tempStr1[45];
char tempStr2[45];
int i;
// Skip the ":B:" part
for (i = 3; i <= strlen(encPassword); i++)
    tempStr1[i-3] = encPassword[i];
i = strcspn(tempStr1,":");
safencpy(tempStr2, sizeof(tempStr2), tempStr1, i);
safecpy(salt, saltSize,tempStr2);
}

bool checkPwd(char *password, char *encPassword)
/* check an encrypted password */
{
char salt[14];
int saltSize;
saltSize = sizeof(salt);
findSalt(encPassword, salt, saltSize);
char encPwd[45] = "";
encryptPWD(password, salt, encPwd, sizeof(encPwd));
if (sameString(encPassword,encPwd))
    return TRUE;
else
    return FALSE;
}

unsigned int randInt(unsigned int n)
/* little randome number helper returns 0 to n-1 */
{
return (unsigned int) n * (rand() / (RAND_MAX + 1.0));
}

char *generateRandomPassword()
/* Generate random password for users who have lost their old one. */
{
char boundary[256];
char punc[] = "!@#$%&()";
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
            c = punc[randInt(8)];
            break;
        }
    boundary[i] = c;
    }
boundary[i]=0;
return cloneString(boundary);
}

/* ---- General purpose helper routines. ---- */
void backToHgSession(int nSec)
/* delay for N/10 micro seconds then go back to hgSession page */
{
char *hgLoginHost = wikiLinkHost();
int delay=nSec*100;
hPrintf("<script  language=\"JavaScript\">\n"
    "<!-- \n"
    "window.setTimeout(afterDelay, %d);\n"
    "function afterDelay() {\n"
    "window.location =\"http://%s/cgi-bin/hgSession?hgS_doMainPage=1\";\n}"
    "\n//-->\n</script>", delay, hgLoginHost);
}

void backToDoLoginPage(int nSec)
/* delay for N micro seconds then go back to Login page */
{
char *hgLoginHost = wikiLinkHost();
int delay=nSec*1000;
hPrintf("<script  language=\"JavaScript\">\n"
    "<!-- \n"
    "window.setTimeout(afterDelay, %d);\n"
    "function afterDelay() {\n"
    "window.location =\"http://%s/cgi-bin/hgLogin?hgLogin.do.displayLoginPage=1\";\n}"
    "//-->\n</script>", delay, hgLoginHost);
}

boolean tokenExpired(char *dateTime)
/* Is token expired? */
{
return FALSE;
}

void getReturnToURL(char **returnTo)
/* get URL passed in with returnto URL */
{
char *returnURL = cartUsualString(cart, "returnto", "");
char *hgLoginHost = wikiLinkHost();

if (!returnURL || sameString(returnURL,""))
   safef(returnTo, sizeof(returnTo),
        "http://%s/cgi-bin/hgSession?hgS_doMainPage=1", hgLoginHost);
else
   safecpy(returnTo, sizeof(returnTo), returnURL);
}

void returnToURL(int nSec)
/* delay for N/10  micro seconds then return to the "returnto" URL */
{
char *returnURL = cartUsualString(cart, "returnto", "");
char *hgLoginHost = wikiLinkHost();
char returnTo[512];

if (!returnURL || sameString(returnURL,""))
   safef(returnTo, sizeof(returnTo),
        "http://%s/cgi-bin/hgSession?hgS_doMainPage=1", hgLoginHost);
else
   safecpy(returnTo, sizeof(returnTo), returnURL);
int delay=nSec*100;
hPrintf(
    "<script  language=\"JavaScript\">\n"
    "<!-- "
    "\n"
    "window.setTimeout(afterDelay, %d);\n"
    "function afterDelay() {\n"
    "window.location =\"%s\";\n}"
    "\n//-->\n"
    "</script>", delay, returnURL);
}


void  displayActMailSuccess()
/* display Activate mail success box */
{
char *returnURL = cartUsualString(cart, "returnto", "");
char *hgLoginHost = wikiLinkHost();
char returnTo[512];

if (!returnURL || sameString(returnURL,""))
   safef(returnTo, sizeof(returnTo),
        "http://%s/cgi-bin/hgSession?hgS_doMainPage=1", hgLoginHost);
else
   safecpy(returnTo, sizeof(returnTo), returnURL);

hPrintf(
    "<div id=\"confirmationBox\" class=\"centeredContainer formBox\">"
    "\n"
    "<h2>UCSC Genome Browser</h2>"
    "<p id=\"confirmationMsg\" class=\"confirmationTxt\">An account activation email has been sent to you \n"
   "Please activate your account within 7 days.</p>"
    "\n"
    "<p><a href=\"%s\">Return</a></p>", returnURL);
cartRemove(cart, "hgLogin_helpWith");
cartRemove(cart, "hgLogin_email");
cartRemove(cart, "hgLogin_userName");
}


void sendActMailOut(char *email, char *subject, char *msg)
/* send mail to email address */
{
char *hgLoginHost = wikiLinkHost();
char cmd[4096];
safef(cmd,sizeof(cmd),
    "echo '%s' | mail -s \"%s\" %s" , msg, subject, email);
int result = system(cmd);
if (result == -1)
    {
    hPrintf(
        "<h2>UCSC Genome Browser</h2>"
        "<p align=\"left\">"
        "</p>"
        "<h3>Error emailing to: %s</h3>"
        "Click <a href=hgLogin?hgLogin.do.displayAccHelpPage=1>here</a> to return.<br>", email );
    }
else
    {
    hPrintf("<script  language=\"JavaScript\">\n"
        "<!-- \n"
        "window.location =\"http://%s/cgi-bin/hgLogin?hgLogin.do.displayActMailSuccess=1\""
        "//-->"
        "\n"
        "</script>", hgLoginHost);
    }
}

void  displayMailSuccess()
/* display mail success confirmation box */
{
hPrintf(
    "<div id=\"confirmationBox\" class=\"centeredContainer formBox\">"
    "\n"
    "<h2>UCSC Genome Browser</h2>"
    "<p id=\"confirmationMsg\" class=\"confirmationTxt\">An email has been sent to you \n"
   "containing information that you requested.</p>"
    "\n"
    "<p><a href=\"hgLogin?hgLogin.do.displayLoginPage=1\">Return to Login</a></p>");
cartRemove(cart, "hgLogin_helpWith");
cartRemove(cart, "hgLogin_email");
cartRemove(cart, "hgLogin_userName");
}

void sendMailOut(char *email, char *subject, char *msg)
/* send mail to email address */
{
char *hgLoginHost = wikiLinkHost();
char *obj = cartUsualString(cart, "hgLogin_helpWith", "");
char cmd[4096];
safef(cmd,sizeof(cmd),
    "echo '%s' | mail -s \"%s\" %s" , msg, subject, email);
int result = system(cmd);
if (result == -1)
    {
    hPrintf( 
        "<h2>UCSC Genome Browser</h2>"
        "<p align=\"left\">"
        "</p>"
        "<h3>Error emailing %s to: %s</h3>"
        "Click <a href=hgLogin?hgLogin.do.displayAccHelpPage=1>here</a> to return.<br>", 
        obj, email );
    }
else
    {
    hPrintf("<script  language=\"JavaScript\">\n"
        "<!-- \n"
        "window.location =\"http://%s/cgi-bin/hgLogin?hgLogin.do.displayMailSuccess=1\""
        "//-->"
        "\n"
        "</script>", hgLoginHost);
    }
}

void mailUsername(char *email, char *users)
/* send user name list to the email address */
{
char subject[256];
char msg[256];
char *remoteAddr=getenv("REMOTE_ADDR");

safef(subject, sizeof(subject),"Your user name at the UCSC Genome Browser");
safef(msg, sizeof(msg), 
    "Someone (probably you, from IP address %s) has requested user name associated with this email address at UCSC Genome Browser. Your user name is: \n\n  %s\n\n", 
   remoteAddr, users);
safecat (msg, sizeof(msg), signature);
sendMailOut(email, subject, msg);
}


void sendUsername(struct sqlConnection *conn, char *email)
/* email user username(s)  */
{
struct sqlResult *sr;
char **row;
char query[256];

/* find all the user names assocaited with this email address */
char user[256];
safef(query,sizeof(query),"select * from gbMembers where email='%s'", email);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct gbMembers *m = gbMembersLoad(row);
    safecpy(user, sizeof(user), m->userName);
    mailUsername(email, user);   
    }
sqlFreeResult(&sr);
}

void sendNewPwdMail(char *username, char *email, char *password)
/* send user new password */
{
char subject[256];
char msg[4096];
char *remoteAddr=getenv("REMOTE_ADDR");
safef(subject, sizeof(subject),"New temporary password for UCSC Genome Browse");
safef(msg, sizeof(msg),
    "Someone (probably you, from IP address %s) requested a new password for UCSC Genome Browser (http://genome.ucsc.edu). A temporary password for user \"%s\" has been created and was set to \"%s\". If this was your intent, you will need to log in and choose a new password now. Your temporary password will expire in 7 days.\nIf someone else made this request, or if you have remembered your password, and you no longer wish to change it, you may ignore this message and continue using your old password.\n",
    remoteAddr, username, password);
safecat (msg, sizeof(msg), signature);
sendMailOut(email, subject, msg);
}

void displayAccHelpPage(struct sqlConnection *conn)
/* draw the account help page */
{
char *email = cartUsualString(cart, "hgLogin_email", "");
char *username = cartUsualString(cart, "hgLogin_userName", "");
hPrintf("<script  language=\"JavaScript\">\n"
    "<!-- "
    "\n"
    "function toggle(value){\n"
    "if(value=='showE')\n"
    "{\n"
    " document.getElementById('usernameBox').style.display='none';\n"
    " document.getElementById('emailAddrBox').style.display='inline';\n"
    " } else {\n"
    " document.getElementById('usernameBox').style.display='inline';\n"
    " document.getElementById('emailAddrBox').style.display='none';\n"
    "}\n"
    "}\n"
    "//-->"
    "\n"
    "</script>"
    "\n");
hPrintf("<div id=\"accountHelpBox\" class=\"centeredContainer formBox\">"
    "\n"
    "<h2>UCSC Genome Browser</h2>"
    "\n"
    "<h3>Having trouble signing in?</h3>"
    "\n"
    "<form method=post action=\"hgLogin\" name=\"accountLoginForm\" id=\"acctHelpForm\">"
    "\n"
    "<p><span style='color:red;'>%s</span><p>"
    "\n", errMsg ? errMsg : "");
hPrintf("<div class=\"inputGroup\">"
    "<div class=\"acctHelpSection\"><input name=\"hgLogin_helpWith\" type=\"radio\" value=\"password\" id=\"password\" onclick=\"toggle('showU');\">"
    "<label for=\"password\" class=\"radioLabel\">I forgot my <b>password</b>. Send me a new one.</label></div>"
    "<div class=\"acctHelpSection\"><input name=\"hgLogin_helpWith\" type=\"radio\" value=\"username\" id=\"username\"  onclick=\"toggle('showE');\">"
    "<label for=\"username\" class=\"radioLabel\">I forgot my <b>username</b>. Please email it to me.</label></div>"
    "\n"
    "</div>"
    "\n");
hPrintf("<div class=\"inputGroup\" id=\"usernameBox\" style=\"display: none;\">"
    "<label for=\"emailUsername\">Username</label>"
    "<input type=\"text\" name=\"hgLogin_userName\" value=\"%s\" size=\"30\" id=\"emailUsername\">"
    "</div>"
    "\n"
    "<div class=\"inputGroup\" id=\"emailAddrBox\" style=\"display: none;\">"
    "<label for=\"emailPassword\">Email address</label>"
    "<input type=\"text\" name=\"hgLogin_email\" value=\"%s\" size=\"30\" id=\"emailPassword\">"
    "</div>"
    "\n"
    "<div class=\"formControls\">"
    "    <input type=\"submit\" name=\"hgLogin.do.accountHelp\" value=\"Continue\" class=\"largeButton\">"
    "     &nbsp;<a href=\"javascript:history.go(-1)\">Cancel</a>"
    "</div>"
    "</form>"
    "</div><!-- END - accountHelpBox -->", username, email);
cartSaveSession(cart);
}

void sendNewPassword(struct sqlConnection *conn, char *username, char *password)
/* email user new password  */
{
struct sqlResult *sr;
char query[256];
/* find email address associated with this username */
safef(query,sizeof(query),"select email from gbMembers where userName='%s'", username);
char *email = sqlQuickString(conn, query);
if (!email || sameString(email,""))
    {
    freez(&errMsg);
    errMsg = cloneString("Email address not found.");
    displayAccHelpPage(conn);
    return;
    }
sendNewPwdMail(username, email, password);
sqlFreeResult(&sr);
}

void lostPassword(struct sqlConnection *conn, char *username)
/* Generate and mail new password to user */
{
char query[256];
char *password = generateRandomPassword();
char encPwd[45] = "";
encryptNewPwd(password, encPwd, sizeof(encPwd));
safef(query,sizeof(query), "update gbMembers set lastUse=NOW(),newPassword='%s', newPasswordExpire=DATE_ADD(NOW(), INTERVAL 7 DAY), passwordChangeRequired='Y' where userName='%s'",
    sqlEscapeString(encPwd), sqlEscapeString(username));
sqlUpdate(conn, query);
sendNewPassword(conn, username, password);
return;
}

void clearNewPasswordFields(struct sqlConnection *conn, char *username)
/* clear the newPassword fields */
{
char query[256];
safef(query,sizeof(query), "update gbMembers set lastUse=NOW(),newPassword='', newPasswordExpire='', passwordChangeRequired='N' where userName='%s'",
sqlEscapeString(username));
sqlUpdate(conn, query);
cartRemove(cart, "hgLogin_changeRequired");
return;
}

void sendActivateMail(char *email, char *username, char *encToken)
/* Send activation mail with token to user*/
{
char subject[256];
char msg[4096];
char activateURL[256];
char *hgLoginHost = wikiLinkHost();
char *remoteAddr=getenv("REMOTE_ADDR");
safef(activateURL, sizeof(activateURL),
    "http://%s/cgi-bin/hgLogin?hgLogin.do.activateAccount=1&user=%s&token=%s\n",
    sqlEscapeString(hgLoginHost),
    sqlEscapeString(username),
    sqlEscapeString(encToken));
safef(subject, sizeof(subject),"UCSC Genome Browser account e-mail address confirmation");
safef(msg, sizeof(msg),
    "Someone (probably you, from IP address %s) has requested an account %s with this e-mail address on the UCSC Genome Browser.\n\nTo confirm that this account really does belong to you on the UCSC Genome Browser, open this link in your browser:\n\n%s\n\nIf this is *not* you, do not follow the link. This confirmation code will expire in 7 days.\n", 
     remoteAddr, username, activateURL);
safecat (msg, sizeof(msg), signature);
sendActMailOut(email, subject, msg);
}

void setupNewAccount(struct sqlConnection *conn, char *email, char *username)
/* Set up  new user account and send activation mail to user */
{
char query[256];
char *token = generateRandomPassword();
char *tokenMD5 = generateTokenMD5(token);
safef(query,sizeof(query), "update gbMembers set lastUse=NOW(),emailToken='%s', emailTokenExpires=DATE_ADD(NOW(), INTERVAL 7 DAY), accountActivated='N' where userName='%s'",
    sqlEscapeString(tokenMD5),
    sqlEscapeString(username)
    );
sqlUpdate(conn, query);
sendActivateMail(email, username, tokenMD5);
return;
}

void displayLoginPage(struct sqlConnection *conn)
/* draw the account login page */
{
char *username = cartUsualString(cart, "hgLogin_userName", "");
hPrintf("<div id=\"loginBox\" class=\"centeredContainer formBox\">"
    "\n"
    "<h2>UCSC Genome Browser</h2>"
    "\n"
    "<h3>Login</h3>"
    "\n"
    "<span style='color:red;'>%s</span>"
    "\n", errMsg ? errMsg : "");
hPrintf("<form method=post action=\"hgLogin\" name=\"accountLoginForm\" id=\"accountLoginForm\">"
    "\n"
    "<div class=\"inputGroup\">"
    "<label for=\"userName\">Username</label>"
    "<input type=text name=\"hgLogin_userName\" value=\"%s\" size=\"30\" id=\"userName\">"
    "</div>"
    "\n"
    "<div class=\"inputGroup\">"
    "<label for=\"password\">Password</label>"
    "<input type=password name=\"hgLogin_password\" value=\"\" size=\"30\" id=\"password\">"
    "</div>"
    "\n"
    "<div class=\"formControls\">"
    "   <input type=\"submit\" name=\"hgLogin.do.displayLogin\" value=\"Login\" class=\"largeButton\">"
    "    &nbsp;<a href=\"javascript:history.go(-1)\">Cancel</a>"
    "</div>"
    "</form>"
    "\n"
    "\n"
    "<div id=\"helpBox\">"
    "<a href=\"hgLogin?hgLogin.do.displayAccHelpPage=1\">Can't access your account?</a><br>"
    "Need an account? <a href=\"hgLogin?hgLogin.do.signupPage=1\">Sign up</a>.<br>"
    "To change password, click <a href=\"hgLogin?hgLogin.do.changePasswordPage=1\">here</a>."
    "</div><!-- END - helpBox -->"
    "</div><!-- END - loginBox -->"
    "\n"
    "\n"
    "</body>"
    "</html>", username);
cartSaveSession(cart);
}

void activateAccount(struct sqlConnection *conn)
/* activate account */
{
char query[256];
char *token = cgiUsualString("token", "");
char *username = cgiUsualString("user","");
safef(query,sizeof(query),
    "select emailToken from gbMembers where userName='%s'", username);
char *emailToken = sqlQuickString(conn, query);
if (sameString(emailToken, token))
    {
    safef(query,sizeof(query), "update gbMembers set lastUse=NOW(), dateActivated=NOW(), emailToken='', emailTokenExpires='', accountActivated='Y' where userName='%s'",
    username);
    sqlUpdate(conn, query);
    } 
else
    {
    freez(&errMsg);
    errMsg = cloneString("Token does not match.");
    }
displayLoginPage(conn);
return;
}

/* -------- functions ---- */

void changePasswordPage(struct sqlConnection *conn)
/* change password page */
{
hPrintf("<div id=\"changePwBox\" class=\"centeredContainer formBox\">"
    "\n"
    "<h2>UCSC Genome Browser</h2>"
    "\n"
    "<h3>Change Password</h3>"
    "\n"
    "<p> <span style='color:red;'>%s</span> </p>"
    "\n"
    "<form method=\"post\" action=\"hgLogin\" name=\"changePasswordForm\" id=\"changePasswordForm\">"
    "\n"
    "<div class=\"inputGroup\">"
    "<label for=\"userName\">Username</label>"
    "<input type=\"text\" name=\"hgLogin_userName\" size=\"30\" value=\"%s\" id=\"email\">"
    "</div>"
    "\n", errMsg ? errMsg : "",
    cartUsualString(cart, "hgLogin_userName", ""));
hPrintf("<div class=\"inputGroup\">"
    "\n"
    "<label for=\"currentPw\">Current Password</label>"
    "<input type=\"password\" name=\"hgLogin_password\" value=\"\" size=\"30\" id=\"currentPw\">"
    "</div>"
    "\n"
    "<div class=\"inputGroup\">"
    "<label for=\"newPw1\">New Password</label>"
    "<input type=\"password\" name=\"hgLogin_newPassword1\" value=\"\" size=\"30\" id=\"newPw\">"
    "</div>"
    "\n"
    "<div class=\"inputGroup\">"
    "<label for=\"newPw2\">Re-enter New Password</label>"
    "<input type=\"password\" name=\"hgLogin_newPassword2\" value=\"\" size=\"30\" id=\"newPw\">"
    "</div>"
    "\n"
    "<div class=\"formControls\">"
    "    <input type=\"submit\" name=\"hgLogin.do.changePassword\" value=\"Change Password\" class=\"largeButton\"> &nbsp; "
    "    <a href=\"javascript:history.go(-1)\">Cancel</a>"
    "\n"
    "</div>"
    "</form>"
    "\n"
    "</div><!-- END - changePwBox -->"
    "\n");
cartSaveSession(cart);
}

void changePassword(struct sqlConnection *conn)
/* process the change password form */
{
char query[256];
char *user = cartUsualString(cart, "hgLogin_userName", "");
char *currentPassword = cartUsualString(cart, "hgLogin_password", "");
char *newPassword1 = cartUsualString(cart, "hgLogin_newPassword1", "");
char *newPassword2 = cartUsualString(cart, "hgLogin_newPassword2", "");
char *changeRequired = cartUsualString(cart, "hgLogin_changeRequired", "");
if (!user || sameString(user,""))
    {
    freez(&errMsg);
    errMsg = cloneString("Username cannot be blank.");
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

if (!newPassword1 || sameString(newPassword1,"") || (strlen(newPassword1)<5))
    {
    freez(&errMsg);
    errMsg = cloneString("New Password must be at least 5 characters long.");
    changePasswordPage(conn);
    return;
    }
if (!newPassword2 || sameString(newPassword2,"") )
    {
    freez(&errMsg);
    errMsg = cloneString("Re-enter New Password field cannot be blank.");
    changePasswordPage(conn);
    return;
    }
if (newPassword1 && newPassword2 && !sameString(newPassword1, newPassword2))
    {
    freez(&errMsg);
    errMsg = cloneString("New passwords do not match.");
    changePasswordPage(conn);
    return;
    }
/* check username existence and is user using a new password */
char *password;
if (changeRequired && sameString(changeRequired, "YES"))
    {
    safef(query,sizeof(query), "select newPassword from gbMembers where userName='%s'", user);
    password = sqlQuickString(conn, query);
    } 
else 
    {
    safef(query,sizeof(query), "select password from gbMembers where userName='%s'", user);
    password = sqlQuickString(conn, query);
    }
if (!password)
    {
    freez(&errMsg);
    errMsg = cloneString("User not found.");
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
char encPwd[45] = "";
encryptNewPwd(newPassword1, encPwd, sizeof(encPwd));
safef(query,sizeof(query), "update gbMembers set password='%s' where userName='%s'", sqlEscapeString(encPwd), sqlEscapeString(user));
sqlUpdate(conn, query);
clearNewPasswordFields(conn, user);

hPrintf("<h2>UCSC Genome Browser</h2>"
    "<p align=\"left\">"
    "</p>"
    "<h3>Password has been changed.</h3>");
cartRemove(cart, "hgLogin_password");
cartRemove(cart, "hgLogin_newPassword1");
cartRemove(cart, "hgLogin_newPassword2");
backToDoLoginPage(1);
}

void signupPage(struct sqlConnection *conn)
/* draw the signup page */
{
hPrintf("<div id=\"signUpBox\" class=\"centeredContainer formBox\">"
    "<h2>UCSC Genome Browser</h2>"
    "\n"
    "<p>Signing up enables you to save multiple sessions and to share your sessions with others.</p>"
    "Already have an account? <a href=\"hgLogin?hgLogin.do.displayLoginPage=1\">Login</a>.<br>"
    "\n");
hPrintf("<h3>Sign Up</h3>"
    "<form method=\"post\" action=\"hgLogin\" name=\"mainForm\">"
    "<span style='color:red;'>%s</span>"
    "\n", errMsg ? errMsg : "");
hPrintf("<div class=\"inputGroup\">"
    "<label for=\"userName\">Username</label>"
    "<input type=text name=\"hgLogin_userName\" value=\"%s\" size=\"30\" id=\"userName\">"
    "</div>"
    "\n"
    "<div class=\"inputGroup\">"
    "<label for=\"emailAddr\">Email address</label>"
    "<input type=text name=\"hgLogin_email\" value=\"%s\" size=\"30\" id=\"emailAddr\">"
    "</div>"
    "\n"
    "<div class=\"inputGroup\">"
    "<label for=\"reenterEmail\">Re-enter Email address</label>"
    "<input type=text name=\"hgLogin_email2\" value=\"%s\" size=\"30\" id=\"emailCheck\">"
    "</div>"
    "\n", cartUsualString(cart, "hgLogin_userName", ""), cartUsualString(cart, "hgLogin_email", ""),
    cartUsualString(cart, "hgLogin_email2", ""));
hPrintf("<div class=\"inputGroup\">"
    "<label for=\"password\">Password <small>(must be at least 5 characters)</small></label>"
    "<input type=password name=\"hgLogin_password\" value=\"%s\" size=\"30\" id=\"password\">"
    "</div>"
    "\n"
    "<div class=\"inputGroup\">"
    "<label for=\"password\">Re-enter Password</label>"
    "<input type=password name=\"hgLogin_password2\" value=\"%s\" size=\"30\" id=\"passwordCheck\">"
    "\n"
    "</div>"
    "\n"
    "<div class=\"formControls\">"
    "    <input type=\"submit\" name=\"hgLogin.do.signup\" value=\"Sign Up\" class=\"largeButton\"> &nbsp; "
    "    <a href=\"javascript:history.go(-1)\">Cancel</a>"
    "</div>"
    "</form>"
    "</div><!-- END - signUpBox -->"
    "\n", cartUsualString(cart, "hgLogin_password", ""), cartUsualString(cart, "hgLogin_password2", ""));
cartSaveSession(cart);
hPrintf("</FORM>");
}

void signup(struct sqlConnection *conn)
/* process the signup form */
{
char query[256];
char *user = cartUsualString(cart, "hgLogin_userName", "");
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

char *email2 = cartUsualString(cart, "hgLogin_email2", "");
if (!email2 || sameString(email2,"")) 
    {
    freez(&errMsg);
    errMsg = cloneString("Email cannot be blank.");
    signupPage(conn);
    return;
    }

if (email && email2 && !sameString(email, email2))
    {
    freez(&errMsg);
    errMsg = cloneString("Email addresses do not match.");
    signupPage(conn);
    return;
    }

password = cartUsualString(cart, "hgLogin_password", "");
if (!password || sameString(password,"") || (strlen(password)<5))
    {
    freez(&errMsg);
    errMsg = cloneString("Password must be at least 5 characters long.");
    signupPage(conn);
    return;
    }

char *password2 = cartUsualString(cart, "hgLogin_password2", "");
if (!password2 || sameString(password2,"") )
    {
    freez(&errMsg);
    errMsg = cloneString("Password field cannot be blank.");
    signupPage(conn);
    return;
    }
if (password && password2 && !sameString(password, password2))
    {
    freez(&errMsg);
    errMsg = cloneString("Passwords do not match.");
    signupPage(conn);
    return;
    }

/* pass all the checks, OK to create the account now */
char encPwd[45] = "";
encryptNewPwd(password, encPwd, sizeof(encPwd));
safef(query,sizeof(query), "insert into gbMembers set "
    "userName='%s',password='%s',email='%s', "
    "lastUse=NOW(),accountActivated='N'",
    sqlEscapeString(user),sqlEscapeString(encPwd),sqlEscapeString(email));
sqlUpdate(conn, query);
/********** new signup process start *******************/
setupNewAccount(conn, email, user);
/* send out activate code mail, and display the mail confirmation box */
/* and comback here to contine back to URL */
hPrintf("<h2>UCSC Genome Browser</h2>\n"
    "<p align=\"left\">\n"
    "</p>\n"
    "<h3>User %s successfully added.</h3>\n", user);
cartRemove(cart, "hgLogin_email");
cartRemove(cart, "hgLogin_email2");
cartRemove(cart, "hgLogin_userName");
cartRemove(cart, "user");
cartRemove(cart, "token");
//backToHgSession(1);
returnToURL(1);
}

void accountHelp(struct sqlConnection *conn)
/* email user username(s) or new password */
{
char query[256];
char *email = cartUsualString(cart, "hgLogin_email", "");
char *username = cartUsualString(cart, "hgLogin_userName", "");
char *helpWith = cartUsualString(cart, "hgLogin_helpWith", "");

/* Forgot username */
if (sameString(helpWith,"username"))
    {
    if (sameString(email,""))
        {
        freez(&errMsg);
        errMsg = cloneString("Email address cannot be blank.");
        displayAccHelpPage(conn);
        return;
        } 
    else 
        {
        sendUsername(conn, email);
        return;
        }
    }
/* Forgot password */
if (sameString(helpWith,"password"))
    {
    /* validate username first */
    if (sameString(username,""))
        {
        freez(&errMsg);
        errMsg = cloneString("Username cannot be blank.");
        displayAccHelpPage(conn);
        return;
        } 
    else 
        { 
        safef(query,sizeof(query), 
            "select password from gbMembers where userName='%s'", username);
        char *password = sqlQuickString(conn, query);
        if (!password)
            {
            freez(&errMsg);
            errMsg = cloneString("Username not found.");
            displayAccHelpPage(conn);
            return;
            }
        }
    lostPassword(conn, username);
    return;
    }
displayAccHelpPage(conn);
return;
}

boolean usingNewPassword(struct sqlConnection *conn, char *userName)
/* The user is using  requested new password */
{
char query[256];
safef(query,sizeof(query), "select passwordChangeRequired from gbMembers where userName='%s'", userName);
char *change = sqlQuickString(conn, query);
if (change && sameString(change, "Y"))
    return TRUE;
else
    return FALSE;
}

void displayLoginSuccess(char *userName, int userID)
/* display login success msg, and set cookie */
{
hPrintf("<h2>UCSC Genome Browser</h2>"
    "<p align=\"left\">"
    "</p>"
    "<span style='color:red;'></span>"
    "\n");
/* Set cookies */
hPrintf("<script language=\"JavaScript\">"
    " document.write(\"Login successful, setting cookies now...\");"
    "</script>\n"
    "<script language=\"JavaScript\">"
    "document.cookie =  \"wikidb_mw1_UserName=%s; domain=ucsc.edu; expires=Thu, 31 Dec 2099, 20:47:11 UTC; path=/\"; "
    "\n"
    "document.cookie =  \"wikidb_mw1_UserID=%d; domain=ucsc.edu; expires=Thu, 31 Dec 2099, 20:47:11 UTC; path=/\";"
    " </script>"
    "\n", userName,userID);
cartRemove(cart,"hgLogin_userName");
returnToURL(1);
}

void displayLogin(struct sqlConnection *conn)
/* display and process login info */
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

/* Check user name exist and account activated */
if (!sameString(m->accountActivated,"Y"))
    {              
    freez(&errMsg);
    errMsg = cloneString("Account is not activated.");
    displayLoginPage(conn);
    return;
    }
if (checkPwd(password,m->password))
    {
    unsigned int userID=m->idx;  
    hPrintf("<h2>Login successful for user %s with id %d.\n</h2>\n"
        ,userName,userID);
    clearNewPasswordFields(conn, userName);
    displayLoginSuccess(userName,userID);
    return;
    } 
else if (usingNewPassword(conn, userName))
    {
    cartSetString(cart, "hgLogin_changeRequired", "YES");
    changePasswordPage(conn);
    } 
else
    {
    errMsg = cloneString("Invalid user name or password.");
    displayLoginPage(conn);
    return;
    }
gbMembersFree(&m);
}

void  displayLogoutSuccess()
/* display logout success msg, and reset cookie */
{
hPrintf("<h2>UCSC Genome Browser Sign Out</h2>"
    "<p align=\"left\">"
    "</p>"
    "<span style='color:red;'></span>"
    "\n");
hPrintf("<script language=\"JavaScript\">"
    "document.cookie =  \"wikidb_mw1_UserName=; domain=ucsc.edu; expires=Thu, 01-Jan-70 00:00:01 GMT; path=/\"; "
    "\n"
    "document.cookie =  \"wikidb_mw1_UserID=; domain=ucsc.edu; expires=Thu, 01-Jan-70 00:00:01 GMT; path=/\";"
    "</script>\n");
/* return to "returnto" URL */
returnToURL(1);
}

void doMiddle(struct cart *theCart)
/* Write the middle parts of the HTML page.
 * This routine sets up some globals and then
 * dispatches to the appropriate page-maker. */
{
struct sqlConnection *conn = hConnectCentral();
cart = theCart;

if (cartVarExists(cart, "hgLogin.do.changePasswordPage"))
    changePasswordPage(conn);
else if (cartVarExists(cart, "hgLogin.do.changePassword"))
    changePassword(conn);
else if (cartVarExists(cart, "hgLogin.do.displayAccHelpPage"))
    displayAccHelpPage(conn);
else if (cartVarExists(cart, "hgLogin.do.accountHelp"))
    accountHelp(conn);
else if (cartVarExists(cart, "hgLogin.do.activateAccount"))
    activateAccount(conn);
else if (cartVarExists(cart, "hgLogin.do.displayActMailSuccess"))
    displayActMailSuccess();
else if (cartVarExists(cart, "hgLogin.do.displayMailSuccess"))
    displayMailSuccess();
else if (cartVarExists(cart, "hgLogin.do.displayLoginPage"))
    displayLoginPage(conn);
else if (cartVarExists(cart, "hgLogin.do.displayLogin"))
    displayLogin(conn);
else if (cartVarExists(cart, "hgLogin.do.displayLogout"))
    displayLogoutSuccess();
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
  "hgLogin - Stand alone CGI to handle UCSC Genome Browser login.\n"
  "usage:\n"
  "    hgLogin <various CGI settings>\n"
  );
}

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(100000000);
cgiSpoof(&argc, argv);
htmlSetStyleSheet("/style/userAccounts.css");
htmlSetStyle(htmlStyleUndecoratedLink);
htmlSetBgColor(HG_CL_OUTSIDE);
htmlSetFormClass("accountScreen");
oldCart = hashNew(10);
cartHtmlShell("Login - UCSC Genome Browser", doMiddle, hUserCookie(), excludeVars, oldCart);
return 0;
}
