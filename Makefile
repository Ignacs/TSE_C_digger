all: 
	gcc -o   TSE_stock_fetch TSE_stock_fetch.c -L/usr/lib/x86_64-linux-gnu -lcurl -lsqlite3
	gcc -o   TSE_stock_parser TSE_stock_parser.c -lsqlite3  -finput-charset=Big5 -fexec-charset=Big5 
	#gcc -o csvHandlerWC csv_hander_wc.c


