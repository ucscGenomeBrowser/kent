#
#	product/makefile - create image for product CD-ROM
#
#	This makefile expects to be used on hgwdev so it can have
#	access to common html files from apache root to copy into
#	this hierarchy, and access to /gbdb links.
#
include ../inc/common.mk

#	This version string needs to be raised for a release

VER=33

DESTDIR=.
install_dir=${DESTDIR}/install_image

all::	INST_DIR COPYRIGHT.txt README.building.source.txt \
	README.cvs.access.txt README.install.txt \
	README.mysql.setup.txt version source_clean source_tar \
	source_built example gbdb

INST_DIR:
	@if [ ! -d ${install_dir} ]; then \
		${MKDIR} "${install_dir}"; \
	fi

version:
	rm -f ${install_dir}/Version_*
	touch ${install_dir}/Version_${VER}.`date -I`

COPYRIGHT.txt:	COPYRIGHT INST_DIR
	sed -e "s/DATE_STAMP/`date -I`/" COPYRIGHT | \
	unix2dos > ${install_dir}/$@
	chmod 644 ${install_dir}/$@

README.building.source.txt:	README.building.source INST_DIR
	unix2dos -n README.building.source ${install_dir}/$@
	chmod 644 ${install_dir}/$@

README.cvs.access.txt:	README.cvs.access INST_DIR
	unix2dos -n README.cvs.access ${install_dir}/$@
	chmod 644 ${install_dir}/$@

README.install.txt:	README.install INST_DIR
	unix2dos -n README.install ${install_dir}/$@
	chmod 644 ${install_dir}/$@

README.mysql.setup.txt: README.mysql.setup INST_DIR
	unix2dos -n README.mysql.setup ${install_dir}/$@
	chmod 644 ${install_dir}/$@

source_clean: ${install_dir}/source_tree_clean
	if [ ! -d "${install_dir}/source_tree_clean" ]; then \
		${MKDIR} "${install_dir}/source_tree_clean"; \
		(cd "${install_dir}/source_tree_clean"; \
		cvs co -r v${VER}_branch kent;) \
	fi

source_tar: ${install_dir}/source_tree_clean
	rm -f ${install_dir}/source_tree_clean.tgz
	(cd ${install_dir}/source_tree_clean; \
	tar -cvzf ../source_tree_clean.tgz  ./kent)
	touch source_tar

source_built: source_tar
	if [ -d "${install_dir}/source_tree_built" ]; then \
		rm -fr "${install_dir}/source_tree_built"; \
	fi
	${MKDIR} "${install_dir}/source_tree_built"
	(cd "${install_dir}/source_tree_built"; \
		tar xvzf ../source_tree_clean.tgz)
	(cd "${install_dir}/source_tree_built/kent/src"; \
		make libs)
	(TOP=`pwd`; cd "${install_dir}"; DD=`pwd`; \
		cd $$TOP; cd "${install_dir}/source_tree_built/kent/src/hg"; \
		make install DESTDIR=$$DD CGI_BIN=/built_CGI-BIN)
	touch source_built

example: ex.hgcentral.sql ex.hg.conf ex.MySQLUserPerms.sh
	@if [ ! -d ${install_dir}/example ]; then \
		${MKDIR} "${install_dir}/example"; \
	fi
	cp -p ex.hgcentral.sql "${install_dir}/example/hgcentral.sql"
	cp -p ex.hg.conf "${install_dir}/example/hg.conf"
	cp -p ex.MySQLUserPerms.sh "${install_dir}/example/MySQLUserPerms.sh"
	touch example

gbdb:
	@if [ ! -d ${install_dir}/gbdb ]; then \
		${MKDIR} "${install_dir}/gbdb"; \
		${MKDIR} "${install_dir}/gbdb/hgFixed"; \
	fi
	cp -pLR /gbdb/cb1 "${install_dir}/gbdb"
	cp -pLR /gbdb/hgFixed/dbSnpRS.txt.gz "${install_dir}/gbdb/hgFixed"
	touch gbdb

clean:
	rm -fr "${install_dir}" source_tar source_built example
