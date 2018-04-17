PREFIX=/hive/data/outside/otto/uniprot/
install: 
	pylint -E doUniprot
	rsync -avpu *.as doUniprot makeUniProtPsl.sh mapUniprot_doBlast ${PREFIX} # never overwrite newer files
