#!/bin/csh -ef

# This script will create chains and nets on a blastz
# cross species alignment run and load them into the
# browser database.  

# some variable you need to set before running this script
set tDbName = mm3
set qDbName = rn2
set aliDir = /cluster/store2/mm.2003.02/mm3/bed/blastz.rn2.2003-03-07-ASH
set chainDir = $aliDir/axtChain
set tSeqDir = /cluster/store2/mm.2003.02/mm3
set qSeqDir = /cluster/store4/rn2
set fileServer = kkstore
set dbServer = hgwdev
set parasolServer = kkr1u00

mkdir chainNet.tmp
cd chainNet.tmp
cat >chainRun.csh <<endCat
#!/bin/csh -ef

    # Do small cluster run on kkr1u00
    # The little cluster run will probably take about 1 hours.
    cd $aliDir
    mkdir axtChain
    cd axtChain
    mkdir run1
    cd run1
    mkdir chain out
    ls -1S ../../axtChrom/*.axt.gz > input.lst
    echo '#LOOP' > gsub
    echo 'doChain {check in exists \$(path1)} {check out line+ chain/\$(root1).chain} {check out line+ out/\$(root1).out}' >> gsub
    echo '#ENDLOOP' >> gsub
    gensub2 input.lst single gsub spec
    para create spec
    echo '#\!/bin/csh -ef' > doChain
    echo 'gunzip -c \$1 | axtChain stdin  $tSeqDir/mixedNib $qSeqDir/mixedNib \$2 > \$3' >> doChain
    chmod a+x doChain
    para shove
endCat


cat > sortChains.csh <<endCat
#!/bin/csh -ef
    # Do some sorting on the little cluster job on the file server
    # This will take about 20 minutes.  This also ends up assigning
    # a unique id to each member of the chain.
    # ssh $fileServer
    cd $chainDir
    chainMergeSort run1/chain/*.chain > all.chain
    chainSplit chain all.chain
    rm run1/chain/*.chain
endCat

cat > loadChains.csh <<endCat
#!/bin/csh -ef
    # Load the chains into the database as so.  This will take about
    # 45 minutes.
    # ssh $dbServer
    cd $chainDir
    cd chain
    foreach i (*.chain)
	set c = \$i:r
	hgLoadChain $tDbName \${c}_humanChain \$i
	echo done \$c
    end
endCat

cat > makeNet.csh <<endCat
#!/bin/csh -ef
    # Create the nets.  You can do this while the database is loading
    # This is fastest done on the file server.  All told it takes about
    # 40 minutes.
    # ssh $fileServer
    cd $chainDir
    # First do a crude filter that eliminates many chains so the
    # real chainer has less work to do.
    mkdir preNet
    cd chain
    foreach i (*.chain)
      echo preNetting \$i
      chainPreNet \$i $tSeqDir/chrom.sizes $qSeqDir/chrom.sizes ../preNet/\$i
    end
    cd ..
    rm -r preNet
    # Run the main netter, putting the results in n1.
    mkdir n1 
    cd preNet
    foreach i (*.chain)
      set n = \$i:r.net
      echo primary netting \$i
      chainNet \$i -minSpace=1 $tSeqDir/chrom.sizes $qSeqDir/chrom.sizes ../n1/\$n /dev/null
    end
    cd ..
    # Classify parts of net as syntenic, nonsyntenic etc.
    cat n1/*.net | netSyntenic stdin hNoClass.net
endCat

cat > finishLoadNet.csh <<endCat
#!/bin/csh -ef
    # The final step of net creation needs the database.
    # Best to wait for the database load to finish if it
    # hasn't already.
    # ssh $dbServer
    cd $chainDir
    netClass hNoClass.net $tDbName hg13 mouse.net -tNewR=\$HOME/mm/bed/linSpecRep -qNewR=\$HOME/oo/bed/linSpecRep
    rm -r n1 hNoClass.net

    # Load the net into the database as so:
    netFilter -minGap=10 mouse.net |  hgLoadNet $tDbName humanNet stdin
endCat

cat > makeAxtNet.csh <<endCat
#!/bin/csh -ef
    # Move back to the file server to create axt files corresponding
    # to the net.
    # ssh $fileServer
    cd $chainDir
    mkdir ../axtNet
    netSplit mouse.net mouseNet
    cd mouseNet
    foreach i (*.net)
        set c = \$i:r
	netToAxt \$i ../chain/\$c.chain $tSeqDir/mixedNib $qSeqDir/mixedNib ../../axtNet/\$c.axt
	echo done ../axt/\$c.axt
    end
    cd ..
    rm -r mouseNet
endCat

    # At this point there is a blastz..../axtNet directory that should
    # be used in place of the axtBest for making the human/mouse
    # conservation track and for loading up the downloads page.
chmod a+x *.csh
ssh $parasolServer $cwd/chainRun.csh
ssh $fileServer $cwd/sortChains.csh
ssh $dbServer $cwd/loadChains.csh
ssh $fileServer $cwd/makeNet.csh
ssh $dbServer $cwd/finishLoadNet.csh
ssh $fileServer $cwd/makeAxtNet.csh

cd ..

