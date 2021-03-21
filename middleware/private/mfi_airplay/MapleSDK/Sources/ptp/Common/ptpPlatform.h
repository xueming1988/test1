/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_PLATFORM_H_
#define _PTP_PLATFORM_H_

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "ptpConstants.h"

#define ptpSnprintf(...) snprintf(__VA_ARGS__)

/**
 * @brief Converts the unsigned short integer hostshort from host byte order to network byte order.
 * @param s source value
 * @return converted value
 */
#define ptpHtonsu(s) ((uint16_t)hton16(s))

/**
 * @brief Converts the signed short integer hostshort from host byte order to network byte order.
 * @param s source value
 * @return converted value
 */
#define ptpHtonss(s) ((int16_t)hton16(s))

/**
 * @brief Converts the unsigned integer hostlong from host byte order to network byte order.
 * @param l source value
 * @return converted value
 */
#define ptpHtonlu(l) ((uint32_t)hton32(l))

/**
 * @brief Converts the signed integer hostlong from host byte order to network byte order.
 * @param l source value
 * @return converted value
 */
#define ptpHtonls(l) ((int32_t)hton32(l))

/**
 * @brief Converts the unsigned short integer netshort from network byte order to host byte order.
 * @param s source value
 * @return converted value
 */
#define ptpNtohsu(s) ((uint16_t)ntoh16(s))

/**
 * @brief Converts the signed short integer netshort from network byte order to host byte order.
 * @param s source value
 * @return converted value
 */
#define ptpNtohss(s) ((int16_t)ntoh16(s))

/**
 * @brief Converts the unsigned integer netlong from network byte order to host byte order.
 * @param l source value
 * @return converted value
 */
#define ptpNtohlu(l) ((uint32_t)ntoh32(l))

/**
 * @brief Converts the signed integer netlong from network byte order to host byte order.
 * @param l source value
 * @return converted value
 */
#define ptpNtohls(l) ((int32_t)ntoh32(l))

/**
 * @brief Converts a 64-bit unsigned word from host to network order
 * @param x source value
 * @return converted value
 */
#define ptpHtonllu(x) ((uint64_t)hton64(x))

/**
 * @brief Converts a 64-bit signed word from host to network order
 * @param x source value
 * @return converted value
 */
#define ptpHtonlls(x) ((int64_t)hton64(x))

/**
 * @brief Converts a 64 bit unsigned word from network to host order
 * @param x source value
 * @return converted value
 */
#define ptpNtohllu(x) ((uint64_t)ntoh64(x))

/**
 * @brief Converts a 64 bit signed word from network to host order
 * @param x source value
 * @return converted value
 */
#define ptpNtohlls(x) ((int64_t)ntoh64(x))

#endif // _PTP_PLATFORM_H_
