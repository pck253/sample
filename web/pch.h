#ifndef PCH_H
#define PCH_H

#define WEB_MODULE 1

#include "common.h"

#include "cpprest/http_listener.h"
using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;

#include "curl/curl.h"

#include "public/web_accessor.h"

#include "restful/restful.h"
#include "web_accessor_impl.h"
#include "web.h"

#endif //PCH_H
