/**
*make use flag -phread
*/

#ifndef __RESPONDER_H__
#define __RESPONDER_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
*初始化mDNS线程，如有需求另行增加。
*return: 当前实例的id
*/
int mDNS_init(void);

/**
*注册RAOP服务
*name:服务名字，格式：id@name，id最好使用本机mac地址，这样apple设备容易发现。
*port:服务监听端口
*/
//int mDNS_publishRaopService(char* name, int port);

/**
*注册一个服务
*name:服务名字，格式（audio）：id@name，id最好使用本机mac地址，这样apple设备容易发现。
*serviceType:服务协议，不同的服务对应不同的type。
*port:服务监听端口
*/
int mDNS_publishOneService(char* name, char * serviceType, int port, char* text[], int len);


void mDMS_DeregisterServices(void);

/**
*注销ID为instanceId的mDNS实例
*/
int mDNS_release(int instanceId);


#ifdef __cplusplus
}
#endif//__cplusplus

#endif
