#include "bool_decoder.hh"
#include "fixed_array.hh"
#include "model.hh"
#include "block.hh"
#include "context.hh"

using namespace std;


template <unsigned int length, uint16_t base_value_, uint16_t prob_offset_> template <class ProbabilityFunctor>
uint16_t TokenDecoder<length, base_value_, prob_offset_>::decode( BoolDecoder & data, const ProbabilityFunctor& probAt)
{
  uint16_t increment = 0;
  for ( uint8_t i = 0; i < length; i++ ) {
      increment = ( increment << 1 ) + data.get( probAt( i + prob_offset_, base_value, upper_limit ) );
  }
  return base_value_ + increment;
}



template <class ParentContext> struct PerBitDecoderState : public ParentContext{
    BoolDecoder * decoder_;

    template<typename... Targs> PerBitDecoderState(BoolDecoder *decoder,
                                                   Targs&&... Fargs) :
        ParentContext(std::forward<Targs>(Fargs)...),
        decoder_(decoder) {
    }
    bool decode_one(TokenNode index) {
        return decode_one((int)index);
    }
    bool decode_one(int index) {
        return decoder_->get((*this)(index,
                                     min_from_entropy_node_index(index),
                                     max_from_entropy_node_index_inclusive(index)));
    }
};


PerBitContext2u::PerBitContext2u(NestedProbabilityArray  *prob,
                                 Optional<uint16_t> left_coded_length,
                                 Optional<uint16_t> above_coded_length)
   : left_value_(left_coded_length),
     above_value_(above_coded_length) {
    put_one_unsigned_coefficient(left_bits_, left_coded_length.get_or(0));
    put_one_unsigned_coefficient(above_bits_, above_coded_length.get_or(0));
    
    probability_ = prob;
}

PerBitContext4s::PerBitContext4s(NestedProbabilityArray  *prob,
                                 Optional<int16_t> left_block_value,
                                 Optional<int16_t> above_block_value,
                                 Optional<int16_t> left_coef_value,
                                 Optional<int16_t> above_coef_value)
    : left_block_value_(left_block_value),
     above_block_value_(above_block_value),
     left_coef_value_(left_coef_value),
     above_coef_value_(above_coef_value) {
    put_one_signed_coefficient(left_block_bits_, false, false, left_block_value.get_or(0));
    put_one_signed_coefficient(above_block_bits_, false, false, above_block_value.get_or(0));
    put_one_signed_coefficient(left_coef_bits_, false, false, left_coef_value.get_or(0));
    put_one_signed_coefficient(above_coef_bits_, false, false, above_coef_value.get_or(0));
    
        probability_ = prob;
}

typedef PerBitDecoderState<DefaultContext> DecoderState;
typedef PerBitDecoderState<PerBitContext2u> DecoderState2u;
typedef PerBitDecoderState<PerBitContext4s> DecoderState4s;

template<class DecoderT> uint8_t get_ceil_log2_coefficient(DecoderT& d) {
    bool l0 = d.decode_one(TokenNode::LENGTH0);
    bool l1 = d.decode_one(TokenNode::LENGTH1);
    if (l0 == false && l1 == false) {
        return 1;
    }
    bool l2 = d.decode_one(TokenNode::LENGTH2);
    if (l0 == true && l1 == false && l2 == false) {
        return 2;
    }
    bool l3 = d.decode_one(TokenNode::LENGTH3);
    uint8_t prefix_coded_length = (l0 ? 1 : 0) + (l1 ? 2 : 0) + (l2 ? 4 : 0) + (l3 ? 8 : 0);
    return 3 + (l2 ? 1 : 0) + (l3 ? 2 : 0) + (l0? 4 : 0);
}
template<class DecoderT> uint16_t get_one_natural_coefficient(DecoderT& d) {
    uint8_t length = get_ceil_log2_coefficient(d);
    uint16_t value = (1 << (length - 1));
    for (uint8_t i = 0; i + 1 < length; ++i) {
        value += (1 << i) * d.decode_one((int)TokenNode::VAL0 + i);
    }
    return value;
}

/* The unfolded token decoder is not pretty, but it is considerably faster
   than using a tree decoder */
template<class DecoderT>int16_t get_one_signed_nonzero_coefficient( DecoderT &d )
{
    
    int16_t value = get_one_natural_coefficient(d);
    bool invert_sign = d.decode_one(TokenNode::NEGATIVE);
    return (invert_sign ? -value : value);
}

/* The unfolded token decoder is not pretty, but it is considerably faster
   than using a tree decoder */

template<class DecoderT> pair<bool, int16_t> get_one_signed_coefficient( DecoderT &d,
                                                                         const bool last_was_zero )
{
    
    // decode the token
    if ( not last_was_zero ) {
        if ( d.decode_one(TokenNode::EOB)) {
          // EOB
          return { true, 0 };
      }
    }

    if ( d.decode_one(TokenNode::ZERO)) {
      return { false, 0 };
    }
    int16_t value = get_one_natural_coefficient(d);
    bool invert_sign = d.decode_one(TokenNode::NEGATIVE);
    return {false, (invert_sign ? -value : value)};
}
template<class DecoderT> uint16_t get_one_unsigned_coefficient( DecoderT & d) {

    if ( d.decode_one(TokenNode::ZERO)) {
      return 0;
    }
    return get_one_natural_coefficient(d);
}

void Block::parse_tokens( BoolDecoder & data,
                          ProbabilityTables & probability_tables )
{
  /* read which EOB bin we're in */
  uint8_t above_num_zeros = context().above.initialized() ? context().above.get()->num_zeros() + 1 : 0;
  uint8_t left_num_zeros = context().left.initialized() ? context().left.get()->num_zeros() + 1 : 0;

  uint16_t above_coded_length = 65;
  uint16_t left_coded_length = 65;

  if (context().left.initialized()) {
      left_coded_length = context().left.get()->coded_length();
      assert( left_coded_length < AVG_EOB );
  }
  if (context().above.initialized()) {
      above_coded_length = context().above.get()->coded_length();
      assert( above_coded_length < AVG_EOB );
  }
#ifdef DEBUGDECODE
  fprintf(stderr, "XXY %d %d %d %d\n", left_num_zeros,
          above_num_zeros,
          left_coded_length,
          above_coded_length);
#endif
  auto & num_zeros_prob
      = probability_tables.num_zeros_array((int)left_num_zeros,
                                           (int)above_num_zeros,
                                           (int)left_coded_length,
                                           (int)above_coded_length);

  uint64_t nonzero_bitmap = 0;
  coded_length_ = 0;
  num_zeros_ = 64;
  int num_zeros_read = 0;
  bool last_was_zero = false;
  for (unsigned int index = 0; index < 64; ++index) {  
      int num_zeros_context = std::max(15 - num_zeros_read, 0);
      bool cur_zero = data.get( num_zeros_prob.at(index).at(num_zeros_context).at(0) );
#ifdef DEBUGDECODE
      fprintf(stderr, "XXZ %d %d %d %d %d => %d\n", (int)index, 666, 666, num_zeros_context, 0, cur_zero ? 1 : 0);
#endif
      if (!cur_zero) {
          uint64_t to_shift = 1UL;
          to_shift <<= index;
          nonzero_bitmap |= to_shift;
          coded_length_ = index + 1;
          --num_zeros_;
      } else {
          ++num_zeros_read;
      }
      if (!last_was_zero) {
          bool cur_eob = data.get( num_zeros_prob.at(index>0).at(num_zeros_context).at(1) );
#ifdef DEBUGDECODE
          fprintf(stderr, "XXZ %d %d %d %d %d => %d\n", (int)index, 666, 666, num_zeros_context, 1, cur_eob ? 1 : 0);
#endif
          if (cur_eob) {
              break; // EOB reached
          }
      }
      last_was_zero = cur_zero;
  }
  int slice = BLOCK_SLICE;
  uint8_t divided_num_zeros = num_zeros_ / slice;
  if (divided_num_zeros == (64/slice)) divided_num_zeros--;
  assert(divided_num_zeros < 64/slice);

  const int16_t num_zeros = min( uint8_t(NUM_ZEROS_BINS-1), divided_num_zeros );

  const int16_t eob_bin = min( uint8_t(EOB_BINS-1), uint8_t(coded_length_/(64 / EOB_BINS)));

  for (
#ifdef BLOCK_ENCODE_BACKWARDS
      int index = std::min((uint8_t)63, coded_length_); index >= 0 ; --index
#else
      unsigned int index = 0; index <= std::min((uint8_t)63, coded_length_); ++index
#endif
      ) {
    /* select the tree probabilities based on the prediction context */
      uint64_t nonzero_check = 1UL;
      nonzero_check <<= index;
      if (0 == (nonzero_bitmap & nonzero_check)) {
#ifdef DEBUGDECODE
          fprintf(stderr,"XXB\n");
#endif
          coefficients_.at( jpeg_zigzag.at( index ) ) = 0;          
          continue;
      }
      Optional<int16_t> above_neighbor_context;
      Optional<int16_t> left_neighbor_context;
      if ( context().left.initialized() ) {
          left_neighbor_context = Optional<int16_t>(context().left.get()->coefficients().at( jpeg_zigzag.at( index ) ));
      }
      if ( context().above.initialized() ) {
          above_neighbor_context = context().above.get()->coefficients().at( jpeg_zigzag.at( index ) );
      }
      uint8_t coord = jpeg_zigzag.at( index );
      std::pair<Optional<int16_t>,
                Optional<int16_t> > intra_block_neighbors = 
          get_near_coefficients(coord);
      auto & prob = probability_tables.branch_array( std::min((unsigned int)type_, BLOCK_TYPES - 1),
                                                     num_zeros,
                                                     eob_bin,
                                                     index_to_cat(index));
      DecoderState4s dct_decoder_state(&data, &prob,
                                       left_neighbor_context, above_neighbor_context,
                                       intra_block_neighbors.first, intra_block_neighbors.second);
      int16_t value;
      if (false && index == 0) {// DC coefficient
          value = get_one_signed_coefficient( dct_decoder_state , true).second;
          if (left_neighbor_context.initialized()) {
              value += left_neighbor_context.get();
          } else if (above_neighbor_context.initialized()) {
              value += above_neighbor_context.get();
          }
      } else { // AC coefficient
          value= get_one_signed_nonzero_coefficient( dct_decoder_state );
      }
      /* assign to block storage */
      coefficients_.at( jpeg_zigzag.at( index ) ) = value;
#ifdef DEBUGDECODE
      fprintf(stderr, "XXA %d %d(%d) %d %d => %d\n",
              std::min((unsigned int)type_, BLOCK_TYPES - 1),
              (int)num_zeros,(int)num_zeros_,
              (int)eob_bin,
              (int)index_to_cat(index),
              value);
#endif
  }
  recalculate_coded_length();
}
