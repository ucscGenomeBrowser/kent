#!/bin/sh -ex
mkdir -p $dir/kegg
cd $dir/kegg

# Make the keggMapDesc table, which maps KEGG pathway IDs to descriptive names
cp /cluster/data/hg19/bed/ucsc.14.3/kegg/map_title.tab .
# wget --timestamping ftp://ftp.genome.jp/pub/kegg/pathway/map_title.tab
cat map_title.tab | sed -e 's/\t/\thsa\t/' > j.tmp
cut -f 2 j.tmp >j.hsa
cut -f 1,3 j.tmp >j.1
paste j.hsa j.1 |sed -e 's/\t//' > keggMapDesc.tab
rm j.hsa j.1 j.tmp
hgLoadSqlTab -notOnServer $tempDb keggMapDesc $kent/src/hg/lib/keggMapDesc.sql keggMapDesc.tab

# Following in two-step process, build/load a table that maps UCSC Gene IDs
# to LocusLink IDs and to KEGG pathways.  First, make a table that maps 
# LocusLink IDs to KEGG pathways from the downloaded data.  Store it temporarily
# in the keggPathway table, overloading the schema.
cp /cluster/data/hg19/bed/ucsc.14.3/kegg/hsa_pathway.list .

cat hsa_pathway.list| sed -e 's/path://'|sed -e 's/:/\t/' > j.tmp
hgLoadSqlTab -notOnServer $tempDb keggPathway $kent/src/hg/lib/keggPathway.sql j.tmp

# Next, use the temporary contents of the keggPathway table to join with
# knownToLocusLink, creating the real content of the keggPathway table.
# Load this data, erasing the old temporary content
hgsql $tempDb -B -N -e 'select distinct name, locusID, mapID from keggPathway p, knownToLocusLink l where p.locusID=l.value' > keggPathway.tab
hgLoadSqlTab -notOnServer $tempDb \
    keggPathway $kent/src/hg/lib/keggPathway.sql  keggPathway.tab

# Finally, update the knownToKeggEntrez table from the keggPathway table.
hgsql $tempDb -B -N -e 'select kgId, mapID, mapID, "+", locusID from keggPathway' |sort -u| sed -e 's/\t+\t/+/' > knownToKeggEntrez.tab
hgLoadSqlTab -notOnServer $tempDb knownToKeggEntrez $kent/src/hg/lib/knownToKeggEntrez.sql knownToKeggEntrez.tab

echo "BuildKegg successfully finished"
