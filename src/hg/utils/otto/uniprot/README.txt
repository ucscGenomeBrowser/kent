This is the automated pipeline to update uniprot tracks from UniProt.org

UniProt updates its files every month, see http://www.uniprot.org/news/

Our process is this (doUniprot):

- compare local date against ftp and do nothing is not new
- if the file has not been updated on the uniprot.org server, exit
- parse the uniprot data from XML to tab files, one per organism
- create an alignment uniprotId -> genome with MarkD's pslMap and pslSelect
  These are stored in the directory protToGenome
- for pslSelect, the process is described in detail in the track description page:
  use uniProt's annotated transcripts first, then try those annotated to the NCBI Entrez Gene,
  then finally let pslDnaFilter use the best match (so no entry for pslSelect, this is OK because
  of the -qPass option we use for pslSelect here)
- the MINALI global variable set how much proteins must align, it is set to 0.93 by default
- Creates an archive directory and a hub.txt for it
- Finally goes over assemblies and flips the bigBed files
- Produces detailed mapInfo.json files in the bigBed directory


