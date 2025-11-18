#ifndef _SAVE_RESTORE_LOG_H_
#define _SAVE_RESTORE_LOG_H_

#include <errlog.h>

#define ERRLOG(fmt, ...) \
	errlogPrintf("%s:%s " fmt, __FILE__, __func__, ## __VA_ARGS__)
#endif //  _SAVE_RESTORE_LOG_H_
