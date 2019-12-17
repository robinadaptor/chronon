// Copyright (c) 2014-2015 Dash developers
// Copyright (c) 2015-2018 PIVX developers
// Copyright (c) 2018-2019 Chronon developers

// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activemasternode.h"
#include "db.h"
#include "init.h"
#include "main.h"
#include "masternode-budget.h"
#include "masternode-payments.h"
#include "masternodeconfig.h"
#include "masternodeman.h"
#include "rpc/server.h"
#include "utilmoneystr.h"

#include <univalue.h>

#include <boost/tokenizer.hpp>
#include <fstream>

UniValue getpoolinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
            "getpoolinfo\n"
            "\nReturns anonymous pool-related information\n"

            "\nResult:\n"
            "{\n"
            "  \"current\": \"addr\",    (string) Chronon address of current masternode\n"
            "  \"state\": xxxx,        (string) unknown\n"
            "  \"entries\": xxxx,      (numeric) Number of entries\n"
            "  \"accepted\": xxxx,     (numeric) Number of entries accepted\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("getpoolinfo", "") + HelpExampleRpc("getpoolinfo", ""));

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("current_masternode", mnodeman.GetCurrentMasterNode()->addr.ToString()));
    obj.push_back(Pair("state", obfuScationPool.GetState()));
    obj.push_back(Pair("entries", obfuScationPool.GetEntriesCount()));
    obj.push_back(Pair("entries_accepted", obfuScationPool.GetCountEntriesAccepted()));
    return obj;
}

// This command is retained for backwards compatibility, but is depreciated.
// Future removal of this command is planned to keep things clean.
UniValue masternode(const UniValue& params, bool fHelp)
{
    std::string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();

    if (fHelp ||
        (strCommand != "start" && strCommand != "start-alias" && strCommand != "start-many" && strCommand != "start-all" && strCommand != "start-missing" &&
            strCommand != "start-disabled" && strCommand != "list" && strCommand != "list-conf" && strCommand != "count" && strCommand != "enforce" &&
            strCommand != "debug" && strCommand != "current" && strCommand != "winners" && strCommand != "genkey" && strCommand != "connect" &&
            strCommand != "outputs" && strCommand != "status" && strCommand != "calcscore"))
        throw std::runtime_error(
            "masternode \"command\"...\n"
            "\nSet of commands to execute masternode related actions\n"
            "This command is depreciated, please see individual command documentation for future reference\n\n"

            "\nArguments:\n"
            "1. \"command\"        (string or set of strings, required) The command to execute\n"

            "\nAvailable commands:\n"
            "  count        - Print count information of all known masternodes\n"
            "  current      - Print info on current masternode winner\n"
            "  debug        - Print masternode status\n"
            "  genkey       - Generate new masternodeprivkey\n"
            "  outputs      - Print masternode compatible outputs\n"
            "  start        - Start masternode configured in chronon.conf\n"
            "  start-alias  - Start single masternode by assigned alias configured in masternode.conf\n"
            "  start-<mode> - Start masternodes configured in masternode.conf (<mode>: 'all', 'missing', 'disabled')\n"
            "  status       - Print masternode status information\n"
            "  list         - Print list of all known masternodes (see masternodelist for more info)\n"
            "  list-conf    - Print masternode.conf in JSON format\n"
            "  winners      - Print list of masternode winners\n");

    if (strCommand == "list") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return listmasternodes(newParams, fHelp);
    }

    if (strCommand == "connect") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return masternodeconnect(newParams, fHelp);
    }

    if (strCommand == "count") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return getmasternodecount(newParams, fHelp);
    }

    if (strCommand == "current") {
        CMasternode* winner = mnodeman.GetCurrentMasterNode(1);
        if (winner) {
            UniValue obj(UniValue::VOBJ);

            obj.push_back(Pair("ip:port", winner->addr.ToString()));
            obj.push_back(Pair("protocol", (int64_t)winner->protocolVersion));
            obj.push_back(Pair("txhash", winner->vin.prevout.hash.ToString()));
            obj.push_back(Pair("pubkey", CBitcoinAddress(winner->pubKeyCollateralAddress.GetID()).ToString()));
            obj.push_back(Pair("lastseen", (winner->lastPing == CMasternodePing()) ? winner->sigTime : (int64_t)winner->lastPing.sigTime));
            obj.push_back(Pair("activeseconds", (winner->lastPing == CMasternodePing()) ? 0 : (int64_t)(winner->lastPing.sigTime - winner->sigTime)));
            return obj;
        }

        return "unknown";
    }

    if (strCommand == "debug") {
        if (activeMasternode.status != ACTIVE_MASTERNODE_INITIAL || !masternodeSync.IsSynced())
            return activeMasternode.GetStatus();

        CTxIn vin = CTxIn();
        CPubKey pubkey;
        CKey key;
        bool found = activeMasternode.GetMasterNodeVin(vin, pubkey, key);
        if (!found) {
            throw std::runtime_error("Missing masternode input, please look at the documentation for instructions on masternode creation\n");
        } else {
            return activeMasternode.GetStatus();
        }
    }

    if (strCommand == "enforce") {
        return (uint64_t)enforceMasternodePaymentsTime;
    }

    if (strCommand == "start") {
        if (!fMasterNode) throw std::runtime_error("you must set masternode=1 in the configuration\n");

        if (pwalletMain->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (params.size() == 2) {
                strWalletPass = params[1].get_str().c_str();
            } else {
                throw std::runtime_error("Your wallet is locked, passphrase is required\n");
            }

            if (!pwalletMain->Unlock(strWalletPass)) {
                throw std::runtime_error("incorrect passphrase\n");
            }
        }

        if (activeMasternode.status != ACTIVE_MASTERNODE_STARTED) {
            activeMasternode.status = ACTIVE_MASTERNODE_INITIAL; // TODO: consider better way
            activeMasternode.ManageStatus();
            pwalletMain->Lock();
        }

        return activeMasternode.GetStatus();
    }

    if (strCommand == "start-alias") {
        if (params.size() < 2) {
            throw std::runtime_error("command needs at least 2 parameters\n");
        }

        std::string alias = params[1].get_str();

        if (pwalletMain->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (params.size() == 3) {
                strWalletPass = params[2].get_str().c_str();
            } else {
                throw std::runtime_error("Your wallet is locked, passphrase is required\n");
            }

            if (!pwalletMain->Unlock(strWalletPass)) {
                throw std::runtime_error("incorrect passphrase\n");
            }
        }

        bool found = false;

		UniValue statusObj(UniValue::VOBJ);
        statusObj.push_back(Pair("alias", alias));

        for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
            if (mne.getAlias() == alias) {
                found = true;
                std::string errorMessage;
            	CMasternodeBroadcast mnb;				

                bool result = activeMasternode.CreateBroadcast(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), errorMessage, mnb);

                statusObj.push_back(Pair("result", result ? "successful" : "failed"));
                if (!result) {
                    statusObj.push_back(Pair("errorMessage", errorMessage));
                }
                break;
            }
        }

        if (!found) {
            statusObj.push_back(Pair("result", "failed"));
            statusObj.push_back(Pair("errorMessage", "could not find alias in config. Verify with list-conf."));
        }

        pwalletMain->Lock();
        return statusObj;
    }

    if (strCommand == "start-many" || strCommand == "start-all" || strCommand == "start-missing" || strCommand == "start-disabled") {
        if (pwalletMain->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (params.size() == 2) {
                strWalletPass = params[1].get_str().c_str();
            } else {
                throw std::runtime_error("Your wallet is locked, passphrase is required\n");
            }

            if (!pwalletMain->Unlock(strWalletPass)) {
                throw std::runtime_error("incorrect passphrase\n");
            }
        }

        if ((strCommand == "start-missing" || strCommand == "start-disabled") &&
            (masternodeSync.RequestedMasternodeAssets <= MASTERNODE_SYNC_LIST ||
                masternodeSync.RequestedMasternodeAssets == MASTERNODE_SYNC_FAILED)) {
            throw std::runtime_error("You can't use this command until masternode list is synced\n");
        }

        std::vector<CMasternodeConfig::CMasternodeEntry> mnEntries;
        mnEntries = masternodeConfig.getEntries();

        int successful = 0;
        int failed = 0;

		UniValue resultsObj(UniValue::VOBJ);
		
        for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
            std::string errorMessage;
            int nIndex;
            if(!mne.castOutputIndex(nIndex))
                continue;
            CTxIn vin = CTxIn(uint256(mne.getTxHash()), uint32_t(nIndex));
            CMasternode* pmn = mnodeman.Find(vin);

            if (strCommand == "start-missing" && pmn) continue;
            if (strCommand == "start-disabled" && pmn && pmn->IsEnabled()) continue;
          	CMasternodeBroadcast mnb;			

            bool result = activeMasternode.CreateBroadcast(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), errorMessage, mnb);

			UniValue statusObj(UniValue::VOBJ);			
            statusObj.push_back(Pair("alias", mne.getAlias()));
            statusObj.push_back(Pair("result", result ? "successful" : "failed"));

            if (result) {
                successful++;
            } else {
                failed++;
                statusObj.push_back(Pair("errorMessage", errorMessage));
            }

            resultsObj.push_back(Pair("status", statusObj));
        }
        pwalletMain->Lock();

		UniValue returnObj(UniValue::VOBJ);			
        returnObj.push_back(Pair("overall", strprintf("Successfully started %d masternodes, failed to start %d, total %d", successful, failed, successful + failed)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;
    }

    if (strCommand == "create") {
        throw std::runtime_error("Not implemented yet, please look at the documentation for instructions on masternode creation\n");
    }
    if (strCommand == "genkey") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return createmasternodekey(newParams, fHelp);
    }

    if (strCommand == "list-conf") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return listmasternodeconf(newParams, fHelp);
    }

    if (strCommand == "outputs") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return getmasternodeoutputs(newParams, fHelp);
    }

    if (strCommand == "status") {
        if (!fMasterNode) throw std::runtime_error("This is not a masternode\n");

		UniValue mnObj(UniValue::VOBJ);			
        CMasternode* pmn = mnodeman.Find(activeMasternode.vin);

        mnObj.push_back(Pair("vin", activeMasternode.vin.ToString()));
        mnObj.push_back(Pair("service", activeMasternode.service.ToString()));
        if (pmn) mnObj.push_back(Pair("pubkey", CBitcoinAddress(pmn->pubKeyCollateralAddress.GetID()).ToString()));
        mnObj.push_back(Pair("status", activeMasternode.GetStatus()));
        return mnObj;
    }

    if (strCommand == "winners") {
        int nLast = 10;

        if (params.size() >= 2) {
            try {
                nLast = std::stoi(params[1].get_str());
            } catch (const std::exception& e) {
                throw std::runtime_error("Exception on param 2");
            }
        }

		UniValue obj(UniValue::VOBJ);			

        for (int nHeight = chainActive.Tip()->nHeight - nLast; nHeight < chainActive.Tip()->nHeight + 20; nHeight++) {
            obj.push_back(Pair(strprintf("%d", nHeight), GetRequiredPaymentsString(nHeight)));
        }

        return obj;
    }

    if (strCommand == "calcscore") {
        UniValue newParams(UniValue::VARR);
        // forward params but skip command
        for (unsigned int i = 1; i < params.size(); i++) {
            newParams.push_back(params[i]);
        }
        return getmasternodescores(newParams, fHelp);
    }

    return NullUniValue;
}

UniValue masternodelist(const UniValue& params, bool fHelp)
{
    std::string strMode = "status";
    std::string strFilter = "";

    if (params.size() >= 1) strMode = params[0].get_str();
    if (params.size() == 2) strFilter = params[1].get_str();

    if (fHelp ||
        (strMode != "status" && strMode != "vin" && strMode != "pubkey" && strMode != "lastseen" && strMode != "activeseconds" && strMode != "rank" && strMode != "addr" && strMode != "protocol" && strMode != "full" && strMode != "lastpaid")) {
        throw std::runtime_error(
            "masternodelist ( \"mode\" \"filter\" )\n"
            "Get a list of masternodes in different modes\n"
            "\nArguments:\n"
            "1. \"mode\"      (string, optional/required to use filter, defaults = status) The mode to run list in\n"
            "2. \"filter\"    (string, optional) Filter results. Partial match by IP by default in all modes,\n"
            "                                    additional matches in some modes are also available\n"
            "\nAvailable modes:\n"
            "  activeseconds  - Print number of seconds masternode recognized by the network as enabled\n"
            "                   (since latest issued \"masternode start/start-many/start-alias\")\n"
            "  addr           - Print ip address associated with a masternode (can be additionally filtered, partial match)\n"
            "  full           - Print info in format 'status protocol pubkey IP lastseen activeseconds lastpaid'\n"
            "                   (can be additionally filtered, partial match)\n"
            "  lastseen       - Print timestamp of when a masternode was last seen on the network\n"
            "  lastpaid       - The last time a node was paid on the network\n"
            "  protocol       - Print protocol of a masternode (can be additionally filtered, exact match))\n"
            "  pubkey         - Print public key associated with a masternode (can be additionally filtered,\n"
            "                   partial match)\n"
            "  rank           - Print rank of a masternode based on current block\n"
            "  status         - Print masternode status: ENABLED / EXPIRED / VIN_SPENT / REMOVE / POS_ERROR\n"
            "                   (can be additionally filtered, partial match)\n");
    }

    UniValue obj(UniValue::VOBJ);
    if (strMode == "rank") {
        std::vector<std::pair<int, CMasternode> > vMasternodeRanks = mnodeman.GetMasternodeRanks(chainActive.Tip()->nHeight);
        for (PAIRTYPE(int, CMasternode) & s: vMasternodeRanks) {
            std::string strVin = s.second.vin.prevout.ToStringShort();
            if (strFilter != "" && strVin.find(strFilter) == std::string::npos) continue;
            obj.push_back(Pair(strVin, s.first));
        }
    } else {
        std::vector<CMasternode> vMasternodes = mnodeman.GetFullMasternodeVector();
        for (CMasternode& mn: vMasternodes) {
            std::string strVin = mn.vin.prevout.ToStringShort();
            if (strMode == "activeseconds") {
                if (strFilter != "" && strVin.find(strFilter) == std::string::npos) continue;
                obj.push_back(Pair(strVin, (int64_t)(mn.lastPing.sigTime - mn.sigTime)));
            } else if (strMode == "addr") {
                if (strFilter != "" && mn.vin.prevout.hash.ToString().find(strFilter) == std::string::npos &&
                    strVin.find(strFilter) == std::string::npos) continue;
                obj.push_back(Pair(strVin, mn.addr.ToString()));
            } else if (strMode == "full") {
                std::ostringstream addrStream;
                addrStream << std::setw(21) << strVin;

                std::ostringstream stringStream;
                stringStream << std::setw(9) << mn.Status() << " " << mn.protocolVersion << " " << CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString() << " " << std::setw(21) << mn.addr.ToString() << " " << (int64_t)mn.lastPing.sigTime << " " << std::setw(8) << (int64_t)(mn.lastPing.sigTime - mn.sigTime) << " " << (int64_t)mn.GetLastPaid();
                std::string output = stringStream.str();
                stringStream << " " << strVin;
                if (strFilter != "" && stringStream.str().find(strFilter) == std::string::npos &&
                    strVin.find(strFilter) == std::string::npos) continue;
                obj.push_back(Pair(addrStream.str(), output));
            } else if (strMode == "lastseen") {
                if (strFilter != "" && strVin.find(strFilter) == std::string::npos) continue;
                obj.push_back(Pair(strVin, (int64_t)mn.lastPing.sigTime));
            } else if (strMode == "lastpaid") {
                if (strFilter != "" && mn.vin.prevout.hash.ToString().find(strFilter) == std::string::npos &&
                    strVin.find(strFilter) == std::string::npos) continue;
                obj.push_back(Pair(strVin, (int64_t)mn.GetLastPaid()));
            } else if (strMode == "protocol") {
                if (strFilter != "" && strFilter != strprintf("%d", mn.protocolVersion) &&
                    strVin.find(strFilter) == std::string::npos) continue;
                obj.push_back(Pair(strVin, (int64_t)mn.protocolVersion));
            } else if (strMode == "pubkey") {
                CBitcoinAddress address(mn.pubKeyCollateralAddress.GetID());

                if (strFilter != "" && address.ToString().find(strFilter) == std::string::npos &&
                    strVin.find(strFilter) == std::string::npos) continue;
                obj.push_back(Pair(strVin, address.ToString()));
            } else if (strMode == "status") {
                std::string strStatus = mn.Status();
                if (strFilter != "" && strVin.find(strFilter) == std::string::npos && strStatus.find(strFilter) == std::string::npos) continue;
                obj.push_back(Pair(strVin, strStatus));
            }
        }
    }
    return obj;
}

UniValue listmasternodes(const UniValue& params, bool fHelp)
{
    std::string strFilter = "";

    if (params.size() == 1) strFilter = params[0].get_str();

    if (fHelp || (params.size() > 1))
        throw std::runtime_error(
            "listmasternodes ( \"filter\" )\n"
            "\nGet a ranked list of masternodes\n"

            "\nArguments:\n"
            "1. \"filter\"    (string, optional) Filter search text. Partial match by txhash, status, or addr.\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"rank\": n,           (numeric) Masternode Rank (or 0 if not enabled)\n"
            "    \"txhash\": \"hash\",    (string) Collateral transaction hash\n"
            "    \"outidx\": n,         (numeric) Collateral transaction output index\n"
            "    \"pubkey\": \"key\",   (string) Masternode public key used for message broadcasting\n"
            "    \"status\": s,         (string) Status (ENABLED/EXPIRED/REMOVE/etc)\n"
            "    \"addr\": \"addr\",      (string) Masternode Chronon address\n"
            "    \"version\": v,        (numeric) Masternode protocol version\n"
            "    \"lastseen\": ttt,     (numeric) The time in seconds since epoch (Jan 1 1970 GMT) of the last seen\n"
            "    \"activetime\": ttt,   (numeric) The time in seconds since epoch (Jan 1 1970 GMT) masternode has been active\n"
            "    \"lastpaid\": ttt,     (numeric) The time in seconds since epoch (Jan 1 1970 GMT) masternode was last paid\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("listmasternodes", "") + HelpExampleRpc("listmasternodes", ""));

    UniValue ret(UniValue::VARR);
    int nHeight;
    {
        LOCK(cs_main);
        CBlockIndex* pindex = chainActive.Tip();
        if(!pindex) return 0;
        nHeight = pindex->nHeight;
    }
    std::vector<std::pair<int, CMasternode> > vMasternodeRanks = mnodeman.GetMasternodeRanks(nHeight);
    for (PAIRTYPE(int, CMasternode) & s : vMasternodeRanks) {
        UniValue obj(UniValue::VOBJ);
        std::string strVin = s.second.vin.prevout.ToStringShort();
        std::string strTxHash = s.second.vin.prevout.hash.ToString();
        uint32_t oIdx = s.second.vin.prevout.n;

        CMasternode* mn = mnodeman.Find(s.second.vin);

        if (mn != NULL) {
            if (strFilter != "" && strTxHash.find(strFilter) == std::string::npos &&
                mn->Status().find(strFilter) == std::string::npos &&
                CBitcoinAddress(mn->pubKeyCollateralAddress.GetID()).ToString().find(strFilter) == std::string::npos) continue;

            std::string strStatus = mn->Status();
            std::string strHost;
            int port;
            SplitHostPort(mn->addr.ToString(), port, strHost);
            CNetAddr node = CNetAddr(strHost, false);
            std::string strNetwork = GetNetworkName(node.GetNetwork());

            obj.push_back(Pair("rank", (strStatus == "ENABLED" ? s.first : 0)));
            obj.push_back(Pair("network", strNetwork));
            obj.push_back(Pair("txhash", strTxHash));
            obj.push_back(Pair("outidx", (uint64_t)oIdx));
            obj.push_back(Pair("pubkey", HexStr(mn->pubKeyMasternode)));
            obj.push_back(Pair("status", strStatus));
            obj.push_back(Pair("addr", CBitcoinAddress(mn->pubKeyCollateralAddress.GetID()).ToString()));
            obj.push_back(Pair("version", mn->protocolVersion));
            obj.push_back(Pair("lastseen", (int64_t)mn->lastPing.sigTime));
            obj.push_back(Pair("activetime", (int64_t)(mn->lastPing.sigTime - mn->sigTime)));
            obj.push_back(Pair("lastpaid", (int64_t)mn->GetLastPaid()));

            ret.push_back(obj);
        }
    }

    return ret;
}

UniValue masternodeconnect(const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() != 1))
        throw std::runtime_error(
            "masternodeconnect \"address\"\n"
            "\nAttempts to connect to specified masternode address\n"

            "\nArguments:\n"
            "1. \"address\"     (string, required) IP or net address to connect to\n"

            "\nExamples:\n" +
            HelpExampleCli("masternodeconnect", "\"192.168.0.6:15288\"") + HelpExampleRpc("masternodeconnect", "\"192.168.0.6:15288\""));

    std::string strAddress = params[0].get_str();

    CService addr = CService(strAddress);

    CNode* pnode = ConnectNode((CAddress)addr, NULL, false);
    if (pnode) {
        pnode->Release();
        return NullUniValue;
    } else {
        throw std::runtime_error("error connecting\n");
    }
}

UniValue getmasternodecount (const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() > 0))
        throw std::runtime_error(
            "getmasternodecount\n"
            "\nGet masternode count values\n"

            "\nResult:\n"
            "{\n"
            "  \"total\": n,        (numeric) Total masternodes\n"
            "  \"stable\": n,       (numeric) Stable count\n"
            "  \"obfcompat\": n,    (numeric) Obfuscation Compatible\n"
            "  \"enabled\": n,      (numeric) Enabled masternodes\n"
            "  \"inqueue\": n       (numeric) Masternodes in queue\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("getmasternodecount", "") + HelpExampleRpc("getmasternodecount", ""));

    UniValue obj(UniValue::VOBJ);
    int nCount = 0;
    int ipv4 = 0, ipv6 = 0, onion = 0;

    if (chainActive.Tip())
        mnodeman.GetNextMasternodeInQueueForPayment(chainActive.Tip()->nHeight, true, nCount);

    mnodeman.CountNetworks(ActiveProtocol(), ipv4, ipv6, onion);

    obj.push_back(Pair("total", mnodeman.size()));
    obj.push_back(Pair("stable", mnodeman.stable_size()));
    obj.push_back(Pair("obfcompat", mnodeman.CountEnabled(ActiveProtocol())));
    obj.push_back(Pair("enabled", mnodeman.CountEnabled()));
    obj.push_back(Pair("inqueue", nCount));
    obj.push_back(Pair("ipv4", ipv4));
    obj.push_back(Pair("ipv6", ipv6));
    obj.push_back(Pair("onion", onion));

    return obj;
}

UniValue masternodecurrent (const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() != 0))
        throw std::runtime_error(
            "masternodecurrent\n"
            "\nGet current masternode winner\n"

            "\nResult:\n"
            "{\n"
            "  \"protocol\": xxxx,        (numeric) Protocol version\n"
            "  \"txhash\": \"xxxx\",      (string) Collateral transaction hash\n"
            "  \"pubkey\": \"xxxx\",      (string) MN Public key\n"
            "  \"lastseen\": xxx,       (numeric) Time since epoch of last seen\n"
            "  \"activeseconds\": xxx,  (numeric) Seconds MN has been active\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("masternodecurrent", "") + HelpExampleRpc("masternodecurrent", ""));

    CMasternode* winner = mnodeman.GetCurrentMasterNode(1);
    if (winner) {
        UniValue obj(UniValue::VOBJ);

        obj.push_back(Pair("protocol", (int64_t)winner->protocolVersion));
        obj.push_back(Pair("txhash", winner->vin.prevout.hash.ToString()));
        obj.push_back(Pair("pubkey", CBitcoinAddress(winner->pubKeyCollateralAddress.GetID()).ToString()));
        obj.push_back(Pair("lastseen", (winner->lastPing == CMasternodePing()) ? winner->sigTime : (int64_t)winner->lastPing.sigTime));
        obj.push_back(Pair("activeseconds", (winner->lastPing == CMasternodePing()) ? 0 : (int64_t)(winner->lastPing.sigTime - winner->sigTime)));
        return obj;
    }

    throw std::runtime_error("unknown");
}

UniValue masternodedebug (const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() != 0))
        throw std::runtime_error(
            "masternodedebug\n"
            "\nPrint masternode status\n"

            "\nResult:\n"
            "\"status\"     (string) Masternode status message\n"

            "\nExamples:\n" +
            HelpExampleCli("masternodedebug", "") + HelpExampleRpc("masternodedebug", ""));

    if (activeMasternode.status != ACTIVE_MASTERNODE_INITIAL || !masternodeSync.IsSynced())
        return activeMasternode.GetStatus();

    CTxIn vin = CTxIn();
    CPubKey pubkey;
    CKey key;
    if (!activeMasternode.GetMasterNodeVin(vin, pubkey, key))
        throw std::runtime_error("Missing masternode input, please look at the documentation for instructions on masternode creation\n");
    else
        return activeMasternode.GetStatus();
}

UniValue startmasternode (const UniValue& params, bool fHelp)
{
    std::string strCommand;
    if (params.size() >= 1) {
        strCommand = params[0].get_str();

        // Backwards compatibility with legacy 'masternode' super-command forwarder
        if (strCommand == "start") strCommand = "local";
        if (strCommand == "start-alias") strCommand = "alias";
        if (strCommand == "start-all") strCommand = "all";
        if (strCommand == "start-many") strCommand = "many";
        if (strCommand == "start-missing") strCommand = "missing";
        if (strCommand == "start-disabled") strCommand = "disabled";
    }

    if (fHelp || params.size() < 2 || params.size() > 3 ||
        (params.size() == 2 && (strCommand != "local" && strCommand != "all" && strCommand != "many" && strCommand != "missing" && strCommand != "disabled")) ||
        (params.size() == 3 && strCommand != "alias"))
        throw std::runtime_error(
            "startmasternode \"local|all|many|missing|disabled|alias\" lockwallet ( \"alias\" )\n"
            "\nAttempts to start one or more masternode(s)\n"

            "\nArguments:\n"
            "1. set         (string, required) Specify which set of masternode(s) to start.\n"
            "2. lockwallet  (boolean, required) Lock wallet after completion.\n"
            "3. alias       (string) Masternode alias. Required if using 'alias' as the set.\n"

            "\nResult: (for 'local' set):\n"
            "\"status\"     (string) Masternode status message\n"

            "\nResult: (for other sets):\n"
            "{\n"
            "  \"overall\": \"xxxx\",     (string) Overall status message\n"
            "  \"detail\": [\n"
            "    {\n"
            "      \"node\": \"xxxx\",    (string) Node name or alias\n"
            "      \"result\": \"xxxx\",  (string) 'success' or 'failed'\n"
            "      \"error\": \"xxxx\"    (string) Error message, if failed\n"
            "    }\n"
            "    ,...\n"
            "  ]\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("startmasternode", "\"alias\" \"0\" \"my_mn\"") + HelpExampleRpc("startmasternode", "\"alias\" \"0\" \"my_mn\""));

    bool fLock = (params[1].get_str() == "true" ? true : false);

    EnsureWalletIsUnlocked();

    if (strCommand == "local") {
        if (!fMasterNode) throw std::runtime_error("you must set masternode=1 in the configuration\n");

        if (activeMasternode.status != ACTIVE_MASTERNODE_STARTED) {
            activeMasternode.status = ACTIVE_MASTERNODE_INITIAL; // TODO: consider better way
            activeMasternode.ManageStatus();
            if (fLock)
                pwalletMain->Lock();
        }

        return activeMasternode.GetStatus();
    }

    if (strCommand == "all" || strCommand == "many" || strCommand == "missing" || strCommand == "disabled") {
        if ((strCommand == "missing" || strCommand == "disabled") &&
            (masternodeSync.RequestedMasternodeAssets <= MASTERNODE_SYNC_LIST ||
                masternodeSync.RequestedMasternodeAssets == MASTERNODE_SYNC_FAILED)) {
            throw std::runtime_error("You can't use this command until masternode list is synced\n");
        }

        std::vector<CMasternodeConfig::CMasternodeEntry> mnEntries;
        mnEntries = masternodeConfig.getEntries();

        int successful = 0;
        int failed = 0;

        UniValue resultsObj(UniValue::VARR);

        for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
            std::string errorMessage;
            int nIndex;
            if(!mne.castOutputIndex(nIndex))
                continue;
            CTxIn vin = CTxIn(uint256(mne.getTxHash()), uint32_t(nIndex));
            CMasternode* pmn = mnodeman.Find(vin);
            CMasternodeBroadcast mnb;

            if (pmn != NULL) {
                if (strCommand == "missing") continue;
                if (strCommand == "disabled" && pmn->IsEnabled()) continue;
            }

            bool result = activeMasternode.CreateBroadcast(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), errorMessage, mnb);

            UniValue statusObj(UniValue::VOBJ);
            statusObj.push_back(Pair("alias", mne.getAlias()));
            statusObj.push_back(Pair("result", result ? "success" : "failed"));

            if (result) {
                successful++;
                statusObj.push_back(Pair("error", ""));
            } else {
                failed++;
                statusObj.push_back(Pair("error", errorMessage));
            }

            resultsObj.push_back(statusObj);
        }
        if (fLock)
            pwalletMain->Lock();

        UniValue returnObj(UniValue::VOBJ);
        returnObj.push_back(Pair("overall", strprintf("Successfully started %d masternodes, failed to start %d, total %d", successful, failed, successful + failed)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;
    }

    if (strCommand == "alias") {
        std::string alias = params[2].get_str();

        bool found = false;
        int successful = 0;
        int failed = 0;

        UniValue resultsObj(UniValue::VARR);
        UniValue statusObj(UniValue::VOBJ);
        statusObj.push_back(Pair("alias", alias));

        for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
            if (mne.getAlias() == alias) {
                found = true;
                std::string errorMessage;
                CMasternodeBroadcast mnb;

                bool result = activeMasternode.CreateBroadcast(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), errorMessage, mnb);

                statusObj.push_back(Pair("result", result ? "successful" : "failed"));

                if (result) {
                    successful++;
                    mnodeman.UpdateMasternodeList(mnb);
                    mnb.Relay();
                } else {
                    failed++;
                    statusObj.push_back(Pair("errorMessage", errorMessage));
                }
                break;
            }
        }

        if (!found) {
            failed++;
            statusObj.push_back(Pair("result", "failed"));
            statusObj.push_back(Pair("error", "could not find alias in config. Verify with list-conf."));
        }

        resultsObj.push_back(statusObj);

        if (fLock)
            pwalletMain->Lock();

        UniValue returnObj(UniValue::VOBJ);
        returnObj.push_back(Pair("overall", strprintf("Successfully started %d masternodes, failed to start %d, total %d", successful, failed, successful + failed)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;
    }
    return NullUniValue;
}

UniValue createmasternodekey (const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() != 0))
        throw std::runtime_error(
            "createmasternodekey\n"
            "\nCreate a new masternode private key\n"

            "\nResult:\n"
            "\"key\"    (string) Masternode private key\n"

            "\nExamples:\n" +
            HelpExampleCli("createmasternodekey", "") + HelpExampleRpc("createmasternodekey", ""));

    CKey secret;
    secret.MakeNewKey(false);

    return CBitcoinSecret(secret).ToString();
}

UniValue getmasternodeoutputs (const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() != 0))
        throw std::runtime_error(
            "getmasternodeoutputs\n"
            "\nPrint all masternode transaction outputs\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"txhash\": \"xxxx\",    (string) output transaction hash\n"
            "    \"outputidx\": n       (numeric) output index number\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("getmasternodeoutputs", "") + HelpExampleRpc("getmasternodeoutputs", ""));

    // Find possible candidates
    std::vector<COutput> possibleCoins = activeMasternode.SelectCoinsMasternode();

    UniValue ret(UniValue::VARR);
    for (COutput& out : possibleCoins) {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("txhash", out.tx->GetHash().ToString()));
        obj.push_back(Pair("outputidx", out.i));
        ret.push_back(obj);
    }

    return ret;
}

UniValue listmasternodeconf (const UniValue& params, bool fHelp)
{
    std::string strFilter = "";

    if (params.size() == 1) strFilter = params[0].get_str();

    if (fHelp || (params.size() > 1))
        throw std::runtime_error(
            "listmasternodeconf ( \"filter\" )\n"
            "\nPrint masternode.conf in JSON format\n"

            "\nArguments:\n"
            "1. \"filter\"    (string, optional) Filter search text. Partial match on alias, address, txHash, or status.\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"alias\": \"xxxx\",        (string) masternode alias\n"
            "    \"address\": \"xxxx\",      (string) masternode IP address\n"
            "    \"privateKey\": \"xxxx\",   (string) masternode private key\n"
            "    \"txHash\": \"xxxx\",       (string) transaction hash\n"
            "    \"outputIndex\": n,       (numeric) transaction output index\n"
            "    \"status\": \"xxxx\"        (string) masternode status\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("listmasternodeconf", "") + HelpExampleRpc("listmasternodeconf", ""));

    std::vector<CMasternodeConfig::CMasternodeEntry> mnEntries;
    mnEntries = masternodeConfig.getEntries();

    UniValue ret(UniValue::VARR);

    for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
        int nIndex;
        if(!mne.castOutputIndex(nIndex))
            continue;
        CTxIn vin = CTxIn(uint256(mne.getTxHash()), uint32_t(nIndex));
        CMasternode* pmn = mnodeman.Find(vin);

        std::string strStatus = pmn ? pmn->Status() : "MISSING";

        if (strFilter != "" && mne.getAlias().find(strFilter) == std::string::npos &&
            mne.getIp().find(strFilter) == std::string::npos &&
            mne.getTxHash().find(strFilter) == std::string::npos &&
            strStatus.find(strFilter) == std::string::npos) continue;

        UniValue mnObj(UniValue::VOBJ);
        mnObj.push_back(Pair("alias", mne.getAlias()));
        mnObj.push_back(Pair("address", mne.getIp()));
        mnObj.push_back(Pair("privateKey", mne.getPrivKey()));
        mnObj.push_back(Pair("txHash", mne.getTxHash()));
        mnObj.push_back(Pair("outputIndex", mne.getOutputIndex()));
        mnObj.push_back(Pair("status", strStatus));
        ret.push_back(mnObj);
    }

    return ret;
}

UniValue getmasternodestatus (const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() != 0))
        throw std::runtime_error(
            "getmasternodestatus\n"
            "\nPrint masternode status\n"

            "\nResult:\n"
            "{\n"
            "  \"txhash\": \"xxxx\",      (string) Collateral transaction hash\n"
            "  \"outputidx\": n,        (numeric) Collateral transaction output index number\n"
            "  \"netaddr\": \"xxxx\",     (string) Masternode network address\n"
            "  \"addr\": \"xxxx\",        (string) Chronon address for masternode payments\n"
            "  \"status\": \"xxxx\",      (string) Masternode status\n"
            "  \"message\": \"xxxx\"      (string) Masternode status message\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("getmasternodestatus", "") + HelpExampleRpc("getmasternodestatus", ""));

    if (!fMasterNode) throw std::runtime_error("This is not a masternode");

    CMasternode* pmn = mnodeman.Find(activeMasternode.vin);

    if (pmn) {
        UniValue mnObj(UniValue::VOBJ);
        mnObj.push_back(Pair("txhash", activeMasternode.vin.prevout.hash.ToString()));
        mnObj.push_back(Pair("outputidx", (uint64_t)activeMasternode.vin.prevout.n));
        mnObj.push_back(Pair("netaddr", activeMasternode.service.ToString()));
        mnObj.push_back(Pair("addr", CBitcoinAddress(pmn->pubKeyCollateralAddress.GetID()).ToString()));
        mnObj.push_back(Pair("status", activeMasternode.status));
        mnObj.push_back(Pair("message", activeMasternode.GetStatus()));
        return mnObj;
    }
    throw std::runtime_error("Masternode not found in the list of available masternodes. Current status: "
                        + activeMasternode.GetStatus());
}

UniValue getmasternodewinners (const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw std::runtime_error(
            "getmasternodewinners ( blocks \"filter\" )\n"
            "\nPrint the masternode winners for the last n blocks\n"

            "\nArguments:\n"
            "1. blocks      (numeric, optional) Number of previous blocks to show (default: 10)\n"
            "2. filter      (string, optional) Search filter matching MN address\n"

            "\nResult (single winner):\n"
            "[\n"
            "  {\n"
            "    \"nHeight\": n,           (numeric) block height\n"
            "    \"winner\": {\n"
            "      \"address\": \"xxxx\",    (string) Chronon MN Address\n"
            "      \"nVotes\": n,          (numeric) Number of votes for winner\n"
            "    }\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nResult (multiple winners):\n"
            "[\n"
            "  {\n"
            "    \"nHeight\": n,           (numeric) block height\n"
            "    \"winner\": [\n"
            "      {\n"
            "        \"address\": \"xxxx\",  (string) Chronon MN Address\n"
            "        \"nVotes\": n,        (numeric) Number of votes for winner\n"
            "      }\n"
            "      ,...\n"
            "    ]\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("getmasternodewinners", "") + HelpExampleRpc("getmasternodewinners", ""));

    int nHeight;
    {
        LOCK(cs_main);
        CBlockIndex* pindex = chainActive.Tip();
        if(!pindex) return 0;
        nHeight = pindex->nHeight;
    }

    int nLast = 10;
    std::string strFilter = "";

    if (params.size() >= 1)
        nLast = atoi(params[0].get_str());

    if (params.size() == 2)
        strFilter = params[1].get_str();

    UniValue ret(UniValue::VARR);

    for (int i = nHeight - nLast; i < nHeight + 20; i++) {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("nHeight", i));

        std::string strPayment = GetRequiredPaymentsString(i);
        if (strFilter != "" && strPayment.find(strFilter) == std::string::npos) continue;

        if (strPayment.find(',') != std::string::npos) {
            UniValue winner(UniValue::VARR);
            boost::char_separator<char> sep(",");
            boost::tokenizer< boost::char_separator<char> > tokens(strPayment, sep);
            for (const std::string& t : tokens) {
                UniValue addr(UniValue::VOBJ);
                std::size_t pos = t.find(":");
                std::string strAddress = t.substr(0,pos);
                uint64_t nVotes = atoi(t.substr(pos+1));
                addr.push_back(Pair("address", strAddress));
                addr.push_back(Pair("nVotes", nVotes));
                winner.push_back(addr);
            }
            obj.push_back(Pair("winner", winner));
        } else if (strPayment.find("Unknown") == std::string::npos) {
            UniValue winner(UniValue::VOBJ);
            std::size_t pos = strPayment.find(":");
            std::string strAddress = strPayment.substr(0,pos);
            uint64_t nVotes = atoi(strPayment.substr(pos+1));
            winner.push_back(Pair("address", strAddress));
            winner.push_back(Pair("nVotes", nVotes));
            obj.push_back(Pair("winner", winner));
        } else {
            UniValue winner(UniValue::VOBJ);
            winner.push_back(Pair("address", strPayment));
            winner.push_back(Pair("nVotes", 0));
            obj.push_back(Pair("winner", winner));
        }

            ret.push_back(obj);
    }

    return ret;
}

UniValue getmasternodescores (const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw std::runtime_error(
            "getmasternodescores ( blocks )\n"
            "\nPrint list of winning masternode by score\n"

            "\nArguments:\n"
            "1. blocks      (numeric, optional) Show the last n blocks (default 10)\n"

            "\nResult:\n"
            "{\n"
            "  xxxx: \"xxxx\"   (numeric : string) Block height : Masternode hash\n"
            "  ,...\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("getmasternodescores", "") + HelpExampleRpc("getmasternodescores", ""));

    int nLast = 10;

    if (params.size() == 1) {
        try {
            nLast = std::stoi(params[0].get_str());
        } catch (const std::invalid_argument&) {
            throw std::runtime_error("Exception on param 2");
        }
    }
    UniValue obj(UniValue::VOBJ);

    std::vector<CMasternode> vMasternodes = mnodeman.GetFullMasternodeVector();
    for (int nHeight = chainActive.Tip()->nHeight - nLast; nHeight < chainActive.Tip()->nHeight + 20; nHeight++) {
        uint256 nHigh = 0;
        CMasternode* pBestMasternode = NULL;
        for (CMasternode& mn : vMasternodes) {
            uint256 n = mn.CalculateScore(1, nHeight - 100);
            if (n > nHigh) {
                nHigh = n;
                pBestMasternode = &mn;
            }
        }
        if (pBestMasternode)
            obj.push_back(Pair(strprintf("%d", nHeight), pBestMasternode->vin.prevout.hash.ToString().c_str()));
    }

    return obj;
}

bool DecodeHexMnb(CMasternodeBroadcast& mnb, std::string strHexMnb) {

    if (!IsHex(strHexMnb))
        return false;

    std::vector<unsigned char> mnbData(ParseHex(strHexMnb));
    CDataStream ssData(mnbData, SER_NETWORK, PROTOCOL_VERSION);
    try {
        ssData >> mnb;
    }
    catch (const std::exception&) {
        return false;
    }

    return true;
}
UniValue createmasternodebroadcast(const UniValue& params, bool fHelp)
{
    std::string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();
    if (fHelp || (strCommand != "alias" && strCommand != "all") || (strCommand == "alias" && params.size() < 2))
        throw std::runtime_error(
            "createmasternodebroadcast \"command\" ( \"alias\")\n"
            "\nCreates a masternode broadcast message for one or all masternodes configured in masternode.conf\n" +
            HelpRequiringPassphrase() + "\n"

            "\nArguments:\n"
            "1. \"command\"      (string, required) \"alias\" for single masternode, \"all\" for all masternodes\n"
            "2. \"alias\"        (string, required if command is \"alias\") Alias of the masternode\n"

            "\nResult (all):\n"
            "{\n"
            "  \"overall\": \"xxx\",        (string) Overall status message indicating number of successes.\n"
            "  \"detail\": [                (array) JSON array of broadcast objects.\n"
            "    {\n"
            "      \"alias\": \"xxx\",      (string) Alias of the masternode.\n"
            "      \"success\": true|false, (boolean) Success status.\n"
            "      \"hex\": \"xxx\"         (string, if success=true) Hex encoded broadcast message.\n"
            "      \"error_message\": \"xxx\"   (string, if success=false) Error message, if any.\n"
            "    }\n"
            "    ,...\n"
            "  ]\n"
            "}\n"

            "\nResult (alias):\n"
            "{\n"
            "  \"alias\": \"xxx\",      (string) Alias of the masternode.\n"
            "  \"success\": true|false, (boolean) Success status.\n"
            "  \"hex\": \"xxx\"         (string, if success=true) Hex encoded broadcast message.\n"
            "  \"error_message\": \"xxx\"   (string, if success=false) Error message, if any.\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("createmasternodebroadcast", "alias mymn1") + HelpExampleRpc("createmasternodebroadcast", "alias mymn1"));

    EnsureWalletIsUnlocked();

    if (strCommand == "alias")
    {
        // wait for reindex and/or import to finish
        if (fImporting || fReindex)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Wait for reindex and/or import to finish");

        std::string alias = params[1].get_str();
        bool found = false;

        UniValue statusObj(UniValue::VOBJ);
        statusObj.push_back(Pair("alias", alias));

        for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
            if(mne.getAlias() == alias) {
                found = true;
                std::string errorMessage;
                CMasternodeBroadcast mnb;

                bool success = activeMasternode.CreateBroadcast(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), errorMessage, mnb, true);

                statusObj.push_back(Pair("success", success));
                if(success) {
                    CDataStream ssMnb(SER_NETWORK, PROTOCOL_VERSION);
                    ssMnb << mnb;
                    statusObj.push_back(Pair("hex", HexStr(ssMnb.begin(), ssMnb.end())));
                } else {
                    statusObj.push_back(Pair("error_message", errorMessage));
                }
                break;
            }
        }

        if(!found) {
            statusObj.push_back(Pair("success", false));
            statusObj.push_back(Pair("error_message", "Could not find alias in config. Verify with list-conf."));
        }

        return statusObj;

    }

    if (strCommand == "all")
    {
        // wait for reindex and/or import to finish
        if (fImporting || fReindex)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Wait for reindex and/or import to finish");

        std::vector<CMasternodeConfig::CMasternodeEntry> mnEntries;
        mnEntries = masternodeConfig.getEntries();

        int successful = 0;
        int failed = 0;

        UniValue resultsObj(UniValue::VARR);

        for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
            std::string errorMessage;

            CTxIn vin = CTxIn(uint256S(mne.getTxHash()), uint32_t(atoi(mne.getOutputIndex().c_str())));
            CMasternodeBroadcast mnb;

            bool success = activeMasternode.CreateBroadcast(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), errorMessage, mnb, true);

            UniValue statusObj(UniValue::VOBJ);
            statusObj.push_back(Pair("alias", mne.getAlias()));
            statusObj.push_back(Pair("success", success));

            if(success) {
                successful++;
                CDataStream ssMnb(SER_NETWORK, PROTOCOL_VERSION);
                ssMnb << mnb;
                statusObj.push_back(Pair("hex", HexStr(ssMnb.begin(), ssMnb.end())));
            } else {
                failed++;
                statusObj.push_back(Pair("error_message", errorMessage));
            }

            resultsObj.push_back(statusObj);
        }

        UniValue returnObj(UniValue::VOBJ);
        returnObj.push_back(Pair("overall", strprintf("Successfully created broadcast messages for %d masternodes, failed to create %d, total %d", successful, failed, successful + failed)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;
    }
    return NullUniValue;
}

UniValue decodemasternodebroadcast(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "decodemasternodebroadcast \"hexstring\"\n"
            "\nCommand to decode masternode broadcast messages\n"

            "\nArgument:\n"
            "1. \"hexstring\"        (string) The hex encoded masternode broadcast message\n"

            "\nResult:\n"
            "{\n"
            "  \"vin\": \"xxxx\"                (string) The unspent output which is holding the masternode collateral\n"
            "  \"addr\": \"xxxx\"               (string) IP address of the masternode\n"
            "  \"pubkeycollateral\": \"xxxx\"   (string) Collateral address's public key\n"
            "  \"pubkeymasternode\": \"xxxx\"   (string) Masternode's public key\n"
            "  \"vchsig\": \"xxxx\"             (string) Base64-encoded signature of this message (verifiable via pubkeycollateral)\n"
            "  \"sigtime\": \"nnn\"             (numeric) Signature timestamp\n"
            "  \"protocolversion\": \"nnn\"     (numeric) Masternode's protocol version\n"
            "  \"nlastdsq\": \"nnn\"            (numeric) The last time the masternode sent a DSQ message (for mixing) (DEPRECATED)\n"
            "  \"lastping\" : {                 (object) JSON object with information about the masternode's last ping\n"
            "      \"vin\": \"xxxx\"            (string) The unspent output of the masternode which is signing the message\n"
            "      \"blockhash\": \"xxxx\"      (string) Current chaintip blockhash minus 12\n"
            "      \"sigtime\": \"nnn\"         (numeric) Signature time for this ping\n"
            "      \"vchsig\": \"xxxx\"         (string) Base64-encoded signature of this ping (verifiable via pubkeymasternode)\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("decodemasternodebroadcast", "hexstring") + HelpExampleRpc("decodemasternodebroadcast", "hexstring"));

    CMasternodeBroadcast mnb;

    if (!DecodeHexMnb(mnb, params[0].get_str()))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Masternode broadcast message decode failed");

    if(!mnb.VerifySignature())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Masternode broadcast signature verification failed");

    UniValue resultObj(UniValue::VOBJ);

    resultObj.push_back(Pair("vin", mnb.vin.prevout.ToString()));
    resultObj.push_back(Pair("addr", mnb.addr.ToString()));
    resultObj.push_back(Pair("pubkeycollateral", CBitcoinAddress(mnb.pubKeyCollateralAddress.GetID()).ToString()));
    resultObj.push_back(Pair("pubkeymasternode", CBitcoinAddress(mnb.pubKeyMasternode.GetID()).ToString()));
    resultObj.push_back(Pair("vchsig", EncodeBase64(&mnb.sig[0], mnb.sig.size())));
    resultObj.push_back(Pair("sigtime", mnb.sigTime));
    resultObj.push_back(Pair("protocolversion", mnb.protocolVersion));
    resultObj.push_back(Pair("nlastdsq", mnb.nLastDsq));

    UniValue lastPingObj(UniValue::VOBJ);
    lastPingObj.push_back(Pair("vin", mnb.lastPing.vin.prevout.ToString()));
    lastPingObj.push_back(Pair("blockhash", mnb.lastPing.blockHash.ToString()));
    lastPingObj.push_back(Pair("sigtime", mnb.lastPing.sigTime));
    lastPingObj.push_back(Pair("vchsig", EncodeBase64(&mnb.lastPing.vchSig[0], mnb.lastPing.vchSig.size())));

    resultObj.push_back(Pair("lastping", lastPingObj));

    return resultObj;
}

UniValue relaymasternodebroadcast(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "relaymasternodebroadcast \"hexstring\"\n"
            "\nCommand to relay masternode broadcast messages\n"

            "\nArguments:\n"
            "1. \"hexstring\"        (string) The hex encoded masternode broadcast message\n"

            "\nExamples:\n" +
            HelpExampleCli("relaymasternodebroadcast", "hexstring") + HelpExampleRpc("relaymasternodebroadcast", "hexstring"));


    CMasternodeBroadcast mnb;

    if (!DecodeHexMnb(mnb, params[0].get_str()))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Masternode broadcast message decode failed");

    if(!mnb.VerifySignature())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Masternode broadcast signature verification failed");

    mnodeman.UpdateMasternodeList(mnb);
    mnb.Relay();

    return strprintf("Masternode broadcast sent (service %s, vin %s)", mnb.addr.ToString(), mnb.vin.ToString());
}
