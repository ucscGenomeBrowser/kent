# Build all directories in utils

all:
	@for D in $(USE_DIRS) x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && $(MAKE) ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	    done

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
	detab \
	dnsInfo \
	endsInLf \
	faCmp \
	faCount \
	faFilterN \
	faFrag \
	faNcbiToUcsc \
	faNoise \
	faOneRecord \
	faRc \
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
	countChars \
	geneStarts \
	jimgrep \
	jkShell \
	cCp
