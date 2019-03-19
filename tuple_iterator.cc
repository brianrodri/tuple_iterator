#include <iostream>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace tuple_ext {
namespace detail {

// Helper aliases to help cut down boilerplate when working with compile-time
// index values.
template <size_t I> using index_t = std::integral_constant<size_t, I>;
template <size_t I> constexpr auto index_c = index_t<I>{};

// Exposes types required by TupleIterator to be standard-compliant.
//
// The types are derived from the given type parameter, which is assumed to be a
// "tuple-like" structure. Specifically, `Tup` must compile in the following
// contexts:
//   - std::get<I>(std::declval<Tup&>())
//   - std::tuple_size<Tup>
//
// std::tuple defines these overloads thanks to the standard, but you can create
// overloads for custom classes as necessary.
template <typename Tup> struct IterTypeTraitsImpl;
template <typename... T> struct IterTypeTraitsImpl<std::tuple<T...>> {
  private:
    // Returns a variant of compile-time index values from the provided sequence
    // of index values.
    template <size_t... I> static constexpr auto
    MakeIndexVariant(std::index_sequence<I...>) -> std::variant<index_t<I>...> {
        // NOTE: This function is only inspected at compile-time — never called.
        return {};
    };

  public:
    using PointerType = std::variant<T*...>;
    using ReferenceType = std::variant<std::reference_wrapper<T>...>;
    using ValueType = ReferenceType;

    // An implementation detail for remembering the current index which a tuple
    // iterator is pointing to.
    using IndexVariant =
        decltype(MakeIndexVariant(std::index_sequence_for<T...>()));
};

}  // namespace detail

// Provides interface for creating tuple iterators.
template <typename T> class TupleRange;

template <typename T>
class TupleIterator {
    using index_variant_opt =
        std::optional<typename detail::IterTypeTraitsImpl<T>::IndexVariant>;

  public:
    // Type aliases expected by the standard.
    using value_type = typename detail::IterTypeTraitsImpl<T>::ValueType;
    using reference = typename detail::IterTypeTraitsImpl<T>::ReferenceType;
    using pointer = typename detail::IterTypeTraitsImpl<T>::PointerType;
    using difference_type = ptrdiff_t;

    // NOTE: This can be upgraded to random-access, but more work is required :)
    using iterator_category = std::bidirectional_iterator_tag;

    TupleIterator(const TupleIterator<T>& src)
        : tuple_ptr_(src.tuple_ptr_), index_opt_(src.index_opt_) {}

    TupleIterator& operator=(const TupleIterator<T>& src) {
        tuple_ptr_ = src.tuple_ptr_;
        index_opt_ = src.index_opt_;
        return *this;
    }

    constexpr TupleIterator& operator++() {
        increment_index();
        return *this;
    }

    constexpr TupleIterator operator++(int _) {
        TupleIterator next_iter = *this;
        ++(*this);
        return next_iter;
    }

    constexpr TupleIterator& operator--() {
        decrement_index();
        return *this;
    }

    constexpr TupleIterator operator--(int _) {
        TupleIterator next_iter = *this;
        --(*this);
        return next_iter;
    }

    constexpr reference operator*() {
        return std::visit([this](auto i) {
            return reference(std::reference_wrapper(std::get<i>(*tuple_ptr_)));
        }, *index_opt_);
    }

    constexpr reference operator*() const {
        return std::visit([this](auto i) {
            return reference(std::reference_wrapper(std::get<i>(*tuple_ptr_)));
        }, *index_opt_);
    }

    // NOTE: operator-> is not defined because there is no way to make it
    // standard-compliant.
    //
    // The standard expects that values returned by operator-> may eventually be
    // resolved by 1+ repeated applications. However, we can only return a
    // std::variant of pointers. This implies that eventually, a call to
    // std::visit *must be made*.
    //
    // For now, I've chosen to simply leave out the definition of operator->
    // while keeping the definition of a "pointer"-type. I kept the pointer-type
    // because it is required by std::distance.

    constexpr difference_type operator-(const TupleIterator& rhs) const {
        return get_index() - rhs.get_index();
    }

    constexpr bool operator==(const TupleIterator& rhs) const {
        return tuple_ptr_ == rhs.tuple_ptr_ && index_opt_ == rhs.index_opt_;
    }

    constexpr bool operator!=(const TupleIterator& rhs) const {
        return !(*this == rhs);
    }

  private:
    constexpr TupleIterator(T& t, index_variant_opt i)
        : tuple_ptr_(&t), index_opt_(i) {};

    // Provides interface for creating tuple iterators.
    friend class TupleRange<T>;


    constexpr void increment_index() {
        if (index_opt_ != std::nullopt) {
            index_opt_ = std::visit([](auto i) {
                if constexpr (i + 1 < std::tuple_size_v<T>) {
                    return index_variant_opt(detail::index_c<i + 1>);
                } else {
                    return index_variant_opt(std::nullopt);
                }
            }, *index_opt_);
        }
    }

    constexpr void decrement_index() {
        if (index_opt_ == std::nullopt) {
            index_opt_ = detail::index_c<std::tuple_size_v<T> - 1>;
        } else {
            index_opt_ = std::visit([](auto i) {
                if constexpr (i > 0) {
                    return index_variant_opt(detail::index_c<i - 1>);
                } else {
                    return index_variant_opt(detail::index_c<0>);
                }
            }, *index_opt_);
        }
    }

    constexpr ptrdiff_t get_index() const {
        return index_opt_ ? index_opt_->index() : std::tuple_size_v<T>;
    }

    index_variant_opt index_opt_;
    T* tuple_ptr_;
};

// Provides interface for creating tuple iterators.
template <typename T>
class TupleRange {
  public:
    constexpr TupleRange(T& t) : tuple_ptr_(&t) {}

    constexpr TupleIterator<T> begin() const {
        if constexpr (0 < std::tuple_size_v<T>) {
            return {*tuple_ptr_, detail::index_c<0>};
        } else {
            return end();
        }
    }

    constexpr TupleIterator<T> end() const {
        return {*tuple_ptr_, std::nullopt};
    }

  private:
    T* tuple_ptr_;
};

}  // namespace tuple_ext

int main() {
    using namespace std::string_literals;
    using tuple_ext::TupleRange;

    auto t = std::tuple(1, 3.14, "olive"s);  // "olive" is my favorite cat :)
    auto tuple_range = TupleRange(t);

    // NOTE: We need to use .get() because we can't create a variant of
    // references. Instead, we must create a variant of std::reference_wrapper.
    auto element_printer = [](const auto& e) { std::cout << e.get() << '\n'; };

    std::cout << "# Forward Iteration\n";
    auto b = tuple_range.begin();
    auto e = tuple_range.end();
    while (b != e) { std::visit(element_printer, *b++); }

    std::cout << "# Backwards Iteration\n";
    b = tuple_range.begin();
    e = tuple_range.end();
    for (ptrdiff_t n = std::distance(b, e); n > 0; --n) {
        std::visit(element_printer, *--e);
    }

    std::cout << "# <algorithm> for_each\n";
    std::for_each(
        tuple_range.begin(), tuple_range.end(), [&](const auto& variant_of_refs) {
            std::visit(element_printer, variant_of_refs);
        });

    std::cout << "# Tuple Iterator size\n";
    std::cout << sizeof(tuple_range.begin()) << '\n';

    std::cout << "# Constexpr context\n";
    constexpr bool is_equal = ++(++(++TupleRange(t).begin())) == TupleRange(t).end();
    static_assert(is_equal == true);
    std::cout << std::boolalpha << is_equal << '\n';

    return 0;
}
