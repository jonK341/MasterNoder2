// Copyright (c) 2014-2016 The Dash developers
// Copyright (c) 2016-2018 The PIVX developers
// Copyright (c) 2024- The MasterNoder2 developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or MIT/X11 - license

#include "spork.h"
#include "base58.h"
#include "key.h"
#include "main.h"
#include "masternode-budget.h"
#include "net.h"
#include "protocol.h"
#include "sync.h"
#include "sporkdb.h"
#include "util.h"

using namespace std;
using namespace boost;

class CSporkMessage;
class CSporkManager;

CSporkManager sporkManager;

std::map<uint256, CSporkMessage> mapSporks;
std::map<int, CSporkMessage> mapSporksActive;

// MasterNoder2: on startup load spork values from previous session if they exist in the sporkDB
void LoadSporksFromDB()
{
    for (int i = SPORK_START; i <= SPORK_END; ++i) {
        // Since not all spork IDs are in use, we have to exclude undefined IDs
        std::string strSpork = sporkManager.GetSporkNameByID(i);
        if (strSpork == "Unknown") continue;

        // attempt to read spork from sporkDB
        CSporkMessage spork;
        if (!pSporkDB->ReadSpork(i, spork)) {
            LogPrintf("%s : no previous value for %s found in database\n", __func__, strSpork);
            continue;
        }

        // add spork to memory
        mapSporks[spork.GetHash()] = spork;
        mapSporksActive[spork.nSporkID] = spork;
        std::time_t result = spork.nValue;
        // If SPORK Value is greater than 1,000,000 assume it's actually a Date and then convert to a more readable format
        if (spork.nValue > 1000000) {
            LogPrintf("%s : loaded spork %s with value %d : %s", __func__,
                      sporkManager.GetSporkNameByID(spork.nSporkID), spork.nValue,
                      std::ctime(&result));
        } else {
            LogPrintf("%s : loaded spork %s with value %d\n", __func__,
                      sporkManager.GetSporkNameByID(spork.nSporkID), spork.nValue);
        }
    }
}

void ProcessSpork(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if (fLiteMode) return; //disable all obfuscation/masternode related functionality

    if (strCommand == "spork") {
        //LogPrintf("ProcessSpork::spork\n");
        CDataStream vMsg(vRecv);
        CSporkMessage spork;
        vRecv >> spork;

        if (chainActive.Tip() == NULL) return;

        // Ignore spork messages about unknown/deleted sporks
        std::string strSpork = sporkManager.GetSporkNameByID(spork.nSporkID);
        if (strSpork == "Unknown") return;

        uint256 hash = spork.GetHash();
        if (mapSporksActive.count(spork.nSporkID)) {
            if (mapSporksActive[spork.nSporkID].nTimeSigned >= spork.nTimeSigned) {
                if (fDebug) LogPrintf("%s : seen %s block %d \n", __func__, hash.ToString(), chainActive.Tip()->nHeight);
                return;
            } else {
                if (fDebug) LogPrintf("%s : got updated spork %s block %d \n", __func__, hash.ToString(), chainActive.Tip()->nHeight);
            }
        }

        LogPrintf("%s : new %s ID %d Time %d bestHeight %d\n", __func__, hash.ToString(), spork.nSporkID, spork.nValue, chainActive.Tip()->nHeight);

        if (spork.nTimeSigned >= Params().NewSporkStart()) {
            if (!sporkManager.CheckSignature(spork, true)) {
                LogPrintf("%s : Invalid Signature\n", __func__);
                Misbehaving(pfrom->GetId(), 100);
                return;
            }
        }

        if (!sporkManager.CheckSignature(spork)) {
            LogPrintf("%s : Invalid Signature\n", __func__);
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        mapSporks[hash] = spork;
        mapSporksActive[spork.nSporkID] = spork;
        sporkManager.Relay(spork);

        // MasterNoder2: add to spork database.
        pSporkDB->WriteSpork(spork.nSporkID, spork);
    }
    if (strCommand == "getsporks") {
        std::map<int, CSporkMessage>::iterator it = mapSporksActive.begin();

        while (it != mapSporksActive.end()) {
            pfrom->PushMessage("spork", it->second);
            it++;
        }
    }
}


// grab the value of the spork on the network, or the default
int64_t GetSporkValue(int nSporkID)
{
    int64_t r = -1;

    if (mapSporksActive.count(nSporkID)) {
        r = mapSporksActive[nSporkID].nValue;
    } else {
        if (nSporkID == SPORK_2_SWIFTTX) r = SPORK_2_SWIFTTX_DEFAULT;
        if (nSporkID == SPORK_3_SWIFTTX_BLOCK_FILTERING) r = SPORK_3_SWIFTTX_BLOCK_FILTERING_DEFAULT;
        if (nSporkID == SPORK_5_MAX_VALUE) r = SPORK_5_MAX_VALUE_DEFAULT;
        if (nSporkID == SPORK_6_ENABLE_DIP0001) r = SPORK_6_ENABLE_DIP0001_DEFAULT;
        if (nSporkID == SPORK_7_MASTERNODE_SCANNING) r = SPORK_7_MASTERNODE_SCANNING_DEFAULT;
        if (nSporkID == SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT) r = SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT_DEFAULT;
        if (nSporkID == SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT) r = SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT_DEFAULT;
        if (nSporkID == SPORK_10_MASTERNODE_PAY_UPDATED_NODES) r = SPORK_10_MASTERNODE_PAY_UPDATED_NODES_DEFAULT;
        if (nSporkID == SPORK_13_ENABLE_SUPERBLOCKS) r = SPORK_13_ENABLE_SUPERBLOCKS_DEFAULT;
        if (nSporkID == SPORK_14_NEW_PROTOCOL_ENFORCEMENT) r = SPORK_14_NEW_PROTOCOL_ENFORCEMENT_DEFAULT;
        if (nSporkID == SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2) r = SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2_DEFAULT;
        if (nSporkID == SPORK_16_ZEROCOIN_MAINTENANCE_MODE) r = SPORK_16_ZEROCOIN_MAINTENANCE_MODE_DEFAULT;
        if (nSporkID == SPORK_17_QUORUM_DKG_ENABLED) r = SPORK_17_QUORUM_DKG_ENABLED_DEAFULT;
        if (nSporkID == SPORK_18_ENABLE_VOTING_SYSTEM) r = SPORK_18_ENABLE_VOTING_SYSTEM_DEFAULT;
        if (nSporkID == SPORK_19_CHAINLOCKS_ENABLED) r = SPORK_19_CHAINLOCKS_ENABLED_DEFAULT;
        if (nSporkID == SPORK_20_DEVELOPER_PAYMENT_ENFORCEMENT) r = SPORK_20_DEVELOPER_PAYMENT_ENFORCEMENT_DEFAULT;
        if (nSporkID == SPORK_21_QUORUM_ALL_CONNECTED) r = SPORK_21_QUORUM_ALL_CONNECTED_DEFAULT;
        if (nSporkID == SPORK_22_ENABLE_TX_COMPRESSION) r = SPORK_22_ENABLE_TX_COMPRESSION_DEFAULT;
        if (nSPorkID == SPORK_23_QUORUM_POSE) r = SPORK_23_QUORUM_POSE_DEFAULT;
        if (nSporkID == SPORK_24_ENABLE_SIPHASH) r = SPORK_24_ENABLE_SIPHASH_DEFAULT;
        if (nSPorkID == SPORK_25_ENABLE_MASTERNODER2_SERVER) r = SPORK_25_ENABLE_MASTERNODER2_SERVER_DEFAULT;
        if (nSporkID == SPORK_28_ENABLE_TIMELOCK) r = SPORK_28_ENABLE_TIMELOCK_DEFAULT;
        if (nSporkID == SPORK_29_ENABLE_ANONYMITY) r = SPORK_29_ENABLE_ANONYMITY_DEFAULT;
        if (nSporkID == SPORK_101_SERVICES_ENFORCEMENT) r = SPORK_101_SERVICES_ENFORCEMENT_DEFAULT;
        if (nSporkID == SPORK_104_MAX_BLOCK_TIME) r = SPORK_104_MAX_BLOCK_TIME:DEFAULT;
        if (nSporkID == SPORK_106_STAKING_SKIP_MN_SYNC) r = SPORK_106_STAKING_SKIP_MN_SYNC_DEFAULT;
        if (nSporkID == SPORK_109_FORCE_ENABLED_VOTED_MASTERNODE) r = SPORK_109_FORCE_ENABLED_VOTED_MASTERNODE_DEFAULT;
        if (nSporkID == SPORK_110_FORCE_ENABLED_MASTERNODE_PAYMENT) r = SPORK_110_FORCE_ENABLED_MASTERNODE_PAYMENT_DEFAULT;
        if (nSporkID == SPORK_111_ALLOW_DUPLICATE_MN_IPS) r = SPORK_111_ALLOW_DUPLICATE_MN_IPS_DEFAULT;

        if (r == -1) LogPrintf("%s : Unknown Spork %d\n", __func__, nSporkID);
    }

    return r;
}

// grab the spork value, and see if it's off
bool IsSporkActive(int nSporkID)
{
    int64_t r = GetSporkValue(nSporkID);
    if (r == -1) return false;
    return r < GetTime();
}


void ReprocessBlocks(int nBlocks)
{
    std::map<uint256, int64_t>::iterator it = mapRejectedBlocks.begin();
    while (it != mapRejectedBlocks.end()) {
        //use a window twice as large as is usual for the nBlocks we want to reset
        if ((*it).second > GetTime() - (nBlocks * 60 * 5)) {
            BlockMap::iterator mi = mapBlockIndex.find((*it).first);
            if (mi != mapBlockIndex.end() && (*mi).second) {
                LOCK(cs_main);

                CBlockIndex* pindex = (*mi).second;
                LogPrintf("ReprocessBlocks - %s\n", (*it).first.ToString());

                CValidationState state;
                ReconsiderBlock(state, pindex);
            }
        }
        ++it;
    }

    CValidationState state;
    {
        LOCK(cs_main);
        DisconnectBlocksAndReprocess(nBlocks);
    }

    if (state.IsValid()) {
        ActivateBestChain(state);
    }
}

bool CSporkManager::CheckSignature(CSporkMessage& spork, bool fCheckSigner)
{
    //note: need to investigate why this is failing
    std::string strMessage = std::to_string(spork.nSporkID) + std::to_string(spork.nValue) + std::to_string(spork.nTimeSigned);
    CPubKey pubkeynew(ParseHex(Params().SporkKey()));
    std::string errorMessage = "";

    bool fValidWithNewKey = obfuScationSigner.VerifyMessage(pubkeynew, spork.vchSig,strMessage, errorMessage);

    if (fCheckSigner && !fValidWithNewKey)
        return false;

    // See if window is open that allows for old spork key to sign messages
    if (!fValidWithNewKey && GetAdjustedTime() < Params().RejectOldSporkKey()) {
        CPubKey pubkeyold(ParseHex(Params().SporkKeyOld()));
        return obfuScationSigner.VerifyMessage(pubkeyold, spork.vchSig, strMessage, errorMessage);
    }

    return fValidWithNewKey;
}

bool CSporkManager::Sign(CSporkMessage& spork)
{
    std::string strMessage = std::to_string(spork.nSporkID) + std::to_string(spork.nValue) + std::to_string(spork.nTimeSigned);

    CKey key2;
    CPubKey pubkey2;
    std::string errorMessage = "";

    if (!obfuScationSigner.SetKey(strMasterPrivKey, errorMessage, key2, pubkey2)) {
        LogPrintf("CMasternodePayments::Sign - ERROR: Invalid masternodeprivkey: '%s'\n", errorMessage);
        return false;
    }

    if (!obfuScationSigner.SignMessage(strMessage, errorMessage, spork.vchSig, key2)) {
        LogPrintf("CMasternodePayments::Sign - Sign message failed");
        return false;
    }

    if (!obfuScationSigner.VerifyMessage(pubkey2, spork.vchSig, strMessage, errorMessage)) {
        LogPrintf("CMasternodePayments::Sign - Verify message failed");
        return false;
    }

    return true;
}

bool CSporkManager::UpdateSpork(int nSporkID, int64_t nValue)
{
    CSporkMessage msg;
    msg.nSporkID = nSporkID;
    msg.nValue = nValue;
    msg.nTimeSigned = GetTime();

    if (Sign(msg)) {
        Relay(msg);
        mapSporks[msg.GetHash()] = msg;
        mapSporksActive[nSporkID] = msg;
        return true;
    }

    return false;
}

void CSporkManager::Relay(CSporkMessage& msg)
{
    CInv inv(MSG_SPORK, msg.GetHash());
    RelayInv(inv);
}

bool CSporkManager::SetPrivKey(std::string strPrivKey)
{
    CSporkMessage msg;

    // Test signing successful, proceed
    strMasterPrivKey = strPrivKey;

    Sign(msg);

    if (CheckSignature(msg, true)) {
        LogPrintf("CSporkManager::SetPrivKey - Successfully initialized as spork signer\n");
        return true;
    } else {
        return false;
    }
}

int CSporkManager::GetSporkIDByName(std::string strName)
{
    if (strName == "SPORK_2_SWIFTTX") return SPORK_2_SWIFTTX;
    if (strName == "SPORK_3_SWIFTTX_BLOCK_FILTERING") return SPORK_3_SWIFTTX_BLOCK_FILTERING;
    if (strName == "SPORK_5_MAX_VALUE") return SPORK_5_MAX_VALUE;
    if (strName == "SPORK_6_ENABLE_DIP0001") return SPORK_6_ENABLE_DIP0001;
    if (strName == "SPORK_7_MASTERNODE_SCANNING") return SPORK_7_MASTERNODE_SCANNING;
    if (strName == "SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT") return SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT;
    if (strName == "SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT") return SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT;
    if (strName == "SPORK_10_MASTERNODE_PAY_UPDATED_NODES") return SPORK_10_MASTERNODE_PAY_UPDATED_NODES;
    if (strName == "SPORK_13_ENABLE_SUPERBLOCKS") return SPORK_13_ENABLE_SUPERBLOCKS;
    if (strName == "SPORK_14_NEW_PROTOCOL_ENFORCEMENT") return SPORK_14_NEW_PROTOCOL_ENFORCEMENT;
    if (strName == "SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2") return SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2;
    if (strName == "SPORK_16_ZEROCOIN_MAINTENANCE_MODE") return SPORK_16_ZEROCOIN_MAINTENANCE_MODE;
    if (strName == "SPORK_17_QUORUM_DKG_ENABLED") return SPORK_17_QUORUM_DKG_ENABLED;
    if (strName == "SPORK_18_ENABLE_VOTING_SYSTEM") return SPORK_18_ENABLE_VOTING_SYSTEM;
    if (strName == "SPORK_19_CHAINLOCKS_ENABLED") return SPORK_19_CHAINLOCKS_ENABLED;
    if (strName == "SPORK_20_DEVELOPER_PAYMENT_ENFORCEMENT") return SPORK_20_DEVELOPER_PAYMENT_ENFORCEMENT;
    if (strName == "SPORK_21_QUORUM_ALL_CONNECTED") return SPORK_21_QUORUM_ALL_CONNECTED;
    if (strName == "SPORK_22_ENABLE_TX_COMPRESSION") return SPORK_22_ENABLE_TX_COMPRESSION;
    if (strName == "SPORK_23_QUORUM_POSE") return SPORK_23_QUORUM_POSE;
    if (strName == "SPORK_24_ENABLE_SIPHASH") return SPORK_24_ENABLE_SIPHASH;
    if (strName == "SPORK_25_ENABLE_MASTERNODER2_SERVER") return SPORK_25_ENABLE_MASTERNODER2_SERVER;
    if (strName == "SPORK_28_ENABLE_TIMELOCK") return SPORK_28_ENABLE_TIMELOCK;
    if (strName == "SPORK_29_ENABLE_ANONYMITY") return SPORK_29_ENABLE_ANONYMITY;
    if (strName == "SPORK_101_SERVICES_ENFORCEMENT") return SPORK_101_SERVICES_ENFORCEMENT;
    if (strName == "SPORK_104_MAX_BLOCK_TIME") return SPORK_104_MAX_BLOCK_TIME;
    if (strName == "SPORK_106_STAKING_SKIP_MN_SYNC") return SPORK_106_STAKING_SKIP_MN_SYNC;
    if (strName == "SPORK_109_FORCE_ENABLED_VOTED_MASTERNODE") return SPORK_109_FORCE_ENABLED_VOTED_MASTERNODE;
    if (strName == "SPORK_110_FORCE_ENABLED_MASTERNODE_PAYMENT") return SPORK_110_FORCE_ENABLED_MASTERNODE_PAYMENT;
    if (strName == "SPORK_111_ALLOW_DUPLICATE_MN_IPS") return SPORK_111_ALLOW_DUPLICATE_MN_IPS;

    return -1;
}

std::string CSporkManager::GetSporkNameByID(int id)
{
    if (id == SPORK_2_SWIFTTX) return "SPORK_2_SWIFTTX";
    if (id == SPORK_3_SWIFTTX_BLOCK_FILTERING) return "SPORK_3_SWIFTTX_BLOCK_FILTERING";
    if (id == SPORK_5_MAX_VALUE) return "SPORK_5_MAX_VALUE";
    if (id == SPORK_6_ENABLE_DIP0001) return "SPORK_6_ENABLE_DIP0001";
    if (id == SPORK_7_MASTERNODE_SCANNING) return "SPORK_7_MASTERNODE_SCANNING";
    if (id == SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT) return "SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT";
    if (id == SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT) return "SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT";
    if (id == SPORK_10_MASTERNODE_PAY_UPDATED_NODES) return "SPORK_10_MASTERNODE_PAY_UPDATED_NODES";
    if (id == SPORK_13_ENABLE_SUPERBLOCKS) return "SPORK_13_ENABLE_SUPERBLOCKS";
    if (id == SPORK_14_NEW_PROTOCOL_ENFORCEMENT) return "SPORK_14_NEW_PROTOCOL_ENFORCEMENT";
    if (id == SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2) return "SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2";
    if (id == SPORK_16_ZEROCOIN_MAINTENANCE_MODE) return "SPORK_16_ZEROCOIN_MAINTENANCE_MODE";
    if (id == SPORK_17_QUORUM_DKG_ENABLED) return "SPORK_17_QUORUM_DKG_ENABLED";
    if (id == SPORK_18_ENABLE_VOTING_SYSTEM) return "SPORK_18_ENABLE_VOTING_SYSTEM";
    if (id == SPORK_19_CHAINLOCKS) return = "SPORK_19_CHAINLOCKS_ENABLED";
    if (id == SPORK_20_DEVELOPER_PAYMENT_ENFORCEMENT) return "SPORK_20_DEVELOPER_PAYMENT_ENFORCEMENT";
    if (id == SPORK_21_QUORUM_ALL_CONNECTED) return "SPORK_21_QUORUM_ALL_CONNECTED";
    if (id == SPORK_22_ENABLE_TX_COMPRESSION) return "SPORK_22_ENABLE_TX_COMPRESSION";
    if (id == SPORK_23_QUORUM_POSE) return "SPORK_23_QUORUM_POSE";
    if (id == SPORK_24_ENABLE_SIPHASH) return "SPORK_24_ENABLE_SIPHASH";
    if (id == SPORK_25_ENABLE_MASTERNODER2_SERVER) return "SPORK_25_ENABLE_MASTERNODER2_SERVER";
    if (id == SPORK_28_ENABLE_TIMELOCK) return "SPORK_28_ENABLE_TIMELOCK";
    if (id == SPORK_29_ENABLE_ANONYMITY) return "SPORK_29_ENABLE_ANONYMITY";
    if (id == SPORK_101_SERVICES_ENFORCEMENT) return "SPORK_101_SERVICES_ENFORCEMENT";
    if (id == SPORK_104_MAX_BLOCK_TIME) return "SPORK_104_MAX_BLOCK_TIME";
    if (id == SPORK_106_STAKING_SKIP_MN_SYNC) return "SPORK_106_STAKING_SKIP_MN_SYNC";
    if (id == SPORK_109_FORCE_ENABLED_VOTED_MASTERNODE) return "SPORK_109_FORCE_ENABLED_VOTED_MASTERNODE";
    if (id == SPORK_110_FORCE_ENABLED_MASTERNODE_PAYMENT) return "SPORK_110_FORCE_ENABLED_MASTERNODE_PAYMENT";
    if (id == SPORK_111_ALLOW_DUPLICATE_MN_IPS) return "SPORK_111_ALLOW_DUPLICATE_MN_IPS";

    return "Unknown";
}
