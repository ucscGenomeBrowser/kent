#!/bin/csh -ef

# encodeStatus - show or change status of an ENCODE data submission
#
# Requires that the new status be the correct one to follow the
# existing status.  Statuses that can be changed manually do not
# overlap statuses set by the pipeline automation.

# $Header: /projects/compbio/cvsroot/kent/src/hg/encode/encodeStatus/encodeStatus.csh,v 1.1 2008/02/10 02:29:43 kate Exp $

set usage = "usage: encodeStatus project-id|project-name [status]"

set pipelineDb = encpipeline_kate
   # when Q/A begins, use:
# set pipelineDb = encpipeline_beta
   # when deployed, use:
#set pipelineDb = encpipeline

# These status are set by DCC staff currently, and must be used
# in this order
set DISPLAYED_STATUS = "displayed"
set APPROVED_STATUS = "approved"
set REVIEWING_STATUS = "reviewing"
set RELEASED_STATUS = "released"

# Last status set by pipeline
set LOADED_STATUS = "loaded"

set newStatuses = ($DISPLAYED_STATUS $APPROVED_STATUS $REVIEWING_STATUS $RELEASED_STATUS)
set prevStatuses = ($LOADED_STATUS $newStatuses)

if ($#argv < 1 || $#argv > 2) then
    goto usage
endif

set project = "$1"
set newStatus = "$2"

seq 0 $project >& /dev/null
set isName = $status

if ($#argv == 1) then
    # Show project status
    if ($isName) then
        set query = "SELECT id, status, id FROM projects WHERE name='$project'"
    else
        set query = "SELECT id, status, name FROM projects WHERE id=$project"
    endif
    set projectInfo = `echo $query | hgsql $pipelineDb`
    if ("$projectInfo" == "") then
        echo "ERROR: Project '$project' not found"
        goto usage
    endif
    echo $query | hgsql $pipelineDb
    exit 0
endif

# Change project status in database
if ($isName) then
    set query = "SELECT status FROM projects WHERE name='$project'"
else
    set query = "SELECT status FROM projects WHERE id=$project"
endif
set oldStatus = `echo $query | hgsql -N $pipelineDb`
if ("$oldStatus" == "") then
    echo "ERROR: Project '$project' not found"
    goto usage
endif

foreach s ($newStatuses)
    if ("$s" == "$newStatus") then
        if ("$oldStatus" != "$prevStatuses[1]") then
            echo "ERROR: New status '$newStatus' cannot follow '$oldStatus'"
            goto usage
        endif
        if ($isName) then
            set query = "UPDATE projects SET status='$newStatus' \
                    WHERE name='$project'"
        else
            set query = "UPDATE projects SET status='$newStatus' \
                    WHERE id=$project"
        echo $query | hgsql $pipelineDb
        echo "Project '$project' successfully updated from '$oldStatus' to '$newStatus'"
        endif
        exit 0
    endif
shift prevStatuses
end

echo "Status '$newStatus' invalid"
goto usage
end

# Target of goto for all error conditions
usage:
    echo "$usage \
           where statuses (in order) = $newStatuses"
    exit 1

