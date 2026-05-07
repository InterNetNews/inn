pragma foreign_keys = 1;

pragma journal_mode = 'PERSIST';

pragma busy_timeout = 999999999;

-- .random
select randomblob(?1);

-- .getmisc
select value from misc
    where key=?1;

-- .setmisc
insert or replace into misc (key, value)
    values (?1, ?2);

-- .unsetmisc
delete from misc
    where key=?1;

-- .begin
begin immediate transaction;

-- .commit
commit transaction;

-- .rollback
rollback transaction;

-- .savepoint
savepoint article_group;

-- .release_savepoint
release savepoint article_group;

-- .rollback_savepoint
rollback to savepoint article_group;

-- .delete_journal
pragma journal_mode = 'DELETE';

-- .add_group
insert into groupinfo (groupname, flag_alias, low, high)
    values(?1, ?2, ?3, ?4);

-- .get_groupinfo
select low, high, "count", flag_alias from groupinfo
    where deleted=0
        and groupname=?1;

-- .update_groupinfo_flag_alias
update groupinfo
    set flag_alias = ?2
    where groupid=?1
        and flag_alias!=?2;

-- .set_group_deleted
update groupinfo
    set deleted = (select max(deleted) from groupinfo)+1
    where deleted=0
        and groupname=?1;

-- .lookup_groupinfo
select groupid, low, high, "count" from groupinfo
    where deleted=0
        and groupname=?1;

-- .list_groups
select groupid, groupname, low, high, "count", flag_alias from groupinfo
    where deleted=0
        and groupid>?1
    order by groupid;

-- .add_article
insert into artinfo (groupid, artnum, arrived, expires, token, overview)
    values(?1, ?2, ?3, ?4, ?5, ?6);

-- .update_groupinfo_add
update groupinfo
    set low = case when "count" then min(low, ?2) else ?2 end,
        high = case when "count" then max(high, ?2) else ?2 end,
        "count" = "count"+1
    where groupid=?1;

-- .get_artinfo
select token
    from groupinfo
        natural join artinfo
    where deleted=0
        and groupname=?1
        and artnum=?2;

-- .delete_article
delete from artinfo
    where groupid=?1
        and artnum=?2;

-- .update_groupinfo_delete_all
update groupinfo
    set low = max(high, 1),
        high = max(high-1, 0),
        "count" = ?2*0
    where groupid=?1;

-- .update_groupinfo_delete_low
update groupinfo
    set low = (select min(artnum) from artinfo
                  where artinfo.groupid=groupinfo.groupid),
        "count" = "count"-?2
    where groupid=?1;

-- .update_groupinfo_delete_middle
update groupinfo
    set "count" = "count"-?2
    where groupid=?1;

-- .start_expire_group
update groupinfo
    set expired = ?2
        where deleted=0
            and groupname=?1;

--
create temporary table expireart(
    artnum integer
        primary key);

-- .add_expireart
insert or ignore into expireart (artnum)
    values(?1);

-- .fill_expireart
insert or ignore into expireart (artnum)
    select artnum from artinfo
        where groupid=?1
        order by artnum
        limit ?2;

-- .expire_articles
delete from artinfo
    where groupid=?1
        and artnum in expireart;

-- .clear_expireart
delete from expireart;

-- .set_forgotten_deleted
update groupinfo
    set deleted = (select max(deleted) from groupinfo)+1
    where deleted=0
        and expired<?1;

-- .delete_empty_groups
delete from groupinfo
    where deleted>0
        and not exists
            (select * from artinfo
                where artinfo.groupid=groupinfo.groupid);

-- .get_deleted_groupid
select groupid from groupinfo
    where deleted>0
    order by deleted
    limit 1;

-- .delete_group
delete from groupinfo
    where groupid=?1;

-- .list_articles
select artnum, arrived, expires, token
    from groupinfo
        natural join artinfo
    where deleted=0
        and groupname=?1
        and artnum>=?2
    order by artnum;

-- .list_articles_overview
select artnum, arrived, expires, token, overview
    from groupinfo
        natural join artinfo
    where deleted=0
        and groupname=?1
        and artnum>=?2
    order by artnum;

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

