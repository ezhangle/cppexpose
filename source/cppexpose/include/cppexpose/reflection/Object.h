
#pragma once


#include <cppexpose/function/Function.h>
#include <cppexpose/reflection/PropertyGroup.h>


namespace cppexpose
{


/**
*  @brief
*    Base class for reflection-enabled objects
*/
class CPPEXPOSE_API Object : public PropertyGroup
{
public:
    /**
    *  @brief
    *    Constructor
    *
    *  @param[in] name
    *    Name
    */
    Object(const std::string & name);

    /**
    *  @brief
    *    Destructor
    */
    virtual ~Object();

    /**
    *  @brief
    *    Get list of functions exported by this object
    *
    *  @return
    *    List of functions
    */
    const std::vector<Function> & functions() const;

    /**
    *  @brief
    *    Add (export) function on object
    *
    *  @param[in] name
    *    Function name
    *  @param[in] fn
    *    Function pointer
    */
    template <typename RET, typename... Arguments>
    void addFunction(const std::string & name, RET (*fn)(Arguments...));

    /**
    *  @brief
    *    Add (export) method on object
    *
    *  @param[in] name
    *    Function name
    *  @param[in] obj
    *    Pointer to object instance
    *  @param[in] member
    *    Method pointer
    */
    template <class T, typename RET, typename... Arguments>
    void addFunction(const std::string & name, T * obj, RET (T::*member)(Arguments...));


protected:
    std::vector<Function> m_functions; ///< List of exported functions
};


} // namespace cppexpose


#include <cppexpose/reflection/Object.hpp>
