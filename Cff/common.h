/*******************************************************************
*  Copyright(c) 2019
*  All rights reserved.
*
*  文件名称:    common.h
*  简要描述:    通用
*
*  作者:  gongluck
*  说明:
*
*******************************************************************/

#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <libavutil/error.h>

#ifdef __cplusplus
}
// C++中使用av_err2str宏
static char av_error[AV_ERROR_MAX_STRING_SIZE] = { 0 };
#ifdef av_err2str
#undef av_err2str
#endif
#define av_err2str(errnum) \
    av_make_error_string(av_error, AV_ERROR_MAX_STRING_SIZE, errnum)
#endif

// 递归锁
#define LOCK() std::lock_guard<std::recursive_mutex> _lock(this->mutex_)
#define TRYLOCK()\
if (!this->mutex_.try_lock())\
{\
    return EBUSY;\
}
#define UNLOCK()\
{\
    this->mutex_.unlock();\
}

// 检查停止状态
#define CHECKSTOP() \
if(this->status_ != STOP)\
{\
    return EINVAL;\
}
#define CHECKNOTSTOP() \
if(this->status_ == STOP)\
{\
    return EINVAL;\
}

// 检查ffmpeg返回值
#define CHECKFFRET(ret) \
if (ret < 0)\
{\
    return ret;\
}

#endif//__COMMON_H__
