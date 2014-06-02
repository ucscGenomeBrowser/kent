/* Copyright (C) 2008 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

void cuttersDrawAt(struct track *tg, void *item,
	struct hvGfx *hvg, int xOff, int y, double scale, 
		   MgFont *font, Color color, enum trackVisibility vis);

void cuttersLoad(struct track *tg);

struct track *cuttersTg();
