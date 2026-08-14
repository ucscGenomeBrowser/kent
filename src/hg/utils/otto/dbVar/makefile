PREFIX=/hive/data/outside/otto/dbVar/
# knownFiles.txt is intentionally not installed from the tree -- it's a
# per-deployment manifest of acknowledged hub filenames that lives under
# ${PREFIX} alongside lastUpdate and the release/ data, and is edited in
# place when a new NCBI hub file is approved for exposure.
install:
	cp dbVarWrapper.sh checkDbVar.sh checkNstd175.sh giabSv.as processNstd175.py ${PREFIX}
clean:
	rm -f *.bed *.bb *.gz
