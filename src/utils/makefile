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
	detab \
	dnsInfo \
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
	faTrans \
	fixCr \
	gffPeek \
	htmlPics \
	jkUniq \
	lineCount \
	newProg \
	nibFrag \
	nt4Frag \
	pslToXa \
	randomLines \
	rmFaDups \
	scaffoldFaToAgp \
	scrambleFa \
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
	undupFa \
	upper \
	venn \
	wordLine

BAD_DIRS= \
	ccCpLock \
	geneStarts \
	jimgrep \
	jkShell \
	cCp
