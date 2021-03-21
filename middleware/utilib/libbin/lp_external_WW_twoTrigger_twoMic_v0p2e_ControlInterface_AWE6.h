/****************************************************************************
 *
 *		Target Tuning Symbol File
 *		-------------------------
 *
 * 		This file is populated with symbol information only for modules
 *		that have an object ID of 30000 or greater assigned.
 *
 *          Generated on:  12-Mar-2020 20:11:55
 *
 ***************************************************************************/

#ifndef LP_EXTERNAL_WW_TWOTRIGGER_TWOMIC_V0P2E_CONTROLINTERFACE_AWE6_H
#define LP_EXTERNAL_WW_TWOTRIGGER_TWOMIC_V0P2E_CONTROLINTERFACE_AWE6_H

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



#endif // LP_EXTERNAL_WW_TWOTRIGGER_TWOMIC_V0P2E_CONTROLINTERFACE_AWE6_H

