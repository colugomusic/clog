#include <cassert>
#include <optional>
#include <variant>

namespace clg {

template <typename... Types> struct const_ref;
template <typename... Types> struct object;
template <typename... Types> struct optional_const_ref;
template <typename... Types> struct optional_ref;
template <typename... Types> struct ref;

namespace detail {

template <typename Tag, typename T, typename... Args>
auto call(T& o, Args&&... args) { return Tag::call(o, std::move(args)...); }
template <typename Tag, typename T, typename... Args>
auto call(T* o, Args&&... args) { return Tag::call(*o, std::move(args)...); }

template <typename T> struct traits {};

template <typename... Types>
struct traits<object<Types...>> {
	using type = object<Types...>;
	using variant_type = std::variant<Types...>;
	template <typename T> struct compose_type { using type = T; };
	template <typename T> static auto& compose_value(T&& value) { return value; }
	template <typename T> static auto& decompose_value(T&& value) { return value; }
	template <typename T> static auto& decompose_variant(T&& variant) { return variant; }
	template <typename T> static auto assert_variant(T&& variant) { return true; }
};

template <typename... Types>
struct traits<ref<Types...>> {
	using type = ref<Types...>;
	using variant_type = std::variant<std::add_pointer_t<Types>...>;
	template <typename T> struct compose_type { using type = std::add_pointer_t<T>; };
	template <typename T> static auto compose_value(T&& value) { return &value; }
	template <typename T> static auto& decompose_value(T* value) { return *value; }
	template <typename T> static auto& decompose_variant(T&& variant) { return variant; }
	template <typename T> static auto assert_variant(T&& variant) { return true; }
};

template <typename... Types>
struct traits<const_ref<Types...>> {
	using type = const_ref<Types...>;
	using variant_type = std::variant<std::add_pointer_t<std::add_const_t<Types>>...>;
	template <typename T> struct compose_type { using type = std::add_pointer_t<std::add_const_t<T>>; };
	template <typename T> static auto compose_value(T&& value) { return &value; }
	template <typename T> static auto& decompose_value(T* value) { return *value; }
	template <typename T> static auto& decompose_variant(T&& variant) { return variant; }
	template <typename T> static auto assert_variant(T&& variant) { return true; }
};

template <typename... Types>
struct traits<optional_ref<Types...>> {
	using type = optional_ref<Types...>;
	using variant_type = std::optional<std::variant<std::add_pointer_t<Types>...>>;
	template <typename T> struct compose_type { using type = std::add_pointer_t<T>; };
	template <typename T> static auto compose_value(T&& value) { return &value; }
	template <typename T> static auto& decompose_value(T* value) { return *value; }
	template <typename T> static auto& decompose_variant(T&& variant) { return variant.value(); }
	template <typename T> static auto assert_variant(T&& variant) { return variant.has_value(); }
};

template <typename... Types>
struct traits<optional_const_ref<Types...>> {
	using type = optional_const_ref<Types...>;
	using variant_type = std::optional<std::variant<std::add_pointer_t<std::add_const_t<Types>>...>>;
	template <typename T> struct compose_type { using type = std::add_pointer_t<std::add_const_t<T>>; };
	template <typename T> static auto compose_value(T&& value) { return &value; }
	template <typename T> static auto& decompose_value(T* value) { return *value; }
	template <typename T> static auto& decompose_variant(T&& variant) { return variant.value(); }
	template <typename T> static auto assert_variant(T&& variant) { return variant.has_value(); }
};

template <typename DstType, typename SrcType>
static auto copy(SrcType& rhs) {
	using dst_traits = traits<DstType>;
	using src_traits = traits<SrcType>;
	assert (src_traits::assert_variant(rhs.v_));
	return std::visit([](auto&& value) -> typename dst_traits::variant_type {
		return dst_traits::compose_value(src_traits::decompose_value(value));
	}, src_traits::decompose_variant(rhs.v_));
}

template <typename DstType, typename SrcType>
static auto copy(const SrcType& rhs) {
	using dst_traits = traits<DstType>;
	using src_traits = traits<SrcType>;
	assert (src_traits::assert_variant(rhs.v_));
	return std::visit([](auto&& value) -> typename dst_traits::variant_type {
		return dst_traits::compose_value(src_traits::decompose_value(value));
	}, src_traits::decompose_variant(rhs.v_));
}

} // detail

template <typename Traits>
struct var_base
{
	using variant_type = typename Traits::variant_type;

	template <typename T>
	var_base(T&& initial_value) : v_{std::forward<T>(initial_value)} {}

	template <typename Tag>
	auto call() {
		return std::visit([](auto&& o) { return detail::call<Tag>(Traits::decompose_value(o)); }, Traits::decompose_variant(v_));
	}

	template <typename Tag>
	auto call() const {
		return std::visit([](auto&& o) { return detail::call<Tag>(Traits::decompose_value(o)); }, Traits::decompose_variant(v_));
	}

	template <typename Tag, typename... Args>
	auto call(Args&&... args) {
		return std::visit([args...](auto&& o) { return detail::call<Tag>(Traits::decompose_value(o), std::move(args)...); }, Traits::decompose_variant(v_));
	}

	template <typename Tag, typename... Args>
	auto call(Args&&... args) const {
		return std::visit([args...](auto&& o) { return detail::call<Tag>(Traits::decompose_value(o), std::move(args)...); }, Traits::decompose_variant(v_));
	}

	template <typename T> auto& get() {
		return std::get<typename Traits::template compose_type<T>::type>(Traits::decompose_variant(v_));
	}

	template <typename T> auto& get() const {
		return std::get<typename Traits::template compose_type<T>::type>(Traits::decompose_variant(v_));
	}

	template <typename T> auto holds() const {
		return std::holds_alternative<typename Traits::template compose_type<T>::type>(Traits::decompose_variant(v_));
	}

	variant_type v_;
};

template <typename... Types>
struct object : public var_base<detail::traits<object<Types...>>>
{
	using base_t = var_base<detail::traits<object<Types...>>>;
	using variant_type = typename base_t::variant_type;

	template <typename T>
	object(T&& value) : base_t{std::forward<T>(value)} {}

	template <typename T>
	auto operator=(T&& value) -> object<Types...>&
	{
		base_t::v_ = std::forward<T>(value);
		return *this;
	}
};

template <typename... Types>
struct ref : public var_base<detail::traits<ref<Types...>>>
{
	using me_t = ref<Types...>;
	using base_t = var_base<detail::traits<ref<Types...>>>;
	using variant_type = typename base_t::variant_type;
	using object_t = object<Types...>;
	using optional_ref_t = optional_ref<Types...>;

	template <typename T> ref(T* value) : base_t{value} {}
	ref(const ref& rhs) = default;
	ref(object_t& rhs) : base_t{detail::copy<me_t>(rhs)} {}
	ref(const optional_ref_t& rhs) : base_t{detail::copy<me_t>(rhs)} {}

	template <typename T>
	auto operator=(T* value) -> ref<Types...>& {
		base_t::v_ = value;
		return *this;
	}

	auto operator=(object_t& rhs) -> ref<Types...>& {
		base_t::v_ = detail::copy<variant_type>(rhs);
		return *this;
	}

	auto operator=(optional_ref_t& rhs) -> ref<Types...>& {
		base_t::v_ = detail::copy<variant_type>(rhs);
		return *this;
	}
};

template <typename... Types>
struct const_ref : public var_base<detail::traits<const_ref<Types...>>>
{
	using me_t = const_ref<Types...>;
	using base_t = var_base<detail::traits<const_ref<Types...>>>;
	using variant_type = typename base_t::variant_type;
	using object_t = object<Types...>;
	using optional_const_ref_t = optional_const_ref<Types...>;
	using optional_ref_t = optional_ref<Types...>;
	using ref_t = ref<Types...>;

	template <typename T> const_ref(const T* value) : base_t{value} {}
	const_ref(const const_ref& rhs) = default;
	const_ref(object_t& rhs) : base_t{detail::copy<me_t>(rhs)} {}
	const_ref(const ref_t& rhs) : base_t{detail::copy<me_t>(rhs)} {}
	const_ref(const optional_const_ref_t& rhs) : base_t{detail::copy<me_t>(rhs)} {}
	const_ref(const optional_ref_t& rhs) : base_t{detail::copy<me_t>(rhs)} {}
	const_ref(ref_t&& rhs) : base_t{std::move(rhs.v_)} {}
	const_ref(optional_const_ref_t&& rhs) : base_t{detail::copy<me_t>(rhs)} {}

	template <typename T> auto operator=(const T* value) -> const_ref<Types...>& {
		base_t::v_ = value;
		return *this;
	}

	auto operator=(const object_t& rhs) -> const_ref<Types...>& {
		base_t::v_ = detail::copy<me_t>(rhs);
		return *this;
	}

	auto operator=(const ref_t& rhs) -> const_ref<Types...>& {
		base_t::v_ = detail::copy<me_t>(rhs);
		return *this;
	}

	auto operator=(optional_const_ref_t& rhs) -> const_ref<Types...>& {
		base_t::v_ = detail::copy<me_t>(rhs);
		return *this;
	}

	auto operator=(optional_ref_t& rhs) -> const_ref<Types...>&
	{
		base_t::v_ = detail::copy<me_t>(rhs);
		return *this;
	}

	auto operator=(ref_t&& rhs) -> const_ref<Types...>&
	{
		base_t::v_ = std::move(rhs.v_);
		return *this;
	}
};

template <typename... Types>
struct optional_ref : public var_base<detail::traits<optional_ref<Types...>>>
{
	using me_t = optional_ref<Types...>;
	using base_t = var_base<detail::traits<optional_ref<Types...>>>;
	using variant_type = typename base_t::variant_type;
	using object_t = object<Types...>;
	using ref_t = ref<Types...>;

	optional_ref() : base_t{std::nullopt} {}
	template <typename T> optional_ref(T* value) : base_t{value} {}
	optional_ref(object_t& rhs) : base_t{detail::copy<me_t>(rhs)} {}
	optional_ref(ref_t& rhs) : base_t{rhs.v_} {}
	optional_ref(ref_t&& rhs) : base_t{std::move(rhs.v_)} {}

	template <typename T>
	auto operator=(T* value) -> ref<Types...>& {
		base_t::v_ = value;
		return *this;
	}

	auto operator=(ref_t& rhs) -> optional_ref<Types...>& {
		base_t::v_ = rhs.v_;
		return *this;
	}

	auto operator=(object_t& rhs) -> optional_ref<Types...>& {
		base_t::v_ = detail::copy<me_t>(rhs);
		return *this;
	}

	auto operator=(ref_t&& rhs) -> optional_ref<Types...>& {
		base_t::v_ = std::move(rhs.v_);
		return *this;
	}

	operator bool() const { return base_t::v_.has_value(); }
};

template <typename... Types>
struct optional_const_ref : public var_base<detail::traits<optional_const_ref<Types...>>>
{
	using me_t = optional_const_ref<Types...>;
	using base_t = var_base<detail::traits<optional_const_ref<Types...>>>;
	using variant_type = typename base_t::variant_type;
	using object_t = object<Types...>;
	using optional_ref_t = optional_ref<Types...>;
	using const_ref_t = const_ref<Types...>;
	using ref_t = ref<Types...>;

	optional_const_ref() : base_t{std::nullopt} {}
	template <typename T> optional_const_ref(const T* value) : base_t{value} {}
	optional_const_ref(const object_t& rhs) : base_t{detail::copy<me_t>(rhs)} {}
	optional_const_ref(const ref_t& rhs) : base_t{detail::copy<me_t>(rhs)} {}
	optional_const_ref(const optional_ref_t& rhs) : base_t{detail::copy<me_t>(rhs)} {}
	optional_const_ref(ref_t&& rhs) : base_t{std::move(rhs.v_)} {}
	optional_const_ref(const_ref_t&& rhs) : base_t{std::move(rhs.v_)} {}

	template <typename T>
	auto operator=(const T* value) -> ref<Types...>& {
		base_t::v_ = value;
		return *this;
	}

	auto operator=(ref_t& rhs) -> optional_const_ref<Types...>& {
		base_t::v_ = detail::copy<me_t>(rhs);
		return *this;
	}

	auto operator=(const_ref_t& rhs) -> optional_const_ref<Types...>& {
		base_t::v_ = rhs.v_;
		return *this;
	}

	auto operator=(const object_t& rhs) -> optional_const_ref<Types...>& {
		base_t::v_ = detail::copy<me_t>(rhs);
		return *this;
	}

	auto operator=(const optional_ref_t& rhs) -> optional_const_ref<Types...>& {
		base_t::v_ = detail::copy<me_t>(rhs);
		return *this;
	}

	auto operator=(const_ref_t&& rhs) -> optional_const_ref<Types...>& {
		base_t::v_ = std::move(rhs.v_);
		return *this;
	}

	auto operator=(ref_t&& rhs) -> optional_const_ref<Types...>& {
		base_t::v_ = std::move(rhs.v_);
		return *this;
	}

	operator bool() const { return base_t::v_.has_value(); }
};

template <typename... Types>
struct var
{
	using const_ref = clg::const_ref<Types...>;
	using object = clg::object<Types...>;
	using optional_const_ref = clg::optional_const_ref<Types...>;
	using optional_ref = clg::optional_ref<Types...>;
	using ref = clg::ref<Types...>;

	template <typename T> struct has : std::disjunction<std::is_same<T, Types>...> {};

	template <typename T>
	static constexpr auto has_v = has<T>::value;
};

} // clg