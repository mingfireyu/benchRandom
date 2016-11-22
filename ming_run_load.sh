#!/bin/sh
WORKLOAD=workloadu
REQUESTDISTRIBUTION=zipfian
RECORDCOUNT=100
FIELDLENGTHDISTRIBUTION=constant
INSERTORDER=ordered
FIELDLENGTH=1024
BLOOMBITS=10
COMPRESSION=0
TABLECACHESIZE=1000
dir=./
load_suffix="$WORKLOAD"_"$REQUESTDISTRIBUTION"_load_"$RECORDCOUNT"_"$FIELDLENGTHDISTRIBUTION"_"$INSERTORDER"_"$FIELDLENGTH".trace0
run_suffix="$WORKLOAD"_"$REQUESTDISTRIBUTION"_run_"$RECORDCOUNT"_"$FIELDLENGTHDISTRIBUTION"_"$INSERTORDER"_"$FIELDLENGTH".trace0
filename_load="$dir""$load_suffix"
filename_run="$dir""$run_suffix"
#echo "$filename_load"
#echo "$filename_run"
dbname=/home/ming/storage/adb
./a.out "$filename_load" "$dbname" run > result_"$load_suffix"<<EOF
$BLOOMBITS $COMPRESSION $TABLECACHESIZE
EOF
./a.out "$filename_run" "$dbname" run > result_"$run_suffix"<<EOF
$BLOOMBITS $COMPRESSION $TABLECACHESIZE
EOF
newdir=bloom_"$BLOOMBITS"_compression_"$COMPRESSION"_tablecachesize_"$TABLECACHESIZE"
if [ -d $newdir ]
then
    echo ""
else
    mkdir "$newdir"    
fi
mv result_"$load_suffix" "$newdir"
mv result_"$run_suffix" "$newdir"
