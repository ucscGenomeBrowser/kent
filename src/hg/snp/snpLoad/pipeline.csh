hgsql dbSnpHumanBuild125 < dropSplit125.sql
echo "split..."
snpSplitByChrom dbSnpHumanBuild125 ref_haplotype
echo ""
echo "locType..."
snpLocType dbSnpHumanBuild125 ref_haplotype
echo ""
echo "expand allele..."
snpExpandAllele dbSnpHumanBuild125 ref_haplotype
echo ""
echo "sort by chromStart..."
snpSort dbSnpHumanBuild125 ref_haplotype
hgsql -e 'drop table chrM_snpTmp' dbSnpHumanBuild125
hgsql -e 'rename table chrMT_snpTmp to chrM_snpTmp' dbSnpHumanBuild125
echo ""
echo "nib lookup..."
snpRefUCSC dbSnpHumanBuild125 
echo ""
echo "check alleles..."
snpCheckAlleles dbSnpHumanBuild125 
echo ""
echo "lookup in chrN_snpFasta..."
snpReadFasta dbSnpHumanBuild125 
echo ""
echo "check class and observed..."
snpCheckClassAndObserved dbSnpHumanBuild125 
echo ""
echo "get function..."
snpFunction dbSnpHumanBuild125
echo ""
echo "get validation status and heterozygosity..."
snpSNP dbSnpHumanBuild125
echo ""
echo "final table..."
cp snpCheckAlleles.exceptions snpCheckAlleles.tab
cp snpCheckClassAndObserved.exceptions snpCheckClassAndObserved.tab
cp snpExpandAllele.exceptions snpExpandAllele.tab
cp snpLocType.exceptions snpLocType.tab
hgsql -e 'drop table snp125ExceptionsOld' dbSnpHumanBuild125
hgsql -e 'rename table snp125Exceptions to snp125ExceptionsOld' dbSnpHumanBuild125
/cluster/home/heather/kent/src/hg/snp/snpLoad/snpFinalTable dbSnpHumanBuild125
rm snpCheckAlleles.tab
rm snpCheckClassAndObserved.tab
rm snpExpandAllele.tab
rm snpLocType.tab
 
# done with tmp files
rm chr*snpTmp.tab
hgsql dbSnpHumanBuild125 < dropTmp.sql

# PAR SNPs
snpPAR dbSnpHumanBuild125
hgsql -e 'load data local infile "snpPARexceptions.tab" into table snp125Exceptions' dbSnpHumanBuild125
 
# load including PAR SNPs
rm snp125.tab
/bin/sh concat.sh
 rm chr*snp125.tab
hgsql -e 'drop table snp125old' dbSnpHumanBuild125
hgsql -e 'rename table snp125 to snp125old' dbSnpHumanBuild125
hgsql dbSnpHumanBuild125 < $HOME/kent/src/hg/lib/snp125.sql
hgsql -e 'load data local infile "snp125.tab" into table snp125' dbSnpHumanBuild125
rm snp125.tab

# compareLoctype
hgsql -e 'rename table snp125 to snp125new' dbSnpHumanBuild125
snpCompareLoctype dbSnpHumanBuild125 snp124subset snp125new
hgsql -e 'rename table snp125new to snp125' dbSnpHumanBuild125

# multiples
snpMultiple dbSnpHumanBuild125
hgsql -e 'load data local infile "snpMultiple.tab" into table snp125Exceptions' dbSnpHumanBuild125

hgsql -e 'select count(*), exception from snp125Exceptions group by exception' dbSnpHumanBuild125
