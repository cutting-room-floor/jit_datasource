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
// mapnik
#include <mapnik/feature_factory.hpp>
#include <mapnik/geometry.hpp>
#include <mapnik/datasource.hpp>
#include <mapnik/json/feature_collection_parser.hpp>
// boost
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>

// urdl
#include <urdl/istream.hpp>
// stl
#include <string>
#include <vector>

#include "jit_featureset.hpp"
#include "spherical_mercator.hpp"
#include "downloader.hpp"


#ifdef MAPNIK_DEBUG
//#include <mapnik/timer.hpp>
#include <iomanip>
#include <sstream>
#endif


jit_featureset::jit_featureset(
    mapnik::box2d<double> const& bbox, int zoom,
    std::string const& tileurl,
    std::string const& encoding)
    : box_(bbox),
      feature_id_(1),
      tr_(new mapnik::transcoder(encoding)),
      features_()
{
    
#ifdef MAPNIK_DEBUG
    // std::clog << "JIT Plugin: unbuffered bbox: " << bb << std::endl;
#endif

    mapnik::spherical_mercator<> merc;
    
    double x0 = bbox.minx();
    double y0 = bbox.maxy();
    double x1 = bbox.maxx();
    double y1 = bbox.miny();
    
    merc.to_pixels(x0,y0,zoom);    
    merc.to_pixels(x1,y1,zoom);

    const double tile_size = 256.0;
    int minx = int(x0/tile_size);
    int maxx = int(x1/tile_size) + 1;
    int miny = int(y0/tile_size);
    int maxy = int(y1/tile_size) + 1;
    std::cerr << minx << "<->" << maxx << "  " << miny <<"<->" << maxy << std::endl;    
    
    std::vector<std::string> json_input;
    int count=0;
#if 1
    {
        mapnik::tile_downloader downloader(json_input); // RAII
        for ( int x = minx; x < maxx; ++x)
        {
            for (int y = miny; y < maxy; ++y)
            {               
                std::string url = boost::replace_all_copy(
                    boost::replace_all_copy(boost::replace_all_copy(tileurl,
                    "{z}", boost::lexical_cast<std::string>(zoom)),
                    "{x}", boost::lexical_cast<std::string>(x)),
                    "{y}", boost::lexical_cast<std::string>(y));
                downloader.push(boost::protect(boost::bind(&mapnik::download_handler::sync_start, _1, url)));
                ++count;
            }        
        }
    }

#else
    
    {
        for ( int x = minx; x < maxx; ++x)
        {
            for (int y = miny; y < maxy; ++y)
            {               
                std::string url = boost::replace_all_copy(
                    boost::replace_all_copy(boost::replace_all_copy(tileurl,
                    "{z}", boost::lexical_cast<std::string>(zoom)),
                    "{x}", boost::lexical_cast<std::string>(x)),
                    "{y}", boost::lexical_cast<std::string>(y));
                std::cerr << url << std::endl;
                urdl::istream is(url);
                if (is)
                {
                    std::stringstream buffer;
                    buffer << is.rdbuf();
                    json_input.push_back(buffer.str());
                }
                ++count;
            }        
        }
    }
#endif    
    
    typedef std::string::const_iterator iterator_type;
    mapnik::context_ptr ctx = boost::make_shared<mapnik::context_type>();
    mapnik::json::feature_collection_parser<iterator_type> p(ctx,*tr_);
    
    BOOST_FOREACH ( std::string const& str, json_input)
    {        
        bool result = p.parse(str.begin(),str.end(), features_);
        if (!result) 
        {
            std::clog << "Failed parsing input JSON!" << std::endl;
        }
    }
}

jit_featureset::~jit_featureset() { }

mapnik::feature_ptr jit_featureset::next() 
{    
    feature_id_++;
    if (feature_id_ <= features_.size()) {
        return features_.at(feature_id_ - 1);
    } else {
        return mapnik::feature_ptr();
    }
}
