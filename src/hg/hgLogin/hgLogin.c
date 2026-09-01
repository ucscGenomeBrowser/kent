/* hgLogin - Administer UCSC Genome Browser membership - signup, lost password, etc. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include <openssl/evp.h>
#include <openssl/opensslv.h>
#include <openssl/md5.h>

#include "common.h"
#include "hash.h"
#include "portable.h"
#include "hmac.h"
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
     "hgLogin_curPassword", "code", "state", "provider", "user", "token",
     "newEmail", "recovEmail", "exp", "sig",
     "hgLogin_newRecovEmail1", "hgLogin_newRecovEmail2", NULL };
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
boolean recovEmailVerifyOk = FALSE; /* TRUE when gbMembers has the recovEmailVerified column, so
    a confirmed recovery address can be told apart from one that was merely typed into the signup
    form.  Set in doMiddle() after the auto-upgrade; FALSE on a mirror where the ALTER failed. */

/* for earlyBotCheck() function at the beginning of main() */
#define delayFraction   1.0    /* standard penalty is 1.0 for most CGIs */

/* Forward declarations for functions used before their definitions. */
static void printSocialButtons(boolean dividerAbove, boolean dividerBelow, char *action);
static void printEmailLinkButton();
static boolean emailLinkEnabled();
static boolean recovEmailChangeEnabled();
void changeRecovEmailPage(struct sqlConnection *conn);
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

char *getReturnToURL()
/* get URL from cart var returnto; if empty, make URL to hgSession on login host.  */
{
char *returnURL = cartUsualString(cart, "returnto", "");

if (isEmpty(returnURL))
    {
    char returnTo[2048];
    safef(returnTo, sizeof(returnTo), "%shgSession?hgS_doMainPage=1", hLoginHostCgiBinUrl());
    return cloneString(returnTo);
    }

/* Check the shape of the URL on every install.  login.approvedReturn is optional, and
 * where it is set it only matches the front of the URL, so the rest of the URL is
 * unchecked either way.  The check lives in wikiLink.c so that the CGIs building a
 * returnto can apply the same rules before they write the link. */
if (!loginReturnUrlIsAcceptable(returnURL))
    {
    hDumpStackDisallow();
    errAbort("Error: Invalid returnto URL. Please send email to genome-www@soe.ucsc.edu "
            "with the returnto argument from the URL (or just the full URL) so we can "
            "fix this.");
    }
return cloneString(returnURL);
}

static char *getReturnToUrlForAttr()
/* getReturnToURL() escaped for printing inside an href="" attribute.  Escaping the ampersand
 * is the part that matters here: the browser expands an entity in an attribute value, so
 * javascript&colon;alert(1) would otherwise become a javascript: URL after the checks above
 * have passed it. */
{
return htmlEncode(getReturnToURL());
}

void returnToURL(int delay)
/* delay for delay mill-seconds then return to the "returnto" URL */
{
char *returnURL = javaScriptLiteralEncode(getReturnToURL());
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
char *returnURL = getReturnToUrlForAttr();
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
char *sendMailTo = htmlEncode(cartUsualString(cart, "hgLogin_sendMailTo", ""));  // printed into the page; escape (XSS)
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
char *username = htmlEncode(cgiUsualString("user",""));  // printed into the page; escape (XSS)
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
        "Click <a href=\"%s?hgLogin.do.displayAccHelpPage=1\">here</a> to return.<br>",
        htmlEncode(obj), htmlEncode(email), hgLoginUrl );
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

static char *sqlAddressMatch(char *email)
/* Return a SQL fragment matching the gbMembers rows that belong to whoever controls email: the
 * accounts carrying it as their primary address, plus the accounts carrying it as a *confirmed*
 * recovery address.  An unconfirmed recovEmail is only a string that a signup form typed in --
 * nobody ever proved they can read mail there -- so matching it would let someone who registered
 * with a victim's address as their recovery address capture that victim's login (see
 * confirmRecovEmail).  Callers must pass a non-empty email, or rows with a blank recovEmail
 * match.  Result is allocd and carries the sqlSafef prefix; embed it with %-s. */
{
struct dyString *dy = sqlDyStringCreate("(email='%s'", email);
if (recovEmailVerifyOk)
    sqlDyStringPrintf(dy, " OR (recovEmail='%s' AND recovEmailVerified='Y')", email);
else
    /* A mirror whose gbMembers predates the column: we cannot tell confirmed from unconfirmed,
     * so keep the old behavior rather than locking those users out of their own accounts. */
    sqlDyStringPrintf(dy, " OR recovEmail='%s'", email);
sqlDyStringPrintf(dy, ")");
return dyStringCannibalize(&dy);
}

void sendUsername(struct sqlConnection *conn, char *email)
/* email user username(s)  */
{
struct sqlResult *sr;
char **row;
char query[1024];

/* find all the user names associated with this email address */
char userList[512]="";
char *addrMatch = sqlAddressMatch(email);
sqlSafef(query,sizeof(query),"SELECT * FROM gbMembers WHERE %-s", addrMatch);
freeMem(addrMatch);
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
        "Click <a href=\"%s?hgLogin.do.displayAccHelpPage=1\">here</a> to return.<br>",
        htmlEncode(obj), htmlEncode(email), hgLoginUrl );
    }
else
    {
    jsInlineF(
        "window.location = '%s?hgLogin.do.displayMailSuccessPwd=1&user=%s';\n"
        , hgLoginUrl, cgiEncodeFull(username));
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
// these go into value="" attributes further down; escape them (reflected XSS)
char *email = htmlEncode(cartUsualString(cart, "hgLogin_email", ""));
char *username = htmlEncode(cartUsualString(cart, "hgLogin_userName", ""));

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
    "</div><!-- END - accountHelpBox -->", username, email, getReturnToUrlForAttr());
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

/* Only a confirmed recovery address gets a copy: an unconfirmed one never proved it belongs to
 * this account, and a new password must not be mailed to a stranger whose address someone typed
 * into the signup form. */
if (recovEmailVerifyOk)
    sqlSafef(query,sizeof(query),
        "SELECT recovEmail FROM gbMembers WHERE userName='%s' AND recovEmailVerified='Y'",
        username);
else
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
/* print a clickable eye icon as a normal sibling right after a password
 * input (not overlapping it); slashId is the <line> toggled to show
 * "hidden". No-op if disabled via hg.conf login.pwdEyeIcon. */
{
if (!pwdEyeIconEnabled)
    return;
hPrintf(
    "<span id=\"%s\" title=\"Show/hide password\" "
    "style=\"display:inline-block; margin-left:6px; vertical-align:middle; "
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
// goes into a value="" attribute further down; escape it (reflected XSS)
char *username = htmlEncode(cartUsualString(cart, "hgLogin_userName", ""));
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
    "<span style=\"display:inline-flex; align-items:center;\">"
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
    , hgLoginUrl, getReturnToUrlForAttr());
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
    errMsg = cloneString("This activation link is not valid or has already been used.");
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
    htmlEncode(cartUsualString(cart, "hgLogin_userName", "")));  // value="" attribute; escape (XSS)
hPrintf("<div class=\"inputGroup\">"
    "\n"
    "<label for=\"currentPw\">Current or emailed password</label>"
    "<span style=\"display:inline-flex; align-items:center;\">"
    "<input type=\"password\" name=\"hgLogin_password\" value=\"\" size=\"30\" id=\"currentPw\">");
printPwdEyeIcon("curPwEyeIcon", "curPwEyeSlash");
hPrintf(
    "</span>"
    "</div>"
    "\n"
    "<div class=\"inputGroup\">"
    "<label for=\"newPw1\">New password</label>"
    "<span style=\"display:inline-flex; align-items:center;\">"
    "<input type=\"password\" name=\"hgLogin_newPassword1\" value=\"\" size=\"30\" id=\"newPw1\">");
printPwdEyeIcon("newPw1EyeIcon", "newPw1EyeSlash");
hPrintf(
    "</span>"
    "</div>"
    "\n"
    "<div class=\"inputGroup\">"
    "<label for=\"newPw2\">Re-enter new password</label>"
    "<span style=\"display:inline-flex; align-items:center;\">"
    "<input type=\"password\" name=\"hgLogin_newPassword2\" value=\"\" size=\"30\" id=\"newPw2\">");
printPwdEyeIcon("newPw2EyeIcon", "newPw2EyeSlash");
hPrintf(
    "</span>"
    "</div>"
    "\n"
    "<div class=\"formControls\">"
    "    <input type=\"submit\" name=\"hgLogin.do.changePassword\" value=\"Change password\" class=\"largeButton\"> &nbsp; "
    "    <a href=\"%s\" class=\"cancelButton\">Cancel</a>"
    "\n"
    "</div>"
    "</form>"
    "\n"
    "</div><!-- END - changePwBox -->"
    "\n", getReturnToUrlForAttr());
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
    errMsg = cloneString("New password must be at least 5 characters long.");
    changePasswordPage(conn);
    return;
    }
if (!newPassword2 || sameString(newPassword2,"") )
    {
    freez(&errMsg);
    errMsg = cloneString("Re-enter new password field cannot be blank.");
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

static char *changeEmailSig(char *user, char *curEmail, char *newEmail, char *expStr)
/* HMAC-MD5 over a pending email change, keyed by the secret login.cookieSalt.  It goes in the
 * confirmation link so that clicking the link -- and only clicking it -- applies the change,
 * proving the new address really reaches the requester.  curEmail is the account's address when
 * the link was minted; because confirmChangeEmail recomputes the signature from the address
 * currently on the account, a link stops validating once it has been used (the address is no
 * longer curEmail), so each link works exactly once and a stale link cannot silently undo a
 * newer change.  Result is allocd. */
{
char *salt = cfgOption(CFG_LOGIN_COOKIE_SALT);
if (isEmpty(salt))
    errAbort("Confirming an email change requires %s in hg.conf, set to a secret random "
        "string.  Without a secret we cannot sign the confirmation link.", CFG_LOGIN_COOKIE_SALT);
char buf[1024];
safef(buf, sizeof(buf), "changeEmail|%s|%s|%s|%s",
    emptyForNull(user), emptyForNull(curEmail), emptyForNull(newEmail), emptyForNull(expStr));
return hmacMd5(salt, buf);
}

static void sendChangeEmailConfirmMail(char *newEmail, char *user, char *curEmail)
/* Email a one-time link to newEmail that, when opened, changes user's address to newEmail.
 * curEmail is the account's current address; it is folded into the signature so the link stops
 * working once the change has been applied (see changeEmailSig). */
{
char expStr[32];
safef(expStr, sizeof(expStr), "%ld", clock1() + 3600);   // link good for one hour
char *sig = changeEmailSig(user, curEmail, newEmail, expStr);
char url[1024];
safef(url, sizeof(url),
    "%s?hgLogin.do.confirmChangeEmail=1&user=%s&newEmail=%s&exp=%s&sig=%s",
    hgLoginUrl, cgiEncode(user), cgiEncode(newEmail), expStr, sig);
char subject[256];
safef(subject, sizeof(subject), "Confirm your new %s email address", brwName);
char *remoteAddr = getenv("REMOTE_ADDR");
char message[4096];
safef(message, sizeof(message),
    "Someone (probably you, from IP address %s) asked to change the email address on the %s "
    "account \"%s\" to this address.\nTo confirm the change, open this link in your browser:\n\n"
    "%s\n\nThe link works once and expires in one hour.\n\n%s\n%s",
    emptyForNull(remoteAddr), brwName, user, url, signature, returnAddr);
sendActMailOut(newEmail, subject, message);
freeMem(sig);
}

static void sendChangeEmailAlertMail(char *oldEmail, char *user, char *newEmail)
/* Tell the OLD address that the account's email was just changed, so its owner finds out if the
 * change was not theirs and can ask us to undo it.  This is the notice that protects the current
 * owner -- confirming the new address only proves the new mailbox is reachable. */
{
char subject[256];
safef(subject, sizeof(subject), "Your %s email address was changed", brwName);
char *remoteAddr = getenv("REMOTE_ADDR");
char message[4096];
safef(message, sizeof(message),
    "The email address on the %s account \"%s\" was just changed to %s (request from IP address "
    "%s).\n\nIf you made this change, nothing more is needed.  If you did NOT, please reply to "
    "this message right away so we can help you secure the account.\n\n%s\n%s",
    brwName, user, newEmail, emptyForNull(remoteAddr), signature, returnAddr);
sendActMailOut(oldEmail, subject, message);
}

static char *recovEmailSig(char *user, char *newRecov, char *curRecov, char *curVerified,
                           char *expStr)
/* HMAC-MD5 over a pending recovery address, keyed by the secret login.cookieSalt.  It goes in
 * the link mailed to that address, so that opening the link -- and only opening it -- puts the
 * address on the account and marks it confirmed, proving the mailbox really does reach the
 * person who claimed it.  One signature serves both cases: the address given at signup (where
 * newRecov is already stored, unconfirmed) and a later change (where it is not stored at all
 * until the link is opened, so a typo cannot cost the user a working recovery address).
 * curRecov and curVerified are the account's stored address and flag when the link was minted;
 * because confirmRecovEmail recomputes the signature from what is on the account now, a link
 * stops validating once it has been used, so each link works exactly once and a stale link
 * cannot quietly undo a newer change.  Result is allocd. */
{
char *salt = cfgOption(CFG_LOGIN_COOKIE_SALT);
if (isEmpty(salt))
    errAbort("Confirming a recovery email address requires %s in hg.conf, set to a secret random "
        "string.  Without a secret we cannot sign the confirmation link.", CFG_LOGIN_COOKIE_SALT);
char buf[1024];
safef(buf, sizeof(buf), "recovEmail|%s|%s|%s|%s|%s",
    emptyForNull(user), emptyForNull(newRecov), emptyForNull(curRecov),
    emptyForNull(curVerified), emptyForNull(expStr));
return hmacMd5(salt, buf);
}

static void sendRecovEmailConfirmMail(char *recovEmail, char *user, char *curRecov,
                                      char *curVerified)
/* Email a one-time link to recovEmail that, when opened, puts it on account user as a confirmed
 * recovery address.  Until that happens the address counts for nothing: it cannot sign anyone in
 * and it gets no copy of a password reset.  The link lasts a week, like the account activation
 * mail, because a recovery mailbox is often not the one its owner reads every day. */
{
char expStr[32];
safef(expStr, sizeof(expStr), "%ld", clock1() + 7*24*3600);   // link good for a week
char *sig = recovEmailSig(user, recovEmail, curRecov, curVerified, expStr);
char url[1024];
safef(url, sizeof(url),
    "%s?hgLogin.do.confirmRecovEmail=1&user=%s&recovEmail=%s&exp=%s&sig=%s",
    hgLoginUrl, cgiEncode(user), cgiEncode(recovEmail), expStr, sig);
char subject[256];
safef(subject, sizeof(subject), "Confirm your %s recovery email address", brwName);
char *remoteAddr = getenv("REMOTE_ADDR");
char message[4096];
safef(message, sizeof(message),
    "Someone (probably you, from IP address %s) gave this address as the recovery email address "
    "for the %s account \"%s\".\nTo confirm that this mailbox is yours, open this link in your "
    "browser:\n\n%s\n\nThe link works once and expires in seven days.  Until it is opened, this "
    "address cannot be used to sign in to that account and will not receive a password reset.\n\n"
    "If this is *not* you, do not open the link: someone typed your address by mistake, and "
    "ignoring this message is all it takes to keep them from using it.\n\n%s\n%s",
    emptyForNull(remoteAddr), brwName, user, url, signature, returnAddr);
/* Not sendActMailOut(): that exits the CGI when the address will not take mail, which would
 * end the signup response after the account has already been created and its activation mail
 * sent.  A recovery address is optional and easy to mistype, so a bad one must not derail
 * signing up -- the address simply stays unconfirmed, which is the safe state. */
if (mailViaPipeBounce(recovEmail, subject, message, returnAddr) == -1)
    fprintf(stderr, "hgLogin: could not mail recovery-address confirmation to %s for account "
        "%s\n", recovEmail, user);
freeMem(sig);
}

static void sendRecovEmailChangeAlertMail(char *email, char *user, char *newRecov)
/* Tell the account's main address that its recovery address just changed, so its owner finds
 * out if the change was not theirs.  A confirmed recovery address can sign in to the account,
 * so moving it deserves the same notice as changing the main address itself. */
{
char subject[256];
safef(subject, sizeof(subject), "Your %s recovery email address was changed", brwName);
char *remoteAddr = getenv("REMOTE_ADDR");
char message[4096];
safef(message, sizeof(message),
    "The recovery email address on the %s account \"%s\" was just changed to %s (request from "
    "IP address %s).\n\nIf you made this change, nothing more is needed.  If you did NOT, please "
    "reply to this message right away so we can help you secure the account.\n\n%s\n%s",
    brwName, user, newRecov, emptyForNull(remoteAddr), signature, returnAddr);
sendActMailOut(email, subject, message);
}

void changeEmailPage(struct sqlConnection *conn)
/* Draw the change-email page for the currently logged-in user.  The account is taken from
 * the validated login cookie (wikiLinkUserName), never from a form field, so a user can only
 * change their own email.  Where the account has a password we also ask for it here, so a
 * borrowed login cookie alone cannot change the address (and from there take over the account
 * via password recovery).  Social-login accounts have no password and are asked for none; for
 * them the new address is instead confirmed by email before it takes effect (see changeEmail). */
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
sqlSafef(query, sizeof(query), "SELECT password FROM gbMembers WHERE userName='%s'", user);
boolean hasPassword = isNotEmpty(sqlQuickString(conn, query));
char *encUser = htmlEncode(user);
char *encCurEmail = htmlEncode(isNotEmpty(curEmail) ? curEmail : "(none)");

hPrintf("<div id=\"changeEmailBox\" class=\"centeredContainer formBox\">"
    "<h2>%s</h2>", brwName);
hPrintf("<h3>Change Email</h3>");
hPrintf("<p><span style='color:red;'>%s</span></p>", errMsg ? errMsg : "");
hPrintf("<form method=\"post\" action=\"%s\" name=\"changeEmailForm\">", hgLoginUrl);
hPrintf("<p>Signed in as <b>%s</b>.<br>Current email address: <b>%s</b></p>",
    encUser, encCurEmail);
freeMem(encUser);
freeMem(encCurEmail);
if (hasPassword)
    hPrintf("<div class=\"inputGroup\">"
        "<label for=\"curPassword\">Current password</label>"
        "<input type=\"password\" name=\"hgLogin_curPassword\" value=\"\" size=\"30\" id=\"curPassword\">"
        "</div>");
hPrintf("<div class=\"inputGroup\">"
    "<label for=\"newEmail1\">New email address</label>"
    "<input type=\"text\" name=\"hgLogin_newEmail1\" value=\"\" size=\"30\" id=\"newEmail1\">"
    "</div>");
hPrintf("<div class=\"inputGroup\">"
    "<label for=\"newEmail2\">Re-enter new email address</label>"
    "<input type=\"text\" name=\"hgLogin_newEmail2\" value=\"\" size=\"30\" id=\"newEmail2\">"
    "</div>");
hPrintf("<div class=\"formControls\">"
    "<input type=\"submit\" name=\"hgLogin.do.changeEmail\" value=\"Change email\" class=\"largeButton\">"
    " &nbsp;<a href=\"%s\" class=\"cancelButton\">Cancel</a>"
    "</div></form></div><!-- END - changeEmailBox -->", getReturnToUrlForAttr());
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
/* Re-authenticate where we can: if the account has a password, require the current one.  A
 * stolen login cookie by itself must not be enough to change the address. */
char query[512];
sqlSafef(query, sizeof(query), "SELECT password FROM gbMembers WHERE userName='%s'", user);
char *curPwd = sqlQuickString(conn, query);
if (isNotEmpty(curPwd))
    {
    char *given = cartUsualString(cart, "hgLogin_curPassword", "");
    if (isEmpty(given) || !checkPwd(given, curPwd))
        {
        freez(&errMsg);
        errMsg = cloneString("Please enter your current password.");
        changeEmailPage(conn);
        return;
        }
    }
/* Do not change the address yet: email a one-time confirmation link to the NEW address and
 * apply the change only when it is clicked (see confirmChangeEmail).  This proves the address
 * is real and controlled by the requester, so an unconfirmed address cannot silently become
 * the account's recovery address. */
sqlSafef(query, sizeof(query), "SELECT email FROM gbMembers WHERE userName='%s'", user);
char *curEmail = sqlQuickString(conn, query);
sendChangeEmailConfirmMail(email1, user, curEmail);
cartRemove(cart, "hgLogin_newEmail1");
cartRemove(cart, "hgLogin_newEmail2");
cartRemove(cart, "hgLogin_curPassword");
char *encEmail = htmlEncode(email1);
hPrintf("<div class=\"centeredContainer formBox\"><h2>%s</h2>", brwName);
hPrintf("<h3>Almost done. Please check your email</h3>");
hPrintf("<p>We sent a confirmation link to <b>%s</b>. Open the link in that message to finish "
    "changing your email address. The link works once and expires in one hour.</p></div>",
    encEmail);
freeMem(encEmail);
returnToURL(3000);
}

void confirmChangeEmail(struct sqlConnection *conn)
/* Apply a confirmed email change.  Reached by opening the signed link sent to the new address
 * (see sendChangeEmailConfirmMail); the signature and its expiry are the authorization, so this
 * does not require a login cookie -- the link may be opened from the new mailbox in any browser. */
{
if (!emailLinkEnabled())
    {
    displayLoginPage(conn);
    return;
    }
char *user = cgiUsualString("user", "");
char *newEmail = cgiUsualString("newEmail", "");
char *expStr = cgiUsualString("exp", "");
char *sig = cgiUsualString("sig", "");
/* Recompute the signature over the address currently on the account.  Once the change has been
 * applied that address is newEmail, so re-opening the same link no longer matches: the link works
 * exactly once, and a stale link cannot silently undo a newer change. */
char query[512];
sqlSafef(query, sizeof(query), "SELECT email FROM gbMembers WHERE userName='%s'", user);
char *oldEmail = sqlQuickString(conn, query);
char *expected = changeEmailSig(user, emptyForNull(oldEmail), newEmail, expStr);
boolean sigOk = isNotEmpty(sig) && sameString(sig, expected);
freeMem(expected);
if (!sigOk || isEmpty(user) || spc_email_isvalid(newEmail) == 0)
    {
    freez(&errMsg);
    errMsg = cloneString("This confirmation link is not valid or has already been used.");
    displayLoginPage(conn);
    return;
    }
if (clock1() > atol(expStr))
    {
    freez(&errMsg);
    errMsg = cloneString("This confirmation link has expired. Please request the change again.");
    displayLoginPage(conn);
    return;
    }
sqlSafef(query, sizeof(query),
    "UPDATE gbMembers SET email='%s', lastUse=NOW() WHERE userName='%s'", newEmail, user);
sqlUpdate(conn, query);
/* Alert the previous address that the change happened, so a hijack is noticed. */
if (isNotEmpty(oldEmail) && differentWord(oldEmail, newEmail))
    sendChangeEmailAlertMail(oldEmail, user, newEmail);
char *encEmail = htmlEncode(newEmail);
hPrintf("<div class=\"centeredContainer formBox\"><h2>%s</h2>", brwName);
hPrintf("<h3>Your email address has been changed.</h3>");
hPrintf("<p>Your email address is now <b>%s</b>.</p></div>", encEmail);
freeMem(encEmail);
returnToURL(1500);
}

void confirmRecovEmail(struct sqlConnection *conn)
/* Mark a recovery address confirmed.  Reached by opening the signed link mailed to that address
 * (see sendRecovEmailConfirmMail); the signature and its expiry are the authorization, so this
 * needs no login cookie -- the link may be opened from that mailbox in any browser. */
{
if (!recovEmailVerifyOk || isEmpty(cfgOption(CFG_LOGIN_COOKIE_SALT)))
    {
    /* No column to record the answer in, or no secret to check the signature against, so the
     * link cannot have come from us.  Checked before recovEmailSig(), which aborts without a
     * secret: a link is only ever minted where one is configured, so reaching here means a
     * hand-made URL and it deserves the ordinary refusal, not an error page. */
    freez(&errMsg);
    errMsg = cloneString("This confirmation link is not valid.");
    displayLoginPage(conn);
    return;
    }
char *user = cgiUsualString("user", "");
char *recovEmail = cgiUsualString("recovEmail", "");
char *expStr = cgiUsualString("exp", "");
char *sig = cgiUsualString("sig", "");
/* Recompute the signature over the address and flag currently on the account.  Applying the
 * link changes both, so re-opening it no longer matches: the link works exactly once. */
char query[1024];
sqlSafef(query, sizeof(query),
    "SELECT recovEmail, recovEmailVerified FROM gbMembers WHERE userName='%s'", user);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row = sqlNextRow(sr);
char *curRecov = (row != NULL) ? cloneString(emptyForNull(row[0])) : NULL;
char *curVerified = (row != NULL) ? cloneString(emptyForNull(row[1])) : NULL;
sqlFreeResult(&sr);
char *expected = recovEmailSig(user, recovEmail, emptyForNull(curRecov),
                               emptyForNull(curVerified), expStr);
boolean sigOk = isNotEmpty(sig) && sameString(sig, expected);
freeMem(expected);
if (!sigOk || isEmpty(user) || spc_email_isvalid(recovEmail) == 0)
    {
    freez(&errMsg);
    errMsg = cloneString("This confirmation link is not valid or has already been used.");
    displayLoginPage(conn);
    return;
    }
if (clock1() > atol(expStr))
    {
    freez(&errMsg);
    errMsg = cloneString("This confirmation link has expired.");
    displayLoginPage(conn);
    return;
    }
/* Set the address as well as the flag: for a signup this rewrites the same value, and for a
 * change this is the point at which the new address takes effect. */
sqlSafef(query, sizeof(query),
    "UPDATE gbMembers SET recovEmail='%s', recovEmailVerified='Y', lastUse=NOW() "
    "WHERE userName='%s'", recovEmail, user);
sqlUpdate(conn, query);
/* A change, not a signup confirmation: tell the main address, so a hijack gets noticed. */
if (isNotEmpty(curRecov) && differentWord(curRecov, recovEmail))
    {
    sqlSafef(query, sizeof(query), "SELECT email FROM gbMembers WHERE userName='%s'", user);
    char *email = sqlQuickString(conn, query);
    if (isNotEmpty(email))
        sendRecovEmailChangeAlertMail(email, user, recovEmail);
    }
char *encEmail = htmlEncode(recovEmail);
hPrintf("<div class=\"centeredContainer formBox\"><h2>%s</h2>", brwName);
hPrintf("<h3>Your recovery email address has been confirmed.</h3>");
hPrintf("<p><b>%s</b> can now be used to sign in to your account and to recover your "
    "password.</p></div>", encEmail);
freeMem(encEmail);
returnToURL(1500);
}

void changeRecovEmailPage(struct sqlConnection *conn)
/* Draw the set/change-recovery-address page for the currently logged-in user.  As on the
 * change-email page the account comes from the validated login cookie, never from a form
 * field, and an account that has a password must supply it: a confirmed recovery address can
 * sign in to the account, so a borrowed login cookie alone must not be able to point it
 * somewhere new. */
{
if (!recovEmailChangeEnabled())
    {
    displayLoginPage(conn);
    return;
    }
char *user = wikiLinkUserName();
if (isEmpty(user))
    {
    freez(&errMsg);
    errMsg = cloneString("Please log in first to change your recovery email address.");
    displayLoginPage(conn);
    return;
    }
char query[512];
sqlSafef(query, sizeof(query),
    "SELECT recovEmail, recovEmailVerified FROM gbMembers WHERE userName='%s'", user);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row = sqlNextRow(sr);
char *curRecov = (row != NULL) ? cloneString(emptyForNull(row[0])) : cloneString("");
boolean curConfirmed = (row != NULL) && sameWord(emptyForNull(row[1]), "Y");
sqlFreeResult(&sr);
sqlSafef(query, sizeof(query), "SELECT password FROM gbMembers WHERE userName='%s'", user);
boolean hasPassword = isNotEmpty(sqlQuickString(conn, query));
char *encUser = htmlEncode(user);
char *encCurRecov = htmlEncode(isNotEmpty(curRecov) ? curRecov : "(none)");

hPrintf("<div id=\"changeRecovEmailBox\" class=\"centeredContainer formBox\">"
    "<h2>%s</h2>", brwName);
hPrintf("<h3>Recovery Email</h3>");
hPrintf("<p><span style='color:red;'>%s</span></p>", errMsg ? errMsg : "");
hPrintf("<form method=\"post\" action=\"%s\" name=\"changeRecovEmailForm\">", hgLoginUrl);
hPrintf("<p>Signed in as <b>%s</b>.<br>Current recovery email address: <b>%s</b>%s</p>",
    encUser, encCurRecov,
    (isNotEmpty(curRecov) && !curConfirmed) ? " (waiting to be confirmed)" : "");
hPrintf("<p style=\"font-size:0.9em\">A second address you can use to get back into your "
    "account: it can sign you in, including with the Google and ORCID buttons, and it receives "
    "a copy of a password reset. We email it a link to confirm it, and it does nothing until "
    "you open that link. Your current address keeps working until then.</p>");
freeMem(encUser);
freeMem(encCurRecov);
if (hasPassword)
    hPrintf("<div class=\"inputGroup\">"
        "<label for=\"curPassword\">Current password</label>"
        "<input type=\"password\" name=\"hgLogin_curPassword\" value=\"\" size=\"30\" "
        "id=\"curPassword\">"
        "</div>");
hPrintf("<div class=\"inputGroup\">"
    "<label for=\"newRecovEmail1\">New recovery email address</label>"
    "<input type=\"text\" name=\"hgLogin_newRecovEmail1\" value=\"\" size=\"30\" "
    "id=\"newRecovEmail1\">"
    "</div>");
hPrintf("<div class=\"inputGroup\">"
    "<label for=\"newRecovEmail2\">Re-enter new recovery email address</label>"
    "<input type=\"text\" name=\"hgLogin_newRecovEmail2\" value=\"\" size=\"30\" "
    "id=\"newRecovEmail2\">"
    "</div>");
hPrintf("<div class=\"formControls\">"
    "<input type=\"submit\" name=\"hgLogin.do.changeRecovEmail\" value=\"Change recovery email\" "
    "class=\"largeButton\">"
    " &nbsp;<a href=\"%s\" class=\"cancelButton\">Cancel</a>"
    "</div></form></div><!-- END - changeRecovEmailBox -->", getReturnToUrlForAttr());
cartSaveSession(cart);
}

void changeRecovEmail(struct sqlConnection *conn)
/* Process the set/change-recovery-address form for the currently logged-in user. */
{
if (!recovEmailChangeEnabled())
    {
    displayLoginPage(conn);
    return;
    }
char *user = wikiLinkUserName();
if (isEmpty(user))
    {
    freez(&errMsg);
    errMsg = cloneString("Please log in first to change your recovery email address.");
    displayLoginPage(conn);
    return;
    }
char *recov1 = cartUsualString(cart, "hgLogin_newRecovEmail1", "");
char *recov2 = cartUsualString(cart, "hgLogin_newRecovEmail2", "");
if (isEmpty(recov1) || spc_email_isvalid(recov1) == 0)
    {
    freez(&errMsg);
    errMsg = cloneString("Please enter a valid email address.");
    changeRecovEmailPage(conn);
    return;
    }
if (differentString(recov1, recov2))
    {
    freez(&errMsg);
    errMsg = cloneString("Email addresses do not match.");
    changeRecovEmailPage(conn);
    return;
    }
char query[512];
/* Re-authenticate where we can: if the account has a password, require the current one. */
sqlSafef(query, sizeof(query), "SELECT password FROM gbMembers WHERE userName='%s'", user);
char *curPwd = sqlQuickString(conn, query);
if (isNotEmpty(curPwd))
    {
    char *given = cartUsualString(cart, "hgLogin_curPassword", "");
    if (isEmpty(given) || !checkPwd(given, curPwd))
        {
        freez(&errMsg);
        errMsg = cloneString("Please enter your current password.");
        changeRecovEmailPage(conn);
        return;
        }
    }
sqlSafef(query, sizeof(query),
    "SELECT recovEmail, recovEmailVerified, email FROM gbMembers WHERE userName='%s'", user);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row = sqlNextRow(sr);
char *curRecov = (row != NULL) ? cloneString(emptyForNull(row[0])) : cloneString("");
char *curVerified = (row != NULL) ? cloneString(emptyForNull(row[1])) : cloneString("");
char *curEmail = (row != NULL) ? cloneString(emptyForNull(row[2])) : cloneString("");
sqlFreeResult(&sr);
/* An address the account already uses needs no second proof. */
if (sameWord(recov1, curEmail))
    {
    freez(&errMsg);
    errMsg = cloneString("That is already the main address on this account.");
    changeRecovEmailPage(conn);
    return;
    }
if (sameWord(recov1, curRecov) && sameWord(curVerified, "Y"))
    {
    freez(&errMsg);
    errMsg = cloneString("That is already your confirmed recovery email address.");
    changeRecovEmailPage(conn);
    return;
    }
/* Do not store the address yet: mail a one-time confirmation link and put it on the account
 * only when that link is opened (see confirmRecovEmail).  Until then the address the user has
 * now keeps working, so a typo here costs them nothing. */
sendRecovEmailConfirmMail(recov1, user, curRecov, curVerified);
cartRemove(cart, "hgLogin_newRecovEmail1");
cartRemove(cart, "hgLogin_newRecovEmail2");
cartRemove(cart, "hgLogin_curPassword");
char *encRecov = htmlEncode(recov1);
hPrintf("<div class=\"centeredContainer formBox\"><h2>%s</h2>", brwName);
hPrintf("<h3>Almost done. Please check your email</h3>");
hPrintf("<p>We sent a confirmation link to <b>%s</b>. Open the link in that message to finish "
    "setting your recovery email address. The link works once and expires in seven days. Until "
    "then nothing about your account changes.</p></div>", encRecov);
freeMem(encRecov);
returnToURL(3000);
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
    "<label for=\"reenterEmail\">Re-enter email address</label>"
    "<input type=text name=\"hgLogin_email2\" value=\"%s\" size=\"30\" id=\"emailCheck\">"
    "</div>\n",
    htmlEncode(cartUsualString(cart, "hgLogin_userName", "")),   // all three go into value="" attributes; escape (XSS)
    htmlEncode(cartUsualString(cart, "hgLogin_email", "")),
    htmlEncode(cartUsualString(cart, "hgLogin_email2", "")));

if (sqlFieldIndex(conn, "gbMembers", "recovEmail") != -1)
    hPrintf("<div class=\"inputGroup\">"
        "<label for=\"recovEmail\">Optional secondary recovery email</label>"
        "<input type=text name=\"hgLogin_recovEmail\" size=\"30\" id=\"recovEmail\">"
        "<p style=\"font-size:0.9em\">We will email this address a link to confirm it. Until "
        "you open that link, the address cannot be used to sign in or to recover your "
        "password.</p>"
        "</div>"
        "\n");

hPrintf("<div class=\"inputGroup\">"
    "<label for=\"password\">Password <small>(must be at least 5 characters)</small></label>"
    "<span style=\"display:inline-flex; align-items:center;\">"
    "<input type=password name=\"hgLogin_password\" value=\"%s\" size=\"30\" id=\"password\">",
    htmlEncode(cartUsualString(cart, "hgLogin_password", "")));  // value="" attribute; escape (XSS)
printPwdEyeIcon("signupPwEyeIcon", "signupPwEyeSlash");
hPrintf(
    "</span>"
    "</div>"
    "\n"
    "<div class=\"inputGroup\">"
    "<label for=\"passwordCheck\">Re-enter password</label>"
    "<span style=\"display:inline-flex; align-items:center;\">"
    "<input type=password name=\"hgLogin_password2\" value=\"%s\" size=\"30\" id=\"passwordCheck\">",
    htmlEncode(cartUsualString(cart, "hgLogin_password2", "")));  // value="" attribute; escape (XSS)
printPwdEyeIcon("signupPwCheckEyeIcon", "signupPwCheckEyeSlash");
hPrintf(
    "</span>"
    "\n"
    "</div>"
    "\n"
    "<div class=\"formControls\">"
    "    <input type=\"submit\" name=\"hgLogin.do.signup\" value=\"Sign up using email\" class=\"largeButton\"> &nbsp; "
    "    <a href=\"%s\" class=\"cancelButton\">Cancel</a>"
    "</div>"
    "</form>"
    "</div><!-- END - signUpBox -->",
    getReturnToUrlForAttr());
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
/* A recovery address is confirmed by mail before it counts for anything (see
 * sendRecovEmailConfirmMail).  Two kinds of install cannot confirm anything: one that sends no
 * mail at all, and one with no login.cookieSalt to sign the link with (plain login does not
 * need the salt, so an install can run happily without one).  There, leave the address as
 * usable as it is today rather than storing one that could never be confirmed. */
boolean confirmRecov = !isEmpty(recovEmail) && recovEmailVerifyOk
                        && !sameWord(returnAddr, "NOEMAIL")
                        && isNotEmpty(cfgOption(CFG_LOGIN_COOKIE_SALT));
// set the recov email only if we got one (and we only got one if the table has this field)
if (!isEmpty(recovEmail))
    sqlDyStringPrintf(query2, ",recovEmail='%s'", recovEmail);
if (confirmRecov)
    sqlDyStringPrintf(query2, ",recovEmailVerified='N'");

sqlUpdate(conn, dyStringContents(query2));
dyStringFree(&query2);

if (sameWord(returnAddr, "NOEMAIL"))
    {
    redirectToLoginPage("hgLogin.do.displayLoginPage=1");
    return;
    }

setupNewAccount(conn, email, user);
if (confirmRecov)
    sendRecovEmailConfirmMail(recovEmail, user, recovEmail, "N");
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
char query[1024];   // room for an address-matching clause holding a long address twice
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
        char *addrMatch = sqlAddressMatch(email);
        sqlSafef(query,sizeof(query),
            "SELECT password FROM gbMembers WHERE %-s", addrMatch);
        freeMem(addrMatch);
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

static boolean recovEmailChangeEnabled()
/* Return TRUE if users may set or change their own recovery email address.  Needs working
 * outbound mail to confirm the new address, a login.cookieSalt to sign the confirmation link,
 * and the recovEmailVerified column to record the answer in, so all three are required on top
 * of the admin turning it on with login.recovEmailChange=on in hg.conf. */
{
if (!cfgOptionBooleanDefault(CFG_LOGIN_RECOV_EMAIL_CHANGE, FALSE))
    return FALSE;
if (!recovEmailVerifyOk || isEmpty(cfgOption(CFG_LOGIN_COOKIE_SALT)))
    return FALSE;
return !sameWord(returnAddr, "NOEMAIL");
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

static void loginAndReturn(struct sqlConnection *conn, char *userName, uint idx)
/* Set the permanent login cookies for userName and bounce back to the returnto URL.
 * Every social/email-link login funnels through here, so they all produce the same
 * long-lived cookies and all record the sign-in in gbMembers.lastUse.  (The password
 * path uses displayLoginSuccess and stamps lastUse via clearNewPasswordFields.) */
{
char query[256];
sqlSafef(query, sizeof(query), "UPDATE gbMembers SET lastUse=NOW() WHERE idx=%u", idx);
sqlUpdate(conn, query);
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
    if (isalnum((unsigned char)*s) || *s == '_' || *s == '-')
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

/* A pending social identity is good for this many seconds -- long enough to choose a username
 * or an account, short enough that a leaked signature is quickly useless. */
#define OAUTH_PENDING_TTL 900

static char *oauthPendingSig(char *provider, char *subject, char *email, char *emailVerified,
                             char *timeStr)
/* HMAC-MD5 over a pending social identity, keyed by the secret login.cookieSalt.  Only
 * resolveIdentity (which runs after a genuine provider verification) can produce a valid one,
 * so a pending identity injected through cart/CGI variables will not validate.  The signature
 * also covers this browser's hguid (cart->userId, which -- unlike the hgsid -- survives the
 * provider redirect) and the time it was minted, so a signature that leaks into a saved or
 * shared session cannot be replayed by a different browser or after it expires (see
 * pendingIdentityValid).  Result is allocd. */
{
char *salt = cfgOption(CFG_LOGIN_COOKIE_SALT);
if (isEmpty(salt))
    errAbort("Signing in with an external identity provider requires %s in hg.conf, set to a "
        "secret random string.  Without a secret we cannot sign the pending identity, and the "
        "account chooser would accept a forged one.", CFG_LOGIN_COOKIE_SALT);
char buf[1024];
safef(buf, sizeof(buf), "%s|%s|%s|%s|%s|%s",
    emptyForNull(provider), emptyForNull(subject), emptyForNull(email),
    emptyForNull(emailVerified), emptyForNull(cart->userId), emptyForNull(timeStr));
return hmacMd5(salt, buf);
}

static boolean pendingIdentityValid()
/* TRUE only if the pending-identity cart variables carry a signature we minted, for this
 * browser, within the last OAUTH_PENDING_TTL seconds.  Guards the OAuth chooser and
 * completeAccount against forged, injected, replayed, or stale pending identities. */
{
char *sig = cartUsualString(cart, "oauth_pending_sig", "");
char *timeStr = cartUsualString(cart, "oauth_pending_time", "");
if (isEmpty(sig) || isEmpty(timeStr))
    return FALSE;
if (clock1() - atol(timeStr) > OAUTH_PENDING_TTL)
    return FALSE;
char *expected = oauthPendingSig(cartUsualString(cart, "oauth_pending_provider", ""),
                                 cartUsualString(cart, "oauth_pending_subject", ""),
                                 cartUsualString(cart, "oauth_pending_email", ""),
                                 cartUsualString(cart, "oauth_pending_email_verified", ""),
                                 timeStr);
boolean ok = sameString(sig, expected);
freeMem(expected);
return ok;
}

static void setPendingIdentity(struct oauthIdentity *id)
/* Stash an authenticated-but-not-yet-linked identity in the cart so it survives a form
 * round-trip (the "choose a username" or "choose an account" page).  The signature is what
 * proves, on the way back, that we really verified this identity, for this browser, recently. */
{
char timeStr[32];
safef(timeStr, sizeof(timeStr), "%ld", clock1());
cartSetString(cart, "oauth_pending_provider", id->provider);
cartSetString(cart, "oauth_pending_subject", id->subject);
cartSetString(cart, "oauth_pending_email", emptyForNull(id->email));
char *emailVerified = id->emailVerified ? "1" : "0";
cartSetString(cart, "oauth_pending_email_verified", emailVerified);
cartSetString(cart, "oauth_pending_name", emptyForNull(id->displayName));
cartSetString(cart, "oauth_pending_time", timeStr);
cartSetString(cart, "oauth_pending_sig",
    oauthPendingSig(id->provider, id->subject, emptyForNull(id->email), emailVerified, timeStr));
}

static void clearPendingIdentity()
/* Remove the pending-identity cart variables.  Call this on every path that finishes with the
 * pending identity -- success or definitive failure -- so a stale signature is not left behind
 * in the cart to be swept into a saved session. */
{
cartRemove(cart, "oauth_pending_provider");
cartRemove(cart, "oauth_pending_subject");
cartRemove(cart, "oauth_pending_email");
cartRemove(cart, "oauth_pending_email_verified");
cartRemove(cart, "oauth_pending_name");
cartRemove(cart, "oauth_pending_time");
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
    clearPendingIdentity();
    displayLoginPage(conn);
    return;
    }
char *suggested = cartUsualString(cart, "hgLogin_userName", "");
if (isEmpty(suggested))
    suggested = suggestUsername(conn, email, name);
char *encSuggested = htmlEncode(suggested);   // both go into value="" attributes; escape (XSS)
char *encEmail = htmlEncode(email);

hPrintf("<div id=\"completeAccountBox\" class=\"centeredContainer formBox\">"
    "<h2>%s</h2>", brwName);
hPrintf("<h3>Choose a username</h3>");
if (sameWord(provider, "orcid"))
    hPrintf("<p>A new genome browser account is created for any ORCID sign-in not seen before, "
        "because ORCID only shares an ORCID iD, not an email address. So you cannot sign into an "
        "existing account using ORCID. Use another sign-in option instead if you don't want to "
        "create a new account.</p>"
        "<p>Pick a username for your %s account. You can change the suggested name below.</p>",
        brwName);
else if (sameWord(provider, "cilogon"))
    hPrintf("<p>A new genome browser account is created for any CILogon sign-in not seen before. "
        "So you cannot sign into an existing account using CILogon this way. Use another sign-in "
        "option instead if you don't want to create a new account.</p>"
        "<p>Pick a username for your %s account. You can change the suggested name below.</p>",
        brwName);
else
    hPrintf("<p>You signed in with %s. Pick a username for your new %s account. "
        "You can change the suggested name below.</p>",
        oauthProviderLabel(provider), brwName);
printUsernameNote();
hPrintf("<span style='color:red;'>%s</span>", errMsg ? errMsg : "");
hPrintf("<form method=\"post\" action=\"%s\" name=\"completeAccountForm\">", hgLoginUrl);
hPrintf("<div class=\"inputGroup\">"
    "<label for=\"userName\">Username</label>"
    "<input type=\"text\" name=\"hgLogin_userName\" value=\"%s\" size=\"30\" id=\"userName\">"
    "</div>", encSuggested);
hPrintf("<div class=\"inputGroup\">"
    "<label for=\"emailAddr\">Email address</label>"
    "<input type=\"text\" name=\"hgLogin_email\" value=\"%s\" size=\"30\" id=\"emailAddr\">"
    "</div>", encEmail);
hPrintf("<div class=\"formControls\">"
    "<input type=\"submit\" name=\"hgLogin.do.completeAccount\" value=\"Create account\" class=\"largeButton\">"
    " &nbsp;<a href=\"%s\" class=\"cancelButton\">Cancel</a>"
    "</div></form></div><!-- END - completeAccountBox -->", getReturnToUrlForAttr());
cartSaveSession(cart);
freeMem(encSuggested);
freeMem(encEmail);
}

void completeAccount(struct sqlConnection *conn)
/* Create the account for a first-time social-login user, link the identity, and log in. */
{
char *provider = cartUsualString(cart, "oauth_pending_provider", "");
char *subject = cartUsualString(cart, "oauth_pending_subject", "");
if (isEmpty(provider) || isEmpty(subject) || !pendingIdentityValid())
    {
    clearPendingIdentity();
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

/* The new account is created "activated" -- its email trusted for future auto-linking (see
 * resolveIdentity) -- only when the provider actually verified this address and the user kept
 * it unchanged.  If the address is unverified (the provider released none, e.g. ORCID, or the
 * user typed a different one), create the account inactive and send the usual confirmation
 * mail, so an unverified address can never be planted as a trusted one.  The user still signs
 * in now either way: their provider identity, not the email, is what logs them in. */
char *verifiedEmail = cartUsualString(cart, "oauth_pending_email", "");
boolean emailVerified = cartUsualBoolean(cart, "oauth_pending_email_verified", FALSE)
                        && isNotEmpty(verifiedEmail) && sameString(email, verifiedEmail);

struct dyString *q = sqlDyStringCreate(
    "INSERT INTO gbMembers SET userName='%s', realName='%s', password='', email='%s', "
    "lastUse=NOW(), dateActivated=NOW(), accountActivated='%s'",
    user, realName, emptyForNull(email), emailVerified ? "Y" : "N");
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
if (!emailVerified)
    setupNewAccount(conn, email, user);   // send confirmation mail for the unverified address
loginAndReturn(conn, user, idx);
}

void chooseAccountPage(struct sqlConnection *conn)
/* Ask the user which of several accounts sharing an email address to sign in to.  Used by
 * two flows: OAuth (oauth_pending_* in the cart -> the chosen account is linked to the social
 * identity) and the passwordless email link (emailLogin_* in the cart -> just sign in). */
{
char *provider = cartUsualString(cart, "oauth_pending_provider", "");
boolean emailMode = isEmpty(provider);
if (emailMode && !emailLinkEnabled())
    {
    // The email-link chooser must not run where passwordless login is switched off.
    displayLoginPage(conn);
    return;
    }
char *email = emailMode ? cartUsualString(cart, "emailLogin_email", "")
                        : cartUsualString(cart, "oauth_pending_email", "");
if (isEmpty(email) || (!emailMode && !pendingIdentityValid()))
    {
    if (!emailMode)
        clearPendingIdentity();
    displayLoginPage(conn);
    return;
    }
char *encEmail = htmlEncode(email);   // the address is displayed; never trust it raw (XSS)
char query[1024];
char *addrMatch = sqlAddressMatch(email);   // email is non-empty here (checked above)
if (emailMode)
    // Only the accounts that hold the just-validated login token, matching what emailLogin saw.
    sqlSafef(query, sizeof(query),
        "SELECT * FROM gbMembers WHERE %-s AND loginToken='%s' "
        "AND loginToken<>'' AND loginTokenExpires > NOW() AND accountActivated='Y' ORDER BY idx",
        addrMatch, cartUsualString(cart, "emailLogin_tokenMd5", ""));
else
    // Only activated accounts, matching what chooseAccount() and resolveIdentity() accept;
    // otherwise the page offers a row the action refuses, and shows the username of an
    // unactivated row anyone could have created with this address.
    sqlSafef(query, sizeof(query),
        "SELECT * FROM gbMembers WHERE %-s AND accountActivated='Y' ORDER BY idx", addrMatch);
freeMem(addrMatch);
struct gbMembers *list = gbMembersLoadByQuery(conn, query), *m;

hPrintf("<div id=\"chooseAccountBox\" class=\"centeredContainer formBox\">"
    "<h2>%s</h2>", brwName);
hPrintf("<h3>Choose an account</h3>");
if (emailMode)
    hPrintf("<p>The email address <b>%s</b> is associated with more than one %s account. "
        "Select the account you would like to sign in to.</p>", encEmail, brwName);
else
    hPrintf("<p>The email address <b>%s</b> is associated with more than one %s account. "
        "Select the account you would like to sign in to; your %s login will be linked to it.</p>",
        encEmail, brwName, oauthProviderLabel(provider));
hPrintf("<span style='color:red;'>%s</span>", errMsg ? errMsg : "");
hPrintf("<form method=\"post\" action=\"%s\" name=\"chooseAccountForm\">", hgLoginUrl);
hPrintf("<div class=\"inputGroup\">");
boolean first = TRUE;
for (m = list;  m != NULL;  m = m->next)
    {
    char *encUserName = htmlEncode(m->userName);
    hPrintf("<div class=\"acctHelpSection\">"
        "<input name=\"hgLogin_chosenIdx\" type=\"radio\" value=\"%u\" id=\"acct_%u\"%s>"
        "<label for=\"acct_%u\" class=\"radioLabel\">%s</label></div>",
        m->idx, m->idx, first ? " checked" : "", m->idx, encUserName);
    freeMem(encUserName);
    first = FALSE;
    }
hPrintf("</div>");
hPrintf("<div class=\"formControls\">"
    "<input type=\"submit\" name=\"hgLogin.do.chooseAccount\" value=\"Sign in\" class=\"largeButton\">"
    " &nbsp;<a href=\"%s\" class=\"cancelButton\">Cancel</a>"
    "</div></form></div><!-- END - chooseAccountBox -->", getReturnToUrlForAttr());
cartSaveSession(cart);
freeMem(encEmail);
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
char query[1024];   // room for an address-matching clause holding a long address twice

if (isEmpty(provider))
    {
    /* Passwordless email-link mode. */
    if (!emailLinkEnabled())
        {
        displayLoginPage(conn);
        return;
        }
    char *email = cartUsualString(cart, "emailLogin_email", "");
    char *tokenMd5 = cartUsualString(cart, "emailLogin_tokenMd5", "");
    if (isEmpty(email) || isEmpty(tokenMd5))
        {
        freez(&errMsg);
        errMsg = cloneString("Your login link expired. Please request a new one.");
        displayLoginPage(conn);
        return;
        }
    char *addrMatch = sqlAddressMatch(email);
    sqlSafef(query, sizeof(query),
        "SELECT * FROM gbMembers WHERE idx=%d AND %-s "
        "AND loginToken='%s' AND loginToken<>'' AND loginTokenExpires > NOW() "
        "AND accountActivated='Y'",
        chosenIdx, addrMatch, tokenMd5);
    struct gbMembers *m = gbMembersLoadByQuery(conn, query);
    if (m == NULL)
        {
        freez(&errMsg);
        errMsg = cloneString("Please choose one of the listed accounts.");
        chooseAccountPage(conn);
        return;
        }
    /* Consume the token on every account that shared it (single use), then sign in.
     * loginAndReturn records the sign-in on the chosen account in gbMembers.lastUse. */
    sqlSafef(query, sizeof(query),
        "UPDATE gbMembers SET loginToken='' WHERE %-s AND loginToken='%s'",
        addrMatch, tokenMd5);
    freeMem(addrMatch);
    sqlUpdate(conn, query);
    cartRemove(cart, "emailLogin_email");
    cartRemove(cart, "emailLogin_tokenMd5");
    cartRemove(cart, "hgLogin_chosenIdx");
    loginAndReturn(conn, m->userName, m->idx);
    gbMembersFree(&m);
    return;
    }

/* OAuth mode. */
char *subject = cartUsualString(cart, "oauth_pending_subject", "");
char *email = cartUsualString(cart, "oauth_pending_email", "");
if (isEmpty(subject) || isEmpty(email) || !pendingIdentityValid())
    {
    clearPendingIdentity();
    freez(&errMsg);
    errMsg = cloneString("Your login session expired. Please sign in again.");
    displayLoginPage(conn);
    return;
    }
/* Only an activated account counts: an unactivated row can hold any address someone typed
 * without ever proving they own it (see resolveIdentity), so it must not receive a social link.
 * Match what chooseAccountPage() offers; email is non-empty here (checked above). */
char *oauthAddrMatch = sqlAddressMatch(email);
sqlSafef(query, sizeof(query),
    "SELECT * FROM gbMembers WHERE idx=%d AND %-s AND accountActivated='Y'",
    chosenIdx, oauthAddrMatch);
freeMem(oauthAddrMatch);
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
loginAndReturn(conn, m->userName, m->idx);
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
    char query[1024];
    /* Match the provider email against the primary address and any confirmed recovery address,
     * the same as password and email-link login do (see sqlAddressMatch).  The isNotEmpty()
     * guard above keeps an empty id->email out of the query, so a blank recovEmail='' row can
     * never match.
     * Match only activated accounts.  gbMembers has no unique key on email, and the plain
     * signup form will create an unactivated row for any address a person types -- the
     * activation mail goes to the address's real owner, who ignores it.  Without this filter
     * someone could pre-register a victim's address, and the victim's first social login would
     * then auto-link to (and sign in as) the attacker's account. */
    char *addrMatch = sqlAddressMatch(id->email);
    sqlSafef(query, sizeof(query),
        "SELECT * FROM gbMembers WHERE %-s AND accountActivated='Y' ORDER BY idx", addrMatch);
    freeMem(addrMatch);
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
    loginAndReturn(conn, linked->userName, linked->idx);
    gbMembersFree(&linked);
    gbMembersFreeList(&matches);
    return;
    }

if (n == 1)
    {
    linkIdentity(conn, matches->idx, id);
    loginAndReturn(conn, matches->userName, matches->idx);
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
// clone this: cartRemove below frees the cart's copy, but we still use provider afterward
char *provider = cloneString(cartUsualString(cart, "oauth_provider", ""));

// Validate the anti-CSRF state before consuming any cart state or acting on an error param.  A
// stray code/error link (a re-opened redirect, or a crafted hgLogin?error=...) must not be able to
// consume the state nonce of a login in flight, so check first and only then clear the flow.  A
// compliant provider echoes state on an error return too (RFC 6749 4.1.2.1), and we always send it.
if (isEmpty(state) || isEmpty(savedState) || differentString(state, savedState))
    {
    freez(&errMsg);
    errMsg = cloneString("Your login session expired or was invalid. Please try again.");
    displayLoginPage(conn);
    return;
    }
cartRemove(cart, "oauth_state");      // one-time use
cartRemove(cart, "oauth_provider");   // end the flow so a later code/error param can't re-enter

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
char *encEmail = htmlEncode(cartUsualString(cart, "hgLogin_email", ""));
hPrintf("<div class=\"inputGroup\">"
    "<label for=\"emailLink\">Email address</label>"
    "<input type=\"text\" name=\"hgLogin_email\" value=\"%s\" size=\"30\" id=\"emailLink\">"
    "</div>", encEmail);
freeMem(encEmail);
hPrintf("<div class=\"formControls\">"
    "<input type=\"submit\" name=\"hgLogin.do.sendEmailLink\" value=\"Send login link\" class=\"largeButton\">"
    " &nbsp;<a href=\"%s\" class=\"cancelButton\">Cancel</a>"
    "</div></form></div><!-- END - emailLinkBox -->", getReturnToUrlForAttr());
cartSaveSession(cart);
}

void displayLoginLinkSuccess()
/* Confirmation shown after a passwordless login link is (possibly) emailed.  Phrased so it
 * does not reveal whether an account exists for the address. */
{
char *email = htmlEncode(cartUsualString(cart, "hgLogin_sendMailTo", ""));
hPrintf("<div id=\"confirmationBox\" class=\"centeredContainer formBox\">"
    "<h2>%s</h2>", brwName);
hPrintf("<p id=\"confirmationMsg\" class=\"confirmationTxt\">If an account exists for "
    "<B>%s</B>, a login link has been sent to that address.<BR><BR>"
    "Click the link in that email to sign in. No password needed. "
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
    "It works once and expires in one hour:\n\n%s\n\n%s\n%s",
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
char query[1024];
char *addrMatch = sqlAddressMatch(email);
sqlSafef(query, sizeof(query),
    "SELECT * FROM gbMembers WHERE %-s AND accountActivated='Y'", addrMatch);
freeMem(addrMatch);
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
char query[1024];
char *addrMatch = sqlAddressMatch(email);
sqlSafef(query, sizeof(query),
    "SELECT * FROM gbMembers WHERE %-s AND loginToken='%s' "
    "AND loginToken<>'' AND loginTokenExpires > NOW() AND accountActivated='Y' ORDER BY idx",
    addrMatch, tokenMD5);
freeMem(addrMatch);
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
        "UPDATE gbMembers SET loginToken='' WHERE idx=%u", list->idx);
    sqlUpdate(conn, query);
    loginAndReturn(conn, list->userName, list->idx);
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

static void dropRequestSuppliedFlowVars()
/* The cart variables holding the state of a login in flight are written by hgLogin and by
 * nothing else: the nonce and provider of an OAuth round trip, the pending identity behind
 * the account chooser, and the verified address behind the email-link chooser.  The cart
 * takes CGI variables verbatim (loadCgiOverHash in hg/lib/cart.c), so a copy arriving with
 * the request would stand in for the copy we stored.  Drop those before anything reads them;
 * a flow whose state is dropped fails closed and the user starts it again.  Note that
 * excludeVars would not do this job: it governs what is saved at the end of a request, not
 * what is read during it. */
{
static char *serverOwned[] = {
    "oauth_state", "oauth_provider",
    "oauth_pending_provider", "oauth_pending_subject", "oauth_pending_email",
    "oauth_pending_email_verified", "oauth_pending_name", "oauth_pending_time",
    "oauth_pending_sig",
    "emailLogin_email", "emailLogin_tokenMd5",
    };
int i;
for (i = 0;  i < ArraySize(serverOwned);  i++)
    if (cgiVarExists(serverOwned[i]))
        cartRemove(cart, serverOwned[i]);
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

/* Tells a confirmed recovery address from one that was only typed into the signup form.
 * Existing rows default to 'Y': every recovery address that predates this column keeps working
 * exactly as before, so no one has to re-confirm an address they set up long ago.  Only
 * addresses entered from now on have to be confirmed (see sendRecovEmailConfirmMail). */
if (sqlFieldIndex(conn, "gbMembers", "recovEmailVerified") == -1)
    autoUpgradeTableAddColumn(conn, "gbMembers", "recovEmailVerified", "varchar(1)", FALSE, "'Y'");
recovEmailVerifyOk = (sqlFieldIndex(conn, "gbMembers", "recovEmailVerified") != -1);

// columns for the passwordless email-link login feature
if (sqlFieldIndex(conn, "gbMembers", "loginToken") == -1)
    autoUpgradeTableAddColumn(conn, "gbMembers", "loginToken", "varchar(255)", FALSE, "NULL");
if (sqlFieldIndex(conn, "gbMembers", "loginTokenExpires") == -1)
    autoUpgradeTableAddColumn(conn, "gbMembers", "loginTokenExpires", "DATETIME", FALSE, "NULL");

// table linking accounts to social (Google/ORCID) identities; only needed where OAuth is set up
if (oauthAnyProviderEnabled())
    createIdentityTable(conn);

cart = theCart;
dropRequestSuppliedFlowVars();
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
else if (cartVarExists(cart, "hgLogin.do.confirmChangeEmail"))
    confirmChangeEmail(conn);
else if (cartVarExists(cart, "hgLogin.do.confirmRecovEmail"))
    confirmRecovEmail(conn);
else if (cartVarExists(cart, "hgLogin.do.changeRecovEmailPage"))
    changeRecovEmailPage(conn);
else if (cartVarExists(cart, "hgLogin.do.changeRecovEmail"))
    changeRecovEmail(conn);
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
