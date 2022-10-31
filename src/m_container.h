//
// Copyright(C) 2020-2022 Ethan Watson
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION: Container library. Wraps some STL containers.
//


#ifndef __M_CONTAINER__
#define __M_CONTAINER__

#include "doomtype.h"
#include "z_zone.h"

#ifdef __cplusplus

#include <span>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <tuple>
#include <array>
#include <type_traits>

template< typename _ty, int32_t _tag = PU_STATIC >
struct DoomAllocator
{
	enum : int32_t										{ tag = _tag };
	typedef typename std::remove_cv< _ty >::type		value_type;
	typedef value_type*									pointer;
	typedef const pointer								const_pointer;
	typedef value_type&									reference;
	typedef const value_type&							const_reference;
	typedef size_t										size_type;
	typedef ptrdiff_t									difference_type;
	typedef std::true_type								propagate_on_container_move_assignment;
	template< typename _newty >
	struct rebind
	{
		typedef DoomAllocator< _newty, tag >			other;
	};
	typedef std::true_type								is_always_equal;

	constexpr DoomAllocator() noexcept { }
	constexpr DoomAllocator( const DoomAllocator& other ) noexcept { }
	template< typename U, int32_t T = PU_STATIC >
	constexpr DoomAllocator( const DoomAllocator< U, T >& other ) noexcept { }


	~DoomAllocator() = default;

	pointer address( reference x ) noexcept
	{
		return &x;
	}

	const_pointer address( const_reference x ) const noexcept
	{
		return &x;
	}

	pointer allocate( size_type n, const void * hint = 0 )
	{
		return ( pointer )Z_Malloc( (int32_t)( sizeof( value_type ) * n ), tag, NULL );
	}

	void deallocate( pointer p, std::size_t n )
	{
		if constexpr( tag < PU_LEVEL )
		{
			Z_Free( p );
		}
	}

	size_type max_size() const noexcept
	{
		return std::numeric_limits<size_type>::max() / sizeof(value_type);
	}

	template< class U, class... Args >
	void construct( U* p, Args&&... args )
	{
		::new( (void *)p ) U( std::forward< Args >( args )... );
	}

	template< class U >
	void destroy( U* p )
	{
		p->~U();
	}

	template< class _other >
	bool operator==( const DoomAllocator< _other >& rhs ) noexcept
	{
		return std::is_same< value_type, _other >::value;
	}

	template< class _other >
	bool operator!=( const DoomAllocator< _other >& rhs ) noexcept
	{
		return !std::is_same< value_type, _other >::value;
	}
};

template< typename _ty, int32_t _tag = PU_STATIC >
using DoomVector = std::vector< _ty, DoomAllocator< _ty, _tag > >;

template< typename _key, typename _val, int32_t _tag = PU_STATIC >
using DoomUnorderedMap = std::unordered_map< _key, _val, std::hash< _key >, std::equal_to< _key > >; //, DoomAllocator< std::pair< const _key, _val >, _tag > >;

template< typename _key, typename _val, int32_t _tag = PU_STATIC >
using DoomMap = std::map< _key, _val, std::less< _key > >; //, DoomAllocator< std::pair< const _key, _val >, _tag > >;

using DoomString = std::basic_string< char, std::char_traits< char > >; //, DoomAllocator< char, PU_STATIC > >;

// Reformatted copypasta from https://ctrpeach.io/posts/cpp20-string-literal-template-parameters/
template< size_t _length >
struct StringLiteral
{
	constexpr StringLiteral( const char( &str )[ _length ] )
	{
		std::copy_n( str, _length, value );
	}

	constexpr const char* c_str() const { return value; }
	constexpr size_t size() const		{ return _length; }

	char value[ _length ];
};

// Support for something as simple as iota is incomplete in Clang hahaha
struct iota
{
	struct iterator
	{
		int32_t val;
		constexpr int32_t operator*() noexcept			{ return val; }
		bool INLINE operator!=( const iterator& rhs ) 	{ return val != rhs.val; }
		INLINE iterator& operator++()					{ ++val; return *this; }
	};

	iota( int32_t b, int32_t e )
	{
		_begin = { b };
		_end = { e };
	}

	constexpr iterator begin() noexcept { return _begin; }
	constexpr iterator end() noexcept { return _end; }

	iterator _begin;
	iterator _end;
};

template< typename _func, typename _t1, typename _t2, size_t... _i >
auto foreachtupleelem_impl( _t1& t1, _t2& t2, _func&& f, std::index_sequence< _i... > )
{
	return ( f( std::get< _i >( t1 ), std::get< _i >( t2 ) ) || ... );
}

template< typename _func, typename _t1, typename _t2 >
auto foreachtupleelem( _t1& t1, _t2& t2, _func&& f )
{
	enum : size_t
	{
		_t1len = std::tuple_size_v< _t1 >,
		_t2len = std::tuple_size_v< _t1 >,
		indexlen = _t1len < _t2len ? _t1len : _t2len,
	};

	return foreachtupleelem_impl( t1, t2, f, std::make_index_sequence< indexlen >() );
}

// Because C++ won't let this be proper hidden in a function definition...
template< typename... _ranges >
class range_iterators_view
{
public:
	std::tuple< typename _ranges::iterator... >	val;

	range_iterators_view()
	{
	}

	range_iterators_view( typename _ranges::iterator... v )
		: val( v... )
	{
	}

	template< size_t index >
	auto& get()
	{
		return *std::get< index >( val );
	}

	bool operator!=( const range_iterators_view& rhs )
	{
		return val != rhs.val;
	}
};

template< typename... _ranges >
auto MultiRangeView( _ranges&... r )
{
	class view
	{
	public:
		using value_type = range_iterators_view< _ranges... >;

		struct iterator
		{
			iterator()
			{
			}

			iterator( value_type& c, value_type& e )
				: curr( c )
				, end( e )
			{
			}

			value_type curr;
			value_type end;

			iterator& operator++()
			{
				std::apply(	[]( auto&... it )
							{
								auto val = std::make_tuple( ++it... );
							}, curr.val );

				bool anytrue = foreachtupleelem( curr.val, end.val, []( auto& lhs, auto& rhs ) -> bool { return lhs == rhs; } );
				if( anytrue )
				{
					curr = end;
				}

				return *this;
			}

			value_type& operator*()
			{
				return curr;
			}

			bool operator!=( const iterator& rhs )
			{
				return curr != rhs.curr;
			}
		};

		view()
		{
		}

		view( _ranges&... r )
		{
			value_type b( r.begin()... );
			value_type e( r.end()... );

			_begin	= iterator( b, e );
			_end	= iterator( e, e );
		}

		constexpr iterator begin() { return _begin; }
		constexpr iterator end() { return _end ; }

	private:
		iterator _begin;
		iterator _end;
	};

	view retval( r... );
	return retval;
}


template< typename _c1, typename _c2 >
requires std::is_same_v< typename _c1::value_type, typename _c2::value_type >
class ConcatImpl
{
public:
	using left_type = _c1;
	using right_type = _c2;

	using value_type = typename _c1::value_type;

	struct iterator
	{
		iterator()
		{
		}

		iterator( left_type& l, right_type& r, bool end )
			: currleft( end ? l.end() : l.begin() )
			, endleft( l.end() )
			, currright( end ? r.end() : r.begin() )
			, endright( r.end() )
		{
		}

		INLINE value_type& operator*() noexcept			{ return currleft == endleft ? *currright : *currleft; }
		bool INLINE operator!=( const iterator& rhs )
		{
			// This logic makes me crosseyed. But it is basically just a matrix of checking
			// when something should be checked, and when something definitively is not
			// equal to the current iterator (ie they're not iterating the same side)
			if( currleft == endleft )
			{
				if( rhs.currleft == rhs.endleft )
				{
					return currright != rhs.endright;
				}
				return true;
			}
			else
			{
				if( rhs.currleft == rhs.endleft )
				{
					return true;
				}
				return currleft != rhs.endleft;
			}
		}
		INLINE iterator& operator++()					{ if( currleft == endleft ) ++currright; else ++currleft; return *this; }

	private:
		left_type::iterator currleft;
		left_type::iterator endleft;
		right_type::iterator currright;
		right_type::iterator endright;
	};

	ConcatImpl( left_type& l, right_type& r )
		: _begin( l, r, false )
		, _end( l, r, true )
	{
	}

	iterator begin() { return _begin; }
	iterator end() { return _end; }

private:
	iterator _begin;
	iterator _end;
};

template< typename _c1, typename _c2 >
auto Concat( _c1& c1, _c2& c2 )
{
	using ConcatType = ConcatImpl< _c1, _c2 >;

	return ConcatType( c1, c2 );
}

template< typename _c1, typename _c2, typename... _cN >
auto Concat( _c1& c1, _c2& c2, _cN&... cN )
{
	auto concat = Concat( c2, cN... );

	using ConcatType = ConcatImpl< _c1, decltype( concat ) >;

	return ConcatType( c1, concat );
}

#endif // __cplusplus

#endif // __M_CONTAINER__