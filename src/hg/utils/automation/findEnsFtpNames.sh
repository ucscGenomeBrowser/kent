#!/bin/sh

# $Id: findEnsFtpNames.sh,v 1.1 2009/07/14 18:40:25 hiram Exp $

VERSION=$1
if [ "x${VERSION}y" = "xy" ]; then
    echo "usage: findEnsFtpNames.sh <ens version>"
    echo "where <ens version> is something like: 55"
    echo "this script will scan the ftp.ensembl.org site and extract"
    echo "the names from the files there that we need to create"
    echo "correspondence to UCSC database names"
    echo "when complete, look for result files:"
    echo "release.<ens version>.gtf.names"
    echo "release.<ens version>.MySQL.names"
    echo "release.<ens version>.fasta.names"
    echo "use those lists to edit EnsGeneAutomate.pm"
    exit 255
fi

if [ 0 = 1 ]; then
echo "Scanning for GTF file names"

echo "user anonymous hiram@soe
cd pub/release-${VERSION}/gtf
ls -lR
bye" > ftp.rsp

ftp -n -v -i ftp.ensembl.org < ftp.rsp > release.${VERSION}.gtf.ls-lR

egrep -v "CHECKSUMS|README" release.${VERSION}.gtf.ls-lR | awk '
{
if (match($1,"^[a-z_]*:$")) {gsub(":$","",$1); printf "%s/", $1 }
if (NF == 9) { if (match($1,"^-rw")) {printf "%s\n", $NF} }
}
' | sed -e "s#^#'x' => '#; s#\$#',#" > release.${VERSION}.gtf.names

exit $?

echo "Scanning for MySQL table files"

echo "user anonymous hiram@soe
cd pub/release-${VERSION}/mysql
ls -lR
bye" > ftp.rsp

ftp -i -n -v ftp.ensembl.org < ftp.rsp > release.${VERSION}.MySQL.ls-lR

egrep "_core_${VERSION}.*:$" release.${VERSION}.MySQL.ls-lR \
  | sed -e 's/://;' | sed -e "s#^#'x' => '#; s#\$#',#" \
     > release.${VERSION}.MySQL.names

if [ 0 = 1 ]; then
awk '
BEGIN{ D="notYet" }
{
  if (!match($1,"^d")) {
    if (match($1,"^./")) {
        gsub("^./","",$1); gsub(":$","",$1); D = $1;
        if (match(D,"_core_")) { printf "%s\n", D }
    }
  }
}
' release.${VERSION}.MySQL.ls-lR \
	| sed -e "s#^#'x' => '#; s#\$#',#" > release.${VERSION}.MySQL.names

fi

fi

echo "Scanning for protein fasta files:"

echo "user anonymous hiram@ucsc
cd pub/release-${VERSION}/fasta
ls -lR
bye" > ftp.rsp

ftp -i -n -v ftp.ensembl.org < ftp.rsp > release.${VERSION}.fasta.ls-lR

awk '
BEGIN{ D="notYet" }
{
  if (!match($1,"^d")) {
    if (match($1,"^[a-z_]*/pep:$")) {
        gsub(":$","",$1); D = $1;
    }
    if ((9 == NF) && match($1,"^-rw") && match($NF,"pep.all.fa")) {
        printf "%s/%s\n", D, $NF
    }
  }
}
' release.${VERSION}.fasta.ls-lR \
	| sed -e "s#^#'x' => '#; s#\$#',#" > release.${VERSION}.fasta.names

rm -f ftp.rsp
