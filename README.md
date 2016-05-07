# Parser-Combinator
A Parser Combinator in C++

- A parser is a functor(mapping) of an istream to a parse tree; it is working on istream rather than the lower-level streambuf for directly referencing and manipulating the failbit and the eofbit.  
- Like Parsec in Haskell, there are two kinds of failures:  
  - "weak failure" or "failure but consume nothing", if a parser is failed at the very first character, the parser does not throw an exception but returns a default value (like nullptr) instead, with making the istream failed and not consuming the first character.  
  - "error failure", if a parser is failed at the second or later character, the parser throws an exception whether or not the first character was matched and consumed.  
  - this differentiation helps to write a (sub-)parser for LL(1) without try_().  

- references:  
  - "An Introduction to the Parsec Library",  
    https://kunigami.wordpress.com/2014/01/21/an-introduction-to-the-parsec-library/  
  - "jneen/parsimmon", https://github.com/jneen/parsimmon  
  - "keean/Parser-Combinators", https://github.com/keean/Parser-Combinators  
  - "Stupid Template Tricks - Pride and Parser Combinators",  
    http://blog.mattbierner.com/stupid-template-tricks-pride-and-parser-combinators-part-one/  
  - C++ containers member function table  
    http://en.cppreference.com/w/cpp/container  
  - "Packat Parsing: Simple, Powerful, Lazy, Linear Time",  
    http://pdos.csail.mit.edu/~baford/packrat/icfp02/  
  - "Getting Lazy with C++", http://bartoszmilewski.com/2014/04/21/getting-lazy-with-c/  
  - "C++11: STD::FUNCTION AND STD::BIND",  
    http://oopscenities.net/2012/02/24/c11-stdfunction-and-stdbind/  
  - "Higher-Order Functions in C++",  
    https://forfunand.wordpress.com/2012/04/09/higher-order-functions-in-c/  
  - "How to get the line number from a file in C++?",  
    http://stackoverflow.com/questions/4813129/how-to-get-the-line-number-from-a-file-in-c  
  - "substream from istream",  
    http://stackoverflow.com/questions/7623277/substream-from-istream  
  - "Copy, load, redirect and tee using C++ streambufs",  
    http://wordaligned.org/articles/cpp-streambufs  
  - "A beginner's guide to writing a custom stream buffer (std::streambuf)",  
    http://www.mr-edd.co.uk/blog/beginners_guide_streambuf  
  - "Tips and tricks for using C++ I/O (input/output)",  
    http://www.augustcouncil.com/~tgibson/tutorial/iotips.html  

- character parsers:  
  - `chr('c')`  
  - `any_chr()`       - /./  
  - `one_of("abc")`   - /[abc]/  
  - `none_of("abc")`  - /[^abc]/  
  - `blank()`         - space or tab  
  - `letter()`  
  - `alphanum()`  
  - `digit()`  

- void parsers:  
  - `eof()`  
  - `skip(p)`         - generic void parser  
  - `skip('c')`  
  - `skip("abc")`  
  - `blanks()`        - optionally consume blanks  

- string parsers:  
  - `+p`              - convert a character parser into a string parser  
  - `p + q`           - concatenate string parsers  

- parser combinators:  
  - `p >> f`          - parse p, and if p succeeds apply T f(U) to the result U from p; p can be a void parser (and f is of type T f())
  - `p >> f`	        - for custom parsering function f of type T f(istream &, U) or T f(istream &)  
  - `many<C>(p)`	    - parse /p*/ and return the collection of results from p's in a container of type C  
  - `many(p)`	        - string parser when p is a character parser  
  - `many(p)`         - void parser when p is a void parser  
  - `many1(p, f)`	    - parse /p+/ and return the collection of results from p's using T f(T, T) such as foldl1() in Haskell  
  - `many1(p)`	      - void parser when p is a void parser  
  - `p > q`	          - return result from q if p and q are parsed successfully; p can be a void parser; return result from p if q is a void parser  
  - `sep_by<C>(p, q)` - parse /(p (q p)*)?/ and return the collection of results from p's in a container of type C  
  - `sep_by(p, q)`	  - string parser when p is a character parser  
  - `sep_by(p, q)`	  - void parser when p is a void parser  
  - `sep_by1(p, q, f)` - parse /p (q p)*/ and return the collection of results from p's using T f(T, T)  
  - `sep_by1(p, q)`   - void parser when p is a void parser  
  - `p | q`	          - parse p first, and if p fails and consumes nothing parse q  
  - `try_(p)`	        - parse p, and backtrack the istream if "error failure" (but istream remains marked as failure)  
