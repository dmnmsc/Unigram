#include "pch.h"
#include <iphlpapi.h>
#include <Windows.Storage.h>
#include "ConnectionManager.h"
#include "Datacenter.h"
#include "DatacenterCryptography.h"
#include "Connection.h"
#include "MessageResponse.h"
#include "MessageRequest.h"
#include "MessageError.h"
#include "TLTypes.h"
#include "TLMethods.h"
#include "DefaultUserConfiguration.h"
#include "Collections.h"
#include "TLBinaryReader.h"
#include "TLBinaryWriter.h"
#include "NativeBuffer.h"
#include "Helpers\COMHelper.h"

#include "MethodDebug.h"

#define FLAGS_GET_CONNECTIONSTATE(flags) static_cast<ConnectionState>(flags & ConnectionManagerFlag::ConnectionState)
#define FLAGS_SET_CONNECTIONSTATE(flags, connectionState) (flags & ~ConnectionManagerFlag::ConnectionState) | static_cast<ConnectionManagerFlag>(connectionState)
#define FLAGS_GET_NETWORKTYPE(flags) static_cast<ConnectionNeworkType>(static_cast<int>(flags & ConnectionManagerFlag::NetworkType) >> 2)
#define FLAGS_SET_NETWORKTYPE(flags, networkType) (flags & ~ConnectionManagerFlag::NetworkType) | static_cast<ConnectionManagerFlag>(static_cast<int>(networkType) << 2)
#define REQUEST_TIMER_TIMEOUT 1000
#define REQUEST_TIMER_WINDOW 0
#define RUNNING_GENERIC_REQUESTS_MAX_COUNT 60
#define RUNNING_DOWNLOAD_REQUESTS_MAX_COUNT 5
#define RUNNING_UPLOAD_REQUESTS_MAX_COUNT 5
#define DATACENTER_EXPIRATION_TIME 60 * 60

using namespace ABI::Windows::Storage;
using namespace ABI::Windows::Networking::Connectivity;
using namespace Telegram::Api::Native;
using namespace Telegram::Api::Native::TL;
using Windows::Foundation::Collections::VectorView;

ActivatableStaticOnlyFactory(ConnectionManagerStatics);


ConnectionManager::ConnectionManager() :
	m_flags(static_cast<ConnectionManagerFlag>(ConnectionState::Connecting)),
	m_currentDatacenterId(0),
	m_movingToDatacenterId(0),
	m_datacentersExpirationTime(0),
	m_timeDifference(0),
	m_lastRequestToken(0),
	m_lastOutgoingMessageId(0),
	m_userId(0)
{
	ZeroMemory(m_runningRequestCount, sizeof(m_runningRequestCount));
}

ConnectionManager::~ConnectionManager()
{
	m_networkInformation->remove_NetworkStatusChanged(m_networkChangedEventToken);

	for (auto& datacenter : m_datacenters)
	{
		datacenter.second->Close();
	}

	m_datacenters.clear();

	WSACleanup();
}

HRESULT ConnectionManager::RuntimeClassInitialize(UINT32 minimumThreadCount, UINT32 maximumThreadCount)
{
	HRESULT result;
	ReturnIfFailed(result, MakeAndInitialize<DefaultUserConfiguration>(&m_userConfiguration));

	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != NO_ERROR)
	{
		return WSAGetLastHRESULT();
	}

	ReturnIfFailed(result, ThreadpoolManager::RuntimeClassInitialize(minimumThreadCount, maximumThreadCount));
	ReturnIfFailed(result, EventObjectT::AttachToThreadpool(this));
	ReturnIfFailed(result, Windows::Foundation::GetActivationFactory(HStringReference(RuntimeClass_Windows_Networking_Connectivity_NetworkInformation).Get(), &m_networkInformation));
	ReturnIfFailed(result, m_networkInformation->add_NetworkStatusChanged(Callback<INetworkStatusChangedEventHandler>(this, &ConnectionManager::OnNetworkStatusChanged).Get(), &m_networkChangedEventToken));
	ReturnIfFailed(result, UpdateNetworkStatus(false));

	return InitializeSettings();
}

HRESULT ConnectionManager::add_SessionCreated(__FITypedEventHandler_2_Telegram__CApi__CNative__CConnectionManager_IInspectable* handler, EventRegistrationToken* token)
{
	return m_sessionCreatedEventSource.Add(handler, token);
}

HRESULT ConnectionManager::remove_SessionCreated(EventRegistrationToken token)
{
	return m_sessionCreatedEventSource.Remove(token);
}

HRESULT ConnectionManager::add_CurrentNetworkTypeChanged(__FITypedEventHandler_2_Telegram__CApi__CNative__CConnectionManager_IInspectable* handler, EventRegistrationToken* token)
{
	return m_currentNetworkTypeChangedEventSource.Add(handler, token);
}

HRESULT ConnectionManager::remove_CurrentNetworkTypeChanged(EventRegistrationToken token)
{
	return m_currentNetworkTypeChangedEventSource.Remove(token);
}

HRESULT ConnectionManager::add_ConnectionStateChanged(__FITypedEventHandler_2_Telegram__CApi__CNative__CConnectionManager_IInspectable* handler, EventRegistrationToken* token)
{
	return m_connectionStateChangedEventSource.Add(handler, token);
}

HRESULT ConnectionManager::remove_ConnectionStateChanged(EventRegistrationToken token)
{
	return m_connectionStateChangedEventSource.Remove(token);
}

HRESULT ConnectionManager::add_UnprocessedMessageReceived(__FITypedEventHandler_2_Telegram__CApi__CNative__CConnectionManager_Telegram__CApi__CNative__CMessageResponse* handler, EventRegistrationToken* token)
{
	return m_unprocessedMessageReceivedEventSource.Add(handler, token);
}

HRESULT ConnectionManager::remove_UnprocessedMessageReceived(EventRegistrationToken token)
{
	return m_unprocessedMessageReceivedEventSource.Remove(token);
}

HRESULT ConnectionManager::get_ConnectionState(ConnectionState* value)
{
	if (value == nullptr)
	{
		return E_POINTER;
	}

	auto lock = LockCriticalSection();

	*value = FLAGS_GET_CONNECTIONSTATE(m_flags);
	return S_OK;
}

HRESULT ConnectionManager::get_CurrentNetworkType(ConnectionNeworkType* value)
{
	if (value == nullptr)
	{
		return E_POINTER;
	}

	auto lock = LockCriticalSection();

	*value = FLAGS_GET_NETWORKTYPE(m_flags);
	return S_OK;
}

HRESULT ConnectionManager::get_CurrentDatacenter(IDatacenter** value)
{
	if (value == nullptr)
	{
		return E_POINTER;
	}

	auto lock = LockCriticalSection();

	ComPtr<Datacenter> datacenter;
	if (GetDatacenterById(m_currentDatacenterId, datacenter))
	{
		*value = datacenter.Detach();
	}

	return S_OK;
}

HRESULT ConnectionManager::get_IsIPv6Enabled(boolean* value)
{
	if (value == nullptr)
	{
		return E_POINTER;
	}

	auto lock = LockCriticalSection();

	*value = (m_flags & ConnectionManagerFlag::UseIPv6) == ConnectionManagerFlag::UseIPv6;
	return S_OK;
}

HRESULT ConnectionManager::get_IsNetworkAvailable(boolean* value)
{
	if (value == nullptr)
	{
		return E_POINTER;
	}

	auto lock = LockCriticalSection();

	*value = FLAGS_GET_NETWORKTYPE(m_flags) != ConnectionNeworkType::None;
	return S_OK;
}

HRESULT ConnectionManager::get_UserConfiguration(IUserConfiguration** value)
{
	if (value == nullptr)
	{
		return E_POINTER;
	}

	auto lock = LockCriticalSection();
	return m_userConfiguration.CopyTo(value);
}

HRESULT ConnectionManager::put_UserConfiguration(IUserConfiguration* value)
{
	auto lock = LockCriticalSection();

	if (value != m_userConfiguration.Get())
	{
		if (value == nullptr)
		{
			ComPtr<IDefaultUserConfiguration> defaultUserConfiguration;
			if (FAILED(m_userConfiguration.As(&defaultUserConfiguration)))
			{
				return MakeAndInitialize<DefaultUserConfiguration>(&m_userConfiguration);
			}
		}
		else
		{
			m_userConfiguration = value;

			I_WANT_TO_DIE_IS_THE_NEW_TODO("Handle UserConfiguration changes");
		}
	}

	return S_OK;
}

HRESULT ConnectionManager::get_UserId(INT32* value)
{
	if (value == nullptr)
	{
		return E_POINTER;
	}

	auto lock = LockCriticalSection();

	*value = m_userId;
	return S_OK;
}

HRESULT ConnectionManager::put_UserId(INT32 value)
{
	auto lock = LockCriticalSection();

	if (value != m_userId)
	{
		m_userId = value;

		if (m_userId != 0)
		{
			I_WANT_TO_DIE_IS_THE_NEW_TODO("Handle UserId changes");
		}
	}

	return S_OK;
}

HRESULT ConnectionManager::get_Proxy(IProxySettings** value)
{
	if (value == nullptr)
	{
		return E_POINTER;
	}

	auto lock = LockCriticalSection();

	return m_proxySettings.CopyTo(value);
}

HRESULT ConnectionManager::put_Proxy(IProxySettings* value)
{
	return E_NOTIMPL;

	auto lock = LockCriticalSection();

	m_proxySettings = value;

	if (value == nullptr)
	{
		I_WANT_TO_DIE_IS_THE_NEW_TODO("Handle Proxy changes");
	}

	return S_OK;
}

HRESULT ConnectionManager::get_Datacenters(_Out_ __FIVectorView_1_Telegram__CApi__CNative__CDatacenter** value)
{
	if (value == nullptr)
	{
		return E_POINTER;
	}

	auto lock = LockCriticalSection();
	auto vectorView = Make<VectorView<ABI::Telegram::Api::Native::Datacenter*>>();

	std::transform(m_datacenters.begin(), m_datacenters.end(), std::back_inserter(vectorView->GetItems()), [](auto& pair)
	{
		return static_cast<IDatacenter*>(pair.second.Get());
	});

	*value = vectorView.Detach();
	return S_OK;
}

HRESULT ConnectionManager::SendRequest(ITLObject* object, ISendRequestCompletedCallback* onCompleted, IRequestQuickAckReceivedCallback* onQuickAckReceived, ConnectionType connectionType, INT32* value)
{
	return SendRequestWithFlags(object, onCompleted, onQuickAckReceived, DEFAULT_DATACENTER_ID, connectionType, RequestFlag::None, value);
}

HRESULT ConnectionManager::SendRequestWithDatacenter(ITLObject* object, ISendRequestCompletedCallback* onCompleted, IRequestQuickAckReceivedCallback* onQuickAckReceived,
	INT32 datacenterId, ConnectionType connectionType, INT32* value)
{
	return SendRequestWithFlags(object, onCompleted, onQuickAckReceived, datacenterId, connectionType, RequestFlag::None, value);
}

HRESULT ConnectionManager::SendRequestWithFlags(ITLObject* object, ISendRequestCompletedCallback* onCompleted, IRequestQuickAckReceivedCallback* onQuickAckReceived,
	INT32 datacenterId, ConnectionType connectionType, RequestFlag flags, INT32* value)
{
	if (object == nullptr || (connectionType != ConnectionType::Generic && connectionType != ConnectionType::Download && connectionType != ConnectionType::Upload))
	{
		return E_INVALIDARG;
	}

	if (value == nullptr)
	{
		return E_POINTER;
	}

	auto lock = LockCriticalSection();

	if (m_userId == 0 && (flags & RequestFlag::WithoutLogin) != RequestFlag::WithoutLogin)
	{
		return E_INVALIDARG;
	}

	auto requestToken = m_lastRequestToken + 1;

	HRESULT result;
	ComPtr<MessageRequest> request;
	ReturnIfFailed(result, MakeAndInitialize<MessageRequest>(&request, object, requestToken, connectionType, datacenterId, onCompleted, onQuickAckReceived, flags));

	{
		auto requestsLock = m_requestsCriticalSection.Lock();
		m_requestsQueue.push_back(request);
	}

	auto requestsTimer = EventObjectT::GetHandle();
	if ((flags & RequestFlag::Immediate) == RequestFlag::Immediate)
	{
		FILETIME timeout = {};
		SetThreadpoolTimer(requestsTimer, &timeout, 0, REQUEST_TIMER_WINDOW);
	}
	else if (!IsThreadpoolTimerSet(requestsTimer))
	{
		FILETIME timeout;
		TimeoutToFileTime(REQUEST_TIMER_TIMEOUT, timeout);
		SetThreadpoolTimer(requestsTimer, &timeout, 0, REQUEST_TIMER_WINDOW);
	}

	*value = requestToken;
	m_lastRequestToken = requestToken;
	return S_OK;
}

HRESULT ConnectionManager::CancelRequest(INT32 requestToken, boolean notifyServer, boolean* value)
{
	if (value == nullptr)
	{
		return E_POINTER;
	}

	auto requestsLock = m_requestsCriticalSection.Lock();
	auto requestIterator = std::find_if(m_requestsQueue.begin(), m_requestsQueue.end(), [&requestToken](auto const& request)
	{
		return request->GetToken() == requestToken;
	});

	if (requestIterator != m_requestsQueue.end())
	{
		m_requestsQueue.erase(requestIterator);

		*value = true;
		return S_OK;
	}

	auto runningRequestIterator = std::find_if(m_runningRequests.begin(), m_runningRequests.end(), [&requestToken](auto const& request)
	{
		return request.second->GetToken() == requestToken;
	});

	if (runningRequestIterator != m_runningRequests.end())
	{
		if (notifyServer)
		{
			auto rpcDropAnswer = Make<Methods::TLRpcDropAnswer>(runningRequestIterator->second->GetMessageContext()->Id);

			HRESULT result;
			INT32 requestToken;
			ReturnIfFailed(result, SendRequestWithFlags(rpcDropAnswer.Get(), nullptr, nullptr, runningRequestIterator->first, runningRequestIterator->second->GetConnectionType(),
				RequestFlag::EnableUnauthorized | RequestFlag::WithoutLogin | RequestFlag::FailOnServerError | RequestFlag::Immediate | REQUEST_FLAG_NO_LAYER, &requestToken));
		}

		m_runningRequestCount[static_cast<UINT32>(runningRequestIterator->second->GetConnectionType()) >> 1]--;
		m_runningRequests.erase(runningRequestIterator);

		*value = true;
		return S_OK;
	}

	*value = false;
	return S_OK;
}

HRESULT ConnectionManager::GetDatacenterById(INT32 id, IDatacenter** value)
{
	if (value == nullptr)
	{
		return E_POINTER;
	}

	auto lock = LockCriticalSection();

	if (id == DEFAULT_DATACENTER_ID)
	{
		id = m_currentDatacenterId;
	}

	ComPtr<Datacenter> datacenter;
	if (!GetDatacenterById(id, datacenter))
	{
		return E_INVALIDARG;
	}

	*value = datacenter.Detach();
	return S_OK;
}

HRESULT ConnectionManager::UpdateDatacenters()
{
	auto lock = LockCriticalSection();

	if ((m_flags & ConnectionManagerFlag::UpdatingDatacenters) == ConnectionManagerFlag::UpdatingDatacenters)
	{
		return S_FALSE;
	}

	auto helpGetConfig = Make<Methods::TLHelpGetConfig>();

	m_flags |= ConnectionManagerFlag::UpdatingDatacenters;

	INT32 requestToken;
	return SendRequestWithFlags(helpGetConfig.Get(), Callback<ISendRequestCompletedCallback>([this](IMessageResponse* response, IMessageError* error) -> HRESULT
	{
		auto lock = LockCriticalSection();

		m_flags &= ~ConnectionManagerFlag::UpdatingDatacenters;

		if (error == nullptr)
		{
			auto config = GetMessageResponseObject<TLConfig>(response);

			std::map<std::pair<INT32, TLDCOptionFlag>, std::vector<ServerEndpoint>> datacentersEndpoints;

			for (auto& dcOption : config->GetDcOptions())
			{
				auto& key = std::make_pair(dcOption->GetId(), dcOption->GetFlags());
				datacentersEndpoints[key].push_back({ dcOption->GetIpAddress().GetRawBuffer(nullptr), static_cast<UINT32>(dcOption->GetPort()) });
			}

			HRESULT result;
			for (auto& datacenterEndpoints : datacentersEndpoints)
			{
				auto datacenterIterator = m_datacenters.find(datacenterEndpoints.first.first);
				if (datacenterIterator == m_datacenters.end())
				{
					ComPtr<Datacenter> datacenter;
					ReturnIfFailed(result, MakeAndInitialize<Datacenter>(&datacenter, this, datacenterEndpoints.first.first, (datacenterEndpoints.first.second & TLDCOptionFlag::CDN) == TLDCOptionFlag::CDN));

					datacenterIterator = m_datacenters.insert(datacenterIterator, std::make_pair(datacenterEndpoints.first.first, std::move(datacenter)));
				}

				ReturnIfFailed(result, datacenterIterator->second->ReplaceEndpoints(datacenterEndpoints.second, (datacenterEndpoints.first.second & TLDCOptionFlag::MediaOnly) == TLDCOptionFlag::MediaOnly ?
					ConnectionType::Download : ConnectionType::Generic, (datacenterEndpoints.first.second & TLDCOptionFlag::IPv6) == TLDCOptionFlag::IPv6));

				/*if (datacenterEndpoints.first.first == m_movingToDatacenterId)
				{
					m_movingToDatacenterId = DEFAULT_DATACENTER_ID;

					ReturnIfFailed(result, MoveToDatacenter(datacenterEndpoints.first.first));
				}*/
			}

			m_datacentersExpirationTime = config->GetExpires() - m_timeDifference;

			FILETIME timeout = {};
			SetThreadpoolTimer(EventObjectT::GetHandle(), &timeout, 0, REQUEST_TIMER_WINDOW);

			ReturnIfFailed(result, m_unprocessedMessageReceivedEventSource.InvokeAll(this, response));

			for (auto& datacenter : m_datacenters)
			{
				ReturnIfFailed(result, SaveDatacenterSettings(datacenter.second.Get()));
			}

			return SaveSettings();
		}
		else
		{
			return S_OK;
		}
	}).Get(), nullptr, m_currentDatacenterId, ConnectionType::Generic, RequestFlag::EnableUnauthorized | RequestFlag::WithoutLogin | RequestFlag::TryDifferentDc, &requestToken);
}

HRESULT ConnectionManager::InitializeDefaultDatacenters()
{
	HRESULT result;

#if _DEBUG

	if (m_datacenters.find(1) == m_datacenters.end())
	{
		ReturnIfFailed(result, MakeAndInitialize<Datacenter>(&m_datacenters[1], this, 1, false));
		ReturnIfFailed(result, m_datacenters[1]->AddEndpoint({ L"149.154.175.40", 443 }, ConnectionType::Generic, false));
		ReturnIfFailed(result, m_datacenters[1]->AddEndpoint({ L"2001:b28:f23d:f001:0000:0000:0000:000e", 443 }, ConnectionType::Generic, true));
	}

	if (m_datacenters.find(2) == m_datacenters.end())
	{
		ReturnIfFailed(result, MakeAndInitialize<Datacenter>(&m_datacenters[2], this, 2, false));
		ReturnIfFailed(result, m_datacenters[2]->AddEndpoint({ L"149.154.167.40", 443 }, ConnectionType::Generic, false));
		ReturnIfFailed(result, m_datacenters[2]->AddEndpoint({ L"2001:67c:4e8:f002:0000:0000:0000:000e", 443 }, ConnectionType::Generic, true));
	}

	if (m_datacenters.find(3) == m_datacenters.end())
	{
		ReturnIfFailed(result, MakeAndInitialize<Datacenter>(&m_datacenters[3], this, 3, false));
		ReturnIfFailed(result, m_datacenters[3]->AddEndpoint({ L"149.154.175.117", 443 }, ConnectionType::Generic, false));
		ReturnIfFailed(result, m_datacenters[3]->AddEndpoint({ L"2001:b28:f23d:f003:0000:0000:0000:000e", 443 }, ConnectionType::Generic, true));
	}

#else

	if (m_datacenters.find(1) == m_datacenters.end())
	{
		ReturnIfFailed(result, MakeAndInitialize<Datacenter>(&m_datacenters[1], this, 1, false));
		ReturnIfFailed(result, m_datacenters[1]->AddEndpoint({ L"149.154.175.50", 443 }, ConnectionType::Generic, false));
		ReturnIfFailed(result, m_datacenters[1]->AddEndpoint({ L"2001:b28:f23d:f001:0000:0000:0000:000a", 443 }, ConnectionType::Generic, true));
	}

	if (m_datacenters.find(2) == m_datacenters.end())
	{
		ReturnIfFailed(result, MakeAndInitialize<Datacenter>(&m_datacenters[2], this, 2, false));
		ReturnIfFailed(result, m_datacenters[2]->AddEndpoint({ L"149.154.167.51", 443 }, ConnectionType::Generic, false));
		ReturnIfFailed(result, m_datacenters[2]->AddEndpoint({ L"2001:67c:4e8:f002:0000:0000:0000:000a", 443 }, ConnectionType::Generic, true));
	}

	if (m_datacenters.find(3) == m_datacenters.end())
	{
		ReturnIfFailed(result, MakeAndInitialize<Datacenter>(&m_datacenters[3], this, 3, false));
		ReturnIfFailed(result, m_datacenters[3]->AddEndpoint({ L"149.154.175.100", 443 }, ConnectionType::Generic, false));
		ReturnIfFailed(result, m_datacenters[3]->AddEndpoint({ L"2001:b28:f23d:f003:0000:0000:0000:000a", 443 }, ConnectionType::Generic, true));
	}

	if (m_datacenters.find(4) == m_datacenters.end())
	{
		ReturnIfFailed(result, MakeAndInitialize<Datacenter>(&m_datacenters[4], this, 4, false));
		ReturnIfFailed(result, m_datacenters[4]->AddEndpoint({ L"149.154.167.91", 443 }, ConnectionType::Generic, false));
		ReturnIfFailed(result, m_datacenters[4]->AddEndpoint({ L"2001:67c:4e8:f004:0000:0000:0000:000a", 443 }, ConnectionType::Generic, true));

		m_datacenters[4] = datacenter;
	}

	if (m_datacenters.find(5) == m_datacenters.end())
	{
		ReturnIfFailed(result, MakeAndInitialize<Datacenter>(&m_datacenters[5], this, 5, false));
		ReturnIfFailed(result, m_datacenters[5]->AddEndpoint({ L"149.154.171.5", 443 }, ConnectionType::Generic, false));
		ReturnIfFailed(result, m_datacenters[5]->AddEndpoint({ L"2001:b28:f23f:f005:0000:0000:0000:000a", 443 }, ConnectionType::Generic, true));
	}

#endif

	m_currentDatacenterId = 1;
	m_movingToDatacenterId = DEFAULT_DATACENTER_ID;
	m_datacentersExpirationTime = static_cast<INT32>(GetCurrentSystemTime() / 1000) + DATACENTER_EXPIRATION_TIME;
	return S_OK;
}

HRESULT ConnectionManager::UpdateNetworkStatus(bool raiseEvent)
{
	HRESULT result;
	ComPtr<IConnectionProfile> connectionProfile;
	ReturnIfFailed(result, m_networkInformation->GetInternetConnectionProfile(&connectionProfile));

	ConnectionNeworkType currentNetworkType;
	if (connectionProfile == nullptr)
	{
		currentNetworkType = ConnectionNeworkType::None;
	}
	else
	{
		ComPtr<IConnectionCost> connectionCost;
		ReturnIfFailed(result, connectionProfile->GetConnectionCost(&connectionCost));

		NetworkCostType networkCostType;
		ReturnIfFailed(result, connectionCost->get_NetworkCostType(&networkCostType));

		boolean isRoaming;
		ReturnIfFailed(result, connectionCost->get_Roaming(&isRoaming));

		if (isRoaming)
		{
			currentNetworkType = ConnectionNeworkType::Roaming;
		}
		else
		{
			ComPtr<INetworkAdapter> networkAdapter;
			ReturnIfFailed(result, connectionProfile->get_NetworkAdapter(&networkAdapter));

			UINT32 interfaceIanaType;
			ReturnIfFailed(result, networkAdapter->get_IanaInterfaceType(&interfaceIanaType));

			switch (interfaceIanaType)
			{
			case IF_TYPE_ETHERNET_CSMACD:
			case IF_TYPE_IEEE80211:
				currentNetworkType = ConnectionNeworkType::WiFi;
				break;
			case IF_TYPE_WWANPP:
			case IF_TYPE_WWANPP2:
				currentNetworkType = ConnectionNeworkType::Mobile;
				break;
			default:
				currentNetworkType = ConnectionNeworkType::None;
				break;
			}
		}
	}

	if (currentNetworkType != FLAGS_GET_NETWORKTYPE(m_flags))
	{
		m_flags = FLAGS_SET_NETWORKTYPE(m_flags, currentNetworkType);
		return m_currentNetworkTypeChangedEventSource.InvokeAll(this, nullptr);
	}

	return S_OK;
}

HRESULT ConnectionManager::Reset()
{
	auto lock = LockCriticalSection();
	auto requestsLock = m_requestsCriticalSection.Lock();

	CloseAllObjects(true);

	for (auto& datacenter : m_datacenters)
	{
		datacenter.second->Close();
	}

	m_userId = 0;
	m_datacenters.clear();
	m_cdnPublicKeys.clear();
	m_requestsQueue.clear();
	m_runningRequests.clear();
	m_quickAckRequests.clear();

	ZeroMemory(m_runningRequestCount, sizeof(m_runningRequestCount));

	HRESULT result;
	ReturnIfFailed(result, EventObjectT::AttachToThreadpool(this));
	ReturnIfFailed(result, InitializeDefaultDatacenters());

	return SaveSettings();
}

HRESULT ConnectionManager::UpdateCDNPublicKeys()
{
	auto lock = LockCriticalSection();

	if ((m_flags & ConnectionManagerFlag::UpdatingCDNPublicKeys) == ConnectionManagerFlag::UpdatingCDNPublicKeys)
	{
		return S_FALSE;
	}

	auto helpGetCDNConfig = Make<Methods::TLHelpGetCDNConfig>();

	m_flags |= ConnectionManagerFlag::UpdatingCDNPublicKeys;

	INT32 requestToken;
	return SendRequestWithFlags(helpGetCDNConfig.Get(), Callback<ISendRequestCompletedCallback>([this](IMessageResponse* response, IMessageError* error) -> HRESULT
	{
		auto lock = LockCriticalSection();

		m_flags &= ~ConnectionManagerFlag::UpdatingCDNPublicKeys;

		if (error == nullptr)
		{
			auto cdnConfig = GetMessageResponseObject<TLCDNConfig>(response);

			for (auto& cdnPublicKey : cdnConfig->GetPublicKeys())
			{
				auto publicKey = m_cdnPublicKeys.find(cdnPublicKey->GetDatacenterId());
				if (publicKey == m_cdnPublicKeys.end())
				{
					publicKey = m_cdnPublicKeys.insert(publicKey, std::make_pair(cdnPublicKey->GetDatacenterId(), ServerPublicKey()));
				}

				publicKey->second.Key.Attach(DatacenterCryptography::GetRSAPublicKey(cdnPublicKey->GetPublicKey()));
				publicKey->second.Fingerprint = DatacenterCryptography::ComputePublickKeyFingerprint(publicKey->second.Key.Get());
			}

			FILETIME timeout = {};
			SetThreadpoolTimer(EventObjectT::GetHandle(), &timeout, 0, REQUEST_TIMER_WINDOW);

			return SaveCDNPublicKeys();
		}

		return S_OK;
	}).Get(), nullptr, m_currentDatacenterId, ConnectionType::Generic, RequestFlag::Immediate, &requestToken);
}

HRESULT ConnectionManager::MoveToDatacenter(INT32 datacenterId)
{
	if (datacenterId == m_movingToDatacenterId)
	{
		return S_FALSE;
	}

	auto datacenterIterator = m_datacenters.find(datacenterId);
	if (datacenterIterator == m_datacenters.end())
	{
		HRESULT result;
		ReturnIfFailed(result, UpdateDatacenters());

		return S_FALSE;
	}
	else if (datacenterIterator->second->IsCDN())
	{
		return E_INVALIDARG;
	}

	ResetRequests([this](auto datacenterId, auto const& request) -> boolean
	{
		return datacenterId == m_currentDatacenterId;
	}, true);

	if (m_userId == 0)
	{
		m_currentDatacenterId = datacenterId;
		m_movingToDatacenterId = DEFAULT_DATACENTER_ID;

		datacenterIterator->second->RecreateSessions();

		ResetRequests([this](auto datacenterId, auto const& request) -> boolean
		{
			return datacenterId == m_currentDatacenterId;
		}, true);

		return ProcessRequestsForDatacenter(datacenterIterator->second.Get(), ConnectionType::Generic | ConnectionType::Download | ConnectionType::Upload);
	}
	else
	{
		m_movingToDatacenterId = datacenterId;

		auto authExportAuthorization = Make<Methods::TLAuthExportAuthorization>(datacenterId);

		auto datacenter = datacenterIterator->second;
		datacenter->SetImportingAuthorization();

		HRESULT result;
		INT32 requestToken;
		if (FAILED(result = SendRequestWithFlags(authExportAuthorization.Get(), Callback<ISendRequestCompletedCallback>([this, datacenter](IMessageResponse* response, IMessageError* error) -> HRESULT
		{
			auto lock = LockCriticalSection();

			if (error == nullptr)
			{
				datacenter->RecreateSessions();

				ResetRequests([this](auto datacenterId, auto const& request) -> boolean
				{
					return datacenterId == m_movingToDatacenterId;
				}, true);

				HRESULT result;
				if (!datacenter->IsAuthenticated())
				{
					datacenter->ClearServerSalts();

					ReturnIfFailed(result, datacenter->BeginHandshake(true, false));
				}

				auto authExportedAuthorization = GetMessageResponseObject<TLAuthExportedAuthorization>(response);

				ComPtr<Methods::TLAuthImportAuthorization> authImportAuthorization;
				ReturnIfFailed(result, MakeAndInitialize<Methods::TLAuthImportAuthorization>(&authImportAuthorization, authExportedAuthorization->GetId(), authExportedAuthorization->GetBytes().Get()));

				INT32 requestToken;
				return SendRequestWithFlags(authImportAuthorization.Get(), Callback<ISendRequestCompletedCallback>([this, datacenter](IMessageResponse* response, IMessageError* error) -> HRESULT
				{
					auto lock = LockCriticalSection();

					if (error == nullptr)
					{
						m_currentDatacenterId = m_movingToDatacenterId;
						m_movingToDatacenterId = DEFAULT_DATACENTER_ID;

						datacenter->SetAuthorized();

						HRESULT result;
						ReturnIfFailed(result, SaveDatacenterSettings(datacenter.Get()));

						return SaveSettings();
					}
					else
					{


						auto movingToDatacenterId = m_movingToDatacenterId;
						m_movingToDatacenterId = DEFAULT_DATACENTER_ID;

						datacenter->SetUnauthorized();

						return MoveToDatacenter(movingToDatacenterId);
					}
				}).Get(), nullptr, m_movingToDatacenterId, ConnectionType::Generic, RequestFlag::EnableUnauthorized | RequestFlag::Immediate, &requestToken);
			}
			else
			{
				datacenter->SetUnauthorized();

				auto movingToDatacenterId = m_movingToDatacenterId;
				m_movingToDatacenterId = DEFAULT_DATACENTER_ID;

				return MoveToDatacenter(movingToDatacenterId);
			}
		}).Get(), nullptr, m_currentDatacenterId, ConnectionType::Generic, RequestFlag::Immediate, &requestToken)))
		{
			datacenter->SetUnauthorized();
			return result;
		}

		return S_OK;
	}
}

HRESULT ConnectionManager::CreateTransportMessage(MessageRequest* request, INT64& lastRpcMessageId, bool& requiresLayer, TLMessage** message)
{
	ComPtr<ITLObject> object = request->GetObject();

	if (requiresLayer && request->IsLayerRequired())
	{
		HRESULT result;
		ComPtr<Methods::TLInitConnection> initConnectionObject;
		ReturnIfFailed(result, MakeAndInitialize<Methods::TLInitConnection>(&initConnectionObject, m_userConfiguration.Get(), object.Get()));
		ReturnIfFailed(result, MakeAndInitialize<Methods::TLInvokeWithLayer>(&object, initConnectionObject.Get()));

		request->SetInitConnection();
		requiresLayer = false;
	}

	if (lastRpcMessageId != 0 && request->InvokeAfter())
	{
		auto rpcMessageId = request->GetMessageContext()->Id;
		if (rpcMessageId != lastRpcMessageId)
		{
			HRESULT result;
			ComPtr<ITLObject> invokeAfter;
			ReturnIfFailed(result, MakeAndInitialize<Methods::TLInvokeAfterMsg>(&invokeAfter, lastRpcMessageId, object.Get()));

			object.Swap(invokeAfter);
			lastRpcMessageId = rpcMessageId;
		}
	}

	if (request->CanCompress())
	{
		HRESULT result;
		ComPtr<ITLObject> gzipPacked;
		ReturnIfFailed(result, MakeAndInitialize<TLGZipPacked>(&gzipPacked, object.Get()));

		object.Swap(gzipPacked);
	}

	return MakeAndInitialize<TLMessage>(message, request->GetMessageContext(), object.Get());
}

HRESULT ConnectionManager::ProcessRequests()
{
	auto currentTime = static_cast<INT32>(GetCurrentSystemTime() / 1000);

	auto requestsLock = m_requestsCriticalSection.Lock();

	{
		auto requestIterator = m_runningRequests.begin();
		while (requestIterator != m_runningRequests.end())
		{
			if (requestIterator->second->IsTimedOut(currentTime))
			{
				m_runningRequestCount[static_cast<UINT32>(requestIterator->second->GetConnectionType()) >> 1]--;
				m_requestsQueue.push_back(std::move(requestIterator->second));
				requestIterator = m_runningRequests.erase(requestIterator);
			}
			else
			{
				requestIterator++;
			}
		}
	}

	HRESULT result = S_OK;
	std::map<UINT32, DatacenterRequestContext> datacentersContexts;

	{
		auto lock = LockCriticalSection();

		if (m_datacentersExpirationTime < currentTime)
		{
			return UpdateDatacenters();
		}

		auto requestIterator = m_requestsQueue.begin();
		while (requestIterator != m_requestsQueue.end())
		{
			BreakIfFailed(result, ProcessRequest(requestIterator->Get(), currentTime, datacentersContexts));

			if (result == S_OK)
			{
				requestIterator = m_requestsQueue.erase(requestIterator);
			}
			else
			{
				requestIterator++;
			}
		}

		for (auto& datacenter : m_datacenters)
		{
			auto datacenterContextIterator = datacentersContexts.find(datacenter.first);
			if (datacenterContextIterator == datacentersContexts.end())
			{
				ComPtr<Connection> genericConnection;
				datacenter.second->GetGenericConnection(false, genericConnection);

				if (genericConnection != nullptr && genericConnection->HasMessagesToConfirm())
				{
					datacentersContexts.insert(datacenterContextIterator, std::make_pair(datacenter.first, DatacenterRequestContext(datacenter.second.Get())));
				}
			}
		}
	}

	if (FAILED(result) || FAILED(result = ProcessRequests(datacentersContexts)))
	{
		ResetRequests(datacentersContexts);
		return result;
	}

	return S_OK;
}

HRESULT ConnectionManager::ProcessRequestsForDatacenter(Datacenter* datacenter, ConnectionType connectionType)
{
	auto currentTime = static_cast<INT32>(GetCurrentSystemTime() / 1000);
	auto requestsLock = m_requestsCriticalSection.Lock();

	{
		auto requestIterator = m_runningRequests.begin();
		while (requestIterator != m_runningRequests.end())
		{
			if (requestIterator->first == datacenter->GetId() && requestIterator->second->MatchesConnection(connectionType) && requestIterator->second->IsTimedOut(currentTime))
			{
				m_runningRequestCount[static_cast<UINT32>(requestIterator->second->GetConnectionType()) >> 1]--;
				m_requestsQueue.push_back(std::move(requestIterator->second));
				requestIterator = m_runningRequests.erase(requestIterator);
				continue;
			}

			requestIterator++;
		}
	}

	HRESULT result = S_OK;
	std::map<UINT32, DatacenterRequestContext> datacentersContexts;

	{
		auto lock = LockCriticalSection();

		if (m_datacentersExpirationTime < currentTime)
		{
			return UpdateDatacenters();
		}

		auto requestIterator = m_requestsQueue.begin();
		while (requestIterator != m_requestsQueue.end())
		{
			auto& request = *requestIterator;
			auto datacenterId = request->GetDatacenterId();
			if (datacenterId == DEFAULT_DATACENTER_ID)
			{
				datacenterId = m_currentDatacenterId;
			}

			if (datacenterId == datacenter->GetId() && request->MatchesConnection(connectionType))
			{
				BreakIfFailed(result, ProcessRequest(request.Get(), currentTime, datacentersContexts));
				if (result == S_OK)
				{
					requestIterator = m_requestsQueue.erase(requestIterator);
					continue;
				}
			}

			requestIterator++;
		}
	}

	auto datacenterContextIterator = datacentersContexts.find(datacenter->GetId());
	if (datacenterContextIterator == datacentersContexts.end())
	{
		ComPtr<Connection> genericConnection;
		datacenter->GetGenericConnection(false, genericConnection);

		if (genericConnection != nullptr && genericConnection->HasMessagesToConfirm())
		{
			datacentersContexts.insert(datacenterContextIterator, std::make_pair(datacenter->GetId(), DatacenterRequestContext(datacenter)));
		}
	}

	if (FAILED(result) || FAILED(result = ProcessRequests(datacentersContexts)))
	{
		ResetRequests(datacentersContexts);
		return result;
	}

	return S_OK;
}

HRESULT ConnectionManager::ProcessRequest(MessageRequest* request, INT32 currentTime, std::map<UINT32, DatacenterRequestContext>& datacentersContexts)
{
	auto datacenterId = request->GetDatacenterId();
	if (datacenterId == DEFAULT_DATACENTER_ID)
	{
		if (m_movingToDatacenterId != DEFAULT_DATACENTER_ID)
		{
			return S_FALSE;
		}

		datacenterId = m_currentDatacenterId;
	}

	if (request->GetStartTime() > currentTime)
	{
		return S_FALSE;
	}
	else if (request->IsTimedOut(currentTime) && request->TryDifferentDc())
	{
		std::vector<INT32> datacenterIds;
		for (auto& datacenter : m_datacenters)
		{
			if (!(datacenter.first == datacenterId || datacenter.second->IsCDN()))
			{
				datacenterIds.push_back(datacenter.first);
			}
		}

		BYTE index;
		RAND_bytes(&index, sizeof(BYTE));
		datacenterId = datacenterIds[index % datacenterIds.size()];
	}

	auto datacenterContextIterator = datacentersContexts.find(datacenterId);
	if (datacenterContextIterator == datacentersContexts.end())
	{
		ComPtr<Datacenter> datacenter;
		GetDatacenterById(datacenterId, datacenter);

		datacenterContextIterator = datacentersContexts.insert(datacenterContextIterator, std::make_pair(datacenterId, DatacenterRequestContext(datacenter.Get())));
	}

	if (datacenterContextIterator->second.Datacenter == nullptr)
	{
		return S_FALSE;
	}

	if (!datacenterContextIterator->second.Datacenter->IsAuthenticated())
	{
		datacenterContextIterator->second.Flags |= DatacenterRequestContextFlag::RequiresHandshake;
		return S_FALSE;
	}

	if (!(request->EnableUnauthorized() || datacenterId == m_currentDatacenterId || datacenterContextIterator->second.Datacenter->IsAuthorized() || datacenterContextIterator->second.Datacenter->IsCDN()))
	{
		if (m_userId != 0 && datacenterId != m_movingToDatacenterId)
		{
			datacenterContextIterator->second.Flags |= DatacenterRequestContextFlag::RequiresAuthorization;
		}

		return S_FALSE;
	}

	request->SetStartTime(currentTime);
	request->IncrementAttemptCount();

	switch (request->GetConnectionType())
	{
	case ConnectionType::Generic:
	{
		if (m_runningRequestCount[0] >= RUNNING_GENERIC_REQUESTS_MAX_COUNT)
		{
			return S_FALSE;
		}

		datacenterContextIterator->second.GenericRequests.push_back(request);
		return S_OK;
	}
	break;
	case ConnectionType::Download:
	{
		if (m_runningRequestCount[1] >= RUNNING_DOWNLOAD_REQUESTS_MAX_COUNT)
		{
			return S_FALSE;
		}

		I_WANT_TO_DIE_IS_THE_NEW_TODO("Implement Download connection selection");

		HRESULT result;
		ComPtr<Connection> connection;
		ReturnIfFailed(result, datacenterContextIterator->second.Datacenter->GetDownloadConnection(0, true, connection));

		return ProcessConnectionRequest(connection.Get(), request);
	}
	break;
	case ConnectionType::Upload:
	{
		if (m_runningRequestCount[2] >= RUNNING_UPLOAD_REQUESTS_MAX_COUNT)
		{
			return S_FALSE;
		}

		I_WANT_TO_DIE_IS_THE_NEW_TODO("Implement Upload connection selection");

		HRESULT result;
		ComPtr<Connection> connection;
		ReturnIfFailed(result, datacenterContextIterator->second.Datacenter->GetUploadConnection(0, true, connection));

		return ProcessConnectionRequest(connection.Get(), request);
	}
	break;
	default:
		return E_INVALIDARG;
	}
}

HRESULT ConnectionManager::ProcessRequests(std::map<UINT32, DatacenterRequestContext>& datacentersContexts)
{
	HRESULT result = S_OK;
	bool updateDatacenters = false;

	auto datacenterIterator = datacentersContexts.begin();
	while (datacenterIterator != datacentersContexts.end())
	{
		auto& datacenterContext = datacenterIterator->second;

		if (datacenterContext.Datacenter == nullptr)
		{
			if (!updateDatacenters)
			{
				updateDatacenters = true;
				ReturnIfFailed(result, UpdateDatacenters());
			}
		}
		else if ((datacenterContext.Flags & DatacenterRequestContextFlag::RequiresHandshake) == DatacenterRequestContextFlag::RequiresHandshake)
		{
			ReturnIfFailed(result, datacenterContext.Datacenter->BeginHandshake(true, false));
		}
		else
		{
			if ((datacenterContext.Flags & DatacenterRequestContextFlag::RequiresAuthorization) == DatacenterRequestContextFlag::RequiresAuthorization)
			{
				ReturnIfFailed(result, datacenterContext.Datacenter->ImportAuthorization());
			}

			ReturnIfFailed(result, ProcessDatacenterRequests(datacenterContext));
		}

		datacenterIterator = datacentersContexts.erase(datacenterIterator);
	}

	return S_OK;
}

HRESULT ConnectionManager::ProcessDatacenterRequests(DatacenterRequestContext const& datacenterContext)
{
	HRESULT result;
	ComPtr<Connection> connection;
	ReturnIfFailed(result, datacenterContext.Datacenter->GetGenericConnection(true, connection));

	INT64 lastRpcMessageId = 0;
	bool requiresQuickAck = false;

	for (auto& request : datacenterContext.GenericRequests)
	{
		auto messageId = GenerateMessageId();
		request->SetMessageContext({ messageId, connection->GenerateMessageSequenceNumber(true) });

		if (request->InvokeAfter())
		{
			lastRpcMessageId = messageId;
		}

		requiresQuickAck |= request->RequiresQuickAck();
	}

	bool requiresLayer = !datacenterContext.Datacenter->IsConnectionInitialized();
	auto requestCount = datacenterContext.GenericRequests.size();
	std::vector<ComPtr<TLMessage>> transportMessages(requestCount);

	for (size_t i = 0; i < requestCount; i++)
	{
		ReturnIfFailed(result, CreateTransportMessage(datacenterContext.GenericRequests[i].Get(), lastRpcMessageId, requiresLayer, &transportMessages[i]));
	}

	ReturnIfFailed(result, connection->AddConfirmationMessage(this, transportMessages));

	if (transportMessages.empty())
	{
		return S_OK;
	}

	MessageContext messageContext;
	ComPtr<ITLObject> messageBody;

	if (transportMessages.size() > 1)
	{
		auto msgContainer = Make<TLMsgContainer>();
		auto& messages = msgContainer->GetMessages();
		messages.insert(messages.begin(), transportMessages.begin(), transportMessages.end());

		messageContext.Id = GenerateMessageId();
		messageContext.SequenceNumber = connection->GenerateMessageSequenceNumber(false);
		messageBody = msgContainer;
	}
	else
	{
		auto& transportMessage = transportMessages.front();

		CopyMemory(&messageContext, transportMessage->GetMessageContext(), sizeof(MessageContext));
		messageBody = transportMessage->GetQuery();
	}

	if (requiresQuickAck)
	{
		INT32 quickAckId;
		ReturnIfFailed(result, connection->SendEncryptedMessage(&messageContext, messageBody.Get(), &quickAckId));

		auto& quickAckRequests = m_quickAckRequests[quickAckId];
		quickAckRequests.insert(quickAckRequests.begin(), datacenterContext.GenericRequests.begin(), datacenterContext.GenericRequests.end());
	}
	else
	{
		ReturnIfFailed(result, connection->SendEncryptedMessage(&messageContext, messageBody.Get(), nullptr));
	}

	auto datacenterId = datacenterContext.Datacenter->GetId();
	for (auto& request : datacenterContext.GenericRequests)
	{
		m_runningRequestCount[static_cast<UINT32>(request->GetConnectionType()) >> 1]++;
		m_runningRequests.push_back(std::make_pair(datacenterId, std::move(request)));
	}

	return S_OK;
}

HRESULT ConnectionManager::ProcessConnectionRequest(Connection* connection, MessageRequest* request)
{
	auto& datacenter = connection->GetDatacenter();
	auto messageId = GenerateMessageId();
	request->SetMessageContext({ messageId, connection->GenerateMessageSequenceNumber(true) });

	HRESULT result;
	INT64 lastRpcMessageId = 0;
	bool requiresLayer = !datacenter->IsConnectionInitialized();
	std::vector<ComPtr<TLMessage>> transportMessages(1);

	ReturnIfFailed(result, CreateTransportMessage(request, lastRpcMessageId, requiresLayer, &transportMessages[0]));
	ReturnIfFailed(result, connection->AddConfirmationMessage(this, transportMessages));

	MessageContext messageContext;
	ComPtr<ITLObject> messageBody;

	if (transportMessages.size() > 1)
	{
		auto msgContainer = Make<TLMsgContainer>();
		auto& messages = msgContainer->GetMessages();
		messages.insert(messages.begin(), transportMessages.begin(), transportMessages.end());

		messageContext.Id = GenerateMessageId();
		messageContext.SequenceNumber = connection->GenerateMessageSequenceNumber(false);
		messageBody = msgContainer;
	}
	else
	{
		auto& transportMessage = transportMessages.front();

		CopyMemory(&messageContext, transportMessage->GetMessageContext(), sizeof(MessageContext));
		messageBody = transportMessage->GetQuery();
	}

	if (request->RequiresQuickAck())
	{
		INT32 quickAckId;
		ReturnIfFailed(result, connection->SendEncryptedMessage(&messageContext, messageBody.Get(), &quickAckId));

		auto& quickAckRequests = m_quickAckRequests[quickAckId];
		quickAckRequests.insert(quickAckRequests.begin(), request);
	}
	else
	{
		ReturnIfFailed(result, connection->SendEncryptedMessage(&messageContext, messageBody.Get(), nullptr));
	}

	m_runningRequestCount[static_cast<UINT32>(request->GetConnectionType()) >> 1]++;
	m_runningRequests.push_back(std::make_pair(datacenter->GetId(), std::move(request)));
	return S_OK;
}

HRESULT ConnectionManager::CompleteMessageRequest(INT64 requestMessageId, MessageContext const* messageContext, ITLObject* messageBody, Connection* connection)
{
	METHOD_DEBUG();

	ComPtr<MessageRequest> request;

	{
		auto requestsLock = m_requestsCriticalSection.Lock();
		auto requestIterator = std::find_if(m_runningRequests.begin(), m_runningRequests.end(), [&requestMessageId](auto const& request)
		{
			return request.second->MatchesMessage(requestMessageId);
		});

		if (requestIterator == m_runningRequests.end())
		{
			return S_FALSE;
		}

		request = requestIterator->second;
		m_runningRequestCount[static_cast<UINT32>(request->GetConnectionType()) >> 1]--;
		m_runningRequests.erase(requestIterator);
	}

	HRESULT result = S_OK;
	ComPtr<ITLRPCError> rpcError;
	if (SUCCEEDED(messageBody->QueryInterface(IID_PPV_ARGS(&rpcError))))
	{
		INT32 errorCode;
		ReturnIfFailed(result, rpcError->get_Code(&errorCode));

		HString errorText;
		ReturnIfFailed(result, rpcError->get_Text(errorText.GetAddressOf()));

		if ((result = HandleRequestError(connection->GetDatacenter().Get(), request.Get(), errorCode, errorText)) == S_OK)
		{
			auto& sendCompletedCallback = request->GetSendCompletedCallback();
			if (sendCompletedCallback != nullptr)
			{
				auto messageError = Make<MessageError>(errorCode, std::move(errorText));
				auto messageResponse = Make<MessageResponse>(messageContext->Id, request->GetConnectionType(), messageBody);
				return sendCompletedCallback->Invoke(messageResponse.Get(), messageError.Get());
			}

			return S_OK;
		}
		else if (result == S_FALSE)
		{
			request->Reset(false);

			{
				auto requestsLock = m_requestsCriticalSection.Lock();
				m_requestsQueue.push_back(std::move(request));
			}

			FILETIME timeout = {};
			SetThreadpoolTimer(EventObjectT::GetHandle(), &timeout, 0, REQUEST_TIMER_WINDOW);
			return S_OK;
		}
	}

	if (request->IsInitConnection())
	{
		auto& datacenter = connection->GetDatacenter();
		datacenter->SetConnectionInitialized();

		ReturnIfFailed(result, SaveDatacenterSettings(datacenter.Get()));
	}

	auto& sendCompletedCallback = request->GetSendCompletedCallback();
	if (sendCompletedCallback != nullptr)
	{
		auto messageResponse = Make<MessageResponse>(messageContext->Id, request->GetConnectionType(), messageBody);

		if (SUCCEEDED(result))
		{
			return sendCompletedCallback->Invoke(messageResponse.Get(), nullptr);
		}
		else
		{
			ComPtr<MessageError> messageError;
			ReturnIfFailed(result, MakeAndInitialize<MessageError>(&messageError, result));

			return sendCompletedCallback->Invoke(messageResponse.Get(), messageError.Get());
		}
	}

	return S_OK;
}

HRESULT ConnectionManager::HandleRequestError(Datacenter* datacenter, MessageRequest* request, INT32 code, HString const& text)
{
	static const std::wstring knownErrors[] = { L"NETWORK_MIGRATE_", L"PHONE_MIGRATE_", L"USER_MIGRATE_", L"MSG_WAIT_FAILED", L"SESSION_PASSWORD_NEEDED", L"FLOOD_WAIT_" };

	auto lock = LockCriticalSection();
	auto errorText = text.GetRawBuffer(nullptr);

	if (code == 303)
	{
		HRESULT result;
		for (size_t i = 0; i < 3; i++)
		{
			if (wcsstr(errorText, knownErrors[i].c_str()) != nullptr)
			{
				INT32 datacenterId = _wtoi(errorText + knownErrors[i].size());
				ReturnIfFailed(result, MoveToDatacenter(datacenterId));
				return S_FALSE;
			}
		}
	}

	if (request->FailOnServerError())
	{
		return S_OK;
	}

	INT32 waitTime = 0;

	switch (code)
	{
	case 400:
		if (wcsstr(errorText, knownErrors[3].c_str()) == nullptr)
		{
			return S_OK;
		}

		waitTime = 1;
		break;
	case 401:
		if (wcsstr(errorText, knownErrors[4].c_str()) == nullptr)
		{
			if ((datacenter->GetId() == m_currentDatacenterId || datacenter->GetId() == m_movingToDatacenterId) &&
				request->GetConnectionType() == ConnectionType::Generic && m_userId != 0)
			{
				I_WANT_TO_DIE_IS_THE_NEW_TODO("Handle user logout");
			}

			datacenter->SetUnauthorized();
		}
		else
		{
			return S_OK;
		}
		break;
	case 420:
		if (wcsstr(errorText, knownErrors[5].c_str()) == nullptr)
		{
			return S_OK;
		}
		else if ((waitTime = _wtoi(errorText + knownErrors[5].size())) <= 0)
		{
			waitTime = 2;
		}
		break;
	default:
		if (code == 500 || code < 0)
		{
			waitTime = max(request->GetAttemptCount(), 10);
		}
		else
		{
			return S_OK;
		}
		break;
	}

	request->SetStartTime(static_cast<INT32>(GetCurrentSystemTime() / 1000) + waitTime);
	return S_FALSE;
}

void ConnectionManager::ResetRequests(std::map<UINT32, DatacenterRequestContext> const& datacentersContexts)
{
	for (auto& datacenter : datacentersContexts)
	{
		for (auto& request : datacenter.second.GenericRequests)
		{
			request->Reset(true);
			m_requestsQueue.push_back(std::move(request));
		}
	}
}

void ConnectionManager::ResetRequests(std::function<bool(INT32, ComPtr<MessageRequest> const&)> selector, bool resetStartTime)
{
	auto requestsLock = m_requestsCriticalSection.Lock();

	auto requestIterator = m_runningRequests.begin();
	while (requestIterator != m_runningRequests.end())
	{
		if (selector(requestIterator->first, requestIterator->second))
		{
			requestIterator->second->Reset(resetStartTime);

			m_runningRequestCount[static_cast<UINT32>(requestIterator->second->GetConnectionType()) >> 1]--;
			m_requestsQueue.push_back(std::move(requestIterator->second));
			requestIterator = m_runningRequests.erase(requestIterator);
		}
		else
		{
			requestIterator++;
		}
	}
}

HRESULT ConnectionManager::AdjustCurrentTime(INT64 messageId)
{
	auto lock = LockCriticalSection();

	INT64 currentTime = GetCurrentSystemTime();
	INT64 messageTime = static_cast<INT64>((messageId / 4294967296.0) * 1000.0);

	m_timeDifference = static_cast<INT32>((messageTime - currentTime) / 1000LL); // -currentPingTime / 2);
	m_lastOutgoingMessageId = 0;

	return SaveSettings();
}

HRESULT ConnectionManager::OnUnprocessedMessageResponse(MessageContext const* messageContext, ITLObject* messageBody, Connection* connection)
{
	auto unprocessedMessage = Make<MessageResponse>(messageContext->Id, connection->GetType(), messageBody);
	return m_unprocessedMessageReceivedEventSource.InvokeAll(this, unprocessedMessage.Get());
}

HRESULT ConnectionManager::OnNetworkStatusChanged(IInspectable* sender)
{
	auto lock = LockCriticalSection();
	return UpdateNetworkStatus(true);
}

HRESULT ConnectionManager::OnConnectionOpened(Connection* connection)
{
	auto& datacenter = connection->GetDatacenter();

	if (connection->GetType() == ConnectionType::Generic)
	{
		auto lock = LockCriticalSection();

		if (datacenter->GetId() == m_currentDatacenterId && FLAGS_GET_CONNECTIONSTATE(m_flags) != ConnectionState::Connected)
		{
			m_flags = FLAGS_SET_CONNECTIONSTATE(m_flags, ConnectionState::Connected);

			HRESULT result;
			ReturnIfFailed(result, m_connectionStateChangedEventSource.InvokeAll(this, nullptr));
		}
	}

	if (datacenter->IsAuthenticated())
	{
		return ProcessRequestsForDatacenter(datacenter.Get(), connection->GetType());
	}

	return S_OK;
}

HRESULT ConnectionManager::OnConnectionClosed(Connection* connection)
{
	if (connection->GetType() == ConnectionType::Generic)
	{
		auto lock = LockCriticalSection();
		auto datacenter = connection->GetDatacenter();

		if (datacenter->GetId() == m_currentDatacenterId)
		{
			if (static_cast<ConnectionNeworkType>(m_flags & ConnectionManagerFlag::NetworkType) == ConnectionNeworkType::None)
			{
				if (FLAGS_GET_CONNECTIONSTATE(m_flags) != ConnectionState::WaitingForNetwork)
				{
					m_flags = FLAGS_SET_CONNECTIONSTATE(m_flags, ConnectionState::WaitingForNetwork);
					return m_connectionStateChangedEventSource.InvokeAll(this, nullptr);
				}
			}
			else
			{
				if (FLAGS_GET_CONNECTIONSTATE(m_flags) != ConnectionState::Connecting)
				{
					m_flags = FLAGS_SET_CONNECTIONSTATE(m_flags, ConnectionState::Connecting);
					return m_connectionStateChangedEventSource.InvokeAll(this, nullptr);
				}
			}
		}
	}

	return S_OK;
}

HRESULT ConnectionManager::OnDatacenterHandshakeComplete(Datacenter* datacenter, INT32 timeDifference)
{
	{
		auto lock = LockCriticalSection();

		if (datacenter->GetId() == m_currentDatacenterId || datacenter->GetId() == m_movingToDatacenterId)
		{
			m_timeDifference = timeDifference;

			HRESULT result;
			ReturnIfFailed(result, SaveSettings());

			datacenter->RecreateSessions();

			ResetRequests([datacenter](INT32 datacenterId, auto& request) -> boolean
			{
				return datacenterId == datacenter->GetId();
			}, true);
		}
	}

	return ProcessRequestsForDatacenter(datacenter, ConnectionType::Generic | ConnectionType::Download | ConnectionType::Upload);
}

HRESULT ConnectionManager::OnDatacenterImportAuthorizationComplete(Datacenter* datacenter)
{
	return ProcessRequestsForDatacenter(datacenter, ConnectionType::Generic | ConnectionType::Download | ConnectionType::Upload);
}

HRESULT ConnectionManager::OnDatacenterBadServerSalt(Datacenter* datacenter, INT64 requestMessageId, INT64 responseMessageId)
{
	HRESULT result;
	ReturnIfFailed(result, AdjustCurrentTime(responseMessageId));

	ResetRequests([datacenter](auto datacenterId, auto const& request) -> boolean
	{
		return datacenterId == datacenter->GetId();
	}, true);

	ReturnIfFailed(result, datacenter->RequestFutureSalts(32));

	if (datacenter->IsAuthenticated())
	{
		return ProcessRequestsForDatacenter(datacenter, ConnectionType::Generic | ConnectionType::Download | ConnectionType::Upload);
	}

	return S_OK;
}

HRESULT ConnectionManager::OnDatacenterBadMessage(Datacenter* datacenter, INT64 requestMessageId, INT64 responseMessageId)
{
	HRESULT result;
	ReturnIfFailed(result, AdjustCurrentTime(responseMessageId));

	ResetRequests([datacenter](auto datacenterId, auto const& request) -> boolean
	{
		return datacenterId == datacenter->GetId();
	}, true);

	if (datacenter->IsAuthenticated())
	{
		return ProcessRequestsForDatacenter(datacenter, ConnectionType::Generic | ConnectionType::Download | ConnectionType::Upload);
	}

	return S_OK;
}

HRESULT ConnectionManager::OnConnectionQuickAckReceived(Connection* connection, INT32 ackId)
{
	auto requestsLock = m_requestsCriticalSection.Lock();

	auto requestsIterator = m_quickAckRequests.find(ackId);
	if (requestsIterator == m_quickAckRequests.end())
	{
		return S_FALSE;
	}

	HRESULT result;
	for (auto& request : requestsIterator->second)
	{
		auto& quickAckReceivedCallback = request->GetQuickAckReceivedCallback();
		if (quickAckReceivedCallback != nullptr)
		{
			ReturnIfFailed(result, quickAckReceivedCallback->Invoke());
		}
	}

	m_quickAckRequests.erase(requestsIterator);
	return S_OK;
}

HRESULT ConnectionManager::OnConnectionSessionCreated(Connection* connection, INT64 firstMessageId)
{
	auto lock = LockCriticalSection();
	auto datacenter = connection->GetDatacenter();

	ResetRequests([datacenter, connection, firstMessageId](auto datacenterId, auto const& request) -> boolean
	{
		return datacenterId == datacenter->GetId() && request->MatchesConnection(connection->GetType()) && request->GetMessageContext()->Id < firstMessageId;
	}, true);

	if (connection->GetType() == ConnectionType::Generic && datacenter->GetId() == m_currentDatacenterId && m_userId != 0)
	{
		return m_sessionCreatedEventSource.InvokeAll(this, nullptr);
	}

	return S_OK;
}

HRESULT ConnectionManager::OnEvent(PTP_CALLBACK_INSTANCE callbackInstance, ULONG_PTR param)
{
	SetThreadpoolTimer(EventObjectT::GetHandle(), nullptr, 0, 0);

	return ProcessRequests();
}

HRESULT ConnectionManager::BoomBaby(IUserConfiguration* userConfiguration, ITLObject** object, IConnection** value)
{
	if (object == nullptr || value == nullptr)
	{
		return E_POINTER;
	}

	auto lock = LockCriticalSection();

	HRESULT result;
	ReturnIfFailed(result, SaveSettings());

	for (auto& datacenter : m_datacenters)
	{
		std::wstring settingsFileName;
		GetDatacenterSettingsFileName(datacenter.first, settingsFileName);

		ComPtr<TLFileBinaryWriter> settingsWriter;
		ReturnIfFailed(result, MakeAndInitialize<TLFileBinaryWriter>(&settingsWriter, settingsFileName.data(), CREATE_ALWAYS));
		ReturnIfFailed(result, datacenter.second->SaveSettings(settingsWriter.Get()));
	}

	ComPtr<Datacenter> datacenter;
	GetDatacenterById(m_currentDatacenterId, datacenter);
	/*ReturnIfFailed(result, datacenter->RequestFutureSalts(10));*/

	return datacenter->SendPing();
}

bool ConnectionManager::GetDatacenterById(UINT32 id, ComPtr<Datacenter>& datacenter)
{
	auto datacenterIterator = m_datacenters.find(id);
	if (datacenterIterator == m_datacenters.end())
	{
		return false;
	}

	datacenter = datacenterIterator->second;
	return true;
}

bool ConnectionManager::GetRequestByMessageId(INT64 messageId, ComPtr<MessageRequest>& request)
{
	auto requestsLock = m_requestsCriticalSection.Lock();
	auto requestIterator = std::find_if(m_runningRequests.begin(), m_runningRequests.end(), [&messageId](auto const& request)
	{
		return request.second->MatchesMessage(messageId);
	});

	if (requestIterator == m_runningRequests.end())
	{
		return false;
	}

	request = requestIterator->second;
	return true;
}

bool ConnectionManager::GetCDNPublicKey(INT32 datacenterId, std::vector<INT64> const& fingerprints, ServerPublicKey const** publicKey)
{
	auto lock = LockCriticalSection();
	auto cdnPublicKeyIterator = m_cdnPublicKeys.find(datacenterId);
	if (cdnPublicKeyIterator == m_cdnPublicKeys.end())
	{
		return false;
	}

	for (size_t i = 0; i < fingerprints.size(); i++)
	{
		if (fingerprints[i] == cdnPublicKeyIterator->second.Fingerprint)
		{
			*publicKey = &cdnPublicKeyIterator->second;
			return true;
		}
	}

	return false;
}

INT64 ConnectionManager::GenerateMessageId()
{
	auto lock = LockCriticalSection();
	auto messageId = static_cast<INT64>(((static_cast<double>(GetCurrentSystemTime()) + static_cast<double>(m_timeDifference) * 1000) * 4294967296.0) / 1000.0);
	if (messageId <= m_lastOutgoingMessageId)
	{
		messageId = m_lastOutgoingMessageId + 1;
	}

	while ((messageId % 4) != 0)
	{
		messageId++;
	}

	m_lastOutgoingMessageId = messageId;
	return messageId;
}

INT32 ConnectionManager::GetCurrentTime()
{
	auto lock = LockCriticalSection();
	return static_cast<INT32>(GetCurrentSystemTime() / 1000) + m_timeDifference;
}

HRESULT ConnectionManager::InitializeSettings()
{
	HRESULT result;
	ComPtr<IApplicationDataStatics> applicationDataStatics;
	ReturnIfFailed(result, Windows::Foundation::GetActivationFactory(HStringReference(RuntimeClass_Windows_Storage_ApplicationData).Get(), &applicationDataStatics));

	ComPtr<IApplicationData> applicationData;
	ReturnIfFailed(result, applicationDataStatics->get_Current(&applicationData));

	ComPtr<IStorageFolder> localStorageFolder;
	ReturnIfFailed(result, applicationData->get_LocalFolder(&localStorageFolder));

	ComPtr<IStorageItem> localStorageItem;
	ReturnIfFailed(result, localStorageFolder.As(&localStorageItem));

	HString localStorageFolderPath;
	ReturnIfFailed(result, localStorageItem->get_Path(localStorageFolderPath.GetAddressOf()));

	m_settingsFolderPath.resize(MAX_PATH);
	m_settingsFolderPath.resize(swprintf_s(&m_settingsFolderPath[0], MAX_PATH, L"%s\\Telegram.Api.Native", localStorageFolderPath.GetRawBuffer(nullptr)));

	CreateDirectory(m_settingsFolderPath.data(), nullptr);

	if (LoadSettings() != S_OK)
	{
		ReturnIfFailed(result, InitializeDefaultDatacenters());
		ReturnIfFailed(result, SaveSettings());
	}

	return LoadCDNPublicKeys();
}

HRESULT ConnectionManager::LoadSettings()
{
	std::wstring settingsFileName = m_settingsFolderPath + L"\\Settings.dat";

	HRESULT result;
	ComPtr<TLFileBinaryReader> settingsReader;
	if (FAILED(result = MakeAndInitialize<TLFileBinaryReader>(&settingsReader, settingsFileName.data(), OPEN_EXISTING)))
	{
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			return S_FALSE;
		}

		return result;
	}

	UINT32 version;
	ReturnIfFailed(result, settingsReader->ReadUInt32(&version));

	if (version != TELEGRAM_API_NATIVE_SETTINGS_VERSION)
	{
		return COMADMIN_E_APP_FILE_VERSION;
	}

	INT32 currentDatacenterId;
	ReturnIfFailed(result, settingsReader->ReadInt32(&currentDatacenterId));

	INT32 datacentersExpirationTime;
	ReturnIfFailed(result, settingsReader->ReadInt32(&datacentersExpirationTime));

	INT32 timeDifference;
	ReturnIfFailed(result, settingsReader->ReadInt32(&timeDifference));

	UINT32 datacenterCount;
	ReturnIfFailed(result, settingsReader->ReadUInt32(&datacenterCount));

	std::vector<ComPtr<Datacenter>> datacenters;

	for (UINT32 i = 0; i < datacenterCount; i++)
	{
		INT32 datacenterId;
		ReturnIfFailed(result, settingsReader->ReadInt32(&datacenterId));

		std::wstring settingsFileName;
		GetDatacenterSettingsFileName(datacenterId, settingsFileName);

		ComPtr<TLFileBinaryReader> datacenterSettingsReader;
		if (FAILED(result = MakeAndInitialize<TLFileBinaryReader>(&datacenterSettingsReader, settingsFileName.data(), OPEN_EXISTING)))
		{
			if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
			{
				continue;
			}

			return result;
		}

		if (SUCCEEDED(MakeAndInitialize<TLFileBinaryReader>(&datacenterSettingsReader, settingsFileName.data(), OPEN_EXISTING)))
		{
			ComPtr<Datacenter> datacenter;
			ReturnIfFailed(result, MakeAndInitialize<Datacenter>(&datacenter, this, datacenterSettingsReader.Get()));

			datacenters.push_back(std::move(datacenter));
		}
	}

	if (datacenters.empty())
	{
		return E_FAIL;
	}

	//auto lock = LockCriticalSection();

	m_currentDatacenterId = currentDatacenterId;
	m_movingToDatacenterId = DEFAULT_DATACENTER_ID;
	m_datacentersExpirationTime = datacentersExpirationTime;
	m_timeDifference = timeDifference;

	for (auto& datacenter : m_datacenters)
	{
		datacenter.second->Close();
	}

	m_datacenters.clear();

	for (auto& datacenter : datacenters)
	{
		m_datacenters[datacenter->GetId()] = std::move(datacenter);
	}

	return S_OK;
}

HRESULT ConnectionManager::SaveSettings()
{
	std::wstring settingsFileName = m_settingsFolderPath + L"\\Settings.dat";

	//auto lock = LockCriticalSection();

	HRESULT result;
	ComPtr<TLFileBinaryWriter> settingsWriter;
	ReturnIfFailed(result, MakeAndInitialize<TLFileBinaryWriter>(&settingsWriter, settingsFileName.data(), CREATE_ALWAYS));
	ReturnIfFailed(result, settingsWriter->WriteUInt32(TELEGRAM_API_NATIVE_SETTINGS_VERSION));
	ReturnIfFailed(result, settingsWriter->WriteInt32(m_currentDatacenterId));
	ReturnIfFailed(result, settingsWriter->WriteInt32(m_datacentersExpirationTime));
	ReturnIfFailed(result, settingsWriter->WriteInt32(m_timeDifference));
	ReturnIfFailed(result, settingsWriter->WriteUInt32(static_cast<UINT32>(m_datacenters.size())));

	for (auto& datacenter : m_datacenters)
	{
		ReturnIfFailed(result, settingsWriter->WriteInt32(datacenter.first));
	}

	return S_OK;
}

HRESULT ConnectionManager::LoadCDNPublicKeys()
{
	std::wstring publicKeysFileName = m_settingsFolderPath + L"\\CDN_PublicKeys.dat";

	HRESULT result;
	ComPtr<TLFileBinaryReader> publicKeysReader;
	if (FAILED(result = MakeAndInitialize<TLFileBinaryReader>(&publicKeysReader, publicKeysFileName.data(), OPEN_EXISTING)))
	{
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			return S_FALSE;
		}

		return result;
	}

	UINT32 version;
	ReturnIfFailed(result, publicKeysReader->ReadUInt32(&version));

	if (version != TELEGRAM_API_NATIVE_SETTINGS_VERSION)
	{
		return COMADMIN_E_APP_FILE_VERSION;
	}

	UINT32 cdnPublicKeyCount;
	ReturnIfFailed(result, publicKeysReader->ReadUInt32(&cdnPublicKeyCount));

	std::vector<std::pair<INT32, ServerPublicKey>> cdnPublicKeys(cdnPublicKeyCount);
	for (UINT32 i = 0; i < cdnPublicKeyCount; i++)
	{
		ReturnIfFailed(result, publicKeysReader->ReadInt32(&cdnPublicKeys[i].first));
		ReturnIfFailed(result, publicKeysReader->ReadInt64(&cdnPublicKeys[i].second.Fingerprint));

		BYTE const* buffer;
		UINT32 length;
		ReturnIfFailed(result, publicKeysReader->ReadBuffer2(&buffer, &length));

		Wrappers::BIO keyBio(BIO_new_mem_buf(buffer, length));
		cdnPublicKeys[i].second.Key.Attach(PEM_read_bio_RSAPublicKey(keyBio.Get(), nullptr, nullptr, nullptr));
	}

	//auto lock = LockCriticalSection();

	m_cdnPublicKeys.clear();

	for (auto& publicKey : cdnPublicKeys)
	{
		m_cdnPublicKeys[publicKey.first] = std::move(publicKey.second);
	}

	return S_OK;
}

HRESULT ConnectionManager::SaveCDNPublicKeys()
{
	std::wstring publicKeysFileName = m_settingsFolderPath + L"\\CDN_PublicKeys.dat";

	//auto lock = LockCriticalSection();

	HRESULT result;
	ComPtr<TLFileBinaryWriter> publicKeysWriter;
	ReturnIfFailed(result, MakeAndInitialize<TLFileBinaryWriter>(&publicKeysWriter, publicKeysFileName.data(), CREATE_ALWAYS));
	ReturnIfFailed(result, publicKeysWriter->WriteUInt32(TELEGRAM_API_NATIVE_SETTINGS_VERSION));
	ReturnIfFailed(result, publicKeysWriter->WriteUInt32(static_cast<UINT32>(m_cdnPublicKeys.size())));

	for (auto& publicKey : m_cdnPublicKeys)
	{
		ReturnIfFailed(result, publicKeysWriter->WriteInt32(publicKey.first));
		ReturnIfFailed(result, publicKeysWriter->WriteInt64(publicKey.second.Fingerprint));

		Wrappers::BIO keyBio(BIO_new(BIO_s_mem()));
		PEM_write_bio_RSAPublicKey(keyBio.Get(), publicKey.second.Key.Get());

		std::vector<BYTE> buffer(BIO_pending(keyBio.Get()));
		BIO_read(keyBio.Get(), buffer.data(), static_cast<int>(buffer.size()));

		ReturnIfFailed(result, publicKeysWriter->WriteBuffer(buffer.data(), static_cast<UINT32>(buffer.size())));
	}

	return S_OK;
}

HRESULT ConnectionManager::SaveDatacenterSettings(Datacenter* datacenter)
{
	std::wstring settingsFileName;
	GetDatacenterSettingsFileName(datacenter->GetId(), settingsFileName);

	HRESULT result;
	ComPtr<TLFileBinaryWriter> settingsWriter;
	ReturnIfFailed(result, MakeAndInitialize<TLFileBinaryWriter>(&settingsWriter, settingsFileName.data(), CREATE_ALWAYS));

	return datacenter->SaveSettings(settingsWriter.Get());
}


HRESULT ConnectionManagerStatics::RuntimeClassInitialize()
{
	return MakeAndInitialize<ConnectionManager>(&m_instance);
}

HRESULT ConnectionManagerStatics::get_Instance(IConnectionManager** value)
{
	if (value == nullptr)
	{
		return E_POINTER;
	}

	return m_instance.CopyTo(value);
}

HRESULT ConnectionManagerStatics::get_Version(Version* value)
{
	if (value == nullptr)
	{
		return E_POINTER;
	}

	value->ProtocolVersion = TELEGRAM_API_NATIVE_PROTOVERSION;
	value->Layer = TELEGRAM_API_NATIVE_LAYER;
	value->ApiId = TELEGRAM_API_NATIVE_APIID;
	value->SettingsVersion = TELEGRAM_API_NATIVE_SETTINGS_VERSION;
	return S_OK;
}

HRESULT ConnectionManagerStatics::get_DefaultDatacenterId(INT32* value)
{
	if (value == nullptr)
	{
		return E_POINTER;
	}

	*value = DEFAULT_DATACENTER_ID;
	return S_OK;
}