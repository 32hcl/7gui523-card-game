#pragma once

// ai/players/ai_player.h
//
// 统一接口约定：
//   std::vector<Card> aiN_xxx_choose(
//       const Player& me,               // 我方（含手牌）
//       const Player& opp,              // 对方
//       const CardTypeResult& lastPlay, // 上一手（type==Invalid 表示先手）
//       const Deck& myDeck,             // 我方牌堆
//       int tableScore                  // 桌面分数
//   );
//
//   返回值：非空 = 出的牌；空 vector = pass
//
// 后续 AI 按此签名添加，注册到 battle_screen_game.cpp 的 switch 中。