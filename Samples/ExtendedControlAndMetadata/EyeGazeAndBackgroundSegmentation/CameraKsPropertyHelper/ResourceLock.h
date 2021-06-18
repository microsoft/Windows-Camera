// Copyright (C) Microsoft Corporation
#pragma once
#include <Windows.h>

//
//  Simple CRITICAL_SECTION based locks
//
class CResource_Lock
{
    // make copy constructor and assignment operator inaccessible

    CResource_Lock(const CResource_Lock& refCritSec);
    CResource_Lock& operator=(const CResource_Lock& refCritSec);

    CRITICAL_SECTION m_CritSec;

public:
    CResource_Lock()
    {
        InitializeCriticalSection(&m_CritSec);
    };

    ~CResource_Lock()
    {
        DeleteCriticalSection(&m_CritSec);
    };

    void Lock()
    {
        EnterCriticalSection(&m_CritSec);
    };
    void Unlock()
    {
        LeaveCriticalSection(&m_CritSec);
    };
    void LockExclusive()
    {
        EnterCriticalSection(&m_CritSec);
    };
    void UnlockExclusive()
    {
        LeaveCriticalSection(&m_CritSec);
    };
    bool IsLockHeldByCurrentThread()
    {
        return GetCurrentThreadId() == static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(m_CritSec.OwningThread));
    };
    bool IsLockHeld()
    {
        return m_CritSec.OwningThread != 0;
    };
    bool TryLock()
    {
        return TryEnterCriticalSection(&m_CritSec);
    };
    // aliased methods to help code compat so that CriticalSections can be passed to ReaderWriter templates
    void LockShared()
    {
        EnterCriticalSection(&m_CritSec);
    };
    void UnlockShared()
    {
        LeaveCriticalSection(&m_CritSec);
    };
};

// locks a critical section, and unlocks it automatically
// when the lock goes out of scope
class CResource_AutoLock
{

    // make copy constructor and assignment operator inaccessible

    CResource_AutoLock(const CResource_AutoLock& refAutoLock);
    CResource_AutoLock& operator=(const CResource_AutoLock& refAutoLock);

protected:
    CResource_Lock* m_pLock;

public:
    CResource_AutoLock(CResource_Lock* plock)
    {
        m_pLock = plock;
        m_pLock->Lock();
    };

    ~CResource_AutoLock()
    {
        m_pLock->Unlock();
    };
};
#pragma once
