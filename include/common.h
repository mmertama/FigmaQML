#ifndef COMMON_H
#define COMMON_H

#include <QDebug>

#ifdef ASSERT_NESTED
extern int _nested_count;
#define NESTED_START ([](){qDebug() << " nested in" << _nested_count; return false;}() || ++_nested_count == 1 || []()->bool{std::abort(); return true;}());
#define NESTED_END ([](){qDebug() << " nested out" << _nested_count; return false;}() || --_nested_count == 0 || []()->bool{std::abort(); return true;}());
#else
#define NESTED_START
#define NESTED_END
#endif

#endif // COMMON_H
