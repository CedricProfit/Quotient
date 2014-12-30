// Copyright (c) 2014 The Quotient developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef _QDEX_H_
#define _QDEX_H_ 1

#include <leveldb/db.h>
#include <leveldb/write_batch.h>

#include <map>
#include <set>
#include <string>

#include "db.h"
#include "uint256.h"
#include "util.h"

class CNode;

extern CCriticalSection cs_qdexDB;

bool LoadFeeds();
bool WriteIndexData();
bool WriteNewsData();

class CUnsignedIndexFeedItem
{
public:
    int nVersion;
    int64_t nRelayUntil;
    std::string strCode;
    std::string strName;
    std::string strSource;
    std::string strLink;
    int64_t nTimestamp;
    int64_t nIndexValue; // in bitcoin amount format (e.g. n * COIN)
    
    IMPLEMENT_SERIALIZE
    (
	READWRITE(this->nVersion);
        READWRITE(nRelayUntil);
	READWRITE(nTimestamp);
	READWRITE(nIndexValue);
	READWRITE(strCode);
	READWRITE(strName);
	READWRITE(strSource);
	READWRITE(strLink);
    )

    void SetNull();
    std::string ToString() const;
    void print() const;
};

class CIndexFeedItem : public CUnsignedIndexFeedItem
{
public:
    std::vector<unsigned char> vchMsg;
    std::vector<unsigned char> vchSig;

    CIndexFeedItem()
    {
	SetNull();
    }

    IMPLEMENT_SERIALIZE
    (
	READWRITE(vchMsg);
	READWRITE(vchSig);
    )

    void SetNull();
    bool IsNull() const;
    uint256 GetHash() const;
    bool RelayTo(CNode* pnode) const;
    bool CheckSignature() const;
    bool ProcessIndexFeedItem();
    static CIndexFeedItem getIndexFeedItemByHash(const uint256 &hash);
};

class CUnsignedNewsFeedItem
{
public:
    int nVersion;
    int64_t nRelayUntil;
    int64_t nTimestamp;
    bool bIsVideo;
    std::string strTitle;
    std::string strSummary;
    std::string strVideoCode;
    std::string strLink;
    std::string strSource;

    IMPLEMENT_SERIALIZE
    (
	READWRITE(nVersion);
	READWRITE(nRelayUntil);
	READWRITE(nTimestamp);
	READWRITE(bIsVideo);
	READWRITE(strTitle);
	READWRITE(strSummary);
	READWRITE(strVideoCode);
	READWRITE(strLink);
	READWRITE(strSource);
    )

    void SetNull();
    std::string ToString() const;
    void print() const;
};

class CNewsFeedItem : public CUnsignedNewsFeedItem
{
public:
    std::vector<unsigned char> vchMsg;
    std::vector<unsigned char> vchSig;

    CNewsFeedItem()
    {
	SetNull();
    }

    IMPLEMENT_SERIALIZE
    (
	READWRITE(vchMsg);
	READWRITE(vchSig);
    )

    void SetNull();
    bool IsNull() const;
    uint256 GetHash() const;
    bool RelayTo(CNode* pnode) const;
    bool CheckSignature() const;
    bool ProcessNewsFeedItem();
    static CNewsFeedItem getNewsFeedItemByHash(const uint256 &hash);
};

class QDexDB
{
public:
    QDexDB()
    {
    };
    
    ~QDexDB()
    {
    };
    
    bool Open(const char* pszMode="r+");
    
    bool ReadIndexData(std::map<uint64_t, CIndexFeedItem>& indexData);
    bool WriteIndexData(std::map<uint64_t, CIndexFeedItem>& indexData);
    bool ReadNewsData(std::map<uint64_t, CNewsFeedItem>& newsData);
    bool WriteNewsData(std::map<uint64_t, CNewsFeedItem>& newsData);
        
    leveldb::DB *pdb;       // points to the global instance
    leveldb::WriteBatch *activeBatch;
};

#endif
