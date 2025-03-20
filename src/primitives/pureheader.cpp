// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/pureheader.h"
#include "chainparams.h"
#include "crypto/scrypt.h"
#include "hash.h"
#include "streams.h"
#include "utilstrencodings.h"

void CPureBlockHeader::SetBaseVersion(int32_t nBaseVersion, int32_t nChainId)
{
    //assert(nBaseVersion >= 1 && nBaseVersion < VERSION_AUXPOW);
    //assert(!IsAuxpow());
    //nVersion = nBaseVersion | (nChainId * VERSION_CHAIN_START);
    // Just set the base version, ignore chain ID
    nVersion = nBaseVersion;
}

uint256 CPureBlockHeader::GetHash() const
{
    return SerializeHash(*this);
}

uint256 CPureBlockHeader::GetPoWHash() const
{
    //std::string network = Params().NetworkIDString();
    if (nTime > 1406160000 && nTime < 1721779200) { // July 24 2014 12:00:00 AM UTC, X11 fork - but let's go back to Scrypt for relaunch
        std::vector<unsigned char> vch(80);
        CVectorWriter ss(SER_NETWORK, PROTOCOL_VERSION, vch, 0);
        ss << *this;
        return Hash11((const char *)vch.data(), (const char *)vch.data() + vch.size());
    }
    else {
        uint256 thash;
        scrypt_1024_1_1_256(BEGIN(nVersion), BEGIN(thash));
        return thash;
    }
}
