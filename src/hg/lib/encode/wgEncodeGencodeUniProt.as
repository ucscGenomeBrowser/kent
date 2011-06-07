table wgEncodeGencodeUniProt
"GENCODE transcript to UniProt peptide mapping"
   (
    string transcriptId; "GENCODE transcript identifier"
    string acc; "UniProt/Swiss-Prot accession"
    string name; "UniProt/Swiss-Prot entry name"
    enum("SwissProt", "TrEMBL") dataset; "UniProt dataset"
   )
