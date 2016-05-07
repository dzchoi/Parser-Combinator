// Parsers using combinatory parsing (rather than manual recursive-descent parsing)
// dzchoi,
// Apr/20/15, first working version
// Apr/21/15, :%s/shared_ptr/shared_ptr/g
// Apr/21/15, fix to return for first parser failure in parser_cat and parser_seq
// Apr/25/15, renamed apply(p, f) to "p >> f" and f can be a parsing function as well as
//	      a normal function

#include <istream> // for std::istream, ...
#include <memory> // for std::shared_ptr
#include <string> // for std::string

// A parser is a functor(mapping) of an istream to a parse tree; it is working on istream
// rather than the lower-level streambuf for directly referencing and manipulating the
// failbit and the eofbit.
// Like Parsec in Haskell, there are two kinds of failures:
// - "weak failure" or "failure but consume nothing", if a parser is failed at the very
//   first character, the parser does not throw an exception but returns a default value
//   (like nullptr) instead, with making the istream failed and not consuming the first
//   character.
// - "error failure", if a parser is failed at the second or later character, the parser
//   throws an exception whether or not the first character was matched and consumed.
// - this differentiation helps to write a (sub-)parser for LL(1) without try_().

// references:
// - "An Introduction to the Parsec Library",
//   https://kunigami.wordpress.com/2014/01/21/an-introduction-to-the-parsec-library/
// - "jneen/parsimmon", https://github.com/jneen/parsimmon
// - "keean/Parser-Combinators", https://github.com/keean/Parser-Combinators
// - "Stupid Template Tricks - Pride and Parser Combinators",
//   http://blog.mattbierner.com/stupid-template-tricks-pride-and-parser-combinators-part-one/
// - C++ containers member function table
//   http://en.cppreference.com/w/cpp/container
// - "Packat Parsing: Simple, Powerful, Lazy, Linear Time",
//   http://pdos.csail.mit.edu/~baford/packrat/icfp02/
// - "Getting Lazy with C++", http://bartoszmilewski.com/2014/04/21/getting-lazy-with-c/ 
// - "C++11: STD::FUNCTION AND STD::BIND",
//   http://oopscenities.net/2012/02/24/c11-stdfunction-and-stdbind/
// - "Higher-Order Functions in C++",
//   https://forfunand.wordpress.com/2012/04/09/higher-order-functions-in-c/
// - "How to get the line number from a file in C++?",
//   http://stackoverflow.com/questions/4813129/how-to-get-the-line-number-from-a-file-in-c
// - "substream from istream",
//   http://stackoverflow.com/questions/7623277/substream-from-istream
// - "Copy, load, redirect and tee using C++ streambufs",
//   http://wordaligned.org/articles/cpp-streambufs
// - "A beginner's guide to writing a custom stream buffer (std::streambuf)",
//   http://www.mr-edd.co.uk/blog/beginners_guide_streambuf
// - "Tips and tricks for using C++ I/O (input/output)",
//   http://www.augustcouncil.com/~tgibson/tutorial/iotips.html

// TODO:
// - a parser combinator (,) combining parser results into a tuple and passing it to a
//   variadic function (C++11 required)

// character parsers:
// chr('c')
// any_chr()	    - /./
// one_of("abc")   - /[abc]/
// none_of("abc")  - /[^abc]/
// blank()	    - space or tab
// letter()
// alphanum()
// digit()

// void parsers:
// eof()
// skip(p)	    - generic void parser
// skip('c')
// skip("abc")
// blanks()	    - optionally consume blanks

// string parsers:
// +p		    - convert a character parser into a string parser
// p + q	    - concatenate string parsers

// parser combinators:
// p >> f	    - parse p, and if p succeeds apply T f(U) to the result U from p;
//		      p can be a void parser (and f is of type T f())
// p >> f	    - for custom parsering function f of type T f(istream &, U) or
//		      T f(istream &)
// many<C>(p)	    - parse /p*/ and return the collection of results from p's in a
//		      container of type C
// many(p)	    - string parser when p is a character parser
// many(p)	    - void parser when p is a void parser
// many1(p, f)	    - parse /p+/ and return the collection of results from p's using
//		      T f(T, T) such as foldl1() in Haskell
// many1(p)	    - void parser when p is a void parser
// p > q	    - return result from q if p and q are parsed successfully; p can be
//		      a void parser; return result from p if q is a void parser
// sep_by<C>(p, q)  - parse /(p (q p)*)?/ and return the collection of results from p's
//		      in a container of type C
// sep_by(p, q)	    - string parser when p is a character parser
// sep_by(p, q)	    - void parser when p is a void parser
// sep_by1(p, q, f) - parse /p (q p)*/ and return the collection of results from p's
//		      using T f(T, T)
// sep_by1(p, q)    - void parser when p is a void parser
// p | q	    - parse p first, and if p fails and consumes nothing parse q
// try_(p)	    - parse p, and backtrack the istream if "error failure" (but istream
//		      remains marked as failure)

// TODO:
// - other name for try_()? lookahead?

// ToThink:
// - always the recursive-descent version of parser
// - match_and := sep_by1(match_seq, blanks() >> skip('&'), regex_and)
// - match_id := apply( lookup,
//		(chr('_') | letter()) + many(chr('_') | alphanum()) )
// - blanks() := many(one_of(" \t"), skip) where skip is a void-function
//   or blanks() := many(skip(blank()))
// - !{symbol_alias|symbol_alias|...} :=
//   skip('!') > skip('{') > sep_by1(match_id, blanks() > skip('|'), aliases) > skip('}')
// - variadic apply(f, p, q, ...)



#include <streambuf> // for std::streambuf

// pos_stream derives streambuf and contains an additional Pos object
class pos_stream : public std::streambuf {
protected:
    std::streambuf *const sbuf;

    std::streambuf::int_type underflow() { return sbuf->sgetc(); }

    std::streambuf::int_type uflow() { return c = sbuf->sbumpc(); }
	// Note uflow() is not called for reading out eof.

    std::streampos seekoff(std::streamoff off, std::ios_base::seekdir way,
	std::ios_base::openmode which =std::ios_base::in | std::ios_base::out)
    {
	// Note istream(not streambuf) implements tellg() as seekoff(0, ios_base::cur).
	return sbuf->pubseekoff(off, way, which);
    }

    std::streampos seekpos(std::streampos pos,
	std::ios_base::openmode which =std::ios_base::in | std::ios_base::out)
    {
	return sbuf->pubseekpos(pos, which);
    }

public:
    // Pos keeps track of the current reading position. Parsec in Haskell has every
    // parser get a pos as an argument and return a pos as a result. However, we here
    // include into a streambuf the pos that is updated by request, update(); the pos
    // here gets updated manually to accomodate streambufs that do not enable seekoff()
    // and seekpos(). For automatic updates, uflow() would take the responsibility of
    // advancing the pos, and seekoff(), seekpos() and pbackfail() should also take care
    // of rewinding the pos and besides the case the nested streambuf itself might not
    // enable seekoff() and seekpos() should be considered.
    struct Pos {
	std::streamoff off; // tracks file position like istream::tellg()
	int row, col; // cursor position

	Pos() : off(0), row(1), col(1) {}
	// the default functions will do enough
	//Pos(const Pos &p) : off(p.off), row(p.row), col(p.col) {}
	//void operator=(const Pos &p) { off = p.off; row = p.row; col = p.col; }

	void update(char c) {
	    off++;
	    if ( c == '\t' )
		col += 8 - (col-1) % 8;
	    else if ( c == '\n' )
		row++, col = 1;
	    else
		col++;
	}
    } pos;

    char c; // last character read

    pos_stream(std::streambuf *sbuf) : sbuf(sbuf) {}
};

// typing savers for static_cast<pos_stream *>(s.rdbuf())
inline void update_pos(std::istream &s)
{
    pos_stream *const ps = static_cast<pos_stream *>(s.rdbuf());
    ps->pos.update(ps->c);
}

inline std::streamoff tellg(std::istream &s, std::streamoff since)
// similar to s.tellg() but also work for istreams that disables it
{
    return static_cast<pos_stream *>(s.rdbuf())->pos.off - since;
}

inline pos_stream::Pos &pos(std::istream &s)
{
    return static_cast<pos_stream *>(s.rdbuf())->pos;
}

// exception for a parsing error
struct ParserError {};

// macros to help throwing exceptions
#define MARK	std::streamoff _off(tellg(s, 0))

#define CHECK \
    if ( s.fail() && tellg(s, _off) ) \
	throw ParserError(); \
    else

#define RETURN(...) \
    do { \
	if ( s.fail() && tellg(s, _off) ) \
	    throw ParserError(); \
	return __VA_ARGS__; \
    } while( false )
    // throw exception only if fail and consume nothing
    // RETURN(x) equals CHECK; return x;

#define RETURN_IF_FAIL(...) \
    if ( s.fail() ) { \
	if ( tellg(s, _off) ) \
	    throw ParserError(); \
	return __VA_ARGS__; \
    } else
    // throw exception only if fail and consume nothing
    // RETURN() and RETURN_IF_FAIL() is designed to be close to a statement (though not
    // a function) that can be nested in another statement.



/* // for supporting function parsers that are not so useful as parsing functions
template <typename T, typename U =void>
// parser of type T(U)
class parser {
public:
    virtual T operator()(std::istream &, U) const =0; // a parser is just a function
    virtual ~parser() {}
};

template <typename T>
// T is the result type(a parse tree normally) of parser
class parser<T, void> {
public:
    virtual T operator()(std::istream &) const =0; // a parser is just a function
    virtual ~parser() {}
};
*/

template <typename T>
// T is the result type(a parse tree normally) of parser
class parser {
public:
    virtual T operator()(std::istream &) const =0; // a parser is just a function
    virtual ~parser() {}
};

// cin >> p: apply a parser to an istream
template <typename T>
inline T operator>>(std::istream &s, const std::shared_ptr<parser<T>> &p)
{
    // p is declared of a const reference type not to affect its memory allocation
    return p->operator()(s);
}

template <> // function template specialization
inline void operator>>(std::istream &s, const std::shared_ptr<parser<void>> &p)
{
    p->operator()(s);
}

/* // provided in C++ by default??
// cin >> f for a parsing function f
template <typename T>
inline T operator>>(std::istream &s, T (*f)(std::istream &))
{
    return f(s);
}
*/



// abstract character-matching parser
class parser_match : public parser<char> {
protected:
    virtual bool match(char) const =0;

public:
    char operator()(std::istream &s) const override {
	if ( s.fail() )
	    // Note fail() == failbit | badbit and failbit is independent of the eofbit.
	    throw ParserError(); // expecting 'c'

	const char c = s.peek();
	if ( match(c) ) { // may possibly set eofbit
	    s.ignore(); // consume 'c'
	    update_pos(s);
	    return c;
	}

	s.setstate(std::ios::failbit); // mark failure
	return char(); // return 0 if not matched
    }

    virtual ~parser_match() {}
};



class parser_chr : public parser_match {
protected:
    const char c;
    bool match(char c) const override { return c == parser_chr::c; }

public:
    parser_chr(char c) : c(c) {}
};

// chr('c'): character-matching parser
inline std::shared_ptr<parser<char>> chr(char c)
{
    return std::shared_ptr<parser<char>>(new parser_chr(c));
}



class parser_any_char : public parser_match {
protected:
    bool match(char) const override { return true; }

public:
    parser_any_char() {}
};

// any_chr(): /./
inline std::shared_ptr<parser<char>> any_chr()
{
    return std::shared_ptr<parser<char>>(new parser_any_char());
}



#include <cstring> // for strchr()

class parser_one_of : public parser_match {
protected:
    const char *const s;
    bool match(char c) const override { return strchr(s, c); }

public:
    parser_one_of(const char *s) : s(s) {}
};

// one_of("abc"): /[abc]/
inline std::shared_ptr<parser<char>> one_of(const char *s)
{
    return std::shared_ptr<parser<char>>(new parser_one_of(s));
}



class parser_none_of : public parser_match {
protected:
    const char *const s;
    bool match(char c) const override { return !strchr(s, c); }

public:
    parser_none_of(const char *s) : s(s) {}
};

// none_of("abc"): /[^abc]/
inline std::shared_ptr<parser<char>> none_of(const char *s)
{
    return std::shared_ptr<parser<char>>(new parser_none_of(s));
}

// blank(): /[ \t]/
inline std::shared_ptr<parser<char>> blank() { return one_of(" \t"); }
    // could use isblank() if in C++11



#include <cctype> // for isalpha(), isalnum(), isdigit()

class parser_fmatch : public parser_match {
protected:
    int (*const f)(int);
    bool match(char c) const override { return f(c); }

public:
    parser_fmatch(int (*f)(int)) : f(f) {}
};

// letter()
inline std::shared_ptr<parser<char>> letter()
{
    return std::shared_ptr<parser<char>>(new parser_fmatch(isalpha));
}

// alphanum()
inline std::shared_ptr<parser<char>> alphanum()
{
    return std::shared_ptr<parser<char>>(new parser_fmatch(isalnum));
}

// digit()
inline std::shared_ptr<parser<char>> digit()
{
    return std::shared_ptr<parser<char>>(new parser_fmatch(isdigit));
}



class parser_eof : public parser<void> {
public:
    void operator()(std::istream &s) const override {
	if ( s.fail() )
	    throw ParserError(); // expecting eof

	if ( !(s.eof() || s.peek() == EOF) ) // may possibly set eofbit
	    s.setstate(std::ios::failbit); // mark failure if not eof
	// no need for s.ignore() and s.rdbuf()->update() on eof
    }

    parser_eof() {}
};

// eof()
inline std::shared_ptr<parser<void>> eof()
{
    return std::shared_ptr<parser<void>>(new parser_eof());
}
//inline std::shared_ptr<parser<void>> eof() { return skip(EOF); }
    // This is not working because after peeking eof s.ignore() will mark the failbit.



template <typename T>
class parser_skip : public parser<void> {
protected:
    const std::shared_ptr<parser<T>> p;

public:
    void operator()(std::istream &s) const override { p->operator()(s); }

    parser_skip(std::shared_ptr<parser<T>> p) : p(std::move(p)) {}
};

// skip(p): parse p and return nothing
template <typename T>
inline std::shared_ptr<parser<void>> skip(std::shared_ptr<parser<T>> p)
{
    return std::shared_ptr<parser<void>>(new parser_skip<T>(std::move(p)));
}

// skip('c'): character-matching void parser
inline std::shared_ptr<parser<void>> skip(char c) { return skip(chr(c)); }



class parser_str : public parser<void> {
protected:
    const char *const s;

public:
    void operator()(std::istream &s) const override {
	if ( s.fail() )
	    throw ParserError(); // expecting s

	MARK;
	for ( const char *t = parser_str::s ; *t ; t++ )
	    if ( s.peek() == *t ) {
		s.ignore(); // consume *t
		update_pos(s);
	    }
	    else {
		s.setstate(std::ios::failbit);
		RETURN(); // result in "weak failure" or "error failure"
		    // redundant to check for s.fail() within RETURN()
	    }
	// do nothing if parser_str::s is empty
    }

    parser_str(const char *s) : s(s) {}
};

// skip("abc"): string-matching void parser
// skip("abc") equals "skip('a') >> skip('b') >> skip('c')"
inline std::shared_ptr<parser<void>> skip(const char *s)
{
    return std::shared_ptr<parser<void>>(new parser_str(s));
}



template <typename U, typename T>
class parser_map : public parser<T> {
protected:
    const std::shared_ptr<parser<U>> p;
    T (*const f)(U);

public:
    T operator()(std::istream &s) const override {
	U u(p->operator()(s)); //or const U &u??
	if ( s.fail() )
	    return T(); // return the default value of T if failed
	return f(u);
    }

    parser_map(std::shared_ptr<parser<U>> p, T (*f)(U)) : p(std::move(p)), f(f) {}
};

// "p >> f": parse p, and if p succeeds apply T f(U) to the result from p
// operator>() could be used rather than operator>>(), but for the sake of readability
template <typename U, typename T>
inline std::shared_ptr<parser<T>> operator>>(std::shared_ptr<parser<U>> p, T (*f)(U))
{
    return std::shared_ptr<parser<T>>(new parser_map<U, T>( std::move(p), f ));
}
 
template <typename T>
class parser_map<void, T> : public parser<T> {
protected:
    const std::shared_ptr<parser<void>> p;
    T (*const f)();

public:
    T operator()(std::istream &s) const override {
	p->operator()(s);
	if ( s.fail() )
	    return T(); // return the default value of T if failed
	return f();
    }

    parser_map(std::shared_ptr<parser<void>> p, T (*f)()) : p(std::move(p)), f(f) {}
};

// "p >> f" when p is a void parser; that is, "p >> return f()" in Haskell
template <typename T>
inline std::shared_ptr<parser<T>> operator>>(std::shared_ptr<parser<void>> p, T (*f)())
// overloading necessary since U of T (*f)(U) cannot be deduced as void from T(*f)()
{
    return std::shared_ptr<parser<T>>(new parser_map<void, T>( std::move(p), f ));
}



class parser_cat : public parser<std::string> {
protected:
    const std::shared_ptr<parser<std::string>> p;
    const std::shared_ptr<parser<std::string>> q;

public:
    std::string operator()(std::istream &s) const override {
	MARK;
	std::string t(p->operator()(s));
	if ( !s.fail() ) {
	    t.append(q->operator()(s));
	    CHECK;
	}
	return t;
    }

    parser_cat(
	std::shared_ptr<parser<std::string>> p, std::shared_ptr<parser<std::string>> q)
    : p(std::move(p)), q(std::move(q)) {}
};

// "p + q": concatenation of string parsers
inline std::shared_ptr<parser<std::string>> operator+(
    std::shared_ptr<parser<std::string>> p, std::shared_ptr<parser<std::string>> q)
{
    return std::shared_ptr<parser<std::string>>(new parser_cat(
	std::move(p), std::move(q) ));
}

inline std::string char_to_string(char c) { return std::string(1, c); }

// "+p": unary operator for converting a character parser into a string parser
inline std::shared_ptr<parser<std::string>> operator+(std::shared_ptr<parser<char>> p)
{
    return std::move(p) >> char_to_string;
}

// "p + q" when p is a character parser
inline std::shared_ptr<parser<std::string>> operator+(
    std::shared_ptr<parser<char>> p, std::shared_ptr<parser<std::string>> q)
{
    return +std::move(p) + std::move(q);
}

// "p + q" when q is a character parser
inline std::shared_ptr<parser<std::string>> operator+(
    std::shared_ptr<parser<std::string>> p, std::shared_ptr<parser<char>> q)
{
    return std::move(p) + +std::move(q);
}

// "p + q" when both are character parsers
inline std::shared_ptr<parser<std::string>> operator+(
    std::shared_ptr<parser<char>> p, std::shared_ptr<parser<char>> q)
{
    return +std::move(p) + +std::move(q);
}



template <class C>
// T(== C::value_type) is type of the result from p, C is type of a (generic) container
// for Ts.
class parser_many : public parser<C> {
protected:
    const std::shared_ptr<parser<typename C::value_type>> p;
	// parser to apply repeatedly

public:
    C operator()(std::istream &s) const override {
	for ( C c ;; ) {
	    C::value_type t(p->operator()(s)); //or const C::value_type &t??
	    if ( s.fail() )
		// recover failure, since the failure is used to check only for the end
		// of the combined parser and the combined parser will always result in
		// success as an optional parser.
		return s.clear(), c; // c is passed by copying
	    c.insert(c.end(), t); // build up result
	}
    }

    parser_many(std::shared_ptr<parser<typename C::value_type>> p)
    : p(std::move(p)) {}
};

// many<C>(p): parse /p*/ and return the collection of results from p's in a C container
template <class C>
inline std::shared_ptr<parser<C>> many(std::shared_ptr<parser<typename C::value_type>> p)
{
    return std::shared_ptr<parser<C>>(new parser_many<C>( std::move(p) ));
}

// many(p) when p is a character parser
inline std::shared_ptr<parser<std::string>> many(std::shared_ptr<parser<char>> p)
// no specialization "template <>", otherwise the compiler could not deduce C from the
// element type C::value_type; e.g., std::vector<char> and std::string have the same
// value_type.
{
    return many<std::string>(std::move(p));
    // equivalently,
    //return std::shared_ptr<parser<std::string>>(new parser_many<std::string>(
	//std::move(p) ));
}

template <>
class parser_many<void> : public parser<void> {
protected:
    const std::shared_ptr<parser<void>> p;

public:
    void operator()(std::istream &s) const override {
	do p->operator()(s);
	while ( !s.fail() );
	s.clear();
    }

    parser_many(std::shared_ptr<parser<void>> p) : p(std::move(p)) {}
};

// many(p) when p is a void parser
inline std::shared_ptr<parser<void>> many(std::shared_ptr<parser<void>> p)
{
    return std::shared_ptr<parser<void>>(new parser_many<void>( std::move(p) ));
}

// blanks(): optional void parser consuming blanks
inline std::shared_ptr<parser<void>> blanks()
{
    return many(skip(blank()));
}



template <typename T>
class parser_many1 : public parser<T> {
protected:
    const std::shared_ptr<parser<T>> p;
    T (*const f)(T, T); // combiner

public:
    T operator()(std::istream &s) const override {
	T c(p->operator()(s));
	if ( s.fail() )
	    return c; // we must parse p at least once
	for ( ;; ) {
	    T t(p->operator()(s)); //or const T &t??
	    if ( s.fail() )
		// recover failure, since the failure is used to check only for the end
		// of the combined parser and the combined parser will result in success
		// for 2nd or later parse failure.
		return s.clear(), c; // c is passed by copying
	    c = f(c, t); // build up result
	}
    }

    parser_many1(std::shared_ptr<parser<T>> p, T (*f)(T, T)) : p(std::move(p)), f(f) {}
};

// many1(p, f): parse /p+/ and return the collection of results from p's using T f(T, T)
template <typename T>
inline std::shared_ptr<parser<T>> many1(std::shared_ptr<parser<T>> p, T (*f)(T, T))
{
    return std::shared_ptr<parser<T>>(new parser_many1<T>( std::move(p), f ));
}

template <>
class parser_many1<void> : public parser<void> {
protected:
    const std::shared_ptr<parser<void>> p;

public:
    void operator()(std::istream &s) const override {
	p->operator()(s);
	if ( !s.fail() ) { // we must parse p at least once
	    do p->operator()(s);
	    while ( !s.fail() );
	    s.clear();
	}
    }

    parser_many1(std::shared_ptr<parser<void>> p) : p(std::move(p)) {}
};

// many1(p) when p is a void parser
inline std::shared_ptr<parser<void>> many1(std::shared_ptr<parser<void>> p)
{
    return std::shared_ptr<parser<void>>(new parser_many1<void>( std::move(p) ));
}

// many1(p) for a character parser p and more general many1<C>(p) are not provided as now



/* // for supporting function parsers that are not so useful as parsing functions
template <typename U, typename T>
class parser_chain : public parser<T> {
protected:
    const std::shared_ptr<parser<U>> p;
    const std::shared_ptr<parser<T, U>> q;

public:
    T operator()(std::istream &s) const override {
	MARK;
	U u(p->operator()(s)); //or const U &u??
	if ( s.fail() )
	    return T();
	T t(q->operator()(s, u)); //or const T &t??
	RETURN(t);
    }

    parser_chain(std::shared_ptr<parser<U>> p, std::shared_ptr<parser<T, U>> q)
    : p(std::move(p)), q(std::move(q)) {}
};

template <typename U, typename T>
inline std::shared_ptr<parser<T>> operator>>(
    std::shared_ptr<parser<U>> p, std::shared_ptr<parser<T, U>> q)
{
    return std::shared_ptr<parser<T>>(new parser_chain<U, T>(
	std::move(p), std::move(q) ));
}
*/

// chaining a parser and a parsing(consuming stream) function (rather than a function
// parser) since a parsing function is easier to write in C/C++ than a function parser
// which should be defined through a derived parser class. A parsing function is the same
// as the normal function except it takes istream & as an additional argument.
template <typename U, typename T>
class parser_chain : public parser<T> {
protected:
    const std::shared_ptr<parser<U>> p;
    T (*const f)(std::istream &, U);

public:
    T operator()(std::istream &s) const override {
	U u(p->operator()(s)); //or const U &u??
	if ( s.fail() )
	    return T(); // return the default value of T if failed
	return f(s, u);
    }

    parser_chain(std::shared_ptr<parser<U>> p, T (*f)(std::istream &, U))
    : p(std::move(p)), f(f) {}
};

// "p >> f": parse p, and if p succeeds apply T f(s, U) to the result U from p
// operator>() could be used rather than operator>>(), but for the sake of readability
template <typename U, typename T>
inline std::shared_ptr<parser<T>> operator>>(
    std::shared_ptr<parser<U>> p, T (*f)(std::istream &, U))
{
    return std::shared_ptr<parser<T>>(new parser_chain<U, T>( std::move(p), f ));
}

template <typename T>
class parser_chain<void, T> : public parser<T> {
protected:
    const std::shared_ptr<parser<void>> p;
    T (*const f)(std::istream &);

public:
    T operator()(std::istream &s) const override {
	p->operator()(s);
	if ( s.fail() )
	    return T(); // return the default value of T if failed
	return f(s);
    }

    parser_chain(std::shared_ptr<parser<void>> p, T (*f)(std::istream &))
    : p(std::move(p)), f(f) {}
};

// "p >> f" when p is a void parser
template <typename T>
inline std::shared_ptr<parser<T>> operator>>(
    std::shared_ptr<parser<void>> p, T (*f)(std::istream &))
// overloading necessary since U of T (*f)(U) cannot be deduced as void from T(*f)()
{
    return std::shared_ptr<parser<T>>(new parser_chain<void, T>( std::move(p), f ));
}



template <typename U, typename T>
// p is a U-parser and q is a T-parser
class parser_seq : public parser<T> {
protected:
    const std::shared_ptr<parser<U>> p;
    const std::shared_ptr<parser<T>> q;

public:
    T operator()(std::istream &s) const override {
	// p and q are parsed sequentially
	MARK;
	p->operator()(s);
	if ( s.fail() )
	    // if first parser fails we do not try second parser
	    return T(); // whether or not U is void
	T t(q->operator()(s)); //or const T &t??
	RETURN(t);
	    // throw exception if second parser fails; otherwise, a sequence of parsers
	    // with all but the last one having succeeded would escape the outer try_().
    }

    parser_seq(std::shared_ptr<parser<U>> p, std::shared_ptr<parser<T>> q)
    : p(std::move(p)), q(std::move(q)) {}
};

// "p > q": return result from q if p and q are parsed successfully
template <typename U, typename T>
inline std::shared_ptr<parser<T>> operator>(
    std::shared_ptr<parser<U>> p, std::shared_ptr<parser<T>> q)
{
    return std::shared_ptr<parser<T>>(new parser_seq<U, T>(
	std::move(p), std::move(q) ));
}

template <typename T>
class parser_seq<T, void> : public parser<T> {
protected:
    const std::shared_ptr<parser<T>> p;
    const std::shared_ptr<parser<void>> q;

public:
    T operator()(std::istream &s) const override {
	MARK;
	T t(p->operator()(s)); //or const T &t??
	if ( !s.fail() ) {
	    q->operator()(s);
	    CHECK;
	}
	return t;
    }

    parser_seq(std::shared_ptr<parser<T>> p, std::shared_ptr<parser<void>> q)
    : p(std::move(p)), q(std::move(q)) {}
};

template <>
class parser_seq<void, void> : public parser<void> {
protected:
    const std::shared_ptr<parser<void>> p;
    const std::shared_ptr<parser<void>> q;

public:
    void operator()(std::istream &s) const override {
	MARK;
	p->operator()(s);
	if ( !s.fail() ) {
	    q->operator()(s);
	    CHECK;
	}
	return;
    }

    parser_seq(std::shared_ptr<parser<void>> p, std::shared_ptr<parser<void>> q)
    : p(std::move(p)), q(std::move(q)) {}
};

// "p > q" when q is a void parser
template <typename T>
inline std::shared_ptr<parser<T>> operator>(
    std::shared_ptr<parser<T>> p, std::shared_ptr<parser<void>> q)
{
    return std::shared_ptr<parser<T>>(new parser_seq<T, void>(
	std::move(p), std::move(q) ));
}

/* // unnecessary as a plain specialization of above operator>()
// "p > q" when both are void parsers
template <>
inline std::shared_ptr<parser<void>> operator>(
    std::shared_ptr<parser<void>> p, std::shared_ptr<parser<void>> q)
{
    return std::shared_ptr<parser<void>>(new parser_seq<void, void>(
	std::move(p), std::move(q) ));
}
*/



template <class C, typename U>
// T(== C::value_type) is type of the result from p, C is type of a (generic) container
// for Ts. U is type of the separator, usually void.
class parser_sep_by : public parser<C> {
protected:
    const std::shared_ptr<parser<typename C::value_type>> p;
	// parser to apply repeatedly
    const std::shared_ptr<parser<U>> q; // separator

public:
    C operator()(std::istream &s) const override {
	MARK;
	C c;
	C::value_type t(p->operator()(s)); //or const C::value_type &t??
	if ( s.fail() )
	    return s.clear(), c; // return empty container
	c.insert(c.end(), t);
	for ( ;; ) {
	    q->operator()(s);
	    if ( s.fail() )
		// recover failure, since the failure is used to check only for the end
		// of the combined parser and the combined parser will result in success
		// for 2nd or later parse failure.
		return s.clear(), c; // c is passed by copying
	    C::value_type t(p->operator()(s)); //or const C::value_type &t??
	    RETURN_IF_FAIL(C()); // must parse p after the separator
		// return the default value of C if failed
	    c.insert(c.end(), t); // build up result
	}
    }

    parser_sep_by(
	std::shared_ptr<parser<typename C::value_type>> p, std::shared_ptr<parser<U>> q)
    : p(std::move(p)), q(std::move(q)) {}
};

// sep_by<C>(p, q): parse /(p (q p)*)?/ and return the collection of results from p's
// in a C container
template <class C, typename U>
inline std::shared_ptr<parser<C>> sep_by(
    std::shared_ptr<parser<typename C::value_type>> p, std::shared_ptr<parser<U>> q)
{
    return std::shared_ptr<parser<C>>(new parser_sep_by<C, U>(
	std::move(p), std::move(q) ));
}

// sep_by(p, q) when p is a character parser
template <typename U>
inline std::shared_ptr<parser<std::string>> sep_by(
    std::shared_ptr<parser<char>> p, std::shared_ptr<parser<U>> q)
{
    return sep_by<std::string>(std::move(p), std::move(q));
}

template <typename U>
class parser_sep_by<void, U> : public parser<void> {
protected:
    const std::shared_ptr<parser<void>> p;
    const std::shared_ptr<parser<U>> q; // separator

public:
    void operator()(std::istream &s) const override {
	MARK;
	p->operator()(s);
	if ( !s.fail() )
	    while ( q->operator()(s), !s.fail() ) {
		p->operator()(s);
		RETURN_IF_FAIL(); // must parse p after the separator
	    }
	s.clear(); // always success except for failure after separator
    }

    parser_sep_by(std::shared_ptr<parser<void>> p, std::shared_ptr<parser<U>> q)
    : p(std::move(p)), q(std::move(q)) {}
};

// sep_by(p, q) when p is a void parser
template <typename U>
inline std::shared_ptr<parser<void>> sep_by(
    std::shared_ptr<parser<void>> p, std::shared_ptr<parser<U>> q)
{
    return std::shared_ptr<parser<void>>(new parser_sep_by<void, U>(
	std::move(p), std::move(q) ));
}



/* // not provided since "many1<C>(p)" is not provided either
template <class C, typename U>
// T(== C::value_type) is type of the result from p, C is type of a (generic) container
// for Ts. U is type of the separator, usually void.
class parser_sep_by1 : public parser<C> {
protected:
    const std::shared_ptr<parser<typename C::value_type>> p;
	// parser to apply repeatedly
    const std::shared_ptr<parser<U>> q; // separator

public:
    C operator()(std::istream &s) const override {
	MARK;
	for ( C c ;; ) {
	    C::value_type t(p->operator()(s)); //or const C::value_type &t??
	    RETURN_IF_FAIL(C()); // we must parse p at least once
		// return the default value of C if failed
	    c.insert(c.end(), t); // build up result
	    q->operator()(s);
	    if ( s.fail() )
		// recover failure, since the failure is used to check only for the end
		// of the combined parser and the combined parser will result in success
		// for 2nd or later parse failure.
		return s.clear(), c; // c is passed by copying
	}
    }

    parser_sep_by1(
	std::shared_ptr<parser<typename C::value_type>> p, std::shared_ptr<parser<U>> q)
    : p(std::move(p)), q(std::move(q)) {}
};

// sep_by1<C>(p, q): parse /(p (q p)*)?/ and return the collection of results from p's
// in a C container
template <class C, typename U>
inline std::shared_ptr<parser<C>> sep_by1(
    std::shared_ptr<parser<typename C::value_type>> p, std::shared_ptr<parser<U>> q)
{
    return std::shared_ptr<parser<C>>(new parser_sep_by1<C, U>(
	std::move(p), std::move(q) ));
}
*/

template <typename T, typename U>
// p is a T-parser and q is a U-parser(separator), U is usually void.
class parser_sep_by1 : public parser<T> {
protected:
    const std::shared_ptr<parser<T>> p; // parser to apply repeatedly
    const std::shared_ptr<parser<U>> q; // separator
    T (*const f)(T, T); // combiner

public:
    T operator()(std::istream &s) const override {
	MARK;
	T c(p->operator()(s));
	if ( s.fail() )
	    return c; // we must parse p at least once
	for ( ;; ) {
	    q->operator()(s);
	    if ( s.fail() )
		// recover failure, since the failure is used to check only for the end
		// of the combined parser and the combined parser will result in success
		// for 2nd or later parse failure.
		return s.clear(), c; // c is passed by copying
	    T t(p->operator()(s)); //or const T &t??
	    RETURN_IF_FAIL(T()); // must parse p after the separator
		// return the default value of T if failed
	    c = f(c, t); // build up result
	}
    }

    parser_sep_by1(
	std::shared_ptr<parser<T>> p, std::shared_ptr<parser<U>> q, T (*f)(T, T))
    : p(std::move(p)), q(std::move(q)), f(f) {}
};

// sep_by1(p, q, f): parse /p (q p)*/ and return the collection of results from p's
// using T f(T, T)
template <typename T, typename U>
inline std::shared_ptr<parser<T>> sep_by1(
    std::shared_ptr<parser<T>> p, std::shared_ptr<parser<U>> q, T (*f)(T, T))
{
    return std::shared_ptr<parser<T>>(new parser_sep_by1<T, U>(
	std::move(p), std::move(q), f ));
}

template <typename U>
// p is a void parser and q is a U-parser(separator), U is usually void.
class parser_sep_by1<void, U> : public parser<void> {
protected:
    const std::shared_ptr<parser<void>> p; // parser to apply repeatedly
    const std::shared_ptr<parser<U>> q; // separator

public:
    void operator()(std::istream &s) const override {
	MARK;
	do {
	    p->operator()(s);
	    RETURN_IF_FAIL(); // we must parse p at least once
	    q->operator()(s);
	} while ( !s.fail() )
	s.clear();
    }

    parser_sep_by1(std::shared_ptr<parser<void>> p, std::shared_ptr<parser<U>> q)
    : p(std::move(p)), q(std::move(q)) {}
};

// sep_by1(p, q) when p is a void parser
template <typename U>
inline std::shared_ptr<parser<void>> sep_by1(
    std::shared_ptr<parser<void>> p, std::shared_ptr<parser<U>> q)
{
    return std::shared_ptr<parser<void>>(new parser_sep_by1<void, U>(
	std::move(p), std::move(q) ));
}



template <typename T>
class parser_alt : public parser<T> {
protected:
    const std::shared_ptr<parser<T>> p;
    const std::shared_ptr<parser<T>> q;

public:
    T operator()(std::istream &s) const override {
	T t(p->operator()(s)); //or const T &t??
	if ( !s.fail() )
	    return t;
	s.clear();
	    // recover failure, since the combined parser is not failed yet and now
	    // depends on the second parser.
	return q->operator()(s);
    }

    parser_alt(std::shared_ptr<parser<T>> p, std::shared_ptr<parser<T>> q)
    : p(std::move(p)), q(std::move(q)) {}
};

template <>
class parser_alt<void> : public parser<void> {
protected:
    const std::shared_ptr<parser<void>> p;
    const std::shared_ptr<parser<void>> q;

public:
    void operator()(std::istream &s) const override {
	p->operator()(s);
	if ( s.fail() ) {
	    s.clear();
	    q->operator()(s);
	}
    }

    parser_alt(std::shared_ptr<parser<void>> p, std::shared_ptr<parser<void>> q)
    : p(std::move(p)), q(std::move(q)) {}
};

// "p | q": parse p first, and if p fails and consumes nothing parse q
template <typename T>
inline std::shared_ptr<parser<T>> operator|(
    std::shared_ptr<parser<T>> p, std::shared_ptr<parser<T>> q)
{
    return std::shared_ptr<parser<T>>(new parser_alt<T>(
	std::move(p), std::move(q) ));
}



template <typename T>
class parser_try : public parser<T> {
protected:
    const std::shared_ptr<parser<T>> p;

public:
    T operator()(std::istream &s) const override {
	pos_stream::Pos saved_pos = pos(s);
	std::streampos	tellg = s.tellg();
	try {
	    return p->operator()(s);
	}
	catch ( ParserError ) {
	    if ( tellg != std::ios::pos_type(std::ios::off_type(-1)) ) {
		// call seekg() only if enabled
		s.clear(); // unnecessary if in C++11
		s.seekg(tellg);
		s.setstate(std::ios::failbit); // mark failure again
		pos(s) = saved_pos;
	    }
	    return T(); // return the default value of T if failed
	}
    }

    parser_try(std::shared_ptr<parser<T>> p) : p(std::move(p)) {}
};

// try_(p): parse p, and backtrack the istream if "error failure" (but istream remains
// marked as failure)
template <typename T>
inline std::shared_ptr<parser<T>> try_(std::shared_ptr<parser<T>> p)
{
    return std::shared_ptr<parser<T>>(new parser_try<T>( std::move(p) ));
}
