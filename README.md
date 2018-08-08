# Lepton

Lepton is a tool and file format for losslessly compressing JPEGs by an average of 22%.

This can be used to archive large photo collections, or to serve images live and save 22% bandwidth.


## Structure of the Lepton codebase
### Important modules
| Module                    | Purpose |
|:-------------------------:|---------|
| arithmetic_coder          | Handles the math of compression/decompression, i.e. arithmetic coding |
| codec                     | Manages the control flow of the compression/decompression of image data and keeps track of the states |
| codec/specialization      | Polymorphism for different control flow for compression/decompression |
| compressor/brotli_encoder | Wrapper around the `brotli` crate |
| compressor/compressor     | Dispatches input among discard, prefix random data, lepton encoder and suffix random data. Builds primary header and compresses secondary header. |
| compressor/lepton_encoder | Handles transition between JPEG parsing and compression. Builds partial secondary header. |
| iostream                  | Provides an (`InputStrea`, `OutputStream`) pair that can be used to transform a streaming interface to a more friendly input pattern on a second thread |
| jpeg/decoder              | Parses the semantics of JPEG header and decodes image data into frequency space |
| jpeg/encoder              | Encodes frequency image data according to JPEG standard |
| jpeg/stream_decoder       | Wrapper around jpeg/decoder that provides a streaming interface |

### Flow
#### To encode a file
* `LeptonCompressor` has a `LeptonEncoder` and a `BrotliEncoder` from the `brotli` crate.
* When `LeptonCompressor::encode` is called, the compressor buffers prefix random data until the start of the JPEG image and then starts feeding data to `LeptonEncoder`.
* `LeptonEncoder` has a `JpegStreamDecoder`, which parses the semantics of the JPEG header and decodes the huffman-coded image data back to frequency space.
* After `JpegStreamDecoder` finishes or `LeptonCompressor::flush` is called, `LeptonEncoder` generates a partial raw secondary Lepton header and compresses image data.
  * `LeptonEncoder` chooses the number of threads/`LeptonCodec`s according to the distance between the start of the first scan and the end of the last scan.
  * Each codec compresses its segment independently using arithmetic encoding.
  * The output of the codecs is serialized using `Mux` from the `lepton-mux` crate.
* After `LeptonEncoder` finishes, `LeptonCompressor` compresses the partial secondary Lepton header using `BrotliEncoder`.
* After `LeptonCompressor::flush` is called, `LeptonCompressor` completes the secondary Lepton header, constructs the primary Lepton header and starts writing out compression output.

#### To decode a file
* `LeptonDecompressor` has a `InternalDecompressor`, which is a enum that switches from `PrimaryHeader(PrimaryHeaderParser)` mode to `SecondaryHeader(SecondaryHeaderParser)` mode to `CMP(LeptonDecoder)` mode.
* In addition to demultiplexing the secondary Lepton header and writing out prefix random data in the `PGE` section, `SecondaryHeaderParser` also parses the semantics of the JPEG header contained in the `HDR` section.
* `LeptonDecoder` deserializes compressed image data into each `LeptonCodec`'s input using `Mux` from `lepton-mux` crate.
* Each codec decompresses its segment independently by performing the exact reverse operation as in compression.
* The output from each codec are written out in sequence, followed by suffix random data.

## License

Unless otherwise noted:

```
Copyright (c) 2016 Dropbox, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```
