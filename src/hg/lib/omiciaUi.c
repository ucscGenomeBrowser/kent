/* omiciaUi.c - disclaimer notice */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "omiciaUi.h"
#include "common.h"
#include "cart.h"
#include "cheapcgi.h"

static char const rcsid[] = "$Id$";

void omiciaDisclaimer ()
/* displays page with disclaimer forwarding query string that got us here */
{
struct cgiVar *cv, *cvList = cgiVarList();

cartHtmlStart("Omicia Disclaimer");
printf("</FORM>"); /* end TrackHeaderForm */
printf("<TABLE WIDTH=\"%s\" BGCOLOR=\"#888888\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>", "100%");
printf("<TABLE BGCOLOR=\"fffee8\" WIDTH=\"%s\"  BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\"><TR><TD>", "100%");
printf("<TABLE BGCOLOR=\"D9E4F8\" BACKGROUND=\"../../images/hr.gif\" WIDTH=%s><TR><TD>", "100%");
printf("<FONT SIZE=\"4\"><b>&nbsp;Omicia Disclaimer </B></FONT> ");
printf("</TD></TR></TABLE>");
printf("<TABLE BGCOLOR=\"fffee8\" WIDTH=\"%s\" CELLPADDING=0><TR><TH HEIGHT=10></TH></TR>", "100%");
printf("<TR><TD WIDTH=10>&nbsp;</TD><TD>");
printf("<b>Disclaimer</b>\n");
printf("<p>\n");
printf("Note: OMIM is intended for use primarily ");
printf("by physicians and other professionals concerned with genetic disorders, ");
printf("by genetics researchers, and by advanced students in science and medicine. ");
printf("While the OMIM database is open to the public, users seeking information ");
printf("about a personal medical or genetic condition are urged to consult with a ");
printf("qualified physician for diagnosis and for answers to personal questions.\n<P>\n");
printf("Because many OMIM records refer to multiple gene names, or syndromes not ");
printf("tightly mapped to individual genes, the associations in this track should ");
printf("be treated with skepticism and any conclusions based on them should be ");
printf("carefully scrutinized using independent resources.\n");
printf("<p>\n");
printf("<br>\n");
printf("<FORM ACTION=\"%s\" METHOD=POST>\n", cgiScriptName());
for (cv = cvList; cv != NULL; cv = cv->next)
    {
    cgiContinueHiddenVar(cv->name);
    }
printf("\n");
cgiMakeButtonWithMsg("omiciaDisclaimer", "Agree", NULL);
printf("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;");
cgiMakeButtonWithMsg("omiciaDisclaimer", "Disagree", NULL);
printf("</FORM>\n");
printf("</td></tr></table></td></tr></table></td></tr></table>");
cartHtmlEnd();
exit(0);
}


