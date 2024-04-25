#include "FileDecode.h"

FileDecode::~FileDecode()
{
    if (swrResample != NULL)
    {
        delete swrResample;
        swrResample = NULL;
    }
}

int FileDecode::AVOpenFile(std::string filename)
{

#ifdef WRITE_DECODED_PCM_FILE
    outdecodedfile = fopen("decode.pcm", "wb");
    if (!outdecodedfile) {
        std::cout << "open out put file failed";
    }
#endif

#ifdef WRITE_DECODED_YUV_FILE
    outdecodedYUVfile = fopen("decoded_video.yuv", "wb");
    if (!outdecodedYUVfile) {
        std::cout << "open out put YUV file failed";
    }
#endif

	int openInputResult = avformat_open_input(&formatCtx, filename.c_str(), NULL, NULL);
    if (openInputResult != 0) {
        std::cout << "open input failed" << std::endl;
        return -1;
    }

    if (avformat_find_stream_info(formatCtx, NULL) < 0) {
        std::cout << "find stram info faild" << std::endl;
        return -1;
    }

    av_dump_format(formatCtx, 0, filename.c_str(), 0);

    audioStream = av_find_best_stream(formatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audioStream < 0) {
        std::cout << "av find best audio stream failed" << std::endl;
        return -1;
    }

    videoStream = av_find_best_stream(formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (videoStream < 0) {
        std::cout << "av find best video stream failed" << std::endl;
        return -1;
    }

    return 0;
}

int FileDecode::OpenAudioDecode()
{
    audioCodecCtx = formatCtx->streams[audioStream]->codec;

    AVCodec* codec = avcodec_find_decoder(audioCodecCtx->codec_id);
    if (codec == NULL) {
        std::cout << "cannot find audio codec id: " << audioCodecCtx->codec_id << std::endl;
        return -1;
    }

    // Open codec
    AVDictionary* dict = NULL;
    int codecOpenResult = avcodec_open2(audioCodecCtx, codec, &dict);
    if (codecOpenResult < 0) {
        std::cout << "open audio decode faild" << std::endl;
        return -1;
    }

    return 0;
}

int FileDecode::OpenVideoDecode() {
    videoCodecCtx = formatCtx->streams[videoStream]->codec;

    AVCodec* codec = avcodec_find_decoder(videoCodecCtx->codec_id);
    if (codec == NULL) {
        std::cout << "cannot find video codec id: " << videoCodecCtx->codec_id << std::endl;
        return -1;
    }

    // Open codec
    AVDictionary* dict = NULL;
    int codecOpenResult = avcodec_open2(videoCodecCtx, codec, &dict);
    if (codecOpenResult < 0) {
        std::cout << "open video decode faild" << std::endl;
        return -1;
    }

    return 0;
}

int FileDecode::Decode()
{
    
    AVPacket avpkt;
    av_init_packet(&avpkt);
    do {
        
       
        if (av_read_frame(formatCtx, &avpkt) < 0) {

            //û�ж������ݣ�˵��������
            return 0;
        }
        if (avpkt.stream_index == audioStream)
        {
            //std::cout << "read one audio frame" << std::endl;
            DecodeAudio(&avpkt);
            av_packet_unref(&avpkt);
        }
        else if(avpkt.stream_index == videoStream) {
            DecodeVideo(&avpkt);
            av_packet_unref(&avpkt);
            continue;
        }
    } while (avpkt.data == NULL);
}

void FileDecode::Close()
{
    if (swrResample) {
        swrResample->Close();
    }

#ifdef  WRITE_DECODED_PCM_FILE
    fclose(outdecodedfile);
#endif //  WRITE_DECODED_PCM_FILE

#ifdef  WRITE_DECODED_YUV_FILE
    fclose(outdecodedYUVfile);
#endif //  WRITE_DECODED_PCM_FILE



  
    if (audioCodecCtx) {
        avcodec_close(audioCodecCtx); // ע������Ҫ�ùرգ���Ҫ������free���᲻�����ף�����avformat_close_input����
        //avcodec_free_context(&audioCodecCtx);
    }
    if (videoCodecCtx) {
        avcodec_close(videoCodecCtx);
        //avcodec_free_context(&videoCodecCtx);
    }

    if (formatCtx) {
        avformat_close_input(&formatCtx);
    }
    
}

int FileDecode::DecodeAudio(AVPacket* originalPacket)
{
    int ret = avcodec_send_packet(audioCodecCtx, originalPacket);
    if (ret < 0)
    {
        return -1;
    }
    AVFrame* frame = av_frame_alloc();
    ret = avcodec_receive_frame(audioCodecCtx, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
        return -2;
    }else if (ret < 0) {
        std::cout << "error decoding";
        return -1;
    }

    int data_size = av_get_bytes_per_sample(audioCodecCtx->sample_fmt);
    if (data_size < 0) {
        /* This should not occur, checking just for paranoia */
        std::cout << "Failed to calculate data size\n";
        return -1;
    }

#ifdef WRITE_DECODED_PCM_FILE
    for (int i = 0; i < frame->nb_samples; i++)
        for (int ch = 0; ch < audioCodecCtx->channels; ch++)
            fwrite(frame->data[ch] + data_size * i, 1, data_size, outdecodedfile);
#endif

    // ��AVFrame��������ݿ�������Ԥ����src_data����
    if (swrResample == NULL)
    {
        swrResample = new SwrResample();

        //�����ز�����Ϣ
        int src_ch_layout = audioCodecCtx->channel_layout;
        int src_rate = audioCodecCtx->sample_rate;
        enum AVSampleFormat src_sample_fmt = audioCodecCtx->sample_fmt;

        int dst_ch_layout = AV_CH_LAYOUT_STEREO;
        int dst_rate = 44100;
        enum AVSampleFormat dst_sample_fmt = audioCodecCtx->sample_fmt;

        //aac����һ�������,ʵ�����ֵֻ�ܴӽ��������������ȡ�����������ʼ�����̿��Է��ڽ������һ֡��ʱ��
        int src_nb_samples = frame->nb_samples;

        swrResample->Init(src_ch_layout, dst_ch_layout,
            src_rate, dst_rate,
            src_sample_fmt, dst_sample_fmt,
            src_nb_samples);
    }
   
    ret = swrResample->WriteInput(frame);

    int res = swrResample->SwrConvert();
   
    av_frame_free(&frame);
    return 0;
}

int FileDecode::DecodeVideo(AVPacket* originalPacket)
{
    int ret = avcodec_send_packet(videoCodecCtx, originalPacket);
    if (ret < 0)
    {
        return -1;
    }
    AVFrame* frame = av_frame_alloc();
    ret = avcodec_receive_frame(videoCodecCtx, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return -2;
    }
    else if (ret < 0) {
        std::cout << "error decoding";
        return -1;
    }
#ifdef  WRITE_DECODED_YUV_FILE


    enum AVPixelFormat pix_fmt = videoCodecCtx->pix_fmt;
    bool isPlanar = is_planar_yuv(pix_fmt);

    int wrapy = frame->linesize[0];
    int wrapu = frame->linesize[1];
    int wrapv = frame->linesize[2];
    int xsize = frame->width;
    int ysize = frame->height;

    if (pix_fmt == AV_PIX_FMT_YUV420P) {
        fwrite(frame->data[0], 1, wrapy * ysize, outdecodedYUVfile); // Y
        fwrite(frame->data[1], 1, wrapu * ysize / 2, outdecodedYUVfile); // U
        fwrite(frame->data[2], 1, wrapv * ysize / 2, outdecodedYUVfile); // V
    }


#endif //  WRITE_DECODED_PCM_FILE

    av_frame_free(&frame);
   
}

bool FileDecode::is_planar_yuv(enum AVPixelFormat pix_fmt) {
   
    // Check if the pixel format corresponds to planar layout
    switch (pix_fmt) {
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUV422P:
    case AV_PIX_FMT_YUV444P:
        // Add more planar pixel formats here if needed
        return 1; // Planar layout
    default:
        return 0; // Non-planar layout
    }
}
