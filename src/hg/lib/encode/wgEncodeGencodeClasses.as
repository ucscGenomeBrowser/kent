table wgEncodeGencodeClasses
"Class for wgEncode Gencode genes"
   (
    string geneId; "Gene Id"
    string transcriptId; "Transcript Id found in wgEncodeSangerGencodeGenes.name2"
    string geneName; "Gene Name"
    string transcriptName; "Transcript Name foune in wgEncodeSangerGencodeGenes.name"
    string geneType; "Gene Type (ie pseudogene)"
    string transcriptType; "Transcript Type (ie pseudogene)"
    string geneStatus; "Gene Status (ie Known)"
    string transcriptStatus; "Transcript Status (ie Known)"
    uint level; "Level: 1=validated, 2=Havana, 3=Ensembl"
    string class; "Class of gene.  enum('Validated_coding','Validated_processed','Validated_processed_pseudogene','Validated_unprocessed_pseudogene','Validated_pseudogene','Havana_coding','Havana_nonsense','Havana_non_coding','Havana_processed_pseudogene','Havana_unprocessed_pseudogene','Havana_pseudogene','Havana_TEC','Havana_polyA','Ensembl_coding','Ensembl_RNA','Ensembl_pseudogene','Undefined')"
   )
