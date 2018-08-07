#include "d3d9_adapter.h"

#include <cstring>

namespace dxvk {
  // Use the first output of an adapter.
  static IDXGIOutput* getFirstOutput(IDXGIAdapter* adapter) {
    IDXGIOutput* output;
    if (FAILED(adapter->EnumOutputs(0, &output)))
      throw DxvkError("No monitors attached to adapter");
    return output;
  }

  // Retrieve the HMONITOR of an output.
  static HMONITOR getOutputMonitor(IDXGIOutput* output) {
    DXGI_OUTPUT_DESC desc{};
    if (FAILED(output->GetDesc(&desc)))
      throw DxvkError("Failed to retrieve output HMONITOR");

    return desc.Monitor;
  }

  // Cache the supported display modes for later.
  static std::vector<DXGI_MODE_DESC> getOutputModes(IDXGIOutput* output) {
    // Note: we just use a common format, and assume the same modes for other formats.
    UINT count = 0;
    output->GetDisplayModeList(DXGI_FORMAT_B8G8R8A8_UNORM, 0, &count, nullptr);

    std::vector<DXGI_MODE_DESC> modes(count);

    if (FAILED(output->GetDisplayModeList(DXGI_FORMAT_B8G8R8A8_UNORM, 0, &count, modes.data())))
      throw DxvkError("Failed to get display mode list");

    return modes;
  }

  D3D9Adapter::D3D9Adapter(Com<IDXGIAdapter1>&& adapter)
    : m_adapter(adapter),
      m_output(getFirstOutput(m_adapter.ptr())),
      m_monitor(getOutputMonitor(m_output.ptr())),
      m_modes(getOutputModes(m_output.ptr())) {
  }

  HRESULT D3D9Adapter::GetIdentifier(D3DADAPTER_IDENTIFIER9& ident) {
    DXGI_ADAPTER_DESC1 desc;

    if (FAILED(m_adapter->GetDesc1(&desc))) {
      Logger::err("Failed to retrieve adapter description");
      return D3DERR_INVALIDCALL;
    }

    // Zero out the memory.
    ident = {};

    // DXVK Adapter's description is simply the Vulkan device name.
    const auto name = str::fromws(desc.Description);
    // This is what game GUIs usually display.
    const auto description = str::format(name, " (D3D9 DXVK Driver)");
    const auto driver = "DXVK";

    std::strcpy(ident.DeviceName, name.data());
    std::strcpy(ident.Description, description.data());
    std::strcpy(ident.Driver, driver);

    ident.DriverVersion.QuadPart = 1;

    ident.VendorId = desc.VendorId;
    ident.DeviceId = desc.DeviceId;
    ident.SubSysId = desc.SubSysId;
    ident.Revision = desc.Revision;

    // LUID is only 64-bit long, but better something than nothing.
    std::memcpy(&ident.DeviceIdentifier, &desc.AdapterLuid, sizeof(LUID));

    // Just claim we're a validated driver.
    ident.WHQLLevel = 1;

    return D3D_OK;
  }

  UINT D3D9Adapter::GetModeCount() const {
    return m_modes.size();
  }

  void D3D9Adapter::GetMode(UINT index, D3DDISPLAYMODE& mode) const {
    const auto& desc = m_modes[index];

    // DXVK always returns refresh rate as ((refresh rate * 1000) / 1000).
    const auto refresh = desc.RefreshRate.Numerator / desc.RefreshRate.Denominator;

    mode.Width = desc.Width;
    mode.Height = desc.Height;
    mode.RefreshRate = refresh;
  }
}