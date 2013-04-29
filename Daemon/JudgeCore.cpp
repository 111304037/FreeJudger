
#include "JudgeCore.h"

#include "../judgerlib/logger/Logger.h"
#include "../judgerlib/logger/Logger_log4cxx.h"
#include "../judgerlib/config/AppConfig.h"
#include "../judgerlib/filetool/FileTool.h"
#include "../judgerlib/process/Process.h"
#include "Task.h"
#include "InitApp.h"

bool g_sigExit = false;

namespace IMUST
{

JudgeCore::JudgeCore()
    : mysql_(0)
    , dbManager_(0)
    , dbThread_(0)
    , workingTaskMgr_(new TaskManager())
    , finishedTaskMgr_(new TaskManager())
    , taskFactory_(new JudgeTaskFactory())
{
}

JudgeCore::~JudgeCore()
{
}

bool JudgeCore::startService()
{
    if (!InitApp())
        return false;

    ILogger *logger = LoggerFactory::getLogger(LoggerId::AppInitLoggerId);

    //��¼Windows�����û�
    if(AppConfig::WindowsUser::Enable)
    {
        windowsUser_ = WindowsUserPtr(new WindowsUser());
        if(!windowsUser_->login(
            AppConfig::WindowsUser::Name, 
            OJStr(""), 
            AppConfig::WindowsUser::Password))
        {
            logger->logError(OJStr("JudgeCore::startService login windows user failed."));
            return false;
        }

        logger->logInfo(OJStr("login windows succed."));
        ProcessFactory::setWindowsUser(windowsUser_);
    }

    //����sql�豸
    mysql_ = SqlFactory::createDriver(SqlType::MySql);
    if(!mysql_->loadService())
    {
        OJString msg(OJStr("[Daemon] - WinMain - mysql->loadService failed - "));
        msg += mysql_->getErrorString();
        logger->logError(msg);
        return false;
    }

    //�������ݿ�
    if(!mysql_->connect(AppConfig::MySql::Ip, 
                    AppConfig::MySql::Port,
                    AppConfig::MySql::User,
                    AppConfig::MySql::Password,
                    AppConfig::MySql::DBName))
    {
        OJString msg(OJStr("[Daemon] - WinMain - connect mysql faild - "));
        msg += mysql_->getErrorString();
        logger->logError(msg);
        return false;
    }
    mysql_->setCharSet(OJStr("utf-8"));
    
    //�������ݿ������
    dbManager_ = DBManagerPtr(new DBManager(mysql_, 
        workingTaskMgr_, finishedTaskMgr_, taskFactory_));

    //TODO: �Ƴ��˴��Ĳ��Բ���
    logger->logInfo(OJStr("clear db data."));
    dbManager_->doTestBeforeRun();

    //hook����
    if(false && NULL == LoadLibrary(OJStr("windowsapihook.dll")))
    {
        logger->logWarn(GetOJString("[Daemon] - WinMain - load hook dll faild! - "));
    }

    //�������ݿ��߳�
    dbThread_ = ThreadPtr(new Thread(JudgeDBRunThread(dbManager_)));

    //��������Ŀ¼
    FileTool::MakeDir(OJStr("work"));

    //���������߳�
    for(IMUST::OJInt32_t i=0; i<AppConfig::CpuInfo::NumberOfCore; ++i)
    {
        ThreadPtr ptr(new IMUST::Thread(IMUST::JudgeThread(i, workingTaskMgr_, finishedTaskMgr_)));
        judgeThreadPool_.push_back(ptr);
    }
    
    return true;
}

void JudgeCore::stopService()
{
    g_sigExit = true;

    if (dbThread_)
    {
        dbThread_->join();
    }

    for(JudgeThreadVector::iterator it = judgeThreadPool_.begin();
        it != judgeThreadPool_.end(); ++it)
    {
        if (*it)
        {
            (*it)->join();
        }
    }

    if (mysql_)
    {
        mysql_->disconect();
        mysql_->unloadService();
    }

    if (windowsUser_)
    {
        windowsUser_->logout();
    }
}

}//namespace IMUST