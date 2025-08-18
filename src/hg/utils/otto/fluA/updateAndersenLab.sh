#!/bin/bash
set -beEu -o pipefail

export PATH=~/bin/x86_64:~/bin/scripts:$PATH
fluADir=/hive/data/outside/otto/fluA

# Update Andersen Lab avian-influenza repo (assemblies of USDA H5N1 sequences) and map by name
# to GenBank seqs
cd ~/github/avian-influenza
git pull
./scripts/map_genbank.sh ~/github/avian-influenza/metadata/SraRunTable_automated.csv \
    ~/github/avian-influenza/fasta \
    > /data/tmp/angie/genbank_mapping.tsv

wc -l /data/tmp/angie/genbank_mapping.tsv

cd /data/tmp/angie

# Extract sequences that are in SRA but not (yet) in GenBank.
find ~/github/avian-influenza/fasta -name \*.fa \
| grep -vFwf <(cut -f 1 genbank_mapping.tsv) \
| xargs cat > sraNotGb.fa
faSize sraNotGb.fa | head -2

# Rename those sequences to look nicer in the tree and include some metadata.
csvToTab < ~/github/avian-influenza/metadata/SraRunTable_automated.csv \
| tail -n+2 \
| cut -f 1,10,16,19,31 \
| perl -wne 'chomp;
    ($run, $date, $country, $host, $sample) = split(/\t/);
    $year = $date;  $year =~ s/^(\d{4}).*/$1/;
    $host = ucfirst(lc($host));
    $host =~ s/ /-/g;
    $host =~ s/['"'"':;\[\]()]//g;
    $country =~ s/:.*//;
    if ($sample =~ m@^Influenza A virus (A/.*)/\d{4}\(H\dN\d\)\)?@) {
      $sample = $1;
      foreach $segment (qw/HA MP M2 NA NP NS PA PB1 PB2/) {
        print "Consensus_${run}_${segment}_cns_threshold_0.5_quality_20\t${sample}_$segment/$year|$run|$date\n";
        print "Consensus_${run}_${segment}_cns_threshold_0.75_quality_20\t${sample}_$segment/$year|$run|$date\n";
      }
    } else {
      $sample =~ s/-original$//; $sample =~ s/-repeat2?//;
      $sample =~ s/([0-9]{2}-[0-9]{6}-[0-9]{3})-(300|MTM)/$1/;
      foreach $segment (qw/HA MP M2 NA NP NS PA PB1 PB2/) {
        print "Consensus_${run}_${segment}_cns_threshold_0.5_quality_20\tA/$host/$country/${sample}_$segment/$year|$run|$date\n";
        print "Consensus_${run}_${segment}_cns_threshold_0.75_quality_20\tA/$host/$country/${sample}_$segment/$year|$run|$date\n";
      }
    }' \
| grep -Fwf <(grep ^\> sraNotGb.fa | sed -re 's/^>//;') \
    > srr_renaming.tsv
wc -l srr_renaming.tsv

faRenameRecords sraNotGb.fa srr_renaming.tsv andersen_lab.srrNotGb.renamed.fa
faSize andersen_lab.srrNotGb.renamed.fa | head -2

mv andersen_lab.srrNotGb.renamed.fa $fluADir/

# Format metadata for my build
echo -e "strain\tgenbank_accession\tdate\tcountry\tlocation\tlength\thost\tbioproject_accession\tbiosample_accession\tsra_accession\tauthors\tpublications\tserotype\tsegment" \
        > $fluADir/andersen_lab.srrNotGb.renamed.metadata.tsv
csvToTab < ~/github/avian-influenza/metadata/SraRunTable_automated.csv \
| cut -f 1,5,6,9,10,16,18,19,31,33 \
| perl -wne 'chomp;
    ($run, $proj, $biosamp, $center, $date, $country, $loc, $host, $sample, $serotype) = split(/\t/);
    $year = $date;  $year =~ s/^(\d{4}).*/$1/;
    $host = ucfirst(lc($host));
    $host =~ s/ /-/g;
    $host =~ s/['"'"':;\[\]()]//g;
    $country =~ s/: ?(.*)//;
    $loc =~ s/: /:/;
    $loc = $1 if ($loc eq "" && $1 ne "");
    if ($sample =~ m@^Influenza A virus (A/.*)/\d{4}\(H\dN\d\)\)?@) {
      $sample = $1;
      foreach $segment (qw/HA MP M2 NA NP NS PA PB1 PB2/) {
        print join("\t", "${sample}_$segment/$year|$run|$date", "", $date, $country, $loc, "",
                   $host, $proj, $biosamp, $run, $center, "", $serotype, $segment) . "\n";
      }
    } else {
      $sample =~ s/-original$//; $sample =~ s/-repeat2?//;
      $sample =~ s/([0-9]{2}-[0-9]{6}-[0-9]{3})-(300|MTM)/$1/;
      foreach $segment (qw/HA MP M2 NA NP NS PA PB1 PB2/) {
        print join("\t", "A/$host/$country/${sample}_$segment/$year|$run|$date", "", $date, $country, $loc, "",
                   $host, $proj, $biosamp, $run, $center, "", $serotype, $segment) . "\n";
      }
    }' \
| grep -Fwf <(grep ^\> $fluADir/andersen_lab.srrNotGb.renamed.fa | sed -re 's/^>//;') \
| sort -u \
    >> $fluADir/andersen_lab.srrNotGb.renamed.metadata.tsv
wc -l $fluADir/andersen_lab.srrNotGb.renamed.metadata.tsv
echo done
