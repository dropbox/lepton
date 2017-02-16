#include "tf_perceptron_luma.hh"
#include "tf_perceptron_chroma1.hh"
#include "tf_perceptron_chroma2.hh"

template<BlockType B, typename T> void tf_predict(T* vals_in, float* vals_out) {
  if (B == BlockType::Cb) {
    tf_predict_chroma1(vals_in, vals_out);
  } else if (B == BlockType::Cr) {
    tf_predict_chroma2(vals_in, vals_out);
  } else {
    tf_predict_luma(vals_in, vals_out);
  }
}

template<BlockType B, typename T> float tf_unpredict(T* vals_in, int index) {
  if (B == BlockType::Cb) {
    return tf_unpredict_chroma1(vals_in, index);
  } else if (B == BlockType::Cr) {
    return tf_unpredict_chroma2(vals_in, index);
  } else {
    return tf_unpredict_luma(vals_in, index);
  }
}
