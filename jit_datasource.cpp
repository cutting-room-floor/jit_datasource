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

// curl
#include "./basiccurl.h"

// yajl
#include "yajl/yajl_tree.h"

// file plugin
#include "jit_datasource.hpp"
#include "jit_featureset.hpp"

#ifdef MAPNIK_DEBUG
#include <mapnik/timer.hpp>
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

    std::clog << "Rebinding to datasource\n";

    mapnik::projection const merc =  mapnik::projection(MERCATOR_PROJ4);
    mapnik::projection const wgs84 = mapnik::projection("+init=epsg:4326");
    mapnik::proj_transform transformer(merc, wgs84);

    CURL_LOAD_DATA* resp = grab_http_response(url_.c_str());

    if ((resp == NULL) || (resp->nbytes == 0)) {
        throw mapnik::datasource_exception("JIT Plugin: TileJSON endpoint could not be reached.");
    }

    char *blx = new char[resp->nbytes + 1];
    memcpy(blx, resp->data, resp->nbytes);
    blx[resp->nbytes] = '\0';

    std::string tjstring = boost::trim_left_copy(std::string(blx));

    delete[] blx;
    free(resp->data);
    char errbuf[1024];
    errbuf[0] = 0;
    yajl_val node;
    yajl_val v;

    node = yajl_tree_parse(tjstring.c_str(), errbuf, sizeof(errbuf));

    if (node == NULL) {
        throw mapnik::datasource_exception("JIT Plugin: TileJSON endpoint invalid.");
    }

    const char * minzoom_path[] = { "minzoom", (const char *) 0 };
    const char * maxzoom_path[] = { "maxzoom", (const char *) 0 };
    const char * vectors_path[] = { "vectors", (const char *) 0 };
    const char * type_path[] = { "geometry_type", (const char *) 0 };
    const char * fields_path[] = { "fields", (const char *) 0 };
    const char * bounds_path[] = { "bounds", (const char *) 0 };

    v = yajl_tree_get(node, minzoom_path, yajl_t_number);
    minzoom_ = YAJL_GET_INTEGER(v);

    v = yajl_tree_get(node, maxzoom_path, yajl_t_number);
    maxzoom_ = YAJL_GET_INTEGER(v);

    v = yajl_tree_get(node, vectors_path, yajl_t_string);
    char* ts = YAJL_GET_STRING(v);
    tileurl_ = std::string(ts);

    v = yajl_tree_get(node, type_path, yajl_t_string);
    if (v != NULL) {
      // const char* type_c_str = strdup(YAJL_GET_STRING(v));
      // TODO: assign geometry type
    } else {
      // std::clog << "geometry type undefined\n";
    }

    v = yajl_tree_get(node, fields_path, yajl_t_object);
    if (v != NULL) {
      for (unsigned i = 0; i < YAJL_GET_OBJECT(v)->len; i++) {
        std::string ft = std::string(strdup(
              YAJL_GET_STRING(YAJL_GET_OBJECT(v)->values[i])));
        std::string fn = std::string(strdup(
              YAJL_GET_OBJECT(v)->keys[i]));
        if (ft == "string") {
          desc_.add_descriptor(mapnik::attribute_descriptor(fn, mapnik::String));
        }
      }
    } else {
      // std::clog << "fields undefined\n";
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

int jit_datasource::type() const {
    return type_;
}

mapnik::box2d<double> jit_datasource::envelope() const {
    if (!is_bound_) bind();

    return extent_;
}

mapnik::layer_descriptor jit_datasource::get_descriptor() const {
    if (!is_bound_) bind();

    return desc_;
}

mapnik::featureset_ptr jit_datasource::features(mapnik::query const& q) const {
    if (!is_bound_) bind();

    mapnik::projection const merc = mapnik::projection(MERCATOR_PROJ4);
    mapnik::projection const wgs84 = mapnik::projection("+init=epsg:4326");
    mapnik::proj_transform transformer(merc, wgs84);

    mapnik::box2d <double> bb = q.get_unbuffered_bbox();

    transformer.forward(bb);

    if (bb.width() == 0) {
        // Invalid tiles mean we'll do dangerous math.
        return mapnik::featureset_ptr();
    }

#ifdef MAPNIK_DEBUG
    // std::clog << "JIT Plugin: unbuffered bbox: " << bb << std::endl;
#endif

    mapnik::coord2d c = bb.center();

    const double MAXEXTENT = 20037508.34;
    const double MERCA = 6378137;
    const double D2R = M_PI / 180.0;
    double mercwidth = (MERCA * bb.maxx() * D2R) -
      (MERCA * bb.minx() * D2R);
    double z = abs(ceil(-(std::log(mercwidth / MAXEXTENT) -
      std::log(2.0)) / std::log(2.0)));

    // Also bail early if the datasource indicates that there
    // will be no tiles here.
    if (z > maxzoom_ || z < minzoom_) {
        return mapnik::featureset_ptr();
    }

    double d = 256.0 * std::pow(2.0, z - 1.0);
    double Bc = (256.0 * std::pow(2.0, z)) / 360.0;
    double Cc = (256.0 * std::pow(2.0, z)) / (2 * M_PI);
    double f = std::min(std::max(std::sin(D2R * c.y), -0.99999999), 0.9999999);
    double x = floor(d + c.x * Bc);
    double y = d + ((0.5 * std::log((1.0 + f) / (1.0 - f))) * (Cc * -1.0));

    int tx = floor(x / 256);
    int ty = floor(y / 256);

    thisurl_ = boost::replace_all_copy(
        boost::replace_all_copy(
        boost::replace_all_copy(tileurl_,
            "{z}", boost::lexical_cast<std::string>(z)),
            "{x}", boost::lexical_cast<std::string>(tx)),
            "{y}", boost::lexical_cast<std::string>(ty));

#ifdef MAPNIK_DEBUG
    mapnik::progress_timer download_timer(std::clog, "download");
#endif
    CURL_LOAD_DATA* resp = grab_http_response(thisurl_.c_str());
#ifdef MAPNIK_DEBUG
    download_timer.stop();
    download_timer.discard();
#endif

    if ((resp != NULL) && (resp->nbytes > 0)) {
        char *blx = new char[resp->nbytes + 1];
        memcpy(blx, resp->data, resp->nbytes);
        blx[resp->nbytes] = '\0';
        std::string dstring = std::string(blx);
        delete[] blx;
        free(resp->data);

        return boost::make_shared<jit_featureset>(
            q.get_bbox(),
            dstring,
            desc_.get_encoding());
    } else {
        return mapnik::featureset_ptr();
    }
}

mapnik::featureset_ptr
jit_datasource::features_at_point(mapnik::coord2d const& pt) const {
    if (!is_bound_) bind();

    // features_at_point is rarely used - only by custom applications,
    // so for this sample plugin let's do nothing...
    return mapnik::featureset_ptr();
}
