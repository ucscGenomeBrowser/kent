#C_OPT=-O3 -funroll-loops -march=pentiumpro
#C_OPT=-O3 -march=pentiumpro
#C_DEBUG=-W -Wall -g
#C_DEF=-DUSE_OBSTACK

BZ_SRCS=bz_main.c bz_align.c bz_extend.c bz_chain.c bz_dna.c bz_print.c \
	bz_table.c bz_census.c bz_hit19.c util.c seq.c args.c edit.c dna.c \
	charvec.c nib.c

SR_SRCS=strip_rpts.c util.c seq.c charvec.c nib.c
RC_SRCS=revcomp.c util.c seq.c charvec.c nib.c
RR_SRCS=restore_rpts.c util.c seq.c charvec.c nib.c dna.c
RT_SRCS=nrepeats_tag.c util.c

blastz: $(BZ_SRCS)
	$(CC) $(CFLAGS) $(BZ_SRCS) \
		$(M) \
		-lm \
		-o $@

strip_rpts : $(SR_SRCS)
	$(CC) $(CFLAGS) $(SR_SRCS) -o $@

revcomp : $(RC_SRCS)
	$(CC) $(CFLAGS) $(RC_SRCS) -o $@

restore_rpts : $(RR_SRCS)
	$(CC) $(CFLAGS) $(RR_SRCS) -o $@

nrepeats_tag : $(RT_SRCS)
	$(CC) $(CFLAGS) $(RT_SRCS) -o $@

# This test determines the additional regions aligned, for one very small
# dataset, using an approach to human-mouse alignments pioneered by Arian Smit.
# We start with human (softmasked) and mouse (softmasked) and the corresponding
# RepeatMasker ".out" files, human.out and mouse.out. Lineage-specific are
# removed, the sequences are aligned, and the blastz output files are adjusted
# as if run on the original sequences.

test : blastz strip_rpts revcomp restore_rpts nrepeats_tag
	./nrepeats_tag -primate human.out | grep -v Simple | grep -v RNA \
		> hum.new.rpts
	./nrepeats_tag -rodent mouse.out | grep -v Simple | grep -v RNA \
		> mus.new.rpts
	./strip_rpts human hum.new.rpts > hum.stripped
	./strip_rpts mouse mus.new.rpts > mus.stripped
	./blastz hum.stripped mus.stripped B=0 | \
		./restore_rpts - \
		human "\"human\" 1 10001" \
		mouse "\"mouse\" 1 10000" \
		hum.new.rpts mus.new.rpts \
		> restored.blastz
	./revcomp mus.stripped > mus.stripped.rev
	./blastz hum.stripped mus.stripped.rev B=0 | \
		./restore_rpts - \
		human "\"human\" 1 10001" \
		mouse "\"mouse-\" 1 10000" \
		hum.new.rpts mus.new.rpts \
		reverse >> restored.blastz

	./blastz human mouse > regular.blastz
	wc regular.blastz restored.blastz

README : README.tex
	latex README.tex
	dvips -Ppdf -G0 -o README.ps README.dvi
	ps2pdf README.ps README.pdf


tarfile : README
	mkdir blastz-source
	cp README.pdf Makefile *.c *.h human human.out mouse mouse.out \
		blastz-source
	tar -cf - blastz-source | gzip > blastz.tar.gz
	rm -r blastz-source

dist : tarfile
	cp blastz.tar.gz /home/nog/httpd/html/dist
