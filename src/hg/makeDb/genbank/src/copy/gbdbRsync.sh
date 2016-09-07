rsync -aLv /gbdb/genbank/data/processed/ hgnfs1:/gbdb/genbank/data/processed/ --include="*/*/*.fa" --exclude="*/*/*" 
rsync -aLv /data/tmp/grepIndex/hgFixed/  hgnfs1:/gbdb/genbank/grepIndex/hgFixed/
