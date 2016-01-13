all::
	expMatrixToJson matrix.mx testClusters
	mv testClusters* ~/public_html/tests

meta::
	cd /cluster/home/ceisenhart/kent/src/hg/cirm/cdw/wrangle/addMetaDataToJson/tests && make 
	cd ~/kent/src/hg/expMatrixToJson/tests

full::
	${all} ${meta}
