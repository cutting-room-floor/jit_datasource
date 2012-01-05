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

    std::clog << "binding on " << url_ << std::endl;

    CURL_LOAD_DATA* resp = grab_http_response(url_.c_str());

    if ((resp == NULL) || (resp->nbytes == 0))
    {
        throw mapnik::datasource_exception("JIT Plugin: TileJSON endpoint could not be reached.");
    }

    std::clog << "JIT: using curl response\n";

    char *blx = new char[resp->nbytes + 1];
    memcpy(blx, resp->data, resp->nbytes);
    blx[resp->nbytes] = '\0';

    std::string tjstring = boost::trim_left_copy(std::string(blx));

    delete[] blx;
    char errbuf[1024];
    errbuf[0] = 0;
    yajl_val node;
    yajl_val v;

    node = yajl_tree_parse(tjstring.c_str(), errbuf, sizeof(errbuf));

    if (node == NULL) {
        throw mapnik::datasource_exception("JIT Plugin: TileJSON endpoint invalid.");
    }

    const char * minzoom_path[] = { "minzoom", (const char *) 0 };
    v = yajl_tree_get(node, minzoom_path, yajl_t_number);
    minzoom_ = YAJL_GET_INTEGER(v);

    const char * maxzoom_path[] = { "maxzoom", (const char *) 0 };
    v = yajl_tree_get(node, maxzoom_path, yajl_t_number);
    maxzoom_ = YAJL_GET_INTEGER(v);

    const char * vectors_path[] = { "vectors", (const char *) 0 };
    v = yajl_tree_get(node, vectors_path, yajl_t_string);
    char* ts = YAJL_GET_STRING(v);
    tileurl_ = std::string(ts);

    // const char * type_path[] = { "data", "geometry_type", (const char *) 0 };
    // v = yajl_tree_get(node, type_path, yajl_t_string);
    // const char* type_c_str = YAJL_GET_STRING(v);
    // std::clog << type_c_str << "\n";

    // const char * extent_path[] = { "extent", (const char *) 0 };
    // v = yajl_tree_get(node, extent_path, yajl_t_array);

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

    std::clog << "JIT: requesting " << thisurl_ << "\n";
    CURL_LOAD_DATA* resp = grab_http_response(thisurl_.c_str());
    std::clog << "JIT: got " << thisurl_ << "\n";

    if ((resp != NULL) && (resp->nbytes > 0))
    {
        char *blx = new char[resp->nbytes + 1];
        memcpy(blx, resp->data, resp->nbytes);
        blx[resp->nbytes] = '\0';
        std::string dstring = std::string(blx);
        delete[] blx;

        return boost::make_shared<jit_featureset>(
            q.get_bbox(),
            dstring,
            desc_.get_encoding());
    }
    else
    {
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
