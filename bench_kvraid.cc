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
#include "util/debug.hh"
#include "define.hh"
#include "kvserver.hh"
#include "stdlib.h"     // srand(), rand()
#include "string.h"
using namespace std;
#define KVBUFFER_LENGTH 6
#define LIST_LENGTH 5
#define KEYSIZE 24
#define VALUESIZE 65536
#define DISK_SIZE   ((unsigned long long )1024 * 1024 * 1024 * 20) // 20GB
using std::mutex;
using std::cout;
using std::endl;
typedef struct kv_pair{
  char key[KEYSIZE+1];
  char value[VALUESIZE];
  len_t keyLength;
  len_t valueLength;
  long long timestamp;
  char operation;
}kv_pair;

typedef struct kvBuffer{
  int length;
  kv_pair kvs[KVBUFFER_LENGTH];
}kvBuffer;
kvBuffer checkBuffer;
list<kvBuffer *> consume_list,recycle_list;
std::mutex mut1,mut2;
std::condition_variable data_cond1,data_cond2;
bool more_data_to_produce;
// leveldb::DB *db;
// leveldb::Options ops;
// leveldb::Status status;
FILE *fp;
static timeval ycsb_begin_time;
long long sum_time;
unsigned long error_count;
unsigned long read_count;
unsigned long record_count;
unsigned long key_count;
bool LOAD_FLAG = false;
RandomGenerator rdgen;
KvServer *kvserver = NULL;

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
    kvp->keyLength = 0;
    kvp->valueLength = 0;

    //value length
    length=atoi(line);
    pos = strchr(line,',');
    pos++;
    //operation
    kvp->operation = *pos;
    pos = strchr(pos,',');
    pos++;
    //key
    strncpy(kvp->key,pos,KEYSIZE);
    kvp->keyLength = KEYSIZE;
    //value
    if(LOAD_FLAG){
      if(kvp->operation == 'R'){
	strncpy(kvp->value,rdgen.Generate(length).data(),length);
	kvp->valueLength = length;
      }else{
	kviter--;
      }
    }else{
      if(length > 0){// read hava no values
	strncpy(kvp->value,rdgen.Generate(length).data(),length);
	kvp->valueLength = length;
	/*if(checkBuffer.length < 6){
	  strncpy(checkBuffer.kvs[checkBuffer.length].key,kvp->key,KEYSIZE);
	  strncpy(checkBuffer.kvs[checkBuffer.length].value,kvp->value,length);
	  checkBuffer.kvs[checkBuffer.length].keyLength = KEYSIZE;
	  checkBuffer.kvs[checkBuffer.length].valueLength = length;
	  checkBuffer.length++;
	  cout<<kvp->key<<endl;
	  cout<<kvp->value<<endl;
	}*/
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
  char *readValue = NULL;
  len_t valueSize = 0;
  kv_pair *kvp = NULL;
  struct timeval start_time;
  struct timeval end_time;
  //  struct timeval now_time;
  long long diff;
  bool resFlag = false;
  for(kviter = 0 ; kviter < kvb->length ; kviter++){
    kvp = &kvb->kvs[kviter];
    record_count++;
    gettimeofday(&start_time,NULL);
    if(kvp->operation == 'R'){
      if(LOAD_FLAG){
	resFlag = kvserver->setValue(kvp->key,kvp->keyLength,kvp->value,kvp->valueLength);
      }
      else{
	resFlag = kvserver->getValue(kvp->key,kvp->keyLength,readValue,valueSize);
	read_count++;
      }
    }else{
      // cout<<"value:length:"<<(kvp->value).length()<<endl;
      resFlag = kvserver->setValue(kvp->key,kvp->keyLength,kvp->value,kvp->valueLength);
    }      
    if(!resFlag){
      kvp->key[kvp->keyLength] = 0;
      cout<<"error in key:"<<kvp->key<<"operation"<<kvp->operation<<endl;
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

void init(char filename[],char *diskPaths[],int numDisks){
  int i;
  fp = fopen(filename,"r");
  error_count = 0;
  read_count=0;
  record_count=0;
  key_count=0;
  read_latency_sum = 0;
  write_latency_sum = 0;
  checkBuffer.length = 0;
  if( fp == NULL ){
    printf("error\n");
  }
  fprintf(stderr,"> Beginning of test\n");
  ConfigMod::getInstance().setConfigPath("config.ini");
  std::vector<DiskInfo> *disks= new std::vector<DiskInfo>;
  for(i = 0 ; i < numDisks ; i++){
    DiskInfo *disk = new DiskInfo(i,diskPaths[i],DISK_SIZE);
    disks->push_back(*disk);
  }
  DiskMod *diskMod = new DiskMod(*disks);
  kvserver = new KvServer(diskMod);
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
      std::cout<<"read_average_latency:"<<read_latency_sum*1.0/read_count<<"micros"<<endl;
      std::cout<<"write_average_latency:"<<write_latency_sum*1.0/(record_count - read_count)<<"micros"<<endl;
      //  int i;
      // leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
      /*    for (it->SeekToFirst(),i=0; it->Valid() && i < 5; it->Next(),i++) {
	    cout << it->key().ToString() << ": "  << it->value().ToString() << endl;
	    }*/
      gettimeofday(&ycsb_end_time,NULL);
      compute_diff(ycsb_end_time,ycsb_begin_time,diff);
      std::cout<<"ycsb iops:"<<(diff*1.0/100)/(record_count)<<"ms/op"<<endl;
      // std::string stat_str;
      // db->GetProperty("leveldb.stats",&stat_str);
      // std::cout<<stat_str<<std::endl;
      // delete db;
     /*  len_t valueSize;
	 bool resFlag = false; 
         char *readValue=NULL;

     for(int i = 0 ; i < 6 ; i++){
      	cout<<checkBuffer.kvs[i].key<<endl;
      	resFlag = kvserver->getValue(checkBuffer.kvs[i].key,checkBuffer.kvs[i].keyLength,readValue,valueSize);
      	if(!resFlag){
      	  cout<<"not found"<<endl;
      	}else{
	  if(strncmp(readValue,checkBuffer.kvs[i].value,valueSize) == 0){
	    cout<<"correct"<<endl;
	  }
      	  cout<<readValue<<endl;
      	}
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
  
  init(argv[1],argv+2,atoi(argv[argc-1]));
  thread one(produce);
  thread two(consume);
  gettimeofday(&ycsb_begin_time,NULL);
  one.join();
  two.join();
  while(true)
    sleep(1);
  return 0;
}
