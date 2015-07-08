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
