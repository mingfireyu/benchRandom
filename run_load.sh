#!/bin/sh
WORKLOAD=workloadb
FIELDLENGTH=1024
#FIELDLENGTH=8
RECORDCOUNT=52428800
#RECORDCOUNT=500000
disknum=5
FIELDLENGTHDISTRIBUTION=constant
requestdistribution=zipfian
db_size=50G
mdlevel=5
DISKENV=RAID"$mdlevel"_512K
filename_load=/home/cyx/workspace/YCSB/traceGen/"$WORKLOAD"_"$requestdistribution"_load_"$FIELDLENGTH"bytes_"$RECORDCOUNT"_"$FIELDLENGTHDISTRIBUTION".trace
filename_run=/home/cyx/workspace/YCSB/traceGen/"$WORKLOAD"_"$requestdistribution"_run_"$FIELDLENGTH"bytes_"$RECORDCOUNT"_"$FIELDLENGTHDISTRIBUTION".trace
result_load="$filename_load"_"$DISKENV"_result
result_run="$filename_run"_"$DISKENV"_result
#dbname=/home/cyx/workspace/tempraidkv/kvraidStatistics/bin/testdir/bdb
dbname=/home/cyx/storage/bdb
rm -r "$dbname"
sync;echo 3 > /proc/sys/vm/drop_caches
iostat > "$WORKLOAD"_"$disknum"d_"$db_size"_RAID_"$mdlevel"_before
./a.out "$filename_load" "$dbname" run > "$result_load"
./a.out "$filename_run" "$dbname" run > "$result_run"
sync;echo 3 > /proc/sys/vm/drop_caches
iostat > "$WORKLOAD"_"$disknum"d_"$db_size"_RAID_"$mdlevel"_after

dirname="$FIELDLENGTH"_"$RECORDCOUNT"_"$WORKLOAD"_"$DISKENV"_"$FIELDLENGTHDISTRIBUTION"
mkdir "$dirname"
mv read_static* "$dirname"/
mv "$result_load" "$dirname"/
mv "$result_run"  "$dirname"/
mv "$WORKLOAD"_"$disknum"d_"$db_size"_RAID_"$mdlevel"_before "$dirname"/
mv "$WORKLOAD"_"$disknum"d_"$db_size"_RAID_"$mdlevel"_after  "$dirname"/
