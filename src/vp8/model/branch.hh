#ifndef _BRANCH_HH_
#define _BRANCH_HH_
typedef uint8_t Probability;
//#define JPEG_ENCODER
// ^^^ if we want to try to use the JPEG spec arithmetic coder, uncomment above

class Branch
{
private:
  uint32_t false_count_ = 1, true_count_ = 0;
    uint32_t full_count_ =0;
  Probability probability_ = 128;
  friend class JpegBoolDecoder;
  friend class JpegBoolEncoder;
public:
  Probability prob() const { return probability_; }

  uint32_t true_count() const { return true_count_; }
  uint32_t false_count() const { return false_count_; }
  
  void record_true( void ) { true_count_ = true_count_ + 1; }
  void record_false( void ) { false_count_ = false_count_ + 1; }
  void record_true_and_update( void ) {
      ++full_count_;
      if (true_count_ < 511) {
          true_count_ += 1;
      } else {
          normalize();
      }
#ifdef STOP_TRAINING
      if (full_count_ < 1024)
#endif
      {
          optimize();
      }
  }
  void record_false_and_update( void ) {
      ++full_count_;
      if (false_count_ < 511) {// 2x the size of prob
          false_count_ += 1;
      } else {
          normalize();
      }
#ifdef STOP_TRAINING
      if (full_count_ < 1024)
#endif
      {//4x the size of prob
          optimize();
      }
  }
  void normalize() {
      true_count_ = true_count_ / 2 + (true_count_ & 1);
      false_count_ = false_count_ / 2 + (false_count_ & 1);
      
  }
  void optimize()
  {
#ifndef JPEG_ENCODER
    const int prob = 256 * (false_count() + 1) / (false_count() + true_count() + 2);

    assert( prob >= 0 );
    assert( prob <= 255 );

    probability_ = prob;
#endif
  }

  Branch();
};
#endif
