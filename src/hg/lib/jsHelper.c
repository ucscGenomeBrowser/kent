/* javascript - some little helper routines  to manage our javascript.  
 * We don't do much javascript - just occassionally use it so that
 * when they select something from a pull-down, it will go hit the server to 
 * figure out how to reload other control options based on the choice.
 * (For instance if they change the group, which items in the track
 * drop-down need to change).
 *
 * We accomplish this by maintaining two forms - a mainForm and a
 * hiddenForm.  The hiddenForm maintains echo's of all the variables
 * in the main form, which get updated onChange of controls that need
 * to 'ripple' to other controls.  The onChange also submits the
 * control. */

#include "common.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hPrint.h"
#include "hash.h"
#include "jsHelper.h"
#include "hui.h"

static char const rcsid[] = "$Id: jsHelper.c,v 1.11 2008/04/30 00:26:06 larrym Exp $";

static boolean jsInited = FALSE;
struct hash *includedFiles = NULL;

void jsInit()
/* If this is the first call, set window.onload to the operations
 * performed upon loading a page and print supporting javascript.
 * Currently this just sets the page vertical position if specified on
 * CGI, and also calls jsWriteFunctions.
 * Subsequent calls do nothing, so this can be called many times. */
{
if (! jsInited)
    {
    puts(

"<INPUT TYPE=HIDDEN NAME=\"jsh_pageVertPos\" VALUE=0>\n"
"<script language=\"javascript\">\n"
"// f_scrollTop and f_filterResults taken from\n"
"// http://www.softcomplex.com/docs/get_window_size_and_scrollbar_position.html\n"
"function f_scrollTop() {\n"
"	return f_filterResults (\n"
"		window.pageYOffset ? window.pageYOffset : 0,\n"
"		document.documentElement ? document.documentElement.scrollTop : 0,\n"
"		document.body ? document.body.scrollTop : 0\n"
"	);\n"
"}\n"
"function f_filterResults(n_win, n_docel, n_body) {\n"
"	var n_result = n_win ? n_win : 0;\n"
"	if (n_docel && (!n_result || (n_result > n_docel)))\n"
"		n_result = n_docel;\n"
"	return n_body && (!n_result || (n_result > n_body)) ? n_body : n_result;\n"
"}\n"
"</script>\n");
    int pos = cgiOptionalInt("jsh_pageVertPos", 0);
    if (pos > 0)
	printf("\n<script language=\"javascript\">"
	       "window.onload = function () { window.scrollTo(0, %d); }"
	       "</script>\n", pos);
    jsWriteFunctions();
    jsInited = TRUE;
    }
}

void jsWriteFunctions()
/* Write out Javascript functions. */
{
hPrintf("\n<SCRIPT>\n");
hPrintf("%s\n",
"function setRadioCheck(varName, value) \n"
"{\n"
"var len = document.mainForm.elements.length;\n"
"var i = 0;\n"
"for (i = 0; i < len; i++) \n"
"    {\n"
"    if (document.mainForm.elements[i].name == varName) \n"
"	{\n"
"	if (document.mainForm.elements[i].value == value)\n"
"	    document.mainForm.elements[i].checked = true;\n"
"	else\n"
"	    document.mainForm.elements[i].checked = false;\n"
"	}\n"
"    }\n"
"}\n"
"\n"
"function getKeyCode(e)\n"
"{\n"
"if (window.event) // IE\n"
"    {\n"
"    return e.keyCode;\n"
"    }\n"
"else \n"
"    {\n"
"    return e.which;\n"
"    }\n"
"}\n"
"\n"
"function getKey(e)\n"
"{\n"
"return String.fromCharCode(getKeyCode(e));\n"
"}\n"
"\n"
"function gotEnterKey(e)\n"
"{\n"
"return getKeyCode(e) == 13;\n"
"}\n"
"\n"
"var submitted = false;\n"
"\n"
"function submitOnEnter(e,f)\n"
"{\n"
"if(gotEnterKey(e))\n"
"   {\n"
"   if (!submitted)\n"
"      {\n"
"      submitted = true;\n"
"      f.submit();\n"
"      }\n"
"   return false;\n"
"   }\n"
"else\n"
"   return true;\n"
"}\n"
"\n"
"function noSubmitOnEnter(e)\n"
"{\n"
"return !gotEnterKey(e);\n"
"}\n"
"function pressOnEnter(e, button)\n"
"{\n"
"if (gotEnterKey(e))\n"
"    {\n"
"    button.click();\n"
"    return false;\n"
"    }\n"
"else\n"
"    {\n"
"    return true;\n"
"    }\n"
"}\n"
);
hPrintf("</SCRIPT>\n");
}

struct dyString *jsOnChangeStart()
/* Start up an onChange string */
{
struct dyString *dy = dyStringNew(1024);
dyStringAppend(dy, "onChange=\"");
return dy;
}

char *jsOnChangeEnd(struct dyString **pDy)
/* Finish up javascript onChange command. */
{
dyStringAppend(*pDy, "document.hiddenForm.submit();\"");
return dyStringCannibalize(pDy);
}

void jsDropDownCarryOver(struct dyString *dy, char *var)
/* Add statement to carry-over drop-down item to dy. */
{
dyStringPrintf(dy, "document.hiddenForm.%s.value=", var);
dyStringPrintf(dy, "document.mainForm.%s.options", var);
dyStringPrintf(dy, "[document.mainForm.%s.selectedIndex].value; ", var);
}

void jsTextCarryOver(struct dyString *dy, char *var)
/* Add statement to carry-over text item to dy. */
{
dyStringPrintf(dy, 
    "document.hiddenForm.%s.value=document.mainForm.%s.value; ",
    var, var);
}

void jsTrackingVar(char *jsVar, char *val)
/* Emit a little Javascript to keep track of a variable. 
 * This helps especially with radio buttons. */
{
hPrintf("<SCRIPT>\n");
hPrintf("var %s='%s';\n", jsVar, val);
hPrintf("</SCRIPT>\n");
}

void jsMakeTrackingRadioButton(char *cgiVar, char *jsVar, 
	char *val, char *selVal)
/* Make a radio button that also sets tracking variable
 * in javascript. */
{
hPrintf("<INPUT TYPE=RADIO NAME=\"%s\"", cgiVar);
hPrintf(" VALUE=\"%s\"", val);
hPrintf(" onClick=\"%s='%s';\"", jsVar, val);
if (sameString(val, selVal))
    hPrintf(" CHECKED");
hPrintf(">");
}

void jsMakeTrackingCheckBox(struct cart *cart,
	char *cgiVar, char *jsVar, boolean usualVal)
/* Make a check box filling in with existing value and
 * putting a javascript tracking variable on it. */
{
char buf[256];
boolean oldVal = cartUsualBoolean(cart, cgiVar, usualVal);
hPrintf("<SCRIPT>var %s=%d;</SCRIPT>\n", jsVar, oldVal);
hPrintf("<INPUT TYPE=CHECKBOX NAME=%s VALUE=1", cgiVar);
if (oldVal)
    hPrintf(" CHECKED");
hPrintf(" onClick=\"%s=%d;\"", jsVar, !oldVal);
hPrintf(">");
sprintf(buf, "%s%s", cgiBooleanShadowPrefix(), cgiVar);
cgiMakeHiddenVar(buf, "1");
}

void jsTrackedVarCarryOver(struct dyString *dy, char *cgiVar, char *jsVar)
/* Carry over tracked variable (radio button?) to hidden form. */
{
dyStringPrintf(dy, "document.hiddenForm.%s.value=%s; ", cgiVar, jsVar);
}

char *jsRadioUpdate(char *cgiVar, char *jsVar, char *val)
/* Make a little javascript to check and uncheck radio buttons
 * according to new value.  To use this you must have called
 * jsWriteFunctions somewhere, and also must use jsMakeTrackingRadioButton
 * to make the buttons. */
{
static char buf[256];
safef(buf, sizeof(buf),
    "setRadioCheck('%s', '%s'); %s='%s'", cgiVar, val, jsVar, val);
return buf;
}

void jsCreateHiddenForm(struct cart *cart, char *scriptName,
	char **vars, int varCount)
/* Create a hidden form with the given variables */
{
int i;
hPrintf(
    "<FORM ACTION=\"%s\" "
    "METHOD=\"GET\" NAME=\"hiddenForm\">\n", scriptName);
cartSaveSession(cart);
for (i=0; i<varCount; ++i)
    hPrintf("<input type=\"hidden\" name=\"%s\" value=\"\">\n", vars[i]);
puts("</FORM>");
}

char *jsSetVerticalPosition(char *form)
/* Returns a javascript statement for storing the vertical position of the
 * page; typically this would go just before a document submit.  
 * jsInit must be called first.
 * Do not free return value!   */
{
if (! jsInited)
    errAbort("jsSetVerticalPosition: jsInit must be called first.");
static char vertPosSet[2048];
safef(vertPosSet, sizeof(vertPosSet),
      "document.%s.jsh_pageVertPos.value = f_scrollTop(); ", form);
return vertPosSet;
}

void jsMakeSetClearButton(struct cart *cart,
			  char *form, char *buttonVar, char *buttonLabel,
			  char *cartVarPrefix, struct slName *cartVarSuffixList,
			  char *anchor, boolean currentPos, boolean isSet)
/* Make a button for setting or clearing all of a list of boolean 
 * cart variables (i.e. checkboxes).  If this button was just pressed, 
 * set or clear those cart variables.
 * Optional html anchor is appended to the form's action if given. 
 * If currentPos, anchor is ignored and jsSetVerticalPosition is used so
 * that the new page gets the same vertical offset as the current page. */
{
struct slName *suffix;
char javascript[2048];
char *vertPosJs = "";
if (currentPos)
    {
    anchor = NULL;
    jsInit();
    vertPosJs = jsSetVerticalPosition(form);
    }
cgiMakeHiddenVar(buttonVar, "");
safef(javascript, sizeof javascript,
      "document.%s.action = '%s%s%s'; document.%s.%s.value='%s'; %s"
      "document.%s.submit();",
      form, cgiScriptName(),
      (isNotEmpty(anchor) ? "#" : ""), (isNotEmpty(anchor) ? anchor : ""),
      form, buttonVar, buttonLabel, jsSetVerticalPosition(form), form);
cgiMakeOnClickButton(javascript, buttonLabel);

if (isNotEmpty(cgiOptionalString(buttonVar)))
    {
    char option[1024];
    if (cartVarPrefix == NULL)
	cartVarPrefix = "";
    for (suffix = cartVarSuffixList;  suffix != NULL;  suffix = suffix->next)
        {
        safef(option, sizeof(option), "%s%s", cartVarPrefix, suffix->name);
        cartSetBoolean(cart, option, isSet);
        }
    }
}

char *jsPressOnEnter(char *button)
/* Returns a javascript statement that clicks button when the Enter key
 * has been pressed; typically this would go in a text input.
 * jsInit must be called first. 
 * Do not free return value!  */
{
if (! jsInited)
    errAbort("jsPressOnEnter: jsInit must be called first.");
static char poe[2048];
safef(poe, sizeof(poe), "return pressOnEnter(event, %s);", button);
return poe;
}

void jsIncludeFile(char *fileName)
{
/* Prints out html to include given javascript file from the js directory; suppresses redundant
 *  <script ...> tags if called repeatedly */

if(!includedFiles)
    includedFiles = newHash(0);
if(hashLookup(includedFiles, fileName) == NULL)
    {
    char *cgiRoot = hCgiRoot();
    /* tolerate missing cgiRoot (i.e. when running from command line) */
    if(cgiRoot)
        {
        static char realFileName[2048];
        safef(realFileName, sizeof(realFileName), "%sjs/%s", cgiRoot, fileName);
        if(!fileExists(realFileName))
            {
            fprintf(stderr, "jsIncludeFile: javascript fileName: %s doesn't exist.\n", realFileName);
            }
        }
    hashAdd(includedFiles, fileName, NULL);
    hPrintf("<script type='text/javascript' src='../cgi-bin/js/%s'></script>\n", fileName);
    }
}

char *jsCheckAllOnClickHandler(char *idPrefix, boolean state)
/* Returns javascript for use as an onclick attribute value to check all/uncheck all
 * all checkboxes with given idPrefix. 
 * state parameter determines whether to "check all" or "uncheck all" (TRUE means "check all"). */
{
static char buf[512];
jsIncludeFile("utils.js");
safef(buf, sizeof(buf), "setCheckBoxesWithPrefix(this, '%s', %s); return false", idPrefix, state ? "true" : "false");
return buf;
}

/* cgiMakeCheckAllSubmitButton really belongs in cheapcgi.c, but that is compiled without access to jsHelper.h */

void cgiMakeCheckAllSubmitButton(char *name, char *value, char *id, char *idPrefix, boolean state)
/* Make submit button which uses javascript to apply check all or uncheck all to all
 * checkboxes with given idPrefix.
 * state parameter determines whether to "check all" or "uncheck all" (TRUE means "check all"). 
 * id parameter may be NULL */
{
cgiMakeOnClickSubmitButton(jsCheckAllOnClickHandler(idPrefix, state), name, value);
}
