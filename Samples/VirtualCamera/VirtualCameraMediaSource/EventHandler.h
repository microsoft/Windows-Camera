//
// Copyright (C) Microsoft Corporation. All rights reserved.
//
#pragma once
#ifndef EVENTHANDLER_H
#define EVENTHANDLER_H

namespace winrt::WindowsSample::implementation
{
    template<class T>
    struct CAsyncCallback : winrt::implements<CAsyncCallback<T>, IMFAsyncCallback>
    {
    public:
        typedef void(T::* InvokeFn)(IMFAsyncResult* pAsyncResult);

        CAsyncCallback(_In_ T* pParent, _In_ InvokeFn pCallback, _In_ DWORD dwQueue)
            : m_pParent(pParent)
            , m_pInvokeFn(pCallback)
            , m_dwQueue(dwQueue)
        {}

        // IMFAsyncCallback
        IFACEMETHODIMP GetParameters(DWORD* pdwFlags, DWORD* pdwQueue) override
        {
            // Implementation of this method is optional.
            *pdwFlags = 0;
            *pdwQueue = m_dwQueue;
            return S_OK;
        }

        IFACEMETHODIMP Invoke(IMFAsyncResult* pAsyncResult) override
        {
            (m_pParent->*m_pInvokeFn)(pAsyncResult);
            return S_OK;
        };

    private:
        T* m_pParent;
        InvokeFn m_pInvokeFn;
        DWORD m_dwQueue;
    };
}

#endif
