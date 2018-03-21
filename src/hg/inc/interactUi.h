/* interact track UI */

/* Copyright (C) 2018 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef INTERACT_UI_H
#define INTERACT_UI_H

#define INTERACT_HEIGHT "heightPer"
#define INTERACT_MINHEIGHT 20
#define INTERACT_MAXHEIGHT 300
#define INTERACT_DEFHEIGHT "200"
#define INTERACT_MINSCORE "minScore"
#define INTERACT_DEFMINSCORE 0

#define INTERACT_DIRECTIONAL    "interactDirectional"

#define INTERACT_DRAW           "draw"
#define INTERACT_DRAW_LINE      "line"
#define INTERACT_DRAW_ELLIPSE   "ellipse"
#define INTERACT_DRAW_CURVE     "curve"
#define INTERACT_DRAW_DEFAULT   INTERACT_DRAW_CURVE

void interactCfgUi(char *database, struct cart *cart, struct trackDb *tdb, char *track,
                        char *title, boolean boxed);
/* Configure interact track type */

boolean interactUiDirectional(struct trackDb *tdb);
/* Determine if interactions are directional */

#endif
