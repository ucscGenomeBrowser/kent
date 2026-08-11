#!/bin/bash

# fail on any failed command
set -eEu -o pipefail

export TOP="/hive/data/inside/GenArk/asmAlias"

cd "${TOP}"

eval "$(date "+YMD=%F Y=%Y M=%m DS=%F_%T")"
export YMD Y M DS

export db="hgcentraltest"
export logDir="${TOP}/log/${Y}/${M}"
mkdir -p "${logDir}"
export logFile="${logDir}/${DS}.txt"
export prevAliasTsv="${logDir}/${YMD}.asmAlias.tsv.gz"
export updateTsv="${logDir}/update.${YMD}.tsv"

# do not overwrite this copy if it was already done today
if [ ! -s "${prevAliasTsv}" ]; then
  hgsql -N -e 'select * from asmAlias;' "${db}" \
    | gzip -c > "${prevAliasTsv}"
fi

printf "### cwd: %s\n" "`pwd -P`" >> "${logFile}"
printf "### logFile: log/${Y}/${M}/${DS}.txt.gz\n" >> "${logFile}"
printf "####### running the update command:\n" >> "${logFile}"
printf "time (./asmAliasUpdate.py -o update.${YMD}.tsv > update.${DS}.out 2> update.${DS}.err) >> logFile\n" >> "${logFile}"

time (./asmAliasUpdate.py -o "${updateTsv}" > ${logDir}/update.${DS}.out 2> ${logDir}/update.${DS}.err) >> "${logFile}" 2>&1

# watch for any new types of errors
unexpected=$(egrep -v "OBSOLETE|UPDATE|NEW ALIAS|NOTE" "${logDir}/update.${DS}.err" \
    | egrep -v "ing to browser=[a-z]" \
      | egrep -v "are not case-independent|skipping" \
        | grep ERROR || true)
if [ -n "${unexpected}" ]; then
    printf '%s\n' "${unexpected}" 1>&2
    echo "unexpected ERROR lines found, aborting before load" 1>&2
    exit 1
fi

hgLoadSqlTab "${db}" asmAliasUpdate ${TOP}/asmAlias.sql \
   "${updateTsv}" >> "${logFile}" 2>&1

gzip -f "${updateTsv}"
hgsql "${db}" -e 'DROP TABLE IF EXISTS asmAliasPrev;' >> "${logFile}" 2>&1
hgsql "${db}" -e "RENAME TABLE asmAlias TO asmAliasPrev, asmAliasUpdate TO asmAlias;" >> "${logFile}" 2>&1

printf "previous asmAlias size:\n" >> "${logFile}" 2>&1
hgsql -t "${db}" -e 'select count(*) from asmAliasPrev;' >> "${logFile}" 2>&1
printf "updated asmAlias size:\n" >> "${logFile}" 2>&1
hgsql -t "${db}" -e 'select count(*) from asmAlias;' >> "${logFile}" 2>&1
gzip -f "${logFile}" "${logDir}/update.${DS}.out" "${logDir}/update.${DS}.err"

printf "##############################################\n" 1>&2
printf "### asmAlias update change log ${DS}\n" 1>&2
zcat "${logFile}.gz" 1>&2
printf "##############################################\n" 1>&2
