#ifndef notsoob
#define notsoob
// ===========================================================================
// MSVC <=> LLVM wchar_t -- cross-toolchain workarounds

#include "llpreprocessor.h"  
#undef LL_WCHAR_T_NATIVE  
#include "stdtypes.h"  
#include "llstring.h"  

// #if defined(__clang__) && defined(_WIN32)  
namespace {  
    template<typename T>  
    inline llutf16string make_llutf16string(T* ptr) {  
        return llutf16string(reinterpret_cast<const U16*>(ptr));  
    }  
      
    // hybrid wrapper neeed for handling ostensibly mixed (wchar_t <=> unsigned short) wcsncpy  
    inline wchar_t* wcsncpy(wchar_t* dest, const unsigned short* src, size_t count) {  
        return ::wcsncpy(dest, reinterpret_cast<const wchar_t*>(src), count);  
    }  
}  
#define llutf16string(x) make_llutf16string(x)  

// #endif

// ===========================================================================


#include "llviewerprecompiledheaders.h"
#include "lldynamictexture.h"
// #include "llfloatermodelpreview.h"
#include "llmeshrepository.h"
#include "llmodelloader.h" //NUM_LOD
#include "llmodel.h"
#include "llviewermenufile.h" // llfloatermodelpreview

#include <boost/json.hpp>
#include <boost/function.hpp>
#include <boost/compressed_pair.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/regex.hpp>
#include <boost/variant.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/token_functions.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/algorithm/string/find_format.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/function_types/property_tags.hpp>
#include <boost/function_types/components.hpp>
#include <boost/type_traits.hpp>
#include "llcond.h"
#include "llrand.h"
#include "llcachename.h"

namespace actualboost {
    using namespace boost;
}
#define boost tsoob

class LLFolderViewItem;
namespace boost {
    // template <typename Signature>
    // using function = std::function<Signature>; // Requires #include <functional>
    using actualboost::function;

    namespace signals2 {
        // using actualboost::function;
        using actualboost::signals2::connection;
        using actualboost::signals2::scoped_connection;
        using actualboost::signals2::trackable;
         template <typename Signature, typename xFunctionWrapper>
         class slot : public std::function<Signature> {public:
            using FunctionWrapper = std::function<Signature>;//::FunctionWrapper;
            template <typename T> slot(T&& cb) : FunctionWrapper(std::forward<T>(cb)) {}
         };
        template <typename F, typename A = void*> struct signal {
            using slot_type = slot<F, boost::function<F>>;
            std::vector<std::function<F>> cbs;
            inline connection connect(std::function<F> const& cb) {
                cbs.push_back(cb);
                return {};
            }
            inline void operator()(F32 a, F32 b, F32 c) { for (const auto& i : cbs) i(a,b,c); }
            inline void operator()(bool a) { for (const auto& i : cbs) i(a); }
            inline void operator()() { for (const auto& i : cbs) i(); }
            inline void operator()(const std::vector<std::string>& a, std::string b) { for (const auto& i : cbs) i(a,b); }
            inline void operator()(const std::deque<LLFolderViewItem*>& a, bool b) { for (const auto& i : cbs) i(a,b); }

            
            
            // void disconnect_all_slots() { cbs.clear(); }

        };
    }
    namespace algorithm { 
        using namespace actualboost::algorithm;
        namespace detail { using namespace actualboost::algorithm::detail; }
    }
    using actualboost::current_exception_diagnostic_information;
    using actualboost::function_traits;
    namespace mpl { using namespace actualboost::mpl; }
    namespace iostreams { using namespace actualboost::iostreams; }
    namespace noncopyable_ { using namespace actualboost::noncopyable_; }
    // namespace hash { using namespace actualboost::hash; }
    namespace function_types { using namespace actualboost::function_types; }
    // using actualboost::function_types::nonmember_callable_builtin_tag; 
    // using actualboost::function_types::callable_builtin_tag; 

    using actualboost::as_literal;
    using actualboost::variant;
    // using actualboost::base_integer_type;
    using actualboost::get;
    using actualboost::blank;
    using actualboost::add_reference;
    using actualboost::add_const;
    using actualboost::add_volatile;
    using actualboost::conjunction;
    using actualboost::is_enum;
    using actualboost::is_signed;
    using actualboost::is_base_of;
    using actualboost::is_base_and_derived;
    using actualboost::is_reference;
    using actualboost::is_void;
    using actualboost::is_volatile;
    using actualboost::is_const;
    using actualboost::is_volatile;
    using actualboost::is_complete;
    using actualboost::is_constructible;
    using actualboost::is_destructible;
    using actualboost::has_trivial_destructor;
    using actualboost::has_trivial_move_assign;
    using actualboost::has_trivial_copy;
    using actualboost::has_range_iterator;
    using actualboost::enable_if;
    using actualboost::has_trivial_assign;
    using actualboost::make_void;
    using actualboost::move;
    using actualboost::condition_variable;
    using actualboost::unique_lock;
    using actualboost::mutex;
    namespace filesystem {
        using actualboost::filesystem::last_write_time;
        using actualpath = actualboost::filesystem::path;
        using actualboost::filesystem::current_path;
        struct path : actualpath {
            using actualpath::actualpath;
            inline path(std::basic_string<unsigned short> const& src) : path(reinterpret_cast<const wchar_t*>(src.data())) {}
        };
    }
    using actualboost::function2;
    using actualboost::optional;
    //using actualboost::set_value_type;

    using actualboost::true_type;
    using ulong_ulong_type = unsigned long long;
    using ulong_long_type = unsigned long long;
    using long_long_type = long long;
    using actualboost::distance;
    using actualboost::none;
    using actualboost::negation;
    using actualboost::begin;
    using actualboost::end;
    using actualboost::ref;
    using actualboost::source_location;
    using actualboost::lexical_cast;
    using actualboost::bad_lexical_cast;
    using actualboost::result_of;
    using actualboost::scoped_array;
    using actualboost::is_final;
    using actualboost::is_fundamental;
    using actualboost::is_function;
    using actualboost::char_separator;
    using actualboost::tokenizer;
    using actualboost::false_type;
    using actualboost::noncopyable;
    using actualboost::declval;
    using actualboost::ptr_vector;
    using actualboost::regex;
    using actualboost::sregex_iterator;
    using actualboost::smatch;
    using actualboost::regex_error;
    
    using actualboost::nullable;
    using actualboost::intrusive_ptr;
    using actualboost::enable_if_c;
    using actualboost::is_same;
    using actualboost::is_floating_point;
    using actualboost::integral_constant;
    using actualboost::throw_exception;
    using actualboost::hash_range;
    using actualboost::hash_value;
    using actualboost::hash;
    using actualboost::hash_combine;
    using actualboost::conditional;
    using actualboost::filter_iterator;
    using actualboost::range_iterator;
    using actualboost::range_difference;
    using actualboost::range_const_iterator;
    using actualboost::range_mutable_iterator;
    using actualboost::empty;
    using actualboost::make_iterator_range;
    using actualboost::make_filter_iterator;
    using actualboost::make_transform_iterator;
    using actualboost::forward_traversal_tag;
    using actualboost::bidirectional_traversal_tag;
    using actualboost::random_access_traversal_tag;
    
    using actualboost::remove_const;
    namespace concepts { using namespace actualboost::concepts; }
    using actualboost::iterator_facade;
    using actualboost::iterator_core_access;
    using actualboost::transform_iterator;
    
    using actualboost::iterator_range;
    using actualboost::range_size;
    using actualboost::range_value;
    using actualboost::bind;
    using actualboost::is_empty;
    using actualboost::is_array;
    using actualboost::is_convertible;
    using actualboost::iterator_reference;
    using actualboost::iterator_pointer;
    using actualboost::iterator_category;
    using actualboost::iterator_traversal;
    using actualboost::is_pointer;
    namespace type_traits { using namespace actualboost::type_traits; }
    namespace fibers { using namespace actualboost::fibers; }
    using actualboost::is_integral;
    using actualboost::remove_cv;
    using actualboost::remove_reference;
    
    using actualboost::remove_bounds;
    using actualboost::compressed_pair;
    using actualboost::reverse_iterator;
    namespace json { using namespace actualboost::json; }
    using namespace boost;
    // using namespace boost;
}

#include "llmodelpreview.h"

#endif
