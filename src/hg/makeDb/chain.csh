#!/bin/csh -efx

# some variable you need to set before running this script
set tDbName = mm3
set qDbName = mm3
set tOrg = mouse
set qOrg = mouse
set aliDir = /mnt/angie/blastz.mm3.2003-03-06-ASH
set chainDir = $aliDir/axtChain
set tSeqDir = /cluster/store2/mm.2003.02/mm3
set qSeqDir = /cluster/store2/mm.2003.02/mm3
set fileServer = eieio
set dbServer = hgwdev
set parasolServer = kkr1u00
set subShellDir = chain.$tDbName.$qDbName.tmp

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
    echo 'axtChain \$1  $tSeqDir/mixedNib $qSeqDir/mixedNib \$2 > \$3' >> doChain
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

chmod a+x *.csh
ssh $parasolServer $cwd/chainRun.csh
ssh $fileServer $cwd/sortChains.csh
ssh $dbServer $cwd/loadChains.csh 
# rm -f $subShellDir

cd ..

