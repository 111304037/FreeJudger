
#include "Process.h"
#include "../logger/Logger.h"

namespace IMUST
{

namespace
{

WindowsProcessInOut::WindowsProcessInOut(const OJString &inputFileName,
										const OJString &outputFileName) :
	inputFileHandle_(NULL),
	outputFileHandle_(NULL),
	inputFileName_(inputFileName),
	outputFileName_(outputFileName)
{

}

WindowsProcessInOut::~WindowsProcessInOut()
{
	if (inputFileHandle_)
		::CloseHandle(inputFileHandle_);
	if (outputFileHandle_)
		::CloseHandle(outputFileHandle_);
}

HANDLE WindowsProcessInOut::createInputFile()
{
	if (inputFileName_.empty())
		return NULL;

	SECURITY_ATTRIBUTES saAttr = {0};
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE; 
	saAttr.lpSecurityDescriptor = NULL;

	HANDLE handle = CreateFileW(
		inputFileName_.c_str(),
		GENERIC_READ,          	
		FILE_SHARE_READ,       	
		&saAttr,                  
		OPEN_EXISTING,         	
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (!handle)
	{
		ILogger *logger = LoggerFactory::getLogger(LoggerId::AppInitLoggerId);
		logger->logTrace(GetOJString("[process] - IMUST::WindowsProcessInOut::createInputFile can't creat file"));
	}

	return handle;
}

HANDLE WindowsProcessInOut::createOutputFile()
{
    if (outputFileName_.empty())
        return NULL;

    SECURITY_ATTRIBUTES saAttr = {0};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE; 
    saAttr.lpSecurityDescriptor = NULL;

    HANDLE handle = CreateFileW(
        outputFileName_.c_str(),
        GENERIC_WRITE, 
        FILE_SHARE_READ,
        &saAttr,                   	
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

	if (!handle)
	{
		ILogger *logger = LoggerFactory::getLogger(LoggerId::AppInitLoggerId);
		logger->logTrace(GetOJString("[process] - IMUST::WindowsProcessInOut::createOutputFile can't creat file"));
	}

    return handle;
}

WindowsJob::WindowsJob() :
	jobHandle_(NULL)
{

}

WindowsJob::~WindowsJob()
{
	if (jobHandle_)
		::CloseHandle(jobHandle_);
}

bool WindowsJob::create(LPSECURITY_ATTRIBUTES lpJobAttributes)
{
	if (jobHandle_)
		return false;
	
	jobHandle_ = CreateJobObjectW(lpJobAttributes, NULL);

	return INVALID_HANDLE_VALUE != jobHandle_;
}


DWORD WindowsJob::wait(DWORD time)
{
	return WaitForSingleObject(jobHandle_, time);
}

bool WindowsJob::terminate(DWORD exitCode)
{
	return !!TerminateJobObject(jobHandle_, exitCode);
}

bool WindowsJob::setInformation(JOBOBJECTINFOCLASS infoClass,
								LPVOID lpInfo,
								DWORD cbInfoLength)
{
	return !!SetInformationJobObject(jobHandle_, infoClass, lpInfo, cbInfoLength);
}

bool WindowsJob::assinProcess(HANDLE handel)
{
	return !!AssignProcessToJobObject(jobHandle_, handel);
}

}	// namespace

Mutex WindowsProcess::s_mutex_;
ULONG WindowsProcess::s_id_ = 0;

WindowsProcess::WindowsProcess(
	const OJString &inputFileName,
	const OJString &outputFileName) :
	WindowsProcessInOut(inputFileName, outputFileName),
    processHandle_(NULL),
	threadHandle_(NULL),
	iocpHandle_(NULL)
{

}

WindowsProcess::~WindowsProcess()
{
    if (processHandle_)
        ::CloseHandle(processHandle_);
	if (threadHandle_)
		::CloseHandle(threadHandle_);
	if (iocpHandle_)
		::CloseHandle(iocpHandle_);
}

OJInt32_t WindowsProcess::create(const OJString &cmd,
								const OJInt32_t timeLimit,
								const OJInt32_t memoryLimit,
								bool startImmediately)
{
	OJInt64_t limitTime = timeLimit * 10000; // ms
	int limitMemory = memoryLimit * 1024; //bytes

	if (limitMemory < 0)	//����int��Χ��
		limitMemory = 128 * 1024 * 1024; //Ĭ��128M

	if (!jobHandle_.create())
		return -1;

	//���û���������Ϣ
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION subProcessLimitRes;
	ZeroMemory(&subProcessLimitRes, sizeof(subProcessLimitRes));

	JOBOBJECT_BASIC_LIMIT_INFORMATION & basicInfo = subProcessLimitRes.BasicLimitInformation;
	basicInfo.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_TIME| \
		JOB_OBJECT_LIMIT_PRIORITY_CLASS| \
		JOB_OBJECT_LIMIT_PROCESS_MEMORY| \
		JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
	basicInfo.PriorityClass = NORMAL_PRIORITY_CLASS;      //���ȼ�ΪĬ��
	basicInfo.PerProcessUserTimeLimit.QuadPart = limitTime; //�ӽ���ִ��ʱ��ns(1s=10^9ns)
	subProcessLimitRes.ProcessMemoryLimit = limitMemory;    //�ڴ�����

	jobHandle_.setInformation(JobObjectExtendedLimitInformation,
							&subProcessLimitRes, 
							sizeof(subProcessLimitRes));

	//����ɶ˿ڷ���ʱ�����Ƶ���Ϣ
	JOBOBJECT_END_OF_JOB_TIME_INFORMATION timeReport;
	ZeroMemory(&timeReport, sizeof(timeReport));
	timeReport.EndOfJobTimeAction = JOB_OBJECT_POST_AT_END_OF_JOB;

	jobHandle_.setInformation(
		JobObjectEndOfJobTimeInformation, 
		&timeReport,
		sizeof(JOBOBJECT_END_OF_JOB_TIME_INFORMATION));


	////UI����
	//JOBOBJECT_BASIC_UI_RESTRICTIONS subProcessLimitUi;
	//ZeroMemory(&subProcessLimitUi, sizeof(subProcessLimitUi));
	//subProcessLimitUi.UIRestrictionsClass = JOB_OBJECT_UILIMIT_NONE| \
	//	JOB_OBJECT_UILIMIT_DESKTOP| \
	//	JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS| \
	//	JOB_OBJECT_UILIMIT_DISPLAYSETTINGS| \
	//	JOB_OBJECT_UILIMIT_EXITWINDOWS| \
	//	JOB_OBJECT_UILIMIT_GLOBALATOMS| \
	//	JOB_OBJECT_UILIMIT_HANDLES| \
	//	JOB_OBJECT_UILIMIT_READCLIPBOARD;

	//jobHandle_.setInformation(
	//	JobObjectBasicUIRestrictions,
	//	&subProcessLimitUi,
	//	sizeof(subProcessLimitUi));

	//����ҵ��������ɶ˿ڣ���ȷ����������������˳���ԭ��
	s_mutex_.lock();
	ULONG id = ++s_id_;
	s_mutex_.unlock();

	iocpHandle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, id, 0);

	JOBOBJECT_ASSOCIATE_COMPLETION_PORT jobCP;
	ZeroMemory(&jobCP, sizeof(jobCP));

	jobCP.CompletionKey = (PVOID)id;
	jobCP.CompletionPort = iocpHandle_;

	jobHandle_.setInformation(
		JobObjectAssociateCompletionPortInformation,
		&jobCP,
		sizeof(jobCP));

#define CMDLINE_BUFFER_SIZE 1024
	OJChar_t cmdline[CMDLINE_BUFFER_SIZE];
	wcscpy_s(cmdline, cmd.c_str());
#undef CMDLINE_BUFFER_SIZE

	STARTUPINFO   si;
	PROCESS_INFORMATION   pi;

	ZeroMemory(&si, sizeof(si));
	si.wShowWindow = SW_SHOWNORMAL;
	si.cb = sizeof(si); 
	ZeroMemory(&pi, sizeof(pi)); 

	bool res =  !!CreateProcess(NULL,    //   No module name (use command line).   
		cmdline, //   Command line.   
		NULL,    //   Process handle not inheritable.   
		NULL,    //   Thread handle not inheritable.   
		TRUE,    //   Set handle inheritance to ...
		//CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB, //   No creation  flags.  
		CREATE_SUSPENDED |DEBUG_ONLY_THIS_PROCESS,
		NULL,    //   Use parent 's environment block.   
		NULL,    //   Use parent 's starting  directory.   
		&si,     //   Pointer to STARTUPINFO structure. 
		&pi);    //   Pointer to PROCESS_INFORMAT\ION structure.

	if (!res)
		return -1;

	processHandle_ = pi.hProcess;
	threadHandle_ = pi.hThread;

	ResumeThread(threadHandle_);

	DEBUG_EVENT de;
	ZeroMemory(&de, sizeof(de));
	DWORD dwContinueStatus = DBG_CONTINUE;

	while (WaitForDebugEvent(&de, INFINITE)) 
	{
		if (de.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT)
		{
			//ǿ�ƹر�exe�ļ�
			//SAFE_CLOSE_HANDLE(de.u.CreateProcessInfo.hFile);
			;
		}
		else if (de.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) 
		{
			if (de.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT)
			{
			}
			else
			{
				switch(de.u.Exception.ExceptionRecord.ExceptionCode) 
				{ 
				case EXCEPTION_INT_DIVIDE_BY_ZERO: //���������ĳ�����0ʱ�������쳣��
					MessageBoxW(NULL, L"���������ĳ�����0ʱ�������쳣", L"", MB_OK); 
					//OutputMsgA("INT_DIVIDE_BY_ZERO");
					break;

				case EXCEPTION_INT_OVERFLOW://���������Ľ�����ʱ�������쳣��
					MessageBoxW(NULL, L"���������Ľ�����ʱ�������쳣", L"", MB_OK); 
					//OutputMsgA("INT_OVERFLOW");
					break;

				case EXCEPTION_ACCESS_VIOLATION: //������ͼ��дһ�����ɷ��ʵĵ�ַʱ�������쳣��������ͼ��ȡ0��ַ�����ڴ档
					MessageBoxW(NULL, L"������ͼ��дһ�����ɷ��ʵĵ�ַʱ�������쳣��������ͼ��ȡ0��ַ�����ڴ�", L"", MB_OK); 
					//OutputMsgA("ACCESS_VIOLATION");
					break;

				case EXCEPTION_DATATYPE_MISALIGNMENT://�����ȡһ��δ�����������ʱ�������쳣��
					MessageBoxW(NULL, L"�����ȡһ��δ�����������ʱ�������쳣", L"", MB_OK); 
					//OutputMsgA("DATATYPE_MISALIGNMENT");
					break;

				case EXCEPTION_FLT_STACK_CHECK: //���и���������ʱջ�������������ʱ�������쳣��
					MessageBoxW(NULL, L"���и���������ʱջ�������������ʱ�������쳣", L"", MB_OK); 
					//OutputMsgA("FLT_STACK_CHECK");
					break;

				case EXCEPTION_INVALID_DISPOSITION: //�쳣����������һ����Ч�Ĵ����ʱ�������쳣��
					MessageBoxW(NULL, L"�쳣����������һ����Ч�Ĵ����ʱ�������쳣", L"", MB_OK); 
					//OutputMsgA("INVALID_DISPOSITION");
					break;

				case EXCEPTION_STACK_OVERFLOW: //ջ���ʱ�������쳣��
					MessageBoxW(NULL, L"ջ���ʱ�������쳣", L"", MB_OK); 
					//OutputMsgA("STACK_OVERFLOW");
					break;

				default:
					//OutputMsgA("UNKNOW_EXCEPTION");
					MessageBoxW(NULL, L"UNKNOW_EXCEPTION", L"", MB_OK); 
					break;
				} 

				if (de.u.Exception.dwFirstChance)
				{
					//OutputMsgA("exception at 0x%08x, exception-code: 0x%08x",
					//	de.u.Exception.ExceptionRecord.ExceptionAddress,
					//	de.u.Exception.ExceptionRecord.ExceptionCode);
				}
				dwContinueStatus = DBG_EXCEPTION_NOT_HANDLED;
			}
		}
		else if(de.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) 
		{
			//OutputMsgA("progress exit with code:%u��%u", de.u.ExitProcess.dwExitCode, GetLastError());

			DebugSetProcessKillOnExit(TRUE);
			DebugActiveProcessStop(de.dwProcessId);


			if (de.u.ExitProcess.dwExitCode != 0)
			{
				return false;
			}
			break;
		}

		ContinueDebugEvent(de.dwProcessId, de.dwThreadId, dwContinueStatus); 
	}

	if(startImmediately)
		start();

	return 1;
}

OJInt32_t WindowsProcess::start()
{
	if (!jobHandle_.assinProcess(processHandle_))
		return -1;

    ResumeThread(threadHandle_);

	jobHandle_.wait();
#if 1

	DWORD ExecuteResult = -1;  
	ULONG completeKey;  
	LPOVERLAPPED processInfo;  
	bool done = false;  
	while(!done)  
	{  
		GetQueuedCompletionStatus(iocpHandle_,  
								&ExecuteResult,   
								&completeKey,   
								&processInfo,   
								INFINITE);  

		switch (ExecuteResult)   
		{  
		case JOB_OBJECT_MSG_NEW_PROCESS:    
			MessageBoxW(NULL, L"JOB_OBJECT_MSG_NEW_PROCESS", L"", MB_OK);
			break;  

		case JOB_OBJECT_MSG_END_OF_JOB_TIME:  
			MessageBoxW(NULL, L"Job time limit reached", L"", MB_OK); 
			exitCode_ = 1;  
			done = true;  
			break;  

		case JOB_OBJECT_MSG_END_OF_PROCESS_TIME:   
			MessageBoxW(NULL, L"Job process time limit reached", L"", MB_OK); 
			exitCode_ = 1;  
			done = true;  
			break;  

		case JOB_OBJECT_MSG_PROCESS_MEMORY_LIMIT:   
			MessageBoxW(NULL, L"Process exceeded memory limit", L"", MB_OK);  
			exitCode_ = 2;  
			done = true;  
			break;  

		case JOB_OBJECT_MSG_JOB_MEMORY_LIMIT:   
			MessageBoxW(NULL, L"Process exceeded job memory limit", L"", MB_OK);
			exitCode_ = 2;  
			done = true;  
			break;  

		case JOB_OBJECT_MSG_ACTIVE_PROCESS_LIMIT:  
			MessageBoxW(NULL, L"Too many active processes in job", L"", MB_OK);
			break;  

		case JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO:  
			MessageBoxW(NULL, L"Job contains no active processes", L"", MB_OK);  
			done = true;  
			break;  

		case JOB_OBJECT_MSG_EXIT_PROCESS:   
			MessageBoxW(NULL, L"Process terminated", L"", MB_OK);
			done = true;  
			break;  

		case JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS:   
			MessageBoxW(NULL, L"Process terminated abnormally", L"", MB_OK); 
			exitCode_ = 3;  
			done = true;  
			break;  

		default:  
			MessageBoxW(NULL, L"Unknown notification:", L"", MB_OK);
			exitCode_ = 99;  
			break;  
		}  
	}  
#endif

	/*
	JOBOBJECT_BASIC_AND_IO_ACCOUNTING_INFORMATION jobai;  
	ZeroMemory(&jobai, sizeof(jobai));  
	QueryInformationJobObject(m_job.handle(), JobObjectBasicAndIoAccountingInformation,   
		&jobai, sizeof(jobai), NULL);  

	JOBOBJECT_EXTENDED_LIMIT_INFORMATION joeli;  
	ZeroMemory(&joeli, sizeof(joeli));  
	QueryInformationJobObject(m_job.handle(), JobObjectExtendedLimitInformation,   
		&joeli, sizeof(joeli), NULL);  

	m_runTime = jobai.BasicInfo.TotalUserTime.LowPart/10000;  
	m_runMemory = joeli.PeakProcessMemoryUsed/1024;  
	*/

	return true;  
}

bool WindowsProcess::isRunning()
{
    return getExitCode() == STILL_ACTIVE;
}

OJInt32_t WindowsProcess::join(OJInt32_t time)
{
    if (isRunning())
        WaitForSingleObject(processHandle_, time); 

    return getExitCode();
}

OJInt32_t WindowsProcess::getExitCode()
{
    DWORD code = 0;
    GetExitCodeProcess(processHandle_, &code);
    return code;
}

void WindowsProcess::kill()
{
    TerminateProcess(processHandle_, -1);
}







}   // namespace IMUST