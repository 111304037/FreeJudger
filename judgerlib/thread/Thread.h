#ifndef IMUST_OJ_THREAD_H
#define IMUST_OJ_THREAD_H

#include "../../thirdpartylib/boost/thread.hpp"

namespace IMUST
{

// �߳̿����Boost.Thread�������Ҫ������ƣ��ӿ�Ҫ��Boost����
typedef             ::boost::thread                 Thread;
typedef             ::boost::mutex                  Mutex;


}

#endif  // IMUST_OJ_THREAD_H