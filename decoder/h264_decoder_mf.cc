#include "h264_decoder_mf.h"

#include <memory>
//#include "webrtc/rtc_base/logging.h"

namespace owt {
namespace base {

std::unique_ptr<H264DecoderMFImpl> MFH264Decoder::Create() {
    //RTC_LOG(LS_INFO) << "Creating H264DecoderMFImpl.";
    return std::make_unique<H264DecoderMFImpl>();
}

bool MFH264Decoder::IsSupported() {
#ifdef WEBRTC_WIN
    return true;
#else
    return false;
#endif
}

}
}
