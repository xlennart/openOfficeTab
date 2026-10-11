#pragma once
#include <algorithm>
namespace tablayout {
constexpr int minWidth=80,maxWidth=360;
inline bool validWidth(int width,int mode){return width>=minWidth&&width<=maxWidth&&mode>=0&&mode<=2;}
inline int preferred(int width,int mode,int textWidth,int available,int count){
    width=std::clamp(width,minWidth,maxWidth);
    if(mode==1)return std::clamp(textWidth+46,minWidth,width);
    if(mode==2)return std::min(width,std::max(minWidth,available/std::max(1,count)));
    return width;
}
}
