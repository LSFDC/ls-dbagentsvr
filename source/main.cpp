// ls_dbagent.cpp : �ܼ� ���� ���α׷��� ���� �������� �����մϴ�.
//

#include "stdafx.h"
#include "ServiceLS.h"

int _tmain(int argc, _TCHAR* argv[])
{
	ServiceLS *service = new ServiceLS(argc, argv);
	service->ServiceMainProc();
	delete service;
	return 0;
}

