#include "../src/rename.h"
#include <cassert>
#include <iostream>
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
    std::cout<<"Filename validation and extension preservation passed.\n";
}
