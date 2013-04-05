/**
 * botcmd.cpp 实现 .qqbot 命令控制
 */

#include <string>
#include <algorithm>
#include <vector>
#include <memory>

#include <boost/regex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/date_time.hpp>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/noncopyable.hpp>
#include <boost/foreach.hpp>
#include <boost/locale.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/bind/protect.hpp>

#include <locale.h>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <wchar.h>
#if defined(_MSC_VER)
#include <direct.h>
#endif
#include "libirc/irc.h"
#include "libwebqq/webqq.h"
#include "libwebqq/url.hpp"
#include "libxmpp/xmpp.h"
#include "libmailexchange/mx.hpp"

#include "counter.hpp"
#include "logger.hpp"

#include "auto_question.hpp"
#include "messagegroup.hpp"
#include "botctl.hpp"
#include "selfexec.hpp"

#ifndef QQBOT_VERSION
#define QQBOT_VERSION "unknow"
#endif

static auto_question question;	// 自动问问题.

extern qqlog logfile;			// 用于记录日志文件.

static void mail_send_hander( const boost::system::error_code & ec, boost::function<void( std::string )> msg_sender )
{
	if( ec ) {
		msg_sender( "邮件没发送成功, 以上" );
	} else {
		msg_sender( "邮件发送成功, 以上" );
	}
}

static void iopost_msg( boost::asio::io_service& io_service, boost::function<void( std::string )> msg_sender, std::string msg )
{
	io_service.post( boost::bind( msg_sender, msg ) );
}

static void write_vcode(const std::string & vc_img_data)
{
	std::ofstream vcode("vercode.jpeg", std::ofstream::out);
	vcode.write(vc_img_data.data(), vc_img_data.size());
	vcode.close();
}

static boost::function<void (std::string)> do_vc_code;

static void handle_join_group(qqGroup_ptr group, bool needvc, const std::string & vc_img_data, webqq * qqclient, boost::function<void( std::string )> msg_sender)
{
	if (needvc && group){
		// 写入验证码.
		write_vcode(vc_img_data);
		std::string msg = boost::str(
				boost::format("哎呀，加入群%s的过程中要输入验证码，请使用 .qqbot vc XXXX 输入。文件为 qqlog 目录下的 vercode.jpeg") % group->qqnum
			);
		std::cout <<  msg <<  std::endl;
		msg_sender(msg);

		webqq::join_group_handler  join_group_handler = boost::bind(handle_join_group, _1, _2, _3, qqclient, msg_sender);
		do_vc_code = boost::bind(
							&webqq::join_group, qqclient,
								group,
								_1,
								join_group_handler
						);
	}else if (group && !needvc){
		msg_sender("哎呦妈呀，群加入了呢～记得修改 qqbotrc 将群添加到频道组哦～");
	}
}


static void handle_search_group(std::string groupqqnum, qqGroup_ptr group, bool needvc, const std::string & vc_img_data, webqq * qqclient, boost::function<void( std::string )> msg_sender)
{
	static qqGroup_ptr _group;

	if (needvc){
		// 写入验证码.
		write_vcode(vc_img_data);
		// 向大家吵闹输入验证码.
		std::string msg = boost::str(
				boost::format("哎呀，查找群%s的过程中要输入验证码，请使用 .qqbot vc XXXX 输入。文件为 qqlog 目录下的 vercode.jpeg") % groupqqnum
			);
		std::cout <<  msg <<  std::endl;
		msg_sender(msg);

		webqq::search_group_handler  search_group_handler = boost::bind(handle_search_group, groupqqnum, _1, _2, _3, qqclient, msg_sender);
		do_vc_code = boost::bind(
							&webqq::search_group, qqclient,
								groupqqnum,
								_1,
								search_group_handler
						);

	}else if (group){
		// 很好，加入群吧！
		msg_sender("哈呀，验证码正确了个去的，申请加入ing");
		qqclient->join_group(group, "", boost::bind(handle_join_group, _1, _2, _3, qqclient, msg_sender));
	}else{
		msg_sender("没找到没找到");
	}
}

//-------------

// 命令控制, 所有的协议都能享受的命令控制在这里实现.
// msg_sender 是一个函数, on_command 用它发送消息.
void on_bot_command( boost::asio::io_service& io_service, 
					std::string message,
					std::string from_channel,
					std::string sender,
					sender_flags sender_flag,
					boost::function<void( std::string )> msg_sender,
					webqq *qqclient )
{
	boost::regex ex;
	boost::cmatch what;
	qqGroup_ptr  group;

	boost::function<void( std::string )> sendmsg = boost::bind( iopost_msg, boost::ref( io_service ), msg_sender, _1 );

	std::vector< std::string > qqGroups;
	messagegroup* chanelgroup = find_group( from_channel );

	if( chanelgroup ) {
		qqclient = chanelgroup->qq_;
		qqGroups =  chanelgroup->get_qqgroups();
	}
	

	if( qqclient )
		group =  qqclient->get_Group_by_qq( from_channel.substr( 3 ) );

	if( message == ".qqbot help" ) {
		io_service.post(
			boost::bind( msg_sender, "可用的命令\n"
						 "\t.qqbot help\n"
						 "\t.qqbot version\n"
						 "\t.qqbot ping\n"
						 "\t.qqbot mail to \"emailaddress\"\n"
						 "\t 将命令中间的聊天内容发送到邮件 emailaddress,  注意引号\n"
						 "\t 使用 .qqbot mail subject 设置主题\n"
						 "\t.qqbot mail end\n"
						 "== 以下命令需要管理员才能使用==\n"
						 "\t.qqbot relogin 强制重新登录qq\n\t.qqbot reload 重新加载群成员列表\n"
						 "\t.qqbot begin class XXX\t\n\t.qqbot end class\n"
						 "\t.qqbot newbee SB\n"
						 "以上!" )
		);
	}

	if( message == ".qqbot ping" ) {
		sendmsg( "我还活着" );
		return;
	}

	if( message == ".qqbot version" ) {
		sendmsg( boost::str( boost::format( "我的版本是 %s (%s %s)" ) % QQBOT_VERSION % __DATE__ % __TIME__ ) );
		return;
	}

	ex.set_expression( ".qqbot mail to \"?(.*)\"?" );

	if( boost::regex_match( message.c_str(), what, ex ) ) {
		if( chanelgroup ) {
			// 进入邮件记录模式.
			chanelgroup->pimf.reset( new InternetMailFormat );
			chanelgroup->pimf->header["from"] = chanelgroup->mx_->mailaddres();
			chanelgroup->pimf->header["to"] = what[1];
			chanelgroup->pimf->header["subject"] = "send by avbot";
			chanelgroup->pimf->body = std::string( "" );
		}

		return;
	}

	ex.set_expression( ".qqbot mail subject \"?(.*)\"?" );

	if( boost::regex_match( message.c_str(), what, ex ) ) {
		if( chanelgroup && chanelgroup->pimf ) {
			// 进入邮件记录模式.
			chanelgroup->pimf->header["subject"] = what[1];
		}

		return;
	}

	ex.set_expression( ".qqbot mail end" );

	if( boost::regex_match( message.c_str(), what, ex ) ) {
		if( chanelgroup && chanelgroup->pimf ) {

			// 发送邮件内容.
			chanelgroup->mx_->async_send_mail(
				*chanelgroup->pimf,
				// 报告发送成功还是失败, 怎么报告?
				boost::bind( mail_send_hander, _1, msg_sender )
			);
			chanelgroup->pimf.reset();
		}

		return;
	}

	if( sender_flag == sender_is_op ) {
		if( qqclient && message == ".qqbot relogin" ) {
			io_service.post(
				boost::bind( &webqq::login, qqclient )
			);
			return;
		}

		if( message == ".qqbot exit" ) {
			exit( 0 );
		}

		if( message == ".qqbot reexec" ) {
			re_exec_self();
		}

		ex.set_expression( ".qqbot join group ([0-9]+)" );
		if (qqclient && boost::regex_match( message.c_str(), what, ex ) )
		{
			qqclient->search_group(what[1], "", boost::bind(handle_search_group, std::string(what[1]), _1, _2, _3, qqclient, msg_sender));
		}

		ex.set_expression( ".qqbot vc (.*)" );
		if (qqclient && boost::regex_match( message.c_str(), what, ex ) )
		{
			if ( do_vc_code)
				do_vc_code(what[1]);
			else{
				sendmsg("哈？输入验证码干嘛？");
			}
		}

		// 重新加载群成员列表.
		if( qqclient && message == ".qqbot reload" ) {
			if( qqGroups.empty() ) {
				sendmsg( "加载哪个群? 你没设置啊!" );
			} else {
				BOOST_FOREACH(std::string g, qqGroups)
				{
					group = qqclient->get_Group_by_qq(g);
					if (group)
						io_service.post( boost::bind( &webqq::update_group_member, qqclient , group) );
				}
				sendmsg( "群成员列表重加载" );
			}

			return;
		}

		// 开始讲座记录.
		ex.set_expression( ".qqbot begin class ?\"(.*)?\"" );

		if( qqclient && group && boost::regex_match( message.c_str(), what, ex ) ) {
			std::string title = what[1];

			if( title.empty() ) return ;

			if( !logfile.begin_lecture( group->qqnum, title ) ) {
				std::printf( "lecture failed!\n" );
			}

			return;
		}

		// 停止讲座记录.
		if( qqclient && message == ".qqbot end class" ) {
			logfile.end_lecture();
			return;
		}

		// 向新人问候.
		ex.set_expression( ".qqbot newbee ?(.*)?" );

		if( qqclient && boost::regex_match( message.c_str(), what, ex ) ) {
			std::string nick = what[1];

			if( nick.empty() )
				return;

			auto_question::value_qq_list list;
			list.push_back( nick );

			question.add_to_list( list );
			question.on_handle_message( msg_sender );
		}
	}
}
