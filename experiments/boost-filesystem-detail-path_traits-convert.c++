#include <string>  // For std::basic_string
#include <locale>  // For std::codecvt
#include <cwchar>  // For std::mbstate_t
#include <stdio.h> // For printf
#include <windows.h> // For Win32 API calls

// Keep your stderr redirection hack if you need it
// #undef stderr
// FILE* stderr = fopen("stderr.txt", "wt");

// We need the exact types from the linker error.
using CvtFacet = std::codecvt<unsigned short, char, std::mbstate_t>;

// We must define these inside their full C++ namespace
namespace boost {
namespace filesystem {
namespace detail {
namespace path_traits {

/**
 * LINKER PATCH 1 (Wide to Narrow / UTF-16 to UTF-8)
 *
 * Implements:
 * void __cdecl convert(
 * const unsigned short* from_begin,
 * const unsigned short* from_end,
 * std::string& to,
 * const CvtFacet* cvt)
 */
void __cdecl convert(const unsigned short* from_begin,
                     const unsigned short* from_end,
                     std::string& to,
                     const CvtFacet* cvt)
{
    // Log that our *real* patch was hit
    fprintf(stderr,
            "\n[JBWELD_V2_PATCH_HIT]: "
            "boost::filesystem::detail::path_traits::convert(ushort* -> string&) CALLED!\n");
    
    // Suppress unused parameter warning for cvt
    (void)cvt;

    if (from_begin == from_end) {
        to.clear();
        return;
    }

    // Get the length of the input wide string
    int in_len = static_cast<int>(from_end - from_begin);

    // First, call WideCharToMultiByte to get the required buffer size
    // We specify CP_UTF8 as the target encoding.
    int required_size = WideCharToMultiByte(
        CP_UTF8,      // CodePage (UTF-8)
        0,            // dwFlags
        (wchar_t*)from_begin,   // lpWideCharStr
        in_len,       // cchWideChar
        NULL,         // lpMultiByteStr
        0,            // cbMultiByte (0 to get size)
        NULL,         // lpDefaultChar
        NULL          // lpUsedDefaultChar
    );

    if (required_size > 0) {
        // Resize the std::string to fit the output
        to.resize(required_size);

        // Now, perform the actual conversion into the string's buffer
        WideCharToMultiByte(
            CP_UTF8,
            0,
            (wchar_t*)from_begin,
            in_len,
            &to[0],       // Target buffer
            required_size, // Size of target buffer
            NULL,
            NULL
        );
    } else {
        to.clear();
    }
}


/**
 * LINKER PATCH 2 (Narrow to Wide / UTF-8 to UTF-16)
 *
 * Implements:
 * void __cdecl convert(
 * const char* from_begin,
 * const char* from_end,
 * std::basic_string<unsigned short, ...>& to,
 * const CvtFacet* cvt)
 */

// Define the "wide string" type from the error
using WideString = std::basic_string<unsigned short,
                                     std::char_traits<unsigned short>,
                                     std::allocator<unsigned short>>;

void __cdecl convert(const char* from_begin,
                     const char* from_end,
                     WideString& to,
                     const CvtFacet* cvt)
{
    // Log that our *real* patch was hit
    fprintf(stderr,
            "\n[JBWELD_V2_PATCH_HIT]: "
            "boost::filesystem::detail::path_traits::convert(char* -> wstring&) CALLED!\n");
            
    // Suppress unused parameter warning for cvt
    (void)cvt;

    if (from_begin == from_end) {
        to.clear();
        return;
    }

    // Get the length of the input narrow string
    int in_len = static_cast<int>(from_end - from_begin);

    // First, call MultiByteToWideChar to get the required buffer size
    // We assume the input string is UTF-8 (CP_UTF8).
    int required_size = MultiByteToWideChar(
        CP_UTF8,      // CodePage (UTF-8)
        0,            // dwFlags
        from_begin,   // lpMultiByteStr
        in_len,       // cbMultiByte
        NULL,         // lpWideCharStr
        0             // cchWideChar (0 to get size)
    );

    if (required_size > 0) {
        // Resize the wide string to fit the output
        to.resize(required_size);

        // Now, perform the actual conversion into the string's buffer
        MultiByteToWideChar(
            CP_UTF8,
            0,
            from_begin,
            in_len,
            (wchar_t*)&to[0],       // Target buffer
            required_size // Size of target buffer
        );
    } else {
        to.clear();
    }
}

} // namespace path_traits
} // namespace detail
} // namespace filesystem
} // namespace boost

