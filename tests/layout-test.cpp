#include "../src/layout.h"
#include <cassert>
#include <iostream>
int main(){
    using namespace tablayout;
    assert(!validWidth(79,0)&&!validWidth(361,0)&&!validWidth(120,-1)&&!validWidth(120,3));
    assert(validWidth(80,0)&&validWidth(360,2));
    assert(preferred(200,0,500,100,30)==200);
    assert(preferred(200,1,10,500,1)==80);
    assert(preferred(200,1,100,500,1)==146);
    assert(preferred(200,1,500,500,1)==200);
    assert(preferred(200,2,500,500,5)==100);
    assert(preferred(200,2,500,60,50)==80);
    assert(preferred(120,2,100,1000,0)==120);
    for(int width=80;width<=360;width++)for(int mode=0;mode<3;mode++)for(int count=1;count<=50;count++){
        int value=preferred(width,mode,count*20,800,count);assert(value>=80&&value<=width);
    }
    std::cout<<"Width bounds, title sizing, compression and 50-tab cases passed.\n";
}
