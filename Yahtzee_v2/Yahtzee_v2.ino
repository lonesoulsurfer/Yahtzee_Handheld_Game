/*
 * Gameplay:
 * - 2 players alternate turns
 * - Each turn: Roll dice up to 3 times
 * - Hold dice between rolls with buttons 1-5
 * - After rolls, select scoring category
 * - Game ends after 26 turns (13 categories × 2 players)
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <EEPROM.h>
#include "vtable.h"   // Optimal Yahtzee value table (option B). Must be in the sketch folder.

// ============================================================================
// BLACKJACK TYPES — defined here so Arduino prototype-generator sees them
// ============================================================================

struct Card {
  uint8_t value;  // 1=Ace 2-10 11=J 12=Q 13=K
  uint8_t suit;   // 0=Spades 1=Hearts 2=Diamonds 3=Clubs
};

const char* bjValueNames[] = {"A","2","3","4","5","6","7","8","9","10","J","Q","K"};

#define BJ_MAX_CARDS 11   // max cards in one BJ hand (same as original MAX_HAND)
#define MAX_HAND BJ_MAX_CARDS  // keep original name working in BJ file

int  bjDeckIndices[52];
int  bjDeckTop = 0;

Card bjPlayerHand[BJ_MAX_CARDS];
Card bjCompHand[BJ_MAX_CARDS];
Card bjSplitHand[BJ_MAX_CARDS];
int  bjPlayerCount = 0;
int  bjCompCount   = 0;
int  bjSplitCount  = 0;

enum BJGameState {
  BJ_STATE_MENU,
  BJ_STATE_BETTING,
  BJ_STATE_SIDEBET,
  BJ_STATE_PLAYER_TURN,
  BJ_STATE_COMP_TURN,
  BJ_STATE_RESULT,
  BJ_STATE_GAMEOVER
};

// Alias for side-bet screen (BJ file may reference BJ_STATE_SIDE_BETS)
#define BJ_STATE_SIDE_BETS BJ_STATE_SIDEBET

enum BJHandResult {
  BJ_RES_NONE,
  BJ_RES_PLAYER_BJ,
  BJ_RES_COMP_BJ,
  BJ_RES_PLAYER_WIN,
  BJ_RES_COMP_WIN,
  BJ_RES_PUSH,
  BJ_RES_PLAYER_BUST,
  BJ_RES_COMP_BUST
};

#define BJ_SB_COUNT 3

// ── EXACT EV LOOKUP TABLE (precomputed, 9.6KB flash) ─────────────────
// Auto-generated exact EV lookup table
// 252 dice states × 13 categories × 3 roll counts
// Values are EV × 2 (uint8, precision 0.5pts, max 50pts)
// State index: use lookupStateIndex(sorted_dice)

// Sorted dice states (for index lookup)
const uint8_t EV_STATES[252][5] = {
  {1,1,1,1,1},
  {1,1,1,1,2},
  {1,1,1,1,3},
  {1,1,1,1,4},
  {1,1,1,1,5},
  {1,1,1,1,6},
  {1,1,1,2,2},
  {1,1,1,2,3},
  {1,1,1,2,4},
  {1,1,1,2,5},
  {1,1,1,2,6},
  {1,1,1,3,3},
  {1,1,1,3,4},
  {1,1,1,3,5},
  {1,1,1,3,6},
  {1,1,1,4,4},
  {1,1,1,4,5},
  {1,1,1,4,6},
  {1,1,1,5,5},
  {1,1,1,5,6},
  {1,1,1,6,6},
  {1,1,2,2,2},
  {1,1,2,2,3},
  {1,1,2,2,4},
  {1,1,2,2,5},
  {1,1,2,2,6},
  {1,1,2,3,3},
  {1,1,2,3,4},
  {1,1,2,3,5},
  {1,1,2,3,6},
  {1,1,2,4,4},
  {1,1,2,4,5},
  {1,1,2,4,6},
  {1,1,2,5,5},
  {1,1,2,5,6},
  {1,1,2,6,6},
  {1,1,3,3,3},
  {1,1,3,3,4},
  {1,1,3,3,5},
  {1,1,3,3,6},
  {1,1,3,4,4},
  {1,1,3,4,5},
  {1,1,3,4,6},
  {1,1,3,5,5},
  {1,1,3,5,6},
  {1,1,3,6,6},
  {1,1,4,4,4},
  {1,1,4,4,5},
  {1,1,4,4,6},
  {1,1,4,5,5},
  {1,1,4,5,6},
  {1,1,4,6,6},
  {1,1,5,5,5},
  {1,1,5,5,6},
  {1,1,5,6,6},
  {1,1,6,6,6},
  {1,2,2,2,2},
  {1,2,2,2,3},
  {1,2,2,2,4},
  {1,2,2,2,5},
  {1,2,2,2,6},
  {1,2,2,3,3},
  {1,2,2,3,4},
  {1,2,2,3,5},
  {1,2,2,3,6},
  {1,2,2,4,4},
  {1,2,2,4,5},
  {1,2,2,4,6},
  {1,2,2,5,5},
  {1,2,2,5,6},
  {1,2,2,6,6},
  {1,2,3,3,3},
  {1,2,3,3,4},
  {1,2,3,3,5},
  {1,2,3,3,6},
  {1,2,3,4,4},
  {1,2,3,4,5},
  {1,2,3,4,6},
  {1,2,3,5,5},
  {1,2,3,5,6},
  {1,2,3,6,6},
  {1,2,4,4,4},
  {1,2,4,4,5},
  {1,2,4,4,6},
  {1,2,4,5,5},
  {1,2,4,5,6},
  {1,2,4,6,6},
  {1,2,5,5,5},
  {1,2,5,5,6},
  {1,2,5,6,6},
  {1,2,6,6,6},
  {1,3,3,3,3},
  {1,3,3,3,4},
  {1,3,3,3,5},
  {1,3,3,3,6},
  {1,3,3,4,4},
  {1,3,3,4,5},
  {1,3,3,4,6},
  {1,3,3,5,5},
  {1,3,3,5,6},
  {1,3,3,6,6},
  {1,3,4,4,4},
  {1,3,4,4,5},
  {1,3,4,4,6},
  {1,3,4,5,5},
  {1,3,4,5,6},
  {1,3,4,6,6},
  {1,3,5,5,5},
  {1,3,5,5,6},
  {1,3,5,6,6},
  {1,3,6,6,6},
  {1,4,4,4,4},
  {1,4,4,4,5},
  {1,4,4,4,6},
  {1,4,4,5,5},
  {1,4,4,5,6},
  {1,4,4,6,6},
  {1,4,5,5,5},
  {1,4,5,5,6},
  {1,4,5,6,6},
  {1,4,6,6,6},
  {1,5,5,5,5},
  {1,5,5,5,6},
  {1,5,5,6,6},
  {1,5,6,6,6},
  {1,6,6,6,6},
  {2,2,2,2,2},
  {2,2,2,2,3},
  {2,2,2,2,4},
  {2,2,2,2,5},
  {2,2,2,2,6},
  {2,2,2,3,3},
  {2,2,2,3,4},
  {2,2,2,3,5},
  {2,2,2,3,6},
  {2,2,2,4,4},
  {2,2,2,4,5},
  {2,2,2,4,6},
  {2,2,2,5,5},
  {2,2,2,5,6},
  {2,2,2,6,6},
  {2,2,3,3,3},
  {2,2,3,3,4},
  {2,2,3,3,5},
  {2,2,3,3,6},
  {2,2,3,4,4},
  {2,2,3,4,5},
  {2,2,3,4,6},
  {2,2,3,5,5},
  {2,2,3,5,6},
  {2,2,3,6,6},
  {2,2,4,4,4},
  {2,2,4,4,5},
  {2,2,4,4,6},
  {2,2,4,5,5},
  {2,2,4,5,6},
  {2,2,4,6,6},
  {2,2,5,5,5},
  {2,2,5,5,6},
  {2,2,5,6,6},
  {2,2,6,6,6},
  {2,3,3,3,3},
  {2,3,3,3,4},
  {2,3,3,3,5},
  {2,3,3,3,6},
  {2,3,3,4,4},
  {2,3,3,4,5},
  {2,3,3,4,6},
  {2,3,3,5,5},
  {2,3,3,5,6},
  {2,3,3,6,6},
  {2,3,4,4,4},
  {2,3,4,4,5},
  {2,3,4,4,6},
  {2,3,4,5,5},
  {2,3,4,5,6},
  {2,3,4,6,6},
  {2,3,5,5,5},
  {2,3,5,5,6},
  {2,3,5,6,6},
  {2,3,6,6,6},
  {2,4,4,4,4},
  {2,4,4,4,5},
  {2,4,4,4,6},
  {2,4,4,5,5},
  {2,4,4,5,6},
  {2,4,4,6,6},
  {2,4,5,5,5},
  {2,4,5,5,6},
  {2,4,5,6,6},
  {2,4,6,6,6},
  {2,5,5,5,5},
  {2,5,5,5,6},
  {2,5,5,6,6},
  {2,5,6,6,6},
  {2,6,6,6,6},
  {3,3,3,3,3},
  {3,3,3,3,4},
  {3,3,3,3,5},
  {3,3,3,3,6},
  {3,3,3,4,4},
  {3,3,3,4,5},
  {3,3,3,4,6},
  {3,3,3,5,5},
  {3,3,3,5,6},
  {3,3,3,6,6},
  {3,3,4,4,4},
  {3,3,4,4,5},
  {3,3,4,4,6},
  {3,3,4,5,5},
  {3,3,4,5,6},
  {3,3,4,6,6},
  {3,3,5,5,5},
  {3,3,5,5,6},
  {3,3,5,6,6},
  {3,3,6,6,6},
  {3,4,4,4,4},
  {3,4,4,4,5},
  {3,4,4,4,6},
  {3,4,4,5,5},
  {3,4,4,5,6},
  {3,4,4,6,6},
  {3,4,5,5,5},
  {3,4,5,5,6},
  {3,4,5,6,6},
  {3,4,6,6,6},
  {3,5,5,5,5},
  {3,5,5,5,6},
  {3,5,5,6,6},
  {3,5,6,6,6},
  {3,6,6,6,6},
  {4,4,4,4,4},
  {4,4,4,4,5},
  {4,4,4,4,6},
  {4,4,4,5,5},
  {4,4,4,5,6},
  {4,4,4,6,6},
  {4,4,5,5,5},
  {4,4,5,5,6},
  {4,4,5,6,6},
  {4,4,6,6,6},
  {4,5,5,5,5},
  {4,5,5,5,6},
  {4,5,5,6,6},
  {4,5,6,6,6},
  {4,6,6,6,6},
  {5,5,5,5,5},
  {5,5,5,5,6},
  {5,5,5,6,6},
  {5,5,6,6,6},
  {5,6,6,6,6},
  {6,6,6,6,6},
};

// EV table: [state_index][category][rolls_remaining]
// Category: 0-5=upper, 6=3K, 7=4K, 8=FH, 9=SmSt, 10=LgSt, 11=Ytz, 12=Chance
// Rolls: 0=must score now, 1=one roll left, 2=two rolls left
// ─────────────────────────────────────────────────────────────────────


BJGameState  bjGameState  = BJ_STATE_MENU;
BJHandResult bjHandResult = BJ_RES_NONE;

int      bjCredits       = 200;
int      bjBet           = 10;
uint32_t bjHandsPlayed   = 0;
int      bjPlayerWins    = 0;
int      bjCompWins      = 0;
int      bjPushes        = 0;
uint32_t bjLifetimeWon   = 0;
uint32_t bjLifetimeBet   = 0;
bool     bjPlayerIsDealer = true;
int      bjLastPayout    = 0;
int      bjLastNetResult = 0;
int      bjBiggestWin    = 0;
bool     bjDoubledDown   = false;
bool     bjIsSplit       = false;
int      bjSplitHandIdx  = 0;
int      bjSplitBet      = 0;
BJHandResult bjSplitResult = BJ_RES_NONE;

int  bjSideBetAmount[BJ_SB_COUNT] = {0, 0, 0};
int  bjSideBetPayout[BJ_SB_COUNT] = {0, 0, 0};
bool bjSideBetWon[BJ_SB_COUNT]    = {false, false, false};
int  bjSbSelected  = 0;
bool bjSbAdjusting = false;
BJGameState bjSbReturnState = BJ_STATE_BETTING;
const char* bjSbNames[BJ_SB_COUNT] = { "Perfect Pairs", "21 + 3", "Lucky Lucky" };

// REAL BJAILearning struct (fields match Blackjack_V1.ino exactly)
struct BJAILearning {
  uint16_t wins;
  uint16_t games;
  uint16_t hit16wins;
  uint16_t hit16total;
  uint16_t stand16wins;
  uint16_t stand16total;
  uint8_t  softAdj;
} aiData;


// ============================================================================
// PIN DEFINITIONS
// ============================================================================

// ST7789 TFT Display (Hardware SPI1)
#define TFT_CS     13  // GP13
#define TFT_RST    14  // GP14 (RES on display)
#define TFT_DC     15  // GP15
// Hardware SPI1 uses:
// MOSI (SDA) - GP11 (SPI1 TX) - already correct!
// SCLK (SCL) - GP10 (SPI1 SCK) - already correct!

// MAX7219 5-digit 7-segment display
#define DIN_PIN 8   // GP8 (swapped)
#define CLK_PIN 9   // GP9 (swapped)
#define CS_PIN  7   // GP7

// Buzzer
#define BUZZER_PIN 6   // GP6

// Button pins
const int holdButtons[] = {29, 28, 3, 1, 0};  // Hold dice 1-5 (reversed for 5-digit display)
const int rollButton = 5;        // GP5
const int upButton = 2;          // GP2
const int downButton = 27;       // GP27
const int enterButton = 12;      // GP12

#define EEPROM_BRIGHTNESS_ADDR 0  // EEPROM address for brightness storage
#define EEPROM_SOUND_ADDR 1       // EEPROM address for sound on/off
#define EEPROM_VOLUME_ADDR 11     // EEPROM address for volume (1 byte: 1-3)
#define EEPROM_HIGH_SCORE_ADDR 2  // EEPROM address for high score (2 bytes)
#define EEPROM_P1_WINS_ADDR 4     // EEPROM address for P1 wins (2 bytes)
#define EEPROM_P2_WINS_ADDR 6     // EEPROM address for P2 wins (2 bytes)
#define EEPROM_TIES_ADDR 8        // EEPROM address for ties (2 bytes)
#define EEPROM_MOST_YAHTZEES_ADDR 10  // EEPROM address for most Yahtzees in one game (1 byte)
#define EEPROM_AI_SPEED_ADDR 12       // EEPROM address for AI speed (1 byte: 0-3)
#define EEPROM_TFT_BRIGHTNESS_ADDR 13 // EEPROM address for TFT brightness (1 byte: 1-10)
#define EEPROM_AUTO_ADVANCE_ADDR 14   // EEPROM address for auto-advance (1 byte: 0/1)
#define EEPROM_AI_DIFFICULTY_ADDR 15  // EEPROM address for AI difficulty (1 byte: 0=Normal, 1=Hard, 2=God Mode)
#define EEPROM_AI_LEARNING_START 1000   // EEPROM address for AI learning data (moved far from other data)
#define EEPROM_STRATEGY_VARIANTS_ADDR 2048  // EEPROM address for 4 strategy variants (moved to 2048 to give AI data 1024 bytes)
#define EEPROM_AGGR_WEIGHT_ADDR 2112        // Per-difficulty aggressiveness weights: 3 bytes (Normal=0, Hard=1, God=2)
                                            // Stored as uint8_t * 100 (e.g. 0.65 → 65)

// ============================================================================
// DISPLAY INITIALIZATION
// ============================================================================

// Hardware SPI1 for TFT (much faster!)
Adafruit_ST7789 tft = Adafruit_ST7789(&SPI1, TFT_CS, TFT_DC, TFT_RST);

// ============================================================================
// GAME STATE VARIABLES
// ============================================================================

int dice[5];              // Current dice values (1-6)
bool held[5];             // Which dice are held
int currentPlayer = 1;    // Current player (1 or 2)
int rollsLeft = 3;        // Rolls remaining this turn
int rollsUsedThisTurn = 0;  // Track actual rolls used (1, 2, or 3)
int turn = 1;             // Current turn number (1-26)
int playerTurn = 1;       // Current player's turn number (1-13)
int scores1[13];          // Player 1 scores (-1 = unused)
int scores2[13];          // Player 2 scores (-1 = unused)
int yahtzeeBonus1 = 0;    // Player 1 Yahtzee bonus count
int yahtzeeBonus2 = 0;    // Player 2 Yahtzee bonus count
bool gameOver = false;    // Game finished flag
// Per-game roll count for AI: tracks how many times each roll-count (1/2/3)
// was used THIS game so rollCountWins credits the correct distribution.
uint16_t aiRollCountThisGame[4] = {0,0,0,0};  // indices 1-3 used


// UI state
enum GameState {
  STATE_INTRO,            // Animated intro screen (NEW!)
  STATE_MENU,             // Main menu (NEW!)
  STATE_START,            // Waiting to start turn
  STATE_ROLLING,          // Rolling dice phase
  STATE_CATEGORY_SELECT,  // Selecting score category
  STATE_GAME_OVER         // Game finished
};
GameState gameState = STATE_START;

// Menu state
int selectedMenuItem = 0;  // 0 = 2 Player, 1 = vs Computer, 2 = Tools
bool vsComputer = false;   // Track if playing against computer
bool inToolsMenu = false;  // Track if we're in the tools submenu
bool inSoundMenu = false;  // Track if we're in the sound submenu
bool inHelpMenu = false;   // Track if we're in the help/rules screen
bool inGraphMenu = false;  // Track if we're in the AI graphs screen
bool inAIDifficultyMenu = false;  // Track if we're in the AI difficulty selection menu
int selectedAIDifficulty = 1;  // Selected difficulty in the menu (0=Normal, 1=Hard, 2=God)
int helpPage = 0;          // Help screen page (0-2)
int toolsPage = 0;         // Tools menu page (0 = page 1, 1 = page 2)
int selectedToolItem = 0;  // 0=7seg, 1=sound, 2=auto-advance, 3=stats, 4=AI stats, 5=AI speed, 6=export, 8=graphs, 9=help, 10=back
int selectedSoundItem = 0; // 0 = Sound On/Off, 1 = Volume, 2 = Back
int aiStatsPage = 0;  // single AI Stats page
int graphPage = 0;    // Graph page: 0-5 for 6 enhanced analysis graphs
int max7219Brightness = 8;  // Brightness level (0-15, loaded from EEPROM in setup)
bool adjustingBrightness = false;  // Track if actively adjusting brightness
int tftBrightness = 10;     // TFT brightness level (1-10, loaded from EEPROM in setup)
bool adjustingTftBrightness = false;  // Track if actively adjusting TFT brightness
bool soundEnabled = true;  // Sound on/off (loaded from EEPROM in setup)
int volume = 2;  // Volume level (1-3, loaded from EEPROM in setup)
bool adjustingVolume = false;  // Track if actively adjusting volume
bool autoAdvance = false;  // Auto-advance to next turn after scoring (loaded from EEPROM in setup)

// Game over animation state
#define MAX_CONFETTI 40
struct Confetti {
  float x;
  float y;
  float speedY;
  float speedX;
  uint16_t color;
  bool active;
};
Confetti confetti[MAX_CONFETTI];
unsigned long gameOverStartTime = 0;
bool gameOverAnimationStarted = false;
float winnerTextScale = 0.0;  // For dramatic entrance
uint16_t winnerColorHue = 0;  // For color cycling

// Statistics variables
int highScore = 0;        // Highest score ever
int p1Wins = 0;           // Player 1 total wins
int p2Wins = 0;           // Player 2 total wins
int totalTies = 0;        // Total tied games
int mostYahtzees = 0;     // Most Yahtzees in one game
int currentGameYahtzees1 = 0;  // Yahtzees this game for P1
int currentGameYahtzees2 = 0;  // Yahtzees this game for P2
int aiDifficulty = 1;  // AI difficulty: 0 = Normal, 1 = Hard, 2 = God Mode
int aiSpeed = 2;  // AI speed: 0=Slow (5s), 1=Medium (3.5s), 2=Fast (2.5s), 3=Instant (0.5s)

// Per-game tracking for new decision-quality stats
int  aiChanceBucket           = -1;   // Which turn-bucket Chance was scored in (-1=not yet)
int  aiChanceVsUpperLastChoice = -1;  // 0=chose upper, 1=chose Chance, -1=no decision yet
bool aiSacrificedSlot[6]  = {false,false,false,false,false,false}; // Which upper slots were sacrificed
bool aiAheadAtTurn10      = false; // Was AI ahead at turn 10?
bool aiTurn10Recorded     = false; // Have we recorded turn-10 state this game?

// ============================================================================
// AI LEARNING SYSTEM
// ============================================================================

// AI Learning data structure - stores performance metrics in EEPROM
// Strategy variant structure - MUST be defined before AILearningData

// Analyze straight potential and return hold strategy


// Decision outcome tracking
struct DecisionOutcome {
  uint8_t turnNumber;        // Which turn (1-13)
  uint8_t rollNumber;        // Which roll (1-3)
  uint8_t diceHeld;          // How many dice held (0-5)
  uint8_t categoryScored;    // Which category was scored (0-12)
  uint8_t pointsScored;      // How many points
  bool wonGame;              // Did AI win this game?
  bool leadingWhenDecided;   // Was AI leading when decision made?
};

// Track last 100 decisions for pattern analysis
#define MAX_DECISION_HISTORY 100

struct __attribute__((packed)) AILearningData {
  uint16_t gamesPlayed;           // Total games the AI has played
  uint16_t aggressiveWins;        // Wins when playing aggressively
  uint16_t conservativeWins;      // Wins when playing conservatively
  
  // Enhanced learning metrics
  uint16_t totalSelfPlayGames;    // Total self-play training games
  
  // **NEW: Priority 1 - Category Usage Patterns**
  uint16_t categoryScoredCount[13];     // How many times each category was scored (vs human only)
  uint16_t categoryWinsWhenScored[13];  // How many times AI won when scoring each category (vs human only)
  
  // **NEW: Priority 2 - Upper Bonus Granularity**
  
  // **NEW: Priority 3 - Turn-Phase Performance**
  
  // **NEW: Risk/Reward Tracking**
  uint16_t highRiskAttempts;       // Times AI went for low-probability high-value plays
  uint16_t highRiskSuccesses;      // Times high-risk move paid off
  uint16_t highRiskWins;           // Games won after successful high-risk move
  
  // NEW: Strategy weight learning (stored as uint8_t * 10 for precision)
  uint8_t weightYahtzee;          // Yahtzee priority weight * 10 (default 20 = 2.0x)
  uint8_t weightUpperBonus;       // Upper bonus pursuit weight * 10 (default 50 = 5.0x)
  
  // Performance tracking for weight adjustment
  
  // ═══════════════════════════════════════════════════════════════
  // **ENHANCED AI LEARNING - NEW METRICS**
  // ═══════════════════════════════════════════════════════════════
  
  // Roll timing optimization
  uint16_t firstRollScores[13];    // Total points scored on 1st roll per category
  uint16_t secondRollScores[13];   // Total points scored on 2nd roll per category
  uint16_t thirdRollScores[13];    // Total points scored on 3rd roll per category
  uint16_t rollCountUsed[4];       // How often AI used 1, 2, or 3 rolls (index 1-3)
  
  // Score distribution per category (track if AI is scoring optimally)
  uint8_t categoryAvgScore[13];    // Average score per category / 2 (0-127 range)
  uint16_t categoryScoreSum[13];   // Sum of all scores per category (for avg calc)
  
  // Hold pattern effectiveness (which dice counts work best)
  uint16_t holdPattern0Dice;       // Times held 0 dice (reroll all)
  uint16_t holdPattern1Dice;       // Times held 1 die
  uint16_t holdPattern2Dice;       // Times held 2 dice (pairs)
  uint16_t holdPattern3Dice;       // Times held 3 dice
  uint16_t holdPattern4Dice;       // Times held 4 dice
  uint16_t holdPattern5Dice;       // Times held 5 dice (no reroll)
  
  // Bonus pursuit analytics
  
  // Opponent modeling (track human player patterns)
  
  // Endgame scenarios (last 3 turns behavior)
  uint16_t endgameComebackAttempts;// Times tried comeback when down 30+ pts
  
  // Category timing optimization (early vs late game)
  uint8_t optimalTurnForYahtzee;   // Best turn to score Yahtzee (1-13)
  uint8_t optimalTurnForLargeStraight;  // Best turn for Large Straight
  
  // ═══════════════════════════════════════════════════════════════
  // **IMPROVEMENT 1: Hold Pattern Value Tracking**
  // Track which specific dice values/counts work best
  // ═══════════════════════════════════════════════════════════════
  
  // For each dice value (1-6), track holding different counts
  uint16_t holdValueCount[6][6];      // [value-1][count] - times held
  uint16_t holdValueScoreSum[6][6];   // [value-1][count] - total points after
  uint16_t holdValueSuccess[6][6];    // [value-1][count] - times led to 20+ pts
  
  // ═══════════════════════════════════════════════════════════════
  // **IMPROVEMENT 2: Reroll Decision Quality Tracking**
  // Learn when to stop rolling vs push for more
  // ═══════════════════════════════════════════════════════════════
  
  
  // Breakdown by points available before reroll
  
  
  // ═══════════════════════════════════════════════════════════════
  // **IMPROVEMENT 3: Enhanced Category Win Rate Usage**
  // (categoryScoredCount and categoryWinsWhenScored already exist above)
  // Add category average scores for better decision weighting
  // ═══════════════════════════════════════════════════════════════
  
  // NOTE: Using categoryScoreSum from line 280 instead of duplicate field
  uint8_t categoryOptimalScore[13];   // Best score ever achieved / 2 (0-127 range)
  
  // ═══════════════════════════════════════════════════════════════
  // **V39 ENHANCEMENT: GRAPH-SPECIFIC TRACKING**
  // New metrics for enhanced 6-graph analysis system
  // ═══════════════════════════════════════════════════════════════
  
  // Rolling window for learning progress tracking (last 50 games)
  
  // Category-specific success tracking for efficiency analysis
  
  
  
  
  // Score distribution tracking (histogram buckets for graph)
  uint16_t scoreRanges[10];           // Buckets: 0-49, 50-99, ..., 450+
  
  // Learning efficiency comparison (improvement tracking)

  // ═══════════════════════════════════════════════════════════════
  // **DECISION QUALITY STATS — used directly in AI decisions**
  // ═══════════════════════════════════════════════════════════════

  // Chance timing: track outcomes by game phase
  // Buckets: [0]=turns 1-3, [1]=turns 4-6, [2]=turns 7-9, [3]=turns 10-13
  uint16_t chanceTurnBucket[4];     // Times Chance was scored in each phase
  uint16_t chanceScoreSum[4];       // Total Chance score in each phase

  // Upper slot sacrifice: when AI scored weak into an upper slot, did it pay off?
  // Index = die face value - 1 (0=Ones..5=Sixes)
  uint16_t upperSacrificeCount[6];  // Times scored < 33% of max into this slot
  uint16_t upperSacrificeBonus[6];  // Times still achieved bonus after weak score

  // Score differential at turn 10 — key endgame predictor
  // Stored as running sum (divide by gamesReachingTurn10 for avg)
  int16_t  scoreDiffSumTurn10;      // Sum of (aiScore - humanScore) at turn 10
  uint16_t gamesReachingTurn10;     // Total games that reached turn 10

  // Chance vs upper section decision outcomes
  // When both Chance and a weak upper slot were available, which was better?
  // [0]=chose upper section, [1]=chose Chance

  // Per-category timing: which turn each category tends to be scored
  uint8_t  categoryAvgTurn[13];     // Average turn (1-13) when category was scored

  // Total score when category scored early (turns 1-6) — used to judge
  // whether holding a category for later is actually worth it
  uint16_t categoryEarlyScoreSum[13];

  // Rolling last-10-games head-to-head scores (shown on the AI Stats screen)
  uint16_t recentAIScores[10];    // ring buffer of AI final scores
  uint16_t recentHumanScores[10]; // ring buffer of human final scores
  uint8_t  recentCount;           // games recorded so far (caps at 10)
  uint8_t  recentHead;            // next ring write index (0-9)

  // Lifetime head-to-head stats (AI vs human). These are the meaningful ones:
  // the AI is a fixed optimal policy, so its long-run numbers must converge on
  // the solver's reference values (see YZ_REF_* below). A persistent deviation
  // means the on-device brain is misbehaving, not "playing badly".
  uint32_t aiScoreSum;            // lifetime sum of AI final scores
  uint32_t humanScoreSum;         // lifetime sum of human final scores
  uint16_t aiHighScore;           // best AI game
  uint16_t humanHighScore;        // best human game
  uint16_t aiBonusGames;          // games where AI made the 63+ upper bonus
  uint16_t humanBonusGames;       // games where human made the 63+ upper bonus
  uint16_t aiYahtzeeGames;        // games where AI scored a Yahtzee (50 in the box)
  uint16_t humanYahtzeeGames;     // games where human scored a Yahtzee
  uint16_t humanWins;             // human wins (ties = gamesPlayed - aiWins - humanWins)

  uint8_t checksum;               // Data validation checksum (MUST stay last)
};

// ── Optimal-policy reference values ─────────────────────────────────────────
// Measured from the exact joint-DP solver over 60,000 simulated games using the
// same value table the device runs (vtable.h). These are constants of the game,
// not targets: a correct implementation MUST converge on them.
//   mean 253.7 | sd 58.3 | upper-bonus 67.7% | yahtzee 33.9%
// NOTE sd=58 is large: over 10 games the standard error is ~18 points, so short
// runs prove nothing. Judge the AI against these only after ~100+ games.
#define YZ_REF_AVG    254
#define YZ_REF_BONUS   68
#define YZ_REF_YTZ     34
  
AILearningData aiLearning;

// ============================================================================
// STRATEGY VARIANT SYSTEM DOCUMENTATION
// ============================================================================
// The AI uses 4 different strategy variants to test and compare approaches.
// Each variant has different weights for pursuing special scores (Yahtzee, 
// straights, etc.). The system tracks which variants win most often and
// gradually favors the best-performing strategies.
//
// **VARIANT STRUCTURE:**
// Each variant stores:
// - 8 weight values (weightYahtzee, weightLargeStraight, etc.)
// - Performance metrics (wins, losses, totalScore, gamesPlayed)
//
// **HOW VARIANTS ARE USED:**
// 1. During AI training (self-play), different variants compete
// 2. Each variant plays games and tracks its win rate
// 3. Better-performing variants get used more often
// 4. Variants are stored in EEPROM and evolve over time
//
// **VARIANT SELECTION:**
// - In regular games: AI uses the main aiLearning weights
// - In training mode: Variants are selected and compete
// - Selection favors variants with higher win rates (exploitation)
// - But also gives chances to all variants (exploration)
//
// **ACCESSING VARIANT DATA:**
// - Variants are saved to EEPROM at address EEPROM_STRATEGY_VARIANTS_ADDR
// - Each variant is ~18 bytes (8 weights + 4 performance counters)
// - Total: 4 variants × 18 bytes = 72 bytes
// - Use drawAIStatistics() page 3 to view variant performance
//
// **EVOLUTION:**
// - Variants slowly adjust their weights based on success
// - Poor-performing variants adopt strategies from winners
// - System maintains diversity to avoid local maxima
// ============================================================================

// Track 4 strategy variants for A/B testing and evolution
int currentVariantIndex = 0;  // Which variant is currently active (0-3)

// NEW: Decision tracking
DecisionOutcome decisionHistory[MAX_DECISION_HISTORY];
int decisionHistoryCount = 0;
int decisionHistoryIndex = 0;  // Circular buffer index

// Self-play training state

// AI Probability Tables for Expected Value Calculations
// Probability of getting N of a kind when rerolling M dice
const float PROB_OF_KIND[6][6] = {
  // M dice to reroll: 0     1      2      3      4      5
  {1.000, 0.167, 0.028, 0.005, 0.001, 0.000},  // 1 of a kind
  {1.000, 0.028, 0.077, 0.032, 0.008, 0.001},  // 2 of a kind
  {1.000, 0.005, 0.032, 0.074, 0.077, 0.032},  // 3 of a kind  
  {1.000, 0.001, 0.008, 0.077, 0.193, 0.198},  // 4 of a kind
  {1.000, 0.000, 0.001, 0.032, 0.198, 0.598},  // 5 of a kind (Yahtzee)
  {0.000, 0.000, 0.000, 0.000, 0.000, 0.000}   // Unused
};

// Probability of getting a straight when rerolling M dice
// Exact straight probabilities [values_missing][rolls_remaining]
// Computed via backward induction accounting for adaptive re-holding each roll.
// Index 0=already have it, 1=need 1 more value, etc.
const float PROB_LARGE_2D[6][3] = {
  {1.0000, 1.0000, 1.0000},  // 0 missing
  {0.0000, 0.3333, 0.5556},  // 1 missing
  {0.0000, 0.1111, 0.2670},  // 2 missing
  {0.0000, 0.0556, 0.1749},  // 3 missing
  {0.0000, 0.0370, 0.1211},  // 4 missing
  {0.0000, 0.0309, 0.0988},  // 5 missing
};
const float PROB_SMALL_2D[5][3] = {
  {1.0000, 1.0000, 1.0000},  // 0 missing
  {0.0000, 0.5556, 0.8025},  // 1 missing
  {0.0000, 0.2500, 0.5055},  // 2 missing
  {0.0000, 0.2130, 0.4614},  // 3 missing
  {0.0000, 0.1543, 0.3733},  // 4 missing
};
// Legacy 1D tables kept for PROB_OF_KIND references only
const float PROB_SMALL_STRAIGHT[6] = {1.000, 0.308, 0.247, 0.165, 0.086, 0.031};
const float PROB_LARGE_STRAIGHT[6] = {1.000, 0.154, 0.123, 0.082, 0.043, 0.015};

// AI personality traits
float aggressivenessWeight = 0.5;  // 0.0=very conservative, 1.0=very aggressive
int currentGameStrategy = 0;  // 0=conservative, 1=aggressive (tracked for learning)
bool currentGameIsExploration = false;  // NEW: Track if this game is exploration

// Computer AI state
bool computerIsThinking = false;
int computerThinkPhase = 0;  // 0=roll, 1=hold decision, 2=category select

// Particle system for celebrations
struct Particle {
  float x, y;           // Position
  float vx, vy;         // Velocity
  uint16_t color;       // Color
  float life;           // Lifetime (0-1, fades to 0)
  float fadeRate;       // How fast it fades
  bool active;          // Is this particle alive?
};

// Rainbow colors for winner celebration
const uint16_t RAINBOW_COLORS[8] = {
  0xF800,  // Red
  0xFD20,  // Orange
  0xFFE0,  // Yellow
  0x07E0,  // Green
  0x07FF,  // Cyan
  0x001F,  // Blue
  0x8010,  // Purple
  0xF81F   // Magenta
};

const int MAX_PARTICLES = 100;  // Increased for enhanced winner celebration
Particle particles[MAX_PARTICLES];
bool celebrationActive = false;
unsigned long celebrationStartTime = 0;
int celebrationType = 0;  // 0=none, 1=4ofKind, 2=FullHouse, 3=SmallStraight, 4=LargeStraight, 5=Yahtzee, 6=Winner, 7=UpperBonus, 8=YahtzeeBonus

// Animation state for intro
struct DiceAnim {
  float x, y;           // Position
  float vx, vy;         // Velocity
  float rotation;       // Rotation angle
  float rotSpeed;       // Rotation speed
  int faceValue;        // Which face (1-6)
  bool settled;         // Has it stopped moving?
  int bounceCount;      // Number of ground bounces so far (capped at 2)
};
DiceAnim animDice[5];
unsigned long animStartTime = 0;
bool introComplete = false;

// Title animation state
bool titleAnimating = false;
unsigned long titleStartTime = 0;
struct LetterAnim {
  float x, y;        // Current position
  float targetX, targetY;  // Final position
  float vx, vy;      // Velocity
  char letter;
  bool settled;
};
LetterAnim letters[7];  // Y-A-H-T-Z-E-E
int diceGlowIntensity = 0;

int selectedCategory = 0; // Currently selected category (0-12)
int menuScroll = 0;       // Scroll position in category list
int scoreViewSection = 0; // 0 = upper, 1 = lower (for viewing during gameplay)
bool winnerCelebrationShown = false;  // Track if we've shown the winner celebration
bool statsUpdated = false;  // Track if game-over stats have been recorded
int aiLastScoredCategory = -1;  // Category AI just scored (highlighted during score-view pause; -1 = none)
unsigned long winnerFlashTimer = 0;   // Timer for winner badge flashing
bool winnerFlashState = true;         // Current flash state (visible/hidden)

// 7-segment dice roll animation
bool diceAnimating = false;
unsigned long diceAnimStartTime = 0;
int diceAnimFrame = 0;
int finalDiceValues[5];  // Store final values before animation
unsigned long diceSettleTime[5];  // When each die should settle

// ============================================================================
// COLORS
// ============================================================================

#define COLOR_BG       0x0000  // Black
#define COLOR_P1       0x07FF  // Cyan
#define COLOR_P2       0xF81F  // Magenta
#define COLOR_TEXT     0xFFFF  // White
#define COLOR_HELD     0xFFE0  // Yellow
#define COLOR_GRAY     0x7BEF  // Light gray
#define COLOR_GREEN    0x07E0  // Green
#define COLOR_RED      0xF800  // Red

// ============================================================================
// CATEGORY NAMES
// ============================================================================

const char* categoryNames[] = {
  "Aces", "Twos", "Threes", "Fours", "Fives", "Sixes",
  "3 of Kind", "4 of Kind", "Full House", "Sm Straight", 
  "Lg Straight", "Yahtzee", "Chance"
};

const char* sectionNames[] = {
  "UPPER SECTION",
  "LOWER SECTION"
};

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
void drawScreen();
void drawStartScreen();
void drawGamePlay();
void drawCategorySelect();
void drawGameOver();
void drawMenu();
void drawMenuItems();
void drawToolsMenu();
void drawToolsMenuItems(bool forceFullRedraw = false);
void drawSoundMenu();
void drawSoundMenuItems();
void drawHelpMenu();
void drawGraphMenu();  // AI Performance Graphs (V39: 6 graphs)
void drawScoreDistributionGraph();  // Graph 3: Score distribution
void drawStatistics();
void exportStatsToSerial();  // NEW: Export all stats to Serial Monitor
void rollDice();
void selectCategory();
int calculateTotal(int scores[]);
int calculateTotalWithBonuses(int scores[], int yahtzeeBonus);
int getUpperSectionTotal(int scores[]);
bool hasUpperBonus(int scores[]);
float calcBonusFeasibility(int scores[]);   // NEW: realistic bonus probability 0-1
float upperOpportunityCost(int value, int count); // NEW: slot waste metric 0-1
int findBestCategory(int dice[], int scores[]);
void computerTurn();
void computerDecideHolds();
void initCelebration(int type);
void updateCelebration();
void drawCelebration();
void recordDecision(int turnNum, int rollNum, int diceHeldCount, int category, int points, bool leading);
int calculateTotalWithBonuses(int scores[], int yahtzeeBonus);
int getUpperSectionTotal(int scores[]);
bool hasUpperBonus(int scores[]);
int findBestCategory(int dice[], int scores[]);
void computerTurn();  // AI main logic
void computerDecideHolds();  // AI hold decision
void initCelebration(int type);  // Initialize celebration
void updateCelebration();  // Update particle positions
void drawCelebration();  // Render particles
void drawStar(int cx, int cy, int size, uint16_t color);  // Draw star sparkle
void drawTrophy(int cx, int cy, uint16_t baseColor, uint16_t accentColor);  // Draw trophy icon
void recordDecision(int turnNum, int rollNum, int diceHeldCount, int category, int points, bool leading);

// AI Learning and Strategy Functions
void updateAILearning(bool aiWon, int aiScore, int humanScore);
void resetAILearningData();
uint8_t calculateChecksum(AILearningData* data);
void computerDecideHoldsAdvanced();  // New advanced hold decision
bool yzAI_shouldReroll();            // EV-based stop-vs-reroll gate (option B)
int findBestCategoryAI(int dice[], int scores[], bool useExpectedValue);

// Learning helper functions (defined later in file)

// Blackjack entry point (defined in Blackjack_V1.ino, compiled together)
void runBlackjack();
void bj_advanceSplitHand();

void loadAILearningFromFile();
void saveAILearningToFile();

// Buzzer functions
void buzzerBeep(int duration = 100);
void buzzerTone(int frequency, int duration);
void buzzerStartRoll();
void buzzerYahtzee();
void buzzerError();
void buzzerStartupTune();
void buzzerMenuMove();
void buzzerMenuSelect();
void buzzerDiceHold();
void buzzerDiceUnhold();
void buzzerCategorySelect();
void buzzerGameOver();
void buzzerWinnerCelebration();

// ============================================================================
// AI HOLD STRATEGY STRUCT
// ============================================================================
struct HoldStrategy {
  bool holds[5];
  float expectedValue;
  int targetCategory;
  const char* description;
};

// ============================================================================
// MAX7219 FUNCTIONS
// ============================================================================

void max7219Send(byte reg, byte data) {
  digitalWrite(CS_PIN, LOW);
  delayMicroseconds(1);
  shiftOut(DIN_PIN, CLK_PIN, MSBFIRST, reg);
  shiftOut(DIN_PIN, CLK_PIN, MSBFIRST, data);
  delayMicroseconds(1);
  digitalWrite(CS_PIN, HIGH);
}

void max7219Init() {
  pinMode(DIN_PIN, OUTPUT);
  pinMode(CLK_PIN, OUTPUT);
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  
  delay(50);
  
  max7219Send(0x09, 0xFF); // Decode mode: BCD for all digits
  max7219Send(0x0A, max7219Brightness); // Intensity: use variable
  max7219Send(0x0B, 0x04); // Scan limit: 5 digits (0-4)
  max7219Send(0x0C, 0x01); // Shutdown: normal operation
  max7219Send(0x0F, 0x00); // Display test: off
  
  delay(50);
  
  // Clear all digits
for(int i = 1; i <= 5; i++) {
  max7219Send(i, 0x0F); // Blank
}
}

void max7219SetBrightness(int level) {
  // Clamp to valid range (0-15)
  if(level < 0) level = 0;
  if(level > 15) level = 15;
  
  max7219Brightness = level;
  max7219Send(0x0A, max7219Brightness);
  
  // Save to EEPROM for persistence
  EEPROM.write(EEPROM_BRIGHTNESS_ADDR, max7219Brightness);
  EEPROM.commit();  // Make sure it's written to flash
  
}

void updateDiceDisplay() {
  // Display dice on digits 1-5 (digit 1 = leftmost, digit 5 = rightmost)
  for(int i = 0; i < 5; i++) {
    byte value;
    if(dice[i] == 0) {
      value = 0;  // Show 0 if dice is blank/unrolled
    } else {
      value = dice[i];
      if(held[i]) {
        value |= 0x80; // Set decimal point for held indicator
      }
    }
    max7219Send(i + 1, value);
  }
}

// Blank non-held dice on 7-segment immediately after rollDice(),
// so the animation always starts from blank instead of flashing the final value.
void blankDiceDisplay() {
  for(int i = 0; i < 5; i++) {
    if(held[i]) {
      byte value = dice[i] | 0x80; // Keep held dice visible with decimal point
      max7219Send(i + 1, value);
    } else {
      max7219Send(i + 1, 0x0F); // Blank (BCD blank code)
    }
  }
}

void updateDiceAnimation() {
  if(!diceAnimating) return;
  
  unsigned long elapsed = millis() - diceAnimStartTime;
  
  // Animation patterns for moving horizontal lines
  // These create the illusion of scanning lines moving down
  const byte scanPatterns[] = {
    0b00000001,  // Top horizontal
    0b00000110,  // Top + upper verticals
    0b01000000,  // Middle horizontal
    0b00111000,  // Middle + lower verticals
    0b00001000,  // Bottom horizontal
    0b00000000   // Blank before showing number
  };
  
  // Update each die
  for(int i = 0; i < 5; i++) {
    if(held[i]) {
        // Held dice don't animate, just show with decimal point
        byte value = dice[i] | 0x80;  // Set decimal point
        max7219Send(i + 1, value);
      } else {
        // Check if this die should settle and show final number
        if(elapsed >= diceSettleTime[i]) {
          // Show final value
          max7219Send(i + 1, finalDiceValues[i]);
        } else {
          // Still animating - show scanning lines
          
          // Calculate speed based on time remaining
          float timeToSettle = diceSettleTime[i] - elapsed;
          int cycleSpeed;
          
          if(timeToSettle > 300) {
            cycleSpeed = 60;  // Fast at start
          } else if(timeToSettle > 150) {
            cycleSpeed = 100;  // Medium speed
          } else {
            cycleSpeed = 150;  // Slow down near end
          }
          
          // Cycle through scan patterns
          int patternIndex = (elapsed / cycleSpeed) % 6;
          
        // Send raw segment data (not BCD mode)
          max7219Send(i + 1, scanPatterns[patternIndex]);
        }
      }
  }
  
  // Check if animation complete (all dice settled)
  bool allSettled = true;
  for(int i = 0; i < 5; i++) {
    if(!held[i] && elapsed < diceSettleTime[i]) {
      allSettled = false;
      break;
    }
  }
  
  if(allSettled) {
    diceAnimating = false;
    updateDiceDisplay();  // Final update with correct values
    
    // **NEW: Brief pause to let player see final dice**
    delay(200);
    
    // If this was the final roll, go straight to category selection
    if(rollsLeft == 0 && gameState == STATE_ROLLING) {
      int* scores = (currentPlayer == 1) ? scores1 : scores2;
      selectedCategory = findBestCategory(dice, scores);
      if(selectedCategory == -1) selectedCategory = 0;
      
      gameState = STATE_CATEGORY_SELECT;
      drawScreen();  // NOW update the screen with scores
    } else {
      // **NEW: Also update screen for non-final rolls**
      drawScreen();
    }
  }
}

void drawDiceFace(int x, int y, int size, int value, float rotation) {
  // Simple dice drawing - we'll rotate the whole coordinate system
  // For now, draw a square with dots (no rotation yet - we'll add that next)
  
  // Draw white square for die
  tft.fillRect(x - size/2, y - size/2, size, size, COLOR_TEXT);
  tft.drawRect(x - size/2, y - size/2, size, size, COLOR_BG);
  
  // Draw dots based on value
  int dotSize = size / 8;
  int offset = size / 4;
  
  // Center dot positions
  int cx = x;
  int cy = y;
  
  switch(value) {
    case 1:
      tft.fillCircle(cx, cy, dotSize, COLOR_BG);
      break;
    case 2:
      tft.fillCircle(cx - offset, cy - offset, dotSize, COLOR_BG);
      tft.fillCircle(cx + offset, cy + offset, dotSize, COLOR_BG);
      break;
    case 3:
      tft.fillCircle(cx - offset, cy - offset, dotSize, COLOR_BG);
      tft.fillCircle(cx, cy, dotSize, COLOR_BG);
      tft.fillCircle(cx + offset, cy + offset, dotSize, COLOR_BG);
      break;
    case 4:
      tft.fillCircle(cx - offset, cy - offset, dotSize, COLOR_BG);
      tft.fillCircle(cx + offset, cy - offset, dotSize, COLOR_BG);
      tft.fillCircle(cx - offset, cy + offset, dotSize, COLOR_BG);
      tft.fillCircle(cx + offset, cy + offset, dotSize, COLOR_BG);
      break;
    case 5:
      tft.fillCircle(cx - offset, cy - offset, dotSize, COLOR_BG);
      tft.fillCircle(cx + offset, cy - offset, dotSize, COLOR_BG);
      tft.fillCircle(cx, cy, dotSize, COLOR_BG);
      tft.fillCircle(cx - offset, cy + offset, dotSize, COLOR_BG);
      tft.fillCircle(cx + offset, cy + offset, dotSize, COLOR_BG);
      break;
    case 6:
      tft.fillCircle(cx - offset, cy - offset, dotSize, COLOR_BG);
      tft.fillCircle(cx + offset, cy - offset, dotSize, COLOR_BG);
      tft.fillCircle(cx - offset, cy, dotSize, COLOR_BG);
      tft.fillCircle(cx + offset, cy, dotSize, COLOR_BG);
      tft.fillCircle(cx - offset, cy + offset, dotSize, COLOR_BG);
      tft.fillCircle(cx + offset, cy + offset, dotSize, COLOR_BG);
      break;
  }
}

void drawGameDie(int x, int y, int size, int value, bool isHeld) {
  // Draw die background
  uint16_t bgColor = isHeld ? COLOR_HELD : COLOR_TEXT;
  uint16_t dotColor = isHeld ? COLOR_BG : COLOR_BG;
  
  // Draw rounded rectangle (simplified as regular rect)
  tft.fillRect(x, y, size, size, bgColor);
  tft.drawRect(x, y, size, size, COLOR_BG);
  
  // Draw dots
  int dotSize = size / 9;  // Reduced from /7 to /9 for smaller dots
  int offset = size / 4;
  int cx = x + size / 2;
  int cy = y + size / 2;
  
  switch(value) {
    case 1:
      // Center dot
      tft.fillCircle(cx, cy, dotSize, dotColor);
      break;
      
    case 2:
      // Diagonal dots
      tft.fillCircle(cx - offset, cy - offset, dotSize, dotColor);
      tft.fillCircle(cx + offset, cy + offset, dotSize, dotColor);
      break;
      
    case 3:
      // Diagonal line
      tft.fillCircle(cx - offset, cy - offset, dotSize, dotColor);
      tft.fillCircle(cx, cy, dotSize, dotColor);
      tft.fillCircle(cx + offset, cy + offset, dotSize, dotColor);
      break;
      
    case 4:
      // Four corners
      tft.fillCircle(cx - offset, cy - offset, dotSize, dotColor);
      tft.fillCircle(cx + offset, cy - offset, dotSize, dotColor);
      tft.fillCircle(cx - offset, cy + offset, dotSize, dotColor);
      tft.fillCircle(cx + offset, cy + offset, dotSize, dotColor);
      break;
      
    case 5:
      // Four corners + center
      tft.fillCircle(cx - offset, cy - offset, dotSize, dotColor);
      tft.fillCircle(cx + offset, cy - offset, dotSize, dotColor);
      tft.fillCircle(cx, cy, dotSize, dotColor);
      tft.fillCircle(cx - offset, cy + offset, dotSize, dotColor);
      tft.fillCircle(cx + offset, cy + offset, dotSize, dotColor);
      break;
      
    case 6:
      // Six dots (two columns)
      tft.fillCircle(cx - offset, cy - offset, dotSize, dotColor);
      tft.fillCircle(cx + offset, cy - offset, dotSize, dotColor);
      tft.fillCircle(cx - offset, cy, dotSize, dotColor);
      tft.fillCircle(cx + offset, cy, dotSize, dotColor);
      tft.fillCircle(cx - offset, cy + offset, dotSize, dotColor);
      tft.fillCircle(cx + offset, cy + offset, dotSize, dotColor);
      break;
      
    case 0:
      // Blank die - no dots
      break;
  }
}

void initIntroAnimation() {
  animStartTime = millis();
  introComplete = false;
  
  // Initialize 5 dice falling from above — faster drop, staggered slightly
  for(int i = 0; i < 5; i++) {
    animDice[i].x = 30 + i * 45;       // Spread across screen
    animDice[i].y = -30 - i * 15;      // Slight stagger so they don't all land at once
    animDice[i].vx = random(-30, 30) / 10.0;  // Mild horizontal drift
    animDice[i].vy = random(20, 50) / 10.0;   // Initial downward velocity
    animDice[i].rotation = random(0, 360);
    animDice[i].rotSpeed = random(-120, 120) / 10.0;
    animDice[i].faceValue = random(1, 7);
    animDice[i].settled = false;
    animDice[i].bounceCount = 0;
  }

  // Reset title animation
  titleAnimating = false;
  diceGlowIntensity = 0;
}

void initTitleAnimation() {
  titleAnimating = true;
  titleStartTime = millis();
  diceGlowIntensity = 0;
  
  // Calculate center of dice pile
  float centerX = 0, centerY = 0;
  for(int i = 0; i < 5; i++) {
    centerX += animDice[i].x;
    centerY += animDice[i].y;
  }
  centerX /= 5.0;
  centerY /= 5.0;
  
  // Initialize letters starting from dice center
  const char* word = "YAHTZEE";
  int titleY = 80;  // Final position
  int letterSpacing = 28;
  int totalWidth = 7 * letterSpacing;
  int startX = (240 - totalWidth) / 2;
  
  for(int i = 0; i < 7; i++) {
    letters[i].letter = word[i];
    letters[i].x = centerX;  // Start at dice center
    letters[i].y = centerY;
    letters[i].targetX = startX + (i * letterSpacing) + 14;
    letters[i].targetY = titleY;
    
    // Explode outward with random velocity
    float angle = random(0, 360) * 3.14159 / 180.0;
    letters[i].vx = cos(angle) * random(30, 60) / 10.0;
    letters[i].vy = sin(angle) * random(30, 60) / 10.0 - 5.0;  // Bias upward
    letters[i].settled = false;
  }
}

void updateTitleAnimation() {
  unsigned long elapsed = millis() - titleStartTime;
  
  // Brief glow flash on dice when title launches (0-200ms)
  if(elapsed < 200) {
    diceGlowIntensity = (int)(255 * (elapsed / 200.0));
  } else {
    diceGlowIntensity = 0;
  }
  
  // Letters spring toward target immediately
  if(elapsed > 100) {
    bool allSettled = true;
    
    for(int i = 0; i < 7; i++) {
      if(!letters[i].settled) {
        float dx = letters[i].targetX - letters[i].x;
        float dy = letters[i].targetY - letters[i].y;
        float dist = sqrt(dx*dx + dy*dy);
        
        // Strong spring pull toward target
        float attractionStrength = 0.35;
        letters[i].vx += dx * attractionStrength;
        letters[i].vy += dy * attractionStrength;
        
        // Heavy damping — kills overshoot so letters don't oscillate
        letters[i].vx *= 0.68;
        letters[i].vy *= 0.68;
        
        letters[i].x += letters[i].vx;
        letters[i].y += letters[i].vy;
        
        // Snap to target once close enough
        if(dist < 3.0 && abs(letters[i].vx) < 0.3 && abs(letters[i].vy) < 0.3) {
          letters[i].x = letters[i].targetX;
          letters[i].y = letters[i].targetY;
          letters[i].vx = 0;
          letters[i].vy = 0;
          letters[i].settled = true;
        } else {
          allSettled = false;
        }
      }
    }
    
    if(allSettled && elapsed > 300) {
      introComplete = true;
    }
  }
}

void updateIntroAnimation() {
  const float gravity = 2.5;     // Strong gravity for fast, snappy fall
  const float bounce = 0.42;     // 1st bounce ~42%, 2nd ~18% — visibly smaller each time
  const float friction = 0.80;   // Kill horizontal/rotational speed quickly after landing
  const float groundY = 250;
  const int diceSize = 35;
  const int maxBounces = 2;      // Hard cap: exactly 2 bounces, then settle
  
  bool allSettled = true;
  
  for(int i = 0; i < 5; i++) {
    if(!animDice[i].settled) {
      // Apply gravity
      animDice[i].vy += gravity;
      
      // Update position
      animDice[i].x += animDice[i].vx;
      animDice[i].y += animDice[i].vy;
      
      // Update rotation (slow it down after bounces)
      animDice[i].rotation += animDice[i].rotSpeed;
      
      // Ground collision
      if(animDice[i].y >= groundY - diceSize/2) {
        animDice[i].y = groundY - diceSize/2;
        animDice[i].bounceCount++;
        
        if(animDice[i].bounceCount >= maxBounces) {
          // Hard settle after 2nd bounce — snap to rest immediately
          animDice[i].settled = true;
          animDice[i].vy = 0;
          animDice[i].vx = 0;
          animDice[i].rotSpeed = 0;
        } else {
          // Bounce: reverse and reduce vertical velocity
          animDice[i].vy *= -bounce;
          animDice[i].vx *= friction;
          animDice[i].rotSpeed *= friction;
        }
      }
      
      // Soft wall bounce (just deflect, don't kill energy)
      if(animDice[i].x < diceSize/2) {
        animDice[i].x = diceSize/2;
        animDice[i].vx *= -0.5;
      }
      if(animDice[i].x > 240 - diceSize/2) {
        animDice[i].x = 240 - diceSize/2;
        animDice[i].vx *= -0.5;
      }
      
      allSettled = false;
    }
  }
  
  // Start title animation as soon as all dice have settled
  if(allSettled) {
    if(!titleAnimating) {
      initTitleAnimation();
    }
    updateTitleAnimation();
  }
}

void drawIntroScreen() {
  tft.fillScreen(COLOR_BG);
  
  const int diceSize = 35;
  
  // Draw all 5 dice
  for(int i = 0; i < 5; i++) {
    drawDiceFace(
      (int)animDice[i].x, 
      (int)animDice[i].y, 
      diceSize, 
      animDice[i].faceValue,
      animDice[i].rotation
    );
    
    // Draw glow effect around dice
    if(diceGlowIntensity > 0) {
      // Draw expanding circles for glow
      for(int r = diceSize/2 + 2; r < diceSize/2 + 10; r += 2) {
        int alpha = diceGlowIntensity * (1.0 - (r - diceSize/2) / 10.0);
        if(alpha > 0) {
          // Simple circle outline for glow
          tft.drawCircle((int)animDice[i].x, (int)animDice[i].y, r, 
                        tft.color565(0, alpha, 0));  // Green glow
        }
      }
    }
  }
  
  // Draw exploding/settling letters
  if(titleAnimating && !introComplete) {
    tft.setTextSize(4);
    tft.setTextColor(COLOR_GREEN);
    
    for(int i = 0; i < 7; i++) {
      tft.setCursor((int)letters[i].x - 12, (int)letters[i].y - 14);
      tft.print(letters[i].letter);
    }
  }
  
  // Draw final settled title
  if(introComplete) {
    tft.setTextSize(4);
    tft.setTextColor(COLOR_GREEN);
    
    int letterSpacing = 28;
    int totalWidth = 7 * letterSpacing;
    int startX = (240 - totalWidth) / 2;
    
    const char* word = "YAHTZEE";
    for(int i = 0; i < 7; i++) {
      tft.setCursor(startX + (i * letterSpacing), 80);
      tft.print(word[i]);
    }
    
    // No "Press ROLL" message - will transition to menu automatically
  }
}

void drawMenuItems() {
  // Redraw just the menu items without clearing screen
  const char* menuItems[] = {
    "2 Player Game",
    "1 vs Computer",
    "Tools"
  };
  
  int yStart = 100;
  int itemHeight = 50;
  
  for(int i = 0; i < 3; i++) {
    int y = yStart + (i * itemHeight);
    
    // Clear the item area
    tft.fillRect(20, y - 3, 200, 24, COLOR_BG);
    
    // Draw selection box for current item (NARROWER and CENTERED)
    if(i == selectedMenuItem) {
      tft.fillRect(20, y - 3, 200, 24, COLOR_GREEN);  // Narrower: 200px wide, 24px tall
      tft.setTextColor(COLOR_BG);
    } else {
      tft.setTextColor(COLOR_TEXT);
    }
    
    // Draw menu item text (CENTERED in cursor)
    tft.setTextSize(2);
    tft.setCursor(30, y + 2);  // Moved right and adjusted vertical centering
    tft.print(menuItems[i]);
  }
}

void drawMenu() {
  tft.fillScreen(COLOR_BG);
  
  // Draw YAHTZEE title at top
  tft.setTextSize(4);
  tft.setTextColor(COLOR_GREEN);
  
  int letterSpacing = 28;
  int totalWidth = 7 * letterSpacing;
  int startX = (240 - totalWidth) / 2;
  
  const char* word = "YAHTZEE";
  for(int i = 0; i < 7; i++) {
    tft.setCursor(startX + (i * letterSpacing), 20);
    tft.print(word[i]);
  }
  
  // Draw menu items
  drawMenuItems();
  
  // Instructions at bottom
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GRAY);
  tft.setCursor(10, 290);
  tft.print("UP/DOWN: Select  ENTER: Start");
}

void drawToolsMenuItems(bool forceFullRedraw) {
  // Redraw just the menu items without clearing screen
  // OPTIMIZED: Only redraw items that changed to prevent flashing
  static int lastSelectedToolItem = -1;
  
  const char* toolItems[] = {
    "7-Seg Bright",
    "Sound",
    "Auto-Advance",
    "Statistics",
    "AI Stats",
    "AI Speed",
    "Export Stats",
    "AI Graphs",
    "Game Rules",
    "Blackjack",      // NEW
    "Back"
  };
  
  int itemCount = 11;  // 11 items (Train AI removed — optimal brain does not train)
  
  int yStart = 50;  // Start below TOOLS heading
  int itemHeight = 20;  // 20 pixels for 12 items
  
  // Force full redraw if requested (e.g., returning from help menu)
  if(forceFullRedraw) {
    lastSelectedToolItem = -1;
  }
  
  // Only redraw the previously selected item and currently selected item
  // to minimize screen flashing
  for(int i = 0; i < itemCount; i++) {
    // Skip items that haven't changed (unless first draw or forced)
    if(lastSelectedToolItem != -1 && i != selectedToolItem && i != lastSelectedToolItem) {
      continue;
    }
    
    int y = yStart + (i * itemHeight);
    
    // Clear the item area (wider to include values)
    tft.fillRect(10, y - 2, 220, 21, COLOR_BG);
    
    // Draw selection box for current item
    if(i == selectedToolItem) {
      tft.fillRect(10, y - 2, 200, 21, COLOR_GREEN);
      tft.setTextColor(COLOR_BG);
    } else {
      tft.setTextColor(COLOR_TEXT);
    }
    
    // Draw menu item text - SIZE 2 FONT
    tft.setTextSize(2);
    tft.setCursor(15, y + 2);
    tft.print(toolItems[i]);
    
    // Show current 7-seg brightness value
    if(i == 0) {
      tft.setCursor(185, y + 5);
      tft.setTextColor(i == selectedToolItem ? COLOR_BG : COLOR_TEXT);
      tft.print(max7219Brightness);
      
      if(adjustingBrightness && i == selectedToolItem) {
        tft.setTextColor(COLOR_GREEN);
        tft.setCursor(210, y + 5);
        tft.print("<");
      }
    }
    
    // Show auto-advance status (now at index 2)
    if(i == 2) {
      tft.setCursor(175, y + 5);
      if(autoAdvance) {
        tft.setTextColor(i == selectedToolItem ? COLOR_BG : COLOR_GREEN);
        tft.print("ON");
      } else {
        tft.setTextColor(i == selectedToolItem ? COLOR_BG : COLOR_RED);
        tft.print("OFF");
      }
    }
    
    // Show AI speed (now at index 5)
    if(i == 5) {
      tft.setCursor(155, y + 5);
      const char* speedNames[] = {"Slow", "Med", "Fast", "Inst"};
      if(aiSpeed >= 0 && aiSpeed <= 3) {
        tft.setTextColor(i == selectedToolItem ? COLOR_BG : COLOR_GREEN);
        tft.print(speedNames[aiSpeed]);
      }
    }
  }
  
  lastSelectedToolItem = selectedToolItem;
  
  // Redraw instructions at bottom
  tft.fillRect(0, 285, 240, 35, COLOR_BG);  // Clear instruction area
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GRAY);
  tft.setCursor(10, 290);
  
  // Show different instructions based on selection and adjustment mode
  if(selectedToolItem == 0) {
    if(adjustingBrightness) {
      tft.setTextColor(COLOR_GREEN);
      tft.print("UP: +  DOWN: -  ENTER: Done");
    } else {
      tft.print("ENTER: Adjust 7-seg bright");
    }
  } else if(selectedToolItem == 1) {
    tft.print("ENTER: Sound settings");
  } else if(selectedToolItem == 2) {
    tft.print("ENTER: Toggle auto-advance");
  } else if(selectedToolItem == 3) {
    tft.print("ENTER: View game stats");
  } else if(selectedToolItem == 4) {
    tft.print("ENTER: View AI stats");
  } else if(selectedToolItem == 5) {
    tft.print("ENTER: Change AI speed");
  } else if(selectedToolItem == 6) {
    tft.print("ENTER: Export stats (Serial)");
  } else if(selectedToolItem == 7) {
    tft.print("ENTER: View AI graphs");
  } else if(selectedToolItem == 8) {
    tft.print("ENTER: View game rules");
  } else if(selectedToolItem == 9) {
    tft.print("ENTER: Launch Blackjack");
  } else {
    tft.print("UP/DOWN: Select  ENTER: OK");
  }
}

void drawToolsMenu() {
  tft.fillScreen(COLOR_BG);
  
  // Tools title (no YAHTZEE heading - more space for menu items)
  tft.setTextSize(3);
  tft.setTextColor(COLOR_GREEN);
  tft.setCursor(60, 15);
  tft.print("TOOLS");
  
  // No page indicator needed - all on one page now
  
  // Draw menu items - force full redraw since screen was just cleared
  drawToolsMenuItems(true);
}

void drawAIDifficultyMenu() {
  tft.fillScreen(COLOR_BG);
  
  // Title
  tft.setTextSize(3);
  tft.setTextColor(COLOR_GREEN);
  tft.setCursor(10, 15);
  tft.print("SELECT AI");
  
  tft.setCursor(10, 45);
  tft.print("DIFFICULTY");
  
  // Difficulty options
  const char* difficultyNames[] = {
    "Normal",
    "Hard",
    "God Mode"
  };
  
  const char* difficultyDesc[] = {
    "50-60% win rate",
    "60-70% win rate",
    "85-95% win rate"
  };
  
  int yStart = 100;
  int itemHeight = 55;
  
  for(int i = 0; i < 3; i++) {
    int y = yStart + (i * itemHeight);
    
    // Clear the item area
    tft.fillRect(10, y - 3, 220, 50, COLOR_BG);
    
    // Draw selection box for current item
    if(i == selectedAIDifficulty) {
      tft.fillRoundRect(10, y - 3, 220, 50, 5, COLOR_GREEN);
      tft.setTextColor(COLOR_BG);
    } else {
      tft.drawRoundRect(10, y - 3, 220, 50, 5, COLOR_GRAY);
      tft.setTextColor(COLOR_TEXT);
    }
    
    // Draw difficulty name
    tft.setTextSize(2);
    tft.setCursor(20, y + 5);
    tft.print(difficultyNames[i]);
    
    // Draw description
    tft.setTextSize(1);
    if(i == selectedAIDifficulty) {
      tft.setTextColor(COLOR_BG);
    } else {
      tft.setTextColor(COLOR_GRAY);
    }
    tft.setCursor(20, y + 28);
    tft.print(difficultyDesc[i]);
  }
  
  // Instructions at bottom
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GRAY);
  tft.setCursor(10, 290);
  tft.print("UP/DOWN: Select  ENTER: Start");
  tft.setCursor(10, 305);
  tft.print("ROLL: Back to menu");
}

void drawSoundMenuItems() {
  // Redraw just the menu items without clearing screen
  const char* soundItems[] = {
    soundEnabled ? "Sound On" : "Sound Off",  // Dynamic text based on state
    "Volume",
    "Back"
  };
  
  int yStart = 100;
  int itemHeight = 40;
  
  for(int i = 0; i < 3; i++) {
    int y = yStart + (i * itemHeight);
    
    // Clear the item area (wider to include values)
    tft.fillRect(20, y - 3, 220, 30, COLOR_BG);
    
    // Draw selection box for current item
    if(i == selectedSoundItem) {
      tft.fillRect(20, y - 3, 200, 30, COLOR_GREEN);
      tft.setTextColor(COLOR_BG);
    } else {
      tft.setTextColor(COLOR_TEXT);
    }
    
    // Draw menu item text
    tft.setTextSize(2);
    tft.setCursor(30, y + 4);
    tft.print(soundItems[i]);
    
    // No additional ON/OFF text needed - it's in the main text now
    
    // Show current volume with same size font as "Volume" text
    if(i == 1) {
      tft.setTextColor(i == selectedSoundItem ? COLOR_BG : COLOR_TEXT);
      tft.setCursor(150, y + 4);  // Changed from y+7 to y+4 to align with text
      tft.setTextSize(2);  // Changed from size 1 to size 2 to match "Volume"
      tft.print(volume);
      
      // Show indicator if actively adjusting
      if(adjustingVolume && i == selectedSoundItem) {
        tft.setTextColor(COLOR_GREEN);
        tft.setCursor(210, y + 4);
        tft.setTextSize(2);
        tft.print("<");
      }
    }
  }
  
  // Redraw instructions at bottom
  tft.fillRect(0, 270, 240, 50, COLOR_BG);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GRAY);
  tft.setCursor(10, 280);
  
  if(selectedSoundItem == 0) {
    tft.print("ENTER: Toggle sound");
  } else if(selectedSoundItem == 1) {
    if(adjustingVolume) {
      tft.setTextColor(COLOR_GREEN);
      tft.print("UP: +  DOWN: -  ENTER: Done");
    } else {
      tft.print("ENTER: Adjust volume");
    }
  } else if(selectedToolItem == 9) {
    tft.print("ENTER: Launch Blackjack");
  } else {
    tft.print("UP/DOWN: Select  ENTER: OK");
  }
}

void drawSoundMenu() {
  tft.fillScreen(COLOR_BG);
  
  // Draw YAHTZEE title at top
  tft.setTextSize(4);
  tft.setTextColor(COLOR_GREEN);
  
  int letterSpacing = 28;
  int totalWidth = 7 * letterSpacing;
  int startX = (240 - totalWidth) / 2;
  
  const char* word = "YAHTZEE";
  for(int i = 0; i < 7; i++) {
    tft.setCursor(startX + (i * letterSpacing), 20);
    tft.print(word[i]);
  }
  
  // Sound subtitle
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(70, 70);
  tft.print("SOUND");
  
  // Draw menu items
  drawSoundMenuItems();
}

void drawHelpMenu() {
  tft.fillScreen(COLOR_BG);
  
  // Title - Larger and centered
  tft.setTextSize(3);
  tft.setTextColor(COLOR_GREEN);
  tft.setCursor(25, 5);
  tft.print("GAME RULES");
  
  // Page indicator - moved down below title
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(205, 28);
  tft.print(helpPage + 1);
  tft.print("/4");
  
  int y = 38;
  int lineHeight = 9;  // Tight spacing for size 1 font
  
  if(helpPage == 0) {
    // Page 1: Core Gameplay & Turn Structure
    tft.setTextSize(1);
    tft.setTextColor(COLOR_GREEN);
    tft.setCursor(5, y);
    tft.print("CORE GAMEPLAY & TURN STRUCTURE");
    y += lineHeight + 3;
    
    tft.setTextColor(COLOR_TEXT);
    
    // Turns
    tft.setCursor(5, y);
    tft.print("* Turns: Each player gets up to");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  three rolls per turn.");
    y += lineHeight + 4;
    
    // Rolling
    tft.setCursor(5, y);
    tft.print("* Rolling: On the first roll, roll");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  all five dice. You may save any");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  'keepers' and re-roll the rest,");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  repeating this up to two more");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  times (3 rolls total).");
    y += lineHeight + 4;
    
    // Scoring
    tft.setCursor(5, y);
    tft.print("* Scoring: After the third roll");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  (or earlier if desired), you");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  must enter a score or zero in");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  one of the 13 categories on");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  your scorecard.");
    y += lineHeight + 4;
    
    // Game End
    tft.setCursor(5, y);
    tft.print("* Game End: The game ends after");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  13 rounds, when all 13 boxes");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  on the scorecard are filled.");
    
    // Navigation
    tft.setTextColor(COLOR_GRAY);
    tft.setCursor(5, 305);
    tft.print("UP/DOWN: Next page  ENTER: Back");
    
  } else if(helpPage == 1) {
    // Page 2: Upper Section & Lower Section (Part 1)
    tft.setTextSize(1);
    tft.setTextColor(COLOR_GREEN);
    tft.setCursor(5, y);
    tft.print("SCORING CATEGORIES");
    y += lineHeight + 3;
    
    tft.setTextColor(COLOR_TEXT);
    
    // Upper Section
    tft.setCursor(5, y);
    tft.print("* Upper Section (Aces-Sixes):");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  Score the sum of the dice");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  matching that number (e.g.,");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  three 5s in 'Fives' box = 15).");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.setTextColor(COLOR_GREEN);
    tft.print("  If sum of all upper boxes is");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  63+, you get a +35 pt bonus.");
    y += lineHeight + 4;
    
    tft.setTextColor(COLOR_TEXT);
    
    // Three of a Kind
    tft.setCursor(5, y);
    tft.print("* Three of a Kind: At least");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  three dice of the same number.");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  Score: Total of all 5 dice.");
    y += lineHeight + 4;
    
    // Four of a Kind
    tft.setCursor(5, y);
    tft.print("* Four of a Kind: At least four");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  dice of the same number.");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  Score: Total of all 5 dice.");
    y += lineHeight + 4;
    
    // Full House
    tft.setCursor(5, y);
    tft.print("* Full House: Three of one");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  number and two of another.");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  Score: 25 points.");
    
    // Navigation
    tft.setTextColor(COLOR_GRAY);
    tft.setCursor(5, 305);
    tft.print("UP/DOWN: Next page  ENTER: Back");
    
  } else if(helpPage == 2) {
    // Page 3: Lower Section (Part 2) & Yahtzee Rules
    tft.setTextSize(1);
    tft.setTextColor(COLOR_GREEN);
    tft.setCursor(5, y);
    tft.print("SCORING CATEGORIES (CONT.)");
    y += lineHeight + 3;
    
    tft.setTextColor(COLOR_TEXT);
    
    // Small Straight
    tft.setCursor(5, y);
    tft.print("* Small Straight: Four");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  sequential dice (e.g., 1-2-3-4).");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  Score: 30 points.");
    y += lineHeight + 4;
    
    // Large Straight
    tft.setCursor(5, y);
    tft.print("* Large Straight: Five");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  sequential dice (e.g.,2-3-4-5-6)");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  Score: 40 points.");
    y += lineHeight + 4;
    
    // Yahtzee
    tft.setCursor(5, y);
    tft.print("* Yahtzee: Five of a kind");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  (e.g., 5-5-5-5-5).");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  Score: 50 points.");
    y += lineHeight + 4;
    
    // Chance
    tft.setCursor(5, y);
    tft.print("* Chance: Total of any five dice.");
    y += lineHeight + 6;
    
    // Yahtzee Bonus Rules
    tft.setTextColor(COLOR_GREEN);
    tft.setCursor(5, y);
    tft.print("YAHTZEE BONUS RULES");
    y += lineHeight + 3;
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, y);
    tft.print("* If you roll a Yahtzee but have");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  already filled the Yahtzee box");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  with a 50, you earn a 100-point");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  bonus and must fill in another");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  box (Joker rule).");
    y += lineHeight + 4;
    
    tft.setCursor(5, y);
    tft.print("* If you roll a Yahtzee but");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  previously scored a 0 in the");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  Yahtzee box, you do NOT get");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  the bonus.");
    
    // Navigation
    tft.setTextColor(COLOR_GRAY);
    tft.setCursor(5, 305);
    tft.print("UP/DOWN: Next page  ENTER: Back");
    
  } else {
    // Page 4: Winning
    tft.setTextSize(1);
    tft.setTextColor(COLOR_GREEN);
    tft.setCursor(5, y);
    tft.print("WINNING");
    y += lineHeight + 3;
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, y);
    tft.print("Total the scores from all 13");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("boxes, including the upper");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("section bonus and any Yahtzee");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("bonuses. The player with the");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("highest total wins.");
    y += lineHeight + 6;
    
    // Strategy Tips
    tft.setTextColor(COLOR_GREEN);
    tft.setCursor(5, y);
    tft.print("STRATEGY TIPS");
    y += lineHeight + 3;
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, y);
    tft.print("* Try to get the +35 upper bonus");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  early - it's worth it!");
    y += lineHeight + 4;
    
    tft.setCursor(5, y);
    tft.print("* Use Chance wisely - save it");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  for bad rolls.");
    y += lineHeight + 4;
    
    tft.setCursor(5, y);
    tft.print("* Don't waste high-scoring");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  categories on low rolls unless");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  you have no other choice.");
    y += lineHeight + 4;
    
    tft.setCursor(5, y);
    tft.print("* Go for Yahtzees! The 100-point");
    y += lineHeight;
    tft.setCursor(5, y);
    tft.print("  bonuses can change the game.");
    
    // Navigation
    tft.setTextColor(COLOR_GRAY);
    tft.setCursor(5, 305);
    tft.print("UP/DOWN: Next page  ENTER: Back");
  }
}

void drawGraphMenu() {
  tft.fillScreen(COLOR_BG);
  
  // Title
  tft.setTextSize(2);
  tft.setTextColor(COLOR_GREEN);
  tft.setCursor(10, 5);
  tft.print("AI PERFORMANCE");
  
  // Page indicator
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(195, 22);
  tft.print(graphPage + 1);
  tft.print("/1");  // single stats graph (Score Distribution)
  
  // Only the Score Distribution graph remains — it's the one honest, descriptive
  // view. The learning-progress / strategy-comparison / win-rate-trend graphs
  // visualised a learning process that no longer exists, so they were removed.
  drawScoreDistributionGraph();
  
  // Navigation
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GRAY);
  tft.setCursor(10, 305);
  tft.print("UP/DOWN: Pages  ENTER: Back");
}

// ============================================================================
// ENHANCED AI ANALYSIS GRAPHS (V39)
// ============================================================================

// Graph 0: Learning Progress Over Time
void drawScoreDistributionGraph() {
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(10, 35);
  tft.print("Score Distrib.");
  
  if(aiLearning.gamesPlayed == 0) {
    tft.setTextSize(2);
    tft.setTextColor(COLOR_GRAY);
    tft.setCursor(15, 100);
    tft.print("No games");
    tft.setCursor(15, 125);
    tft.print("played yet");
    return;
  }
  
  // Find max count for scaling
  uint16_t maxCount = 1;
  for(int i = 0; i < 10; i++) {
    if(aiLearning.scoreRanges[i] > maxCount) {
      maxCount = aiLearning.scoreRanges[i];
    }
  }
  
  // Histogram bars
  int barWidth = 22;
  int startX = 10;
  int startY = 210;
  int maxBarHeight = 130;
  
  tft.setTextSize(1);
  for(int i = 0; i < 10; i++) {
    int x = startX + i * barWidth;
    int count = aiLearning.scoreRanges[i];
    int barHeight = (int)((float)count / maxCount * maxBarHeight);
    
    // Color code by score range
    uint16_t barColor;
    if(i >= 8) barColor = COLOR_GREEN;        // 400+
    else if(i >= 6) barColor = tft.color565(0, 255, 255);  // 300-399 Cyan
    else if(i >= 4) barColor = tft.color565(255, 255, 0);  // 200-299 Yellow
    else if(i >= 2) barColor = tft.color565(255, 165, 0);  // 100-199 Orange
    else barColor = COLOR_RED;                // 0-99
    
    // Draw bar
    if(barHeight > 0) {
      tft.fillRect(x, startY - barHeight, barWidth - 2, barHeight, barColor);
      tft.drawRect(x, startY - barHeight, barWidth - 2, barHeight, COLOR_TEXT);
      
      // Draw count on top if space and count > 0
      if(count > 0 && barHeight > 12) {
        tft.setCursor(x + 2, startY - barHeight + 2);
        tft.setTextColor(COLOR_BG);
        tft.print(count);
      }
    }
  }
  
  // X-axis labels (score ranges)
  tft.setTextColor(COLOR_TEXT);
  int labelY = startY + 10;
  
  tft.setCursor(5, labelY);
  tft.print("0");
  tft.setCursor(startX + 2 * barWidth - 3, labelY);
  tft.print("100");
  tft.setCursor(startX + 4 * barWidth - 3, labelY);
  tft.print("200");
  tft.setCursor(startX + 6 * barWidth - 3, labelY);
  tft.print("300");
  tft.setCursor(startX + 8 * barWidth, labelY);
  tft.print("400+");
  
  // Statistics
  tft.setCursor(5, labelY + 20);
  tft.setTextColor(tft.color565(0, 255, 255)); // Cyan
  tft.print("Total: ");
  tft.setTextColor(COLOR_TEXT);
  tft.print(aiLearning.gamesPlayed);
  
  // Calculate average score
  uint32_t totalScore = 0;
  uint32_t totalGames = 0;
  for(int i = 0; i < 10; i++) {
    int midpoint = (i * 50) + 25;  // Midpoint of range
    totalScore += aiLearning.scoreRanges[i] * midpoint;
    totalGames += aiLearning.scoreRanges[i];
  }
  
  if(totalGames > 0) {
    int avgScore = totalScore / totalGames;
    tft.setCursor(115, labelY + 20);
    tft.setTextColor(tft.color565(0, 255, 255)); // Cyan
    tft.print("Avg: ");
    tft.setTextColor(COLOR_TEXT);
    tft.print(avgScore);
  }
}

// Graph 4: Category Efficiency
void drawStatistics() {
  tft.fillScreen(COLOR_BG);
  
  // Title
  tft.setTextSize(3);
  tft.setTextColor(COLOR_GREEN);
  tft.setCursor(20, 20);
  tft.print("STATISTICS");
  
  // High Score
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(10, 60);
  tft.print("High Score:");
  tft.setCursor(180, 60);
  tft.setTextColor(COLOR_GREEN);
  tft.print(highScore);
  
  // Win/Loss Record
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(10, 95);
  tft.print("Win Record:");
  
  tft.setCursor(20, 120);
  tft.setTextColor(COLOR_P1);
  tft.print("P1: ");
  tft.setTextColor(COLOR_TEXT);
  tft.print(p1Wins);
  
  tft.setCursor(130, 120);
  tft.setTextColor(COLOR_P2);
  tft.print("P2: ");
  tft.setTextColor(COLOR_TEXT);
  tft.print(p2Wins);
  
  tft.setCursor(20, 145);
  tft.setTextColor(COLOR_GRAY);
  tft.print("Ties: ");
  tft.setTextColor(COLOR_TEXT);
  tft.print(totalTies);
  
  // Most Yahtzees
  tft.setCursor(10, 180);
  tft.setTextColor(COLOR_TEXT);
  tft.print("Most Yahtzees:");
  tft.setCursor(180, 180);
  tft.setTextColor(COLOR_RED);
  tft.print(mostYahtzees);
  
  // Total Games
  int totalGames = p1Wins + p2Wins + totalTies;
  tft.setCursor(10, 215);
  tft.setTextColor(COLOR_TEXT);
  tft.print("Total Games:");
  tft.setCursor(180, 215);
  tft.print(totalGames);
  
  // AI Learning Stats (if any AI games played)
  if(aiLearning.gamesPlayed > 0) {
    tft.setCursor(10, 240);
    tft.setTextColor(COLOR_P2);
    tft.print("AI Games:");
    tft.setCursor(180, 240);
    tft.print(aiLearning.gamesPlayed);
    
    // AI Win Rate
    int aiWins = aiLearning.aggressiveWins + aiLearning.conservativeWins;
    tft.setCursor(10, 265);
    tft.setTextColor(COLOR_TEXT);
    tft.print("AI Win Rate:");
    tft.setCursor(180, 265);
    int winRate = (aiWins * 100) / aiLearning.gamesPlayed;
    tft.print(winRate);
    tft.print("%");
  }
  
  // Instructions — anchored at bottom, size-1 text (8px tall), last line ends at y=308+8=316
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GRAY);
  tft.setCursor(10, 290);
  tft.print("ENTER: Back");
  
  tft.setCursor(10, 300);
  tft.print("Hold ROLL 3s: Reset stats");
  
  tft.setCursor(10, 310);
  tft.print("(AI stats in Tools menu)");
}

void drawAIStatistics() {
  tft.fillScreen(COLOR_BG);

  // ── Title ────────────────────────────────────────────────────────────────
  tft.setTextSize(2);
  tft.setTextColor(COLOR_GREEN);
  tft.setCursor(65, 8);
  tft.print("AI STATS");
  tft.drawFastHLine(5, 30, 230, COLOR_GRAY);

  uint16_t games = aiLearning.gamesPlayed;
  uint16_t aiW   = aiLearning.aggressiveWins;
  uint16_t huW   = aiLearning.humanWins;
  uint16_t ties  = (games > aiW + huW) ? (games - aiW - huW) : 0;

  if(games == 0) {
    tft.setTextSize(2);
    tft.setTextColor(COLOR_GRAY);
    tft.setCursor(5, 90);
    tft.print("No games yet");
    tft.setTextSize(1);
    tft.setCursor(5, 120);
    tft.print("Play the AI to build stats.");
  } else {
    // ── Games + record ─────────────────────────────────────────────────────
    tft.setTextSize(2);
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 40);
    tft.print("Games: ");
    tft.setTextColor(COLOR_GREEN);
    tft.print(games);

    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 62);
    tft.print("AI ");
    tft.setTextColor(COLOR_GREEN);
    tft.print(aiW);
    tft.setTextColor(COLOR_TEXT);
    tft.print(" You ");
    tft.setTextColor(COLOR_HELD);
    tft.print(huW);
    if(ties > 0) {
      tft.setTextColor(COLOR_TEXT);
      tft.print(" T ");
      tft.setTextColor(COLOR_GRAY);
      tft.print(ties);
    }

    // ── Comparison table ───────────────────────────────────────────────────
    const int colL = 5, colA = 108, colY = 176;
    tft.setTextSize(2);
    tft.setTextColor(COLOR_GRAY);
    tft.setCursor(colA, 92);  tft.print("AI");
    tft.setCursor(colY, 92);  tft.print("YOU");
    tft.drawFastHLine(5, 110, 230, COLOR_GRAY);

    int aiAvg = (int)(aiLearning.aiScoreSum / games);
    int huAvg = (int)(aiLearning.humanScoreSum / games);
    int aiBon = (aiLearning.aiBonusGames    * 100) / games;
    int huBon = (aiLearning.humanBonusGames * 100) / games;
    int aiYtz = (aiLearning.aiYahtzeeGames    * 100) / games;
    int huYtz = (aiLearning.humanYahtzeeGames * 100) / games;

    // Avg — highlight whoever is higher
    tft.setTextColor(COLOR_TEXT); tft.setCursor(colL, 118); tft.print("Avg");
    tft.setTextColor(aiAvg >= huAvg ? COLOR_GREEN : COLOR_TEXT);
    tft.setCursor(colA, 118); tft.print(aiAvg);
    tft.setTextColor(huAvg >  aiAvg ? COLOR_HELD  : COLOR_TEXT);
    tft.setCursor(colY, 118); tft.print(huAvg);

    tft.setTextColor(COLOR_TEXT); tft.setCursor(colL, 142); tft.print("Best");
    tft.setTextColor(COLOR_GREEN); tft.setCursor(colA, 142); tft.print(aiLearning.aiHighScore);
    tft.setTextColor(COLOR_HELD);  tft.setCursor(colY, 142); tft.print(aiLearning.humanHighScore);

    tft.setTextColor(COLOR_TEXT); tft.setCursor(colL, 166); tft.print("Bonus");
    tft.setTextColor(COLOR_GREEN); tft.setCursor(colA, 166); tft.print(aiBon); tft.print("%");
    tft.setTextColor(COLOR_HELD);  tft.setCursor(colY, 166); tft.print(huBon); tft.print("%");

    tft.setTextColor(COLOR_TEXT); tft.setCursor(colL, 190); tft.print("Ytz");
    tft.setTextColor(COLOR_GREEN); tft.setCursor(colA, 190); tft.print(aiYtz); tft.print("%");
    tft.setTextColor(COLOR_HELD);  tft.setCursor(colY, 190); tft.print(huYtz); tft.print("%");

    tft.drawFastHLine(5, 214, 230, COLOR_GRAY);

    // ── Optimal reference (health check) ───────────────────────────────────
    // The AI is a fixed optimal policy: over enough games it MUST converge on
    // these. A persistent gap means the brain is broken, not unlucky.
    tft.setTextSize(1);
    tft.setTextColor(COLOR_GRAY);
    tft.setCursor(5, 222);
    tft.print("Optimal ref: avg ");
    tft.print(YZ_REF_AVG);
    tft.print("  bonus ");
    tft.print(YZ_REF_BONUS);
    tft.print("%  ytz ");
    tft.print(YZ_REF_YTZ);
    tft.print("%");

    tft.setCursor(5, 234);
    if(games < 100) {
      tft.setTextColor(COLOR_GRAY);
      tft.print("Needs ~100 games to be meaningful");
    } else {
      int d = aiAvg - YZ_REF_AVG; if(d < 0) d = -d;
      tft.setTextColor(d <= 10 ? COLOR_GREEN : COLOR_RED);
      tft.print(d <= 10 ? "AI matches optimal reference" : "AI OFF reference - check brain");
    }

    // ── Recent form (noisy — small print on purpose) ───────────────────────
    int rc = aiLearning.recentCount;
    if(rc > 0) {
      long aiSum = 0, huSum = 0;
      for(int i = 0; i < rc; i++) { aiSum += aiLearning.recentAIScores[i]; huSum += aiLearning.recentHumanScores[i]; }
      tft.setTextColor(COLOR_TEXT);
      tft.setCursor(5, 250);
      tft.print("Last ");
      tft.print(rc);
      tft.print(": AI ");
      tft.setTextColor(COLOR_GREEN);
      tft.print((int)(aiSum / rc));
      tft.setTextColor(COLOR_TEXT);
      tft.print("  You ");
      tft.setTextColor(COLOR_HELD);
      tft.print((int)(huSum / rc));

      int li = (aiLearning.recentHead + 9) % 10;
      tft.setTextColor(COLOR_TEXT);
      tft.setCursor(5, 262);
      tft.print("Last game: AI ");
      tft.setTextColor(COLOR_GREEN);
      tft.print(aiLearning.recentAIScores[li]);
      tft.setTextColor(COLOR_TEXT);
      tft.print("  You ");
      tft.setTextColor(COLOR_HELD);
      tft.print(aiLearning.recentHumanScores[li]);
    }
  }

  // ── Footer ───────────────────────────────────────────────────────────────
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GRAY);
  tft.setCursor(5, 290);
  tft.print("ENTER: Back");
  tft.setCursor(5, 305);
  tft.print("Hold ENTER 3s: Reset AI data");
}  // end drawAIStatistics()

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  // **EMERGENCY TEST - CONTINUOUS BEEPS**
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Quick startup beep (reduced from 3 to 1)
  digitalWrite(BUZZER_PIN, HIGH);
  delay(50);
  digitalWrite(BUZZER_PIN, LOW);
  delay(50);
  
  Serial.begin(115200);
  
  // **REMOVED 5 SECOND DELAY FOR FASTER STARTUP**
  // If you need Serial Monitor debugging, uncomment the line below:
  // delay(5000);
  
  Serial.flush();

  // Initialize EEPROM
  int requiredSize = 4096;  // 4KB for RP2040
  EEPROM.begin(requiredSize);

  // **V42: EEPROM USAGE DIAGNOSTIC**
  Serial.println("");
  Serial.println("╔════════════════════════════════════════════╗");
  Serial.println("║    YAHTZEE v42.5 - EEPROM DIAGNOSTICS     ║");
  Serial.println("╚════════════════════════════════════════════╝");
  Serial.print("AILearningData size: ");
  Serial.print(sizeof(AILearningData));
  Serial.println(" bytes");
  Serial.println("");
  Serial.println("EEPROM Memory Map:");
  Serial.println("  0-14:    Settings (15 bytes)");
  Serial.print("  1000-");
  Serial.print(EEPROM_STRATEGY_VARIANTS_ADDR - 1);
  Serial.print(": AI Learning (");
  Serial.print(EEPROM_STRATEGY_VARIANTS_ADDR - EEPROM_AI_LEARNING_START);
  Serial.println(" bytes)");
  Serial.print("Total EEPROM used: ~");
  Serial.print(EEPROM_STRATEGY_VARIANTS_ADDR);
  Serial.print(" / ");
  Serial.print(requiredSize);
  Serial.println(" bytes");
  
  size_t aiSpace = EEPROM_STRATEGY_VARIANTS_ADDR - EEPROM_AI_LEARNING_START;
  size_t aiUsed = sizeof(AILearningData);
  if(aiUsed > aiSpace) {
    Serial.println("");
    Serial.println("⚠️  WARNING: AI data overflow detected!");
    Serial.print("    Overflow: ");
    Serial.print(aiUsed - aiSpace);
    Serial.println(" bytes");
  } else {
    Serial.print("✓ AI data fits (");
    Serial.print(aiSpace - aiUsed);
    Serial.println(" bytes free)");
  }
  Serial.println("════════════════════════════════════════════");
  Serial.println("");

  // Load AI learning data
  loadAILearningFromFile();

  // Load settings from EEPROM
  int savedBrightness = EEPROM.read(EEPROM_BRIGHTNESS_ADDR);
  if(savedBrightness >= 0 && savedBrightness <= 15) {
    max7219Brightness = savedBrightness;
  } else {
    max7219Brightness = 8;
    EEPROM.write(EEPROM_BRIGHTNESS_ADDR, max7219Brightness);
  }
  
  int savedSound = EEPROM.read(EEPROM_SOUND_ADDR);
  if(savedSound == 0 || savedSound == 1) {
    soundEnabled = (savedSound == 1);
  } else {
    soundEnabled = true;
    EEPROM.write(EEPROM_SOUND_ADDR, 1);
  }
  
  // Load volume setting
  int savedVolume = EEPROM.read(EEPROM_VOLUME_ADDR);
  if(savedVolume >= 1 && savedVolume <= 3) {
    volume = savedVolume;
  } else {
    volume = 2;  // Default to medium volume
    EEPROM.write(EEPROM_VOLUME_ADDR, volume);
  }
  
  // Load TFT brightness setting
  int savedTftBrightness = EEPROM.read(EEPROM_TFT_BRIGHTNESS_ADDR);
  if(savedTftBrightness >= 1 && savedTftBrightness <= 10) {
    tftBrightness = savedTftBrightness;
  } else {
    tftBrightness = 10;  // Default to 100% brightness
    EEPROM.write(EEPROM_TFT_BRIGHTNESS_ADDR, tftBrightness);
  }
  
  // Load auto-advance setting
  int savedAutoAdvance = EEPROM.read(EEPROM_AUTO_ADVANCE_ADDR);
  if(savedAutoAdvance == 0 || savedAutoAdvance == 1) {
    autoAdvance = (savedAutoAdvance == 1);
  } else {
    autoAdvance = false;  // Default to off
    EEPROM.write(EEPROM_AUTO_ADVANCE_ADDR, 0);
  }
  
  // Load game statistics
  highScore = (EEPROM.read(EEPROM_HIGH_SCORE_ADDR) << 8) | EEPROM.read(EEPROM_HIGH_SCORE_ADDR + 1);
  p1Wins = (EEPROM.read(EEPROM_P1_WINS_ADDR) << 8) | EEPROM.read(EEPROM_P1_WINS_ADDR + 1);
  p2Wins = (EEPROM.read(EEPROM_P2_WINS_ADDR) << 8) | EEPROM.read(EEPROM_P2_WINS_ADDR + 1);
  totalTies = (EEPROM.read(EEPROM_TIES_ADDR) << 8) | EEPROM.read(EEPROM_TIES_ADDR + 1);
  mostYahtzees = EEPROM.read(EEPROM_MOST_YAHTZEES_ADDR);
  
  // Load AI difficulty setting
  int savedDifficulty = EEPROM.read(EEPROM_AI_DIFFICULTY_ADDR);
  if(savedDifficulty >= 0 && savedDifficulty <= 2) {
    aiDifficulty = savedDifficulty;
  } else {
    aiDifficulty = 1;  // Default to Hard
    EEPROM.write(EEPROM_AI_DIFFICULTY_ADDR, aiDifficulty);
  }

  int savedSpeed = EEPROM.read(EEPROM_AI_SPEED_ADDR);
  if(savedSpeed >= 0 && savedSpeed <= 3) {
    aiSpeed = savedSpeed;
  } else {
    aiSpeed = 2;
    EEPROM.write(EEPROM_AI_SPEED_ADDR, aiSpeed);
  }
  
  // Set AI personality based on difficulty.
  // Each difficulty has its own aggressivenessWeight stored independently in EEPROM
  // so playing Normal never contaminates Hard's learned weight, and vice versa.
  //
  // Weight ranges per mode:  Normal 0.25–0.55  |  Hard 0.55–0.85  |  God fixed 0.95
  // These ranges are non-overlapping so difficulty separation is always preserved.
  // Learning can shift within the band but cannot push a mode into another's range.
  //
  // Base difficulty margins (applied on top of weight offset in the EV engine):
  //   Normal:  0.0   Hard: -2.5   God: -5.0
  // This ensures God is always the most aggressive, Hard clearly harder than Normal.
  {
    // Load the saved per-mode weight from EEPROM (stored as uint8 * 100)
    uint8_t savedRaw = EEPROM.read(EEPROM_AGGR_WEIGHT_ADDR + aiDifficulty);

    if(aiDifficulty == 2) {
      // God Mode — fixed, no learning variance
      aggressivenessWeight = 0.95f;

    } else if(aiDifficulty == 1) {
      // Hard — range 0.55–0.85, default 0.70
      float defaultWeight = 0.70f;
      if(savedRaw >= 55 && savedRaw <= 85) {
        aggressivenessWeight = savedRaw / 100.0f;
      } else {
        aggressivenessWeight = defaultWeight + (random(0, 10) / 100.0f);
      }
      // Apply learning: shift weight within Hard's band based on Hard-mode win data
      if(aiLearning.gamesPlayed > 10) {
        float aggressiveSuccessRate = (float)aiLearning.aggressiveWins / aiLearning.gamesPlayed;
        float conservativeSuccessRate = (float)aiLearning.conservativeWins / aiLearning.gamesPlayed;
        if(aggressiveSuccessRate > conservativeSuccessRate + 0.1f) {
          aggressivenessWeight = min(0.85f, aggressivenessWeight + 0.05f);
        } else if(conservativeSuccessRate > aggressiveSuccessRate + 0.1f) {
          aggressivenessWeight = max(0.55f, aggressivenessWeight - 0.05f);
        }
      }

    } else {
      // Normal — range 0.25–0.55, default 0.40
      float defaultWeight = 0.40f;
      if(savedRaw >= 25 && savedRaw <= 55) {
        aggressivenessWeight = savedRaw / 100.0f;
      } else {
        aggressivenessWeight = defaultWeight + (random(0, 10) / 100.0f);
      }
      // Apply learning: shift weight within Normal's band
      if(aiLearning.gamesPlayed > 10) {
        float aggressiveSuccessRate = (float)aiLearning.aggressiveWins / aiLearning.gamesPlayed;
        float conservativeSuccessRate = (float)aiLearning.conservativeWins / aiLearning.gamesPlayed;
        if(aggressiveSuccessRate > conservativeSuccessRate + 0.1f) {
          aggressivenessWeight = min(0.55f, aggressivenessWeight + 0.05f);
        } else if(conservativeSuccessRate > aggressiveSuccessRate + 0.1f) {
          aggressivenessWeight = max(0.25f, aggressivenessWeight - 0.05f);
        }
      }
    }
  }
  
  // Initialize hardware
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  // Startup tune moved to initIntroAnimation() to sync with dice falling
  
  for(int i = 0; i < 5; i++) {
    pinMode(holdButtons[i], INPUT_PULLUP);
  }
  pinMode(rollButton, INPUT_PULLUP);
  pinMode(upButton, INPUT_PULLUP);
  pinMode(downButton, INPUT_PULLUP);
  pinMode(enterButton, INPUT_PULLUP);
  
  max7219Init();
  
  SPI1.setRX(12);
  SPI1.setTX(11);
  SPI1.setSCK(10);
  SPI1.begin();
  tft.init(240, 320);
  tft.setSPISpeed(40000000);
  tft.setRotation(2);
  tft.fillScreen(COLOR_BG);
  
  // Seed random (optimized for faster startup)
  uint32_t seed = 0;
  for(int i = 0; i < 8; i++) {  // Reduced from 32 to 8 iterations
    seed ^= (analogRead(26) << i);
    // No delay needed - analogRead is already slow enough
  }
  seed ^= millis();
  randomSeed(seed);
  
  initializeGame();
  drawScreen();
  
  EEPROM.commit();
  
  Serial.println("✓ Yahtzee Ready");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  static unsigned long lastButtonCheck = 0;
  static unsigned long lastAnimFrame = 0;
  
  // Update intro animation at 60fps
  if(gameState == STATE_INTRO) {
    static bool startupMusicPlayed = false;
    static unsigned long introStartTime = 0;
    
    if(introStartTime == 0) {
      introStartTime = millis();
    }
    
    if(millis() - lastAnimFrame > 16) {  // ~60fps
      lastAnimFrame = millis();
      
      // Play startup music after 100ms delay (so dice are visible and falling)
      if(!startupMusicPlayed && millis() - introStartTime > 100) {
        buzzerStartupTune();
        startupMusicPlayed = true;
      }
      
      updateIntroAnimation();
      
      // Always redraw if not complete (to show title animation)
      if(!introComplete) {
        drawIntroScreen();
      } else {
        // Transition to menu after a short pause so YAHTZEE is readable
        static unsigned long completeTime = 0;
        static bool drewComplete = false;
        
        if(!drewComplete) {
          drawIntroScreen();
          completeTime = millis();
          drewComplete = true;
        } else if(millis() - completeTime > 600) {
          // After 600ms, go to menu
          gameState = STATE_MENU;
          startupMusicPlayed = false;
          introStartTime = 0;
          drewComplete = false;   // Reset for next time
          completeTime = 0;
          drawScreen();
        }
      }
    }
  }

  // Update dice animation
  if(diceAnimating) {
    updateDiceAnimation();
    delay(16);  // ~60fps for smooth animation
  }
  
  // Update celebration particles
  // CRITICAL: Never run celebration once game is over — it overwrites the scorecard
  if(celebrationActive && gameState == STATE_GAME_OVER) {
    celebrationActive = false;  // Kill any active celebration when game ends
  }
  if(celebrationActive) {
    updateCelebration();
    drawCelebration();  // Draw clean celebration screen with particles
    delay(16);  // ~60fps for smooth animation
  }
  
  // Continuously animate the game over screen (winner badge floats each frame)
  static unsigned long lastGameOverFrame = 0;
  if(gameState == STATE_GAME_OVER && !celebrationActive && millis() - lastGameOverFrame > 16) {
    lastGameOverFrame = millis();
    drawGameOver();
  }
  
  // Handle computer turn - trigger automatically when it's Player 2's turn
  // **CRITICAL**: Don't run computer turn during celebrations!
  if(vsComputer && currentPlayer == 2 && 
     gameState != STATE_GAME_OVER && gameState != STATE_INTRO && gameState != STATE_MENU &&
     !celebrationActive) {  // NEW: Don't interrupt celebrations
    computerTurn();
  }
  
  // Check buttons every 100ms (only for human player)
  if(millis() - lastButtonCheck > 100) {
    lastButtonCheck = millis();
    if(!computerIsThinking) {
      // Full button processing for human player
      checkButtons();
    } else {
      // During AI turn, still allow UP/DOWN for viewing scores
      checkViewButtons();
    }
    lastButtonCheck = millis();
  }
  
  // Check for game over
  if(gameOver && gameState != STATE_GAME_OVER) {
    gameState = STATE_GAME_OVER;
    drawScreen();
  }
}

// ============================================================================
// GAME INITIALIZATION
// ============================================================================

void initializeGame() {
  // Initialize dice (blank)
  for(int i = 0; i < 5; i++) {
    dice[i] = 0;  // 0 = blank
    held[i] = false;
  }
  
  // Initialize scores (-1 = unused)
  for(int i = 0; i < 13; i++) {
    scores1[i] = -1;
    scores2[i] = -1;
  }
  
  // Reset game state
  currentPlayer = 1;
  turn = 1;
  playerTurn = 1;
  rollsLeft = 3;
  rollsUsedThisTurn = 0;  // Reset roll counter
  gameOver = false;
  gameState = STATE_INTRO;
  selectedCategory = 0;
  menuScroll = 0;
  winnerCelebrationShown = false;  // Reset for new game
  currentGameYahtzees1 = 0;  // Reset Yahtzee counter
  currentGameYahtzees2 = 0;  // Reset Yahtzee counter
  statsUpdated = false;
  winnerFlashTimer = 0;      // Reset flash timer
  winnerFlashState = true;   // Reset flash state
  gameOverAnimationStarted = false;  // Reset game over animation
  gameOverStartTime = 0;
  winnerTextScale = 0.0;
  winnerColorHue = 0;
  // Reset per-game decision tracking flags
  aiChanceBucket   = -1;
  aiTurn10Recorded = false;
  aiAheadAtTurn10  = false;
  for(int s = 0; s < 6; s++) aiSacrificedSlot[s] = false;
  aiChanceVsUpperLastChoice = -1;  // -1=no decision, 0=chose upper, 1=chose Chance
  aiLastScoredCategory = -1;  // Reset score highlight
  for(int r = 0; r < 4; r++) aiRollCountThisGame[r] = 0;  // Reset per-game roll counter
  
  // Determine AI strategy for this game based on aggressiveness weight
// NEW: Epsilon-Greedy Exploration (10% of games)
float explorationRate = 0.10;  // 10% exploration
currentGameIsExploration = false;
  
if(vsComputer) {
  if(random(100) < explorationRate * 100) {
    // EXPLORATION: Try a random strategy
    currentGameIsExploration = true;
    currentGameStrategy = random(0, 2);  // Random: 0=conservative, 1=aggressive
    
    // Also randomize aggressiveness weight for this game
    aggressivenessWeight = random(30, 90) / 100.0;  // 0.3 to 0.9
    
  } else {
    // EXPLOITATION: Use learned strategy
    currentGameStrategy = (aggressivenessWeight > 0.6) ? 1 : 0;
  }
}

  initIntroAnimation();
  
  updateDiceDisplay();
}

// ============================================================================
// BUTTON HANDLING
// ============================================================================

// ============================================================================
// VIEW BUTTONS - Minimal button handling during AI turns
// ============================================================================

void checkViewButtons() {
  static bool lastUp = HIGH;
  static bool lastDown = HIGH;
  
  bool upBtn = digitalRead(upButton);
  bool downBtn = digitalRead(downButton);
  
  // UP button - toggle between upper and lower section view during AI turn
  if(lastUp == HIGH && upBtn == LOW) {
    if(gameState == STATE_ROLLING || gameState == STATE_CATEGORY_SELECT) {
      scoreViewSection = 1 - scoreViewSection;  // Toggle between 0 and 1
      drawScreen();
      buzzerMenuMove();
    }
  }
  lastUp = upBtn;
  
  // DOWN button - toggle between upper and lower section view during AI turn
  if(lastDown == HIGH && downBtn == LOW) {
    if(gameState == STATE_ROLLING || gameState == STATE_CATEGORY_SELECT) {
      scoreViewSection = 1 - scoreViewSection;  // Toggle between 0 and 1
      drawScreen();
      buzzerMenuMove();
    }
  }
  lastDown = downBtn;
}

// ============================================================================
// BUTTON HANDLING
// ============================================================================

void checkButtons() {
  static bool lastRoll = HIGH;
  static bool lastUp = HIGH;
  static bool lastDown = HIGH;
  static bool lastEnter = HIGH;
  static bool lastHold[5] = {HIGH, HIGH, HIGH, HIGH, HIGH};
  
  // Read current button states
  bool rollBtn = digitalRead(rollButton);
  bool upBtn = digitalRead(upButton);
  bool downBtn = digitalRead(downButton);
  bool enterBtn = digitalRead(enterButton);
  
  // Brightness adjustment in tools menu
  if(gameState == STATE_MENU && inToolsMenu && selectedToolItem == 0) {
    // Roll button decreases brightness
    if(lastRoll == HIGH && rollBtn == LOW) {
      max7219SetBrightness(max7219Brightness - 1);
      drawScreen();
      updateDiceDisplay();
    }
    
    // Hold button 1 increases brightness
    static bool lastHold0 = HIGH;
    bool hold0Btn = digitalRead(holdButtons[0]);
    if(lastHold0 == HIGH && hold0Btn == LOW) {
      max7219SetBrightness(max7219Brightness + 1);
      drawScreen();
      updateDiceDisplay();
    }
    lastHold0 = hold0Btn;
  }

  // Roll button - roll dice or start turn
  if(lastRoll == HIGH && rollBtn == LOW) {
    if(gameState == STATE_MENU) {
      if(inAIDifficultyMenu) {
        // ROLL button goes back to main menu from AI difficulty selection
        buzzerMenuSelect();
        inAIDifficultyMenu = false;
        vsComputer = false;  // Cancel vs Computer selection
        drawScreen();
      }
      // Roll button does nothing in main menu - use ENTER instead
    } else if(gameState == STATE_GAME_OVER) {
      // Restart - go directly to menu (skip intro)
      buzzerBeep(100);
      
      // COMPLETE game state reset
      gameState = STATE_MENU;
      selectedMenuItem = 0;
      inToolsMenu = false;
      inSoundMenu = false;
      adjustingVolume = false;
      vsComputer = false;
      gameOver = false;
      winnerCelebrationShown = false;
      celebrationActive = false;
      celebrationType = 0;
      winnerFlashTimer = 0;    // Reset flash timer
      winnerFlashState = true; // Reset flash state
      
      // Reset all game variables
      currentPlayer = 1;
      turn = 1;
      playerTurn = 1;
      rollsLeft = 3;
      selectedCategory = 0;
      menuScroll = 0;
      scoreViewSection = 0;
      
      // Reset current game Yahtzee counters
      currentGameYahtzees1 = 0;
      currentGameYahtzees2 = 0;
      
      statsUpdated = false;  // Reset for next game
      
      // Reset scores
      for(int i = 0; i < 13; i++) {
        scores1[i] = -1;
        scores2[i] = -1;
      }
      yahtzeeBonus1 = 0;
      yahtzeeBonus2 = 0;
      
      // Reset dice display
      for(int i = 0; i < 5; i++) {
        dice[i] = 0;
        held[i] = false;
      }
      
      // Reset computer AI state
      computerIsThinking = false;
      computerThinkPhase = 0;
      
      // Reset animation states
      diceAnimating = false;
      
      updateDiceDisplay();
      drawScreen();
      
    }else if(gameState == STATE_INTRO && introComplete) {
      // Start rolling immediately, skip STATE_START
      gameState = STATE_ROLLING;
      rollsLeft--;
      rollDice();
      blankDiceDisplay();
      drawScreen();
      
      for(int i = 0; i < 5; i++) {
      }
      } else if(gameState == STATE_START) {
      // Start the turn
      gameState = STATE_ROLLING;
      rollsLeft--;  // Decrement BEFORE rolling
      rollDice();
      blankDiceDisplay();  // Blank non-held dice; animation fills them in
      
      // Auto-enter category selection if no rolls left
      if(rollsLeft == 0) {
        int* scores = (currentPlayer == 1) ? scores1 : scores2;
        selectedCategory = findBestCategory(dice, scores);
        if(selectedCategory == -1) selectedCategory = 0;
        
        gameState = STATE_CATEGORY_SELECT;
      }
      
      // **FIX: Always draw on first roll, delay only on final roll**
      if(rollsLeft > 0 || !diceAnimating) {
        drawScreen();
      }
      
      for(int i = 0; i < 5; i++) {
      }
    } else if(gameState == STATE_ROLLING && rollsLeft > 0) {
      rollsLeft--;  // Decrement BEFORE rolling
      rollDice();
      blankDiceDisplay();  // Blank non-held dice; animation fills them in
      
      // Auto-enter category selection if no rolls left
      if(rollsLeft == 0) {
        int* scores = (currentPlayer == 1) ? scores1 : scores2;
        selectedCategory = findBestCategory(dice, scores);
        if(selectedCategory == -1) selectedCategory = 0;
        
        gameState = STATE_CATEGORY_SELECT;
      }
      
      // **FIX: Always draw on rolls 1-2, delay only on final roll (roll 3)**
      if(rollsLeft > 0 || !diceAnimating) {
        drawScreen();
      }
      
      for(int i = 0; i < 5; i++) {
      }
    }
  }
  lastRoll = rollBtn;
  
  // Up button - navigate menu or toggle view or increase brightness
  if(lastUp == HIGH && upBtn == LOW) {
    if(gameState == STATE_MENU) {
      if(inAIDifficultyMenu) {
        // Navigate AI difficulty menu up
        if(selectedAIDifficulty > 0) {
          selectedAIDifficulty--;
        } else {
          selectedAIDifficulty = 2;  // Wrap to bottom (God Mode)
        }
        buzzerMenuMove();
        drawAIDifficultyMenu();
      } else if(inSoundMenu) {
        if(adjustingVolume) {
          // Currently adjusting volume - UP increases volume
          if(volume < 3) {
            volume++;
            EEPROM.write(EEPROM_VOLUME_ADDR, volume);
            EEPROM.commit();
            buzzerMenuMove();  // Test sound at new volume
          }
          drawSoundMenuItems();
        } else {
          // Navigate sound menu up
          if(selectedSoundItem > 0) {
            selectedSoundItem--;
          } else {
            selectedSoundItem = 2;  // Wrap to bottom
          }
          buzzerMenuMove();
          drawSoundMenuItems();
        }
      } else if(inToolsMenu) {
        if(adjustingBrightness) {
          // Currently adjusting 7-seg brightness - UP increases brightness
          max7219SetBrightness(max7219Brightness + 1);
          drawToolsMenuItems();  // Only redraw menu items
          updateDiceDisplay();
        } else if(inGraphMenu) {
          // UP button — single graph now, stay on page 0
          graphPage = 0;
          drawGraphMenu();
          buzzerMenuMove();
        } else if(inHelpMenu) {
          // Navigate help pages up (with wrapping)
          if(helpPage > 0) {
            helpPage--;
          } else {
            helpPage = 3;  // Wrap to last page (page 4)
          }
          drawHelpMenu();
          buzzerMenuMove();
        } else {
          // Navigate tools menu up (no paging needed - all items fit on screen)
          if(selectedToolItem > 0) {
            selectedToolItem--;
            drawToolsMenuItems();  // Only redraw menu items
          } else {
            selectedToolItem = 10;  // Wrap to bottom (last item — 11 items, 0-10)
            drawToolsMenuItems();  // Only redraw menu items
          }
          buzzerMenuMove();  // Menu navigation sound
        }
      } else {
        // Navigate main menu up
        if(selectedMenuItem > 0) {
          selectedMenuItem--;
        } else {
          selectedMenuItem = 2;  // Wrap to bottom
        }
        buzzerMenuMove();  // Menu navigation sound
        drawMenuItems();  // Only redraw menu items
      }
    } else if(gameState == STATE_ROLLING) {
      // Toggle between upper and lower section view
      scoreViewSection = 1 - scoreViewSection;  // Toggles between 0 and 1
      drawScreen();
    } else if(gameState == STATE_CATEGORY_SELECT) {
      int* scores = (currentPlayer == 1) ? scores1 : scores2;
      int originalCategory = selectedCategory;
      
      // Move up and skip used categories (wrap around to bottom)
      do {
        if(selectedCategory > 0) {
          selectedCategory--;
        } else {
          selectedCategory = 12;  // Wrap to bottom (Chance)
        }
      } while(scores[selectedCategory] != -1 && selectedCategory != originalCategory);
      
      drawScreen();
    }
  }
  lastUp = upBtn;
  
  // Down button - navigate menu or toggle view or decrease brightness
  if(lastDown == HIGH && downBtn == LOW) {
    if(gameState == STATE_MENU) {
      if(inAIDifficultyMenu) {
        // Navigate AI difficulty menu down
        if(selectedAIDifficulty < 2) {
          selectedAIDifficulty++;
        } else {
          selectedAIDifficulty = 0;  // Wrap to top (Normal)
        }
        buzzerMenuMove();
        drawAIDifficultyMenu();
      } else if(inSoundMenu) {
        if(adjustingVolume) {
          // Currently adjusting volume - DOWN decreases volume
          if(volume > 1) {
            volume--;
            EEPROM.write(EEPROM_VOLUME_ADDR, volume);
            EEPROM.commit();
            buzzerMenuMove();  // Test sound at new volume
          }
          drawSoundMenuItems();
        } else {
          // Navigate sound menu down
          if(selectedSoundItem < 2) {
            selectedSoundItem++;
          } else {
            selectedSoundItem = 0;  // Wrap to top
          }
          buzzerMenuMove();
          drawSoundMenuItems();
        }
      } else if(inToolsMenu) {
        if(adjustingBrightness) {
          // Currently adjusting 7-seg brightness - DOWN decreases brightness
          max7219SetBrightness(max7219Brightness - 1);
          drawToolsMenuItems();  // Only redraw menu items
          updateDiceDisplay();
        } else if(inGraphMenu) {
          // DOWN button — single graph now, stay on page 0
          graphPage = 0;
          drawGraphMenu();
          buzzerMenuMove();
        } else if(inHelpMenu) {
          // Navigate help pages down (with wrapping)
          if(helpPage < 3) {  // Changed from 2 to 3 for 4 pages (0-3)
            helpPage++;
          } else {
            helpPage = 0;  // Wrap to first page
          }
          drawHelpMenu();
          buzzerMenuMove();
        } else {
          // Navigate tools menu down (no paging needed - all items fit on screen)
          if(selectedToolItem < 10) {  // 11 items (0-10)
            selectedToolItem++;
            drawToolsMenuItems();  // Only redraw menu items
          } else {
            selectedToolItem = 0;  // Wrap to top
            drawToolsMenuItems();  // Only redraw menu items
          }
          buzzerMenuMove();  // Menu navigation sound
        }
      } else {
        // Navigate main menu down
        if(selectedMenuItem < 2) {
          selectedMenuItem++;
        } else {
          selectedMenuItem = 0;  // Wrap to top
        }
        buzzerMenuMove();  // Menu navigation sound
        drawMenuItems();  // Only redraw menu items
      }
    } else if(gameState == STATE_ROLLING) {
      // Toggle between upper and lower section view
      scoreViewSection = 1 - scoreViewSection;  // Toggles between 0 and 1
      drawScreen();
    } else if(gameState == STATE_CATEGORY_SELECT) {
      int* scores = (currentPlayer == 1) ? scores1 : scores2;
      int originalCategory = selectedCategory;
      
      // Move down and skip used categories (wrap around to top)
      do {
        if(selectedCategory < 12) {
          selectedCategory++;
        } else {
          selectedCategory = 0;  // Wrap to top (Aces)
        }
      } while(scores[selectedCategory] != -1 && selectedCategory != originalCategory);
      
      drawScreen();
    }
  }
  lastDown = downBtn;
  
  // Enter button - confirm menu selection or category
  if(lastEnter == HIGH && enterBtn == LOW) {
    if(gameState == STATE_MENU) {
      if(inAIDifficultyMenu) {
        // Start game with selected AI difficulty
        buzzerMenuSelect();
        aiDifficulty = selectedAIDifficulty;
        
        // Save to EEPROM
        EEPROM.write(EEPROM_AI_DIFFICULTY_ADDR, aiDifficulty);
        EEPROM.commit();
        
        // Exit menu and start game
        inAIDifficultyMenu = false;
        gameState = STATE_START;
        drawScreen();
      } else if(inGraphMenu) {
        // Handle graph menu - ENTER goes back
        buzzerMenuSelect();
        inGraphMenu = false;
        graphPage = 0;
        drawToolsMenu();
      } else if(inHelpMenu) {
        // Handle help menu - ENTER goes back
        buzzerMenuSelect();
        inHelpMenu = false;
        helpPage = 0;
        // Force full redraw of tools menu by calling drawToolsMenu which clears screen
        drawToolsMenu();
      } else if(inSoundMenu) {
        // Handle sound submenu
        if(selectedSoundItem == 0) {
          // Toggle sound on/off
          soundEnabled = !soundEnabled;
          EEPROM.write(EEPROM_SOUND_ADDR, soundEnabled ? 1 : 0);
          EEPROM.commit();
          if(soundEnabled) {
            buzzerMenuSelect();
          }
          drawSoundMenuItems();
        } else if(selectedSoundItem == 1) {
          // Toggle volume adjustment mode
          adjustingVolume = !adjustingVolume;
          drawSoundMenuItems();
          if(!adjustingVolume) {
            buzzerMenuSelect();  // Play sound at current volume
          }
        } else if(selectedSoundItem == 2) {
          // Back to tools menu
          buzzerMenuSelect();
          inSoundMenu = false;
          selectedSoundItem = 0;
          adjustingVolume = false;
          drawScreen();
        }
      } else if(inToolsMenu) {
        // Handle tools menu
        if(selectedToolItem == 0) {
          // Toggle brightness adjustment mode
          adjustingBrightness = !adjustingBrightness;
          drawToolsMenuItems();  // Only redraw menu items
          if(adjustingBrightness) {
          } else {
          }
        } else if(selectedToolItem == 1) {
          // Enter sound submenu
          buzzerMenuSelect();
          inSoundMenu = true;
          selectedSoundItem = 0;
          drawSoundMenu();
        } else if(selectedToolItem == 2) {
          // Toggle auto-advance
          autoAdvance = !autoAdvance;
          EEPROM.write(EEPROM_AUTO_ADVANCE_ADDR, autoAdvance ? 1 : 0);
          EEPROM.commit();
          buzzerBeep(50);
          drawToolsMenuItems();
        } else if(selectedToolItem == 3) {
          // View statistics - switch to stats screen
          gameState = STATE_MENU;  // Stay in menu state
          drawStatistics();
          
          // Wait for ENTER to go back
          while(true) {
            if(digitalRead(enterButton) == LOW) {
              delay(200);  // Debounce
              while(digitalRead(enterButton) == LOW);  // Wait for release
              drawScreen();  // Redraw tools menu
              break;
            }
            
            // Hold ROLL button for 3 seconds to reset stats
            static unsigned long rollPressStart = 0;
            if(digitalRead(rollButton) == LOW) {
              if(rollPressStart == 0) {
                rollPressStart = millis();
              } else if(millis() - rollPressStart > 3000) {
                // Reset all statistics
                highScore = 0;
                p1Wins = 0;
                p2Wins = 0;
                totalTies = 0;
                mostYahtzees = 0;
                
                // Save to EEPROM
                EEPROM.write(EEPROM_HIGH_SCORE_ADDR, highScore >> 8);
                EEPROM.write(EEPROM_HIGH_SCORE_ADDR + 1, highScore & 0xFF);
                EEPROM.write(EEPROM_P1_WINS_ADDR, p1Wins >> 8);
                EEPROM.write(EEPROM_P1_WINS_ADDR + 1, p1Wins & 0xFF);
                EEPROM.write(EEPROM_P2_WINS_ADDR, p2Wins >> 8);
                EEPROM.write(EEPROM_P2_WINS_ADDR + 1, p2Wins & 0xFF);
                EEPROM.write(EEPROM_TIES_ADDR, totalTies >> 8);
                EEPROM.write(EEPROM_TIES_ADDR + 1, totalTies & 0xFF);
                EEPROM.write(EEPROM_MOST_YAHTZEES_ADDR, mostYahtzees);
                EEPROM.commit();
                
                buzzerBeep(100);
                delay(100);
                buzzerBeep(100);
                
                drawStatistics();  // Redraw with zeros
                rollPressStart = 0;
                
              }
            } else {
              rollPressStart = 0;
            }
            
            delay(50);
          }
        } else if(selectedToolItem == 4) {
  // View AI statistics - switch to AI stats screen
  gameState = STATE_MENU;  // Stay in menu state
  aiStatsPage = 0;  // Always start on page 1
  drawAIStatistics();
  
  // Wait for ENTER to go back OR UP/DOWN to change pages OR hold to reset
  unsigned long enterPressStart = 0;
  bool wasPressed = false;
  
  while(true) {
    bool enterPressed = (digitalRead(enterButton) == LOW);
    
    // Check UP button for page navigation (scroll to HIGHER page numbers)
    static bool lastUpInStats = HIGH;
    bool upBtn = digitalRead(upButton);
    if(lastUpInStats == HIGH && upBtn == LOW) {
  aiStatsPage = 0;  // single page
  drawAIStatistics();
  delay(200);
}
    lastUpInStats = upBtn;
    
    // Check DOWN button for page navigation (scroll to LOWER page numbers)
    static bool lastDownInStats = HIGH;
    bool downBtn = digitalRead(downButton);
    if(lastDownInStats == HIGH && downBtn == LOW) {
    aiStatsPage = 0;  // single page
   drawAIStatistics();
   delay(200);
}
    lastDownInStats = downBtn;
    
    if(enterPressed) {
      if(!wasPressed) {
        enterPressStart = millis();
        wasPressed = true;
      } else {
        unsigned long holdDuration = millis() - enterPressStart;
        
        if(holdDuration > 3000) {
          // RESET AI STATS
          
          resetAILearningData();
          
          // Triple beep confirmation
          buzzerBeep(100);
          delay(100);
          buzzerBeep(100);
          delay(100);
          buzzerBeep(100);
          
          // Serial confirmation so reset can be verified
          Serial.println("=== AI LEARNING RESET ===");
          Serial.print("weightYahtzee after reset: "); Serial.println(aiLearning.weightYahtzee);
          Serial.print("weightUpperBonus after reset: "); Serial.println(aiLearning.weightUpperBonus);
          Serial.print("gamesPlayed after reset: "); Serial.println(aiLearning.gamesPlayed);
          Serial.println("=========================");
          
          // Show confirmation message
          tft.fillRect(0, 260, 240, 60, COLOR_BG);
          tft.setTextSize(2);
          tft.setTextColor(COLOR_GREEN);
          tft.setCursor(10, 265);
          tft.print("AI RESET!");
          
          tft.setTextSize(1);
          tft.setTextColor(COLOR_TEXT);
          tft.setCursor(10, 290);
          tft.print("Stats & weights cleared"); 
          
          delay(1500);
          
          drawAIStatistics();
          enterPressStart = 0;
          wasPressed = false;
          
        }
      }
    } else {
      if(wasPressed) {
        unsigned long holdDuration = millis() - enterPressStart;
        
        if(holdDuration < 3000) {
          // Short press - go back to menu
          delay(100);
          drawScreen();
          break;
        }
        
        wasPressed = false;
        enterPressStart = 0;
      }
    }
    
    delay(50);
  }
        } else if(selectedToolItem == 5) {
          // Change AI speed
          aiSpeed++;
          if(aiSpeed > 3) aiSpeed = 0;  // Cycle: Slow -> Medium -> Fast -> Instant -> Slow
          
          // Save to EEPROM
          EEPROM.write(EEPROM_AI_SPEED_ADDR, aiSpeed);
          EEPROM.commit();
          
          buzzerBeep(50);
          drawScreen();
          
          const char* speedNames[] = {"Slow (5s)", "Medium (3.5s)", "Fast (2.5s)", "Instant (0.5s)"};
          
        } else if(selectedToolItem == 6) {
          // Export stats to Serial Monitor
          buzzerMenuSelect();
          
          // Show "Exporting..." message
          tft.fillScreen(COLOR_BG);
          tft.setTextSize(3);
          tft.setTextColor(COLOR_GREEN);
          tft.setCursor(10, 80);
          tft.print("EXPORTING");
          
          tft.setTextSize(2);
          tft.setTextColor(COLOR_TEXT);
          tft.setCursor(20, 130);
          tft.print("Check Serial");
          tft.setCursor(20, 155);
          tft.print("Monitor!");
          
          tft.setTextSize(1);
          tft.setTextColor(COLOR_GRAY);
          tft.setCursor(10, 200);
          tft.print("Baud: 115200");
          tft.setCursor(10, 215);
          tft.print("Export will take ~2 sec");
          
          delay(500);
          
          // Export all stats
          exportStatsToSerial();
          
          // Show completion
          tft.fillRect(0, 240, 240, 60, COLOR_BG);
          tft.setTextSize(2);
          tft.setTextColor(COLOR_GREEN);
          tft.setCursor(40, 250);
          tft.print("COMPLETE!");
          
          buzzerBeep(100);
          delay(100);
          buzzerBeep(100);
          
          delay(2000);
          drawScreen();
          
        } else if(selectedToolItem == 7) {
          // View AI Performance Graphs
          buzzerMenuSelect();
          inGraphMenu = true;
          graphPage = 0;
          drawGraphMenu();
        } else if(selectedToolItem == 8) {
          // View game rules/help
          buzzerMenuSelect();
          inHelpMenu = true;
          helpPage = 0;
          drawHelpMenu();
        } else if(selectedToolItem == 9) {
          // Launch Blackjack
          buzzerMenuSelect();
          runBlackjack();        // defined in Blackjack_V1.ino
          drawScreen();
        } else if(selectedToolItem == 10) {
          // Back to main menu
          buzzerMenuSelect();
          inToolsMenu = false;
          inSoundMenu = false;  // Also exit sound menu if in it
          inHelpMenu = false;   // Also exit help menu if in it
          inGraphMenu = false;  // Also exit graph menu if in it
          selectedMenuItem = 0;
          toolsPage = 0;  // Reset to page 1
          adjustingBrightness = false;  // Reset brightness mode
          adjustingTftBrightness = false;  // Reset TFT brightness mode
          adjustingVolume = false;  // Reset volume mode
          drawScreen();
        }
        } else {
  // Handle main menu selection
  if(selectedMenuItem == 0) {
    // 2 Player Game
    buzzerMenuSelect();
    vsComputer = false;
    gameState = STATE_START;
    drawScreen();
  } else if(selectedMenuItem == 1) {
    // vs Computer - show AI difficulty selection menu
    buzzerMenuSelect();
    vsComputer = true;
    inAIDifficultyMenu = true;
    selectedAIDifficulty = aiDifficulty;  // Start with current difficulty
    drawAIDifficultyMenu();
  } else if(selectedMenuItem == 2) {
          // Tools - enter submenu
          buzzerMenuSelect();
          inToolsMenu = true;
          selectedToolItem = 0;
          toolsPage = 0;  // Reset to page 1
          drawScreen();
        }
      }
    } else if(gameState == STATE_CATEGORY_SELECT && !diceAnimating) {
      // Only allow selection if dice animation is complete
      selectCategory();
    }
  }
  lastEnter = enterBtn;
  
// Hold buttons - toggle held state of dice OR adjust brightness
  if(gameState == STATE_ROLLING) {
    for(int i = 0; i < 5; i++) {
      // Reverse the button index to match physical layout
      // Physical button layout: [1][2][3][4][5] (left to right)
      // Dice array layout: dice[0][1][2][3][4] (left to right)
      // Button array is pre-reversed, so we need to reverse the index
      int buttonIndex = 4 - i;  // Map: i=0→button 4, i=1→button 3, i=2→button 2, i=3→button 1, i=4→button 0
      
      bool holdBtn = digitalRead(holdButtons[buttonIndex]);
      if(lastHold[i] == HIGH && holdBtn == LOW) {
        held[i] = !held[i];
        // Play appropriate sound based on hold state
        if(held[i]) {
          buzzerDiceHold();
        } else {
          buzzerDiceUnhold();
        }
        updateDiceDisplay();
        drawScreen();
      }
      lastHold[i] = holdBtn;
    }
  } else {
    // Update lastHold states even when not in use
    for(int i = 0; i < 5; i++) {
      int buttonIndex = 4 - i;
      lastHold[i] = digitalRead(holdButtons[buttonIndex]);
    }
  }
}

// ============================================================================
// GAME LOGIC
// ============================================================================

void rollDice() {
  bool allHeld = true;
  
  // Increment roll counter
  rollsUsedThisTurn++;
  
  // Roll the dice and store final values
  for(int i = 0; i < 5; i++) {
    if(!held[i]) {
      dice[i] = random(1, 7);
      finalDiceValues[i] = dice[i];
      allHeld = false;
    } else {
      finalDiceValues[i] = dice[i];
    }
  }
  
  // If all dice are held, finish the turn immediately
  if(allHeld) {
    rollsLeft = 0;
    updateDiceDisplay();
  } else {
    // Start animation
    diceAnimating = true;
    diceAnimStartTime = millis();
    diceAnimFrame = 0;
    
    
    // Set staggered settle times for cascading reveal
    for(int i = 0; i < 5; i++) {
      if(!held[i]) {
        diceSettleTime[i] = 500 + (i * 100);  // 500ms, 600ms, 700ms, 800ms, 900ms
      } else {
        diceSettleTime[i] = 0;  // Held dice don't animate
      }
    }
  }
}

void selectCategory() {
  int* scores = (currentPlayer == 1) ? scores1 : scores2;
  int* yahtzeeBonus = (currentPlayer == 1) ? &yahtzeeBonus1 : &yahtzeeBonus2;
  bool anyCelebrationRan = false;  // Set true whenever a celebration runs; suppresses score-view delay
  
  // === DEBUG: Verify what we're scoring ===
  for(int i = 0; i < 5; i++) {
  }

  // === WAIT FOR DICE ANIMATION TO COMPLETE ===
  // Don't update scores or screen until 7-segment animation finishes
  if(diceAnimating) {
    unsigned long waitStart = millis();
    while(diceAnimating && millis() - waitStart < 5000) {  // 5 second timeout
      updateDiceAnimation();
      delay(16);  // ~60fps
    }
  }
  
  // Check if category already used
  if(scores[selectedCategory] != -1) {
    
    // Flash red on screen
    tft.fillRect(5, selectedCategory * 20 + 75 - menuScroll * 20 - 2, 230, 16, COLOR_RED);
    delay(200);
    drawScreen();
    return;
  }
  
  // Check for Yahtzee bonus (rolled Yahtzee but category already used)
  if(isYahtzee(dice) && scores[11] != -1 && scores[11] > 0) {
    (*yahtzeeBonus)++;
    anyCelebrationRan = true;
    // Trigger Yahtzee Bonus celebration
    initCelebration(8);
    
    // Display the Yahtzee Bonus celebration
    if(celebrationActive) {
      // Force initial draw to clear screen and show title
      tft.fillScreen(COLOR_BG);
      delay(10);
      
      // Draw celebration title bar immediately
      drawCelebration();
      delay(100);  // Pause to ensure title is visible
      
      unsigned long celebStart = millis();
      unsigned long lastUpdate = millis();
      unsigned long celebrationDuration = 6000;  // 6 seconds for Yahtzee Bonus (increased from 5)
      
      while(celebrationActive && (millis() - celebStart < celebrationDuration)) {
        // Update at 60fps
        if(millis() - lastUpdate >= 16) {
          updateCelebration();
          drawCelebration();
          lastUpdate = millis();
        }
        delay(1);  // Small delay to prevent watchdog issues
      }
      
      celebrationActive = false;
      
      // Show "YAHTZEE BONUS +100!" message after celebration
      tft.fillScreen(COLOR_BG);
      tft.setTextColor(COLOR_HELD);  // Yellow color
      tft.setTextSize(2);
      tft.setCursor(20, 100);
      tft.print("YAHTZEE BONUS!");
      tft.setTextSize(3);
      tft.setCursor(60, 140);
      tft.print("+100");
      delay(2000);  // Show message for 2 more seconds
    }
  }
  
  // Track Yahtzees for statistics
  if(isYahtzee(dice)) {
    if(currentPlayer == 1) {
      currentGameYahtzees1++;
    } else {
      currentGameYahtzees2++;
    }
  }
  
  // Calculate and assign score
  int score = calculateCategoryScore(dice, selectedCategory);
  
  // Play category selection sound (but not during celebrations which have their own sounds)
  buzzerCategorySelect();
  
  // Check upper total BEFORE scoring (to detect if we just reached bonus)
  int upperBefore = getUpperSectionTotal(scores);
  
  // *** CRITICAL SAFETY CHECK: Verify score is reasonable ***
  if(score == 0 && (selectedCategory == 9 || selectedCategory == 10)) {
    // Scoring 0 for a straight - this might be wrong
    for(int i = 0; i < 5; i++) {
    }
    
    // Double-check with manual verification
    if(selectedCategory == 9) {
      bool actuallyHasSmall = isStraight(dice, 4);
    } else if(selectedCategory == 10) {
      bool actuallyHasLarge = isLargeStraight(dice);
    }
  }
  
  scores[selectedCategory] = score;
  
  // Check if we just achieved upper section bonus (63+)
  if(selectedCategory <= 5) {  // Only for upper section categories
    int upperAfter = getUpperSectionTotal(scores);
    if(upperBefore < 63 && upperAfter >= 63) {
      // Just reached the upper bonus!
      initCelebration(7);
      anyCelebrationRan = true;
      
      // Wait for upper bonus celebration to finish (same pattern as other celebrations)
      if(celebrationActive) {
        tft.fillScreen(COLOR_BG);
        delay(10);
        drawCelebration();
        delay(100);
        
        unsigned long celebStart = millis();
        unsigned long lastUpdate = millis();
        unsigned long celebrationDuration = 5000;  // 5 seconds for upper bonus
        
        while(celebrationActive && (millis() - celebStart < celebrationDuration)) {
          if(millis() - lastUpdate >= 16) {
            updateCelebration();
            drawCelebration();
            lastUpdate = millis();
          }
          delay(1);
        }
        
        celebrationActive = false;
        tft.fillScreen(COLOR_BG);
        delay(200);
      }
    }
  }
  
  // **ENHANCED AI LEARNING: Track new metrics when AI scores**
  if(vsComputer && currentPlayer == 2) {
    // Track which roll was used to score this category
    int rollUsed = rollsUsedThisTurn;  // Use the actual counter (1, 2, or 3)
    if(rollUsed >= 1 && rollUsed <= 3) {
      if(rollUsed == 1) aiLearning.firstRollScores[selectedCategory] += score;
      else if(rollUsed == 2) aiLearning.secondRollScores[selectedCategory] += score;
      else if(rollUsed == 3) aiLearning.thirdRollScores[selectedCategory] += score;
      
      aiLearning.rollCountUsed[rollUsed]++;
      aiRollCountThisGame[rollUsed]++;  // Also track per-game for rollCountWins
    }
    
        // Track score distribution per category
    // **OVERFLOW PROTECTION**: Reset sum if it would overflow (>60000) or every 1000 games
    if(aiLearning.categoryScoreSum[selectedCategory] > 60000 || 
       (aiLearning.categoryScoredCount[selectedCategory] > 0 && 
        aiLearning.categoryScoredCount[selectedCategory] % 1000 == 0)) {
      // Reset to current average * count to maintain accuracy
      uint16_t currentAvg = aiLearning.categoryAvgScore[selectedCategory] * 2;  // Convert back from /2 storage
      aiLearning.categoryScoreSum[selectedCategory] = currentAvg * aiLearning.categoryScoredCount[selectedCategory];
    }
    
    // **CRITICAL FIX**: Increment category usage count
    aiLearning.categoryScoredCount[selectedCategory]++;
    
    aiLearning.categoryScoreSum[selectedCategory] += score;
    uint16_t count = aiLearning.categoryScoredCount[selectedCategory];
    if(count > 0) {
      aiLearning.categoryAvgScore[selectedCategory] = 
        (aiLearning.categoryScoreSum[selectedCategory] / count) / 2;  // Divide by 2 to fit in uint8_t
    }
    
    // Note: categoryWinsWhenScored is updated at game end in updateAIStatsAfterGame().
    // This is intentional — using the previous game's data means the win-rate
    // correctly reflects historical performance, not the current in-progress game.
    
        // Track hold pattern used
    int heldDice = 0;
    for(int i = 0; i < 5; i++) {
      if(held[i]) heldDice++;
    }
    if(heldDice >= 0 && heldDice <= 5) {
      switch(heldDice) {
        case 0: aiLearning.holdPattern0Dice++; break;
        case 1: aiLearning.holdPattern1Dice++; break;
        case 2: aiLearning.holdPattern2Dice++; break;
        case 3: aiLearning.holdPattern3Dice++; break;
        case 4: aiLearning.holdPattern4Dice++; break;
        case 5: aiLearning.holdPattern5Dice++; break;
      }
    }
    
    // Track endgame scenarios (turns 11-13)
    if(playerTurn >= 11) {
      int aiTotal = calculateTotalWithBonuses(scores2, yahtzeeBonus2);
      int humanTotal = calculateTotalWithBonuses(scores1, yahtzeeBonus1);
      int margin = abs(aiTotal - humanTotal);
      
      if(margin <= 20) {
        // Close game in endgame
        // We'll update wins/losses at game end
      } else if(aiTotal > humanTotal + 50) {
        // Blowout lead
        // We'll update wins at game end
      } else if(humanTotal > aiTotal + 30) {
        // Attempting comeback
        aiLearning.endgameComebackAttempts++;
      }
    }
    
    // Track category timing (which turn was best for scoring special categories)
    if(selectedCategory == 11 && score == 50) {  // Yahtzee
      if(aiLearning.optimalTurnForYahtzee == 0 || playerTurn < aiLearning.optimalTurnForYahtzee) {
        aiLearning.optimalTurnForYahtzee = playerTurn;
      }
    } else if(selectedCategory == 10 && score == 40) {  // Large Straight
      if(aiLearning.optimalTurnForLargeStraight == 0 || playerTurn < aiLearning.optimalTurnForLargeStraight) {
        aiLearning.optimalTurnForLargeStraight = playerTurn;
      }
    }

    // ── NEW: Record category average turn ────────────────────────────────
    // NOTE on sequencing: categoryScoredCount[selectedCategory] was already
    // incremented above (line ~6116), so cnt == new count (includes this entry).
    // The running-average formula (oldAvg*(cnt-1) + newValue) / cnt is correct
    // precisely because cnt-1 equals the *previous* count.  If the increment
    // order ever changes, this formula must be updated to match.
    {
      uint16_t cnt = aiLearning.categoryScoredCount[selectedCategory];
      if(cnt > 0) {
        int oldAvg = aiLearning.categoryAvgTurn[selectedCategory];
        int newAvg = ((oldAvg * (cnt - 1)) + playerTurn) / cnt;
        aiLearning.categoryAvgTurn[selectedCategory] = (uint8_t)constrain(newAvg, 1, 13);
      } else {
        // cnt should never be 0 here (count was just incremented), but guard anyway
        aiLearning.categoryAvgTurn[selectedCategory] = (uint8_t)constrain(playerTurn, 1, 13);
      }
    }

    // ── NEW: Record early-game scoring ────────────────────────────────────
    if(playerTurn <= 6 && score > 0) {
      aiLearning.categoryEarlyScoreSum[selectedCategory] =
        min((int)aiLearning.categoryEarlyScoreSum[selectedCategory] + score, 60000);
    }

    // ── NEW: Record Chance timing ─────────────────────────────────────────
    if(selectedCategory == 12 && score > 0) {
      int bucket = (playerTurn <= 3) ? 0 : (playerTurn <= 6) ? 1 : (playerTurn <= 9) ? 2 : 3;
      aiLearning.chanceTurnBucket[bucket]++;
      aiLearning.chanceScoreSum[bucket] =
        min((int)aiLearning.chanceScoreSum[bucket] + score, 60000);
      aiChanceBucket = bucket;  // Remember for win crediting at game end
    }

    // ── NEW: Record upper section sacrifice ───────────────────────────────
    // A "sacrifice" is scoring < 33% of the slot's maximum potential
    if(selectedCategory <= 5 && score > 0) {
      int slotValue    = selectedCategory + 1;
      int maxPossible  = slotValue * 5;
      bool isSacrifice = (score * 3 < maxPossible);  // score < 33% of max
      if(isSacrifice) {
        aiLearning.upperSacrificeCount[selectedCategory]++;
        int upperAfterCheck = getUpperSectionTotal(scores);
        if(upperAfterCheck >= 63) {
          aiLearning.upperSacrificeBonus[selectedCategory]++;
        }
        aiSacrificedSlot[selectedCategory] = true;  // Remember for win crediting
      }
    }
  }
  
  
  // Record AI decision for learning
  if(vsComputer && currentPlayer == 2) {
    int total1 = calculateTotalWithBonuses(scores1, yahtzeeBonus1);
    int total2 = calculateTotalWithBonuses(scores2, yahtzeeBonus2);
    bool leading = (total2 > total1);
    
    // Count held dice
    int heldCount = 0;
    for(int i = 0; i < 5; i++) {
      if(held[i]) heldCount++;
    }
    
    recordDecision(playerTurn, 3 - rollsLeft, heldCount, selectedCategory, score, leading);
    
  }
  
  // Debug: Show dice values
  for(int i = 0; i < 5; i++) {
  }
  
  // Trigger celebration for special achievements
  bool shouldCelebrate = false;
  
  
  if(selectedCategory == 7 && score > 0) {
    // 4 of a Kind
    initCelebration(1);
    shouldCelebrate = true;
  } else if(selectedCategory == 8 && score > 0) {
    // Full House
    initCelebration(2);
    shouldCelebrate = true;
  } else if(selectedCategory == 9 && score > 0) {
    // Small Straight
    initCelebration(3);
    shouldCelebrate = true;
  } else if(selectedCategory == 10 && score > 0) {
    // Large Straight
    initCelebration(4);
    shouldCelebrate = true;
  } else if(selectedCategory == 11 && score > 0) {
    // Yahtzee
    initCelebration(5);
    shouldCelebrate = true;
    
    // Extra verification
    if(!celebrationActive) {
    } else {
    }
  }
  
  // Wait for celebration to finish before next turn
  if(shouldCelebrate && celebrationActive) {
    
    // Force initial draw to clear screen and show title
    tft.fillScreen(COLOR_BG);
    delay(10);
    
    // Draw celebration title bar immediately
    drawCelebration();
    delay(100);  // Longer pause to ensure title is visible
    
    unsigned long celebStart = millis();
    unsigned long lastUpdate = millis();
    
    // Give AI celebrations a bit more time to be visible (6 seconds for Yahtzee)
unsigned long celebrationDuration = 5000;  // Changed from 4000 to 5000
if(celebrationType == 5) {  // Yahtzee
  celebrationDuration = 6000;  // Changed from 5000 to 6000 - 6 seconds for Yahtzee
}
    
    while(celebrationActive && (millis() - celebStart < celebrationDuration)) {
      // Update at 60fps
      if(millis() - lastUpdate >= 16) {
        updateCelebration();
        drawCelebration();
        lastUpdate = millis();
      }
      delay(1);  // Small delay to prevent watchdog issues
    }
    
    if(celebrationActive) {
      celebrationActive = false;
    }
    
    
    // Clear screen after celebration before continuing
    tft.fillScreen(COLOR_BG);
    delay(200);  // Longer delay to ensure clean transition
  }
  
  // Check if this was the last turn BEFORE calling nextTurn
  if(turn >= 26) {
    celebrationActive = false;  // Kill any lingering celebration before game over screen
    gameOverAnimationStarted = false;  // Ensure clean start for game over animation
    gameOver = true;
    gameState = STATE_GAME_OVER;
    drawScreen();  // Go directly to game over screen
  } else {
    // **AI SCORE VISIBILITY**: Show the updated scorecard with the chosen row highlighted.
    // Skip when a celebration just ran — the player already saw something special.
    if(vsComputer && currentPlayer == 2 && !shouldCelebrate && !anyCelebrationRan) {
      // Speed delays: 0=Slow(4s), 1=Medium(3s), 2=Fast(2.5s), 3=Instant(2.5s)
      const unsigned long scoreViewDelays[] = {1500, 1500, 1500, 1500};
      aiLastScoredCategory = selectedCategory;  // Flag for green highlight in drawCategorySelect
      drawScreen();  // Draw scorecard with the just-scored row highlighted
      delay(scoreViewDelays[aiSpeed]);
      aiLastScoredCategory = -1;  // Clear highlight before handing over to nextTurn
    }
    nextTurn();
  }
}

int calculateCategoryScore(int dice[], int category) {
  int counts[7] = {0};
  int total = 0;
  
  // Count each die value and calculate total
  for(int i = 0; i < 5; i++) {
    counts[dice[i]]++;
    total += dice[i];
  }
  
  // Upper section (Ones through Sixes)
  if(category <= 5) {
    return counts[category + 1] * (category + 1);
  }
  
  // Lower section
  switch(category) {
    case 6: // 3 of a kind
      for(int i = 1; i <= 6; i++) {
        if(counts[i] >= 3) return total;
      }
      return 0;
      
    case 7: // 4 of a kind
      for(int i = 1; i <= 6; i++) {
        if(counts[i] >= 4) return total;
      }
      return 0;
      
    case 8: { // Full house (3 of one + 2 of another)
      bool hasThree = false, hasTwo = false;
      for(int i = 1; i <= 6; i++) {
        if(counts[i] == 3) hasThree = true;
        if(counts[i] == 2) hasTwo = true;
      }
      return (hasThree && hasTwo) ? 25 : 0;
    }
      
    case 9: { // Small straight (4 in a row)
      bool hasSmall = isStraight(dice, 4);
      
      // *** DETAILED DEBUG: Always log small straight check ***
      for(int i = 0; i < 5; i++) {
      }
      
      return hasSmall ? 30 : 0;
    }
      
    case 10: { // Large straight (5 in a row)
      bool hasLarge = isLargeStraight(dice);
      
      // *** DETAILED DEBUG: Always log large straight check ***
      for(int i = 0; i < 5; i++) {
      }
      
      return hasLarge ? 40 : 0;
    }
      
    case 11: // Yahtzee (all 5 same)
      return isYahtzee(dice) ? 50 : 0;
      
    case 12: // Chance (sum of all dice)
      return total;
  }
  return 0;
}

int findBestCategory(int dice[], int scores[]) {
  int bestCategory = -1;
  int bestScore = -1;
  
  // Priority order: Yahtzee > Large Straight > Small Straight > Full House >
  //                 4-of-Kind > 3-of-Kind > upper section > Chance (last resort)
  // Chance is treated as a backup — it must NEVER beat a special that scores now.
  
  // Check all categories and find the highest available score
  for(int i = 0; i < 13; i++) {
    if(scores[i] == -1) {  // Category not used yet
      int score = calculateCategoryScore(dice, i);
      
      // Apply priority bonuses for special categories
      int priorityScore = score;
      if(i == 11 && score > 0) {
        // Yahtzee - massive priority
        priorityScore = score + 1000;
      } else if(i == 10 && score > 0) {
        // Large Straight - high priority
        priorityScore = score + 500;
      } else if(i == 9 && score > 0) {
        // Small Straight - above Full House (30 > 25 pts)
        priorityScore = score + 300;
      } else if(i == 8 && score > 0) {
        // Full House - medium priority
        priorityScore = score + 200;
      } else if(i == 7 && score > 0) {
        // 4-of-Kind - medium priority
        priorityScore = score + 150;
      } else if(i == 6 && score > 0) {
        // 3-of-Kind - lower priority than 4K/FH but still beats Chance early
        priorityScore = score + 50;
      } else if(i == 12) {
        // Chance — legitimate fallback when all real options are weak.
        // Penalty of -10 means:
        //   Chance=24 (priority 14) beats upper scoring <= 13, loses to >= 14
        //   Chance=20 (priority 10) beats upper scoring <=  9, loses to >= 10
        // Specials are safe: 3K bonus=50 means even 3K scoring 12 → priority 62,
        // far above any Chance priority.
        if(score > 0) {
          priorityScore = score - 10;
        } else {
          priorityScore = -1000;
        }
      } else if(score == 0) {
        // PENALTY: Categories that score 0 get negative priority
        priorityScore = -1000;
      }
      
      if(priorityScore > bestScore) {
        bestScore = priorityScore;
        bestCategory = i;
      }
    }
  }
  
  // SAFETY: If best category scores 0, find ANY category with points
  if(bestCategory != -1) {
    int actualScore = calculateCategoryScore(dice, bestCategory);
    if(actualScore == 0) {
      
      // Find category with highest actual score
      int backupCat = -1;
      int backupScore = 0;
      
      for(int i = 0; i < 13; i++) {
        if(scores[i] == -1) {
          int score = calculateCategoryScore(dice, i);
          if(score > backupScore) {
            backupScore = score;
            backupCat = i;
          }
        }
      }
      
      if(backupCat != -1 && backupScore > 0) {
        bestCategory = backupCat;
      }
    }
  }
  
  // CRITICAL SAFETY: If no valid category found, search for ANY available category
  if(bestCategory == -1 || scores[bestCategory] != -1) {
    Serial.println("CRITICAL ERROR in findBestCategoryAI: No valid category found!");
    
    for(int i = 0; i < 13; i++) {
      if(scores[i] == -1) {
        return i;
      }
    }
    
    // If we get here, all categories are used (game should be over)
    return 0;  // Return something to prevent crash
  }
  
  // === FINAL SAFETY CHECK: Never return Chance if a special scores now ======
  // This is the last line of defence — catches any edge case where the EV
  // scoring above still let Chance win over a category that would actually
  // score points on the current dice.
  if(bestCategory == 12) {  // Chance selected

    // First: if we actually HAVE a straight that scores, take it immediately.
    bool hasSmallStraight = isStraight(dice, 4);
    bool hasLargeStraight = isLargeStraight(dice);
    if(hasLargeStraight && scores[10] == -1) return 10;
    if(hasSmallStraight && scores[9]  == -1) return 9;

    // Second: check every other special category — if the dice score in it
    // right now AND the slot is open, pick the highest-scoring one over Chance.
    // This covers Full House, 3-of-Kind, 4-of-Kind, and Yahtzee.
    // Priority order: Yahtzee (50) > 4-of-Kind > Full House (25) > 3-of-Kind.
    int specialPriority[] = {11, 7, 8, 6};  // Yahtzee, 4K, FH, 3K
    for(int pi = 0; pi < 4; pi++) {
      int cat = specialPriority[pi];
      if(scores[cat] == -1) {
        int catScore = calculateCategoryScore(dice, cat);
        if(catScore > 0) {
          return cat;  // This special scores now — take it over Chance
        }
      }
    }

    // Third: if any straight slot is open but we don't currently have a
    // straight, only override Chance if the best alternative scores
    // materially more than Chance would.  Don't sacrifice Chance for 2-3
    // pts in an upper slot — that's exactly the scenario where Chance is right.
    if(scores[9] == -1 || scores[10] == -1) {
      int chanceScore = calculateCategoryScore(dice, 12);
      int altCategory = -1;
      int altScore    = -1;
      for(int i = 0; i < 12; i++) {
        if(scores[i] == -1) {
          int categoryScore = calculateCategoryScore(dice, i);
          if(categoryScore > altScore) {
            altScore    = categoryScore;
            altCategory = i;
          }
        }
      }
      // Only override if the alt scores at least 8 pts more than Chance,
      // OR if Chance itself scores zero (nothing to save).
      if(altCategory != -1 && (altScore >= chanceScore + 8 || chanceScore == 0)) {
        return altCategory;
      }
    }
  }
  
  return bestCategory;
}

bool isStraight(int dice[], int length) {
  // Sort dice
  int sorted[5];
  for(int i = 0; i < 5; i++) sorted[i] = dice[i];
  
  for(int i = 0; i < 4; i++) {
    for(int j = 0; j < 4-i; j++) {
      if(sorted[j] > sorted[j+1]) {
        int temp = sorted[j];
        sorted[j] = sorted[j+1];
        sorted[j+1] = temp;
      }
    }
  }
  
  // *** CRITICAL FIX: Remove duplicates first ***
  int unique[5];
  int uniqueCount = 0;
  
  for(int i = 0; i < 5; i++) {
    bool isDuplicate = false;
    for(int j = 0; j < uniqueCount; j++) {
      if(sorted[i] == unique[j]) {
        isDuplicate = true;
        break;
      }
    }
    if(!isDuplicate) {
      unique[uniqueCount++] = sorted[i];
    }
  }
  
  // *** Count consecutive numbers in UNIQUE values ***
  int consecutive = 1;
  int maxConsecutive = 1;
  
  for(int i = 1; i < uniqueCount; i++) {
    if(unique[i] == unique[i-1] + 1) {
      consecutive++;
      if(consecutive > maxConsecutive) {
        maxConsecutive = consecutive;
      }
    } else {
      consecutive = 1;
    }
  }
  
  return (maxConsecutive >= length);
}

bool isLargeStraight(int dice[]) {
  // *** CRITICAL FIX: Large straight requires EXACTLY 5 unique consecutive values ***
  // Valid: 1-2-3-4-5 or 2-3-4-5-6
  // Invalid: 1-1-2-3-4 (has duplicate)
  
  // Sort dice
  int sorted[5];
  for(int i = 0; i < 5; i++) sorted[i] = dice[i];
  
  for(int i = 0; i < 4; i++) {
    for(int j = 0; j < 4-i; j++) {
      if(sorted[j] > sorted[j+1]) {
        int temp = sorted[j];
        sorted[j] = sorted[j+1];
        sorted[j+1] = temp;
      }
    }
  }
  
  // *** Check for duplicates - large straight cannot have any ***
  for(int i = 0; i < 4; i++) {
    if(sorted[i] == sorted[i+1]) {
      return false;  // Has duplicate, cannot be large straight
    }
  }
  
  // *** Now check if all 5 dice are consecutive ***
  for(int i = 0; i < 4; i++) {
    if(sorted[i+1] != sorted[i] + 1) {
      return false;  // Not consecutive
    }
  }
  
  return true;
}

bool isYahtzee(int dice[]) {
  for(int i = 1; i < 5; i++) {
    if(dice[i] != dice[0]) return false;
  }
  return true;
}

void nextTurn() {
  // Increment overall turn counter
  turn++;
  
  // Switch players and reset their turn if switching
  if(currentPlayer == 1) {
    currentPlayer = 2;
    playerTurn = (turn + 1) / 2;  // Player 2's turn number
  } else {
    currentPlayer = 1;
    playerTurn = turn / 2;  // Player 1's turn number
  }
  
  rollsLeft = 3;
  rollsUsedThisTurn = 0;  // Reset roll counter for new turn
  gameState = STATE_START;  // Changed from STATE_INTRO to STATE_START

  // ── NEW: Snapshot score differential when AI reaches turn 10 ─────────
  // playerTurn is now set to the NEW turn number after the increment above.
  // We want to capture state at the START of the AI's 10th turn.
  if(vsComputer && currentPlayer == 2 && playerTurn == 10 && !aiTurn10Recorded) {
    int aiTotal    = calculateTotalWithBonuses(scores2, yahtzeeBonus2);
    int humanTotal = calculateTotalWithBonuses(scores1, yahtzeeBonus1);
    int diff = aiTotal - humanTotal;
    // Clamp to int16 range
    diff = constrain(diff, -32000, 32000);
    aiLearning.scoreDiffSumTurn10 = constrain(
      (int)aiLearning.scoreDiffSumTurn10 + diff, -32000, 32000);
    aiLearning.gamesReachingTurn10++;
    aiAheadAtTurn10  = (diff >= 0);
    aiTurn10Recorded = true;
  }
  
  // Clear dice and held state
  for(int i = 0; i < 5; i++) {
    dice[i] = 0;  // Blank dice
    held[i] = false;
  }
  
  if(turn > 26) {
    celebrationActive = false;
    gameOver = true;
  } else {
  }
  
  updateDiceDisplay();
  drawScreen();
  
  // AUTO-ADVANCE: If enabled and it's a human player's turn, automatically roll
  if(autoAdvance && (!vsComputer || currentPlayer == 1)) {
    delay(800);  // Brief pause to show turn transition
    rollsLeft--;
    rollsUsedThisTurn = 1;
    rollDice();
    gameState = STATE_ROLLING;
    blankDiceDisplay();
    drawScreen();
    buzzerStartRoll();
  }
}

int calculateTotal(int scores[]) {
  int total = 0;
  int upperTotal = 0;
  
  // Upper section total
  for(int i = 0; i <= 5; i++) {
    if(scores[i] != -1) {
      upperTotal += scores[i];
    }
  }
  
  // Bonus for 63+ in upper section
  if(upperTotal >= 63) {
    total += 35;
  }
  total += upperTotal;
  
  // Lower section total
  for(int i = 6; i <= 12; i++) {
    if(scores[i] != -1) {
      total += scores[i];
    }
  }
  
  return total;
}

int calculateTotalWithBonuses(int scores[], int yahtzeeBonus) {
  int total = calculateTotal(scores);
  total += (yahtzeeBonus * 100);  // 100 points per Yahtzee bonus
  return total;
}

int getUpperSectionTotal(int scores[]) {
  int upperTotal = 0;
  for(int i = 0; i <= 5; i++) {
    if(scores[i] != -1) {
      upperTotal += scores[i];
    }
  }
  return upperTotal;
}

bool hasUpperBonus(int scores[]) {
  return getUpperSectionTotal(scores) >= 63;
}

// ─────────────────────────────────────────────────────────────────────────────
// calcBonusFeasibility: returns a 0.0-1.0 score for how realistic the upper
// bonus is, given remaining categories and scores so far.
//
//   1.0 = already have enough / trivially achievable
//   0.0 = mathematically impossible (shouldn't happen if bonusStillPossible)
//   ~0.5 = borderline – need above-average rolls every category
//
// The key insight: each open upper category needs an AVERAGE of
//   (bonusRemaining / upperCategoriesOpen) points.
// That average needed compares against the realistic per-category average:
//   category i can score 1*i to 5*i; the "fair" roll (3 dice on average) = 3*i.
// We compute a weighted probability across open categories.
// ─────────────────────────────────────────────────────────────────────────────
float calcBonusFeasibility(int scores[]) {
  int upperTotal = getUpperSectionTotal(scores);
  if(upperTotal >= 63) return 1.0;   // Already have bonus

  int bonusRemaining = 63 - upperTotal;

  // Collect open upper categories
  int openCats[6];
  int openCount = 0;
  for(int i = 0; i <= 5; i++) {
    if(scores[i] == -1) openCats[openCount++] = i;
  }

  if(openCount == 0) return 0.0;  // No open slots left – impossible

  // Maximum theoretically achievable (5 of each value)
  int maxFromRemaining = 0;
  for(int k = 0; k < openCount; k++) {
    maxFromRemaining += (openCats[k] + 1) * 5;
  }
  if(bonusRemaining > maxFromRemaining) return 0.0;  // Impossible

  // "Expected" from remaining if we roll the average (3 of each)
  int expectedFromRemaining = 0;
  for(int k = 0; k < openCount; k++) {
    expectedFromRemaining += (openCats[k] + 1) * 3;
  }

  // Feasibility: ratio of expected vs needed, clamped to [0,1]
  // If expected >= needed, feasibility is high (> 0.5)
  // We use a simple linear model with a centre at 0.5 = (needed / expected) = 1
  if(expectedFromRemaining <= 0) return 0.0;

  float ratio = (float)expectedFromRemaining / (float)bonusRemaining;
  // ratio > 1 means average rolls are enough; ratio < 1 means we need above-average.
  // Calibrated so ratio==1 → 0.65 (average rolls WILL get there more often than not),
  // ratio==2 → 1.0 (trivially reachable), ratio==0.5 → 0.15 (very hard).
  // Old formula (0.5*ratio) under-valued realistic cases — ratio=1 gave 0.5 which
  // sat right at the threshold and caused the AI to abandon bonus pursuit too early.
  float feasibility = 0.30f + 0.35f * ratio;  // ratio==1→0.65, ratio==2→1.0, ratio==0.5→0.475
  if(feasibility > 1.0f) feasibility = 1.0f;
  if(feasibility < 0.0f) feasibility = 0.0f;
  return feasibility;
}

// ─────────────────────────────────────────────────────────────────────────────
// upperOpportunityCost: for a die value v with 'count' dice showing, how much
// future potential does scoring it NOW "waste"?
//   Returns 0.0 (no waste – this is the slot's maximum) to 1.0 (huge waste).
//   Formula: 1 - (current_score / max_possible_score)
//   e.g. 1x6 = 6/30 -> cost 0.80 (very wasteful, save the 6s slot!)
//        3x6 = 18/30 -> cost 0.40 (decent, still some room but acceptable)
//        5x6 = 30/30 -> cost 0.00 (perfect, take it)
//        2x1 = 2/5   -> cost 0.60 (pretty wasteful, but 1s are low value anyway)
// ─────────────────────────────────────────────────────────────────────────────
float upperOpportunityCost(int value, int count) {
  if(value <= 0) return 1.0f;
  int maxPossible = value * 5;
  int currentScore = value * count;
  return 1.0f - (float)currentScore / (float)maxPossible;
}

void computerTurn() {
  static unsigned long lastActionTime = 0;
  static bool needsReset = true;  // Track if we need to reset timing
  static unsigned long turnStartTime = 0;  // Track when turn started
  
  // TIMEOUT PROTECTION: If computer has been thinking for more than 30 seconds, force action
  if(computerIsThinking && turnStartTime > 0 && millis() - turnStartTime > 30000) {
    
    // Force category selection
    if(gameState == STATE_ROLLING || gameState == STATE_CATEGORY_SELECT) {
      int* scores = scores2;
      
      // Use findBestCategoryAI instead of just picking first
      selectedCategory = findBestCategoryAI(dice, scores, true);
      
      if(selectedCategory == -1) {
        // Find ANY available category
        for(int i = 0; i < 13; i++) {
          if(scores[i] == -1) {
            selectedCategory = i;
            break;
          }
        }
      }
      
      if(selectedCategory != -1) {
        gameState = STATE_CATEGORY_SELECT;
        selectCategory();
      }
    }
    
    // Reset computer state
    computerIsThinking = false;
    computerThinkPhase = 0;
    lastActionTime = 0;
    turnStartTime = 0;
    needsReset = true;
    return;
  }
  
  // AI speed delays: 0=Slow (5000ms), 1=Medium (3500ms), 2=Fast (2500ms), 3=Instant (500ms)
  unsigned long speedDelays[] = {5000, 3500, 2500, 500};
  unsigned long thinkDelay = speedDelays[aiSpeed];
  
  // Initialize timing on first call for this turn
  if(!computerIsThinking) {
    computerIsThinking = true;
    lastActionTime = millis();
    turnStartTime = millis();  // Track when turn started
    needsReset = true;
  }
  
  if(gameState == STATE_START) {
    // Phase 0: Start rolling
    if(millis() - lastActionTime > thinkDelay) {
      
      gameState = STATE_ROLLING;
      rollsLeft--;  // Use first roll
      
      // **v42.8: Track first roll (no before score, using 0)**
      rollDice();
      blankDiceDisplay(); // Don't flash final values before animation
      
      drawScreen();
      
      for(int i = 0; i < 5; i++) {
      }
      
      // Reset timing for next phase
      lastActionTime = millis();
      computerIsThinking = true;
      computerThinkPhase = 1;
    }
  } else if(gameState == STATE_ROLLING && !diceAnimating) {
    // Phase 1: Decide which dice to hold and whether to reroll
    if(millis() - lastActionTime > thinkDelay) {
      
      
      // ═══════════════════════════════════════════════════════════════
      // STEP 1: DECIDE WHAT TO HOLD (before deciding whether to roll)
      // ═══════════════════════════════════════════════════════════════
      
      if(rollsLeft > 0) {
        computerDecideHolds();
        
        // Debug: Show hold decision
        int heldCount = 0;
        for(int i = 0; i < 5; i++) {
          if(held[i]) {
            heldCount++;
          } else {
          }
        }
        
        // Update display to show holds BEFORE rolling
        updateDiceDisplay();
        drawScreen();
        delay(500);  // Brief pause so user can see what AI is holding
      }

      
      // NOTE: the old "chase Large Straight from a Small Straight" special case was
      // removed here. Verified exhaustively against the optimal solver: across all
      // 7,340,032 (scorecard x dice) cases where it could fire, the optimal policy
      // ALWAYS rerolls too — so it only duplicated the gate below. The gate now
      // handles it, using the same optimal hold.

      // ═══════════════════════════════════════════════════════════════════════
      // STEP 2: DECIDE WHETHER TO ROLL AGAIN OR SCORE
      // ═══════════════════════════════════════════════════════════════════════
      
      // ═══════════════════════════════════════════════════════════════
      // STOP-vs-REROLL GATE — the optimal V-brain decides. It also (re)sets
      // held[] to match, so the reroll below uses the optimal hold.
      // ═══════════════════════════════════════════════════════════════
      bool shouldRollAgain = yzAI_shouldReroll();

      // ═══════════════════════════════════════════════════════════════
      // EXECUTE DECISION: Roll again or go to category selection
      // ═══════════════════════════════════════════════════════════════
      
      if(shouldRollAgain && rollsLeft > 0) {
        // If computerDecideHolds somehow held all 5, release the lowest-value
        // die so the roll is meaningful — never silently cancel a reroll.
        int heldCount = 0;
        for(int i = 0; i < 5; i++) { if(held[i]) heldCount++; }
        if(heldCount >= 5) {
          int worstIdx = -1, worstVal = 999;
          for(int i = 0; i < 5; i++) {
            if(dice[i] < worstVal) { worstVal = dice[i]; worstIdx = i; }
          }
          if(worstIdx >= 0) { held[worstIdx] = false; updateDiceDisplay(); }
        }

        // Use one roll NOW (held[] is already set from computerDecideHolds() above)
        rollsLeft--;
        
        // Roll the dice (this respects the held[] array)
        rollDice();
        blankDiceDisplay(); // Don't flash final values before animation
        drawScreen();
        
        for(int i = 0; i < 5; i++) {
        }
        
        // Reset timing for next decision
        lastActionTime = millis();
        computerIsThinking = true;
        computerThinkPhase = 1;
        return;  // Exit and re-evaluate on next loop
      }
      
      // If we reach here, AI decided to stop rolling
      if(!shouldRollAgain || rollsLeft == 0) {
        // No more rolls OR decided to stop - go to category selection
        rollsLeft = 0;
        
        gameState = STATE_CATEGORY_SELECT;
        drawScreen();
        
        // **INSTANT SPEED ENHANCEMENT: Pause on score screen after all rolls complete**
        // Give user time to see the final dice result (same as Fast speed = 2500ms)
        // Skip this pause if we're about to show a celebration (winner, Yahtzee, etc.)
        // so celebrations fire at the same moment they would for player 1.
        bool upcomingCelebration = false;
        {
          int* sc = scores2;
          int bc = findBestCategoryAI(dice, sc, false);
          if(bc >= 0 && bc <= 12 && sc[bc] == -1) {
            int ps = calculateCategoryScore(dice, bc);
            if((bc == 7 && ps > 0) || (bc == 8 && ps > 0) ||
               (bc == 9 && ps > 0) || (bc == 10 && ps > 0) ||
               (bc == 11 && ps > 0)) {
              upcomingCelebration = true;
            }
            // Upper section bonus trigger
            if(bc <= 5 && ps > 0) {
              int ub = getUpperSectionTotal(sc);
              if(ub < 63 && ub + ps >= 63) upcomingCelebration = true;
            }
          }
          // Yahtzee bonus
          if(isYahtzee(dice) && sc[11] != -1 && sc[11] > 0) upcomingCelebration = true;
          // Game-over / winner celebration
          if(turn >= 26) upcomingCelebration = true;
        }
        
        if(aiSpeed == 3 && !upcomingCelebration) {  // Instant mode, no celebration coming
          delay(2500);  // Match Fast speed delay for score viewing
        }
        
        lastActionTime = millis();
        computerIsThinking = true;
        computerThinkPhase = 2;
      }
    }
    } else if(gameState == STATE_CATEGORY_SELECT && !diceAnimating) {
    // Phase 2: Select category
    
    // IMPORTANT: Check for celebrations EVERY loop iteration to ensure we catch them
    // This must be done before the delay check so we can skip the delay immediately
    bool willCelebrate = false;
    int* scores = scores2;
    int bestCat = findBestCategoryAI(dice, scores, true);
    
    if(bestCat >= 0 && bestCat <= 12 && scores[bestCat] == -1) {
      int potentialScore = calculateCategoryScore(dice, bestCat);
      
      // Check if this category will trigger a celebration
      if((bestCat == 7 && potentialScore > 0) ||  // 4 of a Kind
         (bestCat == 8 && potentialScore > 0) ||  // Full House
         (bestCat == 9 && potentialScore > 0) ||  // Small Straight
         (bestCat == 10 && potentialScore > 0) || // Large Straight
         (bestCat == 11 && potentialScore > 0)) { // Yahtzee
        willCelebrate = true;
      }
    }
    
    // ALSO check for Yahtzee Bonus (separate from main category)
    if(isYahtzee(dice) && scores[11] != -1 && scores[11] > 0) {
      willCelebrate = true;
    }
    
    // ALSO check for upper section bonus (if scoring upper section)
    if(bestCat >= 0 && bestCat <= 5 && scores[bestCat] == -1) {
      int potentialScore = calculateCategoryScore(dice, bestCat);
      int upperBefore = getUpperSectionTotal(scores);
      int upperAfter = upperBefore + potentialScore;
      if(upperBefore < 63 && upperAfter >= 63) {
        willCelebrate = true;
      }
    }
    
    // ALSO check for winner / game-over celebration (turn 26 = last AI turn)
    // This ensures the winner screen fires with zero delay, same as player 1.
    if(turn >= 26) {
      willCelebrate = true;
    }
    
    // For celebrations: execute IMMEDIATELY (no delay at all)
    // For non-celebrations: use normal think delay
    bool shouldExecuteNow = false;
    
    if(willCelebrate) {
      // Celebration = instant execution
      shouldExecuteNow = true;
    } else {
      // Normal execution = wait for think delay
      shouldExecuteNow = (millis() - lastActionTime > thinkDelay);
    }
    
    if(shouldExecuteNow) {
      // Get AI scores
      
      // FINAL CATEGORY DECISION - this is what will actually be scored
      
      // CRITICAL: Get the best category (already calculated above if willCelebrate)
      if(bestCat == -1) {
        bestCat = findBestCategoryAI(dice, scores, true);
      }
      
      if(bestCat == -1) {
        // Find first available category as emergency fallback
        for(int i = 0; i < 13; i++) {
          if(scores[i] == -1) {
            bestCat = i;
            break;
          }
        }
      }
      
      // CRITICAL: Verify category is available
      if(bestCat < 0 || bestCat > 12 || scores[bestCat] != -1) {
        
        // Find first available category as emergency fallback
        bestCat = -1;
        for(int i = 0; i < 13; i++) {
          if(scores[i] == -1) {
            bestCat = i;
            break;
          }
        }
        
        // If still no category found, game is over
        if(bestCat == -1) {
          gameOver = true;
          computerIsThinking = false;
          computerThinkPhase = 0;
          lastActionTime = 0;
          needsReset = true;
          return;
        }
        
      }
      
      // SET selectedCategory - this is what selectCategory() will use
      selectedCategory = bestCat;
      
      
      // CALL selectCategory() to actually score it
      selectCategory();
      
      // **CRITICAL**: Don't reset AI state immediately if celebration is active
      // The celebration loop in selectCategory() should handle this, but let's add a safety delay
      if(celebrationActive) {
        // The celebration will run in selectCategory(), so this shouldn't be reached
        // But if it is, wait for celebration to finish
        delay(500);
      }
      
      // Reset for next turn
      computerIsThinking = false;
      computerThinkPhase = 0;
      lastActionTime = 0;
      needsReset = true;
    }
  }
}



// ============================================================================
// OPTIMAL AI BRAIN (option B — exact joint-DP value table, see vtable.h)
//   Replaces the learned-weight hold/category heuristics.  Uses the exact
//   per-category EV table directly (no weight multipliers), plus an
//   opportunity-cost model (baseline[c] = expected value of category c from a
//   fresh roll) so consuming a category is charged its true future cost.
//   Validated standalone against known-correct hands before integration.
// ============================================================================

void yzAI_sort5(int a[5]) {
  for(int i = 0; i < 4; i++)
    for(int j = 0; j < 4 - i; j++)
      if(a[j] > a[j+1]) { int t = a[j]; a[j] = a[j+1]; a[j+1] = t; }
}

// Correct lexicographic index of a SORTED 5-dice state into EV_STATES.
int yzAI_stateIdx(const int sorted[5]) {
  int lo = 0, hi = 251;
  while(lo <= hi) {
    int mid = (lo + hi) / 2;
    bool eq = true, less = false;
    for(int k = 0; k < 5; k++) {
      uint8_t sv = EV_STATES[mid][k];
      if(sorted[k] < sv)      { eq = false; less = true;  break; }
      else if(sorted[k] > sv) { eq = false; less = false; break; }
    }
    if(eq) return mid;
    else if(less) hi = mid - 1;
    else lo = mid + 1;
  }
  return -1;
}

// ============================================================================
// OPTIMAL V-BRAIN (option B) — provably-optimal fixed policy using vtable.h.
// Reuses yzAI_sort5 / yzAI_stateIdx (above) and the sketch's scoring helpers.
// ============================================================================
uint8_t  YZ_KEEPCNT[462][7];      // [0]=size, [1..6]=face counts
int      YZ_NKEEP = 0;
uint16_t YZ_RR_OFF[462], YZ_RR_LEN[462];
uint16_t YZ_RR_IDX[4400]; float YZ_RR_P[4400]; int YZ_RR_N = 0;
uint16_t YZ_HOLD_OFF[252], YZ_HOLD_LEN[252]; uint16_t YZ_HOLD_KEEP[4400]; int YZ_HOLD_N = 0;
bool     YZ_ready = false;
float    yz_W0[252], yz_W1[252], yz_KV[462];
int      yz_lastMask = -1, yz_lastUp = -1, yz_lastElig = -1;

bool yz_isYtz(int d[5]) { for(int v=1;v<=6;v++){ int c=0; for(int i=0;i<5;i++) if(d[i]==v) c++; if(c==5) return true; } return false; }
inline int yz_eligCount(int mask) { return (mask & (1<<11)) ? 1 : 2; }
inline float yz_V(int mask,int up,int elig){ if(mask==0) return 0.0f; return YZ_VPACK[YZ_VOFFSET[mask] + up*yz_eligCount(mask) + elig] / 100.0f; }
int yz_maskOf(int* scores){ int m=0; for(int c=0;c<13;c++) if(scores[c]==-1) m|=(1<<c); return m; }

int yz_keepId(int cnt[7]) {
  int sz=0; for(int v=1;v<=6;v++) sz+=cnt[v];
  for(int i=0;i<YZ_NKEEP;i++){ if(YZ_KEEPCNT[i][0]!=sz) continue; bool eq=true; for(int v=1;v<=6;v++) if(YZ_KEEPCNT[i][v]!=cnt[v]){eq=false;break;} if(eq) return i; }
  YZ_KEEPCNT[YZ_NKEEP][0]=sz; for(int v=1;v<=6;v++) YZ_KEEPCNT[YZ_NKEEP][v]=cnt[v]; return YZ_NKEEP++;
}
static double yz_fd(int n){ double f=1; for(int i=2;i<=n;i++) f*=i; return f; }
void yz_recReroll(int start,int placed,int rr,int* work,const int* orig,int* tIdx,float* tP,int* tn,double* POW6){
  if(placed==rr){
    int full[5],p=0; for(int v=1;v<=6;v++) for(int j=0;j<work[v];j++) full[p++]=v; yzAI_sort5(full); int idx=yzAI_stateIdx(full);
    double perm=yz_fd(rr); for(int v=1;v<=6;v++) perm/=yz_fd(work[v]-orig[v]); double pr=perm/POW6[rr];
    for(int t=0;t<*tn;t++) if(tIdx[t]==idx){ tP[t]+=(float)pr; return; } tIdx[*tn]=idx; tP[*tn]=(float)pr; (*tn)++; return;
  }
  for(int v=start;v<=6;v++){ work[v]++; yz_recReroll(v,placed+1,rr,work,orig,tIdx,tP,tn,POW6); work[v]--; }
}
void yz_initTables(){
  for(int c1=0;c1<=5;c1++)for(int c2=0;c1+c2<=5;c2++)for(int c3=0;c1+c2+c3<=5;c3++)for(int c4=0;c1+c2+c3+c4<=5;c4++)for(int c5=0;c1+c2+c3+c4+c5<=5;c5++)for(int c6=0;c1+c2+c3+c4+c5+c6<=5;c6++){ int cnt[7]={0,c1,c2,c3,c4,c5,c6}; yz_keepId(cnt); }
  double POW6[6]={1,6,36,216,1296,7776};
  for(int id=0;id<YZ_NKEEP;id++){
    int base[7]; for(int v=0;v<7;v++) base[v]=YZ_KEEPCNT[id][v]; int rr=5-base[0];
    YZ_RR_OFF[id]=YZ_RR_N; int tIdx[300]; float tP[300]; int tn=0;
    int work[7]; for(int v=0;v<7;v++) work[v]=base[v];
    yz_recReroll(1,0,rr,work,base,tIdx,tP,&tn,POW6);
    for(int t=0;t<tn;t++){ YZ_RR_IDX[YZ_RR_N]=tIdx[t]; YZ_RR_P[YZ_RR_N]=tP[t]; YZ_RR_N++; } YZ_RR_LEN[id]=tn;
  }
  for(int i=0;i<252;i++){
    YZ_HOLD_OFF[i]=YZ_HOLD_N; bool seen[462]={false}; int cnt0=0;
    for(int m=0;m<32;m++){ int cnt[7]={0}; for(int b=0;b<5;b++) if(m&(1<<b)) cnt[EV_STATES[i][b]]++; int id=yz_keepId(cnt); if(!seen[id]){ seen[id]=true; YZ_HOLD_KEEP[YZ_HOLD_N++]=id; cnt0++; } }
    YZ_HOLD_LEN[i]=cnt0;
  }
  YZ_ready=true;
}
// Value of scoring dice d (0 rerolls) in the best category, returns that category in *bestC.
float yz_leaf(int d[5], bool ytz, int mask, int up, int elig, int* bestC){
  float best=-1e9f; int bc=-1;
  for(int c=0;c<13;c++) if(mask & (1<<c)){
    int sc=calculateCategoryScore(d,c); int g=sc; int nu=up, ne=elig;
    if(elig && ytz) g+=100;
    if(c<=5){ int t=up+sc; if(up<63 && t>=63) g+=35; nu=(t>63)?63:t; }
    if(c==11) ne=(sc==50)?1:0;
    float val = g + yz_V(mask & ~(1<<c), nu, ne);
    if(val>best){ best=val; bc=c; }
  }
  if(bestC) *bestC=bc; return best;
}
// Fill yz_W0 (score-now value per state) and yz_W1 (one-reroll value per state)
// for the current scorecard context. Cached — recomputed only when context changes.
void yz_computeW(int mask,int up,int elig){
  if(YZ_ready && mask==yz_lastMask && up==yz_lastUp && elig==yz_lastElig) return;
  for(int s=0;s<252;s++){ int d[5]; for(int k=0;k<5;k++) d[k]=EV_STATES[s][k]; yz_W0[s]=yz_leaf(d,yz_isYtz(d),mask,up,elig,nullptr); }
  for(int id=0;id<YZ_NKEEP;id++){ float acc=0; int off=YZ_RR_OFF[id]; for(int t=0;t<YZ_RR_LEN[id];t++) acc+=YZ_RR_P[off+t]*yz_W0[YZ_RR_IDX[off+t]]; yz_KV[id]=acc; }
  for(int s=0;s<252;s++){ float b=-1e9f; int off=YZ_HOLD_OFF[s]; for(int t=0;t<YZ_HOLD_LEN[s];t++){ int id=YZ_HOLD_KEEP[off+t]; if(yz_KV[id]>b) b=yz_KV[id]; } yz_W1[s]=b; }
  yz_lastMask=mask; yz_lastUp=up; yz_lastElig=elig;
}
// Best keep-multiset id for sorted dice sd, valuing the continuation with Wnext.
int yz_bestHoldFor(int sd[5], float* Wnext){
  int si=yzAI_stateIdx(sd); int best=-1; float bv=-1e9f; int off=YZ_HOLD_OFF[si];
  for(int t=0;t<YZ_HOLD_LEN[si];t++){ int id=YZ_HOLD_KEEP[off+t]; float acc=0; int ro=YZ_RR_OFF[id]; for(int u=0;u<YZ_RR_LEN[id];u++) acc+=YZ_RR_P[ro+u]*Wnext[YZ_RR_IDX[ro+u]]; if(acc>bv){ bv=acc; best=id; } }
  return best;
}
// Set heldArr[] to keep the faces described by keep-multiset keepId.
void yz_applyHold(int keepId, int* diceArr, bool* heldArr){
  int need[7]; for(int v=1;v<=6;v++) need[v]=YZ_KEEPCNT[keepId][v];
  for(int i=0;i<5;i++) heldArr[i]=false;
  for(int i=0;i<5;i++){ int v=diceArr[i]; if(v>=1 && v<=6 && need[v]>0){ heldArr[i]=true; need[v]--; } }
}

int yzAI_chooseCategory(int dice[], int* scores) {
  if(!YZ_ready) yz_initTables();
  int mask = yz_maskOf(scores);
  int up = getUpperSectionTotal(scores); if(up>63) up=63;
  int elig = (scores[11]==50) ? 1 : 0;
  int bc; yz_leaf(dice, yz_isYtz(dice), mask, up, elig, &bc);
  return bc;
}

// Shared hold evaluation.  Fills:
//   bestProperMask = best mask that rerolls >=1 die (or -1 if none)
//   properVal      = expected opportunity-adjusted value of that reroll
//   scoreNow       = value of committing to the best category with current dice
// "Hold all" is intentionally excluded from the reroll options: keeping all dice
// and passing the roll is provably dominated by either scoring now or the best
// proper reroll, so the reroll-vs-stop choice is exactly properVal vs scoreNow.
void yzAI_decideHolds() {
  if(!YZ_ready) yz_initTables();
  if(rollsLeft <= 0) { for(int i=0;i<5;i++) held[i] = true; updateDiceDisplay(); return; }
  int mask = yz_maskOf(scores2);
  int up = getUpperSectionTotal(scores2); if(up>63) up=63;
  int elig = (scores2[11]==50) ? 1 : 0;
  yz_computeW(mask, up, elig);
  int sd[5]; for(int i=0;i<5;i++) sd[i]=dice[i]; yzAI_sort5(sd);
  float* Wnext = (rollsLeft >= 2) ? yz_W1 : yz_W0;
  int h = yz_bestHoldFor(sd, Wnext);
  yz_applyHold(h, dice, held);
  updateDiceDisplay();
}

// STOP-vs-REROLL gate. Holding all five dice is provably equivalent to stopping,
// so the AI rerolls iff the optimal hold keeps fewer than five dice. Also sets
// held[] consistently so the caller can roll immediately.
bool yzAI_shouldReroll() {
  if(rollsLeft <= 0) return false;
  if(!YZ_ready) yz_initTables();
  int mask = yz_maskOf(scores2);
  int up = getUpperSectionTotal(scores2); if(up>63) up=63;
  int elig = (scores2[11]==50) ? 1 : 0;
  yz_computeW(mask, up, elig);
  int sd[5]; for(int i=0;i<5;i++) sd[i]=dice[i]; yzAI_sort5(sd);
  float* Wnext = (rollsLeft >= 2) ? yz_W1 : yz_W0;
  int h = yz_bestHoldFor(sd, Wnext);
  yz_applyHold(h, dice, held);
  return YZ_KEEPCNT[h][0] < 5;   // reroll iff not holding all
}

void computerDecideHolds() {
  // Use advanced AI with probability calculations
  computerDecideHoldsAdvanced();
}

// ============================================================================
// PROBABILISTIC HOLD EVALUATION
// ============================================================================

struct HoldStrategyEV {
  bool holds[5];
  float expectedValue;
  int targetCategory;
  const char* description;
};

// Calculate expected value of a specific hold pattern

void computerDecideHoldsAdvanced() {
  // Thin wrapper — the optimal V-brain does the work. Kept so existing callers work.
  yzAI_decideHolds();
}

void updateCelebration() {
  if(!celebrationActive) return;
  
  // **NEW: Spawn additional firework bursts for winner celebration**
  if(celebrationType == 6) {
    unsigned long elapsed = millis() - celebrationStartTime;
    
    // Every 500ms, create a new small burst
    if(elapsed % 500 < 50) {  // 50ms window to spawn
      int newParticles = 15;
      int spawned = 0;
      
      // Find inactive particles and reactivate them
      for(int i = 0; i < MAX_PARTICLES && spawned < newParticles; i++) {
        if(!particles[i].active) {
          // Random position across screen
          particles[i].x = random(40, 200);
          particles[i].y = random(80, 250);
          
          // Burst outward
          float angle = random(0, 360) * 3.14159 / 180.0;
          float speed = random(20, 50) / 10.0;
          particles[i].vx = cos(angle) * speed;
          particles[i].vy = sin(angle) * speed;
          
          // Rainbow color
          particles[i].color = RAINBOW_COLORS[random(0, 8)];
          particles[i].life = 1.0;
          particles[i].fadeRate = 0.015;  // Fade faster (small bursts)
          particles[i].active = true;
          
          spawned++;
        }
      }
    }
  }
  
  bool anyActive = false;
  
  for(int i = 0; i < MAX_PARTICLES; i++) {
    if(particles[i].active) {
      // Update position
      particles[i].x += particles[i].vx;
      particles[i].y += particles[i].vy;
      
      // Apply gravity (except for sparkles which float)
      if(celebrationType != 3) {
        particles[i].vy += 0.3;  // Gravity
      }
      
      // Fade out
      particles[i].life -= particles[i].fadeRate;
      
      // Deactivate if dead or off screen
      // For type 3 and 4 (straights flowing left to right), allow starting off left side
      bool offScreen = false;
      if(celebrationType == 3 || celebrationType == 4) {
        // Allow particles to start far left, only deactivate when too far right or vertically off
        offScreen = (particles[i].y > 320 || particles[i].y < -50 || particles[i].x > 250);
      } else {
        // Other celebrations use normal bounds
        offScreen = (particles[i].y > 320 || particles[i].x < -10 || particles[i].x > 250);
      }
      
      if(particles[i].life <= 0 || offScreen) {
        particles[i].active = false;
      } else {
        anyActive = true;
      }
    }
  }
  
    // End celebration when all particles are gone or timeout
    // Winner celebration gets extra time (6 seconds vs 4 seconds)
    unsigned long timeout = (celebrationType == 6) ? 6000 : 4000;
    if(!anyActive || millis() - celebrationStartTime > timeout) {
    celebrationActive = false;
    celebrationType = 0;
  }
}

// ============================================================================
// CELEBRATION INITIALIZATION
// ============================================================================

void initCelebration(int type) {
  celebrationType = type;
  celebrationActive = true;
  celebrationStartTime = millis();
  
  
  // Initialize all particles as inactive
  for(int i = 0; i < MAX_PARTICLES; i++) {
    particles[i].active = false;
  }
  
  // Create particles based on celebration type
  int particleCount = 0;
  
  // Define color palettes for each celebration
  uint16_t colorPalette[4];
  int paletteSize = 4;
  
  switch(type) {
    case 1: // 4 of a Kind - Emerald green theme
      particleCount = 50;
      colorPalette[0] = tft.color565(0, 255, 0);      // Bright green
      colorPalette[1] = tft.color565(50, 255, 50);    // Light green
      colorPalette[2] = tft.color565(0, 200, 100);    // Teal-green
      colorPalette[3] = tft.color565(100, 255, 150);  // Mint
      
      for(int i = 0; i < particleCount; i++) {
        particles[i].x = 120;
        particles[i].y = 160;
        float angle = random(0, 360) * 3.14159 / 180.0;
        float speed = random(25, 70) / 10.0;
        particles[i].vx = cos(angle) * speed;
        particles[i].vy = sin(angle) * speed;
        particles[i].color = colorPalette[random(0, paletteSize)];
        particles[i].life = 1.0;
        particles[i].fadeRate = 0.012 + random(0, 5) / 1000.0;
        particles[i].active = true;
      }
      buzzerTone(1200, 80);
      delay(40);
      buzzerTone(1500, 80);
      break;
      
    case 2: // Full House - Purple/Pink/Blue rainbow
      particleCount = 60;
      colorPalette[0] = tft.color565(255, 0, 255);    // Magenta
      colorPalette[1] = tft.color565(200, 0, 255);    // Purple
      colorPalette[2] = tft.color565(255, 50, 200);   // Hot pink
      colorPalette[3] = tft.color565(150, 0, 255);    // Deep purple
      
      for(int i = 0; i < particleCount; i++) {
        particles[i].x = 120;
        particles[i].y = 160;
        float angle = random(0, 360) * 3.14159 / 180.0;
        float speed = random(30, 80) / 10.0;
        particles[i].vx = cos(angle) * speed;
        particles[i].vy = sin(angle) * speed;
        particles[i].color = colorPalette[random(0, paletteSize)];
        particles[i].life = 1.0;
        particles[i].fadeRate = 0.010 + random(0, 5) / 1000.0;
        particles[i].active = true;
      }
      buzzerTone(1000, 70);
      delay(35);
      buzzerTone(1300, 70);
      delay(35);
      buzzerTone(1600, 70);
      break;
      
    case 3: // Small Straight - Blue/Cyan wave FULL SCREEN
      particleCount = 70;
      colorPalette[0] = tft.color565(0, 200, 255);    // Bright cyan
      colorPalette[1] = tft.color565(0, 150, 255);    // Sky blue
      colorPalette[2] = tft.color565(100, 220, 255);  // Light cyan
      colorPalette[3] = tft.color565(0, 100, 200);    // Deep blue
      
      // Create flowing wave effect - FULL SCREEN HEIGHT
      for(int i = 0; i < particleCount; i++) {
        particles[i].x = -60 + (i * 4);  // Stagger across screen
        
        // **FIX: Cover full screen height (0-320) with wave pattern**
        // Create multiple wave layers for full coverage
        float baseY = 160;  // Center of screen (320/2)
        float waveOffset = sin(i * 0.3) * 80;  // Larger amplitude (±80 pixels)
        particles[i].y = baseY + waveOffset;  // Range: 80 to 240
        
        // Add some vertical variety to fill gaps
        if(i % 3 == 0) {
          particles[i].y += random(-40, 40);  // Extra vertical spread
        }
        
        particles[i].vx = random(20, 40) / 10.0;  // Flow right
        particles[i].vy = random(-20, 20) / 10.0;  // More vertical movement
        particles[i].color = colorPalette[random(0, paletteSize)];
        particles[i].life = 1.0;
        particles[i].fadeRate = 0.008;
        particles[i].active = true;
      }
      buzzerTone(800, 60);
      delay(30);
      buzzerTone(1100, 60);
      delay(30);
      buzzerTone(1400, 60);
      delay(30);
      buzzerTone(1700, 80);
      break;
      
    case 4: // Large Straight - Gold/Orange/Yellow FLOWING WAVE (like small straight)
      particleCount = 80;
      colorPalette[0] = tft.color565(255, 215, 0);    // Gold
      colorPalette[1] = tft.color565(255, 165, 0);    // Orange
      colorPalette[2] = tft.color565(255, 255, 100);  // Bright yellow
      colorPalette[3] = tft.color565(255, 140, 0);    // Dark orange
      
      // **CHANGED: Flow from left like small straight, but with gold colors**
      for(int i = 0; i < particleCount; i++) {
        particles[i].x = -60 + (i * 3.5);  // Slightly denser than small straight
        
        // **FIX: Cover full screen height with wave pattern**
        float baseY = 160;  // Center of screen
        float waveOffset = sin(i * 0.25) * 90;  // Different wave frequency, larger amplitude
        particles[i].y = baseY + waveOffset;  // Range: 70 to 250
        
        // Add vertical variety
        if(i % 3 == 0) {
          particles[i].y += random(-50, 50);  // Extra vertical spread
        }
        
        particles[i].vx = random(25, 45) / 10.0;  // Flow right (slightly faster than small)
        particles[i].vy = random(-20, 20) / 10.0;  // Vertical movement
        particles[i].color = colorPalette[random(0, paletteSize)];
        particles[i].life = 1.0;
        particles[i].fadeRate = 0.008;
        particles[i].active = true;
      }
      buzzerTone(900, 60);
      delay(25);
      buzzerTone(1200, 60);
      delay(25);
      buzzerTone(1500, 60);
      delay(25);
      buzzerTone(1800, 60);
      delay(25);
      buzzerTone(2100, 100);
      break;
      
    case 5: // Yahtzee - RED/Orange/Pink EXPLOSION
      particleCount = 80;
      colorPalette[0] = tft.color565(255, 0, 0);      // Bright red
      colorPalette[1] = tft.color565(255, 50, 0);     // Red-orange
      colorPalette[2] = tft.color565(255, 100, 100);  // Light red/pink
      colorPalette[3] = tft.color565(200, 0, 0);      // Deep red
      
      // Create explosive burst with varying speeds
      for(int i = 0; i < particleCount; i++) {
        particles[i].x = 120;
        particles[i].y = 160;
        float angle = random(0, 360) * 3.14159 / 180.0;
        float speed = random(60, 120) / 10.0;  // Very fast explosion
        particles[i].vx = cos(angle) * speed;
        particles[i].vy = sin(angle) * speed;
        particles[i].color = colorPalette[random(0, paletteSize)];
        particles[i].life = 1.0;
        particles[i].fadeRate = 0.007 + random(0, 5) / 1000.0;
        particles[i].active = true;
      }
      
      // Epic Yahtzee sound
      buzzerYahtzee();
      break;
      
    case 6: { // Winner - SPECTACULAR confetti explosion
  particleCount = 100;  // More particles for winner!
  
  uint16_t winnerColor = (currentPlayer == 1) ? COLOR_P1 : COLOR_P2;
  
  // Create SPECTACULAR confetti explosion from multiple points
  for(int i = 0; i < particleCount; i++) {
    // Create bursts from 3 locations for more coverage
    if(i < 40) {
      // Central burst
      particles[i].x = 120;
      particles[i].y = 160;
    } else if(i < 70) {
      // Left burst
      particles[i].x = 60;
      particles[i].y = 200;
    } else {
      // Right burst
      particles[i].x = 180;
      particles[i].y = 200;
    }
    
    // Create explosion pattern - varied speeds for depth
    float angle = random(0, 360) * 3.14159 / 180.0;
    float speed = random(40, 140) / 10.0;
    
    // Some particles shoot up dramatically
    if(i % 5 == 0) {
      angle = (80 + random(-30, 30)) * 3.14159 / 180.0;
      speed = random(80, 150) / 10.0;
    }
    
    particles[i].vx = cos(angle) * speed;
    particles[i].vy = sin(angle) * speed - 2.0;  // Slight upward bias
    
    // Vibrant rainbow colors with winner color accent
    if(i % 8 < 2) {
      particles[i].color = winnerColor;  // 25% winner color
    } else {
      particles[i].color = RAINBOW_COLORS[random(0, 8)];
    }
    
    particles[i].life = 1.0;
    particles[i].fadeRate = 0.003 + random(0, 4) / 1000.0;  // Even slower fade for longer show
    particles[i].active = true;
  }
  
  // Epic extended winner fanfare
      buzzerTone(1000, 120);
      delay(60);
      buzzerTone(1200, 120);
      delay(60);
      buzzerTone(1500, 120);
      delay(60);
      buzzerTone(1800, 120);
      delay(60);
      buzzerTone(2000, 250);
      break;
    } // End case 6
      
    case 7: // Upper Bonus (63+ points) - Gold coins/stars raining down
      particleCount = 60;
      colorPalette[0] = tft.color565(255, 215, 0);    // Gold
      colorPalette[1] = tft.color565(255, 255, 0);    // Yellow
      colorPalette[2] = tft.color565(255, 200, 0);    // Dark gold
      colorPalette[3] = tft.color565(255, 240, 100);  // Light gold
      
      // Coins falling from top
      for(int i = 0; i < particleCount; i++) {
        particles[i].x = random(10, 230);  // Across screen
        particles[i].y = random(-100, 0);  // Start above screen
        particles[i].vx = random(-10, 10) / 10.0;  // Slight horizontal drift
        particles[i].vy = random(30, 60) / 10.0;   // Fall down
        particles[i].color = colorPalette[random(0, paletteSize)];
        particles[i].life = 1.0;
        particles[i].fadeRate = 0.008 + random(0, 5) / 1000.0;
        particles[i].active = true;
      }
      
      // Success jingle
      buzzerTone(800, 80);
      delay(40);
      buzzerTone(1000, 80);
      delay(40);
      buzzerTone(1200, 80);
      delay(40);
      buzzerTone(1500, 150);
      break;
      
    case 8: // Yahtzee Bonus (additional Yahtzee) - Multicolor spiral
      particleCount = 70;
      
      // Create spiral explosion with all rainbow colors
      for(int i = 0; i < particleCount; i++) {
        particles[i].x = 120;
        particles[i].y = 160;
        
        // Spiral pattern
        float angleOffset = (i * 25) * 3.14159 / 180.0;  // Spiral spacing
        float radius = (i % 7) * 0.8;  // Vary by distance
        float speed = random(40, 90) / 10.0;
        
        particles[i].vx = cos(angleOffset) * speed;
        particles[i].vy = sin(angleOffset) * speed;
        
        // Cycle through rainbow colors
        particles[i].color = RAINBOW_COLORS[i % 8];
        particles[i].life = 1.0;
        particles[i].fadeRate = 0.009 + random(0, 5) / 1000.0;
        particles[i].active = true;
      }
      
      // Special bonus sound (different from regular Yahtzee)
      buzzerTone(1500, 100);
      delay(50);
      buzzerTone(1800, 100);
      delay(50);
      buzzerTone(2100, 100);
      delay(50);
      buzzerTone(2400, 200);
      break;
  }
}

// Helper function to draw a star/sparkle
void drawStar(int cx, int cy, int size, uint16_t color) {
  // Draw a 4-pointed star
  tft.drawLine(cx, cy - size, cx, cy + size, color);  // Vertical
  tft.drawLine(cx - size, cy, cx + size, cy, color);  // Horizontal
  tft.drawLine(cx - size/2, cy - size/2, cx + size/2, cy + size/2, color);  // Diagonal 1
  tft.drawLine(cx - size/2, cy + size/2, cx + size/2, cy - size/2, color);  // Diagonal 2
  // Center bright point
  tft.fillCircle(cx, cy, 2, color);
}

// Helper function to draw a trophy icon
void drawTrophy(int cx, int cy, uint16_t baseColor, uint16_t accentColor) {
  // Trophy cup (simplified pixel art style)
  // Cup bowl
  tft.fillRect(cx - 8, cy, 16, 8, baseColor);
  tft.drawRect(cx - 8, cy, 16, 8, accentColor);
  
  // Cup handles
  tft.drawLine(cx - 9, cy + 2, cx - 9, cy + 5, accentColor);
  tft.drawLine(cx + 9, cy + 2, cx + 9, cy + 5, accentColor);
  
  // Cup stem
  tft.fillRect(cx - 2, cy + 8, 4, 4, baseColor);
  
  // Cup base
  tft.fillRect(cx - 6, cy + 12, 12, 2, baseColor);
  tft.drawRect(cx - 6, cy + 12, 12, 2, accentColor);
  
  // Shine/sparkle on cup
  tft.fillCircle(cx - 3, cy + 3, 1, COLOR_TEXT);
}

void drawCelebration() {
  if(!celebrationActive) {
    // Reset static flag when celebration is inactive
    static bool resetFlag = false;
    if(!resetFlag) {
      resetFlag = true;
      // Force reset on next call
    }
    return;
  }
  
  // Track if this is first draw (per celebration)
  static bool screenCleared = false;
  static unsigned long lastCelebrationStart = 0;
  
  // Reset flag if this is a new celebration
  if(celebrationStartTime != lastCelebrationStart) {
    screenCleared = false;
    lastCelebrationStart = celebrationStartTime;
  }
  
  // Clear screen and draw title only once at start
  if(!screenCleared || millis() - celebrationStartTime < 100) {
    tft.fillScreen(COLOR_BG);
    
    
    // Show achievement title with a colored background bar
    const char* achievementNames[] = {
      "", 
      "4 OF A KIND!", 
      "FULL HOUSE!", 
      "SM STRAIGHT!",
      "LG STRAIGHT!",
      "YAHTZEE!!!",
      "",  // Winner has custom text
      "UPPER BONUS!",  // Type 7
      "YAHTZEE BONUS!"  // Type 8
    };
    
    // NEW: Different colors for title bars (contrasting with particle colors)
    uint16_t titleColors[] = {
      0, 
      tft.color565(0, 100, 50),      // Dark teal for 4-of-kind (vs green particles)
      tft.color565(80, 0, 120),      // Deep purple for Full House (vs magenta particles)
      tft.color565(0, 50, 100),      // Navy blue for Small Straight (vs cyan particles)
      tft.color565(150, 80, 0),      // Brown/bronze for Large Straight (vs gold particles)
      tft.color565(100, 0, 0),       // Dark red for Yahtzee (vs bright red particles)
      0,
      tft.color565(0, 80, 0),        // Dark green for Upper Bonus (vs gold particles)
      tft.color565(120, 0, 60)       // Dark magenta for Yahtzee Bonus (vs rainbow particles)
    };
    
    uint16_t textColors[] = {
      0,
      tft.color565(150, 255, 200),   // Bright mint text for 4-of-kind
      tft.color565(255, 150, 255),   // Bright pink text for Full House
      tft.color565(150, 220, 255),   // Bright cyan text for Small Straight
      tft.color565(255, 220, 100),   // Bright yellow text for Large Straight
      tft.color565(255, 100, 100),   // Bright red-pink text for Yahtzee
      0,
      tft.color565(255, 215, 0),     // Gold text for Upper Bonus
      tft.color565(255, 100, 255)    // Bright magenta-pink text for Yahtzee Bonus
    };
    
    if(celebrationType >= 1 && celebrationType <= 5 || celebrationType == 7 || celebrationType == 8) {
      // Draw colored bar behind text
      tft.fillRect(0, 135, 240, 35, titleColors[celebrationType]);
      
      // Draw border for emphasis
      tft.drawRect(0, 135, 240, 35, textColors[celebrationType]);
      tft.drawRect(1, 136, 238, 33, textColors[celebrationType]);
      
      // Draw text
      // "YAHTZEE BONUS!" (14 chars) is too wide for textSize(3) on a 240px screen
      // (14 * 18 = 252px), so use textSize(2) for type 8 to prevent the Y being clipped
      int textSize = (celebrationType == 8) ? 2 : 3;
      int charWidth = (celebrationType == 8) ? 12 : 18;
      tft.setTextSize(textSize);
      tft.setTextColor(textColors[celebrationType]);
      
      int textLen = strlen(achievementNames[celebrationType]);
      int textWidth = textLen * charWidth;
      int startX = (240 - textWidth) / 2;
      
      // Centre the text vertically in the 35px bar (bar top = 135, height = 35)
      // textSize(3) char height = 24px -> top offset = (35-24)/2 = 5  -> y = 140
      // textSize(2) char height = 16px -> top offset = (35-16)/2 = 9  -> y = 144
      int textY = (celebrationType == 8) ? 144 : 143;
      
      tft.setCursor(startX, textY);
      tft.print(achievementNames[celebrationType]);
    }
    
    screenCleared = true;
  }
  
  // ── WINNER CELEBRATION: redrawn every frame (animated background + scores) ──
  if(celebrationType == 6) {
      // ENHANCED Winner celebration - FULL SCREEN TAKEOVER
      uint16_t winnerColor = (currentPlayer == 1) ? COLOR_P1 : COLOR_P2;
      
      // Calculate animation phases
      unsigned long elapsed = millis() - celebrationStartTime;
      
      // Smooth rainbow color cycling using sine waves
      float colorPhase = (elapsed / 1000.0) * 3.14159;  // Slower, smoother cycling
      int r = 127 + 127 * sin(colorPhase);
      int g = 127 + 127 * sin(colorPhase + 2.094);  // 120 degrees offset
      int b = 127 + 127 * sin(colorPhase + 4.188);  // 240 degrees offset
      uint16_t smoothRainbow = tft.color565(r, g, b);
      
      // Bounce/pulse effect for text size
      float bouncePhase = sin((elapsed / 400.0) * 3.14159);  // Bounce every 800ms
      float scaleMultiplier = 1.0 + (bouncePhase * 0.15);  // Scale between 0.85 and 1.15
      
      // Animated background gradient - vertical rainbow sweep
      for(int y = 0; y < 320; y += 2) {
        float gradPhase = (y / 320.0) * 3.14159 * 2 + (elapsed / 500.0);
        int gr = 40 + 40 * sin(gradPhase);
        int gg = 40 + 40 * sin(gradPhase + 2.094);
        int gb = 40 + 40 * sin(gradPhase + 4.188);
        tft.drawFastHLine(0, y, 240, tft.color565(gr, gg, gb));
      }
      
      // Draw HUGE "PLAYER X" text
      tft.setTextSize(5);  // MASSIVE text
      
      // Calculate positions for centered text
      int playerTextWidth = 7 * 30;  // "PLAYER " = 7 chars at size 5 (30px each)
      int digitWidth = 30;  // One digit
      int totalWidth = playerTextWidth + digitWidth;
      int playerX = (240 - totalWidth) / 2;
      
      // Draw "PLAYER X" with smooth rainbow
      tft.setTextColor(smoothRainbow);
      tft.setCursor(playerX, 80);
      tft.print("PLAYER");
      
      // Player number in winner color with glow
      tft.setTextColor(winnerColor);
      tft.setCursor(playerX + playerTextWidth, 80);
      tft.print(currentPlayer);
      
      // Draw "WINS!" even larger below
      tft.setTextSize(6);  // EVEN BIGGER
      int winsWidth = 5 * 36;  // "WINS!" = 5 chars at size 6 (36px each)
      int winsX = (240 - winsWidth) / 2;
      
      // Offset color for "WINS!"
      float winsColorPhase = colorPhase + 1.57;  // 90 degrees ahead
      int wr = 127 + 127 * sin(winsColorPhase);
      int wg = 127 + 127 * sin(winsColorPhase + 2.094);
      int wb = 127 + 127 * sin(winsColorPhase + 4.188);
      uint16_t winsRainbow = tft.color565(wr, wg, wb);
      
      tft.setTextColor(winsRainbow);
      tft.setCursor(winsX, 150);
      tft.print("WINS!");
      
      // Draw final scores below
      int total1 = calculateTotalWithBonuses(scores1, yahtzeeBonus1);
      int total2 = calculateTotalWithBonuses(scores2, yahtzeeBonus2);
      
      tft.setTextSize(2);
      tft.setTextColor(COLOR_P1);
      tft.setCursor(30, 230);
      tft.print("P1: ");
      tft.print(total1);
      
      tft.setTextColor(COLOR_P2);
      tft.setCursor(140, 230);
      tft.print("P2: ");
      tft.print(total2);
      
      // Draw animated border that pulses
      float pulseBorder = sin((elapsed / 300.0) * 3.14159);
      int borderThickness = 3 + (int)(pulseBorder * 2);  // 1-5 pixels
      for(int i = 0; i < borderThickness; i++) {
        tft.drawRect(5 + i, 5 + i, 230 - (i*2), 310 - (i*2), smoothRainbow);
      }
      
      // Add sparkle/star bursts in corners (animated)
      if((elapsed / 150) % 2 == 0) {
        // Top corners
        drawStar(20, 30, 8, smoothRainbow);
        drawStar(220, 30, 8, winsRainbow);
        // Bottom corners
        drawStar(20, 280, 8, winsRainbow);
        drawStar(220, 280, 8, smoothRainbow);
      }
      
      // Trophy/crown icon at top center
      drawTrophy(120, 35, winnerColor, smoothRainbow);
  }  // end celebrationType == 6 (runs every frame)

  // Subtle per-frame fade for non-winner celebrations (motion blur on particles)
  if(celebrationType != 6) {
    if(screenCleared) {
      // Other celebrations - avoid title bar area
      for(int y = 0; y < 130; y += 8) {
        for(int x = 0; x < 240; x += 8) {
          tft.drawPixel(x, y, COLOR_BG);
        }
      }
      for(int y = 175; y < 320; y += 8) {
        for(int x = 0; x < 240; x += 8) {
          tft.drawPixel(x, y, COLOR_BG);
        }
      }
    }
  }

  
  // Draw all active particles with glow
  int activeCount = 0;
  for(int i = 0; i < MAX_PARTICLES; i++) {
    if(particles[i].active && particles[i].life > 0) {
      activeCount++;
      int x = (int)particles[i].x;
      int y = (int)particles[i].y;
      
      // Skip particles that are off-screen
      if(x < -10 || x > 250 || y < -10 || y > 330) {
        continue;
      }
      
      // Skip particles in text areas (adjust based on celebration type)
      if(celebrationType == 6) {
        // Winner celebration - skip central text area only
        if(y >= 70 && y <= 240 && x >= 20 && x <= 220) continue;
      } else {
        // Other celebrations - skip title bar
        if(y >= 130 && y <= 175) continue;
      }
      
      // Draw particle with size based on lifetime
      int size = (int)(2 + particles[i].life * 2);  // 2-4 pixels
      
      // Draw core
      tft.fillCircle(x, y, size, particles[i].color);
      
      // Draw glow ring
      if(particles[i].life > 0.5) {
        tft.drawCircle(x, y, size + 1, particles[i].color);
      }
    }
  }
  
  // Debug: Print active particle count occasionally
  static int debugFrameCount = 0;
  if(debugFrameCount++ % 30 == 0 && activeCount > 0) {
  }
}

// ============================================================================
// DISPLAY FUNCTIONS
// ============================================================================

void drawScreen() {
  tft.fillScreen(COLOR_BG);
  
  if(gameState == STATE_INTRO) {
    drawIntroScreen();
  } else if(gameState == STATE_MENU) { 
    if(inAIDifficultyMenu) {
      drawAIDifficultyMenu();
    } else if(inGraphMenu) {
      drawGraphMenu();
    } else if(inSoundMenu) {
      drawSoundMenu();
    } else if(inToolsMenu) {
      drawToolsMenu();
    } else {
      drawMenu();
    }
  } else if(gameState == STATE_GAME_OVER) {
    drawGameOver();
  } else if(gameState == STATE_CATEGORY_SELECT) {
    drawCategorySelect();
  } else if(gameState == STATE_START) {
    drawStartScreen();
  } else {
    drawGamePlay();
  }
}

void drawStartScreen() {
  // Header - Player and Turn on same line
  tft.setTextSize(2);
  uint16_t color = (currentPlayer == 1) ? COLOR_P1 : COLOR_P2;
  tft.setTextColor(color);
  tft.setCursor(10, 10);
  tft.print("Player ");
  tft.print(currentPlayer);
  
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(115, 10);  // Changed from 130 to 115
  tft.print("Turn:");
  tft.print(playerTurn);
  tft.print("/13");
  
  // Score display - moved higher
  int total1 = calculateTotalWithBonuses(scores1, yahtzeeBonus1);
  int total2 = calculateTotalWithBonuses(scores2, yahtzeeBonus2);
  
  // Highlight current player's score with a box
  if(currentPlayer == 1) {
    tft.fillRect(5, 47, 110, 20, COLOR_P1);  // Box around P1 score
  } else {
    tft.fillRect(125, 47, 110, 20, COLOR_P2);  // Box around P2 score
  }
  
  tft.setTextSize(2);
  tft.setCursor(10, 50);
  tft.setTextColor(currentPlayer == 1 ? COLOR_BG : COLOR_P1);  // Black text if highlighted
  tft.print("P1:");
  tft.print(total1);
  
  tft.setCursor(130, 50);
  tft.setTextColor(currentPlayer == 2 ? COLOR_BG : COLOR_P2);  // Black text if highlighted
  tft.print("P2:");
  tft.print(total2);
  
  // Instructions
  tft.setTextSize(2);
  tft.setCursor(20, 120);
  tft.setTextColor(COLOR_GREEN);
  
  if(vsComputer && currentPlayer == 2) {
    tft.print("Computer's");
    tft.setCursor(20, 150);
    tft.print("Turn...");
  } else {
    tft.print("Press ROLL");
    tft.setCursor(20, 150);
    tft.print("to start");
  }
}

void drawGamePlay() {
  // Header - Player and Turn on same line
  tft.setTextSize(2);
  uint16_t color = (currentPlayer == 1) ? COLOR_P1 : COLOR_P2;
  tft.setTextColor(color);
  tft.setCursor(10, 10);
  tft.print("Player ");
  tft.print(currentPlayer);
  
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(115, 10);  // Changed from 130 to 115
  tft.print("Turn:");
  tft.print(playerTurn);
  tft.print("/13");
  
  // Dice display - centered with actual dice faces
  int diceSize = 30;  // Size of each die box
  int spacing = 14;   // Space between dice (increased from 8 for better visibility)
  int totalWidth = (diceSize * 5) + (spacing * 4);  // Total width of all 5 dice
  int startX = (240 - totalWidth) / 2;  // Center horizontally (screen is 240 wide)
  int yPos = 40;
  
  for(int i = 0; i < 5; i++) {
    int xPos = startX + (i * (diceSize + spacing));
    
    // Draw actual die with dots
    drawGameDie(xPos, yPos, diceSize, dice[i], held[i]);
  }
  
  // Score display - moved down with more space
  int total1 = calculateTotalWithBonuses(scores1, yahtzeeBonus1);
  int total2 = calculateTotalWithBonuses(scores2, yahtzeeBonus2);
  
  // Highlight current player's score with a box
  if(currentPlayer == 1) {
    tft.fillRect(5, 82, 110, 28, COLOR_P1);  // Box around P1 score
  } else {
    tft.fillRect(125, 82, 110, 28, COLOR_P2);  // Box around P2 score
  }
  
  tft.setTextSize(3);  // Enlarged from 2 to 3
  tft.setCursor(10, 85);  // Moved down from 75 to 85
  tft.setTextColor(currentPlayer == 1 ? COLOR_BG : COLOR_P1);  // Black text if highlighted
  tft.print("P1:");
  tft.print(total1);
  
  tft.setCursor(130, 85);  // Moved down from 75 to 85
  tft.setTextColor(currentPlayer == 2 ? COLOR_BG : COLOR_P2);  // Black text if highlighted
  tft.print("P2:");
  tft.print(total2);
  
  // Score section display - toggle with UP/DOWN - LARGER TEXT
  int* scores = (currentPlayer == 1) ? scores1 : scores2;
  
  tft.setTextSize(2);  // Changed from 1 to 2 (1.5x would require custom font)
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(10, 120);  // Moved down from 105 to 120
  
  if(scoreViewSection == 0) {
    // Upper Section
    tft.print("UPPER");
    
    // Show bonus info next to title
    int upperTotal = getUpperSectionTotal(scores);
    tft.setTextSize(1);
    tft.setCursor(90, 123);
    tft.setTextColor(COLOR_TEXT);
    tft.print("Bonus ");
    tft.print(upperTotal);
    tft.print("/63");
    
    tft.setCursor(90, 133);
    tft.setTextColor(COLOR_TEXT);
    if(hasUpperBonus(scores)) {
      tft.print("+35 pts");
    } else {
      tft.print("Need ");
      tft.print(63 - upperTotal);
    }
    
    // Show rolls left
    // While dice are animating the decrement has already happened, but the
    // player hasn't "seen" the result yet — show the pre-decrement value.
    // Once the animation settles drawScreen() is called again with
    // diceAnimating == false and the real value is displayed.
    int displayRolls = (diceAnimating && rollsLeft < 3) ? rollsLeft + 1 : rollsLeft;
    tft.setCursor(180, 123);
    if(displayRolls > 0) {
      tft.setTextColor(COLOR_GREEN);
    } else {
      tft.setTextColor(COLOR_GRAY);
    }
    tft.print("Rolls:");
    tft.setCursor(180, 133);
    tft.print(displayRolls);
    tft.print("/3");
    
    int y = 150;
    tft.setTextSize(2);
    for(int i = 0; i < 6; i++) {
      bool used = (scores[i] != -1);
      
      tft.setTextColor(used ? COLOR_GRAY : COLOR_TEXT);
      tft.setCursor(10, y);
      tft.print(categoryNames[i]);
      
      tft.setCursor(140, y);
      if(used) {
        tft.print(scores[i]);
      } else {
        tft.print("--");
      }
      
      y += 24;
    }
    
  } else {
    // Lower Section
    tft.print("LOWER");
    
    // Always show Yahtzee bonus info (like upper section bonus)
    int* yahtzeeBonus = (currentPlayer == 1) ? &yahtzeeBonus1 : &yahtzeeBonus2;
    tft.setTextSize(1);
    tft.setCursor(90, 123);
    
    if(*yahtzeeBonus > 0) {
      tft.setTextColor(COLOR_GREEN);
      tft.print("Yahtzee x");
      tft.print(*yahtzeeBonus);
      tft.setCursor(90, 133);
      tft.print("+");
      tft.print(*yahtzeeBonus * 100);
      tft.print(" pts");
    } else {
      tft.setTextColor(COLOR_GRAY);
      tft.print("Yahtzee");
      tft.setCursor(90, 133);
      tft.print("bonus: 0");
    }
    
    // Show rolls left (same animation-aware logic as upper section)
    int displayRolls = (diceAnimating && rollsLeft < 3) ? rollsLeft + 1 : rollsLeft;
    tft.setCursor(180, 123);
    if(displayRolls > 0) {
      tft.setTextColor(COLOR_GREEN);
    } else {
      tft.setTextColor(COLOR_GRAY);
    }
    tft.print("Rolls:");
    tft.setCursor(180, 133);
    tft.print(displayRolls);
    tft.print("/3");
    
    int y = 150;  // Moved down from 135 to 150
    tft.setTextSize(2);  // Size 2 for everything
    for(int i = 6; i < 13; i++) {
      bool used = (scores[i] != -1);
      
      tft.setTextColor(used ? COLOR_GRAY : COLOR_TEXT);
      tft.setCursor(10, y);
      tft.print(categoryNames[i]);
      
      tft.setCursor(160, y);
      if(used) {
        tft.print(scores[i]);
      } else {
        tft.print("--");
      }
      
      y += 24;  // Spacing for size 2
    }
  }
  
  // Instructions - removed "Computer thinking..." as it overlaps with headings
  if(!vsComputer || currentPlayer == 1) {
    if(rollsLeft == 0) {
      tft.setTextSize(1);
      tft.setCursor(10, 295);
      tft.setTextColor(COLOR_GREEN);
      tft.print("Selecting category...");
    }
  }
}

void drawCategorySelect() {
  // Header - Player and Turn on same line
  tft.setTextSize(2);
  uint16_t color = (currentPlayer == 1) ? COLOR_P1 : COLOR_P2;
  tft.setTextColor(color);
  tft.setCursor(10, 10);
  tft.print("Player ");
  tft.print(currentPlayer);
  
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(115, 10);  // Changed from 130 to 115
  tft.print("Turn:");
  tft.print(playerTurn);
  tft.print("/13");
  
  // Dice display - compact at top (no held highlighting in selection screen)
  int diceSize = 30;
  int spacing = 14;  // Increased spacing to match gameplay screen
  int totalWidth = (diceSize * 5) + (spacing * 4);
  int startX = (240 - totalWidth) / 2;
  int yPos = 35;
  
  for(int i = 0; i < 5; i++) {
    int xPos = startX + (i * (diceSize + spacing));
    drawGameDie(xPos, yPos, diceSize, dice[i], false);  // Always pass false for held state
  }
  
  // Get player's scores
  int* scores = (currentPlayer == 1) ? scores1 : scores2;
  
  // FOR AI: Re-evaluate best category RIGHT NOW to ensure display is correct
  if(vsComputer && currentPlayer == 2 && !diceAnimating) {
    // Only re-evaluate if we're NOT in the score-view pause (aiLastScoredCategory set means we just scored)
    if(aiLastScoredCategory == -1) {
      selectedCategory = findBestCategoryAI(dice, scores, true);
      if(selectedCategory == -1) selectedCategory = 0;
    }
  }
  
  // Determine which section to show.
  // During the score-view pause, force the section that contains the just-scored category.
  int displayCategory = (aiLastScoredCategory != -1) ? aiLastScoredCategory : selectedCategory;
  bool showUpperSection = (displayCategory <= 5);
  
  // Bonus info line - replaces bottom navigation text
  tft.setTextSize(1);
  tft.setCursor(10, 72);
  
  if(showUpperSection) {
    // Upper section bonus info
    int upperTotal = getUpperSectionTotal(scores);
    tft.setTextColor(COLOR_TEXT);
    tft.print("Bonus: ");
    tft.print(upperTotal);
    tft.print("/63");
    
    if(hasUpperBonus(scores)) {
      tft.setTextColor(COLOR_GREEN);
      tft.print(" +35pts");
    } else {
      tft.setTextColor(COLOR_TEXT);
      tft.print(" Need ");
      tft.print(63 - upperTotal);
    }
  } else {
    // Lower section - Yahtzee bonus info
    int* yahtzeeBonus = (currentPlayer == 1) ? &yahtzeeBonus1 : &yahtzeeBonus2;
    if(*yahtzeeBonus > 0) {
      tft.setTextColor(COLOR_GREEN);
      tft.print("Yahtzee Bonus x");
      tft.print(*yahtzeeBonus);
      tft.print(" = +");
      tft.print(*yahtzeeBonus * 100);
      tft.print("pts");
    } else {
      tft.setTextColor(COLOR_GRAY);
      tft.print("No Yahtzee bonus yet");
    }
  }
  
  // Find best category for highlighting
  int bestCategory = findBestCategory(dice, scores);
  
  // Draw category list - NO SECTION HEADERS, comfortable spacing
  int y = 90;  // Start position
  int lineHeight = 26;  // Increased from 22 to 26 for better readability
  int startIdx = showUpperSection ? 0 : 6;
  int endIdx = showUpperSection ? 6 : 13;
  
  for(int i = startIdx; i < endIdx; i++) {
    bool selected = (i == selectedCategory);
    bool used = (scores[i] != -1);
    bool isBest = (i == bestCategory);
    bool isJustScored = (i == aiLastScoredCategory);  // AI just filed this row
    
    // Cursor bar: show on selected (unused) row, OR on the just-scored row in green
    if(isJustScored) {
      tft.fillRect(5, y - 2, 230, 20, COLOR_GREEN);  // Green bar for just-scored row
    } else if(selected && !used) {
      tft.fillRect(5, y - 2, 230, 20, COLOR_GRAY);
    }
    
    // Draw star for best option (not relevant during score-view pause but harmless)
    tft.setTextSize(2);
    if(isBest && !used) {
      tft.setTextColor(COLOR_GREEN);
      tft.setCursor(10, y);
      tft.print("*");
    }
    
    // Category name
    if(isJustScored) {
      tft.setTextColor(COLOR_BG);   // Black text on green bar for just-scored row
    } else if(isBest && !used) {
      tft.setTextColor(COLOR_GREEN);
    } else {
      tft.setTextColor(used ? COLOR_RED : COLOR_TEXT);
    }
    tft.setCursor(25, y);
    tft.print(categoryNames[i]);
    
    // Show score value
    tft.setCursor(180, y);
    if(isJustScored) {
      // Show the filed score in black (on green bar) — clearly the chosen value
      tft.setTextColor(COLOR_BG);
      tft.print(scores[i]);
    } else if(used) {
      tft.setTextColor(COLOR_RED);
      tft.print(scores[i]);
    } else {
      int score = calculateCategoryScore(dice, i);
      if(isBest) {
        tft.setTextColor(COLOR_GREEN);
      } else if(selected) {
        tft.setTextColor(COLOR_TEXT);
      } else {
        tft.setTextColor(COLOR_GRAY);
      }
      tft.print(score);
    }
    
    y += lineHeight;
  }
  
  // Total scores at bottom OR computer message
  int total1 = calculateTotalWithBonuses(scores1, yahtzeeBonus1);
  int total2 = calculateTotalWithBonuses(scores2, yahtzeeBonus2);
  
  if(vsComputer && currentPlayer == 2) {
    // Show computer selecting message
    tft.setTextSize(1);
    tft.setCursor(10, 295);
    tft.setTextColor(COLOR_P2);
    tft.print("Computer selecting...");
  } else {
    // Show totals
    tft.setTextSize(2);
    tft.setCursor(10, 290);
    tft.setTextColor(currentPlayer == 1 ? COLOR_P1 : COLOR_GRAY);
    tft.print("P1:");
    tft.print(total1);
    
    tft.setCursor(130, 290);
    tft.setTextColor(currentPlayer == 2 ? COLOR_P2 : COLOR_GRAY);
    tft.print("P2:");
    tft.print(total2);
  }
}

// ============================================================================
// GAME OVER ANIMATION HELPERS
// ============================================================================

// Convert HSV to RGB565 color
uint16_t hsvToRgb565(uint8_t h, uint8_t s, uint8_t v) {
  uint8_t r, g, b;
  uint8_t region, remainder, p, q, t;
  
  if (s == 0) {
    r = g = b = v;
  } else {
    region = h / 43;
    remainder = (h - (region * 43)) * 6;
    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
    
    switch (region) {
      case 0: r = v; g = t; b = p; break;
      case 1: r = q; g = v; b = p; break;
      case 2: r = p; g = v; b = t; break;
      case 3: r = p; g = q; b = v; break;
      case 4: r = t; g = p; b = v; break;
      default: r = v; g = p; b = q; break;
    }
  }
  
  return tft.color565(r, g, b);
}

// Initialize confetti particles
// Confetti removed - using pulsing winner badge instead

void drawGameOver() {
  // Calculate totals
  int total1 = calculateTotalWithBonuses(scores1, yahtzeeBonus1);
  int total2 = calculateTotalWithBonuses(scores2, yahtzeeBonus2);
  
  // Determine winner
  int winner = 0;  // 0=tie, 1=P1, 2=P2
  if(total1 > total2) winner = 1;
  else if(total2 > total1) winner = 2;
  
  // Track what's been drawn to avoid redrawing
  static bool headerDrawn = false;
  static bool scorecardDrawn = false;
  static bool footerDrawn = false;
  
  // Initialize animation on first call
  if(!gameOverAnimationStarted) {
    gameOverAnimationStarted = true;
    gameOverStartTime = millis();
    winnerTextScale = 0.0;
    winnerColorHue = 0;
    
    // Reset draw flags for new game over screen
    headerDrawn = false;
    scorecardDrawn = false;
    footerDrawn = false;
    
    // Full clear and draw static elements ONCE
    tft.fillScreen(COLOR_BG);
    
    // Play winner celebration tune
    if(winner != 0) {  // Only if there's a winner (not a tie)
      buzzerWinnerCelebration();
    } else {
      buzzerGameOver();  // Tie game uses regular game over sound
    }
  }
  
  // Calculate animation progress
  unsigned long elapsed = millis() - gameOverStartTime;
  
  // ═══════════════════════════════════════════════════════════
  // HEADER: "FINAL SCORE" - Draw once when time is right
  // ═══════════════════════════════════════════════════════════
  if(elapsed > 300 && !headerDrawn) {
    tft.setTextSize(2);
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(45, 15);
    tft.print("FINAL SCORE");
    
    // Draw separator line
    tft.drawFastHLine(10, 40, 220, COLOR_GRAY);
    headerDrawn = true;
  }
  
  // ═══════════════════════════════════════════════════════════
  // SCORECARD LAYOUT - Draw once when time is right
  // ═══════════════════════════════════════════════════════════
  
  if(elapsed > 600 && !scorecardDrawn) {
    int leftX = 15;   // Player 1 column
    int rightX = 125; // Player 2 column
    int startY = 55;
    
    // --- PLAYER 1 SCORECARD ---
    tft.setTextSize(2);
    tft.setTextColor(COLOR_P1);
    tft.setCursor(leftX, startY);
    tft.print("P1");
    
    // Player 1 total score (large)
    tft.setTextSize(3);
    tft.setCursor(leftX, startY + 25);
    tft.print(total1);
    
    // "pts" label
    tft.setTextSize(1);
    tft.setCursor(leftX, startY + 55);
    tft.setTextColor(COLOR_GRAY);
    tft.print("points");
    
    // Upper section bonus
    int upper1 = getUpperSectionTotal(scores1);
    tft.setCursor(leftX, startY + 75);
    if(hasUpperBonus(scores1)) {
      tft.setTextColor(COLOR_GREEN);
      tft.print("Bonus +35");
    } else {
      tft.setTextColor(COLOR_GRAY);
      tft.print("Upper: ");
      tft.print(upper1);
    }
    
    // Yahtzee bonus count
    tft.setCursor(leftX, startY + 90);
    if(yahtzeeBonus1 > 0) {
      tft.setTextColor(COLOR_HELD);  // Yellow
      tft.print("Ytz x");
      tft.print(yahtzeeBonus1);
    }
    
    // --- PLAYER 2 SCORECARD ---
    tft.setTextSize(2);
    tft.setTextColor(COLOR_P2);
    tft.setCursor(rightX, startY);
    tft.print("P2");
    
    // Player 2 total score (large)
    tft.setTextSize(3);
    tft.setCursor(rightX, startY + 25);
    tft.print(total2);
    
    // "pts" label
    tft.setTextSize(1);
    tft.setCursor(rightX, startY + 55);
    tft.setTextColor(COLOR_GRAY);
    tft.print("points");
    
    // Upper section bonus
    int upper2 = getUpperSectionTotal(scores2);
    tft.setCursor(rightX, startY + 75);
    if(hasUpperBonus(scores2)) {
      tft.setTextColor(COLOR_GREEN);
      tft.print("Bonus +35");
    } else {
      tft.setTextColor(COLOR_GRAY);
      tft.print("Upper: ");
      tft.print(upper2);
    }
    
    // Yahtzee bonus count
    tft.setCursor(rightX, startY + 90);
    if(yahtzeeBonus2 > 0) {
      tft.setTextColor(COLOR_HELD);  // Yellow
      tft.print("Ytz x");
      tft.print(yahtzeeBonus2);
    }
    
    scorecardDrawn = true;
  }
  
  // ═══════════════════════════════════════════════════════════
  // WINNER BADGE - Clean readable badge with gentle float
  // ═══════════════════════════════════════════════════════════

  if(elapsed > 1200) {
    int startY = 55;

    // --- Entrance: badge slides up from below over 400ms ---
    float animProgress = min(1.0f, (elapsed - 1200) / 400.0f);
    // Ease-out: fast start, smooth settle
    float eased = 1.0f - (1.0f - animProgress) * (1.0f - animProgress);

    // --- Gentle float after entrance: ±3px slow drift ---
    float floatOffset = 0.0f;
    if(animProgress >= 1.0f) {
      float floatPhase = (elapsed - 1600) / 1800.0f;  // Full cycle every 1.8s
      floatOffset = 3.0f * sin(floatPhase * 2.0f * 3.14159f);
    }

    // Resting Y position for badge
    int restY = startY + 110;

    // Slide starts 18px below rest, eases up to rest
    int slideOffset = (int)((1.0f - eased) * 18.0f);
    int badgeY = restY + slideOffset + (int)floatOffset;

    // Fixed badge size - no scaling, always readable
    const int badgeWidth  = 90;
    const int badgeHeight = 28;

    // Clear the badge strip (tall enough to cover full float range)
    tft.fillRect(10, restY - 6, 220, badgeHeight + 16, COLOR_BG);

    if(winner == 1) {
      int badgeX = 13;  // Left column

      // Player-colour outline ring
      tft.fillRoundRect(badgeX - 2, badgeY - 2, badgeWidth + 4, badgeHeight + 4, 6, COLOR_P1);
      // Dark fill so white text pops
      tft.fillRoundRect(badgeX, badgeY, badgeWidth, badgeHeight, 5, COLOR_BG);
      // Player-colour border
      tft.drawRoundRect(badgeX, badgeY, badgeWidth, badgeHeight, 5, COLOR_P1);

      // WINNER text - white, steady, always readable
      tft.setTextSize(2);
      tft.setTextColor(COLOR_TEXT);
      tft.setCursor(badgeX + 9, badgeY + 6);
      tft.print("WINNER");

    } else if(winner == 2) {
      int badgeX = 123;  // Right column

      tft.fillRoundRect(badgeX - 2, badgeY - 2, badgeWidth + 4, badgeHeight + 4, 6, COLOR_P2);
      tft.fillRoundRect(badgeX, badgeY, badgeWidth, badgeHeight, 5, COLOR_BG);
      tft.drawRoundRect(badgeX, badgeY, badgeWidth, badgeHeight, 5, COLOR_P2);

      tft.setTextSize(2);
      tft.setTextColor(COLOR_TEXT);
      tft.setCursor(badgeX + 9, badgeY + 6);
      tft.print("WINNER");

    } else {
      // Tie game - draw once only, no animation
      static bool tieDrawn = false;
      if(!tieDrawn) {
        tft.setTextSize(3);
        tft.setTextColor(COLOR_GREEN);
        tft.setCursor(70, restY + 4);
        tft.print("TIE!");
        tieDrawn = true;
      }
    }
  }
  
  // ═══════════════════════════════════════════════════════════
  // FOOTER: Play again prompt - Draw once when time is right
  // ═══════════════════════════════════════════════════════════
  if(elapsed > 1800 && !footerDrawn) {
    tft.drawFastHLine(10, 205, 220, COLOR_GRAY);
    
    tft.setTextSize(1);
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(35, 220);
    tft.print("Press ROLL to play again");
    
    // Show high score if this is a new record
    if(total1 > highScore || total2 > highScore) {
      tft.setTextColor(COLOR_HELD);  // Yellow
      tft.setCursor(50, 235);
      tft.print("NEW HIGH SCORE!");
    }
    
    footerDrawn = true;
  }
  
  // Update statistics (only once per game over screen)
  if(!statsUpdated) {
    // Update high score
    if(total1 > highScore) {
      highScore = total1;
    }
    if(total2 > highScore) {
      highScore = total2;
    }
    
    // Update win/loss record
    if(total1 > total2) {
      p1Wins++;
    } else if(total2 > total1) {
      p2Wins++;
    } else {
      totalTies++;
    }
    
    // Update most Yahtzees
    int gameYahtzees = currentGameYahtzees1 + currentGameYahtzees2;
    if(gameYahtzees > mostYahtzees) {
      mostYahtzees = gameYahtzees;
    }
    
    // Save all statistics to EEPROM
    EEPROM.write(EEPROM_HIGH_SCORE_ADDR, highScore >> 8);
    EEPROM.write(EEPROM_HIGH_SCORE_ADDR + 1, highScore & 0xFF);
    EEPROM.write(EEPROM_P1_WINS_ADDR, p1Wins >> 8);
    EEPROM.write(EEPROM_P1_WINS_ADDR + 1, p1Wins & 0xFF);
    EEPROM.write(EEPROM_P2_WINS_ADDR, p2Wins >> 8);
    EEPROM.write(EEPROM_P2_WINS_ADDR + 1, p2Wins & 0xFF);
    EEPROM.write(EEPROM_TIES_ADDR, totalTies >> 8);
    EEPROM.write(EEPROM_TIES_ADDR + 1, totalTies & 0xFF);
    EEPROM.write(EEPROM_MOST_YAHTZEES_ADDR, mostYahtzees);
    EEPROM.commit();
    
    // Update AI learning if playing against computer
    if(vsComputer) {
      bool aiWon = (total2 > total1);
      
      
      updateAILearning(aiWon, total2, total1);
      Serial.print("AI: ");
      Serial.print(aiWon ? "WIN" : "LOSS");
      Serial.print(" (");
      Serial.print(aiLearning.gamesPlayed + 1);
      Serial.println(" games)");
      
      saveAILearningToFile();
      delay(100);
    }
    
    statsUpdated = true;
  }
}

// ============================================================================
// LITTLEFS-BASED PERSISTENCE (REPLACES EEPROM)
// ============================================================================

void saveAILearningToFile() {
  
  // **CRITICAL CHECK**: Verify structure fits in allocated EEPROM space
  size_t structSize = sizeof(AILearningData);
  size_t availableSpace = EEPROM_STRATEGY_VARIANTS_ADDR - EEPROM_AI_LEARNING_START;
  
  Serial.println("=== AI LEARNING DATA SAVE ===");
  Serial.print("Structure size: ");
  Serial.print(structSize);
  Serial.println(" bytes");
  Serial.print("Available space: ");
  Serial.print(availableSpace);
  Serial.println(" bytes");
  
  if(structSize > availableSpace) {
    Serial.println("╔════════════════════════════════════════════╗");
    Serial.println("║  ERROR: AILearningData TOO LARGE!          ║");
    Serial.println("╚════════════════════════════════════════════╝");
    Serial.print("Required: ");
    Serial.print(structSize);
    Serial.print(" bytes, Available: ");
    Serial.print(availableSpace);
    Serial.println(" bytes");
    Serial.print("Overflow: ");
    Serial.print(structSize - availableSpace);
    Serial.println(" bytes");
    Serial.println("");
    Serial.println("SOLUTION: Increase EEPROM_STRATEGY_VARIANTS_ADDR");
    Serial.print("Current value: ");
    Serial.println(EEPROM_STRATEGY_VARIANTS_ADDR);
    Serial.print("Recommended value: ");
    Serial.println(EEPROM_AI_LEARNING_START + ((structSize + 63) & ~63));  // Round up to 64-byte boundary
    Serial.println("Update line 69 in the code.");
    Serial.println("");
    return;  // Don't save corrupted data
  }
  
  Serial.print("✓ Size check passed (");
  Serial.print(availableSpace - structSize);
  Serial.println(" bytes free)");
  
  // CRITICAL: The checksum field itself should be zeroed before calculating
  aiLearning.checksum = 0;
  
  // DEBUG: Show what we're about to save
  
  // Calculate checksum
  aiLearning.checksum = calculateChecksum(&aiLearning);
  
  // Write AI learning data to EEPROM
  uint8_t* dataPtr = (uint8_t*)&aiLearning;
  int addr = EEPROM_AI_LEARNING_START;
  
  for(size_t i = 0; i < sizeof(AILearningData); i++) {
    EEPROM.write(addr + i, dataPtr[i]);
  }
  
  // Commit to flash
  bool success = EEPROM.commit();
  
  if(!success) {
    Serial.println("✗ AI save failed!");
  } else {
    Serial.println("✓ AI save successful!");
  }
  
  // Verify
  delay(100);
  AILearningData temp;
  uint8_t* tempPtr = (uint8_t*)&temp;
  addr = EEPROM_AI_LEARNING_START;
  
  for(size_t i = 0; i < sizeof(AILearningData); i++) {
    tempPtr[i] = EEPROM.read(addr + i);
  }
  
  
  if(temp.gamesPlayed == aiLearning.gamesPlayed) {
    Serial.println("✓ Verification passed");
  } else {
    Serial.println("✗ Verification failed!");
  }
  Serial.println("=============================");
}

void loadAILearningFromFile() {
  
  // Read AI learning data from EEPROM (don't memset - we need to read first)
  uint8_t* dataPtr = (uint8_t*)&aiLearning;
  int addr = EEPROM_AI_LEARNING_START;
  
  for(size_t i = 0; i < sizeof(AILearningData); i++) {
    dataPtr[i] = EEPROM.read(addr + i);
  }
  
  // DEBUG: Show what we loaded
  
  
  // Verify checksum
  uint8_t calculatedChecksum = calculateChecksum(&aiLearning);
  
  
  if(aiLearning.checksum != calculatedChecksum || aiLearning.gamesPlayed > 10000) {
    Serial.println("AI data reset (invalid/first boot)");
    resetAILearningData();
    return;
  }
  
  Serial.print("AI: ");
  Serial.print(aiLearning.gamesPlayed);
  Serial.println(" games played");
}

  void resetAILearningData() {
  // CRITICAL: Zero ALL bytes including padding to ensure consistent checksums
  memset(&aiLearning, 0, sizeof(AILearningData));
  
  aiLearning.gamesPlayed = 0;
  aiLearning.aggressiveWins = 0;
  aiLearning.conservativeWins = 0;
  aiLearning.totalSelfPlayGames = 0;
  aiLearning.weightUpperBonus = 50;       // 1.0x default (neutral, applied as /50.0 everywhere)
  
  // Set default strategy weights
  aiLearning.weightYahtzee = 20;          // 2.0x
  
  // Reset performance tracking
  
  // **NEW: Initialize category usage patterns**
  for(int i = 0; i < 13; i++) {
    aiLearning.categoryScoredCount[i] = 0;
    aiLearning.categoryWinsWhenScored[i] = 0;
  }
  
  // **NEW: Initialize upper bonus granularity**
  
  // **NEW: Initialize turn-phase performance**
  
  // **NEW: Initialize risk/reward tracking**
  aiLearning.highRiskAttempts = 0;
  aiLearning.highRiskSuccesses = 0;
  aiLearning.highRiskWins = 0;
  
  // **ENHANCED: Initialize roll timing metrics**
  for(int i = 0; i < 13; i++) {
    aiLearning.firstRollScores[i] = 0;
    aiLearning.secondRollScores[i] = 0;
    aiLearning.thirdRollScores[i] = 0;
    aiLearning.categoryAvgScore[i] = 0;
    aiLearning.categoryScoreSum[i] = 0;
  }
  for(int i = 0; i < 4; i++) {
    aiLearning.rollCountUsed[i] = 0;
  }
  
  // **ENHANCED: Initialize hold pattern metrics**
  aiLearning.holdPattern0Dice = 0;
  aiLearning.holdPattern1Dice = 0;
  aiLearning.holdPattern2Dice = 0;
  aiLearning.holdPattern3Dice = 0;
  aiLearning.holdPattern4Dice = 0;
  aiLearning.holdPattern5Dice = 0;
  
  // **ENHANCED: Initialize bonus pursuit analytics**
  
  // **ENHANCED: Initialize opponent modeling**
  
  // **ENHANCED: Initialize endgame scenarios**
  aiLearning.endgameComebackAttempts = 0;
  
  // **ENHANCED: Initialize category timing**
  aiLearning.optimalTurnForYahtzee = 0;
  aiLearning.optimalTurnForLargeStraight = 0;
  
  // **NEW: Initialize hold pattern value tracking**
  for(int v = 0; v < 6; v++) {
    for(int c = 0; c < 6; c++) {
      aiLearning.holdValueCount[v][c] = 0;
      aiLearning.holdValueScoreSum[v][c] = 0;
      aiLearning.holdValueSuccess[v][c] = 0;
    }
  }
  
  // **NEW: Initialize reroll decision quality tracking**
  
  // **NEW: Initialize category tracking enhancements**
  for(int i = 0; i < 13; i++) {
    aiLearning.categoryOptimalScore[i] = 0;
  }
  
  // Reset decision history
  decisionHistoryCount = 0;
  decisionHistoryIndex = 0;
  for(int i = 0; i < MAX_DECISION_HISTORY; i++) {
    decisionHistory[i].turnNumber = 0;
    decisionHistory[i].rollNumber = 0;
    decisionHistory[i].diceHeld = 0;
    decisionHistory[i].categoryScored = 0;
    decisionHistory[i].pointsScored = 0;
    decisionHistory[i].wonGame = false;
    decisionHistory[i].leadingWhenDecided = false;
  }
  
  
  // **V39: Initialize enhanced graph metrics**
  
  
  
  for(int i = 0; i < 10; i++) {
    aiLearning.scoreRanges[i] = 0;
  }
  

  // Reset new decision-quality stats
  for(int i = 0; i < 4; i++) {
    aiLearning.chanceTurnBucket[i] = 0;
    aiLearning.chanceScoreSum[i]   = 0;
  }
  for(int i = 0; i < 6; i++) {
    aiLearning.upperSacrificeCount[i] = 0;
    aiLearning.upperSacrificeBonus[i] = 0;
  }
  aiLearning.scoreDiffSumTurn10    = 0;
  aiLearning.gamesReachingTurn10   = 0;
  for(int i = 0; i < 13; i++) {
    aiLearning.categoryAvgTurn[i]       = 0;
    aiLearning.categoryEarlyScoreSum[i] = 0;
  }

  // Reset per-mode aggressiveness weights to their defaults.
  // Each difficulty's weight is stored independently — resetting learning
  // data should also reset these so the AI starts fresh on each mode.
  EEPROM.write(EEPROM_AGGR_WEIGHT_ADDR + 0, 40);  // Normal default: 0.40
  EEPROM.write(EEPROM_AGGR_WEIGHT_ADDR + 1, 70);  // Hard default:   0.70
  // God Mode (index 2) is fixed at 0.95 — no reset needed
  EEPROM.commit();

  // **CRITICAL: Save the reset data immediately to EEPROM**
saveAILearningToFile();
}

// ============================================================================
// STAT EXPORT FUNCTION - Export all AI learning data to Serial Monitor
// ============================================================================
void exportStatsToSerial() {
  Serial.println();
  Serial.println("====== COPY INSTRUCTIONS ======");
  Serial.println("1. Select all text below (from CATEGORY STATS to END)");
  Serial.println("2. Copy to clipboard");
  Serial.println("3. Open Excel or Google Sheets");
  Serial.println("4. Paste into cell A1");
  Serial.println("5. Excel should auto-detect columns");
  Serial.println("   If not: Data > Text to Columns > Delimited > Comma");
  Serial.println("===============================");
  Serial.println();
  Serial.print("Export Time (ms): ");
  Serial.println(millis());
  Serial.println();
  Serial.println("====== BEGIN CSV DATA ======");
  Serial.println();
  
  // ============ BASIC PERFORMANCE ============
  Serial.println("BASIC PERFORMANCE,,,,");
  Serial.println("Metric,Value,,,");
  
  Serial.print("Total Games,");
  Serial.println(aiLearning.gamesPlayed);
  
  Serial.print("Aggressive Wins,");
  Serial.println(aiLearning.aggressiveWins);
  
  Serial.print("Conservative Wins,");
  Serial.println(aiLearning.conservativeWins);
  
  int totalWins = aiLearning.aggressiveWins + aiLearning.conservativeWins;
  int totalLosses = aiLearning.gamesPlayed - totalWins;
  Serial.print("Total Wins,");
  Serial.println(totalWins);
  
  Serial.print("Total Losses,");
  Serial.println(totalLosses);
  
  Serial.print("Win Rate %,");
  if(aiLearning.gamesPlayed > 0) {
    Serial.println((totalWins * 100.0) / aiLearning.gamesPlayed, 1);
  } else {
    Serial.println(0);
  }
  
  Serial.print("Self-Play Training Games,");
  Serial.println(aiLearning.totalSelfPlayGames);
  
  Serial.println();
  
  // ============ CATEGORY PERFORMANCE ============
  Serial.println(",,,,");
  Serial.println("CATEGORY STATS,,,,");
  Serial.println("Category,Times Scored,Times Won,Win Rate %,Avg Score");
  
  const char* categoryNames[] = {
    "Ones", "Twos", "Threes", "Fours", "Fives", "Sixes",
    "3-of-Kind", "4-of-Kind", "Full House", "Sm Straight",
    "Lg Straight", "Yahtzee", "Chance"
  };
  
  for(int i = 0; i < 13; i++) {
    Serial.print(categoryNames[i]);
    Serial.print(",");
    Serial.print(aiLearning.categoryScoredCount[i]);
    Serial.print(",");
    Serial.print(aiLearning.categoryWinsWhenScored[i]);
    Serial.print(",");
    if(aiLearning.categoryScoredCount[i] > 0) {
      int winRate = (aiLearning.categoryWinsWhenScored[i] * 100) / 
                    aiLearning.categoryScoredCount[i];
      Serial.print(winRate);
    } else {
      Serial.print(0);
    }
    Serial.print(",");
    Serial.println(aiLearning.categoryAvgScore[i] * 2);  // Convert back from /2 storage
  }
  
  Serial.println(",,,,");
  
  // ============ ROLL TIMING ============
  Serial.println("ROLL TIMING OPTIMIZATION,,");
  Serial.println("Roll Number,Times Used,Percentage");
  
  uint16_t totalRolls = aiLearning.rollCountUsed[1] + aiLearning.rollCountUsed[2] + aiLearning.rollCountUsed[3];
  
  Serial.print("Roll 1 (scored immediately),");
  Serial.print(aiLearning.rollCountUsed[1]);
  Serial.print(",");
  if(totalRolls > 0) {
    Serial.println((aiLearning.rollCountUsed[1] * 100.0) / totalRolls, 1);
  } else {
    Serial.println(0);
  }
  
  Serial.print("Roll 2,");
  Serial.print(aiLearning.rollCountUsed[2]);
  Serial.print(",");
  if(totalRolls > 0) {
    Serial.println((aiLearning.rollCountUsed[2] * 100.0) / totalRolls, 1);
  } else {
    Serial.println(0);
  }
  
  Serial.print("Roll 3 (used all rolls),");
  Serial.print(aiLearning.rollCountUsed[3]);
  Serial.print(",");
  if(totalRolls > 0) {
    Serial.println((aiLearning.rollCountUsed[3] * 100.0) / totalRolls, 1);
  } else {
    Serial.println(0);
  }
  
  Serial.println(",,,");
  
  // ============ HOLD PATTERNS ============
  Serial.println("HOLD PATTERN EFFECTIVENESS,,");
  Serial.println("Dice Held,Times Used,Percentage");
  
  uint16_t totalHolds = aiLearning.holdPattern0Dice + aiLearning.holdPattern1Dice + 
                        aiLearning.holdPattern2Dice + aiLearning.holdPattern3Dice +
                        aiLearning.holdPattern4Dice + aiLearning.holdPattern5Dice;
  
  for(int i = 0; i <= 5; i++) {
    Serial.print(i);
    Serial.print(" dice held,");
    
    uint16_t count = 0;
    switch(i) {
      case 0: count = aiLearning.holdPattern0Dice; break;
      case 1: count = aiLearning.holdPattern1Dice; break;
      case 2: count = aiLearning.holdPattern2Dice; break;
      case 3: count = aiLearning.holdPattern3Dice; break;
      case 4: count = aiLearning.holdPattern4Dice; break;
      case 5: count = aiLearning.holdPattern5Dice; break;
    }
    
    Serial.print(count);
    Serial.print(",");
    if(totalHolds > 0) {
      Serial.println((count * 100.0) / totalHolds, 1);
    } else {
      Serial.println(0);
    }
  }
  
  Serial.println(",,");
  
  // ============ RISK/REWARD ============
  Serial.println("RISK/REWARD TRACKING,,");
  Serial.println("Metric,Value,");
  
  Serial.print("High Risk Attempts,");
  Serial.println(aiLearning.highRiskAttempts);
  
  Serial.print("High Risk Successes,");
  Serial.println(aiLearning.highRiskSuccesses);
  
  Serial.print("High Risk Success Rate %,");
  if(aiLearning.highRiskAttempts > 0) {
    Serial.println((aiLearning.highRiskSuccesses * 100.0) / aiLearning.highRiskAttempts, 1);
  } else {
    Serial.println(0);
  }
  
  Serial.print("High Risk Wins,");
  Serial.println(aiLearning.highRiskWins);
  
  Serial.println(",,,,,");
  
  Serial.println();
  Serial.println("====== END CSV DATA ======");
  Serial.println();
  Serial.println("Export complete! Copy the data above into Excel.");
  Serial.println();
}

uint8_t calculateChecksum(AILearningData* data) {
  uint8_t checksum = 0;
  uint8_t* dataPtr = (uint8_t*)data;
  
  // XOR all bytes up to (but not including) the checksum field.
  // Using offsetof() so this is correct regardless of struct packing or padding.
  size_t checksumOffset = offsetof(AILearningData, checksum);
  for(size_t i = 0; i < checksumOffset; i++) {
    checksum ^= dataPtr[i];
  }
  
  return checksum;
}

// ============================================================================
// V39: TRACK CATEGORY EFFICIENCY FOR ENHANCED GRAPHS
// ============================================================================


void updateAILearning(bool aiWon, int aiScore, int humanScore) {
  // === LEARNING REMOVED ===
  // The AI plays a provably-optimal fixed policy; it does not learn, drift, or
  // tune weights. This records only honest career stats: games, wins, the
  // score-distribution histogram, and the rolling last-10 head-to-head scores.
  aiLearning.gamesPlayed++;
  if(aiWon) aiLearning.aggressiveWins++;                 // total AI wins (stats screen reads aggressiveWins+conservativeWins)
  { int _b = aiScore / 50; if(_b < 0) _b = 0; if(_b > 9) _b = 9; aiLearning.scoreRanges[_b]++; }
  // rolling last-10 head-to-head scores (ring buffer)
  { int _a = aiScore < 0 ? 0 : (aiScore > 65535 ? 65535 : aiScore);
    int _h = humanScore < 0 ? 0 : (humanScore > 65535 ? 65535 : humanScore);
    uint8_t _idx = aiLearning.recentHead % 10;
    aiLearning.recentAIScores[_idx] = (uint16_t)_a;
    aiLearning.recentHumanScores[_idx] = (uint16_t)_h;
    aiLearning.recentHead = (_idx + 1) % 10;
    if(aiLearning.recentCount < 10) aiLearning.recentCount++; }

  // Lifetime head-to-head stats. scores1 = human, scores2 = AI (final cards).
  { int _a = aiScore    < 0 ? 0 : aiScore;
    int _h = humanScore < 0 ? 0 : humanScore;
    if(aiLearning.aiScoreSum    < 0xFFFFFFFFUL - _a) aiLearning.aiScoreSum    += _a;
    if(aiLearning.humanScoreSum < 0xFFFFFFFFUL - _h) aiLearning.humanScoreSum += _h;
    if(_a > aiLearning.aiHighScore)    aiLearning.aiHighScore    = (uint16_t)_a;
    if(_h > aiLearning.humanHighScore) aiLearning.humanHighScore = (uint16_t)_h;
    if(getUpperSectionTotal(scores2) >= 63) aiLearning.aiBonusGames++;
    if(getUpperSectionTotal(scores1) >= 63) aiLearning.humanBonusGames++;
    if(scores2[11] == 50) aiLearning.aiYahtzeeGames++;
    if(scores1[11] == 50) aiLearning.humanYahtzeeGames++;
    if(humanScore > aiScore) aiLearning.humanWins++; }
}

void recordDecision(int turnNum, int rollNum, int diceHeldCount, int category, int points, bool leading) {
  // Store in circular buffer
  decisionHistory[decisionHistoryIndex].turnNumber = turnNum;
  decisionHistory[decisionHistoryIndex].rollNumber = rollNum;
  decisionHistory[decisionHistoryIndex].diceHeld = diceHeldCount;
  decisionHistory[decisionHistoryIndex].categoryScored = category;
  decisionHistory[decisionHistoryIndex].pointsScored = points;
  decisionHistory[decisionHistoryIndex].wonGame = false;  // Updated at game end
  decisionHistory[decisionHistoryIndex].leadingWhenDecided = leading;
  
  decisionHistoryIndex = (decisionHistoryIndex + 1) % MAX_DECISION_HISTORY;
  if(decisionHistoryCount < MAX_DECISION_HISTORY) {
    decisionHistoryCount++;
  }
  
  // Track optimal score for this category (already tracked in selectCategory via categoryScoreSum)
  if(category >= 0 && category < 13 && points >= 0) {
    // Update optimal score if this is better
    if(points / 2 > aiLearning.categoryOptimalScore[category]) {
      aiLearning.categoryOptimalScore[category] = points / 2;
    }
  }
}

// **NEW: Track hold pattern values**

// **NEW: Update hold pattern success after scoring**

// **NEW: Helper function to get best available category score**
// Used for reroll decision quality tracking

// **NEW: Track reroll decision quality**

// **NEW: Get learned hold pattern quality for decision making**
float getHoldPatternQuality(int value, int count) {
  if(value < 1 || value > 6 || count < 1 || count > 5) return 0.5;
  
  int vIdx = value - 1;
  int cIdx = count - 1;
  
  uint16_t timesHeld = aiLearning.holdValueCount[vIdx][cIdx];
  if(timesHeld < 3) return 0.5;  // Not enough data, neutral
  
  // Calculate success rate (times it led to 20+ points)
  float successRate = (float)aiLearning.holdValueSuccess[vIdx][cIdx] / timesHeld;
  
  // Calculate average score
  float avgScore = (float)aiLearning.holdValueScoreSum[vIdx][cIdx] / timesHeld;
  
  // Combine success rate and average score into quality metric (0.0 to 1.0+)
  float quality = (successRate * 0.6) + (avgScore / 40.0 * 0.4);
  
  return quality;
}

// **NEW: Get reroll recommendation based on learning**

// **NEW: Get category win rate for decision weighting**

// **NEW: Get category average score**


// ============================================================================
// AI EXPECTED VALUE CALCULATIONS
// ============================================================================




int findBestCategoryAI(int dice[], int scores[], bool useExpectedValue) {
  // Thin wrapper — the optimal V-brain does the work. Kept so existing callers work.
  return yzAI_chooseCategory(dice, scores);
}

// ============================================================================
// BUZZER FUNCTIONS
// ============================================================================

void buzzerBeep(int duration) {
  if(!soundEnabled) return;  // Exit if sound is disabled
  
  // Volume control via duration (1=short, 2=medium, 3=long)
  // Volume 1: 33% duration (quieter feel)
  // Volume 2: 67% duration (medium feel)
  // Volume 3: 100% duration (loudest feel)
  int volumeMultiplier;
  if(volume == 1) {
    volumeMultiplier = 33;  // Short beep
  } else if(volume == 2) {
    volumeMultiplier = 67;  // Medium beep
  } else {  // volume == 3
    volumeMultiplier = 100;  // Long beep
  }
  
  int adjustedDuration = (duration * volumeMultiplier) / 100;
  if(adjustedDuration > 0) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(adjustedDuration);
    digitalWrite(BUZZER_PIN, LOW);
  }
}

void buzzerTone(int frequency, int duration) {
  if(!soundEnabled) return;  // Exit if sound is disabled
  
  // Volume control via duration (1=short, 2=medium, 3=long)
  // Volume 1: 33% duration (quieter feel)
  // Volume 2: 67% duration (medium feel)
  // Volume 3: 100% duration (loudest feel)
  int volumeMultiplier;
  if(volume == 1) {
    volumeMultiplier = 33;  // Short tone
  } else if(volume == 2) {
    volumeMultiplier = 67;  // Medium tone
  } else {  // volume == 3
    volumeMultiplier = 100;  // Long tone
  }
  
  int adjustedDuration = (duration * volumeMultiplier) / 100;
  
  // Generate a tone at the specified frequency
  int period = 1000000 / frequency;  // Period in microseconds
  int cycles = (adjustedDuration * 1000) / period;
  
  // 50% duty cycle for consistent tone quality
  int highTime = period / 2;
  int lowTime = period / 2;
  
  for(int i = 0; i < cycles; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delayMicroseconds(highTime);
    digitalWrite(BUZZER_PIN, LOW);
    delayMicroseconds(lowTime);
  }
}

void buzzerStartRoll() {
  if(!soundEnabled) return;  // Exit if sound is disabled
  
  // Quick ascending beep for roll
  buzzerTone(800, 50);
  delay(20);
  buzzerTone(1200, 50);
}

void buzzerYahtzee() {
  if(!soundEnabled) return;  // Exit if sound is disabled
  
  // Celebratory sound for Yahtzee
  buzzerTone(1000, 100);
  delay(50);
  buzzerTone(1200, 100);
  delay(50);
  buzzerTone(1500, 100);
  delay(50);
  buzzerTone(2000, 200);
}

void buzzerError() {
  if(!soundEnabled) return;  // Exit if sound is disabled
  
  // Error sound (low tone)
  buzzerTone(300, 100);
  delay(50);
  buzzerTone(250, 100);
}

void buzzerStartupTune() {
  if(!soundEnabled) return;
  
  // Fun startup jingle - "Yahtzee" melody
  // Y-AH-TZEE pattern
  buzzerTone(523, 150);  // C
  delay(50);
  buzzerTone(659, 150);  // E
  delay(50);
  buzzerTone(784, 200);  // G
  delay(100);
  buzzerTone(1047, 250); // High C
}

void buzzerMenuMove() {
  if(!soundEnabled) return;
  
  // Quick soft blip for menu navigation
  buzzerTone(800, 20);
}

void buzzerMenuSelect() {
  if(!soundEnabled) return;
  
  // Confirmation beep for menu selection
  buzzerTone(1000, 40);
  delay(20);
  buzzerTone(1200, 60);
}

void buzzerDiceHold() {
  if(!soundEnabled) return;
  
  // Click sound for holding a die
  buzzerTone(1200, 30);
}

void buzzerDiceUnhold() {
  if(!soundEnabled) return;
  
  // Different click for unholding
  buzzerTone(900, 30);
}

void buzzerCategorySelect() {
  if(!soundEnabled) return;
  
  // Confirmation for category selection
  buzzerTone(600, 50);
  delay(30);
  buzzerTone(800, 50);
  delay(30);
  buzzerTone(1000, 80);
}

void buzzerGameOver() {
  if(!soundEnabled) return;
  
  // Game over fanfare
  buzzerTone(659, 200);  // E
  delay(50);
  buzzerTone(587, 200);  // D
  delay(50);
  buzzerTone(523, 200);  // C
  delay(100);
  buzzerTone(523, 400);  // C (longer)
}

void buzzerWinnerCelebration() {
  if(!soundEnabled) return;
  
  // Victory fanfare - uplifting and celebratory
  // Triumph melody
  buzzerTone(523, 150);  // C
  delay(20);
  buzzerTone(659, 150);  // E
  delay(20);
  buzzerTone(784, 150);  // G
  delay(20);
  buzzerTone(1047, 200); // High C
  delay(100);
  
  // Second phrase
  buzzerTone(988, 150);  // B
  delay(20);
  buzzerTone(1047, 150); // C
  delay(20);
  buzzerTone(1175, 150); // D
  delay(20);
  buzzerTone(1319, 300); // E (triumphant ending)
  delay(50);
  
  // Final flourish
  buzzerTone(1047, 100); // High C
  delay(20);
  buzzerTone(1319, 400); // E (longer, victorious)
}

// ============================================================================
// AI SELF-PLAY TRAINING SYSTEM
// ============================================================================

