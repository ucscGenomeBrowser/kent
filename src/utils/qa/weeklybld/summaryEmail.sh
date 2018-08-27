#!/bin/sh
#

# Called by doNewBranch.csh when the beta build is successfully completed
#   foreach USER (braney larrym angie hiram tdreszer kate)
#       ./summaryEmail.sh $USER | mail -s "Code summaries are due" $USER
#   end

author=${USER}
if [ $# -gt 0 ]; then
    author=${1}
fi
LASTNN=$((BRANCHNN - 1))

#git log --author=${author} v${LASTNN}_branch.1..v${BRANCHNN}_base --pretty=oneline > /dev/null
command="git log --stat --author=${author} --reverse v${LASTNN}_base..v${BRANCHNN}_base"

echo "To those who need reminding..."
echo ""
echo "v${BRANCHNN} has been built successfully on beta."
echo "If you have checked in changes that will affect this release, it is time to do your code summaries. They are due to Brian by 5pm tomorrow."
echo ""
echo "The end of this message contains the log of all your changes for this build."
echo ""
echo "If you prefer to review the git reports directly, visit these three links to see your changes:"
echo ""
echo "    http://genecats.soe.ucsc.edu/git-reports-history/v${BRANCHNN}/review/user/${author}/index.html"
echo "    http://genecats.soe.ucsc.edu/git-reports-history/v${BRANCHNN}/review2/user/${author}/index.html"
echo "    http://genecats.soe.ucsc.edu/git-reports-history/v${BRANCHNN}/branch/user/${author}/index.html"
echo ""
echo "Your bona fide build-meister"
echo ""
echo "- - - - - - - - - - - - - - - -"
echo "Here is the log of all your changes for this build obtained by running the following command:"
echo "    $command"
echo ""
$command
if [ $? -ne 0 ]; then
  echo No changes were found for ${author}
fi
