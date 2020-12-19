##############################################################################
# Orangutan Lastz run (DONE - 2020-05-19 - Hiram)
    screen -S panTro6PonAbe3      # use a screen to manage this longish running job
    mkdir /hive/data/genomes/panTro6/bed/lastzPonAbe3.2020-05-19
    cd /hive/data/genomes/panTro6/bed/lastzPonAbe3.2020-05-19

    # always set the BLASTZ program so we know what version was used
    printf "# chimp vs orangutan
BLASTZ=/cluster/bin/penn/lastz-distrib-1.04.03/bin/lastz
BLASTZ_T=2
BLASTZ_O=600
BLASTZ_E=150
BLASTZ_M=254
BLASTZ_K=4500
BLASTZ_L=4500
BLASTZ_Y=15000
BLASTZ_Q=/hive/data/staging/data/blastz/human_chimp.v2.q
#    A    C    G    T
#    90 -330 -236 -356
#  -330  100 -318 -236
#  -236 -318  100 -330
#  -356 -236 -330   90

# TARGET: chimp PanTro6
SEQ1_DIR=/hive/data/genomes/panTro6/panTro6.2bit
SEQ1_LEN=/hive/data/genomes/panTro6/chrom.sizes
SEQ1_CHUNK=20000000
SEQ1_LIMIT=40
SEQ1_LAP=10000

# QUERY: Orangutan PonAbe3
SEQ2_DIR=/hive/data/genomes/ponAbe3/ponAbe3.2bit
SEQ2_LEN=/hive/data/genomes/ponAbe3/chrom.sizes
SEQ2_CHUNK=20000000
SEQ2_LAP=0
SEQ2_LIMIT=40
SEQ2_IN_CONTIGS=0

BASE=/hive/data/genomes/panTro6/bed/lastzPonAbe3.2020-05-19
TMPDIR=/dev/shm
" > DEF

    time (doBlastzChainNet.pl `pwd`/DEF -verbose=2 \
        -chainMinScore=5000 -chainLinearGap=medium \
        -workhorse=hgwdev -smallClusterHub=hgwdev -bigClusterHub=ku \
        -syntenicNet) > do.log 2>&1 &
    # real    94m43.451s

    cat fb.panTro6.chainPonAbe3Link.txt
    # 2710778689 bases of 3018592990 (89.803%) in intersection
    cat fb.panTro6.chainSynPonAbe3Link.txt
    # 2692073497 bases of 3018592990 (89.183%) in intersection

    # filter with doRecipBest.pl
    time (doRecipBest.pl -load -workhorse=hgwdev -buildDir=`pwd` \
        panTro6 ponAbe3) > rbest.log 2>&1 &
    # real    79m59.335s

    cat fb.panTro6.chainRBest.PonAbe3.txt
    # 2631531198 bases of 3018592990 (87.177%) in intersection

    # running the swap
    mkdir /hive/data/genomes/ponAbe3/bed/blastz.panTro6.swap
    cd /hive/data/genomes/ponAbe3/bed/blastz.panTro6.swap
    time (doBlastzChainNet.pl -verbose=2 \
        -swap /hive/data/genomes/panTro6/bed/lastzPonAbe3.2020-05-19/DEF \
        -chainMinScore=5000 -chainLinearGap=medium \
        -workhorse=hgwdev -smallClusterHub=hgwdev -bigClusterHub=ku \
        -syntenicNet) > swap.log 2>&1
    # real    64m29.790s

    cat fb.ponAbe3.chainPanTro6Link.txt
    # 2685290576 bases of 3043444524 (88.232%) in int rsection
    cat fb.ponAbe3.chainSynPanTro6Link.txt
    # 2669539267 bases of 3043444524 (87.714%) in intersection

    time (doRecipBest.pl -load -workhorse=hgwdev -buildDir=`pwd` \
        ponAbe3 panTro6) > rbest.log 2>&1 &
    # real    84m51.043s

    cat fb.ponAbe3.chainRBest.PanTro6.txt
    # 2631453596 bases of 3043444524 (86.463%) in intersection

#############################################################################
