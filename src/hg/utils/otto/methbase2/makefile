PREFIX=/hive/data/outside/otto/methbase2/

# methbaseDownload is installed here for the record and as the source for the
# hgdownload copy, but it does NOT run here: methbaseOtto.sh ssh's to
# hgdownload and runs it there, against the mirror, so the data never crosses
# to hgwdev. After "make install", push it to hgdownload with "make installRemote".
DOWNLOAD_HOST=qateam@hgdownload
REMOTE_DIR=/mirrordata/hubs/methbase/

install:
	bash -n methbaseOtto.sh
	python3 -c 'import ast; ast.parse(open("methbaseDownload").read())'
	python3 -c 'import ast; ast.parse(open("processMethbaseHub").read())'
	mkdir -p ${PREFIX}
	cp methbaseOtto.sh methbaseDownload processMethbaseHub ${PREFIX}

# separate target on purpose: this writes to another machine, so it is never a
# side effect of "make install"
installRemote:
	scp methbaseDownload ${DOWNLOAD_HOST}:${REMOTE_DIR}
