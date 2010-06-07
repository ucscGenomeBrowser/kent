#
#	product/makefile - create image for product CD-ROM
#
#	$Id: makefile,v 1.20 2007/04/10 23:16:01 hiram Exp $
#
include ../inc/common.mk

DESTDIR=.
install_dir=${DESTDIR}/install_image

all::	INST_DIR source_tar COPYRIGHT.txt README.building.source.txt \
	README.cvs.access.txt README.install.txt \
	README.mysql.setup.txt \
	source_built example webroot gbdb goldenPath

INST_DIR:
	@if [ ! -d ${install_dir} ]; then \
		${MKDIR} "${install_dir}"; \
	fi

COPYRIGHT.txt:	COPYRIGHT INST_DIR
	@VER=`cat ${install_dir}/Version_*`; \
	sed -e "s/VER/$${VER}/" -e "s/DATE_STAMP/`date -I`/" COPYRIGHT | \
	unix2dos > ${install_dir}/$@
	@chmod 644 ${install_dir}/$@

README.building.source.txt:	README.building.source INST_DIR
	@unix2dos -n README.building.source ${install_dir}/$@
	@chmod 644 ${install_dir}/$@

README.cvs.access.txt:	README.cvs.access INST_DIR
	@unix2dos -n README.cvs.access ${install_dir}/$@
	@chmod 644 ${install_dir}/$@

README.install.txt:	README.install INST_DIR
	@unix2dos -n README.install ${install_dir}/$@
	@chmod 644 ${install_dir}/$@

README.mysql.setup.txt: README.mysql.setup INST_DIR
	@unix2dos -n README.mysql.setup ${install_dir}/$@
	@chmod 644 ${install_dir}/$@

#
#	To make this function with the anonymous CVS server,
#	set CVSROOT in your environment:
#	CVSROOT=:pserver:anonymous@genome-test.cse.ucsc.edu:/cbse
#	and perform a 'cvs login' command and enter the password
#	of: genome
#	to that login.  This cvs checkout will then access the
#	anonymous CVS server.
#
source_tar:	INST_DIR
	if [ ! -d "${install_dir}/source_tree_clean" ]; then \
		${MKDIR} "${install_dir}/source_tree_clean"; \
		(cd "${install_dir}/source_tree_clean" && \
		cvs co -r beta kent;) \
	fi
	rm -f ${install_dir}/source_tree_clean.tgz
	(cd ${install_dir}/source_tree_clean && \
	tar -cvzf ../source_tree_clean.tgz  ./kent)
	rm -f ${install_dir}/Version_*
	./touchVersion.sh "${install_dir}"
	rm -fr ${install_dir}/source_tree_clean
	touch source_tar

source_built:	INST_DIR source_tar
	if [ -d "${install_dir}/source_tree_built" ]; then \
		rm -fr "${install_dir}/source_tree_built"; \
	fi
	${MKDIR} "${install_dir}/source_tree_built"
	(cd "${install_dir}/source_tree_built" && \
		tar xvzf ../source_tree_clean.tgz)
	(cd "${install_dir}/source_tree_built/kent/src" && \
		${MAKE} libs)
	(TOP=`pwd`; cd "${install_dir}"; DD=`pwd`; \
		cd $$TOP; cd "${install_dir}/source_tree_built/kent/src/hg" && \
		${MAKE} install DESTDIR=$$DD CGI_BIN=/built_CGI-BIN)
	touch source_built

example:	INST_DIR ex.hgcentral.sql ex.hg.conf ex.MySQLUserPerms.sh \
	ex.InstallExample ex.fixupGbdb.sh ex.cvsup.pl
	@if [ ! -d ${install_dir}/example ]; then \
		${MKDIR} "${install_dir}/example"; \
	fi
	cp -p ex.InstallExample "${install_dir}/example/InstallExample.sh"
	cp -p ex.hgcentral.sql "${install_dir}/example/hgcentral.sql"
	cp -p ex.fixupGbdb.sh "${install_dir}/example/fixupGbdb.sh"
	cp -p ex.cvsup.pl "${install_dir}/example/cvsup.pl"
	cp -p ex.hg.conf "${install_dir}/example/hg.conf"
	VER=`cat ${install_dir}/Version_*`; \
	sed -e "s/VER/$${VER}/" -e "s/DATE_STAMP/`date -I`/" \
		ex.MySQLUserPerms.sh >"${install_dir}/example/MySQLUserPerms.sh"
	chmod 755 "${install_dir}/example/MySQLUserPerms.sh"
	chmod 755 "${install_dir}/example/cvsup.pl"
	chmod 755 "${install_dir}/example/InstallExample.sh"
	touch example

gbdb:	INST_DIR
	@if [ ! -d ${install_dir}/gbdb ]; then \
		${MKDIR} "${install_dir}/gbdb"; \
		${MKDIR} "${install_dir}/gbdb/cb1"; \
	fi
	rsync -avzP \
		rsync://hgdownload.cse.ucsc.edu/gbdb/cb1/ \
		"${install_dir}/gbdb/cb1/"
	touch gbdb

goldenPath:	INST_DIR
	@if [ ! -d ${install_dir}/goldenPath ]; then \
		${MKDIR} "${install_dir}/goldenPath"; \
		${MKDIR} "${install_dir}/goldenPath/cbJul2002"; \
		${MKDIR} "${install_dir}/goldenPath/cbJul2002/database"; \
		${MKDIR} "${install_dir}/goldenPath/hgFixed"; \
		${MKDIR} "${install_dir}/goldenPath/hgFixed/database"; \
		${MKDIR} "${install_dir}/goldenPath/proteinDB"; \
		${MKDIR} "${install_dir}/goldenPath/proteinDB/proteins040315"; \
		${MKDIR} "${install_dir}/goldenPath/proteinDB/proteins040315/database"; \
	fi
	rsync -avzP \
		rsync://hgdownload.cse.ucsc.edu/goldenPath/cbJul2002/database/ \
		"${install_dir}/goldenPath/cbJul2002/database/"
	rsync -avzP \
		rsync://hgdownload.cse.ucsc.edu/goldenPath/hgFixed/database/kimWormLife* \
		"${install_dir}/goldenPath/hgFixed/database/"
	rsync -avzP \
		rsync://hgdownload.cse.ucsc.edu/goldenPath/proteinDB/README.txt \
		"${install_dir}/goldenPath/proteinDB/"
	rsync -avzP \
		rsync://hgdownload.cse.ucsc.edu/goldenPath/proteinDB/proteins040315/database/ \
		"${install_dir}/goldenPath/proteinDB/proteins040315/database/"
	touch goldenPath

webroot:	INST_DIR
	if [ ! -d ${install_dir}/webrootHTML ]; then \
		${MKDIR} "${install_dir}/webrootHTML"; \
		fi;
	rsync -avzP --exclude="CVS" \
		rsync://hgdownload.cse.ucsc.edu/htdocs/ \
		"${install_dir}/webrootHTML/"
	touch webroot

clean:
	rm -fr "${install_dir}" source_tar source_built example gbdb \
		goldenPath webroot
