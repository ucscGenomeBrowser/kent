#!/bin/sh
#

# Called by doNewBranch.csh when the beta build is successfully completed
#   foreach USER (braney larrym angie hiram tdreszer kate)
#       ./summaryEmail.sh $USER | mail -s "Code summaries are due" $USER
#   end

victim=${USER}
if [ $# -gt 0 ]; then
    victim=${1}
fi
LASTNN=$((BRANCHNN - 1))

git log --author=${victim} v${LASTNN}_branch.1..v${BRANCHNN}_base --quiet
if [ $? -eq 0 ]; then
  echo nothing to report for ${victim}
  exit 0
fi
echo "To those who need reminding..."
echo ""
echo "v${BRANCHNN} has been built successfully on beta.  Time to do your code summaries."
echo ""
echo "The end of this message contains the log of all your changes for this build."
echo ""
echo "If you prefer to review the git reports directly, visit these three links to see your changes:"
echo ""
echo "    http://genecats.cse.ucsc.edu/git-reports-history/v${BRANCHNN}/review/user/${victim}/index.html"
echo "    http://genecats.cse.ucsc.edu/git-reports-history/v${BRANCHNN}/review2/user/${victim}/index.html"
echo "    http://genecats.cse.ucsc.edu/git-reports-history/v${BRANCHNN}/branch/user/${victim}/index.html"
echo ""
echo "Your bona fide build-meister"
echo ""
echo "- - - - - - - - - - - - - - - -"
echo "Here is the log of all your changes for this build obtained by running the following command:"
echo "    git log --author=${victim} v${LASTNN}_branch.1..v${BRANCHNN}_base"
echo ""
git log --author=${victim} v${LASTNN}_branch.1..v${BRANCHNN}_base
