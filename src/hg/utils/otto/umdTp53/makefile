PREFIX=/hive/data/outside/otto/umdTp53

install:
	cp umdTp53.as umdTp53ToBed.py download.sh checkAndLoad.sh doUmdTp53Update.sh ${PREFIX}/
	chmod +x ${PREFIX}/download.sh ${PREFIX}/checkAndLoad.sh ${PREFIX}/doUmdTp53Update.sh
	chmod +x ${PREFIX}/umdTp53ToBed.py

clean:
	rm -f ${PREFIX}/*.bed ${PREFIX}/*.bb ${PREFIX}/*.ix ${PREFIX}/*.ixx
