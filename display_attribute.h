#pragma once
#define WIN32_LEAN_AND_MEAN
#include <atlbase.h>
#include <msctf.h>
#include <windows.h>
#include "globals.h"


// 未確定文字列を下線表示するための最小実装 文字装飾をTSFに伝えるためのクラス
class CDisplayAttributeInfo : public ITfDisplayAttributeInfo {
public:
	CDisplayAttributeInfo();
	~CDisplayAttributeInfo();

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	// ITfDisplayAttributeInfo
	STDMETHODIMP GetGUID(GUID* pguid);
	STDMETHODIMP GetDescription(BSTR* pbstrDesc);
	STDMETHODIMP GetAttributeInfo(TF_DISPLAYATTRIBUTE* pda);
	STDMETHODIMP SetAttributeInfo(const TF_DISPLAYATTRIBUTE* pda);
	STDMETHODIMP Reset();

private:
	LONG _refCount;
	//TF_DISPLAYATTRIBUTE _attribute;
};

// GUID_DisplayAttributeInputの1件のみを列挙する最小実装 
// ITfDisplayAttributeInfoを返すためのクラス
class CEnumDisplayAttributeInfo : public IEnumTfDisplayAttributeInfo {
public:
	CEnumDisplayAttributeInfo();
	~CEnumDisplayAttributeInfo();

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	// IEnumTfDisplayAttributeInfo
	STDMETHODIMP Clone(IEnumTfDisplayAttributeInfo** ppEnum);
	STDMETHODIMP Next(ULONG ulCount, ITfDisplayAttributeInfo** rgInfo, ULONG* pcFetched);
	STDMETHODIMP Reset();
	STDMETHODIMP Skip(ULONG ulCount);

private:
	LONG _refCount;
	ULONG _index; // 次に返す要素の位置(0か1)
};
