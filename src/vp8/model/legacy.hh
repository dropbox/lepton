class Boolean
{
private:
  bool i_;

public:
  Boolean( BoolDecoder & data, const Probability probability = 128 ) : i_( data.get( probability ) ) {}
  Boolean( const bool & val = false ) : i_( val ) {}
  operator const bool & () const { return i_; }
};

using Flag = Boolean;

template <unsigned int width>
class Unsigned
{
private:
  uint32_t i_;

public:
  Unsigned( BoolDecoder & data ) : i_( (Boolean( data ) << (width-1)) | Unsigned<width-1>( data ) )
  {
    static_assert( width <= 32, "Unsigned width must be <= 32" );
  }
  Unsigned( const uint32_t val = 0 ) : i_( val ) {}

  operator uint32_t () const { return i_; }
};

template <>
inline Unsigned< 0 >::Unsigned( BoolDecoder & ) : i_() {}

template <unsigned int width>
class Signed
{
private:
  int8_t i_;

public:
  Signed( BoolDecoder & data ) : i_( Unsigned<width>( data ) * (Boolean( data ) ? -1 : 1) )
  {
    static_assert( width <= 7, "Signed width must be <= 7" );
  }
  Signed( const int8_t & val ) : i_( val ) {}

  operator const int8_t & () const { return i_; }
};

template <class T>
class Flagged : public Optional<T>
{
public:
  Flagged( BoolDecoder & data, const Probability probability = 128 )
    : Optional<T>( Flag( data, probability ), data )
  {}

  using Optional<T>::Optional;
  Flagged() = default;
};
template <class T, typename... Targs>
static void encode( BoolEncoder & encoder, const Optional<T> & obj, Targs&&... Fargs )
{
  if ( obj.initialized() ) {
      encode( encoder, obj.get(), std::forward<Targs>( Fargs )... );
  }
}

template <class T>
static void encode( BoolEncoder & encoder, const Flagged<T> & obj, const Probability probability = 128 )
{
  encoder.put( obj.initialized(), probability );

  if ( obj.initialized() ) {
    encode( encoder, obj.get() );
  }
}

template <unsigned int width>
static void encode( BoolEncoder & encoder, const Unsigned<width> & num )
{
  encoder.put( num & (1 << (width-1)) );
  encode( encoder, Unsigned<width-1>( num ) );
}

template<> void encode( BoolEncoder &, const Unsigned<0> & ) {}

template <unsigned int width>
static void encode( BoolEncoder & encoder, const Signed<width> & num )
{
  Unsigned<width> absolute_value = abs( num );
  encode( encoder, absolute_value );
  encoder.put( num < 0 );
}


template <uint64_t x, uint64_t n=32> struct static_log2 {
    enum uint64_t {
        c = ((x >> n ) > 0) ? 1 : 0
    };
    enum uint64_t {
        value = c * n + static_log2<(x >> (c * n)), n / 2>::value
    };
};
template <> struct static_log2 <1,0> {
    enum uint64_t {
        value = 0
    };
};
template <int n> struct static_ceil_log2 {
    enum uint64_t {
        value = (1 << static_log2<n>::value) < n ? static_log2<n>::value + 1 : static_log2<n>::value
    };
};

template<typename intt> intt log2(intt v) {
    constexpr int loop_max = (int)(sizeof(intt) == 1 ? 2
                                   : (sizeof(intt) == 2 ? 3
                                      : (sizeof(intt) == 4 ? 4
                                         : 5)));
    const intt b[] = {0x2,
                      0xC,
                      0xF0,
                      (intt)0xFF00,
                      (intt)0xFFFF0000U,
                      std::numeric_limits<intt>::max() - (intt)0xFFFFFFFFU};
    const intt S[] = {1, 2, 4, 8, 16, 32};

    register intt r = 0; // result of log2(v) will go here
    
    for (signed int i = loop_max; i >= 0; i--) // unroll for speed...
    {
        if (v & b[i])
        {
            v >>= S[i];
            r |= S[i];
        } 
    }
    return r;
}

template <int bits, int highest_likely_value> int skew_log(int number) {
    static_assert(static_ceil_log2<highest_likely_value>::value <= bits,
                  "The highest likely number must be less than the number of bits provided");
    if (number < highest_likely_value) {
        return number;
    }
    int offset = highest_likely_value - static_log2<highest_likely_value>::value;
    if (bits <= 8) {
        offset += log2<uint8_t>((uint8_t)number);
    } else if (bits <= 16) {
        offset += log2<uint16_t>((uint16_t)number);
    } else if (bits <= 32) {
        offset += log2<uint32_t>((uint32_t)number);
    } else {
        offset += log2<uint64_t>((uint64_t)number);
    }
    return std::min(offset, (1 << bits));
}


template<unsigned int prev_coef_contexts=PREV_COEF_CONTEXTS> int combine_priors(int16_t a, int16_t b) {
    const int max_likely_value = 6;
    int16_t al = skew_log<static_ceil_log2<prev_coef_contexts-1>::value / 2,
                          max_likely_value>(abs(a));
    int16_t bl = skew_log<static_ceil_log2<prev_coef_contexts-1>::value / 2,
                          max_likely_value>(abs(b));
    int retval = std::min(al + (1U << (static_ceil_log2<prev_coef_contexts-1>::value / 2)) * bl,
                          prev_coef_contexts-1);
    return retval;
}
