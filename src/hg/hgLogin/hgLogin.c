/* hgLogin - Administer UCSC Genome Browser membership - signup, lost password, etc. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include <openssl/evp.h>
#include <openssl/opensslv.h>
#include <openssl/md5.h>

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
#include "oauthLogin.h"
#include "versionInfo.h"
#include "mailViaPipe.h"
#include "dystring.h"
#include "autoUpgrade.h"
#include "hCommon.h"
#include "botDelay.h"
#include "errCatch.h"

#define EMAILSEP ";"

/* ---- Global variables. ---- */
char msg[4096] = "";
char *incorrectUsernameOrPassword="The username or password you entered is incorrect.";
char *incorrectUsername="The username you entered is incorrect.";
/* The excludeVars are not saved to the cart. */
char *excludeVars[] = { "submit", "Submit", "debug", "fixMembers", "update",
     "hgLogin_password", "hgLogin_password2", "hgLogin_newPassword1",
     "hgLogin_newPassword2", "hgLogin_newEmail1", "hgLogin_newEmail2",
     "code", "state", "provider", "user", "token", NULL };
struct cart *cart;	/* This holds cgi and other variables between clicks. */
char *database;		/* Name of genome database - hg15, mm3, or the like. */
struct hash *oldCart;	/* Old cart hash. */
char *errMsg = NULL;    /* Error message to show user when form data rejected */
char brwName[64];
char brwAddr[256];
char signature[256];
char returnAddr[256];
char *hgLoginUrl = NULL; /* full absolute URL to hgLogin as seen from browser,
    e.g. http://genome.ucsc.edu/cgi-bin/hgLogin. Can be a relative URL /cgi-bin/hgLogin if
    hg.conf login.relativeLink is on. */
boolean pwdEyeIconEnabled = TRUE; /* show/hide eye icon on password fields;
    set from hg.conf login.pwdEyeIcon in doMiddle() */

/* for earlyBotCheck() function at the beginning of main() */
#define delayFraction   1.0    /* standard penalty is 1.0 for most CGIs */

/* Forward declarations for functions used before their definitions. */
static void printSocialButtons(boolean dividerAbove, boolean dividerBelow, char *action);
static void printEmailLinkButton();
static boolean emailLinkEnabled();
static void printUsernameNote();
void emailLinkPage(struct sqlConnection *conn);
void displayLoginPage(struct sqlConnection *conn);
void displayAccHelpPage(struct sqlConnection *conn);
void completeAccountPage(struct sqlConnection *conn);
void sendEmailLink(struct sqlConnection *conn);

/* ---- Global helper functions ---- */
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
/* Return the return addr. to be used by outbound mail or NULL. Allocd here. 
 * If set to "NOEMAIL" then no email will be sent and the account is activated right away.
 * */
{
if isEmpty(cfgOption(CFG_LOGIN_MAIL_RETURN_ADDR))
    return cloneString("NULL_mailReturnAddr");
else
    return cloneString(cfgOption(CFG_LOGIN_MAIL_RETURN_ADDR));
}

/* ---- password functions depend on installed openssl lib ---- */



void md5It(unsigned char *input, int inputSize, unsigned char *output)
/* handle function deprecated by newer versions of openssl */
{ 
#if OPENSSL_VERSION_NUMBER >= 0x30000000L   // > #3.0
EVP_Q_digest(NULL, "MD5", NULL, input, inputSize, output, NULL);
#else
MD5(input, inputSize, output);
#endif  
}

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
md5It((unsigned char *)password, strlen(password), result1);
for(i = 0; i < MD5_DIGEST_LENGTH; i++)
    {
    sprintf(&firstMD5[i*2], "%02x", result1[i]);
    }   
// add the salt with "-" 
char saltDashMD5[256];
strcpy(saltDashMD5,salt);
strcat(saltDashMD5,"-");
strcat(saltDashMD5,firstMD5);
md5It((unsigned char *) saltDashMD5, strlen(saltDashMD5), result2);
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
md5It((unsigned char *) token, strlen(token), result);
// Convert the tokenMD5 value to string
for(i = 0; i < MD5_DIGEST_LENGTH; i++)
    {
    sprintf(&tokenMD5[i*2], "%02x", result[i]);
    }
return cloneString(tokenMD5);
}

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

struct dyString *getLoginCookieJS(char *userName, uint idx)
/* returns javascript statements that set the cookies associated with
 * logging in as a particular user */
{
struct dyString *result = dyStringNew(1024);
struct slName *newCookies = loginLoginUser(userName, idx), *sl;
for (sl = newCookies;  sl != NULL;  sl = sl->next)
    dyStringPrintf(result, " document.cookie = '%s';", sl->name);
return result; 
}

static boolean isValidReturnUrl(char *returnUrl)
/* Verify that returnUrl startswith an hg.conf approved set of hosts. */
{
struct slName *approvedHosts = slNameListFromComma(cfgOptionDefault(CFG_APPROVED_HOSTS, NULL));
slAddHead(&approvedHosts, slNameNew(hLoginHostCgiBinUrl()));
if (approvedHosts)
    {
    struct slName *approvedStart;
    for (approvedStart = approvedHosts; approvedStart != NULL; approvedStart = approvedStart->next)
        {
        if (startsWith(approvedStart->name, returnUrl))
            return TRUE;
        }
    }
return FALSE;
}

char *getReturnToURL()
/* get URL from cart var returnto; if empty, make URL to hgSession on login host.  */
{
char *returnURL = cartUsualString(cart, "returnto", "");
char returnTo[2048];
  
if (!returnURL || sameString(returnURL,""))
    safef(returnTo, sizeof(returnTo), "%shgSession?hgS_doMainPage=1", hLoginHostCgiBinUrl());
else if (cfgOptionDefault(CFG_APPROVED_HOSTS, NULL))
    {
    if (isValidReturnUrl(returnURL))
        safecpy(returnTo, sizeof(returnTo), returnURL);
    else
        {
        hDumpStackDisallow();
        errAbort("Error: Invalid returnto URL. Please send email to genome-www@soe.ucsc.edu "
                "with the returnto argument from the URL (or just the full URL) so we can "
                "fix this.");
        }
    }
else
    safecpy(returnTo, sizeof(returnTo), returnURL);
return cloneString(returnTo);
}

void returnToURL(int delay)
/* delay for delay mill-seconds then return to the "returnto" URL */
{
char *returnURL = getReturnToURL();
jsInlineF(
    "setTimeout(function(){location='%s';}, %d);\n"
    , returnURL, delay);
}

static void redirectToLoginPage(char *paramStr)
/* redirect to hgLogin page with given parameter string */
{
jsInlineF(
    "window.location ='%s?%s';\n"
    , hgLoginUrl, paramStr);
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
    "<p>You may have to look in your spam folder for an email from genome-www@soe.ucsc.edu, "
    "especially if you use Microsoft Outlook or Hotmail.</p>"
    "\n"
    "<p><a href=\"%s\">Return</a></p>", returnURL);
cartRemove(cart, "hgLogin_email");
cartRemove(cart, "hgLogin_userName");
}

void sendActMailOut(char *email, char *subject, char *msg)
/* send mail to email address */
{
int result;

result = mailViaPipeBounce(email, subject, msg, returnAddr);

if (result == -1)
    {
    hPrintf(
        "<h2>%s</h2>", brwName);
    hPrintf(
        "<p align=\"left\">"
        "</p>"
        "<h3>Error emailing to: %s</h3>"
        "Click <a href=%s?hgLogin.do.displayAccHelpPage=1>here</a> to return.<br>",
        hgLoginUrl, email );
    exit(0);
    }
}

void  displayMailSuccess()
/* display mail success confirmation box */
{
char *sendMailTo = cartUsualString(cart, "hgLogin_sendMailTo", "");
hPrintf(
    "<div id=\"confirmationBox\" class=\"centeredContainer formBox\">"
    "<h2>%s</h2>", brwName);
hPrintf(
    "<p id=\"confirmationMsg\" class=\"confirmationTxt\">All usernames on file (if any) for <B>%s</B> "
  "have been sent to that address.<BR><BR>"
    "  If <B>%s</B> is not your registered email address, you will not receive an email."
    " If you can't find the message we sent you, please contact %s for help.</p>", sendMailTo, sendMailTo, returnAddr);
hPrintf("<p><a href=\"%s?hgLogin.do.displayLoginPage=1\">Return to Login</a></p>\n",
        hgLoginUrl);
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
char *contactAddr = returnAddr;
if (sameString(returnAddr, "NOEMAIL"))
    contactAddr = "the administrator of this Genome Browser Mirror";

hPrintf(
    "<p id=\"confirmationMsgPwd\" class=\"confirmationTxt\">An email containing password reset information has been sent to the registered email address of <B>%s</B>.<BR><BR>"
    " If you do not receive an email, please contact %s for help.</p>", username, contactAddr);

if (sameString(returnAddr, "NOEMAIL"))
    hPrintf("<p>If you still have questions, you can contact the Genome Browser team at "
    "genome-www@soe.ucsc.edu. As this is a mirror website not managed by UCSC, please "
    "specify the address of the mirror in your email.</p>");

hPrintf("<p><a href=\"%s?hgLogin.do.displayLoginPage=1\">Return to Login</a></p>\n",
        hgLoginUrl);
cartRemove(cart, "hgLogin_helpWith");
cartRemove(cart, "hgLogin_email");
cartRemove(cart, "hgLogin_userName");
cartRemove(cart, "hgLogin_sendMailTo");
cartRemove(cart, "hgLogin_sendMailContain");
}

void sendMailOut(char *email, char *subject, char *msg)
/* send username reminder email to email address */
{
char *obj = cartUsualString(cart, "hgLogin_helpWith", "");
int result;
result = mailViaPipeBounce(email, subject, msg, returnAddr);
if (result == -1)
    {
    hPrintf( 
        "<h2>%s</h2>", brwName);
    hPrintf(
        "<p align=\"left\">"
        "</p>"
        "<h3>Error emailing %s to: %s</h3>"
        "Click <a href=%s?hgLogin.do.displayAccHelpPage=1>here</a> to return.<br>", 
        hgLoginUrl, obj, email );
    }
else
    {
    jsInlineF(
        "window.location = '%s?hgLogin.do.displayMailSuccess=1';\n"
        , hgLoginUrl);
    }
}

void mailUsername(char *email, char *users)
/* send user name list to the email address */
{
char subject[256];
char msg[4096];
char *remoteAddr=getenv("REMOTE_ADDR");

safef(subject, sizeof(subject),"Your username at the %s", brwName);
safef(msg, sizeof(msg), 
    "  Someone (probably you, from IP address %s) has requested username(s) associated with this email address at the %s: \n\n  %s\n\n%s\n%s", 
   remoteAddr, brwName, users, signature, returnAddr);
sendMailOut(email, subject, msg);
}

void sendUsername(struct sqlConnection *conn, char *email)
/* email user username(s)  */
{
struct sqlResult *sr;
char **row;
char query[256];

/* find all the user names associated with this email address */
char userList[512]="";
sqlSafef(query,sizeof(query),"SELECT * FROM gbMembers WHERE email='%s' or recovEmail='%s'", email, email);
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

void sendPwdMailOut(char *email, char *recovEmail, char *subject, char *msg, char *username)
/* send password reset mail to user at registered email address */
{
char *obj = cartUsualString(cart, "hgLogin_helpWith", "");
int result;

result = mailViaPipeBounce(email, subject, msg, returnAddr);
if ((result != -1) && !isEmpty(recovEmail))
    result = mailViaPipeBounce(recovEmail, subject, msg, returnAddr);

if (result == -1)
    {
    hPrintf(
        "<h2>%s</h2>", brwName);
    hPrintf(
        "<p align=\"left\">"
        "</p>"
        "<h3>Error emailing %s to: %s</h3>"
        "Click <a href=%s?hgLogin.do.displayAccHelpPage=1>here</a> to return.<br>",
        hgLoginUrl, obj, email );
    }
else
    {
    jsInlineF(
        "window.location = '%s?hgLogin.do.displayMailSuccessPwd=1&user=%s';\n"
        , hgLoginUrl, username);
    }
}

void sendNewPwdMail(char *username, char *email, char *recovEmail, char *password)
/* send user new password */
{
char subject[256];
char msg[4096];
char *remoteAddr=getenv("REMOTE_ADDR");

safef(subject, sizeof(subject),"New temporary password for your account at the %s", brwName);
safef(msg, sizeof(msg),
    "  Someone (probably you, from IP address %s) requested a new password for the %s (%s). A temporary password for user \"%s\" has been created and was set to \"%s\". If this was your intent, you will need to log in and choose a new password now. Your temporary password will expire in 7 days.\n\n  If someone else made this request, or if you have remembered your password, and you no longer wish to change it, you may ignore this message and continue using your old password.\n\n%s\n%s",
    remoteAddr, brwName, brwAddr, username, password, signature, returnAddr);
sendPwdMailOut(email, recovEmail, subject, msg, username);
}

void displayAccHelpPage(struct sqlConnection *conn)
/* draw the account help page */
{
char *email = cartUsualString(cart, "hgLogin_email", "");
char *username = cartUsualString(cart, "hgLogin_userName", "");

jsInline(
    "function toggle(value){\n"
    "if(value=='showE'){\n"
    " document.getElementById('usernameBox').style.display='none';\n"
    " document.getElementById('emailAddrBox').style.display='inline';\n"
    " } else {\n"
    " document.getElementById('usernameBox').style.display='inline';\n"
    " document.getElementById('emailAddrBox').style.display='none';\n"
    " }\n"
    "}\n"
    );
hPrintf("<div id=\"accountHelpBox\" class=\"centeredContainer formBox\">"
    "\n"
    "<h2>%s</h2>"
    "\n", brwName);
hPrintf("<h3>Having trouble signing in?</h3>"
    "\n"
    "<form method=post action=\"%s\" name=\"accountLoginForm\" id=\"acctHelpForm\">"
    "\n"
    "<p><span style='color:red;'>%s</span><p>"
    "\n", hgLoginUrl, errMsg ? errMsg : "");
// A "Forgot username/password" link may preselect a radio via hgLogin_helpWith in the URL.
char *pre = cartUsualString(cart, "hgLogin_helpWith", "");
hPrintf("<div class=\"inputGroup\">");
hPrintf("<div class=\"acctHelpSection\"><input name=\"hgLogin_helpWith\" type=\"radio\" value=\"password\" id=\"password\"%s>"
    "<label for=\"password\" class=\"radioLabel\">I forgot my <b>password</b>. Send me a new one.</label></div>",
    sameString(pre, "password") ? " checked" : "");
hPrintf("<div class=\"acctHelpSection\"><input name=\"hgLogin_helpWith\" type=\"radio\" value=\"username\" id=\"username\"%s>"
    "<label for=\"username\" class=\"radioLabel\">I forgot my <b>username</b>. Please email it to me.</label></div>",
    sameString(pre, "username") ? " checked" : "");
if (emailLinkEnabled())
    hPrintf("<div class=\"acctHelpSection\"><input name=\"hgLogin_helpWith\" type=\"radio\" value=\"loginLink\" id=\"loginLink\">"
        "<label for=\"loginLink\" class=\"radioLabel\">Email me a <b>login link</b> so I can sign in without a password.</label></div>");
hPrintf("</div>\n");
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
    "     &nbsp;<a href=\"%s\" class=\"cancelButton\">Cancel</a>"
    "</div>"
    "</form>"
    "</div><!-- END - accountHelpBox -->", username, email, getReturnToURL());
jsOnEventById("click", "password", "toggle('showU');");
jsOnEventById("click", "username", "toggle('showE');");
if (emailLinkEnabled())
    jsOnEventById("click", "loginLink", "toggle('showE');");
// If a radio was preselected via the URL, open its matching input box on load.
if (sameString(pre, "password"))
    jsInline("toggle('showU');");
else if (sameString(pre, "username") || sameString(pre, "loginLink"))
    jsInline("toggle('showE');");
cartSaveSession(cart);
}

void sendNewPassword(struct sqlConnection *conn, char *username, char *password)
/* email user new password  */
{
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

sqlSafef(query,sizeof(query),"SELECT recovEmail FROM gbMembers WHERE userName='%s'", username);
char *recovEmail = sqlQuickString(conn, query);

sendNewPwdMail(username, email, recovEmail, password);
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
char *remoteAddr=getenv("REMOTE_ADDR");

safef(activateURL, sizeof(activateURL),
    "%s?hgLogin.do.activateAccount=1&user=%s&token=%s\n",
    hgLoginUrl,
    cgiEncode(username),
    cgiEncode(encToken));
safef(subject, sizeof(subject),"%s account e-mail address confirmation", brwName);
safef(msg, sizeof(msg),
    "Someone (probably you, from IP address %s) has requested an account %s with this e-mail address on the %s.\nTo confirm that this account really does belong to you on the %s, open this link in your browser:\n\n%s\n\nIf this is *not* you, do not follow the link. This confirmation code will expire in 7 days.\n\nIf this *is* you, after clicking the activation link, your new account gives you access to sessions you can create and name. Sessions allow you to save your Genome Browser screen configuration and share it with others with a link like https://genome.ucsc.edu/s/%s/YourSessionName\n\nFor more information on sessions, see our help page on the topic: https://genome.ucsc.edu/goldenPath/help/hgSessionHelp.html#Introduction\n\nAdditional resources:\nSubscribe to the Genome Browser Mailing List: https://groups.google.com/a/soe.ucsc.edu/group/genome-announce?hl=en\nGenome Browser User Guide: https://genome.ucsc.edu/goldenPath/help/hgTracksHelp.html\nTraining and Tutorials: https://genome.ucsc.edu/training/index.html\n\n%s\n%s",
     remoteAddr, username, brwName, brwName, activateURL, username, signature, returnAddr);
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

void printPwdEyeIcon(char *iconId, char *slashId)
/* print a clickable eye icon, absolutely positioned inside a password
 * input's wrapper span; slashId is the <line> toggled to show "hidden".
 * No-op if disabled via hg.conf login.pwdEyeIcon. */
{
if (!pwdEyeIconEnabled)
    return;
hPrintf(
    "<span id=\"%s\" title=\"Show/hide password\" "
    "style=\"position:absolute; right:8px; top:50%%; transform:translateY(-50%%); "
    "cursor:pointer; user-select:none;\">"
    "<svg width=\"18\" height=\"18\" viewBox=\"0 0 24 24\" fill=\"none\" "
    "stroke=\"#666\" stroke-width=\"2\">"
    "<path d=\"M1 12s4-7 11-7 11 7 11 7-4 7-11 7-11-7-11-7z\"/>"
    "<circle cx=\"12\" cy=\"12\" r=\"3\"/>"
    "<line id=\"%s\" x1=\"2\" y1=\"2\" x2=\"22\" y2=\"22\" style=\"display:none;\"/>"
    "</svg>"
    "</span>", iconId, slashId);
}

void printPwdToggleJS()
/* define the password show/hide toggle function used by all eye icons */
{
jsInline(
    "function togglePwdVisibility(inputId, slashId) {\n"
    "  var inp = document.getElementById(inputId);\n"
    "  var slash = document.getElementById(slashId);\n"
    "  if (inp.type === 'password') {\n"
    "    inp.type = 'text';\n"
    "    slash.style.display = 'inline';\n"
    "  } else {\n"
    "    inp.type = 'password';\n"
    "    slash.style.display = 'none';\n"
    "  }\n"
    "}\n"
    );
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
    "<p>Do not have an account? <a href=\"%s?hgLogin.do.signupPage=1\">Go to the sign up page</a>.</p>"
    "\n", hgLoginUrl);
if (errMsg && sameString(errMsg, "Your account has been activated."))
    hPrintf("<span style='color:green;'>%s</span>\n", errMsg ? errMsg : "");
else
    hPrintf("<span style='color:red;'>%s</span>\n", errMsg ? errMsg : "");
hPrintf("<form method=post action=\"%s\" name=\"accountLoginForm\" id=\"accountLoginForm\">"
    "\n"
    "<div class=\"inputGroup\">"
    "<label for=\"userName\">Username</label>"
    "<input type=text name=\"hgLogin_userName\" value=\"%s\" size=\"30\" id=\"userName\">"
    "<a class=\"forgotLink\" href=\"%s?hgLogin.do.displayAccHelpPage=1&hgLogin_helpWith=username\">Forgot username</a>"
    "</div>"
    "\n"
    "<div class=\"inputGroup\">"
    "<label for=\"password\">Password</label>"
    "<span style=\"position:relative; display:inline-block;\">"
    "<input type=password name=\"hgLogin_password\" value=\"\" size=\"30\" id=\"password\">"
    , hgLoginUrl, username, hgLoginUrl);
printPwdEyeIcon("pwdEyeIcon", "pwdEyeSlash");
hPrintf(
    "</span>"
    "<a class=\"forgotLink\" href=\"%s?hgLogin.do.displayAccHelpPage=1&hgLogin_helpWith=password\">Forgot password</a>"
    "</div>"
    "\n"
    "<div class=\"formControls\">"
    "   <input type=\"submit\" name=\"hgLogin.do.displayLogin\" value=\"Login\" class=\"largeButton\">"
    "    &nbsp;<a href=\"%s\" class=\"cancelButton\">Cancel</a>"
    "</div>"
    , hgLoginUrl, getReturnToURL());
if (pwdEyeIconEnabled)
    {
    printPwdToggleJS();
    jsOnEventById("click", "pwdEyeIcon", "togglePwdVisibility('password','pwdEyeSlash');");
    }
cartSaveSession(cart);
hPrintf("</form>\n");
printEmailLinkButton();
printSocialButtons(TRUE, FALSE, "Sign in");
hPrintf(
    "</div><!-- END - loginBox -->"
    "\n"
    "\n"
    "</body>"
    "</html>");
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
    "<form method=\"post\" action=\"%s\" name=\"changePasswordForm\" id=\"changePasswordForm\">"
    "\n"
    "<div class=\"inputGroup\">"
    "<label for=\"userName\">Username</label>"
    "<input type=\"text\" name=\"hgLogin_userName\" size=\"30\" value=\"%s\" id=\"email\">"
    "</div>"
    "\n", errMsg ? errMsg : "", hgLoginUrl,
    cartUsualString(cart, "hgLogin_userName", ""));
hPrintf("<div class=\"inputGroup\">"
    "\n"
    "<label for=\"currentPw\">Current or Emailed Password</label>"
    "<span style=\"position:relative; display:inline-block;\">"
    "<input type=\"password\" name=\"hgLogin_password\" value=\"\" size=\"30\" id=\"currentPw\">");
printPwdEyeIcon("curPwEyeIcon", "curPwEyeSlash");
hPrintf(
    "</span>"
    "</div>"
    "\n"
    "<div class=\"inputGroup\">"
    "<label for=\"newPw1\">New Password</label>"
    "<span style=\"position:relative; display:inline-block;\">"
    "<input type=\"password\" name=\"hgLogin_newPassword1\" value=\"\" size=\"30\" id=\"newPw1\">");
printPwdEyeIcon("newPw1EyeIcon", "newPw1EyeSlash");
hPrintf(
    "</span>"
    "</div>"
    "\n"
    "<div class=\"inputGroup\">"
    "<label for=\"newPw2\">Re-enter New Password</label>"
    "<span style=\"position:relative; display:inline-block;\">"
    "<input type=\"password\" name=\"hgLogin_newPassword2\" value=\"\" size=\"30\" id=\"newPw2\">");
printPwdEyeIcon("newPw2EyeIcon", "newPw2EyeSlash");
hPrintf(
    "</span>"
    "</div>"
    "\n"
    "<div class=\"formControls\">"
    "    <input type=\"submit\" name=\"hgLogin.do.changePassword\" value=\"Change Password\" class=\"largeButton\"> &nbsp; "
    "    <a href=\"%s\" class=\"cancelButton\">Cancel</a>"
    "\n"
    "</div>"
    "</form>"
    "\n"
    "</div><!-- END - changePwBox -->"
    "\n", getReturnToURL());
if (pwdEyeIconEnabled)
    {
    printPwdToggleJS();
    jsOnEventById("click", "curPwEyeIcon", "togglePwdVisibility('currentPw','curPwEyeSlash');");
    jsOnEventById("click", "newPw1EyeIcon", "togglePwdVisibility('newPw1','newPw1EyeSlash');");
    jsOnEventById("click", "newPw2EyeIcon", "togglePwdVisibility('newPw2','newPw2EyeSlash');");
    }
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
sqlSafef(query,sizeof(query),"SELECT * FROM gbMembers WHERE userName='%s'", user);
struct gbMembers *m = gbMembersLoadByQuery(conn, query);
struct dyString *cookieJS = getLoginCookieJS(user, m->idx);
jsInline(cookieJS->string);
returnToURL(150);
}

void changeEmailPage(struct sqlConnection *conn)
/* Draw the change-email page for the currently logged-in user.  The account is taken from
 * the validated login cookie (wikiLinkUserName), never from a form field, so a user can only
 * change their own email.  Being logged in is the authorization; no password is required,
 * which also lets social-login accounts (which have no password) change their email. */
{
if (!emailLinkEnabled())
    {
    displayLoginPage(conn);
    return;
    }
char *user = wikiLinkUserName();
if (isEmpty(user))
    {
    freez(&errMsg);
    errMsg = cloneString("Please log in first to change your email address.");
    displayLoginPage(conn);
    return;
    }
char query[256];
sqlSafef(query, sizeof(query), "SELECT email FROM gbMembers WHERE userName='%s'", user);
char *curEmail = sqlQuickString(conn, query);

hPrintf("<div id=\"changeEmailBox\" class=\"centeredContainer formBox\">"
    "<h2>%s</h2>", brwName);
hPrintf("<h3>Change Email</h3>");
hPrintf("<p><span style='color:red;'>%s</span></p>", errMsg ? errMsg : "");
hPrintf("<form method=\"post\" action=\"%s\" name=\"changeEmailForm\">", hgLoginUrl);
hPrintf("<p>Signed in as <b>%s</b>.<br>Current email address: <b>%s</b></p>",
    user, isNotEmpty(curEmail) ? curEmail : "(none)");
hPrintf("<div class=\"inputGroup\">"
    "<label for=\"newEmail1\">New email address</label>"
    "<input type=\"text\" name=\"hgLogin_newEmail1\" value=\"\" size=\"30\" id=\"newEmail1\">"
    "</div>");
hPrintf("<div class=\"inputGroup\">"
    "<label for=\"newEmail2\">Re-enter new email address</label>"
    "<input type=\"text\" name=\"hgLogin_newEmail2\" value=\"\" size=\"30\" id=\"newEmail2\">"
    "</div>");
hPrintf("<div class=\"formControls\">"
    "<input type=\"submit\" name=\"hgLogin.do.changeEmail\" value=\"Change Email\" class=\"largeButton\">"
    " &nbsp;<a href=\"%s\" class=\"cancelButton\">Cancel</a>"
    "</div></form></div><!-- END - changeEmailBox -->", getReturnToURL());
cartSaveSession(cart);
}

void changeEmail(struct sqlConnection *conn)
/* Process the change-email form for the currently logged-in user. */
{
if (!emailLinkEnabled())
    {
    displayLoginPage(conn);
    return;
    }
char *user = wikiLinkUserName();
if (isEmpty(user))
    {
    freez(&errMsg);
    errMsg = cloneString("Please log in first to change your email address.");
    displayLoginPage(conn);
    return;
    }
char *email1 = cartUsualString(cart, "hgLogin_newEmail1", "");
char *email2 = cartUsualString(cart, "hgLogin_newEmail2", "");
if (isEmpty(email1) || spc_email_isvalid(email1) == 0)
    {
    freez(&errMsg);
    errMsg = cloneString("Please enter a valid email address.");
    changeEmailPage(conn);
    return;
    }
if (differentString(email1, email2))
    {
    freez(&errMsg);
    errMsg = cloneString("Email addresses do not match.");
    changeEmailPage(conn);
    return;
    }
char query[512];
sqlSafef(query, sizeof(query),
    "UPDATE gbMembers SET email='%s', lastUse=NOW() WHERE userName='%s'", email1, user);
sqlUpdate(conn, query);
cartRemove(cart, "hgLogin_newEmail1");
cartRemove(cart, "hgLogin_newEmail2");
hPrintf("<div class=\"centeredContainer formBox\"><h2>%s</h2>", brwName);
hPrintf("<h3>Your email address has been changed.</h3>");
hPrintf("<p>Your email address is now <b>%s</b>.</p></div>", email1);
returnToURL(1500);
}

void signupPage(struct sqlConnection *conn)
/* draw the signup page */
{
hPrintf("<div id=\"signUpBox\" class=\"centeredContainer formBox\">"
    "<h2>%s</h2>", brwName);
hPrintf(
    "<p>Signing up enables you to save multiple sessions, share your sessions with others via short and stable session links and manage previously uploaded custom tracks and track hubs.</p>"
    "\n");
hPrintf("<p>Already have an account? "
    "<a href=\"%s?hgLogin.do.displayLoginPage=1\">Go to the login page</a>.</p>", hgLoginUrl);
printSocialButtons(FALSE, TRUE, "Sign up");
hPrintf("<h3>Sign Up Using Email</h3>"
    "<form method=\"post\" action=\"%s\" name=\"mainForm\">"
    "<span style='color:red;'>%s</span>"
    "\n", hgLoginUrl, errMsg ? errMsg : "");
printUsernameNote();
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
    "</div>\n",
    cartUsualString(cart, "hgLogin_userName", ""), cartUsualString(cart, "hgLogin_email", ""),
    cartUsualString(cart, "hgLogin_email2", ""));

if (sqlFieldIndex(conn, "gbMembers", "recovEmail") != -1)
    hPrintf("<div class=\"inputGroup\">"
        "<label for=\"recovEmail\">Optional Secondary Recovery Email</label>"
        "<input type=text name=\"hgLogin_recovEmail\" size=\"30\" id=\"recovEmail\">"
        "</div>"
        "\n");

hPrintf("<div class=\"inputGroup\">"
    "<label for=\"password\">Password <small>(must be at least 5 characters)</small></label>"
    "<span style=\"position:relative; display:inline-block;\">"
    "<input type=password name=\"hgLogin_password\" value=\"%s\" size=\"30\" id=\"password\">",
    cartUsualString(cart, "hgLogin_password", ""));
printPwdEyeIcon("signupPwEyeIcon", "signupPwEyeSlash");
hPrintf(
    "</span>"
    "</div>"
    "\n"
    "<div class=\"inputGroup\">"
    "<label for=\"passwordCheck\">Re-enter Password</label>"
    "<span style=\"position:relative; display:inline-block;\">"
    "<input type=password name=\"hgLogin_password2\" value=\"%s\" size=\"30\" id=\"passwordCheck\">",
    cartUsualString(cart, "hgLogin_password2", ""));
printPwdEyeIcon("signupPwCheckEyeIcon", "signupPwCheckEyeSlash");
hPrintf(
    "</span>"
    "\n"
    "</div>"
    "\n"
    "<div class=\"formControls\">"
    "    <input type=\"submit\" name=\"hgLogin.do.signup\" value=\"Sign Up using Email\" class=\"largeButton\"> &nbsp; "
    "    <a href=\"%s\" class=\"cancelButton\">Cancel</a>"
    "</div>"
    "</form>"
    "</div><!-- END - signUpBox -->",
    getReturnToURL());
if (pwdEyeIconEnabled)
    {
    printPwdToggleJS();
    jsOnEventById("click", "signupPwEyeIcon", "togglePwdVisibility('password','signupPwEyeSlash');");
    jsOnEventById("click", "signupPwCheckEyeIcon", "togglePwdVisibility('passwordCheck','signupPwCheckEyeSlash');");
    }
cartSaveSession(cart);
}

void signup(struct sqlConnection *conn)
/* process the signup form */
{
char query[1024];
char *user = cartUsualString(cart, "hgLogin_userName", "");
char *encUserName = cgiEncodeFull(user);
if (!user || sameString(user,""))
    {
    freez(&errMsg);
    errMsg = cloneString("User name cannot be blank.");
    signupPage(conn);
    return;
    }
/* Require at least two characters.  Single-character user names are reserved (e.g. "l" is used
 * internally for anonymous shared-session links). */
if (strlen(user) < 2)
    {
    freez(&errMsg);
    errMsg = cloneString("User name must be at least two characters long.");
    signupPage(conn);
    return;
    }
/* Make sure the escaped usrename is less than 32 characters */
if (strlen(encUserName) > 32)
    {
    char buf[1024];
    safef(buf,sizeof(buf), "Encoded user name: '%s' is %d characters.  Please use a shorter name: less than 32 characters after URL encoding.", encUserName, (int)strlen(encUserName));
    freez(&errMsg);
    errMsg = cloneString(buf);
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

char *recovEmail = cartUsualString(cart, "hgLogin_recovEmail", "");
if (!isEmpty(recovEmail) && spc_email_isvalid(recovEmail) == 0)
    {
    freez(&errMsg);
    errMsg = cloneString("Invalid format of the recovery email address.");
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
char *accActStatus = "N";

if (sameWord(returnAddr, "NOEMAIL"))
    accActStatus = "Y";

struct dyString *query2 = sqlDyStringCreate(
    "INSERT INTO gbMembers SET "
    "userName='%s',realName='%s',password='%s',email='%s',"
    "lastUse=NOW(),accountActivated='%s'",
    user,user,encPwd,email,accActStatus);
// set the recov email only if we got one (and we only got one if the table has this field)
if (!isEmpty(recovEmail))
    sqlDyStringPrintf(query2, ",recovEmail='%s'", recovEmail);

sqlUpdate(conn, dyStringContents(query2));
dyStringFree(&query2);

if (sameWord(returnAddr, "NOEMAIL"))
    {
    redirectToLoginPage("hgLogin.do.displayLoginPage=1");
    return;
    }

setupNewAccount(conn, email, user);
/* send out activate code mail, and display the mail confirmation box */
cartRemove(cart, "hgLogin_email");
cartRemove(cart, "hgLogin_email2");
cartRemove(cart, "hgLogin_userName");
cartRemove(cart, "user");
cartRemove(cart, "token");
redirectToLoginPage("hgLogin.do.displayActMailSuccess=1");
}

void accountHelp(struct sqlConnection *conn)
/* email user username(s) or new password */
{
char query[256];
char *email = cartUsualString(cart, "hgLogin_email", "");
char *username = cartUsualString(cart, "hgLogin_userName", "");
char *helpWith = cartUsualString(cart, "hgLogin_helpWith", "");

/* Passwordless email login link */
if (sameString(helpWith,"loginLink"))
    {
    sendEmailLink(conn);
    return;
    }

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
            "SELECT password FROM gbMembers WHERE email='%s' or recovEmail='%s'", email, email);
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

void displayLoginSuccess(char *userName, uint idx)
/* display login success msg, and set cookie */
{
hPrintf("<h2>%s</h2>", brwName);
hPrintf(
    "<p align=\"left\">"
    "</p>"
    "<span style='color:red;'></span>"
    "\n");
/* Set cookies */
struct dyString *javascript = dyStringNew(1024);
dyStringPrintf(javascript,
        " document.write(\"Login successful, setting cookies now...\");");
jsInline(javascript->string);
struct dyString *cookieJS = getLoginCookieJS(userName, idx);
jsInline(cookieJS->string);
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
    hPrintf("<h2>Login successful for user %s.\n</h2>\n", userName);
    clearNewPasswordFields(conn, userName);
    displayLoginSuccess(m->userName, m->idx);
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
struct dyString *javascript = dyStringNew(1024);
struct slName *newCookies = loginLogoutUser(), *sl;
for (sl = newCookies;  sl != NULL;  sl = sl->next)
    dyStringPrintf(javascript, " document.cookie = '%s';", sl->name);
jsInline(javascript->string);
/* return to "returnto" URL */
returnToURL(150);
}

/* ---- Social login (OAuth) and passwordless email-link login ---- */

static void printSocialButtons(boolean dividerAbove, boolean dividerBelow, char *action)
/* Print social login buttons for any enabled providers, optionally bracketed by "or"
 * dividers.  action is the button verb ("Sign in" on the login page, "Sign up" on the signup
 * page).  Prints nothing if no provider is configured, so mirrors without OAuth credentials
 * are unaffected. */
{
if (!oauthAnyProviderEnabled())
    return;
hPrintf("<div class=\"socialLogin\">");
if (dividerAbove)
    hPrintf("<div class=\"orDivider\"><span>or</span></div>");
struct slName *prov, *providers = oauthProviderNames();
for (prov = providers;  prov != NULL;  prov = prov->next)
    hPrintf("<a class=\"socialButton\" href=\"%s?hgLogin.do.oauthStart=1&provider=%s\">"
            "%s with %s</a>",
            hgLoginUrl, cgiEncode(prov->name), action, oauthProviderLabel(prov->name));
if (dividerBelow)
    hPrintf("<div class=\"orDivider\"><span>or</span></div>");
hPrintf("</div>");
}

static boolean emailLinkEnabled()
/* Return TRUE if passwordless email-link login is turned on in hg.conf.  It needs working
 * outbound email, so it is off unless the admin explicitly enables it with login.emailLink=on. */
{
return cfgOptionBooleanDefault(CFG_LOGIN_EMAIL_LINK, FALSE);
}

static void printEmailLinkButton()
/* Print a grey button that opens the passwordless email-link login page, if enabled. */
{
if (!emailLinkEnabled())
    return;
hPrintf("<a class=\"socialButton\" href=\"%s?hgLogin.do.emailLinkPage=1\">"
    "Email me a sign-in link</a>", hgLoginUrl);
}

static void printUsernameNote()
/* Print a short hint, shown wherever a new username is chosen, explaining that the username
 * shows up in every short link the user later creates, so it should be short and easy to type. */
{
hPrintf("<p style=\"font-size:0.9em\">Note: your username becomes part of every short link "
    "you create later (for example <code>%s/s/<b>username</b>/MySession</code>), so choose "
    "something short and easy to type.</p>", brwAddr);
}

static void loginAndReturn(char *userName, uint idx)
/* Set the permanent login cookies for userName and bounce back to the returnto URL.
 * Every login method (password, social, email link) funnels through here, so they all
 * produce the same long-lived login cookies. */
{
hPrintf("<h2>%s</h2>", brwName);
hPrintf("<p>Login successful, setting cookies now&hellip;</p>");
struct dyString *cookieJS = getLoginCookieJS(userName, idx);
jsInline(cookieJS->string);
cartRemove(cart, "hgLogin_userName");
returnToURL(150);
}

static void createIdentityTable(struct sqlConnection *conn)
/* Create the gbMemberIdentity table if it does not exist.  On a mirror whose central db
 * is read-only this may fail; social login simply won't work there (and won't be enabled
 * without client secrets anyway), so ignore any error. */
{
if (sqlTableExists(conn, "gbMemberIdentity"))
    return;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    struct dyString *dy = sqlDyStringCreate(
        "CREATE TABLE IF NOT EXISTS gbMemberIdentity ("
        "idx int unsigned NOT NULL,"
        "provider varchar(64) NOT NULL,"
        "subject varchar(255) NOT NULL,"
        "email varchar(255) NOT NULL default '',"
        "created DATETIME NOT NULL,"
        "lastUse DATETIME NOT NULL,"
        "UNIQUE KEY provSub (provider, subject),"
        "INDEX(idx))");
    sqlUpdate(conn, dyStringContents(dy));
    dyStringFree(&dy);
    }
errCatchEnd(errCatch);
errCatchFree(&errCatch);
}

static boolean userNameTaken(struct sqlConnection *conn, char *userName)
/* Return TRUE if userName already exists in gbMembers. */
{
char query[512];
sqlSafef(query, sizeof(query), "SELECT count(*) FROM gbMembers WHERE userName='%s'", userName);
return sqlQuickNum(conn, query) > 0;
}

static char *suggestUsername(struct sqlConnection *conn, char *email, char *displayName)
/* Suggest an available username from the email local-part (falling back to the display
 * name), keeping only valid username characters and appending a number to avoid clashes. */
{
char raw[256];
raw[0] = 0;
if (isNotEmpty(email) && strchr(email, '@'))
    {
    safecpy(raw, sizeof(raw), email);
    char *at = strchr(raw, '@');
    *at = 0;
    }
else if (isNotEmpty(displayName))
    safecpy(raw, sizeof(raw), displayName);
char clean[256];
int j = 0;
char *s;
for (s = raw; *s != 0 && j < (int)sizeof(clean)-1; s++)
    if (isalnum(*s) || *s == '_' || *s == '-')
        clean[j++] = tolower((unsigned char)*s);
clean[j] = 0;
if (strlen(clean) < 2)
    safecpy(clean, sizeof(clean), "user");
char candidate[288];
safecpy(candidate, sizeof(candidate), clean);
int n = 1;
while (userNameTaken(conn, candidate))
    safef(candidate, sizeof(candidate), "%s%d", clean, ++n);
return cloneString(candidate);
}

static struct gbMembers *memberForIdentity(struct sqlConnection *conn, struct oauthIdentity *id)
/* Return the gbMembers account already linked to this provider identity, or NULL. */
{
char query[512];
sqlSafef(query, sizeof(query),
    "SELECT idx FROM gbMemberIdentity WHERE provider='%s' AND subject='%s'",
    id->provider, id->subject);
uint idx = (uint)sqlQuickLongLong(conn, query);
if (idx == 0)
    return NULL;
sqlSafef(query, sizeof(query), "SELECT * FROM gbMembers WHERE idx=%u", idx);
return gbMembersLoadByQuery(conn, query);
}

static char *oauthPendingSig(char *provider, char *subject, char *email)
/* Signature over a pending social identity, keyed by the secret login.cookieSalt.  Only
 * resolveIdentity (which runs after a genuine provider verification) can produce a valid one,
 * so a pending identity injected through cart/CGI variables will not validate.  (Not bound to
 * the session id: the hgsid is regenerated across the provider redirect, so a session-bound
 * signature would never match on the way back.)  Result is allocd. */
{
char buf[1024];
safef(buf, sizeof(buf), "%s|%s|%s|%s",
    emptyForNull(cfgOption(CFG_LOGIN_COOKIE_SALT)),
    emptyForNull(provider), emptyForNull(subject), emptyForNull(email));
return generateTokenMD5(buf);
}

static boolean pendingIdentityValid()
/* TRUE only if the pending-identity cart variables carry a signature we minted this session.
 * Guards the OAuth chooser and completeAccount against forged/injected pending identities. */
{
char *sig = cartUsualString(cart, "oauth_pending_sig", "");
if (isEmpty(sig))
    return FALSE;
char *expected = oauthPendingSig(cartUsualString(cart, "oauth_pending_provider", ""),
                                 cartUsualString(cart, "oauth_pending_subject", ""),
                                 cartUsualString(cart, "oauth_pending_email", ""));
boolean ok = sameString(sig, expected);
freeMem(expected);
return ok;
}

static void setPendingIdentity(struct oauthIdentity *id)
/* Stash an authenticated-but-not-yet-linked identity in the cart so it survives a form
 * round-trip (the "choose a username" or "choose an account" page).  The signature is what
 * proves, on the way back, that we really verified this identity. */
{
cartSetString(cart, "oauth_pending_provider", id->provider);
cartSetString(cart, "oauth_pending_subject", id->subject);
cartSetString(cart, "oauth_pending_email", emptyForNull(id->email));
cartSetString(cart, "oauth_pending_name", emptyForNull(id->displayName));
cartSetString(cart, "oauth_pending_sig",
    oauthPendingSig(id->provider, id->subject, emptyForNull(id->email)));
}

static void clearPendingIdentity()
/* Remove the pending-identity cart variables once the account is linked. */
{
cartRemove(cart, "oauth_pending_provider");
cartRemove(cart, "oauth_pending_subject");
cartRemove(cart, "oauth_pending_email");
cartRemove(cart, "oauth_pending_name");
cartRemove(cart, "oauth_pending_sig");
}

static void linkIdentity(struct sqlConnection *conn, uint idx, struct oauthIdentity *id)
/* Insert or refresh the gbMemberIdentity row linking idx to this provider identity. */
{
char query[1024];
char *email = emptyForNull(id->email);
sqlSafef(query, sizeof(query),
    "INSERT INTO gbMemberIdentity SET idx=%u, provider='%s', subject='%s', email='%s', "
    "created=NOW(), lastUse=NOW() "
    "ON DUPLICATE KEY UPDATE idx=%u, email='%s', lastUse=NOW()",
    idx, id->provider, id->subject, email, idx, email);
sqlUpdate(conn, query);
}

void completeAccountPage(struct sqlConnection *conn)
/* Ask a first-time social-login user to confirm a username (and email) for a new account. */
{
char *provider = cartUsualString(cart, "oauth_pending_provider", "");
char *email = cartUsualString(cart, "oauth_pending_email", "");
char *name = cartUsualString(cart, "oauth_pending_name", "");
if (isEmpty(provider) || !pendingIdentityValid())
    {
    displayLoginPage(conn);
    return;
    }
char *suggested = cartUsualString(cart, "hgLogin_userName", "");
if (isEmpty(suggested))
    suggested = suggestUsername(conn, email, name);

hPrintf("<div id=\"completeAccountBox\" class=\"centeredContainer formBox\">"
    "<h2>%s</h2>", brwName);
hPrintf("<h3>Choose a username</h3>");
hPrintf("<p>You signed in with %s. Pick a username for your new %s account. "
    "You can change the suggested name below.</p>",
    oauthProviderLabel(provider), brwName);
printUsernameNote();
hPrintf("<span style='color:red;'>%s</span>", errMsg ? errMsg : "");
hPrintf("<form method=\"post\" action=\"%s\" name=\"completeAccountForm\">", hgLoginUrl);
hPrintf("<div class=\"inputGroup\">"
    "<label for=\"userName\">Username</label>"
    "<input type=\"text\" name=\"hgLogin_userName\" value=\"%s\" size=\"30\" id=\"userName\">"
    "</div>", suggested);
hPrintf("<div class=\"inputGroup\">"
    "<label for=\"emailAddr\">Email address</label>"
    "<input type=\"text\" name=\"hgLogin_email\" value=\"%s\" size=\"30\" id=\"emailAddr\">"
    "</div>", email);
hPrintf("<div class=\"formControls\">"
    "<input type=\"submit\" name=\"hgLogin.do.completeAccount\" value=\"Create Account\" class=\"largeButton\">"
    " &nbsp;<a href=\"%s\" class=\"cancelButton\">Cancel</a>"
    "</div></form></div><!-- END - completeAccountBox -->", getReturnToURL());
cartSaveSession(cart);
}

void completeAccount(struct sqlConnection *conn)
/* Create the account for a first-time social-login user, link the identity, and log in. */
{
char *provider = cartUsualString(cart, "oauth_pending_provider", "");
char *subject = cartUsualString(cart, "oauth_pending_subject", "");
if (isEmpty(provider) || isEmpty(subject) || !pendingIdentityValid())
    {
    freez(&errMsg);
    errMsg = cloneString("Your login session expired. Please sign in again.");
    displayLoginPage(conn);
    return;
    }
char *user = cartUsualString(cart, "hgLogin_userName", "");
char *encUserName = cgiEncodeFull(user);
if (isEmpty(user))
    {
    freez(&errMsg);
    errMsg = cloneString("Please enter a username.");
    completeAccountPage(conn);
    return;
    }
if (strlen(user) < 2)
    {
    freez(&errMsg);
    errMsg = cloneString("User name must be at least two characters long.");
    completeAccountPage(conn);
    return;
    }
if (strlen(encUserName) > 32)
    {
    freez(&errMsg);
    errMsg = cloneString("Please use a shorter user name: less than 32 characters after URL encoding.");
    completeAccountPage(conn);
    return;
    }
if (userNameTaken(conn, user))
    {
    freez(&errMsg);
    errMsg = cloneString("A user with this name already exists. Please choose another.");
    completeAccountPage(conn);
    return;
    }
char *email = cartUsualString(cart, "hgLogin_email", "");
if (isEmpty(email))
    {
    freez(&errMsg);
    errMsg = cloneString("Please enter an email address.");
    completeAccountPage(conn);
    return;
    }
if (spc_email_isvalid(email) == 0)
    {
    freez(&errMsg);
    errMsg = cloneString("Invalid email address format.");
    completeAccountPage(conn);
    return;
    }
char *name = cartUsualString(cart, "oauth_pending_name", "");
char *realName = isNotEmpty(name) ? name : user;

struct dyString *q = sqlDyStringCreate(
    "INSERT INTO gbMembers SET userName='%s', realName='%s', password='', email='%s', "
    "lastUse=NOW(), dateActivated=NOW(), accountActivated='Y'",
    user, realName, emptyForNull(email));
sqlUpdate(conn, dyStringContents(q));
dyStringFree(&q);
uint idx = sqlLastAutoId(conn);

struct oauthIdentity pending;
ZeroVar(&pending);
pending.provider = provider;
pending.subject = subject;
pending.email = email;
linkIdentity(conn, idx, &pending);

clearPendingIdentity();
loginAndReturn(user, idx);
}

void chooseAccountPage(struct sqlConnection *conn)
/* Ask the user which of several accounts sharing an email address to sign in to.  Used by
 * two flows: OAuth (oauth_pending_* in the cart -> the chosen account is linked to the social
 * identity) and the passwordless email link (emailLogin_* in the cart -> just sign in). */
{
char *provider = cartUsualString(cart, "oauth_pending_provider", "");
boolean emailMode = isEmpty(provider);
char *email = emailMode ? cartUsualString(cart, "emailLogin_email", "")
                        : cartUsualString(cart, "oauth_pending_email", "");
if (isEmpty(email) || (!emailMode && !pendingIdentityValid()))
    {
    displayLoginPage(conn);
    return;
    }
char query[512];
if (emailMode)
    // Only the accounts that hold the just-validated login token, matching what emailLogin saw.
    sqlSafef(query, sizeof(query),
        "SELECT * FROM gbMembers WHERE (email='%s' OR recovEmail='%s') AND loginToken='%s' "
        "AND loginToken<>'' AND loginTokenExpires > NOW() ORDER BY idx",
        email, email, cartUsualString(cart, "emailLogin_tokenMd5", ""));
else
    sqlSafef(query, sizeof(query),
        "SELECT * FROM gbMembers WHERE email='%s' ORDER BY idx", email);
struct gbMembers *list = gbMembersLoadByQuery(conn, query), *m;

hPrintf("<div id=\"chooseAccountBox\" class=\"centeredContainer formBox\">"
    "<h2>%s</h2>", brwName);
hPrintf("<h3>Choose an account</h3>");
if (emailMode)
    hPrintf("<p>The email address <b>%s</b> is associated with more than one %s account. "
        "Select the account you would like to sign in to.</p>", email, brwName);
else
    hPrintf("<p>The email address <b>%s</b> is associated with more than one %s account. "
        "Select the account you would like to sign in to; your %s login will be linked to it.</p>",
        email, brwName, oauthProviderLabel(provider));
hPrintf("<span style='color:red;'>%s</span>", errMsg ? errMsg : "");
hPrintf("<form method=\"post\" action=\"%s\" name=\"chooseAccountForm\">", hgLoginUrl);
hPrintf("<div class=\"inputGroup\">");
boolean first = TRUE;
for (m = list;  m != NULL;  m = m->next)
    {
    hPrintf("<div class=\"acctHelpSection\">"
        "<input name=\"hgLogin_chosenIdx\" type=\"radio\" value=\"%u\" id=\"acct_%u\"%s>"
        "<label for=\"acct_%u\" class=\"radioLabel\">%s</label></div>",
        m->idx, m->idx, first ? " checked" : "", m->idx, m->userName);
    first = FALSE;
    }
hPrintf("</div>");
hPrintf("<div class=\"formControls\">"
    "<input type=\"submit\" name=\"hgLogin.do.chooseAccount\" value=\"Sign In\" class=\"largeButton\">"
    " &nbsp;<a href=\"%s\" class=\"cancelButton\">Cancel</a>"
    "</div></form></div><!-- END - chooseAccountBox -->", getReturnToURL());
cartSaveSession(cart);
gbMembersFreeList(&list);
}

void chooseAccount(struct sqlConnection *conn)
/* Finish the "which account?" chooser: for OAuth, link the pending identity to the chosen
 * account; for the email link, just sign in.  Either way, only accept an account that really
 * matches the verified email (and, for the email link, still holds the valid token), never an
 * arbitrary username the client might submit. */
{
int chosenIdx = cartUsualInt(cart, "hgLogin_chosenIdx", 0);
char *provider = cartUsualString(cart, "oauth_pending_provider", "");
char query[512];

if (isEmpty(provider))
    {
    /* Passwordless email-link mode. */
    char *email = cartUsualString(cart, "emailLogin_email", "");
    char *tokenMd5 = cartUsualString(cart, "emailLogin_tokenMd5", "");
    if (isEmpty(email) || isEmpty(tokenMd5))
        {
        freez(&errMsg);
        errMsg = cloneString("Your login link expired. Please request a new one.");
        displayLoginPage(conn);
        return;
        }
    sqlSafef(query, sizeof(query),
        "SELECT * FROM gbMembers WHERE idx=%d AND (email='%s' OR recovEmail='%s') "
        "AND loginToken='%s' AND loginToken<>'' AND loginTokenExpires > NOW()",
        chosenIdx, email, email, tokenMd5);
    struct gbMembers *m = gbMembersLoadByQuery(conn, query);
    if (m == NULL)
        {
        freez(&errMsg);
        errMsg = cloneString("Please choose one of the listed accounts.");
        chooseAccountPage(conn);
        return;
        }
    /* Consume the token on every account that shared it (single use), then sign in. */
    sqlSafef(query, sizeof(query),
        "UPDATE gbMembers SET loginToken='' WHERE (email='%s' OR recovEmail='%s') AND loginToken='%s'",
        email, email, tokenMd5);
    sqlUpdate(conn, query);
    cartRemove(cart, "emailLogin_email");
    cartRemove(cart, "emailLogin_tokenMd5");
    cartRemove(cart, "hgLogin_chosenIdx");
    loginAndReturn(m->userName, m->idx);
    gbMembersFree(&m);
    return;
    }

/* OAuth mode. */
char *subject = cartUsualString(cart, "oauth_pending_subject", "");
char *email = cartUsualString(cart, "oauth_pending_email", "");
if (isEmpty(subject) || isEmpty(email) || !pendingIdentityValid())
    {
    freez(&errMsg);
    errMsg = cloneString("Your login session expired. Please sign in again.");
    displayLoginPage(conn);
    return;
    }
sqlSafef(query, sizeof(query),
    "SELECT * FROM gbMembers WHERE idx=%d AND email='%s'", chosenIdx, email);
struct gbMembers *m = gbMembersLoadByQuery(conn, query);
if (m == NULL)
    {
    freez(&errMsg);
    errMsg = cloneString("Please choose one of the listed accounts.");
    chooseAccountPage(conn);
    return;
    }
struct oauthIdentity pending;
ZeroVar(&pending);
pending.provider = provider;
pending.subject = subject;
pending.email = email;
linkIdentity(conn, m->idx, &pending);
clearPendingIdentity();
cartRemove(cart, "hgLogin_chosenIdx");
loginAndReturn(m->userName, m->idx);
gbMembersFree(&m);
}

static void resolveIdentity(struct sqlConnection *conn, struct oauthIdentity *id)
/* Log in the user behind an authenticated provider identity:
 *  1. If the provider gave a verified email matching MORE THAN ONE account, always let the
 *     user pick which one -- even if this identity was linked before. Because login cookies
 *     never expire, a user goes through OAuth very rarely, so an occasional pick is cheap
 *     and it lets a person with several same-email accounts choose freely each time.
 *  2. Else if the (provider,subject) is already linked, log into that account.
 *  3. Else if the verified email matches exactly one account, auto-link and log in.
 *  4. Else send the user to the "choose a username" page to finish a new account.
 * (Providers that don't release an email, e.g. ORCID, never reach step 1 or 3 and rely on
 *  the stored link from step 2.) */
{
struct gbMembers *matches = NULL;
int n = 0;
if (id->emailVerified && isNotEmpty(id->email))
    {
    char query[512];
    sqlSafef(query, sizeof(query),
        "SELECT * FROM gbMembers WHERE email='%s' ORDER BY idx", id->email);
    matches = gbMembersLoadByQuery(conn, query);
    n = slCount(matches);
    }

if (n > 1)
    {
    setPendingIdentity(id);
    gbMembersFreeList(&matches);
    chooseAccountPage(conn);
    return;
    }

struct gbMembers *linked = memberForIdentity(conn, id);
if (linked != NULL)
    {
    linkIdentity(conn, linked->idx, id);
    loginAndReturn(linked->userName, linked->idx);
    gbMembersFree(&linked);
    gbMembersFreeList(&matches);
    return;
    }

if (n == 1)
    {
    linkIdentity(conn, matches->idx, id);
    loginAndReturn(matches->userName, matches->idx);
    gbMembersFreeList(&matches);
    return;
    }

gbMembersFreeList(&matches);
setPendingIdentity(id);
completeAccountPage(conn);
}

void oauthStart(struct sqlConnection *conn)
/* Begin a social login: save an anti-CSRF state nonce (in the cart) and redirect the
 * browser to the provider's authorization page. */
{
char *provider = cgiUsualString("provider", "");
if (!oauthProviderEnabled(provider))
    {
    freez(&errMsg);
    errMsg = cloneString("That login method is not available on this server.");
    displayLoginPage(conn);
    return;
    }
char *state = makeRandomKey(128+33);
cartSetString(cart, "oauth_state", state);
cartSetString(cart, "oauth_provider", provider);
/* returnto is already in the cart from the incoming link; leave it in place so the
 * provider round-trip returns the user to where they started. */
char *url = oauthLoginUrl(provider, hgLoginUrl, state);
if (isEmpty(url))
    {
    freez(&errMsg);
    errMsg = cloneString("Could not start social login. Please try again.");
    displayLoginPage(conn);
    return;
    }
jsInlineF("window.location = '%s';\n", url);
}

void oauthReturn(struct sqlConnection *conn)
/* Handle the provider's redirect back to us: verify state, exchange the code for the
 * user's identity, and resolve/auto-link the account. */
{
char *state = cgiUsualString("state", "");
char *savedState = cartUsualString(cart, "oauth_state", "");
char *provider = cartUsualString(cart, "oauth_provider", "");
cartRemove(cart, "oauth_state");   // one-time use

char *errParam = cgiUsualString("error", "");
if (isNotEmpty(errParam))
    {
    // The provider redirected back with an OAuth error instead of a code (e.g. the user
    // declined, or the client is misconfigured/unapproved).  Show its message rather than
    // silently falling through to another page.
    char *desc = cgiUsualString("error_description", "");
    struct dyString *dy = dyStringNew(256);
    dyStringAppend(dy, "Social login failed. ");
    if (isNotEmpty(desc))
        dyStringPrintf(dy, "%s ", htmlEncode(desc));
    dyStringPrintf(dy, "(%s)", htmlEncode(errParam));
    freez(&errMsg);
    errMsg = dyStringCannibalize(&dy);
    displayLoginPage(conn);
    return;
    }
if (isEmpty(state) || isEmpty(savedState) || differentString(state, savedState))
    {
    freez(&errMsg);
    errMsg = cloneString("Your login session expired or was invalid. Please try again.");
    displayLoginPage(conn);
    return;
    }
char *code = cgiUsualString("code", "");
struct oauthIdentity *id = oauthFetchIdentity(provider, code, hgLoginUrl);
if (id == NULL)
    {
    freez(&errMsg);
    errMsg = cloneString("We could not complete the social login. Please try again.");
    displayLoginPage(conn);
    return;
    }
resolveIdentity(conn, id);
oauthIdentityFree(&id);
}

void emailLinkPage(struct sqlConnection *conn)
/* Standalone page that asks for an email address and sends a one-time login link. */
{
if (!emailLinkEnabled())
    {
    displayLoginPage(conn);
    return;
    }
hPrintf("<div id=\"emailLinkBox\" class=\"centeredContainer formBox\">"
    "<h2>%s</h2>", brwName);
hPrintf("<h3>Email me a sign-in link</h3>");
hPrintf("<p>Enter your email address and we'll send you a link that signs you in without a "
    "password. This is handy on a computer where you don't have your password saved.</p>");
hPrintf("<span style='color:red;'>%s</span>", errMsg ? errMsg : "");
hPrintf("<form method=\"post\" action=\"%s\" name=\"emailLinkForm\">", hgLoginUrl);
hPrintf("<div class=\"inputGroup\">"
    "<label for=\"emailLink\">Email address</label>"
    "<input type=\"text\" name=\"hgLogin_email\" value=\"%s\" size=\"30\" id=\"emailLink\">"
    "</div>", cartUsualString(cart, "hgLogin_email", ""));
hPrintf("<div class=\"formControls\">"
    "<input type=\"submit\" name=\"hgLogin.do.sendEmailLink\" value=\"Send login link\" class=\"largeButton\">"
    " &nbsp;<a href=\"%s\" class=\"cancelButton\">Cancel</a>"
    "</div></form></div><!-- END - emailLinkBox -->", getReturnToURL());
cartSaveSession(cart);
}

void displayLoginLinkSuccess()
/* Confirmation shown after a passwordless login link is (possibly) emailed.  Phrased so it
 * does not reveal whether an account exists for the address. */
{
char *email = cartUsualString(cart, "hgLogin_sendMailTo", "");
hPrintf("<div id=\"confirmationBox\" class=\"centeredContainer formBox\">"
    "<h2>%s</h2>", brwName);
hPrintf("<p id=\"confirmationMsg\" class=\"confirmationTxt\">If an account exists for "
    "<B>%s</B>, a login link has been sent to that address.<BR><BR>"
    "Click the link in that email to sign in &mdash; no password needed. "
    "The link works once and expires in one hour.</p>", email);
hPrintf("<p>If you don't see the email, please check your spam folder.</p>");
hPrintf("<p><a href=\"%s?hgLogin.do.displayLoginPage=1\">Return to Login</a></p>\n", hgLoginUrl);
cartRemove(cart, "hgLogin_email");
cartRemove(cart, "hgLogin_sendMailTo");
cartRemove(cart, "hgLogin_helpWith");
}

void sendLoginLinkMail(char *email, char *token)
/* Email a one-time passwordless login link to an address.  The link identifies the address,
 * not a single account: if the address has several accounts, the user picks one after
 * clicking (see emailLogin), so one email covers them all. */
{
char subject[256];
char msg[4096];
char url[512];
char *remoteAddr = getenv("REMOTE_ADDR");
safef(url, sizeof(url), "%s?hgLogin.do.emailLogin=1&email=%s&token=%s",
    hgLoginUrl, cgiEncode(email), cgiEncode(token));
safef(subject, sizeof(subject), "Your login link for the %s", brwName);
safef(msg, sizeof(msg),
    "Someone (probably you, from IP address %s) requested a login link for the %s account "
    "registered to this email address.\nClick the link below to sign in without a password. "
    "It works once and expires in one hour:\n\n%s\n\nIf you did not request this, you can "
    "safely ignore this email.\n\n%s\n%s",
    remoteAddr, brwName, url, signature, returnAddr);
sendActMailOut(email, subject, msg);
}

void sendEmailLink(struct sqlConnection *conn)
/* Generate and email a one-time passwordless login link to the address on file. */
{
if (!emailLinkEnabled())
    {
    displayLoginPage(conn);
    return;
    }
char *email = cartUsualString(cart, "hgLogin_email", "");
if (isEmpty(email) || spc_email_isvalid(email) == 0)
    {
    freez(&errMsg);
    errMsg = cloneString("Please enter a valid email address.");
    emailLinkPage(conn);
    return;
    }
char query[512];
sqlSafef(query, sizeof(query),
    "SELECT * FROM gbMembers WHERE email='%s' OR recovEmail='%s'", email, email);
struct gbMembers *list = gbMembersLoadByQuery(conn, query), *m;
if (list != NULL)
    {
    /* One token for the address, stored on every account that uses it, and one email.
     * The user proves they own the address by clicking; only then (in emailLogin) do we
     * reveal the accounts and let them choose, so we never disclose accounts to someone
     * who merely typed the address here. */
    char *token = makeRandomKey(128+33);
    char *tokenMD5 = generateTokenMD5(token);
    for (m = list; m != NULL; m = m->next)
        {
        sqlSafef(query, sizeof(query),
            "UPDATE gbMembers SET loginToken='%s', "
            "loginTokenExpires=DATE_ADD(NOW(), INTERVAL 1 HOUR) WHERE idx=%u",
            tokenMD5, m->idx);
        sqlUpdate(conn, query);
        }
    sendLoginLinkMail(email, token);
    }
gbMembersFreeList(&list);
/* Always show the same confirmation, even when no account matched, so we don't reveal
 * whether an address is registered. */
cartSetString(cart, "hgLogin_sendMailTo", email);
displayLoginLinkSuccess();
}

void emailLogin(struct sqlConnection *conn)
/* Validate a one-time email login token.  The token proves the user owns the address; if it
 * matches one account, log straight in; if it matches several accounts that share the
 * address, show the account chooser (the same one the OAuth flow uses). */
{
if (!emailLinkEnabled())
    {
    displayLoginPage(conn);
    return;
    }
char *email = cgiUsualString("email", "");
char *token = cgiUsualString("token", "");
char *tokenMD5 = generateTokenMD5(token);
char query[512];
sqlSafef(query, sizeof(query),
    "SELECT * FROM gbMembers WHERE (email='%s' OR recovEmail='%s') AND loginToken='%s' "
    "AND loginToken<>'' AND loginTokenExpires > NOW() ORDER BY idx", email, email, tokenMD5);
struct gbMembers *list = gbMembersLoadByQuery(conn, query);
int n = slCount(list);
if (n == 0)
    {
    freez(&errMsg);
    errMsg = cloneString("This login link is invalid or has expired. Please request a new one.");
    displayLoginPage(conn);
    }
else if (n == 1)
    {
    sqlSafef(query, sizeof(query),
        "UPDATE gbMembers SET loginToken='', lastUse=NOW() WHERE idx=%u", list->idx);
    sqlUpdate(conn, query);
    loginAndReturn(list->userName, list->idx);
    }
else
    {
    /* Several accounts share this now-verified address: stash the proof and let the user
     * pick one.  chooseAccount re-checks the token before logging in. */
    cartSetString(cart, "emailLogin_email", email);
    cartSetString(cart, "emailLogin_tokenMd5", tokenMD5);
    chooseAccountPage(conn);
    }
gbMembersFreeList(&list);
}

void doMiddle(struct cart *theCart)
/* Write the middle parts of the HTML page.
 * This routine sets up some globals and then
 * dispatches to the appropriate page-maker. */
{
struct sqlConnection *conn = hConnectCentral();

// on mirrors, try to add the field 'recovEmail' to gbMembers. This may or may not work, depending on their config
if (sqlFieldIndex(conn, "gbMembers", "recovEmail") == -1) {
    autoUpgradeTableAddColumn(conn, "gbMembers", "recovEmail", "varchar(255)", FALSE, "''");
}

// columns for the passwordless email-link login feature
if (sqlFieldIndex(conn, "gbMembers", "loginToken") == -1)
    autoUpgradeTableAddColumn(conn, "gbMembers", "loginToken", "varchar(255)", FALSE, "NULL");
if (sqlFieldIndex(conn, "gbMembers", "loginTokenExpires") == -1)
    autoUpgradeTableAddColumn(conn, "gbMembers", "loginTokenExpires", "DATETIME", FALSE, "NULL");

// table linking accounts to social (Google/ORCID) identities; only needed where OAuth is set up
if (oauthAnyProviderEnabled())
    createIdentityTable(conn);

cart = theCart;
safecpy(brwName,sizeof(brwName), browserName());
safecpy(brwAddr,sizeof(brwAddr), browserAddr());
safecpy(signature,sizeof(signature), mailSignature());
safecpy(returnAddr,sizeof(returnAddr), mailReturnAddr());
pwdEyeIconEnabled = cfgOptionBooleanDefault(CFG_LOGIN_PWD_EYE_ICON, TRUE);

// A provider's OAuth redirect back to us carries 'code' (success) or 'error' (failure) but
// none of our own hgLogin.do.* variables, so detect it up front.  We gate on an OAuth flow
// being in progress (oauth_provider set by oauthStart) so a stray code/error param can't
// trigger this.  Error returns may omit 'code' and even 'state', so we must not require them.
if ((cgiOptionalString("code") != NULL || cgiOptionalString("error") != NULL)
    && isNotEmpty(cartUsualString(cart, "oauth_provider", "")))
    oauthReturn(conn);
else if (cartVarExists(cart, "hgLogin.do.oauthStart"))
    oauthStart(conn);
else if (cartVarExists(cart, "hgLogin.do.completeAccount"))
    completeAccount(conn);
else if (cartVarExists(cart, "hgLogin.do.chooseAccount"))
    chooseAccount(conn);
else if (cartVarExists(cart, "hgLogin.do.emailLinkPage"))
    emailLinkPage(conn);
else if (cartVarExists(cart, "hgLogin.do.sendEmailLink"))
    sendEmailLink(conn);
else if (cartVarExists(cart, "hgLogin.do.emailLogin"))
    emailLogin(conn);
else if (cartVarExists(cart, "hgLogin.do.changePasswordPage"))
    changePasswordPage(conn);
else if (cartVarExists(cart, "hgLogin.do.changePassword"))
    changePassword(conn);
else if (cartVarExists(cart, "hgLogin.do.changeEmailPage"))
    changeEmailPage(conn);
else if (cartVarExists(cart, "hgLogin.do.changeEmail"))
    changeEmail(conn);
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
earlyBotCheck(enteredMainTime, "hgLogin", delayFraction, 0, 0, "html");
pushCarefulMemHandler(100000000);
cgiSpoof(&argc, argv);
/* Use the site's standard time-stamped resource link (appends ?v=<mtime>) so browsers pick
 * up CSS changes after a release instead of serving a stale cached copy. */
htmlSetStyleSheet(webTimeStampedLinkToResource("userAccounts.css", FALSE));
htmlSetStyle(htmlStyleUndecoratedLink);
htmlSetBgColor(HG_CL_OUTSIDE);
htmlSetFormClass("accountScreen");

struct dyString *dy;
dy = dyStringCreate("%shgLogin", hLoginHostCgiBinUrl());
hgLoginUrl = dyStringCannibalize(&dy);

oldCart = hashNew(10);
cartHtmlShell("Login - UCSC Genome Browser", doMiddle, hUserCookie(), excludeVars, oldCart);
cgiExitTime("hgLogin", enteredMainTime);
return 0;
}
