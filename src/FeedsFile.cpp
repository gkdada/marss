#include "FeedsFile.h"

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cerrno>

extern int (*VerbosePrintf)(const char* format, ...);

char g_szDefaultHomeDir[] = "~";

CConfigFile::CConfigFile()
{
    m_pFeedList = NULL;

    m_bFeedsLoaded = false;
    m_bCreateDateSubdir = true;
    m_bPrependFeedNum = true;
    m_bRenameFeeds = false;

    //default target dir.
    char* pHomeDir = getenv("HOME");
    if(pHomeDir == NULL)
        m_strTargetDir = "~/Podcasts";
    else
    {
        m_strTargetDir = pHomeDir;
        m_strTargetDir += "/Podcasts";
    }
    m_pConfigReader = NULL;
}

CConfigFile::~CConfigFile()
{
    if(m_pConfigReader != NULL)
        g_key_file_free(m_pConfigReader);
    if(m_pFeedList != NULL)
        delete m_pFeedList;
}

int CConfigFile::ReadConfigFile()
{
    string pConfFile;
    char* pHomeDir = getenv("HOME");
    if(pHomeDir == NULL)
    {
        pConfFile = "~";
    }
    else
        pConfFile = pHomeDir;
    pConfFile += "/.marss/marss.conf";

    if(m_pConfigReader != NULL)
    {
        g_key_file_free(m_pConfigReader);
        m_pConfigReader = NULL;
    }
    
    GError* pError = NULL;
    m_pConfigReader = g_key_file_new();
    bool bLoaded = g_key_file_load_from_file(m_pConfigReader,pConfFile.c_str(),G_KEY_FILE_KEEP_COMMENTS,&pError);
    
    if(bLoaded == false)
    {
		printf("Unable to locate the config file %s\r\n", pConfFile.c_str());
        return 1;
    }
    
    pError = NULL;
    gboolean bVal = g_key_file_get_boolean(m_pConfigReader,"global","prepend feed number",&pError);
    if(pError == NULL)
    {
        VerbosePrintf("[global]->prepend feed number = %s\r\n",bVal?"true":"false");
        m_bPrependFeedNum = bVal;
    }
    pError = NULL;
    gchar* pTemp = g_key_file_get_string(m_pConfigReader,"global","target dir",&pError);
    if(pError == NULL)
    {
        VerbosePrintf("[glbal]->target dir = %s\r\n",pTemp);
        m_strTargetDir = pTemp;
    }


    pError = NULL;
    bVal = g_key_file_get_boolean(m_pConfigReader,"global","create subdir with date",&pError);
    if(pError == NULL)
    {
        VerbosePrintf("[global]->create subdir with date = %s\r\n",bVal?"true":"false");
        m_bCreateDateSubdir = bVal;
    }
    
    pError = NULL;
    bVal = g_key_file_get_boolean(m_pConfigReader,"global","reorder feed names",&pError);
    if(pError == NULL)
    {
        VerbosePrintf("[global]->reorder feed names = %s\r\n",bVal?"true":"false");
        m_bRenameFeeds = bVal;
    }
    //any other values.
    
    //the name of each section doesn't matter. just the order.
    gsize numGroups = 0;
    gchar** pGroupList = g_key_file_get_groups(m_pConfigReader,&numGroups);
    
    //any group that isn't named 'global' is fair game.
    for(unsigned int i=0;i<numGroups;i++)
    {
        if(strcmp(pGroupList[i],"global"))
        {
            m_pFeedList = new RSS_FEED;
            return m_pFeedList->Load(1, i, numGroups, pGroupList, m_pConfigReader);
            //we actually only load ONE group since the group loading is recursive.
            break;
        }
    }


    return 0;
}

int CConfigFile::SaveConfigFile()
{
    string pConfFile;
    char* pHomeDir = getenv("HOME");
    if(pHomeDir == NULL)
    {
        pConfFile = "~";
    }
    else
        pConfFile = pHomeDir;
    pConfFile += "/.marss/marss.conf";
    if(m_pConfigReader == NULL)
    {

        m_pConfigReader = g_key_file_new();
        //will be saved to pConfFile.
    }

    //global section.
    g_key_file_set_boolean(m_pConfigReader,"global","prepend feed number", m_bPrependFeedNum?true:false);
    g_key_file_set_string(m_pConfigReader,"global","target dir",m_strTargetDir.c_str());
    g_key_file_set_boolean(m_pConfigReader,"global","create subdir with date", m_bCreateDateSubdir?true:false);
    g_key_file_set_boolean(m_pConfigReader,"global","reorder feed names", false);   //No need to reorder every cycle. When reordering is required again, the user will set the flag.

    //now the feeds.
    if(m_pFeedList)
        m_pFeedList->Save(m_pConfigReader, m_bRenameFeeds);
        
    GError* pError = NULL;
    g_key_file_save_to_file(m_pConfigReader,pConfFile.c_str(),&pError);
    if(pError == NULL)
        return 0;
    return pError->code;
}

int CConfigFile::createPodcastSaveDir(string& strSaveDir)
{
    //1. make sure that specified dir(or defualt dir) exists.
    if(m_strTargetDir.length() == 0)//this is either because config file doesn't have it or we loaded feeds file.
    {
        //so, we populate it with default diretory: <HOME>/Podcasts.
        char* pHomeDir = getenv("HOME");
        if(pHomeDir == NULL)
        {
            printf("Unable to determine Home Directory!\r\n");
            pHomeDir = g_szDefaultHomeDir;
        }
        m_strTargetDir = pHomeDir;
        m_strTargetDir += "/Podcasts";
    }
    
    strSaveDir = m_strTargetDir;

    //if this directory doesn't exist, create it.
    if(mkdir(strSaveDir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
    {
        if(errno != EEXIST)//not ok.
        {
            printf("Error %u creating the Podcast parent directory %s.\r\n", errno, strSaveDir.c_str());
            return 1;
        }
    }
    
    if(!m_bCreateDateSubdir)
        return 0;//nothing more to do.

    //now the date string.
    char strDate[20];

    time_t rawTime;
    struct tm* ptm;
    time(&rawTime);
    ptm = localtime(&rawTime);
    
    sprintf(strDate,"/%04u%02u%02u",ptm->tm_year+1900,ptm->tm_mon+1,ptm->tm_mday);
    
    strSaveDir += strDate;
    
    if(mkdir(strSaveDir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
    {
        if(errno != EEXIST)//not ok.
        {
            printf("Error %u creating the Podcast save directory %s.\r\n", errno, strSaveDir.c_str());
            return 1;
        }
    
    }
    
    return 0;
}

CRssFeed::CRssFeed()
{
    m_pNext = NULL;
    //m_uiLastEncTime = 0;
    m_bSelectNew = false;
}

CRssFeed::~CRssFeed()
{
    if(m_pNext)
        delete m_pNext;
}

int CRssFeed::Load(unsigned int FeedNum, unsigned char* caBuffer, long lCurPtr, long lMaxLength, uint32_t lRemObjects)
{
    
    m_uiFeedNum = FeedNum;
    
    //1. Load Title.
    if(ReadString(caBuffer,lCurPtr,lMaxLength,m_strFeedTitle))
        return 31;
    
    VerbosePrintf("Feed Title: %s\r\n",m_strFeedTitle.c_str());
    
    //2. Load URL
    if(ReadString(caBuffer,lCurPtr,lMaxLength,m_strFeedUrl))
        return 32;
    VerbosePrintf("Feed URL: %s\r\n",m_strFeedUrl.c_str());
    
    //3. Load 'Download all' flag
    uint32_t uiTemp = *((uint32_t*)&caBuffer[lCurPtr]);
    m_bSelectNew = uiTemp?false:true;
    lCurPtr += 4;
    
    VerbosePrintf("Download ALL new podcasts = %s\r\n",m_bSelectNew?"false":"true");
    
    //4. Last encountered URL
    if(ReadString(caBuffer,lCurPtr,lMaxLength,m_strLastEncUrl))
        return 33;
    VerbosePrintf("Last encountered URL: %s\r\n",m_strLastEncUrl.c_str());
    
    //5. Last encountered GUID
    if(ReadString(caBuffer,lCurPtr,lMaxLength,m_strLastEncGUID))
        return 34;
    VerbosePrintf("Last encountered GUID: %s\r\n",m_strLastEncGUID.c_str());
    
    //what is this?
    //edit: This is a marker (0x8000000a) which tells that this is a 64-bit time.
    lCurPtr += 4;

    //6. Timestamp of last encountered.
    //Edit: we are no longer saving uint32_t version of time.
    //so we need to print it out.
    uint32_t uiLastEncTime = *((uint32_t*)&caBuffer[lCurPtr]);
    {
        char DateStr[200];
        char rfc822_str[] = "%a, %d %b %Y %T";
        time_t tmx = uiLastEncTime;
        //this is due to a bug/oversight in legacy application. it is actually gm time.
        struct tm* ptm = localtime(&tmx);
        strftime(DateStr, 100, rfc822_str, ptm );
        m_strLastEncTime = DateStr;
    }

    lCurPtr += 4;
    
    VerbosePrintf("Last encountered time (UTC): %s\r\n",m_strLastEncTime.c_str());
    VerbosePrintf("Last encountered time value: 0x%x\r\n\r\n",uiLastEncTime);
    
    //the upper 32 bits of timestamp are not useful right now (since they are required only after 2038)
    lCurPtr += 4;
    
    lRemObjects--;
    if(lRemObjects)
    {
        m_pNext = new CRssFeed;
        return m_pNext->Load(FeedNum+1, caBuffer, lCurPtr, lMaxLength, lRemObjects);
    }
    
    
    return 0;
}

int CRssFeed::Load(unsigned int FeedNum, GKeyFile* pFile)
{
    m_uiFeedNum = FeedNum;
    char Group[40];
    sprintf(Group,"Feed%u",FeedNum);
    GError* pError = NULL;
    char* pTemp = g_key_file_get_string(pFile,Group,"Title",&pError);
    if(pError == NULL)
    {
        m_strFeedTitle = pTemp;
        VerbosePrintf("[%s]->Title = %s\r\n",Group,pTemp);
    }
    pError = NULL;
    pTemp = g_key_file_get_string(pFile,Group,"URL",&pError);
    if(pError == NULL)
    {
        m_strFeedUrl = pTemp;
        VerbosePrintf("[%s]->URL = %s\r\n",Group,pTemp);
    }
    bool bVal = g_key_file_get_boolean(pFile,Group,"Select",&pError);
    if(pError == NULL)
    {
        m_bSelectNew = bVal?true:false;
        VerbosePrintf("[%s]->Select = %s\r\n",Group,bVal?"true":"false");
    }
    pError = NULL;
    pTemp = g_key_file_get_string(pFile,Group,"LastEpisode",&pError);
    if(pError == NULL)
    {
        m_strLastEncUrl = pTemp;
        VerbosePrintf("[%s]->LastEpisode = %s\r\n",Group,pTemp);
    }
    pError = NULL;
    pTemp = g_key_file_get_string(pFile,Group,"LastGUID",&pError);
    if(pError == NULL)
    {
        m_strLastEncGUID = pTemp;
        VerbosePrintf("[%s]->LastGUID = %s\r\n",Group,pTemp);
    }
    pError = NULL;
    pTemp = g_key_file_get_string(pFile,Group,"LastPubDate",&pError);
    if(pError == NULL)
    {
        VerbosePrintf("[%s]->LastPubDate = %s\r\n",Group,pTemp);
        m_strLastEncTime = pTemp;
        uint32_t uiLastEncTime = convertTimeString(m_strLastEncTime.c_str());
        VerbosePrintf("[%s]->LastPubDate in Hex = 0x%X\r\n\r\n",Group, uiLastEncTime);
    }
    
    sprintf(Group,"Feed%u",FeedNum+1);
    bVal = g_key_file_has_group(pFile,Group);
    if(bVal)
    {
        m_pNext = new RSS_FEED;
        return m_pNext->Load(FeedNum+1, pFile);
    }
    return 0;
}

int CRssFeed::Load(unsigned int FeedNum, unsigned int GroupNum, unsigned int MaxNumGroups, gchar** ppGroupNameList, GKeyFile* pFile)
{
    m_uiFeedNum = FeedNum;
    //it is already assumed that ppGroupList[GroupNum] is a valid group.
    m_strGroupName = ppGroupNameList[GroupNum];
    
    GError* pError = NULL;
    char* pTemp = g_key_file_get_string(pFile,m_strGroupName.c_str(),"Title",&pError);
    if(pError == NULL)
    {
        m_strFeedTitle = pTemp;
        VerbosePrintf("[%s]->Title = %s\r\n",m_strGroupName.c_str(),pTemp);
    }
    pError = NULL;
    pTemp = g_key_file_get_string(pFile,m_strGroupName.c_str(),"URL",&pError);
    if(pError == NULL)
    {
        m_strFeedUrl = pTemp;
        VerbosePrintf("[%s]->URL = %s\r\n",m_strGroupName.c_str(),pTemp);
    }
    bool bVal = g_key_file_get_boolean(pFile,m_strGroupName.c_str(),"Select",&pError);
    if(pError == NULL)
    {
        m_bSelectNew = bVal?true:false;
        VerbosePrintf("[%s]->Select = %s\r\n",m_strGroupName.c_str(),bVal?"true":"false");
    }
    pError = NULL;
    pTemp = g_key_file_get_string(pFile,m_strGroupName.c_str(),"LastEpisode",&pError);
    if(pError == NULL)
    {
        m_strLastEncUrl = pTemp;
        VerbosePrintf("[%s]->LastEpisode = %s\r\n",m_strGroupName.c_str(),pTemp);
    }
    pError = NULL;
    pTemp = g_key_file_get_string(pFile,m_strGroupName.c_str(),"LastGUID",&pError);
    if(pError == NULL)
    {
        m_strLastEncGUID = pTemp;
        VerbosePrintf("[%s]->LastGUID = %s\r\n",m_strGroupName.c_str(),pTemp);
    }
    pError = NULL;
    pTemp = g_key_file_get_string(pFile,m_strGroupName.c_str(),"LastPubDate",&pError);
    if(pError == NULL)
    {
        VerbosePrintf("[%s]->LastPubDate = %s\r\n",m_strGroupName.c_str(),pTemp);
        m_strLastEncTime = pTemp;
        uint32_t uiLastEncTime = convertTimeString(m_strLastEncTime.c_str());
        VerbosePrintf("[%s]->LastPubDate in Hex = 0x%X\r\n\r\n",m_strGroupName.c_str(), uiLastEncTime);
    }
    
    for(unsigned int i = GroupNum+1; i < MaxNumGroups; i++)
    {
        if(strcmp(ppGroupNameList[i],"global"))
        {
            m_pNext = new RSS_FEED;
            return m_pNext->Load(FeedNum+1, i, MaxNumGroups, ppGroupNameList, pFile);
            //recursive, so break.
            break;
        }
    }
    return 0;
}

int CRssFeed::Save(FILE* pFile)
{
    SaveString(pFile, m_strFeedTitle);
    SaveString(pFile, m_strFeedUrl);
    uint32_t uiTemp = m_bSelectNew?0:1;
    fwrite(&uiTemp, 1, 4, pFile);
    
    SaveString(pFile, m_strLastEncUrl);
    SaveString(pFile, m_strLastEncGUID);
    uiTemp = 0x8000000a;
    fwrite(&uiTemp, 1, 4, pFile);   //this will ensure compatibility with windows version.
    uint32_t uiLastEncTime = convertTimeString(m_strLastEncTime.c_str());
    fwrite(&uiLastEncTime, 1, 4, pFile);
    uiTemp = 0;
    
    //if this write succeeds, everything before this must have succeeded too!
    //Lazy...grrrrr......
    if(fwrite(&uiTemp, 1, 4, pFile) != 4)
        return 1;

    if(m_pNext)
        return m_pNext->Save(pFile);
    return 0;
}

int CRssFeed::Save(GKeyFile* pFile, bool bRenameFeeds)
{
    GError* pError;
    const char* pGroupName;
    char GroupName[20];
    if(!m_strGroupName.length())    //this is the case when we use the first or second form (deprecated) of Load
    {
        sprintf(GroupName,"Feed%02u",m_uiFeedNum);
        pGroupName = GroupName;
    }
    else if(bRenameFeeds)
    {
        if(m_strGroupName.length() < 7) //if existing names are like Feed1 or Feed01 etc, new names are Feed001 etc.
            sprintf(GroupName,"Feed%03u", m_uiFeedNum);
        else
            sprintf(GroupName,"Feed%02u", m_uiFeedNum);
        pGroupName = GroupName;
        
        g_key_file_remove_group(pFile, m_strGroupName.c_str(), &pError);
    }
    else
        pGroupName = m_strGroupName.c_str();
    
    g_key_file_set_string(pFile,pGroupName,"Title",m_strFeedTitle.c_str());
    g_key_file_set_string(pFile,pGroupName,"URL",m_strFeedUrl.c_str());
    g_key_file_set_boolean(pFile,pGroupName,"Select",m_bSelectNew);
    g_key_file_set_string(pFile,pGroupName,"LastEpisode",m_strLastEncUrl.c_str());
    g_key_file_set_string(pFile,pGroupName,"LastGUID",m_strLastEncGUID.c_str());
    
    g_key_file_set_string(pFile,pGroupName,"LastPubDate", m_strLastEncTime.c_str());
    
    
    if(m_pNext)
        return m_pNext->Save(pFile, bRenameFeeds);
        
    return 0;
}

//this file reads a widechar CString object into WString.
int CRssFeed::ReadString(unsigned char* caBuffer, long& lCurPtr, long lMaxLength, string& strObj)
{
    //first we expect a FFFE.
    if(caBuffer[lCurPtr] == 0xFF && caBuffer[lCurPtr+1] == 0xFE && caBuffer[lCurPtr+2] == 0xFF)
    {
        printf("Unexpected wide-char string in file.\r\n");
        return 51;
    }
    
    uint32_t lStrLength;
    if(caBuffer[lCurPtr] < 0xFF)    //single byte length
    {
        lStrLength = (uint32_t)(unsigned char)caBuffer[lCurPtr];
        lCurPtr += 1;
    }
    else if(caBuffer[lCurPtr+1] < 0xFF) //2 byte length.
    {
        lStrLength = (uint32_t)*((uint16_t*)&caBuffer[lCurPtr+1]);
        lCurPtr += 3;
    }
    else//4 byte length.
    {
        lStrLength = *((uint32_t*)&caBuffer[lCurPtr+3]);
        lCurPtr += 7;
    }
    
    if((lCurPtr + (long)lStrLength) > lMaxLength)
    {
        printf("Unexpected end-of-file while parsing a string in feeds file.\r\n");
        return 52;
    }
    
    strObj = string((char*)&caBuffer[lCurPtr],lStrLength);
    lCurPtr += (lStrLength);
    return 0;
}

int CRssFeed::SaveString(FILE* pFile, string& strObj)
{
    uint32_t uiLen = strObj.length();
    
    uint32_t uiMax = 0xFFFFFFFF;
    
    if(uiLen < 0xFF)
    {
        unsigned char ucLen = (unsigned char)uiLen;
        fwrite(&ucLen, 1, 1, pFile);
    }
    else if(uiLen < 0xFFFF)
    {
        fwrite(&uiMax, 1, 1, pFile);    //indicator for 2-byte length.
        unsigned short usLen = (unsigned short)uiLen;
        fwrite(&usLen, 1, 2, pFile);
    }
    else
    {
        fwrite(&uiMax, 1, 3, pFile); //indicator for 4-byte length.
        fwrite(&uiLen, 1, 4, pFile);
    }
    
    if(fwrite(strObj.c_str(), 1, uiLen, pFile) != uiLen)
        return 1;
    
    return 0;
}

void CRssFeed::updateFor(CRssItem* pItem)
{
    if(convertTimeString(getLastEncTimeString()) < pItem->getPubDate())
    {
        //m_uiLastEncTime = pItem->getPubDate();
        m_strLastEncTime = pItem->getPubDateString();
        m_strLastEncUrl = pItem->getUrl();
        m_strLastEncGUID = pItem->getGuid();
    }
}

void CRssFeed::setTitle(const char* szTitle)
{
    if(szTitle == NULL)
        return;
    if(szTitle[0] == 0)
        return;
    m_strFeedTitle = szTitle;
}

#if 0
uint32_t CRssFeed::getLastEncTime()
{
    if(m_strLastEncTime.length())
        return convertTimeString(m_strLastEncTime.c_str());
    return 0;
}
#endif

uint32_t CRssFeed::convertTimeString(const char* strTime)
{
    struct tm timex;
    const char* pRet = strptime(strTime,"%a, %e %h %Y %H:%M:%S %z",&timex);
    if(pRet == NULL) //scan failed.
    {
        pRet = strptime(strTime, "%a, %e %h %Y %H:%M:%S",&timex);
    }
	timex.tm_isdst = 0;	//strptime has a bug where it randomly assigns 0 or 1 to tm_isdst.
	//we overcome this - for the time being - by always assigning zero.
	//This works because we only ever compare a feed's time to it's own time in subsequent episodes.
	//printf("Converted Values: %u/%u/%u %u:%u:%u IsDST: %u\r\n",timex.tm_year,timex.tm_mon, timex.tm_mday, timex.tm_hour, timex.tm_min, timex.tm_sec, timex.tm_isdst);
        
    uint32_t epTime = mktime(&timex);
    return epTime;
}



CRssItem::CRssItem()
{
    m_pNext = NULL;
    m_pParentFeed = NULL;   //NULL indicates free slot. non-NULL indicates occupied.
}

CRssItem::~CRssItem()
{
    if(m_pNext)
        delete m_pNext;
}

CRssItem* CRssItem::setParentFeed(LPRSS_FEED pFeed)
{
    if(m_pParentFeed == NULL)
    {
        m_pParentFeed = pFeed;
        return this;
    }
    if(m_pNext == NULL)
        m_pNext = new CRssItem;
    return m_pNext->setParentFeed(pFeed);
}

void GenRandom(string s, const int len)
{
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    s.clear();
    srand((unsigned)time(NULL));
    for (int i = 0; i < len; ++i) {
        s += alphanum[rand() % (sizeof(alphanum) - 1)];
    }
}


void CRssItem::setUrl(xmlChar* url)
{
    m_strItemUrl = (const char*)url;
    
    size_t iLen = m_strItemUrl.length();
    while(iLen)
    {
        if(m_strItemUrl[iLen-1] == '/' || m_strItemUrl[iLen-1] == '\\')
            break;
        iLen--;
    }

    m_strItemFilename = m_strItemUrl.substr(iLen);

    size_t iFound = m_strItemFilename.find_first_of("<>:\"/\\|?*");
    if(iFound != std::string::npos)//truncate it here.
        m_strItemFilename.resize(iFound);
    if(m_strItemFilename.length() == 0)
        GenRandom(m_strItemFilename,12);
    if(m_strItemFilename.length() >  44)  //some arbitrary limit.
        m_strItemFilename.resize(44);
    if(m_strItemFilename.length() < 4 || m_strItemFilename.compare(m_strItemFilename.length()-4, 4, ".mp3"))
        m_strItemFilename += ".mp3";
        
#ifndef NDEBUG
    printf("Filename: %s\r\n",m_strItemFilename.c_str());
#endif
}

uint32_t CRssItem::getCount()
{
    if(m_pNext)
    {
        return ((m_pNext->getCount())+((m_pParentFeed == NULL)?0:1));
    }
    return (m_pParentFeed == NULL)?0:1;
}

CRssItem* CRssItem::getItem(int iNum)
{
    LPRSS_ITEM pRet = this;
    while(iNum--)
    {
        pRet = pRet->m_pNext;
        if(pRet == NULL)
            break;
    }
    return pRet;
}

void CRssItem::Add(CRssItem* pItem)
{
    LPRSS_ITEM pAdder = setParentFeed(pItem->getParentFeed());
    pAdder->m_strItemUrl = pItem->m_strItemUrl;
    pAdder->m_strItemFilename = pItem->m_strItemFilename;
    pAdder->m_strItemTitle = pItem->m_strItemTitle;
    pAdder->m_strItemGuid = pItem->m_strItemGuid;
    pAdder->m_strPubDate = pItem->m_strPubDate;
    pAdder->m_uiLength = pItem->m_uiLength;
}

uint32_t CRssItem::getPubDate()
{
    if(m_strPubDate.length())
        return CRssFeed::convertTimeString(m_strPubDate.c_str());
    return 0;
}

void CRssItem::FixFilename(char& FixChar)
{
    size_t insertBefore = m_strItemFilename.find_last_of('.');
    if(insertBefore != string::npos)
    {
        m_strItemFilename.insert(insertBefore,1,FixChar);
        if(FixChar == '9')
            FixChar = 'A';
        else if(FixChar == 'Z')
            FixChar = 'a';
        else
            FixChar++;
    }
}

void CRssItem::FixDuplicateFilenames(bool bPrependFeedNum)
{
    char FixChar = '1';
    //if bPrependFeedNum is true, we need to check duplicate filenames only within the same feed.
    LPRSS_ITEM pNext = m_pNext;
    //else, we need to check for duplicates throughout the list.
    while(pNext)
    {
        if(bPrependFeedNum)
        {
            if(pNext->m_pParentFeed != m_pParentFeed)   //different feed, different prepend number. stop the iteration.
                break;
        }
        if(!m_strItemFilename.compare(pNext->m_strItemFilename))
        {
            pNext->FixFilename(FixChar);
        }
        pNext = pNext->m_pNext;
    }

    if(m_pNext)
        m_pNext->FixDuplicateFilenames(bPrependFeedNum);
}
