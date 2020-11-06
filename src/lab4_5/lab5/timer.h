#pragma once
#include <chrono>

struct Timer {
    typedef std::chrono::high_resolution_clock clock;
    typedef std::chrono::time_point<clock> time_point;
    typedef std::chrono::duration<double, std::milli> ms_duration;
    void Start() { m_start = clock::now(); }
    void Stop() { m_stop = clock::now(); }
    double Elapsed() const {
        return (ms_duration(m_stop - m_start) - m_overhead).count();
    }
private:
    static ms_duration GetOverhead() {
        Timer t;
        t.Start();
        t.Stop();
        return (t.m_stop - t.m_start);
    }
    time_point m_start;
    time_point m_stop;
    static ms_duration m_overhead;
};

Timer::ms_duration Timer::m_overhead = Timer::GetOverhead();