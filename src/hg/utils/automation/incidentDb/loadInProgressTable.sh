#!/bin/sh

runOne() {
export db=$1
export Db=$2
hgBbiDbLink ${db} nextNcbiIncidentDb \
	/usr/local/apache/htdocs-hgdownload/goldenPath/${db}/inProgress/${Db}.ncbiIncidentDb.bb
}

runOne hg19 Hg19
runOne mm9 Mm9
runOne danRer7 DanRer7
