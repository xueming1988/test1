#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "af_lavcresample.h"
#include "linkplay_resample_interface.h"


af_instance_t *linkplay_resample_instance_create(const char *filter)
{
  af_instance_t * af = (af_instance_t *)malloc(sizeof(af_instance_t));
	af_open(af);
	af->control(af,AF_CONTROL_COMMAND_LINE,(char *)filter);
	return af;
}
int linkplay_set_resample_format(af_data_t *in_format, af_instance_t* af)
{
	af->control(af,AF_CONTROL_REINIT,in_format);
	return 0;
}

af_data_t *linkPlay_resample(af_data_t *in_format,af_instance_t* af)
{
 	return af->play(af,in_format);
}

void linkplay_resample_uninit(af_instance_t* af)
{
	af->uninit(af);
}
