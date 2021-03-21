#ifndef ASR_TTS_H
#define ASR_TTS_H

#define TTS_TODAY "今天"
#define TTS_TOMORROW "明天"
#define TTS_AFTER_TOMORROW "后天"

#define TTS_WANT "我要"
#define TTS_WANT2 "我想"

#define TTS_BOOK "听书"
#define TTS_BOOK2 "听说"
#define TTS_BOOK3 "听输"
#define TTS_BOOK4 "天书"
#define TTS_BOOK5 "情书"

#define TTS_WAKEUP "听吧听吧"
#define TTS_WAKEUP_2 "请808"
#define TTS_WAKEUP_3 "8斤吧"
#define TTS_WAKEUP_4 "8斤8"

#define TTS_WAKEUP_BA1 "吧"
#define TTS_WAKEUP_BA2 "八"
#define TTS_WAKEUP_BA3 "巴"

#define TTS_MUSIC "听歌"
#define TTS_MUSIC2 "听首歌"
#define TTS_MUSIC3 "来首歌"
#define TTS_MUSIC4 "听个歌"

#define TTS_NOVEL1 "故事"
#define TTS_NOVEL2 "小说"
#define TTS_JOKE "笑话"
#define TTS_XIANGSHENG "相声"
#define TTS_PINGSHU "评书"
#define TTS_XIQU "戏曲"
#define TTS_TONGHUA "童话"
#define TTS_TONGHUA2 "通话"
#define TTS_YUYAN "寓言"
#define TTS_YUYAN2 "预言"
#define TTS_YUYAN3 "语言"

#define TTS_LISTEN "听"
#define TTS_RANDOM "随便"

#define TTS_BT_MODE "蓝牙模式"
#define TTS_TCARD_MODE "T卡模式"
#define TTS_TCARD_MODE2 "t卡模式"
#define TTS_AUX_MODE "外线模式"
#define TTS_AUX_MODE2 "外接模式"
#define TTS_WIFI_MODE "wifi模式"

#define TTS_HELLO "您好"
#define TTS_THANKS "谢谢使用"

#define TTS_SHUTDOWN "关机"
#define TTS_AT_ONCE "立即"
#define TTS_CANCEL "取消"

#define TTS_SECOND "分钟"
#define TTS_PLAY "播放"
#define TTS_FAVORITE "收藏"
#define TTS_PRESET "预置"
#define TTS_PRESET2 "预制"
#define TTS_PRESET3 "预支"
#define TTS_PRESET4 "预知"

#define TTS_SAVE "保存"
#define TTS_BUTTON "按键"
#define TTS_BUTTON2 "暗箭"
#define TTS_BUTTON3 "案件"

#define TTS_NETWORK "配置网络"
#define TTS_NETWORK2 "设置网络"

#define TTS_ONE "一"
#define TTS_TWO "二"
#define TTS_THREE "三"
#define TTS_FOUR "四"
#define TTS_FIVE "五"
#define TTS_FIVE2 "我"
#define TTS_SIX "六"
#define TTS_SEVEN "七"
#define TTS_EIGHT "八"
#define TTS_NINE "九"
#define TTS_TEN "十"
#define TTS_TEN_ONE "十一"
#define TTS_TEN_TWO "十二"
#define TTS_TEN_THREE "十三"
#define TTS_TEN_FOUR "十四"
#define TTS_TEN_FIVE "十五"
#define TTS_TEN_FIVE2 "十我"
#define TTS_TEN_SIX "十六"
#define TTS_TEN_SEVEN "十七"
#define TTS_TEN_EIGHT "十八"
#define TTS_TEN_NINE "十九"
#define TTS_ONE_TEN "一十"
#define TTS_TWO_TEN "二十"
#define TTS_THREE_TEN "三十"
#define TTS_FOUR_TEN "四十"
#define TTS_FIVE_TEN "五十"
#define TTS_FIVE_TEN2 "我十"
#define TTS_SIX_TEN "六十"
#define TTS_SEVEN_TEN "七十"
#define TTS_EIGHT_TEN "八十"
#define TTS_NINE_TEN "九十"
#define TTS_HUNDRED "一百"
#define TTS_THOUSND "一千"

#define TTS_TIME_TELL "现在是北京时间"
#define TTS_HOUR_TELL "点"
#define TTS_MINUTE_TELL "分"

#define TTS_ALARM_SET_SUCCESS "设置成功"
#define TTS_REMINDER_SET_SUCCESS "提醒设置成功"

#define ASR_AISPEECH_DOMAIN_MUSIC "音乐"
#define TTS_SONG_NAME "歌曲名"
#define TTS_SONG_ARTIST "艺术家"
#define TTS_SONG_TYPE "类型"
#define TTS_SONG_ALBUM "专辑"
#define TTS_SONG_STYLE "风格"
#define ASR_AISPEECH_DOMAIN_REMIND "提醒"
#define ASR_AISPEECH_DOMAIN_WEATHER "天气"
#define ASR_AISPEECH_WEATHER_PROVINCE "省份"
#define ASR_AISPEECH_WEATHER_CITY "城市"
#define ASR_AISPEECH_WEATHER_REGION "区域"
#define ASR_AISPEECH_ALARM_OBJECT "对象"
#define ASR_AISPEECH_ALARM_OPERATE "操作"
#define ASR_AISPEECH_ITEM_NUMBER "数量"
#define ASR_AISPEECH_DATE "日期"
#define ASR_AISPEECH_TIME "时间"
#define ASR_AISPEECH_ALARM_SET "设置"
#define ASR_AISPEECH_ALARM_CANCEL "关闭"
#define ASR_AISPEECH_OBJECT_ALARM "闹钟"

#define ALEXA_UPLOAD_FILE "/tmp/web/alexa_upload.pcm"
#define ALEXA_RECORD_FILE "/tmp/web/alexa_record.pcm"
#define ALEXA_ERROR_FILE "/tmp/web/alexa_error.txt"

typedef struct {
    int login;
    int on_line;
    int http_ret_code;
    int record_to_sd;
    int asr_test_flag;
    time_t last_dialog_ts;
    time_t last_record_ts;
    long long record_start_ts;
    long long record_end_ts;
    long long vad_ts;
    unsigned char last_rec_file[256];
} asr_dump_t;
extern asr_dump_t asr_dump_info;

#define ASR_DATE_FMT "%d%.2d%.2d_%.2d_%.2d_%.2d"
#define ASR_RECORD_FMT "%s/%s_record.pcm"
#define ASR_UPLOAD_FMT "%s/%s_upload.pcm"
#define ASR_RECORD_WAV "%s/%s_record.wav"

#define ASR_UPLOAD_FILE "/tmp/web/asr_upload.pcm"
#define ASR_RECORD_FILE "/tmp/web/asr_record.pcm"
#define ASR_RESULT_FILE "/tmp/web/asr_result.txt"

#endif
