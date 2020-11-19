#ifndef AXPBOX_MAKE_UNIQUE_H
#define AXPBOX_MAKE_UNIQUE_H
#include <memory>
/* Allow C++11 code to use std::make_unique
 * example ifdef:  #if __cplusplus < 201402L
 */
namespace std
{
template<typename T, typename ...Args>
std::unique_ptr<T> make_unique( Args&& ...args )
{
  return std::unique_ptr<T>( new T( std::forward<Args>(args)... ) );
}
}
#endif // AXPBOX_MAKE_UNIQUE_H
