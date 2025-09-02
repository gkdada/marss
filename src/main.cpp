#include <stdio.h>
#include <curl/curl.h>
#include <memory.h>
#include <unistd.h>
#include <sys/stat.h>

#include "FeedsFile.h"
#include "XmlParser.h"

#define PACKAGE_VERSION "1.0"

#define BUILD_YEAR_CH0 (__DATE__[ 7])
#define BUILD_YEAR_CH1 (__DATE__[ 8])
#define BUILD_YEAR_CH2 (__DATE__[ 9])
#define BUILD_YEAR_CH3 (__DATE__[10])

bool g_bVerbose;    //this ensures
unsigned int  g_uiTestCount = 0;	//if this is not zero, a test (with no downloading of podcast files)
                         		//will be done on the specified number of feeds.

int (*VerbosePrintf)(const char* format, ...);

//to be populated after reading config file/feeds file.
string g_strPodcastSaveDir;

//empty printf
int EmptyPrintf(const char* format, ...)
{
    return 0;
}

struct MemoryStruct
{
  char *memory;
  size_t size;
  
  MemoryStruct(){memory = (char*)malloc(1);  /* will be grown as needed by the realloc above */ 
                            size = 0;};
  ~MemoryStruct(){free(memory);}
};

typedef MemoryStruct MEM_STRUCT,*LPMEM_STRUCT;

//returns 0 for successful read.
int ReadUrl(LPMEM_STRUCT pMem, const char* szURL);
void GenerateName(string& strTarget, LPRSS_FEED pFeed, const char* szURL, string& strPodcastSaveDir, bool bPrependFeedNum);
void CheckFeed(string& strFeed);

int main(int argc, char **argv)
{
    VerbosePrintf = &EmptyPrintf;
    
    setbuf(stdout, NULL);
    
    bool bListOnly = false;
    bool bDownload = false;
    bool bCheckFeed = false;
    string feedUrl;
    
	printf("marss command-line tool Ver. %s (built - %s). Copyright (c) gkdada 2013-%c%c%c%c.\r\n",PACKAGE_VERSION,__DATE__,BUILD_YEAR_CH0,BUILD_YEAR_CH1,BUILD_YEAR_CH2,BUILD_YEAR_CH3);
    
    
    for(int i = 1; i < argc ; i++ )
    {
        if(argv[i][0] == '-')
        {
            int j = 1;
            while(argv[i][j])
            {
                if(argv[i][j] == 'c')
                {
                    bCheckFeed = true;
                    VerbosePrintf = &printf;
                }
                if(argv[i][j] == 'l')
                {
                    bListOnly = true;
                    VerbosePrintf = &printf;
                }
				if(argv[i][j] == 't')
				{
					g_uiTestCount++;
					VerbosePrintf = &printf;
				}
                if(argv[i][j] == 'v')
                    VerbosePrintf = &printf;
                if(argv[i][j] == 'd')
                    bDownload = true;
                j++;
            }
        }
        else
        {
            feedUrl = argv[i];
        }
    }
    
    if(bCheckFeed)
    {
        if(feedUrl.length() == 0)
        {
            printf("No feed specified for checking. Usage: 'marss -d <url of feed>'.\r\n");
            exit(0);
        }
        printf("Checking the feed \"%s\".\r\n\r\n", feedUrl.c_str());
        CheckFeed(feedUrl);
    }
    
    if(bListOnly == false && bDownload == false && g_uiTestCount == 0)
    {
        printf("Use marss -d to download new podcasts.\r\n");
		exit(0);
    }
    
    RSS_ITEM xItemList;
    
    //load the marss file
    CConfigFile xFile;

    int iResult = xFile.ReadConfigFile();
    
    if(iResult != 0)
        return 0;
        
    if(bListOnly)
        return 0;
    
    if(!bDownload && g_uiTestCount == 0)  //shouldn't come here anyway. if bDownload is false, bListOnly or bImport would be true.
        return 0;
        

    LPRSS_FEED pFeed = xFile.GetFeedList();
    
   
    while(pFeed)
    {
        if(pFeed->getURL()[0])
        {
            MEM_STRUCT RssFile;
            if(!ReadUrl(&RssFile, pFeed->getURL()))
            {
                XmlParser xmlp;
                int oResult = xmlp.parseStream(&xItemList, pFeed, RssFile.memory, RssFile.size);
                if(oResult)
                    printf("Error %u parsing RSS from feed '%s'\r\n",oResult, pFeed->getTitle());
            }
            
        }
	if(g_uiTestCount > 1)
		g_uiTestCount--;
	else if(g_uiTestCount == 1)
	{
		printf("End of test run. Exiting...\r\n");
		return 0;
	}
      
        pFeed = pFeed->getNext();
    }
    
    if(xItemList.getCount() == 0)
    {
        printf("No new podcast episodes found. Exiting...\r\n");
        return 0;
    }
    
    //ensure that target folder exists.
    if(xFile.createPodcastSaveDir(g_strPodcastSaveDir))
        return 0;   //error creating the target folder.

    LPRSS_ITEM pList = &xItemList;
    if(pList)
        pList->FixDuplicateFilenames(xFile.getPrependFlag());

        
    printf("Downloading %u new/selected podcast episodes.\r\n",xItemList.getCount());
    
    //now download and save the podcasts.
    while(pList)
    {
        if(pList->getParentFeed() != NULL)
        {

            MEM_STRUCT Mp3File;
            if(!ReadUrl(&Mp3File,pList->getUrl()))
            {
                //extract file name.
                string fName;
                GenerateName(fName, pList->getParentFeed(), pList->getFilename(), g_strPodcastSaveDir, xFile.getPrependFlag());
                
                FILE* pFile = fopen(fName.c_str(),"w");
                if(pFile != NULL)
                {
                    if(fwrite(Mp3File.memory,1,Mp3File.size, pFile) != Mp3File.size)
                    {
                        printf("Error writing to the podcast episode file %s. Please check disk space.\r\n",fName.c_str());
                        xFile.SaveConfigFile();
                        return 0;
                    }
                    fclose(pFile);
                    
                    pList->getParentFeed()->updateFor(pList);
                }
                else
                {
                    printf("Error opening the file %s to save a podcast. Please check permissions/disk space.\r\n",fName.c_str());
                    //don't forget to update the file for any podcasts already downloaded. 
                    xFile.SaveConfigFile();
                    return 0;
                }
            }
        }
        pList = pList->getNext();
    }
    
    //return 0;

    
//    XmlParser xmlp;
//    int oResult = xmlp.tryParser();
//    return 0;

    xFile.SaveConfigFile();
	return 0;
}

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
  
  
  if((mem->size/1000000) < ((mem->size+realsize)/1000000))
      printf(".");
  
 
  mem->memory = (char*)realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL) {
    /* out of memory! */ 
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }
  
  //printf(".");
 
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
 
  return realsize;
}


int ReadUrl(LPMEM_STRUCT pMem, const char* szURL)
{
    printf("Reading from URL '%s'\r\n",szURL);
    CURLcode res;
    
    CURL* pCurl = curl_easy_init();
    curl_easy_setopt(pCurl, CURLOPT_URL, szURL);
    
    /* send all data to this function  */ 
    curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

    /* we pass our 'chunk' struct to the callback function */ 
    curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, (void *)pMem);

    //if the content has moved, we follow it.
    curl_easy_setopt(pCurl, CURLOPT_FOLLOWLOCATION, 1L);


    /* some servers don't like requests that are made without a user-agent
     field, so we provide one */ 
    curl_easy_setopt(pCurl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    
    /* get it! */ 
    res = curl_easy_perform(pCurl);
    
    if(res == CURLE_OK)
    {
        printf("Successfully read the content from the URL.\r\n");
        return 0;
    }
    return 1;
}

char strDate[20] = "";

void GenerateName(string& strTarget, LPRSS_FEED pFeed, const char* szFilename, string& strPodcastSaveDir, bool bPrependFeedNum)
{
    strTarget = strPodcastSaveDir + "/";
    
    if(bPrependFeedNum)
    {
        char szPrepend[20];
        snprintf(szPrepend, sizeof(szPrepend), "%02u_",pFeed->getFeedNum());
        strTarget += szPrepend;
    }
    
    string strName;
    strName = szFilename;
    //remove reserved characters.
        
    strTarget += strName;
    
}

void CheckFeed(string& strFeed)
{
    RSS_ITEM xItemList;
    MEM_STRUCT RssFile;
    if(!ReadUrl(&RssFile, strFeed.c_str()))
    {
        XmlParser xmlp;
        int oResult = xmlp.parseStream(&xItemList, NULL, RssFile.memory, RssFile.size);
        if(oResult)
            printf("Error %u parsing RSS from feed '%s'\r\n",oResult, "New Feed");
    }
}
