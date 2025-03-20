// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"
#include "auxpow.h"
#include "arith_uint256.h"
#include "chain.h"
#include "primitives/block.h"
#include "uint256.h"
#include "util.h"

// Determine if the for the given block, a min difficulty setting applies
bool AllowMinDifficultyForBlock(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    // check if the chain allows minimum difficulty blocks
    if (!params.fPowAllowMinDifficultyBlocks)
        return false;

    // Smartcoin: Magic number at which reset protocol switches
    // check if we allow minimum difficulty at this block-height
    if (pindexLast->nHeight < 157500)
        return false;

    // Allow for a minimum block time if the elapsed time > 2*nTargetSpacing
    return (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2);
}

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting) return pindexLast->nBits;
    
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Genesis block
    if (pindexLast == NULL)
        return nProofOfWorkLimit;

    // Dogecoin: Special rules for minimum difficulty blocks with Digishield
    /** if (AllowDigishieldMinDifficultyForBlock(pindexLast, pblock, params))
    {
        // Special difficulty rule for testnet:
        // If the new block's timestamp is more than 2* nTargetSpacing minutes
        // then allow mining of a min-difficulty block.
        return nProofOfWorkLimit;
    } */

    if (pindexLast->nHeight+1 >= params.fork4Height)
        return DarkGravityWave(pindexLast, pblock, params);
    else if (pindexLast->nHeight+1 >= params.fork2Height+1) // DigiShield started at fork2Height+1
        return DigiShield(pindexLast, pblock, params);
    else if (pindexLast->nHeight+1 == params.fork2Height) // No retarget at block 200K due to a glitch
        return pindexLast->nBits;
    else if (pindexLast->nHeight+1 >= params.fork1Height)
        return KimotoGravityWell(pindexLast, pblock, params);

    if ((pindexLast->nHeight+1) % params.DifficultyAdjustmentInterval() != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // If the new block's timestamp is more than 1 hour then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing * 90)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Litecoin: This fixes an issue where a 51% attack can change difficulty at will.
    // Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
    int blockstogoback = params.DifficultyAdjustmentInterval();
    if ((pindexLast->nHeight+1) == params.DifficultyAdjustmentInterval())
        blockstogoback = params.DifficultyAdjustmentInterval() - 1;
    else if ((pindexLast->nHeight) >= params.adjustmentIntervalForkHeight)
        blockstogoback = 4 * params.DifficultyAdjustmentInterval();

    // Go back by what we want to be 4 minutes of blocks
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < blockstogoback; i++)
        pindexFirst = pindexFirst->pprev;
    assert(pindexFirst);

    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting) return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (pindexLast->nHeight >= params.adjustmentIntervalForkHeight)
        nActualTimespan = (pindexLast->GetBlockTime() - nFirstBlockTime) / 4;
    if (nActualTimespan < params.nPowTargetTimespan / 4)
        nActualTimespan = params.nPowTargetTimespan / 4;
    if (nActualTimespan > params.nPowTargetTimespan * 4)
        nActualTimespan = params.nPowTargetTimespan * 4;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

unsigned int KimotoGravityWell(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    unsigned int TimeDaySeconds = 60 * 60 * 24;
    uint64_t TargetBlocksSpacingSeconds = params.nPowTargetSpacing;
    int64_t PastSecondsMin = TimeDaySeconds * 0.0185;
    int64_t PastSecondsMax = TimeDaySeconds * 0.23125;
    uint64_t PastBlocksMin = PastSecondsMin / TargetBlocksSpacingSeconds;
    uint64_t PastBlocksMax = PastSecondsMax / TargetBlocksSpacingSeconds;

    const CBlockIndex *BlockLastSolved = pindexLast;
    const CBlockIndex *BlockReading = pindexLast;
    const CBlockHeader *BlockCreating = pblock;
    BlockCreating = BlockCreating;
    uint64_t PastBlocksMass = 0;
    int64_t PastRateActualSeconds = 0;
    int64_t PastRateTargetSeconds = 0;
    double PastRateAdjustmentRatio = double(1);
    arith_uint256 PastDifficultyAverage;
    arith_uint256 PastDifficultyAveragePrev;
    double EventHorizonDeviation;
    double EventHorizonDeviationFast;
    double EventHorizonDeviationSlow;

    if (BlockLastSolved == NULL || BlockLastSolved->nHeight == 0 || (uint64_t)BlockLastSolved->nHeight < PastBlocksMin)
        return UintToArith256(params.powLimit).GetCompact();

    for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0; i++) {
        if (PastBlocksMax > 0 && i > PastBlocksMax) { break; }

        PastBlocksMass++;

        if (i == 1) { 
            PastDifficultyAverage.SetCompact(BlockReading->nBits); 
        }
        else { 
            arith_uint256 lastDifficulty;
            lastDifficulty.SetCompact(BlockReading->nBits);
            PastDifficultyAverage = ((lastDifficulty - PastDifficultyAveragePrev) / i) + PastDifficultyAveragePrev; 
        }
        PastDifficultyAveragePrev = PastDifficultyAverage;

        PastRateActualSeconds = BlockLastSolved->GetBlockTime() - BlockReading->GetBlockTime();
        PastRateTargetSeconds = TargetBlocksSpacingSeconds * PastBlocksMass;
        PastRateAdjustmentRatio = double(1);
        if (PastRateActualSeconds < 0) { PastRateActualSeconds = 0; }
        if (PastRateActualSeconds != 0 && PastRateTargetSeconds != 0) {
        PastRateAdjustmentRatio = double(PastRateTargetSeconds) / double(PastRateActualSeconds);
        }
        EventHorizonDeviation = 1 + (0.7084 * pow((double(PastBlocksMass)/double(39.96)), -1.228));
        EventHorizonDeviationFast = EventHorizonDeviation;
        EventHorizonDeviationSlow = 1 / EventHorizonDeviation;

        if (PastBlocksMass >= PastBlocksMin) {
            if ((PastRateAdjustmentRatio <= EventHorizonDeviationSlow) || (PastRateAdjustmentRatio >= EventHorizonDeviationFast)) { assert(BlockReading); break; }
        }
        if (BlockReading->pprev == NULL) { assert(BlockReading); break; }

        BlockReading = BlockReading->pprev;
    }

    arith_uint256 bnNew(PastDifficultyAverage);
    if (PastRateActualSeconds != 0 && PastRateTargetSeconds != 0) {
        bnNew *= PastRateActualSeconds;
        bnNew /= PastRateTargetSeconds;
    }

    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    if (bnNew > bnPowLimit) { 
        bnNew = bnPowLimit; 
    }

    //if (fDebug) {
    //  LogPrintf("KimotoGravityWell: PastRateAdjustmentRatio = %g\n", PastRateAdjustmentRatio);
    //  LogPrintf("Before: %08x %s\n", BlockLastSolved->nBits, arith_uint256().SetCompact(BlockLastSolved->nBits).ToString().c_str());
    //  LogPrintf("After: %08x %s\n", bnNew.GetCompact(), bnNew.ToString().c_str());
    //}

    return bnNew.GetCompact();
}

unsigned int DigiShield(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();
    int blockstogoback = 0;

    int64_t nTargetSpacing = params.nPowTargetSpacing;

    if (pindexLast->nHeight+1 >= params.X11ForkHeight) {
        nTargetSpacing = 60 * 2; // switch to 2 minute blocks after block 300,000
    }
    else if (pindexLast->nHeight+1 >= params.fork2Height) {
        nTargetSpacing = 30; // 30 second blocks between block 200,000 and 300,000
    }

    // Retarget every block
    int64_t retargetTimespan = nTargetSpacing;
    int64_t retargetSpacing = nTargetSpacing;
    int64_t retargetInterval = retargetTimespan / retargetSpacing;

    // Only change once per interval
    if ((pindexLast->nHeight+1) % retargetInterval != 0) {
        // Special difficulty rule for testnet
		// Return the last non-special-min-difficulty-rules-block
		const CBlockIndex* pindex = pindexLast;
		while (pindex->pprev && pindex->nHeight % retargetInterval != 0 && pindex->nBits == nProofOfWorkLimit)
            pindex = pindex->pprev;
        return pindex->nBits;
    }

    // DigiByte: This fixes an issue where a 51% attack can change difficulty at will.
    // Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
    blockstogoback = retargetInterval-1;
    if ((pindexLast->nHeight+1) != retargetInterval) blockstogoback = retargetInterval;

    // Go back by what we want to be 14 days worth of blocks
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < blockstogoback; i++) {
        pindexFirst = pindexFirst->pprev;
    }
    assert(pindexFirst);

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - pindexFirst->GetBlockTime();
	//if (fDebug) {
	//  LogPrintf("  nActualTimespan = %g  before bounds\n", nActualTimespan);
	//}

    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);

    if (nActualTimespan < (retargetTimespan - (retargetTimespan/4)) ) nActualTimespan = (retargetTimespan - (retargetTimespan/4));
    if (nActualTimespan > (retargetTimespan + (retargetTimespan/2)) ) nActualTimespan = (retargetTimespan + (retargetTimespan/2));

    // Retarget
    bnNew *= nActualTimespan;
    bnNew /= retargetTimespan;

	//if (fDebug) {
	//  LogPrintf("DigiShield: retargetTimespan = %g nActualTimespan = %g\n", retargetTimespan, nActualTimespan);
	//  LogPrintf("Before: %08x %s\n", pindexLast->nBits, arith_uint256().SetCompact(pindexLast->nBits).ToString().c_str());
	//  LogPrintf("After: %08x %s\n", bnNew.GetCompact(), bnNew.ToString().c_str());
	//}

    if (bnNew > UintToArith256(params.powLimit))
        bnNew = UintToArith256(params.powLimit);

    return bnNew.GetCompact();
}

unsigned int DarkGravityWave(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params) {
	/* current difficulty formula, darkcoin - DarkGravity v3, written by Evan Duffield - evan@darkcoin.io */
	const CBlockIndex *BlockLastSolved = pindexLast;
	const CBlockIndex *BlockReading = pindexLast;
	const CBlockHeader *BlockCreating = pblock;
	BlockCreating = BlockCreating;
	int64_t nActualTimespan = 0;
	int64_t LastBlockTime = 0;
	int64_t PastBlocksMin = 24;
	int64_t PastBlocksMax = 24;
	int64_t CountBlocks = 0;
	arith_uint256 PastDifficultyAverage;
	arith_uint256 PastDifficultyAveragePrev;

	if (BlockLastSolved == NULL || BlockLastSolved->nHeight == 0 || BlockLastSolved->nHeight < PastBlocksMin) {
		return UintToArith256(params.powLimit).GetCompact();
	}

	for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0; i++) {
		if (PastBlocksMax > 0 && i > PastBlocksMax) { break; }
		CountBlocks++;

		if (CountBlocks <= PastBlocksMin) {
			if (CountBlocks == 1) {
                PastDifficultyAverage.SetCompact(BlockReading->nBits);
            }
			else {
                arith_uint256 currentDifficulty;
                currentDifficulty.SetCompact(BlockReading->nBits);
                PastDifficultyAverage = ((PastDifficultyAveragePrev * CountBlocks) + currentDifficulty) / (CountBlocks + 1);
            }
			PastDifficultyAveragePrev = PastDifficultyAverage;
		}

		if (LastBlockTime > 0) {
			int64_t Diff = (LastBlockTime - BlockReading->GetBlockTime());
			nActualTimespan += Diff;
		}
		LastBlockTime = BlockReading->GetBlockTime();

		if (BlockReading->pprev == NULL) { assert(BlockReading); break; }
		BlockReading = BlockReading->pprev;
	}

    arith_uint256 bnNew(PastDifficultyAverage);
    int64_t _nTargetTimespan = CountBlocks * 120;

    if (nActualTimespan < _nTargetTimespan / 3)
        nActualTimespan = _nTargetTimespan / 3;
    if (nActualTimespan > _nTargetTimespan * 3)
        nActualTimespan = _nTargetTimespan * 3;

    // Retarget
    bnNew *= nActualTimespan;
    bnNew /= _nTargetTimespan;

    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    if (bnNew > bnPowLimit) {
        bnNew = bnPowLimit;
    }

    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit)) {
        if (fDebug) LogPrintf("CheckProofOfWork: nBits: %08x != bnTarget: %08x\n", nBits, bnTarget.GetCompact());
        return false;
    }

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget) {
        if (fDebug) LogPrintf("CheckProofOfWork: GetPowHash(): %08x != bnTarget: %08x\n", UintToArith256(hash).GetCompact(), bnTarget.GetCompact());
        return false;
    }

    return true;
}
