//
// Generic arithmetic coding. Used both for recoded encoding/decoding
//
// Some notes on the data representations used by the encoder and decoder.
// Uncompressed data:
//   Symbols: b_1 ... b_n \in {0,1} .
//   Probabilities: p_1 ... p_n \in [0,1], where p_i estimates the probability that b_i=1.
// Compressed data:
//   Arithmetic coding represents a compressed stream of symbols as an
//   arbitrary-precision number C \in [0,1] .
//   If the compressed digits in base M are c_k \in {0..M-1}, then
//   C = \sum_{k=1}^K c_k M^{-k} .
// Arithmetic coding uses the probabilities p_i to link the symbols b_i with
// the compressed digits c_k:
//   C_i = (1-p_i) b_i + p_i C_{i+1} (1-b_i)
//   C_i \in [0,1]
//   C_1 = C = \sum_{k=1}^K c_k M^{-k}
//   C_n is an arbitrary value in [0,1] (normally used to encode a stop bit).
//

#pragma once

#include <cassert>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>


template <typename FixedPoint = uint64_t, typename CompressedDigit = uint16_t, int MinRange = 0>
struct arithmetic_code {
 private:
  static_assert(std::numeric_limits<FixedPoint>::is_exact, "integer types only");
  static_assert(!std::numeric_limits<FixedPoint>::is_signed, "unsigned integer types only");

  template <typename T>
  static constexpr bool is_power_of_2(T x) {
    static_assert(std::numeric_limits<T>::is_exact, "expected integer type");
    return (x != 0) && (x & (x-1)) == 0;
  }
  template <typename Digit>
  static constexpr FixedPoint digit_base_for() {
    static_assert(std::numeric_limits<Digit>::is_exact, "integer types only");
    static_assert(!std::numeric_limits<Digit>::is_signed, "unsigned integer types only");
    static_assert(sizeof(FixedPoint) > sizeof(Digit), "digit must be smaller than fixed point");
    static_assert(sizeof(FixedPoint) % sizeof(Digit) == 0, "digit must divide fixed point evenly");
    static_assert(is_power_of_2(FixedPoint(std::numeric_limits<Digit>::max()) + 1), "expected power of 2");
    return FixedPoint(std::numeric_limits<Digit>::max()) + 1;
  }

 public:
  // The representation of 1.0 in fixed-point, e.g. 0x80000000 for uint32_t.
  static constexpr FixedPoint fixed_one =
    std::numeric_limits<FixedPoint>::max()/2 + 1;
  // The base for compressed digit outputs, e.g. 0x10000 for uint16_t.
  static constexpr FixedPoint digit_base = digit_base_for<CompressedDigit>();
  // The minimum precision for probability estimates
  // There is a space-time tradeoff: less precision
  // means poorer compression, but more precision causes overflow digits more often.
  static constexpr FixedPoint min_range =
    MinRange > 0 ? MinRange : (fixed_one/digit_base) / 16;
  // The maximum range to reach when normalizing.
  static constexpr FixedPoint max_range = fixed_one;

  static_assert(is_power_of_2(fixed_one), "expected power of 2");
  static_assert(is_power_of_2(min_range), "expected power of 2");
  static_assert((fixed_one/digit_base)*digit_base == fixed_one,
      "expected digit_base to divide fixed_one");
  static_assert(min_range > 1, "min_range too small");
  static_assert(min_range < fixed_one/digit_base, "min_range too large");

  // The encoder object takes an output iterator (e.g. to vector or ostream) to
  // emit compressed digits.
  // In addition to uncompressed data and compressed digits, the intermediate state is:
  //   Maximum R (any positive number, typically 2^k)
  //   Lower and upper bounds x,y \in [0,R)
  //   Range r = y-x \in [0,R)
  // Representation invariant:
  //   C = \sum_{k=1}^{K_i} c_k M^{-k} + (x_i + r_i C_i) M^{-K_i}/R_i
  //   Base case: K_1 = 0, x_1 = 0, r_1 = R_1
  // In the base case i=1, K_1=0: C=C_1 is represented as a series of future decisions b_i.
  // In the final case i=n, K_n=K: C is represented as a string of compressed digits.
  // The various encoding methods modify K, x, r, R while keeping C fixed.
  template <typename OutputIterator,
            typename OutputDigit = typename std::iterator_traits<OutputIterator>::value_type>
  class encoder {
    static_assert(std::numeric_limits<OutputDigit>::is_exact,
        "integer types only");
    static_assert(!std::numeric_limits<OutputDigit>::is_signed,
        "unsigned integer types only");
    static_assert(sizeof(CompressedDigit) % sizeof(OutputDigit) == 0,
        "size of compressed digit must be a multiple of size of output digit");

   public:
    explicit encoder(OutputIterator out)
      : encoder(out, fixed_one) {}
    encoder(OutputIterator out, FixedPoint initial_range)
      : bytes_emitted(0), out(out), low(0), range(initial_range) {}
    ~encoder() {}
     size_t get_bytes_emitted()const {
        return bytes_emitted;
     }
    // Symbol is int instead of bool because additional versions of `put()` could
    // accept more than two symbols, e.g. one could call `put(2, p1, p2, p3)`.
    size_t put(int symbol, std::function<FixedPoint(FixedPoint)> probability_of_1) {
      FixedPoint range_of_1 = probability_of_1(range);
      FixedPoint range_of_0 = range - range_of_1;
      if (symbol != 0) {
        low += range_of_0;
        range = range_of_1;
      } else {
        range = range_of_0;
      }
      if (range < min_range) {
        if (range == 0) {
            dev_assert(false && "Encoder error: emitted a zero-probability symbol.");
            abort();
        }
        size_t emitted_before = get_bytes_emitted();
        while (range < max_range/digit_base) {
          renormalize_and_emit_digit<CompressedDigit>();
        }
        return get_bytes_emitted() - emitted_before;
      }
      return 0;
    }

    void finish() {
      // Find largest stop bit 2^k < range, and x such that 2^k divides x,
      // 2^{k+1} doesn't divide x, and x is in [low, low+range).
      for (FixedPoint stop_bit = (fixed_one >> 1); stop_bit > 0; stop_bit >>= 1) {
        FixedPoint x = (low | stop_bit) & ~(stop_bit - 1);
        if (stop_bit < range && low <= x && x < low + range) {
          low = x;
          break;
        }
      }

      while (low != 0) {
        range = 1;
        renormalize_and_emit_digit<OutputDigit>();
      }
      range = 0;  // mark complete
    }

   private:
    template <typename Digit>
    void renormalize_and_emit_digit() {
      static constexpr FixedPoint base = digit_base_for<Digit>();
      static constexpr FixedPoint most_significant_digit = fixed_one / base;
      static_assert(is_power_of_2(most_significant_digit), "expected power of 2");

      // Check for a carry bit, and cascade from lowest overflow digit to highest.
      if (low >= fixed_one) {
        for (int i = overflow.size()-1; i >= 0; i--) {
          if (++overflow[i] != 0) break;
        }
        low -= fixed_one;
      }
      dev_assert(low < fixed_one);

      // Compare the minimum and maximum possible values of the top digit.
      // If different, defer emitting the digit until we're sure we won't have to carry.
      Digit digit = Digit(low / most_significant_digit);
      if (digit != Digit((low + range - 1) / most_significant_digit)) {
        dev_assert(range < most_significant_digit);
        overflow.push_back(digit);
      } else {
        for (CompressedDigit overflow_digit : overflow) {
          emit_digit(overflow_digit);
        }
        overflow.clear();
        emit_digit(digit);
      }

      // Subtract away the emitted/overflowed digit and renormalize.
      low -= digit * most_significant_digit;
      low *= base;
      range *= base;
    }

    // Emit a CompressedDigit as one or more OutputDigits. Loop should be
    // unrolled by the compiler.
    template <typename Digit>
    void emit_digit(Digit digit) {
      for (int i = sizeof(Digit)-sizeof(OutputDigit); i >= 0; i -= sizeof(OutputDigit)) {
        *out++ = OutputDigit(digit >> (8*i));
      }
      bytes_emitted += sizeof(digit);
    }
    size_t bytes_emitted;
    // Output digits are emitted to this iterator as they are produced.
    OutputIterator out;
    // The lower bound x, initialized to 0. (When overflow.size() > 0, low is
    // the fractional digits of x/R_0.)
    FixedPoint low;
    // The range r, which starts as fixed-point 1.0.
    FixedPoint range;
    // High digits of x. If overflow.size() = s, then R = R_0 M^s (where R_0 = fixed_one).
    std::vector<CompressedDigit> overflow;
  };

  // The decoder object takes an input iterator (e.g. from vector or istream)
  // to read compressed digits.
  // In addition to uncompressed data and compressed digits, the intermediate state is:
  //   TODO(ctl) document the state, representation invariant, and decoding transitions.
  template <typename InputIterator,
            typename InputDigit = typename std::iterator_traits<InputIterator>::value_type>
  class decoder {
    static_assert(std::numeric_limits<InputDigit>::is_exact,
        "integer types only");
    static_assert(!std::numeric_limits<InputDigit>::is_signed,
        "unsigned integer types only");
    static_assert(sizeof(CompressedDigit) % sizeof(InputDigit) == 0,
        "size of compressed digit must be a multiple of size of input digit");

   public:
    explicit decoder(InputIterator in, InputIterator end = InputIterator())
      : decoder(in, end, fixed_one) {}
    decoder(InputIterator in, InputIterator end, FixedPoint initial_range)
      : in(in), end(end) {
      // Initialize the decoder state by reading in bits until range ~ initial_range.
      next_digit = consume_digit_aligned();
      low = next_digit / digit_alignment;
      range = digit_base / digit_alignment;
      while (range < initial_range) {
        renormalize_and_consume_digit();
      }
      dev_assert(range == initial_range);  // Should be true if we set digit_alignment correctly.
    }

    int get(std::function<FixedPoint(FixedPoint)> probability_of_1) {
      FixedPoint range_of_1 = probability_of_1(range);
      FixedPoint range_of_0 = range - range_of_1;
      int symbol = (low >= range_of_0);
      if (symbol != 0) {
        low -= range_of_0;
        range = range_of_1;
      } else {
        range = range_of_0;
      }
      if (range < min_range) {
        while (range < max_range/digit_base) {
          renormalize_and_consume_digit();
        }
      }
      return symbol;
    }

   private:
    static constexpr CompressedDigit digit_alignment =
      std::numeric_limits<FixedPoint>::max()/fixed_one + 1;
    static_assert(is_power_of_2(digit_alignment), "");
    static_assert((fixed_one/digit_base)*digit_alignment == (std::numeric_limits<FixedPoint>::max()/digit_base) + 1,
        "expected fixed_one > max/digit_base");
    static_assert(is_power_of_2(digit_base/digit_alignment),
        "expected digit_base > digit_alignment");

    void renormalize_and_consume_digit() {
      dev_assert(low < fixed_one/digit_base);

      CompressedDigit digit = consume_digit();
      low = low * digit_base + digit;
      range *= digit_base;
    }

    // Consume a CompressedDigit. Because our initialization is not
    // digit-aligned, we have to bit-align the reads here.
    CompressedDigit consume_digit() {
      CompressedDigit in_digit = consume_digit_aligned();
      CompressedDigit digit = ((next_digit * (digit_base/digit_alignment)) |
                               (in_digit / digit_alignment));
      next_digit = in_digit;
      return digit;
    }

    // Consume a CompressedDigit as one or more InputDigits. Loop should be
    // unrolled by the compiler.
    CompressedDigit consume_digit_aligned() {
      CompressedDigit digit = 0;
      for (int i = sizeof(CompressedDigit)-sizeof(InputDigit); i >= 0; i -= sizeof(InputDigit)) {
        digit *= digit_base_for<InputDigit>();
        if (in != end) {
          digit |= CompressedDigit(InputDigit(*in++));
        }
      }
      return digit;
    }

    // Input digits are read from this iterator.
    InputIterator in, end;
    // The last digit read from the input - the lower bits are still to be used.
    CompressedDigit next_digit;
    // The offset z from the lower bound.
    FixedPoint low;
    // The range r, which is initialized to fixed-point 1.0.
    FixedPoint range;
  };
};


template <typename Coder = arithmetic_code<>,
          typename OutputContainer>
typename Coder::template encoder<std::back_insert_iterator<OutputContainer>,
                                 typename OutputContainer::value_type>
make_encoder(OutputContainer* container) {
  auto it = std::back_inserter(*container);
  typedef typename OutputContainer::value_type OutputDigit;
  return typename Coder::template encoder<decltype(it), OutputDigit>(it);
}

template <typename Coder = arithmetic_code<>,
          typename InputContainer>
typename Coder::template decoder<typename InputContainer::const_iterator,
                                 typename InputContainer::value_type>
make_decoder(const InputContainer& container) {
  auto begin = std::begin(container), end = std::end(container);
  typedef typename InputContainer::value_type InputDigit;
  return typename Coder::template decoder<decltype(begin), InputDigit>(begin, end);
}
