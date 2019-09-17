#########################################################################
# LASTZ mouse lemur/micMur3 Bushbaby/otoGar3 - (DONE - 2018-01-31 - Hiram)
    mkdir /hive/data/genomes/micMur3/bed/lastzOtoGar3.2018-01-31
    cd /hive/data/genomes/micMur3/bed/lastzOtoGar3.2018-01-31

    printf '# mouse lemur vs Bushbaby
BLASTZ=/cluster/bin/penn/lastz-distrib-1.03.66/bin/lastz
BLASTZ_T=2
BLASTZ_O=400
BLASTZ_E=30
BLASTZ_M=254
# default BLASTZ_Q score matrix:
#       A     C     G     T
# A    91  -114   -31  -123
# C  -114   100  -125   -31
# G   -31  -125   100  -114
# T  -123   -31  -114    91

# TARGET: mouse lemur micMur3
SEQ1_DIR=/hive/data/genomes/micMur3/micMur3.2bit
SEQ1_LEN=/hive/data/genomes/micMur3/chrom.sizes
SEQ1_CHUNK=40000000
SEQ1_LIMIT=50
SEQ1_LAP=10000

# QUERY: Bushbaby otoGar3
SEQ2_DIR=/hive/data/genomes/otoGar3/otoGar3.2bit
SEQ2_LEN=/hive/data/genomes/otoGar3/chrom.sizes
SEQ2_CHUNK=20000000
SEQ2_LIMIT=50
SEQ2_LAP=0

BASE=/hive/data/genomes/micMur3/bed/lastzOtoGar3.2018-01-31
TMPDIR=/dev/shm
' > DEF

    time (doBlastzChainNet.pl `pwd`/DEF -verbose=2 \
        -chainMinScore=3000 -chainLinearGap=medium \
          -workhorse=hgwdev -smallClusterHub=ku -bigClusterHub=ku \
            -syntenicNet) > do.log 2>&1
    # real    1018m41.024s

    cat fb.micMur3.chainOtoGar3Link.txt
    # 1687187383 bases of 2386321975 (70.702%) in intersection
    cat fb.micMur3.chainSynOtoGar3Link.txt
    # 1557372752 bases of 2386321975 (65.262%) in intersection

    time (doRecipBest.pl -load  -workhorse=hgwdev -buildDir=`pwd` \
	micMur3 otoGar3) > rbest.log 2>&1 &
    # real    711m44.214s

    cat fb.micMur3.chainRBestOtoGar3Link.txt
    # 1530000987 bases of 2386321975 (64.115%) in intersection

    # and for the swap:
    mkdir /hive/data/genomes/otoGar3/bed/blastz.micMur3.swap
    cd /hive/data/genomes/otoGar3/bed/blastz.micMur3.swap

    time (doBlastzChainNet.pl -verbose=2 \
      /hive/data/genomes/micMur3/bed/lastzOtoGar3.2018-01-31/DEF \
        -swap -chainMinScore=3000 -chainLinearGap=medium \
          -workhorse=hgwdev -smallClusterHub=ku -bigClusterHub=ku \
            -syntenicNet) > swap.log 2>&1
    #  real    611m55.835s

    cat fb.otoGar3.chainMicMur3Link.txt
    # 1632274025 bases of 2359530453 (69.178%) in intersection
    cat fb.otoGar3.chainSynMicMur3Link.txt
    # 1512529389 bases of 2359530453 (64.103%) in intersection

    time (doRecipBest.pl -load -workhorse=hgwdev -buildDir=`pwd` \
	otoGar3 micMur3) > rbest.log 2>&1
    # real    709m34.527s

    cat fb.otoGar3.chainRBestMicMur3Link.txt
    # 1531566615 bases of 2359530453 (64.910%) in intersection

#########################################################################

