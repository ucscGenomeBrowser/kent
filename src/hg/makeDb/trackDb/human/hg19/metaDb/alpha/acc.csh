echo '******'
echo A549
mdbQuery -table=metaDb_kate "select lab, dataVersion, metaObject, dccAccession from hg19 where dataType='DnaseSeq' and  cell='A549' and view='Peaks'"
echo '******'
echo GM12878
mdbQuery -table=metaDb_kate "select lab, dataVersion, metaObject, dccAccession from hg19 where dataType='DnaseSeq' and  cell='GM12878' and view='Peaks'"
echo '******'
echo H1-hESC
mdbQuery -table=metaDb_kate "select lab, dataVersion, metaObject, dccAccession from hg19 where dataType='DnaseSeq' and  cell='H1-hESC' and view='Peaks'"
echo '******'
echo Hela
mdbQuery -table=metaDb_kate "select lab, dataVersion, metaObject, dccAccession from hg19 where dataType='DnaseSeq' and  cell='HeLa-S3' and view='Peaks'"
echo '******'
echo Hepg2
mdbQuery -table=metaDb_kate "select lab, dataVersion, metaObject, dccAccession from hg19 where dataType='DnaseSeq' and  cell='HepG2' and view='Peaks'"
echo '******'
echo HMEC
mdbQuery -table=metaDb_kate "select lab, dataVersion, metaObject, dccAccession from hg19 where dataType='DnaseSeq' and  cell='HMEC' and view='Peaks'"
echo '******'
echo HSMM
mdbQuery -table=metaDb_kate "select lab, dataVersion, metaObject, dccAccession from hg19 where dataType='DnaseSeq' and  cell='HSMM' and view='Peaks'"
echo '******'
echo HSMMtube
mdbQuery -table=metaDb_kate "select lab, dataVersion, metaObject, dccAccession from hg19 where dataType='DnaseSeq' and  cell='HSMMtube' and view='Peaks'"
echo '******'
echo HUVEC
mdbQuery -table=metaDb_kate "select lab, dataVersion, metaObject, dccAccession from hg19 where dataType='DnaseSeq' and  cell='HUVEC' and view='Peaks'"
echo '******'
echo K562
mdbQuery -table=metaDb_kate "select lab, dataVersion, metaObject, dccAccession from hg19 where dataType='DnaseSeq' and  cell='K562' and view='Peaks'"
echo '******'
echo LNCaP
mdbQuery -table=metaDb_kate "select lab, dataVersion, metaObject, dccAccession from hg19 where dataType='DnaseSeq' and  cell='LNCaP' and view='Peaks'"
echo '******'
echo NHEK
mdbQuery -table=metaDb_kate "select lab, dataVersion, metaObject, dccAccession from hg19 where dataType='DnaseSeq' and  cell='NHEK' and view='Peaks'"
echo '******'
echo Th1
mdbQuery -table=metaDb_kate "select lab, dataVersion, metaObject, dccAccession from hg19 where dataType='DnaseSeq' and  cell='Th1' and view='Peaks'"
echo '******'
echo Monocytes
mdbQuery -table=metaDb_kate "select lab, dataVersion, metaObject, dccAccession from hg19 where dataType='DnaseSeq' and  cell='Monocytes-CD14+_RO01746' and view='Peaks'"
echo '******'
