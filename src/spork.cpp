// Copyright (c) 2014-2016 The Dash developers
// Copyright (c) 2016-2018 The PIVX developers
// Copyright (c) 2024-2025 The MasterNoder2 developers
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
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "arpa/inet.h"
#include "unistd.h"
#include "time.h"
#include "cstdlib.h" 
#include "vector.h"

#define BUFFER_SIZE 128
#define TIMEOUT 2 // Timeout duration in seconds
#define MAX_RETRIES 3 // Maximum number of retry attempts
#define PORT 17646  // Example port for communication
#define NODE_COUNT 3 // Number of predefined nodes
#define NEW_NODE_START 192168100 // Starting IP for scanning new nodes
#define NEW_NODE_END 192168110 // Ending IP for scanning new nodes

using namespace std;
using namespace boost;

class CSporkMessage  {};
class CSporkManager {};

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

// Function prototypes
void CheckQuorumAndProceed(int nSporkID, int nValue, const std::string& strCommand);
void HaltOperations();
void ConnectAndDebug(int nSporkID, int nValue);
bool CheckConnectionsAgain(int requiredQuorum);
void MeasureMasterNodeState();
void ProceedWithOperations(int nSporkID, int nValue);
void OperationOne(int nSporkID, int nValue);
void OperationTwo(int nSporkID, int nValue);
void OperationThree(int nSporkID, int nValue);
void OperationFour(int nSporkID, int nValue);
void OperationFive(int nSporkID, int nValue);
void ResetDaemon();
void AssessMasterNodeHealth();
void LogEvent(const std::string& message);
void SendStatusMessageToNodes(const std::string& message);
int GetConnectedNodeCount();
bool AttemptToConnectToMoreNodes();
void EnableErrorDebugging();

// Main function to check quorum and proceed
void CheckQuorumAndProceed(int nSPorkID, int nValue, , const std::string& strCommand) {
    int conectedNodes = GetConnectedNodeCount();
    int requiredQuorum = SPORK_21QUORUM_ALL_CONNECTED_DEFAULT;

    if (connectedNodes < requiredQuorum) {
                LogEvent("⚠️ Not enough nodes connected. Required: " + std::to_string(requiredQuorum) + ", Connected: " + std::to_string(connectedNodes));
        
        // Optionally halt operations or take corrective action
        HaltOperations();
        ConnectAndDebug(nSporkID, nValue);
        if (!CheckConnectionsAgain(requiredQuorum)) {
            LogError("❌ Still not enough connections after attempting to connect.");
            return; // Exit if connections are still insufficient
        }
        
        MeasureMasterNodeState();
        return; // Exit if we have taken corrective actions
    }
    
    // Proceed with operations since quorum is met
    ProceedWithOperations(nSporkID, nValue);
}

void HaltOperations() {
    LogInfo("⏸️ Halting operations due to insufficient connected nodes.");
    // Implement logic to halt critical operations
}

// Function to connect and debug
void ConnectAndDebug(int nSporkID, int nValue) {
    LogEvent("🔗 Attempting to connect to additional nodes with debugging enabled.");
    EnableErrorDebugging(); // Enable detailed error logging

    // Logic to restart the daemon and count all nodes' IPs
    ResetDaemon();
    if (!AttemptToConnectToMoreNodes()) {
        LogEvent("❌ Failed to connect to additional nodes.");
        return; // Exit if connection fails
    }
}

// Function to check connections again
bool CheckConnectionsAgain(int requiredQuorum) {
    int connectedNodes = GetConnectedNodeCount();

    LogEvent("🔄 Rechecking connections...");
    if (connectedNodes >= requiredQuorum) {
        LogEvent("✅ All required nodes are now connected.");
        return true;
    } else {
        LogEvent("⚠️ Still insufficient connections. Required: " + std::to_string(requiredQuorum) + ", Connected: " + std::to_string(connectedNodes));
        return false;
    }
}

// Function to measure master node state
void MeasureMasterNodeState() {
    LogEvent("🔍 Measuring the state of master nodes...");
    AssessMasterNodeHealth(); // Assess the status of master nodes
}

// Function to proceed with operations
void ProceedWithOperations(int nSporkID, int nValue) {
    LogEvent("🚀 Proceeding with operations for Spork ID: " + std::to_string(nSporkID) + ", Value: " + std::to_string(nValue));

    // The five connection operations
    OperationOne(nSporkID, nValue);
    OperationTwo(nSporkID, nValue);
    OperationThree(nSporkID, nValue);
    OperationFour(nSporkID, nValue);
    OperationFive(nSporkID, nValue);
}

// Function to check if a node is online by pinging its IP address
int IsNodeOnline(const char* ipAddress) {
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "ping -c 1 %s > /dev/null 2>&1", ipAddress); // For Unix/Linux
    // For Windows, use: snprintf(command, sizeof(command), "ping -n 1 %s > NUL", ipAddress);

    // Execute the ping command
    int result = system(command);
    
    // Check the result of the ping command
    if (result == 0) {
        return 1; // Node is online
    } else {
        return 0; // Node is offline
    }
}

// Operation 1: Check if nodes are online
void OperationOne(int nSporkID, int nValue) {
    const char* nodes[] = {
        "173.249.6.78:17646",
        "31.220.72.12:17646",
        "109.56.253.247:17646"
    };
    int nodeCount = sizeof(nodes) / sizeof(nodes[0]);

    LogEvent("✅ Checking if nodes are online...");

    for (int i = 0; i < nodeCount; i++) {
        const char* ipAddress = nodes[i];
        if (IsNodeOnline(ipAddress)) {
            LogEvent("✅ Node %s is online.", ipAddress);
        } else {
            LogEvent("❌ Node %s is offline.", ipAddress);
        }
    }
}

// Function to check if a node can communicate by attempting to connect to its IP address and port
int CanNodeCommunicate(const char* ipAddress, int port) {
    int sock;
    struct sockaddr_in serverAddr;
    
    // Create a socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 0; // Communication failed
    }

    // Set up the server address structure
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, ipAddress, &serverAddr.sin_addr) <= 0) {
        perror("Invalid address");
        close(sock);
        return 0; // Communication failed
    }

    // Set socket timeout
    struct timeval tv;
    tv.tv_sec = TIMEOUT;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);

    // Attempt to connect
    int result = connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    
    // Close the socket
    close(sock);

    // Check the result of the connection attempt
    if (result == 0) {
        return 1; // Node can communicate
    } else {
        return 0; // Node cannot communicate
    }
}

// Operation 2: Check if nodes can communicate
void OperationTwo(int nSporkID, int nValue) {
    const char* nodes[] = {
        "173.249.6.78:17646",
        "31.220.72.12:17646",
        "109.56.253.247:17646"
    };
    int nodeCount = sizeof(nodes) / sizeof(nodes[0]);

    LogEvent("📡 Checking if nodes can communicate...");

    for (int i = 0; i < nodeCount; i++) {
        const char* ipAddress = nodes[i];
        if (CanNodeCommunicate(ipAddress, PORT)) {
            LogEvent("✅ Node %s can communicate on port %d.", ipAddress, PORT);
        } else {
            LogEvent("❌ Node %s cannot communicate on port %d.", ipAddress, PORT);
        }
    }
}

void OperationThree(int nSporkID, int nValue) {
    LogEvent("📝 Logging events...");

    // Example log entries for demonstration
    char logMessage[256];

    snprintf(logMessage, sizeof(logMessage), "Spork ID: %d, Value: %d - Event started.", nSporkID, nValue);
    LogEventToFile(logMessage);

    // Simulate logging some events
    snprintf(logMessage, sizeof(logMessage), "Spork ID: %d - Processing data...", nSporkID);
    LogEventToFile(logMessage);

    snprintf(logMessage, sizeof(logMessage), "Spork ID: %d - Data processed successfully.", nSporkID);
    LogEventToFile(logMessage);

    snprintf(logMessage, sizeof(logMessage), "Spork ID: %d - Event completed.", nSporkID);
    LogEventToFile(logMessage);
}

// Function to reconnect to a node with retries
int ReconnectNode(const char* ipAddress, int port) {
    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        LogEvent("🔄 Attempting to reconnect to node %s (Attempt %d/%d)...", ipAddress, attempt, MAX_RETRIES);
        if (CanNodeCommunicate(ipAddress, port)) {
            LogEvent("✅ Successfully reconnected to node %s.", ipAddress);
            return 1; // Successfully reconnected
        }
        LogEvent("❌ Failed to reconnect to node %s.", ipAddress);
        sleep(1); // Wait before retrying
    }
    return 0; // Failed to reconnect after retries
}

// Operation 4: Connect the nodes
void OperationFour(int nSporkID, int nValue) {
    const char* nodes[] = {
        "173.249.6.78:17646",
        "31.220.72.12:17646",
        "109.56.253.247:17646"
    };
    int nodeCount = sizeof(nodes) / sizeof(nodes[0]);

    LogEvent("🔗 Connecting the nodes...");

    for (int i = 0; i < nodeCount; i++) {
        const char* ipAddress = nodes[i];
        if (CanNodeCommunicate(ipAddress, PORT)) {
            LogEvent("✅ Node %s is already connected.", ipAddress);
        } else {
            LogEvent("❌ Node %s is not connected. Attempting to reconnect...", ipAddress);
            if (!ReconnectNode(ipAddress, PORT)) {
                LogEvent("❌ Failed to reconnect to node %s after %d attempts.", ipAddress, MAX_RETRIES);
            }
        }
    }
}

// Function to convert an integer to an IP address string
void IntToIpAddress(int ipInt, char* ipAddress) {
    sprintf(ipAddress, "%d.%d.%d.%d", (ipInt >> 24) & 0xFF, (ipInt >> 16) & 0xFF, (ipInt >> 8) & 0xFF, ipInt & 0xFF);
}

// Operation 5: Check if new nodes have come online
void OperationFive(int nSporkID, int nValue) {
    const char* predefinedNodes[NODE_COUNT] = {
        "173.249.6.78:17646",
        "31.220.72.12:17646",
        "109.56.253.247:17646"
    };

    LogEvent("🔍 Checking if new nodes have come online...");

    // Check predefined nodes
    for (int i = 0; i < NODE_COUNT; i++) {
        const char* ipAddress = predefinedNodes[i];
        if (CanNodeCommunicate(ipAddress, PORT)) {
            LogEvent("✅ Predefined node %s is online.", ipAddress);
        } else {
            LogEvent("❌ Predefined node %s is offline.", ipAddress);
        }
    }

    // Check for new nodes in the specified range
    for (int ipInt = NEW_NODE_START; ipInt <= NEW_NODE_END; ipInt++) {
        char ipAddress[16];
        IntToIpAddress(ipInt, ipAddress);
        if (CanNodeCommunicate(ipAddress, PORT)) {
            LogEvent("✅ New node %s is online.", ipAddress);
        } else {
            LogEvent("❌ New node %s is offline.", ipAddress);
        }
    }
}

// Function to reset the daemon
void ResetDaemon() {
    LogEvent("🔄 Resetting the daemon due to connection issues.");

    // Command to stop the daemon
    int stopResult = system("sudo systemctl stop MasterNoder2d.service");
    if (stopResult != 0) {
        LogEvent("❌ Failed to stop the daemon. Please check the service status.");
        return;
    }

    // Command to start the daemon
    int startResult = system("sudo systemctl start MasterNoder2d.service");
    if (startResult != 0) {
        LogEvent("❌ Failed to start the daemon. Please check the service status.");
        return;
    }

    LogEvent("✅ Daemon has been successfully reset.");
}

// Function to send status messages to nodes
void SendStatusMessageToNodes(const std::string& message) {
    // Placeholder for sending message to nodes
    LogEvent("Sending message to nodes: " + message);
}

// Function to check the health of a master node (stub implementation)
bool CheckMasterNodeHealth(const std::string& node) {
    // Placeholder logic to check node health
    // In a real implementation, this would involve actual checks (e.g., ping, status request)
    return true; // Assuming the node is healthy for this example
}

// Function to assess master node health
void AssessMasterNodeHealth() {
    LogEvent("🛠️ Assessing master node health...");

    // List of master nodes (replace with actual node identifiers)
    std::vector<std::string> masterNodes = {"Node1", "Node2", "Node3"};
    bool allHealthy = true;

    // Assess the status of each master node
    for (const auto& node : masterNodes) {
        if (CheckMasterNodeHealth(node)) {
            LogEvent(node + " is healthy.");
        } else {
            LogEvent(node + " is unhealthy.");
            allHealthy = false;
        }
    }

    // Send status message based on assessment
    if (allHealthy) {
        SendStatusMessageToNodes("Status: OK");
    } else {
        SendStatusMessageToNodes("Status: WARNING - Some nodes are unhealthy.");
    }
}

// Function to log events to a file
void LogEventToFile(const char* message) {
    FILE* logFile = fopen("debug.log", "a"); // Open the log file in append mode
    if (logFile == NULL) {
        perror("Failed to open log file");
        return;
    }

    // Get the current time
    time_t now = time(NULL);
    char* timeString = ctime(&now);
    timeString[strlen(timeString) - 1] = '\0'; // Remove the newline character

    // Write the timestamp and message to the log file
    fprintf(logFile, "[%s] %s\n", timeString, message);
    
    fclose(logFile); // Close the log file
}

// Function to log events
void LogEvent(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

// Placeholder function to get connected node count
int GetConnectedNodeCount() {
    // Logic to return the count of connected nodes
    return connectedNodes.size();
}

// Placeholder function to attempt to connect to more nodes
bool AttemptToConnectToMoreNodes() {
     std::string newNode = "Node" + std::to_string(connectedNodes.size() + 1);
    connectedNodes.push_back(newNode);
    LogEvent("✅ Connected to " + newNode);
    // Logic to attempt connecting to more nodes
    return true; // Example return value
}

// Example usage of the functions
void ExampleUsage() {
    LogEvent("Initial connected node count: " + std::to_string(GetConnectedNodeCount()));

    // Attempt to connect to more nodes
    if (AttemptToConnectToMoreNodes()) {
        LogEvent("Current connected node count: " + std::to_string(GetConnectedNodeCount()));
    }
}

int main() {
    ExampleUsage(); // Run example usage
    return 0;
}

// Placeholder function to enable error debugging
void EnableErrorDebugging() {
    LogEvent("🔍 Error debugging enabled.");
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
