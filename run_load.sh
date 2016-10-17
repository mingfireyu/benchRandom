#!/bin/sh
WORKLOAD=workloadr
FIELDLENGTH=1024
RECORDCOUNT=1000
filename_load="$WORKLOAD"_load_"$FIELDLENGTH"bytes_"$RECORDCOUNT".trace
filename_run="$WORKLOAD"_run_"$FIELDLENGTH"bytes_"$RECORDCOUNT".trace

dbname=/home/ming/storage/adb
./a.out "$filename_load" "$dbname" run
./a.out "$filename_run" "$dbname" run