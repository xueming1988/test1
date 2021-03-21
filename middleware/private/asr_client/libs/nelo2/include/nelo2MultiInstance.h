#ifndef __NELO2__MULTIINSTANCE__H__
#define __NELO2__MULTIINSTANCE__H__

#include "nelo2Common.h"

#define NELO2_API
// define NHANDLE object
typedef void *NHANDLE;

// for pure C have no bool type
#if !defined(__cplusplus) && !defined(c_plusplus)
typedef enum { false, true } bool;
#endif

////////////////////////////////////////////////////////////////////////////////
// BEGIN: C interface
#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

// crash callback function type
typedef void (*crash_func_type)(void *content);

// init/destory function
//[WARNING]: please call destroyInstance() function before you close your application.
NELO2_API int createInstance(NHANDLE *handle, const char *projectName,
                             const char *projectVersion = NELO2_DEF_PROJECTVERSION,
                             const char *logSource = NELO2_DEF_LOGSOURCE,
                             const char *logType = NELO2_DEF_LOGTYPE,
                             const char *serverAddr = NELO2_DEF_SERVERHOST,
                             const int serverPort = NELO2_DEF_SERVERPORT, const int https = 0);
NELO2_API void destroyInstance(NHANDLE handle);

// enable/disable collect the host information
NELO2_API void enableHostField(NHANDLE handle);
NELO2_API void disableHostField(NHANDLE handle);

// enable/disable collect the platform information
NELO2_API void enablePlatformField(NHANDLE handle);
NELO2_API void disablePlatformField(NHANDLE handle);

// add/delete custom field
NELO2_API bool addField(NHANDLE handle, const char *key, const char *val);
NELO2_API void removeField(NHANDLE handle, const char *key);
NELO2_API void removeAllFields(NHANDLE handle);

// get/set log level function, the default log level is NELO_LL_INFO
NELO2_API NELO_LOG_LEVEL getLogLevel(NHANDLE handle);
NELO2_API void setLogLevel(NHANDLE handle, const NELO_LOG_LEVEL eLevel = NELO_LL_INFO);

// get/set user id function
NELO2_API const char *getUserId(NHANDLE handle);
NELO2_API void setUserId(NHANDLE handle, const char *userID);

// set crash report UI window
NELO2_API bool openCrashCatcher(NHANDLE handle, const char *dumpFilePath);
NELO2_API void setCrashCallback(NHANDLE handle, const crash_func_type fnCrashCallback,
                                void *content);
NELO2_API void closeCrashCatcher(NHANDLE handle);

// send log messages
NELO2_API bool sendLog(NHANDLE handle, const NELO_LOG_LEVEL eLevel, const char *strMsg);
NELO2_API bool sendLogEx(NHANDLE handle, const time_t tUTCTime, const NELO_LOG_LEVEL eLevel,
                         const char *strMsg);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
// END: C interface
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// BEGIN: C++ interface
#if !defined(NELO2_DYNAMIC_MODE)
class NELO2_API NELO2Log
{
  public:
    // custom field type define
    class NELO2_API CustomField
    {
      public:
        CustomField();
        ~CustomField();

      public:
        bool addField(const char *key, const char *value);

        void delField(const char *key);

        void clearAll();

      private:
        friend class NELO2Log;
        void *m_customFiled;
    };

    ////////////////////////////////////////////////////////////////////////////
  private:
    void *m_plogOrigin;

  public:
    NELO2Log();
    ~NELO2Log();

  private:
    NELO2Log(const NELO2Log &rhs);
    NELO2Log &operator=(const NELO2Log &rhs);

  public:
    // return code:
    //  NELO2_OK: initial resources is successful.
    //  INVALID_PROJECT: the project name is empty
    //  INVALID_SERVER: the server's address is empty
    //  INVALID_PORT: the server's port is invalid
    int initialize(const char *projectName, const char *projectVersion = NELO2_DEF_PROJECTVERSION,
                   const char *logSource = NELO2_DEF_LOGSOURCE,
                   const char *logType = NELO2_DEF_LOGTYPE,
                   const char *serverAddr = NELO2_DEF_SERVERHOST,
                   const int serverPort = NELO2_DEF_SERVERPORT, const bool https = false);

    //[WARNING]: you must call destroy() before exit your application
    void destory();

  public:
    // enable/disable collect the host information
    void enableHostField();
    void disableHostField();

    // enable/disable collect the platform information
    void enablePlatformField();
    void disablePlatformField();

  public:
    // open crash catch function and set crash report's cache path
    // return true: open crash catch function is successful.
    //      false: open crash catch function is failed, maybe can't access the cache path.
    bool openCrashCatcher(const char *dumpFilePath = ".");

    // call the callback function when crash is happened and user already set it.
    void setCrashCallback(const crash_func_type fnCrashCallback, void *content = NULL);

    // close crash catch function
    void closeCrashCatcher();

  public:
    // add global custom field
    bool addGlobalField(const char *key, const char *value);

    // delete a specific field
    void delGlobalField(const char *key);

    // clear all global custom fields
    void clearGlobalFields();

  public:
    // set send log level, if priority of log level is higher than set value, the log can send to
    // nelo2 system, if less, don't send it
    // default value is INFO
    void setLogLevel(const NELO_LOG_LEVEL eLevel = NELO_LL_INFO);

    // get currently log level
    NELO_LOG_LEVEL getLogLevel();

    // get user id
    const char *getUserId();

    // set user id
    void setUserId(const char *userID);

  public:
    // true:  send log message is successful
    // false: send log message is failed
    //[WARNING]: SDK will use system UTC time as logTime
    bool sendLog(const NELO_LOG_LEVEL eLevel, const char *strMsg);
    bool sendLog(const NELO_LOG_LEVEL eLevel, const char *strMsg, const CustomField &customFields);

  public:
    // true:  send log message is successful
    // false: send log message is failed
    //[WARNING]: you can set log time, but the tUTCTime must is second/millisecond level
    bool sendLog(const time_t tUTCTime, const NELO_LOG_LEVEL eLevel, const char *strMsg);
    bool sendLog(const time_t tUTCTime, const NELO_LOG_LEVEL eLevel, const char *strMsg,
                 const CustomField &customFields);
};

#endif
// END: C++ interface
////////////////////////////////////////////////////////////////////////////////

#endif //__NELO2__MULTIINSTANCE__H__
