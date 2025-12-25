# Utilities and eXtensionS (UXS) for Standard C++ Library

This is a collection of useful (template) classes and functions developed upon standard C++ library. It can be compiled
using *C++11*, but *C++17* or *C++20* are preferable. This README file briefly describes contents of this library and
can be not up-do-date, it also gives guidelines on how to use this stuff. For more detailed information see
[wiki](https://github.com/gbuzykin/uxs/wiki) pages.

The collection includes the following functionality:

- implementation of *C++20*-like string *formatting* and *printing* library (*C++20* is needed for compile-time
  formatting string checking); it is very close to *C++20* standard implementation (all standard format specifiers are
  implemented), but faster and has some extensions
- full string *formatting* implementation for *ranges*
- string *formatting* implementation for `std::filesystem::path` (*C++17* is needed)
- string *formatting* implementation for *chrono* classes (*C++20* is needed)
- library for string *parsing*
- library for *buffered input/output* (alternative to rather slow and consuming standard streams), which is compliant
  with the *printing* functions; special support of ANSI color escape sequences
- binary serializing and deserializing operator implementation for basic types
- powerful and universal `uxs::db::value` data structure to store hierarchical records and arrays (*JSON DOM*)
- fast full-featured *JSON* reader (SAX-like & DOM) and writer for *buffered input/output* (`uxs::db::value` text
  serializer & deserializer), which can handle large, *z-deflated* or *zip-compressed* files
- binary `uxs::db::value` serializer & deserializer (*C++17* is needed)
- limited (no DTD and XSL support) *XML* SAX parser
- JSON DOM `uxs::db::value` reader and writer for *XML* (*JSON* to *XML* converter)
- Z-inflation and Z-deflation support for *buffered input/output* (*zlib* integration)
- special *buffered input/output* derived classes to read and write files inside *zip* archives (*libzip* integration)

Additional functionality:

- some useful utility functions for *string* manipulation, such as *splitting*, *joining*, and *searching*
- functions for converting *UTF-8*, *UTF-16*, *UTF-32* to each other
- pretty command line interface *(CLI)* implementation
- dynamic *variant* object implementation `uxs::variant`, which can hold data of various types known at runtime and
  implicitly convert one to another (not a template with predefined set of types); it easily integrates with mentioned
  string parsers and formatters for to- and from- string conversion
- insert, erase and find algorithm implementations for *bidirectional lists* and *red-black trees* (as functions to make
  it possible to implement either universal containers or intrusive data structures)
- intrusive bidirectional list implementation
- Python-like *zip iterator* `uxs::zip_iterator<>` implementation
- *COW* pointer `uxs::cow_ptr<>` implementation
- *CRC32* calculator
- *UUID* generator

Byproduct features:

- *iterator* helper classes and functions
- *algorithm* and *functional* extensions
- *C++20*-like `est::span<>` template type for *С++11*
- *C++17*-like `est::optional<>` template type for *С++11*
- standard `std::basic_string_view<>` implementation for *C++11*
- implementation of base utility templates such as `std::enable_if_t<>`, `std::index_sequence<>`, and other needed but
  missing stuff for *С++11*

This [project](https://github.com/gbuzykin/uxs-tests) can be referenced as `uxs` usage example. It is a separate project
for testing `uxs` and contains lots of use- and test-cases. A simple test-suite is implemented to sort and organize
tests; it involves verification tests with predefined input data, bruteforce-like tests with large amount of
random-generated data, and performance tests.

## How to Build the UXS library

Perform these steps to build the project:

1. Clone `uxs` repository and enter it

    ```bash
    git clone https://github.com/gbuzykin/uxs.git
    cd uxs
    ```

2. Create compilation script using `cmake` tool

    ```bash
    cmake --preset default
    ```
   
    Default C & C++ compilers will be used.

3. Build library

    ```bash
    cmake --build build --config Release
    ```

    To run several parallel processes (e.g. 8) for building use `-j` key

    ```bash
    cmake --build build --config Release -j 8
    ```

4. Install library

    ```bash
    cmake --install build --config Release --prefix <install-dir>
    ```
