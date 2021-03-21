
#su
#/mount -o rw,remount /system
#cp /data/ztx-test/tt.sh /system/bin/tt 
#chmod 777 /system/bin/tt
cp ../build/example/hrsctest  ./hrsctest
chmod 777 hrsctest
echo "start!"
export LD_LIBRARY_PATH=./hobotspeechapi/lib:$LD_LIBRARY_PATH
#valgrind --tool=memcheck --leak-check=yes --show-reachable=yes --run-libc-freeres=yes --log-file=./log.txt  ./hrsctest2 -i test.pcm -o ./ -cfg hobotspeechapi/hrsc/ 
./hrsctest -i xiaoweixiaowei_2+2.pcm -o ./ -cfg hobotspeechapi/hrsc/
