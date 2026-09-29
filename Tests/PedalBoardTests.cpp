#include "PedalBoardTests.h"
int main() {
    std::cout<<std::unitbuf;
    try {pedalBoardTests::run();return 0;}
    catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
