#
#	product/makefile - create image for product CD-ROM
#
#	This makefile expects to be used on hgwdev so it can have
#	access to common html files from apache root to copy into
#	this hierarchy, as well as access also to /gbdb links.
#
include ../inc/common.mk

#	This version string needs to be raised for a release

VER=33

DESTDIR=.
install_dir=${DESTDIR}/install_image

all::	INST_DIR COPYRIGHT.txt README.building.source.txt \
	README.cvs.access.txt README.install.txt \
	README.mysql.setup.txt version source_tar \
	source_built example gbdb goldenPath

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

source_tar: ${install_dir}/source_tree_clean.tgz
	if [ ! -d "${install_dir}/source_tree_clean" ]; then \
		${MKDIR} "${install_dir}/source_tree_clean"; \
		(cd "${install_dir}/source_tree_clean"; \
		cvs co -r v${VER}_branch kent;) \
	fi
	rm -f ${install_dir}/source_tree_clean.tgz
	(cd ${install_dir}/source_tree_clean; \
	tar -cvzf ../source_tree_clean.tgz  ./kent)
	rm -fr ${install_dir}/source_tree_clean
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

goldenPath:
	@if [ ! -d ${install_dir}/goldenPath ]; then \
		${MKDIR} "${install_dir}/goldenPath"; \
		${MKDIR} "${install_dir}/goldenPath/cbJul2002"; \
		${MKDIR} "${install_dir}/goldenPath/hgFixed"; \
	fi
	cp -pLR /usr/local/apache/htdocs/goldenPath/cbJul2002 \
		"${install_dir}/goldenPath/"
	cp -pLR /usr/local/apache/htdocs/goldenPath/hgFixed \
		"${install_dir}/goldenPath/"
	rm -f "${install_dir}/goldenPath/cbJul2002/sanger/cb25.agp8.reads.placed.gz"
	rm -f "${install_dir}/goldenPath/cbJul2002/sanger/cb25.agp8.supercontigs.fasta.gz"
	rm -f "${install_dir}/goldenPath/cbJul2002/sanger/cb25.agp8.supercontigs.fasta.qual.gz"
	rm -fr "${install_dir}/goldenPath/cbJul2002/chromosomes"
	cp -p /cluster/data/cb1/sanger/README "${install_dir}/goldenPath/cbJul2002/sanger/"
	touch goldenPath

webroot:
	@for dirs in admin goldenPath/customTracks goldenPath/help \
	icons images style; do \
		echo "webrootHTML/$$dirs"; \
		if [ ! -d ${install_dir}/webrootHTML/$$dirs ]; then \
		${MKDIR} "${install_dir}/webrootHTML/$$dirs"; \
		fi \
	done
	@for f in `cat webroot.filelist`; do \
		echo "$$f"; \
		cp -p "/usr/local/apache/htdocs/$$f" "${install_dir}/webrootHTML/$$f"; \
	done
	@cp -p "/usr/local/apache/icons/back.gif" "${install_dir}/webrootHTML/icons/back.gif"
	@cp -p "/usr/local/apache/icons/blank.gif" "${install_dir}/webrootHTML/icons/blank.gif"
	@cp -p "/usr/local/apache/icons/folder.gif" "${install_dir}/webrootHTML/icons/folder.gif"
	touch webroot

clean:
	rm -fr "${install_dir}" source_tar source_built example gbdb \
		goldenPath webroot
