#include<stdio.h>
#include<iostream>
#include<mutex>
#include<chrono>
#include<condition_variable>
#include<cstdlib>
#include<thread>  
#include<string>
#include<leveldb/db.h>
#include<assert.h>
#include<list>
#include<sys/time.h>
#include<time.h>
#include<unistd.h>
#include"buildRandomValue.h"
#include"slice.h"
#include <leveldb/filter_policy.h>
using namespace std;
#define KVBUFFER_LENGTH 100
#define LIST_LENGTH 5
#define KEYLENGTH 24
using std::mutex;
using std::cout;
using std::endl;
typedef struct kv_pair{
  std::string key;
  std::string value;
  long long timestamp;
  char operation;
}kv_pair;

typedef struct kvBuffer{
  int length;
  kv_pair kvs[KVBUFFER_LENGTH];
}kvBuffer;

list<kvBuffer *> consume_list,recycle_list;
kvBuffer checkBuffer;
std::mutex mut1,mut2;
std::condition_variable data_cond1,data_cond2;
bool more_data_to_produce;
leveldb::DB *db;
leveldb::Options ops;
leveldb::Status status;
FILE *fp;
static timeval ycsb_begin_time;
long long sum_time;
unsigned long error_count;
unsigned long read_count;
unsigned long record_count;
unsigned long key_count;
bool LOAD_FLAG = true;
RandomGenerator rdgen;


unsigned long long read_latency_sum;
unsigned long long write_latency_sum;
void make_data(kvBuffer *kvb,FILE *trace_file,bool &eof_flag){


  //  char number[10];
  char line[200];
  //  int readcount = 0;
  int length;
  int kviter = 0;
  //  int i ;
  char* pos;
  kv_pair* kvp = NULL;
  while(kviter < KVBUFFER_LENGTH && fscanf(trace_file,"%s",line) > 0 ){
    kvp = &kvb->kvs[kviter];
    kviter++;
    kvp->key.clear();
    kvp->value.clear();

    //value length
    length=atoi(line);
    pos = strchr(line,',');
    pos++;
    //operation
    kvp->operation = *pos;
    pos = strchr(pos,',');
    pos++;
    //key
    kvp->key.append(pos);
    kvp->key.resize(KEYLENGTH);
    //kvp->key.resize(kvp->key.size() - 1);
   
    //value
    if(LOAD_FLAG){
      if(kvp->operation == 'R'){
	kvp->value.append(rdgen.Generate(length).data(),length);
	if(kvp->value.size() != length){
	  cout<<"copy error"<<endl;
	}
      }else{
	kviter--;
      }
    }else{
      if(length > 0){
	(kvp->value).append(rdgen.Generate(length).data(),length);
	if(kvp->value.size() != length){
	  cout<<"copy error"<<endl;
	}
	// if(checkBuffer.length < 6 && rand()%2 == 0){
	//   cout<<"kvp->key:"<<kvp->key<<endl;
	//   cout<<"kvp->value:"<<kvp->value<<endl;
	//   checkBuffer.length++;
	// }
      }
    }

 /* cout<<"key:"<<kvp->key<<" ";
  cout<<"operation:"<<kvp->operation;
  cout<<"length:"<<length<<endl;*/


  }

  if(ferror(fp)){
    perror("read trace file error");
  }
  if(feof(fp)){
    eof_flag = true;
  }
  kvb->length = kviter;

}

void compute_diff(struct timeval &t1,struct timeval &t2,long long &diff)
{
  diff = (long long)(t1.tv_sec - t2.tv_sec) * 1000000;
  diff = diff + t1.tv_usec - t2.tv_usec;
}

void process(kvBuffer *kvb){

  int kviter;
  string value;
  kv_pair *kvp = NULL;
  struct timeval start_time;
  struct timeval end_time;
  //  struct timeval now_time;
  long long diff;
  if(!record_count){
    gettimeofday(&ycsb_begin_time,NULL);
  }
  for(kviter = 0 ; kviter < kvb->length ; kviter++){
    kvp = &kvb->kvs[kviter];
    record_count++;
    if(record_count % 10000 == 0){
      fprintf(stderr,"\rrecord_count:%lu",record_count);
      fflush(stderr);
    }
    gettimeofday(&start_time,NULL);
    if(kvp->operation == 'R'){
      if(LOAD_FLAG){
	status = db->Put(leveldb::WriteOptions(),kvp->key,kvp->value);
      }
      else{
	status = db->Get(leveldb::ReadOptions(),kvp->key, &value);
	read_count++;
      }
      if(!status.ok()){
	cerr<<"key:"<<kvp->key<<" ";
	cerr<<status.ToString()<<endl;
	error_count++;
      }
    }else{
      // cout<<"value:length:"<<(kvp->value).length()<<endl;
      status = db->Put(leveldb::WriteOptions(),kvp->key,kvp->value);
      if(!status.ok()){
	cerr<<status.ToString()<<endl;
	error_count++;
      }
    }      
    gettimeofday(&end_time,NULL);
    compute_diff(end_time,start_time,diff);
    
    sum_time = sum_time + diff;
    if(kvp->operation == 'R'){
      read_latency_sum += diff;
    }else{
      write_latency_sum += diff;
    }
  }
  
}

void init(char filename[],char dbfilename[],char load_str[]){
  int i;
  int bloomBits;
  unsigned long long tableCacheSize;
  int compressionFlag;
  fp = fopen(filename,"r");
  error_count = 0;
  read_count=0;
  record_count=0;
  key_count=0;
  read_latency_sum = 0;
  write_latency_sum = 0;
  
  checkBuffer.length = 0;
   srand( (unsigned)time( NULL ) ); 
  if( fp == NULL ){
    printf("error\n");
  }
  ops.create_if_missing = true;
  fprintf(stderr,"please input bloom filter bits Compression?1(true) or 0(false) tableCache size\n");
  scanf("%d %d %llu",&bloomBits,&compressionFlag,&tableCacheSize);
  
  ops.filter_policy = leveldb::NewBloomFilterPolicy(bloomBits);
  if(!compressionFlag){
    ops.compression = leveldb::kNoCompression;   
  }
  ops.max_open_files = tableCacheSize;

  printf("environment:\n");
  printf("bloomfilterbits\tCompression\ttableCacheSize\tlogOpen\n");
  printf("%15d\t%11s\t%14llu\t%7s\n",bloomBits,compressionFlag?"true":"false",tableCacheSize,ops.log_open?"true":"false");
  printf("filename:%s\t dbfilename:%s\n",filename,dbfilename?dbfilename:"testdb");
  
  if(load_str[0] == 'l' || load_str[0] == 'L'){
    LOAD_FLAG = true;
  }else{
    LOAD_FLAG = false;
  }
  if(dbfilename == NULL){
    leveldb::Status status = leveldb::DB::Open(ops,"testdb",&db);
  }else{
    leveldb::Status status = leveldb::DB::Open(ops,dbfilename,&db);
  }

  for(i = 1 ; i <= LIST_LENGTH ; i++){
    recycle_list.push_back(new kvBuffer());
  }
  consume_list.clear();
  more_data_to_produce = true;
}


void consume(){

  kvBuffer* kvb = NULL;
  std::unique_lock<std::mutex> lk(mut1);
  struct timeval ycsb_end_time;
  data_cond1.wait(lk,[]{return !consume_list.empty();});
  kvb = consume_list.front();
  consume_list.pop_front();
  lk.unlock();
  long long diff;
  process(kvb);
  
  std::unique_lock<std::mutex> lk2(mut2);
  recycle_list.push_back(kvb);
  lk2.unlock();
  data_cond2.notify_one();
  /* if more_data_to_produce set to be false just now,consume_list may be not empty.So we must continue to process data 
     until consume list be empty.*/
  while(true){
    std::unique_lock<std::mutex> lk1(mut1);
    if(!more_data_to_produce && consume_list.empty()){  
      std::cout<<"error_count:"<<error_count<<std::endl;
      std::cout<<"read_count:"<<read_count<<std::endl;
      std::cout<<"record_count:"<<record_count<<std::endl;
      std::cout<<record_count*1.0/(sum_time*1.0/1000000)<<"op/s"<<endl;
      std::cout<<(sum_time*1.0/1000)/(record_count)<<"ms/op"<<endl;
      std::cout<<"read_average_latency:"<<read_latency_sum*1.0/read_count<<"micros"<<endl;
      std::cout<<"write_average_latency:"<<write_latency_sum*1.0/(record_count - read_count)<<"micros"<<endl;
      //  int i;
      // leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
      /*    for (it->SeekToFirst(),i=0; it->Valid() && i < 5; it->Next(),i++) {
	    cout << it->key().ToString() << ": "  << it->value().ToString() << endl;
	    }*/
      gettimeofday(&ycsb_end_time,NULL);
      compute_diff(ycsb_end_time,ycsb_begin_time,diff);
      std::cout<<"overall time:"<<diff<<"us"<<endl;
      std::string stat_str;
      db->GetProperty("leveldb.stats",&stat_str);
      std::cout<<stat_str<<std::endl;
      delete db;
      exit(0);      
    }
    data_cond1.wait(lk1,[]{return !consume_list.empty();});
    kvb = consume_list.front();
    consume_list.pop_front();
    lk1.unlock();
    process(kvb);
    
    std::unique_lock<std::mutex> lk2(mut2);
    recycle_list.push_back(kvb);
    lk2.unlock();
    data_cond2.notify_one();
   
  }
}

void produce(){

  kvBuffer* kvb = NULL;
  bool eof_flag = false;
  while(true){
    std::unique_lock<std::mutex> lk2(mut2);
    data_cond2.wait(lk2,[]{return !recycle_list.empty();});
    kvb = recycle_list.front();
    recycle_list.pop_front();
    /*    if(recycle_list.empty()){
      cout<<"waiting for recycle list"<<endl;
      }*/
    lk2.unlock();
    make_data(kvb,fp,eof_flag);
    
    std::unique_lock<std::mutex> lk1(mut1);
    consume_list.push_back(kvb);
    lk1.unlock();
    data_cond1.notify_one();
    
    if(eof_flag){
      more_data_to_produce = false;  //no more data to produce
      cout<<"load data end!"<<endl;
      return ;
    }
    
  }
  cout<<"I'm living"<<endl;
}


int main(int argc,char *argv[]){
  if(argc == 3){
    init(argv[1],NULL,argv[2]);
  }else{
    //filename,dbfilename,load_str
    init(argv[1],argv[2],argv[3]);
  }
  thread one(produce);
  thread two(consume);

  one.join();
  two.join();
  while(true)
    sleep(1);
  return 0;
}
