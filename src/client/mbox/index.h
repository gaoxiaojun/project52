#ifndef _P52_MBOX_INDEX_H_
#define _P52_MBOX_INDEX_H_
#include <mbox/mmap.h>
#include <mbox/fix.h>
#include <utility>
#include <tuple>

#include <map>
#include <iostream>
#include <boost/asio/buffer.hpp>

#include "thread_pool.h"

namespace p52 {
namespace mbox {

namespace asio = boost::asio;

template <typename Mmap = mbox::mmap>
class index 
{
public:
  typedef std::vector<asio::const_buffer> buf_seq_type;
  typedef std::multimap<std::size_t, buf_seq_type> index_type;

public:
  index () = default;
  index (char const* file) { open (file); }

  void open (char const* file) 
  { 
    mmap_.open (file); 

    typename Mmap::const_iterator pos = mmap_.begin ();
    typename Mmap::const_iterator end = mmap_.end ();

    std::size_t count = 0;
    std::size_t pcnt = 0;


    thread_pool tp (100, 24);

    while (pos < end)
    {
      ++count;
      std::size_t size;
      char const* addr;
      
      std::tie (size, addr) = parse (pos, end);

      std::size_t p = static_cast<std::size_t> (100.0*(pos-mmap_.begin ())/mmap_.size ());
      if (p != pcnt)
      {
        std::cout << "Parsed: " << count << " msgs, " << p << "%\r" << std::flush;
        pcnt = p;
      }

      auto inserted_iter = index_.emplace (size, buf_seq_type ());
      buf_seq_type* bufs = &(inserted_iter->second);

      tp.post (
        [bufs,addr,size,inserted_iter] (std::size_t const&)
        {
          fix_message_parser fixer;
          fixer.parse (*bufs, addr, addr+size);
          fixer.parse_end (*bufs);
	        assert (asio::buffers_end (*bufs) - asio::buffers_begin (*bufs) > 0);
	        assert (inserted_iter->first > 0);
        }
      );
    }

    tp.close ();

    for (auto b: index_)
    {
	    buf_seq_type& bufs = b.second;
      assert (asio::buffers_end (bufs) - asio::buffers_begin (bufs) > 0);
      int i = b.first;
    }

    std::cout << "\n";
  }

  std::pair<std::size_t, char const*>
  parse (typename Mmap::const_iterator& first, typename Mmap::const_iterator const& last)
  {
    std::size_t size = 0;
    char const* addr = 0;

    typename Mmap::const_iterator saved = first;

    while (first != last && (first-saved < 10) && *first >= '0' && *first <= '9')
    {
      size *= 10;
      size += (*first - '0');
      ++first;
    }

    while (first != last && *first != '\n') ++first;

    addr = ++first;
    first += size;

    return std::make_pair (size, addr);
  }

  typedef typename index_type::const_iterator const_iterator;

  const_iterator begin () const { return index_.begin (); }
  const_iterator   end () const { return index_.end (); }

  const_iterator lower_bound (std::size_t const& n) const 
  { return index_.lower_bound (n); }

  const_iterator upper_bound (std::size_t const& n) const 
  { return index_.upper_bound (n); }



  template <typename X>
  const_iterator find (X&& x)
  {
    return index_.find (std::forward<X> (x));
  }

private:
  Mmap mmap_;
  index_type index_;
};

}}
#endif // _P52_MBOX_INDEX_H_
