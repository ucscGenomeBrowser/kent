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

void jsCreateHiddenForm(struct cart *cart, char *scriptName,
	char **vars, int varCount);
/* Create a hidden form with the given variables.  Must be
 * called. */

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

