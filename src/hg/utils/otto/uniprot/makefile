PREFIX=/hive/data/outside/otto/uniprot/
PARSER=../../../../utils/uniprotToTab

# Everything doUniprot calls at run time must be listed here, or the copy that cron runs
# is not under version control. refs #38300
FILES=*.sh *.as doUniprot mapUniprot_doBlast pslProtCnv trackDb.template.txt README.txt ${PARSER}

# Not "pylint -E": every pylint on hgwdev is itself pinned to a python that no longer
# exists, so "make install" used to abort on its first line.
SYNTAXCHECK=python3 -c 'import ast,sys; [ast.parse(open(f).read(), f) for f in sys.argv[1:]]'

install:
	${SYNTAXCHECK} doUniprot ${PARSER}
	-pylint -E doUniprot ${PARSER} # best effort, do not stop the install if pylint is broken
	rsync -avpu ${FILES} ${PREFIX} # never overwrite newer files

# rsync -u above refuses to overwrite a file that is newer in ${PREFIX} than in the tree,
# so a hand edit of the live copy wins silently and forever. Run this to see what has
# drifted apart.
diff:
	@for f in ${FILES}; do \
	    for g in $$f; do \
	        cmp -s $$g ${PREFIX}`basename $$g` || echo "DIFFERS: $$g"; \
	    done; \
	done
