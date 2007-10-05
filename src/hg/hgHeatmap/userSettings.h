/* hgNear.h - interfaces to plug columns into hgNear.  The must important
 * thing here is the column structure. */

#ifndef USERFEATURES_H
#define USERFEATURES_H

#include "cart.h"

#include "jksql.h"


/* Global Variables */

#define colSaveSettingsPrefix "near_colUserSet_"  /* Prefix for column sets. */
    /* Underbars on this one for sake of javascript. */
#define saveCurrentConfName "near.do.colUserSet.save"   /* Save column set. */
#define savedCurrentConfName "near.do.colUserSet.saved" /* Saved column set. */
#define useSavedConfName "near.do.colUserSet.used"      /* Use column set. */



struct userSettings 
/* This helps us the user a set of cart variables, defined
 * by settingsPrefix.  It allows the user to save different
 * sets of settings under different names. */
{
  struct userSettings *next;/* Next in list. */
  struct cart *cart;/* Associated cart. */
  char *formTitle;/* Heading of settings save form. */
  char *nameVar;/* Variable for name on give-it-a-name page. */
  char *savePrefix;/* Prefix for where we store settings.
		    * We store at savePrefix.name where name
		    * is taken from listVar above. */
  char *formVar;/* Submit button on give-it-a-name page. */
  char *listDisplayVar;/* Variable name for list display control. */
  struct slName *saveList;/* List of variables to save. */
};

struct userSettings *userSettingsNew(
    struct cart *cart,/* Persistent variable cart. */
    char *formTitle,/* Heading of settings save form. */
    char *formVar,/* Name of button variable on save form. */
    char *localVarPrefix);  /* Prefix to use for internal cart variables. 
			     * No periods allowed because of javascript. */
/* Make new object to help manage sets of user settings. */

void userSettingsCaptureVar(struct userSettings *us, char *varName);
/* Add a single variable to list of variables to capture. */

void userSettingsCapturePrefix(struct userSettings *us, char *prefix);
/* Capture all variables that start with prefix. */

boolean userSettingsAnySaved(struct userSettings *us);
/* Return TRUE if any user settings are saved. */

void userSettingsUseNamed(struct userSettings *us, char *setName);
/* Use named collection of settings. */

void userSettingsUseSelected(struct userSettings *us);
/* Use currently selected user settings. */

void userSettingsSaveForm(struct userSettings *us);
/* Put up controls that let user name and save the current
 * set. */

void userSettingsLoadForm(struct userSettings *us);
/* Put up controls that let user name and save the current
 * set. */

boolean userSettingsProcessForm(struct userSettings *us);
/* Handle button press in userSettings form. 
 * If this returns TRUE then form is finished processing
 * and you can call something to make the next page. */

void userSettingsDropDown(struct userSettings *us);
/* Display list of available saved settings . */



#endif /* USERFEATURES_H */
