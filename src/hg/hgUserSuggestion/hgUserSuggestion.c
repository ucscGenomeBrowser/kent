/* hgUserSuggestion - CGI-script to collect user's suggestion. */

#include "common.h"
#include "errabort.h"
#include "hCommon.h"
#include "jksql.h"
#include "portable.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hdb.h"
#include "hui.h"
#include "cart.h"
#include "hPrint.h"
#include "dbDb.h"
#include "web.h"
#include "hash.h"
#include "hgConfig.h"
#include "hgUserSuggestion.h"
#include "mailViaPipe.h"

/* ---- Global variables. ---- */
struct cart *cart;	/* The user's ui state. */
struct hash *oldVars = NULL;

/* ---- Global helper functions ---- */
void checkHgConfForSuggestion()
/* Abort if hg.conf has not been set up to accept suggestion */
{
if (isEmpty(cfgOption(CFG_SUGGEST_MAILTOADDR)) ||
    isEmpty(cfgOption(CFG_SUGGEST_MAILFROMADDR)) ||
    isEmpty(cfgOption(CFG_FILTERKEYWORD))        ||
    isEmpty(cfgOption(CFG_SUGGEST_MAIL_SIGNATURE)) ||
    isEmpty(cfgOption(CFG_SUGGEST_MAIL_RETURN_ADDR)) ||
    isEmpty(cfgOption(CFG_SUGGEST_BROWSER_NAME)))
    errAbort("This Genome Browser has not been configured to accept suggestions yet. Please contact the browser administrator for more information.");
}

char *mailToAddr()
/* Return the address to send suggestion to  */
{
return cloneString(cfgOption(CFG_SUGGEST_MAILTOADDR));
}

char *mailFromAddr()
/* Return the bogus sender address to help filter out spam */
{
return cloneString(cfgOption(CFG_SUGGEST_MAILFROMADDR));
}

char *filterKeyword()
/* Return the keyword used to filter out spam  */
{
return cloneString(cfgOption(CFG_FILTERKEYWORD));
}

char *mailSignature()
/* Return the signature to be used by outbound mail. */
{
return cloneString(cfgOption(CFG_SUGGEST_MAIL_SIGNATURE));
}

char *mailReturnAddr()
/* Return the return addr. to be used by outbound mail. */
{
return cloneString(cfgOption(CFG_SUGGEST_MAIL_RETURN_ADDR));
}

char *browserName()
/* Return the browser name like 'UCSC Genome Browser' */
{
return cloneString(cfgOption(CFG_SUGGEST_BROWSER_NAME));
}

static char *now()
/* Return a mysql-formatted time like "2008-05-19 15:33:34". */
{
char nowBuf[512];
time_t curtime;
curtime = time (NULL); 
struct tm *theTime = localtime(&curtime);
strftime(nowBuf, sizeof nowBuf, "%Y-%m-%d %H:%M:%S", theTime);
return cloneString(nowBuf);
}

int spc_email_isvalid(const char *address) {
/* Check the format of an email address syntactically. Return 1 if
 * valid, else 0 */
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


/* javascript functions */
void printMainForm()
/* Create the main suggestion form */
{
hPrintf(
    "     <FORM ACTION=\"../cgi-bin/hgUserSuggestion?do.suggestSendMail=1\" METHOD=\"POST\" ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\" onLoad=\"document.forms.mainForm.name.focus()\">\n");
hPrintf(
    "<H2>User Suggestion Form</H2>\n"
    "<P>If you have ideas about how we can improve the value of the Genome Browser to your research, "
    "we'd like to hear from you. Please provide a concise description below. "
    "A copy of the suggestion will be sent to your email address along with a reference number. "
    "You may follow up on the status of your request at any time by <a href=\"../contacts.html#followup\">contacting us</a> and quoting the reference number.</P>");
hPrintf("<P>Please note: this form is not the proper place to submit questions regarding browser use or bug reports. Use the links on our contact page instead.</P>");
hPrintf("<HR><BR>"); 
hPrintf(
    "      <div id=\"suggest\">  \n"
    "       <label for=\"name\">Your Name:</label><input type=\"text\" name=\"suggestName\" id=\"name\" size=\"50\"style=\"margin-left:20px\" maxlength=\"256\"/><BR><BR>\n"
    "       <label for=\"email\">Your Email:</label><input type=\"text\" name=\"suggestEmail\" id=\"email\" size=\"50\" style=\"margin-left:70px\" maxlength=\"254\"/><BR><BR>\n"
    "       <label for=\"confirmEmail\">Re-enter Your Email:</label><input type=\"text\" \n"
    "          name=\"suggestCfmEmail\" id=\"cfmemail\" size=\"50\" style=\"margin-left:20px\" maxlength=\"254\"/><BR><BR>\n");
hPrintf(
    "       <label for=\"category\">Category:</label><select name=\"suggestCategory\" id=\"category\" style=\"margin-left:20px\" maxlength=\"256\">\n"
    "         <option selected>Tracks</option> \n"
    "         <option>Genome Assemblies</option>\n"
    "         <option>Browser Tools</option>\n"
    "         <option>Command-line Utilities</option>\n"
    "         <option>Others</option>\n"
    "         </select><BR><BR>\n");
hPrintf(
    "       <label for=\"summary\">Summary:</label><input type=\"text\" name=\"suggestSummary\" id=\"summary\" size=\"74\" style=\"margin-left:20px\" maxlength=\"256\"/><BR><BR>\n"
    "       <label for=\"details\">Details:</label><BR><textarea name=\"suggestDetails\" id=\"details\" cols=\"100\" rows=\"15\" maxlength=\"4096\"></textarea><BR><BR>\n"
    "     </div>\n");
hPrintf(
    "         <p>\n"
    "           <label for=\"code\">Enter the following value below: <span id=\"txtCaptchaDiv\" style=\"color:#F00\"></span><BR> \n"
    "           <input type=\"hidden\" id=\"txtCaptcha\" /></label>\n"
    "           <input type=\"text\" name=\"txtInput\" id=\"txtInput\" size=\"30\" />\n"
    "         </p>\n");
hPrintf(
    "      <div class=\"formControls\">\n"
    "        <input id=\"sendButton\" type=\"button\" value=\"Send\" onclick=\"submitform()\"/> \n"
    "        <input type=\"reset\" name=\"suggestClear\" value=\"Clear\" class=\"largeButton\"> \n"
    "      </div>\n"
    "      \n"
    "     </FORM>\n\n");
}
void printValidateScript()
/* javascript to validate form inputs */
{
hPrintf(  
    "    <script type=\"text/javascript\">\n"
    "    function validateMainForm(theform)\n"
    "    {\n"
    "    var x=theform.suggestName.value;\n"
    "    if (x==null || x==\"\")\n"
    "      {\n"
    "      alert(\"Name field must be filled out\");\n"
    "      theform.suggestName.focus() ;\n"
    "      return false;\n"
    "      }\n");
hPrintf(
    "    if (!validateMailAddr(theform.suggestEmail.value))\n"
    "      {\n"
    "      alert(\"Not a valid e-mail address\");\n"
    "      theform.suggestEmail.focus() ;\n"
    "      return false;\n"
    "      }\n"
    "    var str1 = theform.suggestEmail.value;\n"
    "    var str2 = theform.suggestCfmEmail.value;\n"
    "    if (str2==null || str2==\"\")\n"
    "      {\n"
    "      alert(\"Please re-enter your email address.\");\n"
    "      theform.suggestCfmEmail.focus();\n"
    "      return false;\n"
    "      }\n"
    "    if (str1 != str2)\n"
    "      {\n"
    "      alert(\"Email addresses do not match, please re-enter.\");\n"
    "      theform.suggestCfmEmail.focus();\n"
    "      return false;\n"
    "      }\n");
hPrintf(
    "    var y=theform.suggestSummary.value;\n"
    "    if (y==null || y==\"\")\n"
    "      {\n"
    "      alert(\"Summary field must be filled out\");\n"
    "      theform.suggestSummary.focus() ;\n"
    "      return false;\n"
    "      }          \n"
    "    return true; \n"
    "    }");
hPrintf(
    "    function validateMailAddr(x)\n"
    "    {\n"
    "    var atpos=x.indexOf(\"@\");\n"
    "    var dotpos=x.lastIndexOf(\".\");\n"
    "    if (atpos<1 || dotpos<atpos+2 || dotpos+2>=x.length)\n"
    "      {\n"
    "      return false;\n"
    "      } \n"
    "    return true;\n"
    "    }\n"
    "    </script><br />\n\n");
}

void printCheckCaptchaScript()
/* javascript to check CAPTCHA code */
{
hPrintf( 
    " <script type=\"text/javascript\">\n"
    "         function checkCaptcha(theform){\n"
    "                 var why = \"\";\n"
    "                  \n"
    "                 if(theform.txtInput.value == \"\"){\n"
    "                         why += \"- Security code should not be empty.\";\n"
    "                 }\n"
    "                 if(theform.txtInput.value != \"\"){\n"
    "                         if(ValidCaptcha(theform.txtInput.value) == false){\n"
    "                                 why += \"- Security code did not match.\";\n"
    "                         }\n"
    "                 }\n"
    "                 if(why != \"\"){\n"
    "                         alert(why);\n"
    "                         theform.txtInput.focus() ;\n"
    "                         return false;\n"
    "                 }\n"
    "            return true;\n"
    "         }\n\n");
hPrintf(
    "         var a = Math.ceil(Math.random() * 9)+ '';\n"
    "         var b = Math.ceil(Math.random() * 9)+ '';\n"
    "         var c = Math.ceil(Math.random() * 9)+ '';\n"
    "         var d = Math.ceil(Math.random() * 9)+ '';\n"
    "         var e = Math.ceil(Math.random() * 9)+ '';\n\n"
    "         var code = a + b + c + d + e;\n"
    "         document.getElementById(\"txtCaptcha\").value = code;\n"
    "         document.getElementById(\"txtCaptchaDiv\").innerHTML = code;\n\n");
hPrintf(
    " function ValidCaptcha(){\n"
    "         var str1 = removeSpaces(document.getElementById('txtCaptcha').value);\n"
    "         var str2 = removeSpaces(document.getElementById('txtInput').value);\n"
    "         if (str1 == str2){\n"
    "                 return true;\n"
    "         } else {\n"
    "                 return false;\n"
    "         }\n"
    " }\n\n");
hPrintf(
    " function removeSpaces(string){\n"
    "         return string.split(' ').join('');\n"
    " }\n"
    " </script><br />\n\n");
}

void printSubmitFormScript()
/* javascript to submit form */
{
hPrintf(
    "     <script type=\"text/javascript\">\n"
    "     function submitform()\n"
    "     {\n"
    "      if ( validateMainForm(document.forms[\"mainForm\"]) && checkCaptcha(document.forms[\"mainForm\"]))\n"
    "        {\n"
    "          document.forms[\"mainForm\"].submit();\n"
    "        }\n"
    "     }\n"
    "     </script>\n\n");
}

void printSuggestionConfirmed(char *summary, char * refID, char *userAddr, char *adminAddr, char *details)
/* display suggestion confirm page */
{
hPrintf(
    "<h2>Thank you for your suggestion!</h2>");
hPrintf(
    "<p>"
    "You may follow up on the status of your request at any time by "
    "<a href=\"../contacts.html#followup\">contacting us</a> and quoting your reference number:<BR><BR>%s<BR><BR>"
    "A copy of this information has also been sent to you at %s.<BR></p>",
     refID, userAddr); 
hPrintf(
    "<p><a href=\"hgUserSuggestion\">Click here if you wish to make additional suggestions.</a></p>");
hPrintf(
    "<p>"
    "<B>Your suggestion summary:</B><BR>"
    "%s<BR>"
    "<B>Your suggestion details:</B><BR>"
    "<pre>%s</pre>"
    "</p>",
    summary, details);
} 

void printInvalidEmailAddr(char *invalidEmailAddr)
/* display suggestion confirm page */
{
hPrintf(
    "<h2>Invalid email address format.</h2>");
hPrintf(
    "<p>"
    "The email address \"%s\" is invalid. Please correct it and "
    "<a href=\"javascript: history.go(-1)\">submit</a> again.</p>",
    invalidEmailAddr);
}

void sendSuggestionBack(char *sName, char *sEmail, char *sCategory, char *sSummary, char *sDetails, char *suggestID)
/* send back the suggestion */
{
/* parameters from hg.cong */
char *mailTo = mailToAddr();
char *mailFrom=mailFromAddr();
char *filter=filterKeyword();
char subject[512];
char msg[4608]; /* need to make larger */
safef(msg, sizeof(msg),
    "SuggestionID:: %s\nUserName:: %s\nUserEmail:: %s\nCategory:: %s\nSummary:: %s\n\n\nDetails::\n%s",
    suggestID, sName, sEmail, sCategory, sSummary, sDetails);

safef(subject, sizeof(subject),"%s %s", filter, suggestID);   
int result;
result = mailViaPipe(mailTo, subject, msg, mailFrom);
}

void sendConfirmMail(char *emailAddr, char *suggestID, char *summary, char *details)
/* send user suggestion confirm mail */
{
char subject[512];
char msg[4608];
char *remoteAddr=getenv("REMOTE_ADDR");
char brwName[512];
char returnAddr[512];
char signature[512];
char userEmailAddr[512];
safecpy(brwName,sizeof(brwName), browserName());
safecpy(returnAddr,sizeof(returnAddr), mailReturnAddr());
safecpy(signature,sizeof(signature), mailSignature());
safecpy(userEmailAddr, sizeof(userEmailAddr),emailAddr);
safef(subject, sizeof(subject),"Thank you for your suggestion to the %s", brwName);
safef(msg, sizeof(msg),
    "  Someone (probably you, from IP address %s) submitted a suggestion to the %s regarding \"%s\".\n\n  The suggestion has been assigned a reference number of \"%s\". If you wish to follow up on the progress of this suggestion with browser staff, you may contact us at %s. Please include the reference number of your suggestion in the email.\n\nThank you for your input,\n%s\n\nYour suggestion summary:\n%s\n\nYour suggestion details:\n%s",
remoteAddr, brwName, summary, suggestID, returnAddr, signature, summary, details);
int result;
result = mailViaPipe(userEmailAddr, subject, msg, returnAddr);
}

void askForSuggest(char *organism, char *db)
/* Put up the suggestion form. */
{
printMainForm();
printValidateScript();
printCheckCaptchaScript();
printSubmitFormScript();
//cartSaveSession(cart);
}

void  submitSuggestion()
/* send the suggestion to ,.. */
{
/* parameters from hg.cong */
char *filter=filterKeyword();

/* values from cart */
char *sName=cartUsualString(cart,"suggestName","");
char *sEmail=cartUsualString(cart,"suggestEmail","");
char *sCategory=cartUsualString(cart,"suggestCategory","");
char *sSummary=cartUsualString(cart,"suggestSummary","");
char *sDetails=cartUsualString(cart,"suggestDetails","");

char suggestID[512];
safef(suggestID, sizeof(suggestID),"%s %s", sEmail, now());
char subject[512];
safef(subject, sizeof(subject),"%s %s", filter, suggestID);
/* Send back suggestion only with valid user email address */
if (spc_email_isvalid(sEmail) != 0)
{
    /* send back the suggestion */
    sendSuggestionBack(sName, sEmail, sCategory, sSummary, sDetails, suggestID);
    /* send confirmation mail to user */
    sendConfirmMail(sEmail,suggestID, sSummary, sDetails);
    /* display confirmation page */
    printSuggestionConfirmed(sSummary, suggestID, sEmail, mailReturnAddr(), sDetails);
} else {
    /* save all field value in cart */
     printInvalidEmailAddr(sEmail);
}
cartRemove(cart, "do.suggestSendMail");
}

void doMiddle(struct cart *theCart)
/* Write header and body of html page. */
{
char *db, *organism;
cart = theCart;
getDbAndGenome(cart, &db, &organism, oldVars);
cartWebStart(theCart, db, "User Suggestion");
checkHgConfForSuggestion();
if (cartVarExists(cart, "do.suggestSendMail"))
    {
    submitSuggestion();
    cartRemove(cart, "do.suggestSendMail");
    return;
    }

askForSuggest(organism,db);
cartWebEnd();
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", "Clear", NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
long enteredMainTime = clock1000();
oldVars = hashNew(10);
cgiSpoof(&argc, argv);

htmlSetStyleSheet("/style/userAccounts.css");
htmlSetFormClass("accountScreen");
cartHtmlShell("User Suggestion",doMiddle, hUserCookie(), excludeVars, oldVars);
//cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
cgiExitTime("hgUserSuggestion", enteredMainTime);
return 0;
}

