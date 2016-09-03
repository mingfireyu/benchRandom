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
#include<unistd.h>
#include"buildRandomValue.h"
#include"slice.h"
using namespace std;
#define KVBUFFER_LENGTH 100
#define LIST_LENGTH 5
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
void make_data(kvBuffer *kvb,bool& flag){

  char ch;
  char number[10];
  //  int readcount = 0;
  int length;
  int kviter = 0;
  int i ;
  kv_pair* kvp = NULL;
  
  while((ch = fgetc(fp))!=EOF && kviter < KVBUFFER_LENGTH){
    kvp = &kvb->kvs[kviter];    
    kvp->key.clear();
    kvp->value.clear();

    while(!(ch >= '0' && ch <='9')){
      ch = fgetc(fp);
      if(ch == EOF)
	break;
    }
    if(ch == EOF){
      flag = false;
      break;
    }

    kviter++;

    for(i = 0 ;ch >= '0' && ch <= '9' ; ch = fgetc(fp),i++){
      number[i] = ch;
    }
    number[i] = '\0';
    length = atoi(number);

    ch = fgetc(fp);
    for(i = 0 ;ch >= '0' && ch <= '9' ; ch = fgetc(fp),i++){
      number[i] = ch;
    }
    number[i] = '\0';
    kvp->timestamp = atoll(number);

    kvp->operation = ch;
    if(ch == 'R'){
      read_count++;
      for(i = 0 ; i < 3 ; i++){
	fgetc(fp);
      }
    }
    else{
      for(i = 0 ; i < 4 ; i++){
	fgetc(fp);
      }
    }

    ch = fgetc(fp);
    for(i = 0 ; ch != '\n' ; ch = fgetc(fp),i++){
	(kvp->key).append(1,ch);
    }
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
      (kvp->value).append(rdgen.Generate(length).data(),length);
      if(kvp->value.size() != length){
	cout<<"copy error"<<endl;
      }
    }

  }
  if(ch == EOF){
    flag = false;
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

  for(kviter = 0 ; kviter < kvb->length ; kviter++){
    kvp = &kvb->kvs[kviter];
    // gettimeofday(&now_time,NULL);
    // compute_diff(now_time,begin_time,diff);
    // while(kvp->timestamp > diff){
    //   //        usleep(kvp->timestamp - diff);
    // 	gettimeofday(&now_time,NULL);
    // 	compute_diff(now_time,begin_time,diff);
    // }
    // usleep(50000);
    record_count++;
    gettimeofday(&start_time,NULL);
    if(kvp->operation == 'R'){
      if(LOAD_FLAG){
	status = db->Put(leveldb::WriteOptions(),kvp->key,kvp->value);
      }
      else{
	status = db->Get(leveldb::ReadOptions(),kvp->key, &value);
      }
      if(!status.ok()){
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
   
  }
  
}

void init(char filename[],char dbfilename[],char load_str[]){
  int i;
  fp = fopen(filename,"r");
  error_count = 0;
  read_count=0;
  record_count=0;
  key_count=0;
  if( fp == NULL ){
    printf("error\n");
  }
  ops.create_if_missing = true;
  ops.compression = leveldb::kNoCompression;   
  if(load_str[0] == 'l'){
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
      std::cout<<(sum_time*1.0/100)/(record_count)<<"ms/op"<<endl;
      //  int i;
      // leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
      /*    for (it->SeekToFirst(),i=0; it->Valid() && i < 5; it->Next(),i++) {
	    cout << it->key().ToString() << ": "  << it->value().ToString() << endl;
	    }*/
      gettimeofday(&ycsb_end_time,NULL);
      compute_diff(ycsb_end_time,ycsb_begin_time,diff);
      std::cout<<"ycsb iops:"<<(diff*1.0/100)/(record_count)<<"ms/op"<<endl;
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
  bool flag = true;
  while(true){
    std::unique_lock<std::mutex> lk2(mut2);
    data_cond2.wait(lk2,[]{return !recycle_list.empty();});
    kvb = recycle_list.front();
    recycle_list.pop_front();
    /*    if(recycle_list.empty()){
      cout<<"waiting for recycle list"<<endl;
      }*/
    lk2.unlock();
    make_data(kvb,flag);
    if(!flag){
      more_data_to_produce = false;  //no more data to produce
      cout<<"load data end!"<<endl;
    }
    if(kvb->length == 0){
      cout<<"produce end"<<endl;
      return  ;
    }

    std::unique_lock<std::mutex> lk1(mut1);
    consume_list.push_back(kvb);
    lk1.unlock();
    data_cond1.notify_one();

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
  gettimeofday(&ycsb_begin_time,NULL);
  one.join();
  two.join();
  while(true)
    sleep(1);
  return 0;
}
