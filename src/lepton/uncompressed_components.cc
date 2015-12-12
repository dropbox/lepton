/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include "../../vp8/util/memory.hh"
#include <thread>
#include "uncompressed_components.hh"
#include "component_info.hh"

int UncompressedComponents::max_number_of_blocks = 0;
