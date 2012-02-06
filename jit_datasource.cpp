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

// boost
#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>
// mapnik
#include <mapnik/box2d.hpp>
#include <mapnik/proj_transform.hpp>

#include <string>
#include <algorithm>

#include <urdl/istream.hpp>
// yajl
#include "yajl/yajl_tree.h"

// file plugin
#include "jit_datasource.hpp"
#include "jit_featureset.hpp"

#ifdef MAPNIK_DEBUG
//#include <mapnik/timer.hpp>
#include <iomanip>
#include <sstream>
#endif

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

    // Paths for yajl
    const char * minzoom_path[] = { "minzoom", (const char *) 0 };
    const char * maxzoom_path[] = { "maxzoom", (const char *) 0 };
    const char * vectors_path[] = { "vectors", (const char *) 0 };
    const char * type_path[] = { "data", "geometry_type", (const char *) 0 };
    const char * bounds_path[] = { "bounds", (const char *) 0 };
    const char * statistics_path[] = { "statistics", (const char *) 0 };

    mapnik::proj_transform transformer(merc, wgs84);

    urdl::istream is(url_);
    if (!is)
    {
        throw mapnik::datasource_exception("JIT Plugin: TileJSON endpoint could not be reached.");
    }
    std::stringstream buffer;
    buffer << is.rdbuf();
    std::string tjstring(buffer.str());
    boost::trim_left(tjstring);

    char errbuf[1024];
    yajl_val node;
    yajl_val v;

    node = yajl_tree_parse(tjstring.c_str(), errbuf, sizeof(errbuf));

    if (node == NULL) {
        throw mapnik::datasource_exception("JIT Plugin: TileJSON endpoint invalid.");
    }

    v = yajl_tree_get(node, minzoom_path, yajl_t_number);
    minzoom_ = YAJL_GET_INTEGER(v);

    v = yajl_tree_get(node, maxzoom_path, yajl_t_number);
    maxzoom_ = YAJL_GET_INTEGER(v);

    v = yajl_tree_get(node, vectors_path, yajl_t_string);
    char* ts = YAJL_GET_STRING(v);
    tileurl_ = std::string(ts);

    v = yajl_tree_get(node, type_path, yajl_t_string);
    if (v != NULL) {
      const char* type_c_str = strdup(YAJL_GET_STRING(v));
      geometry_type_string_ = std::string(type_c_str);
    }

    v = yajl_tree_get(node, statistics_path, yajl_t_object);
    if (v != NULL) {
        std::clog << "statistics defined\n";
        for (unsigned i = 0; i < YAJL_GET_OBJECT(v)->len; i++) {
            // std::string ft = std::string(strdup(
            //       YAJL_GET_STRING(YAJL_GET_OBJECT(v)->values[i])));
            std::string field_name = std::string(strdup(
                  YAJL_GET_OBJECT(v)->keys[i]));

            mapnik::parameters field_parameters;
            yajl_val field_stats_obj;
            field_stats_obj = YAJL_GET_OBJECT(v)->values[i];

            if (field_stats_obj != NULL && YAJL_IS_OBJECT(field_stats_obj)) {
                for (unsigned j = 0; j < YAJL_GET_OBJECT(field_stats_obj)->len; j++) {
                    std::string stat_name = std::string(strdup(
                          YAJL_GET_OBJECT(field_stats_obj)->keys[j]));

                    if (YAJL_IS_NUMBER(YAJL_GET_OBJECT(field_stats_obj)->values[j])) {
                        double val = strtod(YAJL_GET_NUMBER(YAJL_GET_OBJECT(field_stats_obj)->values[j]), NULL);
                        field_parameters[stat_name] = val;
                    }
                }
            }
            statistics_.insert(std::pair<std::string, mapnik::parameters>(field_name, field_parameters));
        }
    } else {
        std::clog << "statistics undefined\n";
    }

    v = yajl_tree_get(node, bounds_path, yajl_t_array);
    if ((v != NULL) && (YAJL_GET_ARRAY(v)->len == 4)) {
      mapnik::box2d <double> latBox = mapnik::box2d<double>(
          YAJL_GET_DOUBLE(YAJL_GET_ARRAY(v)->values[0]),
          YAJL_GET_DOUBLE(YAJL_GET_ARRAY(v)->values[1]),
          YAJL_GET_DOUBLE(YAJL_GET_ARRAY(v)->values[2]),
          YAJL_GET_DOUBLE(YAJL_GET_ARRAY(v)->values[3]));

      transformer.backward(latBox);
      extent_ = latBox;
    } else {
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
    boost::optional<mapnik::datasource::geometry_t> result;
    if (geometry_type_string_ == "linestring") {
        result.reset(mapnik::datasource::LineString);
    } else if (geometry_type_string_ == "point") {
        result.reset(mapnik::datasource::Point);
    } else if (geometry_type_string_ == "polygon") {
        result.reset(mapnik::datasource::Polygon);
    }
    return result;
}

mapnik::layer_descriptor jit_datasource::get_descriptor() const {
    if (!is_bound_) bind();
    return desc_;
}

mapnik::statistics_ptr jit_datasource::get_statistics() const {
    return boost::make_shared<mapnik::statistics>(statistics_);
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
