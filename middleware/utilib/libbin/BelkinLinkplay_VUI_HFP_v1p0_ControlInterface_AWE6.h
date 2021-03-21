/****************************************************************************
 *
 *		Target Tuning Symbol File
 *		-------------------------
 *
 * 		This file is populated with symbol information only for modules
 *		that have an object ID of 30000 or greater assigned.
 *
 *          Generated on:  09-Nov-2020 19:47:16
 *
 ***************************************************************************/

#ifndef BELKINLINKPLAY_VUI_HFP_V1P0_CONTROLINTERFACE_AWE6_H
#define BELKINLINKPLAY_VUI_HFP_V1P0_CONTROLINTERFACE_AWE6_H

// ----------------------------------------------------------------------
// ACM.VC_TxProc.EchoBulkDelay [DelayMsec]
// Time delay in which the delay is specified in milliseconds

#define AWE_ACM____VC_TxProc____EchoBulkDelay_ID 30016

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_ACM____VC_TxProc____EchoBulkDelay_profileTime_OFFSET 7
#define AWE_ACM____VC_TxProc____EchoBulkDelay_profileTime_MASK 0x00000080
#define AWE_ACM____VC_TxProc____EchoBulkDelay_profileTime_SIZE -1

// float maxDelayTime - Maximum delay, in milliseconds.
#define AWE_ACM____VC_TxProc____EchoBulkDelay_maxDelayTime_OFFSET 8
#define AWE_ACM____VC_TxProc____EchoBulkDelay_maxDelayTime_MASK 0x00000100
#define AWE_ACM____VC_TxProc____EchoBulkDelay_maxDelayTime_SIZE -1

// float currentDelayTime - Current delay.
// Default value: 0
// Range: 0 to 30
#define AWE_ACM____VC_TxProc____EchoBulkDelay_currentDelayTime_OFFSET 9
#define AWE_ACM____VC_TxProc____EchoBulkDelay_currentDelayTime_MASK 0x00000200
#define AWE_ACM____VC_TxProc____EchoBulkDelay_currentDelayTime_SIZE -1

// int stateHeap - Heap in which to allocate state buffer memory.
#define AWE_ACM____VC_TxProc____EchoBulkDelay_stateHeap_OFFSET 10
#define AWE_ACM____VC_TxProc____EchoBulkDelay_stateHeap_MASK 0x00000400
#define AWE_ACM____VC_TxProc____EchoBulkDelay_stateHeap_SIZE -1


// ----------------------------------------------------------------------
// ACM.VC_TxProc.RefLevOffset [ScalerV2]
// General purpose scaler with a single gain

#define AWE_ACM____VC_TxProc____RefLevOffset_ID 30017

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_ACM____VC_TxProc____RefLevOffset_profileTime_OFFSET 7
#define AWE_ACM____VC_TxProc____RefLevOffset_profileTime_MASK 0x00000080
#define AWE_ACM____VC_TxProc____RefLevOffset_profileTime_SIZE -1

// float gain - Gain in either linear or dB units.
// Default value: 17
// Range: -12 to 25
#define AWE_ACM____VC_TxProc____RefLevOffset_gain_OFFSET 8
#define AWE_ACM____VC_TxProc____RefLevOffset_gain_MASK 0x00000100
#define AWE_ACM____VC_TxProc____RefLevOffset_gain_SIZE -1

// float smoothingTime - Time constant of the smoothing process (0 = 
//         unsmoothed).
// Default value: 10
// Range: 0 to 1000
#define AWE_ACM____VC_TxProc____RefLevOffset_smoothingTime_OFFSET 9
#define AWE_ACM____VC_TxProc____RefLevOffset_smoothingTime_MASK 0x00000200
#define AWE_ACM____VC_TxProc____RefLevOffset_smoothingTime_SIZE -1

// int isDB - Selects between linear (=0) and dB (=1) operation
// Default value: 1
// Range: 0 to 1
#define AWE_ACM____VC_TxProc____RefLevOffset_isDB_OFFSET 10
#define AWE_ACM____VC_TxProc____RefLevOffset_isDB_MASK 0x00000400
#define AWE_ACM____VC_TxProc____RefLevOffset_isDB_SIZE -1

// float targetGain - Target gain in linear units.
#define AWE_ACM____VC_TxProc____RefLevOffset_targetGain_OFFSET 11
#define AWE_ACM____VC_TxProc____RefLevOffset_targetGain_MASK 0x00000800
#define AWE_ACM____VC_TxProc____RefLevOffset_targetGain_SIZE -1

// float currentGain - Instantaneous gain applied by the module.
#define AWE_ACM____VC_TxProc____RefLevOffset_currentGain_OFFSET 12
#define AWE_ACM____VC_TxProc____RefLevOffset_currentGain_MASK 0x00001000
#define AWE_ACM____VC_TxProc____RefLevOffset_currentGain_SIZE -1

// float smoothingCoeff - Smoothing coefficient.
#define AWE_ACM____VC_TxProc____RefLevOffset_smoothingCoeff_OFFSET 13
#define AWE_ACM____VC_TxProc____RefLevOffset_smoothingCoeff_MASK 0x00002000
#define AWE_ACM____VC_TxProc____RefLevOffset_smoothingCoeff_SIZE -1


// ----------------------------------------------------------------------
// ACM.VC_TxProc.MicCalibGain [ScalerNV2]
// General purpose scaler with separate gains per channel

#define AWE_ACM____VC_TxProc____MicCalibGain_ID 30018

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_ACM____VC_TxProc____MicCalibGain_profileTime_OFFSET 7
#define AWE_ACM____VC_TxProc____MicCalibGain_profileTime_MASK 0x00000080
#define AWE_ACM____VC_TxProc____MicCalibGain_profileTime_SIZE -1

// float masterGain - Overall gain to apply.
// Default value: 26
// Range: -24 to 50
#define AWE_ACM____VC_TxProc____MicCalibGain_masterGain_OFFSET 8
#define AWE_ACM____VC_TxProc____MicCalibGain_masterGain_MASK 0x00000100
#define AWE_ACM____VC_TxProc____MicCalibGain_masterGain_SIZE -1

// float smoothingTime - Time constant of the smoothing process (0 = 
//         unsmoothed).
// Default value: 10
// Range: 0 to 1000
#define AWE_ACM____VC_TxProc____MicCalibGain_smoothingTime_OFFSET 9
#define AWE_ACM____VC_TxProc____MicCalibGain_smoothingTime_MASK 0x00000200
#define AWE_ACM____VC_TxProc____MicCalibGain_smoothingTime_SIZE -1

// int isDB - Selects between linear (=0) and dB (=1) operation
// Default value: 1
// Range: 0 to 1
#define AWE_ACM____VC_TxProc____MicCalibGain_isDB_OFFSET 10
#define AWE_ACM____VC_TxProc____MicCalibGain_isDB_MASK 0x00000400
#define AWE_ACM____VC_TxProc____MicCalibGain_isDB_SIZE -1

// float smoothingCoeff - Smoothing coefficient.
#define AWE_ACM____VC_TxProc____MicCalibGain_smoothingCoeff_OFFSET 11
#define AWE_ACM____VC_TxProc____MicCalibGain_smoothingCoeff_MASK 0x00000800
#define AWE_ACM____VC_TxProc____MicCalibGain_smoothingCoeff_SIZE -1

// float trimGain[1] - Array of trim gains, one per channel
// Default value:
//     0
// Range: -24 to 24
#define AWE_ACM____VC_TxProc____MicCalibGain_trimGain_OFFSET 12
#define AWE_ACM____VC_TxProc____MicCalibGain_trimGain_MASK 0x00001000
#define AWE_ACM____VC_TxProc____MicCalibGain_trimGain_SIZE 1

// float targetGain[1] - Computed target gains in linear units
#define AWE_ACM____VC_TxProc____MicCalibGain_targetGain_OFFSET 13
#define AWE_ACM____VC_TxProc____MicCalibGain_targetGain_MASK 0x00002000
#define AWE_ACM____VC_TxProc____MicCalibGain_targetGain_SIZE 1

// float currentGain[1] - Instanteous gains.  These ramp towards 
//         targetGain
#define AWE_ACM____VC_TxProc____MicCalibGain_currentGain_OFFSET 14
#define AWE_ACM____VC_TxProc____MicCalibGain_currentGain_MASK 0x00004000
#define AWE_ACM____VC_TxProc____MicCalibGain_currentGain_SIZE 1


// ----------------------------------------------------------------------
// ACM.VC_TxProc.ResidualEcho.LimiterCore1 [AGCLimiterCore]
// Gain computer used to realize soft-knee peak limiters

#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_ID 30002

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_profileTime_OFFSET 7
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_profileTime_MASK 0x00000080
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_profileTime_SIZE -1

// float threshold - Amplitude level at which the AGC Limiter Core 
//         reduces its output gain value
// Default value: -24
// Range: -60 to 0.  Step size = 1
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_threshold_OFFSET 8
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_threshold_MASK 0x00000100
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_threshold_SIZE -1

// float gain - Value used to scale the output of the AGC Limiter Core
// Default value: 0
// Range: 0 to 100
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_gain_OFFSET 9
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_gain_MASK 0x00000200
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_gain_SIZE -1

// float slope - Internal derived variable which holds the slope of the 
//         compression curve
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_slope_OFFSET 10
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_slope_MASK 0x00000400
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_slope_SIZE -1

// float kneeDepth - Knee depth controls the sharpness of the transition 
//         between no limiting and limiting
// Default value: 2
// Range: 0.1 to 60
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_kneeDepth_OFFSET 11
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_kneeDepth_MASK 0x00000800
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_kneeDepth_SIZE -1

// float ratio - Slope of the output attenuation when the signal is 
//         above threshold - derived from the standard compression ratio 
//         parameter by the formula slope = 1.0 - (1.0/ratio)
// Default value: 20
// Range: 1 to 100
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_ratio_OFFSET 12
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_ratio_MASK 0x00001000
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_ratio_SIZE -1

// float attackTime - Envelope detector attack time constant
// Default value: 0.01
// Range: 0.01 to 1000
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_attackTime_OFFSET 13
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_attackTime_MASK 0x00002000
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_attackTime_SIZE -1

// float decayTime - Envelope detector decay time constant
// Default value: 30
// Range: 0.01 to 1000
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_decayTime_OFFSET 14
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_decayTime_MASK 0x00004000
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_decayTime_SIZE -1

// float currentGain - Instantaneous gain computed by the block
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_currentGain_OFFSET 15
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_currentGain_MASK 0x00008000
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_currentGain_SIZE -1

// float sharpnessFactor - Internal derived variable which is used to 
//         implement the soft knee
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_sharpnessFactor_OFFSET 16
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_sharpnessFactor_MASK 0x00010000
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_sharpnessFactor_SIZE -1

// float attackCoeff - Internal derived variable which implements the 
//         attackTime
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_attackCoeff_OFFSET 17
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_attackCoeff_MASK 0x00020000
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_attackCoeff_SIZE -1

// float decayCoeff - Internal derived variable which implements the 
//         decayTime
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_decayCoeff_OFFSET 18
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_decayCoeff_MASK 0x00040000
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_decayCoeff_SIZE -1

// float envState - Holds the instantaneous state of the envelope 
//         detector
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_envState_OFFSET 19
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_envState_MASK 0x00080000
#define AWE_ACM____VC_TxProc____ResidualEcho____LimiterCore1_envState_SIZE -1


// ----------------------------------------------------------------------
// ACM.VC_TxProc.ResidualEcho.OnSetDetect.GainDropThresh [LogicCompareConst]
// Compares the value at the input pin against a stored value

#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____GainDropThresh_ID 30003

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____GainDropThresh_profileTime_OFFSET 7
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____GainDropThresh_profileTime_MASK 0x00000080
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____GainDropThresh_profileTime_SIZE -1

// int compareType - Selects the type of comparison that is implemented 
//         by the module: Equal=0 NotEqual=1 lessThan=2 LessOrEqual=3 
//         greaterThan=4 GreaterOrEqual=5
// Default value: 2
// Range: 0 to 5
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____GainDropThresh_compareType_OFFSET 8
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____GainDropThresh_compareType_MASK 0x00000100
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____GainDropThresh_compareType_SIZE -1

// float constValue - Selects value against to compare against.  In 
//         linear units.
// Default value: -4
// Range: unrestricted
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____GainDropThresh_constValue_OFFSET 9
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____GainDropThresh_constValue_MASK 0x00000200
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____GainDropThresh_constValue_SIZE -1


// ----------------------------------------------------------------------
// ACM.VC_TxProc.ResidualEcho.OnSetDetect.OnSetAttackRls [AGCAttackRelease]
// Multi-channel envelope detector with programmable attack and release 
// times

#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____OnSetAttackRls_ID 30019

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____OnSetAttackRls_profileTime_OFFSET 7
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____OnSetAttackRls_profileTime_MASK 0x00000080
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____OnSetAttackRls_profileTime_SIZE -1

// float attackTime - Speed at which the detector reacts to increasing 
//         levels.
// Default value: 100
// Range: 0.01 to 1000
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____OnSetAttackRls_attackTime_OFFSET 8
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____OnSetAttackRls_attackTime_MASK 0x00000100
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____OnSetAttackRls_attackTime_SIZE -1

// float releaseTime - Speed at which the detector reacts to decreasing 
//         levels.
// Default value: 0.01
// Range: 0.01 to 1000
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____OnSetAttackRls_releaseTime_OFFSET 9
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____OnSetAttackRls_releaseTime_MASK 0x00000200
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____OnSetAttackRls_releaseTime_SIZE -1

// float attackCoeff - Internal coefficient realizing the attack time.
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____OnSetAttackRls_attackCoeff_OFFSET 10
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____OnSetAttackRls_attackCoeff_MASK 0x00000400
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____OnSetAttackRls_attackCoeff_SIZE -1

// float releaseCoeff - Internal coefficient realizing the release time.
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____OnSetAttackRls_releaseCoeff_OFFSET 11
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____OnSetAttackRls_releaseCoeff_MASK 0x00000800
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____OnSetAttackRls_releaseCoeff_SIZE -1

// float envStates[1] - Vector of sample-by-sample states of the 
//         envelope detectors; each column is the state for a channel.
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____OnSetAttackRls_envStates_OFFSET 12
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____OnSetAttackRls_envStates_MASK 0x00001000
#define AWE_ACM____VC_TxProc____ResidualEcho____OnSetDetect____OnSetAttackRls_envStates_SIZE 1


// ----------------------------------------------------------------------
// ACM.VC_TxProc.ResidualEcho.FinalResidualGate.DownwardExpanderCore [DownwardExpanderCore]
// Gain computer used to realize a downward expander (or noise gate)

#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_ID 30021

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_profileTime_OFFSET 7
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_profileTime_MASK 0x00000080
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_profileTime_SIZE -1

// float threshold - Below this level the module reduces gain
// Default value: -75
// Range: -100 to 0
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_threshold_OFFSET 8
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_threshold_MASK 0x00000100
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_threshold_SIZE -1

// float ratio - Slope of the gain curve when the signal is below the 
//         threshold
// Default value: 1.5
// Range: 0.1 to 100
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_ratio_OFFSET 9
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_ratio_MASK 0x00000200
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_ratio_SIZE -1

// float kneeDepth - Knee depth controls the sharpness of the transition 
//         between expanding and not expanding
// Default value: 2
// Range: 0 to 60
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_kneeDepth_OFFSET 10
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_kneeDepth_MASK 0x00000400
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_kneeDepth_SIZE -1

// float attackTime - Envelope detector attack time constant
// Default value: 100
// Range: 0.01 to 1000
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_attackTime_OFFSET 11
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_attackTime_MASK 0x00000800
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_attackTime_SIZE -1

// float decayTime - Envelope detector decay time constant
// Default value: 20
// Range: 0.01 to 1000
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_decayTime_OFFSET 12
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_decayTime_MASK 0x00001000
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_decayTime_SIZE -1

// float currentGain - Instantaneous gain computed by the block
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_currentGain_OFFSET 13
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_currentGain_MASK 0x00002000
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_currentGain_SIZE -1

// float attackCoeff - Internal derived variable which implements the 
//         attackTime
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_attackCoeff_OFFSET 14
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_attackCoeff_MASK 0x00004000
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_attackCoeff_SIZE -1

// float decayCoeff - Internal derived variable which implements the 
//         decayTime
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_decayCoeff_OFFSET 15
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_decayCoeff_MASK 0x00008000
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_decayCoeff_SIZE -1

// float envState - Holds the instantaneous state of the envelope 
//         detector
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_envState_OFFSET 16
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_envState_MASK 0x00010000
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_envState_SIZE -1

// float kneePoly[3] - Derived variable for interpolating the soft knee
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_kneePoly_OFFSET 17
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_kneePoly_MASK 0x00020000
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____DownwardExpanderCore_kneePoly_SIZE 3


// ----------------------------------------------------------------------
// ACM.VC_TxProc.ResidualEcho.FinalResidualGate.BgNoiseOffset [ScaleOffset]
// Scales a signal and then adds an offset

#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____BgNoiseOffset_ID 30022

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____BgNoiseOffset_profileTime_OFFSET 7
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____BgNoiseOffset_profileTime_MASK 0x00000080
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____BgNoiseOffset_profileTime_SIZE -1

// float gain - Linear gain.
// Default value: 1
// Range: -10 to 10
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____BgNoiseOffset_gain_OFFSET 8
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____BgNoiseOffset_gain_MASK 0x00000100
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____BgNoiseOffset_gain_SIZE -1

// float offset - DC offset.
// Default value: 4
// Range: -10 to 10
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____BgNoiseOffset_offset_OFFSET 9
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____BgNoiseOffset_offset_MASK 0x00000200
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____BgNoiseOffset_offset_SIZE -1


// ----------------------------------------------------------------------
// ACM.VC_TxProc.ResidualEcho.FinalResidualGate.Att_Slow [DCSourceV2]
// Outputs a constant DC value with optional input control pin to scale 
// DC value.

#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____Att_Slow_ID 30023

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____Att_Slow_profileTime_OFFSET 7
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____Att_Slow_profileTime_MASK 0x00000080
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____Att_Slow_profileTime_SIZE -1

// float value - Data Value
// Default value: 60000
// Range: 0 to 200000
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____Att_Slow_value_OFFSET 8
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____Att_Slow_value_MASK 0x00000100
#define AWE_ACM____VC_TxProc____ResidualEcho____FinalResidualGate____Att_Slow_value_SIZE -1


// ----------------------------------------------------------------------
// ACM.VC_TxProc.TxProcess.TableLookupActThresh [TableLookupIntFloat]
// Table lookup where the input serves as an index into a lookup table

#define AWE_ACM____VC_TxProc____TxProcess____TableLookupActThresh_ID 30011

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_ACM____VC_TxProc____TxProcess____TableLookupActThresh_profileTime_OFFSET 7
#define AWE_ACM____VC_TxProc____TxProcess____TableLookupActThresh_profileTime_MASK 0x00000080
#define AWE_ACM____VC_TxProc____TxProcess____TableLookupActThresh_profileTime_SIZE -1

// int L - Number of entries in the table
#define AWE_ACM____VC_TxProc____TxProcess____TableLookupActThresh_L_OFFSET 8
#define AWE_ACM____VC_TxProc____TxProcess____TableLookupActThresh_L_MASK 0x00000100
#define AWE_ACM____VC_TxProc____TxProcess____TableLookupActThresh_L_SIZE -1

// float table[2] - Table of lookup values
// Default value:
//     -45
//     -50
// Range: unrestricted
#define AWE_ACM____VC_TxProc____TxProcess____TableLookupActThresh_table_OFFSET 9
#define AWE_ACM____VC_TxProc____TxProcess____TableLookupActThresh_table_MASK 0x00000200
#define AWE_ACM____VC_TxProc____TxProcess____TableLookupActThresh_table_SIZE 2


// ----------------------------------------------------------------------
// ACM.VC_TxProc.TxProcess.TxOutGain [ScalerV2]
// General purpose scaler with a single gain

#define AWE_ACM____VC_TxProc____TxProcess____TxOutGain_ID 30012

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_ACM____VC_TxProc____TxProcess____TxOutGain_profileTime_OFFSET 7
#define AWE_ACM____VC_TxProc____TxProcess____TxOutGain_profileTime_MASK 0x00000080
#define AWE_ACM____VC_TxProc____TxProcess____TxOutGain_profileTime_SIZE -1

// float gain - Gain in either linear or dB units.
// Default value: 27
// Range: -24 to 50
#define AWE_ACM____VC_TxProc____TxProcess____TxOutGain_gain_OFFSET 8
#define AWE_ACM____VC_TxProc____TxProcess____TxOutGain_gain_MASK 0x00000100
#define AWE_ACM____VC_TxProc____TxProcess____TxOutGain_gain_SIZE -1

// float smoothingTime - Time constant of the smoothing process (0 = 
//         unsmoothed).
// Default value: 0
// Range: 0 to 1000
#define AWE_ACM____VC_TxProc____TxProcess____TxOutGain_smoothingTime_OFFSET 9
#define AWE_ACM____VC_TxProc____TxProcess____TxOutGain_smoothingTime_MASK 0x00000200
#define AWE_ACM____VC_TxProc____TxProcess____TxOutGain_smoothingTime_SIZE -1

// int isDB - Selects between linear (=0) and dB (=1) operation
// Default value: 1
// Range: 0 to 1
#define AWE_ACM____VC_TxProc____TxProcess____TxOutGain_isDB_OFFSET 10
#define AWE_ACM____VC_TxProc____TxProcess____TxOutGain_isDB_MASK 0x00000400
#define AWE_ACM____VC_TxProc____TxProcess____TxOutGain_isDB_SIZE -1

// float targetGain - Target gain in linear units.
#define AWE_ACM____VC_TxProc____TxProcess____TxOutGain_targetGain_OFFSET 11
#define AWE_ACM____VC_TxProc____TxProcess____TxOutGain_targetGain_MASK 0x00000800
#define AWE_ACM____VC_TxProc____TxProcess____TxOutGain_targetGain_SIZE -1

// float currentGain - Instantaneous gain applied by the module.
#define AWE_ACM____VC_TxProc____TxProcess____TxOutGain_currentGain_OFFSET 12
#define AWE_ACM____VC_TxProc____TxProcess____TxOutGain_currentGain_MASK 0x00001000
#define AWE_ACM____VC_TxProc____TxProcess____TxOutGain_currentGain_SIZE -1

// float smoothingCoeff - Smoothing coefficient.
#define AWE_ACM____VC_TxProc____TxProcess____TxOutGain_smoothingCoeff_OFFSET 13
#define AWE_ACM____VC_TxProc____TxProcess____TxOutGain_smoothingCoeff_MASK 0x00002000
#define AWE_ACM____VC_TxProc____TxProcess____TxOutGain_smoothingCoeff_SIZE -1


// ----------------------------------------------------------------------
// ACM.VC_TxProc.TxProcess.TxAGCCore [AGCCore]
// Slowly varying RMS based gain computer

#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_ID 30025

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_profileTime_OFFSET 7
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_profileTime_MASK 0x00000080
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_profileTime_SIZE -1

// float targetLevel - Target audio level.
// Default value: -29
// Range: -50 to 50.  Step size = 0.1
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_targetLevel_OFFSET 8
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_targetLevel_MASK 0x00000100
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_targetLevel_SIZE -1

// float maxAttenuation - Maximum attenuation of the AGC.
// Default value: 21
// Range: 0 to 100
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_maxAttenuation_OFFSET 9
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_maxAttenuation_MASK 0x00000200
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_maxAttenuation_SIZE -1

// float maxGain - Maximum gain of the AGC.
// Default value: 18
// Range: 6 to 50
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_maxGain_OFFSET 10
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_maxGain_MASK 0x00000400
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_maxGain_SIZE -1

// float ratio - Slope of the output attenuation when the signal is 
//         above threshold.
// Default value: 20
// Range: 1 to 100
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_ratio_OFFSET 11
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_ratio_MASK 0x00000800
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_ratio_SIZE -1

// float activationThreshold - Activation threshold of the AGC. The AGC 
//         stops updating its gain when the instantaneous input level is below 
//         this value.
// Default value: -45
// Range: -100 to -20
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_activationThreshold_OFFSET 12
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_activationThreshold_MASK 0x00001000
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_activationThreshold_SIZE -1

// float smoothingTime - Response time of the AGC. This controls the RMS 
//         smoothing interval and the time constant of the smoothly updating 
//         gain.
// Default value: 500
// Range: 1 to 5000
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_smoothingTime_OFFSET 13
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_smoothingTime_MASK 0x00002000
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_smoothingTime_SIZE -1

// int enableRecovery - Boolean which enables the AGC's gain recovery. 
//         If enabled, the AGC slows returns 0 dB gain when the input is below 
//         the activation threshold.
// Default value: 1
// Range: 0 to 1
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_enableRecovery_OFFSET 14
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_enableRecovery_MASK 0x00004000
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_enableRecovery_SIZE -1

// float recoveryRate - Rate at which the gain is adjusted when the 
//         input is below the activation threshold.
// Default value: 1
// Range: 0.1 to 20
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_recoveryRate_OFFSET 15
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_recoveryRate_MASK 0x00008000
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_recoveryRate_SIZE -1

// float currentGain - Instantaneous gain of the smoothing operation.
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_currentGain_OFFSET 16
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_currentGain_MASK 0x00010000
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_currentGain_SIZE -1

// float oneOverSlope - Used by the processing function to compute the 
//         amount of cut/boost. Equal to 1-1/ratio.
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_oneOverSlope_OFFSET 17
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_oneOverSlope_MASK 0x00020000
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_oneOverSlope_SIZE -1

// float smoothingCoeffSample - Sample-by-sample smoothing coefficient 
//         for the output gain adjustment. Set via smoothingTime.
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_smoothingCoeffSample_OFFSET 18
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_smoothingCoeffSample_MASK 0x00040000
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_smoothingCoeffSample_SIZE -1

// float smoothingCoeffBlock - Block-by-block smoothing coefficient for 
//         the RMS measurement. Set via smoothingTime.
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_smoothingCoeffBlock_OFFSET 19
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_smoothingCoeffBlock_MASK 0x00080000
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_smoothingCoeffBlock_SIZE -1

// float recoveryRateUp - Recovery rate coefficient when increasing the 
//         gain (it is always >= 1).
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_recoveryRateUp_OFFSET 20
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_recoveryRateUp_MASK 0x00100000
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_recoveryRateUp_SIZE -1

// float recoveryRateDown - Recovery rate coefficient when decreasing 
//         the gain (it is always <= 1).
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_recoveryRateDown_OFFSET 21
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_recoveryRateDown_MASK 0x00200000
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_recoveryRateDown_SIZE -1

// float targetGain - Target gain of the smoothing operation.
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_targetGain_OFFSET 22
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_targetGain_MASK 0x00400000
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_targetGain_SIZE -1

// float energy - Smoothed energy measurement.
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_energy_OFFSET 23
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_energy_MASK 0x00800000
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_energy_SIZE -1

// float oneOverNumSamples - 1 divided by the number of samples in the 
//         input pin. Used internally by the algorithm in order to save a divide 
//         operation.
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_oneOverNumSamples_OFFSET 24
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_oneOverNumSamples_MASK 0x01000000
#define AWE_ACM____VC_TxProc____TxProcess____TxAGCCore_oneOverNumSamples_SIZE -1


// ----------------------------------------------------------------------
// ACM.VC_TxProc.VoiceCommState [SourceInt]
// Source buffer holding 1 wire of integer data

#define AWE_VoiceCommState_ID 30024

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_VoiceCommState_profileTime_OFFSET 7
#define AWE_VoiceCommState_profileTime_MASK 0x00000080
#define AWE_VoiceCommState_profileTime_SIZE -1

// int value[1] - Array of interleaved audio data
// Default value:
//     0
// Range: unrestricted
#define AWE_VoiceCommState_value_OFFSET 8
#define AWE_VoiceCommState_value_MASK 0x00000100
#define AWE_VoiceCommState_value_SIZE 1


// ----------------------------------------------------------------------
// ACM.VC_TxProc.DemoBypass [BooleanSource]
// Source buffer holding 1 wire of data

#define AWE_ACM____VC_TxProc____DemoBypass_ID 30027

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_ACM____VC_TxProc____DemoBypass_profileTime_OFFSET 7
#define AWE_ACM____VC_TxProc____DemoBypass_profileTime_MASK 0x00000080
#define AWE_ACM____VC_TxProc____DemoBypass_profileTime_SIZE -1

// int value[1] - Array of interleaved audio real data.
// Default value:
//     0
// Range: 0 to 1.  Step size = 1
#define AWE_ACM____VC_TxProc____DemoBypass_value_OFFSET 8
#define AWE_ACM____VC_TxProc____DemoBypass_value_MASK 0x00000100
#define AWE_ACM____VC_TxProc____DemoBypass_value_SIZE 1


// ----------------------------------------------------------------------
// ACMTxGain [ScalerV2]
// General purpose scaler with a single gain

#define AWE_ACMTxGain_ID 32407

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_ACMTxGain_profileTime_OFFSET 7
#define AWE_ACMTxGain_profileTime_MASK 0x00000080
#define AWE_ACMTxGain_profileTime_SIZE -1

// float gain - Gain in either linear or dB units.
// Default value: 6
// Range: -48 to 48
#define AWE_ACMTxGain_gain_OFFSET 8
#define AWE_ACMTxGain_gain_MASK 0x00000100
#define AWE_ACMTxGain_gain_SIZE -1

// float smoothingTime - Time constant of the smoothing process (0 = 
//         unsmoothed).
// Default value: 10
// Range: 0 to 1000
#define AWE_ACMTxGain_smoothingTime_OFFSET 9
#define AWE_ACMTxGain_smoothingTime_MASK 0x00000200
#define AWE_ACMTxGain_smoothingTime_SIZE -1

// int isDB - Selects between linear (=0) and dB (=1) operation
// Default value: 1
// Range: 0 to 1
#define AWE_ACMTxGain_isDB_OFFSET 10
#define AWE_ACMTxGain_isDB_MASK 0x00000400
#define AWE_ACMTxGain_isDB_SIZE -1

// float targetGain - Target gain in linear units.
#define AWE_ACMTxGain_targetGain_OFFSET 11
#define AWE_ACMTxGain_targetGain_MASK 0x00000800
#define AWE_ACMTxGain_targetGain_SIZE -1

// float currentGain - Instantaneous gain applied by the module.
#define AWE_ACMTxGain_currentGain_OFFSET 12
#define AWE_ACMTxGain_currentGain_MASK 0x00001000
#define AWE_ACMTxGain_currentGain_SIZE -1

// float smoothingCoeff - Smoothing coefficient.
#define AWE_ACMTxGain_smoothingCoeff_OFFSET 13
#define AWE_ACMTxGain_smoothingCoeff_MASK 0x00002000
#define AWE_ACMTxGain_smoothingCoeff_SIZE -1


// ----------------------------------------------------------------------
// AFE.TrimGains [ScalerNV2]
// General purpose scaler with separate gains per channel

#define AWE_MicTrimGains_ID 30015

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_MicTrimGains_profileTime_OFFSET 7
#define AWE_MicTrimGains_profileTime_MASK 0x00000080
#define AWE_MicTrimGains_profileTime_SIZE -1

// float masterGain - Overall gain to apply.
// Default value: 26
// Range: -24 to 30
#define AWE_MicTrimGains_masterGain_OFFSET 8
#define AWE_MicTrimGains_masterGain_MASK 0x00000100
#define AWE_MicTrimGains_masterGain_SIZE -1

// float smoothingTime - Time constant of the smoothing process (0 = 
//         unsmoothed).
// Default value: 0
// Range: 0 to 1000
#define AWE_MicTrimGains_smoothingTime_OFFSET 9
#define AWE_MicTrimGains_smoothingTime_MASK 0x00000200
#define AWE_MicTrimGains_smoothingTime_SIZE -1

// int isDB - Selects between linear (=0) and dB (=1) operation
// Default value: 1
// Range: 0 to 1
#define AWE_MicTrimGains_isDB_OFFSET 10
#define AWE_MicTrimGains_isDB_MASK 0x00000400
#define AWE_MicTrimGains_isDB_SIZE -1

// float smoothingCoeff - Smoothing coefficient.
#define AWE_MicTrimGains_smoothingCoeff_OFFSET 11
#define AWE_MicTrimGains_smoothingCoeff_MASK 0x00000800
#define AWE_MicTrimGains_smoothingCoeff_SIZE -1

// float trimGain[10] - Array of trim gains, one per channel
// Default value:
//     0
//     0
//     0
//     0
//     0
//     0
//     0
//     0
//     0
//     0
// Range: -24 to 24
#define AWE_MicTrimGains_trimGain_OFFSET 12
#define AWE_MicTrimGains_trimGain_MASK 0x00001000
#define AWE_MicTrimGains_trimGain_SIZE 10

// float targetGain[10] - Computed target gains in linear units
#define AWE_MicTrimGains_targetGain_OFFSET 13
#define AWE_MicTrimGains_targetGain_MASK 0x00002000
#define AWE_MicTrimGains_targetGain_SIZE 10

// float currentGain[10] - Instanteous gains.  These ramp towards 
//         targetGain
#define AWE_MicTrimGains_currentGain_OFFSET 14
#define AWE_MicTrimGains_currentGain_MASK 0x00004000
#define AWE_MicTrimGains_currentGain_SIZE 10


// ----------------------------------------------------------------------
// AFE.InputMeter [Meter]
// Peak and RMS meter module

#define AWE_AFE____InputMeter_ID 30000

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_AFE____InputMeter_profileTime_OFFSET 7
#define AWE_AFE____InputMeter_profileTime_MASK 0x00000080
#define AWE_AFE____InputMeter_profileTime_SIZE -1

// int meterType - Operating mode of the meter. Selects between peak and 
//         RMS calculations. See the discussion section for more details.
// Default value: 18
// Range: unrestricted
#define AWE_AFE____InputMeter_meterType_OFFSET 8
#define AWE_AFE____InputMeter_meterType_MASK 0x00000100
#define AWE_AFE____InputMeter_meterType_SIZE -1

// float attackTime - Attack time of the meter. Specifies how quickly 
//         the meter value rises.
#define AWE_AFE____InputMeter_attackTime_OFFSET 9
#define AWE_AFE____InputMeter_attackTime_MASK 0x00000200
#define AWE_AFE____InputMeter_attackTime_SIZE -1

// float releaseTime - Release time of the meter. Specifies how quickly 
//         the meter decays.
#define AWE_AFE____InputMeter_releaseTime_OFFSET 10
#define AWE_AFE____InputMeter_releaseTime_MASK 0x00000400
#define AWE_AFE____InputMeter_releaseTime_SIZE -1

// float attackCoeff - Internal coefficient that realizes the attack 
//         time.
#define AWE_AFE____InputMeter_attackCoeff_OFFSET 11
#define AWE_AFE____InputMeter_attackCoeff_MASK 0x00000800
#define AWE_AFE____InputMeter_attackCoeff_SIZE -1

// float releaseCoeff - Internal coefficient that realizes the release 
//         time.
#define AWE_AFE____InputMeter_releaseCoeff_OFFSET 12
#define AWE_AFE____InputMeter_releaseCoeff_MASK 0x00001000
#define AWE_AFE____InputMeter_releaseCoeff_SIZE -1

// float value[10] - Array of meter output values, one per channel.
#define AWE_AFE____InputMeter_value_OFFSET 13
#define AWE_AFE____InputMeter_value_MASK 0x00002000
#define AWE_AFE____InputMeter_value_SIZE 10


// ----------------------------------------------------------------------
// AFE.MicHPF [SecondOrderFilterSmoothed]
// General 2nd order filter designer with smoothing

#define AWE_AFE____MicHPF_ID 32006

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_AFE____MicHPF_profileTime_OFFSET 7
#define AWE_AFE____MicHPF_profileTime_MASK 0x00000080
#define AWE_AFE____MicHPF_profileTime_SIZE -1

// int filterType - Selects the type of filter that is implemented by 
//         the module: Bypass=0, Gain=1, Butter1stLPF=2, Butter2ndLPF=3, 
//         Butter1stHPF=4, Butter2ndHPF=5, Allpass1st=6, Allpass2nd=7, 
//         Shelf2ndLow=8, Shelf2ndLowQ=9, Shelf2ndHigh=10, Shelf2ndHighQ=11, 
//         PeakEQ=12, Notch=13, Bandpass=14, Bessel1stLPF=15, Bessel1stHPF=16, 
//         AsymShelf1stLow=17, AsymShelf1stHigh=18, SymShelf1stLow=19, 
//         SymShelf1stHigh=20, VariableQLPF=21, VariableQHPF=22.
// Default value: 5
// Range: 0 to 22
#define AWE_AFE____MicHPF_filterType_OFFSET 8
#define AWE_AFE____MicHPF_filterType_MASK 0x00000100
#define AWE_AFE____MicHPF_filterType_SIZE -1

// float freq - Cutoff frequency of the filter, in Hz.
// Default value: 100
// Range: 10 to 7920.  Step size = 0.1
#define AWE_AFE____MicHPF_freq_OFFSET 9
#define AWE_AFE____MicHPF_freq_MASK 0x00000200
#define AWE_AFE____MicHPF_freq_SIZE -1

// float gain - Amount of boost or cut to apply, in dB if applicable.
// Default value: 0
// Range: -24 to 24.  Step size = 0.1
#define AWE_AFE____MicHPF_gain_OFFSET 10
#define AWE_AFE____MicHPF_gain_MASK 0x00000400
#define AWE_AFE____MicHPF_gain_SIZE -1

// float Q - Specifies the Q of the filter, if applicable.
// Default value: 1
// Range: 0 to 20.  Step size = 0.1
#define AWE_AFE____MicHPF_Q_OFFSET 11
#define AWE_AFE____MicHPF_Q_MASK 0x00000800
#define AWE_AFE____MicHPF_Q_SIZE -1

// float smoothingTime - Time constant of the smoothing process.
// Default value: 10
// Range: 0 to 1000.  Step size = 1
#define AWE_AFE____MicHPF_smoothingTime_OFFSET 12
#define AWE_AFE____MicHPF_smoothingTime_MASK 0x00001000
#define AWE_AFE____MicHPF_smoothingTime_SIZE -1

// int updateActive - Specifies whether the filter coefficients are 
//         updating (=1) or fixed (=0).
// Default value: 1
// Range: 0 to 1
#define AWE_AFE____MicHPF_updateActive_OFFSET 13
#define AWE_AFE____MicHPF_updateActive_MASK 0x00002000
#define AWE_AFE____MicHPF_updateActive_SIZE -1

// float b0 - Desired first numerator coefficient.
#define AWE_AFE____MicHPF_b0_OFFSET 14
#define AWE_AFE____MicHPF_b0_MASK 0x00004000
#define AWE_AFE____MicHPF_b0_SIZE -1

// float b1 - Desired second numerator coefficient.
#define AWE_AFE____MicHPF_b1_OFFSET 15
#define AWE_AFE____MicHPF_b1_MASK 0x00008000
#define AWE_AFE____MicHPF_b1_SIZE -1

// float b2 - Desired third numerator coefficient.
#define AWE_AFE____MicHPF_b2_OFFSET 16
#define AWE_AFE____MicHPF_b2_MASK 0x00010000
#define AWE_AFE____MicHPF_b2_SIZE -1

// float a1 - Desired second denominator coefficient.
#define AWE_AFE____MicHPF_a1_OFFSET 17
#define AWE_AFE____MicHPF_a1_MASK 0x00020000
#define AWE_AFE____MicHPF_a1_SIZE -1

// float a2 - Desired third denominator coefficient.
#define AWE_AFE____MicHPF_a2_OFFSET 18
#define AWE_AFE____MicHPF_a2_MASK 0x00040000
#define AWE_AFE____MicHPF_a2_SIZE -1

// float current_b0 - Instantaneous first numerator coefficient.
#define AWE_AFE____MicHPF_current_b0_OFFSET 19
#define AWE_AFE____MicHPF_current_b0_MASK 0x00080000
#define AWE_AFE____MicHPF_current_b0_SIZE -1

// float current_b1 - Instantaneous second numerator coefficient.
#define AWE_AFE____MicHPF_current_b1_OFFSET 20
#define AWE_AFE____MicHPF_current_b1_MASK 0x00100000
#define AWE_AFE____MicHPF_current_b1_SIZE -1

// float current_b2 - Instantaneous third numerator coefficient.
#define AWE_AFE____MicHPF_current_b2_OFFSET 21
#define AWE_AFE____MicHPF_current_b2_MASK 0x00200000
#define AWE_AFE____MicHPF_current_b2_SIZE -1

// float current_a1 - Instantaneous second denominator coefficient.
#define AWE_AFE____MicHPF_current_a1_OFFSET 22
#define AWE_AFE____MicHPF_current_a1_MASK 0x00400000
#define AWE_AFE____MicHPF_current_a1_SIZE -1

// float current_a2 - Instantaneous third denominator coefficient.
#define AWE_AFE____MicHPF_current_a2_OFFSET 23
#define AWE_AFE____MicHPF_current_a2_MASK 0x00800000
#define AWE_AFE____MicHPF_current_a2_SIZE -1

// float smoothingCoeff - Smoothing coefficient. This is computed based 
//         on the smoothingTime, sample rate, and block size of the module.
#define AWE_AFE____MicHPF_smoothingCoeff_OFFSET 24
#define AWE_AFE____MicHPF_smoothingCoeff_MASK 0x01000000
#define AWE_AFE____MicHPF_smoothingCoeff_SIZE -1

// float state[4] - State variables. 2 per channel.
#define AWE_AFE____MicHPF_state_OFFSET 25
#define AWE_AFE____MicHPF_state_MASK 0x02000000
#define AWE_AFE____MicHPF_state_SIZE 4


// ----------------------------------------------------------------------
// AFE.MicMuteTimeConstant [AGCAttackRelease]
// Multi-channel envelope detector with programmable attack and release 
// times

#define AWE_AFE____MicMuteTimeConstant_ID 32009

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_AFE____MicMuteTimeConstant_profileTime_OFFSET 7
#define AWE_AFE____MicMuteTimeConstant_profileTime_MASK 0x00000080
#define AWE_AFE____MicMuteTimeConstant_profileTime_SIZE -1

// float attackTime - Speed at which the detector reacts to increasing 
//         levels.
// Default value: 0.01
// Range: 0.01 to 1000
#define AWE_AFE____MicMuteTimeConstant_attackTime_OFFSET 8
#define AWE_AFE____MicMuteTimeConstant_attackTime_MASK 0x00000100
#define AWE_AFE____MicMuteTimeConstant_attackTime_SIZE -1

// float releaseTime - Speed at which the detector reacts to decreasing 
//         levels.
// Default value: 250
// Range: 0.01 to 1000
#define AWE_AFE____MicMuteTimeConstant_releaseTime_OFFSET 9
#define AWE_AFE____MicMuteTimeConstant_releaseTime_MASK 0x00000200
#define AWE_AFE____MicMuteTimeConstant_releaseTime_SIZE -1

// float attackCoeff - Internal coefficient realizing the attack time.
#define AWE_AFE____MicMuteTimeConstant_attackCoeff_OFFSET 10
#define AWE_AFE____MicMuteTimeConstant_attackCoeff_MASK 0x00000400
#define AWE_AFE____MicMuteTimeConstant_attackCoeff_SIZE -1

// float releaseCoeff - Internal coefficient realizing the release time.
#define AWE_AFE____MicMuteTimeConstant_releaseCoeff_OFFSET 11
#define AWE_AFE____MicMuteTimeConstant_releaseCoeff_MASK 0x00000800
#define AWE_AFE____MicMuteTimeConstant_releaseCoeff_SIZE -1

// float envStates[1] - Vector of sample-by-sample states of the 
//         envelope detectors; each column is the state for a channel.
#define AWE_AFE____MicMuteTimeConstant_envStates_OFFSET 12
#define AWE_AFE____MicMuteTimeConstant_envStates_MASK 0x00001000
#define AWE_AFE____MicMuteTimeConstant_envStates_SIZE 1


// ----------------------------------------------------------------------
// AFE.MicMuteThreshold [LogicCompareConst]
// Compares the value at the input pin against a stored value

#define AWE_AFE____MicMuteThreshold_ID 30007

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_AFE____MicMuteThreshold_profileTime_OFFSET 7
#define AWE_AFE____MicMuteThreshold_profileTime_MASK 0x00000080
#define AWE_AFE____MicMuteThreshold_profileTime_SIZE -1

// int compareType - Selects the type of comparison that is implemented 
//         by the module: Equal=0 NotEqual=1 lessThan=2 LessOrEqual=3 
//         greaterThan=4 GreaterOrEqual=5
// Default value: 2
// Range: 0 to 5
#define AWE_AFE____MicMuteThreshold_compareType_OFFSET 8
#define AWE_AFE____MicMuteThreshold_compareType_MASK 0x00000100
#define AWE_AFE____MicMuteThreshold_compareType_SIZE -1

// float constValue - Selects value against to compare against.  In 
//         linear units.
// Default value: -100
// Range: unrestricted
#define AWE_AFE____MicMuteThreshold_constValue_OFFSET 9
#define AWE_AFE____MicMuteThreshold_constValue_MASK 0x00000200
#define AWE_AFE____MicMuteThreshold_constValue_SIZE -1


// ----------------------------------------------------------------------
// AFE.LatencyControlSamps [Delay]
// Time delay in which the delay is specified in samples

#define AWE_AFE____LatencyControlSamps_ID 30001

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_AFE____LatencyControlSamps_profileTime_OFFSET 7
#define AWE_AFE____LatencyControlSamps_profileTime_MASK 0x00000080
#define AWE_AFE____LatencyControlSamps_profileTime_SIZE -1

// int maxDelay - Maximum delay, in samples. The size of the delay 
//         buffer is (maxDelay+1)*numChannels.
#define AWE_AFE____LatencyControlSamps_maxDelay_OFFSET 8
#define AWE_AFE____LatencyControlSamps_maxDelay_MASK 0x00000100
#define AWE_AFE____LatencyControlSamps_maxDelay_SIZE -1

// int currentDelay - Current delay.
// Default value: 0
// Range: 0 to 6400.  Step size = 1
#define AWE_AFE____LatencyControlSamps_currentDelay_OFFSET 9
#define AWE_AFE____LatencyControlSamps_currentDelay_MASK 0x00000200
#define AWE_AFE____LatencyControlSamps_currentDelay_SIZE -1

// int stateIndex - Index of the oldest state variable in the array of 
//         state variables.
#define AWE_AFE____LatencyControlSamps_stateIndex_OFFSET 10
#define AWE_AFE____LatencyControlSamps_stateIndex_MASK 0x00000400
#define AWE_AFE____LatencyControlSamps_stateIndex_SIZE -1

// int stateHeap - Heap in which to allocate memory.
#define AWE_AFE____LatencyControlSamps_stateHeap_OFFSET 11
#define AWE_AFE____LatencyControlSamps_stateHeap_MASK 0x00000800
#define AWE_AFE____LatencyControlSamps_stateHeap_SIZE -1

// float state[13316] - State variable array.
#define AWE_AFE____LatencyControlSamps_state_OFFSET 12
#define AWE_AFE____LatencyControlSamps_state_MASK 0x00001000
#define AWE_AFE____LatencyControlSamps_state_SIZE 13316


// ----------------------------------------------------------------------
// AFE.RefMuteTimeConstant [AGCAttackRelease]
// Multi-channel envelope detector with programmable attack and release 
// times

#define AWE_AFE____RefMuteTimeConstant_ID 32008

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_AFE____RefMuteTimeConstant_profileTime_OFFSET 7
#define AWE_AFE____RefMuteTimeConstant_profileTime_MASK 0x00000080
#define AWE_AFE____RefMuteTimeConstant_profileTime_SIZE -1

// float attackTime - Speed at which the detector reacts to increasing 
//         levels.
// Default value: 0.01
// Range: 0.01 to 1000
#define AWE_AFE____RefMuteTimeConstant_attackTime_OFFSET 8
#define AWE_AFE____RefMuteTimeConstant_attackTime_MASK 0x00000100
#define AWE_AFE____RefMuteTimeConstant_attackTime_SIZE -1

// float releaseTime - Speed at which the detector reacts to decreasing 
//         levels.
// Default value: 300
// Range: 0.01 to 1000
#define AWE_AFE____RefMuteTimeConstant_releaseTime_OFFSET 9
#define AWE_AFE____RefMuteTimeConstant_releaseTime_MASK 0x00000200
#define AWE_AFE____RefMuteTimeConstant_releaseTime_SIZE -1

// float attackCoeff - Internal coefficient realizing the attack time.
#define AWE_AFE____RefMuteTimeConstant_attackCoeff_OFFSET 10
#define AWE_AFE____RefMuteTimeConstant_attackCoeff_MASK 0x00000400
#define AWE_AFE____RefMuteTimeConstant_attackCoeff_SIZE -1

// float releaseCoeff - Internal coefficient realizing the release time.
#define AWE_AFE____RefMuteTimeConstant_releaseCoeff_OFFSET 11
#define AWE_AFE____RefMuteTimeConstant_releaseCoeff_MASK 0x00000800
#define AWE_AFE____RefMuteTimeConstant_releaseCoeff_SIZE -1

// float envStates[1] - Vector of sample-by-sample states of the 
//         envelope detectors; each column is the state for a channel.
#define AWE_AFE____RefMuteTimeConstant_envStates_OFFSET 12
#define AWE_AFE____RefMuteTimeConstant_envStates_MASK 0x00001000
#define AWE_AFE____RefMuteTimeConstant_envStates_SIZE 1


// ----------------------------------------------------------------------
// AFE.RefMuteThreshold [LogicCompareConst]
// Compares the value at the input pin against a stored value

#define AWE_AFE____RefMuteThreshold_ID 32001

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_AFE____RefMuteThreshold_profileTime_OFFSET 7
#define AWE_AFE____RefMuteThreshold_profileTime_MASK 0x00000080
#define AWE_AFE____RefMuteThreshold_profileTime_SIZE -1

// int compareType - Selects the type of comparison that is implemented 
//         by the module: Equal=0 NotEqual=1 lessThan=2 LessOrEqual=3 
//         greaterThan=4 GreaterOrEqual=5
// Default value: 2
// Range: 0 to 5
#define AWE_AFE____RefMuteThreshold_compareType_OFFSET 8
#define AWE_AFE____RefMuteThreshold_compareType_MASK 0x00000100
#define AWE_AFE____RefMuteThreshold_compareType_SIZE -1

// float constValue - Selects value against to compare against.  In 
//         linear units.
// Default value: -70
// Range: unrestricted
#define AWE_AFE____RefMuteThreshold_constValue_OFFSET 9
#define AWE_AFE____RefMuteThreshold_constValue_MASK 0x00000200
#define AWE_AFE____RefMuteThreshold_constValue_SIZE -1


// ----------------------------------------------------------------------
// AFE.VRState [SourceInt]
// Source buffer holding 1 wire of integer data

#define AWE_AFE____VRState_ID 30006

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_AFE____VRState_profileTime_OFFSET 7
#define AWE_AFE____VRState_profileTime_MASK 0x00000080
#define AWE_AFE____VRState_profileTime_SIZE -1

// int value[1] - Array of interleaved audio data
// Default value:
//     0
// Range: unrestricted
#define AWE_AFE____VRState_value_OFFSET 8
#define AWE_AFE____VRState_value_MASK 0x00000100
#define AWE_AFE____VRState_value_SIZE 1


// ----------------------------------------------------------------------
// AFE.AEC_Out_Delay [Delay]
// Time delay in which the delay is specified in samples

#define AWE_AFE____AEC_Out_Delay_ID 30004

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_AFE____AEC_Out_Delay_profileTime_OFFSET 7
#define AWE_AFE____AEC_Out_Delay_profileTime_MASK 0x00000080
#define AWE_AFE____AEC_Out_Delay_profileTime_SIZE -1

// int maxDelay - Maximum delay, in samples. The size of the delay 
//         buffer is (maxDelay+1)*numChannels.
#define AWE_AFE____AEC_Out_Delay_maxDelay_OFFSET 8
#define AWE_AFE____AEC_Out_Delay_maxDelay_MASK 0x00000100
#define AWE_AFE____AEC_Out_Delay_maxDelay_SIZE -1

// int currentDelay - Current delay.
// Default value: 0
// Range: 0 to 130.  Step size = 1
#define AWE_AFE____AEC_Out_Delay_currentDelay_OFFSET 9
#define AWE_AFE____AEC_Out_Delay_currentDelay_MASK 0x00000200
#define AWE_AFE____AEC_Out_Delay_currentDelay_SIZE -1

// int stateIndex - Index of the oldest state variable in the array of 
//         state variables.
#define AWE_AFE____AEC_Out_Delay_stateIndex_OFFSET 10
#define AWE_AFE____AEC_Out_Delay_stateIndex_MASK 0x00000400
#define AWE_AFE____AEC_Out_Delay_stateIndex_SIZE -1

// int stateHeap - Heap in which to allocate memory.
#define AWE_AFE____AEC_Out_Delay_stateHeap_OFFSET 11
#define AWE_AFE____AEC_Out_Delay_stateHeap_MASK 0x00000800
#define AWE_AFE____AEC_Out_Delay_stateHeap_SIZE -1

// float state[133] - State variable array.
#define AWE_AFE____AEC_Out_Delay_state_OFFSET 12
#define AWE_AFE____AEC_Out_Delay_state_MASK 0x00001000
#define AWE_AFE____AEC_Out_Delay_state_SIZE 133


// ----------------------------------------------------------------------
// AFE.FreqDomainProcessing.Direction [SinkInt]
// Copies the data at the input pin and stores it in an internal buffer

#define AWE_AFE____FreqDomainProcessing____Direction_ID 30009

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_AFE____FreqDomainProcessing____Direction_profileTime_OFFSET 7
#define AWE_AFE____FreqDomainProcessing____Direction_profileTime_MASK 0x00000080
#define AWE_AFE____FreqDomainProcessing____Direction_profileTime_SIZE -1

// int value[1] - Captured values
#define AWE_AFE____FreqDomainProcessing____Direction_value_OFFSET 8
#define AWE_AFE____FreqDomainProcessing____Direction_value_MASK 0x00000100
#define AWE_AFE____FreqDomainProcessing____Direction_value_SIZE 1


// ----------------------------------------------------------------------
// AFE.FreqDomainProcessing.AECReduction [Sink]
// Copies the data at the input pin and stores it in an internal buffer.

#define AWE_AECReduction_ID 30010

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_AECReduction_profileTime_OFFSET 7
#define AWE_AECReduction_profileTime_MASK 0x00000080
#define AWE_AECReduction_profileTime_SIZE -1

// int enable - To enable or disable the plotting.
// Default value: 0
// Range: unrestricted
#define AWE_AECReduction_enable_OFFSET 8
#define AWE_AECReduction_enable_MASK 0x00000100
#define AWE_AECReduction_enable_SIZE -1

// float value[625] - Captured values.
#define AWE_AECReduction_value_OFFSET 9
#define AWE_AECReduction_value_MASK 0x00000200
#define AWE_AECReduction_value_SIZE 625

// float yRange[2] - Y-axis range.
// Default value:
//     -5  5
// Range: unrestricted
#define AWE_AECReduction_yRange_OFFSET 10
#define AWE_AECReduction_yRange_MASK 0x00000400
#define AWE_AECReduction_yRange_SIZE 2


// ----------------------------------------------------------------------
// AFE.FreqDomainProcessing.AEC_Perf_Sink [Sink]
// Copies the data at the input pin and stores it in an internal buffer.

#define AWE_AFE____FreqDomainProcessing____AEC_Perf_Sink_ID 30014

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_AFE____FreqDomainProcessing____AEC_Perf_Sink_profileTime_OFFSET 7
#define AWE_AFE____FreqDomainProcessing____AEC_Perf_Sink_profileTime_MASK 0x00000080
#define AWE_AFE____FreqDomainProcessing____AEC_Perf_Sink_profileTime_SIZE -1

// int enable - To enable or disable the plotting.
// Default value: 0
// Range: unrestricted
#define AWE_AFE____FreqDomainProcessing____AEC_Perf_Sink_enable_OFFSET 8
#define AWE_AFE____FreqDomainProcessing____AEC_Perf_Sink_enable_MASK 0x00000100
#define AWE_AFE____FreqDomainProcessing____AEC_Perf_Sink_enable_SIZE -1

// float value[1250] - Captured values.
#define AWE_AFE____FreqDomainProcessing____AEC_Perf_Sink_value_OFFSET 9
#define AWE_AFE____FreqDomainProcessing____AEC_Perf_Sink_value_MASK 0x00000200
#define AWE_AFE____FreqDomainProcessing____AEC_Perf_Sink_value_SIZE 1250

// float yRange[2] - Y-axis range.
// Default value:
//     -5  5
// Range: unrestricted
#define AWE_AFE____FreqDomainProcessing____AEC_Perf_Sink_yRange_OFFSET 10
#define AWE_AFE____FreqDomainProcessing____AEC_Perf_Sink_yRange_MASK 0x00000400
#define AWE_AFE____FreqDomainProcessing____AEC_Perf_Sink_yRange_SIZE 2


// ----------------------------------------------------------------------
// AFE.FreqDomainProcessing.AICReduction [Sink]
// Copies the data at the input pin and stores it in an internal buffer.

#define AWE_AICReduction_ID 30035

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_AICReduction_profileTime_OFFSET 7
#define AWE_AICReduction_profileTime_MASK 0x00000080
#define AWE_AICReduction_profileTime_SIZE -1

// int enable - To enable or disable the plotting.
// Default value: 0
// Range: unrestricted
#define AWE_AICReduction_enable_OFFSET 8
#define AWE_AICReduction_enable_MASK 0x00000100
#define AWE_AICReduction_enable_SIZE -1

// float value[625] - Captured values.
#define AWE_AICReduction_value_OFFSET 9
#define AWE_AICReduction_value_MASK 0x00000200
#define AWE_AICReduction_value_SIZE 625

// float yRange[2] - Y-axis range.
// Default value:
//     -5  5
// Range: unrestricted
#define AWE_AICReduction_yRange_OFFSET 10
#define AWE_AICReduction_yRange_MASK 0x00000400
#define AWE_AICReduction_yRange_SIZE 2


// ----------------------------------------------------------------------
// AFE.OutputMeter [Meter]
// Peak and RMS meter module

#define AWE_AFE____OutputMeter_ID 30005

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_AFE____OutputMeter_profileTime_OFFSET 7
#define AWE_AFE____OutputMeter_profileTime_MASK 0x00000080
#define AWE_AFE____OutputMeter_profileTime_SIZE -1

// int meterType - Operating mode of the meter. Selects between peak and 
//         RMS calculations. See the discussion section for more details.
// Default value: 18
// Range: unrestricted
#define AWE_AFE____OutputMeter_meterType_OFFSET 8
#define AWE_AFE____OutputMeter_meterType_MASK 0x00000100
#define AWE_AFE____OutputMeter_meterType_SIZE -1

// float attackTime - Attack time of the meter. Specifies how quickly 
//         the meter value rises.
#define AWE_AFE____OutputMeter_attackTime_OFFSET 9
#define AWE_AFE____OutputMeter_attackTime_MASK 0x00000200
#define AWE_AFE____OutputMeter_attackTime_SIZE -1

// float releaseTime - Release time of the meter. Specifies how quickly 
//         the meter decays.
#define AWE_AFE____OutputMeter_releaseTime_OFFSET 10
#define AWE_AFE____OutputMeter_releaseTime_MASK 0x00000400
#define AWE_AFE____OutputMeter_releaseTime_SIZE -1

// float attackCoeff - Internal coefficient that realizes the attack 
//         time.
#define AWE_AFE____OutputMeter_attackCoeff_OFFSET 11
#define AWE_AFE____OutputMeter_attackCoeff_MASK 0x00000800
#define AWE_AFE____OutputMeter_attackCoeff_SIZE -1

// float releaseCoeff - Internal coefficient that realizes the release 
//         time.
#define AWE_AFE____OutputMeter_releaseCoeff_OFFSET 12
#define AWE_AFE____OutputMeter_releaseCoeff_MASK 0x00001000
#define AWE_AFE____OutputMeter_releaseCoeff_SIZE -1

// float value[6] - Array of meter output values, one per channel.
#define AWE_AFE____OutputMeter_value_OFFSET 13
#define AWE_AFE____OutputMeter_value_MASK 0x00002000
#define AWE_AFE____OutputMeter_value_SIZE 6


// ----------------------------------------------------------------------
// AFE.awb_vinfo [SourceInt]
// Source buffer holding 1 wire of integer data

#define AWE_AFE____awb_vinfo_ID 32767

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_AFE____awb_vinfo_profileTime_OFFSET 7
#define AWE_AFE____awb_vinfo_profileTime_MASK 0x00000080
#define AWE_AFE____awb_vinfo_profileTime_SIZE -1

// int value[4] - Array of interleaved audio data
// Default value:
//     1
//     2
//     5
//     1
// Range: unrestricted
#define AWE_AFE____awb_vinfo_value_OFFSET 8
#define AWE_AFE____awb_vinfo_value_MASK 0x00000100
#define AWE_AFE____awb_vinfo_value_SIZE 4


// ----------------------------------------------------------------------
// AFE.BypassAEC [BooleanSource]
// Source buffer holding 1 wire of data

#define AWE_AFE____BypassAEC_ID 32000

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_AFE____BypassAEC_profileTime_OFFSET 7
#define AWE_AFE____BypassAEC_profileTime_MASK 0x00000080
#define AWE_AFE____BypassAEC_profileTime_SIZE -1

// int value[1] - Array of interleaved audio real data.
// Default value:
//     0
// Range: 0 to 1.  Step size = 1
#define AWE_AFE____BypassAEC_value_OFFSET 8
#define AWE_AFE____BypassAEC_value_MASK 0x00000100
#define AWE_AFE____BypassAEC_value_SIZE 1


// ----------------------------------------------------------------------
// AFE.AECBypassState [BooleanSink]
// Sink module holding Boolean data

#define AWE_AFE____AECBypassState_ID 32005

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_AFE____AECBypassState_profileTime_OFFSET 7
#define AWE_AFE____AECBypassState_profileTime_MASK 0x00000080
#define AWE_AFE____AECBypassState_profileTime_SIZE -1

// int value[1] - Boolean array of response to the signals on the wires
#define AWE_AFE____AECBypassState_value_OFFSET 8
#define AWE_AFE____AECBypassState_value_MASK 0x00000100
#define AWE_AFE____AECBypassState_value_SIZE 1


// ----------------------------------------------------------------------
// AFE.RecordASR [BooleanSource]
// Source buffer holding 1 wire of data

#define AWE_AFE____RecordASR_ID 31052

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_AFE____RecordASR_profileTime_OFFSET 7
#define AWE_AFE____RecordASR_profileTime_MASK 0x00000080
#define AWE_AFE____RecordASR_profileTime_SIZE -1

// int value[1] - Array of interleaved audio real data.
// Default value:
//     0
// Range: 0 to 1.  Step size = 1
#define AWE_AFE____RecordASR_value_OFFSET 8
#define AWE_AFE____RecordASR_value_MASK 0x00000100
#define AWE_AFE____RecordASR_value_SIZE 1


// ----------------------------------------------------------------------
// AcmAvsState [SourceInt]
// Source buffer holding 1 wire of integer data

#define AWE_AcmAvsState_ID 32406

// int profileTime - 24.8 fixed point filtered execution time. Must be pumped 1000 times to get to within .1% accuracy
#define AWE_AcmAvsState_profileTime_OFFSET 7
#define AWE_AcmAvsState_profileTime_MASK 0x00000080
#define AWE_AcmAvsState_profileTime_SIZE -1

// int value[1] - Array of interleaved audio data
// Default value:
//     0
// Range: unrestricted
#define AWE_AcmAvsState_value_OFFSET 8
#define AWE_AcmAvsState_value_MASK 0x00000100
#define AWE_AcmAvsState_value_SIZE 1



#endif // BELKINLINKPLAY_VUI_HFP_V1P0_CONTROLINTERFACE_AWE6_H

