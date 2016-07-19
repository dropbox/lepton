#include "MemReadWriter.hh"
namespace Sirikata {
std::pair<Sirikata::uint32, Sirikata::JpegError> MemReadWriter::Write(const Sirikata::uint8*data, unsigned int size) {
    using namespace Sirikata;
    mBuffer.insert(mBuffer.begin() + mWriteCursor, data, data + size);
    mWriteCursor += size;
    return std::pair<Sirikata::uint32, JpegError>(size, JpegError());
}
std::pair<Sirikata::uint32, Sirikata::JpegError> MemReadWriter::Read(Sirikata::uint8*data, unsigned int size) {
    using namespace Sirikata;
    size_t bytesLeft = mBuffer.size() - mReadCursor;
    size_t actualBytesRead = size;
    if (bytesLeft < size) {
        actualBytesRead = bytesLeft;
    }
    if (actualBytesRead > 0) {
        memcpy(data, &mBuffer[mReadCursor], actualBytesRead);
    }
    mReadCursor += actualBytesRead;
    JpegError err = JpegError();
    if (actualBytesRead == 0) {
        err = JpegError::errEOF();
    }
    //fprintf(stderr, "%d READ %02x%02x%02x%02x - %02x%02x%02x%02x\n", (uint32)actualBytesRead, data[0], data[1],data[2], data[3],
    //        data[actualBytesRead-4],data[actualBytesRead-3],data[actualBytesRead-2],data[actualBytesRead-1]);

	//	The size_t -> Sirikata::uint32 cast is safe because sizeof(size) is <= sizeof(Sirikata::uint32)
	std::pair<Sirikata::uint32, JpegError> retval(static_cast<Sirikata::uint32>(actualBytesRead), err);
    return retval;
}
}
