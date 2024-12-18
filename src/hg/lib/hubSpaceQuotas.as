table hubSpaceQuotas
"Table for per-user quotas that differ from the default quota"
    (
    string userName; "userName in gbMembers"
    bigint quota; "the users quota in gigabytes"
    )
