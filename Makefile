all:
	gcc -o   TSE_stock_fetch TSE_stock_fetch.c -L/usr/lib/x86_64-linux-gnu -lcurl -lsqlite3
	#gcc -o   TSE_stock_parser TSE_stock_parser.c -lsqlite3  -finput-charset=Big5 -fexec-charset=Big5
	gcc -o   Stock_query Stock_query.c -lsqlite3  -finput-charset=Big5 -fexec-charset=Big5  -ldl
	gcc -o  Stock_ind_gen  Stock_ind_gen.c -lsqlite3  -finput-charset=Big5 -fexec-charset=Big5  -ldl

	#@####gcc -o csvHandlerWC csv_hander_wc.c
	gcc -c libVR.c -fPIC
	gcc -shared -o libVR.so libVR.o
	gcc -c libVRrelated.c -fPIC
	gcc -shared -o libVRrelated.so libVRrelated.o
