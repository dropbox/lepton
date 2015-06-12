#ifndef _OPTION_HH
#define _OPTION_HH

#include <cassert>

template <class T>
class Optional
{
private:
  union { T content_; unsigned char placeholder_; };
  bool initted_; // this should be the last member since it has no alignment restriction

public:
  Optional() : placeholder_(), initted_(false) {}

  Optional(const T & other) : content_(other), initted_(true) {}

  Optional(T && other) : content_(std::move(other)), initted_(true) {}

  Optional(Optional<T> && other)
    : initted_(other.initted_)
  {
    if (initted_) {
      new(&content_) T(std::move(other.content_));
    }
  }

  Optional(const Optional<T> & other)
    : initted_(other.initted_)
  {
    if (initted_) {
      new(&content_) T(other.content_);
    }
  }

  const Optional & operator= (Optional<T> && other)
  {
    if (initted_) {
      content_.~T();
    }

    initted_ = other.initted_;
    if (initted_) {
      new(&content_) T(std::move(other.content_));
    }
    return *this;
  }

  const Optional & operator=(const Optional<T> & other)
  {
    if (initted_) {
      content_.~T();
    }

    initted_ = other.initted_;
    if (initted_) {
      new(&content_) T(other.content_);
    }
    return *this;
  }

  bool initialized() const {
    return initted_;
  }
  template <class U> void initialize(U&&arg) {
    if (initted_) {
      content_.~T();
    }
    initted_ = true;
    new(&content_) T(std::move(arg));
  }
  void clear() { if (initialized()) { content_.~T(); } initted_ = false; }

  const T & get() const {
    assert(initialized());
    return content_;
  }
  T & get() {
    assert(initialized());
    return content_;
  }
  const T & get_or(const T & default_value) const {
    if (initted_) {
      return content_;
    }
    return default_value;
  }

  ~Optional() {
    if (initialized()) {
      content_.~T();
    }
  }
};

#endif
