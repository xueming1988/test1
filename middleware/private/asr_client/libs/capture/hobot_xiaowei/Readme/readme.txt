** import thing **
-This SDK has usage time from now to 2019.8.27, the available time is two months



** instructions for hobotspeechdemo.zip **
1. /bin
- brief: Offline tool for offline audio file(This audio file maybe include xiaoweixiaowei for wakeup)
- usage: 
     [1] export LD_LIBRARY_PATH=XXX (such as lib/)
     [2] ./hrsctest -i xiaoweixiaowei.wav -o output/ -cfg hrsc/  

2. /hrsc
- brief: This file include the configuration files of Horizon Voice Pre-process(hisf_config.ini) and wakeup model(libhesr_conf.so),
         the others are the trained model of wakeup word(xiaoweixiaowei), we don't need do anything, just for use
- usage: The path of this file can be set into *cfg_file_path

3. /include		 
- brief: This file include the header file of the .so library: hrsc_sdk.h

4. /lib
- brief: This file include the .so library: libhrsc.so



** instructions for example.zip **
- brief: This file include example code of hrsctest for offline audio file test