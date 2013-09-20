// jsHelper.c - helper routines for interface between CGIs and client-side javascript

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

static boolean jsInited = FALSE;

struct jsonElement *jsonGlobalsHash = NULL;

/* mainForm/hiddenForm code supports the following: when the user selects
 * something from a pull-down, it will go hit the server to
 * figure out how to reload other control options based on the choice.
 * (For instance if they change the group, which items in the track
 * drop-down need to change).
 *
 * We accomplish this by maintaining two forms - a mainForm and a
 * hiddenForm.  The hiddenForm maintains echo's of all the variables
 * in the main form, which get updated onChange of controls that need
 * to 'ripple' to other controls.  The onChange also submits the
 * control. */

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
{
/* Strip out text matching regEx from str.
   flags is passed through to regcomp as the cflags argument.
   Returned string should be free'ed after use. */
return replaceRegEx(str, NULL, regEx, flags);
}

char *replaceRegEx(char *str, char *replace, char *regEx, int flags)
{
/* Replace text matching regEx in str with replace string.
   flags is passed through to regcomp as the cflags argument.
   Returned string should be free'ed after use. */
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
    if(replace != NULL)
        dyStringAppend(dy, replace);
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

void jsBeginCollapsibleSectionFontSize(struct cart *cart, char *track, char *section,
				       char *sectionTitle, boolean isOpenDefault, char *fontSize)
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
#ifdef BUTTONS_BY_CSS
hPrintf("<span class='pmButton bigBlue' onclick=\"setTableRowVisibility(this, '%s', "
        "'%s.section', 'section', true)\" id='%s_button' title='%s this section'>%c</span>",
        section, track, section, (isOpen ? "Collapse": "Expand"), (isOpen ? '-' : '+'));
#else///ifndef BUTTONS_BY_CSS
char *buttonImage = (isOpen ? "../images/remove_sm.gif" : "../images/add_sm.gif");
printf("<IMG height='18' width='18' "
       "onclick=\"return setTableRowVisibility(this, '%s', '%s.section', 'section', true);\" "
       "id='%s_button' src='%s' alt='%s' title='%s this section' class='bigBlue'"
       " style='cursor:pointer;'>\n",
       section, track,
       section, buttonImage, (isOpen ? "-" : "+"), (isOpen ? "Collapse": "Expand"));
#endif///ndef BUTTONS_BY_CSS
printf("<B style='font-size:%s;'>&nbsp;%s</B></TD></TR>\n", fontSize, sectionTitle);
printf("<TR %sid='%s-%d'><TD colspan=2>", isOpen ? "" : "style='display: none' ", section, 1);
}

void jsBeginCollapsibleSection(struct cart *cart, char *track, char *section, char *sectionTitle,
			       boolean isOpenDefault)
/* Make the hidden input, collapse/expand button and <TR id=...> needed for utils.js's
 * setTableRowVisibility().  Caller needs to have already created a <TABLE> and <FORM>. */
{
jsBeginCollapsibleSectionFontSize(cart, track, section, sectionTitle, isOpenDefault, "larger");
}

void jsEndCollapsibleSection()
/* End the collapsible <TR id=...>. */
{
puts("</TD></TR>");
}

void jsReloadOnBackButton(struct cart *cart)
/* Add some javascript to detect that the back button (or reload) has been pressed,
 * and to resubmit in that case to redraw the page with the latest cart contents. */
// __detectback trick from
// http://siphon9.net/loune/2009/07/detecting-the-back-or-refresh-button-click/
// Yes, I know this along with every other inline <script> here belongs in a .js module
{
printf("<script>\n"
       "document.write(\"<form style='display: none'><input name='__detectback' id='__detectback' "
       "value=''></form>\");\n"
       "function checkPageBackOrRefresh() {\n"
       "  if (document.getElementById('__detectback').value) {\n"
       "    return true;\n"
       "  } else {\n"
       "    document.getElementById('__detectback').value = 'been here';\n"
       "    return false;\n"
       "  }\n"
       "}\n"
       "window.onload = function() { "
       "  if (checkPageBackOrRefresh()) { \n"
       "    if (window.location.search == '?%s') { \n"
	      // We already have the hgsid-only URL that we want, reload it.
	      // (necessary for IE because IE doesn't reload on replace,
	      //  unless window.location and/or window.search changes)
       "      window.location.reload(true);\n"
       "    } else { \n"
       "      window.location.replace('%s?%s');\n"
       "    } \n"
       "  } "
       "};\n"
       "</script>\n", cartSidUrlString(cart), cgiScriptName(), cartSidUrlString(cart));
}

static struct jsonElement *newJsonElement(jsonElementType type)
// generic constructor for a jsonElement; callers fill in the appropriate value
{
struct jsonElement *ele;
AllocVar(ele);
ele->type = type;
return ele;
}

struct jsonElement *newJsonString(char *str)
{
struct jsonElement *ele = newJsonElement(jsonString);
ele->val.jeString = cloneString(str);
return ele;
}

struct jsonElement *newJsonBoolean(boolean val)
{
struct jsonElement *ele = newJsonElement(jsonBoolean);
ele->val.jeBoolean = val;
return ele;
}

struct jsonElement *newJsonNumber(long val)
{
struct jsonElement *ele = newJsonElement(jsonNumber);
ele->val.jeNumber = val;
return ele;
}

struct jsonElement *newJsonDouble(double val)
{
struct jsonElement *ele = newJsonElement(jsonDouble);
ele->val.jeDouble = val;
return ele;
}

struct jsonElement *newJsonObject(struct hash *h)
{
struct jsonElement *ele = newJsonElement(jsonObject);
ele->val.jeHash = h;
return ele;
}

struct jsonElement *newJsonList(struct slRef *list)
{
struct jsonElement *ele = newJsonElement(jsonList);
ele->val.jeList = list;
return ele;
}

void jsonObjectAdd(struct jsonElement *h, char *name, struct jsonElement *ele)
// Add a new element to a jsonObject; existing values are replaced.
// NOTE: Adding to a NULL hash will add to the global "common" hash printed with jsonPrintGlobals();
{
if (h == NULL)  // If hash isn't provided, assume global
    {
    if (jsonGlobalsHash == NULL)
        jsonGlobalsHash = newJsonObject(newHash(8));
    h = jsonGlobalsHash;
    }
if(h->type != jsonObject)
    errAbort("jsonObjectAdd called on element with incorrect type (%d)", h->type);
hashReplace(h->val.jeHash, name, ele);
}

void jsonListAdd(struct jsonElement *list, struct jsonElement *ele)
{
if(list->type != jsonList)
    errAbort("jsonListAdd called on element with incorrect type (%d)", list->type);
slAddHead(&list->val.jeList, ele);
}

static char *makeIndentBuf(int indentLevel)
{
if (indentLevel < 0)
    return "";
char *indentBuf;
indentBuf = needMem(indentLevel + 1);
memset(indentBuf, '\t', indentLevel);
indentBuf[indentLevel] = 0;
return indentBuf;
}

static void jsonPrintRecurse(struct jsonElement *ele, int indentLevel)
{
if (indentLevel >= -1) // Note that < -1 will result in no indenting
    indentLevel++;
char *tab = "\t";
char *nl = "\n";
if (indentLevel < 0)
    {
    tab = "";
    nl = "";
    }
char *indentBuf = makeIndentBuf(indentLevel);
switch (ele->type)
    {
    case jsonObject:
        {
        hPrintf("{%s",nl);
        if(hashNumEntries(ele->val.jeHash))
            {
            struct hashEl *el, *list = hashElListHash(ele->val.jeHash);
            slSort(&list, hashElCmp);
            for (el = list; el != NULL; el = el->next)
                {
                struct jsonElement *val = el->val;
                hPrintf("%s%s\"%s\": ", indentBuf, tab, el->name);
                jsonPrintRecurse(val, indentLevel);
                hPrintf("%s%s", el->next == NULL ? "" : ",",nl);
                }
            hashElFreeList(&list);
            }
        hPrintf("%s}", indentBuf);
        break;
        }
    case jsonList:
        {
        struct slRef *el;
        hPrintf("[%s",nl);
        if(ele->val.jeList)
            {
            for (el = ele->val.jeList; el != NULL; el = el->next)
                {
                struct jsonElement *val = el->val;
                hPrintf("%s%s", indentBuf,tab);
                jsonPrintRecurse(val, indentLevel);
                hPrintf("%s%s", el->next == NULL ? "" : ",",nl);
                }
            }
        hPrintf("%s]", indentBuf);
        break;
        }
    case jsonString:
        {
        hPrintf("\"%s\"", jsonStringEscape(ele->val.jeString));
        break;
        }
    case jsonBoolean:
        {
        hPrintf("%s", ele->val.jeBoolean ? "true" : "false");
        break;
        }
    case jsonNumber:
        {
        char buf[256];
        safef(buf, sizeof(buf), "%ld", ele->val.jeNumber);
        hPrintf("%s", buf);
        break;
        }
    case jsonDouble:
        {
        char buf[256];
        safef(buf, sizeof(buf), "%g", ele->val.jeDouble);
        hPrintf("%s", buf);
        break;
        }
    default:
        {
        errAbort("jsonPrintRecurse; invalid type: %d", ele->type);
        break;
        }
    }
if (indentLevel >= 0)
    freez(&indentBuf);
}

void jsonPrint(struct jsonElement *json, char *name, int indentLevel)
{
// print out a jsonElement, indentLevel -1 means no indenting

char *indentBuf = makeIndentBuf(indentLevel);
if(name != NULL)
    {
    if (indentLevel >= 0 )
        hPrintf("// START %s\n%s", name, indentBuf);
    hPrintf("var %s = ", name);
    }
jsonPrintRecurse(json, (indentLevel - 1)); // will increment back to indentLevel
if(name != NULL)
    {
    hPrintf("%s;\n", indentBuf);
    if (indentLevel >= 0 )
        hPrintf("// END %s\n", name);
    }
if (indentLevel >= 0)
    freez(&indentBuf);
}

void jsonPrintGlobals(boolean wrapWithScriptTags)
// prints out the "common" globals json hash
// This hash is the one utils.js and therefore all CGIs know about
{
if (jsonGlobalsHash != NULL)
    {
    if (wrapWithScriptTags)
        printf("<script type='text/javascript'>\n");
    jsonPrint(jsonGlobalsHash, "common", 0);
    if (wrapWithScriptTags)
        printf("</script>\n");
    }
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
dyStringAppend(ds, jsonStringEscape(dyStringCannibalize(&buf)));
dyStringPrintf(ds, "\"}");
va_end(args);
}

static void skipLeadingSpacesWithPos(char *s, int *posPtr)
/* skip leading white space. */
{
for (;;)
    {
    char c = s[*posPtr];
    if (!isspace(c))
	return;
    (*posPtr)++;
    }
}

static void getSpecificChar(char c, char *str, int *posPtr)
{
// get specified char from string or errAbort
if(str[*posPtr] != c)
    errAbort("Unexpected character '%c' (expected '%c') - string position %d\n", str[*posPtr], c, *posPtr);
(*posPtr)++;
}

static char *getString(char *str, int *posPtr)
{
// read a double-quote delimited string; we handle backslash escaping.
// returns allocated string.
boolean escapeMode = FALSE;
int i;
struct dyString *ds = dyStringNew(1024);
getSpecificChar('"', str, posPtr);
for(i = 0;; i++)
    {
    char c = str[*posPtr + i];
    if(!c)
        errAbort("Premature end of string (missing trailing double-quote); string position '%d'", *posPtr);
    else if(escapeMode)
        {
        // We support escape sequences listed in http://www.json.org,
        // except for Unicode which we cannot support in C-strings
        switch(c)
            {
            case 'b':
                c = '\b';
                break;
            case 'f':
                c = '\f';
                break;
            case 'n':
                c = '\n';
                break;
            case 'r':
                c = '\r';
                break;
            case 't':
                c = '\t';
                break;
            case 'u':
                errAbort("Unicode in JSON is unsupported");
                break;
            default:
                // we don't need to convert \,/ or "
                break;
            }
        dyStringAppendC(ds, c);
        escapeMode = FALSE;
        }
    else if(c == '"')
        break;
    else if(c == '\\')
        escapeMode = TRUE;
    else
        {
        dyStringAppendC(ds, c);
        escapeMode = FALSE;
        }
    }
*posPtr += i;
getSpecificChar('"', str, posPtr);
return dyStringCannibalize(&ds);
}

static struct jsonElement *jsonParseExpression(char *str, int *posPtr);

static struct jsonElement *jsonParseObject(char *str, int *posPtr)
{
struct hash *h = newHash(0);
getSpecificChar('{', str, posPtr);
while(str[*posPtr] != '}')
    {
    // parse out a name : val pair
    skipLeadingSpacesWithPos(str, posPtr);
    char *name = getString(str, posPtr);
    skipLeadingSpacesWithPos(str, posPtr);
    getSpecificChar(':', str, posPtr);
    skipLeadingSpacesWithPos(str, posPtr);
    hashAdd(h, name, jsonParseExpression(str, posPtr));
    skipLeadingSpacesWithPos(str, posPtr);
    if(str[*posPtr] == ',')
        (*posPtr)++;
    else
        break;
    }
skipLeadingSpacesWithPos(str, posPtr);
getSpecificChar('}', str, posPtr);
return newJsonObject(h);
}

static struct jsonElement *jsonParseList(char *str, int *posPtr)
{
struct slRef *list = NULL;
getSpecificChar('[', str, posPtr);
while(str[*posPtr] != ']')
    {
    struct slRef *e;
    AllocVar(e);
    skipLeadingSpacesWithPos(str, posPtr);
    e->val = jsonParseExpression(str, posPtr);
    slAddHead(&list, e);
    skipLeadingSpacesWithPos(str, posPtr);
    if(str[*posPtr] == ',')
        (*posPtr)++;
    else
        break;
    }
skipLeadingSpacesWithPos(str, posPtr);
getSpecificChar(']', str, posPtr);
slReverse(&list);
return newJsonList(list);
}

static struct jsonElement *jsonParseString(char *str, int *posPtr)
{
return newJsonString(getString(str, posPtr));
}

static struct jsonElement *jsonParseBoolean(char *str, int *posPtr)
{
struct jsonElement *ele = NULL;
int i;
for(i = 0; str[*posPtr + i] && isalpha(str[*posPtr + i]); i++);
    ;
char *val = cloneStringZ(str + *posPtr, i);
if(sameString(val, "true"))
    ele = newJsonBoolean(TRUE);
else if(sameString(val, "false"))
    ele =  newJsonBoolean(FALSE);
else
    errAbort("Invalid boolean value '%s'; pos: %d", val, *posPtr);
*posPtr += i;
freez(&val);
return ele;
}

static struct jsonElement *jsonParseNumber(char *str, int *posPtr)
{
int i;
boolean integral = TRUE;
struct jsonElement *retVal = NULL;

for(i = 0;; i++)
    {
    char c = str[*posPtr + i];
    if(c == 'e' || c == 'E' || c == '.')
        integral = FALSE;
    else if(!c || (!isdigit(c) && c != '-'))
        break;
    }
char *val = cloneStringZ(str + *posPtr, i);
*posPtr += i;
if(integral)
    retVal = newJsonNumber(sqlLongLong(val));
else
    {
    double d;
    if(sscanf(val, "%lf", &d))
        retVal = newJsonDouble(d);
    else
        errAbort("Invalid JSON Double: %s", val);
    }
freez(&val);
return retVal;
}

static struct jsonElement *jsonParseExpression(char *str, int *posPtr)
{
skipLeadingSpacesWithPos(str, posPtr);
char c = str[*posPtr];
if(c == '{')
    return jsonParseObject(str, posPtr);
else if (c == '[')
    return jsonParseList(str, posPtr);
else if (c == '"')
    return jsonParseString(str, posPtr);
else if (isdigit(c) || c == '-')
    return jsonParseNumber(str, posPtr);
else
    return jsonParseBoolean(str, posPtr);
// XXXX support null?
}

struct jsonElement *jsonParse(char *str)
{
// parse string into an in-memory json representation
int pos = 0;
struct jsonElement *ele = jsonParseExpression(str, &pos);
skipLeadingSpacesWithPos(str, &pos);
if(str[pos])
    errAbort("Invalid JSON: unprocessed trailing string at position: %d: %s", pos, str + pos);
return ele;
}

char *jsonStringEscape(char *inString)
/* backslash escape a string for use in a double quoted json string.
 * More conservative than javaScriptLiteralEncode because
 * some json parsers complain if you escape & or ' */
{
char c;
int outSize = 0;
char *outString, *out, *in;

if (inString == NULL)
    return(cloneString(""));

/* Count up how long it will be */
in = inString;
while ((c = *in++) != 0)
    {
    switch(c)
        {
        case '\"':
        case '\\':
        case '/':
        case '\b':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            outSize += 2;
            break;
        default:
            outSize += 1;
        }
    }
outString = needMem(outSize+1);

/* Encode string */
in = inString;
out = outString;
while ((c = *in++) != 0)
    {
    switch(c)
        {
        case '\"':
        case '\\':
        case '/':
        case '\b':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            *out++ = '\\';
            break;
        }
    *out++ = c;
    }
*out++ = 0;
return outString;
}
