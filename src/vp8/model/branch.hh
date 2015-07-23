#ifndef _BRANCH_HH_
#define _BRANCH_HH_
typedef uint8_t Probability;
//#define JPEG_ENCODER
// ^^^ if we want to try to use the JPEG spec arithmetic coder, uncomment above
class Branch
{
private:
  uint32_t false_count_ = 0, true_count_ = 0;
    Probability probability_ = 255;
  friend class JpegBoolDecoder;
  friend class JpegBoolEncoder;
public:
  Probability prob() const { return probability_; }

  uint32_t true_count() const { return true_count_; }
  uint32_t false_count() const { return false_count_; }
  
  void record_true( void ) { true_count_ = true_count_ + 1; }
  void record_false( void ) { false_count_ = false_count_ + 1; }
  void record_true_and_update( void ) {
      if (true_count_ < 255) {
          true_count_ += 1;
      } else if (false && false_count_ > 0 && false_count_ < 128){
          --false_count_;
      } else {
          true_count_ = true_count_ / 2 + (true_count_ & 1);
          false_count_ = false_count_ / 2 + (false_count_ & 1);
      }
      normalize();
      optimize();
  }
  void record_false_and_update( void ) {
      if (false_count_ < 255) {
          false_count_ += 1;
      } else if (false && true_count_ > 0 && true_count_ < 128){
          --true_count_;
      } else {
          true_count_ = true_count_ / 2 + (true_count_ & 1);
          false_count_ = false_count_ / 2 + (false_count_ & 1);
      }
      normalize();
      optimize();
  }
  void normalize() 
  {
#ifndef JPEG_ENCODER
#if 0
      while (true_count() > 254 || false_count() > 254) {
          true_count_ = true_count_ / 2 + (true_count_ & 1);
          false_count_ = false_count_ / 2 + (false_count_ & 1);
      }
#endif
#endif
      /*    
     if (probability_ > 204) {
          true_count_ = 0;
          false_count_ = 3;
      } else if (probability_ < 51) {
          true_count_ = 3;
          false_count_ = 0;
      } else if (probability_ < 64) {
          true_count_ = 2;
          false_count_ = 0;
      } else if (probability_ > 192) {
          true_count_ = 0;
          false_count_ = 2;
      } else if (probability_ > 168) {
          true_count_ = 1;
          false_count_ = 2;
      } else if (probability_ < 85) {
          true_count_ = 2;
          false_count_ = 1;
      } else {
          true_count_ = 2;
          false_count_ = 2;
      }
*/
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
