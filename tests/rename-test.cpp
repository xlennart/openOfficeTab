#include "../src/rename.h"
#include <cassert>
#include <iostream>
#include <stdexcept>
int main(){
    using namespace documentrename;
    assert(leaf(L"C:\\docs\\report.final.docm")==L"report.final.docm");
    assert(stem(L"C:\\docs\\report.final.docm")==L"report.final");
    assert(extension(L"\\\\server\\share\\report.xlsx")==L".xlsx");
    assert(extension(L"C:\\with.dot\\report").empty());
    assert(target(L"C:\\docs\\report.pptm",L"new")==L"C:\\docs\\new.pptm");
    assert(target(L"\\\\server\\share\\report.xlsx",L"new")==L"\\\\server\\share\\new.xlsx");
    assert(target(L"report.docx",L"new").empty());
    assert(validStem(L"report.final",L".docx")&&validStem(L"\u62a5\u544a & [2]",L".xlsx"));
    for(auto name:{L"",L".",L"..",L"trailing.",L"trailing ",L"../escape",L"folder\\escape",L"drive:name",L"bad?",L"bad*",L"bad\"",L"bad<",L"bad>",L"bad|",L"bad\n",L"CON",L"prn",L"Aux.doc",L"NUL",L"COM1",L"lpt9",L"COM\u00b9",L"LPT\u00b2",L"CON .extra",L"CONIN$",L"CONOUT$"})assert(!validStem(name,L".docx"));
    assert(validStem(L"COM10",L".docx")&&validStem(L"console",L".docx"));
    assert(validStem(std::wstring(250,L'a'),L".docx"));assert(!validStem(std::wstring(251,L'a'),L".docx"));
    int saved=0,verified=0,removed=0;
    auto save=[&]{++saved;return true;};auto verify=[&]{assert(saved>0);++verified;return true;};auto remove=[&]{assert(verified>0);++removed;return true;};
    assert(perform(save,verify,remove)==Result::renamed&&removed==1);
    saved=verified=removed=0;
    assert(perform([]{return false;},verify,remove)==Result::failed&&verified==0&&removed==0);
    assert(perform(save,[]{return false;},remove)==Result::failed&&removed==0);
    assert(perform(save,verify,[]{return false;})==Result::originalRetained&&removed==0);
    try{perform([]()->bool{throw std::runtime_error("COM rejected");},verify,remove);assert(false);}catch(const std::runtime_error&){}
    assert(removed==0);
    try{perform(save,[]()->bool{throw std::runtime_error("verification rejected");},remove);assert(false);}catch(const std::runtime_error&){}
    assert(removed==0);
    std::cout<<"Filename validation, extension preservation and safe save/verify/remove ordering passed.\n";
}
