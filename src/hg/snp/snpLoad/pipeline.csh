# hgsql hg18snp126 < dropSplit126.sql
echo "split..."
snpSplitByChrom hg18snp126 ref_assembly
echo ""
echo "locType..."
snpLocType hg18snp126 ref_assembly
echo ""
echo "expand allele..."
snpExpandAllele hg18snp126 ref_assembly
echo ""
echo "sort by chromStart..."
snpSort hg18snp126 ref_assembly
hgsql -e 'drop table chrM_snpTmp' hg18snp126
hgsql -e 'rename table chrMT_snpTmp to chrM_snpTmp' hg18snp126
echo ""
echo "nib lookup..."
snpRefUCSC hg18snp126 
echo ""
echo "check alleles..."
snpCheckAlleles hg18snp126 
echo ""
echo "get class and observed..."
snpClassAndObserved hg18snp126 
echo ""
echo "check class and observed..."
snpCheckClassAndObserved hg18snp126 
echo ""
echo "get function..."
snpFunction hg18snp126
echo ""
echo "get validation status and heterozygosity..."
snpSNP hg18snp126
echo ""
echo "get moltype..."
snpMoltype hg18snp126
echo ""
echo "final table..."
hgsql hg18snp126 < /cluster/home/heather/kent/src/hg/lib/snp126Exceptions.sql
cp snpCheckAlleles.exceptions snpCheckAlleles.tab
cp snpCheckClassAndObserved.exceptions snpCheckClassAndObserved.tab
cp snpExpandAllele.exceptions snpExpandAllele.tab
cp snpLocType.exceptions snpLocType.tab
hgsql -e 'drop table snp126Exceptions' hg18snp126
snpFinalTable hg18snp126 126
rm snpCheckAlleles.tab
rm snpCheckClassAndObserved.tab
rm snpExpandAllele.tab
rm snpLocType.tab
 
# done with tmp files
# rm chr*snpTmp.tab
# hgsql hg18snp126 < dropTmp.sql

# PAR SNPs
snpPAR hg18snp126
hgsql -e 'load data local infile "snpPARexceptions.tab" into table snp126Exceptions' hg18snp126
 
# load including PAR SNPs
/bin/sh concat.sh
rm chr*snp126.tab
hgsql -e 'drop table snp126' hg18snp126
hgsql hg18snp126 < /cluster/home/heather/kent/src/hg/lib/snp126.sql
hgsql -e 'load data local infile "snp126.tab" into table snp126' hg18snp126
cp snp126.tab /cluster/home/heather/transfer/snp
rm snp126.tab

# compareLoctype
hgsql -e "drop table snp126new" hg18snp126
hgsql -e 'rename table snp126 to snp126new' hg18snp126
snpCompareLoctype hg18snp126 snp125subset snp126new
hgsql -e 'rename table snp126new to snp126' hg18snp126

# multiples
snpMultiple hg18snp126
hgsql -e 'load data local infile "snpMultiple.tab" into table snp126Exceptions' hg18snp126

hgsql -e 'select count(*), exception from snp126Exceptions group by exception' hg18snp126
hgsql -N -e 'select * from snp126Exceptions' hg18snp126 > /cluster/home/heather/transfer/snp/snp126Exceptions.tab
