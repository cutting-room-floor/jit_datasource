// file plugin
#include "jit_datasource.hpp"
#include "jit_featureset.hpp"

// curl
#include "basiccurl.h"

// yajl
#include "yajl/yajl_tree.h"

// boost
#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>

// mapnik
#include <mapnik/box2d.hpp>
#include <mapnik/proj_transform.hpp>
#include <mapnik/projection.hpp>

using mapnik::datasource;
using mapnik::parameters;

DATASOURCE_PLUGIN(jit_datasource)

jit_datasource::jit_datasource(parameters const& params, bool bind)
: datasource(params),
    type_(datasource::Vector),
    desc_(*params_.get<std::string>("type"),
        *params_.get<std::string>("encoding","utf-8")),
    url_(*params_.get<std::string>("url","")),
    extent_()
{
    if (url_.empty()) throw mapnik::datasource_exception("JIT Plugin: missing <url> parameter");
    if (bind)
    {
        this->bind();
    }
}

mapnik::projection const *_merc =  new mapnik::projection("+init=epsg:3857");
mapnik::projection const *_wgs84 = new mapnik::projection("+init=epsg:4326");
mapnik::proj_transform const *transformer_ = new mapnik::proj_transform(*_merc, *_wgs84);

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

    mapnik::box2d <double> bb = q.get_bbox();
    transformer_->forward(bb);

    // stolen from mm
    double px = M_PI * bb.minx() / 180.0;
    double py = M_PI * bb.miny() / 180.0;
    py = std::log(std::tan(0.25 * M_PI + 0.5 * py));
    double ax = 0.15915494309189535;
    double by = -0.15915494309189535;
    px = ax * px + 0 * py + 0.5;
    py = 0 * px + by * py + 0.5;

    double zoom = floor((
        std::log(2) -
        std::log(q.get_bbox().width() / (20037508.34 * 2)) /
        std::log(2)));

    double power = std::pow(2.0, zoom);

    px = ceil(px * power);
    py = ceil(py * power);

    thisurl_ = boost::replace_all_copy(
        boost::replace_all_copy(
        boost::replace_all_copy(url_,
                "{z}", boost::lexical_cast<std::string>(zoom)),
                "{x}", boost::lexical_cast<std::string>(px)),
                "{y}", boost::lexical_cast<std::string>(py));

    // std::clog << thisurl_ << "\n";
    std::clog << "fetching\n";
    std::clog << thisurl_ << "\n";
    CURL_LOAD_DATA* resp = grab_http_response(thisurl_.c_str());

    std::clog << "fetched\n";
    char errbuf[1024];
    yajl_val node;

    if (resp != NULL)
    {
        char *blx = new char[resp->nbytes + 1];
        memcpy(blx, resp->data, resp->nbytes);
        blx[resp->nbytes] = '\0';

        delete[] blx;

        std::string dstring = boost::trim_left_copy(std::string(resp->data));

        std::clog << dstring;
        std::clog << "starting parse\n";

        node = yajl_tree_parse((const char *) dstring.c_str(), errbuf, sizeof(errbuf));

        /* parse error handling */
        if (node == NULL) {
            std::clog << "parse_error: " << "\n";
            if (strlen(errbuf)) {
                fprintf(stderr, " %s", errbuf);
            }
            else fprintf(stderr, "unknown error");
            fprintf(stderr, "\n");
        }

        std::clog << "looking good\n";
    }
    else
    {
        std::clog << "fail" << "\n";
    }

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
