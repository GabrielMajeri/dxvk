#include "d3d9_device.h"

#include "d3d9_caps.h"
#include "d3d9_format.h"
#include "d3d9_surface.h"

namespace dxvk {
  D3D9Device::D3D9Device(IDirect3D9* parent, D3D9Adapter& adapter,
    const D3DDEVICE_CREATION_PARAMETERS& cp, D3DPRESENT_PARAMETERS& pp)
    : m_parent(parent), m_creationParams(cp), m_adapter(adapter) {
    // Get a handle to the DXGI adapter.
    auto dxgiAdapter = m_adapter.GetAdapter();

    // We're supposed to use the device window for the back buffer,
    // or fall back to the focus window otherwise.
    const auto window = pp.hDeviceWindow ? pp.hDeviceWindow : cp.hFocusWindow;

    // TODO: use the focus window for something.
    // It is currently ignored.

    // Back buffer width and height.
    UINT width = pp.BackBufferWidth, height = pp.BackBufferWidth;

    // If either dimension is 0, we use the window to determine the dimensions.
    if (!width || !height) {
      RECT r{};

      GetClientRect(window, &r);

      width = r.right - r.left;
      height = r.bottom - r.top;

      pp.BackBufferWidth = width;
      pp.BackBufferHeight = height;
    }

    DXGI_RATIONAL refreshRate { 60, 1 };

    if (pp.Windowed || pp.FullScreen_RefreshRateInHz == 0) {
      // TODO: In windowed mode we must use the desktop's refresh rate.
    } else {
      refreshRate.Numerator = pp.FullScreen_RefreshRateInHz;
    }

    // TODO: store PresentationInterval to use it when calling swapchain->Present

    const DXGI_MODE_DESC mode {
      pp.BackBufferWidth,
      pp.BackBufferHeight,
      refreshRate,
      SurfaceFormatToDXGIFormat(pp.BackBufferFormat),
      DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
      DXGI_MODE_SCALING_UNSPECIFIED,
    };

    // TODO: support multisampling
    const auto msSamples = pp.MultiSampleType; // from 0 to 16
    const auto msQuality = pp.MultiSampleQuality; // Quality

    DXGI_SAMPLE_DESC samples {
      1,
      0,
    };

    if (pp.SwapEffect != D3DSWAPEFFECT_DISCARD) {
      Logger::warn("Multisampling is only supported when the swap effect is DISCARD");
      Logger::warn("Disabling multisampling");
      samples = { 1, 0 };
    }

    const auto usage = DXGI_USAGE_BACK_BUFFER | DXGI_USAGE_RENDER_TARGET_OUTPUT;

    const auto backBufferCount = std::min(pp.BackBufferCount, 1u);
    pp.BackBufferCount = backBufferCount;

    // TODO: DXVK only supports this swap effect, for now.
    const auto swapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const DXGI_SWAP_CHAIN_DESC scDesc {
      mode,
      samples,
      usage,
      backBufferCount,
      window,
      pp.Windowed,
      swapEffect,
      0,
    };

    const auto result = D3D11CreateDeviceAndSwapChain(dxgiAdapter,
      D3D_DRIVER_TYPE_UNKNOWN,
      nullptr,
      0,
      // TODO: determine which feature level we actually need.
      nullptr,
      0,
      D3D11_SDK_VERSION,
      &scDesc,
      &m_swapChain,
      &m_device,
      nullptr,
      &m_ctx
    );

    if (FAILED(result)) {
      Logger::err(str::format("D3D11CreateDeviceAndSwapChain failed: ", result));
      throw DxvkError("Failed to create D3D9 device");
    }

    // Create the default render target, and the corresponding depth buffer, if requested.
    // Retrieve the back buffer from the swap chain.
    Com<ID3D11Texture2D> backBuffer;
    m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);

    // Create a surface for the render target.
    const Com<D3D9Surface> surface = new D3D9Surface(this, backBuffer.ptr(), D3DUSAGE_RENDERTARGET);

    Com<ID3D11RenderTargetView> rtView;

    // Create the RT view.
    if (FAILED(m_device->CreateRenderTargetView(backBuffer.ptr(), nullptr, &rtView)))
      throw DxvkError("Failed to create render target");

    SetInterface(surface.ptr(), rtView.ref());

    m_renderTarget = surface.ptr();

    if (pp.EnableAutoDepthStencil) {
      // TODO: support auto creating the depth / stencil buffer.
      Logger::err("Automatically creating depth buffer not yet supported");
    }

    UpdateOMViews();
  }

  D3D9Device::~D3D9Device() = default;

  // Synchronises D3D9's views with the D3D11's Output Merger
  // render target & depth/stencil views.
  void D3D9Device::UpdateOMViews() {
    const auto renderTargetView = GetInterface<ID3D11RenderTargetView>(m_renderTarget.ptr());
    const auto depthStencilView = GetInterface<ID3D11DepthStencilView>(m_depthStencil.ptr());

    m_ctx->OMSetRenderTargets(1, &renderTargetView, depthStencilView);
  }

  HRESULT D3D9Device::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(IDirect3DDevice9)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D9Device::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT D3D9Device::GetDirect3D(IDirect3D9** ppD3D9) {
    InitReturnPtr(ppD3D9);
    CHECK_NOT_NULL(ppD3D9);

    *ppD3D9 = ref(m_parent);

    return D3D_OK;
  }

  HRESULT D3D9Device::GetDeviceCaps(D3DCAPS9* pCaps) {
    CHECK_NOT_NULL(pCaps);

    // The caps were not passed in by the constructor,
    // but they're the same for all devices anyway.
    FillCaps(m_creationParams.AdapterOrdinal, *pCaps);

    return D3D_OK;
  }

  HRESULT D3D9Device::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* pParameters) {
    CHECK_NOT_NULL(pParameters);

    *pParameters = m_creationParams;

    return D3D_OK;
  }

  HRESULT D3D9Device::TestCooperativeLevel() {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Device::Reset(D3DPRESENT_PARAMETERS *pPresentationParameters) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  UINT D3D9Device::GetAvailableTextureMem() {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Device::EvictManagedResources() {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }
}
