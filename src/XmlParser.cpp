#include "XmlParser.h"
#include "FeedsFile.h"

extern int (*VerbosePrintf)(const char* format, ...);

XmlParser::XmlParser()
{
    m_xmlDoc = NULL;
}

XmlParser::~XmlParser()
{
    if(m_xmlDoc != NULL)
        xmlFreeDoc(m_xmlDoc);
}

void printChildren(int iLevel, xmlNodePtr pCur)
{
    xmlNodePtr pChild = pCur->xmlChildrenNode;
    while(pChild)
    {
        if(pChild->name != NULL)
        for(int i=0;i<iLevel;i++)
            printf(" ");
        if(pChild->name != NULL)
            printf("%s\r\n", pChild->name);
        else
            printf("****noname!****");
               
        if(pChild->content != NULL)
        {
            for(int i=0;i<iLevel+1;i++)
                printf(" ");
            printf(" Content: %s\r\n",pChild->content);
        }
        //printChildren(iLevel+1,pChild);
        pChild = pChild->next;
    }
}

#define SECONDS_IN_A_WEEK 604800

int XmlParser::parseStream(LPRSS_ITEM pItemList, LPRSS_FEED pFeed, const char* szStream, long lSize)
{
    
    //pFeed is only used for LastEncTime. If pFeed is NULL, we just make up our 7-day period.
    uint32_t selTime = 0;
    if(pFeed != NULL)
    {
        selTime = CRssFeed::convertTimeString(pFeed->getLastEncTimeString());
        VerbosePrintf("Selection Time: 0x%X (%s)\r\n", selTime, pFeed->getLastEncTimeString());
    }
    if(selTime == 0)
    {
		time_t tx;
        time(&tx);
        selTime = tx - 604800;  //from last one week, by default.
    }
    
    
    m_xmlDoc = xmlReadMemory(szStream, lSize, "noname.xml", NULL, 0);
    
    if(m_xmlDoc == NULL)
        return 1;
        
    //now get root element.
    
    xmlNodePtr pRoot = xmlDocGetRootElement(m_xmlDoc);
    
    if(pRoot == NULL)
        return 2;
        
    xmlNodePtr pChannel = getFirst(pRoot,(const xmlChar*)"channel");
    
    if(pChannel == NULL)
        return 3;
        
    xmlNodePtr pTitle = getFirst(pChannel, (const xmlChar*)"title");
    if(pTitle != NULL && pFeed != NULL)
    {
        if(pTitle->children != NULL)
            pFeed->setTitle((const char*)pTitle->children->content);
            //printf("%s\r\n",pTitle->children->content);
    }
        
    //todo: parse the title and set it if necessary.
    xmlNodePtr pItem = getFirst(pChannel,(const xmlChar*)"item");
    
    RSS_ITEM Selector;
    LPRSS_ITEM pSaveTo = pItemList;
    
    if(pFeed != NULL)
    {
        LPRSS_ITEM pSaveTo = pFeed->getSelectNew()?&Selector:pItemList;
    }
    
    uint32_t ItemCount = 0,SelCount = 0;
    //check the necessary elements.
    while(pItem)
    {
        do  //so that we can 'break' easily.
        {

            xmlNodePtr pEnc = getFirst(pItem, (const xmlChar*)"enclosure");
            if(pEnc == NULL)
                break;
            
            xmlChar* xType = xmlGetProp(pEnc, (const xmlChar*)"type");
            if(xType== NULL)
                break;
            //printf("      type: %s\r\n",xType);
            if(xmlStrcmp(xType,(const xmlChar*)"audio/mpeg"))
            {
                //audio/mp3 is also ok.
                if(xmlStrcmp(xType,(const xmlChar*)"audio/mp3"))
                {
                    VerbosePrintf("*********wrong type of enclosure found: %s\r\n",xType);
                    break;
                }
            }
           
            
            
            //without date, we cannot figure out whether this episode is newer or not.
            xmlNodePtr xDate = getFirst(pItem, (const xmlChar*)"pubDate");
            if(xDate == NULL || xDate->children == NULL)
                break;
            ItemCount++;
            
            uint32_t epTime = CRssFeed::convertTimeString((const char*)xDate->children->content);

            if(epTime <= selTime)
                break;

            VerbosePrintf("epTime: 0x%X, selTime: 0x%X\r\n",epTime, selTime);

            xmlChar* url = xmlGetProp(pEnc,(const xmlChar*)"url");
            if(url == NULL)
                break;  //no url, no item!
                
            SelCount++;

            //protection against losing items.
            pSaveTo = pSaveTo->setParentFeed(pFeed==NULL?(LPRSS_FEED)this:pFeed);
            
            VerbosePrintf("      Date: %s\r\n",xDate->children->content);
            VerbosePrintf(    "      Date value: 0x%x\r\n",epTime);
            //pSaveTo->setDate(epTime);
            pSaveTo->setDateString(xDate->children->content);
           
            {
                printf("Found url: %s\r\n",url);
                pSaveTo->setUrl(url);
            }
                
            xmlChar* xLength = xmlGetProp(pEnc, (const xmlChar*)"length");
            if(xLength != NULL)
            {
                VerbosePrintf("      length: %s\r\n",xLength);
                pSaveTo->setLength(atoi((const char*)xLength));
            }
         
                  
            xmlNodePtr xTitle = getFirst(pItem, (const xmlChar*)"title");
            if(xTitle != NULL && xTitle->children != NULL)
            {
                VerbosePrintf("      title: %s\r\n",xTitle->children->content);
                pSaveTo->setTitle(xTitle->children->content);
            }
            
            xmlNodePtr xGUID = getFirst(pItem, (const xmlChar*)"guid");
            if(xGUID != NULL && xGUID->children != NULL)
            {
                VerbosePrintf("      guid: %s\r\n",xGUID->children->content);
                pSaveTo->setGuid(xGUID->children->content);
            }
   
        }while(0);
        while(pItem)
        {
            pItem = pItem->next;
            if(pItem == NULL)
                break;
            if(!xmlStrcmp(pItem->name,(const xmlChar*)"item"))
                break;
        }
    }
    
    
    if(pFeed == NULL || pFeed->getSelectNew() == false)
    {
        printf("Selected %u of %u podcasts.\r\n",SelCount,ItemCount);
        return 0;
    }
    
    //to make the process simpler, we update the SELECT podcast feed here,
    //rather than when downloading podcasts, since otherwise, we need to carry the latest podcast item
    //with every item we add from this source.
    
    //now the selection process.
    printf("Select from the following new episodes for the feed.\r\n");
    LPRSS_ITEM pCur = &Selector;
    int iMaxSel = 1;
    while(pCur)
    {
        printf("%u - %s\r\n",iMaxSel,pCur->getTitle());
        printf("       Date added: %s\r\n",pCur->getPubDateString());
        //this is a shortcut as explained above. the other option is to figure out the latest one
        //among these and attach it to every item added to the item list, so that when THOSE are 
        //downloaded, we update the feed.
        pFeed->updateFor(pCur);
        pCur = pCur->getNext();
        iMaxSel++;
    }
    
    char SelectionList[100];
    SelectionList[0] = 0;
    printf("Enter comma separated number(s) of podcasts to download:");
    int iData = scanf("%s",SelectionList);
    
    if(iData)
    {

        int iSelected[100];
        int iCur = 0,iSelCount = 0;
        while(SelectionList[iCur])
        {
            iSelected[iSelCount] = atoi(&SelectionList[iCur]);
            if(iSelected[iSelCount] > 0 && iSelected[iSelCount] < iMaxSel)
                iSelCount++;    //valid selection.

            //skip over this number
            while(SelectionList[iCur] >= '0' && SelectionList[iCur] <= '9')
                iCur++;

            //skip over whitespaces and commas etc.
            while(SelectionList[iCur] != 0 && (SelectionList[iCur] < '0' || SelectionList[iCur] > '9'))
                iCur++;
        }
            
        for(iCur = 0;iCur < iSelCount; iCur++)
        {
            //this is a one-based list. so, ask for item number-1.
            pItemList->Add(Selector.getItem(iSelected[iCur]-1));
        }
    }
        
    return 0;
    
}

xmlNodePtr XmlParser::getFirst(xmlNodePtr pRoot, const xmlChar* szName)
{
    xmlNodePtr pChild = pRoot->xmlChildrenNode;
    xmlNodePtr pCur = pChild;
    while(pCur)
    {
       if(!xmlStrcmp(pCur->name, szName))
           return pCur;
       pCur = pCur->next;
    }
    if(pChild)
        return getFirst(pChild, szName);
    return NULL;
}
