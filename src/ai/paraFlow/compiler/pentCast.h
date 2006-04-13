/* pentCast - generate pentium code to cast from one type to another */

#ifndef PENTCAST_H
#define PENTCAST_H

void pentCast(struct pfCompile *pfc, struct isx *isx, struct dlNode *nextNode, 
	struct pentCoder *coder);
/* Create code for a cast instruction */

#endif /* PENTCAST_H */

