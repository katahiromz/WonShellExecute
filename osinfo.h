// osinfo.h --- OS information
// Author: katahiromz
// License: MIT

#pragma once

#ifndef OS_WIN32S
    #define OS_WIN32S 0x00
#endif
#ifndef OS_NT
    #define OS_NT 0x01
#endif
#ifndef OS_WIN9XORNT
    #define OS_WIN9XORNT 0x02
#endif
#ifndef OS_NTORGREATER
    #define OS_NTORGREATER 0x03
#endif
#ifndef OS_WIN2000ORGREATER
    #define OS_WIN2000ORGREATER 0x04
#endif
#ifndef OS_WIN98ORGREATER
    #define OS_WIN98ORGREATER 0x05
#endif
#ifndef OS_WIN98_GOLD
    #define OS_WIN98_GOLD 0x06
#endif
#ifndef OS_NT5ORGREATER
    #define OS_NT5ORGREATER 0x07
#endif
#ifndef OS_WIN2000PRO
    #define OS_WIN2000PRO 0x08
#endif
#ifndef OS_WIN2000SERVER
    #define OS_WIN2000SERVER 0x09
#endif
#ifndef OS_WIN2000ADVSERVER
    #define OS_WIN2000ADVSERVER 0x0A
#endif
#ifndef OS_WIN2000DATACENTER
    #define OS_WIN2000DATACENTER 0x0B
#endif
#ifndef OS_WIN2000TERMINAL
    #define OS_WIN2000TERMINAL 0x0C
#endif
#ifndef OS_EMBEDDED
    #define OS_EMBEDDED 0x0D
#endif
#ifndef OS_TERMINALCLIENT
    #define OS_TERMINALCLIENT 0x0E
#endif
#ifndef OS_TERMINALREMOTEADMIN
    #define OS_TERMINALREMOTEADMIN 0x0F
#endif
#ifndef OS_WIN95_GOLD
    #define OS_WIN95_GOLD 0x10
#endif
#ifndef OS_MEORGREATER
    #define OS_MEORGREATER 0x11
#endif
#ifndef OS_XPORGREATER
    #define OS_XPORGREATER 0x12
#endif
#ifndef OS_HOME
    #define OS_HOME 0x13
#endif
#ifndef OS_PROFESSIONAL
    #define OS_PROFESSIONAL 0x14
#endif
#ifndef OS_DATACENTER
    #define OS_DATACENTER 0x15
#endif
#ifndef OS_ADVSERVER
    #define OS_ADVSERVER 0x16
#endif
#ifndef OS_SERVER
    #define OS_SERVER 0x17
#endif
#ifndef OS_TERMINALSERVER
    #define OS_TERMINALSERVER 0x18
#endif
#ifndef OS_PERSONALTERMINALSERVER
    #define OS_PERSONALTERMINALSERVER 0x19
#endif
#ifndef OS_FASTUSERSWITCHING
    #define OS_FASTUSERSWITCHING 0x1A
#endif
#ifndef OS_WELCOMELOGONUI
    #define OS_WELCOMELOGONUI 0x1B
#endif
#ifndef OS_DOMAINMEMBER
    #define OS_DOMAINMEMBER 0x1C
#endif
#ifndef OS_ANYSERVER
    #define OS_ANYSERVER 0x1D
#endif
#ifndef OS_WOW6432
    #define OS_WOW6432 0x1E
#endif
#ifndef OS_WEBSERVER
    #define OS_WEBSERVER 0x1F
#endif
#ifndef OS_SMALLBUSINESSSERVER
    #define OS_SMALLBUSINESSSERVER 0x20
#endif
#ifndef OS_TABLETPC
    #define OS_TABLETPC 0x21
#endif
#ifndef OS_SERVERADMINUI
    #define OS_SERVERADMINUI 0x22
#endif
#ifndef OS_MEDIACENTER
    #define OS_MEDIACENTER 0x23
#endif
#ifndef OS_APPLIANCE
    #define OS_APPLIANCE 0x24
#endif
#ifndef OS_SERVERR2
    #define OS_SERVERR2 0x2A
#endif
#ifndef SUITE_SMALLBUSINESS
    #define SUITE_SMALLBUSINESS 0x0001
#endif
#ifndef SUITE_ENTERPRISE
    #define SUITE_ENTERPRISE 0x0002
#endif
#ifndef SUITE_BACKOFFICE
    #define SUITE_BACKOFFICE 0x0004
#endif
#ifndef SUITE_COMMUNICATIONS
    #define SUITE_COMMUNICATIONS 0x0008
#endif
#ifndef SUITE_TERMINAL
    #define SUITE_TERMINAL 0x0010
#endif
#ifndef SUITE_SMALLBUSINESS_RESTRICTED
    #define SUITE_SMALLBUSINESS_RESTRICTED 0x0020
#endif
#ifndef SUITE_EMBEDDEDNT
    #define SUITE_EMBEDDEDNT 0x0040
#endif
#ifndef SUITE_DATACENTER
    #define SUITE_DATACENTER 0x0080
#endif
#ifndef SUITE_SINGLEUSERTS
    #define SUITE_SINGLEUSERTS 0x0100
#endif
#ifndef SUITE_PERSONAL
    #define SUITE_PERSONAL 0x0200
#endif
#ifndef SUITE_BLADE
    #define SUITE_BLADE 0x0400
#endif
#ifndef SUITE_EMBEDDED_RESTRICTED
    #define SUITE_EMBEDDED_RESTRICTED 0x0800
#endif
#ifndef SUITE_SECURITY_APPLIANCE
    #define SUITE_SECURITY_APPLIANCE 0x1000
#endif
#ifndef SUITE_STORAGE_SERVER
    #define SUITE_STORAGE_SERVER 0x2000
#endif
#ifndef SUITE_COMPUTE_SERVER
    #define SUITE_COMPUTE_SERVER 0x4000
#endif
#ifndef SUITE_WH_SERVER
    #define SUITE_WH_SERVER 0x8000
#endif

BOOL staticIsOS(DWORD dwInfoType);
