#!/bin/csh -efx

# This script will create chains and nets on a blastz
# cross species alignment run and load them into the
# browser database.  

# some variable you need to set before running this script
set tDbName = hg15
set qDbName = mm3
set tOrg = human
set qOrg = mouse
set tSeqDir = /cluster/store5/gs.16/build33
set qSeqDir = /cluster/store2/mm.2003.02/mm3
set aliDir = $tSeqDir/bed/blastz.mm3.2003-04-12-03-MS
set chainDir = $aliDir/axtChain
set fileServer = eieio
set dbServer = hgwdev
set parasolServer = kkr1u00
set subShellDir = chainNet.$tDbName.$qDbName.tmp

if ! -d $chainDir then
    echo $chainDir doesnt exist
endif
if ! -d $tSeqDir/nib then
    echo $tSeqDir/nib doesn't exist
endif
if ! -d $qSeqDir/nib then
    echo $qSeqDir/nib doesnt exist
endif

mkdir -p $subShellDir
cd $subShellDir
cat >chainRun.csh <<endCat
#!/bin/csh -efx

    # Do small cluster run on kkr1u00
    # The little cluster run will probably take about 1 hours.
    cd $aliDir
    mkdir -p axtChain
    cd axtChain
    mkdir -p run1
    cd run1
    mkdir -p chain out
    ls -1S ../../axtChrom/*.axt > input.lst
    echo '#LOOP' > gsub
    echo 'doChain {check in exists \$(path1)} {check out line+ chain/\$(root1).chain} {check out line+ out/\$(root1).out}' >> gsub
    echo '#ENDLOOP' >> gsub
    gensub2 input.lst single gsub spec
    para create spec
    echo '#\!/bin/csh -ef' > doChain
    echo 'axtChain \$1  $tSeqDir/nib $qSeqDir/nib \$2 > \$3' >> doChain
    chmod a+x doChain
    para shove
endCat


cat > sortChains.csh <<endCat
#!/bin/csh -efx
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
#!/bin/csh -efx
    # Load the chains into the database as so.  This will take about
    # 45 minutes.
    # ssh $dbServer
    cd $chainDir
    cd chain
    foreach i (*.chain)
	set c = \$i:r
	hgLoadChain $tDbName \${c}_${qOrg}Chain \$i
    end
endCat

cat > makeNet.csh <<endCat
#!/bin/csh -efx
    # Create the nets.  You can do this while the database is loading
    # This is fastest done on the file server.  All told it takes about
    # 40 minutes.
    # ssh $fileServer
    cd $chainDir
    # First do a crude filter that eliminates many chains so the
    # real chainer has less work to do.
    mkdir -p preNet
    cd chain
    foreach i (*.chain)
      chainPreNet \$i $tSeqDir/chrom.sizes $qSeqDir/chrom.sizes ../preNet/\$i
    end
    cd ..
    # Run the main netter, putting the results in n1.
    mkdir -p n1 
    cd preNet
    foreach i (*.chain)
      set n = \$i:r.net
      chainNet \$i -minSpace=1 $tSeqDir/chrom.sizes $qSeqDir/chrom.sizes ../n1/\$n /dev/null
    end
    cd ..
    rm -r preNet
    # Classify parts of net as syntenic, nonsyntenic etc.
    cat n1/*.net | netSyntenic stdin noClass.net
endCat

cat > finishLoadNet.csh <<endCat
#!/bin/csh -efx
    # The final step of net creation needs the database.
    # Best to wait for the database load to finish if it
    # hasn't already.
    # ssh $dbServer
    cd $chainDir
    netClass noClass.net $tDbName $qDbName $tOrg.net -tNewR=$tSeqDir/bed/linSpecRep -qNewR=$qSeqDir/bed/linSpecRep
    rm -r n1 noClass.net

    # Load the net into the database as so:
    netFilter -minGap=10 $tOrg.net |  hgLoadNet $tDbName ${qOrg}Net stdin
endCat

cat > makeAxtNet.csh <<endCat
#!/bin/csh -efx
    # Move back to the file server to create axt files corresponding
    # to the net.
    # ssh $fileServer
    cd $chainDir
    mkdir -p ../axtNet
    netSplit $tOrg.net ${tOrg}Net
    cd ${tOrg}Net
    foreach i (*.net)
        set c = \$i:r
	netToAxt \$i ../chain/\$c.chain $tSeqDir/nib $qSeqDir/nib ../../axtNet/\$c.axt
    end
    cd ..
    rm -r ${tOrg}Net
endCat

    # At this point there is a blastz..../axtNet directory that should
    # be used in place of the axtBest for making the human/mouse
    # conservation track and for loading up the downloads page.
chmod a+x *.csh
ssh $parasolServer $cwd/chainRun.csh
ssh $fileServer $cwd/sortChains.csh
#Do the next two in parallel.
ssh $dbServer $cwd/loadChains.csh &
ssh $fileServer $cwd/makeNet.csh
wait
ssh $dbServer $cwd/finishLoadNet.csh
ssh $fileServer $cwd/makeAxtNet.csh
# rm -f $subShellDir

cd ..

