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

/* trackDb settings:
      interactDirectional on          use dashes for reverse direction
      interactDirectional offsetSource  use dashes for reverse direction, and offset source
      interactDirectional offsetTarget  use dashes for reverse direction, and offset target
*/
#define INTERACT_DIRECTIONAL    "interactDirectional"
#define INTERACT_OFFSET_SOURCE  "offsetSource"
#define INTERACT_OFFSET_TARGET  "offsetTarget"

/* setting to show interactions with peak up (hill, not valley) */
#define INTERACT_UP             "interactUp"

/* Cart variables */

#define INTERACT_DIRECTION_DASHES "dashes"
#define INTERACT_DIRECTION_DASHES_DEFAULT TRUE

#define INTERACT_DRAW           "draw"
#define INTERACT_DRAW_LINE      "line"
#define INTERACT_DRAW_ELLIPSE   "ellipse"
#define INTERACT_DRAW_CURVE     "curve"
#define INTERACT_DRAW_DEFAULT   INTERACT_DRAW_CURVE

#define INTERACT_ENDS_VISIBLE     "endsVisible"
#define INTERACT_ENDS_VISIBLE_TWO "two"
#define INTERACT_ENDS_VISIBLE_ONE "one"
#define INTERACT_ENDS_VISIBLE_ANY "any"
#define INTERACT_ENDS_VISIBLE_DEFAULT INTERACT_ENDS_VISIBLE_ANY


void interactCfgUi(char *database, struct cart *cart, struct trackDb *tdb, char *track,
                        char *title, boolean boxed);
/* Configure interact track type */

boolean interactUiDirectional(struct trackDb *tdb);
/* Determine if interactions are directional */

char *interactUiOffset(struct trackDb *tdb);
/* Determine whether to offset source or target (or neither if NULL) */

#endif
