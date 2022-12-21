#pragma once

namespace clg {

struct feedback_blocker
{
	struct scope
	{
		scope(bool* flag)
			: flag_{flag}
		{
			*flag_ = true;
		}

		scope(scope&& rhs) noexcept
			: flag_{rhs.flag_}
		{
			rhs.flag_ = {};
		}

		auto operator=(scope&& rhs) noexcept -> scope&
		{
			flag_ = rhs.flag_;
			rhs.flag_ = {};
			return *this;
		}

		~scope()
		{
			if (flag_)
			{
				*flag_ = false;
			}
		}

		scope(const scope& rhs) = delete;
		auto operator=(const scope& rhs) noexcept -> scope& = delete;

	private:

		bool* flag_;
	};

	auto operator()() -> scope
	{
		return {&flag_};
	}

	operator bool() const
	{
		return flag_;
	}

private:

	bool flag_{false};
};

} // clg