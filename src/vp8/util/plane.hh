#ifndef _PLANE_HH
#define _PLANE_HH

#include <cassert>
#include <vector>
#include <memory>
#include <functional>

#include "option.hh"

template <class T>
class Arry2d
{
private:
  uint32_t width_, height_;
  std::vector< T > storage_;

public:
  using const_iterator = typename std::vector< T >::const_iterator;

  class LocalArea {
    uint32_t x, y;
    uint32_t w, h;
  public:
    Optional< const T * > left, above_left, above, above_right;

    LocalArea(const Arry2d *thus, uint32_t in_x, uint32_t in_y, uint32_t width, uint32_t height)
     : x(in_x), y(in_y),
       w(width), h(height) {
        if (in_x > 0) {
            left.initialize(&thus->at(in_x - 1, in_y));
        }
        if (in_x > 0 && in_y > 0) {
            above_left.initialize(&thus->at(in_x - 1, in_y - 1));
        }
        if (in_y > 0) {
            above.initialize(&thus->at(in_x, in_y - 1));
        }
        if (in_x + 1 < thus->width() && in_y > 0) {
            above_right.initialize(&thus->at(in_x + 1, in_y - 1));
        }
    }

    // Seems like when copying a Plane, copying the potentially
    // incorrect pointers is never the right thing to do
    const LocalArea & operator=(const LocalArea &)=delete;
  };

  template< typename Targs>
  Arry2d(const uint32_t width, const uint32_t height, const Targs &default_)
    : width_(width), height_(height), storage_()
  {
    assert(width > 0);
    assert(height > 0);

    storage_.reserve(width * height);

    for (uint32_t row = 0; row < height; row++) {
      for (uint32_t column = 0; column < width; column++) {
        const LocalArea c(this, column, row, width, height);
        storage_.emplace_back(c, default_);
      }
    }
  }

  T & at(const uint32_t column, const uint32_t row)
  {
    assert(column < width_ and row < height_);
    return storage_[ row * width_ + column ];
  }

  const T & at(const uint32_t column, const uint32_t row) const
  {
    assert(column < width_ and row < height_);
    return storage_[ row * width_ + column ];
  }

  uint32_t width() const { return width_; }
  uint32_t height() const { return height_; }

  const_iterator begin() const
  {
    return storage_.begin();
  }

  const_iterator end() const
  {
    return storage_.end();
  }

  Arry2d(const Arry2d & other) = delete;
  Arry2d & operator=(const Arry2d & other) = delete;

  Arry2d(Arry2d && other) = delete;
  Arry2d & operator=(Arry2d && other) = delete;
};

template <class T>
class Plane
{
private:
  std::shared_ptr<Arry2d<T>> storage_;

public:
  typedef typename Arry2d<T>::LocalArea LocalArea;

  template <typename... Targs>
  Plane(Targs&&... Fargs)
    : storage_(std::make_shared<Arry2d<T>, Targs...>(std::forward<Targs>(Fargs)...))
  {}

  T & at(const uint32_t column, const uint32_t row) { return storage_->at(column, row); }
  const T & at(const uint32_t column, const uint32_t row) const { return storage_->at(column, row); }

  uint32_t width() const { return storage_->width(); }
  uint32_t height() const { return storage_->height(); }

  typename Arry2d<T>::const_iterator begin() const
  {
    return storage_->begin();
  }

  typename Arry2d<T>::const_iterator end() const
  {
    return storage_->end();
  }

  bool operator==(const Plane<T> & other) const { return *storage_ == *(other.storage_); }

  Plane(const Plane & other) = delete;
  Plane & operator=(const Plane & other) = delete;

  Plane(Plane && other) noexcept : storage_(std::move(other.storage_)) {}
};

#endif
