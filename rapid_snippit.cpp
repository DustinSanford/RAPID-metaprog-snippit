/*
A brief example of some concepts used in the RAPID framework.
Copyright (C) 2023  Dustin Sanford

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/*
This is a brief example of some of the concepts employed in the 
RAPID framework. It is designed to give a "feel" for how the 
framework works and makes no attempt at portraying production 
level/quality of code. In order to increase readability
and decrease length - most details, such as multithreading, 
heterogenous computing, and error handling, have been stripped out.
Additionally, two of RAPID's primary tools, context based tag
dispatch methods and compile time semantic enforcement via 
static_asserts, are simply unable to fit within the footprint of 
this example. 

What is included, is an example of a tree scan method used to work 
with generalized data structures. While this example is simplistic 
(only a single internal and leaf node type with basic data structures 
and no divergent control structures), I hope it becomes apparent 
the potential generative power a similar system would have.

Much of what follows (may) appear simply like an obtuse version
of similar runtime algorithms. However, the practical differences 
are quite significant. For example, in a standard runtime tree scan, 
all nodes generally contain the same type or similar types of data. 
Whereas, in (a slightly extended version of) what follows, there are
no limits on the variety, complexity, or even locality of the
data stored in nodes. Additionally, any new node types which conform 
to the provided interface are immediately and completely backwards 
compatible. Lastly, most of the function calls below produce no
additional runtime expressions, only consist of a stati_cast, or 
are inlined by the compiler and there are no virtual function 
table lookups (i.e. no runtime polymorphism).
*/

#include <iostream>
#include <random>

// Relies on the boost mpl library and mpl dependencies within boost.
// Solely header only libraries are used. The boost library can be found 
// at www.boost.org
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/equal.hpp>
#include <boost/mpl/pop_front.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/pop_front.hpp>
#include <boost/mpl/empty.hpp>
#include <boost/mpl/string.hpp>

namespace mpl = boost::mpl;

// Static cast a Parent object into a Child object
template< typename Child, typename Parent >
Child& get(Parent& p) { return p; }

// ******************************************************************
// A RECURSIVELY IMPLIMENTED SCAN FOR INHERITANCE TREES
// ******************************************************************

// tag dispatched base case (mpl::bool_<true>)  
template< typename Func, typename Parent, typename Bases, typename... Params >
void inher_tree_scan_recur(mpl::bool_<true>, Parent& p, Params&&... ps) {}

// tag dispatched recursive case (mpl::bool_<false>)
template< typename Func, typename Parent, typename Bases, typename... Params >
void inher_tree_scan_recur(mpl::bool_<false>, Parent& p, Params&&... ps)
{

	// get the first base in the mpl::vector defined by the Bases template parameter
	using current_base =
		typename mpl::deref< typename mpl::begin< Bases >::type >::type;
	// and pop it off the stack
	using remaining_bases = typename mpl::pop_front< Bases >::type;

	// call the static apply function defined by the Func template parameter
	// pass it the Parent type and current_base type
	// and forward the runtime parameters 
	Func::template apply<Parent, current_base>(p, std::forward<Params>(ps)...);

	// check if the stack is empty
	using is_empty = typename mpl::empty<remaining_bases>;

	// recursively call inher_tree_scan_recur with the remaining stack of base classes 
	// when is_stack evaluates to mpl::bool_<false> the recursion ends
	inher_tree_scan_recur< Func, Parent, remaining_bases, Params... >
		(is_empty{}, p, std::forward<Params>(ps)...);

}

// helper function for inher_tree_scan_recur
// * fills in boiler plate for the initial inher_tree_scan_recur call
// * hides recursive call implementation details
template< typename Func, typename Parent, typename... Params >
void inher_tree_scan(Parent& p, Params&&... ps)
{
	// check if the Parent has at least one base class
	using is_empty = typename mpl::empty<typename Parent::base_type>;

	// make initial inher_tree_scan_recur call
	// pass the additional template parameter Bases = Parent::base_type
	// and forward all runtime parameters
	inher_tree_scan_recur< Func, Parent, typename Parent::base_type, Params... >
		(is_empty{}, p, std::forward<Params>(ps)...);
}

// ******************************************************************
// GENERIC FUNCTOR TEMPLATES
// ******************************************************************

// a generic equality functor template
// formatted to match the interface required by inher_tree_scan 
struct add_eq
{
	template <typename Parent, typename Child>
	static void apply(Parent& a, Parent& b)
	{
		get<Child>(a) += get<Child>(b);
	}
};

// a generic random initializer functor template
// formatted to match the interface required by inher_tree_scan
template <typename Generator>
struct rand_gen_f
{
	template<typename Parent, typename Child>
	static void apply(Parent& a, Generator& g) {
		get<Child>(a).rand_gen(g);
	}
};

// a generic print functor template
// formatted to match the interface required by inher_tree_scan
struct print_f
{
	template<typename Parent, typename Child>
	static void apply(Parent& a, std::string prefix) {
		get<Child>(a).print(prefix);
	}
};

// ******************************************************************
// EXAMPLE INTERNAL NODE FOR INHERITANCE TREES
// ******************************************************************

// Variadically inherit a set of base objects
// Name template parameter acts a compile time variable name
//		and allows otherwise identical structures to fill different  
//		semantic roles
template<typename Name, typename... ChildNodes>
struct InternalNode
	: ChildNodes...
{
	using name = Name;

	// compile time equivalent of a list of pointers to child nodes
	using base_type = mpl::vector< ChildNodes... >;

	// perform addition by adding child nodes together
	template <typename RhsName>
	InternalNode& operator+=( InternalNode<RhsName, ChildNodes...>& rhs)
	{
		inher_tree_scan< add_eq >(*this, rhs);
		return *this;
	}

	// a simplistic print function 
	void print(std::string prefix = "") {
		// if not the first level then add a space
		if (prefix != "") prefix += " ";
		// add self Name to the prefix
		prefix += mpl::c_str<name>::value;
		// print all child nodes
		inher_tree_scan< print_f >(*this, prefix);
	}

	// a simplistic random initializer
	template<typename Generator>
	void rand_gen(Generator& g)
	{
		// initialize all child nodes
		inher_tree_scan< rand_gen_f<Generator> >(*this, g);
	}
};

// ******************************************************************
// EXAMPLE LEAF NODE FOR INHERITANCE TREES
// ******************************************************************

// A simple container for a type T and a compile time variable name Name
template<typename Name, typename T>
struct LeafNode
{
	using name = Name;

	LeafNode() : val() {}

	LeafNode& operator+=(LeafNode& rhs) {
		this->val += rhs.val;
		return *this;
	}

	// print the leaf node value
	void print(std::string prefix = "") {
		if (prefix != "") prefix += " ";
		std::cout << prefix << mpl::c_str<name>::value << " == " << val << std::endl;
	}

	// assign the leaf node a random value 
	// different data types are simplistically handled with a static cast
	// normally, tag dispatch would be used to hand different data types
	template< typename Generator >
	void rand_gen(Generator& gen) {
		std::normal_distribution<> dist {100, 50};
		val = static_cast<T>(dist(gen));
	}

	T val;
};

// ******************************************************************
// MAIN
// ******************************************************************

int main()
{

	// Create a data structure "on the fly"
	// This arbitrarily chosen tree is:
	// 
	// one
	//  |--> A (int)
	//  |--> B (float)
	//  |--> two
	//  |     |--> D (double)
	//  |     |--> three
	//  |            |--> E (long)
	//  |--> F (double)
	//  |--> G (int)
	using Bar = InternalNode<
		mpl::string< 'o','n','e' >,
		LeafNode<mpl::string< 'A'>, int>,
		LeafNode<mpl::string<'B'>, float >,
		InternalNode<
			mpl::string<'t','w','o'>,
			LeafNode<mpl::string<'D'>, double > ,
			InternalNode<
				mpl::string<'t','h','r','e','e'>,
				LeafNode<mpl::string<'E'>, long >
			>
		>,
		LeafNode<mpl::string< 'F'>, double>,
		LeafNode<mpl::string<'G'>,  int>
	>;

	Bar foo, bar;

	// initialize a random number generator 
	std::random_device rd{};
	std::mt19937 gen{rd()};

	// initialize bar and foo with the generator
	bar.rand_gen(gen);
	foo.rand_gen(gen);

	// print bar and foo
	bar.print("bar");
	std::cout << std::endl;
	foo.print("foo");
	std::cout << std::endl;

	// add foo to bar
	bar += foo;
	std::cout << "bar += foo\n" << std::endl;

	// reprint bar and foo
	bar.print("bar");
	std::cout << std::endl;
	foo.print("foo");
	std::cout << std::endl;

	return 0;
}