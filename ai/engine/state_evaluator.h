#pragma once

#include "game/position.h"

class StateEvaluator {
public:
    virtual ~StateEvaluator() = default;
    virtual int evaluate(const GamePosition& state) const = 0;
};

class SimpleStateEvaluator : public StateEvaluator {
public:
    int evaluate(const GamePosition& state) const override;
};