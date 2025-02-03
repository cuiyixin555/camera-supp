/*
 *
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */
#include "ze_loader_internal.h"

#include "driver_discovery.h"

namespace loader
{
    ///////////////////////////////////////////////////////////////////////////////
    context_t *context;

    ze_result_t context_t::check_drivers(ze_init_flags_t flags) {
        bool return_first_driver_result=false;
        if(drivers.size()==1) {
            return_first_driver_result=true;
        }

        for(auto it = drivers.begin(); it != drivers.end(); )
        {
            ze_result_t result = init_driver(*it, flags);
            if(result != ZE_RESULT_SUCCESS) {
                FREE_DRIVER_LIBRARY(it->handle);
                it = drivers.erase(it);
                if(return_first_driver_result)
                    return result;
            }
            else {
                it++;
            }
        }

        if(drivers.size() == 0)
            return ZE_RESULT_ERROR_UNINITIALIZED;

        return ZE_RESULT_SUCCESS;
    }

    ze_result_t context_t::init_driver(driver_t driver, ze_init_flags_t flags) {
        
        auto getTable = reinterpret_cast<ze_pfnGetGlobalProcAddrTable_t>(
            GET_FUNCTION_PTR(driver.handle, "zeGetGlobalProcAddrTable"));
        if(!getTable) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        
        ze_global_dditable_t global;
        auto getTableResult = getTable(ZE_API_VERSION_CURRENT, &global);
        if(getTableResult != ZE_RESULT_SUCCESS) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        
        if(nullptr == global.pfnInit) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }

        if(nullptr != validationLayer) {
            auto getTable = reinterpret_cast<ze_pfnGetGlobalProcAddrTable_t>(
                GET_FUNCTION_PTR(validationLayer, "zeGetGlobalProcAddrTable") );
            if(!getTable)
                return ZE_RESULT_ERROR_UNINITIALIZED;
            auto getTableResult = getTable( version, &global);
            if(getTableResult != ZE_RESULT_SUCCESS) {
                return ZE_RESULT_ERROR_UNINITIALIZED;
            }
        }

        if(nullptr != tracingLayer) {
            auto getTable = reinterpret_cast<ze_pfnGetGlobalProcAddrTable_t>(
                GET_FUNCTION_PTR(tracingLayer, "zeGetGlobalProcAddrTable") );
            if(!getTable)
                return ZE_RESULT_ERROR_UNINITIALIZED;
            auto getTableResult = getTable( version, &global);
            if(getTableResult != ZE_RESULT_SUCCESS) {
                return ZE_RESULT_ERROR_UNINITIALIZED;
            }
        }

        auto pfnInit = global.pfnInit;
        if(nullptr == pfnInit) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }

        return pfnInit(flags);
    }

    ///////////////////////////////////////////////////////////////////////////////
    ze_result_t context_t::init()
    {
        auto discoveredDrivers = discoverEnabledDrivers();

        drivers.reserve( discoveredDrivers.size() + getenv_tobool( "ZE_ENABLE_NULL_DRIVER" ) );
        if( getenv_tobool( "ZE_ENABLE_NULL_DRIVER" ) )
        {
            auto handle = LOAD_DRIVER_LIBRARY( MAKE_LIBRARY_NAME( "ze_null", L0_LOADER_VERSION ) );
            if( NULL != handle )
            {
                drivers.emplace_back();
                drivers.rbegin()->handle = handle;
            }
        }

#ifndef _WIN32
        const char* libPathRoot = getenv("LEVEL_ZERO_GPU_LIB");
#endif
        for( auto name : discoveredDrivers )
        {
#ifndef _WIN32
            std::string fullName = (libPathRoot != nullptr) ? std::string(libPathRoot) + name : name;
            auto handle = LOAD_DRIVER_LIBRARY( fullName.c_str() );
#else
            auto handle = LOAD_DRIVER_LIBRARY( name.c_str() );
#endif
            if( NULL != handle )
            {
                drivers.emplace_back();
                drivers.rbegin()->handle = handle;
            }
        }

        if(drivers.size()==0)
            return ZE_RESULT_ERROR_UNINITIALIZED;

        add_loader_version();
        std::string loaderLibraryPath;
#ifdef _WIN32
        loaderLibraryPath = readLevelZeroLoaderLibraryPath();
#endif
        typedef ze_result_t (ZE_APICALL *getVersion_t)(zel_component_version_t *version);
        if( getenv_tobool( "ZE_ENABLE_VALIDATION_LAYER" ) )
        {
            std::string validationLayerLibraryPath = create_library_path(MAKE_LAYER_NAME( "ze_validation_layer" ), loaderLibraryPath.c_str());
            validationLayer = LOAD_DRIVER_LIBRARY( validationLayerLibraryPath.c_str() );
            if(validationLayer)
            {
                auto getVersion = reinterpret_cast<getVersion_t>(
                    GET_FUNCTION_PTR(validationLayer, "zelLoaderGetVersion"));
                zel_component_version_t version;
                if(getVersion && ZE_RESULT_SUCCESS == getVersion(&version))
                {   
                    compVersions.push_back(version);
                }
            }
        }

        if( getenv_tobool( "ZE_ENABLE_TRACING_LAYER" ) )
        {
            std::string tracingLayerLibraryPath = create_library_path(MAKE_LAYER_NAME( "ze_tracing_layer" ), loaderLibraryPath.c_str());
            tracingLayer = LOAD_DRIVER_LIBRARY( tracingLayerLibraryPath.c_str() );
            if(tracingLayer)
            {   
                auto getVersion = reinterpret_cast<getVersion_t>(
                    GET_FUNCTION_PTR(tracingLayer, "zelLoaderGetVersion"));
                zel_component_version_t version;
                if(getVersion && ZE_RESULT_SUCCESS == getVersion(&version))
                {   
                    compVersions.push_back(version);
                }
            }
        }

        forceIntercept = getenv_tobool( "ZE_ENABLE_LOADER_INTERCEPT" );

        if(forceIntercept || drivers.size() > 1)
             intercept_enabled = true;

        return ZE_RESULT_SUCCESS;
    };

    ///////////////////////////////////////////////////////////////////////////////
    context_t::~context_t()
    {
        FREE_DRIVER_LIBRARY( validationLayer );
        FREE_DRIVER_LIBRARY( tracingLayer );

        for( auto& drv : drivers )
        {
            FREE_DRIVER_LIBRARY( drv.handle );
        }
    };

    void context_t::add_loader_version(){
        zel_component_version_t version = {};
        string_copy_s(version.component_name, LOADER_COMP_NAME, ZEL_COMPONENT_STRING_SIZE);
        version.spec_version = ZE_API_VERSION_CURRENT;
        version.component_lib_version.major = LOADER_VERSION_MAJOR;
        version.component_lib_version.minor = LOADER_VERSION_MINOR;
        version.component_lib_version.patch = LOADER_VERSION_PATCH;

        compVersions.push_back(version);
    }

}
