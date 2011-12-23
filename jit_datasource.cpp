// file plugin
#include "jit_datasource.hpp"
#include "jit_featureset.hpp"

// curl
#include "basiccurl.h"

// boost
#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>

// mapnik
#include <mapnik/box2d.hpp>
#include <mapnik/proj_transform.hpp>
#include <mapnik/projection.hpp>

// yajl
#include "yajl/yajl_tree.h"

using mapnik::datasource;
using mapnik::parameters;

DATASOURCE_PLUGIN(jit_datasource)

jit_datasource::jit_datasource(parameters const& params, bool bind)
: datasource(params),
    type_(datasource::Vector),
    desc_(*params_.get<std::string>("type"),
        *params_.get<std::string>("encoding","utf-8")),
    url_(*params_.get<std::string>("url","")),
    minzoom_(0),
    maxzoom_(10),
    extent_()
{
    if (url_.empty()) throw mapnik::datasource_exception("JIT Plugin: missing <url> parameter");
    if (bind)
    {
        this->bind();
    }
}

void jit_datasource::bind() const
{
    if (is_bound_) return;

    CURL_LOAD_DATA* resp = grab_http_response(url_.c_str());

    if ((resp == NULL) || (resp->nbytes == 0))
    {
        throw mapnik::datasource_exception("JIT Plugin: TileJSON endpoint invalid");
    }

    char *blx = new char[resp->nbytes + 1];
    memcpy(blx, resp->data, resp->nbytes);
    blx[resp->nbytes] = '\0';

    delete[] blx;

    std::string tjstring = boost::trim_left_copy(std::string(resp->data));
    char errbuf[1024];
    errbuf[0] = 0;
    yajl_val node;
    yajl_val v;

    node = yajl_tree_parse(tjstring.c_str(), errbuf, sizeof(errbuf));

    const char * minzoom_path[] = { "minzoom", (const char *) 0 };
    minzoom_ = YAJL_GET_INTEGER(yajl_tree_get(node, minzoom_path, yajl_t_number));

    const char * maxzoom_path[] = { "maxzoom", (const char *) 0 };
    minzoom_ = YAJL_GET_INTEGER(yajl_tree_get(node, maxzoom_path, yajl_t_number));

    // const char * extent_path[] = { "extent", (const char *) 0 };
    // yajl_t_array ex;
    // *ex = YAJL_GET_ARRAY(yajl_tree_get(node, maxzoom_path, yajl_t_array));
    // std::clog << ex;

    extent_.init(-20037508.34,-20037508.34,20037508.34,20037508.34);

    is_bound_ = true;
}

jit_datasource::~jit_datasource() { }

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

    mapnik::box2d <double> bb = q.get_unbuffered_bbox();


    if (bb.width() == 0) {
        // Invalid tiles mean we'll do dangerous math.
        return mapnik::featureset_ptr();
    }

    mapnik::coord2d c = bb.center();

    const double MAXEXTENT = 20037508.34;
    const double MERCA = 6378137;
    const double D2R = M_PI / 180.0;
    double mercwidth = (MERCA * bb.maxx() * D2R) - (MERCA * bb.minx() * D2R);
    double z = abs(ceil(-(std::log(mercwidth / MAXEXTENT) - std::log(2.0)) / std::log(2.0)));
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
        boost::replace_all_copy(url_,
                "{z}", boost::lexical_cast<std::string>(z)),
                "{x}", boost::lexical_cast<std::string>(tx)),
                "{y}", boost::lexical_cast<std::string>(ty));

    CURL_LOAD_DATA* resp = grab_http_response(thisurl_.c_str());

    if ((resp != NULL) && (resp->nbytes > 0))
    {
        char *blx = new char[resp->nbytes + 1];
        memcpy(blx, resp->data, resp->nbytes);
        blx[resp->nbytes] = '\0';

        delete[] blx;

        std::string dstring = boost::trim_left_copy(std::string(resp->data));
        return boost::make_shared<jit_featureset>(
                q.get_bbox(),
                dstring,
                desc_.get_encoding());
    }
    else
    {
        std::clog << "null ptr\n";
        return mapnik::featureset_ptr();
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
