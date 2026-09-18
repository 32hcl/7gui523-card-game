#pragma once

enum class GameEvent {
    RoundStart,
    CardPlayed,
    CardPassed,
    BigCardCountered,   // 大牌被反压
    ScoreStolen,        // 抢分成功
    SpecialVictory,
    GameOver,
};