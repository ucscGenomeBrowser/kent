foreach id (`cat $1`)
    hgsql encpipeline_prod  -NB -e "select id, status, data_type from projects where id='$id'"
end
