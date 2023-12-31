// stdafx.h : 자주 사용하지만 자주 변경되지는 않는
// 표준 시스템 포함 파일 및 프로젝트 관련 포함 파일이
// 들어 있는 포함 파일입니다.
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

// TODO: 프로그램에 필요한 추가 헤더는 여기에서 참조합니다.
