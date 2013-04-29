kentSrc = ../..
A = faTrimRead
include $(kentSrc)/inc/userApp.mk
L += -lm

test:: test1
%:	input/%.fa
	faTrimRead input/$@.fa input/$@.qual output/$@L.fa output/$@.lft -lower -lineSize=70
	diff expected/$@L.fa output/$@L.fa
	rm output/$@L.fa
	mkdir -p output
	faTrimRead input/$@.fa input/$@.qual output/$@.fa output/$@.lft
	diff expected/$@.fa output/$@.fa
	rm output/$@.fa
