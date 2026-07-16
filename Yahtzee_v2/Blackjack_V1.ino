/*
 * BLACKJACK v3
 * Hardware: RP2040 (Raspberry Pi Pico or compatible)
 *   - ST7789 TFT 240x320 (Hardware SPI1: MOSI=GP11, SCK=GP10)
 *   - MAX7219 5-digit 7-segment display (DIN=GP8, CLK=GP9, CS=GP7)
 *   - Buzzer on GP6
 *   - 5 action buttons left→right: GP0, GP1, GP3, GP28, GP29
 *   - DEAL button GP5  |  UP GP2  |  DOWN GP27  |  ENTER GP12
 *
 * ┌─────────────────────────────────────────────────────┐
 * │  BUTTON GUIDE (5 physical buttons, left to right)   │
 * │  [1: HIT ] [2:STAND] [3: DBL] [4:SPLIT] [5:SIDE$]  │
 * │  DEAL button = also HIT   |   ENTER button = STAND  │
 * └─────────────────────────────────────────────────────┘
 *
 * CONTROLS:
 *   MENU        : DEAL = Start game | UP/DN = set bjCredits
 *                 DEAL hold 3s = Return to Tools menu | ENTER hold 3s = Reset stats
 *   BETTING     : UP/DN = Adjust bjBet | ENTER = Max bjBet | DEAL = Deal cards
 *                 Btn5 = Open side bets menu
 *   SIDE BETS   : UP/DN = scroll bets | ENTER = select/adjust | Btn5 = close
 *   PLAYER TURN : Btn1 or DEAL = HIT | Btn2 or ENTER = STAND | Btn3 = DOUBLE DOWN | Btn4 = SPLIT
 *   SPLIT TURN  : Btn1 or DEAL = HIT | Btn2 or ENTER = STAND (plays each split hand in turn)
 *   RESULT      : Auto-advances to next hand after 1 second (or press early)
 *   MENU 3s hold DEAL = Return to Tools menu | MENU 3s hold ENTER = Reset all stats
 *
 * SIDE BETS (persist hand-to-hand until removed, max 500 each):
 *   Perfect Pairs : Mixed pair 5:1 | Coloured pair 12:1 | Perfect pair 25:1
 *   21+3          : Flush 5:1 | Straight 10:1 | Three of a Kind 30:1
 *                   Straight Flush 40:1 | Suited Trips 100:1
 *   Lucky Lucky   : 19 or 20 total 2:1 | Unsuited 21 3:1 | Suited 21 15:1
 *                   6-7-8 or 7-7-7 100:1
 *
 * BLACKJACK RULES:
 *   - Closest to 21 without busting wins
 *   - Natural Blackjack (A + 10-value on first 2 cards) pays 3:2
 *   - Push (tie) returns your bjBet
 *   - Double Down: doubles your bjBet, you receive exactly one more card
 *   - Split: on equal-value first two cards, pay another bjBet, play two hands
 *   - Dealer (computer) hits soft 16 and below, stands on 17+
 *   - BJ DEALER ROLE: Player starts as dealer. If computer gets Blackjack it
 *     becomes dealer next hand. If player gets Blackjack, player takes dealer back.
 */


// ============================================================================
// PIN DEFINITIONS  (identical hardware to VideoPoker)
// ============================================================================




// Physical layout left→right: GP0, GP1, GP3, GP28, GP29
// Array index 4 = GP0 (leftmost), index 0 = GP29 (rightmost)
#define BTN_HIT_IDX    4   // leftmost  → GP0
#define BTN_STAND_IDX  3   // 2nd left  → GP1
#define BTN_DOUBLE_IDX 2   // middle    → GP3
#define BTN_SPLIT_IDX  1   // 4th left  → GP28
#define BTN_SIDEBET_IDX 0  // rightmost → GP29


// ============================================================================
// EEPROM ADDRESSES
// ============================================================================

#define EE_CREDITS_ADDR  450   // 2 bytes  (int)
#define EE_HANDS_ADDR  454   // 4 bytes  (uint32_t)
#define EE_P_WINS_ADDR  458   // 2 bytes  player wins
#define EE_C_WINS_ADDR  460   // 2 bytes  computer wins
#define EE_PUSHES_ADDR  462   // 2 bytes  bjPushes/ties
#define EE_BRIGHTNESS_ADDR  464  // 1 byte
#define EE_SOUND_ADDR  465   // 1 byte
#define EE_TOTAL_WON_ADDR  466  // 4 bytes
#define EE_TOTAL_BET_ADDR  470  // 4 bytes
#define EE_DEALER_ADDR  474   // 1 byte  0=player is dealer, 1=computer is dealer
// AI learning data
#define EE_AI_WINS_ADDR  476   // 2 bytes
#define EE_AI_GAMES_ADDR  478   // 2 bytes
#define EE_AI_HIT16_ADDR  480   // 2 bytes  times AI hit on 16 and won
#define EE_AI_H16T_ADDR  482   // 2 bytes  times AI hit on 16 (total)
#define EE_AI_STAND16_ADDR  484  // 2 bytes  times AI stood on 16 and won
#define EE_AI_S16T_ADDR  486   // 2 bytes  times AI stood on 16 (total)
#define EE_AI_SOFTADJ_ADDR  488  // 1 byte   soft-17 hit rate bias (0-255, 128=neutral)
#define EE_BIGGEST_WIN_ADDR  490 // 2 bytes  biggest net win

// ============================================================================
// DISPLAY
// ============================================================================


// ============================================================================
// COLOURS
// ============================================================================

#define C_BG          0x0821   // Very dark blue-black
#define C_TEXT        0xFFFF   // White
#define C_GREEN       0x07E0
#define C_LTGREEN     0x87F0
#define C_RED         0xF800
#define C_YELLOW      0xFFE0
#define C_ORANGE      0xFD20
#define C_CYAN        0x07FF
#define C_GRAY        0x7BEF
#define C_DARKGRAY    0x2965
#define C_GOLD        0xFEA0
#define C_DARKGOLD    0xB400
#define C_NAVY        0x000F
#define C_BLUE        0x001F
#define C_LTBLUE      0x04FF
#define C_MAGENTA     0xF81F
#define C_FELT        0x02A0   // Casino felt green
#define C_FELT_LT     0x04C0   // Lighter felt
#define C_CARD_WHITE  0xFFFF
#define C_RED_SUIT    0xE800
#define C_BLACK_SUIT  0x18C3
#define C_WIN_BG      0x0400
#define C_LOSE_BG     0x2000
#define C_PUSH_BG     0x18C3
#define C_HIGHLIGHT   0xFEA0   // Gold highlight border

// Card struct + deck vars + hand arrays defined in Yahtzee_v1.ino
// ============================================================================
// LAYOUT — Screen 240×320 portrait
// ============================================================================
// Pixel budget breakdown (must total 320):
//   0.. 21  Header bar        22px
//  22.. 40  Computer label    19px  (taller for size-2 score text)
//  41..100  Computer cards    60px
// 101..109  Mid result strip   9px
// 110..128  Player label      19px  (taller for size-2 score text)
// 129..188  Player cards      60px
// 189..220  Bottom info bar   32px  (bjBet + bjCredits on one row each, compact)
// 221..255  Hint bar          35px
// 256..319  Button guide bar  64px
// Total: 22+19+60+9+19+60+32+35+64 = 320 ✓

#define HDR_H         22
#define COMP_LBL_Y    22
#define COMP_LBL_H    19
#define COMP_CARD_Y   41
#define COMP_CARD_H   60
#define MID_SCORE_Y   101
#define MID_SCORE_H   9
#define PLAY_LBL_Y    110
#define PLAY_LBL_H    19
#define PLAY_CARD_Y   129
#define PLAY_CARD_H   60
#define PLAY_SCORE_Y  189
#define PLAY_SCORE_H  13
#define BOTTOM_Y      189
#define BOTTOM_H      67
#define BTN_BAR_Y     256
#define BTN_BAR_H     64

// Cards: 31px wide, 3px gap → 7 cards = 7*31+6*3 = 235px fits in 240 ✓
#define CARD_W   31
#define CARD_H   60
#define CARD_GAP  3
#define CARD_X0   2

// ============================================================================
// GAME STATE
// ============================================================================

// BJGameState enum defined in Yahtzee_v1.ino

// BJHandResult enum defined in Yahtzee_v1.ino




// ── Split state ──

// ── Side Bet state ──
// 3 side bets: 0=Perfect Pairs, 1=21+3, 2=Lucky Lucky
#define SB_PERFECT_PAIRS 0
#define SB_21PLUS3       1
#define SB_LUCKY_LUCKY   2


// Side bjBet UI state


// ── AI Learning ──
// BJAILearning struct + aiData defined in Yahtzee_v1.ino

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

void bj_drawScreen();
void bj_drawMenu();
void bj_drawBetting();
void bj_drawSideBetScreen();
void bj_drawGameScreen(bool hideCompHole);
void bj_drawResult();
void bj_drawGameOver();

void bj_drawSuitSymbol(int cx, int cy, int suit, uint16_t color, int sz);
void bj_drawBJCard(int x, int y, Card& c, bool highlight);
void bj_drawCardBack(int x, int y);
void bj_drawCardClipped(int x, int y, Card& c, int clipH);
void bj_drawCardBackClipped(int x, int y, int clipH);
void bj_drawHandRow(Card* hand, int count, int rowY, bool hideSecond);
void bj_animateDealCard(int x, int y, Card& c, bool faceUp);

void bj_shuffleDeck();
Card dealCard();
int  bj_cardBJValue(uint8_t val);
int  bj_handTotal(Card* hand, int count);
bool bj_isSoft(Card* hand, int count);
bool bj_isBlackjack(Card* hand, int count);
bool bj_isBust(Card* hand, int count);

void bj_startNewHand();
void bj_goToBetting();
void bj_openSideBets();
void bj_closeSideBets();
void bj_playerHit();
void bj_playerStand();
void bj_playerDouble();
void bj_playerSplit();
void bj_runComputerTurn();
void bj_resolveHand();
void bj_resolveSideBets();
bool bj_aiShouldHit(Card* hand, int count, int playerTotal);

// Side bjBet evaluation helpers
int  bj_evalPerfectPairs(Card& c1, Card& c2);
int  bj_eval21Plus3(Card& p1, Card& p2, Card& dealer);
int  bj_evalLuckyLucky(Card& p1, Card& p2, Card& dealer);

void bj_drawHeaderBar();
void bj_drawBottomBar();
void bj_drawButtonBar();
void bj_drawResultBanner();
void bj_flashWin(bool playerWon);

void max7219Send(byte reg, byte data);
void bj_segInit();
// max7219SetBrightness removed — using Yahtzee's

void bj_showCreditsOnSeg() {
  int tmp = constrain(bjCredits, 0, 99999);
  int digits[5];
  for(int i = 4; i >= 0; i--) { digits[i] = tmp % 10; tmp /= 10; }
  int firstNZ = 4;
  for(int i = 0; i < 5; i++) { if(digits[i] != 0) { firstNZ = i; break; } }
  for(int i = 0; i < 5; i++) {
    byte reg = i + 1;
    byte val = (i < firstNZ) ? 0x0F : (byte)digits[i];
    max7219Send(reg, val);
  }
}

void bj_segCountUp(int from, int to, int durationMs) {
  if(to <= from) { bj_showCreditsOnSeg(); return; }
  int steps = min(to - from, 30);
  int stepDelay = max(durationMs / steps, 1);
  for(int s = 0; s <= steps; s++) {
    int val = from + (int)((long)(to - from) * s / steps);
    int sv = bjCredits; bjCredits = val; bj_showCreditsOnSeg(); bjCredits = sv;
    delay(stepDelay);
  }
  bj_showCreditsOnSeg();
}

// ============================================================================
// BUZZER
// ============================================================================

// buzzerTone removed — using Yahtzee's

void bj_buzzerMenu()   { buzzerTone(700, 35); }
void buzzerCoinIn() { buzzerTone(1400, 30); delay(15); buzzerTone(1800, 30); }
void bj_buzzerPush()   { buzzerTone(600, 80); delay(30); buzzerTone(600, 80); }

void bj_buzzerHit() {
  buzzerTone(900, 25); delay(10); buzzerTone(1100, 25);
}

void bj_buzzerDeal() {
  for(int i = 0; i < 4; i++) { buzzerTone(400 + i*80, 22); delay(15); }
}

void bj_buzzerBust() {
  int n[] = {440, 330, 220};
  for(int i = 0; i < 3; i++) { buzzerTone(n[i], 160); delay(30); }
}

void bj_buzzerWin() {
  buzzerTone(659, 80); delay(15); buzzerTone(784, 80); delay(15);
  buzzerTone(1047, 160);
}

void bj_buzzerBlackjack() {
  int n[] = {523, 659, 784, 1047, 1319, 1047, 1319, 1568};
  int d[] = {80,  80,  80,  120,  180,  80,  120,  300};
  for(int i = 0; i < 8; i++) { buzzerTone(n[i], d[i]); delay(20); }
}

void bj_buzzerGameOver() {
  int n[] = {440, 370, 311, 261};
  for(int i = 0; i < 4; i++) { buzzerTone(n[i], 220); delay(40); }
}

void buzzerStartup() {
  int n[] = {523, 659, 784, 1047};
  int d[] = {80,  80,  80,  200};
  for(int i = 0; i < 4; i++) { buzzerTone(n[i], d[i]); delay(25); }
}

// ============================================================================
// EEPROM
// ============================================================================

void bj_loadFromEEPROM() {
  // Must match Yahtzee's EEPROM.begin(4096) — Yahtzee AI data sits at address 1000+
  // Using 512 here would truncate the address space and corrupt Yahtzee AI saves
  EEPROM.begin(4096);
  int c = (EEPROM.read(EE_CREDITS_ADDR) << 8) | EEPROM.read(EE_CREDITS_ADDR + 1);
  bjCredits = (c > 0 && c <= 32767) ? c : 200;

  bjHandsPlayed = ((uint32_t)EEPROM.read(EE_HANDS_ADDR)<<24) |
                ((uint32_t)EEPROM.read(EE_HANDS_ADDR+1)<<16) |
                ((uint32_t)EEPROM.read(EE_HANDS_ADDR+2)<<8)  |
                 (uint32_t)EEPROM.read(EE_HANDS_ADDR+3);

  bjPlayerWins = (EEPROM.read(EE_P_WINS_ADDR)<<8) | EEPROM.read(EE_P_WINS_ADDR+1);
  bjCompWins   = (EEPROM.read(EE_C_WINS_ADDR)<<8) | EEPROM.read(EE_C_WINS_ADDR+1);
  bjPushes     = (EEPROM.read(EE_PUSHES_ADDR)<<8) | EEPROM.read(EE_PUSHES_ADDR+1);

  int br = EEPROM.read(EE_BRIGHTNESS_ADDR);
  if(br >= 0 && br <= 15) max7219Brightness = br;
  int snd = EEPROM.read(EE_SOUND_ADDR);
  if(snd == 0 || snd == 1) soundEnabled = (snd == 1);

  bjLifetimeWon = ((uint32_t)EEPROM.read(EE_TOTAL_WON_ADDR)<<24) |
                ((uint32_t)EEPROM.read(EE_TOTAL_WON_ADDR+1)<<16) |
                ((uint32_t)EEPROM.read(EE_TOTAL_WON_ADDR+2)<<8)  |
                 (uint32_t)EEPROM.read(EE_TOTAL_WON_ADDR+3);
  bjLifetimeBet = ((uint32_t)EEPROM.read(EE_TOTAL_BET_ADDR)<<24) |
                ((uint32_t)EEPROM.read(EE_TOTAL_BET_ADDR+1)<<16) |
                ((uint32_t)EEPROM.read(EE_TOTAL_BET_ADDR+2)<<8)  |
                 (uint32_t)EEPROM.read(EE_TOTAL_BET_ADDR+3);

  uint8_t df = EEPROM.read(EE_DEALER_ADDR);
  bjPlayerIsDealer = (df != 1);

  int bw = (EEPROM.read(EE_BIGGEST_WIN_ADDR)<<8) | EEPROM.read(EE_BIGGEST_WIN_ADDR+1);
  bjBiggestWin = (bw >= 0 && bw <= 32767) ? bw : 0;
}

void bj_saveToEEPROM() {
  EEPROM.write(EE_CREDITS_ADDR,    (bjCredits>>8)&0xFF);
  EEPROM.write(EE_CREDITS_ADDR+1,   bjCredits&0xFF);
  EEPROM.write(EE_HANDS_ADDR,   (bjHandsPlayed>>24)&0xFF);
  EEPROM.write(EE_HANDS_ADDR+1, (bjHandsPlayed>>16)&0xFF);
  EEPROM.write(EE_HANDS_ADDR+2, (bjHandsPlayed>>8)&0xFF);
  EEPROM.write(EE_HANDS_ADDR+3,  bjHandsPlayed&0xFF);
  EEPROM.write(EE_P_WINS_ADDR,   (bjPlayerWins>>8)&0xFF);
  EEPROM.write(EE_P_WINS_ADDR+1,  bjPlayerWins&0xFF);
  EEPROM.write(EE_C_WINS_ADDR,   (bjCompWins>>8)&0xFF);
  EEPROM.write(EE_C_WINS_ADDR+1,  bjCompWins&0xFF);
  EEPROM.write(EE_PUSHES_ADDR,   (bjPushes>>8)&0xFF);
  EEPROM.write(EE_PUSHES_ADDR+1,  bjPushes&0xFF);
  EEPROM.write(EE_BRIGHTNESS_ADDR, max7219Brightness);
  EEPROM.write(EE_SOUND_ADDR,      soundEnabled ? 1 : 0);
  EEPROM.write(EE_TOTAL_WON_ADDR,   (bjLifetimeWon>>24)&0xFF);
  EEPROM.write(EE_TOTAL_WON_ADDR+1, (bjLifetimeWon>>16)&0xFF);
  EEPROM.write(EE_TOTAL_WON_ADDR+2, (bjLifetimeWon>>8)&0xFF);
  EEPROM.write(EE_TOTAL_WON_ADDR+3,  bjLifetimeWon&0xFF);
  EEPROM.write(EE_TOTAL_BET_ADDR,   (bjLifetimeBet>>24)&0xFF);
  EEPROM.write(EE_TOTAL_BET_ADDR+1, (bjLifetimeBet>>16)&0xFF);
  EEPROM.write(EE_TOTAL_BET_ADDR+2, (bjLifetimeBet>>8)&0xFF);
  EEPROM.write(EE_TOTAL_BET_ADDR+3,  bjLifetimeBet&0xFF);
  EEPROM.write(EE_DEALER_ADDR,  bjPlayerIsDealer ? 0 : 1);
  EEPROM.write(EE_BIGGEST_WIN_ADDR,  (bjBiggestWin>>8)&0xFF);
  EEPROM.write(EE_BIGGEST_WIN_ADDR+1, bjBiggestWin&0xFF);
  EEPROM.commit();
}

void bj_loadAIFromEEPROM() {
  aiData.wins         = (EEPROM.read(EE_AI_WINS_ADDR)<<8)  | EEPROM.read(EE_AI_WINS_ADDR+1);
  aiData.games        = (EEPROM.read(EE_AI_GAMES_ADDR)<<8) | EEPROM.read(EE_AI_GAMES_ADDR+1);
  aiData.hit16wins    = (EEPROM.read(EE_AI_HIT16_ADDR)<<8) | EEPROM.read(EE_AI_HIT16_ADDR+1);
  aiData.hit16total   = (EEPROM.read(EE_AI_H16T_ADDR)<<8)  | EEPROM.read(EE_AI_H16T_ADDR+1);
  aiData.stand16wins  = (EEPROM.read(EE_AI_STAND16_ADDR)<<8)| EEPROM.read(EE_AI_STAND16_ADDR+1);
  aiData.stand16total = (EEPROM.read(EE_AI_S16T_ADDR)<<8)  | EEPROM.read(EE_AI_S16T_ADDR+1);
  uint8_t sa = EEPROM.read(EE_AI_SOFTADJ_ADDR);
  aiData.softAdj = (sa < 255) ? sa : 128;
  if(aiData.softAdj == 0) aiData.softAdj = 128;
}

void bj_saveAIToEEPROM() {
  EEPROM.write(EE_AI_WINS_ADDR,   (aiData.wins>>8)&0xFF);
  EEPROM.write(EE_AI_WINS_ADDR+1,  aiData.wins&0xFF);
  EEPROM.write(EE_AI_GAMES_ADDR,  (aiData.games>>8)&0xFF);
  EEPROM.write(EE_AI_GAMES_ADDR+1, aiData.games&0xFF);
  EEPROM.write(EE_AI_HIT16_ADDR,  (aiData.hit16wins>>8)&0xFF);
  EEPROM.write(EE_AI_HIT16_ADDR+1, aiData.hit16wins&0xFF);
  EEPROM.write(EE_AI_H16T_ADDR,   (aiData.hit16total>>8)&0xFF);
  EEPROM.write(EE_AI_H16T_ADDR+1,  aiData.hit16total&0xFF);
  EEPROM.write(EE_AI_STAND16_ADDR, (aiData.stand16wins>>8)&0xFF);
  EEPROM.write(EE_AI_STAND16_ADDR+1,aiData.stand16wins&0xFF);
  EEPROM.write(EE_AI_S16T_ADDR,   (aiData.stand16total>>8)&0xFF);
  EEPROM.write(EE_AI_S16T_ADDR+1,  aiData.stand16total&0xFF);
  EEPROM.write(EE_AI_SOFTADJ_ADDR, aiData.softAdj);
  EEPROM.commit();
}

// ============================================================================
// DECK / CARD LOGIC
// ============================================================================

void bj_shuffleDeck() {
  for(int i = 0; i < 52; i++) bjDeckIndices[i] = i;
  for(int i = 51; i > 0; i--) {
    int j = random(0, i + 1);
    int t = bjDeckIndices[i]; bjDeckIndices[i] = bjDeckIndices[j]; bjDeckIndices[j] = t;
  }
  bjDeckTop = 0;
}

Card indexToCard(int idx) {
  Card c;
  c.suit  = idx / 13;
  c.value = (idx % 13) + 1; // 1=Ace, 2-10, 11=J, 12=Q, 13=K
  return c;
}

Card dealCard() {
  if(bjDeckTop >= 52) bj_shuffleDeck();
  return indexToCard(bjDeckIndices[bjDeckTop++]);
}

// Blackjack value of one card: Ace=11, face=10, others face value
int bj_cardBJValue(uint8_t val) {
  if(val == 1)  return 11;
  if(val >= 10) return 10;
  return val;
}

// Best Blackjack total ≤ 21 (reduces Aces 11→1 as needed)
int bj_handTotal(Card* hand, int count) {
  int total = 0, aces = 0;
  for(int i = 0; i < count; i++) {
    int v = hand[i].value;
    if(v == 1) { aces++; total += 11; }
    else total += (v >= 10) ? 10 : v;
  }
  while(total > 21 && aces > 0) { total -= 10; aces--; }
  return total;
}

// True if there's an Ace still counted as 11
bool bj_isSoft(Card* hand, int count) {
  int total = 0, aces = 0;
  for(int i = 0; i < count; i++) {
    int v = hand[i].value;
    if(v == 1) { aces++; total += 11; }
    else total += (v >= 10) ? 10 : v;
  }
  while(total > 21 && aces > 0) { total -= 10; aces--; }
  return (aces > 0 && total <= 21);
}

bool bj_isBlackjack(Card* hand, int count) {
  return (count == 2 && bj_handTotal(hand, count) == 21);
}

bool bj_isBust(Card* hand, int count) {
  return (bj_handTotal(hand, count) > 21);
}

// ============================================================================
// AI STRATEGY  (based on Yahtzee AI pattern — learned decision thresholds)
// ============================================================================

// Returns true if computer should HIT given its hand and the player's up-card
// Implements standard Basic Strategy with learned soft-17 adjustment
bool bj_aiShouldHit(Card* hand, int count, int playerUpValue) {
  int total   = bj_handTotal(hand, count);
  bool soft   = bj_isSoft(hand, count);
  int upCard  = playerUpValue; // player visible card value (capped at 10)
  if(upCard > 10) upCard = 10;
  if(upCard == 1) upCard = 11;

  // Never hit 21, never hit >21 (bust check happens before calling)
  if(total >= 21) return false;

  // === SOFT HANDS (Ace counted as 11) ===
  if(soft) {
    if(total <= 17) {
      // Learned adjustment: if AI has historically won more with soft17 hits, be more aggressive
      // aiData.softAdj: 128=standard, >128=hit more, <128=hit less
      if(total == 17) {
        // Standard: always hit soft 17
        // Learning tweak: if softAdj > 180 (learned aggression), also hit soft 18 vs strong player
        return true; // always hit soft 17
      }
      return true; // soft ≤16 always hit
    }
    if(total == 18) {
      // Hit soft 18 vs strong player up-card 9/10/A — learned aggression
      if(upCard >= 9) return true;
      // Learned: if softAdj biased high, hit more
      if(aiData.softAdj > 160 && upCard >= 7) return true;
      return false;
    }
    return false; // soft 19+ stand
  }

  // === HARD HANDS ===
  if(total <= 8)  return true;  // always hit 8 or under
  if(total >= 17) return false; // always stand hard 17+

  // total 9-16: use Basic Strategy vs player up-card
  // Dealer (computer) strategy table:
  //   Stand on 12-16 vs player showing 2-6 (weak hand)
  //   Hit  on 12-16 vs player showing 7+  (strong hand)
  //   Always hit 9, 10, 11

  if(total == 11) return true;
  if(total == 10) return (upCard < 10); // hit unless player shows 10/A
  if(total == 9)  return (upCard >= 3 && upCard <= 6);

  // 12-16: the classic "grey zone"
  // === LEARNED THRESHOLD for hard 16 ===
  if(total == 16) {
    // Check what we've learned about hit vs stand on 16
    float hitRate   = (aiData.hit16total   > 0) ? (float)aiData.hit16wins   / aiData.hit16total   : 0.5f;
    float standRate = (aiData.stand16total > 0) ? (float)aiData.stand16wins / aiData.stand16total : 0.5f;
    // If we have at least 10 data points for each, use learned preference
    if(aiData.hit16total >= 10 && aiData.stand16total >= 10) {
      if(upCard >= 7) {
        return (hitRate >= standRate); // learned whether to hit vs strong up-card
      } else {
        return (hitRate > standRate + 0.05f); // needs clear advantage to hit vs weak
      }
    }
    // Fallback to basic strategy
    return (upCard >= 7);
  }

  // 12-15: standard Basic Strategy
  if(total >= 12 && total <= 15) {
    return (upCard >= 7); // hit vs strong up-card, stand vs weak
  }

  return true; // catch-all hit
}

// Update AI learning data after a hand
// wasHit16: whether AI chose to hit on hard 16 this hand
// aiWon: true if AI won
void bj_aiLearnFromHand(bool wasHit16, bool wasStand16, bool aiWon) {
  aiData.games++;
  if(aiWon) aiData.wins++;
  if(wasHit16)   { aiData.hit16total++;   if(aiWon) aiData.hit16wins++;   }
  if(wasStand16) { aiData.stand16total++; if(aiWon) aiData.stand16wins++; }

  // Adjust soft17 tendency based on overall win rate
  if(aiData.games > 0 && aiData.games % 20 == 0) {
    float wr = (float)aiData.wins / aiData.games;
    if(wr > 0.52f && aiData.softAdj < 240) aiData.softAdj++;
    if(wr < 0.48f && aiData.softAdj > 16)  aiData.softAdj--;
  }
}

// ============================================================================
// SIDE BET EVALUATION
// ============================================================================

// Perfect Pairs — player's first 2 cards
// Returns multiplier (5, 12, 25) or 0 for no win
// Mixed pair (same value, diff colour)   → 5:1
// Coloured pair (same value, same colour, diff suit) → 12:1
// Perfect pair (same value, same suit)   → 25:1
int bj_evalPerfectPairs(Card& c1, Card& c2) {
  if(c1.value != c2.value) return 0;   // not a pair
  if(c1.suit == c2.suit) return 25;    // perfect pair — identical suit
  // Same colour? Suits 0,3 = black (Spades, Clubs); 1,2 = red (Hearts, Diamonds)
  bool c1red = (c1.suit == 1 || c1.suit == 2);
  bool c2red = (c2.suit == 1 || c2.suit == 2);
  if(c1red == c2red) return 12;        // coloured pair — same colour, different suit
  return 5;                            // mixed pair — same value, different colours
}

// 21+3 — player's 2 cards + dealer's face-up card (3-card poker hand)
// Returns multiplier or 0
// Flush (same suit, not straight)              → 5:1
// Straight (3 consecutive, not same suit)      → 10:1
// Three of a Kind (same value, diff suits)     → 30:1
// Straight Flush (consecutive + same suit)     → 40:1
// Suited Three of a Kind (same value+suit)     → 100:1
int bj_eval21Plus3(Card& p1, Card& p2, Card& dealer) {
  // Normalise values for straight checking: Ace=1, J=11, Q=12, K=13
  int v1 = p1.value, v2 = p2.value, v3 = dealer.value;
  int s1 = p1.suit,  s2 = p2.suit,  s3 = dealer.suit;

  bool sameSuit  = (s1 == s2 && s2 == s3);
  bool sameValue = (v1 == v2 && v2 == v3);

  // Sort values for straight detection
  int vals[3] = {v1, v2, v3};
  // Simple sort
  if(vals[0] > vals[1]) { int t = vals[0]; vals[0] = vals[1]; vals[1] = t; }
  if(vals[1] > vals[2]) { int t = vals[1]; vals[1] = vals[2]; vals[2] = t; }
  if(vals[0] > vals[1]) { int t = vals[0]; vals[0] = vals[1]; vals[1] = t; }

  bool isStraight = (vals[2] - vals[1] == 1 && vals[1] - vals[0] == 1) ||
                    // Ace-low wrap: A-2-3 (1,2,3) already covered; Ace-high: Q-K-A (12,13,1→treat as 14)
                    (vals[0] == 1 && vals[1] == 12 && vals[2] == 13);

  if(sameValue && sameSuit)  return 100;  // Suited Three of a Kind
  if(sameValue)              return 30;   // Three of a Kind
  if(isStraight && sameSuit) return 40;   // Straight Flush
  if(isStraight)             return 10;   // Straight
  if(sameSuit)               return 5;    // Flush
  return 0;
}

// Lucky Lucky — player's 2 cards + dealer's face-up card, based on total
// Returns multiplier or 0
// Total 19 or 20                          → 2:1
// Total 21 unsuited                       → 3:1
// Total 21 suited (not 6-7-8 or 7-7-7)   → 15:1
// 6-7-8 or 7-7-7 (any suits)             → 100:1
// 6-7-8 or 7-7-7 suited                  → 200:1 (bonus!)
int bj_evalLuckyLucky(Card& p1, Card& p2, Card& dealer) {
  // Use BJ values (Ace=11, face=10)
  int v1 = bj_cardBJValue(p1.value);
  int v2 = bj_cardBJValue(p2.value);
  int v3 = bj_cardBJValue(dealer.value);
  int total = v1 + v2 + v3;
  // Reduce aces if over 21
  // Count aces
  int aces = (p1.value==1?1:0) + (p2.value==1?1:0) + (dealer.value==1?1:0);
  while(total > 21 && aces > 0) { total -= 10; aces--; }

  bool sameSuit = (p1.suit == p2.suit && p2.suit == dealer.suit);

  // Special combos: 6-7-8 or 7-7-7
  int rv1 = p1.value, rv2 = p2.value, rv3 = dealer.value;
  // Sort raw values
  int rvals[3] = {rv1, rv2, rv3};
  if(rvals[0]>rvals[1]){int t=rvals[0];rvals[0]=rvals[1];rvals[1]=t;}
  if(rvals[1]>rvals[2]){int t=rvals[1];rvals[1]=rvals[2];rvals[2]=t;}
  if(rvals[0]>rvals[1]){int t=rvals[0];rvals[0]=rvals[1];rvals[1]=t;}

  bool is678 = (rvals[0]==6 && rvals[1]==7 && rvals[2]==8);
  bool is777 = (rvals[0]==7 && rvals[1]==7 && rvals[2]==7);

  if((is678 || is777) && sameSuit) return 200;
  if(is678 || is777)               return 100;
  if(total == 21 && sameSuit)      return 15;
  if(total == 21)                  return 3;
  if(total == 20 || total == 19)   return 2;
  return 0;
}

// Evaluate all active side bets and update bjCredits
// Call AFTER cards are dealt (bjPlayerHand[0,1] and bjCompHand[0] available)
void bj_resolveSideBets() {
  // Reset payout tracking
  for(int i = 0; i < BJ_SB_COUNT; i++) {
    bjSideBetPayout[i] = 0;
    bjSideBetWon[i]    = false;
  }
  if(bjPlayerCount < 2 || bjCompCount < 1) return;

  Card& p1 = bjPlayerHand[0];
  Card& p2 = bjPlayerHand[1];
  Card& d1 = bjCompHand[0];  // dealer face-up card

  // Perfect Pairs
  if(bjSideBetAmount[SB_PERFECT_PAIRS] > 0) {
    int mult = bj_evalPerfectPairs(p1, p2);
    if(mult > 0) {
      int win = bjSideBetAmount[SB_PERFECT_PAIRS] * mult;
      bjCredits += win + bjSideBetAmount[SB_PERFECT_PAIRS];  // win + stake back
      bjSideBetPayout[SB_PERFECT_PAIRS] = win;
      bjSideBetWon[SB_PERFECT_PAIRS] = true;
    }
    // No refund on loss — side bjBet lost
  }

  // 21+3
  if(bjSideBetAmount[SB_21PLUS3] > 0) {
    int mult = bj_eval21Plus3(p1, p2, d1);
    if(mult > 0) {
      int win = bjSideBetAmount[SB_21PLUS3] * mult;
      bjCredits += win + bjSideBetAmount[SB_21PLUS3];
      bjSideBetPayout[SB_21PLUS3] = win;
      bjSideBetWon[SB_21PLUS3] = true;
    }
  }

  // Lucky Lucky
  if(bjSideBetAmount[SB_LUCKY_LUCKY] > 0) {
    int mult = bj_evalLuckyLucky(p1, p2, d1);
    if(mult > 0) {
      int win = bjSideBetAmount[SB_LUCKY_LUCKY] * mult;
      bjCredits += win + bjSideBetAmount[SB_LUCKY_LUCKY];
      bjSideBetPayout[SB_LUCKY_LUCKY] = win;
      bjSideBetWon[SB_LUCKY_LUCKY] = true;
    }
  }
}

// ============================================================================
// SUIT SYMBOL DRAWING (reused from VideoPoker verbatim)
// ============================================================================

void bj_drawSuitSymbol(int cx, int cy, int suit, uint16_t color, int sz) {
  switch(suit) {
    case 0: // Spades
      tft.fillTriangle(cx, cy-5*sz, cx-5*sz, cy+3*sz, cx+5*sz, cy+3*sz, color);
      tft.fillCircle(cx-3*sz, cy, 3*sz, color);
      tft.fillCircle(cx+3*sz, cy, 3*sz, color);
      tft.fillRect(cx-sz, cy+3*sz, 2*sz, 3*sz, color);
      tft.fillRect(cx-3*sz, cy+5*sz, 6*sz, sz, color);
      break;
    case 1: // Hearts
      tft.fillCircle(cx-3*sz, cy-2*sz, 3*sz, color);
      tft.fillCircle(cx+3*sz, cy-2*sz, 3*sz, color);
      tft.fillTriangle(cx-6*sz, cy, cx+6*sz, cy, cx, cy+6*sz, color);
      break;
    case 2: // Diamonds
      tft.fillTriangle(cx, cy-6*sz, cx-5*sz, cy, cx+5*sz, cy, color);
      tft.fillTriangle(cx, cy+6*sz, cx-5*sz, cy, cx+5*sz, cy, color);
      break;
    case 3: // Clubs
      tft.fillCircle(cx,       cy-3*sz, 3*sz, color);
      tft.fillCircle(cx-4*sz, cy+1*sz, 3*sz, color);
      tft.fillCircle(cx+4*sz, cy+1*sz, 3*sz, color);
      tft.fillRect(cx-sz,   cy+2*sz, 2*sz, 4*sz, color);
      tft.fillRect(cx-3*sz, cy+5*sz, 6*sz, sz,   color);
      break;
  }
}

// ============================================================================
// CARD DRAWING
// ============================================================================

void bj_drawBJCard(int x, int y, Card& c, bool highlight) {
  uint16_t bg     = highlight ? C_HIGHLIGHT : C_CARD_WHITE;
  uint16_t border = highlight ? C_GOLD : C_DARKGRAY;
  uint16_t sc     = (c.suit == 1 || c.suit == 2) ? C_RED_SUIT : C_BLACK_SUIT;

  tft.fillRoundRect(x, y, CARD_W, CARD_H, 4, bg);
  tft.drawRoundRect(x, y, CARD_W, CARD_H, 4, border);
  if(highlight) tft.drawRoundRect(x+1, y+1, CARD_W-2, CARD_H-2, 3, C_GOLD);

  const char* vn = bjValueNames[c.value - 1];

  // Centre-top: large value — moved up to y+5 (was y+12)
  tft.setTextSize(2);
  tft.setTextColor(sc);
  int cw = (c.value == 10) ? 24 : 12;
  tft.setCursor(x + (CARD_W - cw) / 2, y + 5);
  tft.print(vn);

  // Centre-lower: suit symbol — moved down to 72% (was 63%)
  bj_drawSuitSymbol(x + CARD_W/2, y + (CARD_H * 72) / 100, c.suit, sc, 2);
}

void bj_drawCardBack(int x, int y) {
  tft.fillRoundRect(x, y, CARD_W, CARD_H, 4, C_NAVY);
  tft.drawRoundRect(x, y, CARD_W, CARD_H, 4, C_GOLD);
  tft.drawRoundRect(x+2, y+2, CARD_W-4, CARD_H-4, 3, C_DARKGOLD);
  int cx = x + CARD_W/2, cy = y + CARD_H/2;
  for(int r = 4; r <= 16; r += 6) {
    tft.drawLine(cx-r, cy, cx, cy-r, C_LTBLUE);
    tft.drawLine(cx, cy-r, cx+r, cy, C_LTBLUE);
    tft.drawLine(cx+r, cy, cx, cy+r, C_LTBLUE);
    tft.drawLine(cx, cy+r, cx-r, cy, C_LTBLUE);
  }
  tft.fillTriangle(cx-4, cy, cx, cy-4, cx+4, cy, C_GOLD);
  tft.fillTriangle(cx-4, cy, cx, cy+4, cx+4, cy, C_GOLD);
}

// Clipped card face (for flip animation — draws centre strip of height clipH)
void bj_drawCardClipped(int x, int y, Card& c, int clipH) {
  if(clipH <= 0) return;
  uint16_t sc = (c.suit == 1 || c.suit == 2) ? C_RED_SUIT : C_BLACK_SUIT;
  int stripY  = y + (CARD_H - clipH) / 2;
  tft.fillRect(x, stripY, CARD_W, clipH, C_CARD_WHITE);
  if(clipH > CARD_H / 2) tft.drawRoundRect(x, y, CARD_W, CARD_H, 4, C_DARKGRAY);
  if(clipH < 8) return;
  const char* vn = bjValueNames[c.value - 1];
  int topY = y + 5;   // matches bj_drawBJCard
  if(stripY <= topY && stripY + clipH >= topY + 16) {
    tft.setTextSize(2); tft.setTextColor(sc);
    int cw = (c.value == 10) ? 24 : 12;
    tft.setCursor(x + (CARD_W - cw) / 2, topY);
    tft.print(vn);
  }
  int scy = y + (CARD_H * 72) / 100;  // matches bj_drawBJCard
  if(stripY <= scy && stripY + clipH >= scy + 6)
    bj_drawSuitSymbol(x + CARD_W/2, scy, c.suit, sc, 2);
}

void bj_drawCardBackClipped(int x, int y, int clipH) {
  if(clipH <= 0) return;
  int stripY = y + (CARD_H - clipH) / 2;
  tft.fillRect(x, stripY, CARD_W, clipH, C_NAVY);
  if(clipH > CARD_H / 2) tft.drawRoundRect(x, y, CARD_W, CARD_H, 4, C_GOLD);
  if(clipH > 6) {
    int cx = x + CARD_W/2, cy = y + CARD_H/2;
    for(int r = 4; r <= 14; r += 6)
      tft.drawLine(cx-r, cy, cx, cy-r, C_LTBLUE);
  }
}

// Animate a single card dealing into position (flip-up effect)
void bj_animateDealCard(int x, int y, Card& c, bool faceUp) {
  const int STEPS = 8, STEP_MS = 18;
  // Phase 1: Shrink back
  for(int s = STEPS; s >= 0; s--) {
    tft.fillRect(x, y, CARD_W, CARD_H, C_BG);
    bj_drawCardBackClipped(x, y, (s * CARD_H) / STEPS);
    delay(STEP_MS);
  }
  delay(25);
  // Phase 2: Unsquish as face or back
  for(int s = 0; s <= STEPS; s++) {
    tft.fillRect(x, y, CARD_W, CARD_H, C_BG);
    if(faceUp) bj_drawCardClipped(x, y, c, (s * CARD_H) / STEPS);
    else       bj_drawCardBackClipped(x, y, (s * CARD_H) / STEPS);
    delay(STEP_MS);
  }
  if(faceUp) bj_drawBJCard(x, y, c, false);
  else       bj_drawCardBack(x, y);
}

// Draw a hand row starting at cardY. If hideSecond=true, card[1] is face-down (hole card)
void bj_drawHandRow(Card* hand, int count, int rowY, bool hideSecond) {
  for(int i = 0; i < count; i++) {
    int x = CARD_X0 + i * (CARD_W + CARD_GAP);
    if(x + CARD_W > 240) x = 240 - CARD_W - 1; // clamp
    if(hideSecond && i == 1)
      bj_drawCardBack(x, rowY);
    else
      bj_drawBJCard(x, rowY, hand[i], false);
  }
  // Clear any leftover area after last card
  int endX = CARD_X0 + count * (CARD_W + CARD_GAP);
  if(endX < 240)
    tft.fillRect(endX, rowY, 240 - endX, CARD_H, C_BG);
}

// ============================================================================
// HEADER BAR
// ============================================================================

void bj_drawHeaderBar() {
  // Gold top bar
  tft.fillRect(0, 0, 240, HDR_H, C_DARKGOLD);
  tft.setTextSize(1);
  tft.setTextColor(C_BG);
  tft.setCursor(4, 4);
  tft.print("BLACKJACK");

  // Dealer badge on right
  int badgeX = 130;
  tft.fillRoundRect(badgeX, 2, 107, 18, 3, bjPlayerIsDealer ? C_GREEN : C_RED);
  tft.setTextColor(C_BG);
  tft.setTextSize(1);
  tft.setCursor(badgeX + 4, 7);
  if(bjPlayerIsDealer)
    tft.print("YOU: DEALER");
  else
    tft.print("CPU: DEALER");

  // Suit symbols
  bj_drawSuitSymbol(68, 11, 0, 0x18C3, 1);
  bj_drawSuitSymbol(80, 11, 1, C_RED,   1);
  bj_drawSuitSymbol(92, 11, 2, C_RED,   1);
  bj_drawSuitSymbol(104, 11, 3, 0x18C3, 1);
}

// ============================================================================
// SCORE / LABEL STRIPS
// ============================================================================

// Draw a coloured label strip above a hand row: "COMPUTER" or "PLAYER"
void drawHandLabel(int y, const char* label, uint16_t col, int total, bool hidden) {
  tft.fillRect(0, y, 240, 18, C_BG);
  // Label on left (size 1)
  tft.setTextSize(1);
  tft.setTextColor(col);
  tft.setCursor(2, y + 5);
  tft.print(label);
  if(!hidden && total > 0) {
    // Show score on right — size 2 for prominence
    char buf[8];
    itoa(total, buf, 10);
    uint16_t scoreCol = (total > 21) ? C_RED : (total == 21 ? C_GOLD : C_YELLOW);
    tft.setTextSize(2);
    tft.setTextColor(scoreCol);
    int sw = strlen(buf) * 12;  // size-2: each char ~12px wide
    tft.setCursor(240 - sw - 4, y + 1);
    tft.print(buf);
    if(total > 21) {
      tft.setTextSize(1);
      tft.setTextColor(C_RED);
      tft.setCursor(240 - sw - 34, y + 5);
      tft.print("BUST");
    }
  }
  tft.drawFastHLine(0, y + 17, 240, C_DARKGRAY);
}

// ============================================================================
// BOTTOM INFO BAR — Bet / Credits / hint text
// ============================================================================

void bj_drawBottomBar() {
  tft.fillRect(0, BOTTOM_Y, 240, BOTTOM_H, C_BG);
  tft.drawFastHLine(0, BOTTOM_Y, 240, C_DARKGRAY);

  // Layout: two rows side-by-side, each row = label on left + value on right
  // Row 1: "BET"  [value up to 5 digits]  |  "CREDITS"  [value up to 5 digits]
  // Row 2: hint text / split indicator
  //
  // Screen is 240px wide.  Each half = 120px.
  // Label at size 1 (6px/char), value at size 2 (12px/char).
  // 5 digits at size 2 = 60px.  Label "BET" = 18px, "CRED" = 24px.
  // Left half:   "BET" at x=4,  value right-aligned ending at x=116
  // Right half:  "CRED" at x=122, value right-aligned ending at x=236
  // Divider at x=120.

  int row1 = BOTTOM_Y + 4;   // main info row
  int row2 = BOTTOM_Y + 24;  // hint / split row

  tft.drawFastVLine(120, BOTTOM_Y + 2, 18, C_DARKGRAY);  // centre divider

  // ── Left: BET ──
  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  tft.setCursor(4, row1 + 4); tft.print("BET");

  {
    char buf[16];
    int  dispBet = bjDoubledDown ? bjBet * 2 : bjBet;
    itoa(dispBet, buf, 10);
    // 5 digits max at size 2 = 60px wide; right-align to x=116
    int vw = strlen(buf) * 12;
    tft.setTextSize(2); tft.setTextColor(C_YELLOW);
    tft.setCursor(116 - vw, row1);
    tft.print(buf);
    if(bjDoubledDown) {
      tft.setTextSize(1); tft.setTextColor(C_ORANGE);
      tft.setCursor(4, row1 + 11); tft.print("DBL");
    }
  }

  // ── Right: CREDITS ──
  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  tft.setCursor(124, row1 + 4); tft.print("CRED");

  {
    char buf[16];
    itoa(bjCredits, buf, 10);
    int vw = strlen(buf) * 12;
    tft.setTextSize(2); tft.setTextColor(C_YELLOW);
    tft.setCursor(236 - vw, row1);
    tft.print(buf);
  }

  // ── Row 2: hint or split/sidebet indicator ──
  tft.setTextSize(1);
  int totalSB = 0;
  for(int si = 0; si < BJ_SB_COUNT; si++) totalSB += bjSideBetAmount[si];

  if(bjIsSplit) {
    tft.setTextColor(C_MAGENTA);
    tft.setCursor(4, row2);
    tft.print("SPLIT  Hand ");
    tft.print(bjSplitHandIdx + 1);
    tft.print("/2  each bjBet:");
    tft.setTextColor(C_YELLOW); tft.print(bjSplitBet);
  } else if(totalSB > 0 && (bjGameState == BJ_STATE_BETTING || bjGameState == BJ_STATE_PLAYER_TURN || bjGameState == BJ_STATE_RESULT)) {
    tft.setTextColor(C_GOLD);
    tft.setCursor(4, row2);
    tft.print("SIDE:");
    for(int si = 0; si < BJ_SB_COUNT; si++) {
      if(bjSideBetAmount[si] > 0) {
        tft.setTextColor(C_DARKGRAY);
        if(si == SB_PERFECT_PAIRS) tft.print("PP");
        else if(si == SB_21PLUS3)  tft.print("21+3");
        else                        tft.print("LL");
        tft.setTextColor(C_YELLOW); tft.print(bjSideBetAmount[si]); tft.print(" ");
      }
    }
  } else {
    tft.setTextColor(C_DARKGRAY);
    tft.setCursor(4, row2);
    if(bjGameState == BJ_STATE_BETTING) {
      tft.print("UP/DN:Bet  DEAL:Play  Btn5:SideBets");
    } else if(bjGameState == BJ_STATE_RESULT) {
      tft.print("DEAL or ENTER = Next Hand");
    }
  }

  // ── Row 3: Last hand result (WIN / LOSS / PUSH) ─────────────────────────────
  // Show during result screen and carry through to next betting screen
  if(bjGameState == BJ_STATE_RESULT || bjGameState == BJ_STATE_BETTING || bjGameState == BJ_STATE_PLAYER_TURN) {
    int row3 = BOTTOM_Y + 42;
    // Clear the row area first
    tft.fillRect(0, row3 - 2, 240, 22, C_BG);

    if(bjLastNetResult > 0) {
      // Win — green label + gold value
      tft.setTextSize(1); tft.setTextColor(C_GREEN);
      tft.setCursor(4, row3 + 4); tft.print("WIN");
      char buf[12]; itoa(bjLastNetResult, buf, 10);
      int vw = strlen(buf) * 12;
      tft.setTextSize(2); tft.setTextColor(C_GOLD);
      tft.setCursor(36, row3);  // label "WIN" = 18px + small gap
      tft.print("+"); tft.print(buf);
    } else if(bjLastNetResult < 0) {
      // Loss — red label + red value
      tft.setTextSize(1); tft.setTextColor(C_RED);
      tft.setCursor(4, row3 + 4); tft.print("LOSS");
      char buf[12]; itoa(-bjLastNetResult, buf, 10);  // print as positive
      tft.setTextSize(2); tft.setTextColor(C_RED);
      tft.setCursor(36, row3);
      tft.print("-"); tft.print(buf);
    } else {
      // Push — gray
      tft.setTextSize(1); tft.setTextColor(C_DARKGRAY);
      tft.setCursor(4, row3 + 4); tft.print("PUSH");
      tft.setTextSize(2); tft.setTextColor(C_GRAY);
      tft.setCursor(36, row3); tft.print("  --");
    }
  }
}

// ============================================================================
// BUTTON GUIDE BAR — physical button labels shown on screen
// BTN1=HIT  BTN2=STAND  BTN3=DOUBLE  BTN4=SPLIT  BTN5=SIDE$
// ============================================================================

void bj_drawButtonBar() {
  tft.fillRect(0, BTN_BAR_Y, 240, BTN_BAR_H, C_BG);
  tft.drawFastHLine(0, BTN_BAR_Y, 240, C_GOLD);

  bool inPlay     = (bjGameState == BJ_STATE_PLAYER_TURN);
  bool canDouble  = inPlay && (bjPlayerCount == 2) && (bjCredits >= bjBet) && !bjIsSplit;
  bool canSplit   = inPlay && (bjPlayerCount == 2) && !bjIsSplit &&
                    (bjCredits >= bjBet) &&
                    (bj_cardBJValue(bjPlayerHand[0].value) == bj_cardBJValue(bjPlayerHand[1].value));
  bool hasSideBets = false;
  for(int i = 0; i < BJ_SB_COUNT; i++) if(bjSideBetAmount[i] > 0) hasSideBets = true;
  bool canSideBet = (bjGameState == BJ_STATE_BETTING);

  // 5 boxes: each 44px wide, 3px gap → 5*44+4*3 = 232px ✓
  const int BW = 44, BH = 40, BGAP = 3, BY = BTN_BAR_Y + 2, BX0 = 2;

  struct B { const char* top; const char* bot; uint16_t col; bool lit; };
  B boxes[5] = {
    { "1:HIT",   "DEAL btn",  C_CYAN,    inPlay },
    { "2:STAND", "ENTER btn", C_GREEN,   inPlay },
    { "3:DBL",   "x2 bjBet",    C_ORANGE,  canDouble },
    { "4:SPLIT", "=val pair", C_MAGENTA, canSplit },
    { "5:SIDE$", hasSideBets?"active":"set",  C_GOLD, canSideBet || hasSideBets },
  };

  for(int i = 0; i < 5; i++) {
    int bx = BX0 + i * (BW + BGAP);
    uint16_t bg  = boxes[i].lit ? 0x0841 : C_BG;
    uint16_t bdr = boxes[i].lit ? boxes[i].col : C_DARKGRAY;
    uint16_t tc  = boxes[i].lit ? boxes[i].col : C_DARKGRAY;

    tft.fillRoundRect(bx, BY, BW, BH, 3, bg);
    tft.drawRoundRect(bx, BY, BW, BH, 3, bdr);

    tft.setTextSize(1); tft.setTextColor(tc);
    // top label centred
    int tw = strlen(boxes[i].top) * 6;
    tft.setCursor(bx + (BW - tw)/2, BY + 6);
    tft.print(boxes[i].top);
    // bottom label centred (smaller / hint)
    if(boxes[i].bot[0]) {
      int bw2 = strlen(boxes[i].bot) * 6;
      tft.setTextColor(C_DARKGRAY);
      tft.setCursor(bx + (BW - bw2)/2, BY + 20);
      tft.print(boxes[i].bot);
    }
  }

  // Row below boxes: DEAL and ENTER aliases
  tft.setTextSize(1); tft.setTextColor(C_DARKGRAY);
  tft.setCursor(4, BY + BH + 3);
  if(inPlay) {
    if(bjIsSplit)
      tft.print("DEAL=HIT  ENTER=STAND  Hand 1 then 2");
    else
      tft.print("DEAL=HIT  ENTER=STAND  Btn4=SPLIT");
  } else if(bjGameState == BJ_STATE_BETTING)
    tft.print("DEAL=Play  ENTER=MaxBet  Btn5=SideBets");
}

// ============================================================================
// FULL GAME SCREEN
// ============================================================================

void bj_drawGameScreen(bool hideCompHole) {
  tft.fillScreen(C_BG);
  bj_drawHeaderBar();

  int compTotal = hideCompHole ? bj_cardBJValue(bjCompHand[0].value) : bj_handTotal(bjCompHand, bjCompCount);
  int playTotal = bj_handTotal(bjPlayerHand, bjPlayerCount);

  // Computer side
  drawHandLabel(COMP_LBL_Y, "COMPUTER", C_CYAN, compTotal, hideCompHole);
  bj_drawHandRow(bjCompHand, bjCompCount, COMP_CARD_Y, hideCompHole);

  // Mid separator
  tft.drawFastHLine(0, MID_SCORE_Y, 240, C_DARKGRAY);

  // Player side — during split, show active hand label with hand number
  if(bjIsSplit) {
    char lbl[16];
    if(bjSplitHandIdx == 0)
      strcpy(lbl, "HAND 1 of 2");
    else
      strcpy(lbl, "HAND 2 of 2");
    drawHandLabel(PLAY_LBL_Y, lbl, C_LTGREEN, playTotal, false);
  } else {
    drawHandLabel(PLAY_LBL_Y, "PLAYER", C_LTGREEN, playTotal, false);
  }
  bj_drawHandRow(bjPlayerHand, bjPlayerCount, PLAY_CARD_Y, false);

  bj_drawBottomBar();
  bj_drawButtonBar();
}

// ============================================================================
// SIDE BET SCREEN — full-screen overlay over betting layout
// ============================================================================

void bj_drawSideBetScreen() {
  // Draw betting screen underneath as context
  tft.fillScreen(C_BG);
  bj_drawHeaderBar();

  // Draw a dark panel covering the card area
  tft.fillRect(0, COMP_LBL_Y, 240, PLAY_CARD_Y + PLAY_CARD_H - COMP_LBL_Y, 0x0010);
  tft.drawRect(0, COMP_LBL_Y, 240, PLAY_CARD_Y + PLAY_CARD_H - COMP_LBL_Y, C_GOLD);

  // Title
  tft.setTextSize(1); tft.setTextColor(C_GOLD);
  tft.setCursor(4, COMP_LBL_Y + 4);
  tft.print("SIDE BETS  (UP/DN=scroll  ENTER=select)");
  tft.drawFastHLine(0, COMP_LBL_Y + 14, 240, C_DARKGOLD);

  // Side bjBet rows
  // Payouts to show per bjBet
  const char* sbPayouts[BJ_SB_COUNT] = {
    "Mix5  Col12  Perf25",
    "Fl5 St10 3K30 SF40 S3K100",
    "19/20x2 21x3 Suit21x15 678/777x100"
  };

  int rowH = 38;
  int startY = COMP_LBL_Y + 18;

  for(int i = 0; i < BJ_SB_COUNT; i++) {
    int ry = startY + i * rowH;
    bool sel = (i == bjSbSelected);
    bool adj = sel && bjSbAdjusting;

    // Row background
    uint16_t rowBg  = adj  ? 0x0820 :   // dark green when adjusting
                      sel  ? 0x1082 :   // dark blue when selected
                             0x0010;    // very dark otherwise
    uint16_t rowBdr = adj  ? C_GREEN :
                      sel  ? C_CYAN  : C_DARKGRAY;

    tft.fillRoundRect(2, ry, 236, rowH - 2, 3, rowBg);
    tft.drawRoundRect(2, ry, 236, rowH - 2, 3, rowBdr);

    // Bet name
    tft.setTextSize(1);
    tft.setTextColor(sel ? C_YELLOW : C_GRAY);
    tft.setCursor(8, ry + 4);
    tft.print(bjSbNames[i]);

    // Bet amount — right side, large
    char abuf[12];
    if(bjSideBetAmount[i] > 0) {
      itoa(bjSideBetAmount[i], abuf, 10);
      tft.setTextSize(2);
      tft.setTextColor(adj ? C_GREEN : C_YELLOW);
      int aw = strlen(abuf) * 12;
      tft.setCursor(236 - aw - 4, ry + 2);
      tft.print(abuf);
    } else {
      tft.setTextSize(1);
      tft.setTextColor(C_DARKGRAY);
      tft.setCursor(170, ry + 8);
      tft.print("OFF");
    }

    // Payout hint
    tft.setTextSize(1);
    tft.setTextColor(0x4208);  // dim
    tft.setCursor(8, ry + 16);
    // Truncate to fit
    char pline[36];
    strncpy(pline, sbPayouts[i], 35);
    pline[35] = 0;
    tft.print(pline);

    // Adjust arrows if in adjust mode
    if(adj) {
      tft.setTextColor(C_GREEN);
      tft.setCursor(8, ry + 26);
      tft.print("UP/DN: +/-10   ENTER: done");
    } else if(sel) {
      tft.setTextColor(C_CYAN);
      tft.setCursor(8, ry + 26);
      tft.print("ENTER to adjust amount");
    }
  }

  // Bottom hint
  bj_drawBottomBar();

  // Override button bar with side bjBet controls
  tft.fillRect(0, BTN_BAR_Y, 240, BTN_BAR_H, C_BG);
  tft.drawFastHLine(0, BTN_BAR_Y, 240, C_GOLD);
  tft.setTextSize(1); tft.setTextColor(C_DARKGRAY);
  tft.setCursor(4, BTN_BAR_Y + 8);
  tft.print("UP/DN = scroll bets");
  tft.setCursor(4, BTN_BAR_Y + 20);
  tft.print("ENTER = select / confirm amount");
  tft.setCursor(4, BTN_BAR_Y + 32);
  tft.setTextColor(C_GOLD);
  tft.print("Btn5 = close side bets");

  // Total side bjBet display
  int total = 0;
  for(int i = 0; i < BJ_SB_COUNT; i++) total += bjSideBetAmount[i];
  if(total > 0) {
    tft.setTextColor(C_YELLOW);
    tft.setCursor(4, BTN_BAR_Y + 48);
    tft.print("Total side bets: ");
    tft.print(total);
  }
}

void bj_drawMenu() {
  tft.fillScreen(C_BG);

  // ── Title ──
  tft.setTextSize(4); tft.setTextColor(C_GOLD);
  tft.setCursor(8, 12); tft.print("BLACK");
  tft.setTextColor(C_DARKGOLD);
  tft.setCursor(10, 14); tft.print("BLACK");
  tft.setTextColor(C_GOLD);
  tft.setCursor(8, 12); tft.print("BLACK");

  tft.setTextSize(4); tft.setTextColor(C_GREEN);
  tft.setCursor(8, 50); tft.print("JACK");
  tft.setTextColor(0x03C0);
  tft.setCursor(10, 52); tft.print("JACK");
  tft.setTextColor(C_GREEN);
  tft.setCursor(8, 50); tft.print("JACK");

  // Suit decoration — spaced 40px apart so symbols don't touch (sz=2 → ~20px wide each)
  bj_drawSuitSymbol(172, 28, 1, C_RED,   2);   // Hearts
  bj_drawSuitSymbol(212, 28, 2, C_RED,   2);   // Diamonds
  bj_drawSuitSymbol(172, 62, 0, C_TEXT,  2);   // Spades
  bj_drawSuitSymbol(212, 62, 3, C_TEXT,  2);   // Clubs

  tft.drawFastHLine(0, 84, 240, C_DARKGOLD);

  // ── Dealer status + Credits — side by side ──
  // Left: Dealer role   Right: Credits
  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  tft.setCursor(8,  90); tft.print("DEALER");
  tft.setTextSize(2);
  tft.setTextColor(bjPlayerIsDealer ? C_GREEN : C_RED);
  tft.setCursor(8, 100);
  tft.print(bjPlayerIsDealer ? "YOU" : "COMPUTER");

  tft.drawFastVLine(118, 86, 38, C_DARKGRAY);

  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  tft.setCursor(126, 90); tft.print("CREDITS");
  tft.setTextSize(2); tft.setTextColor(C_YELLOW);
  tft.setCursor(126, 100); tft.print(bjCredits);
  tft.setTextSize(1); tft.setTextColor(C_GREEN);
  tft.setCursor(126, 117); tft.print("UP/DN: adjust");

  // ── Stats ──
  tft.drawFastHLine(0, 128, 240, C_DARKGRAY);
  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  int sy = 134;
  tft.setCursor(8, sy);     tft.print("Hands:"); tft.setTextColor(C_TEXT);    tft.setCursor(80, sy);    tft.print(bjHandsPlayed);
  tft.setTextColor(C_GRAY);
  tft.setCursor(8, sy+12);  tft.print("You win:"); tft.setTextColor(C_GREEN); tft.setCursor(80, sy+12); tft.print(bjPlayerWins);
  tft.setTextColor(C_GRAY);
  tft.setCursor(8, sy+24);  tft.print("CPU win:"); tft.setTextColor(C_RED);   tft.setCursor(80, sy+24); tft.print(bjCompWins);
  tft.setTextColor(C_GRAY);
  tft.setCursor(8, sy+36);  tft.print("Pushes:"); tft.setTextColor(C_CYAN);   tft.setCursor(80, sy+36); tft.print(bjPushes);

  if(bjLifetimeBet > 50) {
    int rtp = (int)((float)bjLifetimeWon / bjLifetimeBet * 100);
    tft.setTextColor(C_GRAY); tft.setCursor(140, sy+12); tft.print("RTP:");
    tft.setTextColor(rtp >= 95 ? C_GREEN : (rtp >= 80 ? C_YELLOW : C_RED));
    tft.setCursor(165, sy+12); tft.print(rtp); tft.print("%");
  }
  if(aiData.games > 10) {
    int awr = (int)((float)aiData.wins / aiData.games * 100);
    tft.setTextColor(C_GRAY); tft.setCursor(140, sy+24); tft.print("AI WR:");
    tft.setTextColor(C_MAGENTA); tft.setCursor(165, sy+24); tft.print(awr); tft.print("%");
  }

  // ── Pay table ──
  tft.drawFastHLine(0, 184, 240, C_DARKGRAY);
  tft.setTextSize(1); tft.setTextColor(C_CYAN);
  tft.setCursor(8, 190); tft.print("PAYOUTS:");
  tft.setTextColor(C_GOLD);
  tft.setCursor(8, 201); tft.print("Blackjack      3:2");
  tft.setTextColor(C_YELLOW);
  tft.setCursor(8, 212); tft.print("21 (hit)       2:1");
  tft.setTextColor(C_TEXT);
  tft.setCursor(8, 223); tft.print("Win            1:1");
  tft.setTextColor(C_CYAN);
  tft.setCursor(8, 234); tft.print("Tie   Dealer wins (wins ties!)");
  tft.setTextColor(C_GRAY);
  tft.setCursor(8, 245); tft.print("Split: equal pair, new bjBet each hand");

  // ── Dealer rule ──
  tft.drawFastHLine(0, 257, 240, C_DARKGRAY);
  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  tft.setCursor(4, 263); tft.print("Dealer: hits soft 16, stands on 17+");

  // ── Controls ──
  tft.drawFastHLine(0, 275, 240, C_DARKGRAY);
  tft.setTextColor(C_GREEN);
  tft.setCursor(4, 281); tft.print("UP/DN: set credits   DEAL: start game");
  tft.setTextColor(C_DARKGRAY);
  tft.setCursor(4, 292); tft.print("DEAL 3s:back  ENTER 3s:reset stats");
}

// ============================================================================
// BETTING SCREEN
// ============================================================================

void bj_drawBetting() {
  tft.fillScreen(C_BG);
  bj_drawHeaderBar();

  drawHandLabel(COMP_LBL_Y, "COMPUTER", C_CYAN, 0, true);
  for(int i = 0; i < 2; i++)
    bj_drawCardBack(CARD_X0 + i*(CARD_W+CARD_GAP), COMP_CARD_Y);

  tft.drawFastHLine(0, MID_SCORE_Y, 240, C_DARKGRAY);

  drawHandLabel(PLAY_LBL_Y, "PLAYER", C_LTGREEN, 0, true);
  for(int i = 0; i < 2; i++)
    bj_drawCardBack(CARD_X0 + i*(CARD_W+CARD_GAP), PLAY_CARD_Y);

  bj_drawBottomBar();
  bj_drawButtonBar();
}

// ============================================================================
// RESULT SCREEN
// ============================================================================

void bj_drawResultBanner() {
  const char* msg  = "";
  uint16_t    bg   = C_PUSH_BG;
  uint16_t    fg   = C_YELLOW;

  switch(bjHandResult) {
    case BJ_RES_PLAYER_BJ:  msg = "BLACKJACK! YOU WIN!"; bg = C_WIN_BG;  fg = C_GOLD;    break;
    case BJ_RES_COMP_BJ:    msg = "CPU BLACKJACK!";       bg = C_LOSE_BG; fg = C_RED;     break;
    case BJ_RES_PLAYER_WIN: msg = "YOU WIN!";              bg = C_WIN_BG;  fg = C_GREEN;   break;
    case BJ_RES_COMP_WIN:   msg = "CPU WINS";              bg = C_LOSE_BG; fg = C_RED;     break;
    case BJ_RES_PUSH:       msg = "PUSH — TIE";            bg = C_PUSH_BG; fg = C_CYAN;    break;
    case BJ_RES_PLAYER_BUST:msg = "BUST! CPU WINS";        bg = C_LOSE_BG; fg = C_ORANGE;  break;
    case BJ_RES_COMP_BUST:  msg = "CPU BUSTS! YOU WIN!";   bg = C_WIN_BG;  fg = C_LTGREEN; break;
    default: break;
  }

  // Override banner text for tied scores — dealer wins the tie.
  // Make the dealer advantage explicit so the player understands the mechanic.
  if(!bjIsSplit) {
    int pt = bj_handTotal(bjPlayerHand, bjPlayerCount);
    int ct = bj_handTotal(bjCompHand, bjCompCount);
    if(pt == ct && !bj_isBust(bjCompHand, bjCompCount) && !bj_isBust(bjPlayerHand, bjPlayerCount)
       && bjHandResult != BJ_RES_PLAYER_BJ && bjHandResult != BJ_RES_COMP_BJ) {
      if(bjHandResult == BJ_RES_PLAYER_WIN) msg = "TIE - YOU WIN  (Dealer perk!)";
      else                             msg = "TIE - CPU WINS (Dealer perk!)";
    }
  }

  // Banner at mid-score strip
  tft.fillRect(0, MID_SCORE_Y, 240, MID_SCORE_H, bg);
  tft.drawRect(0, MID_SCORE_Y, 240, MID_SCORE_H, fg);
  tft.setTextSize(1); tft.setTextColor(fg);
  int msgW = strlen(msg) * 6;
  tft.setCursor((240 - msgW) / 2, MID_SCORE_Y + 1);
  tft.print(msg);

  // Payout info
  bool playerWon = (bjHandResult == BJ_RES_PLAYER_BJ || bjHandResult == BJ_RES_PLAYER_WIN || bjHandResult == BJ_RES_COMP_BUST);
  bool push      = (bjHandResult == BJ_RES_PUSH);

  int payY = PLAY_SCORE_Y;
  tft.fillRect(0, payY, 240, PLAY_SCORE_H + 18, C_BG);
  tft.setTextSize(1);

  if(bjIsSplit) {
    // Show each hand result individually, then combined net
    bool h1Won  = (bjSplitResult == BJ_RES_PLAYER_WIN || bjSplitResult == BJ_RES_COMP_BUST);
    bool h1Push = (bjSplitResult == BJ_RES_PUSH);
    bool h2Won  = (bjHandResult  == BJ_RES_PLAYER_WIN || bjHandResult  == BJ_RES_COMP_BUST);
    bool h2Push = (bjHandResult  == BJ_RES_PUSH);

    tft.setCursor(4, payY + 2);
    tft.setTextColor(C_MAGENTA); tft.print("H1:");
    if(h1Won)       { tft.setTextColor(C_GREEN);  tft.print("WIN"); }
    else if(h1Push) { tft.setTextColor(C_CYAN);   tft.print("PUSH"); }
    else            { tft.setTextColor(C_RED);    tft.print("LOSS"); }

    tft.setTextColor(C_DARKGRAY); tft.print("  ");
    tft.setTextColor(C_MAGENTA); tft.print("H2:");
    if(h2Won)       { tft.setTextColor(C_GREEN);  tft.print("WIN"); }
    else if(h2Push) { tft.setTextColor(C_CYAN);   tft.print("PUSH"); }
    else            { tft.setTextColor(C_RED);    tft.print("LOSS"); }

    // Combined net
    tft.setCursor(4, payY + 13);
    if(bjLastPayout > 0) {
      tft.setTextColor(C_GREEN); tft.print("NET +"); tft.setTextColor(C_YELLOW); tft.print(bjLastPayout); tft.print(" cr");
    } else if(bjLastPayout == 0) {
      tft.setTextColor(C_CYAN); tft.print("BREAK EVEN");
    } else {
      tft.setTextColor(C_RED); tft.print("NET "); tft.print(bjLastPayout); tft.print(" cr");
    }
  } else {
    if(playerWon && bjLastPayout > 0) {
      tft.setTextColor(C_GREEN); tft.setCursor(4, payY + 2);
      tft.print("WIN +"); tft.setTextColor(C_YELLOW); tft.print(bjLastPayout); tft.print(" cr");
      if(bjHandResult == BJ_RES_PLAYER_BJ)                               { tft.setTextColor(C_GOLD);   tft.print("  (BJ 3:2)"); }
      else if(bj_handTotal(bjPlayerHand, bjPlayerCount) == 21)             { tft.setTextColor(C_GOLD);   tft.print("  (21! 2:1)"); }
    } else if(push) {
      tft.setTextColor(C_CYAN); tft.setCursor(4, payY + 2); tft.print("BET RETURNED");
    } else {
      tft.setTextColor(C_RED); tft.setCursor(4, payY + 2);
      if(bjHandResult == BJ_RES_COMP_WIN && bj_handTotal(bjPlayerHand, bjPlayerCount) == bj_handTotal(bjCompHand, bjCompCount)) {
        tft.print("TIE — DEALER WINS");
      } else {
        tft.print("LOST -"); tft.print(bjDoubledDown ? bjBet*2 : bjBet);
      }
    }
  }

  // Side bjBet results
  bool anySideWin = false;
  for(int i = 0; i < BJ_SB_COUNT; i++) if(bjSideBetWon[i]) { anySideWin = true; break; }
  if(anySideWin) {
    tft.setCursor(4, payY + 22);
    tft.setTextColor(C_GOLD); tft.print("SIDE:");
    for(int i = 0; i < BJ_SB_COUNT; i++) {
      if(bjSideBetWon[i]) {
        tft.setTextColor(C_GREEN); tft.print("+"); tft.print(bjSideBetPayout[i]);
        tft.setTextColor(C_DARKGRAY); tft.print("(");
        // Short name
        if(i == SB_PERFECT_PAIRS) tft.print("PP");
        else if(i == SB_21PLUS3)  tft.print("21+3");
        else                       tft.print("LL");
        tft.print(") ");
      }
    }
  }

  // New dealer indicator
  tft.setTextColor(C_DARKGRAY); tft.setCursor(150, payY + 2);
  tft.print(bjPlayerIsDealer ? "You=Dealer" : "CPU=Dealer");
}

void bj_drawResult() {
  // Full redraw of game screen with both hands revealed
  tft.fillScreen(C_BG);
  bj_drawHeaderBar();

  int compTotal = bj_handTotal(bjCompHand, bjCompCount);
  int playTotal = bj_handTotal(bjPlayerHand, bjPlayerCount);

  drawHandLabel(COMP_LBL_Y, "COMPUTER", C_CYAN, compTotal, false);
  bj_drawHandRow(bjCompHand, bjCompCount, COMP_CARD_Y, false);

  bj_drawResultBanner();

  drawHandLabel(PLAY_LBL_Y, "PLAYER", C_LTGREEN, playTotal, false);
  bj_drawHandRow(bjPlayerHand, bjPlayerCount, PLAY_CARD_Y, false);

  bj_drawBottomBar();
  bj_drawButtonBar();
}

void bj_flashWin(bool playerWon) {
  for(int f = 0; f < 4; f++) {
    tft.fillRect(0, MID_SCORE_Y, 240, MID_SCORE_H,
                 f%2 == 0 ? (playerWon ? C_GOLD : C_RED) : C_BG);
    delay(90);
  }
}

// ============================================================================
// GAME OVER SCREEN
// ============================================================================

void bj_drawGameOver() {
  tft.fillScreen(C_BG);
  tft.setTextSize(3); tft.setTextColor(C_RED);
  tft.setCursor(18, 30);  tft.print("GAME");
  tft.setCursor(18, 66);  tft.print("OVER");
  tft.drawFastHLine(0, 102, 240, C_RED);

  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  tft.setCursor(8, 112); tft.print("Out of bjCredits!");
  tft.setCursor(8, 130); tft.print("Hands played:"); tft.setTextColor(C_TEXT);    tft.setCursor(110,130); tft.print(bjHandsPlayed);
  tft.setTextColor(C_GRAY);
  tft.setCursor(8, 142); tft.print("Your wins:");    tft.setTextColor(C_GREEN);   tft.setCursor(110,142); tft.print(bjPlayerWins);
  tft.setTextColor(C_GRAY);
  tft.setCursor(8, 154); tft.print("CPU wins:");     tft.setTextColor(C_RED);     tft.setCursor(110,154); tft.print(bjCompWins);
  tft.setTextColor(C_GRAY);
  tft.setCursor(8, 166); tft.print("Biggest win:");  tft.setTextColor(C_GOLD);    tft.setCursor(110,166); tft.print(bjBiggestWin);

  tft.drawFastHLine(0, 182, 240, C_DARKGRAY);
  tft.setTextSize(2); tft.setTextColor(C_GREEN);
  tft.setCursor(8, 192); tft.print("ENTER or DEAL: New Game (200 cr)");
  tft.setTextSize(1); tft.setTextColor(C_GRAY);
  tft.setCursor(8, 218); tft.print("ENTER 3s: reset stats");
}

// ============================================================================
// SCREEN ROUTER
// ============================================================================

void bj_drawScreen() {
  switch(bjGameState) {
    case BJ_STATE_MENU:        bj_drawMenu();     break;
    case BJ_STATE_BETTING:     bj_drawBetting();  break;
    case BJ_STATE_SIDEBET:     bj_drawSideBetScreen(); break;
    case BJ_STATE_PLAYER_TURN: bj_drawGameScreen(true);  break;
    case BJ_STATE_COMP_TURN:   bj_drawGameScreen(false); break;
    case BJ_STATE_RESULT:      bj_drawResult();   break;
    case BJ_STATE_GAMEOVER:    bj_drawGameOver(); break;
  }
  bj_showCreditsOnSeg();
}

// ============================================================================
// DEAL ANIMATION HELPERS
// ============================================================================

// Return X position for card index i in a hand row
int cardX(int i) {
  return CARD_X0 + i * (CARD_W + CARD_GAP);
}

// Animate dealing one card into the player or computer row
// Used during bj_startNewHand for the initial 4-card deal-in
void dealAnimCard(Card* hand, int idx, int rowY, bool faceUp) {
  int x = cardX(idx);
  // Quick slide-in from top of respective zone
  const int SAFE_TOP_COMP = COMP_LBL_Y + COMP_LBL_H;
  const int SAFE_TOP_PLAY = PLAY_LBL_Y + PLAY_LBL_H;
  int safeTop = (rowY == COMP_CARD_Y) ? SAFE_TOP_COMP : SAFE_TOP_PLAY;
  int safeBot = rowY + CARD_H;
  const int STEPS = 10, STEP_MS = 16;

  int prevTop = safeTop;
  for(int s = 0; s <= STEPS; s++) {
    int yPos  = safeTop + (s * (rowY - safeTop)) / STEPS;
    int yBot  = min(yPos + CARD_H, safeBot);
    int drawH = yBot - yPos;
    if(yPos > prevTop)
      tft.fillRect(x, prevTop, CARD_W, yPos - prevTop, C_BG);
    prevTop = yPos;
    if(drawH > 0) {
      if(faceUp) bj_drawCardClipped(x, yPos, hand[idx], drawH);
      else       bj_drawCardBackClipped(x, yPos, drawH);
    }
    delay(STEP_MS);
  }
  if(rowY > safeTop) tft.fillRect(x, safeTop, CARD_W, rowY - safeTop, C_BG);
  if(faceUp) bj_drawBJCard(x, rowY, hand[idx], false);
  else       bj_drawCardBack(x, rowY);
}

void bj_openSideBets() {
  bjSbReturnState = bjGameState;
  bjSbSelected    = 0;
  bjSbAdjusting   = false;
  bjGameState     = BJ_STATE_SIDEBET;
  bj_drawScreen();
}

void bj_closeSideBets() {
  bjGameState = bjSbReturnState;
  bj_drawScreen();
}

// ── Track total side bjBet amount deducted this hand so we can refund on error ──
static int bj_sideBetDeductedThisHand = 0;

// ============================================================================
// GAME FLOW
// ============================================================================

// Track AI decisions for learning (reset each hand)
static bool bjAiHitOn16 = false, bjAiStoodOn16 = false;

void bj_startNewHand() {
  bjDoubledDown  = false;
  bjHandResult   = BJ_RES_NONE;
  bjAiHitOn16    = false;
  bjAiStoodOn16  = false;
  bjPlayerCount  = 0;
  bjCompCount    = 0;
  bjLastNetResult = 0;  // clear last-hand result so it doesn't show mid-game
  // Reset split state
  bjIsSplit      = false;
  bjSplitHandIdx = 0;
  bjSplitCount   = 0;
  bjSplitBet     = 0;
  bjSplitResult  = BJ_RES_NONE;

  // Deduct main bjBet
  bjCredits -= bjBet;
  bjLifetimeBet += bjBet;
  if(bjCredits < 0) bjCredits = 0;
  bjHandsPlayed++;

  // Deduct active side bets (they persist until removed)
  bj_sideBetDeductedThisHand = 0;
  for(int i = 0; i < BJ_SB_COUNT; i++) {
    bjSideBetPayout[i] = 0;
    bjSideBetWon[i]    = false;
    if(bjSideBetAmount[i] > 0) {
      // Only bjBet if we have enough bjCredits
      if(bjCredits >= bjSideBetAmount[i]) {
        bjCredits -= bjSideBetAmount[i];
        bj_sideBetDeductedThisHand += bjSideBetAmount[i];
      } else {
        // Not enough bjCredits — zero out this side bjBet
        bjSideBetAmount[i] = 0;
      }
    }
  }

  // Shuffle only when fewer than 15 cards remain (enough for a full hand with splits/doubles).
  // Reshuffling every hand biases the deck — cards early in a fresh shuffle
  // are statistically more likely to cluster, skewing results.
  if(bjDeckTop >= 37) {
    bj_shuffleDeck();
  }

  // Deal alternating: player, comp, player, comp  (standard BJ deal order)
  bjPlayerHand[bjPlayerCount++] = dealCard();
  bjCompHand[bjCompCount++]     = dealCard();
  bjPlayerHand[bjPlayerCount++] = dealCard();
  bjCompHand[bjCompCount++]     = dealCard();   // this is the hole card (index 1)

  bjGameState = BJ_STATE_PLAYER_TURN;

  // Draw base layout
  tft.fillScreen(C_BG);
  bj_drawHeaderBar();
  drawHandLabel(COMP_LBL_Y, "COMPUTER", C_CYAN, 0, true);
  drawHandLabel(PLAY_LBL_Y, "PLAYER",   C_LTGREEN, 0, false);
  tft.drawFastHLine(0, MID_SCORE_Y, 240, C_DARKGRAY);
  bj_drawBottomBar();
  bj_drawButtonBar();

  // Animate deal: P1, C1, P2, C2 (hole face-down)
  dealAnimCard(bjPlayerHand, 0, PLAY_CARD_Y, true);  buzzerTone(500, 22); delay(30);
  dealAnimCard(bjCompHand,   0, COMP_CARD_Y, true);  buzzerTone(550, 22); delay(30);
  dealAnimCard(bjPlayerHand, 1, PLAY_CARD_Y, true);  buzzerTone(600, 22); delay(30);
  dealAnimCard(bjCompHand,   1, COMP_CARD_Y, false); buzzerTone(650, 22); delay(40);
  // Final buzzer flourish
  bj_buzzerDeal();

  // Update score labels now we have cards
  int cShow = bj_cardBJValue(bjCompHand[0].value);
  int pTot  = bj_handTotal(bjPlayerHand, bjPlayerCount);
  drawHandLabel(COMP_LBL_Y, "COMPUTER", C_CYAN,    cShow, true);
  drawHandLabel(PLAY_LBL_Y, "PLAYER",   C_LTGREEN, pTot,  false);
  bj_drawBottomBar();
  bj_drawButtonBar();
  bj_showCreditsOnSeg();

  // ── Resolve side bets immediately (based on first 2 cards + dealer up card) ──
  bj_resolveSideBets();
  // Show side bjBet wins if any
  bool anySideBetWon = false;
  for(int i = 0; i < BJ_SB_COUNT; i++) if(bjSideBetWon[i]) { anySideBetWon = true; break; }
  if(anySideBetWon) {
    // Flash a quick side bjBet win message in the mid strip
    tft.fillRect(0, MID_SCORE_Y, 240, MID_SCORE_H, C_WIN_BG);
    tft.setTextSize(1); tft.setTextColor(C_GOLD);
    int msgX = 4;
    tft.setCursor(msgX, MID_SCORE_Y + 1);
    tft.print("SIDE BET WIN! ");
    for(int i = 0; i < BJ_SB_COUNT; i++) {
      if(bjSideBetWon[i]) {
        tft.print(bjSbNames[i]); tft.print("+"); tft.print(bjSideBetPayout[i]); tft.print(" ");
      }
    }
    bj_buzzerWin();
    bj_showCreditsOnSeg();
    delay(1500);
    // Clear mid strip
    tft.fillRect(0, MID_SCORE_Y, 240, MID_SCORE_H, C_BG);
    tft.drawFastHLine(0, MID_SCORE_Y, 240, C_DARKGRAY);
  }

  // ── Check for immediate Blackjacks ──
  bool playerBJ = bj_isBlackjack(bjPlayerHand, bjPlayerCount);
  bool compBJ   = bj_isBlackjack(bjCompHand,   bjCompCount);

  if(playerBJ || compBJ) {
    // Reveal hole card with flip animation
    bj_animateDealCard(cardX(1), COMP_CARD_Y, bjCompHand[1], true);
    delay(300);
    if(playerBJ && compBJ) {
      bjHandResult = BJ_RES_PUSH;
    } else if(playerBJ) {
      bjHandResult = BJ_RES_PLAYER_BJ;
      bjPlayerIsDealer = true;   // player keeps/reclaims dealer role
    } else {
      bjHandResult = BJ_RES_COMP_BJ;
      bjPlayerIsDealer = false;  // computer becomes dealer
    }
    bj_resolveHand();
    return;
  }

  // Normal play — player's turn
  bj_drawBottomBar();
}

void bj_playerHit() {
  if(bjGameState != BJ_STATE_PLAYER_TURN || bjPlayerCount >= MAX_HAND) return;
  bjPlayerHand[bjPlayerCount++] = dealCard();
  bj_buzzerHit();
  int x = cardX(bjPlayerCount - 1);
  bj_animateDealCard(x, PLAY_CARD_Y, bjPlayerHand[bjPlayerCount-1], true);
  int tot = bj_handTotal(bjPlayerHand, bjPlayerCount);
  if(bjIsSplit) {
    char lbl[16];
    sprintf(lbl, "HAND %d of 2", bjSplitHandIdx + 1);
    drawHandLabel(PLAY_LBL_Y, lbl, C_LTGREEN, tot, false);
  } else {
    drawHandLabel(PLAY_LBL_Y, "PLAYER", C_LTGREEN, tot, false);
  }
  bj_drawBottomBar();
  bj_drawButtonBar();
  bj_showCreditsOnSeg();
  if(bj_isBust(bjPlayerHand, bjPlayerCount)) {
    bj_buzzerBust();
    if(bjIsSplit) {
      if(bjSplitHandIdx == 0) {
        delay(300);
        bj_advanceSplitHand();
      } else {
        // Hand 2 busted — check if Hand 1 also busted.
        // If both hands are bust the computer doesn't need to play:
        // the player has already lost everything, skip straight to resolve.
        bool hand1AlsoBust = bj_isBust(bjSplitHand, bjSplitCount);
        delay(300);
        if(hand1AlsoBust) {
          bjHandResult = BJ_RES_PLAYER_BUST;
          bj_resolveHand();
        } else {
          bjHandResult = BJ_RES_PLAYER_BUST;
          bjGameState = BJ_STATE_COMP_TURN;
          bj_runComputerTurn();
        }
      }
    } else {
      bjHandResult = BJ_RES_PLAYER_BUST;
      delay(300);
      bj_resolveHand();
    }
  } else if(bj_handTotal(bjPlayerHand, bjPlayerCount) == 21) {
    // Auto-stand on 21 — let AI play out (adds drama, AI must try to match/beat)
    delay(400);
    bj_playerStand();
  }
}

void bj_playerStand() {
  if(bjGameState != BJ_STATE_PLAYER_TURN) return;
  if(bjIsSplit) {
    bj_advanceSplitHand();
  } else {
    bjGameState = BJ_STATE_COMP_TURN;
    bj_runComputerTurn();
  }
}

void bj_playerDouble() {
  if(bjGameState != BJ_STATE_PLAYER_TURN) return;
  if(bjPlayerCount != 2) return;        // double only on first 2 cards
  if(bjCredits < bjBet)    return;        // need enough bjCredits for second bjBet
  bjCredits     -= bjBet;
  bjLifetimeBet += bjBet;
  bjDoubledDown  = true;
  bj_showCreditsOnSeg();
  bj_drawBottomBar();
  bj_drawButtonBar();

  // Draw one card, then stand
  bjPlayerHand[bjPlayerCount++] = dealCard();
  bj_buzzerHit();
  int x = cardX(bjPlayerCount - 1);
  bj_animateDealCard(x, PLAY_CARD_Y, bjPlayerHand[bjPlayerCount-1], true);
  int tot = bj_handTotal(bjPlayerHand, bjPlayerCount);
  drawHandLabel(PLAY_LBL_Y, "PLAYER", C_LTGREEN, tot, false);
  bj_drawBottomBar();
  delay(400);

  if(bj_isBust(bjPlayerHand, bjPlayerCount)) {
    bjHandResult = BJ_RES_PLAYER_BUST;
    bj_buzzerBust();
    bj_resolveHand();
  } else {
    bjGameState = BJ_STATE_COMP_TURN;
    bj_runComputerTurn();
  }
}

// ── SPLIT ──────────────────────────────────────────────────────────────────────
// Conditions: exactly 2 cards, same BJ value, enough bjCredits for second bjBet
void bj_playerSplit() {
  if(bjGameState != BJ_STATE_PLAYER_TURN) return;
  if(bjPlayerCount != 2) return;
  if(bjIsSplit) return;                  // no re-split
  if(bjCredits < bjBet) return;
  if(bj_cardBJValue(bjPlayerHand[0].value) != bj_cardBJValue(bjPlayerHand[1].value)) return;

  // Deduct second bjBet
  bjCredits    -= bjBet;
  bjLifetimeBet += bjBet;
  bjSplitBet    = bjBet;
  bjIsSplit     = true;
  bjSplitHandIdx = 0;

  // Move bjPlayerHand[1] to bjSplitHand[0]
  bjSplitHand[0] = bjPlayerHand[1];
  bjSplitCount   = 1;
  bjPlayerCount  = 1;  // hand 1 now has just card[0]

  // Deal one new card to each hand
  bjPlayerHand[bjPlayerCount++] = dealCard();
  bjSplitHand[bjSplitCount++]   = dealCard();

  buzzerTone(700, 30); delay(25); buzzerTone(900, 30);
  bj_showCreditsOnSeg();

  // Redraw with hand 1
  tft.fillScreen(C_BG);
  bj_drawHeaderBar();
  tft.drawFastHLine(0, MID_SCORE_Y, 240, C_DARKGRAY);
  drawHandLabel(COMP_LBL_Y, "COMPUTER", C_CYAN, bj_cardBJValue(bjCompHand[0].value), true);
  bj_drawHandRow(bjCompHand, bjCompCount, COMP_CARD_Y, true);
  drawHandLabel(PLAY_LBL_Y, "HAND 1 of 2", C_LTGREEN, bj_handTotal(bjPlayerHand, bjPlayerCount), false);
  bj_drawHandRow(bjPlayerHand, bjPlayerCount, PLAY_CARD_Y, false);
  bj_drawBottomBar();
  bj_drawButtonBar();

  // Player plays Hand 1 (stays in BJ_STATE_PLAYER_TURN)
}

// Called when player stands on a split hand — advances to next hand or comp turn
void bj_advanceSplitHand() {
  if(bjSplitHandIdx == 0) {
    // Switch to hand 2
    bjSplitHandIdx = 1;
    // Swap bjPlayerHand ↔ bjSplitHand so existing hit/stand/bust logic works on "bjPlayerHand"
    // Save hand 1 result
    Card tmp[MAX_HAND]; int tc = bjPlayerCount;
    for(int i = 0; i < bjPlayerCount; i++) tmp[i] = bjPlayerHand[i];

    for(int i = 0; i < bjSplitCount; i++) bjPlayerHand[i] = bjSplitHand[i];
    bjPlayerCount = bjSplitCount;

    for(int i = 0; i < tc; i++) bjSplitHand[i] = tmp[i];
    bjSplitCount = tc;

    buzzerTone(800, 25); delay(20); buzzerTone(1000, 25);

    // Redraw for hand 2
    tft.fillScreen(C_BG);
    bj_drawHeaderBar();
    tft.drawFastHLine(0, MID_SCORE_Y, 240, C_DARKGRAY);
    drawHandLabel(COMP_LBL_Y, "COMPUTER", C_CYAN, bj_cardBJValue(bjCompHand[0].value), true);
    bj_drawHandRow(bjCompHand, bjCompCount, COMP_CARD_Y, true);
    drawHandLabel(PLAY_LBL_Y, "HAND 2 of 2", C_LTGREEN, bj_handTotal(bjPlayerHand, bjPlayerCount), false);
    bj_drawHandRow(bjPlayerHand, bjPlayerCount, PLAY_CARD_Y, false);
    bj_drawBottomBar();
    bj_drawButtonBar();

    // Stay in BJ_STATE_PLAYER_TURN for hand 2
  } else {
    // Both hands played — go to computer turn
    // Before going to comp turn, record hand 2 result temporarily
    // We'll handle split payout in bj_resolveHand
    bjGameState = BJ_STATE_COMP_TURN;
    bj_runComputerTurn();
  }
}

void bj_runComputerTurn() {
  bjGameState = BJ_STATE_COMP_TURN;

  // Reveal hole card
  bj_animateDealCard(cardX(1), COMP_CARD_Y, bjCompHand[1], true);
  delay(400);

  // AI knows the full player total after the player has stood.
  // During a split, target the HIGHEST non-busted hand so the AI tries
  // to beat both hands rather than stopping short at the lower total.
  int playerTotal;
  if(bjIsSplit) {
    int h1 = bj_handTotal(bjSplitHand, bjSplitCount);
    int h2 = bj_handTotal(bjPlayerHand, bjPlayerCount);
    bool h1Bust = bj_isBust(bjSplitHand, bjSplitCount);
    bool h2Bust = bj_isBust(bjPlayerHand, bjPlayerCount);
    if(h1Bust && h2Bust)  playerTotal = 0;   // both bust → AI wins regardless
    else if(h1Bust)       playerTotal = h2;
    else if(h2Bust)       playerTotal = h1;
    else                  playerTotal = max(h1, h2);
  } else {
    playerTotal = bj_handTotal(bjPlayerHand, bjPlayerCount);
  }

  // Computer draws until done
  while(!bj_isBust(bjCompHand, bjCompCount) && bjCompCount < MAX_HAND) {
    int ct = bj_handTotal(bjCompHand, bjCompCount);
    drawHandLabel(COMP_LBL_Y, "COMPUTER", C_CYAN, ct, false);

    // ── Hard stop: never hit on 21 ──
    if(ct == 21) break;

    // ── If AI already beats the player, stop immediately ──
    if(ct > playerTotal) break;

    // ── If AI is strictly LOSING it must keep hitting; on a TIE it stands
    //    (ties are resolved by the dealer rule — no need to hit into a bust) ──
    bool mustHit = (ct < playerTotal);

    // Consult basic-strategy AI only when the outcome is NOT a forced hit
    // (i.e. when the score is tied or AI is ahead — though ahead is caught above)
    bool strategicHit = mustHit ? true : bj_aiShouldHit(bjCompHand, bjCompCount, playerTotal);

    // Track hard-16 decisions for AI learning ONLY when it is a genuine
    // strategic choice (not a forced "must catch up" hit)
    bool hardSixteen = (!bj_isSoft(bjCompHand, bjCompCount) && ct == 16);
    if(hardSixteen && !mustHit) {
      if(strategicHit) bjAiHitOn16   = true;
      else             bjAiStoodOn16 = true;
    }

    bool shouldHit = strategicHit;
    if(!shouldHit) break;

    delay(500); // AI "thinking" pause
    bjCompHand[bjCompCount++] = dealCard();
    bj_buzzerHit();
    int x = cardX(bjCompCount - 1);
    bj_animateDealCard(x, COMP_CARD_Y, bjCompHand[bjCompCount-1], true);
    delay(200);
  }

  int ct = bj_handTotal(bjCompHand, bjCompCount);
  drawHandLabel(COMP_LBL_Y, "COMPUTER", C_CYAN, ct, false);
  delay(300);

  // Determine result — tied score goes to the dealer
  if(bj_isBust(bjCompHand, bjCompCount)) {
    bjHandResult = BJ_RES_COMP_BUST;
  } else {
    int pt = bj_handTotal(bjPlayerHand, bjPlayerCount);
    if(pt > ct)       bjHandResult = BJ_RES_PLAYER_WIN;
    else if(ct > pt)  bjHandResult = BJ_RES_COMP_WIN;
    else {
      // Tie — dealer wins
      if(bjPlayerIsDealer) bjHandResult = BJ_RES_PLAYER_WIN;  // player is dealer → player wins tie
      else               bjHandResult = BJ_RES_COMP_WIN;    // computer is dealer → computer wins tie
    }
  }

  bj_resolveHand();
}

void bj_resolveHand() {
  bool playerWon = (bjHandResult == BJ_RES_PLAYER_BJ ||
                    bjHandResult == BJ_RES_PLAYER_WIN ||
                    bjHandResult == BJ_RES_COMP_BUST);
  bool compWon_b = (bjHandResult == BJ_RES_COMP_BJ ||
                    bjHandResult == BJ_RES_COMP_WIN ||
                    bjHandResult == BJ_RES_PLAYER_BUST);
  bool isPush    = (bjHandResult == BJ_RES_PUSH);

  int activeBet = bjDoubledDown ? bjBet * 2 : bjBet;

  // ── Split payout: evaluate both hands against computer total ──
  if(bjIsSplit) {
    int compTotal   = bj_handTotal(bjCompHand, bjCompCount);
    bool compBust_s = bj_isBust(bjCompHand, bjCompCount);

    // Hand 2 is in bjPlayerHand (current active hand after bj_advanceSplitHand swaps)
    int h2Total = bj_handTotal(bjPlayerHand, bjPlayerCount);
    bool h2Bust = bj_isBust(bjPlayerHand, bjPlayerCount);

    // Hand 1 is now in bjSplitHand
    int h1Total = bj_handTotal(bjSplitHand, bjSplitCount);
    bool h1Bust = bj_isBust(bjSplitHand, bjSplitCount);

    // Evaluate hand 1
    BJHandResult h1Res;
    if(h1Bust)                    h1Res = BJ_RES_PLAYER_BUST;
    else if(compBust_s)           h1Res = BJ_RES_COMP_BUST;
    else if(h1Total > compTotal)  h1Res = BJ_RES_PLAYER_WIN;
    else if(compTotal > h1Total)  h1Res = BJ_RES_COMP_WIN;
    else                          h1Res = bjPlayerIsDealer ? BJ_RES_PLAYER_WIN : BJ_RES_COMP_WIN;

    // Evaluate hand 2
    BJHandResult h2Res;
    if(h2Bust)                    h2Res = BJ_RES_PLAYER_BUST;
    else if(compBust_s)           h2Res = BJ_RES_COMP_BUST;
    else if(h2Total > compTotal)  h2Res = BJ_RES_PLAYER_WIN;
    else if(compTotal > h2Total)  h2Res = BJ_RES_COMP_WIN;
    else                          h2Res = bjPlayerIsDealer ? BJ_RES_PLAYER_WIN : BJ_RES_COMP_WIN;

    // Pay each hand: WIN=1:1 normally, 2:1 if total is 21, PUSH=stake back, LOSS=nothing
    auto payResult = [&](BJHandResult r, int b, Card* hand, int count) -> int {
      if(r == BJ_RES_PLAYER_WIN || r == BJ_RES_COMP_BUST) {
        int mult = (bj_handTotal(hand, count) == 21) ? 3 : 2;  // 21→2:1, else 1:1
        bjCredits += b * mult; bjLifetimeWon += b * mult; return b * (mult - 1);
      }
      else if(r == BJ_RES_PUSH) { bjCredits += b; bjLifetimeWon += b; return 0; }
      return -b;
    };

    int net1 = payResult(h1Res, bjSplitBet, bjSplitHand, bjSplitCount);
    int net2 = payResult(h2Res, bjSplitBet, bjPlayerHand, bjPlayerCount);

    // Store individual hand results for display
    bjSplitResult = h1Res;   // hand 1 result (shown as H1)
    bjHandResult  = h2Res;   // hand 2 result (shown as H2, drives main banner colour)

    // Combined net across both hands — this is what the player actually gained/lost
    int netTotal = net1 + net2;
    bjLastPayout    = netTotal;  // positive = net win, 0 = break even, negative = net loss
    bjLastNetResult = netTotal;  // true signed net for display
    if(netTotal > (int)bjBiggestWin) bjBiggestWin = netTotal;

    // Override the main banner to reflect the OVERALL outcome, not just hand 2
    bool anyWin  = (h1Res == BJ_RES_PLAYER_WIN || h1Res == BJ_RES_COMP_BUST ||
                    h2Res == BJ_RES_PLAYER_WIN || h2Res == BJ_RES_COMP_BUST);
    bool anyLoss = (h1Res == BJ_RES_COMP_WIN   || h1Res == BJ_RES_PLAYER_BUST ||
                    h2Res == BJ_RES_COMP_WIN   || h2Res == BJ_RES_PLAYER_BUST);

    if(anyWin && !anyLoss)  {
      // Both won or one won/one pushed
      bjHandResult = BJ_RES_PLAYER_WIN;
      bjPlayerWins++;
      // Dealer role only changes on natural Blackjack
    } else if(anyLoss && !anyWin) {
      // Both lost
      bjHandResult = BJ_RES_COMP_WIN;
      bjCompWins++;
      // Dealer role only changes on natural Blackjack
    } else if(anyWin && anyLoss) {
      // Split result — one won, one lost — net zero from profit perspective
      bjHandResult = BJ_RES_PUSH;
      bjPushes++;
    } else {
      // Both pushed
      bjHandResult = BJ_RES_PUSH;
      bjPushes++;
    }

    playerWon = anyWin && !anyLoss;
    compWon_b = anyLoss && !anyWin;
    isPush    = !playerWon && !compWon_b;

  } else {
    // Normal (non-split) payout
    if(playerWon) {
      int payout;
      if(bjHandResult == BJ_RES_PLAYER_BJ) {
        // Natural Blackjack: 3:2 — stake back + 1.5x
        payout = activeBet + (activeBet * 3) / 2;
      } else if(bj_handTotal(bjPlayerHand, bjPlayerCount) == 21) {
        // Non-natural 21 (hit to 21): 2:1 — stake back + 2x
        payout = activeBet * 3;
      } else {
        // Normal win: 1:1 — stake back + 1x
        payout = activeBet * 2;
      }
      bjCredits     += payout;
      bjLifetimeWon += payout;
      bjLastPayout   = payout - activeBet;
      bjLastNetResult = payout - activeBet;   // net profit
      int net      = payout - activeBet;
      if(net > bjBiggestWin) bjBiggestWin = net;
      bjPlayerWins++;
      // Dealer role only changes on natural Blackjack (handled in bj_startNewHand)

    } else if(compWon_b) {
      bjLastPayout    = 0;
      bjLastNetResult = -activeBet;   // true loss amount
      bjCompWins++;
      // Dealer role only changes on natural Blackjack (handled in bj_startNewHand)

    } else if(isPush) {
      bjCredits    += activeBet;
      bjLifetimeWon += activeBet;
      bjLastPayout    = 0;
      bjLastNetResult = 0;   // break even
      bjPushes++;
    }
  }

  // AI learns
  bj_aiLearnFromHand(bjAiHitOn16, bjAiStoodOn16, compWon_b);

  bjGameState = BJ_STATE_RESULT;
  bj_saveToEEPROM();
  bj_saveAIToEEPROM();

  bj_drawResult();

  // Sound + flash — for splits, use bjLastPayout (combined net) to decide
  if(playerWon || (bjIsSplit && bjLastPayout > 0)) {
    if(bjHandResult == BJ_RES_PLAYER_BJ) bj_buzzerBlackjack();
    else bj_buzzerWin();
    bj_flashWin(true);
    bj_segCountUp(bjCredits - bjLastPayout, bjCredits, 600);
  } else if(compWon_b || (bjIsSplit && bjLastPayout < 0)) {
    if(bjHandResult == BJ_RES_COMP_BJ) {
      buzzerTone(523, 80); delay(15); buzzerTone(392, 80); delay(15); buzzerTone(330, 200);
    } else {
      bj_buzzerBust();
    }
    bj_flashWin(false);
  } else {
    bj_buzzerPush();
  }

  bj_showCreditsOnSeg();

  // ── Auto-advance to betting after 1 second ──
  // Player can press DEAL or ENTER early to skip the wait
  unsigned long waitStart = millis();
  while(millis() - waitStart < 1000) {
    if(digitalRead(rollButton)  == LOW) break;
    if(digitalRead(enterButton) == LOW) break;
    delay(30);
  }
  // Small debounce pause
  delay(80);
  bj_goToBetting();
}

void bj_goToBetting() {
  if(bjCredits <= 0) {
    bjGameState = BJ_STATE_GAMEOVER;
    bj_buzzerGameOver();
    bj_drawScreen();
    return;
  }
  bjBet = constrain(bjBet, 10, min(500, bjCredits));
  // Snap to nearest 10
  bjBet = (bjBet / 10) * 10;
  if(bjBet < 10) bjBet = 10;
  bjDoubledDown = false;
  bjGameState   = BJ_STATE_BETTING;
  bj_drawScreen();
}

// ============================================================================
// SETUP
// ============================================================================

// setup() removed — hardware owned by Yahtzee

// loop() replaced by runBlackjack() entry point below
// ============================================================================
// BLACKJACK ENTRY POINT — called from Yahtzee Tools menu
// Hold ROLL button 3 seconds from BJ menu to return to Yahtzee.
// ============================================================================

void runBlackjack() {
  bj_loadFromEEPROM();
  bj_loadAIFromEEPROM();
  bj_shuffleDeck();   // initialise deck so cards are not all Aces
  bjGameState = BJ_STATE_MENU;
  tft.fillScreen(0x0000);
  bj_drawScreen();

  int lDeal=HIGH, lUp=HIGH, lDown=HIGH, lEnter=HIGH;
  int lAction[5]={HIGH,HIGH,HIGH,HIGH,HIGH};
  unsigned long upHold=0, upRpt=0, dnHold=0, dnRpt=0;
  unsigned long rollHoldStart=0;
  bool rollLongDone=false;

  while(true) {
    int dealBtn  = digitalRead(rollButton);  // ROLL = DEAL
    int upBtn    = digitalRead(upButton);
    int downBtn  = digitalRead(downButton);
    int enterBtn = digitalRead(enterButton);
    int actBtn[5];
    for(int i=0;i<5;i++) actBtn[i]=digitalRead(holdButtons[i]);

    // ── EXIT: hold ROLL 3s from any state → return to Tools menu ──
    if(dealBtn==LOW) {
      if(rollHoldStart==0) rollHoldStart=millis();
      if(!rollLongDone && millis()-rollHoldStart>3000) {
        rollLongDone=true;
        buzzerTone(400,80); delay(80); buzzerTone(400,80);
        bj_saveToEEPROM();
        bj_saveAIToEEPROM();
        rollHoldStart=0; rollLongDone=false;
        tft.fillScreen(COLOR_BG);
        inToolsMenu=true;
        drawScreen();  // Yahtzee's drawScreen
        return;
      }
    } else { rollHoldStart=0; rollLongDone=false; }

    // ── DEAL button (ROLL) short-press ───────────────────────────
    // Only fires on the leading edge AND only if hold time < 2s (not an exit gesture)
    if(lDeal==HIGH && dealBtn==LOW) {
      delay(30);
      // Don't act on short-press if we are building toward a long-press exit
      // (rollHoldStart will be set; we check again after the delay)
      unsigned long heldFor = (rollHoldStart>0) ? millis()-rollHoldStart : 0;
      if(heldFor < 3000) {
        switch(bjGameState) {
          case BJ_STATE_MENU:
            bj_buzzerMenu(); bj_goToBetting(); break;
          case BJ_STATE_BETTING:
            if(bjCredits>=bjBet) bj_startNewHand(); else buzzerTone(180,120); break;
          case BJ_STATE_PLAYER_TURN:
            bj_playerHit(); break;
          case BJ_STATE_RESULT:
            bj_goToBetting(); break;
          case BJ_STATE_GAMEOVER:
            { if(bjCredits<=0) { bjCredits=200; bj_saveToEEPROM(); }
              bjGameState=BJ_STATE_MENU; bj_drawScreen(); } break;
          default: break;
        }
      }
    }
    lDeal=dealBtn;

    // ── UP button ────────────────────────────────────────────────
    {
      bool upP=(upBtn==LOW);
      unsigned long now=millis();
      if(bjGameState==BJ_STATE_BETTING) {
        if(upP){
          bool go=false;
          if(lUp==HIGH){delay(30);go=true;upHold=now;upRpt=now;}
          else{unsigned long h=now-upHold;unsigned long iv=(h>2000)?30:(h>800)?80:(h>400)?200:9999;if(now-upRpt>=iv){go=true;upRpt=now;}}
          if(go){int mx=min(bjCredits,500);if(bjBet<mx){bjBet+=5;if(bjBet>mx)bjBet=mx;bj_buzzerMenu();bj_drawScreen();}}
        } else { upHold=0; }
      } else if(bjGameState==BJ_STATE_MENU||bjGameState==BJ_STATE_GAMEOVER) {
        if(upP){
          bool go=false;
          if(lUp==HIGH){delay(30);go=true;upHold=now;upRpt=now;}
          else{unsigned long h=now-upHold;unsigned long iv=(h>2000)?60:(h>800)?150:9999;if(now-upRpt>=iv){go=true;upRpt=now;}}
          if(go&&bjCredits<32767){bjCredits=min(bjCredits+100,32767);bj_saveToEEPROM();buzzerTone(1400,30);bj_showCreditsOnSeg();bj_drawScreen();}
        } else { upHold=0; }
      } else if(bjGameState==BJ_STATE_SIDE_BETS) {
        if(lUp==HIGH&&upP){delay(30);
          if(!bjSbAdjusting){bjSbSelected=(bjSbSelected+BJ_SB_COUNT-1)%BJ_SB_COUNT;bj_buzzerMenu();bj_drawScreen();}
          else{if(bjSideBetAmount[bjSbSelected]<bjCredits)bjSideBetAmount[bjSbSelected]+=5;bj_buzzerMenu();bj_drawScreen();}
        }
      }
    }
    lUp=upBtn;

    // ── DOWN button ──────────────────────────────────────────────
    {
      bool dnP=(downBtn==LOW);
      unsigned long now=millis();
      if(bjGameState==BJ_STATE_BETTING) {
        if(dnP){
          bool go=false;
          if(lDown==HIGH){delay(30);go=true;dnHold=now;dnRpt=now;}
          else{unsigned long h=now-dnHold;unsigned long iv=(h>2000)?30:(h>800)?80:(h>400)?200:9999;if(now-dnRpt>=iv){go=true;dnRpt=now;}}
          if(go&&bjBet>5){bjBet-=5;bj_buzzerMenu();bj_drawScreen();}
        } else { dnHold=0; }
      } else if(bjGameState==BJ_STATE_SIDE_BETS) {
        if(lDown==HIGH&&dnP){delay(30);
          if(!bjSbAdjusting){bjSbSelected=(bjSbSelected+1)%BJ_SB_COUNT;bj_buzzerMenu();bj_drawScreen();}
          else{if(bjSideBetAmount[bjSbSelected]>=5)bjSideBetAmount[bjSbSelected]-=5;bj_buzzerMenu();bj_drawScreen();}
        }
      } else if(bjGameState==BJ_STATE_MENU||bjGameState==BJ_STATE_GAMEOVER) {
        if(lDown==HIGH&&dnP&&bjCredits>0){delay(30);bjCredits=max(bjCredits-100,0);bj_saveToEEPROM();bj_buzzerMenu();bj_drawScreen();bj_showCreditsOnSeg();}
      }
    }
    lDown=downBtn;

    // Static vars for ENTER long-press — declared here so they are in scope
    // for both the short-press guard below and the long-press block further down
    static unsigned long enterMenuHold=0; static bool enterMenuLong=false;

    // ── ENTER button short-press ─────────────────────────────────
    // Guard: don't fire short-press if a 3s stats-reset hold is in progress
    if(lEnter==HIGH && enterBtn==LOW) {
      delay(30);
      unsigned long enterHeldFor = (enterMenuHold>0) ? millis()-enterMenuHold : 0;
      if(enterHeldFor < 3000) {
        switch(bjGameState) {
          case BJ_STATE_MENU:
            // Short-press ENTER does nothing on menu — use DEAL to start
            // ENTER hold 3s = reset stats  |  DEAL hold 3s = exit to tools
            break;
          case BJ_STATE_BETTING:
            {int mx=min(bjCredits,500);if(bjBet!=mx){bjBet=mx;bj_buzzerMenu();bj_drawScreen();}else bj_startNewHand();}
            break;
          case BJ_STATE_SIDE_BETS:
            if(!bjSbAdjusting){bjSbAdjusting=true;bj_buzzerMenu();bj_drawScreen();}
            else{bjSbAdjusting=false;buzzerTone(1000,30);bj_drawScreen();}
            break;
          case BJ_STATE_PLAYER_TURN:
            bj_playerStand(); break;
          case BJ_STATE_RESULT:
            bj_goToBetting(); break;
          case BJ_STATE_GAMEOVER:
            { if(bjCredits<=0) { bjCredits=200; bj_saveToEEPROM(); }
              bjGameState=BJ_STATE_MENU; bj_drawScreen(); } break;
          default: break;
        }
      }
    }
    lEnter=enterBtn;

    // ── ACTION buttons (holdButtons) ─────────────────────────────
    // holdButtons[] = {GP29, GP28, GP3, GP1, GP0}  (indices 0..4)
    // BTN_HIT_IDX=4     → GP0  leftmost
    // BTN_STAND_IDX=3   → GP1  2nd left
    // BTN_DOUBLE_IDX=2  → GP3  middle
    // BTN_SPLIT_IDX=1   → GP28 4th left
    // BTN_SIDEBET_IDX=0 → GP29 rightmost
    for(int i=0;i<5;i++) {
      if(lAction[i]==HIGH && actBtn[i]==LOW) {
        delay(30);
        if(bjGameState==BJ_STATE_PLAYER_TURN) {
          if(i==BTN_HIT_IDX)    bj_playerHit();
          else if(i==BTN_STAND_IDX)   bj_playerStand();
          else if(i==BTN_DOUBLE_IDX)  bj_playerDouble();
          else if(i==BTN_SPLIT_IDX)   bj_playerSplit();
        } else if(bjGameState==BJ_STATE_BETTING) {
          if(i==BTN_SIDEBET_IDX) bj_openSideBets();
        } else if(bjGameState==BJ_STATE_SIDE_BETS) {
          if(i==BTN_SIDEBET_IDX) bj_closeSideBets();
          else if(i==BTN_DOUBLE_IDX){if(!bjSbAdjusting&&bjSideBetAmount[bjSbSelected]<bjCredits)bjSideBetAmount[bjSbSelected]+=5;bj_buzzerMenu();bj_drawScreen();}
        }
      }
      lAction[i]=actBtn[i];
    }

    // ── Long-press ENTER 3s on menu or gameover to reset stats ───
    if(bjGameState==BJ_STATE_MENU || bjGameState==BJ_STATE_GAMEOVER) {
      if(enterBtn==LOW){
        if(enterMenuHold==0) enterMenuHold=millis();
        if(!enterMenuLong&&millis()-enterMenuHold>3000){
          bjHandsPlayed=0;bjPlayerWins=0;bjCompWins=0;bjPushes=0;
          bjLifetimeWon=0;bjLifetimeBet=0;bjBiggestWin=0;
          aiData.wins=0;aiData.games=0;
          aiData.hit16wins=0;aiData.hit16total=0;
          aiData.stand16wins=0;aiData.stand16total=0;
          aiData.softAdj=128;  // reset to neutral
          bj_saveToEEPROM();
          bj_saveAIToEEPROM();
          enterMenuLong=true;
          buzzerTone(300,80);delay(100);buzzerTone(300,80);
          bj_drawScreen();
        }
      } else { enterMenuHold=0; enterMenuLong=false; }
    } else { enterMenuHold=0; enterMenuLong=false; }

    delay(8);
  }
}
