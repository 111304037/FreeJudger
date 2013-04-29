/** 
 *  ��ģ�����ڵ�¼һ����Ȩ�ޡ��ɽ�����Windows�û���������һ���û����ơ�
 *  ԭ��ο���[Windows]ACM��������ϵͳ�İ�ȫ�Խ��������http://www.cnblogs.com/zplutor/archive/2010/03/27/1698290.html��
 *  ����ο���Starting an Interactive Client Process in C++��http://msdn.microsoft.com/en-us/library/windows/desktop/aa379608(v=vs.85).aspx��
 */
#pragma once

#include "../util/Utility.h"
#include "../platformlayer/PlatformLayer.h"

#include <Windows.h>
#include <memory>

namespace IMUST
{

class JUDGER_API WindowsUser
{
    MAKE_CLASS_UNCOPYABLE(WindowsUser);
public:

    WindowsUser(void);
    ~WindowsUser(void);

    bool login(const OJString & userName, const OJString & domain, const OJString & password);

    void logout();

    HANDLE getToken() const { return tokenHandle_; }

    bool setPrivilege(LPCTSTR lpszPrivilege, BOOL bEnablePrivilege ) ;

    bool createProcess(
        LPCTSTR lpApplicationName,
        LPTSTR lpCommandLine,
        LPSECURITY_ATTRIBUTES lpProcessAttributes,
        LPSECURITY_ATTRIBUTES lpThreadAttributes,
        BOOL bInheritHandles,
        DWORD dwCreationFlags,
        LPVOID lpEnvironment,
        LPCTSTR lpCurrentDirectory,
        LPSTARTUPINFO lpStartupInfo,
        LPPROCESS_INFORMATION lpProcessInformation);
public:

    static bool addAceToWindowStation(HWINSTA hwinsta, PSID psid);

    static bool addAceToDesktop(HDESK hdesk, PSID psid);

    static bool getLogonSID (HANDLE hToken, PSID *ppsid);

    static void freeLogonSID (PSID *ppsid);

private:

    HANDLE      tokenHandle_;
    HDESK       deskHandle_;
    HWINSTA     winstaHandle_;
    PSID        pSid_;
};

typedef std::shared_ptr<WindowsUser> WindowsUserPtr;


}//namespace IMUST