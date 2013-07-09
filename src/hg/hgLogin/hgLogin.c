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
#include "net.h"
#include "wikiLink.h"
#include "hgLogin.h"
#include "gbMembers.h"
#include "versionInfo.h"
#include "mailViaPipe.h"

/* ---- Global variables. ---- */
char msg[4096] = "";
char *incorrectUsernameOrPassword="The username or password you entered is incorrect.";
char *incorrectUsername="The username you entered is incorrect.";
/* The excludeVars are not saved to the cart. */
char *excludeVars[] = { "submit", "Submit", "debug", "fixMembers", "update", 
     "hgLogin_password", "hgLogin_password2", "hgLogin_newPassword1",
     "hgLogin_newPassword2", NULL };
struct cart *cart;	/* This holds cgi and other variables between clicks. */
char *database;		/* Name of genome database - hg15, mm3, or the like. */
struct hash *oldCart;	/* Old cart hash. */
char *errMsg;           /* Error message to show user when form data rejected */
char brwName[64];
char brwAddr[256];
char signature[256];
char returnAddr[256];
/* ---- Global helper functions ---- */
char *cookieNameForUserName()
/* Return the cookie name used for logged in user name like 'wikidb_mw1_UserName' */
{
if isEmpty(cfgOption(CFG_COOKIIENAME_USERNAME))
    return cloneString("NULL_cookieNameUserName");
else
    return cloneString(cfgOption(CFG_COOKIIENAME_USERNAME));
}

char *cookieNameForUserID()
/* Return the cookie name used for logged in user ID like 'wikidb_mw1_UserID' */
{
if isEmpty(cfgOption(CFG_COOKIIENAME_USERID))
    return cloneString("NULL_cookieNameUserID");
else
    return cloneString(cfgOption(CFG_COOKIIENAME_USERID));
}



char *browserName()
/* Return the browser name like 'UCSC Genome Browser' */
{
if isEmpty(cfgOption(CFG_LOGIN_BROWSER_NAME))
    return cloneString("NULL_browserName");
else
    return cloneString(cfgOption(CFG_LOGIN_BROWSER_NAME));
}

char *browserAddr()
/* Return the browser address like 'http://genome.ucsc.edu' */
{
if isEmpty(cfgOption(CFG_LOGIN_BROWSER_ADDR))
    return cloneString("NULL_browserAddr");
else
    return cloneString(cfgOption(CFG_LOGIN_BROWSER_ADDR));
}

char *mailSignature()
/* Return the signature to be used by outbound mail or NULL. Allocd here. */
{
if isEmpty(cfgOption(CFG_LOGIN_MAIL_SIGNATURE))
    return cloneString("NULL_mailSignature");
else
    return cloneString(cfgOption(CFG_LOGIN_MAIL_SIGNATURE));
}

char *mailReturnAddr()
/* Return the return addr. to be used by outbound mail or NULL. Allocd here. */
{
if isEmpty(cfgOption(CFG_LOGIN_MAIL_RETURN_ADDR))
    return cloneString("NULL_mailReturnAddr");
else
    return cloneString(cfgOption(CFG_LOGIN_MAIL_RETURN_ADDR));
}

/* ---- password functions depend on optionally installed openssl lib ---- */
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

int spc_email_isvalid(const char *address) {
/* Check the format of an email address syntactically. Return 1 if valid, else 0 */
/* Code copied from the book: 
"Secure Programming Cookbook for C and C++"
By: John Viega; Matt Messier
Publisher: O'Reilly Media, Inc.
Pub. Date: July 14, 2003
Print ISBN-13: 978-0-596-00394-4
*/
int  count = 0;
const char *c, *domain;
static char *rfc822_specials = "()<>@,;:\\\"[]";

/* first we validate the name portion (name@domain) */
for (c = address;  *c;  c++) 
    {
    if (*c == '\"' && (c == address || *(c - 1) == '.' || *(c - 1) ==  '\"')) 
        {
        while (*++c) 
            {
            if (*c == '\"') break;
            if (*c == '\\' && (*++c == ' ')) continue;
            if (*c <= ' ' || *c >= 127) return 0;
            }
         if (!*c++) return 0;
         if (*c == '@') break;
         if (*c != '.') return 0;
         continue;
        }
    if (*c == '@') break;
    if (*c <= ' ' || *c >= 127) return 0;
    if (strchr(rfc822_specials, *c)) return 0;
    }
if (c == address || *(c - 1) == '.') return 0;

/* next we validate the domain portion (name@domain) */
if (!*(domain = ++c)) return 0;
do 
    {
    if (*c == '.') 
        {
        if (c == domain || *(c - 1) == '.') return 0;
        count++;
        }
    if (*c <= ' ' || *c >= 127) return 0;
    if (strchr(rfc822_specials, *c)) return 0;
    } while (*++c);

return (count >= 1);
}

char *getReturnToURL()
/* get URL passed in with returnto URL */
{
char *returnURL = cartUsualString(cart, "returnto", "");
char *hgLoginHost = wikiLinkHost();
char returnTo[2048];
if (!returnURL || sameString(returnURL,""))
   safef(returnTo, sizeof(returnTo),
        "http://%s/cgi-bin/hgSession?hgS_doMainPage=1", hgLoginHost);
else
   safecpy(returnTo, sizeof(returnTo), returnURL);
return cloneString(returnTo);
}

void returnToURL(int delay)
/* delay for delay mill-seconds then return to the "returnto" URL */
{
char *returnURL = getReturnToURL();
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
char *returnURL = getReturnToURL(); 
hPrintf(
    "<div id=\"confirmationBox\" class=\"centeredContainer formBox\">"
    "\n"
    "<h2>%s</h2>", brwName);
hPrintf(
    "<p id=\"confirmationMsg\" class=\"confirmationTxt\">A confirmation email has been sent to you. \n"
    "Please click the confirmation link in the email to activate your account.</p>"
    "\n"
    "<p><a href=\"%s\">Return</a></p>", returnURL);
cartRemove(cart, "hgLogin_email");
cartRemove(cart, "hgLogin_userName");
}

void sendActMailOut(char *email, char *subject, char *msg)
/* send mail to email address */
{
char *hgLoginHost = wikiLinkHost();
int result;
result = mailViaPipe(email, subject, msg, returnAddr);

if (result == -1)
    {
    hPrintf(
        "<h2>%s</h2>", brwName);
    hPrintf(
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
char *sendMailTo = cartUsualString(cart, "hgLogin_sendMailTo", "");
char *sendMailContain = cartUsualString(cart, "hgLogin_sendMailContain", "");
hPrintf(
    "<div id=\"confirmationBox\" class=\"centeredContainer formBox\">"
    "<h2>%s</h2>", brwName);
hPrintf(
    "<p id=\"confirmationMsg\" class=\"confirmationTxt\">All usernames on file (if any) for <B>%s</B> "
  "have been sent to that address.<BR><BR>"
    "  If <B>%s</B> is not your registered email address, you will not receive an email."
    " If you can't find the message we sent you, please contact %s for help.</p>", sendMailTo, sendMailTo, returnAddr);
hPrintf(
    "<p><a href=\"hgLogin?hgLogin.do.displayLoginPage=1\">Return to Login</a></p>");
cartRemove(cart, "hgLogin_helpWith");
cartRemove(cart, "hgLogin_email");
cartRemove(cart, "hgLogin_userName");
cartRemove(cart, "hgLogin_sendMailTo");
cartRemove(cart, "hgLogin_sendMailContain");
}

void  displayMailSuccessPwd()
/* display mail success confirmation box */
{
char *username = cgiUsualString("user","");
hPrintf(
    "<div id=\"confirmationBoxPwd\" class=\"centeredContainer formBox\">"
    "<h2>%s</h2>", brwName);
hPrintf(
    "<p id=\"confirmationMsgPwd\" class=\"confirmationTxt\">An email containing password reset information has been sent to the registered email address of <B>%s</B>.<BR><BR>"
    " If you do not receive an email, please contact genome-www@soe.ucsc.edu for help.</p>", username);
hPrintf(
    "<p><a href=\"hgLogin?hgLogin.do.displayLoginPage=1\">Return to Login</a></p>");
cartRemove(cart, "hgLogin_helpWith");
cartRemove(cart, "hgLogin_email");
cartRemove(cart, "hgLogin_userName");
cartRemove(cart, "hgLogin_sendMailTo");
cartRemove(cart, "hgLogin_sendMailContain");
}

void sendMailOut(char *email, char *subject, char *msg)
/* send mail to email address */
{
char *hgLoginHost = wikiLinkHost();
char *obj = cartUsualString(cart, "hgLogin_helpWith", "");
int result;
result = mailViaPipe(email, subject, msg, returnAddr);
if (result == -1)
    {
    hPrintf( 
        "<h2>%s</h2>", brwName);
    hPrintf(
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
char msg[4096];
char *remoteAddr=getenv("REMOTE_ADDR");

safef(subject, sizeof(subject),"Your user name at the %s", brwName);
safef(msg, sizeof(msg), 
    "  Someone (probably you, from IP address %s) has requested user name(s) associated with this email address at the %s: \n\n  %s\n\n%s\n%s", 
   remoteAddr, brwName, users, signature, returnAddr);
sendMailOut(email, subject, msg);
}

void sendUsername(struct sqlConnection *conn, char *email)
/* email user username(s)  */
{
struct sqlResult *sr;
char **row;
char query[256];

/* find all the user names assocaited with this email address */
char userList[256]="";
sqlSafef(query,sizeof(query),"SELECT * FROM gbMembers WHERE email='%s'", email);
sr = sqlGetResult(conn, query);
int numUser = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct gbMembers *m = gbMembersLoad(row);
    if (numUser >= 1)
        safecat(userList, sizeof(userList), ", ");
    safecat(userList, sizeof(userList), m->userName);
    numUser += 1;
    }
sqlFreeResult(&sr);
mailUsername(email, userList);
}

void sendPwdMailOut(char *email, char *subject, char *msg, char *username)
/* send password reset mail to user at registered email address */
{
char *hgLoginHost = wikiLinkHost();
char *obj = cartUsualString(cart, "hgLogin_helpWith", "");
int result;
result = mailViaPipe(email, subject, msg, returnAddr);
if (result == -1)
    {
    hPrintf(
        "<h2>%s</h2>", brwName);
    hPrintf(
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
        "window.location =\"http://%s/cgi-bin/hgLogin?hgLogin.do.displayMailSuccessPwd=1&user=%s\""
        "//-->"
        "\n"
        "</script>", hgLoginHost, username);
    }
}

void sendNewPwdMail(char *username, char *email, char *password)
/* send user new password */
{
char subject[256];
char msg[4096];
char *remoteAddr=getenv("REMOTE_ADDR");

safef(subject, sizeof(subject),"New temporary password for your account at the %s", brwName);
safef(msg, sizeof(msg),
    "  Someone (probably you, from IP address %s) requested a new password for the %s (%s). A temporary password for user \"%s\" has been created and was set to \"%s\". If this was your intent, you will need to log in and choose a new password now. Your temporary password will expire in 7 days.\n\n  If someone else made this request, or if you have remembered your password, and you no longer wish to change it, you may ignore this message and continue using your old password.\n\n%s\n%s",
    remoteAddr, brwName, brwAddr, username, password, signature, returnAddr);
sendPwdMailOut(email, subject, msg, username);
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
    "<h2>%s</h2>"
    "\n", brwName);
hPrintf("<h3>Having trouble signing in?</h3>"
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
    "     &nbsp;<a href=\"%s\">Cancel</a>"
    "</div>"
    "</form>"
    "</div><!-- END - accountHelpBox -->", username, email, getReturnToURL());
cartSaveSession(cart);
}

void sendNewPassword(struct sqlConnection *conn, char *username, char *password)
/* email user new password  */
{
struct sqlResult *sr;
char query[256];
/* find email address associated with this username */
sqlSafef(query,sizeof(query),"SELECT email FROM gbMembers WHERE userName='%s'", username);
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
sqlSafef(query,sizeof(query), "UPDATE gbMembers SET lastUse=NOW(),newPassword='%s', newPasswordExpire=DATE_ADD(NOW(), INTERVAL 7 DAY), passwordChangeRequired='Y' WHERE userName='%s'",
    encPwd, username);
sqlUpdate(conn, query);
sendNewPassword(conn, username, password);
return;
}

void clearNewPasswordFields(struct sqlConnection *conn, char *username)
/* clear the newPassword fields */
{
char query[256];
sqlSafef(query,sizeof(query), "UPDATE gbMembers SET lastUse=NOW(),newPassword='', newPasswordExpire='', passwordChangeRequired='N' WHERE userName='%s'",
    username);
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
char *urlEncodedUsername=replaceChars(username," ","%20");

safef(activateURL, sizeof(activateURL),
    "http://%s/cgi-bin/hgLogin?hgLogin.do.activateAccount=1&user=%s&token=%s\n",
    hgLoginHost,
    cgiEncode(urlEncodedUsername),
    cgiEncode(encToken));
safef(subject, sizeof(subject),"%s account e-mail address confirmation", brwName);
safef(msg, sizeof(msg),
    "  Someone (probably you, from IP address %s) has requested an account %s with this e-mail address on the %s.\nTo confirm that this account really does belong to you on the %s, open this link in your browser:\n\n%s\n\nIf this is *not* you, do not follow the link. This confirmation code will expire in 7 days.\n\n%s\n%s", 
     remoteAddr, username, brwName, brwName, activateURL, signature, returnAddr);
sendActMailOut(email, subject, msg);
}

void setupNewAccount(struct sqlConnection *conn, char *email, char *username)
/* Set up  new user account and send activation mail to user */
{
char query[256];
char *token = generateRandomPassword();
char *tokenMD5 = generateTokenMD5(token);
sqlSafef(query,sizeof(query), "UPDATE gbMembers SET lastUse=NOW(),emailToken='%s', emailTokenExpires=DATE_ADD(NOW(), INTERVAL 7 DAY), accountActivated='N' WHERE userName='%s'",
    tokenMD5,
    username
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
    "<h2>%s</h2>"
    "\n", brwName);
hPrintf(
    "<h3>Login</h3>"
    "\n");
if (errMsg && sameString(errMsg, "Your account has been activated."))
    hPrintf("<span style='color:green;'>%s</span>\n", errMsg ? errMsg : "");
else
    hPrintf("<span style='color:red;'>%s</span>\n", errMsg ? errMsg : "");
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
    "    &nbsp;<a href=\"%s\">Cancel</a>"
    "</div>"
    "</form>"
    "\n"
    "\n"
    "<div id=\"helpBox\">"
    "<a href=\"hgLogin?hgLogin.do.displayAccHelpPage=1\">Can't access your account?</a><br>"
    "Need an account? <a href=\"hgLogin?hgLogin.do.signupPage=1\">Sign up</a>.<br>"
    "</div><!-- END - helpBox -->"
    "</div><!-- END - loginBox -->"
    "\n"
    "\n"
    "</body>"
    "</html>", username, getReturnToURL());
cartSaveSession(cart);
}

void activateAccount(struct sqlConnection *conn)
/* activate account */
{
char query[256];
char *token = cgiUsualString("token", "");
char *username = cgiUsualString("user","");
sqlSafef(query,sizeof(query),
    "SELECT emailToken FROM gbMembers WHERE userName='%s'", username);
char *emailToken = sqlQuickString(conn, query);
if (sameString(emailToken, token))
    {
    sqlSafef(query,sizeof(query), "UPDATE gbMembers SET lastUse=NOW(), dateActivated=NOW(), emailToken='', emailTokenExpires='', accountActivated='Y' WHERE userName='%s'",
    username);
    sqlUpdate(conn, query);
    freez(&errMsg);
    errMsg = cloneString("Your account has been activated.");
    } 
else
    {
    freez(&errMsg);
    errMsg = cloneString("Token does not match.");
    }
cartSetString(cart, "hgLogin_userName", username);

displayLoginPage(conn);
return;
}

/* -------- functions ---- */

void changePasswordPage(struct sqlConnection *conn)
/* change password page */
{
hPrintf("<div id=\"changePwBox\" class=\"centeredContainer formBox\">"
    "\n"
    "<h2>%s</h2>", brwName);
hPrintf(
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
    "    <a href=\"%s\">Cancel</a>"
    "\n"
    "</div>"
    "</form>"
    "\n"
    "</div><!-- END - changePwBox -->"
    "\n", getReturnToURL());
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
    sqlSafef(query,sizeof(query), "SELECT newPassword FROM gbMembers WHERE userName='%s'", user);
    password = sqlQuickString(conn, query);
    } 
else 
    {
    sqlSafef(query,sizeof(query), "SELECT password FROM gbMembers WHERE userName='%s'", user);
    password = sqlQuickString(conn, query);
    }
if (!password)
    {
    freez(&errMsg);
    errMsg = cloneString(incorrectUsername);
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
sqlSafef(query,sizeof(query), "UPDATE gbMembers SET password='%s' WHERE userName='%s'", encPwd, user);
sqlUpdate(conn, query);
clearNewPasswordFields(conn, user);

hPrintf("<h2>%s</h2>", brwName);
hPrintf(
    "<p align=\"left\">"
    "</p>"
    "<h3>Password has been changed.</h3>");
cartRemove(cart, "hgLogin_password");
cartRemove(cart, "hgLogin_newPassword1");
cartRemove(cart, "hgLogin_newPassword2");
returnToURL(150);
}

void signupPage(struct sqlConnection *conn)
/* draw the signup page */
{
hPrintf("<div id=\"signUpBox\" class=\"centeredContainer formBox\">"
    "<h2>%s</h2>", brwName);
hPrintf(
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
    "    <a href=\"%s\">Cancel</a>"
    "</div>"
    "</form>"
    "</div><!-- END - signUpBox -->",
    cartUsualString(cart, "hgLogin_password", ""), 
    cartUsualString(cart, "hgLogin_password2", ""),
    getReturnToURL());
cartSaveSession(cart);
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
/* Make sure the escaped usrename is less than 32 characters */
if (strlen(user) > 32)
    {
    freez(&errMsg);
    errMsg = cloneString("Encoded username longer than 32 characters.");
    signupPage(conn);
    return;
    }

sqlSafef(query,sizeof(query), "SELECT password FROM gbMembers WHERE userName='%s'", user);

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

if (spc_email_isvalid(email) == 0)
    {
    freez(&errMsg);
    errMsg = cloneString("Invalid email address format.");
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
sqlSafef(query,sizeof(query), "INSERT INTO gbMembers SET "
    "userName='%s',password='%s',email='%s', "
    "lastUse=NOW(),accountActivated='N'",
    user,encPwd,email);
sqlUpdate(conn, query);
setupNewAccount(conn, email, user);
/* send out activate code mail, and display the mail confirmation box */
hPrintf("<h2>%s</h2>", brwName);
hPrintf(
    "<p align=\"left\">\n"
    "</p>\n"
    "<h3>User %s successfully added.</h3>\n", user);
cartRemove(cart, "hgLogin_email");
cartRemove(cart, "hgLogin_email2");
cartRemove(cart, "hgLogin_userName");
cartRemove(cart, "user");
cartRemove(cart, "token");
returnToURL(150);
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
    else if (spc_email_isvalid(email) == 0)
        {
        freez(&errMsg);
        errMsg = cloneString("Invalid email address format.");
        displayAccHelpPage(conn);
        return;
        }
    else 
        {
        sqlSafef(query,sizeof(query),
            "SELECT password FROM gbMembers WHERE email='%s'", email);
        char *password = sqlQuickString(conn, query);
        cartSetString(cart, "hgLogin_sendMailTo", email);
        cartSetString(cart, "hgLogin_sendMailContain", "username(s)");
        if (!password) /* Email address not found */
            {
            displayMailSuccess();
            return;
            }
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
        sqlSafef(query,sizeof(query), 
            "SELECT password FROM gbMembers WHERE userName='%s'", username);
        char *password = sqlQuickString(conn, query);
        if (!password)
            {
            freez(&errMsg);
            errMsg = cloneString(incorrectUsername);
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

boolean usingNewPassword(struct sqlConnection *conn, char *userName, char *password)
/* The user is using  requested new password */
{
char query[256];
sqlSafef(query,sizeof(query), "SELECT passwordChangeRequired FROM gbMembers WHERE userName='%s'", userName);
char *change = sqlQuickString(conn, query);
sqlSafef(query,sizeof(query), "SELECT newPassword FROM gbMembers WHERE userName='%s'", userName);
char *newPassword = sqlQuickString(conn, query);
if (change && sameString(change, "Y") && checkPwd(password, newPassword))
    return TRUE;
else
    return FALSE;
}

char *getCookieDomainName()
/* Return domain name to be used by the cookies or NULL. Allocd here.   */
/* Return central.domain if returnToURL is also in the same domain.     */
/* else return the domain in returnTo URL generated by remote hgSession.*/
{
char *centralDomain=cloneString(cfgOption(CFG_CENTRAL_DOMAIN));
char *returnURL = getReturnToURL();
char returnToDomain[256];

/* parse the URL */
struct netParsedUrl rtpu;
netParseUrl(returnURL, &rtpu);
safecpy(returnToDomain, sizeof(returnToDomain), rtpu.host);
if (endsWith(returnToDomain,centralDomain))
    return centralDomain;
else
    return cloneString(returnToDomain);
}

void displayLoginSuccess(char *userName, int userID)
/* display login success msg, and set cookie */
{
hPrintf("<h2>%s</h2>", brwName);
hPrintf(
    "<p align=\"left\">"
    "</p>"
    "<span style='color:red;'></span>"
    "\n");
/* Set cookies */
char *domainName=getCookieDomainName();
char *userNameCookie=cookieNameForUserName();
char *userIDCookie=cookieNameForUserID();
hPrintf("<script language=\"JavaScript\">"
    " document.write(\"Login successful, setting cookies now...\");"
    "</script>\n"
    "<script language=\"JavaScript\">"
    "document.cookie = \"%s=%s; domain=%s; expires=Thu, 30-Dec-2037 23:59:59 GMT; path=/;\";"
    "\n"
    "document.cookie = \"%s=%d; domain=%s; expires=Thu, 30-Dec-2037 23:59:59 GMT; path=/;\";"
    " </script>"
    "\n", userNameCookie, userName, domainName, userIDCookie, userID, domainName);
cartRemove(cart,"hgLogin_userName");
returnToURL(150);
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

sqlSafef(query,sizeof(query),"SELECT * FROM gbMembers WHERE userName='%s'", userName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    {
    freez(&errMsg);
    errMsg = cloneString(incorrectUsernameOrPassword);
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
else if (usingNewPassword(conn, userName, password))
    {
    cartSetString(cart, "hgLogin_changeRequired", "YES");
    changePasswordPage(conn);
    } 
else
    {
    errMsg = cloneString(incorrectUsernameOrPassword);
    displayLoginPage(conn);
    return;
    }
gbMembersFree(&m);
}

void  displayLogoutSuccess()
/* display logout success msg, and reset cookie */
{
hPrintf("<h2>%s Sign Out</h2>", brwName);
hPrintf(
    "<p align=\"left\">"
    "</p>"
    "<span style='color:red;'></span>"
    "\n");
char *domainName=getCookieDomainName();
char *userNameCookie=cookieNameForUserName();
char *userIDCookie=cookieNameForUserID();
hPrintf("<script language=\"JavaScript\">"
    "document.cookie = \"%s=; domain=%s; expires=Thu, 1-Jan-1970 0:0:0 GMT; path=/;\";"
    "\n"
    "document.cookie = \"%s=; domain=%s; expires=Thu, 1-Jan-1970 0:0:0 GMT; path=/;\";"
    "</script>\n", userNameCookie, domainName, userIDCookie, domainName);
/* return to "returnto" URL */
returnToURL(150);
}

void doMiddle(struct cart *theCart)
/* Write the middle parts of the HTML page.
 * This routine sets up some globals and then
 * dispatches to the appropriate page-maker. */
{
struct sqlConnection *conn = hConnectCentral();
cart = theCart;
safecpy(brwName,sizeof(brwName), browserName());
safecpy(brwAddr,sizeof(brwAddr), browserAddr());
safecpy(signature,sizeof(signature), mailSignature());
safecpy(returnAddr,sizeof(returnAddr), mailReturnAddr());

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
else if (cartVarExists(cart, "hgLogin.do.displayMailSuccessPwd"))
    displayMailSuccessPwd();
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
  "hgLogin - Stand alone CGI to handle Genome Browser login.\n"
  "usage:\n"
  "    hgLogin <various CGI settings>\n"
  );
}

int main(int argc, char *argv[])
/* Process command line. */
{
long enteredMainTime = clock1000();
pushCarefulMemHandler(100000000);
cgiSpoof(&argc, argv);
htmlSetStyleSheet("/style/userAccounts.css");
htmlSetStyle(htmlStyleUndecoratedLink);
htmlSetBgColor(HG_CL_OUTSIDE);
htmlSetFormClass("accountScreen");
oldCart = hashNew(10);
cartHtmlShell("Login - UCSC Genome Browser", doMiddle, hUserCookie(), excludeVars, oldCart);
cgiExitTime("hgLogin", enteredMainTime);
return 0;
}
