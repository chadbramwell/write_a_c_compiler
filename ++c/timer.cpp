#include "timer.h"
#include "windows.h"

void Timer::start()
{
	_STATIC_ASSERT(sizeof(_start) == sizeof(LARGE_INTEGER));
	_STATIC_ASSERT(offsetof(LARGE_INTEGER, QuadPart) == 0);
	QueryPerformanceCounter((LARGE_INTEGER*)&_start);
}

void Timer::end()
{
	QueryPerformanceCounter((LARGE_INTEGER*)&_end);
}

float Timer::milliseconds() const
{
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);

	float ms = (_end - _start) * 1000.f / freq.QuadPart;
	return ms;
}
