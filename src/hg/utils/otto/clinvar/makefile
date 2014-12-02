PREFIX=/hive/data/outside/otto/clinvar/
install: 
	pylint -E clinVarToBed
	cp clinVarToBed clinvar.as doUpdate.sh ${PREFIX}
clean:
	rm -f *.bed *.bb *.gz
