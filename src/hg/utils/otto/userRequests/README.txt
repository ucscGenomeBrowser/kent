
Method of operation:

ottoRequest.py - otto user cron job running each minute to watch the
               - ottoRequest table in hgcentral - when new entries are
               - detected it marks the table entry as pending and sends
               - out notification emails.  TBD: use the 'ottoRequestAlign.sh'
               - script to generate the arguments to 'kegAlignLastz.sh'

ottoRequestAlign.sh - given an 'id' number in the ottoRequest table, this
                    - will generate the arguments to: 'kegAlignLastz.sh' 
                    - to get the alignment started in galaxy
                    - uses the hgcentraltest.genark table and the file
                    - dbDb.name.clade.tsv to determine full assembly ID
                    - names and 'clades' for the kegAlignLastz.sh script:
                    - primate - mammal - other

TBD:  run the kegAlignLastz.sh with the generated arguments to get the
alignment started.


workflowMonitor.sh - after the galaxy WF has started, this script can
                   - check the status of the job and if it is done, then
                   - then the processing of the results will take place to
                   - construct the chain files.

File: dbDb.name.clade.tsv - used to map the UCSC database names into
                          - GenArk 'clades' to make the selection:
                          - primate - mammal - other
