//
// Mutex_POSIX.cpp
//
// $Id: //poco/1.4/Foundation/src/Mutex_POSIX.cpp#4 $
//
// Library: Foundation
// Package: Threading
// Module:  Mutex
//
// Copyright (c) 2004-2008, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
// 
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//


#include "Poco/Mutex_POSIX.h"
#include "Poco/Timestamp.h"
#if !defined(POCO_NO_SYS_SELECT_H)
#include <sys/select.h>
#endif
#include <unistd.h>
#if defined(POCO_VXWORKS)
#include <timers.h>
#include <cstring>
#else
#include <sys/time.h>


#if defined(_POSIX_TIMEOUTS) && (_POSIX_TIMEOUTS - 200112L) >= 0L
#if defined(_POSIX_THREADS) && (_POSIX_THREADS - 200112L) >= 0L
#if !(defined(__SUNPRO_CC))
#define POCO_HAVE_MUTEX_TIMEOUT
#endif
#endif
#endif


namespace Poco {


MutexImpl::MutexImpl()
{
#if defined(POCO_VXWORKS)
        // This workaround is for VxWorks 5.x where
        // pthread_mutex_init() won't properly initialize the mutex
        // resulting in a subsequent freeze in pthread_mutex_destroy()
        // if the mutex has never been used.
        std::memset(&_mutex, 0, sizeof(_mutex));
#endif
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
#if defined(PTHREAD_MUTEX_RECURSIVE_NP)
        pthread_mutexattr_settype_np(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
#elif !defined(POCO_VXWORKS) 
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
#endif
        if (pthread_mutex_init(&_mutex, &attr))
	{
		pthread_mutexattr_destroy(&attr);
		throw SystemException("cannot create mutex");
	}
	pthread_mutexattr_destroy(&attr);
}


MutexImpl::MutexImpl(bool fast)
{
#if defined(POCO_VXWORKS)
        // This workaround is for VxWorks 5.x where
        // pthread_mutex_init() won't properly initialize the mutex
        // resulting in a subsequent freeze in pthread_mutex_destroy()
        // if the mutex has never been used.
        std::memset(&_mutex, 0, sizeof(_mutex));
#endif
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
#if defined(PTHREAD_MUTEX_RECURSIVE_NP)
        pthread_mutexattr_settype_np(&attr, fast ? PTHREAD_MUTEX_NORMAL_NP : PTHREAD_MUTEX_RECURSIVE_NP);
#elif !defined(POCO_VXWORKS)
        pthread_mutexattr_settype(&attr, fast ? PTHREAD_MUTEX_NORMAL : PTHREAD_MUTEX_RECURSIVE);
#endif
        if (pthread_mutex_init(&_mutex, &attr))
	{
		pthread_mutexattr_destroy(&attr);
		throw SystemException("cannot create mutex");
	}
	pthread_mutexattr_destroy(&attr);
}


MutexImpl::~MutexImpl()
{
	pthread_mutex_destroy(&_mutex);
}


bool MutexImpl::tryLockImpl(long milliseconds)
{
#if defined(POCO_HAVE_MUTEX_TIMEOUT)
	struct timespec abstime;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	abstime.tv_sec  = tv.tv_sec + milliseconds / 1000;
	abstime.tv_nsec = tv.tv_usec*1000 + (milliseconds % 1000)*1000000;
	if (abstime.tv_nsec >= 1000000000)
	{
		abstime.tv_nsec -= 1000000000;
		abstime.tv_sec++;
	}
	int rc = pthread_mutex_timedlock(&_mutex, &abstime);
	if (rc == 0)
		return true;
	else if (rc == ETIMEDOUT)
		return false;
	else
		throw SystemException("cannot lock mutex");
#else
	const int sleepMillis = 5;
	Timestamp now;
	Timestamp::TimeDiff diff(Timestamp::TimeDiff(milliseconds)*1000);
	do
	{
		int rc = pthread_mutex_trylock(&_mutex);
		if (rc == 0)
                        return true;
                else if (rc != EBUSY)
                        throw SystemException("cannot lock mutex");
#if defined(POCO_VXWORKS)
                struct timespec ts;
                ts.tv_sec = 0;
                ts.tv_nsec = sleepMillis*1000000;
                nanosleep(&ts, NULL);
                
#else
                struct timeval tv;
                tv.tv_sec  = 0;
                tv.tv_usec = sleepMillis * 1000;
                select(0, NULL, NULL, NULL, &tv);
#endif
        }
        while (!now.isElapsed(diff));
        return false;
#endif
}


FastMutexImpl::FastMutexImpl(): MutexImpl(true)
{
}


FastMutexImpl::~FastMutexImpl()
{
}


} // namespace Poco
