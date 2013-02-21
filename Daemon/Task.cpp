#include "Task.h"

#include "../judgerlib/taskmanager/TaskManager.h"
#include "../judgerlib/config/AppConfig.h"
#include "../judgerlib/filetool/FileTool.h"
#include "../judgerlib/util/Utility.h"


#include "../judgerlib/compiler/Compiler.h"
#include "../judgerlib/excuter/Excuter.h"
#include "../judgerlib/matcher/Matcher.h"

extern bool g_sigExit;


namespace IMUST
{
/* �ӳ�ɾ���ļ���
��ĳ�ӽ�����δ��ȫ�˳�ʱ����ռ�õ��ļ��ٴα��򿪻�ɾ��������ʧ�ܡ������ӳ٣��ȴ�һ��ʱ�䡣
����ļ�ʼ���޷�ɾ��������ʾ���ӽ����޷��˳����⽫��һ���������������߳�Ӧ�������� */
bool safeRemoveFile(const OJString & file)
{
    OJString infoBuffer;

    for(OJInt32_t i=0; i<10; ++i)//����ɾ��10��
    {
        if(FileTool::RemoveFile(file))
        {
            return true;
        }
        OJSleep(1000);

        FormatString(infoBuffer, OJStr("safeRemoveFile '%s' faild with %d times. code:%d"), 
            file.c_str(), i+1, GetLastError());
        LoggerFactory::getLogger(LoggerId::AppInitLoggerId)->logError(infoBuffer);
    }

    return false;
}

//����ļ���չ��
//TODO: �������õ������֣��ó������档
OJString getLanguageExt(OJInt32_t language)
{
    if(language == AppConfig::Language::C)
    {
        return OJStr("c");
    }
    else if(language == AppConfig::Language::Cxx)
    {
        return OJStr("cpp");
    }
    else if(language == AppConfig::Language::Java)
    {
        return OJStr("java");
    }

    return OJStr("unknown");
}

OJString getExcuterExt(OJInt32_t language)
{
    if(language == AppConfig::Language::C)
    {
        return OJStr("exe");
    }
    else if(language == AppConfig::Language::Cxx)
    {
        return OJStr("exe");
    }
    else if(language == AppConfig::Language::Java)
    {
        return OJStr("class");
    }

    return OJStr("unknown");
}

JudgeTask::JudgeTask(const TaskInputData & inputData) 
    : Input(inputData)
    , judgeID_(0)
{
    output_.Result = AppConfig::JudgeCode::SystemError;
    output_.PassRate = 0.0f;
    output_.RunTime = 0;
    output_.RunMemory = 0;
}

void JudgeTask::init(OJInt32_t judgeID)
{
    judgeID_ = judgeID;

    OJString fileExt = getLanguageExt(Input.Language);
    OJString exeExt = getExcuterExt(Input.Language);

    FormatString(codeFile_, OJStr("work\\%d\\Main.%s"), judgeID_, fileExt.c_str());
    FormatString(exeFile_, OJStr("work\\%d\\Main.%s"), judgeID_, exeExt.c_str());
    FormatString(compileFile_, OJStr("work\\%d\\compile.txt"), judgeID_);
    FormatString(userOutputFile_, OJStr("work\\%d\\output.txt"), judgeID_);

    FileTool::WriteFile(Input.UserCode, codeFile_);
}

bool JudgeTask::run()
{
    doRun();

    if(!doClean())
    {
        return false;//��������
    }

    return true;
}

void JudgeTask::doRun()
{
    ILogger *logger = LoggerFactory::getLogger(LoggerId::AppInitLoggerId);

    OJString infoBuffer;

    FormatString(infoBuffer, OJStr("[JudgeTask] task %d"), Input.SolutionID);
    logger->logInfo(infoBuffer);

    //����
    if(!compile())
    {
        return;
    }

    //������������
    //TODO: �����Ƿ�specialJudge����������.out����.in�ļ���

    FormatString(infoBuffer, OJStr("/%d"), Input.ProblemID);
    OJString path = AppConfig::Path::TestDataPath;
    path += infoBuffer;

    logger->logTrace(OJString(OJStr("[JudgeTask] searche path: "))+path);

    FileTool::FileNameList fileList;
    FileTool::GetSpecificExtFiles(fileList, path, OJStr(".out"), true);

    OJUInt32_t testCount = fileList.size();
    if(testCount <= 0)
    {
        output_.Result = AppConfig::JudgeCode::SystemError;

        FormatString(infoBuffer, OJStr("[JudgeTask] not found test data for solution %d problem %d."),
            Input.SolutionID, Input.ProblemID);
        logger->logError(infoBuffer);
        return;
    }

    //���Զ�������
    OJUInt32_t accepted = 0;
    for(OJUInt32_t i=0; i<testCount; ++i)
    {
        answerOutputFile_ = fileList[i];
        answerInputFile_ = FileTool::RemoveFileExt(answerOutputFile_);
        answerInputFile_ += OJStr(".in");

        logger->logTrace(OJString(OJStr("[JudgeTask] input file: ")) + answerInputFile_);
        logger->logTrace(OJString(OJStr("[JudgeTask] output file: ")) + answerOutputFile_);

        if(!safeRemoveFile(userOutputFile_))
        {
            output_.Result = AppConfig::JudgeCode::SystemError;
            break;
        }

        if(!excute())
        {
            break;
        }
            
        if(!match())
        {
            break;
        }
        
        ++accepted;
    }

    output_.PassRate = float(accepted)/testCount;
}

bool JudgeTask::doClean()
{
    bool faild = false;

    if(!safeRemoveFile(codeFile_))
    {
        faild = true;
    }
    if(!safeRemoveFile(exeFile_))
    {
        faild = true;
    }
    if(!safeRemoveFile(userOutputFile_))
    {
        faild = true;
    }
    if(!safeRemoveFile(compileFile_))
    {
        faild = true;
    }

    if(faild)
    {
        output_.Result = AppConfig::JudgeCode::SystemError;
    }

    return !faild;
}

bool JudgeTask::compile()
{
    ILogger *logger = LoggerFactory::getLogger(LoggerId::AppInitLoggerId);
    logger->logTrace(OJStr("[JudgeTask] start compile..."));
    
    CompilerPtr compiler = CompilerFactory::create(Input.Language);
    compiler->run(codeFile_, exeFile_, compileFile_);

    if(compiler->isAccept())
    {
        output_.Result = AppConfig::JudgeCode::Accept;
    }
    else if(compiler->isSystemError())
    {
        output_.Result = AppConfig::JudgeCode::SystemError;
    }
    else if(compiler->isCompileError())
    {
        output_.Result = AppConfig::JudgeCode::CompileError;
        
        std::vector<OJChar_t> buffer;
        if(FileTool::ReadFile(buffer, compileFile_) && !buffer.empty())
        {
            output_.CompileError = &buffer[0];
        }
    }

    return compiler->isAccept();
}

bool JudgeTask::excute()
{
    ILogger *logger = LoggerFactory::getLogger(LoggerId::AppInitLoggerId);
    logger->logTrace(OJStr("[JudgeTask] start excute..."));
    
    OJString infoBuffer;

    if(!FileTool::IsFileExist(exeFile_))
    {
        FormatString(infoBuffer, OJStr("[JudgeTask] not found exe file! %s."), exeFile_);
        logger->logError(infoBuffer);
        output_.Result = AppConfig::JudgeCode::SystemError;
        return false;
    }

    ExcuterPtr excuter = ExcuterFactory::create(Input.Language);
    excuter->run(exeFile_, answerInputFile_, userOutputFile_, Input.LimitTime, Input.LimitMemory);
    
    if(excuter->isAccept())
    {
        output_.Result = AppConfig::JudgeCode::Accept;
    }
    else if(excuter->isSystemError())
    {
        output_.Result = AppConfig::JudgeCode::SystemError;
    }
    else if(excuter->isOutputOutOfLimited())
    {
        output_.Result = AppConfig::JudgeCode::OutputLimited;
    }
    else if(excuter->isTimeOutOfLimited())
    {
        output_.Result = AppConfig::JudgeCode::TimeLimitExceed;
    }
    else if(excuter->isMemoryOutOfLimited())
    {
        output_.Result = AppConfig::JudgeCode::MemoryLimitExceed;
    }
    else if(excuter->isRuntimeError())
    {
        output_.Result = AppConfig::JudgeCode::RuntimeError;
    }

    output_.RunTime = excuter->getRunTime();
    output_.RunMemory = excuter->getRunMemory();

    return excuter->isAccept();
}

bool JudgeTask::match()
{
    ILogger *logger = LoggerFactory::getLogger(LoggerId::AppInitLoggerId);
    logger->logTrace(OJStr("[JudgeTask] start match..."));

    MatcherPtr matcher = MatcherFactory::create(false, OJStr(""));
    matcher->run(answerOutputFile_, userOutputFile_);

    if(matcher->isAccept())
    {
        output_.Result = AppConfig::JudgeCode::Accept;
    }
    else if(matcher->isPresentError())
    {
        output_.Result = AppConfig::JudgeCode::PresentError;
    }
    else if(matcher->isWrongAnswer())
    {
        output_.Result = AppConfig::JudgeCode::WrongAnswer;
    }
    else if(matcher->isSystemError())
    {
        output_.Result = AppConfig::JudgeCode::SystemError;
    }

    return matcher->isAccept();
}



JudgeThread::JudgeThread(int id, IMUST::TaskManagerPtr working, IMUST::TaskManagerPtr finish)
    : id_(id)
    , workingTaskMgr_(working)
    , finisheTaskMgr_(finish)
{

}

void JudgeThread::operator()()
{
    ILogger *logger = LoggerFactory::getLogger(LoggerId::AppInitLoggerId);

    OJString infoBuffer;
    FormatString(infoBuffer, OJStr("work/%d"), id_);
    FileTool::MakeDir(infoBuffer);

    FormatString(infoBuffer, OJStr("[JudgeThread][%d]start..."), id_);
    logger->logTrace(infoBuffer);

    while (!g_sigExit)
    {

        IMUST::ITask* pTask = NULL;

        //���������ȡ����
        workingTaskMgr_->lock();
        if(workingTaskMgr_->hasTask())
        {
            pTask = workingTaskMgr_->popTask();
        }
        workingTaskMgr_->unlock();

        if(!pTask)//û������
        {
            OJSleep(1000);
            continue;
        }

        pTask->init(id_);
        if(!pTask->run())
        {
            FormatString(infoBuffer, 
                OJStr("[JudgeThread][%d]System Error!Judge thread will exit!"), id_);
            logger->logError(infoBuffer);
            break;
        }

        //��ӵ���ɶ���
        finisheTaskMgr_->lock();
        finisheTaskMgr_->addTask(pTask);
        finisheTaskMgr_->unlock();

        OJSleep(10);//��ֹ�̹߳��ȷ�æ
    }

    FormatString(infoBuffer, OJStr("[JudgeThread][%d]end."), id_);
    logger->logTrace(infoBuffer);

}

ITask* JudgeTaskFactory::create(const TaskInputData & input)
{
    return new JudgeTask(input);
}

void JudgeTaskFactory::destroy(ITask* pTask)
{
    delete pTask;
}


JudgeDBRunThread::JudgeDBRunThread(IMUST::DBManagerPtr dbm)
    : dbm_(dbm)
{
}

void JudgeDBRunThread::operator()()
{
    IMUST::ILogger *logger = IMUST::LoggerFactory::getLogger(IMUST::LoggerId::AppInitLoggerId);
    logger->logTrace(GetOJString("[DBThread] thread start..."));

    while(!g_sigExit)
    {
        if(!dbm_->run())
        {
            logger->logError(GetOJString("[DBThread] thread was dead!"));
            break;
        }
        OJSleep(100);
    }

    logger->logTrace(GetOJString("[DBThread] thread end."));
}

}   // namespace IMUST
