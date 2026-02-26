#pragma once
//
// FOAF "Design Pattern" (friend-of-a-friend protected access):
//
//   class Foo { protected: int bar; } foo;
//
//   // printf("foo.bar = %d\n", foo.bar); 
//   // ERROR: 'bar' is a protected member of 'Foo'
//
//   printf("foo.bar = %d\n", via_foaf(&foo, { return bar; }));
//   // OK! (coming in via trusted foaf access)
//
//   // NOTE: only use this pattern as a last resort... -humbletim

/*
// TEST CASE
#include <cstdio>
#include "humbletim/foaf.hpp"

class Foo { protected: int bar; } foo;

int main() {
    printf("foo.bar = %d\n", via_foaf(&foo, { return bar; }));
    return 0;
}
*/

#include <type_traits>
template<typename T> auto via_foaf_unwrap(T&& ptr) -> decltype(&*ptr) { return &*ptr; }
#define via_foaf(PTR, ...) \
    ([&]() -> decltype(auto) { \
        auto* _raw = via_foaf_unwrap(PTR); \
        /* Strip const/volatile to allow inheritance */ \
        using _BaseType = typename std::remove_cv<typename std::remove_pointer<decltype(_raw)>::type>::type; \
        /* The FOAF */ \
        struct _FOAF : public _BaseType { \
            auto _access() -> decltype(auto) __VA_ARGS__ \
        }; \
        /* Cast away const and type to force access */ \
        return reinterpret_cast<_FOAF*>(const_cast<_BaseType*>(_raw))->_access(); \
    }())
