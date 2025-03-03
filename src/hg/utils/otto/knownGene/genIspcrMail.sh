
(echo Hey my friends,
echo
echo Could you please start an untranslated -stepSize=5 production gfserver with this 2bit file?
echo
echo  hgwdev:/gbdb/$db/targetDb/${db}KgSeq${GENCODE_VERSION}.2bit
echo
echo        thanks!
echo        $USER) | mail -s "production gfserver start" -r $USER@ucsc.edu cluster-admin@soe.ucsc.edu $USER@ucsc.edu
