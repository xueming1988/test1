#ifndef _ALEXACOMMSAPI_H_
#define _ALEXACOMMSAPI_H_

#if defined(__GNUC__) && (__GNUC__ >= 4)
#ifdef _WIN32
#define ALEXACOMMSLIB_API __attribute__((dllexport))
#else
#define ALEXACOMMSLIB_API __attribute__((visibility("default")))
#endif
#define ALEXACOMMSLIB_DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
#define ALEXACOMMSLIB_API __declspec(dllexport)
#define ALEXACOMMSLIB_DEPRECATED __declspec(deprecated)
#else
#define ALEXACOMMSLIB_API
#define ALEXACOMMSLIB_DEPRECATED
#pragma message("DEPRECATED is not defined for this compiler")
#endif

#endif
