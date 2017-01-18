#!/bin/bash
__runLeveldb(){
WORKLOAD=$1
REQUESTDISTRIBUTION=$2
RECORDCOUNT=$3
FIELDLENGTHDISTRIBUTION=$4
INSERTORDER=$5
FIELDLENGTH=$6
BLOOMBITS=$7
COMPRESSION=$8
TABLECACHESIZE=$9
dir=${10}
DISKENV=${11}
dbname=${12}
db_size=${13}
WRITEBUFFERSIZE=${14}
load_suffix="$WORKLOAD"_"$REQUESTDISTRIBUTION"_load_"$RECORDCOUNT"_"$FIELDLENGTHDISTRIBUTION"_"$INSERTORDER"_"$FIELDLENGTH".trace0
run_suffix="$WORKLOAD"_"$REQUESTDISTRIBUTION"_run_"$RECORDCOUNT"_"$FIELDLENGTHDISTRIBUTION"_"$INSERTORDER"_"$FIELDLENGTH".trace0
filename_load="$dir""$load_suffix"
filename_run="$dir""$run_suffix"
#echo "$filename_load"
#echo "$filename_run"
rm -rf "$dbname"
size_before="$WORKLOAD"_"$REQUESTDISTRIBUTION"_"$DISKENV"_"$db_size"_"$WRITEBUFFERSIZE"_before
size_after="$WORKLOAD"_"$REQUESTDISTRIBUTION"_"$DISKENV"_"$db_size"_"$WRITEBUFFERSIZE"_after
iostat > "$size_before"
time -p ./a.out "$filename_load" "$dbname" run > result_"$load_suffix"<<EOF
$BLOOMBITS $COMPRESSION $TABLECACHESIZE $WRITEBUFFERSIZE
EOF
time -p ./a.out "$filename_run" "$dbname" run > result_"$run_suffix"<<EOF
$BLOOMBITS $COMPRESSION $TABLECACHESIZE $WRITEBUFFERSIZE
EOF
let WRITEBUFFERSIZE=WRITEBUFFERSIZE/1024/1024
newdir=bloom_"$BLOOMBITS"_compression_"$COMPRESSION"_tablecachesize_"$TABLECACHESIZE"_"$WRITEBUFFERSIZE"MB
if [ -d $newdir ]
then
    echo ""
else
    mkdir "$newdir"    
fi
mv result_"$load_suffix" "$newdir"
mv result_"$run_suffix" "$newdir"
}
WORKLOAD=workloadu
REQUESTDISTRIBUTION=zipfian
RECORDCOUNT=50000
FIELDLENGTHDISTRIBUTION=constant
INSERTORDER=hashed
FIELDLENGTH=1024
BLOOMBITS=10
COMPRESSION=0
TABLECACHESIZE=1000
dir=./
DISKENV=5disks
dbname=~/storage/adb
db_size=50MB
WRITEBUFFERSIZE=4194304
__runLeveldb "$WORKLOAD" "$REQUESTDISTRIBUTION" "$RECORDCOUNT" "$FIELDLENGTHDISTRIBUTION" \
"$INSERTORDER" "$FIELDLENGTH" "$BLOOMBITS" "$COMPRESSION" "$TABLECACHESIZE" "$dir" "$DISKENV" "$dbname" "$db_size" "$WRITEBUFFERSIZE"