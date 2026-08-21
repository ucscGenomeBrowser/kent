/* htmlSanitize - reduce a piece of HTML that came from outside to an allowlist of
 * elements, attributes and style properties. */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef HTMLSANITIZE_H
#define HTMLSANITIZE_H

#ifndef COMMON_H
#include "common.h"
#endif

char *htmlSanitize(char *html);
/* Return a cloned copy of html holding only allowlisted elements, attributes and style
 * properties.  An element that is not on the keep list loses its tag but keeps its text,
 * so a whole pasted document comes out as the article it was meant to be.  A few elements
 * that carry no text for a reader, script and style and form among them, go away with
 * their contents.  Returns NULL if html is NULL.  Never aborts, whatever the input. */

char *htmlSanitizeReport(char *html, struct slName **retRemoved);
/* Like htmlSanitize, and if retRemoved is not NULL also return a list of one-line messages
 * naming each kind of thing that was removed.  The list is NULL when nothing was removed. */

#endif /* HTMLSANITIZE_H */
