kentSrc = ..
include ../src/inc/common.mk
rsyncOpts = --whole-file --times --recursive --omit-dir-times --cvs-exclude --exclude='makefile' --exclude='cpAndLinkToResourceFiles.pl' --exclude='style/*'

# Find all .md files in tutorials/ and subdirectories
DOCS := $(shell find . -name '*.md' -type f | sed 's|^\./||')

user:
	${MAKE} doInstall destDir=${DOCUMENTROOT}-${USER}
alpha:
	${MAKE} doInstall destDir=${DOCUMENTROOT}
beta:
	${MAKE} doInstall destDir=${DOCUMENTROOT}-beta
	rsync ${rsyncOpts} ${DOCUMENTROOT}-beta/docs/ qateam@hgwbeta:${DOCUMENTROOT}/docs/

doInstall: $(DOCS)
	@for mdfile in $(DOCS); do \
		htmlfile=$$(echo "$$mdfile" | sed 's|\.md$$|.html|'); \
		dirpart=$$(dirname "$$htmlfile"); \
		if [ "$$dirpart" = "." ]; then \
			targetdir=${destDir}/docs; \
			depth=1; \
		else \
			targetdir=${destDir}/docs/$$dirpart; \
			depth=$$(echo "$$htmlfile" | tr -cd '/' | wc -c); \
			depth=$$((depth + 1)); \
		fi; \
		rootpath=$$(printf '../%.0s' $$(seq 1 $$depth)); \
		rootpath=$${rootpath%/}; \
		mkdir -p "$$targetdir"; \
		echo "Processing $$mdfile -> $$targetdir/$$(basename $$htmlfile) (ROOT=$$rootpath)"; \
		rm -f "$$targetdir/$$(basename $$htmlfile).tmp"; \
		pandoc --ascii -f markdown-smart -t html5 -o "$$targetdir/$$(basename $$htmlfile).tmp" "$$mdfile" --template staticPage.html -t staticPage.lua --lua-filter GBpandocHTMLrules.lua -V ROOT="$$rootpath"; \
		mv -f "$$targetdir/$$(basename $$htmlfile).tmp" "$$targetdir/$$(basename $$htmlfile)"; \
		chmod a+x "$$targetdir/$$(basename $$htmlfile)"; \
	done

testPandoc:
	@printf "Test the following path and commands look ok\n"
	@printf "pandoc path: %s\n" "$$(which pandoc)"
	@printf "Found markdown files:\n"
	@for f in $(DOCS); do printf "  %s -> %s\n" "$$f" "$$(echo $$f | sed 's|\.md$$|.html|')"; done
