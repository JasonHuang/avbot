
/*
 * buddy manager - manager/update/cache the group/buddies information
 *
 * copyright 2013 microcai
 * copyright 2012 - 2013 avplayer.org
 */

#pragma once

#include <boost/asio.hpp>

#include <soci-sqlite3.h>
#include <boost-optional.h>
#include <boost-tuple.h>
#include <boost-gregorian-date.h>
#include <soci.h>

#include "../webqq.hpp"

namespace webqq {
namespace qqimpl {
namespace detail {

} // nsmespace detail

class buddy_mgr
{
public:
	buddy_mgr(std::string dbname  = ":memory:")
	{
		sqlite_api::sqlite3_enable_shared_cache(1);
		m_sql.open(soci::sqlite3, dbname);
		db_initialize();
	}

	void update_group_list(std::string gid, std::string name, std::string code);

	bool group_has_qqnum(std::string code);
	void map_group_qqnum(std::string code, std::string qqnum);

	qqGroup_ptr get_group_by_gid(std::string gid);
	qqGroup_ptr get_group_by_qq(std::string qqnum);

	std::vector<std::string> get_group_all_buddies_uin(std::string gid);

	qqBuddy_ptr get_buddy_by_uin(std::string uid);

	bool buddy_has_qqnum(std::string uid);
	void map_buddy_qqnum(std::string uid, std::string qqnum);

	void set_group_owner(std::string gid, std::string owner);

	void group_new_buddy(std::string gid, std::string uid, std::string qqnum, std::string nick);

	void buddy_update_mflag(std::string uid,  unsigned int mflag);
	void buddy_update_card(std::string uid, std::string card);

private:

	void db_initialize()
	{
		soci::transaction trans(m_sql);
		// 初始化存储数据库
		m_sql << ""
			"create table if not exists groups ( "
				"`gid` TEXT not null,"
				"`group_code` TEXT not null,"
				"`name` TEXT not null, "
				"`qqnum` TEXT, "
				"`owner` TEXT, "
				// last time that this group information retrived from TX
				// libwebqq will remove outdated one
				"`generate_time` TEXT not null"
			");";

		m_sql << ""
			"create table if not exists group_buddies ( "
				"`gid` TEXT not null,"
				"`uid` TEXT not null,"
				"`nick` TEXT,"
				"`card` TEXT,"
				"`mflag` INTEGER,"
				"`qqnum` TEXT"
			");";

		trans.commit();
	}

private:
	soci::session	m_sql;

};


} // nsmespace qqimpl
} // namespace webqq