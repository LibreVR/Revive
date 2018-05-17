#pragma once
#include <winrt/base.h>

WINRT_WARNING_PUSH
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Holographic.h>
#include <winrt/Windows.UI.Input.Spatial.h>

WINRT_EXPORT namespace winrt::Windows::Graphics::Holographic {
	struct IHolographicSpaceStatics4;
}

namespace winrt::impl {
	template <> struct guid<Windows::Graphics::Holographic::IHolographicSpaceStatics4> { static constexpr GUID value{ 0x5C4EE536,0x6A98,0x4B86,{ 0xA1,0x70,0x58,0x70,0x13,0xD6,0xFD,0x4B } }; };

	template <typename D>
	struct consume_Windows_Graphics_Holographic_IHolographicSpaceStatics4
	{
		Windows::Graphics::Holographic::HolographicSpace CreateForHWND(HWND window, const GUID * guid) const;
	};
	template <> struct consume<Windows::Graphics::Holographic::IHolographicSpaceStatics4> { template <typename D> using type = consume_Windows_Graphics_Holographic_IHolographicSpaceStatics4<D>; };

	template <> struct abi<Windows::Graphics::Holographic::IHolographicSpaceStatics4>{ struct type : ::IInspectable
	{
		virtual HRESULT __stdcall CreateForHWND(HWND window, const GUID * guid, void** value) = 0;
	};};

	template <typename D> Windows::Graphics::Holographic::HolographicSpace consume_Windows_Graphics_Holographic_IHolographicSpaceStatics4<D>::CreateForHWND(HWND window, const GUID * guid) const
	{
		Windows::Graphics::Holographic::HolographicSpace value{ nullptr };
		check_hresult(WINRT_SHIM(Windows::Graphics::Holographic::IHolographicSpaceStatics4)->CreateForHWND(window, guid, put_abi(value)));
		return value;
	}
}

WINRT_EXPORT namespace winrt::Windows::Graphics::Holographic {
	struct WINRT_EBO IHolographicSpaceStatics4 :
		Windows::Foundation::IInspectable,
		impl::consume_t<IHolographicSpaceStatics4>
	{
		IHolographicSpaceStatics4(std::nullptr_t = nullptr) noexcept {}
	};

	inline Windows::Graphics::Holographic::HolographicSpace CreateForHWND(HWND window)
	{
		return get_activation_factory<HolographicSpace, Windows::Graphics::Holographic::IHolographicSpaceStatics4>().CreateForHWND(window, &guid_of<IHolographicSpace>());
	}
}

WINRT_EXPORT namespace winrt::Windows::UI::Input::Spatial {
	struct ISpatialInteractionManagerStatics2;
}

namespace winrt::impl {
	template <> struct guid<Windows::UI::Input::Spatial::ISpatialInteractionManagerStatics2> { static constexpr GUID value{ 0x5C4EE536,0x6A98,0x4B86,{ 0xA1,0x70,0x58,0x70,0x13,0xD6,0xFD,0x4B } }; };

	template <typename D>
	struct consume_Windows_UI_Input_Spatial_ISpatialInteractionManagerStatics2
	{
		Windows::UI::Input::Spatial::SpatialInteractionManager GetForHWND(HWND window, const GUID * guid) const;
	};
	template <> struct consume<Windows::UI::Input::Spatial::ISpatialInteractionManagerStatics2> { template <typename D> using type = consume_Windows_UI_Input_Spatial_ISpatialInteractionManagerStatics2<D>; };

	template <> struct abi<Windows::UI::Input::Spatial::ISpatialInteractionManagerStatics2>{ struct type : ::IInspectable
	{
		virtual HRESULT __stdcall GetForHWND(HWND window, const GUID * guid, void** value) = 0;
	};};

	template <typename D> Windows::UI::Input::Spatial::SpatialInteractionManager consume_Windows_UI_Input_Spatial_ISpatialInteractionManagerStatics2<D>::GetForHWND(HWND window, const GUID * guid) const
	{
		Windows::UI::Input::Spatial::SpatialInteractionManager value{ nullptr };
		check_hresult(WINRT_SHIM(Windows::UI::Input::Spatial::ISpatialInteractionManagerStatics2)->GetForHWND(window, guid, put_abi(value)));
		return value;
	}
}

WINRT_EXPORT namespace winrt::Windows::UI::Input::Spatial {
	struct WINRT_EBO ISpatialInteractionManagerStatics2 :
		Windows::Foundation::IInspectable,
		impl::consume_t<ISpatialInteractionManagerStatics2>
	{
		ISpatialInteractionManagerStatics2(std::nullptr_t = nullptr) noexcept {}
	};

	inline Windows::UI::Input::Spatial::SpatialInteractionManager GetForHWND(HWND window)
	{
		return get_activation_factory<SpatialInteractionManager, Windows::UI::Input::Spatial::ISpatialInteractionManagerStatics2>().GetForHWND(window, &guid_of<SpatialInteractionManager>());
	}
}

WINRT_WARNING_POP
