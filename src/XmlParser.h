#ifndef XMLPARSER_H
#define XMLPARSER_H

#include "FeedsFile.h"

class XmlParser
{
public:
    XmlParser();
    ~XmlParser();
   
    //int tryParser();
    int parseStream(LPRSS_ITEM pItem, LPRSS_FEED pFeed, const char* szStream, long lSize);
protected:
    xmlDocPtr m_xmlDoc;

    xmlNodePtr getFirst(xmlNodePtr pRoot, const xmlChar* szName);
};

#endif // XMLPARSER_H