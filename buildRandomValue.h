#include<string>
#include<iostream>
#include"slice.h"
#include"random.h"


using namespace std;
#define FLAGS_compression_ratio 1.0
class RandomGenerator {
 private:
  std::string data_;
  int pos_;

 public:
  RandomGenerator() {
    // We use a limited amount of data over and over again and ensure
    // that it is larger than the compression window (32KB), and also
    // large enough to serve all typical value sizes we want to write.
    Random rnd(301);
    std::string piece;
    while (data_.size() < 10485760) {
      // Add a short fragment that is as compressible as specified
      // by FLAGS_compression_ratio.
      CompressibleString(&rnd, FLAGS_compression_ratio, 100, &piece);
      data_.append(piece);
    }
    pos_ = 0;
  }

  Slice Generate(size_t len) {
    if (pos_ + len > data_.size()) {
      pos_ = 0;
      if(len > data_.size()){
	cout<<"len"<<len<<endl;
	assert(len < data_.size());
      }
    }
    pos_ += len;
    return Slice(data_.data() + pos_ - len, len);
  }
};


