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
#include <mapnik/util/geometry_to_wkt.hpp>
#include "spherical_mercator.hpp"
#include "downloader.hpp"

#include <string>
#include <vector>
// boost
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
// yajl
#include "yajl/yajl_parse.h"
#include "jit_featureset.hpp"

// urdl
#include <urdl/istream.hpp>

#ifdef MAPNIK_DEBUG
//#include <mapnik/timer.hpp>
#include <iomanip>
#include <sstream>
#endif

static int gj_start_map(void * ctx) {
    return 1;
}
//////////////

static int gj_map_key(void * ctx, const unsigned char* key, size_t t) {
    std::string key_ = std::string((const char*) key, t);
    pstate *cs = static_cast<pstate*>(ctx);
    if (cs->state == parser_in_properties) {
        cs->property_name = key_;
    } else {
        if (key_ == "features") {
            cs->state = parser_in_features;
        } else if (key_ == "geometry") {
            cs->state = parser_in_geometry;
        } else if ((cs->state == parser_in_geometry) &&
            (key_ == "type")) {
            cs->state = parser_in_type;
        } else if (key_ == "properties") {
            cs->state = parser_in_properties;
        } else if (key_ == "coordinates") {
            cs->state = parser_in_coordinates;
        }
    }
    return 1;
}

static int gj_end_map(void * ctx) {
    pstate *cs = static_cast<pstate*>(ctx);

    if ((cs->state == parser_in_properties) ||
        (cs->state == parser_in_geometry)) {
        cs->state = parser_in_feature;
    } else if (cs->state == parser_in_feature) {
        if (cs->geometry_type == "Point") {
            mapnik::geometry_type * pt;
            pt = new mapnik::geometry_type(mapnik::Point);
#ifdef MAPNIK_DEBUG
            std::clog << "first dimension size: " << cs->point_cache.size() << std::endl;
            std::clog << "second dimension size: " << cs->point_cache.at(0).size() << std::endl;
#endif
            pt->move_to(
                cs->point_cache.at(0).at(0),
                cs->point_cache.at(0).at(1));
            cs->feature->add_geometry(pt);
        } else if (cs->geometry_type == "Polygon") {
            mapnik::geometry_type * pt;
            pt = new mapnik::geometry_type(mapnik::Polygon);

            pt->set_capacity(cs->point_cache.at(0).size() / 2);
            pt->move_to(
                cs->point_cache.at(0).at(0),
                cs->point_cache.at(0).at(1));

            for (unsigned i = 2;
                i < cs->point_cache.at(0).size();
                i += 2) {
                pt->line_to(
                    cs->point_cache.at(0).at(i),
                    cs->point_cache.at(0).at(i + 1));
            }
            cs->feature->add_geometry(pt);
        } else if (cs->geometry_type == "LineString") {
            mapnik::geometry_type * pt;
            pt = new mapnik::geometry_type(mapnik::LineString);

            pt->set_capacity(cs->point_cache.at(0).size() / 2);
            pt->move_to(
                cs->point_cache.at(0).at(0),
                cs->point_cache.at(0).at(1));

            for (unsigned i = 2;
                i < cs->point_cache.at(0).size();
                i += 2) {
                pt->line_to(
                    cs->point_cache.at(0).at(i),
                    cs->point_cache.at(0).at(i + 1));
            }
            cs->feature->add_geometry(pt);
        } else if (cs->geometry_type == "MultiLineString") {
            for (unsigned i = 0; i < cs->point_cache.size(); i++) {
                mapnik::geometry_type * pt;
                pt = new mapnik::geometry_type(mapnik::LineString);
                pt->set_capacity(cs->point_cache.at(i).size() / 2);
                pt->move_to(
                    cs->point_cache.at(i).at(0),
                    cs->point_cache.at(i).at(1));

                for (unsigned j = 2;
                    j < cs->point_cache.at(i).size();
                    j += 2) {
                    pt->line_to(
                        cs->point_cache.at(i).at(j),
                        cs->point_cache.at(i).at(j + 1));
                }
                cs->feature->add_geometry(pt);
            }
        }
        cs->state = parser_in_features;
        cs->done = 1;
    }
    return 1;
}

static int gj_null(void * ctx) {
    pstate *cs = static_cast<pstate*>(ctx);
    if (cs->state == parser_in_properties) {
        cs->feature->put_new(cs->property_name, mapnik::value_null());
    }
    return 1;
}

static int gj_boolean(void * ctx, int x) {
    pstate *cs = static_cast<pstate*>(ctx);
    if (cs->state == parser_in_properties) {
        cs->feature->put_new(cs->property_name, x);
    }
    return 1;
}

static int gj_number(void * ctx, const char* str, size_t t) {
    pstate *cs = static_cast<pstate*>(ctx);
    double x = strtod(str, NULL);

    if (cs->state == parser_in_coordinates) {
        if (cs->coord_dimensions == 1) {
            cs->point_cache.at(0).push_back(x);
        } else if (cs->coord_dimensions == 2) {
            cs->point_cache.at(0).push_back(x);
        } else if (cs->coord_dimensions == 3) {
            cs->point_cache.back().push_back(x);
        }
    } else if (cs->state == parser_in_properties) {
        cs->feature->put_new(cs->property_name, x);
    }
    return 1;
}

static int gj_string(void * ctx, const unsigned char* str, size_t t) {
    pstate *cs = static_cast<pstate*>(ctx);
    std::string str_ = std::string((const char*) str, t);
    if (cs->state == parser_in_type) {
        cs->geometry_type = str_;
    } else if (cs->state == parser_in_properties) {
        UnicodeString ustr = cs->tr->transcode(str_.c_str());
        cs->feature->put_new(cs->property_name, ustr);
    }
    return 1;
}

static int gj_start_array(void * ctx) {
    pstate *cs = static_cast<pstate*>(ctx);
    if (cs->state == parser_in_coordinates) {
        cs->coord_dimensions++;
        if (cs->coord_dimensions == 1) {
            std::vector <double> sub_cache;
            cs->point_cache.push_back(sub_cache);
        }
    }
    return 1;
}

static int gj_end_array(void * ctx) {
    pstate *cs = static_cast<pstate*>(ctx);
    if (cs->state == parser_in_coordinates) {
        cs->coord_dimensions--;
        if (cs->coord_dimensions < 1) {
            cs->state = parser_in_geometry;
        }
    } else if (cs->state == parser_in_features) {
        cs->state = parser_outside;
    }
    return 1;
}

static yajl_callbacks callbacks = {
    gj_null,
    gj_boolean,
    NULL,
    NULL,
    gj_number,
    gj_string,
    gj_start_map,
    gj_map_key,
    gj_end_map,
    gj_start_array,
    gj_end_array
};

jit_featureset::jit_featureset(
    mapnik::box2d<double> const& bbox, int zoom,
    std::string const& tileurl,
    std::string const& encoding)
    : box_(bbox),
      feature_id_(1),
      tr_(new mapnik::transcoder(encoding)),
      features_(),
      hand() 
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
    
    mapnik::context_ptr ctx=boost::make_shared<mapnik::context_type>();
    
    
    BOOST_FOREACH ( std::string const& input_string, json_input)
    {
        pstate state_bundle;
        
        state_bundle.state = parser_outside;
        state_bundle.done = 0;
        
        hand = yajl_alloc(
            &callbacks, NULL,
            &state_bundle);
        
        yajl_config(hand, yajl_allow_comments, 1);
        yajl_config(hand, yajl_allow_trailing_garbage, 1);
        mapnik::feature_ptr feature(mapnik::feature_factory::create(ctx,feature_id_));
        state_bundle.feature = feature;
        
        for ( int itt = 0; itt < input_string.length(); ++itt) 
        {            
            int parse_result = yajl_parse(hand,
                                      (const unsigned char *)&input_string[itt], 1);
            
            if (parse_result == yajl_status_error) 
            {
                unsigned char *str = yajl_get_error(hand,
                                                    1,  (const unsigned char*) input_string.c_str(),
                                                    input_string.length());
                std::ostringstream errmsg;
                errmsg << "GeoJSON Plugin: invalid GeoJSON detected: " << (const char*) str << "\n";
                yajl_free_error(hand, str);
                throw mapnik::datasource_exception(errmsg.str());
            } 
            else if (state_bundle.done == 1) 
            {
                features_.push_back(state_bundle.feature);
                mapnik::feature_ptr
                    feature(mapnik::feature_factory::create(ctx,feature_id_++));        
                // reset
                state_bundle.point_cache.clear();
                state_bundle.done = 0;
                state_bundle.geometry_type = "";
                state_bundle.feature = feature;
            }
        }
        std::cerr << "SIZE = " << features_.size() << std::endl;
        yajl_free(hand);
    }
    
    
    feature_id_ = 0;

}

jit_featureset::~jit_featureset() { }

mapnik::feature_ptr jit_featureset::next() {
    
    //return mapnik::feature_ptr();
    feature_id_++;
    if (feature_id_ <= features_.size()) {
        return features_.at(feature_id_ - 1);
    } else {
        return mapnik::feature_ptr();
    }
}
