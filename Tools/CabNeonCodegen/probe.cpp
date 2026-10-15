#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <vector>
#include <time.h>
using Fn=void(*)(const float*,const float*,float*,int);
extern "C" {
void scalar(const float*,const float*,float*,int);
void neon4(const float*,const float*,float*,int);
void neon8(const float*,const float*,float*,int);
void neon16(const float*,const float*,float*,int);
void scalar_ic1(const float*,const float*,float*,int);
void scalar_ic2(const float*,const float*,float*,int);
void bound4(const float*,const float*,float*,int);
void bound8(const float*,const float*,float*,int);
}
const Fn funcs[]={scalar,neon4,neon8,neon16,scalar_ic1,scalar_ic2,bound4,bound8};
const char* names[]={"scalar","neon4","neon8","neon16","scalar_ic1","scalar_ic2","bound4","bound8"};
double cpu(){timespec t{};if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t))std::abort();return t.tv_sec+1e-9*t.tv_nsec;}
int main(int argc,char**argv){
 std::mt19937 rng(73015);
 if(argc==1){
  uint64_t samples=0;
  // Misaligned plane/partition starts, all production FFT bin counts, short and odd tails.
  for(int n:{1,2,3,4,5,7,8,9,15,16,17,33,65,128,129,256,257,512,513,1024})
   for(int offset=0;offset<4;++offset)for(int trial=0;trial<80;++trial){
    const int len=2*n+offset+8;
    std::vector<float>x(len),h(len),seed(len);
    for(auto* v:{&x,&h,&seed})for(float&f:*v){
     if(trial<60)f=std::ldexp(float(int(rng()%20001)-10000)/10000.f,int(rng()%81)-40);
     else {const uint32_t bits[]={0,0x80000000,1,0x80000001,0x007fffff,0x807fffff,0x00800000,0x80800000,0x3f800000,0xbf800000};f=std::bit_cast<float>(bits[rng()%10]);}
    }
    auto ref=seed; scalar(x.data()+offset,h.data()+offset,ref.data()+offset,n);
    for(int f=1;f<8;++f){auto out=seed;funcs[f](x.data()+offset,h.data()+offset,out.data()+offset,n);
     if(std::memcmp(out.data(),ref.data(),len*sizeof(float))){
      for(int i=0;i<len;++i)if(std::bit_cast<uint32_t>(out[i])!=std::bit_cast<uint32_t>(ref[i])){
       std::cerr<<"BIT_FAIL "<<names[f]<<" bins="<<n<<" offset="<<offset<<" trial="<<trial<<" index="<<i<<" reference="<<std::hex<<std::bit_cast<uint32_t>(ref[i])<<" actual="<<std::bit_cast<uint32_t>(out[i])<<"\n";break;}
      return 1;
     }
     samples+=2*n;
    }
   }
  std::cout<<"exact_bits=1 guarded_samples="<<samples<<"\n";return 0;
 }
 int f=std::atoi(argv[1]),n=std::atoi(argv[2]),parts=std::atoi(argv[3]);
 if(f<0||f>=8||n<1||parts<1)return 2;
 std::vector<float>x(2*n*parts),h(x.size()),out(2*n);
 for(auto*v:{&x,&h})for(auto&a:*v)a=float(int(rng()%20001)-10000)*1e-6f;
 Fn fn=funcs[f];
 auto batch=[&](int count){for(int k=0;k<count;++k){std::fill(out.begin(),out.end(),0.f);for(int p=0;p<parts;++p)fn(x.data()+2*n*p,h.data()+2*n*p,out.data(),n);}};
 batch(20); const int reps=2000;
 double start=cpu();batch(reps);double duration=cpu()-start;
 uint32_t hash=2166136261u;for(float a:out)hash=(hash^std::bit_cast<uint32_t>(a))*16777619u;
 std::cout<<names[f]<<','<<n<<','<<parts<<','<<duration*1e9/(reps*parts)<<','<<hash<<'\n';
}
