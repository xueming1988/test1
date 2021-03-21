/****************************************************************************
 *
 *		Target Tuning Symbol File
 *		-------------------------
 *
 * 		This file is populated with symbol information only for modules
 *		that have an object ID of 30000 or greater assigned.
 *
 *          Generated on:  22-Apr-2020 13:01:01
 *
 ***************************************************************************/

#ifndef LP_EXTERNAL_WW_TWOTRIGGER_TWOMIC_V0P2F_CONTROLINTERFACE_AWE6_H
#define LP_EXTERNAL_WW_TWOTRIGGER_TWOMIC_V0P2F_CONTROLINTERFACE_AWE6_H

/* The current layout contains no controllable modules.
   To access a module via the control API, it must have an assigned objectID that's 30000 or greater.
   To assign an ObjectID, right-click a module, open Properties, then enter a number under the Build tab. */
// ----------------------------------------------------------------------
// LatencyControlSamps [Delay]
// Time delay in which the delay is specified in samples

#define AWE_LatencyControlSamps_ID 30001

// int maxDelay - Maximum delay, in samples. The size of the delay 
//         buffer is (maxDelay+1)*numChannels.
#define AWE_LatencyControlSamps_maxDelay_OFFSET 8
#define AWE_LatencyControlSamps_maxDelay_MASK 0x00000100
#define AWE_LatencyControlSamps_maxDelay_SIZE -1

// int currentDelay - Current delay.
// Default value: 0
// Range: 0 to 6400.  Step size = 1
#define AWE_LatencyControlSamps_currentDelay_OFFSET 9
#define AWE_LatencyControlSamps_currentDelay_MASK 0x00000200
#define AWE_LatencyControlSamps_currentDelay_SIZE -1

// int stateIndex - Index of the oldest state variable in the array of 
//         state variables.
#define AWE_LatencyControlSamps_stateIndex_OFFSET 10
#define AWE_LatencyControlSamps_stateIndex_MASK 0x00000400
#define AWE_LatencyControlSamps_stateIndex_SIZE -1

// int stateHeap - Heap in which to allocate memory.
#define AWE_LatencyControlSamps_stateHeap_OFFSET 11
#define AWE_LatencyControlSamps_stateHeap_MASK 0x00000800
#define AWE_LatencyControlSamps_stateHeap_SIZE -1

// float state[13316] - State variable array.
#define AWE_LatencyControlSamps_state_OFFSET 12
#define AWE_LatencyControlSamps_state_MASK 0x00001000
#define AWE_LatencyControlSamps_state_SIZE 13316


// ----------------------------------------------------------------------
// TrimGains [ScalerNV2]
// General purpose scaler with separate gains per channel

#define AWE_MicTrimGains_ID 30015

// float masterGain - Overall gain to apply.
// Default value: -9
// Range: -24 to 24
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
// InputMeter [Meter]
// Peak and RMS meter module

#define AWE_InputMeter_ID 30000

// int meterType - Operating mode of the meter. Selects between peak and 
//         RMS calculations. See the discussion section for more details.
// Default value: 18
// Range: unrestricted
#define AWE_InputMeter_meterType_OFFSET 8
#define AWE_InputMeter_meterType_MASK 0x00000100
#define AWE_InputMeter_meterType_SIZE -1

// float attackTime - Attack time of the meter. Specifies how quickly 
//         the meter value rises.
#define AWE_InputMeter_attackTime_OFFSET 9
#define AWE_InputMeter_attackTime_MASK 0x00000200
#define AWE_InputMeter_attackTime_SIZE -1

// float releaseTime - Release time of the meter. Specifies how quickly 
//         the meter decays.
#define AWE_InputMeter_releaseTime_OFFSET 10
#define AWE_InputMeter_releaseTime_MASK 0x00000400
#define AWE_InputMeter_releaseTime_SIZE -1

// float attackCoeff - Internal coefficient that realizes the attack 
//         time.
#define AWE_InputMeter_attackCoeff_OFFSET 11
#define AWE_InputMeter_attackCoeff_MASK 0x00000800
#define AWE_InputMeter_attackCoeff_SIZE -1

// float releaseCoeff - Internal coefficient that realizes the release 
//         time.
#define AWE_InputMeter_releaseCoeff_OFFSET 12
#define AWE_InputMeter_releaseCoeff_MASK 0x00001000
#define AWE_InputMeter_releaseCoeff_SIZE -1

// float value[10] - Array of meter output values, one per channel.
#define AWE_InputMeter_value_OFFSET 13
#define AWE_InputMeter_value_MASK 0x00002000
#define AWE_InputMeter_value_SIZE 10


// ----------------------------------------------------------------------
// RefMuteTimeConstant [AGCAttackRelease]
// Multi-channel envelope detector with programmable attack and release 
// times

#define AWE_RefMuteTimeConstant_ID 32008

// float attackTime - Speed at which the detector reacts to increasing 
//         levels.
// Default value: 0.01
// Range: 0.01 to 1000
#define AWE_RefMuteTimeConstant_attackTime_OFFSET 8
#define AWE_RefMuteTimeConstant_attackTime_MASK 0x00000100
#define AWE_RefMuteTimeConstant_attackTime_SIZE -1

// float releaseTime - Speed at which the detector reacts to decreasing 
//         levels.
// Default value: 300
// Range: 0.01 to 1000
#define AWE_RefMuteTimeConstant_releaseTime_OFFSET 9
#define AWE_RefMuteTimeConstant_releaseTime_MASK 0x00000200
#define AWE_RefMuteTimeConstant_releaseTime_SIZE -1

// float attackCoeff - Internal coefficient realizing the attack time.
#define AWE_RefMuteTimeConstant_attackCoeff_OFFSET 10
#define AWE_RefMuteTimeConstant_attackCoeff_MASK 0x00000400
#define AWE_RefMuteTimeConstant_attackCoeff_SIZE -1

// float releaseCoeff - Internal coefficient realizing the release time.
#define AWE_RefMuteTimeConstant_releaseCoeff_OFFSET 11
#define AWE_RefMuteTimeConstant_releaseCoeff_MASK 0x00000800
#define AWE_RefMuteTimeConstant_releaseCoeff_SIZE -1

// float envStates[1] - Vector of sample-by-sample states of the 
//         envelope detectors; each column is the state for a channel.
#define AWE_RefMuteTimeConstant_envStates_OFFSET 12
#define AWE_RefMuteTimeConstant_envStates_MASK 0x00001000
#define AWE_RefMuteTimeConstant_envStates_SIZE 1


// ----------------------------------------------------------------------
// RefMuteThreshold [LogicCompareConst]
// Compares the value at the input pin against a stored value

#define AWE_RefMuteThreshold_ID 32001

// int compareType - Selects the type of comparison that is implemented 
//         by the module: Equal=0 NotEqual=1 lessThan=2 LessOrEqual=3 
//         greaterThan=4 GreaterOrEqual=5
// Default value: 2
// Range: 0 to 5
#define AWE_RefMuteThreshold_compareType_OFFSET 8
#define AWE_RefMuteThreshold_compareType_MASK 0x00000100
#define AWE_RefMuteThreshold_compareType_SIZE -1

// float constValue - Selects value against to compare against.  In 
//         linear units.
// Default value: -70
// Range: unrestricted
#define AWE_RefMuteThreshold_constValue_OFFSET 9
#define AWE_RefMuteThreshold_constValue_MASK 0x00000200
#define AWE_RefMuteThreshold_constValue_SIZE -1


// ----------------------------------------------------------------------
// MicHPF [SecondOrderFilterSmoothed]
// General 2nd order filter designer with smoothing

#define AWE_MicHPF_ID 32006

// int filterType - Selects the type of filter that is implemented by 
//         the module: Bypass=0, Gain=1, Butter1stLPF=2, Butter2ndLPF=3, 
//         Butter1stHPF=4, Butter2ndHPF=5, Allpass1st=6, Allpass2nd=7, 
//         Shelf2ndLow=8, Shelf2ndLowQ=9, Shelf2ndHigh=10, Shelf2ndHighQ=11, 
//         PeakEQ=12, Notch=13, Bandpass=14, Bessel1stLPF=15, Bessel1stHPF=16, 
//         AsymShelf1stLow=17, AsymShelf1stHigh=18, SymShelf1stLow=19, 
//         SymShelf1stHigh=20, VariableQLPF=21, VariableQHPF=22.
// Default value: 5
// Range: 0 to 22
#define AWE_MicHPF_filterType_OFFSET 8
#define AWE_MicHPF_filterType_MASK 0x00000100
#define AWE_MicHPF_filterType_SIZE -1

// float freq - Cutoff frequency of the filter, in Hz.
// Default value: 100
// Range: 10 to 7920.  Step size = 0.1
#define AWE_MicHPF_freq_OFFSET 9
#define AWE_MicHPF_freq_MASK 0x00000200
#define AWE_MicHPF_freq_SIZE -1

// float gain - Amount of boost or cut to apply, in dB if applicable.
// Default value: 0
// Range: -24 to 24.  Step size = 0.1
#define AWE_MicHPF_gain_OFFSET 10
#define AWE_MicHPF_gain_MASK 0x00000400
#define AWE_MicHPF_gain_SIZE -1

// float Q - Specifies the Q of the filter, if applicable.
// Default value: 1
// Range: 0 to 20.  Step size = 0.1
#define AWE_MicHPF_Q_OFFSET 11
#define AWE_MicHPF_Q_MASK 0x00000800
#define AWE_MicHPF_Q_SIZE -1

// float smoothingTime - Time constant of the smoothing process.
// Default value: 10
// Range: 0 to 1000.  Step size = 1
#define AWE_MicHPF_smoothingTime_OFFSET 12
#define AWE_MicHPF_smoothingTime_MASK 0x00001000
#define AWE_MicHPF_smoothingTime_SIZE -1

// int updateActive - Specifies whether the filter coefficients are 
//         updating (=1) or fixed (=0).
// Default value: 1
// Range: 0 to 1
#define AWE_MicHPF_updateActive_OFFSET 13
#define AWE_MicHPF_updateActive_MASK 0x00002000
#define AWE_MicHPF_updateActive_SIZE -1

// float b0 - Desired first numerator coefficient.
#define AWE_MicHPF_b0_OFFSET 14
#define AWE_MicHPF_b0_MASK 0x00004000
#define AWE_MicHPF_b0_SIZE -1

// float b1 - Desired second numerator coefficient.
#define AWE_MicHPF_b1_OFFSET 15
#define AWE_MicHPF_b1_MASK 0x00008000
#define AWE_MicHPF_b1_SIZE -1

// float b2 - Desired third numerator coefficient.
#define AWE_MicHPF_b2_OFFSET 16
#define AWE_MicHPF_b2_MASK 0x00010000
#define AWE_MicHPF_b2_SIZE -1

// float a1 - Desired second denominator coefficient.
#define AWE_MicHPF_a1_OFFSET 17
#define AWE_MicHPF_a1_MASK 0x00020000
#define AWE_MicHPF_a1_SIZE -1

// float a2 - Desired third denominator coefficient.
#define AWE_MicHPF_a2_OFFSET 18
#define AWE_MicHPF_a2_MASK 0x00040000
#define AWE_MicHPF_a2_SIZE -1

// float current_b0 - Instantaneous first numerator coefficient.
#define AWE_MicHPF_current_b0_OFFSET 19
#define AWE_MicHPF_current_b0_MASK 0x00080000
#define AWE_MicHPF_current_b0_SIZE -1

// float current_b1 - Instantaneous second numerator coefficient.
#define AWE_MicHPF_current_b1_OFFSET 20
#define AWE_MicHPF_current_b1_MASK 0x00100000
#define AWE_MicHPF_current_b1_SIZE -1

// float current_b2 - Instantaneous third numerator coefficient.
#define AWE_MicHPF_current_b2_OFFSET 21
#define AWE_MicHPF_current_b2_MASK 0x00200000
#define AWE_MicHPF_current_b2_SIZE -1

// float current_a1 - Instantaneous second denominator coefficient.
#define AWE_MicHPF_current_a1_OFFSET 22
#define AWE_MicHPF_current_a1_MASK 0x00400000
#define AWE_MicHPF_current_a1_SIZE -1

// float current_a2 - Instantaneous third denominator coefficient.
#define AWE_MicHPF_current_a2_OFFSET 23
#define AWE_MicHPF_current_a2_MASK 0x00800000
#define AWE_MicHPF_current_a2_SIZE -1

// float smoothingCoeff - Smoothing coefficient. This is computed based 
//         on the smoothingTime, sample rate, and block size of the module.
#define AWE_MicHPF_smoothingCoeff_OFFSET 24
#define AWE_MicHPF_smoothingCoeff_MASK 0x01000000
#define AWE_MicHPF_smoothingCoeff_SIZE -1

// float state[4] - State variables. 2 per channel.
#define AWE_MicHPF_state_OFFSET 25
#define AWE_MicHPF_state_MASK 0x02000000
#define AWE_MicHPF_state_SIZE 4


// ----------------------------------------------------------------------
// MicMuteTimeConstant [AGCAttackRelease]
// Multi-channel envelope detector with programmable attack and release 
// times

#define AWE_MicMuteTimeConstant_ID 32009

// float attackTime - Speed at which the detector reacts to increasing 
//         levels.
// Default value: 0.01
// Range: 0.01 to 1000
#define AWE_MicMuteTimeConstant_attackTime_OFFSET 8
#define AWE_MicMuteTimeConstant_attackTime_MASK 0x00000100
#define AWE_MicMuteTimeConstant_attackTime_SIZE -1

// float releaseTime - Speed at which the detector reacts to decreasing 
//         levels.
// Default value: 250
// Range: 0.01 to 1000
#define AWE_MicMuteTimeConstant_releaseTime_OFFSET 9
#define AWE_MicMuteTimeConstant_releaseTime_MASK 0x00000200
#define AWE_MicMuteTimeConstant_releaseTime_SIZE -1

// float attackCoeff - Internal coefficient realizing the attack time.
#define AWE_MicMuteTimeConstant_attackCoeff_OFFSET 10
#define AWE_MicMuteTimeConstant_attackCoeff_MASK 0x00000400
#define AWE_MicMuteTimeConstant_attackCoeff_SIZE -1

// float releaseCoeff - Internal coefficient realizing the release time.
#define AWE_MicMuteTimeConstant_releaseCoeff_OFFSET 11
#define AWE_MicMuteTimeConstant_releaseCoeff_MASK 0x00000800
#define AWE_MicMuteTimeConstant_releaseCoeff_SIZE -1

// float envStates[1] - Vector of sample-by-sample states of the 
//         envelope detectors; each column is the state for a channel.
#define AWE_MicMuteTimeConstant_envStates_OFFSET 12
#define AWE_MicMuteTimeConstant_envStates_MASK 0x00001000
#define AWE_MicMuteTimeConstant_envStates_SIZE 1


// ----------------------------------------------------------------------
// MicMuteThreshold [LogicCompareConst]
// Compares the value at the input pin against a stored value

#define AWE_MicMuteThreshold_ID 30002

// int compareType - Selects the type of comparison that is implemented 
//         by the module: Equal=0 NotEqual=1 lessThan=2 LessOrEqual=3 
//         greaterThan=4 GreaterOrEqual=5
// Default value: 2
// Range: 0 to 5
#define AWE_MicMuteThreshold_compareType_OFFSET 8
#define AWE_MicMuteThreshold_compareType_MASK 0x00000100
#define AWE_MicMuteThreshold_compareType_SIZE -1

// float constValue - Selects value against to compare against.  In 
//         linear units.
// Default value: -100
// Range: unrestricted
#define AWE_MicMuteThreshold_constValue_OFFSET 9
#define AWE_MicMuteThreshold_constValue_MASK 0x00000200
#define AWE_MicMuteThreshold_constValue_SIZE -1


// ----------------------------------------------------------------------
// VRState [SourceInt]
// Source buffer holding 1 wire of integer data

#define AWE_VRState_ID 30006

// int value[1] - Array of interleaved audio data
// Default value:
//     0
// Range: unrestricted
#define AWE_VRState_value_OFFSET 8
#define AWE_VRState_value_MASK 0x00000100
#define AWE_VRState_value_SIZE 1


// ----------------------------------------------------------------------
// AEC_Out_Delay [Delay]
// Time delay in which the delay is specified in samples

#define AWE_AEC_Out_Delay_ID 30004

// int maxDelay - Maximum delay, in samples. The size of the delay 
//         buffer is (maxDelay+1)*numChannels.
#define AWE_AEC_Out_Delay_maxDelay_OFFSET 8
#define AWE_AEC_Out_Delay_maxDelay_MASK 0x00000100
#define AWE_AEC_Out_Delay_maxDelay_SIZE -1

// int currentDelay - Current delay.
// Default value: 0
// Range: 0 to 130.  Step size = 1
#define AWE_AEC_Out_Delay_currentDelay_OFFSET 9
#define AWE_AEC_Out_Delay_currentDelay_MASK 0x00000200
#define AWE_AEC_Out_Delay_currentDelay_SIZE -1

// int stateIndex - Index of the oldest state variable in the array of 
//         state variables.
#define AWE_AEC_Out_Delay_stateIndex_OFFSET 10
#define AWE_AEC_Out_Delay_stateIndex_MASK 0x00000400
#define AWE_AEC_Out_Delay_stateIndex_SIZE -1

// int stateHeap - Heap in which to allocate memory.
#define AWE_AEC_Out_Delay_stateHeap_OFFSET 11
#define AWE_AEC_Out_Delay_stateHeap_MASK 0x00000800
#define AWE_AEC_Out_Delay_stateHeap_SIZE -1

// float state[133] - State variable array.
#define AWE_AEC_Out_Delay_state_OFFSET 12
#define AWE_AEC_Out_Delay_state_MASK 0x00001000
#define AWE_AEC_Out_Delay_state_SIZE 133


// ----------------------------------------------------------------------
// FreqDomainProcessing.Direction [SinkInt]
// Copies the data at the input pin and stores it in an internal buffer

#define AWE_FreqDomainProcessing____Direction_ID 30009

// int value[1] - Captured values
#define AWE_FreqDomainProcessing____Direction_value_OFFSET 8
#define AWE_FreqDomainProcessing____Direction_value_MASK 0x00000100
#define AWE_FreqDomainProcessing____Direction_value_SIZE 1


// ----------------------------------------------------------------------
// FreqDomainProcessing.AICReduction [Sink]
// Copies the data at the input pin and stores it in an internal buffer.

#define AWE_AICReduction_ID 30035

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
// FreqDomainProcessing.AECReduction [Sink]
// Copies the data at the input pin and stores it in an internal buffer.

#define AWE_AECReduction_ID 30010

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
// FreqDomainProcessing.AEC_Perf_Sink [Sink]
// Copies the data at the input pin and stores it in an internal buffer.

#define AWE_FreqDomainProcessing____AEC_Perf_Sink_ID 30014

// int enable - To enable or disable the plotting.
// Default value: 0
// Range: unrestricted
#define AWE_FreqDomainProcessing____AEC_Perf_Sink_enable_OFFSET 8
#define AWE_FreqDomainProcessing____AEC_Perf_Sink_enable_MASK 0x00000100
#define AWE_FreqDomainProcessing____AEC_Perf_Sink_enable_SIZE -1

// float value[1250] - Captured values.
#define AWE_FreqDomainProcessing____AEC_Perf_Sink_value_OFFSET 9
#define AWE_FreqDomainProcessing____AEC_Perf_Sink_value_MASK 0x00000200
#define AWE_FreqDomainProcessing____AEC_Perf_Sink_value_SIZE 1250

// float yRange[2] - Y-axis range.
// Default value:
//     -5  5
// Range: unrestricted
#define AWE_FreqDomainProcessing____AEC_Perf_Sink_yRange_OFFSET 10
#define AWE_FreqDomainProcessing____AEC_Perf_Sink_yRange_MASK 0x00000400
#define AWE_FreqDomainProcessing____AEC_Perf_Sink_yRange_SIZE 2


// ----------------------------------------------------------------------
// OutputMeter [Meter]
// Peak and RMS meter module

#define AWE_OutputMeter_ID 30005

// int meterType - Operating mode of the meter. Selects between peak and 
//         RMS calculations. See the discussion section for more details.
// Default value: 18
// Range: unrestricted
#define AWE_OutputMeter_meterType_OFFSET 8
#define AWE_OutputMeter_meterType_MASK 0x00000100
#define AWE_OutputMeter_meterType_SIZE -1

// float attackTime - Attack time of the meter. Specifies how quickly 
//         the meter value rises.
#define AWE_OutputMeter_attackTime_OFFSET 9
#define AWE_OutputMeter_attackTime_MASK 0x00000200
#define AWE_OutputMeter_attackTime_SIZE -1

// float releaseTime - Release time of the meter. Specifies how quickly 
//         the meter decays.
#define AWE_OutputMeter_releaseTime_OFFSET 10
#define AWE_OutputMeter_releaseTime_MASK 0x00000400
#define AWE_OutputMeter_releaseTime_SIZE -1

// float attackCoeff - Internal coefficient that realizes the attack 
//         time.
#define AWE_OutputMeter_attackCoeff_OFFSET 11
#define AWE_OutputMeter_attackCoeff_MASK 0x00000800
#define AWE_OutputMeter_attackCoeff_SIZE -1

// float releaseCoeff - Internal coefficient that realizes the release 
//         time.
#define AWE_OutputMeter_releaseCoeff_OFFSET 12
#define AWE_OutputMeter_releaseCoeff_MASK 0x00001000
#define AWE_OutputMeter_releaseCoeff_SIZE -1

// float value[6] - Array of meter output values, one per channel.
#define AWE_OutputMeter_value_OFFSET 13
#define AWE_OutputMeter_value_MASK 0x00002000
#define AWE_OutputMeter_value_SIZE 6


// ----------------------------------------------------------------------
// awb_vinfo [SourceInt]
// Source buffer holding 1 wire of integer data

#define AWE_awb_vinfo_ID 32767

// int value[4] - Array of interleaved audio data
// Default value:
//     1
//     2
//     5
//     1
// Range: unrestricted
#define AWE_awb_vinfo_value_OFFSET 8
#define AWE_awb_vinfo_value_MASK 0x00000100
#define AWE_awb_vinfo_value_SIZE 4


// ----------------------------------------------------------------------
// BypassAEC [BooleanSource]
// Source buffer holding 1 wire of data

#define AWE_BypassAEC_ID 32000

// int value[1] - Array of interleaved audio real data.
// Default value:
//     0
// Range: 0 to 1.  Step size = 1
#define AWE_BypassAEC_value_OFFSET 8
#define AWE_BypassAEC_value_MASK 0x00000100
#define AWE_BypassAEC_value_SIZE 1


// ----------------------------------------------------------------------
// AECBypassState [BooleanSink]
// Sink module holding Boolean data

#define AWE_AECBypassState_ID 32005

// int value[1] - Boolean array of response to the signals on the wires
#define AWE_AECBypassState_value_OFFSET 8
#define AWE_AECBypassState_value_MASK 0x00000100
#define AWE_AECBypassState_value_SIZE 1



#endif // LP_EXTERNAL_WW_TWOTRIGGER_TWOMIC_V0P2F_CONTROLINTERFACE_AWE6_H

