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
MAXFILESIZE=${15}
load_suffix="$WORKLOAD"_"$REQUESTDISTRIBUTION"_load_"$RECORDCOUNT"_"$FIELDLENGTHDISTRIBUTION"_"$INSERTORDER"_"$FIELDLENGTH".trace0
run_suffix="$WORKLOAD"_"$REQUESTDISTRIBUTION"_run_"$RECORDCOUNT"_"$FIELDLENGTHDISTRIBUTION"_"$INSERTORDER"_"$FIELDLENGTH".trace0
filename_load="$dir""$load_suffix"
filename_run="$dir""$run_suffix"
ls -l "$filename_load"
#echo "$filename_run"
rm -rf "$dbname"
size_before="$WORKLOAD"_"$REQUESTDISTRIBUTION"_"$DISKENV"_"$db_size"_"$WRITEBUFFERSIZE"_before
size_after="$WORKLOAD"_"$REQUESTDISTRIBUTION"_"$DISKENV"_"$db_size"_"$WRITEBUFFERSIZE"_after
iostat -k > "$size_before"
time -p ./a.out "$filename_load" "$dbname" run 0 > result_"$load_suffix"<<EOF
$BLOOMBITS $COMPRESSION $TABLECACHESIZE $WRITEBUFFERSIZE $MAXFILESIZE
EOF
time -p ./a.out "$filename_run" "$dbname" run 0 > result_"$run_suffix"<<EOF
$BLOOMBITS $COMPRESSION $TABLECACHESIZE $WRITEBUFFERSIZE $MAXFILESIZE
EOF
iostat -k > "$size_after"
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
mv "$size_before" "$newdir"
mv "$size_after" "$newdir"
}
WORKLOAD=workloada
REQUESTDISTRIBUTION=zipfian
RECORDCOUNT=50000
FIELDLENGTHDISTRIBUTION=constant
INSERTORDER=hashed
FIELDLENGTH=128
BLOOMBITS=10
COMPRESSION=0
TABLECACHESIZE=50000
dir=./
DISKENV=1disks
dbname=~/storage/adb
db_size=50M
WRITEBUFFERSIZE=4194304
MAXFILESIZE=4194304
__runLeveldb "$WORKLOAD" "$REQUESTDISTRIBUTION" "$RECORDCOUNT" "$FIELDLENGTHDISTRIBUTION" \
"$INSERTORDER" "$FIELDLENGTH" "$BLOOMBITS" "$COMPRESSION" "$TABLECACHESIZE" "$dir" "$DISKENV" "$dbname" "$db_size" "$WRITEBUFFERSIZE" "$MAXFILESIZE"

# #workloada
# WORKLOAD=workloada
# __runLeveldb "$WORKLOAD" "$REQUESTDISTRIBUTION" "$RECORDCOUNT" "$FIELDLENGTHDISTRIBUTION" \
# "$INSERTORDER" "$FIELDLENGTH" "$BLOOMBITS" "$COMPRESSION" "$TABLECACHESIZE" "$dir" "$DISKENV" "$dbname" "$db_size" "$WRITEBUFFERSIZE"
