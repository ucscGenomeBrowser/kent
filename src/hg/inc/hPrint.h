/* hPrint - turning html printing on and off, which is useful
 * when postscript and PDF images are being drawn  */

#ifndef HPRINT_H
#define HPRINT_H

#include "common.h"
#include "errabort.h"
#include "cheapcgi.h"

void hPrintf(char *format, ...);
/* Printf that can be suppressed if not making html. */

void hvPrintf(char *format, va_list args);
/* Suppressable variable args printf. Check for write error so we can
 * terminate if http connection breaks. */

void hPrintDisable();
/* turn html printing off */

void hPrintEnable();
/* turn html printing on */

void hPrintNonBreak(char *s);
/* Print out string but replace spaces with &nbsp; */

void hPuts(char *string);
/* Puts that can be suppressed if not making html. */

void hPutc(char c);
/* putc that can be suppressed if not making html. */

void hWrites(char *string);
/* Write string with no '\n' if not suppressed. */

void hButton(char *name, char *label);
/* Write out button if not suppressed. */

void hOnClickButton(char *command, char *label);
/* Write out push button if not suppressed. */

void hTextVar(char *varName, char *initialVal, int charSize);
/* Write out text entry field if not suppressed. */

void hIntVar(char *varName, int initialVal, int maxDigits);
/* Write out numerical entry field if not supressed. */

void hDoubleVar(char *varName, double initialVal, int maxDigits);
/* Write out numerical entry field if not supressed. */

void hCheckBox(char *varName, boolean checked);
/* Make check box if not suppressed. */

void hCheckBoxJS(char *varName, boolean checked, char *javascript);
/* Make check box if not suppressed, with javascript. */

void hDropList(char *name, char *menu[], int menuSize, char *checked);
/* Make a drop-down list with names if not suppressed. */

void hDropListClass(char *name, char *menu[], int menuSize, char *checked,
                        char *class);
/* Make a drop-down list with names if not suppressed, using specified class. */

void hDropListClassWithStyle(char *name, char *menu[], int menuSize, 
                                char *checked, char *class, char *style);
/* Make a drop-down list with names if not suppressed, 
 * using specified class and style */

void hPrintComment(char *format, ...);
/* Function to print output as a comment so it is not seen in the HTML
 * output but only in the HTML source. */

#endif /* HPRINT_H */
