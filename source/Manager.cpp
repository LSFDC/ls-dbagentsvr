#include "StdAfx.h"
#include "MainProcess.h"
#include "ThreadPool/ioThreadPool.h"
#include "Network/DBServer.h"
#include "Network/iocpHandler.h"
#include "Network/ioPacketQueue.h"
#include "Network/ioLogServer.h"
#include "NodeInfo/UserNodeManager.h"
#include "NodeInfo/LogNodeManager.h"
#include "Database/cQueryManager.h"
#include "Manager.h"
#include "../ioINILoader/ioINILoader.h"
#include <iphlpapi.h>
#include <ws2tcpip.h>


BOOL ConsoleHandler(DWORD fdwCtrlType) 
{ 
	switch (fdwCtrlType) 
	{ 
		// Handle the CTRL+C signal. 
	case CTRL_C_EVENT: 
	case CTRL_CLOSE_EVENT: // CTRL+CLOSE: confirm! that the user wants to exit. 
	case CTRL_BREAK_EVENT: 
	case CTRL_LOGOFF_EVENT: 
	case CTRL_SHUTDOWN_EVENT: 
	default: 
	 g_MainProc.Save();
		return FALSE;
	} 
	return TRUE;
}	

Manager::Manager(void)
{
	Init();
}

Manager::~Manager(void)
{
	Destroy();
}

void Manager::Init()
{
}

void Manager::Destroy()
{
}

BOOL Manager::Run(const char* scriptName)
{
	Startup(scriptName);

	if( !SetConsoleCtrlHandler( (PHANDLER_ROUTINE)ConsoleHandler, TRUE) ) 
		return FALSE; 
		
	if(!LoadINI())			return FALSE;
	if(!CreateDB())			return FALSE;
	if(!ListenNetwork())	return FALSE;
	if(!CreateUserPool())	return FALSE;
	if(!Prepare())			return FALSE;
	if(!StartModules())		return FALSE;

	Timer();
	return TRUE;
}

void Manager::Startup(const char* scriptName)
{
	timeBeginPeriod(1);

	g_MainProc.SetINI(scriptName);
}

BOOL Manager::LoadINI()
{
	//---------------------------------
	//        ini 정보
	//---------------------------------
	const char* szINI = g_MainProc.GetINI().c_str();

	m_logSvrPort = GetPrivateProfileInt("Default", "LogServerPort", 0, szINI);	 //로그 서버별로 
	(m_logSvrPort == 0) ? g_MainProc.SetLoggerState(FALSE) : g_MainProc.SetLoggerState(TRUE);

	m_port = GetPrivateProfileInt("Default", "Port", 10000, szINI);	
	TCHAR szLog[256];
	GetPrivateProfileString("Default", "Log", "MLOG", szLog, sizeof(szLog), szINI);	
	g_MainProc.SetLogFolder( szLog );

	if(g_MainProc.GetLoggerState() == FALSE)
	{
		Information("Initializing database configuration...");
		GetPrivateProfileString("DATABASE", "Host", "", m_dbIP, sizeof(m_dbIP), szINI);	
		GetPrivateProfileString("DATABASE", "Name", "", m_dbName, sizeof(m_dbName), szINI);	
		GetPrivateProfileString("DATABASE", "Username", "", m_dbID, sizeof(m_dbID), szINI);	
		GetPrivateProfileString("DATABASE", "Password", "", m_dbPwd, sizeof(m_dbPwd), szINI);
		m_dbPort = GetPrivateProfileInt("DATABASE", "Port", 1433, szINI);

		if (strcmp(m_dbIP, "") == 0 || strcmp(m_dbName, "") == 0 || strcmp(m_dbID, "") == 0 || strcmp(m_dbPwd, "") == 0)
		{
			Information("failed\n");
			m_error = 0x1000;
			return FALSE;
		}

		string tempdbpw(m_dbPwd);
		TCHAR encoding[512] = {};

		g_queryManager.Encode(
			tempdbpw.c_str(),
			tempdbpw.size(),
			encoding,
			sizeof(encoding));
		strcpy_s(m_dbPwd, encoding);

		WritePrivateProfileString("DATABASE", "Password", m_dbPwd, szINI);

		
		Information("done\n");
	}
	g_MainProc.LogCurrentStates( 0 );
	return TRUE;
}

BOOL Manager::CreateDB()
{
	if(g_MainProc.GetLoggerState())
		return TRUE;
	 
	Information( "Creating Database Pool..." );
	//--------------------------------
	//     DB 접속
	//--------------------------------

	if(!g_MainProc.LoadQuery())
	{
		m_error = 0x1002;
		return FALSE;
	}

	if(!g_queryManager.AddDatabase(1, m_dbIP, m_dbPort, m_dbName, m_dbID, m_dbPwd))
	{
		m_error = 0x1002;
		return FALSE;
	}
	Information( "done\n" );
	return TRUE;
}

BOOL Manager::CreateUserPool()
{
	if(!g_MainProc.GetLoggerState())
		g_UserNodeManager.InitMemoryPool();
	else
		g_LogNodeManager->InitMemoryPool();
	return TRUE;
}

BOOL Manager::ListenNetwork()
{
	
	g_RecvQueue.Initialize();

	//
	m_logicThread = new LogicThread;
	m_logicThread->SetProcessor(&(g_MainProc));

	// 메인프로세스와 로직스레드 구동
	if( !g_iocp.Initialize() )
	{
		m_error = 0x1003;
		return FALSE;
	}

	//--------------------------------
	//     SOCKET Init
	//--------------------------------
	if( !BeginSocket() )
	{
		m_error = 0x1004;
		return FALSE;
	}

	// SET IP
	char szCurrentPath[MAX_PATH] = "";
	GetCurrentDirectory(MAX_PATH, szCurrentPath);
	strcat_s(szCurrentPath, "\\ls_config_dba.ini");

	ioHashString iPrivateIPFirstByte;
	GetPrivateProfileString("NETWORK", "SocketIP", "", (char*)iPrivateIPFirstByte.c_str(), sizeof(iPrivateIPFirstByte), szCurrentPath);

	if (strcmp(iPrivateIPFirstByte.c_str(), "") == 0)
	{
		MessageBox(NULL, "Socket IP cannot be empty !", "IOEnter", MB_ICONERROR);
		return FALSE;
	}


	if( !SetLocalIP2(iPrivateIPFirstByte) )
	{
		m_error = 0x1005;
		return FALSE;
	}
	
	Information( "Initializing Socket ...");

	if(!g_MainProc.GetLoggerState())
	{
		m_pDBServer = new ioDBServer; 
		if( !m_pDBServer->Start( (char*)m_szPrivateIP.c_str(), m_port ) )
		{
			Information( "failed\n" );
			m_error = 0x1006;
			return FALSE;
		}
	}

	else 
	{
		m_pLogServer = new ioLogServer;
		if( !m_pLogServer->Start( (char*)m_szPrivateIP.c_str(), m_logSvrPort) )
		{
			Information( "fialed\n");
			m_error = 0x1006;
			return FALSE;
		}
		LOG.PrintTimeAndLog(0,"Logger(%s:%d) Start done",m_szPrivateIP.c_str(),m_logSvrPort);
	}
	Information( "done\n" );

	return TRUE;
}

BOOL Manager::StartModules()
{
	if(!g_MainProc.GetLoggerState())
	{
		Information( "Starting Database ThreadPool..." );
		if(g_ThreadPool.Initialize() == FALSE)
		{
			Information( "failed\n" );
			m_error = 0x1008;
			return FALSE;
		}
		Information( "done\n" );
	}

	Information( "Starting processor..." );
	if(!m_logicThread->Begin())
	{
		Information( "failed\n" );
		return FALSE;
	}
	Information( "done\n" );

	return TRUE;
}

BOOL Manager::Prepare()
{
	char szTitle[512] ="";
	if(!g_MainProc.GetLoggerState())
	{
		wsprintf( szTitle, "%s | %s:%d", m_dbName, m_dbIP, m_dbPort );
	}
	else
	{
		wsprintf( szTitle, "ls_logger %s:%d", m_szPrivateIP.c_str(), m_port );
	}
	
	if(!g_MainProc.Initialize( szTitle ))
	{
		m_error = 0x1001;
		return FALSE;
	}

	return TRUE;
}

void Manager::Timer()
{
	Information( " >> %s service running on %s:%d\n", m_dbName, m_szPrivateIP.c_str(), m_port );
	while(TRUE)
	{
		Sleep(10000);
	}
}

bool Manager::GetLocalIpAddressList(ioHashStringVec& rvIPList) {

	char szHostName[MAX_PATH];
	ZeroMemory(szHostName, sizeof(szHostName));
	gethostname(szHostName, sizeof(szHostName));

	LPHOSTENT lpstHostent = gethostbyname(szHostName);
	if (!lpstHostent) {
		m_error = 0x1009;
		return false;
	}

	enum { MAX_LOOP = 100 };
	LPIN_ADDR lpstInAddr = NULL;
	if (lpstHostent->h_addrtype == AF_INET) {

		for (int i = 0; i < MAX_LOOP; i++) {
			lpstInAddr = (LPIN_ADDR)lpstHostent->h_addr_list[i];

			if (lpstInAddr == NULL)
				break;

			char szTemp[MAX_PATH] = "";
			strcpy_s(szTemp, sizeof(szTemp), inet_ntoa(*lpstInAddr));
			ioHashString sTemp = szTemp;
			rvIPList.push_back(sTemp);
		}
	}

	if (rvIPList.empty()) {
		m_error = 0x1010;
		return false;
	}

	return true;
}

#pragma comment(lib, "iphlpapi.lib")

bool Manager::GetLocalIpAddressList2(ioHashStringVec& rvIPList) {


	ULONG bufferSize = 0;
	if (GetAdaptersAddresses(AF_INET, 0, NULL, NULL, &bufferSize) != ERROR_BUFFER_OVERFLOW) {

		Information("GetAdaptersAddresses failed to get buffer size");
		return false;
	}

	IP_ADAPTER_ADDRESSES* pAdapterAddresses = (IP_ADAPTER_ADDRESSES*)malloc(bufferSize);
	if (!pAdapterAddresses) {

		Information("Memory allocation failed");

		return false;
	}

	if (GetAdaptersAddresses(AF_INET, 0, NULL, pAdapterAddresses, &bufferSize) != ERROR_SUCCESS) {

		Information("GetAdaptersAddresses failed");

		free(pAdapterAddresses);
		return false;
	}


	for (IP_ADAPTER_ADDRESSES* pAdapter = pAdapterAddresses; pAdapter; pAdapter = pAdapter->Next) {
		IP_ADAPTER_UNICAST_ADDRESS* pUnicast = pAdapter->FirstUnicastAddress;
		while (pUnicast) {
			SOCKADDR_IN* pSockAddr = (SOCKADDR_IN*)pUnicast->Address.lpSockaddr;
			if (pSockAddr->sin_family == AF_INET) {
				char szTemp[MAX_PATH] = "";
				strcpy_s(szTemp, sizeof(szTemp), inet_ntoa(pSockAddr->sin_addr));
				ioHashString sTemp = szTemp;
				rvIPList.push_back(sTemp);
				
			}
			pUnicast = pUnicast->Next;
		}
	}

	free(pAdapterAddresses);

	return true;
}


bool Manager::SetLocalIP(int iPrivateIPFirstByte)
{
	ioHashStringVec vIPList;
	if (!GetLocalIpAddressList(vIPList))
		return false;

	int iSize = vIPList.size();

	// 1, 2 아니면 에러
	if (!COMPARE(iSize, 1, 3))
	{
		LOG.PrintTimeAndLog(0, "%s Size Error %d", __FUNCTION__, iSize);
		return false;
	}

	// 1
	if (iSize == 1)
	{
		m_szPublicIP = vIPList[0];
		m_szPrivateIP = vIPList[0];

		if (m_szPrivateIP.IsEmpty() || m_szPublicIP.IsEmpty())
		{
			LOG.PrintTimeAndLog(0, "%s Local IP Error %s:%s", __FUNCTION__, m_szPrivateIP.c_str(), m_szPublicIP.c_str());
			return false;
		}

		return true;
	}

	// 2
	for (int i = 0; i < iSize; i++)
	{
		if (atoi(vIPList[i].c_str()) != iPrivateIPFirstByte)
		{
			m_szPublicIP = vIPList[i];
		}
		else
		{
			m_szPrivateIP = vIPList[i];
		}
	}

	if (m_szPrivateIP.IsEmpty() || m_szPublicIP.IsEmpty())
	{
		LOG.PrintTimeAndLog(0, "[error][main]%s Local IP Empty %s:%s", __FUNCTION__, m_szPrivateIP.c_str(), m_szPublicIP.c_str());
		return false;
	}

	return true;
}

bool Manager::SetLocalIP2(ioHashString iPrivateIPFirstByte)
{
	ioHashStringVec vIPList;

	if (!GetLocalIpAddressList2(vIPList))
	{
		m_error = 0x1011;
		return false;
	}

	int iSize = vIPList.size();

	if (iSize == 0)
	{
		m_error = 0x1012;
		return false;
	}

	if (iSize == 1)
	{
		m_szPublicIP = vIPList[0];
		m_szPrivateIP = vIPList[0];

		if (m_szPrivateIP.IsEmpty() || m_szPublicIP.IsEmpty())
		{
			LOG.PrintTimeAndLog(0, "%s Local IP Error %s:%s", __FUNCTION__, m_szPrivateIP.c_str(), m_szPublicIP.c_str());
			return false;
		}

		return true;
	}

	for (int i = 0; i < iSize; i++)
	{
		if (vIPList[i] == iPrivateIPFirstByte.c_str())
		{
			m_szPublicIP = vIPList[i];
			m_szPrivateIP = vIPList[i];
			break;
		}
		else
		{
			printf("Socket IP doesn't match with any IP : %s \n", iPrivateIPFirstByte.c_str());
			return false;
		}
	}

	if (m_szPrivateIP.IsEmpty() || m_szPublicIP.IsEmpty())
	{
		m_error = 0x1015;
		return false;
	}

	return true;
}
