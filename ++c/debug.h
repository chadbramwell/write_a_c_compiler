#pragma once

#define debug_break() do{__debugbreak();}while(0)
#define assert(x) do{if(!(x)){debug_break();}}while(0)