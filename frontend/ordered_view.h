//	Copyright (c) 2011-2018 by Artem A. Gevorkyan (gevorkyan.org) and Denis Burenko
//
//	Permission is hereby granted, free of charge, to any person obtaining a copy
//	of this software and associated documentation files (the "Software"), to deal
//	in the Software without restriction, including without limitation the rights
//	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//	copies of the Software, and to permit persons to whom the Software is
//	furnished to do so, subject to the following conditions:
//
//	The above copyright notice and this permission notice shall be included in
//	all copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//	THE SOFTWARE.

#pragma once

#include "series.h"

#include <algorithm>
#include <functional>
#include <list>
#include <memory>
#include <vector>
#include <wpl/ui/models.h>

namespace micro_profiler
{
	template <class ContainerT>
	class ordered_view : public series<double>
	{
	public:
		typedef typename ContainerT::value_type value_type;
		typedef typename value_type::first_type key_type;
		typedef typename value_type::second_type mapped_type;

	public:
		ordered_view(const ContainerT &underlying);
		~ordered_view();

		// Paermanently detaches from the underlying map. Postcondition: size() becomes zero.
		void detach() throw();

		// Set order for internal orderd storage, save it until new order is set.
		template <typename PredicateT>
		void set_order(PredicateT predicate, bool ascending);

		template <typename ExtractorT>
		void project_value(const ExtractorT &extractor);

		void disable_projection();

		// Repopulate internal ordered storage from source map with respect to predicate set.
		void resort();

		virtual index_type size() const throw();
		const value_type &at(index_type index) const;
		virtual double get_value(index_type index) const throw();
		index_type find_by_key(const key_type &key) const;
		std::shared_ptr<const wpl::ui::trackable> track(index_type index) const;

	private:
		typedef std::vector<const value_type *> ordered_container;

		template <typename PredicateT>
		class predicate_wrap_a;
		template <typename PredicateT>
		class predicate_wrap_d;

		class trackable : public wpl::ui::trackable
		{
		public:
			trackable(key_type key_, index_type current_index_) : key(key_), current_index(current_index_) {	}
			virtual index_type index() const { return current_index; }

		public:
			key_type key;
			index_type current_index;

		private:
			const trackable &operator =(const trackable &rhs);
		};

		typedef std::list<trackable> trackables_t;

	private:
		// Repopulate internal storage with data from source map, ignores any predicate set.
		void fetch_data();

		void invalidate_trackables();
		void update_trackables();

		template <typename PredicateT>
		void sort(const PredicateT &predicate);

		const ordered_view &operator =(const ordered_view &);

	private:
		const ContainerT *_underlying;
		ordered_container _ordered_data;
		std::function<void()> _sorter;
		std::function<double(const mapped_type &entry)> _extractor;
		std::shared_ptr<trackables_t> _trackables;
	};


	template <typename ContainerT>
	template <typename PredicateT>
	class ordered_view<ContainerT>::predicate_wrap_a
	{
	public:
		predicate_wrap_a(const PredicateT &predicate)
			: _predicate(predicate)
		{ }

		template <typename ArgT>
		bool operator ()(ArgT lhs, ArgT rhs) const
		{	return _predicate(lhs->first, lhs->second, rhs->first, rhs->second);	}

	private:
		PredicateT _predicate;
	};


	template <typename ContainerT>
	template <typename PredicateT>
	class ordered_view<ContainerT>::predicate_wrap_d
	{
	public:
		predicate_wrap_d(const PredicateT &predicate)
			: _predicate(predicate)
		{ }

		template <typename ArgT>
		bool operator ()(ArgT lhs, ArgT rhs) const
		{	return _predicate(rhs->first, rhs->second, lhs->first, lhs->second);	}

	private:
		PredicateT _predicate;
	};



	template <class ContainerT>
	inline ordered_view<ContainerT>::ordered_view(const ContainerT &underlying)
		: _underlying(&underlying), _trackables(new trackables_t)
	{	fetch_data();	}

	template <class ContainerT>
	inline ordered_view<ContainerT>::~ordered_view()
	{
		_ordered_data.clear();
		update_trackables();
	}

	template <class ContainerT>
	inline void ordered_view<ContainerT>::detach() throw()
	{
		_underlying = 0;
		_ordered_data.clear();
		invalidate_trackables();
		invalidated();
	}

	template <class ContainerT>
	inline typename ordered_view<ContainerT>::index_type ordered_view<ContainerT>::size() const throw()
	{	return static_cast<index_type>(_ordered_data.size());	}

	template <class ContainerT>
	template <typename PredicateT>
	inline void ordered_view<ContainerT>::set_order(PredicateT predicate, bool ascending)
	{
		if (ascending)
			_sorter = std::bind(&ordered_view::sort< predicate_wrap_a<PredicateT> >, this,
				predicate_wrap_a<PredicateT>(predicate));
		else
			_sorter = std::bind(&ordered_view::sort< predicate_wrap_d<PredicateT> >, this,
				predicate_wrap_d<PredicateT>(predicate));
		_sorter();
		update_trackables();
	}

	template <class ContainerT>
	template <typename ExtractorT>
	inline void ordered_view<ContainerT>::project_value(const ExtractorT &extractor)
	{	_extractor = extractor;	}

	template <class ContainerT>
	inline void ordered_view<ContainerT>::disable_projection()
	{	_extractor = std::function<double(const typename ContainerT::mapped_type &entry)>();	}

	template <class ContainerT>
	inline void ordered_view<ContainerT>::resort()
	{
		fetch_data();

		if (_sorter)
			_sorter();
		invalidated();
		update_trackables();
	}

	template <class ContainerT>
	inline void ordered_view<ContainerT>::fetch_data()
	{
		if (!_underlying)
			return;
		_ordered_data.clear();
		_ordered_data.reserve(_underlying->size());
		for (typename ContainerT::const_iterator i = _underlying->begin(), end = _underlying->end(); i != end; ++i)
			_ordered_data.push_back(&*i);
	}

	template <typename ContainerT>
	inline void ordered_view<ContainerT>::invalidate_trackables()
	{
		for (trackables_t::iterator j = _trackables->begin(); j != _trackables->end(); ++j)
			j->current_index = trackable::npos;
	}

	template <typename ContainerT>
	inline void ordered_view<ContainerT>::update_trackables()
	{
		const trackables_t::iterator b = _trackables->begin(), e = _trackables->end();

		invalidate_trackables();
		for (index_type i = 0, count = size(); i != count; ++i)
		{
			const value_type &entry = at(i);

			for (trackables_t::iterator j = b; j != e; ++j)
				if (j->key == entry.first)
					j->current_index = i;
		}
	}

	template <typename ContainerT>
	template <typename PredicateT>
	inline void ordered_view<ContainerT>::sort(const PredicateT &predicate)
	{	std::sort(_ordered_data.begin(), _ordered_data.end(), predicate);	}

	template <class ContainerT>
	inline const typename ordered_view<ContainerT>::value_type &ordered_view<ContainerT>::at(index_type index) const
	{	return *_ordered_data[index];	}

	template <class ContainerT>
	inline double ordered_view<ContainerT>::get_value(index_type index) const throw()
	{	return _extractor ? _extractor(at(index).second) : double();	}

	template <class ContainerT>
	inline typename ordered_view<ContainerT>::index_type ordered_view<ContainerT>::find_by_key(const key_type &key) const
	{
		for (index_type i = 0, count = size(); i < count; ++i)
		{
			if (at(i).first == key)
				return i;
		}
		return npos;
	}

	template <class ContainerT>
	inline std::shared_ptr<const wpl::ui::trackable> ordered_view<ContainerT>::track(index_type index) const
	{
		using namespace std;

		struct deleter
		{	static void erase(const shared_ptr<trackables_t> &c, typename trackables_t::iterator i) { c->erase(i); } };

		trackables_t::iterator i = _trackables->insert(_trackables->end(), trackable(at(index).first, index));

		return shared_ptr<trackable>(&*i, bind(&deleter::erase, _trackables, i));
	}
}
