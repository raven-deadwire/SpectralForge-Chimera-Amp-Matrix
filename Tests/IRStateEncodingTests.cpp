#include "IRStateEncoding.h"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
}

int main() {
    try {
        using namespace spectralforge::irState;
        juce::MemoryBlock empty,decoded;
        require(empty.toBase64Encoding()=="0.","JUCE empty-block contract changed");
        require(decodeAsset("0.",decoded) && decoded.getSize()==0,"empty block rejected");
        for(size_t length=0;length<258;++length) {
            juce::MemoryBlock input(length);
            auto* data=static_cast<unsigned char*>(input.getData());
            for(size_t i=0;i<length;++i)data[i]=static_cast<unsigned char>((i*137+length)&255);
            const auto encoded=input.toBase64Encoding();
            require(decodeAsset(encoded,decoded) && decoded==input,"canonical JUCE data changed");
            require(decodeAsset(encoded+"...",decoded) && decoded==input,"legacy trailing dots rejected");
        }
        const char* malformed[]={"", ".", "-1.A", "-2147483648.A", "4194305.A", "2147483647.A",
            "4294967296.A", "999999999999999999999.A", "+1.A", "1x.A", "one.A", "@@ not JUCE base64 @@"};
        for(const auto* encoded:malformed) {
            juce::MemoryBlock sentinel("safe",4);
            require(!decodeAsset(encoded,sentinel),"malformed size accepted");
            require(sentinel==juce::MemoryBlock("safe",4),"rejected prefix mutated destination");
        }
        juce::MemoryBlock largest(maxAssetBytes,true);
        static_cast<char*>(largest.getData())[maxAssetBytes-1]=42;
        require(decodeAsset(largest.toBase64Encoding(),decoded) && decoded==largest,"4 MiB IR rejected");
        require(!decodeAsset(juce::String::repeatedString(".",6*1024*1024+1),decoded),"encoded size limit lost");
        std::cout<<"PASS IR state encoding: 258 canonical/padded round trips, empty 0., 12 malformed prefixes, 4 MiB boundary\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
