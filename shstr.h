// shstr.h --- The shell string classes
// Author: katahiromz
// License: MIT

#pragma once

#ifndef SHSTR_BUFF_SIZE
    #define SHSTR_BUFF_SIZE 128
#endif

class ShStrA
{
public:
    ShStrA()
    {
        m_szBuff[0] = ANSI_NULL;
        m_pch = m_szBuff;
        m_cchBuff = _countof(m_szBuff) - 1;
    }

    ~ShStrA()
    {
        Reset();
    }

    operator LPSTR() { return m_pch; }
    INT GetBuffLength() const { return m_cchBuff; }

    void Reset()
    {
        if (m_pch && m_cchBuff != _countof(m_szBuff) - 1)
            LocalFree(m_pch);
        m_szBuff[0] = ANSI_NULL;
        m_pch = m_szBuff;
        m_cchBuff = _countof(m_szBuff) - 1;
    }

    HRESULT SetSize(UINT cchMax)
    {
        UINT ich;
        for (ich = m_cchBuff; cchMax > ich; ich *= 4)
            ;
        if (ich == m_cchBuff)
            return S_OK;

        if (ich <= _countof(m_szBuff) - 1)
        {
            if (m_pch && m_cchBuff)
                lstrcpynA(m_szBuff, m_pch, _countof(m_szBuff) - 1);
            Reset();
            m_pch = m_szBuff;
        }
        else
        {
            PSTR pszText = (PSTR)LocalAlloc(LPTR, ich);
            if (!pszText)
                return E_OUTOFMEMORY;

            lstrcpynA(pszText, m_pch, cchMax);
            Reset();
            m_cchBuff = ich;
            m_pch = pszText;
        }
        return S_OK;
    }

    HRESULT SetStr(LPCWSTR lpWideCharStr, UINT cchWideChar = -1)
    {
        Reset();
        return _SetStr(lpWideCharStr, cchWideChar);
    }

protected:
    CHAR m_szBuff[SHSTR_BUFF_SIZE];
    PCHAR m_pch;
    INT m_cchBuff;

    HRESULT _SetStr(LPCWSTR lpWideCharStr, INT cchWideChar)
    {
        INT cch = cchWideChar;
        if (!lpWideCharStr || !cchWideChar)
            return S_FALSE;

        if (cchWideChar == -1)
            cch = WideCharToMultiByte(CP_ACP, 0, lpWideCharStr, -1, NULL, 0, NULL, NULL);

        if (!cch)
            return S_FALSE;

        HRESULT hr = SetSize(cch + 1);
        if (FAILED(hr))
            return hr;

        INT cchStr = WideCharToMultiByte(CP_ACP, 0, lpWideCharStr, cchWideChar, m_pch, m_cchBuff, NULL, NULL);
        if (cchStr >= m_cchBuff)
            cchStr = m_cchBuff;

        m_pch[cchStr] = ANSI_NULL;
        return hr;
    }
};

class ShStrW
{
public:
    ShStrW()
    {
        m_szBuff[0] = UNICODE_NULL;
        m_pch = m_szBuff;
        m_cchBuff = _countof(m_szBuff) - 1;
    }

    ~ShStrW()
    {
        Reset();
    }

    operator LPWSTR() { return m_pch; }
    INT GetBuffLength() const { return m_cchBuff; }

    void Reset()
    {
        if (m_pch && m_cchBuff != _countof(m_szBuff) - 1)
            LocalFree(m_pch);

        m_szBuff[0] = UNICODE_NULL;
        m_pch = m_szBuff;
        m_cchBuff = _countof(m_szBuff) - 1;
    }

    HRESULT SetSize(INT cchMax)
    {
        INT ich;
        for (ich = m_cchBuff; cchMax > ich; ich *= 4)
            ;
        if (ich == m_cchBuff)
            return S_OK;

        if (ich <= _countof(m_szBuff) - 1)
        {
            if (m_pch && m_cchBuff)
                StrCpyNW(m_szBuff, m_pch, _countof(m_szBuff) - 1);
            Reset();
        }
        else
        {
            PWSTR pszText = (PWSTR)LocalAlloc(LPTR, ich * sizeof(WCHAR));
            if (!pszText)
                return E_OUTOFMEMORY;

            StrCpyNW(pszText, m_pch, cchMax);
            Reset();
            m_pch = pszText;
            m_cchBuff = ich;
        }

        return S_OK;
    }

    HRESULT SetStr(LPCWSTR lpString, INT cchString = -1)
    {
        Reset();
        return _SetStr(lpString, cchString);
    }

    HRESULT SetStr(ShStrW *pOther)
    {
        return SetStr(pOther->m_pch, -1);
    }

    HRESULT SetStr(LPCSTR lpString)
    {
        Reset();
        return _SetStr(lpString);
    }

    HRESULT Append(LPCWSTR lpString, INT cchString)
    {
        if (!lpString)
            return E_INVALIDARG;
        INT cchStr = lstrlenW(m_pch);
        if (cchString == -1)
            cchString = lstrlenW(lpString);
        INT cch2 = cchString + 1;
        if (FAILED(SetSize(cch2 + cchStr)))
            return E_OUTOFMEMORY;
        StrCpyNW(&m_pch[cchStr], lpString, cch2);
        return S_OK;
    }

protected:
    WCHAR m_szBuff[SHSTR_BUFF_SIZE];
    PWCHAR m_pch;
    INT m_cchBuff;

    HRESULT _SetStr(LPCSTR lpString)
    {
        if (!lpString)
            return S_FALSE;

        INT cchString = lstrlenA(lpString);
        if (!cchString)
            return S_FALSE;

        HRESULT hr = SetSize(cchString + 1);
        if (SUCCEEDED(hr))
            MultiByteToWideChar(CP_ACP, 0, lpString, -1, m_pch, m_cchBuff);
        return hr;
    }

    HRESULT _SetStr(LPCWSTR lpString, INT cchString)
    {
        if (!lpString || !cchString)
            return S_FALSE;

        INT cchStr = cchString;
        if (cchString == -1)
            cchStr = lstrlenW(lpString);

        if (!cchStr)
            return S_FALSE;

        INT cchMax = cchStr + 1;
        HRESULT hr = SetSize(cchStr + 1);
        if (SUCCEEDED(hr))
        {
            INT cchBuff = m_cchBuff;
            if (cchMax < cchBuff)
                cchBuff = cchMax;
            StrCpyNW(m_pch, lpString, cchBuff);
        }

        return hr;
    }
};
