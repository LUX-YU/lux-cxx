#pragma once

#include <lux/cxx/dref/runtime/Marker.hpp>

struct LUX_META(marked;render::feature,name=Skybox,priority=50,enabled=true,deps=DepthPrepass|ForwardOpaque) AnnotationFeature
{
    int LUX_META(luxref::property::member,display_name=World Position,readonly=true,labels=X|Y|Z) value;
};

