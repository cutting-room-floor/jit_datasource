// mapnik
#include <mapnik/feature_factory.hpp>
#include <mapnik/geometry.hpp>

// yajl
#include "yajl/yajl_parse.h"

#include "jit_featureset.hpp"

mapnik::transcoder* tr = new mapnik::transcoder("utf-8");

static int gj_start_map(void * ctx)
{
    return 1;
}

static int gj_map_key(void * ctx, const unsigned char* key, size_t t)
{
    std::string key_ = std::string((const char*) key, t);
    if (((fm *) ctx)->state == parser_in_properties)
    {
        ((fm *) ctx)->property_name = key_;
    }
    else
    {
        if (key_ == "features")
        {
            ((fm *) ctx)->state = parser_in_features;
        }
        else if (key_ == "geometry")
        {
            ((fm *) ctx)->state = parser_in_geometry;
        }
        else if ((((fm *) ctx)->state == parser_in_geometry) && (key_ == "type"))
        {
            ((fm *) ctx)->state = parser_in_type;
        }
        else if (key_ == "properties")
        {
            ((fm *) ctx)->state = parser_in_properties;
        }
        else if (key_ == "coordinates")
        {
            ((fm *) ctx)->state = parser_in_coordinates;
        }
    }
    return 1;
}

static int gj_end_map(void * ctx)
{
    if ((((fm *) ctx)->state == parser_in_properties) ||
        (((fm *) ctx)->state == parser_in_geometry))
    {
        ((fm *) ctx)->state = parser_in_feature;
    }
    else if (((fm *) ctx)->state == parser_in_feature)
    {
        if (((fm *) ctx)->geometry_type == "Point")
        {
            mapnik::geometry_type * pt;
            pt = new mapnik::geometry_type(mapnik::Point);
            pt->move_to(
                ((fm *) ctx)->point_cache.at(0),
                ((fm *) ctx)->point_cache.at(1));
            ((fm *) ctx)->feature->add_geometry(pt);
        }
        if (((fm *) ctx)->geometry_type == "LineString")
        {
            mapnik::geometry_type * pt;
            pt = new mapnik::geometry_type(mapnik::LineString);

            pt->set_capacity(((fm *) ctx)->point_cache.size() / 2);
            pt->move_to(
                ((fm *) ctx)->point_cache.at(0),
                ((fm *) ctx)->point_cache.at(1));

            for (int i = 2; i < ((fm *) ctx)->point_cache.size(); i += 2) {
                pt->line_to(
                    ((fm *) ctx)->point_cache.at(i),
                    ((fm *) ctx)->point_cache.at(i + 1));
            }
            ((fm *) ctx)->feature->add_geometry(pt);
        }
        ((fm *) ctx)->state = parser_in_features;
        ((fm *) ctx)->done = 1;
    }
    return 1;
}

static int gj_null(void * ctx)
{
    if (((fm *) ctx)->state == parser_in_properties)
    {
        boost::put(*((fm *) ctx)->feature, ((fm *) ctx)->property_name, mapnik::value_null());
    }
    return 1;
}

static int gj_boolean(void * ctx, int x)
{
    if (((fm *) ctx)->state == parser_in_properties)
    {
        boost::put(*((fm *) ctx)->feature, ((fm *) ctx)->property_name, x);
    }
    return 1;
}

static int gj_number(void * ctx, const char* str, size_t t)
{
    std::string str_ = std::string((const char*) str, t);
    double x = boost::lexical_cast<double>(str_);

    if (((fm *) ctx)->state == parser_in_coordinates)
    {
        ((fm *) ctx)->point_cache.push_back(x);
    }
    if (((fm *) ctx)->state == parser_in_properties)
    {
        boost::put(*((fm *) ctx)->feature, ((fm *) ctx)->property_name, x);
    }
    return 1;
}

static int gj_string(void * ctx, const unsigned char* str, size_t t)
{
    std::string str_ = std::string((const char*) str, t);
    if (((fm *) ctx)->state == parser_in_type)
    {
        ((fm *) ctx)->geometry_type = str_;
    }
    else if (((fm *) ctx)->state == parser_in_properties)
    {
        UnicodeString ustr = tr->transcode(str_.c_str());
        boost::put(*((fm *) ctx)->feature, ((fm *) ctx)->property_name, ustr);
    }
    return 1;
}

static int gj_start_array(void * ctx)
{
    if (((fm *) ctx)->state == parser_in_coordinates)
    {
        ((fm *) ctx)->coord_dimensions++;
    }
    return 1;
}

static int gj_end_array(void * ctx)
{
    if (((fm *) ctx)->state == parser_in_coordinates)
    {
        ((fm *) ctx)->coord_dimensions--;
        if (((fm *) ctx)->coord_dimensions < 1)
        {
            ((fm *) ctx)->state = parser_in_geometry;
        }
    }
    else if (((fm *) ctx)->state == parser_in_features)
    {
        ((fm *) ctx)->state = parser_outside;
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
    mapnik::box2d<double> const& box,
    std::string input_string,
    std::string const& encoding)
    : box_(box),
      feature_id_(1),
      input_string_(input_string),
      tr_(new mapnik::transcoder(encoding)) {

    state_bundle.state = parser_outside;
    state_bundle.done = 0;

    // FIXME: manually free
    hand = yajl_alloc(
        &callbacks, NULL,
        &state_bundle);

    yajl_config(hand, yajl_allow_comments, 1);

    mapnik::feature_ptr feature(mapnik::feature_factory::create(feature_id_));
    state_bundle.feature = feature;

    for (; itt_ < input_string_.length(); itt_++) {

        int parse_result;

        parse_result = yajl_parse(hand,
                (const unsigned char*) &input_string_[itt_], 1);

        if (parse_result == yajl_status_error)
        {
            // TODO: better error reporting
            //char* s;
            //unsigned char *str = yajl_get_error(hand, 1,  (const unsigned char*) s, strlen(s));
            // throw mapnik::datasource_exception("GeoJSON Plugin: invalid GeoJSON detected:" +
            //             std::string((const char*) str));
            throw mapnik::datasource_exception("GeoJSON Plugin: invalid GeoJSON detected");
            // yajl_free_error(hand, str);
        }
        else if (state_bundle.done == 1)
        {
            features_.push_back(state_bundle.feature);

            feature_id_++;
            mapnik::feature_ptr feature(mapnik::feature_factory::create(feature_id_));

            // reset
            state_bundle.point_cache.clear();
            state_bundle.done = 0;
            state_bundle.geometry_type = "";
            state_bundle.feature = feature;

        }

    }
}

jit_featureset::~jit_featureset() { }

mapnik::feature_ptr jit_featureset::next()
{
    feature_id_++;
    if (feature_id_ <= features_.size())
    {
        return features_.at(feature_id_ - 1);
    }
    else
    {
        return mapnik::feature_ptr();
    }
}
