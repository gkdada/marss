#ifndef CFEEDSFILE_H
#define CFEEDSFILE_H

#include <string>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <glib.h>

#define MAJOR_VER       0
#define MINOR_VER       5

/******************************************************************************************
Change list:
 * 0.1 - Initial version
 * 0.2 - Added ability to select episodes for a feed (Select = true)
 * 0.3 - 01/09/2017 - Name changed from MaRSS to marss. Scan and change duplicate filenames in the download list.
 *     - 02/2018    - Added ability to reorder the feed list with numbers starting from 01 or 001 when the option is set in config file.
 * 0.4 - 11/06/2018 - Moving to gcc style configure/make/make install
 * 0.5 - 01/20/2019 - Fixed the time difference bug in FeedsFile caused by time_t/uint32_t.
                    - Removing compatibility with Windows version. 
******************************************************************************************/

using namespace std;

class CRssFeed;
class CRssItem;

//this is a lazy fail-proof way to allocate a buffer and to destroy it when not needed.
class CBuffer
{
public:
    CBuffer(long iLength){m_iBufLength = iLength;m_caBuffer = new unsigned char[iLength];}
    ~CBuffer(){delete m_caBuffer;}
    unsigned char* GetBuffer(){return m_caBuffer;};
protected:
    long m_iBufLength;
    unsigned char* m_caBuffer;
};

typedef CRssFeed RSS_FEED,*LPRSS_FEED;

//CConfigFile has all the feeds along with config information
//CConfigFile loads from (and saves to) a text config file

class CConfigFile 
{
public:
    CConfigFile();
    ~CConfigFile();
    
    int ReadConfigFile(); //as of now, this is hard-coded to ~/.marss/marss.conf. Will change it if required.
                            //will read Feeds file if marss.conf is not available.
    int SaveConfigFile(); //call with 'true' to save (export) to Feeds.MaRSS instead.
    int createPodcastSaveDir(string& strSaveDir);
    bool getPrependFlag(){return m_bPrependFeedNum;};

    LPRSS_FEED GetFeedList(){return m_pFeedList;};

    
protected:
    bool m_bFeedsLoaded;    //true if loaded from Feeds file. we need to create config file sections in that case.
    string m_strTargetDir;
    bool m_bCreateDateSubdir;
    bool m_bPrependFeedNum;
    bool m_bRenameFeeds;
    
    GKeyFile* m_pConfigReader;
    //Config m_ConfigReader;
    CRssFeed* m_pFeedList;
};

class CRssFeed
{
public:
    friend CConfigFile;
    
    //declared as a static function here, so it can be used by unrelated classes.
    static uint32_t convertTimeString(const char* strTime);

    uint32_t getObjectCount(){if(m_pNext)return m_pNext->getObjectCount()+1;return 1;}
    
    LPRSS_FEED getNext(){return m_pNext;};
    const char* getURL(){return m_strFeedUrl.c_str();};
    const char* getTitle(){return m_strFeedTitle.c_str();};
    //uint32_t getLastEncTime();/*{return m_uiLastEncTime;};*/
    const char* getLastEncTimeString(){return m_strLastEncTime.c_str();}
    uint32_t getFeedNum(){return m_uiFeedNum;};
    bool getSelectNew(){return m_bSelectNew;}
   
    
    void setTitle(const char* szTitle);
    void updateFor(CRssItem* pItem);
    
private:
    CRssFeed();
    ~CRssFeed();
    
    //FeedNum is used to set prefixes for downloaded mp3 files. start with '1'
    int Load(unsigned int FeedNum, unsigned char* caBuffer, long lCurPtr, long lMaxLength, uint32_t lRemObjects);
    int Load(unsigned int FeedNum, GKeyFile* pFile);
    int Load(unsigned int FeedNum, unsigned int GroupNum, unsigned int MaxNumGroups, gchar** ppGroupNameList, GKeyFile* pFile);
    int Save(FILE* pFile);
    int Save(GKeyFile* pFile, bool bRenameFeeds);
    int ReadString(unsigned char* caBuffer, long& lCurPtr, long lMaxLength, string& strObj);
    int SaveString(FILE* pFile, string& strObj);
    
   

    uint32_t m_uiFeedNum;
    string m_strFeedTitle,m_strFeedUrl;
    bool m_bSelectNew;
    string m_strLastEncUrl,m_strLastEncGUID;
    //change of spec: We never save the uint32_t version of LastEncTime.
    //uint32_t m_uiLastEncTime;
    string m_strLastEncTime;
    string m_strGroupName;  //to save group name in conf file.
    bool m_bError;
    
    CRssFeed* m_pNext;
};


class CRssItem
{
public:
    CRssItem();
    ~CRssItem();
    
    //this method will set the feed number for an (otherwise) empty item and return it.
    //if the item is not empty, it will allocate a new item, set feed number and return THAT item.
    CRssItem* setParentFeed(LPRSS_FEED pFeed);
    CRssItem* getItem(int iNum);    //zero-based counting.
    
    void Add(CRssItem* pItem);
    void setUrl(xmlChar* url);
    void setLength(unsigned int Length){m_uiLength = Length;};
    void setTitle(xmlChar* title){m_strItemTitle = (const char*)title;};
    void setGuid(xmlChar* guid){m_strItemGuid = (const char*)guid;};
    //void setDate(/*uint32_t*/const char* date){m_strPubDate = date;};
    void setDateString(xmlChar* strDate){m_strPubDate = (const char*)strDate;};
    
    void FixDuplicateFilenames(bool bPrependFeedNum);
    void FixFilename(char& FixChar);
    
    LPRSS_FEED getParentFeed(){return m_pParentFeed;};
    const char* getUrl(){return m_strItemUrl.c_str();};
    const char* getFilename(){return m_strItemFilename.c_str();};
    const char* getGuid(){return m_strItemGuid.c_str();};
    uint32_t getPubDate();/*{return m_uiPubDate;}*/
    const char* getTitle(){return m_strItemTitle.c_str();};
    const char* getPubDateString(){return m_strPubDate.c_str();};
    
    CRssItem* getNext(){return m_pNext;}
    
    uint32_t getCount();
protected:
    LPRSS_FEED m_pParentFeed;
    string m_strItemUrl, m_strItemFilename, m_strItemTitle, m_strItemGuid, m_strPubDate;
    unsigned int m_uiLength;
    //uint32_t m_uiPubDate; //never save uint32_t version of time.
    
    CRssItem* m_pNext;
};




typedef CRssItem RSS_ITEM,*LPRSS_ITEM;




#endif // CFEEDSFILE_H
