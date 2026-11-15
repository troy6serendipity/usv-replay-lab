#ifndef USV_COMM_EXPORT_H_
#define USV_COMM_EXPORT_H_

#if defined(_WIN32)
#if defined(USV_COMM_BUILD)
#define USV_COMM_API __declspec(dllexport)
#else
#define USV_COMM_API __declspec(dllimport)
#endif
#else
#define USV_COMM_API __attribute__((visibility("default")))
#endif

#endif // USV_COMM_EXPORT_H_
