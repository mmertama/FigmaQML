#ifndef COMMON_H
#define COMMON_H

#include <QDebug>

#ifdef ASSERT_NESTED

#else
#define NESTED_START
#define NESTED_END
#endif

#endif // COMMON_H
