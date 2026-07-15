extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

#include <windows.h>
#include <stdint.h>
#include <wchar.h>
#include <string>

static AVFormatContext* g_fmt = nullptr;
static AVCodecContext* g_enc = nullptr;
static AVStream* g_st = nullptr;

static AVCodecContext* g_jpegDec = nullptr;
static SwsContext* g_sws = nullptr;

static bool g_bRecording = false;

static std::string Utf16ToUtf8(const wchar_t* ws) {
    int len = WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
    std::string s(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws, -1, &s[0], len, nullptr, nullptr);
    if (!s.empty() && s.back() == '\0') s.pop_back();
    return s;
}

static int64_t g_lastPts = 0;
static int64_t g_frameIndex = 0;


static void Log(const char* msg)
{
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");
}
static void LogErr(const char* msg, int err)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "%s (err=%d)\n", msg, err);
    OutputDebugStringA(buf);
}


extern "C" {

    __declspec(dllexport)
        int __stdcall StartRecorder(int width, int height, const wchar_t* outputPath)
    {
        if (g_bRecording) { Log("StartRecorder: already recording"); return 0; }

        std::string out = Utf16ToUtf8(outputPath);

        int ret = avformat_alloc_output_context2(&g_fmt, nullptr, "webm", out.c_str());
        if (ret < 0) { LogErr("avformat_alloc_output_context2 failed", ret); return 0; }

        const AVCodec* vcodec = avcodec_find_encoder(AV_CODEC_ID_VP8);
        if (!vcodec) { Log("avcodec_find_encoder VP8 failed"); return 0; }

        g_st = avformat_new_stream(g_fmt, vcodec);
        if (!g_st) { Log("avformat_new_stream failed"); return 0; }

        g_enc = avcodec_alloc_context3(vcodec);
        g_enc->width = width;
        g_enc->height = height;
        g_enc->pix_fmt = AV_PIX_FMT_YUV420P;
        g_enc->time_base = AVRational{ 1, 1000 };   // timestamps in ms
        g_enc->bit_rate = 1'000'000;
        g_st->time_base = g_enc->time_base;

        ret = avcodec_open2(g_enc, vcodec, nullptr);
        if (ret < 0) { LogErr("avcodec_open2 encoder failed", ret); return 0; }

        const AVCodec* mjpeg = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
        if (!mjpeg) { Log("avcodec_find_decoder MJPEG failed"); return 0; }

        g_jpegDec = avcodec_alloc_context3(mjpeg);
        ret = avcodec_open2(g_jpegDec, mjpeg, nullptr);
        if (ret < 0) { LogErr("avcodec_open2 jpegDec failed", ret); return 0; }

        // Open output file
        if (!(g_fmt->oformat->flags & AVFMT_NOFILE)) {
            ret = avio_open(&g_fmt->pb, out.c_str(), AVIO_FLAG_WRITE);
            if (ret < 0) { LogErr("avio_open failed", ret); return 0; }
        }

        // Copy encoder parameters into stream (MANDATORY)
        if (avcodec_parameters_from_context(g_st->codecpar, g_enc) < 0) {
            Log("avcodec_parameters_from_context failed");
            return 0;
        }

        ret = avformat_write_header(g_fmt, nullptr);
        if (ret < 0) { LogErr("avformat_write_header failed", ret); return 0; }

        // Reset PTS counter for new recording
        g_frameIndex = 0;
        g_lastPts = 0;

        Log("StartRecorder: success");
        g_bRecording = true;
        return 1;
    }


    // Decode MJPEG → raw frame
    static AVFrame* DecodeJpeg(const uint8_t* buf, int len)
    {
        AVPacket* pkt = av_packet_alloc();
        if (!pkt) { Log("DecodeJpeg: av_packet_alloc failed"); return nullptr; }

        pkt->data = const_cast<uint8_t*>(buf);
        pkt->size = len;

        int ret = avcodec_send_packet(g_jpegDec, pkt);
        if (ret < 0) { LogErr("DecodeJpeg: send_packet failed", ret); av_packet_free(&pkt); return nullptr; }

        AVFrame* frame = av_frame_alloc();
        if (!frame) { Log("DecodeJpeg: av_frame_alloc failed"); av_packet_free(&pkt); return nullptr; }

        ret = avcodec_receive_frame(g_jpegDec, frame);
        if (ret < 0) {
            LogErr("DecodeJpeg: receive_frame failed", ret);
            av_frame_free(&frame);
            av_packet_free(&pkt);
            return nullptr;
        }

        av_packet_unref(pkt);
        av_packet_free(&pkt);
        return frame;
    }




    __declspec(dllexport)
        int __stdcall WriteFrame(const uint8_t* buf, int len, double timestampMs)
    {
        if (!g_bRecording) { Log("WriteFrame: not recording"); return -1; }
        if (!buf || len <= 0) { Log("WriteFrame: invalid buffer"); return -1; }

        // Decode MJPEG → src frame
        AVFrame* src = DecodeJpeg(buf, len);
        if (!src) { Log("WriteFrame: DecodeJpeg failed"); return -1; }

        // Allocate YUV420P frame for encoder
        AVFrame* yuv = av_frame_alloc();
        if (!yuv) {
            Log("WriteFrame: av_frame_alloc yuv failed");
            av_frame_free(&src);
            return -1;
        }

        yuv->format = g_enc->pix_fmt;
        yuv->width = g_enc->width;
        yuv->height = g_enc->height;

        int ret = av_frame_get_buffer(yuv, 32);
        if (ret < 0) {
            LogErr("WriteFrame: av_frame_get_buffer failed", ret);
            av_frame_free(&src);
            av_frame_free(&yuv);
            return -1;
        }

        // Init scaler if needed
        if (!g_sws) {
            g_sws = sws_getContext(
                src->width, src->height, (AVPixelFormat)src->format,
                g_enc->width, g_enc->height, g_enc->pix_fmt,
                SWS_BILINEAR, nullptr, nullptr, nullptr
            );
            if (!g_sws) {
                Log("WriteFrame: sws_getContext failed");
                av_frame_free(&src);
                av_frame_free(&yuv);
                return -1;
            }
        }

        // Convert MJPEG → YUV420P
        sws_scale(
            g_sws,
            src->data, src->linesize,
            0, src->height,
            yuv->data, yuv->linesize
        );

        av_frame_free(&src);

        //
        // REAL-TIME PTS (ms) — THIS FIXES PLAYBACK SPEED
        //
        int64_t pts = (int64_t)timestampMs;

        // enforce monotonic PTS (VP8 requirement)
        if (pts <= g_lastPts)
            pts = g_lastPts + 1;

        g_lastPts = pts;
        yuv->pts = pts;

        //
        // Encode
        //
        ret = avcodec_send_frame(g_enc, yuv);
        if (ret < 0) {
            LogErr("WriteFrame: avcodec_send_frame failed", ret);
            av_frame_free(&yuv);
            return -1;
        }

        av_frame_free(&yuv);

        //
        // Try to pull one packet
        //
        AVPacket* pkt = av_packet_alloc();
        if (!pkt) {
            Log("WriteFrame: av_packet_alloc failed");
            return -1;
        }

        ret = avcodec_receive_packet(g_enc, pkt);
        if (ret == 0) {

            pkt->stream_index = g_st->index;
            pkt->pts = pts;
            pkt->dts = pts;
            pkt->duration = 1;   // 1 ms duration in a 1/1000 time_base

            int wret = av_interleaved_write_frame(g_fmt, pkt);
            if (wret < 0) LogErr("WriteFrame: av_interleaved_write_frame failed", wret);

            av_packet_unref(pkt);
        }
        else {
            LogErr("WriteFrame: avcodec_receive_packet no packet", ret);
        }

        av_packet_free(&pkt);
        return 1;
    }





    __declspec(dllexport)
        int __stdcall StopRecorder()
    {
        if (!g_bRecording) { Log("StopRecorder: not recording"); return 0; }

        AVPacket* pkt = av_packet_alloc();
        if (pkt) {
            int sret = avcodec_send_frame(g_enc, nullptr);
            LogErr("StopRecorder: send_frame(NULL)", sret);

            int rret;
            while ((rret = avcodec_receive_packet(g_enc, pkt)) == 0) {
                pkt->stream_index = g_st->index;
                int wret = av_interleaved_write_frame(g_fmt, pkt);
                LogErr("StopRecorder: write_frame(flush)", wret);
                av_packet_unref(pkt);
            }
            LogErr("StopRecorder: receive_packet flush done", rret);
            av_packet_free(&pkt);
        }

        av_write_trailer(g_fmt);

        if (!(g_fmt->oformat->flags & AVFMT_NOFILE))
            avio_close(g_fmt->pb);

        avformat_free_context(g_fmt);
        g_fmt = nullptr;

        avcodec_free_context(&g_enc);
        avcodec_free_context(&g_jpegDec);

        if (g_sws) {
            sws_freeContext(g_sws);
            g_sws = nullptr;
        }

        g_bRecording = false;
        return 1;
    }



} // extern "C"
