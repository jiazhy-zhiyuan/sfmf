#include "pch.h"
#define _USE_MATH_DEFINES
#include <cmath>
#include <math.h>
#include <wrl\client.h>
#include <ppltasks.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>  
//#include "DirectXBase.h"
#include "sfmf.h"

#pragma comment(lib, "shlwapi")
#pragma comment(lib,"Mfplat.lib")
#pragma comment(lib,"Mf.lib")
#pragma comment(lib,"Mfuuid.lib")
#pragma comment(lib,"Strmiids.lib")
#pragma comment(lib, "mfreadwrite")
#pragma comment(lib, "mfplat")
#pragma comment(lib, "mfuuid")


// Format constants
const UINT32 VIDEO_FPS = 30;
const UINT32 VIDEO_BIT_RATE = 800000;
const GUID   VIDEO_ENCODING_FORMAT = MFVideoFormat_H264;
const GUID   VIDEO_INPUT_FORMAT = MFVideoFormat_RGB32;
const UINT32 VIDEO_PELS = VIDEO_WIDTH * VIDEO_HEIGHT;
const UINT32 VIDEO_FRAME_COUNT = 20 * VIDEO_FPS;



template <class T> void SafeRelease(T **ppT)
{
  if (*ppT)
  {
    (*ppT)->Release();
    *ppT = NULL;
  }
};

// Buffer to hold the video frame data.
DWORD videoFrameBuffer[VIDEO_PELS];

HRESULT InitializeSinkWriter(IMFSinkWriter **ppWriter, DWORD *pStreamIndex)
{
  *ppWriter = NULL;
  *pStreamIndex = NULL;

  IMFSinkWriter   *pSinkWriter = NULL;
  IMFMediaType    *pMediaTypeOut = NULL;   
  IMFMediaType    *pMediaTypeIn = NULL;   
  DWORD           streamIndex;     

  HRESULT hr = MFCreateSinkWriterFromURL(L"output.m4v", NULL, NULL, &pSinkWriter);

  // Set the output media type.
  if (SUCCEEDED(hr))
  {
    hr = MFCreateMediaType(&pMediaTypeOut);   
  }
  if (SUCCEEDED(hr))
  {
    hr = pMediaTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);     
  }
  if (SUCCEEDED(hr))
  {
    hr = pMediaTypeOut->SetGUID(MF_MT_SUBTYPE, VIDEO_ENCODING_FORMAT);   
  }
  if (SUCCEEDED(hr))
  {
    hr = pMediaTypeOut->SetUINT32(MF_MT_AVG_BITRATE, VIDEO_BIT_RATE);   
  }
  if (SUCCEEDED(hr))
  {
    hr = pMediaTypeOut->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);   
  }
  if (SUCCEEDED(hr))
  {
    hr = MFSetAttributeSize(pMediaTypeOut, MF_MT_FRAME_SIZE, VIDEO_WIDTH, VIDEO_HEIGHT);   
  }
  if (SUCCEEDED(hr))
  {
    hr = MFSetAttributeRatio(pMediaTypeOut, MF_MT_FRAME_RATE, VIDEO_FPS, 1);   
  }
  if (SUCCEEDED(hr))
  {
    hr = MFSetAttributeRatio(pMediaTypeOut, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);   
  }
  if (SUCCEEDED(hr))
  {
    hr = pSinkWriter->AddStream(pMediaTypeOut, &streamIndex);   
  }

  // Set the input media type.
  if (SUCCEEDED(hr))
  {
    hr = MFCreateMediaType(&pMediaTypeIn);   
  }
  if (SUCCEEDED(hr))
  {
    hr = pMediaTypeIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);   
  }
  if (SUCCEEDED(hr))
  {
    hr = pMediaTypeIn->SetGUID(MF_MT_SUBTYPE, VIDEO_INPUT_FORMAT);     
  }
  if (SUCCEEDED(hr))
  {
    hr = pMediaTypeIn->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);   
  }
  if (SUCCEEDED(hr))
  {
    hr = MFSetAttributeSize(pMediaTypeIn, MF_MT_FRAME_SIZE, VIDEO_WIDTH, VIDEO_HEIGHT);   
  }
  if (SUCCEEDED(hr))
  {
    hr = MFSetAttributeRatio(pMediaTypeIn, MF_MT_FRAME_RATE, VIDEO_FPS, 1);   
  }
  if (SUCCEEDED(hr))
  {
    hr = MFSetAttributeRatio(pMediaTypeIn, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);   
  }
  if (SUCCEEDED(hr))
  {
    hr = pSinkWriter->SetInputMediaType(streamIndex, pMediaTypeIn, NULL);   
  }

  // Tell the sink writer to start accepting data.
  if (SUCCEEDED(hr))
  {
    hr = pSinkWriter->BeginWriting();
  }

  // Return the pointer to the caller.
  if (SUCCEEDED(hr))
  {
    *ppWriter = pSinkWriter;
    (*ppWriter)->AddRef();
    *pStreamIndex = streamIndex;
  }

  SafeRelease(&pSinkWriter);
  SafeRelease(&pMediaTypeOut);
  SafeRelease(&pMediaTypeIn);
  return hr;
}

HRESULT WriteFrame(
  IMFSinkWriter *pWriter, 
  DWORD streamIndex, 
  const LONGLONG& rtStart,        // Time stamp.
  const LONGLONG& rtDuration      // Frame duration.
  )
{
  IMFSample *pSample = NULL;
  IMFMediaBuffer *pBuffer = NULL;

  const LONG cbWidth = 4 * VIDEO_WIDTH;
  const DWORD cbBuffer = cbWidth * VIDEO_HEIGHT;

  BYTE *pData = NULL;

  // Create a new memory buffer.
  HRESULT hr = MFCreateMemoryBuffer(cbBuffer, &pBuffer);

  // Lock the buffer and copy the video frame to the buffer.
  if (SUCCEEDED(hr))
  {
    hr = pBuffer->Lock(&pData, NULL, NULL);
  }
  if (SUCCEEDED(hr))
  {
    hr = MFCopyImage(
      pData,                      // Destination buffer.
      cbWidth,                    // Destination stride.
      (BYTE*)videoFrameBuffer,    // First row in source image.
      cbWidth,                    // Source stride.
      cbWidth,                    // Image width in bytes.
      VIDEO_HEIGHT                // Image height in pixels.
      );
  }
  if (pBuffer)
  {
    pBuffer->Unlock();
  }

  // Set the data length of the buffer.
  if (SUCCEEDED(hr))
  {
    hr = pBuffer->SetCurrentLength(cbBuffer);
  }

  // Create a media sample and add the buffer to the sample.
  if (SUCCEEDED(hr))
  {
    hr = MFCreateSample(&pSample);
  }
  if (SUCCEEDED(hr))
  {
    hr = pSample->AddBuffer(pBuffer);
  }

  // Set the time stamp and the duration.
  if (SUCCEEDED(hr))
  {
    hr = pSample->SetSampleTime(rtStart);
  }
  if (SUCCEEDED(hr))
  {
    hr = pSample->SetSampleDuration(rtDuration);
  }

  // Send the sample to the Sink Writer.
  if (SUCCEEDED(hr))
  {
    hr = pWriter->WriteSample(streamIndex, pSample);
  }

  SafeRelease(&pSample);
  SafeRelease(&pBuffer);
  return hr;
}

void testout()
{
  // Set all pixels to green
  for (DWORD i = 0; i < VIDEO_PELS; ++i)
  {
    videoFrameBuffer[i] = 0x0000FF00;
  }

  //HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  //if (SUCCEEDED(hr))
  {
    HRESULT  hr = MFStartup(MF_VERSION);
    if (SUCCEEDED(hr))
    {
      IMFSinkWriter *pSinkWriter = NULL;
      DWORD stream;

      hr = InitializeSinkWriter(&pSinkWriter, &stream);
      if (SUCCEEDED(hr))
      {
        // Send frames to the sink writer.
        LONGLONG rtStart = 0;
        UINT64 rtDuration = 333333;

        //MFFrameRateToAverageTimePerFrame(VIDEO_FPS, 1, &rtDuration);

        for (DWORD i = 0; i < VIDEO_FRAME_COUNT; ++i)
        {
          hr = WriteFrame(pSinkWriter, stream, rtStart, rtDuration);
          if (FAILED(hr))
          {
            break;
          }
          rtStart += rtDuration;
        }
      }
      if (SUCCEEDED(hr))
      {
        hr = pSinkWriter->Finalize();
      }
      SafeRelease(&pSinkWriter);
      MFShutdown();
    }
    //  CoUninitialize();
  }
}
//----------------------------------------------------------------------

//#include "MediaWriter.h"

using namespace Microsoft::WRL;
using namespace concurrency;
//using namespace Extensions;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;


namespace sf {

    const unsigned int WIDTH = VIDEO_WIDTH;   
    const unsigned int HEIGHT = VIDEO_HEIGHT;   
    const unsigned int RATE_NUM = 30000;   
    const unsigned int RATE_DENOM = 1000;   
    const unsigned int BITRATE = 3000000;   
    const unsigned int ASPECT_NUM = 1;   
    const unsigned int ASPECT_DENOM = 1;   
    const unsigned long  BPP_IN = 32;   
    const long long hnsSampleDuration = 10000000 * (long long)RATE_DENOM / (long long)RATE_NUM;   
    const unsigned long cbMaxLength = WIDTH * HEIGHT * BPP_IN / 8; 
    const unsigned int ONE_SECOND = RATE_NUM / RATE_DENOM;
    const unsigned int FRAME_NUM = 10 * ONE_SECOND;

  Windows::Foundation::IAsyncActionWithProgress<double>^ sf::WriteAsync(Windows::Storage::Streams::IRandomAccessStream^ stream)
  {
    return create_async([stream]
    (progress_reporter<double> reporter, cancellation_token token) {

      // some parameters   

      //AutoMF mf;

      //
      // Sink Writer の作成
      //

      ComPtr<IMFByteStream> spByteStream;
      CHK(MFCreateMFByteStreamOnStreamEx((IUnknown*)stream, &spByteStream));

      ComPtr<IMFAttributes> spAttr;
      CHK(MFCreateAttributes(&spAttr, 10));
      CHK(spAttr->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, true));

      ComPtr<IMFSinkWriter> spSinkWriter;
      CHK(MFCreateSinkWriterFromURL(L".mp4", spByteStream.Get(), spAttr.Get(), &spSinkWriter));

      //   
      // 出力メディアタイプのセットアップ   
      //   

      ComPtr<IMFMediaType> spTypeOut;  
      CHK(MFCreateMediaType(&spTypeOut));   
      CHK(spTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));     
      CHK(spTypeOut->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264));   
      CHK(spTypeOut->SetUINT32(MF_MT_AVG_BITRATE, BITRATE));   
      CHK(spTypeOut->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));
      CHK(MFSetAttributeSize(spTypeOut.Get(), MF_MT_FRAME_SIZE, WIDTH, HEIGHT));   
      CHK(MFSetAttributeRatio(spTypeOut.Get(), MF_MT_FRAME_RATE, RATE_NUM, RATE_DENOM));
      CHK(MFSetAttributeRatio(spTypeOut.Get(), MF_MT_PIXEL_ASPECT_RATIO, ASPECT_NUM, ASPECT_DENOM));

      DWORD streamIndex;     
      CHK(spSinkWriter->AddStream(spTypeOut.Get(), &streamIndex));   

      //   
      // 入力メディアタイプのセットアップ  
      //   

      ComPtr<IMFMediaType> spTypeIn;
      CHK(MFCreateMediaType(&spTypeIn));   
      CHK(spTypeIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));   
      CHK(spTypeIn->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32));     
      CHK(spTypeIn->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));   
      CHK(MFSetAttributeSize(spTypeIn.Get(), MF_MT_FRAME_SIZE, WIDTH, HEIGHT) );   
      CHK(MFSetAttributeRatio(spTypeIn.Get(), MF_MT_FRAME_RATE, RATE_NUM, RATE_DENOM));   
      CHK(MFSetAttributeRatio(spTypeIn.Get(), MF_MT_PIXEL_ASPECT_RATIO, ASPECT_NUM, ASPECT_DENOM));   

      CHK(spSinkWriter->SetInputMediaType(streamIndex, spTypeIn.Get(), nullptr));   

      //   
      //    
      //   

      CHK(spSinkWriter->BeginWriting());   

      double progress = 0.;
      LONGLONG hnsSampleTime = 0;   
      for (unsigned int nFrame = 0; nFrame < FRAME_NUM; nFrame++)   
      {   
        if (token.is_canceled())
        {
          break;
        }

        double newProgress = 100. * (double)nFrame / (double)FRAME_NUM;
        if (newProgress - progress >= 1.)
        {
          progress = newProgress;
          reporter.report(progress);
        }

        //   
        // Create a media sample   
        //   

        ComPtr<IMFSample> spSample;   
        CHK(MFCreateSample(&spSample));   
        CHK(spSample->SetSampleDuration(hnsSampleDuration));   
        CHK(spSample->SetSampleTime(hnsSampleTime));   
        hnsSampleTime += hnsSampleDuration;   

        //   
        // Add a media buffer filled with random data   
        //   

        ComPtr<IMFMediaBuffer> spBuffer;   
        CHK(MFCreateMemoryBuffer(cbMaxLength, &spBuffer));   
        CHK(spBuffer->SetCurrentLength(cbMaxLength));   
        CHK(spSample->AddBuffer(spBuffer.Get()));   

        // Draw a bouncing white rectangle over black background
        unsigned char *pbBuffer = nullptr;     
        CHK(spBuffer->Lock(&pbBuffer, nullptr, nullptr));   
        for (unsigned int i=0; i < HEIGHT; i++)
        {
          for (unsigned int j=0; j < WIDTH; j++)   
          {   
            unsigned int pos = 4 * (i * WIDTH + j);
            unsigned char val = 255 * (
              (abs((int)WIDTH / 2 - (int)j) < (WIDTH / 4)) && 
              (abs(HEIGHT * (.5 + .1 * sin(2. * M_PI * (double)nFrame / (double)ONE_SECOND)) - (int)i) < (HEIGHT / 4))
              );
            pbBuffer[pos  ] = val;
            pbBuffer[pos+1] = val;
            pbBuffer[pos+2] = val;
            pbBuffer[pos+3] = val;
          }
        }
        CHK(spBuffer->Unlock());   

        //   
        // Write the media sample   
        //   

        CHK(spSinkWriter->WriteSample(streamIndex, spSample.Get()));   
      }   

      if (!token.is_canceled())
      {
        CHK(spSinkWriter->Finalize());   

        reporter.report(100.);
      }
    });
  }

  //---------------------------------------------------------------------
  VideoWriter::VideoWriter(
        Windows::Storage::Streams::IRandomAccessStream^ stream
      )
  {


    stream_ = stream;

	SamplesPerSecond = 44100;
	AverageBytesPerSecond = 24000;
	ChannelCount = 2;
	BitsPerSample = 16;

	// 入力ストリームから SinkWriterを生成する

    CHK(MFCreateMFByteStreamOnStreamEx(reinterpret_cast<IUnknown *>(stream_), &byteStream_));
    CHK(MFCreateAttributes(&attr_, 10));
    CHK(attr_->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, true));
    CHK(attr_->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING , true));
	IMFSinkWriterPtr sinkWriter;

	CHK(MFCreateSinkWriterFromURL(L".mp4", byteStream_.Get(), attr_.Get(), &sinkWriter));
	CHK(sinkWriter.As(&sinkWriter_));

    //   
    // 出力メディアタイプのセットアップ   
    //   

	// ビデオ

	CHK(MFCreateMediaType(&mediaTypeOut_));   
    CHK(mediaTypeOut_->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));     
    CHK(mediaTypeOut_->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264));   
    //CHK(mediaTypeOut_->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32));   
    CHK(mediaTypeOut_->SetUINT32(MF_MT_AVG_BITRATE, BITRATE));   
    CHK(mediaTypeOut_->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));   
    CHK(MFSetAttributeSize(mediaTypeOut_.Get(), MF_MT_FRAME_SIZE, WIDTH, HEIGHT));   
    CHK(MFSetAttributeRatio(mediaTypeOut_.Get(), MF_MT_FRAME_RATE, RATE_NUM, RATE_DENOM));
    CHK(MFSetAttributeRatio(mediaTypeOut_.Get(), MF_MT_PIXEL_ASPECT_RATIO, ASPECT_NUM, ASPECT_DENOM));

    CHK(sinkWriter_->AddStream(mediaTypeOut_.Get(), &streamIndex_));

	// オーディオ
	CHK(MFCreateMediaType(&mediaTypeOutAudio_));
	CHK(mediaTypeOutAudio_->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
	CHK(mediaTypeOutAudio_->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC));
	CHK(mediaTypeOutAudio_->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, SamplesPerSecond));
	CHK(mediaTypeOutAudio_->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, BitsPerSample));
	CHK(mediaTypeOutAudio_->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, ChannelCount));
	CHK(mediaTypeOutAudio_->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, AverageBytesPerSecond));
	CHK(mediaTypeOutAudio_->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 1));
	CHK(sinkWriter_->AddStream(mediaTypeOutAudio_.Get(),&streamIndexAudio_));

    //   
    // 入力メディアタイプのセットアップ  
    //   

	// ビデオ
    
	CHK(MFCreateMediaType(&mediaTypeIn_));   
    CHK(mediaTypeIn_->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));   
    CHK(mediaTypeIn_->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32));     
    CHK(mediaTypeIn_->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));   
    CHK(MFSetAttributeSize(mediaTypeIn_.Get(), MF_MT_FRAME_SIZE, WIDTH, HEIGHT) );   
    CHK(MFSetAttributeRatio(mediaTypeIn_.Get(), MF_MT_FRAME_RATE, RATE_NUM, RATE_DENOM));   
    CHK(MFSetAttributeRatio(mediaTypeIn_.Get(), MF_MT_PIXEL_ASPECT_RATIO, ASPECT_NUM, ASPECT_DENOM));   

    CHK(sinkWriter_->SetInputMediaType(streamIndex_, mediaTypeIn_.Get(), nullptr));   

	// オーディオ

	CHK(MFCreateMediaType(&mediaTypeInAudio_));
	CHK(mediaTypeInAudio_->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
	CHK(mediaTypeInAudio_->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM));
	CHK(mediaTypeInAudio_->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, BitsPerSample));
	CHK(mediaTypeInAudio_->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, SamplesPerSecond));
	CHK(mediaTypeInAudio_->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, ChannelCount));

	CHK(sinkWriter_->SetInputMediaType(streamIndexAudio_, mediaTypeInAudio_.Get(), NULL));

    //   
    // 出力開始  
    //   

    CHK(sinkWriter_->BeginWriting());   

    //   
    // メディア・サンプルの作成   
    //   

    CHK(MFCreateSample(&sample_));   
    videoSampleTime_ = 0;
    CHK(sample_->SetSampleDuration(hnsSampleDuration));   

    //   
    // メディア・バッファの生成と、メディア・サンプルへの追加    
    //   

    CHK(MFCreateMemoryBuffer(cbMaxLength, &buffer_));   
    CHK(buffer_->SetCurrentLength(cbMaxLength));   
    CHK(sample_->AddBuffer(buffer_.Get()));   

  }

  // テクスチャをメディアバッファに書き込む
  void VideoWriter::SetTextureToSample(ID3D11DeviceContext1* context,ID3D11Texture2D* texture)
  {

    // タイムスタンプの設定
    CHK(sample_->SetSampleTime(videoSampleTime_));   
    
    // 書き込み先バッファのロック
    unsigned char *pbBuffer = nullptr;     
    CHK(buffer_->Lock(&pbBuffer, nullptr, nullptr));
    
    // 読み込みテクスチャをマップ
    D3D11_MAPPED_SUBRESOURCE mapped;
    CHK(context->Map(texture,0,D3D11_MAP_READ,0,&mapped));
    
    MFCopyImage(pbBuffer,WIDTH * 4,reinterpret_cast<BYTE*>(mapped.pData),mapped.RowPitch,WIDTH * 4,HEIGHT);

    // 書き込み先バッファのアンロック
    CHK(buffer_->Unlock());   
    // テクスチャをアンマップ
    context->Unmap(texture,0);
    videoSampleTime_ += hnsSampleDuration;   
  }

  void VideoWriter::WriteVideoSample()
  {
  
    CHK(sinkWriter_->WriteSample(streamIndex_, sample_.Get()));   
  }

  void VideoWriter::WriteAudioSample(IMFSample* sample)
  {
    CHK(sinkWriter_->WriteSample(streamIndexAudio_, sample));
    CHK(sample->GetSampleTime(&audioSampleTime_));
    dout(boost::wformat(L"%10x \n") % audioSampleTime_);
  }


  AudioReader::AudioReader(Windows::Storage::Streams::IRandomAccessStream^ stream) {

	  SamplesPerSecond = 44100;
	  AverageBytesPerSecond = 24000;
	  ChannelCount = 2;
	  BitsPerSample = 16;

	  IMFSourceReaderPtr reader;
	  //CHK(MFCreateSourceReaderFromURL(url->Data(), nullptr, reader.GetAddressOf()));

	  CHK(MFCreateMFByteStreamOnStreamEx(reinterpret_cast<IUnknown *>(stream), &byteStream_));
	  //CHK(MFCreateAttributes(&attr_, 10));
	  //CHK(attr_->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, true));
	  //IMFSinkWriterPtr sinkWriter;

	  CHK(MFCreateSourceReaderFromByteStream(byteStream_.Get(), nullptr, &reader));
	  

	  //CHK(MFCreateSinkWriterFromURL(L".mp4", byteStream_.Get(), attr_.Get(), &sinkWriter));

	  CHK(reader.As(&reader_));
	  CHK(reader_->GetNativeMediaType(0,0, &nativeMediaType_));
	  CHK(MFCreateMediaType(&currentMediaType_));
	  CHK(currentMediaType_->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
	  CHK(currentMediaType_->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM));
	  CHK(currentMediaType_->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, BitsPerSample));
	  CHK(currentMediaType_->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, SamplesPerSecond));
	  CHK(currentMediaType_->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, ChannelCount));
	  //DWORD blockAlign;
	  //CHK(nativeMediaType_->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &blockAlign));
	  //CHK(currentMediaType_->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT,blockAlign ));
	  CHK(reader_->SetCurrentMediaType(0, nullptr, currentMediaType_.Get()));
	  CHK(reader_->GetCurrentMediaType(0,currentMediaType_.ReleaseAndGetAddressOf()));
	  UINT32 blockAlign;
	  CHK(currentMediaType_->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &blockAlign));

	  DOUT(boost::wformat(L"Block Align: %10d %10x") % blockAlign % blockAlign);
    fileSize_ = stream->Size;

  }

  DWORD AudioReader::ReadSample(IMFSamplePtr& sample)
  {
	  DWORD streamIndex,flags;
	  CHK(reader_->ReadSample(0,0,&streamIndex,&flags,&videoSampleTime_,sample.ReleaseAndGetAddressOf()));
	  return flags;
  }
}




