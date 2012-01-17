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

struct pstate {
    int done;
    int coord_dimensions;
    std::string property_name;
    std::string geometry_type;
    mapnik::feature_ptr feature;
    std::vector< std::vector<double> > point_cache;
    parser_state state;
    pstate() :
        done(0),
        coord_dimensions(0),
        property_name(""),
        geometry_type(""),
        feature(),
        point_cache(),
        state()
    { };
};

class jit_featureset : public mapnik::Featureset
{
public:
    jit_featureset(mapnik::box2d<double> const& box,
            std::string input_string,
            std::string const& encoding);
    virtual ~jit_featureset();
    mapnik::feature_ptr next();

private:
    mapnik::box2d<double> box_;
    mutable unsigned int feature_id_;
    mutable std::string input_string_;
    boost::shared_ptr<mapnik::transcoder> tr_;
    mutable std::vector<mapnik::feature_ptr> features_;
    unsigned itt_;
    yajl_handle hand;
};

#endif // HELLO_FEATURESET_HPP
