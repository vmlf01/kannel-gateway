/* 
 * gw/wap-error.c - smart wap error handling 
 * 
 * Stipe Tolj <tolj@wapme-systems.de> 
 */ 

#include "gwlib/gwlib.h"
#include "wap/wsp.h"
#include "wap-error.h"

Octstr* error_requesting_back(Octstr *url, Octstr *referer)
{
    Octstr *wml;

    gw_assert(url != NULL);
    gw_assert(referer != NULL);

    wml = octstr_format(
            "<?xml version=\"1.0\"?>" \
            "<!DOCTYPE wml PUBLIC \"-//WAPFORUM//DTD 1.1//EN\" " \
            "\"http://www.wapforum.org/DTD/wml_1.1.xml\">" \
            "<wml><card title=\"Error\" ontimer=\"%s\">" \
            "<timer value=\"20\"/><p>Error: could not request URL `%s'.</p>" \
            "<p>Either the HTTP server is down or the request timed out. " \
            "Returning to previous page</p> "\
            "<p>--<br/>" GW_NAME "/%s</p></card></wml>",
            octstr_get_cstr(referer), octstr_get_cstr(url), GW_VERSION
          );

    return wml;
}

Octstr* error_requesting(Octstr *url)
{
    Octstr *wml;

    gw_assert(url != NULL);

    wml = octstr_format(
            "<?xml version=\"1.0\"?>" \
            "<!DOCTYPE wml PUBLIC \"-//WAPFORUM//DTD 1.1//EN\" " \
            "\"http://www.wapforum.org/DTD/wml_1.1.xml\">" \
            "<wml><card title=\"Error\">" \
            "<p>Error: could not request URL `%s'.</p>" \
            "<p>Either the HTTP server is down or the request timed out.</p>" \
            "<p>--<br/>" GW_NAME "/%s</p></card></wml>",
            octstr_get_cstr(url), GW_VERSION
          );

    return wml;
}

Octstr* error_converting(Octstr *url, Octstr *type)
{
    Octstr *wml;

    gw_assert(url != NULL && type != NULL);

    wml = octstr_format(
            "<?xml version=\"1.0\"?>" \
            "<!DOCTYPE wml PUBLIC \"-//WAPFORUM//DTD 1.1//EN\" " \
            "\"http://www.wapforum.org/DTD/wml_1.1.xml\">" \
            "<wml><card title=\"Error\">" \
            "<p>Error: could not convert or compile content-type `%s' " \
            "at URL `%s'.</p>" \
            "<p>Either there was no supported converting routine " \
            "or converter failed while processing.</p>" \
            "<p>--<br/>" GW_NAME "/%s</p></card></wml>",
            octstr_get_cstr(type), octstr_get_cstr(url), GW_VERSION
          );

    return wml;
}

