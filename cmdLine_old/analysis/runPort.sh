#$1 is the token size
echo $1
./total_port.py ../../bsd_data/token_size_output/$1/out_txt/open_free.txt 1
./total_port.py ../../bsd_data/token_size_output/$1/out_txt/open_free.txt 2

./total_port.py ../../bsd_data/token_size_output/$1/out_txt/net_free.txt 1
./total_port.py ../../bsd_data/token_size_output/$1/out_txt/net_free.txt 2

./total_port.py ../../bsd_data/token_size_output/$1/out_txt/net_open.txt 1
./total_port.py ../../bsd_data/token_size_output/$1/out_txt/net_open.txt 2

echo "==============="
echo "FreeBSD"
./find_common.py ../../bsd_data/token_size_output/$1/shrink_txt/open_free.txt ../../bsd_data/token_size_output/$1/shrink_txt/net_free.txt

echo "NetBSD"
./find_common.py ../../bsd_data/token_size_output/$1/shrink_txt/net_open.txt ../../bsd_data/token_size_output/$1/shrink_txt/net_free.txt

echo "OpenBSD"
./find_common.py ../../bsd_data/token_size_output/$1/shrink_txt/open_free.txt ../../bsd_data/token_size_output/$1/shrink_txt/net_open.txt
