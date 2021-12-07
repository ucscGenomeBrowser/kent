
Thu Oct 21 11:17:52 PDT 2021

The cron job is daily at 09:33

33 09 * * * /hive/data/outside/otto/grcIncidentDb/runUpdate.sh makeItSo

Tue Sep  3 10:48:30 PDT 2019

Create the directory:
   /gbdb/<db>/bbi/grcIncidentDb

Add the new work directory to the list in runUpdate.sh:

for D in GRCh37 GRCh38 GRCm38 Zv9 MGSCv37 GRCz10 GRCz11 Gallus_gallus-5.0 GRCg6a
...

Need to get the refSeq.chromNames.tab file established in the new
directory:

  hgsql -e 'select * from chromAlias;' galGal6 \
     | grep -i refseq | cut -f1,2 | sort > refSeq.chromNames.tab

And add the command in the set of commands there: e.g.:

./grcUpdate.sh GRCg6a galGal6 GRCg6a_issues chicken/GRC/Issue_Mapping \
  > GRCg6a/log/${YM}/${DS}.txt 2>&1

You can verify the issues.gff3 file name at the NCBI FTP directory:

   https://ftp.ncbi.nlm.nih.gov/pub/grc/chicken/GRC/Issue_Mapping/

run the runUpdate.sh (optionally just the new one to avoid other issues)

Will need to add the symlink, e.g.:

/gbdb/galGal6/bbi/grcIncidentDb/galGal6.grcIncidentDb.bb -> /hive/data/outside/otto/grcIncidentDb/GRCg6a/GalGal6.grcIncidentDb.bb

And run the hgBbiDbLink command:

hgBbiDbLink galGal6 grcIncidentDb "/gbdb/galGal6/bbi/grcIncidentDb/galGal6.grcIncidentDb.bb"

Add the verification log check at the end of runUpdate.sh:

WC=`tail --quiet --lines=1 ${TOP}/GRCg6a/log/${YM}/${DS}.txt ${TOP}/GRCh37/log/${YM}/${DS}.txt ${TOP}/GRCh38/log/${YM}/${DS}.txt ${TOP}/GRCm38/log/${YM}/${DS}.txt ${TOP}/GRCz10/log/${YM}/${DS}.txt ${TOP}/GRCz11/log/${YM}/${DS}.txt ${TOP}/Gallus_gallus-5.0/log/${YM}/${DS}.txt ${TOP}/MGSCv37/log/${YM}/${DS}.txt ${TOP}/Zv9/log/${YM}/${DS}.txt | grep SUCCESS | wc -l`

And change the expected count:

if [ "${WC}" -ne 9 ]; then
    ${ECHO} "WC: ${WC} <- should be nine" 1>&2

######################################################################
OBSOLETE:

To start a new one:

hgsql -e 'create database tmpIncidentGRCh38;' tmpIncidentGRCh37

And a table in the target db, after the file is established
in the genomewiki::

hgBbiDbLink hg38 grcIncidentDb "http://genomewiki.ucsc.edu/images/7/7f/Hg38.grcIncidentDb.bb"

