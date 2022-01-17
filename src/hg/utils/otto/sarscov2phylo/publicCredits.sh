#!/bin/bash
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/publicCredits.sh

usage() {
    echo "usage: $0"
}

if [ $# != 0 ]; then
  usage
  exit 1
fi

cncbMetadata=/hive/data/outside/otto/sarscov2phylo/cncb.latest/cncb.metadata.tsv
ncbiMetadata=ncbi.authors.20-11-13.csv
#*** TODO AUTOMATE ME
#   * https://www.ncbi.nlm.nih.gov/labs/virus/vssi/#/virus?SeqType_s=Nucleotide&VirusLineage_ss=SARS-CoV-2,%20taxid:2697049
#   * Download button
#   * Current table view result --> CSV format, Next button
#   * Download all records, Next button
#   * Select Accession and Authors [no labs options unfortunately]
#   * Download button, save as ncbi.authors.date.csv

#*** TODO AUTOMATE ME
#   * https://www.ebi.ac.uk/ena/browser/view/PRJEB37886
#   * select columns center_name, sample_accession, sample_alias
#   * Download report: TSV
#   * file saved to filereport_read_run_PRJEB37886_tsv.2020-11-3.txt (extra first column, run_accession)
# 2021-01-08: the project seems to have 133312 sequences, but the download file is cut off
# after 100,000.  It would probably be more efficient to just identify all of the country/center
# strings in strain names, and ask COG-UK how to attribute sequences from each one.
cogUkMetadata=filereport_read_run_PRJEB37886_tsv.2020-11-13.txt


# Actually BioSample has this for COG-UK:
# xzcat $ncbiDir/all.bioSample.tab.xz \
# | grep COG-UK \
# | sed -re 's@COG-UK/@@; s/(\t202[01])[0-9-]+/\1/;' \
# | tawk '{print $8 "/" $3 "/" $4, $5;}' \
# | sort > cogToLab
# -- although some entries break it down like this:
# Wales/QEUH-B2C4CB/2020  Originating lab: Wales Specialist Virology Centre Sequencing lab: Pathogen Genomics Unit
# -- well, I guess just that one:
# g : cogToLab | cut -f 2 | sort | uniq -c | sort -nr | less
#  38415 Originating lab: Wales Specialist Virology Centre Sequencing lab: Pathogen Genomics Unit



# Author credits file... strip GenBank version numbers because NCBI metadata doesn't have those
cut -f 2 treeToPublic \
| cut -d \| -f 2 \
| sed -re 's/^([A-Z][A-Z][0-9]{6})\.[0-9]/\1/;' \
| sort > publicIdsInTree
tail -n+2 $cncbMetadata \
| cut -f 2,12,14 \
| grep -v ^EPI_ISL_ \
| egrep -v '^[A-Z][A-Z][0-9]{6}' \
| sed -e 's/"//g; s/$/\tn\/a/;' \
  > cncb.credits
tail -n+2 $ncbiMetadata \
| csvToTab \
| tawk '{print $1, "n/a", "n/a", $2;}' \
  > ncbi.credits
tail -n+2 $cogUkMetadata \
| tawk '{print $4, $3, $3, "COVID-19 Genomics UK Consortium";}' \
| sed -e 's@^COG-UK/@@;' \
| sort -u \
  > cogUk.credits.partialIds
grep / publicIdsInTree \
| awk -F/ '{print $2 "\t" $0;}' \
| sort \
  > cogUk.partialToFull
join -a 2 -e "n/a" -t$'\t' -o 2.2,1.2,1.3,1.4 cogUk.credits.partialIds cogUk.partialToFull \
| tawk '{ if ($4 == "n/a") { $4 = "COVID-19 Genomics UK Consortium"; } print; }' \
  > cogUk.credits
/bin/echo -e "accession\toriginating_lab\tsubmitting_lab\tauthors" > acknowledgements.tsv

#*** THIS IS just for the subset... we need a full acknowledgements file too.
grep -Fwf publicIdsInTree cncb.credits >> acknowledgements.tsv
grep -Fwf publicIdsInTree ncbi.credits >> acknowledgements.tsv
grep -Fwf publicIdsInTree cogUk.credits >> acknowledgements.tsv
gzip acknowledgements.tsv
