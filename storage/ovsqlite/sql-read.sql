pragma busy_timeout = 10000;

pragma query_only = 1;

-- .getmisc
select value from misc
    where key=?1;

-- .get_groupinfo
select low, high, "count", flag_alias from groupinfo
    where deleted=0
        and groupname=?1;

-- .get_artinfo
select token
    from groupinfo
        natural join artinfo
    where deleted=0
        and groupname=?1
        and artnum=?2;

-- .list_articles_high
select artnum, arrived, expires, token
    from groupinfo
        natural join artinfo
    where deleted=0
        and groupname=?1
        and artnum>=?2
        and artnum<=?3
    order by artnum;

-- .list_articles_high_overview
select artnum, arrived, expires, token, overview
    from groupinfo
        natural join artinfo
    where deleted=0
        and groupname=?1
        and artnum>=?2
        and artnum<=?3
    order by artnum;
