#include <assert.h>
#include "nn_framework.hh"
Sirikata::Array3d<float (*)(const Sirikata::Array1d<int16_t, 111>&, int index), 17, 16, 64> global_gen_lepton_prior;
class F {
 public:
  mutable int count;
  F() {count = 0;}
  template<class T>void operator()(T* item) const{
    if (item)
      ++count;
}
};
/*
int main() {
  F f;
  global_gen_lepton_prior.foreach(f);
  assert(f.count == 0);
}
*/
