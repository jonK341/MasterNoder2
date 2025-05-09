// Copyright (c) 2014-2016 The Dash developers
// Copyright (c) 2016-2018 The PIVX developers
// Copyright (c) 2024- The MasterNoder2 developers
// Distributed under the MIT/X11 software license, see the accompanying

#ifndef SPORK_H
#define SPORK_H

#include "base58.h"
#include "key.h"
#include "main.h"
#include "net.h"
#include "sync.h"
#include "util.h"

#include "obfuscation.h"
#include "protocol.h"

using namespace std;
using namespace boost;

/*
    Don't ever reuse these IDs for other sporks
    - This would result in old clients getting confused about which spork is for what

    Sporks 11,12, and 16 to be removed with 1st zerocoin release
*/
#define SPORK_START 10001
#define SPORK_END 10111

#define SPORK_2_SWIFTTX 10001
#define SPORK_3_SWIFTTX_BLOCK_FILTERING 10002
#define SPORK_5_MAX_VALUE 10004
#define SPORK_6_ENABLE_DIP0001 10005
#define SPORK_7_MASTERNODE_SCANNING 10006
#define SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT 10007
#define SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT 10008
#define SPORK_10_MASTERNODE_PAY_UPDATED_NODES 10009
#define SPORK_13_ENABLE_SUPERBLOCKS 10012
#define SPORK_14_NEW_PROTOCOL_ENFORCEMENT 10013
#define SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2 10014
#define SPORK_16_ZEROCOIN_MAINTENANCE_MODE 10015
#define SPORK_17_QUORUM_DKG_ENABLED 10016
#define SPORK_18_ENABLE_VOTING_SYSTEM 10017
#define SPORK_19_CHAINLOCKS_ENABLED 10018
#define SPORK_20_DEVELOPER_PAYMENT_ENFORCEMENT 10019
#define SPORK_21_QUORUM_ALL_CONNECTED 10021
#define SPORK_22_ENABLE_TX_COMPRESSION 10022
#define SPORK_23_QUORUM_POSE 10023
#define SPORK_24_ENABLE_SIPHASH 10024
#define SPORK_25_ENABLE_MASTERNODER2_SERVER 10025
#define SPORK_28_ENABLE_TIMELOCK 10028
#define SPORK_29_ENABLE_ANONYMITY 10029
#define SPORK_101_SERVICES_ENFORCEMENT 10101
#define SPORK_104_MAX_BLOCK_TIME 10104
#define SPORK_106_STAKING_SKIP_MN_SYNC 10106
#define SPORK_109_FORCE_ENABLED_VOTED_MASTERNODE 10109
#define SPORK_110_FORCE_ENABLED_MASTERNODE_PAYMENT 10110
#define SPORK_111_ALLOW_DUPLICATE_MN_IPS 10111

#define SPORK_2_SWIFTTX_DEFAULT 978307200                             //2001-1-1
#define SPORK_3_SWIFTTX_BLOCK_FILTERING_DEFAULT 1424217600            //2015-2-18
#define SPORK_5_MAX_VALUE_DEFAULT 1000                                //1000 MN2
#define SPORK_6_ENABLE_DIP0001_DEFAULT 1703122560                     //2024-12-21 Effective now
#define SPORK_7_MASTERNODE_SCANNING_DEFAULT 978307200                 //2001-1-1
#define SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT_DEFAULT 4070908800     //OFF
#define SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT_DEFAULT 4070908800      //OFF
#define SPORK_10_MASTERNODE_PAY_UPDATED_NODES_DEFAULT 4070908800      //OFF
#define SPORK_13_ENABLE_SUPERBLOCKS_DEFAULT 4070908800                //OFF
#define SPORK_14_NEW_PROTOCOL_ENFORCEMENT_DEFAULT 4070908800          //OFF
#define SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2_DEFAULT 4070908800        //OFF
#define SPORK_16_ZEROCOIN_MAINTENANCE_MODE_DEFAULT 4070908800         //OFF
#define SPORK_17_QUORUM_DKG_ENABLED_DEAFULT 1703122560                //2024-12-21 Effective now
#define SPORK_18_ENABLE_VOTING_SYSTEM_DEFAULT 1703122560              //2024-12-21 Effective now
#define SPORK_19_CHAINLOCKS_ENABLED_DEFAULT 1703122560                //2024-12-21 Effective now
#define SPORK_20_DEVELOPER_PAYMENT_ENFORCEMENT_DEFAULT 1703122560     //2024-12-21 Effective now
#define SPORK_21_QUORUM_ALL_CONNECTED_DEFAULT 1703122560              //2024-12-21 Effective now
#define SPORK_22_ENABLE_TX_COMPRESSION_DEFAULT 1703122560             //2024-12-21 Effective now
#define SPORK_23_QUORUM_POSE_DEFAULT 1703122560                       //2024-12-21 Effective now
#define SPORK_24_ENABLE_SIPHASH_DEFAULT 1703122560                            //2024-12-21 Effective now
#define SPORK_25_ENABLE_MASTERNODER2_SERVER_DEFAULT 4070908800                //OFF
#define SPORK_28_ENABLE_TIMELOCK_DEFAULT 1734658560                  //2025-01-01
#define SPORK_29_ENABLE_ANONYMITY_DEFAULT 1734658560                  //2025-01-01
#define SPORK_101_SERVICES_ENFORCEMENT_DEFAULT 1703122560             //2024-12-21 Effective now
#define SPORK_104_MAX_BLOCK_TIME_DEFAULT 600                          //10 mins max Blocktime
#define SPORK_106_STAKING_SKIP_MN_SYNC_DEFAULT 1703122560             //2024-12-21 Effective now
#define SPORK_109_FORCE_ENABLED_VOTED_MASTERNODE_DEFAULT 1703122560   //2024-12-21 Effective now
#define SPORK_110_FORCE_ENABLED_MASTERNODE_PAYMENT_DEFAULT 4070908800 //OFF
#define SPORK_111_ALLOW_DUPLICATE_MN_IPS_DEFAULT 1703122560                   //2024-12-21 Effective now

class CSporkMessage;
class CSporkManager;

extern std::map<uint256, CSporkMessage> mapSporks;
extern std::map<int, CSporkMessage> mapSporksActive;
extern CSporkManager sporkManager;

void LoadSporksFromDB();
void ProcessSpork(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
int64_t GetSporkValue(int nSporkID);
bool IsSporkActive(int nSporkID);
void ReprocessBlocks(int nBlocks);

//
// Spork Class
// Keeps track of all of the network spork settings
//

class CSporkMessage
{
public:
    std::vector<unsigned char> vchSig;
    int nSporkID;
    int64_t nValue;
    int64_t nTimeSigned;

    uint256 GetHash()
    {
        uint256 n = HashQuark(BEGIN(nSporkID), END(nTimeSigned));
        return n;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(nSporkID);
        READWRITE(nValue);
        READWRITE(nTimeSigned);
        READWRITE(vchSig);
    }
};


class CSporkManager
{
private:
    std::vector<unsigned char> vchSig;
    std::string strMasterPrivKey;

public:
    CSporkManager()
    {
    }

    std::string GetSporkNameByID(int id);
    int GetSporkIDByName(std::string strName);
    bool UpdateSpork(int nSporkID, int64_t nValue);
    bool SetPrivKey(std::string strPrivKey);
    bool CheckSignature(CSporkMessage& spork, bool fCheckSigner = false);
    bool Sign(CSporkMessage& spork);
    void Relay(CSporkMessage& msg);
};

#endif
