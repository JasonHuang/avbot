/*
 * Copyright (C) 2013  microcai <microcai@fedoraproject.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#pragma once

#ifndef __GOLDPRICE_HPP_
#define __GOLDPRICE_HPP_

#include <boost/make_shared.hpp>
#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>

#include <avhttp.hpp>
#include <avhttp/async_read_body.hpp>

#include "extension.hpp"

namespace metal{

// 向 http://hq.sinajs.cn/?_=1369230050901/&list=hf_GC 查询金价.
template<class MsgSender>
struct metalprice_fetcher_op{

	metalprice_fetcher_op(boost::asio::io_service & _io_service, MsgSender _sender, std::string _metal)
	  : io_service(_io_service), sender(_sender), stream(new avhttp::http_stream(_io_service)), metal(_metal), buf(boost::make_shared<boost::asio::streambuf>())
	{
		std::string list;
		if (metal == "黄金")
			list = "hf_GC";
		else if ( metal == "白银")
			list = "hf_SI";
		if (list.empty()){
			sender( std::string( metal + " 无报价或avbot暂不支持"));
		}else{
			std::string url = boost::str(boost::format("http://hq.sinajs.cn/?_=%d&list=%s") % std::time(0) % list);
			avhttp::async_read_body(*stream, url, * buf, *this);
		}
	}

	void operator()(boost::system::error_code ec, std::size_t bytes_transfered)
	{

		if (!ec || ec == boost::asio::error::eof){
			// 读取 buf 内容, 执行 regex 匹配, 获取金价.
			std::string jscript;
			jscript.resize(bytes_transfered);
			buf->sgetn(&jscript[0], bytes_transfered);

			boost::cmatch what;
			boost::regex ex("var ([^=]*)=\"([0-9\\.\\-]*),([0-9\\.\\-]*),(.*)");
			if (boost::regex_search(jscript.c_str(), what, ex))
			{
				std::string price = what[2];
				std::string zhangfu = what[3];
				std::string msg = boost::str(boost::format("%s 当前价 %s 涨幅 %s%%") % metal % price % zhangfu);

				sender(msg);
			}
		}
	}

	boost::shared_ptr<boost::asio::streambuf>  buf;
	boost::asio::io_service & io_service;
	MsgSender sender;
	boost::shared_ptr<avhttp::http_stream> stream;
	std::string metal;
};

template<class MsgSender>
void metalprice_fetcher(boost::asio::io_service & io_service, MsgSender sender, std::string metal)
{
	metalprice_fetcher_op<MsgSender>(io_service, sender, metal);
}

}

class metalprice : avbotextension
{
private:

public:
	template<class MsgSender>
	metalprice(boost::asio::io_service & _io_service, MsgSender sender, std::string channel_name)
	  : avbotextension(_io_service, sender, channel_name)
	{
	}

	void operator()(const boost::system::error_code& error);

	void operator()(boost::property_tree::ptree msg)
	{
 		if( msg.get<std::string>( "channel" ) != m_channel_name )
			return;

		std::string textmsg = boost::trim_copy( msg.get<std::string>( "message.text" ) );

		boost::cmatch what;
		if (boost::regex_search(textmsg.c_str(), what, boost::regex(".qqbot (.*)报价")))
		{
			metal::metalprice_fetcher(io_service, m_sender, what[1]);
		}
	}
};

#endif // __GOLDPRICE_HPP_
