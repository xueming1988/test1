
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "asr_debug.h"

unsigned int g_debug_level = ASR_DEBUG_NONE;

void asr_set_debug_level(int level) { g_debug_level = level; }
