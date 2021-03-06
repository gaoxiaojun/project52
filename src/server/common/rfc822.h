#ifndef P52_SERVER_COMMON_RFC822_H_
#define P52_SERVER_COMMON_RFC822_H_
//#define BOOST_SPIRIT_DEBUG 1

// #define BOOST_SPIRIT_THREADSAFE
// #define PHOENIX_THREADSAFE
#include <boost/spirit/include/classic.hpp>
#include <boost/spirit/include/support_istream_iterator.hpp>

#include <common/rfc822/rfc822.h>
#include <common/rfc822/rfc2822_grammar.h>
#include <common/rfc822/rfc2822_hooks.h>
#include <common/rfc822/unfolding.h>
#include <iostream>
#include <fstream>
#include <boost/algorithm/string.hpp>
#include <boost/range.hpp>

namespace rfc822 {
using namespace boost::spirit;
using namespace p52::rfc822;
using namespace rfc822;

static const std::locale loc = std::locale();

template<typename IteratorT>
struct test_actions: public p52::rfc822::rfc2822::null_actions<IteratorT> {
    typedef IteratorT iterator_t;
    typedef boost::iterator_range<iterator_t> data_range_t;
    typedef address_list_field_value<iterator_t> address_list;
    typedef mime_content_type_field_value<iterator_t> mime_type;
    typedef mime_with_params_field_value<iterator_t> mime_value;

    template <typename T>
    auto value( field_data<iterator_t> const& fd ) const ->
            decltype(boost::dynamic_pointer_cast<T>(fd.value)) {
        return boost::dynamic_pointer_cast<T>(fd.value);
    }

    void on_field_data(field_data<iterator_t> const& /*fd*/) const 
    {
#if 0
        if( auto al = value<address_list>(fd) ) {
            for (auto const& at : al->addrs) {
                std::cout << fd.name <<": \"" << at.name << "\" <"
                        << at.local << "@" << at.domain << ">\n";
            }
            return;
        }
#endif

#if 0
        if( auto mp = value<mime_type>(fd) ) {
            std::cout << "mime content type: " << mp->content_type.type
                    << "/" << mp->content_type.subtype << "\n";
            for (auto const& par : mp->params) {
                std::cout << "\t" << par.attr << " = " << par.value << "\n";
            }
            return;
        }
#endif

#if 0
        if( auto mp = value<mime_value>(fd) ) {
            std::cout << "mime value: " << mp->value << "\n";
            for (auto const& par : mp->params) {
                std::cout << "\t" << par.attr << " = " << par.value
                        << "\n";
            }
            return;
        }
#endif

#if 0
        std::cout << fd.name << ": "
                << std::string(fd.value->raw.begin(), fd.value->raw.end())
                << std::endl;
#endif

#if 0
				if (boost::algorithm::iequals (fd.name, "date"))
        {
					std::cout << fd.name << ": " << make_unfolding_range (fd.value->raw)
					<< "\n";
					return;
        }
#endif
    }

    void on_body_prefix(data_range_t const&) const {}

    void on_body(data_range_t const&) const {}
};

template <typename ForwardIterator>
inline bool parse(ForwardIterator first, ForwardIterator const& last) {
    test_actions<ForwardIterator> actions;
    rfc2822::grammar<test_actions<ForwardIterator> > g(actions);

    return boost::spirit::classic::parse(first, last, g).full;
}

inline bool parse(std::istream & is) {
    namespace spirit = boost::spirit;
#if 1

    // is.unsetf (std::ios::skipws);
    spirit::istream_iterator first (is);
    spirit::istream_iterator last;

    return parse (first, last);
#else

    typedef std::istream::char_type char_t;
    typedef classic::multi_pass<std::istreambuf_iterator<char_t> > multi_pass_iterator_t;

    multi_pass_iterator_t in_begin(
            classic::make_multi_pass(std::istreambuf_iterator<char_t>(is)));
    multi_pass_iterator_t in_end(
            classic::make_multi_pass(std::istreambuf_iterator<char_t>()));

    test_actions<multi_pass_iterator_t> actions;
    rfc2822::grammar<test_actions<multi_pass_iterator_t> > g(actions);

    return boost::spirit::classic::parse(in_begin, in_end, g).full;
#endif
}

} //namespace rfc822
#endif /* P52_SERVER_COMMON_RFC822_H_ */
