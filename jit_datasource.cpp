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
#include <mapnik/box2d.hpp>
#include <mapnik/proj_transform.hpp>
// boost
#include <boost/make_shared.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <urdl/istream.hpp>
// file plugin
#include "jit_datasource.hpp"
#include "jit_featureset.hpp"

#ifdef MAPNIK_DEBUG
//#include <mapnik/timer.hpp>
#include <iomanip>
#include <sstream>
#endif
#include <string>
#include <algorithm>


using mapnik::datasource;
using mapnik::parameters;

DATASOURCE_PLUGIN(jit_datasource)

jit_datasource::jit_datasource(parameters const& params, bool bind)
    : datasource(params),
    type_(datasource::Vector),
    desc_(*params_.get<std::string>("type"),
        *params_.get<std::string>("encoding", "utf-8")),
    url_(*params_.get<std::string>("url", "")),
    minzoom_(0),
    maxzoom_(10),
    extent_() {
    if (url_.empty()) {
      throw mapnik::datasource_exception("JIT Plugin: missing <url> parameter");
    }
    if (bind) {
        this->bind();
    }
}

void jit_datasource::bind() const {
    if (is_bound_) return;

    mapnik::projection const merc =  mapnik::projection(MERCATOR_PROJ4);
    mapnik::projection const wgs84 = mapnik::projection("+proj=lonlat +datum=WGS84");
    
    // std::map<std::string, mapnik::parameters> statistics_;

    mapnik::proj_transform transformer(merc, wgs84);

    urdl::istream is(url_);
    if (!is)
    {
        throw mapnik::datasource_exception("JIT Plugin: TileJSON endpoint could not be reached.");
    }
    std::stringstream buffer;
    buffer << is.rdbuf();

    using boost::property_tree::ptree;
    ptree pt;
    read_json(buffer,pt);
    minzoom_ = pt.get<unsigned>("minzoom",0);
    maxzoom_ = pt.get<unsigned>("maxzoom",18);
    tileurl_ = pt.get<std::string>("vectors");
    
    std::vector<double> b;
    BOOST_FOREACH(const ptree::value_type& v, pt.get_child("bounds"))
    {        
        b.push_back(boost::lexical_cast<double>(v.second.data()));
    }
    
    if (b.size() == 4)
    {
        mapnik::box2d <double> latBox(b[0],b[1],b[2],b[3]);    
        transformer.backward(latBox);
        extent_ = latBox;
    } 
    else 
    {
#ifdef MAPNIK_DEBUG
        std::clog << "JIT Plugin: extent not found in TileJSON, setting to world.\n";
#endif
        extent_.init(-20037508.34, -20037508.34, 20037508.34, 20037508.34);
    }
    
    is_bound_ = true;
}

jit_datasource::~jit_datasource() { }

std::string const jit_datasource::name_="jit";

std::string jit_datasource::name() {
    return name_;
}

mapnik::datasource::datasource_t jit_datasource::type() const {
    return type_;
}

mapnik::box2d<double> jit_datasource::envelope() const {
    if (!is_bound_) bind();
    return extent_;
}

boost::optional<mapnik::datasource::geometry_t> jit_datasource::get_geometry_type() const {
    return boost::optional<mapnik::datasource::geometry_t>();
}

mapnik::layer_descriptor jit_datasource::get_descriptor() const {
    if (!is_bound_) bind();
    return desc_;
}

std::map<std::string, mapnik::parameters> jit_datasource::get_statistics() const {
    return statistics_;
}

mapnik::featureset_ptr jit_datasource::features(mapnik::query const& q) const {
    if (!is_bound_) bind();

    mapnik::projection const merc = mapnik::projection(MERCATOR_PROJ4);
    mapnik::projection const wgs84 = mapnik::projection("+proj=lonlat +datum=WGS84");
    mapnik::proj_transform transformer(merc, wgs84);
    mapnik::box2d <double> bb = q.get_unbuffered_bbox();
    transformer.forward(bb);
    
    if (bb.width() == 0) {
        // Invalid tiles mean we'll do dangerous math.
        return mapnik::featureset_ptr();
    }
    
    mapnik::coord2d c = bb.center();
    
    const double MAXEXTENT = 20037508.34;
    const double MERCA = 6378137;
    const double D2R = M_PI / 180.0;
    double mercwidth = (MERCA * bb.maxx() * D2R) -
      (MERCA * bb.minx() * D2R);
    
// FIXME !! ////////////////////////////////////////////////
    double z = abs(ceil(-(std::log(mercwidth / MAXEXTENT) -
      std::log(2.0)) / std::log(2.0)));
///////////////////////////////////////////////////////////
    
    // Also bail early if the datasource indicates that there
    // will be no tiles here.
    if (z > maxzoom_ || z < minzoom_) {
        return mapnik::featureset_ptr();
    }
    // passed transformed bbox (WGS84) and zoom level
    return boost::make_shared<jit_featureset>(bb, z, tileurl_, desc_.get_encoding());
}

mapnik::featureset_ptr
jit_datasource::features_at_point(mapnik::coord2d const& pt) const {
    if (!is_bound_) bind();

    // features_at_point is rarely used - only by custom applications,
    // so for this sample plugin let's do nothing...
    return mapnik::featureset_ptr();
}
