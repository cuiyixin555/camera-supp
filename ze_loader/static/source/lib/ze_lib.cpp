/*
 *
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 * @file ze_lib.cpp
 *
 */
#include "ze_lib.h"
#ifndef DYNAMIC_LOAD_LOADER
#include "../loader/ze_loader_api.h"
#endif

namespace ze_lib
{
    ///////////////////////////////////////////////////////////////////////////////
    context_t *context;

    ///////////////////////////////////////////////////////////////////////////////
    context_t::context_t()
    {
#ifndef DYNAMIC_LOAD_LOADER
        loader::context = new loader::context_t;
#endif
    };

    ///////////////////////////////////////////////////////////////////////////////
    context_t::~context_t()
    {
#ifdef DYNAMIC_LOAD_LOADER
        FREE_DRIVER_LIBRARY( loader );
#else
        delete loader::context;
#endif
    };

    //////////////////////////////////////////////////////////////////////////
    __zedlllocal ze_result_t context_t::Init(ze_init_flags_t flags)
    {
        ze_result_t result;
#ifdef DYNAMIC_LOAD_LOADER
        std::string loaderLibraryPath;
#ifdef _WIN32
        loaderLibraryPath = readLevelZeroLoaderLibraryPath();
#endif
        std::string loaderFullLibraryPath = create_library_path(MAKE_LIBRARY_NAME( "ze_loader", L0_LOADER_VERSION), loaderLibraryPath.c_str());
        loader = LOAD_DRIVER_LIBRARY(loaderFullLibraryPath.c_str());

        if( NULL == loader )
            return ZE_RESULT_ERROR_UNINITIALIZED;

        typedef ze_result_t (ZE_APICALL *loaderInit_t)();
        auto loaderInit = reinterpret_cast<loaderInit_t>(
                GET_FUNCTION_PTR(loader, "zeLoaderInit") );
        result = loaderInit();
        if( ZE_RESULT_SUCCESS == result ) {
            typedef HMODULE (ZE_APICALL *getTracing_t)();
            auto getTracing = reinterpret_cast<getTracing_t>(
                GET_FUNCTION_PTR(loader, "zeLoaderGetTracingHandle") );
            tracing_lib = getTracing();
        }
#else
        result = zeLoaderInit();
        if( ZE_RESULT_SUCCESS == result ) {
            tracing_lib = zeLoaderGetTracingHandle();
        }

#endif

        if( ZE_RESULT_SUCCESS == result )
        {
            result = zeInit();
        }

        if( ZE_RESULT_SUCCESS == result )
        {
            result = zetInit();
        }

        if( ZE_RESULT_SUCCESS == result )
        {
            result = zesInit();
        }

        if( ZE_RESULT_SUCCESS == result )
        {
            result = zelTracingInit();
        }

        if( ZE_RESULT_SUCCESS == result )
        {
            result = zelLoaderDriverCheck(flags);
        }

        if( ZE_RESULT_SUCCESS == result )
        {
            isInitialized = true;
        }

        return result;
    }

} // namespace ze_lib

#ifdef ZE_STATIC_INIT
struct context_init_helper
{
    context_init_helper()
    {
        ze_lib::context = new ze_lib::context_t;
    }

    ~context_init_helper()
    {
        delete ze_lib::context;
    }
};

context_init_helper context_init_helper_instance;
#endif

extern "C" {


ze_result_t ZE_APICALL
zelLoaderGetVersions(
   size_t *num_elems,                     //Pointer to num versions to get.
   zel_component_version_t *versions)     //Pointer to array of versions. If set to NULL, num_elems is returned
{
#ifdef DYNAMIC_LOAD_LOADER
    if(nullptr == ze_lib::context->loader)
        return ZE_RESULT_ERROR;
    typedef ze_result_t (ZE_APICALL *zelLoaderGetVersions_t)(size_t *num_elems, zel_component_version_t *versions);
    auto getVersions = reinterpret_cast<zelLoaderGetVersions_t>(
            GET_FUNCTION_PTR(ze_lib::context->loader, "zelLoaderGetVersionsInternal") );
    return getVersions(num_elems, versions);
#else
    return zelLoaderGetVersionsInternal(num_elems, versions);
#endif
}


ze_result_t ZE_APICALL
zelLoaderTranslateHandle(
   zel_handle_type_t handleType,
   void *handleIn,
   void **handleOut)

{
    return zelLoaderTranslateHandleInternal(handleType, handleIn, handleOut);
}




} //extern "c"