kentSrc = ../../../..
PREFIX=/hive/data/outside/otto/clinvar/
install: 
	pylint -E clinVarToBed
	cp clinVarToBed clinvar.as doUpdate.sh ${PREFIX}
	cp ${kentSrc}/hg/lib/clinvarSubBB.as ${kentSrc}/hg/lib/clinvarSubLolly.as ${PREFIX}
	cp assignColors assignScore clinvarSubLolly makeFranklin ${PREFIX}
clean:
	rm -f *.bed *.bb *.gz
