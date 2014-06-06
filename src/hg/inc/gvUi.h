/* gvUi.h - enums and char arrays for gv UI features */

/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef GENOMEVARUI_H
#define GENOMEVARUI_H

extern char *gvTypeLabel[];
extern char *gvTypeString[];
extern char *gvTypeDbValue[];
extern int gvTypeSize;

extern char *gvLocationLabel[];
extern char *gvLocationString[];
extern char *gvLocationDbValue[];
extern int gvLocationSize;

extern char *gvSrcString[];
extern char *gvSrcDbValue[];
extern int gvSrcSize;

extern char *gvAttrTypeKey[];
extern char *gvAttrTypeDisplay[];
extern char *gvAttrCategory[];
extern int gvAttrSize;

extern char *gvAccuracyString[];
extern char *gvAccuracyLabel[];
extern unsigned gvAccuracyDbValue[];
extern int gvAccuracySize;

extern char *gvFilterDAString[];
extern char *gvFilterDALabel[];
extern char *gvFilterDADbValue[];
extern int gvFilterDASize;

extern char *gvColorLabels[];
extern int gvColorLabelSize;

extern char *gvColorTypeLabels[];
extern char *gvColorTypeStrings[];
extern char *gvColorTypeDefault[];
extern char *gvColorTypeBaseChangeType[];
extern int gvColorTypeSize;
extern char *gvColorDALabels[];
extern char *gvColorDAStrings[];
extern char *gvColorDADefault[];
extern char *gvColorDAAttrVal[];
extern int gvColorDASize;

void gvDisclaimer ();
/* displays page with disclaimer forwarding query string that got us here */

#endif
