###
 # FiberTaskingLib - A tasking library that uses fibers for efficient task switching
 #
 # This library was created as a proof of concept of the ideas presented by
 # Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
 #
 # http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 #
 # FiberTaskingLib is the legal property of Adrian Astley
 # Copyright Adrian Astley 2015 - 2018
 #
 # Licensed under the Apache License, Version 2.0 (the "License");
 # you may not use this file except in compliance with the License.
 # You may obtain a copy of the License at
 # 
 # http://www.apache.org/licenses/LICENSE-2.0
 # 
 # Unless required by applicable law or agreed to in writing, software
 # distributed under the License is distributed on an "AS IS" BASIS,
 # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 # See the License for the specific language governing permissions and
 # limitations under the License.
 ##

# FTL
set(FTL_BUILD_TESTS OFF CACHE BOOL "")
set(FTL_BUILD_BENCHMARKS OFF CACHE BOOL "")
set(FTL_VALGRIND OFF CACHE BOOL "")
set(FTL_FIBER_STACK_GUARD_PAGES  OFF CACHE BOOL "")
set(FTL_CPP_17 ON CACHE BOOL "")
set(FTL_WERROR ON CACHE BOOL "")
set(FTL_DISABLE_ITERATOR_DEBUG ON CACHE BOOL "")
add_subdirectory(ftl)


set(OPTICK_INSTALL_TARGETS OFF CACHE BOOL "")
set(OPTICK_USE_VULKAN OFF CACHE BOOL "")
set(OPTICK_USE_D3D12 OFF CACHE BOOL "")
set(OPTICK_BUILD_GUI_APP OFF CACHE BOOL "")
set(OPTICK_BUILD_CONSOLE_SAMPLE OFF CACHE BOOL "")
add_subdirectory(optick)
