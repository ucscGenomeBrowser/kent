#!/bin/sh
install_dir=$1
if [ ! -f "${install_dir}/source_tree_clean/kent/src/hg/inc/versionInfo.h" ]
then
	echo "Can not find: '${install_dir}/source_tree_clean/kent/src/hg/inc/versionInfo.h'"
	exit 255
else
	VER=`awk '{print $3}' ${install_dir}/source_tree_clean/kent/src/hg/inc/versionInfo.h | sed -e 's/\"//g'`
	echo "${VER}" > "${install_dir}/Version_${VER}.`date -I`"
fi
