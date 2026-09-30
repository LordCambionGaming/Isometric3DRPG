#pragma once
class D20Test {
public:
    static int Roll(); // 1-20
    static bool IsCriticalHit(int roll) { return roll == 20; }
    static bool IsCriticalFail(int roll) { return roll == 1; }
};
