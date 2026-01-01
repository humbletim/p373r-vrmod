#include "llstring.h"
#include <iostream>

// Verify we can use llutf16string without tsoob.c++
int main() {
    // Test 1: Construct llutf16string from wchar_t*
    const wchar_t* wstr = L"Hello World";
    llutf16string utf16(wstr);

    // Test 2: wcsncpy with mismatched types (if that's what tsoob fixes)
    // tsoob.c++ has: inline wchar_t* wcsncpy(wchar_t* dest, const unsigned short* src, size_t count)
    wchar_t dest[20];
    const unsigned short* src = (const unsigned short*)L"Test";
    // This is what we expect to fail without the overload if strict types are enforced
    // wcsncpy(dest, src, 10);

    std::cout << "Compilation Successful" << std::endl;
    return 0;
}
