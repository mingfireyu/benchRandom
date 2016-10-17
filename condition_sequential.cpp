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
#define KEY_COUNT 1350000
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
static timeval begin_time;
long long sum_time;
unsigned long error_count;
unsigned long read_count;
unsigned long record_count;
unsigned long key_count;
unsigned long final_size;
RandomGenerator rdgen;
void make_data(kvBuffer *kvb,bool& flag){



  //  int readcount = 0;
  int length = 8934;
  int kviter = 0;
  kv_pair* kvp = NULL;  
  while(kviter < KVBUFFER_LENGTH && key_count < KEY_COUNT){
    kvp = &kvb->kvs[kviter];    
    kvp->key.clear();
    kvp->value.clear();
    char key[100];
    snprintf(key, sizeof(key), "%016lu", key_count++);
    //cout<<key<<endl;
    kvp->key.append(key,16);
    kvp->value.append(rdgen.Generate(length).data(),length);
    kviter++;
    kvp->operation='W';
  }
  if(key_count == KEY_COUNT){
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
    record_count++;
    gettimeofday(&start_time,NULL);
    status = db->Put(leveldb::WriteOptions(),kvp->key,kvp->value);

    if(!status.ok()){
      cout<<status.ToString()<<endl;
    }
    final_size += kvp->value.length();
    gettimeofday(&end_time,NULL);
    compute_diff(end_time,start_time,diff);    
    sum_time = sum_time + diff;    
  }
  
}

void init(char dbfilename[]){
  int i;
  final_size = 0;
  error_count = 0;
  read_count=0;
  record_count=0;
  key_count=0;
  ops.create_if_missing = true;
  ops.compression = leveldb::kNoCompression;   
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
  data_cond1.wait(lk,[]{return !consume_list.empty();});
  kvb = consume_list.front();
  consume_list.pop_front();
  lk.unlock();
  gettimeofday(&begin_time,NULL);
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
      std::cout<<"final_size:"<<final_size<<endl;
      std::cout<<record_count*1.0/(sum_time*1.0/1000000)<<"op/s"<<endl;
      std::cout<<(sum_time*1.0/100)/(record_count)<<"ms/op"<<endl;
      //  int i;
      // leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
      /*    for (it->SeekToFirst(),i=0; it->Valid() && i < 5; it->Next(),i++) {
	    cout << it->key().ToString() << ": "  << it->value().ToString() << endl;
	    }*/
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


