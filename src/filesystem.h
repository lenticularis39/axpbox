#ifndef AXPBOX_FILESYSTEM_H
#define AXPBOX_FILESYSTEM_H

/** This is a convenience wrapper for developers to
 * include and not worry about specific
 * gcc versions (where filesystem lives under the
 * std::experimental header) and newer versions
 * (where its just std::filesystem).  simply
 * use `fs::`.
 */
#if defined(__GNUC__) && __GNUC__ < 9
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif

#endif //AXPBOX_FILESYSTEM_H
