/* hPrint - turning html printing on and off, which is useful
 * when postscript and PDF images are being drawn  */

/* Copyright (C) 2010 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef HPRINT_H
#define HPRINT_H

#include "common.h"
#include "errAbort.h"
#include "cheapcgi.h"

void hPrintf(char *format, ...)
/* Printf that can be suppressed if not making html. */
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
;

void hvPrintf(char *format, va_list args);
/* Suppressable variable args printf. Check for write error so we can
 * terminate if http connection breaks. */

boolean hPrintStatus();
/* is html printing on or off ?
   return TRUE for print is on, FALSE for printing is off */

void hPrintDisable();
/* turn html printing off */

void hPrintEnable();
/* turn html printing on */

void hPrintNonBreak(char *s);
/* Print out string but replace spaces with &nbsp; */

void hPrintEncodedNonBreak(char *s);
/* Print with htmlEncode and non-break */

void hPuts(char *string);
/* Puts that can be suppressed if not making html. */

void hPutc(char c);
/* putc that can be suppressed if not making html. */

void hWrites(char *string);
/* Write string with no '\n' if not suppressed. */

void hButton(char *name, char *label);
/* Write out button if not suppressed. */

void hButtonWithMsg(char *name, char *label, char *msg);
/* Write out button with msg if not suppressed. */

void hButtonWithOnClick(char *name, char *label, char *msg, char *onClick);
/* Write out button with onclick javascript if not suppressed. */

void hOnClickButton(char *id, char *command, char *label);
/* Write out push button if not suppressed. */

void hButtonMaybePressed(char *name, char *label, char *msg, char *onClick, boolean pressed);
/* If not suppresed, write out button optionally with onclick javascript, message and 
   styled to indicate modal state (button pressed)
 */

void hTextVar(char *varName, char *initialVal, int charSize);
/* Write out text entry field if not suppressed. */

void hIntVar(char *varName, int initialVal, int maxDigits);
/* Write out numerical entry field if not supressed. */

void hDoubleVar(char *varName, double initialVal, int maxDigits);
/* Write out numerical entry field if not supressed. */

void hCheckBox(char *varName, boolean checked);
/* Make check box if not suppressed. */

void hDropList(char *name, char *menu[], int menuSize, char *checked);
/* Make a drop-down list with names if not suppressed. */

void hDropListClass(char *name, char *menu[], int menuSize, char *checked,
                        char *class);
/* Make a drop-down list with names if not suppressed, using specified class. */

void hDropListClassWithStyle(char *name, char *menu[], int menuSize, 
                                char *checked, char *class, char *style);
/* Make a drop-down list with names if not suppressed, 
 * using specified class and style */

void hPrintComment(char *format, ...)
/* Function to print output as a comment so it is not seen in the HTML
 * output but only in the HTML source. */
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
;

#endif /* HPRINT_H */
