# Build all directories in utils

BIN_DIR = $(HOME)/bin

all:    scripts
	@for D in $(USE_DIRS) x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && $(MAKE) ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	done

scripts:
	@for F in $(SCRIPTS); do \
	    chmod +x $$F; \
	    echo cp $$F $(BIN_DIR); \
	    cp $$F $(BIN_DIR); \
	 done

test:
	cd faToTwoBit && ${MAKE} test
	cd twoBitToFa && ${MAKE} test

clean:
	- find . -name \*.o -print | xargs --no-run-if-empty rm

SCRIPTS = \
        iSync

# List of directories to build
USE_DIRS = $(DIRS)

DIRS = \
	aNotB \
	addCols \
	ave \
	aveCols \
	calc \
	catDir \
	catUncomment \
	ccCp \
	chopFaLines \
	countChars \
	convolve \
	detab \
	endsInLf \
	faCmp \
	faCount \
	faFilterN \
	faFlyBaseToUcsc \
	faFrag \
	faNcbiToUcsc \
	faNoise \
	faOneRecord \
	faRc \
	faSimplify \
	faSize \
	faSomeRecords \
	faSplit \
	faToNib \
	faToTwoBit \
	faTrans \
	faTrimPolyA \
	fixCr \
	gffPeek \
	htmlPics \
	jkUniq \
	lineCount \
	newProg \
	nibFrag \
	nibSize \
	nt4Frag \
	pslToXa \
	randomLines \
	rmFaDups \
	scaffoldFaToAgp \
	scrambleFa \
	spacedToTab \
	splitFile \
	stringify \
	subChar \
	subs \
	tableSum \
	textHist2 \
	textHistogram \
	tickToDate \
	toLower \
	toUpper \
	twoBitToFa \
	undupFa \
	upper \
	venn \
	wigAsciiToBinary \
	wordLine

BAD_DIRS= \
	ccCpLock \
	geneStarts \
	jimgrep \
	jkShell \
	dnsInfo \
	cCp
