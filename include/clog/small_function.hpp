#pragma once

//
// std::function replacement which does not ever allocate
// heap memory
// 
// This is based on github:rigtorp/Function by Erik Rigtorp
// except I fixed all the problems with it!
//

#include <functional>
#include <memory>

namespace clg {

namespace detail {

template< class T >
struct remove_cvref {
	typedef std::remove_cv_t<std::remove_reference_t<T>> type;
};

template< class T >
using remove_cvref_t = typename remove_cvref<T>::type;

} // detail

template <typename, size_t MaxSize = 1024> class small_function;

template <typename R, class... Args, size_t MaxSize>
class small_function<R(Args...), MaxSize>
{
public:

	small_function() noexcept = default;
	small_function(std::nullptr_t) noexcept {}

	small_function(const small_function& rhs)
		: operations_{ rhs.operations_ }
	{
		if (!(*this)) return;

		operations_.copier(&data_, &rhs.data_);
	}

	small_function(small_function&& rhs) noexcept
		: operations_{ std::move(rhs.operations_) }
	{
		if (!(*this)) return;

		operations_.mover(&data_, &rhs.data_);
	}

	template<typename FnT,
		typename Dummy = typename std::enable_if_t<!std::is_same_v<small_function<R(Args...), MaxSize>, detail::remove_cvref_t<FnT>>>>
		small_function(FnT&& fn)
	{
		from_fn(std::forward<FnT>(fn));
	}

	~small_function()
	{
		if (!!(*this))
		{
			operations_.destroyer(&data_);
		}
	}

	auto operator=(const small_function& rhs) -> small_function&
	{
		operations_ = rhs.operations_;

		if (!(*this)) return *this;

		operations_.copier(&data_, &rhs.data_);

		return *this;
	}

	auto operator=(small_function&& rhs) noexcept -> small_function&
	{
		operations_ = rhs.operations_;

		if (!(*this)) return *this;

		operations_.mover(&data_, &rhs.data_);

		return *this;
	}

	auto operator=(std::nullptr_t) -> small_function&
	{
		if (!(*this)) return *this;

		operations_.destroyer(&data_);
		operations_ = {};

		return *this;
	}

	template <typename FnT,
		typename Dummy = typename std::enable_if_t<!std::is_same_v<small_function<R(Args...), MaxSize>, detail::remove_cvref_t<FnT>>>>
		auto operator=(FnT&& fn) -> small_function&
	{
		from_fn(std::forward<FnT>(fn));
		return *this;
	}

	template <typename FnT>
	auto operator=(std::reference_wrapper<FnT> fn) -> small_function&
	{
		from_fn(fn);
		return *this;
	}

	explicit operator bool() const noexcept
	{
		return !!operations_;
	}

	auto operator()(Args... args) const -> R
	{
		if (!operations_)
		{
			throw std::bad_function_call();
		}

		return operations_.invoker(&data_, std::forward<Args>(args)...);
	}

private:

	struct Operations
	{
		using Copier = void (*)(void*, const void*);
		using Destroyer = void (*)(void*);
		using Invoker = R (*)(const void*, Args &&...);
		using Mover = void (*)(void*, void*);

		Operations() = default;

		template <typename FnT>
		static auto make() -> Operations
		{
			using fn_t = typename std::decay<FnT>::type;

			Operations out;

			out.copier = &copy<fn_t>;
			out.destroyer = &destroy<fn_t>;
			out.mover = &move<fn_t>;
			out.invoker = &invoke<fn_t>;

			return out;
		}

		template <typename FnT>
		static auto copy(void* dest, const void* src) -> void
		{
			const auto& src_fn{ *static_cast<const FnT*>(src) };

			new (dest) FnT(src_fn);
		}

		template <typename FnT>
		static auto destroy(void* dest) -> void
		{
			static_cast<FnT*>(dest)->~FnT();
		}

		template <typename FnT>
		static auto move(void* dest, void* src) -> void
		{
			auto& src_fn{ *static_cast<FnT*>(src) };

			new (dest) FnT(std::move(src_fn));
		}

		template <typename FnT>
		static auto invoke(const void* data, Args&&... args) -> R
		{
			const FnT& fn{ *static_cast<const FnT*>(data) };

			return fn(std::forward<Args>(args)...);
		}

		explicit operator bool() const noexcept
		{
			return !!invoker;
		}

		Copier copier{};
		Destroyer destroyer{};
		Invoker invoker{};
		Mover mover{};
	};

	using Storage = typename std::aligned_storage<MaxSize - sizeof(Operations), 8>::type;

	template<typename FnT>
	auto from_fn(FnT&& fn) -> void
	{
		using fn_t = typename std::decay<FnT>::type;

		static_assert(alignof(fn_t) <= alignof(Storage), "invalid alignment");
		static_assert(sizeof(fn_t) <= sizeof(Storage), "storage too small");

		new (&data_) fn_t(std::forward<FnT>(fn));

		operations_ = Operations::make<FnT>();
	}

	Storage data_{};
	Operations operations_{};
};

} // clg