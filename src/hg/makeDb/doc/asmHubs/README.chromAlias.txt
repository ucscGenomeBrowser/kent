
To add custom alias names to any GenArk genome:

1. go to the 'chromAlias' build directory for the assembly.
   For example in:

cd /hive/data/genomes/asmHubs/genbankBuild/GCA/044/165/215/GCA_044165215.1_HG00097_hap1_hprc_f2/trackData/chromAlias

2. Create or add to any existing customNames.tsv file there:

   head customNames.tsv 
# genbank       hprcV2
CM094060.1      HG00097#1#CM094060.1
CM094061.1      HG00097#1#CM094061.1
CM094062.1      HG00097#1#CM094062.1
CM094063.1      HG00097#1#CM094063.1

   The first column name needs to match the first column name in
   the existing chromAlias.txt file there:

   head GCA_044165215.1_HG00097_hap1_hprc_f2.chromAlias.txt
# genbank       assembly        hprcV2  ncbi    ucsc
CM094060.1      HG00097#1#h1tg000005l   HG00097#1#CM094060.1    1       chr1
CM094061.1      HG00097#1#h1tg000007l   HG00097#1#CM094061.1    2       chr2
CM094062.1      HG00097#1#h1tg000012l   HG00097#1#CM094062.1    3       chr3
CM094063.1      HG00097#1#h1tg000001l   HG00097#1#CM094063.1    5       chr5

   This customNames.tsv file can have any number of additional columns.
   The title name is also necessary to be in the source code script:

     src/hg/utils/automation/aliasTextToBed.pl

   If the name you want to use is not in this list, add it to
   this source file:

my %nameLabels = (
   "assembly" => "Assembly",
   "genbank" => "GenBank",
   "ncbi" => "NCBI",
   "refseq" => "RefSeq",
   "ucsc" => "UCSC",
   "ensembl" => "Ensembl",
   "xenbase" => "Xenbase",
   "chrNames" => "chrNames",
   "chrN" => "chrN",
   "VEuPathDB" => "VEuPathDB",
   "hprcV2" => "HPRCv2",
   "custom" => "custom"
);

3.  With customNames.tsv in place, run the existing script in
    this directory:

    ./doChromAlias.bash

    This will regenerate the chromAlias.txt and chromAlias.bb files with
    this new column.  If it fails it will show errors, success finishes
    with the output:
++ grep -c -v '^#' GCA_018504665.2_NA21309_pat_hprc_f2.chromAlias.txt
+ export aliasCount=95
+ aliasCount=95
++ grep -c -v '^#' test.chromAlias.bed
+ export testCount=95
+ testCount=95
+ '[' 95 -ne 95 ']'
+ '[' 95 -ne 95 ']'
+ exit 0

    Since this is most likely already an existing
    GenArk assembly hub, the symLinks are already in place to make these new
    files get out.  The 'otto' process overnight will get the chromAlias.bb
    file out to its /gbdb/genark/ location.  The alias names will function
    on the RR the next day.

4.  To get the files out to hgdownload, run the 'make sendDownload' command
    in the respective source tree 'clade' grouping:

    cd ~/kent/src/hg/makeDb/doc/primatesAsmHub
    time (make sendDownload) >> send.down.log 2>&1

5. All of the other files you see generated in this chromAlias/ directory
   are made for the testing and verification that everthing is correct.
