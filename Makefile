ALL:
	gcc -g -o counter -levent -levent_openssl -levent_pthreads -lssl -lcrypto -levhtp -ldl -lrt counter.c  /usr/lib/libevhtp.a -ltokyocabinet
