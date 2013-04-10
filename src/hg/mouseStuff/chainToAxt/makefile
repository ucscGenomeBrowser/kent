kentSrc = ../../..
A = chainToAxt
include ${kentSrc}/inc/userApp.mk

test:: chrM
%:	input/%.chain
	${MKDIR} output
	chainToAxt input/$@.chain /gbdb/hg16/nib /gbdb/mm3/nib output/$@.axt
	diff output/$@.axt expected/$@.axt
	rm output/$@.axt

clean::
	rm -fr output
