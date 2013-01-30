#include "DBManager.h"

#include "../taskmanager/TaskManager.h"

#include "../config/AppConfig.h"

#pragma warning(push)
#pragma warning(disable:4996)

namespace IMUST
{
namespace
{
    
const OJInt32_t MaxBufferSize = 1024;

}

namespace Statement
{

const OJString SelectSolution = OJStr("SELECT solution_id, problem_id, user_id, language ")
    OJStr("FROM `solution` WHERE result=%d or result=%d limit 20");

const OJString SelectProblem = OJStr("SELECT time_limit, memory_limit ")
    OJStr("FROM problem WHERE problem_id =%d");

const OJString SelectCode = OJStr("SELECT `source` FROM `source_code` WHERE `solution_id`=%d");

const OJString SelectCustomInput = OJStr("SELECT `input_text` from custominput WHERE `solution_id`=%d");

const OJString UpdateSolutionCompiling = OJStr("UPDATE `solution` SET `result`=%d, `judgetime`=NOW() ")
	OJStr("WHERE `solution_id` = %d");

const OJString UpdateSolutionResult = OJStr("UPDATE `solution` SET `result`=%d, ")
    OJStr("`time`=%d, `memory`=%d, `judgetime`=NOW(), `pass_rate`=%f ")
    OJStr("WHERE `solution_id` = %d");

const OJString DeleteCompile = OJStr("DELETE FROM `compileinfo` WHERE `solution_id`=%d");
const OJString InsertCompile = OJStr("INSERT INTO `compileinfo`(solution_id, error)VALUES(%d, \"%s\")");

const OJString DeleteRuntime = OJStr("DELETE FROM `runtimeinfo` WHERE `solution_id`=%d");
const OJString InsertRuntime = OJStr("INSERT INTO `runtimeinfo`(solution_id, error)VALUES(%d, \"%s\")");


const OJString UpdateUserSolved = OJStr("UPDATE `users` SET `solved`=")
    OJStr("(SELECT count(DISTINCT `problem_id`) FROM `solution` ")
    OJStr("WHERE `user_id`=\'%s\' AND `result`=%d) ")
    OJStr("WHERE `user_id`=\'%s\'");

const OJString UpdateUserSubmit = OJStr("UPDATE `users` SET `submit`=")
    OJStr("(SELECT count(*) FROM `solution` WHERE `user_id`=\'%s\') ")
    OJStr("WHERE `user_id`=\'%s\'");

const OJString UpdateProblemAccept = OJStr("UPDATE `problem` SET `accepted`=")
    OJStr("(SELECT count(*) FROM `solution` WHERE `problem_id`=\'%d\' AND `result`=\'%d\') ")
    OJStr("WHERE `problem_id`=\'%d\'");

const OJString UpdateProblemSubmit = OJStr("UPDATE `problem` SET `submit`=")
    OJStr("(SELECT count(*) FROM `solution` WHERE `problem_id`=\'%d\')")
    OJStr("WHERE `problem_id`=\'%d\'");


}

using namespace AppConfig;

DBManager::DBManager(SqlDriverPtr sqlDriver, TaskManagerPtr working, TaskManagerPtr finished)
    : sqlDriver_(sqlDriver)
    , workingTaskMgr_(working)
    , finishedTaskMgr_(finished)
{
}


DBManager::~DBManager(void)
{
}

bool DBManager::run()
{
    assert(sqlDriver_->valid() && "sql driver not valid!");

    bool result;

    workingTaskMgr_->lock();
    if(!workingTaskMgr_->hasTask())
    {
        result = readTask();
    }
    workingTaskMgr_->unlock();

    if(!result)
    {
        OJCout<<OJStr("read task faild!")<<sqlDriver_->getErrorString()<<std::endl;
        return false;
    }

    if(!writeFinishedTask())
    {
        OJCout<<OJStr("write task faild!")<<sqlDriver_->getErrorString()<<std::endl;
        return false;
    }

    return true;
}

OJInt32_t readOneRow(SqlRowPtr & row, SqlDriverPtr driver, const OJString & sql)
{
    //OJCout<<sql<<std::endl;

    if(!driver->query(sql))
    {
        return -1;;
    }
    SqlResultPtr result = driver->storeResult();
    if(!result)
    {
        return 1;
    }
    row = result->fetchRow();
    if(!row)
    {
        return 2;
    }
    return 0;
}


bool DBManager::readTask()
{
    OJChar_t buffer[MaxBufferSize];

    //��ȡ���Ϊ�����͵ȴ����е��ύ
    OJSprintf(buffer, Statement::SelectSolution.c_str(), JudgeCode::Pending, JudgeCode::Rejudge);

    if(!sqlDriver_->query(buffer))
    {
        return false;
    }
    SqlResultPtr result = sqlDriver_->storeResult();
    if(!result)
    {
        return true;
    }

    OJInt32_t solutionID, problemID, language, limitTime, limitMemory;
    OJString userName, userCode, userInput;

    SqlRowPtr row(NULL), tempRow(NULL);
    while(row = result->fetchRow())
    {
        solutionID = (*row)[0].getInt32();
        problemID = (*row)[1].getInt32();
        userName = (*row)[2].getString();
        language = (*row)[3].getInt32();

#if 0
        //�޸ļ�¼��״̬Ϊ�����У���ֹ�ظ���ȡ
        OJSprintf(buffer, Statement::UpdateSolutionCompiling.c_str(), 
            JudgeCode::Compiling, solutionID);
        if(!sqlDriver_->query(buffer))
        {
            return false;
        }
#endif

        //��ȡ��Ŀ����ʱ����ڴ�
        if(problemID == 0) //IDE���Թ���
        {
            limitTime = 5; //s
            limitMemory = 10;//m

            //��ȡ�û�����Ĳ�������
            OJSprintf(buffer, Statement::SelectCustomInput.c_str(), solutionID);
            OJInt32_t r = readOneRow(tempRow, sqlDriver_, buffer);
            if(r < 0) return false;
            else if(r != 0) continue;

            userInput = tempRow->getVar(0).getString();

            //TODO: ���Ӷ�IDE���Թ��ܵ�֧��
            continue;
        }
        else
        {
            OJSprintf(buffer, Statement::SelectProblem.c_str(), problemID);
            OJInt32_t r = readOneRow(tempRow, sqlDriver_, buffer);
            if(r < 0) return false;
            else if(r != 0) continue;

            limitTime = (*tempRow)[0].getInt32();
            limitMemory = (*tempRow)[1].getInt32();
        }

        //��ȡ����
        OJSprintf(buffer, Statement::SelectCode.c_str(), solutionID);
        OJInt32_t r = readOneRow(tempRow, sqlDriver_, buffer);
        if(r < 0) return false;
        else if(r != 0) continue;
        userCode = (*tempRow)[0].getString();

        //TODO: ����Task�������뵽�����������


        OJCout<<solutionID<<OJStr("\t")<<problemID<<OJStr("\t")<<userName<<OJStr("\t")
            <<language<<OJStr("\t")<<limitTime<<OJStr("\t")<<limitMemory<<OJStr("\t")
            <<std::endl;
    }


    return true;
}

bool DBManager::writeFinishedTask()
{
    ITask* pTask = NULL;
    finishedTaskMgr_->lock();
    if(finishedTaskMgr_->hasTask())
    {
        pTask = finishedTaskMgr_->popTask();
    }
    finishedTaskMgr_->unlock();

    if(NULL == pTask)
    {
        return true;
    }

    OJChar_t buffer[MaxBufferSize];

    OJInt32_t solutionID, problemID, result, runTime, runMemory;
    OJFloat16_t passRate;//��������ͨ���ı���
    OJString userName;
    OJString compileError, runTimeError;

    //TODO: �����ݴ�Task�ж�ȡ������


    return true;

    //���½��
    OJSprintf(buffer, Statement::UpdateSolutionResult.c_str(), result, runTime, runMemory,
        passRate, solutionID);
    if(!sqlDriver_->query(buffer))
    {
        return false;
    }

    OJSprintf(buffer, Statement::UpdateUserSubmit.c_str(), userName.c_str(), userName.c_str());
    if(!sqlDriver_->query(buffer))
    {
        return false;
    }
    
    //�û��ѽ���ġ����ܴ��Ƿ���ȷ��ִ�д˲�������ֹ�����У���������Ϣ����ʱˢ�¡�
    OJSprintf(buffer, Statement::UpdateUserSolved.c_str(), userName.c_str(), userName.c_str());
    if(!sqlDriver_->query(buffer))
    {
        return false;
    }

    OJSprintf(buffer, Statement::UpdateProblemSubmit.c_str(), problemID, problemID);
    if(!sqlDriver_->query(buffer))
    {
        return false;
    }

    OJSprintf(buffer, Statement::UpdateProblemAccept.c_str(), problemID, problemID);
    if(!sqlDriver_->query(buffer))
    {
        return false;
    }

    if(result == JudgeCode::CompileError)//����������
    {
        OJSprintf(buffer, Statement::DeleteCompile.c_str(), solutionID);
        if(!sqlDriver_->query(buffer))
        {
            return false;
        }

        OJSprintf(buffer, Statement::InsertCompile.c_str(), compileError.c_str(), solutionID);
        if(!sqlDriver_->query(buffer))
        {
            return false;
        }
    }
    else if(result == JudgeCode::RuntimeError)//����ʱ����
    {
        OJSprintf(buffer, Statement::DeleteRuntime.c_str(), solutionID);
        if(!sqlDriver_->query(buffer))
        {
            return false;
        }

        OJSprintf(buffer, Statement::InsertRuntime.c_str(), runTimeError.c_str(), solutionID);
        if(!sqlDriver_->query(buffer))
        {
            return false;
        }
    }

    return true;
}


}//namespace IMUST

#pragma warning(pop)
