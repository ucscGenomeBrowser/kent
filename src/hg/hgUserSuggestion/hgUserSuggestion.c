/* hgUserSuggestion - CGI-script to collect user's suggestion. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "errAbort.h"
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
#include "htmlPage.h"
#include "net.h"
#include "jsonParse.h"

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

boolean validateCategory(char *category)
/* Validate the Category from the request */
{
const char *cat[5] = {"Tracks", "Genome Assemblies",  "Browser Tools",
                      "Command-line Utilities", "Others"};
int i;
for(i=0;i<5;i++)
{
    if (strcmp(cat[i], category)==0) return TRUE;
}
return FALSE;
}


/* javascript functions */
void printMainForm()
/* Create the main suggestion form */
{
hPrintf(
    "     <FORM  ACTION=\"../cgi-bin/hgUserSuggestion?do.suggestSendMail=1\" METHOD=\"POST\" ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\" id='mainForm'>\n");
jsOnEventById("load","mainForm","document.forms.mainForm.name.focus();");
hPrintf(
    "<H2>User Suggestion Form</H2>\n"
    "<P>If you have ideas about how we can improve the value of the Genome Browser to your research, "
    "we'd like to hear from you. Please provide a concise description below. "
    "A copy of the suggestion will be sent to your email address along with a reference number. "
    "You may follow up on the status of your request at any time by <a href=\"../contacts.html#followup\">contacting us</a> and quoting the reference number.</P>");
hPrintf("<P>Please note: this form is not the proper place to submit questions regarding browser use or bug reports. Use the links on our <a href=\"../contacts.html\">contact page</a> instead.</P>");
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
    "<input type=\"text\" name=\"suggestWebsite\" style=\"display: none;\" />"
    "     </div>\n");
if (isNotEmpty(cfgOption(CFG_SUGGEST_SITE_KEY)))
    {
    // hidden reCaptcha token input
    hPrintf("<input type='hidden' id='reCaptchaToken' name='reCaptchaToken'>");
    }
else
    {
    hPrintf(
    "         <p>\n"
    "           <label for=\"code\">Enter the following value below: <span id=\"txtCaptchaDiv\" style=\"color:#F00\"></span><BR> \n"
    "           <input type=\"hidden\" id=\"txtCaptcha\" /></label>\n"
    "           <input type=\"text\" name=\"txtInput\" id=\"txtInput\" size=\"30\" />\n"
    "         </p>\n");
    }

hPrintf(
    "      <div class=\"formControls\">\n"
    "        <input id=\"sendButton\" type=\"button\" value=\"Send\"> \n"
    "        <input type=\"reset\" name=\"suggestClear\" value=\"Clear\" class=\"largeButton\"> \n"
    "      </div>\n"
    "      \n"
    "     </FORM>\n\n");


jsOnEventById("click","sendButton","submitform();");
}

void printValidateScript()
/* javascript to validate form inputs */
{
jsInline(
    "    function validateMainForm(theform)\n"
    "    {\n"
    "    var x=theform.suggestName.value;\n"
    "    if (x==null || x==\"\")\n"
    "      {\n"
    "      alert(\"Name field must be filled out\");\n"
    "      theform.suggestName.focus() ;\n"
    "      return false;\n"
    "      }\n"
    "    var y=theform.suggestEmail.value;\n"
    "    if (y==null || y==\"\")\n"
    "      {\n"
    "      alert(\"Email field must be filled out\");\n"
    "      theform.suggestEmail.focus() ;\n"
    "      return false;\n"
    "      }\n"
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
    "      }\n"
    "    var y=theform.suggestSummary.value;\n"
    "    if (y==null || y==\"\")\n"
    "      {\n"
    "      alert(\"Summary field must be filled out\");\n"
    "      theform.suggestSummary.focus() ;\n"
    "      return false;\n"
    "      }          \n"
    "    return true; \n"
    "    }"
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
    );
}

void printCheckCaptchaScript()
/* javascript to check CAPTCHA code */
{
jsInline(
    " // The Simple JavaScript CAPTCHA Generator code is copied from typicalwhiner.com/190/simple-javascript-captcha-generator \n"
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
    "         }\n\n"
    "         var a = Math.ceil(Math.random() * 9)+ '';\n"
    "         var b = Math.ceil(Math.random() * 9)+ '';\n"
    "         var c = Math.ceil(Math.random() * 9)+ '';\n"
    "         var d = Math.ceil(Math.random() * 9)+ '';\n"
    "         var e = Math.ceil(Math.random() * 9)+ '';\n\n"
    "         var code = a + b + c + d + e;\n"
    "         document.getElementById(\"txtCaptcha\").value = code;\n"
    "         document.getElementById(\"txtCaptchaDiv\").innerHTML = code;\n\n"
    " function ValidCaptcha(){\n"
    "         var str1 = removeSpaces(document.getElementById('txtCaptcha').value);\n"
    "         var str2 = removeSpaces(document.getElementById('txtInput').value);\n"
    "         if (str1 == str2){\n"
    "                 return true;\n"
    "         } else {\n"
    "                 return false;\n"
    "         }\n"
    " }\n\n"
    " function removeSpaces(string){\n"
    "         return string.split(' ').join('');\n"
    " }\n"
    );
}

void printSubmitFormScript()
/* javascript to submit form */
{
if (isNotEmpty(cfgOption(CFG_SUGGEST_SITE_KEY)))
    {
    struct dyString *jsText = dyStringNew(0);
    dyStringPrintf(jsText, "\n function submitform(){\n"
    "         if ( validateMainForm(document.forms['mainForm'])){\n"
    "             grecaptcha.execute('%s', { action: 'userSuggest' })\n"
    "               .then(function(token) {\n"
    "                document.getElementById('reCaptchaToken').value = token;\n"
    "                 document.forms['mainForm'].submit();\n"
    "             });\n"
    "         }\n"
    " }\n", cfgOption(CFG_SUGGEST_SITE_KEY));
    jsInline(dyStringCannibalize(&jsText));
    }
else
    {
jsInline(
    "\n function submitform(){\n"
    "         if ( validateMainForm(document.forms['mainForm']) && checkCaptcha(document.forms['mainForm'])){\n"
    "                 document.forms['mainForm'].submit();\n"
    "         }\n"
    " }\n"
    );
    }
}

static void printReCaptchaV3()
/* output the js to perform the reCAPTCHA v3 function */
{
jsInline(
    "\n window.onload = function() {\n"
    "  grecaptcha.ready();\n"
    " };\n"
    );
}

void printSuggestionConfirmed(char *summary, char * refID, char *userAddr, char *adminAddr, char *details, double captchaScore)
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
if (captchaScore > -1.0)
    hPrintf("<p>(google captcha score: %g)</p>\n", captchaScore);
}

void printInvalidForm(double captchaScore, boolean robot)
/* display invalid form page */
{
if (captchaScore > -1.0)
    {
    if (robot)
	{
        char *sName=cartUsualString(cart,"suggestName","no name entered");
        char *sEmail=cartUsualString(cart,"suggestEmail","no email entered");
        char *sCategory=cartUsualString(cart,"suggestCategory","no category entered");
        char *sSummary=cartUsualString(cart,"suggestSummary","no summary entered");
        char *sDetails=cartUsualString(cart,"suggestDetails","no details entered");
	hPrintf(
        "<h2>Invalid Form.</h2>"
	"<p>"
	"Congratulations, your google captcha score (%g) appears to qualify "
        "you as a robot.  If this is in error, please email our support email:"
        "&nbsp;<a href='mailto:%s?subject=suggestion "
        "failed captcha&body=Failed captcha test in suggestion form, "
        "score: %g, %s, %s, %s, %s, %s'>I am *not* a ROBOT !</a>"
	"</p>", captchaScore, mailToAddr(), captchaScore, sName, sEmail, sCategory, sSummary, sDetails
	);
	}
    else
	{
	hPrintf(
        "<h2>Invalid Form.</h2>"
	"<p>"
	"The form is invalid. Please correct it and "
	"<a id='goBack' >submit</a> again. (score: %g)</p>", captchaScore
	);
	}
    }
else
    hPrintf(
        "<h2>Invalid Form.</h2>"
	"<p>"
	"The form is invalid. Please correct it and "
	"<a id='goBack' >submit</a> again.</p>"
    );
jsOnEventById("click", "goBack", "history.go(-1)");
}

void printInvalidCategory(char *invalidCategory)
/* display invalid category page */
{
hPrintf(
    "<h2>Invalid Category.</h2>");
hPrintf(
    "<p>"
    "The category \"%s\" is invalid. Please correct it and "
    "<a id='goBack'>submit</a> again.</p>",
    invalidCategory);
jsOnEventById("click", "goBack", "history.go(-1)");
}

void printInvalidEmailAddr(char *invalidEmailAddr)
/* display suggestion confirm page */
{
hPrintf(
    "<h2>Invalid email address format.</h2>");
hPrintf(
    "<p>"
    "The email address \"%s\" is invalid. Please correct it and "
    "<a id='goBack'>submit</a> again.</p>",
    invalidEmailAddr);
jsOnEventById("click", "goBack", "history.go(-1)");
}

void sendSuggestionBack(char *sName, char *sEmail, char *sCategory, char *sSummary, char *sDetails, char *suggestID)
/* send back the suggestion */
{
/* parameters from hg.conf */
char *mailTo = mailToAddr();
char *mailFrom=mailFromAddr();
char *filter=filterKeyword();
char subject[512];
char msg[4608]; /* need to make larger */
safef(msg, sizeof(msg),
    "SuggestionID:: %s\nUserName:: %s\nUserEmail:: %s\nCategory:: %s\nSummary:: %s\n\n\nDetails::\n%s",
    suggestID, sName, sEmail, sCategory, sSummary, sDetails);

safef(subject, sizeof(subject),"%s %s", filter, suggestID);
// ignore returned result
mailViaPipe(mailTo, subject, msg, mailFrom);
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
// ignore returned result
mailViaPipe(userEmailAddr, subject, msg, returnAddr);
}

void askForSuggest(char *organism, char *db)
/* Put up the suggestion form. */
{
printMainForm();
printValidateScript();
if (isNotEmpty(cfgOption(CFG_SUGGEST_SITE_KEY)))
    {
    printSubmitFormScript();
    }
else
    {
    printCheckCaptchaScript();
    printSubmitFormScript();
    }
//cartSaveSession(cart);
}

static void recordError(double score, char *msg, char *urlResult)
/* record captcha errors in apache error_log */
{
if (urlResult)
    {
    stripChar(urlResult, '\n');
    stripChar(urlResult, ' ');
    fprintf(stderr, "reCAPTCHA score: %g, '%s' '%s'\n", score, msg, urlResult);
    }
else
    fprintf(stderr, "reCAPTCHA score: %g, '%s'\n", score, msg);
}

void  submitSuggestion()
/* send the suggestion to ,.. */
{
/* parameters from hg.conf */

/* values from cart */
char *sName=cartUsualString(cart,"suggestName","");
boolean captchaRobot = FALSE;	// OK when not configured
double captchaScore = -1.0;

if (isNotEmpty(cfgOption(CFG_SUGGEST_SECRET_KEY)))
    {
    captchaRobot = TRUE;	// assume robot until proven human
    captchaScore = -0.9;	// and allow score to show up in printout
  char *threshHoldString = cfgOptionDefault(CFG_SUGGEST_HUMAN_THRESHOLD, "-0.1");
    double threshHoldScore = sqlDouble(threshHoldString);
    char *reCaptcha = cartUsualString(cart,"reCaptchaToken", NULL);
    if (reCaptcha)
        {
        char siteverify[4096];
        safef(siteverify, sizeof(siteverify), "https://www.google.com/recaptcha/api/siteverify?secret=%s&response=%s", cfgOption(CFG_SUGGEST_SECRET_KEY), reCaptcha);
        struct htmlPage *verify = htmlPageGet(siteverify);
    /*  successful return:

    {  "success": true,  "challenge_ts": "2023-06-09T23:47:35Z",  "hostname": "genome-test.gi.ucsc.edu",  "score": 0.9,  "action": "userSuggest"}

        error return:
    {  "success": false,  "error-codes": [    "timeout-or-duplicate"  ]}'
    */

        if (verify)
            {
            struct lm *lm = lmInit(1<<16);
            struct jsonElement *jsonObj = jsonParseLm(verify->htmlText, lm);
            if (jsonObj)
                {
                struct jsonElement *score = jsonFindNamedField(jsonObj, NULL, "score");
                struct jsonElement *success = jsonFindNamedField(jsonObj, NULL, "success");
                if (success)
		    {
		    if (success->val.jeBoolean)
			{
			if (score)
			    {
                            captchaScore = score->val.jeDouble;
			    if (score->val.jeDouble > threshHoldScore)
				{
				captchaRobot = FALSE;
				recordError(captchaScore, "captcha approved", verify->htmlText);
				}
			    else
				recordError(captchaScore, "score < threshold", verify->htmlText);
			    }
			else
			    recordError(captchaScore, "score not found in JSON", verify->htmlText);
			}	// if (success->val.jeBoolean)
		    else
			recordError(captchaScore, "success FALSE", verify->htmlText);
		    }	// if (success)
		else
		  recordError(captchaScore, "success status not found in JSON", verify->htmlText);
                }	// if (jsonObj)
	    else
		recordError(captchaScore, "JSON object not found in siteverify return", verify->htmlText);
            }	// if (verify)
	else
	    recordError(captchaScore, "siteverify URL access failed", NULL);
        }	// if (reCaptcha)
    else
	recordError(captchaScore, "reCaptchaToken not found in form", NULL);
    }	// if (isNotEmpty(cfgOption(CFG_SUGGEST_SECRET_KEY)))

char *sEmail=cartUsualString(cart,"suggestEmail","");
char *sCategory=cartUsualString(cart,"suggestCategory","");
char *sSummary=cartUsualString(cart,"suggestSummary","");
char *sDetails=cartUsualString(cart,"suggestDetails","");
char *sWebsite=cartUsualString(cart,"suggestWebsite","");
char suggestID[512];
safef(suggestID, sizeof(suggestID),"%s %s", sEmail, now());

/* reject if the hidden field is not blank */
if (isNotEmpty(sWebsite) || captchaRobot)
    {
    printInvalidForm(captchaScore, captchaRobot);
    cartSetString(cart, "suggestWebsite", "");
    return;
    }

/* reject suggestion if category is invalid */
if (!validateCategory(sCategory))
    {
    printInvalidCategory(sCategory);
    return;
    }

/* Send back suggestion only with valid user email address */
if (spc_email_isvalid(sEmail) != 0)
    {
    /* send back the suggestion */
    sendSuggestionBack(sName, sEmail, sCategory, sSummary, sDetails, suggestID);
    /* send confirmation mail to user */
    sendConfirmMail(sEmail,suggestID, sSummary, sDetails);
    /* display confirmation page */
    printSuggestionConfirmed(sSummary, suggestID, sEmail, mailReturnAddr(), sDetails, captchaScore);
    }
else
    {
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
cartWebStart(theCart, db, "UCSC Genome Browser: Suggestion Box");
checkHgConfForSuggestion();
if (cartVarExists(cart, "do.suggestSendMail"))
    {
    submitSuggestion();
    cartRemove(cart, "do.suggestSendMail");
    return;
    }

askForSuggest(organism,db);
if (isNotEmpty(cfgOption(CFG_SUGGEST_SITE_KEY)))
    {
    printReCaptchaV3();
    hPrintf("<div><p>this site protected by reCAPTCHA and the Google <a href='https://policies.google.com/privacy?hl=en' target='_blank'>privacy policy</a> and <a href='https://policies.google.com/terms?hl=en' target='_blank'>terms of service</a> apply</p></div>\n");

    char footer[1024];
    safef(footer, sizeof(footer),
   "<script src='https://www.google.com/recaptcha/api.js?render=%s'></script>",
           cfgOption(CFG_SUGGEST_SITE_KEY));
    cartWebEndExtra(footer);
    }
else
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
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
cgiExitTime("hgUserSuggestion", enteredMainTime);
return 0;
}

