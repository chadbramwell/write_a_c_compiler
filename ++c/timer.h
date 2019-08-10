#pragma once
#include "stdint.h"

struct Timer
{
	uint64_t _start;
	uint64_t _end;

	void start();
	void end();
	float milliseconds() const;
};