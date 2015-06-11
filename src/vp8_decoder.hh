/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */


class VP8ComponentDecoder {
    bool started_scan[4];
    int cmp;
    int cur_read_batch[4];
public:
    VP8ComponentDecoder();
    DecoderReturnValue vp8_decoder(UncompressedComponents* colldata, iostream *str_in);
    static void vp8_continuous_decoder(UncompressedComponents* colldata, iostream *str_in);
};
