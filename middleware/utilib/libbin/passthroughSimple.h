/****************************************************************************
 *
 *		Target Tuning Symbol File
 *		-------------------------
 *
 *          Generated on:  28-Apr-2017 11:58:41
 *
 ***************************************************************************/

#ifndef PASSTHROUGHSIMPLE_H
#define PASSTHROUGHSIMPLE_H

// ----------------------------------------------------------------------
// SYS_toFloat [TypeConversion]
// Converts between numeric data types

#define AWE_SYS_toFloat_ID 

// int inputType - Specifies the dataType of the input
// Default value: 1
// Range: 0 to 2
#define AWE_SYS_toFloat_inputType_OFFSET 8
#define AWE_SYS_toFloat_inputType_MASK 0x00000100
#define AWE_SYS_toFloat_inputType_SIZE -1

// int outputType - Specifies the dataType of the output
// Default value: 0
// Range: 0 to 2
#define AWE_SYS_toFloat_outputType_OFFSET 9
#define AWE_SYS_toFloat_outputType_MASK 0x00000200
#define AWE_SYS_toFloat_outputType_SIZE -1


// ----------------------------------------------------------------------
// Scaler1 [ScalerV2]
// General purpose scaler with a single gain

#define AWE_Scaler1_ID 30000

// float gain - Gain in either linear or dB units.
// Default value: 0
// Range: -40 to 40
#define AWE_Scaler1_gain_OFFSET 8
#define AWE_Scaler1_gain_MASK 0x00000100
#define AWE_Scaler1_gain_SIZE -1

// float smoothingTime - Time constant of the smoothing process (0 = 
//         unsmoothed).
// Default value: 0
// Range: 0 to 1000
#define AWE_Scaler1_smoothingTime_OFFSET 9
#define AWE_Scaler1_smoothingTime_MASK 0x00000200
#define AWE_Scaler1_smoothingTime_SIZE -1

// int isDB - Selects between linear (=0) and dB (=1) operation
// Default value: 1
// Range: 0 to 1
#define AWE_Scaler1_isDB_OFFSET 10
#define AWE_Scaler1_isDB_MASK 0x00000400
#define AWE_Scaler1_isDB_SIZE -1

// float targetGain - Target gain in linear units.
#define AWE_Scaler1_targetGain_OFFSET 11
#define AWE_Scaler1_targetGain_MASK 0x00000800
#define AWE_Scaler1_targetGain_SIZE -1

// float currentGain - Instantaneous gain applied by the module.
#define AWE_Scaler1_currentGain_OFFSET 12
#define AWE_Scaler1_currentGain_MASK 0x00001000
#define AWE_Scaler1_currentGain_SIZE -1

// float smoothingCoeff - Smoothing coefficient.
#define AWE_Scaler1_smoothingCoeff_OFFSET 13
#define AWE_Scaler1_smoothingCoeff_MASK 0x00002000
#define AWE_Scaler1_smoothingCoeff_SIZE -1


// ----------------------------------------------------------------------
// Meter1 [Meter]
// Peak and RMS meter module

#define AWE_Meter1_ID 30002

// int meterType - Operating mode of the meter. Selects between peak and 
//         RMS calculations. See the discussion section for more details.
// Default value: 18
// Range: unrestricted
#define AWE_Meter1_meterType_OFFSET 8
#define AWE_Meter1_meterType_MASK 0x00000100
#define AWE_Meter1_meterType_SIZE -1

// float attackTime - Attack time of the meter. Specifies how quickly 
//         the meter value rises.
#define AWE_Meter1_attackTime_OFFSET 9
#define AWE_Meter1_attackTime_MASK 0x00000200
#define AWE_Meter1_attackTime_SIZE -1

// float releaseTime - Release time of the meter. Specifies how quickly 
//         the meter decays.
#define AWE_Meter1_releaseTime_OFFSET 10
#define AWE_Meter1_releaseTime_MASK 0x00000400
#define AWE_Meter1_releaseTime_SIZE -1

// float attackCoeff - Internal coefficient that realizes the attack 
//         time.
#define AWE_Meter1_attackCoeff_OFFSET 11
#define AWE_Meter1_attackCoeff_MASK 0x00000800
#define AWE_Meter1_attackCoeff_SIZE -1

// float releaseCoeff - Internal coefficient that realizes the release 
//         time.
#define AWE_Meter1_releaseCoeff_OFFSET 12
#define AWE_Meter1_releaseCoeff_MASK 0x00001000
#define AWE_Meter1_releaseCoeff_SIZE -1

// float value[1] - Array of meter output values, one per channel.
#define AWE_Meter1_value_OFFSET 13
#define AWE_Meter1_value_MASK 0x00002000
#define AWE_Meter1_value_SIZE 1


// ----------------------------------------------------------------------
// Scaler2 [ScalerV2]
// General purpose scaler with a single gain

#define AWE_Scaler2_ID 30001

// float gain - Gain in either linear or dB units.
// Default value: 0
// Range: -40 to 40
#define AWE_Scaler2_gain_OFFSET 8
#define AWE_Scaler2_gain_MASK 0x00000100
#define AWE_Scaler2_gain_SIZE -1

// float smoothingTime - Time constant of the smoothing process (0 = 
//         unsmoothed).
// Default value: 0
// Range: 0 to 1000
#define AWE_Scaler2_smoothingTime_OFFSET 9
#define AWE_Scaler2_smoothingTime_MASK 0x00000200
#define AWE_Scaler2_smoothingTime_SIZE -1

// int isDB - Selects between linear (=0) and dB (=1) operation
// Default value: 1
// Range: 0 to 1
#define AWE_Scaler2_isDB_OFFSET 10
#define AWE_Scaler2_isDB_MASK 0x00000400
#define AWE_Scaler2_isDB_SIZE -1

// float targetGain - Target gain in linear units.
#define AWE_Scaler2_targetGain_OFFSET 11
#define AWE_Scaler2_targetGain_MASK 0x00000800
#define AWE_Scaler2_targetGain_SIZE -1

// float currentGain - Instantaneous gain applied by the module.
#define AWE_Scaler2_currentGain_OFFSET 12
#define AWE_Scaler2_currentGain_MASK 0x00001000
#define AWE_Scaler2_currentGain_SIZE -1

// float smoothingCoeff - Smoothing coefficient.
#define AWE_Scaler2_smoothingCoeff_OFFSET 13
#define AWE_Scaler2_smoothingCoeff_MASK 0x00002000
#define AWE_Scaler2_smoothingCoeff_SIZE -1


// ----------------------------------------------------------------------
// Meter2 [Meter]
// Peak and RMS meter module

#define AWE_Meter2_ID 30003

// int meterType - Operating mode of the meter. Selects between peak and 
//         RMS calculations. See the discussion section for more details.
// Default value: 18
// Range: unrestricted
#define AWE_Meter2_meterType_OFFSET 8
#define AWE_Meter2_meterType_MASK 0x00000100
#define AWE_Meter2_meterType_SIZE -1

// float attackTime - Attack time of the meter. Specifies how quickly 
//         the meter value rises.
#define AWE_Meter2_attackTime_OFFSET 9
#define AWE_Meter2_attackTime_MASK 0x00000200
#define AWE_Meter2_attackTime_SIZE -1

// float releaseTime - Release time of the meter. Specifies how quickly 
//         the meter decays.
#define AWE_Meter2_releaseTime_OFFSET 10
#define AWE_Meter2_releaseTime_MASK 0x00000400
#define AWE_Meter2_releaseTime_SIZE -1

// float attackCoeff - Internal coefficient that realizes the attack 
//         time.
#define AWE_Meter2_attackCoeff_OFFSET 11
#define AWE_Meter2_attackCoeff_MASK 0x00000800
#define AWE_Meter2_attackCoeff_SIZE -1

// float releaseCoeff - Internal coefficient that realizes the release 
//         time.
#define AWE_Meter2_releaseCoeff_OFFSET 12
#define AWE_Meter2_releaseCoeff_MASK 0x00001000
#define AWE_Meter2_releaseCoeff_SIZE -1

// float value[1] - Array of meter output values, one per channel.
#define AWE_Meter2_value_OFFSET 13
#define AWE_Meter2_value_MASK 0x00002000
#define AWE_Meter2_value_SIZE 1


// ----------------------------------------------------------------------
// SYS_toFract [TypeConversion]
// Converts between numeric data types

#define AWE_SYS_toFract_ID 

// int inputType - Specifies the dataType of the input
// Default value: 0
// Range: 0 to 2
#define AWE_SYS_toFract_inputType_OFFSET 8
#define AWE_SYS_toFract_inputType_MASK 0x00000100
#define AWE_SYS_toFract_inputType_SIZE -1

// int outputType - Specifies the dataType of the output
// Default value: 1
// Range: 0 to 2
#define AWE_SYS_toFract_outputType_OFFSET 9
#define AWE_SYS_toFract_outputType_MASK 0x00000200
#define AWE_SYS_toFract_outputType_SIZE -1



#endif // PASSTHROUGHSIMPLE_H

