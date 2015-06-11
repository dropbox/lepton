/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

class VP8ComponentEncoder {
public:
    static void vp8_full_encoder( const UncompressedComponents * const colldata,
                                  iostream * const str_out );
};
