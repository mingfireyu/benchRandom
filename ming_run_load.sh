#!/bin/sh
WORKLOAD=workloadu
REQUESTDISTRIBUTION=uniform
RECORDCOUNT=102456280
FIELDLENGTHDISTRIBUTION=constant
INSERTORDER=hashed
FIELDLENGTH=1024
BLOOMBITS=10
COMPRESSION=0
TABLECACHESIZE=1000
WRITEBUFFERSIZE=134217728
db_size=100G
DISKENV=5disks
dir=~/traceGen/
load_suffix="$WORKLOAD"_"$REQUESTDISTRIBUTION"_load_"$RECORDCOUNT"_"$FIELDLENGTHDISTRIBUTION"_"$INSERTORDER"_"$FIELDLENGTH".trace0
run_suffix="$WORKLOAD"_"$REQUESTDISTRIBUTION"_run_"$RECORDCOUNT"_"$FIELDLENGTHDISTRIBUTION"_"$INSERTORDER"_"$FIELDLENGTH".trace0
filename_load="$dir""$load_suffix"
filename_run="$dir""$run_suffix"
#echo "$filename_load"
#echo "$filename_run"
dbname=~/storage/adb
size_before="$WORKLOAD"_"$REQUESTDISTRIBUTION"_"$DISKENV"_"$db_size"_"$WRITEBUFFERSIZE"_before
size_after="$WORKLOAD"_"$REQUESTDISTRIBUTION"_"$DISKENV"_"$db_size"_"$WRITEBUFFERSIZE"_after
iostat > "$size_before"
time -p ./a.out "$filename_load" "$dbname" run > result_"$WRITEBUFFERSIZE"_"$load_suffix" <<EOF
$BLOOMBITS $COMPRESSION $TABLECACHESIZE $WRITEBUFFERSIZE
EOF
time -p ./a.out "$filename_run" "$dbname" run > result_"$WRITEBUFFERSIZE"_"$run_suffix" <<EOF
$BLOOMBITS $COMPRESSION $TABLECACHESIZE $WRITEBUFFERSIZE
EOF
iostat > "$size_after"
newdir=bloom_"$BLOOMBITS"_compression_"$COMPRESSION"_tablecachesize_"$TABLECACHESIZE"
if [ -d $newdir ]
then
    echo ""
else
    mkdir "$newdir"    
fi
mv "$size_before" "$newdir"
mv "$size_after" "$newdir"
mv result_"$load_suffix" "$newdir"
mv result_"$run_suffix" "$newdir"
#
#