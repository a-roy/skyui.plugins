#include "PapyrusVM.h"
#include "GameAPI.h"
#include "PapyrusEvents.h"

IObjectHandlePolicy	** g_objectHandlePolicy = (IObjectHandlePolicy **)0x0141CA38;
SkyrimVM			** g_skyrimVM = (SkyrimVM **)0x012DAF1C;


void SkyrimVM::OnFormDelete_Hook(UInt64 handle)
{
	CALL_MEMBER_FN(this, UnregisterForSleep_Internal)(handle);

	g_menuOpenCloseRegs.UnregisterFromAll(handle);
	g_inputEventRegs.UnregisterFromAll(handle);
	g_modCallbackRegs.UnregisterFromAll(handle);

#if _DEBUG
	_MESSAGE("Executed SkyrimVM::OnFormDelete_Hook.");
#endif
}

void SkyrimVM::RevertGlobalData_Hook(void)
{
	CALL_MEMBER_FN(this, RevertGlobalData_Internal)();

	g_menuOpenCloseRegs.Clear();
	g_inputEventRegs.Clear();
	g_modCallbackRegs.Clear();

#if _DEBUG
	_MESSAGE("Executed SkyrimVM::RevertGlobalData_Hook.");
#endif
}


void VMClassInfo::AddRef(void)
{
	InterlockedIncrement(&refCount);
}

void VMClassInfo::Release(void)
{
	if(!InterlockedDecrement(&refCount))
	{
		CALL_MEMBER_FN(this, Destroy)();
		FormHeap_Free(this);
	}
}

UInt64 VMIdentifier::GetHandle(void)
{
	UInt64	result = (*g_objectHandlePolicy)->GetInvalidHandle();

	SInt32	oldLock = Lock();
	result = m_handle;
	Unlock(oldLock);

	return result;
}

SInt32 VMIdentifier::Lock(void)
{
	UInt32	spinCount = 0;
	SInt32	lockValue;

	while(true)
	{
		lockValue = m_lock;

		if(lockValue & kLockBit)
		{
			// someone else is holding the lock, sleep
			if(spinCount <= kFastSpinThreshold)
			{
				Sleep(0);
				spinCount++;
			}
			else
			{
				Sleep(1);
			}
		}
		else
		{
			// try to take the lock
			if(InterlockedCompareExchange(&m_lock, lockValue | kLockBit, lockValue) == lockValue)
				break;
		}
	}

	return lockValue;
}

void VMIdentifier::Unlock(SInt32 oldLock)
{
	m_lock = oldLock;
}

// try to increment the lock
// on 1 -> 2, call IObjectHandlePolicy::Unk_09(m_handle) with the lock held
void VMIdentifier::IncrementLock(void)
{
	UInt32	spinCount = 0;
	
	while(true)
	{
		SInt32	lockValue = m_lock;

		if(lockValue & kLockBit)
		{
			if(spinCount <= kFastSpinThreshold)
			{
				Sleep(0);
				spinCount++;
			}
			else
			{
				Sleep(1);
			}
		}
		else
		{
			if(lockValue == 1)
			{
				if(InterlockedCompareExchange(&m_lock, lockValue | kLockBit, lockValue) == lockValue)
				{
					(*g_objectHandlePolicy)->AddRef(m_handle);
					m_lock = 2;
					break;
				}
			}
			else
			{
				if(InterlockedCompareExchange(&m_lock, lockValue + 1, lockValue) == lockValue)
					break;
			}
		}
	}
}

// try to decrement the lock
// on 2 -> 1, call IObjectHandlePolicy::Unk_0A(m_handle) with the lock held
SInt32 VMIdentifier::DecrementLock(void)
{
	UInt32	spinCount = 0;

	while(true)
	{
		SInt32	lockValue = m_lock;

		if(lockValue & kLockBit)
		{
			if(spinCount <= kFastSpinThreshold)
			{
				Sleep(0);
				spinCount++;
			}
			else
			{
				Sleep(1);
			}
		}
		else
		{
			if(lockValue == 2)
			{
				if(InterlockedCompareExchange(&m_lock, lockValue | kLockBit, lockValue) == lockValue)
				{
					(*g_objectHandlePolicy)->Release(m_handle);
					m_lock = 1;
					return 1;
				}
			}
			else
			{
				if(InterlockedCompareExchange(&m_lock, lockValue - 1, lockValue) == lockValue)
					return lockValue - 1;
			}
		}
	}
}

void VMIdentifier::Destroy(void)
{
	CALL_MEMBER_FN(this, Destroy_Internal)();
	FormHeap_Free(this);
}

UInt32 VMValue::GetUnmangledType(void)
{
	if(type <= 0x0F)
		return type;

	return (type & 1) ? kType_Unk0B : kType_Identifier;
}
