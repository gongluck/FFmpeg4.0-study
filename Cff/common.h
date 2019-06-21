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
    err = "decoder is busing.";\
    return false;\
}
#define UNLOCK()\
{\
    this->mutex_->un_lock();\
}

// 检查停止状态
#define CHECKSTOP(err) \
if(this->status_ != STOP)\
{\
    err = "status is not stop.";\
    return false;\
}
#define CHECKNOTSTOP(err) \
if(this->status_ == STOP)\
{\
    err = "status is stop.";\
    return false;\
}

// 检查ffmpeg返回值
#define CHECKFFRET(ret) \
if (ret < 0)\
{\
    err = av_err2str(ret);\
    return false;\
}
#define CHECKFFRETANDCTX(ret, codectx) \
if (ret < 0)\
{\
    avcodec_free_context(&codectx);\
    err = av_err2str(ret);\
    return false;\
}
#define CHECKFFRETANDCTX2(ret, codectx1, codectx2) \
if (ret < 0)\
{\
    avcodec_free_context(&codectx1);\
    avcodec_free_context(&codectx2);\
    err = av_err2str(ret);\
    return false;\
}

#endif//__COMMON_H__
