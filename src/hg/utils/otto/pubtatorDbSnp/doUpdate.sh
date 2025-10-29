#! /bin/zsh -ex

year=$(date +%Y)
month=$(date +%m)
day=$(date +%d)
final_out_dir=out/${year}/${month}/${day}
mkdir -p $final_out_dir

LOG_FILE="$final_out_dir/script_output.log"

absolute_log=`readlink -f $LOG_FILE`

# Redirect both stdout and stderr to the log file
exec >${LOG_FILE} 2>&1

# Function to handle errors
function on_error {
  # Send the output via email if the script fails
  echo "Error occurred, see ${absolute_log} for details" | mail -s "pubtatorDbSnp BUILD Failure - $(date)" jbirgmei@ucsc.edu
  echo "An error occurred. Email sent to jbirgmei@ucsc.edu"

  [[ -n "$jobdir" ]] && rm -rf "$jobdir" || true
  [[ -n "$out_dir" ]] && rm -rf "$out_dir" || true
  [[ -n "$splits" ]] && rm -rf "$splits" || true
  [[ -n "$VENV_DIR" ]] && rm -rf "$VENV_DIR" || true

  exit 1
}

# Trap ERR to handle script errors
trap 'on_error' ERR


dbs=("hg19" "hg38")

# Check for --force flag
force=false
for arg in "$@"; do
  if [[ "$arg" == "--force" ]]; then
    force=true
    break
  fi
done

http_code=$(curl -w "%{http_code}" -z mutation2pubtator3.gz -o mutation2pubtator3.gz 'https://ftp.ncbi.nlm.nih.gov/pub/lu/PubTator3/mutation2pubtator3.gz' -s)

# Check the HTTP response code
if [[ "$http_code" -eq 200 || "$force" == true ]]; then
  echo "File was updated and downloaded or --force flag is set"
else
  echo "File was not modified"
  mail -s "pubtatorDbSnp BUILD - not modified - $(date)" jbirgmei@ucsc.edu < ${LOG_FILE}
  exit 0
fi

VENV_DIR=$(mktemp -d venv_XXXXXX)
/cluster/home/jbirgmei/opt/miniconda3/bin/python3 -m venv $VENV_DIR
source $VENV_DIR/bin/activate
$VENV_DIR/bin/python3 -m pip install nltk==3.8.1 sqlitedict==2.1.0 sumy==0.11.0 matplotlib==3.8.3

zcat mutation2pubtator3.gz | awk '$3 ~ /^rs[0-9]+$/' | cut -f 1,3 | uniq | sort -u | $VENV_DIR/bin/python3 ./datamash_collapse.py > rs_to_pmid.tsv

deactivate
rm -rf $VENV_DIR

# remove temp dirs and files:

cut -f 1 rs_to_pmid.tsv > rsIds.tsv
splits=`mktemp -d ./rsids_splits_XXXXXXXX`
split -l 2000 rsIds.tsv ${splits}/split_



for db in $dbs
do
  jobdir=`mktemp -d ./jobs_XXXXXXXX`
  out_dir=`mktemp -d ./out_XXXXXXXX`

  mkdir -p /scratch/tmp/pubtatorDbSnp
  cp /hive/data/outside/dbSNP/155/bigDbSnp.2023-03-26/${db}.dbSnp155.bb /scratch/tmp/pubtatorDbSnp
  
  for i in ${splits}/split_*
  do
      basename=`basename $i`
      full_path=`readlink -f $i`
      out=`readlink -f ${out_dir}/${basename}.bed`
      echo "bigBedNamedItems /scratch/tmp/pubtatorDbSnp/${db}.dbSnp155.bb -nameFile ${full_path} ${out}" >> ${jobdir}/joblist
  done
  
  full_jobdir=`readlink -f ${jobdir}`
  
  cat ${full_jobdir}/joblist | parallel -j 20
  
  cat ${out_dir}/* > rsids.${db}.bed
  
  # rm -rf $jobdir $out_dir
  
  export LC_ALL=C
  
  join -1 4 -2 1 -t $'\t' \
    <(sort -t $'\t' -k4,4 rsids.${db}.bed) \
    <(sort -t $'\t' -k1,1 rs_to_pmid.tsv) |
    awk -F $'\t' '{OFS="\t"; print $2, $3, $4, $1, 0, ".", $3, $4, $NF, $(NF-2), $(NF-1)}' |
    sort -t $'\t' -k1,1 -k2,2n > $final_out_dir/track.${db}.bed

  /cluster/bin/x86_64/bedToBigBed -as=pubtatorDbSnp.as -type=bed9+2 -tab $final_out_dir/track.${db}.bed /cluster/data/${db}/chrom.sizes $final_out_dir/pubtatorDbSnp.${db}.bb

  rm -f /gbdb/${db}/pubs2/pubtatorDbSnp.bb
  ln -s `readlink -f $final_out_dir/pubtatorDbSnp.${db}.bb` /gbdb/${db}/pubs2/pubtatorDbSnp.bb

  rm -rf $jobdir
  rm -rf $out_dir
  rm -f /scratch/tmp/pubtatorDbSnp/${db}.dbSnp155.bb
done

rm -rf $splits

echo "Script completed successfully, see ${absolute_log} for details" | mail -s "pubtatorDbSnp BUILD Success - $(date)" jbirgmei@ucsc.edu
