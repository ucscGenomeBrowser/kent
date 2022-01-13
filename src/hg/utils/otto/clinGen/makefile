PREFIX=/hive/data/outside/otto/clinGen/
DOSAGE=clinGenDosage
CNV=clinGenCnv

installValidity:
	cp makeGeneValidity.sh ${PREFIX}

installCnv:
	cp processClinGenCnv.py makeCnv.sh ${PREFIX}
	cp clinGenCnv.as ${PREFIX}/${CNV}

installDosage: 
	cp processClinGenDosage.py makeDosage.sh ${PREFIX}
	cp clinGenDosage{Haplo,Triplo}.as ${PREFIX}/${DOSAGE}

install: installValidity installCnv installDosage
	cp clinGenWrapper.sh checkClinGen.sh ${PREFIX}
