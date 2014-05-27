/* geneCheckWidget - output HTML tables to display geneCheck or
 * geneCheckDetails with browser links */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "geneCheckWidget.h"
#include "geneCheck.h"
#include "geneCheckDetails.h"
#include "cart.h"
#include "hCommon.h"

static void prTracksAnchor(struct cart *cart, char *chrom, int start, int end,
                           char *target, char *fmt, ...)
/* print an anchor to hgTracks, printf text content. Target can be
 * NULL */
{
va_list args;
va_start(args, fmt);
printf("<A HREF=\"%s?%s&position=%s:%d-%d\"", hgTracksName(), cartSidUrlString(cart),
       chrom, start, end);
if (target != NULL)
    printf(" target=%s", target);
printf(">");
vprintf(fmt, args);
printf("</A>");
va_end(args);
}

static void prNameValCellStr(char *name, char *value, char *okVal, char *probClass)
/* output a string name/value pair as adjacent cells in a table, set class to
 * probClass if value is not okVal  */
{
printf("<TR>");
if (!sameString(value, okVal))
    printf("<TH CLASS=\"%s\">%s<TD CLASS=\"%s\">%s", probClass, name, probClass, value);
else
    printf("<TH>%s<TD>%s", name, value);
printf("</TR>\n");
}

static void prNameValCellInt(char *name, int value, int okVal, char *probClass)
/* output a int name/value pair as adjacent cells in a table, set class to
 * probClass if value is not okVal  */
{
printf("<TR>");
if (value != okVal)
    printf("<TH CLASS=\"%s\">%s<TD CLASS=\"%s\">%d", probClass, name, probClass, value);
else
    printf("<TH>%s<TD>%d", name, value);
printf("</TR>\n");
}

void geneCheckWidgetSummary(struct geneCheck *gc, char *tblClass, char *caption)
/* write a table with result summary for one gene; caption maybe NULL */
{
printf("<TABLE class=\"%s\">\n", tblClass);
if (caption != NULL)
    printf("<CAPTION>%s</CAPTION>\n", caption);
printf("<TBODY>\n");
prNameValCellStr("frame", gc->frame, "ok", "errorBg");
prNameValCellStr("start", gc->start, "ok", "warnBg");
prNameValCellStr("stop", gc->stop, "ok", "warnBg");
prNameValCellInt("ORF stops", gc->orfStop, 0, "errorBg");
prNameValCellInt("CDS gaps", gc->cdsGap-gc->cdsMult3Gap, 0, "errorBg");
prNameValCellInt("CDS mult3 gaps", gc->cdsMult3Gap, 0, "warnBg");
prNameValCellInt("CDS bad splices", gc->cdsSplice, 0, "errorBg");
prNameValCellInt("UTR gaps", gc->utrGap, 0, "warnBg");
prNameValCellInt("UTR bad splices", gc->utrSplice, 0, "errorBg");
printf("</TBODY></TABLE>\n");
}

static void printSpliceInfo(struct cart *cart, struct geneCheck *gc, struct geneCheckDetails *gcd,
                            char *target)
/* print details of splice problems, with links */
{
// info is like: CC..AG
static int context = 3;
    
// get correct splice site locations
int start5, end5, start3, end3;
if (gc->strand[0] == '+')
    {
    start5= gcd->chrStart-context;
    end5 = gcd->chrStart+2+context;
    start3 = (gcd->chrEnd-2)-context;
    end3 = (gcd->chrEnd-2)+context;
    }
else
    {
    start5 = (gcd->chrEnd-2)-context;
    end5 = (gcd->chrEnd-2)+context;
    start3= gcd->chrStart-context;
    end3 = gcd->chrStart+2+context;
    }
prTracksAnchor(cart, gcd->chr, start5, end5, target, "%.2s", gcd->info);
printf("..");
prTracksAnchor(cart, gcd->chr, start3, end3, target, "%.2s", gcd->info+4);
}

static void dispDetailsRow(struct cart *cart, struct geneCheck *gc,
                           struct geneCheckDetails *gcd, char *target)
/* display a row of geneCheck details row */
{
/* get location and a little context */
static int context = 20;
int start = (gcd->chrStart < context) ? 0 : gcd->chrStart-context;
int end = gcd->chrEnd+context;
printf("<TR><TD>%s", gcd->problem);
printf("<TD>");
if (sameString(gcd->problem, "badCdsSplice") || sameString(gcd->problem, "badUtrSplice"))
    printSpliceInfo(cart, gc, gcd, target);
else
    printf("%s", gcd->info);
printf("<TD>");
if (gcd->chrStart < gcd->chrEnd)
    prTracksAnchor(cart, gcd->chr, start, end, target,
                   "%s:%d-%d", gcd->chr, gcd->chrStart, gcd->chrEnd);
printf("</TR>\n");
}

void geneCheckWidgetDetails(struct cart *cart, struct geneCheck *gc,
                            struct geneCheckDetails *gcdList, char *tblClass,
                            char *caption, char *target)
/* display gene-check details; caption maybe NULL.  target is target of
 * links if not NULL.*/
{
struct geneCheckDetails *gcd;
printf("<TABLE class=\"%s\">\n", tblClass);
if (caption != NULL)
    printf("<CAPTION>%s</CAPTION>\n", caption);
printf("<THEAD>\n");
printf("<TR><TH>Problem<TH>Info<TH>Location</TR>\n");
printf("</THEAD>\n");
printf("<TBODY>\n");
for (gcd = gcdList; gcd != NULL; gcd = gcd->next)
    dispDetailsRow(cart, gc, gcd, target);
printf("</TBODY></TABLE>\n");
}
