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
#include <regex.h>
#include "dystring.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hPrint.h"
#include "hash.h"
#include "jsHelper.h"
#include "web.h"
#include "hui.h"
#include "hgConfig.h"
#include "portable.h"

static char const rcsid[] = "$Id: jsHelper.c,v 1.31 2009/09/10 04:19:26 larrym Exp $";

static boolean jsInited = FALSE;

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
hPrintf(" onClick=\"%s=(%s+1)%%2;\"", jsVar, jsVar);
hPrintf(">");
safef(buf, sizeof(buf), "%s%s", cgiBooleanShadowPrefix(), cgiVar);
cgiMakeHiddenVar(buf, "0");
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
      form, buttonVar, buttonLabel, vertPosJs, form);
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

void jsMakeCheckboxGroupSetClearButton(char *buttonVar, boolean isSet)
/* Make a button for setting or clearing a set of checkboxes with the same name.
 * Uses only javascript to change the checkboxes, no resubmit. */
{
char javascript[256];
safef(javascript, sizeof(javascript), "var list = document.getElementsByName('%s'); "
      "for (var ix = 0; ix < list.length; ix++) {list[ix].checked = %s}", buttonVar,
      isSet ? "true" : "false");
cgiMakeOnClickButton(javascript, isSet ? JS_SET_ALL_BUTTON_LABEL : JS_CLEAR_ALL_BUTTON_LABEL);
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

void jsIncludeFile(char *fileName, char *noScriptMsg)
{
/* Prints out html to include given javascript file from the js directory; suppresses redundant
 *  <script ...> tags if called repeatedly.
 * <noscript>...</noscript> tags are provided automatically. The noscript message may be specified via
 * the noScriptMsg parameter (the string may contain HTML markup). A default msg is provided
 * if noScriptMsg == NULL; noscript msg is suppressed if noScriptMsg == "" (this is useful
 * if you want to more carefully control where the message will appear on the page). */
char *link = webTimeStampedLinkToResourceOnFirstCall(fileName,TRUE);
if (link != NULL)
    {
    static boolean defaultWarningShown = FALSE;
    if(noScriptMsg == NULL && !defaultWarningShown)
        {
        noScriptMsg = "<b>JavaScript is disabled in your web browser</b></p><p>You must have JavaScript enabled in your web browser to use the Genome Browser";
        defaultWarningShown = TRUE;
        }
    if(noScriptMsg && strlen(noScriptMsg))
        hPrintf("<noscript><div class='noscript'><div class='noscript-inner'><p>%s</p></div></div></noscript>\n", noScriptMsg);
    hPrintf("%s",link);
    freeMem(link);
    }
}

char *jsCheckAllOnClickHandler(char *idPrefix, boolean state)
/* Returns javascript for use as an onclick attribute value to check all/uncheck all
 * all checkboxes with given idPrefix.
 * state parameter determines whether to "check all" or "uncheck all" (TRUE means "check all"). */
{
static char buf[512];
jsIncludeFile("utils.js", NULL);
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

char *stripRegEx(char *str, char *regEx, int flags)
/* Strip out text matching regEx from str.
   flags is passed through to regcomp as the cflags argument.
   Returned string should be free'ed after use. */
{
regex_t re;
regmatch_t match[1];
int err = regcomp(&re, regEx, flags);
if(err)
    errAbort("regcomp failed; err: %d", err);
struct dyString *dy = newDyString(0);
size_t len = strlen(str);
size_t offset = 0;
while(offset < len && !regexec(&re, str + offset, 1, match, 0))
    {
    dyStringAppendN(dy, str + offset, match[0].rm_so);
    offset += match[0].rm_eo;
    }
if(offset < len)
    {
    dyStringAppend(dy, str + offset);
    }
regfree(&re);
return dyStringCannibalize(&dy);
}

char *jsStripJavascript(char *str)
/* Strip out anything that looks like javascript in html string.
   This function is designed to cleanup user input (e.g. to avoid XSS attacks).
   In reality, we cannot remove javascript with 100% accuracy, b/c there are many browser
   specific ways of embedding javascript; see http://ha.ckers.org/xss.html for many, many examples.
   Returned string should be free'ed after use. */
{
char *regExs[] = {"<script\\s*>[^<]*</script\\s*>",
                   "<script[^>]*>" // handles case where they have an un-closed script tag with a src attribute
			};
int i;
str = cloneString(str);
for(i=0;i<ArraySize(regExs);i++)
    {
    char *tmp = str;
    str = stripRegEx(str, regExs[i], REG_ICASE);
    freeMem(tmp);
    }
return str;
}

boolean advancedJavascriptFeaturesEnabled(struct cart *cart)
// Returns TRUE if advanced javascript features are currently enabled
{
static boolean alreadyLookedForadvancedJs = FALSE;
static boolean advancedJsEnabled = FALSE;
if(!alreadyLookedForadvancedJs)
    {
    char *ua = cgiUserAgent();
    boolean defaultVal = TRUE;

    // dragZooming was broken in version 530.4 of AppleWebKit browsers (used by Safari, Chrome and some other browsers).
    // This was explicitly fixed by the WebKit team in version 531.0.1 (see http://trac.webkit.org/changeset/45143).
    // The AppleWebKit version provided by the browser in user agent doesn't always include the minor version number, so to
    // be overly conservative we default drag-and-drop to off when AppleWebKit major version == 530

    if(ua != NULL)
        {
        char *needle = "AppleWebKit/";
        char *ptr = strstr(ua, needle);
        if(ptr != NULL)
            {
            int version = 0;
            sscanf(ptr + strlen(needle), "%d", &version);
            defaultVal = (version != 530);
            }
        }
    advancedJsEnabled = cartUsualBoolean(cart, "enableAdvancedJavascript", defaultVal);
    alreadyLookedForadvancedJs = TRUE;
    }
//else
//    warn("already looked up advancedJsEnabled");  // got msg 41 times in one page!
return advancedJsEnabled;
}

void jsBeginCollapsibleSection(struct cart *cart, char *track, char *section, char *sectionTitle,
			       boolean isOpenDefault)
/* Make the hidden input, collapse/expand button and <TR id=...> needed for utils.js's
 * setTableRowVisibility().  Caller needs to have already created a <TABLE> and <FORM>. */
{
char collapseGroupVar[512];
safef(collapseGroupVar, sizeof(collapseGroupVar), "%s.section_%s_close", track, section);
boolean isOpen = !cartUsualBoolean(cart, collapseGroupVar, !isOpenDefault);

// Both plus button and title are now in same <TD>
// but still colspan=2 because we are lib code and callers own the table.
printf("<TR><TD colspan=2 style='text-align:left;'>\n");
printf("<input type='hidden' name='%s' id='%s' value='%s'>\n",
       collapseGroupVar, collapseGroupVar, isOpen ? "0" : "1");
char *buttonImage = (isOpen ? "../images/remove_sm.gif" : "../images/add_sm.gif");
printf("<IMG height='18' width='18' "
       "onclick=\"return setTableRowVisibility(this, '%s', '%s.section', 'section', true);\" "
       "id='%s_button' src='%s' alt='%s' title='%s this section' class='bigBlue'"
       " style='cursor:pointer;'>\n",
       section, track,
       section, buttonImage, (isOpen ? "-" : "+"), (isOpen ? "Collapse": "Expand"));
printf("<B style='font-size:larger;'>&nbsp;%s</B></TD></TR>\n", sectionTitle);
printf("<TR %sid='%s-%d'><TD colspan=2>", isOpen ? "" : "style='display: none' ", section, 1);
}

void jsEndCollapsibleSection()
/* End the collapsible <TR id=...>. */
{
puts("</TD></TR>");
}

struct jsonStringElement *newJsonString(char *str)
{
struct jsonStringElement *ele;
AllocVar(ele);
ele->str = cloneString(str);
ele->type = jsonString;
return ele;
}

struct jsonHashElement *newJsonHash(struct hash *h)
{
struct jsonHashElement *ele;
AllocVar(ele);
ele->type = jsonHash;
ele->hash = h;
return ele;
}

struct jsonListElement *newJsonList(struct slRef *list)
{
struct jsonListElement *ele;
AllocVar(ele);
ele->type = jsonList;
ele->list = list;
return ele;
}

void jsonHashAdd(struct jsonHashElement *h, char *name, struct jsonElement *ele)
{
hashReplace(h->hash, name, ele);
}

void jsonHashAddString(struct jsonHashElement *h, char *name, char *val)
{
// Add a string to a hash which will be used to print a javascript object;
// existing values are replaced.
val = javaScriptLiteralEncode(val);
char *str = needMem(strlen(val) + 3);
sprintf(str, "'%s'", val);
freez(&val);
jsonHashAdd(h, name, (struct jsonElement *) newJsonString(str));
}

void jsonHashAddNumber(struct jsonHashElement *h, char *name, long val)
{
// Add a number to a hash which will be used to print a javascript object;
// existing values are replaced.
char buf[256];
safef(buf, sizeof(buf), "%ld", val);
jsonHashAdd(h, name, (struct jsonElement *) newJsonString(buf));
}

void jsonHashAddDouble(struct jsonHashElement *h, char *name, double val)
{
// Add a number to a hash which will be used to print a javascript object;
// existing values are replaced.
char buf[256];
safef(buf, sizeof(buf), "%.10f", val);
jsonHashAdd(h, name, (struct jsonElement *) newJsonString(buf));
}

void jsonHashAddBoolean(struct jsonHashElement *h, char *name, boolean val)
{
// Add a boolean to a hash which will be used to print a javascript object;
// existing values are replaced.
jsonHashAdd(h, name, (struct jsonElement *) newJsonString(val ? "true" : "false"));
}

void jsonListAdd(struct slRef **list, struct jsonElement *ele)
{
struct slRef *e;
AllocVar(e);
e->val = ele;
slAddHead(list, e);
}

void jsonListAddString(struct slRef **list, char *val)
{
val = javaScriptLiteralEncode(val);
char *str = needMem(strlen(val) + 3);
sprintf(str, "'%s'", val);
freez(&val);
jsonListAdd(list, (struct jsonElement *) newJsonString(str));
}

void jsonListAddNumber(struct slRef **list, long val)
{
char buf[256];
safef(buf, sizeof(buf), "%ld", val);
jsonListAdd(list, (struct jsonElement *) newJsonString(buf));
}

void jsonListAddDouble(struct slRef **list, double val)
{
char buf[256];
safef(buf, sizeof(buf), "%.10f", val);
jsonListAdd(list, (struct jsonElement *) newJsonString(buf));
}

void jsonListAddBoolean(struct slRef **list, boolean val)
{
jsonListAdd(list, (struct jsonElement *) newJsonString(val ? "true" : "false"));
}

static char *makeIndentBuf(int indentLevel)
{
char *indentBuf;
indentBuf = needMem(indentLevel + 1);
memset(indentBuf, '\t', indentLevel);
indentBuf[indentLevel] = 0;
return indentBuf;
}

static void jsonPrintRecurse(struct jsonElement *json, int indentLevel)
{
char *indentBuf = makeIndentBuf(indentLevel);
switch (json->type)
    {
    case jsonHash:
        {
        struct jsonHashElement *ele = (struct jsonHashElement *) json;
        hPrintf("{\n");
        if(hashNumEntries(ele->hash))
            {
            struct hashEl *el, *list = hashElListHash(ele->hash);
            slSort(&list, hashElCmp);
            for (el = list; el != NULL; el = el->next)
                {
                struct jsonElement *val = (struct jsonElement *) el->val;
                hPrintf("%s\t\"%s\": ", indentBuf, el->name);
                jsonPrintRecurse(val, indentLevel + 1);
                hPrintf("%s\n", el->next == NULL ? "" : ",");
                }
            hashElFreeList(&list);
            }
        hPrintf("%s}", indentBuf);
        break;
        }
    case jsonList:
        {
        struct jsonListElement *rec = (struct jsonListElement *) json;
        struct slRef *el;
        hPrintf("[\n");
        if(rec->list)
            {
            for (el = rec->list; el != NULL; el = el->next)
                {
                struct jsonElement *val = (struct jsonElement *) el->val;
                hPrintf("%s\t", indentBuf);
                jsonPrintRecurse(val, indentLevel + 1);
                hPrintf("%s\n", el->next == NULL ? "" : ",");
                }
            }
        hPrintf("%s]", indentBuf);
        break;
        }
    case jsonString:
        {
        hPrintf("%s", ((struct jsonStringElement *) json)->str);
        break;
        }
    default:
        {
        errAbort("jsonPrintRecurse; invalid type: %d", json->type);
        break;
        }
    }
freez(&indentBuf);
}

void jsonPrint(struct jsonElement *json, char *name, int indentLevel)
{
// print out a jsonElement

char *indentBuf = makeIndentBuf(indentLevel);
if(name != NULL)
    hPrintf("// START %s\n%svar %s = ", name, indentBuf, name);
jsonPrintRecurse(json, indentLevel);
if(name != NULL)
    hPrintf("%s;\n// END %s\n", indentBuf, name);
freez(&indentBuf);
}

void jsonErrPrintf(struct dyString *ds, char *format, ...)
//  Printf a json error to a dyString for communicating with ajax code; format is:
//  {"error": error message here}
{
va_list args;
va_start(args, format);
dyStringPrintf(ds, "{\"error\": \"");
struct dyString *buf = newDyString(1000);
dyStringVaPrintf(buf, format, args);
dyStringAppend(ds, javaScriptLiteralEncode(dyStringCannibalize(&buf)));
dyStringPrintf(ds, "\"}");
va_end(args);
}
