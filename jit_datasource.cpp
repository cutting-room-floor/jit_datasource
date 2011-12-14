// file plugin
#include "jit_datasource.hpp"
#include "jit_featureset.hpp"

// boost
#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>

// mapnik
#include <mapnik/box2d.hpp>

using mapnik::datasource;
using mapnik::parameters;

DATASOURCE_PLUGIN(jit_datasource)

jit_datasource::jit_datasource(parameters const& params, bool bind)
: datasource(params),
    type_(datasource::Vector),
    desc_(*params_.get<std::string>("type"),
        *params_.get<std::string>("encoding","utf-8")),
    url_(*params_.get<std::string>("url")),
    extent_()
{
    if (bind)
    {
        this->bind();
    }
}

void jit_datasource::bind() const
{
    if (is_bound_) return;

    extent_.init(-20037508.34,-20037508.34,20037508.34,20037508.34);

    is_bound_ = true;
}

jit_datasource::~jit_datasource() { }

// This name must match the plugin filename, eg 'jit.input'
std::string const jit_datasource::name_="jit";

std::string jit_datasource::name()
{
    return name_;
}

int jit_datasource::type() const
{
    return type_;
}

mapnik::box2d<double> jit_datasource::envelope() const
{
    if (!is_bound_) bind();

    return extent_;
}

mapnik::layer_descriptor jit_datasource::get_descriptor() const
{
    if (!is_bound_) bind();

    return desc_;
}

mapnik::featureset_ptr jit_datasource::features(mapnik::query const& q) const
{
    if (!is_bound_) bind();

    // Given a width in meters, find the zoom level
    int z = -(floor(std::log(q.get_bbox().width() / (20037508.34 * 2)) / std::log(2)));

    double meters_per_tile = (20037508.34 * 2) / std::pow(2.0, z);

    int y = ceil(q.get_bbox().miny() / meters_per_tile);
    int x = ceil(q.get_bbox().miny() / meters_per_tile);

    std::cout << z << "," << x << "," << y << "\n";

    std::string _thisurl = boost::replace_all_copy(
    boost::replace_all_copy(
    boost::replace_all_copy(url_,
            "{z}", boost::lexical_cast<std::string>(z)),
            "{x}", boost::lexical_cast<std::string>(x)),
            "{y}", boost::lexical_cast<std::string>(y));

    std::cout << _thisurl << "\n";


    // if the query box intersects our world extent then query for features
    if (extent_.intersects(q.get_bbox()))
    {
        return boost::make_shared<jit_featureset>(q.get_bbox(), desc_.get_encoding());
    }

    // otherwise return an empty featureset pointer
    return mapnik::featureset_ptr();
}

mapnik::featureset_ptr jit_datasource::features_at_point(mapnik::coord2d const& pt) const
{
    if (!is_bound_) bind();

    // features_at_point is rarely used - only by custom applications,
    // so for this sample plugin let's do nothing...
    return mapnik::featureset_ptr();
}
