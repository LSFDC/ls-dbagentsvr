// stdafx.h : ���� ��������� ���� ��������� �ʴ�
// ǥ�� �ý��� ���� ���� �� ������Ʈ ���� ���� ������
// ��� �ִ� ���� �����Դϴ�.
//

#pragma once

#include "targetver.h"

#include <winsock2.h>
#include <stdio.h>
#include <tchar.h>
#include "DBHeader.h"
#include <atldbcli.h>
#include <mmsystem.h> 
#include "../include/common.h"

extern void Trace( const char *format, ... );
extern void Debug( const char *format, ... );
extern void Information( const char *format, ... ) ;
extern void Debug();

// TODO: ���α׷��� �ʿ��� �߰� ����� ���⿡�� �����մϴ�.
