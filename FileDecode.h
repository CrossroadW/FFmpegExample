#pragma once
#include <string>
#include <iostream>
#include "SwrResample.h"
#include <thread>
#include "JitterBuffer.h"
#include <memory>

#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <atomic>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
}

//#define WRITE_DECODED_PCM_FILE
//#define WRITE_DECODED_YUV_FILE

class MyQtMainWindow;

#define AVJitterBuffer JitterBuffer<AVPacket*> 

class FileDecode
{
public:

	FileDecode():audio_packet_buffer(nullptr),video_packet_buffer(nullptr), swrResample(nullptr)
	{
		
	}

	~FileDecode();
	int AVOpenFile(std::string filename);
	int OpenAudioDecode();
	int OpenVideoDecode();
	int StartRead(std::string);
	int InnerStartRead();

	void SetPosition(int position);
	
	void Close();
	
	void SetMyWindow(MyQtMainWindow* mywindow);

	void PauseRender();
	void ResumeRender();

	void PauseRead();
	void ResumeRead();

	void ClearJitterBuf();

	void static AVPacketFreeBind(AVPacket* pkt)
	{
		av_free_packet(pkt);
		av_packet_unref(pkt);
	}

	std::string getCurrentTimeAsString() {
		// ��ȡ��ǰʱ���
		auto now = std::chrono::system_clock::now();

		// ����ǰʱ���ת��Ϊ time_t ����
		auto now_c = std::chrono::system_clock::to_time_t(now);

		// ��ȡ���벿��
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

		// ��ʽ�����
		std::stringstream ss;
		ss << " ["<<std::put_time(std::localtime(&now_c), "%T") // ʱ����
			<< '.' << std::setfill('0') << std::setw(3) << ms.count()<<"]:"; // ����

		return ss.str();
	}

	int64_t GetPlayingMs();
	int64_t GetFileLenMs();

private:

	int64_t player_start_time_ms = 0;
	/*
	* �Զ����һ��ϵͳʱ�䣬���ʱ����������Ⱦ��� player_start_time_ms
	* �������ƶ���Ƶλ�õ�ʱ����Ҫ����ʱ�䣬��Ȼ��Ⱦ�жϾ�������
	*/
	inline void StartSysClockMs() // ��¼����ʱ�䣬�������ֻ��ִ��һ��
	{
		if (player_start_time_ms == 0)
		{
			player_start_time_ms = now_ms();
		}
	}

	inline int64_t now_ms()
	{
		auto now = std::chrono::system_clock::now();
		return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
	}

	inline int64_t GetSysClockMs()
	{
		if (player_start_time_ms != 0)
		{
			
			return  now_ms() - player_start_time_ms;
		}
		return 0;
	}

	int64_t video_stream_time = 0; //��ǰ��Ƶ����Ⱦʱ��
	int64_t audio_stream_time = 0; //��ǰ��Ƶ����Ⱦʱ��

	//��seek��ʱ������ʼ������һ��
	inline void ClockReset(int64_t seek_time)
	{
		audio_stream_time = seek_time;
		curr_playing_ms = audio_stream_time;
		video_stream_time = seek_time;

		//�����ⷭ������ȡGetSysClockMs()�����ľ��� seek_time
		// now_ms() - (now_ms() - seek_time)

		player_start_time_ms = now_ms() - seek_time;

		// ��������ʵ�и�������Ĳ�����ⲻ������û����ô����
	}


	int VideoDecodeFun();
	int AudioDecodeFun();
	void RunFFmpeg(std::string url);

	int DecodeAudio(AVPacket* originalPacket);
	int DecodeVideo(AVPacket* originalPacket);
	int ResampleAudio(AVFrame* frame);

	bool is_planar_yuv(enum AVPixelFormat pix_fmt);


private:
	
	AVFormatContext* formatCtx = NULL;
	AVCodecContext* audioCodecCtx = NULL;
	AVCodecContext* videoCodecCtx = NULL;

#ifdef WRITE_DECODED_PCM_FILE
	FILE* outdecodedfile = NULL;
#endif 

#ifdef WRITE_DECODED_YUV_FILE
	FILE* outdecodedYUVfile = NULL;
#endif 

	int audioStream;  //AVFormat ��Ƶ������
	int videoStream;  //AVFormat ��Ƶ������

	std::unique_ptr<SwrResample> swrResample; // ��Ƶ�ز���

	MyQtMainWindow* qtWin = NULL;

	bool videoDecodeThreadFlag = true;
	std::thread* videoDecodeThread = nullptr;;  // ��Ƶ������Ⱦ�߳�

	bool audioDecodeThreadFlag = true;
	std::thread* audioDecodeThread = nullptr;;  //��Ƶ������Ⱦ�߳�

	
	bool read_frame_flag = true; //av_read_frame�̵߳ı�ʶ
	std::thread* player_thread_ = nullptr;
	

	std::unique_ptr<AVJitterBuffer> audio_packet_buffer; //��Ƶ��������Ⱦjitter buffer
	std::unique_ptr<AVJitterBuffer> video_packet_buffer; //��Ƶ��������Ⱦjitter buffer

	int64_t audio_frame_dur = 0; //һ֡��Ƶ��Ҫ������ʱ��
	int64_t video_frame_dur= 0; // һ֡��Ƶ��Ҫ������ʱ��

	int64_t file_len_ms; //��Ƶ�ܳ���
	int64_t curr_playing_ms; //��ǰ���ų���,ȡ����Ƶ����ʱ���

	bool pauseFlag =false;

	

	int64_t position_ms =  -1; //seek ʱ��ָ��λ�õĺ�����

	std::mutex read_mutex_;  //���ļ���

	bool pause_read_flag = true; //��ͣ��֡�ı�ʶ
};

