
#ifndef TASR_AUDIO_TOOLS_ENERGY_VAD_DEFINE_H_
#define TASR_AUDIO_TOOLS_ENERGY_VAD_DEFINE_H_

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

enum EVAD_RES
{
	EVAD_OK      = 0 ,
	EVAD_ERROR   = 1 ,

	EVAD_SPEAK   = 2 ,
	EVAD_SILENCE = 3 ,
	EVAD_UNKNOW  = 4
};

typedef void*  EVAD_HANDLE;


/*
  input param:  sample_rate  : audio sample rate : 16000 or 8000
  input param:  sil_time     : after detect a speak start ,if we detect silence longer then this time,then,we see find a speak end (unit is ms)\
                               this param set to 300 for general condition
  input param:  s_n_ration   : noise / signal,if in high noise condition set high value,default is 2.5f

  input param:  begin_confirm_window:
  input param:  begin_confirm: the up two params used in confirm speech begin window,and confirm time length,
                               if the two param set bigger will skip some short sound ,such as cough
*/
EVAD_RES  EVAD_GetHandle(EVAD_HANDLE* handle, int sample_rate, int sil_time, float s_n_ration = 2.5 , int begin_confirm_window = 600, int begin_confirm = 450);

/*
  input param: hold_history : weather reserve prepare statics data, set true for general condition
*/
EVAD_RES  EVAD_Reset(EVAD_HANDLE handle, int hold_history =  1);


/*
   output param begin_delay_time : we find a speak begin time has some time(ms) delay, so if we find first speak frame,
                                   real speak time is early time for this time,so we must sub this delay time,that is real
                                   speak begin time.
*/
EVAD_RES  EVAD_GetBeginDelayTime(EVAD_HANDLE handle, int* begin_delay_time);

EVAD_RES  EVAD_AddData(EVAD_HANDLE handle, const char* iwdata, size_t isize);

EVAD_RES  EVAD_Release(EVAD_HANDLE* handle);



#ifdef __cplusplus
}
#endif

#endif //TASR_AUDIO_TOOLS_ENERGY_VAD_DEFINE_H_
