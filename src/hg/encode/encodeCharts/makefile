include ../../../inc/common.mk

# files to be installed grouped by location
libFiles = gviz_api.py important.dates.tab encodeReportLib.py
cgiFiles = encodeTimeline.py encodeStatus.py encodeReport.py \
           encodeReportFilter.py encodeDist.py encodeDistUnrelease.py \
           encodeJune2010-Freeze.py
htmlFiles = June2010-Freeze.html

# install destinations for files
libPath = /hive/groups/encode/dcc/charts
cgiPath = /usr/local/apache/cgi-bin-genecats/ENCODE
htmlPath = /usr/local/apache/htdocs-genecats/ENCODE

install: createDir ${libFiles:%=${libPath}/%} ${cgiFiles:%=${cgiPath}/%} \
         ${htmlFiles:%=${htmlPath}/%}

createDir:
	@mkdir -p ${libPath}
	@mkdir -p ${cgiPath}
	@mkdir -p ${htmlPath}

${libPath}/%: %
	cp -f $< $@
	chmod 775 $@

${cgiPath}/%: %
	cp -f $< $@
	chmod 775 $@

${htmlPath}/%: %
	cp -f $< $@
	chmod 775 $@

clean:
	rm -f ${libFiles:%=${libPath}/%}
	rm -f ${cgiFiles:%=${cgiPath}/%}
	rm -f ${htmlFiles:%=${htmlPath}/%}
