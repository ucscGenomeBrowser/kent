/* jsHelper - some little helper routines  to manage our javascript.  
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

#ifndef JSHELPER_H
#define JSHELPER_H

#include "cart.h"

#define JS_CLEAR_ALL_BUTTON_LABEL    "Clear all"
#define JS_SET_ALL_BUTTON_LABEL  "Set all"
#define JS_DEFAULTS_BUTTON_LABEL "Set defaults"

void jsCreateHiddenForm(struct cart *cart, char *scriptName,
	char **vars, int varCount);
/* Create a hidden form with the given variables.  Must be
 * called. */

void jsInit();
/* If this is the first call, set window.onload to the operations
 * performed upon loading a page and print supporting javascript.
 * Currently this just sets the page vertical position if specified on
 * CGI, and also calls jsWriteFunctions.
 * Subsequent calls do nothing, so this can be called many times. */

void jsWriteFunctions();
/* Write out Javascript functions. */

struct dyString *jsOnChangeStart();
/* Start up an onChange string */

char *jsOnChangeEnd(struct dyString **pDy);
/* Finish up javascript onChange command. */

void jsDropDownCarryOver(struct dyString *dy, char *var);
/* Add statement to carry-over drop-down item to dy. */

void jsTextCarryOver(struct dyString *dy, char *var);
/* Add statement to carry-over text item to dy. */

void jsTrackingVar(char *jsVar, char *val);
/* Emit a little Javascript to keep track of a variable. 
 * This helps especially with radio buttons. */

void jsMakeTrackingRadioButton(char *cgiVar, char *jsVar, 
	char *val, char *selVal);
/* Make a radio button that also sets tracking variable
 * in javascript. */

void jsMakeTrackingCheckBox(struct cart *cart, char *cgiVar, 
	char *jsVar, boolean usualVal);
/* Make a check box filling in with existing value and
 * putting a javascript tracking variable on it. */

void jsTrackedVarCarryOver(struct dyString *dy, char *cgiVar, char *jsVar);
/* Carry over tracked variable (radio button?) to hidden form. */

char *jsRadioUpdate(char *cgiVar, char *jsVar, char *val);
/* Make a little javascript to check and uncheck radio buttons
 * according to new value.  To use this you must have called
 * jsWriteFunctions somewhere, and also must use jsMakeTrackingRadioButton
 * to make the buttons. */

char *jsSetVerticalPosition(char *form);
/* Returns a javascript statement for storing the vertical position of the
 * page; typically this would go just before a document submit.  
 * jsInit must be called first.
 * Do not free return value!   */

void jsMakeSetClearButton(struct cart *cart,
			  char *form, char *buttonVar, char *buttonLabel,
			  char *cartVarPrefix, struct slName *cartVarSuffixList,
			  char *anchor, boolean currentPos, boolean isSet);
/* Make a button for setting or clearing all of a list of boolean 
 * cart variables (i.e. checkboxes).  If this button was just pressed, 
 * set or clear those cart variables.
 * Optional html anchor is appended to the form's action if given. 
 * If currentPos, anchor is ignored and jsSetVerticalPosition is used so
 * that the new page gets the same vertical offset as the current page. */

char *jsPressOnEnter(char *button);
/* Returns a javascript statement that clicks button when the Enter key
 * has been pressed; typically this would go in a text input.
 * jsInit must be called first. 
 * Do not free return value!  */

void jsIncludeFile(char *fileName);
/* Prints out html to include given javascript file from the js directory; suppresses redundant
 * <script ...> tags if called repeatedly */

char *jsCheckAllOnClickHandler(char *idPrefix, boolean state);
/* Returns javascript for use as an onclick attribute value to apply "check all"/"uncheck all" 
 * to all checkboxes with given idPrefix. 
 * state parameter determines whether to "check all" or "uncheck all" (TRUE means "check all"). */

void cgiMakeCheckAllSubmitButton(char *name, char *value, char *id, char *idPrefix, boolean state);
/* Make submit button which uses javascript to apply check all or uncheck all to all
 * checkboxes with given idPrefix.
 * state parameter determines whether to "check all" or "uncheck all" (TRUE means "check all"). 
 * id parameter may be NULL */

char *jsStripJavascript(char *str);
/* Strip out anything that looks like javascript in html string.
   This function is designed to cleanup user input (e.g. to avoid XSS attacks).
   Returned string should be free'ed after use. */

#endif /* JSHELPER_H */
