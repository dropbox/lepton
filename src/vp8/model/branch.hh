#ifndef _BRANCH_HH_
#define _BRANCH_HH_
#include "numeric.hh"
#include <cmath>
typedef uint8_t Probability;

//#define VP8_ENCODER 1

//#define JPEG_ENCODER
// ^^^ if we want to try to use the JPEG spec arithmetic coder, uncomment above
//#define USE_COUNT_FREE_UPDATE
class Branch
{
private:
  uint8_t counts_[2] = {1, 1};
  Probability probability_ = 128;
#ifdef USE_COUNT_FREE_UPDATE
  uint8_t lprob = 128;
#endif
  friend class JpegBoolDecoder;
  friend class JpegBoolEncoder;
public:
  Probability prob() const { return probability_; }

  uint32_t true_count() const { return counts_[1]; }
  uint32_t false_count() const { return counts_[0]; }

    __attribute__((always_inline))
  void record_obs_and_update(bool obs) {
      unsigned int fcount = counts_[0];
      unsigned int tcount = counts_[1];
      bool overflow = (counts_[obs]++ == 0xff);
      if (__builtin_expect(overflow, 0)) { // check less than 512
          counts_[0] = ((1 + (unsigned int)fcount) >> 1);
          counts_[1] = ((1 + (unsigned int)tcount) >> 1);
          counts_[obs] = 129;
          probability_ = optimize(counts_[0] + counts_[1]);
      } else {
          probability_ = optimize(fcount + tcount + 1);
      }
#ifdef USE_COUNT_FREE_UPDATE
      int limit = 120;
      if (lprob == 128 && obs) {
          lprob = 127;
      } else if (lprob == 127 && !obs) {
          lprob = 128;
      } else {
          if (lprob > 127 && !obs) {
              if (lprob < 128 + 40) {
                  lprob += 1;
              } else if (lprob < 128 + 96) {
                  lprob += 2;
              } else {
                  lprob += 3;
              }
              if (lprob > 127 + limit) lprob = 127 + limit;
          } else if (lprob < 128 && obs) {
              if (lprob > 127 - 40) {
                  lprob -= 1;
              } else if (lprob > 127 - 96) {
                  lprob -= 2;
              } else {
                  lprob -=3;
              }
              if (lprob < 128 - limit) {
                  lprob = 128 - limit;
              }
          } else {
              uint8_t pval = obs ? lprob - 128 : 127 - lprob;
              double base = .9575;
              double prob = 0.5 * pow(base, (double)pval);
              double new_prob = base * prob + 1 - base;
              int search_result = pval - 1;
              double best_search_dist = fabs(0.5 * pow(base, (double)search_result) - new_prob);
              for (int search = search_result - 1; search > 0; --search) {
                  double search_prob = 0.5 * pow(base, (double)search);
                  if (fabs(search_prob - new_prob) < best_search_dist) {
                      search_result =  search;
                      best_search_dist = fabs(search_prob - new_prob);
                  } else break;
              }
              lprob = obs ? search_result + 128 : 127 - search_result;
              probability_ = new_prob * 256;
              if (obs) {
                  probability_ = 255 - probability_;
              }
          }
      }
#endif
  }
  void normalize() {
      counts_[0] = ((1 + (unsigned int)counts_[0]) >> 1);
      counts_[1] = ((1 + (unsigned int)counts_[1]) >> 1);
      
  }
  __attribute__((always_inline))
  Probability optimize(int sum) const
  {
    assert(false_count() && true_count());
#if 0
      const int prob = (false_count() << 8) / sum;
#else
      const int prob = fast_divide10bit(false_count() << 8,
                                        sum);
#endif
      assert( prob >= 0 );
      assert( prob <= 255 );
      
      return (Probability)prob;

#ifdef JPEG_ENCODER
#error needs to be updated
#endif
  }

  Branch(){}
};
#endif
