#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <cstdio>
#include <cstdlib>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

static const char *dxgiFormatName(DXGI_FORMAT fmt)
{
    switch (fmt)
    {
    case DXGI_FORMAT_B8G8R8A8_UNORM:
        return "B8G8R8A8_UNORM (BGRA)";
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        return "R8G8B8A8_UNORM (RGBA)";
    case DXGI_FORMAT_R10G10B10A2_UNORM:
        return "R10G10B10A2_UNORM";
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        return "R16G16B16A16_FLOAT";
    default:
        return "(other)";
    }
}

int main()
{
    printf("=== DXGI Desktop Duplication API 测试 ===\n\n");

    // 1. 创建 D3D11 设备
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    ID3D11Device *d3dDevice = nullptr;
    ID3D11DeviceContext *d3dContext = nullptr;
    D3D_FEATURE_LEVEL selectedLevel;

    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        featureLevels, 2, D3D11_SDK_VERSION,
        &d3dDevice, &selectedLevel, &d3dContext);

    if (FAILED(hr))
    {
        printf("[FAIL] D3D11CreateDevice 失败: HRESULT=0x%08lX\n", hr);
        printf("原因: 无 GPU 或驱动不支持 D3D11\n");
        return 1;
    }
    printf("[ OK ] D3D11 设备创建成功 (Feature Level: 0x%04X)\n", selectedLevel);

    // 2. 获取 DXGI 设备
    IDXGIDevice *dxgiDevice = nullptr;
    hr = d3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgiDevice);
    if (FAILED(hr))
    {
        printf("[FAIL] 获取 IDXGIDevice 失败: HRESULT=0x%08lX\n", hr);
        d3dContext->Release();
        d3dDevice->Release();
        return 1;
    }
    printf("[ OK ] IDXGIDevice 获取成功\n");

    // 3. 获取 DXGI 适配器
    IDXGIAdapter *adapter = nullptr;
    hr = dxgiDevice->GetAdapter(&adapter);
    dxgiDevice->Release();
    if (FAILED(hr))
    {
        printf("[FAIL] GetAdapter 失败: HRESULT=0x%08lX\n", hr);
        d3dContext->Release();
        d3dDevice->Release();
        return 1;
    }

    DXGI_ADAPTER_DESC adapterDesc;
    adapter->GetDesc(&adapterDesc);
    printf("[ OK ] 适配器: %S (VID:0x%04X PID:0x%04X)\n",
           adapterDesc.Description, adapterDesc.VendorId, adapterDesc.DeviceId);

    // 4. 枚举输出
    IDXGIOutput *output = nullptr;
    int outputCount = 0;
    for (UINT i = 0;; i++)
    {
        hr = adapter->EnumOutputs(i, &output);
        if (hr == DXGI_ERROR_NOT_FOUND)
            break;
        if (FAILED(hr))
        {
            printf("[WARN] EnumOutputs(%u) 失败: HRESULT=0x%08lX\n", i, hr);
            continue;
        }

        DXGI_OUTPUT_DESC desc;
        output->GetDesc(&desc);
        printf("[INFO] 输出 %u: %S (%d x %d) 旋转=%d 桌面坐标=(%ld,%ld)\n",
               i, desc.DeviceName,
               desc.DesktopCoordinates.right - desc.DesktopCoordinates.left,
               desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top,
               desc.Rotation,
               desc.DesktopCoordinates.left, desc.DesktopCoordinates.top);

        // 5. 尝试 Desktop Duplication
        IDXGIOutput1 *output1 = nullptr;
        hr = output->QueryInterface(__uuidof(IDXGIOutput1), (void **)&output1);
        if (FAILED(hr))
        {
            printf("[WARN] 输出 %u 不支持 IDXGIOutput1 (需要 Win8+)\n", i);
            output->Release();
            continue;
        }

        IDXGIOutputDuplication *dup = nullptr;
        hr = output1->DuplicateOutput(d3dDevice, &dup);
        output1->Release();

        if (hr == E_ACCESSDENIED)
        {
            printf("[WARN] 输出 %u: DuplicateOutput 被拒绝 (需要以非管理员身份运行，或桌面未激活)\n", i);
        }
        else if (hr == E_NOTIMPL)
        {
            printf("[WARN] 输出 %u: DuplicateOutput 不支持 (可能是基本显示驱动)\n", i);
        }
        else if (FAILED(hr))
        {
            printf("[WARN] 输出 %u: DuplicateOutput 失败: HRESULT=0x%08lX\n", i, hr);
        }
        else
        {
            printf("[ OK ] 输出 %u: IDXGIOutputDuplication 创建成功!\n", i);

            // 6. 尝试获取一帧
            IDXGIResource *frameResource = nullptr;
            DXGI_OUTDUPL_FRAME_INFO frameInfo;
            hr = dup->AcquireNextFrame(500, &frameInfo, &frameResource);

            if (hr == DXGI_ERROR_WAIT_TIMEOUT)
            {
                printf("[INFO] 输出 %u: AcquireNextFrame 超时 (桌面无变化，这是正常的)\n", i);
            }
            else if (hr == DXGI_ERROR_ACCESS_LOST)
            {
                printf("[WARN] 输出 %u: AcquireNextFrame 访问丢失 (可能发生了模式切换/UAC)\n", i);
            }
            else if (SUCCEEDED(hr))
            {
                printf("[ OK ] 输出 %u: 成功获取一帧!\n", i);
                printf("      帧号: %lld, 鼠标更新: %d, 脏矩形数: %d\n",
                       frameInfo.LastPresentTime.QuadPart,
                       frameInfo.LastMouseUpdateTime.QuadPart != 0,
                       frameInfo.TotalMetadataBufferSize > 0);

                // 获取帧描述
                D3D11_TEXTURE2D_DESC texDesc;
                ID3D11Texture2D *texture = nullptr;
                hr = frameResource->QueryInterface(__uuidof(ID3D11Texture2D), (void **)&texture);
                if (SUCCEEDED(hr))
                {
                    texture->GetDesc(&texDesc);
                    printf("      纹理: %u x %u, 格式=%s, MipLevels=%u, ArraySize=%u\n",
                           texDesc.Width, texDesc.Height,
                           dxgiFormatName(texDesc.Format),
                           texDesc.MipLevels, texDesc.ArraySize);
                    texture->Release();
                }

                dup->ReleaseFrame();
                frameResource->Release();
            }
            else
            {
                printf("[WARN] 输出 %u: AcquireNextFrame 失败: HRESULT=0x%08lX\n", i, hr);
                if (frameResource)
                    frameResource->Release();
            }

            dup->Release();
        }

        output->Release();
        outputCount++;
    }

    adapter->Release();
    d3dContext->Release();
    d3dDevice->Release();

    printf("\n=== 测试完成 ===\n");
    printf("共检测到 %d 个输出设备\n", outputCount);

    return 0;
}
