#pragma once
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#include <boost/core/demangle.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/control/iif.hpp>
#include <boost/preprocessor/facilities/check_empty.hpp>
#include <boost/preprocessor/logical/bitand.hpp>
#include <boost/preprocessor/logical/compl.hpp>
#include <boost/preprocessor/seq/filter.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/for_each_i.hpp>
#include <boost/preprocessor/seq/to_tuple.hpp>
#include <boost/preprocessor/seq/transform.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/tuple/push_front.hpp>
#include <boost/preprocessor/tuple/rem.hpp>
#include <boost/preprocessor/variadic/to_seq.hpp>

#include <psio/get_type_name.hpp>
#include <psio/member_proxy.hpp>

namespace psio
{
   template <typename R, typename C, typename... Args>
   constexpr std::tuple<std::decay_t<Args>...> args_as_tuple(R (C::*)(Args...));  // { return ; }
   template <typename R, typename C, typename... Args>
   constexpr std::tuple<std::decay_t<Args>...> args_as_tuple(R (C::*)(Args...) const)
   {
      return {};
   }
   template <typename R, typename... Args>
   constexpr std::tuple<std::decay_t<Args>...> args_as_tuple(R (*)(Args...))
   {
      return {};
   }

   template <typename Tuple, size_t... I>
   constexpr auto prune_tuple_h(Tuple t, std::index_sequence<I...>)
   {
      return std::make_tuple(std::get<I>(t)...);
   }

   template <typename... Ts>
   constexpr auto prune_tuple(std::tuple<Ts...> t)
   {
      return prune_tuple_h(
          t, std::make_index_sequence<std::tuple_size<std::tuple<Ts...>>::value - 1>{});
   }

   template <typename Tuple, size_t... I>
   constexpr auto get_tuple_args_h(Tuple t, std::index_sequence<I...>)
   {
      return std::make_tuple(args_as_tuple(std::get<I>(t))...);
   }

   template <typename... MPtrs>
   constexpr auto get_tuple_args(std::tuple<MPtrs...> t)
   {
      return get_tuple_args_h(
          t, std::make_index_sequence<std::tuple_size<std::tuple<MPtrs...>>::value>{});
   }

   template <typename... Ts>
   constexpr std::variant<Ts...> tuple_to_variant(std::tuple<Ts...>);

   template <int i, typename T, typename S>
   void tuple_foreach_i(T&& t, S&& f)
   {
      if constexpr (i < std::tuple_size_v<std::decay_t<T>>)
      {
         f(std::get<i>(t));
         tuple_foreach_i<i + 1>(t, std::forward<S>(f));
      }
   }

   template <typename... Ts, typename S>
   void tuple_foreach(const std::tuple<Ts...>& obj, S&& s)
   {
      tuple_foreach_i<0>(obj, std::forward<S>(s));
   }
   template <typename... Ts, typename S>
   void tuple_foreach(std::tuple<Ts...>& obj, S&& s)
   {
      tuple_foreach_i<0>(obj, std::forward<S>(s));
   }

   template <typename R, typename C, typename... Args>
   R result_of(R (C::*)(Args...) const);
   template <typename R, typename C, typename... Args>
   R result_of(R (C::*)(Args...));
   template <typename R, typename... Args>
   R result_of(R (*)(Args...));

   template <typename R, typename C>
   constexpr R result_of_member(R(C::*));
   template <typename R, typename C>
   constexpr C class_of_member(R(C::*));
   template <typename R, typename C, typename... Args>
   constexpr C class_of_member(R (C::*)(Args...));
   template <typename R, typename C, typename... Args>
   constexpr C class_of_member(R (C::*)(Args...) const);

   template <typename R, typename C, typename... Args>
   void result_of_member(R (C::*)(Args...) const);
   template <typename R, typename C, typename... Args>
   void result_of_member(R (C::*)(Args...));

   struct meta
   {
      const char*                        name;
      uint64_t                           offset = 0;
      std::initializer_list<const char*> param_names;
      int32_t                            number;
   };

   template <typename QueryClass>
   struct reflect_undefined
   {
      static constexpr bool is_defined = false;
      static constexpr bool is_struct  = false;
      template <typename L>
      static void get(const std::string_view& m, L&& lambda);
   };

   template <typename QueryClass>
   reflect_undefined<QueryClass> get_reflect_impl(const QueryClass&);

   template <typename QueryClass>
   using reflect = std::decay_t<decltype(get_reflect_impl(std::declval<QueryClass>()))>;

   template <typename>
   struct is_std_vector : std::false_type
   {
   };

   template <typename T, typename A>
   struct is_std_vector<std::vector<T, A>> : std::true_type
   {
      using value_type = T;
   };

   template <typename>
   struct is_std_optional : std::false_type
   {
   };

   template <typename T>
   struct is_std_optional<std::optional<T>> : std::true_type
   {
      using value_type = T;
   };

   template <typename T>
   using is_std_optional_v = typename is_std_optional<T>::value;

   template <typename>
   struct is_std_unique_ptr : std::false_type
   {
   };

   template <typename T>
   struct is_std_unique_ptr<std::unique_ptr<T>> : std::true_type
   {
      using value_type = T;
   };

   template <typename>
   struct is_std_reference_wrapper : std::false_type
   {
   };

   template <typename T>
   struct is_std_reference_wrapper<std::reference_wrapper<T>> : std::true_type
   {
      using value_type = T;
   };

   template <typename>
   struct is_std_array : std::false_type
   {
   };

   template <typename T, auto N>
   struct is_std_array<std::array<T, N>> : std::true_type
   {
      using value_type                        = T;
      constexpr static const std::size_t size = N;
   };

   template <typename T>
   inline constexpr bool is_optional()
   {
      return is_std_optional<T>::value;
   }
   template <typename T>
   inline constexpr bool is_unique_ptr()
   {
      return is_std_unique_ptr<T>::value;
   }
   template <typename T>
   inline constexpr bool is_reference_wrapper()
   {
      return is_std_reference_wrapper<T>::value;
   }

   template <typename T>
   constexpr bool is_array()
   {
      return is_std_array<T>::value;
   }

   template <typename>
   struct is_std_variant : std::false_type
   {
   };

   template <typename... T>
   struct is_std_variant<std::variant<T...>> : std::true_type
   {
      static std::string name() { return get_variant_typename<T...>(); }
      template <typename First, typename... Rest>
      static std::string get_variant_typename()
      {
         if constexpr (sizeof...(Rest) > 0)
            return std::string(get_type_name<First>()) + "|" + get_variant_typename<Rest...>();
         else
            return std::string(get_type_name<First>()) + "|";
      }
      using alts_as_tuple                       = std::tuple<T...>;
      constexpr static const uint16_t num_types = sizeof...(T);
   };

   template <typename>
   struct is_std_tuple : std::false_type
   {
   };

   template <typename... T>
   struct is_std_tuple<std::tuple<T...>> : std::true_type
   {
      static std::string name() { return get_tuple_typename<T...>(); }
      template <typename First, typename... Rest>
      static std::string get_tuple_typename()
      {
         if constexpr (sizeof...(Rest) > 0)
            return std::string(get_type_name<First>()) + "&" + get_tuple_typename<Rest...>();
         else
            return std::string(get_type_name<First>()) + "&";
      }
   };

   template <typename>
   struct is_std_map : std::false_type
   {
   };

   template <typename K, typename V>
   struct is_std_map<std::map<K, V>> : std::true_type
   {
      static const std::string& name()
      {
         static std::string n =
             std::string("map<") + get_type_name<K>() + "," + get_type_name<V>() + ">";
         return n;
      }
   };
   template <typename T>
   struct remove_cvref
   {
      typedef std::remove_cv_t<std::remove_reference_t<T>> type;
   };

   template <typename T>
   using remove_cvref_t = typename remove_cvref<T>::type;

}  // namespace psio

// TODO: not legal to add new definitions to std namespace
namespace std
{
   namespace
   {
      PSIO_REFLECT_TYPENAME(string)
   }
}  // namespace std

#define PSIO_EMPTY(...)
#define PSIO_FIRST(a, ...) a
#define PSIO_APPLY_FIRST(a) PSIO_FIRST(a)
#define PSIO_SKIP_SECOND(a, b, ...) (a, __VA_ARGS__)

#define PSIO_REMOVE_PARENS(...) BOOST_PP_TUPLE_REM() __VA_ARGS__
#define PSIO_SEQ_TO_VA_ARGS(seq) PSIO_REMOVE_PARENS(BOOST_PP_SEQ_TO_TUPLE(seq))

#define PSIO_SEQ_TRANSFORM(op, data, seq) \
   BOOST_PP_IIF(BOOST_PP_CHECK_EMPTY(seq), PSIO_EMPTY, BOOST_PP_SEQ_TRANSFORM)(op, data, seq)

#define PSIO_MATCH(base, x) PSIO_MATCH_CHECK(BOOST_PP_CAT(base, x))
#define PSIO_MATCH_CHECK(...) PSIO_MATCH_CHECK_N(__VA_ARGS__, 0, )
#define PSIO_MATCH_CHECK_N(x, n, r, ...) BOOST_PP_BITAND(n, BOOST_PP_COMPL(BOOST_PP_CHECK_EMPTY(r)))

// Filter out PSIO_DISCARD(...) from seq
#define PSIO_FILTER_DISCARD(seq) BOOST_PP_SEQ_FILTER(PSIO_FILTER_DISCARD_IMPL, _, seq)
#define PSIO_FILTER_DISCARD_IMPL(s, _, elem) \
   BOOST_PP_COMPL(PSIO_MATCH(PSIO_FILTER_DISCARD_IMPL, elem))
#define PSIO_FILTER_DISCARD_IMPLPSIO_DISCARD(...) , 1

// numbered(int, ident)       => ident
// unnumbered(ident)          => ident
// method(ident, arg1, ...)   => ident
#define PSIO_GET_IDENT(x) BOOST_PP_CAT(PSIO_GET_IDENT, x)
#define PSIO_GET_IDENTnumbered(int, ident) ident
#define PSIO_GET_IDENTunnumbered(ident) ident
#define PSIO_GET_IDENTmethod(ident, ...) ident
#define PSIO_GET_IDENThash(ident, ...) ident

// numbered(int, ident) => 1
// unnumbered(ident)    => 0
#define PSIO_IS_NUMBERED(x) BOOST_PP_CAT(PSIO_IS_NUMBERED, x)
#define PSIO_IS_NUMBEREDnumbered(int, ident) 1
#define PSIO_IS_NUMBEREDunnumbered(ident) 0

// numbered(int, ident) => int
#define PSIO_GET_NUMBER(x) BOOST_PP_CAT(PSIO_GET_NUMBER, x)
#define PSIO_GET_NUMBERnumbered(int, ident) int

// numbered(int, ident) => int
// unnumbered(ident)    => i
#define PSIO_NUMBER_OR_AUTO(i, elem) \
   BOOST_PP_CAT(PSIO_NUMBER_OR_AUTO, PSIO_IS_NUMBERED(elem))(i, elem)
#define PSIO_NUMBER_OR_AUTO0(i, ...) i + 1
#define PSIO_NUMBER_OR_AUTO1(i, x) PSIO_GET_NUMBER(x)

// method(ident, arg1, ...) => arg1, ...
#define PSIO_GET_ARGS(x) BOOST_PP_CAT(PSIO_GET_ARGS, x)
#define PSIO_GET_ARGSmethod(ident, ...) __VA_ARGS__

// method(ident, arg1, ...) => "arg1", ...
#define PSIO_GET_QUOTED_ARGS(elem)                                                                \
   BOOST_PP_IIF(BOOST_PP_CHECK_EMPTY(PSIO_GET_ARGS(elem)), PSIO_EMPTY, PSIO_GET_QUOTED_ARGS_IMPL) \
   (elem)
#define PSIO_GET_QUOTED_ARGS_IMPL(elem)                                      \
   PSIO_SEQ_TO_VA_ARGS(BOOST_PP_SEQ_TRANSFORM(PSIO_GET_QUOTED_ARGS_IMPL2, _, \
                                              BOOST_PP_VARIADIC_TO_SEQ(PSIO_GET_ARGS(elem))))
#define PSIO_GET_QUOTED_ARGS_IMPL2(r, _, arg) #arg

// Get seq of data members and methods. Each result is:
// * numbered(int, ident)
// * unnumbered(ident)
// * method(ident, arg1, ...)
#define PSIO_ALL_MEMBERS(...) \
   PSIO_FILTER_DISCARD(       \
       BOOST_PP_SEQ_TRANSFORM(PSIO_MATCH_MEMBER, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)))
#define PSIO_MATCH_MEMBER(r, STRUCT, item)                                             \
   BOOST_PP_IIF(BOOST_PP_CHECK_EMPTY(item), ,                                          \
                BOOST_PP_IIF(PSIO_MATCH(PSIO_MATCH_MEMBER, item), PSIO_MATCHED_MEMBER, \
                             PSIO_REFLECT_UNMATCHED_MEMBER)(STRUCT, item))
#define PSIO_MATCHED_MEMBER(STRUCT, item)                                              \
   PSIO_FIRST PSIO_APPLY_FIRST(BOOST_PP_CAT(PSIO_MATCH_MEMBER, item)) PSIO_SKIP_SECOND \
   BOOST_PP_TUPLE_PUSH_FRONT(PSIO_APPLY_FIRST(BOOST_PP_CAT(PSIO_MATCH_MEMBER, item)), STRUCT)
#define PSIO_REFLECT_UNMATCHED_MEMBER(STRUCT, member) unnumbered(member)

// Get seq of data members. Each result is:
// * numbered(int, ident)
// * unnumbered(ident)
#define PSIO_DATA_MEMBERS(...) \
   PSIO_FILTER_DISCARD(        \
       BOOST_PP_SEQ_TRANSFORM(PSIO_MATCH_DATA_MEMBER, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)))
#define PSIO_MATCH_DATA_MEMBER(r, STRUCT, item)                                                  \
   BOOST_PP_IIF(BOOST_PP_CHECK_EMPTY(item), ,                                                    \
                BOOST_PP_IIF(PSIO_MATCH(PSIO_MATCH_DATA_MEMBER, item), PSIO_MATCHED_DATA_MEMBER, \
                             PSIO_REFLECT_UNMATCHED_MEMBER)(STRUCT, item))
#define PSIO_MATCHED_DATA_MEMBER(STRUCT, item)                                              \
   PSIO_FIRST PSIO_APPLY_FIRST(BOOST_PP_CAT(PSIO_MATCH_DATA_MEMBER, item)) PSIO_SKIP_SECOND \
   BOOST_PP_TUPLE_PUSH_FRONT(PSIO_APPLY_FIRST(BOOST_PP_CAT(PSIO_MATCH_DATA_MEMBER, item)), STRUCT)

// Get seq of methods. Each result is:
// * method(ident, arg1, ...)
#define PSIO_METHODS(...) \
   PSIO_FILTER_DISCARD(   \
       BOOST_PP_SEQ_TRANSFORM(PSIO_MATCH_METHOD, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)))

#define PSIO_MATCH_METHOD(r, STRUCT, item)                                             \
   BOOST_PP_IIF(BOOST_PP_CHECK_EMPTY(item), ,                                          \
                BOOST_PP_IIF(PSIO_MATCH(PSIO_MATCH_METHOD, item), PSIO_MATCHED_METHOD, \
                             PSIO_DISCARD)(STRUCT, item))
#define PSIO_MATCHED_METHOD(STRUCT, item)                                              \
   PSIO_FIRST PSIO_APPLY_FIRST(BOOST_PP_CAT(PSIO_MATCH_METHOD, item)) PSIO_SKIP_SECOND \
   BOOST_PP_TUPLE_PUSH_FRONT(PSIO_APPLY_FIRST(BOOST_PP_CAT(PSIO_MATCH_METHOD, item)), STRUCT)

#define PSIO_HASHS(...) \
   PSIO_FILTER_DISCARD( \
       BOOST_PP_SEQ_TRANSFORM(PSIO_MATCH_HASH, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)))

#define PSIO_MATCH_HASH(r, STRUCT, item)                                                          \
   BOOST_PP_IIF(BOOST_PP_CHECK_EMPTY(item), ,                                                     \
                BOOST_PP_IIF(PSIO_MATCH(PSIO_MATCH_HASH, item), PSIO_MATCHED_HASH, PSIO_DISCARD)( \
                    STRUCT, item))
#define PSIO_MATCHED_HASH(STRUCT, item)                                              \
   PSIO_FIRST PSIO_APPLY_FIRST(BOOST_PP_CAT(PSIO_MATCH_HASH, item)) PSIO_SKIP_SECOND \
   BOOST_PP_TUPLE_PUSH_FRONT(PSIO_APPLY_FIRST(BOOST_PP_CAT(PSIO_MATCH_HASH, item)), STRUCT)

// Handle base(ident)
#define PSIO_MATCH_MEMBERbase(...) (PSIO_DISCARD), 1
#define PSIO_MATCH_DATA_MEMBERbase(...) (PSIO_DISCARD), 1
#define PSIO_MATCH_METHODbase(...) (PSIO_DISCARD), 1

// Handle method(ident, arg1, ...)
#define PSIO_MATCH_HASHhash(...) (PSIO_KEEP_METHOD, __VA_ARGS__), 1

// Handle method(ident, arg1, ...)
#define PSIO_MATCH_MEMBERmethod(...) (PSIO_KEEP_METHOD, __VA_ARGS__), 1
#define PSIO_MATCH_DATA_MEMBERmethod(...) (PSIO_DISCARD), 1
#define PSIO_MATCH_METHODmethod(...) (PSIO_KEEP_METHOD, __VA_ARGS__), 1

#define PSIO_KEEP_METHOD(_, ident, ...) method(ident, __VA_ARGS__)

// Handle numbered(int, ident)
#define PSIO_MATCH_MEMBERnumbered(...) (PSIO_KEEP_NUMBERED, __VA_ARGS__), 1
#define PSIO_MATCH_DATA_MEMBERnumbered(...) (PSIO_KEEP_NUMBERED, __VA_ARGS__), 1
#define PSIO_MATCH_METHODnumbered(...) (PSIO_DISCARD), 1
#define PSIO_KEEP_NUMBERED(_, int, ident) numbered(int, ident)

#define PSIO_TUPLE_TYPE(s, STRUCT, elem) \
   psio::remove_cvref_t<decltype(psio::result_of_member(&STRUCT::PSIO_GET_IDENT(elem)))>

#define PSIO_FOR_EACH_MEMBER(r, STRUCT, i, elem)                                                  \
   {                                                                                              \
      auto off = __builtin_offsetof(STRUCT, PSIO_GET_IDENT(elem));                                \
      (void)lambda(psio::meta{.name = BOOST_PP_STRINGIZE(PSIO_GET_IDENT(elem)), .offset = off,    \
                                                         .number = PSIO_NUMBER_OR_AUTO(i, elem)}, \
                              &STRUCT::PSIO_GET_IDENT(elem));                                     \
   }

#define PSIO_FOR_EACH_HASH(r, STRUCT, elem) PSIO_GET_IDENT(elem)

#define PSIO_FOR_EACH_METHOD(r, STRUCT, elem)                                                     \
   (void)lambda(psio::meta{                                                                       \
       .name =                                                                                    \
           BOOST_PP_STRINGIZE(PSIO_GET_IDENT(elem)), .param_names = {PSIO_GET_QUOTED_ARGS(elem)}, \
           },                                                                                     \
       &STRUCT::PSIO_GET_IDENT(elem));

#define PSIO_GET_BY_STR(r, STRUCT, elem)              \
   if (BOOST_PP_STRINGIZE(PSIO_GET_IDENT(elem)) == m) \
   {                                                  \
      (void)lambda(&STRUCT::PSIO_GET_IDENT(elem));    \
      return true;                                    \
   }

#define PSIO_REQ_COMPRESS(r, STRUCT, elem) \
   static_assert( not require_compressed or psio::is_compressed_name( psio::hash_name(BOOST_PP_STRINGIZE(PSIO_GET_IDENT(elem)))), "method name does not compress" ); \


#define PSIO_GET_BY_NUMBER(r, STRUCT, i, elem)                              \
   case BOOST_PP_CAT(PSIO_NUMBER_OR_AUTO, PSIO_IS_NUMBERED(elem))(i, elem): \
      (void)lambda(&STRUCT::PSIO_GET_IDENT(elem));                          \
      return true;

#define PSIO_GET_MEMBER_BY_NAME(r, STRUCT, i, elem)                                               \
   case psio::hash_name(BOOST_PP_STRINGIZE(PSIO_GET_IDENT(elem))):                                \
      (void)lambda(psio::meta{.name = BOOST_PP_STRINGIZE(PSIO_GET_IDENT(elem)),                   \
                                                         .number = PSIO_NUMBER_OR_AUTO(i, elem)}, \
                              &STRUCT::PSIO_GET_IDENT(elem));                                     \
      return true;

#define PSIO_GET_METHOD_BY_NAME(r, STRUCT, elem)                                              \
   case psio::hash_name(BOOST_PP_STRINGIZE(PSIO_GET_IDENT(elem))):                            \
      (void)lambda(                                                                           \
          psio::meta{.name = BOOST_PP_STRINGIZE(PSIO_GET_IDENT(elem)),                        \
                                                .param_names = {PSIO_GET_QUOTED_ARGS(elem)}}, \
                     &STRUCT::PSIO_GET_IDENT(elem));                                          \
      return true;

#define PSIO_MEMBER_POINTER_IMPL1(s, STRUCT, elem) &STRUCT::PSIO_GET_IDENT(elem)
#define PSIO_MEMBER_POINTER_IMPL2(STRUCT, members) \
   PSIO_SEQ_TO_VA_ARGS(BOOST_PP_SEQ_TRANSFORM(PSIO_MEMBER_POINTER_IMPL1, STRUCT, members))
#define PSIO_MEMBER_POINTER(STRUCT, members)                                          \
   BOOST_PP_IIF(BOOST_PP_CHECK_EMPTY(members), PSIO_EMPTY, PSIO_MEMBER_POINTER_IMPL2) \
   (STRUCT, members)

#define PSIO_PROXY_DATA(r, STRUCT, i, elem)                                                \
   psio::member_proxy<i, psio::hash_name(BOOST_PP_STRINGIZE(PSIO_GET_IDENT(elem))),        \
                                         &STRUCT::PSIO_GET_IDENT(elem), ProxyObject>       \
                      PSIO_GET_IDENT(elem)()                                               \
   {                                                                                       \
      return _psio_proxy_obj;                                                              \
   }                                                                                       \
   psio::member_proxy<i, psio::hash_name(BOOST_PP_STRINGIZE(PSIO_GET_IDENT(elem))),        \
                                         &STRUCT::PSIO_GET_IDENT(elem), const ProxyObject> \
                      PSIO_GET_IDENT(elem)() const                                         \
   {                                                                                       \
      return _psio_proxy_obj;                                                              \
   }

#define PSIO_PROXY_METHOD(r, STRUCT, i, elem)                                           \
   template <typename... Args>                                                          \
   auto PSIO_GET_IDENT(elem)(Args... args)                                              \
   {                                                                                    \
      psio::member_proxy<i, psio::hash_name(BOOST_PP_STRINGIZE(PSIO_GET_IDENT(elem))),  \
                                            &STRUCT::PSIO_GET_IDENT(elem), ProxyObject> \
                             m(_psio_proxy_obj);                                        \
      return m.call(std::forward<decltype(args)>(args)...);                             \
   }

/**
 * PSIO_REFLECT(<struct>, <member or base spec>...)
 * Each parameter may be one of the following:
 *    * ident:                                     non-static data member
 *    * hash(bool)                                 don't hash method identifiesrs
 *    * base(ident):                               base class
 *    * method(ident, arg1, ...):                  method
 *    * numbered(int, ident):                      non-static data member with field number
 */
#define PSIO_REFLECT(STRUCT, ...)                                                                   \
   PSIO_REFLECT_TYPENAME(STRUCT)                                                                    \
   struct reflect_impl_##STRUCT                                                                     \
   {                                                                                                \
      static constexpr bool is_defined = true;                                                      \
      static constexpr bool is_struct  = true;                                                      \
      static constexpr bool requires_compressed_method_names()                                      \
      {                                                                                             \
         constexpr bool require_compressed = BOOST_PP_CHECK_EMPTY( PSIO_HASHS(__VA_ARGS__) ); \
         BOOST_PP_SEQ_FOR_EACH(PSIO_REQ_COMPRESS, STRUCT, PSIO_METHODS(__VA_ARGS__)); \
         return require_compressed; \
      }                                                                                            \
      static inline constexpr const char* name() { return BOOST_PP_STRINGIZE(STRUCT); }             \
      typedef std::tuple<BOOST_PP_IIF(BOOST_PP_CHECK_EMPTY(PSIO_DATA_MEMBERS(__VA_ARGS__)),         \
                                      PSIO_EMPTY,                                                   \
                                      PSIO_SEQ_TO_VA_ARGS)(                                         \
          PSIO_SEQ_TRANSFORM(PSIO_TUPLE_TYPE, STRUCT, PSIO_DATA_MEMBERS(__VA_ARGS__)))>             \
          struct_tuple_type;                                                                        \
      template <typename L>                                                                         \
      constexpr inline static void for_each(L&& lambda)                                             \
      {                                                                                             \
         BOOST_PP_SEQ_FOR_EACH_I(PSIO_FOR_EACH_MEMBER, STRUCT, PSIO_DATA_MEMBERS(__VA_ARGS__))      \
         BOOST_PP_SEQ_FOR_EACH(PSIO_FOR_EACH_METHOD, STRUCT, PSIO_METHODS(__VA_ARGS__))             \
      }                                                                                             \
      template <typename L>                                                                         \
      inline static constexpr bool get(const std::string_view& m, L&& lambda)                       \
      {                                                                                             \
         BOOST_PP_SEQ_FOR_EACH(PSIO_GET_BY_STR, STRUCT, PSIO_DATA_MEMBERS(__VA_ARGS__))             \
         BOOST_PP_SEQ_FOR_EACH(PSIO_GET_BY_STR, STRUCT, PSIO_METHODS(__VA_ARGS__))                  \
         return false;                                                                              \
      }                                                                                             \
      template <typename L>                                                                         \
      inline static constexpr bool get(int64_t m, L&& lambda)                                       \
      {                                                                                             \
         switch (m)                                                                                 \
         {                                                                                          \
            BOOST_PP_SEQ_FOR_EACH_I(PSIO_GET_BY_NUMBER, STRUCT, PSIO_DATA_MEMBERS(__VA_ARGS__))     \
         }                                                                                          \
         return false;                                                                              \
      }                                                                                             \
      template <typename L>                                                                         \
      inline static bool get_by_name(uint64_t n, L&& lambda)                                        \
      {                                                                                             \
         switch (n)                                                                                 \
         {                                                                                          \
            BOOST_PP_SEQ_FOR_EACH_I(PSIO_GET_MEMBER_BY_NAME, STRUCT,                                \
                                    PSIO_DATA_MEMBERS(__VA_ARGS__))                                 \
            BOOST_PP_SEQ_FOR_EACH(PSIO_GET_METHOD_BY_NAME, STRUCT, PSIO_METHODS(__VA_ARGS__))       \
         }                                                                                          \
         return false;                                                                              \
      }                                                                                             \
      static constexpr auto member_pointers()                                                       \
      {                                                                                             \
         return std::make_tuple(PSIO_MEMBER_POINTER(STRUCT, PSIO_ALL_MEMBERS(__VA_ARGS__)));        \
      }                                                                                             \
                                                                                                    \
      template <typename ProxyObject>                                                               \
      struct proxy                                                                                  \
      {                                                                                             \
        private:                                                                                    \
         ProxyObject _psio_proxy_obj;                                                               \
                                                                                                    \
        public:                                                                                     \
         template <typename... Args>                                                                \
         explicit proxy(Args&&... args) : _psio_proxy_obj(std::forward<Args>(args)...)              \
         {                                                                                          \
         }                                                                                          \
         auto&              psio_get_proxy() const { return _psio_proxy_obj; }                      \
         ProxyObject*       operator->() { return &_psio_proxy_obj; }                               \
         const ProxyObject* operator->() const { return &_psio_proxy_obj; }                         \
         ProxyObject&       operator*() { return _psio_proxy_obj; }                                 \
         const ProxyObject& operator*() const { return _psio_proxy_obj; }                           \
         BOOST_PP_SEQ_FOR_EACH_I(PSIO_PROXY_DATA, STRUCT, PSIO_DATA_MEMBERS(__VA_ARGS__))           \
         BOOST_PP_SEQ_FOR_EACH_I(PSIO_PROXY_METHOD, STRUCT, PSIO_METHODS(__VA_ARGS__))              \
      };                                                                                            \
   };                                                                                               \
   reflect_impl_##STRUCT get_reflect_impl(const STRUCT&);
