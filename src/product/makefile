include ../inc/common.mk

VER=31

DESTDIR=.
install_dir=${DESTDIR}/install_image

all::	INST_DIR COPYRIGHT.txt README.building.source.txt \
	README.cvs.access.txt README.install.txt \
	README.mysql.setup.txt version

INST_DIR:
	@if [ ! -d ${install_dir} ]; then \
		${MKDIR} "${install_dir}"; \
	fi

version:
	rm -f ${install_dir}/Version_*
	touch ${install_dir}/Version_${VER}.`date -I`

COPYRIGHT.txt:	COPYRIGHT
	sed -e "s/DATE_STAMP/`date -I`/" COPYRIGHT | \
	unix2dos > ${install_dir}/$@
	chmod 644 ${install_dir}/$@

README.building.source.txt:	README.building.source
	unix2dos -n README.building.source ${install_dir}/$@
	chmod 644 ${install_dir}/$@

README.cvs.access.txt:	README.cvs.access
	unix2dos -n README.cvs.access ${install_dir}/$@
	chmod 644 ${install_dir}/$@

README.install.txt:	README.install
	unix2dos -n README.install ${install_dir}/$@
	chmod 644 ${install_dir}/$@

README.mysql.setup.txt: README.mysql.setup
	unix2dos -n README.mysql.setup ${install_dir}/$@
	chmod 644 ${install_dir}/$@

clean:
	rm -fr "${install_dir}"
