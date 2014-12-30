#include <algorithm>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/foreach.hpp>
#include <map>
#include <stdint.h>
#include <time.h>
#include <stdexcept>
#include <sstream>
#include <errno.h>

#include "db.h"
#include "txdb.h"
#include "qdex.h"
#include "key.h"
#include "net.h"
#include "sync.h"
#include "ui_interface.h"

using namespace std;
namespace fs = boost::filesystem;

map<uint64_t, CIndexFeedItem> mapIndexesByTime;
map<uint256, CIndexFeedItem> mapIndexes;
CCriticalSection cs_mapIndexes;
map<uint64_t, CNewsFeedItem> mapNewsByTime;
map<uint256, CNewsFeedItem> mapNews;
CCriticalSection cs_mapNews;

static const char* pszMainKey = "04be1353e6d671923f73a730bc6abc51417873f5ba810f5e9e48f153999950787bfc7cee4bc039b847d644637224e1d5c1194111cc5bdeab78ae0dcd87a71d889f";

static const char* pszTestKey = "04be1353e6d671923f73a730bc6abc51417873f5ba810f5e9e48f153999950787bfc7cee4bc039b847d644637224e1d5c1194111cc5bdeab78ae0dcd87a71d889f";

leveldb::DB *qdexDB = NULL;
CCriticalSection cs_qdexDB;

bool LoadFeeds()
{
    LOCK(cs_qdexDB);
    QDexDB qdexdb;
    if (!qdexdb.Open("cw"))
        return false;

    if(!qdexdb.ReadIndexData(mapIndexesByTime))
    {
        printf("Creating local index feed data database");
	if(!qdexdb.WriteIndexData(mapIndexesByTime))
	    return false;
    }
    else
    {
        BOOST_FOREACH(const PAIRTYPE(uint64_t, CIndexFeedItem)& p, mapIndexesByTime)
	    p.second.CheckSignature();

	printf("Read %d index feed items from database.\n", mapIndexesByTime.size());
    }

    if(!qdexdb.ReadNewsData(mapNewsByTime))
    {
        printf("Creating local news feed data database\n");
	if(!qdexdb.WriteNewsData(mapNewsByTime))
	    return false;
    }
    else
    {
        BOOST_FOREACH(const PAIRTYPE(uint64_t, CNewsFeedItem)& p, mapNewsByTime)
        {
	    printf("Read news feed item with vchMsg size %d\n", p.second.vchMsg.size());
	    if(!p.second.CheckSignature())
		printf("Could not verify signature of news feed item.\n");
        }

	printf("Read %d news feed items from database.\n", mapNewsByTime.size());
    }

    BOOST_FOREACH(const PAIRTYPE(uint64_t, CIndexFeedItem)& p, mapIndexesByTime)
    {
	mapIndexes.insert(make_pair(p.second.GetHash(), p.second));
    }

    BOOST_FOREACH(const PAIRTYPE(uint64_t, CNewsFeedItem)& p, mapNewsByTime)
    {
	mapNews.insert(make_pair(p.second.GetHash(), p.second));
    }

    return true;
}

bool WriteIndexData()
{
    LOCK(cs_qdexDB);
    QDexDB qdexdb;
    if (!qdexdb.Open("cw"))
        return false;
    return qdexdb.WriteIndexData(mapIndexesByTime);
}

bool WriteNewsData()
{
    LOCK(cs_qdexDB);
    QDexDB qdexdb;
    if (!qdexdb.Open("cw"))
        return false;
    return qdexdb.WriteNewsData(mapNewsByTime);
}

void CUnsignedIndexFeedItem::SetNull()
{
    nVersion = 1;
    nRelayUntil = 0;
    nTimestamp = 0;
    nIndexValue = 0;
    strCode = "";
    strName = "";
    strSource = "";
    strLink = "";
}

std::string CUnsignedIndexFeedItem::ToString() const
{
    return strprintf(
	"CIndexFeedItem(\n"
	"    nVersion	 = %d\n"
	"    nRelayUntil = %"PRId64"\n"
	"    nTimestamp  = %"PRId64"\n"
	"    nIndexValue = %"PRId64"\n"
	"    strCode     = %s\n"
	"    strName	 = %s\n"
	"    strSource   = %s\n"
	"    strLink	 = %s\n"
	")\n",
	nVersion,
	nRelayUntil,
	nTimestamp,
	nIndexValue,
	strCode.c_str(),
	strName.c_str(),
	strSource.c_str(),
	strLink.c_str());
}

void CUnsignedIndexFeedItem::print() const
{
    printf("%s", ToString().c_str());
}

void CIndexFeedItem::SetNull()
{
    CUnsignedIndexFeedItem::SetNull();
    vchMsg.clear();
    vchSig.clear();
}

bool CIndexFeedItem::IsNull() const
{
    return (nTimestamp == 0);
}

uint256 CIndexFeedItem::GetHash() const
{
    return Hash(this->vchMsg.begin(), this->vchMsg.end());
}

bool CIndexFeedItem::RelayTo(CNode* pnode) const
{
    if(pnode->setKnown.insert(GetHash()).second)
    {
        if(GetAdjustedTime() < nRelayUntil)
	{
	    pnode->PushMessage("indexfeed", *this);
	    return true;
	}
    }
    return false;
}

bool CIndexFeedItem::CheckSignature() const
{
    CKey key;
    if (!key.SetPubKey(ParseHex(fTestNet ? pszTestKey : pszMainKey)))
        return error("CIndexFeedItem::CheckSignature() : SetPubKey failed");
    if (!key.Verify(Hash(vchMsg.begin(), vchMsg.end()), vchSig))
        return error("CIndexFeedItem::CheckSignature() : verify signature failed");

    // Now unserialize the data
    CDataStream sMsg(vchMsg, SER_NETWORK, PROTOCOL_VERSION);
    sMsg >> *(CUnsignedIndexFeedItem*)this;
    return true;
}

CIndexFeedItem CIndexFeedItem::getIndexFeedItemByHash(const uint256 &hash)
{
    CIndexFeedItem retval;
    {
        LOCK(cs_mapIndexes);
	map<uint256, CIndexFeedItem>::iterator mi = mapIndexes.find(hash);
        if(mi != mapIndexes.end())
	    retval = mi->second;
    }
    return retval;
}

bool CIndexFeedItem::ProcessIndexFeedItem()
{
    if(!CheckSignature())
	return false;
    
    LOCK(cs_mapIndexes);
    mapIndexes.insert(make_pair(GetHash(), *this));
    mapIndexesByTime.insert(make_pair((*this).nTimestamp, *this));
    WriteIndexData();
    uiInterface.NotifyIndexFeedChanged(GetHash(), CT_NEW);
    printf("accepted index feed item\n");
    return true;
}

void CUnsignedNewsFeedItem::SetNull()
{
    nVersion = 1;
    nRelayUntil = 0;
    nTimestamp = 0;
    bIsVideo = false;
    strTitle = "";
    strSummary = "";
    strVideoCode = "";
    strSource = "";
    strLink = "";
}

std::string CUnsignedNewsFeedItem::ToString() const
{
    return strprintf(
	"CNewsFeedItem(\n"
	"    nVersion	  = %d\n"
	"    nRelayUntil  = %"PRId64"\n"
	"    nTimestamp   = %"PRId64"\n"
	"    strTitle     = %s\n"
	"    strSummary	  = %s\n"
	"    strVideoCode = %s\n"
	"    strSource    = %s\n"
	"    strLink	  = %s\n"
	")\n",
	nVersion,
	nRelayUntil,
	nTimestamp,
	strTitle.c_str(),
	strSummary.c_str(),
	strVideoCode.c_str(),
	strSource.c_str(),
	strLink.c_str());
}

void CUnsignedNewsFeedItem::print() const
{
    printf("%s", ToString().c_str());
}

void CNewsFeedItem::SetNull()
{
    CUnsignedNewsFeedItem::SetNull();
    vchMsg.clear();
    vchSig.clear();
}

bool CNewsFeedItem::IsNull() const
{
    return (nTimestamp == 0);
}

uint256 CNewsFeedItem::GetHash() const
{
    return Hash(this->vchMsg.begin(), this->vchMsg.end());
}

bool CNewsFeedItem::RelayTo(CNode* pnode) const
{
    if(pnode->setKnown.insert(GetHash()).second)
    {
        if(GetAdjustedTime() < nRelayUntil)
	{
	    pnode->PushMessage("newsfeed", *this);
	    return true;
	}
    }
    return false;
}

bool CNewsFeedItem::CheckSignature() const
{
    CKey key;
    if (!key.SetPubKey(ParseHex(fTestNet ? pszTestKey : pszMainKey)))
        return error("CNewsFeedItem::CheckSignature() : SetPubKey failed");
    if (!key.Verify(Hash(vchMsg.begin(), vchMsg.end()), vchSig))
        return error("CNewsFeedItem::CheckSignature() : verify signature failed");

    // Now unserialize the data
    CDataStream sMsg(vchMsg, SER_NETWORK, PROTOCOL_VERSION);
    sMsg >> *(CUnsignedNewsFeedItem*)this;
    return true;
}

CNewsFeedItem CNewsFeedItem::getNewsFeedItemByHash(const uint256 &hash)
{
    CNewsFeedItem retval;
    {
        LOCK(cs_mapNews);
	map<uint256, CNewsFeedItem>::iterator mi = mapNews.find(hash);
        if(mi != mapNews.end())
	    retval = mi->second;
    }
    return retval;
}

bool CNewsFeedItem::ProcessNewsFeedItem()
{
    if(!CheckSignature())
	return false;
    
    LOCK(cs_mapNews);
    mapNews.insert(make_pair(GetHash(), *this));
    mapNewsByTime.insert(make_pair((*this).nTimestamp, *this));
    WriteNewsData();
    uiInterface.NotifyNewsFeedChanged(GetHash(), CT_NEW);
    printf("accepted news/video feed item\n");
    return true;
}

bool QDexDB::Open(const char* pszMode)
{
    if (qdexDB)
    {
        pdb = qdexDB;
        return true;
    };
    
    bool fCreate = strchr(pszMode, 'c');
    
    fs::path fullpath = GetDataDir() / "qdex";
    
    if (!fCreate
        && (!fs::exists(fullpath)
            || !fs::is_directory(fullpath)))
    {
        printf("QDexDB::open() - DB does not exist.\n");
        return false;
    };
    
    leveldb::Options options;
    options.create_if_missing = fCreate;
    leveldb::Status s = leveldb::DB::Open(options, fullpath.string(), &qdexDB);
    
    if (!s.ok())
    {
        printf("QDexDB::open() - Error opening db: %s.\n", s.ToString().c_str());
        return false;
    };
    
    pdb = qdexDB;
    
    return true;
};

bool QDexDB::ReadIndexData(map<uint64_t, CIndexFeedItem>& indexData)
{
    if (!pdb)
        return false;
    
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey << 'i';
    ssKey << 'f';
    std::string strValue;

    leveldb::Status s = pdb->Get(leveldb::ReadOptions(), ssKey.str(), &strValue);
    if (!s.ok())
    {
        if (s.IsNotFound())
            return false;
        printf("LevelDB read failure: %s\n", s.ToString().c_str());
        return false;
    };
    
    try {
        CDataStream ssValue(strValue.data(), strValue.data() + strValue.size(), SER_DISK, CLIENT_VERSION);
        ssValue >> indexData;
    } catch (std::exception& e) {
        printf("QDexDB::ReadIndexData() unserialize threw: %s.\n", e.what());
        return false;
    }
    
    return true;
};

bool QDexDB::WriteIndexData(map<uint64_t, CIndexFeedItem>& indexData)
{
    if (!pdb)
        return false;
    
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey << 'i';
    ssKey << 'f';
    CDataStream ssValue(SER_DISK, CLIENT_VERSION);
    ssValue.reserve(sizeof(indexData));
    ssValue << indexData;

    leveldb::WriteOptions writeOptions;
    writeOptions.sync = true;
    leveldb::Status s = pdb->Put(writeOptions, ssKey.str(), ssValue.str());
    if (!s.ok())
    {
        printf("QDexDB write failure: %s\n", s.ToString().c_str());
        return false;
    };
    
    return true;
};

bool QDexDB::ReadNewsData(map<uint64_t, CNewsFeedItem>& newsData)
{
    if (!pdb)
        return false;
    
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey << 'n';
    ssKey << 'f';
    std::string strValue;

    leveldb::Status s = pdb->Get(leveldb::ReadOptions(), ssKey.str(), &strValue);
    if (!s.ok())
    {
        if (s.IsNotFound())
            return false;
        printf("LevelDB read failure: %s\n", s.ToString().c_str());
        return false;
    };
    
    try {
        CDataStream ssValue(strValue.data(), strValue.data() + strValue.size(), SER_DISK, CLIENT_VERSION);
        ssValue >> newsData;
    } catch (std::exception& e) {
        printf("QDexDB::ReadNewsData() unserialize threw: %s.\n", e.what());
        return false;
    }
    
    return true;
};

bool QDexDB::WriteNewsData(map<uint64_t, CNewsFeedItem>& newsData)
{
    if (!pdb)
        return false;
    
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey << 'n';
    ssKey << 'f';
    CDataStream ssValue(SER_DISK, CLIENT_VERSION);
    ssValue.reserve(sizeof(newsData));
    ssValue << newsData;

    leveldb::WriteOptions writeOptions;
    writeOptions.sync = true;
    leveldb::Status s = pdb->Put(writeOptions, ssKey.str(), ssValue.str());
    if (!s.ok())
    {
        printf("QDexDB write failure: %s\n", s.ToString().c_str());
        return false;
    };
    
    return true;
};
