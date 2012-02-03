/*****************************************************************************
 *
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2011 Artem Pavlenko
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *****************************************************************************/

#ifndef MAPNIK_DOWNLOADER_HPP
#define MAPNIK_DOWNLOADER_HPP

#include <string>

#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/bind.hpp>
#include <boost/bind/protect.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
// mapnik
#include <mapnik/box2d.hpp>

// urdl
#include <urdl/istream.hpp>
#include <urdl/read_stream.hpp>

#include "spherical_mercator.hpp"

boost::mutex global_stream_lock;

namespace mapnik {

class download_handler
    : public boost::enable_shared_from_this<download_handler>
{
public:
    download_handler(boost::asio::io_service& io_service, std::vector<std::string> & tiles)
        : io_service_(io_service),
          tiles_(tiles),
          strand_(io_service),
          read_stream_(io_service)
    {
        read_stream_.set_option(urdl::http::user_agent("Urdl"));
    }
    void sync_start(urdl::url const& url)
    {
        global_stream_lock.lock(); 
        std::cerr << "START:" << url.path() << std::endl;
        std::cerr << "[" << boost::this_thread::get_id()
                  << "] Thread" << std::endl;  
        global_stream_lock.unlock(); 
        
        urdl::istream is(url);
        
        if (is)
        {
            is.open_timeout(10000);
            is.read_timeout(5000);
            std::stringstream buffer;
            buffer << is.rdbuf();
            
            global_stream_lock.lock(); 
            tiles_.push_back(buffer.str());
            global_stream_lock.unlock();
        }
        else
        {
            global_stream_lock.lock(); 
            std::cerr << "ERROR:sync_start " << is.error().message() << std::endl;
            global_stream_lock.unlock(); 
        }
        
    }

    void async_start(urdl::url const& url, std::string const& output_dir)
    {       
        
        global_stream_lock.lock();
        std::cerr << "START:" << url.path() << std::endl;
        global_stream_lock.unlock();
        
        file_ = output_dir + url.path();
        try 
        {                        
            boost::filesystem::path p(file_);
            if (!boost::filesystem::exists(p.parent_path()))
            {
                boost::filesystem::create_directories(p.parent_path());
            }
#if 0
// sync read            
            ofstream_.open(file_.c_str(), std::ios_base::out | std::ios_base::binary);
            read_stream_.open(url);
            for (;;)
            {
                char data[1024];
                boost::system::error_code ec;
                std::size_t length = read_stream_.read_some(boost::asio::buffer(data), ec);
                if (ec == boost::asio::error::eof)
                    break;
                if (ec)
                    throw boost::system::system_error(ec);
                ofstream_.write(data, length);
            }
            
            read_stream_.close();
            ofstream_.close();
            global_stream_lock.lock();        
            std::cerr << "DONE:" << file_ << std::endl;
            std::cerr << "[" << boost::this_thread::get_id()
                      << "] Thread Start" << std::endl;        
            std::cerr << file_ <<  " Done!" << std::endl;
            global_stream_lock.unlock();
#else       
// async     
            read_stream_.async_open(url, 
                                    strand_.wrap(boost::bind(&download_handler::handle_open,
                                                             shared_from_this(), _1)));
#endif
        }
        catch (boost::filesystem::filesystem_error const& ex)
        {
            std::cerr << ex.what() << '\n';
        }
        catch (...)
        {
            std::cerr << "Exception caught!!!" << std::endl;
        }
        
    }
    ~download_handler() 
    {
    }
private:
    void handle_open(const boost::system::error_code& ec)
    {  
        if (!ec)
        {
            ofstream_.open(file_.c_str(), std::ios_base::out | std::ios_base::binary);
            read_stream_.async_read_some(
                boost::asio::buffer(buffer_),
                strand_.wrap(boost::bind(&download_handler::handle_read,
                                         shared_from_this(), _1, _2)));
        }
        else
        {
            //global_stream_lock.lock(); 
            std::cerr << "Error start:" << ec.message() << std::endl;
            //global_stream_lock.unlock(); 
        }
    }

    void handle_read(const boost::system::error_code& ec, std::size_t length)
    {
        if (!ec)
        {
            ofstream_.write(buffer_, length);
            read_stream_.async_read_some(
                boost::asio::buffer(buffer_),
                strand_.wrap(boost::bind(&download_handler::handle_read,
                                         shared_from_this(), _1, _2)));
        }
        else if (ec == boost::asio::error::eof)
        {
            //global_stream_lock.lock();        
            std::cerr << "[" << boost::this_thread::get_id()
                      << "] Thread Start" << std::endl;        
            std::cerr << file_ <<  " Done!" << std::endl;
            //global_stream_lock.unlock();
        }
        else
        {
            //global_stream_lock.lock(); 
            std::cerr << "Error:" <<  ec.message() << std::endl;
            //global_stream_lock.unlock(); 
        }

    }
    boost::asio::io_service & io_service_;
    std::vector<std::string> & tiles_;
    boost::asio::strand strand_;
    urdl::read_stream read_stream_;
    std::string file_;
    std::ofstream ofstream_;
    char buffer_[1024];
};


class tile_downloader
{
public:
    
    tile_downloader(std::vector<std::string> & cont, int pool_size = 4)    
        : cont_(cont) 
    {
        work_.reset( new boost::asio::io_service::work(io_service_) );
        
        for ( int i = 0; i < pool_size; ++i)
        {
            threads_.create_thread( boost::bind(&boost::asio::io_service::run, &io_service_) );
        }
    }
    
    ~tile_downloader()
    {
        work_.reset(); 
        threads_.join_all();
    }
 
    template <typename TFunc>
    void push(TFunc fun)
    {
        io_service_.post(
            boost::bind( &tile_downloader::execute<TFunc>, this, fun )
            );
    }
    
private:
 
    template <typename TFunc>
    void execute(TFunc fun)
    {
        boost::shared_ptr<download_handler> d(new download_handler(io_service_,cont_));
        fun(d);
    }
 
private:
    boost::asio::io_service io_service_;
    std::vector<std::string> & cont_;
    boost::shared_ptr<boost::asio::io_service::work> work_;
    boost::thread_group threads_;
};
}
#endif // MAPNIK_DOWNLOADER_HPP
