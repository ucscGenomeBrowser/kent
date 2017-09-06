This is the automated pipeline to update uniprot tracks from UniProt.org

UniProt updates its files every month, see http://www.uniprot.org/news/

Our process is this (doUpdate.sh):
- compare local date of uniprot_sprot.xml.gz in /hive/data/outside/uniprot against the file 
ftp://ftp.uniprot.org/pub/databases/uniprot/current_release/knowledgebase/complete/uniprot_sprot.xml.gz
- if the file has not been updated on the uniprot.org server, exit
- parse the uniprot data from XML to tab files, one per organism
- create an alignment uniprotId -> genome with MarkD's pslMap
- run uniprotLift and use this tab-sep file and the alignment to create the bigBed files
