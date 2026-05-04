#include "../hpp/screenrecorder.hpp"
#include <QDebug>
#include <QDateTime>
#include <QCoreApplication>
#include <QDir>
#include <cmath>

ScreenRecorder::ScreenRecorder(QObject *parent)
    : QObject(parent), m_recordState(Stopped), m_recordThread(nullptr), m_recordingTimer(), m_recordingSeconds(0), m_currentFileName(""), m_outputDir(""), m_captureBackend(Backend_None), m_inputFmtCtx(nullptr), m_decoderCtx(nullptr), m_inputVideoStreamIdx(-1), m_d3dDevice(nullptr), m_d3dContext(nullptr), m_dxgiDup(nullptr), m_dxgiStagingTex(nullptr), m_dxgiOutputWidth(0), m_dxgiOutputHeight(0), m_filterGraph(nullptr), m_bufferSrcCtx(nullptr), m_bufferSinkCtx(nullptr), m_outputFmtCtx(nullptr), m_encoderCtx(nullptr), m_outputStream(nullptr), m_frameCount(0), m_frame(nullptr), m_filteredFrame(nullptr), m_packet(nullptr), m_encPkt(nullptr)
{
    qDebug() << "ScreenRecorder初始化完成";
}

ScreenRecorder::~ScreenRecorder()
{
    stopRecording();
    cleanup();
    qDebug() << "ScreenRecorder析构完成";
}

bool ScreenRecorder::initialize()
{
    qDebug() << "初始化录制环境";

    avdevice_register_all();

    qDebug() << "avdevice注册完成";
    return true;
}

void ScreenRecorder::setOutputDir(const QString &dir)
{
    m_outputDir = dir;
    qDebug() << "设置输出目录:" << m_outputDir;
}

bool ScreenRecorder::startRecording()
{
    qDebug() << "开始录制";

    if (m_recordState.load() != Stopped)
    {
        qWarning() << "录制状态不正确，无法开始录制";
        return false;
    }

    m_currentFileName = generateFileName();

    if (!openInput())
    {
        qWarning() << "打开输入设备失败";
        setRecordState(Error);
        emit recordError("无法打开屏幕捕获设备");
        return false;
    }

    if (!createOutput())
    {
        qWarning() << "创建输出文件失败";
        closeInput();
        setRecordState(Error);
        emit recordError("无法创建输出文件");
        return false;
    }

    if (!createFilterGraph())
    {
        qWarning() << "创建滤镜图失败";
        closeOutput();
        closeInput();
        setRecordState(Error);
        emit recordError("无法创建滤镜图");
        return false;
    }

    m_frame = av_frame_alloc();
    m_filteredFrame = av_frame_alloc();
    m_packet = av_packet_alloc();
    m_encPkt = av_packet_alloc();
    m_frameCount = 0;

    setRecordState(Recording);
    m_recordingSeconds = 0;
    m_recordingTimer.start();

    m_recordThread = QThread::create([this]()
                                     { recordingLoop(); });

    m_recordThread->start();

    emit recordingTimeUpdated(m_recordingSeconds);

    qDebug() << "录制已开始，文件:" << m_currentFileName;
    return true;
}

void ScreenRecorder::stopRecording()
{
    qDebug() << "停止录制";

    if (m_recordState.load() != Recording)
    {
        return;
    }

    m_recordState.store(Stopped);

    if (m_recordThread)
    {
        m_recordThread->quit();
        m_recordThread->wait();
        delete m_recordThread;
        m_recordThread = nullptr;
    }

    closeOutput();
    closeInput();

    av_frame_free(&m_frame);
    av_frame_free(&m_filteredFrame);
    av_packet_free(&m_packet);
    av_packet_free(&m_encPkt);

    if (m_filterGraph)
    {
        avfilter_graph_free(&m_filterGraph);
        m_filterGraph = nullptr;
        m_bufferSrcCtx = nullptr;
        m_bufferSinkCtx = nullptr;
    }

    emit recordStateChanged(Stopped);

    qDebug() << "录制停止，文件:" << m_currentFileName;
}

ScreenRecorder::RecordState ScreenRecorder::getRecordState() const
{
    return static_cast<RecordState>(m_recordState.load());
}

int ScreenRecorder::getRecordingTime() const
{
    return m_recordingSeconds;
}

QString ScreenRecorder::getCurrentFileName() const
{
    return m_currentFileName;
}

bool ScreenRecorder::openInput()
{
    qDebug() << "打开输入设备（ddagrab → dxgi → gdigrab）";

    {
        const AVInputFormat *inputFormat = av_find_input_format("ddagrab");
        if (inputFormat)
        {
            m_inputFmtCtx = avformat_alloc_context();
            AVDictionary *options = nullptr;
            av_dict_set(&options, "offset_x", "0", 0);
            av_dict_set(&options, "offset_y", "0", 0);
            av_dict_set(&options, "video_size", "800x600", 0);
            av_dict_set(&options, "framerate", "20", 0);

            int ret = avformat_open_input(&m_inputFmtCtx, "desktop", inputFormat, &options);
            av_dict_free(&options);

            if (ret >= 0)
            {
                m_captureBackend = Backend_Ddagrab;
                qDebug() << "使用 FFmpeg ddagrab 设备";
                goto setup_ffmpeg_input;
            }

            char errBuf[AV_ERROR_MAX_STRING_SIZE] = {0};
            av_strerror(ret, errBuf, sizeof(errBuf));
            qWarning() << "ddagrab 打开失败:" << errBuf;
            avformat_free_context(m_inputFmtCtx);
            m_inputFmtCtx = nullptr;
        }
        else
        {
            qDebug() << "ddagrab 输入格式不可用";
        }
    }

    qDebug() << "尝试原生 DXGI Desktop Duplication";
    if (openDxgi())
    {
        m_captureBackend = Backend_Dxgi;
        qDebug() << "DXGI Desktop Duplication 打开成功";
        return true;
    }

    {
        const AVInputFormat *inputFormat = av_find_input_format("gdigrab");
        if (inputFormat)
        {
            m_inputFmtCtx = avformat_alloc_context();
            AVDictionary *options = nullptr;
            av_dict_set(&options, "offset_x", "0", 0);
            av_dict_set(&options, "offset_y", "0", 0);
            av_dict_set(&options, "video_size", "800x600", 0);
            av_dict_set(&options, "framerate", "20", 0);

            int ret = avformat_open_input(&m_inputFmtCtx, "desktop", inputFormat, &options);
            av_dict_free(&options);

            if (ret >= 0)
            {
                m_captureBackend = Backend_Gdigrab;
                qDebug() << "使用 FFmpeg gdigrab 设备";
                goto setup_ffmpeg_input;
            }

            char errBuf[AV_ERROR_MAX_STRING_SIZE] = {0};
            av_strerror(ret, errBuf, sizeof(errBuf));
            qWarning() << "gdigrab 打开失败:" << errBuf;
            avformat_free_context(m_inputFmtCtx);
            m_inputFmtCtx = nullptr;
        }
        else
        {
            qDebug() << "gdigrab 输入格式不可用";
        }
    }

    qWarning() << "所有捕获后端均不可用";
    return false;

setup_ffmpeg_input:
{
    int ret = avformat_find_stream_info(m_inputFmtCtx, nullptr);
    if (ret < 0)
    {
        qWarning() << "无法获取流信息";
        return false;
    }

    m_inputVideoStreamIdx = av_find_best_stream(m_inputFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (m_inputVideoStreamIdx < 0)
    {
        qWarning() << "找不到视频流";
        return false;
    }

    AVStream *inStream = m_inputFmtCtx->streams[m_inputVideoStreamIdx];
    AVCodecParameters *codecPar = inStream->codecpar;

    qDebug() << "输入视频流:"
             << codecPar->width << "x" << codecPar->height
             << "pix_fmt:" << codecPar->format
             << "codec_id:" << codecPar->codec_id;

    const AVCodec *decoder = avcodec_find_decoder(codecPar->codec_id);
    if (!decoder)
    {
        qWarning() << "找不到原始视频解码器";
        return false;
    }

    m_decoderCtx = avcodec_alloc_context3(decoder);
    if (!m_decoderCtx)
    {
        qWarning() << "无法分配解码器上下文";
        return false;
    }

    ret = avcodec_parameters_to_context(m_decoderCtx, codecPar);
    if (ret < 0)
    {
        qWarning() << "无法复制解码器参数";
        return false;
    }

    ret = avcodec_open2(m_decoderCtx, decoder, nullptr);
    if (ret < 0)
    {
        char errBuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, errBuf, sizeof(errBuf));
        qWarning() << "无法打开解码器:" << errBuf;
        return false;
    }

    qDebug() << "FFmpeg 输入设备打开成功";
    return true;
}
}

static uint8_t halfToByte(uint16_t h)
{
    int exp = (h >> 10) & 0x1F;
    int mant = h & 0x3FF;
    if (exp == 0)
        return 0;
    if (exp == 31)
        return (mant == 0) ? 255 : 0;
    float value = ldexpf((float)(mant | 0x400), exp - 15 - 10);
    int b = (int)(value * 255.0f + 0.5f);
    if (b < 0)
        return 0;
    if (b > 255)
        return 255;
    return (uint8_t)b;
}

bool ScreenRecorder::openDxgi()
{
    qDebug() << "尝试打开 DXGI Desktop Duplication";

    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &m_d3dDevice, nullptr, &m_d3dContext);

    if (FAILED(hr))
    {
        qWarning() << "D3D11CreateDevice 失败: 0x" << Qt::hex << (unsigned long)hr;
        return false;
    }

    IDXGIDevice *dxgiDevice = nullptr;
    hr = m_d3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgiDevice);
    if (FAILED(hr))
    {
        qWarning() << "获取 IDXGIDevice 失败";
        closeDxgi();
        return false;
    }

    IDXGIAdapter *adapter = nullptr;
    hr = dxgiDevice->GetAdapter(&adapter);
    dxgiDevice->Release();
    if (FAILED(hr))
    {
        qWarning() << "获取 IDXGIAdapter 失败";
        closeDxgi();
        return false;
    }

    IDXGIOutput *output = nullptr;
    hr = adapter->EnumOutputs(0, &output);
    adapter->Release();
    if (FAILED(hr))
    {
        qWarning() << "EnumOutputs(0) 失败";
        closeDxgi();
        return false;
    }

    IDXGIOutput1 *output1 = nullptr;
    hr = output->QueryInterface(__uuidof(IDXGIOutput1), (void **)&output1);
    output->Release();
    if (FAILED(hr))
    {
        qWarning() << "获取 IDXGIOutput1 失败（需要 Windows 8+）";
        closeDxgi();
        return false;
    }

    hr = output1->DuplicateOutput(m_d3dDevice, &m_dxgiDup);
    output1->Release();
    if (FAILED(hr))
    {
        qWarning() << "DuplicateOutput 失败: 0x" << Qt::hex << (unsigned long)hr;
        closeDxgi();
        return false;
    }

    DXGI_OUTDUPL_DESC dupDesc;
    m_dxgiDup->GetDesc(&dupDesc);
    m_dxgiOutputWidth = dupDesc.ModeDesc.Width;
    m_dxgiOutputHeight = dupDesc.ModeDesc.Height;

    qDebug() << "DXGI 输出:" << m_dxgiOutputWidth << "x" << m_dxgiOutputHeight
             << "格式:" << dupDesc.ModeDesc.Format;

    D3D11_TEXTURE2D_DESC stagingDesc = {};
    stagingDesc.Width = m_dxgiOutputWidth;
    stagingDesc.Height = m_dxgiOutputHeight;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = dupDesc.ModeDesc.Format;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    hr = m_d3dDevice->CreateTexture2D(&stagingDesc, nullptr, &m_dxgiStagingTex);
    if (FAILED(hr))
    {
        qWarning() << "创建暂存纹理失败";
        closeDxgi();
        return false;
    }

    return true;
}

void ScreenRecorder::closeDxgi()
{
    if (m_dxgiStagingTex)
    {
        m_dxgiStagingTex->Release();
        m_dxgiStagingTex = nullptr;
    }
    if (m_dxgiDup)
    {
        m_dxgiDup->Release();
        m_dxgiDup = nullptr;
    }
    if (m_d3dContext)
    {
        m_d3dContext->Release();
        m_d3dContext = nullptr;
    }
    if (m_d3dDevice)
    {
        m_d3dDevice->Release();
        m_d3dDevice = nullptr;
    }
    m_dxgiOutputWidth = 0;
    m_dxgiOutputHeight = 0;
}

bool ScreenRecorder::captureDxgiFrame(AVFrame *frame)
{
    if (!m_dxgiDup || !m_d3dContext || !m_dxgiStagingTex)
        return false;

    IDXGIResource *frameResource = nullptr;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    HRESULT hr = m_dxgiDup->AcquireNextFrame(16, &frameInfo, &frameResource);

    if (hr == DXGI_ERROR_WAIT_TIMEOUT)
        return false;

    if (hr == DXGI_ERROR_ACCESS_LOST)
    {
        qWarning() << "DXGI 访问丢失，尝试重新创建";
        closeDxgi();
        openDxgi();
        return false;
    }

    if (FAILED(hr))
        return false;

    ID3D11Texture2D *srcTex = nullptr;
    hr = frameResource->QueryInterface(__uuidof(ID3D11Texture2D), (void **)&srcTex);
    if (FAILED(hr))
    {
        m_dxgiDup->ReleaseFrame();
        frameResource->Release();
        return false;
    }

    m_d3dContext->CopyResource(m_dxgiStagingTex, srcTex);
    srcTex->Release();

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = m_d3dContext->Map(m_dxgiStagingTex, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr))
    {
        m_dxgiDup->ReleaseFrame();
        frameResource->Release();
        return false;
    }

    int outW = 800;
    int outH = 600;
    float scaleX = (float)m_dxgiOutputWidth / (float)outW;
    float scaleY = (float)m_dxgiOutputHeight / (float)outH;

    frame->format = AV_PIX_FMT_BGRA;
    frame->width = outW;
    frame->height = outH;
    av_frame_get_buffer(frame, 0);

    const uint8_t *srcRow = (const uint8_t *)mapped.pData;
    uint8_t *dstRow = frame->data[0];

    for (int y = 0; y < outH; y++)
    {
        int srcY = (int)((float)y * scaleY);
        if (srcY >= (int)m_dxgiOutputHeight)
            srcY = (int)m_dxgiOutputHeight - 1;

        const uint16_t *srcPixel = (const uint16_t *)(srcRow + srcY * mapped.RowPitch);
        uint8_t *dstPixel = dstRow + y * frame->linesize[0];

        for (int x = 0; x < outW; x++)
        {
            int srcX = (int)((float)x * scaleX);
            if (srcX >= (int)m_dxgiOutputWidth)
                srcX = (int)m_dxgiOutputWidth - 1;

            const uint16_t *p = srcPixel + srcX * 4;
            dstPixel[x * 4 + 0] = halfToByte(p[2]); // B
            dstPixel[x * 4 + 1] = halfToByte(p[1]); // G
            dstPixel[x * 4 + 2] = halfToByte(p[0]); // R
            dstPixel[x * 4 + 3] = halfToByte(p[3]); // A
        }
    }

    m_d3dContext->Unmap(m_dxgiStagingTex, 0);
    m_dxgiDup->ReleaseFrame();
    frameResource->Release();

    return true;
}

bool ScreenRecorder::createOutput()
{
    qDebug() << "创建输出文件";

    QString outputPath = m_outputDir + "/" + m_currentFileName;

    int ret = avformat_alloc_output_context2(&m_outputFmtCtx, nullptr, "matroska", outputPath.toUtf8().constData());
    if (ret < 0 || !m_outputFmtCtx)
    {
        char errBuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, errBuf, sizeof(errBuf));
        qWarning() << "无法创建输出格式上下文:" << errBuf;
        return false;
    }

    const AVCodec *encoder = avcodec_find_encoder_by_name("ffv1");
    if (!encoder)
    {
        qWarning() << "找不到FFV1编码器";
        return false;
    }

    m_encoderCtx = avcodec_alloc_context3(encoder);
    if (!m_encoderCtx)
    {
        qWarning() << "无法分配编码器上下文";
        return false;
    }

    m_encoderCtx->width = 800;
    m_encoderCtx->height = 600;
    m_encoderCtx->pix_fmt = AV_PIX_FMT_BGR0;
    m_encoderCtx->time_base = AVRational{1, 20};
    m_encoderCtx->framerate = AVRational{20, 1};

    av_opt_set_int(m_encoderCtx->priv_data, "coder", 1, 0);
    av_opt_set_int(m_encoderCtx->priv_data, "level", 3, 0);

    ret = avcodec_open2(m_encoderCtx, encoder, nullptr);
    if (ret < 0)
    {
        char errBuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, errBuf, sizeof(errBuf));
        qWarning() << "无法打开FFV1编码器:" << errBuf;
        return false;
    }

    m_outputStream = avformat_new_stream(m_outputFmtCtx, nullptr);
    if (!m_outputStream)
    {
        qWarning() << "无法创建输出流";
        return false;
    }

    ret = avcodec_parameters_from_context(m_outputStream->codecpar, m_encoderCtx);
    if (ret < 0)
    {
        qWarning() << "无法从编码器复制参数";
        return false;
    }

    m_outputStream->time_base = m_encoderCtx->time_base;

    if (!(m_outputFmtCtx->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&m_outputFmtCtx->pb, outputPath.toUtf8().constData(), AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            char errBuf[AV_ERROR_MAX_STRING_SIZE] = {0};
            av_strerror(ret, errBuf, sizeof(errBuf));
            qWarning() << "无法打开输出文件:" << errBuf;
            return false;
        }
    }

    ret = avformat_write_header(m_outputFmtCtx, nullptr);
    if (ret < 0)
    {
        char errBuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, errBuf, sizeof(errBuf));
        qWarning() << "无法写入文件头:" << errBuf;
        return false;
    }

    qDebug() << "输出文件创建成功:" << outputPath;
    return true;
}

bool ScreenRecorder::createFilterGraph()
{
    qDebug() << "创建滤镜图";

    m_filterGraph = avfilter_graph_alloc();
    if (!m_filterGraph)
    {
        qWarning() << "无法分配滤镜图";
        return false;
    }

    const AVFilter *bufferSrc = avfilter_get_by_name("buffer");
    if (!bufferSrc)
    {
        qWarning() << "找不到buffer源滤镜";
        return false;
    }

    char args[512];
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             800, 600, AV_PIX_FMT_BGRA, 1, 20, 1, 1);

    int ret = avfilter_graph_create_filter(&m_bufferSrcCtx, bufferSrc, "in", args, nullptr, m_filterGraph);
    if (ret < 0)
    {
        qWarning() << "无法创建buffer源滤镜";
        return false;
    }

    const AVFilter *formatFilter = avfilter_get_by_name("format");
    if (!formatFilter)
    {
        qWarning() << "找不到format滤镜";
        return false;
    }

    AVFilterContext *formatCtx = nullptr;
    ret = avfilter_graph_create_filter(&formatCtx, formatFilter, "fmt", "pix_fmts=bgr0", nullptr, m_filterGraph);
    if (ret < 0)
    {
        qWarning() << "无法创建format滤镜";
        return false;
    }

    const AVFilter *bufferSink = avfilter_get_by_name("buffersink");
    if (!bufferSink)
    {
        qWarning() << "找不到buffersink滤镜";
        return false;
    }

    ret = avfilter_graph_create_filter(&m_bufferSinkCtx, bufferSink, "out", nullptr, nullptr, m_filterGraph);
    if (ret < 0)
    {
        qWarning() << "无法创建buffersink滤镜";
        return false;
    }

    ret = avfilter_link(m_bufferSrcCtx, 0, formatCtx, 0);
    if (ret < 0)
    {
        qWarning() << "无法链接buffer→format";
        return false;
    }

    ret = avfilter_link(formatCtx, 0, m_bufferSinkCtx, 0);
    if (ret < 0)
    {
        qWarning() << "无法链接format→buffersink";
        return false;
    }

    ret = avfilter_graph_config(m_filterGraph, nullptr);
    if (ret < 0)
    {
        qWarning() << "无法配置滤镜图";
        return false;
    }

    qDebug() << "滤镜图创建成功: buffer(bgra)→format(bgr0)→buffersink";
    return true;
}

void ScreenRecorder::recordingLoop()
{
    qDebug() << "录制线程开始 (后端:" << (int)m_captureBackend << ")";

    int lastEmittedSecond = 0;
    bool useDxgi = (m_captureBackend == Backend_Dxgi);

    while (m_recordState.load() == Recording)
    {
        qint64 frameStartMs = m_recordingTimer.elapsed();

        if (useDxgi)
        {
            if (!captureDxgiFrame(m_frame))
            {
                qint64 frameElapsedMs = m_recordingTimer.elapsed() - frameStartMs;
                qint64 sleepMs = 16 - frameElapsedMs;
                if (sleepMs > 0)
                    QThread::msleep(sleepMs);
                continue;
            }
        }
        else
        {
            int ret = av_read_frame(m_inputFmtCtx, m_packet);
            if (ret < 0)
            {
                if (ret == AVERROR_EOF)
                {
                    qDebug() << "输入流结束";
                }
                else
                {
                    char errBuf[AV_ERROR_MAX_STRING_SIZE] = {0};
                    av_strerror(ret, errBuf, sizeof(errBuf));
                    qWarning() << "读取帧失败:" << errBuf;
                }
                break;
            }

            if (m_packet->stream_index != m_inputVideoStreamIdx)
            {
                av_packet_unref(m_packet);
                continue;
            }

            ret = avcodec_send_packet(m_decoderCtx, m_packet);
            av_packet_unref(m_packet);

            if (ret < 0)
            {
                qWarning() << "发送包到解码器失败";
                continue;
            }

            ret = avcodec_receive_frame(m_decoderCtx, m_frame);
            if (ret < 0)
            {
                if (ret != AVERROR(EAGAIN))
                {
                    qWarning() << "从解码器接收帧失败";
                }
                continue;
            }
        }

        int ret = av_buffersrc_add_frame_flags(m_bufferSrcCtx, m_frame, AV_BUFFERSRC_FLAG_KEEP_REF);
        if (ret < 0)
        {
            qWarning() << "推送帧到滤镜图失败";
            av_frame_unref(m_frame);
            continue;
        }

        ret = av_buffersink_get_frame(m_bufferSinkCtx, m_filteredFrame);
        if (ret < 0)
        {
            qWarning() << "从滤镜图获取帧失败";
            av_frame_unref(m_filteredFrame);
            av_frame_unref(m_frame);
            continue;
        }

        m_filteredFrame->pts = m_frameCount++;

        ret = avcodec_send_frame(m_encoderCtx, m_filteredFrame);
        av_frame_unref(m_filteredFrame);
        av_frame_unref(m_frame);

        if (ret < 0)
        {
            qWarning() << "发送帧到编码器失败";
            continue;
        }

        while (avcodec_receive_packet(m_encoderCtx, m_encPkt) == 0)
        {
            av_packet_rescale_ts(m_encPkt, m_encoderCtx->time_base, m_outputStream->time_base);
            m_encPkt->stream_index = m_outputStream->index;
            av_interleaved_write_frame(m_outputFmtCtx, m_encPkt);
            av_packet_unref(m_encPkt);
        }

        qint64 elapsedMs = m_recordingTimer.elapsed();
        int currentSecond = static_cast<int>(elapsedMs / 1000);

        if (currentSecond > lastEmittedSecond)
        {
            lastEmittedSecond = currentSecond;
            m_recordingSeconds = currentSecond;
            emit recordingTimeUpdated(currentSecond);

            if (currentSecond % 5 == 0)
            {
                qDebug() << "录制中..." << currentSecond << "秒, 帧数:" << m_frameCount;
            }
        }

        qint64 frameElapsedMs = m_recordingTimer.elapsed() - frameStartMs;
        qint64 sleepMs = 50 - frameElapsedMs;
        if (sleepMs > 0)
        {
            QThread::msleep(sleepMs);
        }
    }

    qDebug() << "录制线程结束，总帧数:" << m_frameCount;
}

void ScreenRecorder::closeInput()
{
    qDebug() << "关闭输入设备 (后端:" << (int)m_captureBackend << ")";

    if (m_captureBackend == Backend_Dxgi)
    {
        closeDxgi();
    }
    else
    {
        if (m_decoderCtx)
        {
            avcodec_free_context(&m_decoderCtx);
            m_decoderCtx = nullptr;
        }

        if (m_inputFmtCtx)
        {
            avformat_close_input(&m_inputFmtCtx);
            m_inputFmtCtx = nullptr;
        }

        m_inputVideoStreamIdx = -1;
    }

    m_captureBackend = Backend_None;
}

void ScreenRecorder::closeOutput()
{
    qDebug() << "关闭输出文件";

    if (m_encoderCtx)
    {
        avcodec_send_frame(m_encoderCtx, nullptr);

        while (avcodec_receive_packet(m_encoderCtx, m_encPkt) == 0)
        {
            av_packet_rescale_ts(m_encPkt, m_encoderCtx->time_base, m_outputStream->time_base);
            m_encPkt->stream_index = m_outputStream->index;
            av_interleaved_write_frame(m_outputFmtCtx, m_encPkt);
            av_packet_unref(m_encPkt);
        }
    }

    if (m_outputFmtCtx)
    {
        av_write_trailer(m_outputFmtCtx);

        if (!(m_outputFmtCtx->oformat->flags & AVFMT_NOFILE))
        {
            avio_closep(&m_outputFmtCtx->pb);
        }

        avformat_free_context(m_outputFmtCtx);
        m_outputFmtCtx = nullptr;
    }

    if (m_encoderCtx)
    {
        avcodec_free_context(&m_encoderCtx);
        m_encoderCtx = nullptr;
    }

    m_outputStream = nullptr;
    m_frameCount = 0;
}

void ScreenRecorder::cleanup()
{
    qDebug() << "清理录制资源";

    if (m_recordThread)
    {
        if (m_recordThread->isRunning())
        {
            m_recordThread->quit();
            m_recordThread->wait();
        }
        delete m_recordThread;
        m_recordThread = nullptr;
    }
}

void ScreenRecorder::setRecordState(RecordState state)
{
    int oldState = m_recordState.exchange(static_cast<int>(state));

    if (oldState != static_cast<int>(state))
    {
        qDebug() << "录制状态改变:" << oldState << "->" << state;
        emit recordStateChanged(state);
    }
}

QString ScreenRecorder::generateFileName() const
{
    QDateTime currentTime = QDateTime::currentDateTime();
    QString fileName = currentTime.toString("MMdd-hh-mm-ss-zzz");
    fileName += ".mkv";

    qDebug() << "生成文件名:" << fileName;

    return fileName;
}
