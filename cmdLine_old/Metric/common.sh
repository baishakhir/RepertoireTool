./detect_source.py ../../bsd_data/token_size_output/$1/out_txt/open_free.txt ../../bsd_data/token_size_output/$1/out_txt/net_open.txt  > ../../bsd_data/token_size_output/$1/out_txt/open_common.txt
./detect_source.py ../../bsd_data/token_size_output/$1/out_txt/open_free.txt ../../bsd_data/token_size_output/$1/out_txt/net_free.txt  > ../../bsd_data/token_size_output/$1/out_txt/free_common.txt
./detect_source.py ../../bsd_data/token_size_output/$1/out_txt/net_free.txt ../../bsd_data/token_size_output/$1/out_txt/net_open.txt  > ../../bsd_data/token_size_output/$1/out_txt/net_common.txt
