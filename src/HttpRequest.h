#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H





#include "curl/curl.h"

class HttpRequest
{
public:
    HttpRequest();
    ~HttpRequest();

protected:
    CURL* m_pCurl;
};

#endif // HTTPREQUEST_H
