kentSrc = ../../..
A = hgLoadWiggle hgWiggle wigEncode

include ${kentSrc}/inc/userApp.mk

perl_progs = fixStepToBedGraph varStepToBedGraph
default:: ${perl_progs:%=%.pl}

clean::
	rm -f test_data/chr1_0.wib test_data/chr1_0.wig test_data/chr1_1.wib \
	test_data/chr1_1.wig test_data/chr1_2.wib test_data/chr1_2.wig \
	test_data/chr1_begin.ascii test_data/chr1_end.ascii \
	test_data/chr1_middle.ascii test_data/chrM.ascii test_data/chrM.wib \
	test_data/chrM.wig test_data/wiggle.tab

user_loader_dir = ${CGI_BIN}-${USER}/loader
alpha_loader_dir = ${CGI_BIN}/loader
beta_loader_dir = ${CGI_BIN}-beta/loader

cgi:: ${A:%=${user_loader_dir}/%}

${user_loader_dir}/%: %
	@{MKDIR} $(dir $@)
	cp -f $< $@

alpha:: 

