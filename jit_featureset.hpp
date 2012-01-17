#ifndef JIT_FEATURESET_HPP
#define JIT_FEATURESET_HPP

#include <mapnik/datasource.hpp>
#include "yajl/yajl_parse.h"
#include <boost/scoped_ptr.hpp>
#include <vector>

enum parser_state {
    parser_outside,
    parser_in_featurecollection,
    parser_in_features,
    parser_in_feature,
    parser_in_geometry,
    parser_in_coordinates,
    parser_in_properties,
    parser_in_coordinate_pair,
    parser_in_type
};

struct fm {
    int done;
    int coord_dimensions;
    std::string property_name;
    std::string geometry_type;
    mapnik::feature_ptr feature;
    std::vector< std::vector<double> > point_cache;
    parser_state state;
    fm() :
        done(0),
        coord_dimensions(0),
        property_name(""),
        geometry_type(""),
        feature(),
        point_cache(),
        state()
    { };
};

// extend the mapnik::Featureset defined in include/mapnik/datasource.hpp
class jit_featureset : public mapnik::Featureset
{
public:
    // this constructor can have any arguments you need
    jit_featureset(mapnik::box2d<double> const& box,
            std::string input_string,
            std::string const& encoding);

    // desctructor
    virtual ~jit_featureset();

    // mandatory: you must expose a next() method, called when rendering
    mapnik::feature_ptr next();

private:
    // members are up to you, but these are recommended
    mapnik::box2d<double> box_;
    mutable int feature_id_;
    mutable std::string input_string_;
    boost::shared_ptr<mapnik::transcoder> tr_;

    mutable std::vector<mapnik::feature_ptr> features_;

    // parsing related
    unsigned itt_;
    yajl_handle hand;
    // fm state_bundle;
};

#endif // HELLO_FEATURESET_HPP
