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
int selectedToolItem = 0;  // 0=7seg, 1=sound, 2=auto-advance, 3=stats, 4=AI stats, 5=AI speed, 6=train, 7=export, 8=graphs, 9=help, 10=back
int selectedSoundItem = 0; // 0 = Sound On/Off, 1 = Volume, 2 = Back
int aiStatsPage = 0;  // 0 = page 1, 1 = page 2, 2 = page 3 (strategy variants)
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

// ============================================================================
// AI LEARNING SYSTEM
// ============================================================================

// AI Learning data structure - stores performance metrics in EEPROM
// Strategy variant structure - MUST be defined before AILearningData
struct __attribute__((packed)) StrategyVariant {
  uint8_t weightYahtzee;
  uint8_t weightLargeStraight;
  uint8_t weightSmallStraight;
  uint8_t weightFullHouse;
  uint8_t weight3OfKindLow;
  uint8_t weight3OfKindHigh;
  uint8_t weightStraightHold;
  uint8_t weightUpperBonus;
  
  // Performance tracking
  uint16_t wins;
  uint16_t losses;
  uint16_t totalScore;
  uint16_t gamesPlayed;
  
  float getWinRate() {
    if(gamesPlayed == 0) return 0.5;
    return (float)wins / gamesPlayed;
  }
  
  int getAvgScore() {
    if(gamesPlayed == 0) return 0;
    return totalScore / gamesPlayed;
  }
};

// Analyze straight potential and return hold strategy
struct StraightAnalysis {
  bool shouldPursueStraight;
  bool holds[5];
  int consecutiveLength;
  int startValue;
  int missingCount;
  float expectedValue;
};

struct EndgameStrategy {
  bool useAggressiveStrategy;
  int targetCategory;
  float targetPriority;
  const char* description;
};

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
  uint16_t yahtzeeAttempts;       // Times AI went for Yahtzee (3+ of kind with rolls left)
  uint16_t yahtzeeSuccesses;      // Times AI successfully got Yahtzee
  uint16_t straightAttempts;      // Times AI went for straights
  uint16_t straightSuccesses;     // Times AI successfully got straights
  uint8_t avgWinningScore;        // Average winning score / 3 (0-255 range)
  
  // Enhanced learning metrics
  uint16_t earlyYahtzees;         // Yahtzees scored in turns 1-5
  uint16_t lateYahtzees;          // Yahtzees scored in turns 10-13
  uint16_t bonusAchieved;         // Times got 63+ upper bonus
  uint16_t pairHoldAttempts;      // Times held just a pair
  uint16_t pairHoldSuccesses;     // Times holding pair led to 20+ score
  uint16_t rerollOnGoodHand;      // Times rerolled with 25+ pts available
  uint16_t rerollSuccesses;       // Times that improved the score
  uint16_t totalSelfPlayGames;    // Total self-play training games
  
  // **NEW: Priority 1 - Category Usage Patterns**
  uint16_t categoryScoredCount[13];     // How many times each category was scored (vs human only)
  uint16_t categoryWinsWhenScored[13];  // How many times AI won when scoring each category (vs human only)
  
  // **NEW: Priority 2 - Upper Bonus Granularity**
  uint16_t bonusAchievedVsHuman;   // Times got 63+ when playing vs human
  uint16_t bonusAchievedAndWon;    // Times got bonus AND won vs human
  uint16_t noBonusButWon;          // Times won WITHOUT bonus vs human
  
  // **NEW: Priority 3 - Turn-Phase Performance**
  uint16_t earlyGameWins;          // Wins when leading after turn 4 (vs human)
  uint16_t earlyGameLosses;        // Losses when leading after turn 4 (vs human)
  uint16_t lateGameComebacks;      // Wins when trailing after turn 9 (vs human)
  
  // **NEW: Risk/Reward Tracking**
  uint16_t highRiskAttempts;       // Times AI went for low-probability high-value plays
  uint16_t highRiskSuccesses;      // Times high-risk move paid off
  uint16_t highRiskWins;           // Games won after successful high-risk move
  
  // NEW: Strategy weight learning (stored as uint8_t * 10 for precision)
  uint8_t weightYahtzee;          // Yahtzee priority weight * 10 (default 20 = 2.0x)
  uint8_t weightLargeStraight;    // Large straight weight * 10 (default 20 = 2.0x)
  uint8_t weightSmallStraight;    // Small straight weight * 10 (default 16 = 1.6x)
  uint8_t weightFullHouse;        // Full house weight * 10 (default 14 = 1.4x)
  uint8_t weight3OfKindLow;       // 3-of-kind low value weight (default 30)
  uint8_t weight3OfKindHigh;      // 3-of-kind high value weight * 10 (default 80 = 8.0x)
  uint8_t weightStraightHold;     // Straight hold priority (default 35)
  uint8_t weightUpperBonus;       // Upper bonus pursuit weight * 10 (default 50 = 5.0x)
  
  // Performance tracking for weight adjustment
  uint16_t gamesWithYahtzee;      // Games where AI got Yahtzee
  uint16_t gamesWithLargeStraight;// Games where AI got Large Straight
  uint16_t gamesWithSmallStraight;// Games where AI got Small Straight
  uint16_t gamesWithFullHouse;    // Games where AI got Full House
  uint16_t winsWithYahtzee;       // Wins where AI got Yahtzee
  uint16_t winsWithLargeStraight; // Wins where AI got Large Straight
  uint16_t winsWithSmallStraight; // Wins where AI got Small Straight
  uint16_t winsWithFullHouse;     // Wins where AI got Full House
  
  // ═══════════════════════════════════════════════════════════════
  // **ENHANCED AI LEARNING - NEW METRICS**
  // ═══════════════════════════════════════════════════════════════
  
  // Roll timing optimization
  uint16_t firstRollScores[13];    // Total points scored on 1st roll per category
  uint16_t secondRollScores[13];   // Total points scored on 2nd roll per category
  uint16_t thirdRollScores[13];    // Total points scored on 3rd roll per category
  uint16_t rollCountUsed[4];       // How often AI used 1, 2, or 3 rolls (index 1-3)
  uint16_t rollCountWins[4];       // Wins associated with each roll count
  
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
  uint16_t holdPatternWins[6];     // Wins for each hold pattern (0-5 dice)
  
  // Bonus pursuit analytics
  uint16_t bonusPursuitAbandoned;  // Times AI gave up on bonus mid-game
  uint16_t bonusAbandonWins;       // Wins after abandoning bonus
  uint16_t bonusNearMiss;          // Times ended with 60-62 points (just missed)
  uint16_t bonusOverkill;          // Times got 75+ in upper section
  
  // Opponent modeling (track human player patterns)
  uint8_t humanAvgScore;           // Human average score / 3
  uint16_t humanBonusRate;         // Times human achieved bonus (out of 100)
  uint8_t humanRiskLevel;          // Human risk-taking level (0-100)
  uint16_t humanYahtzees;          // Total Yahtzees human has scored
  
  // Endgame scenarios (last 3 turns behavior)
  uint16_t endgameCloseWins;       // Wins when within 20 pts in turn 11+
  uint16_t endgameCloseLosses;     // Losses when within 20 pts in turn 11+
  uint16_t endgameBlowoutWins;     // Wins when ahead 50+ pts in turn 11+
  uint16_t endgameComebackAttempts;// Times tried comeback when down 30+ pts
  uint16_t endgameComebackSuccess; // Successful comebacks
  
  // Category timing optimization (early vs late game)
  uint8_t optimalTurnForYahtzee;   // Best turn to score Yahtzee (1-13)
  uint8_t optimalTurnForLargeStraight;  // Best turn for Large Straight
  uint8_t optimalTurnForBonus;     // Best turn to complete upper bonus
  
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
  
  uint16_t rerollDecisions;           // Total reroll decisions made
  uint16_t rerollImprovements;        // Times reroll improved score
  uint16_t rerollDegradations;        // Times reroll hurt score
  int16_t totalRerollChange;          // Net point change from all rerolls (can be negative)
  
  // Breakdown by points available before reroll
  uint16_t rerollWith0_9pts;          // Rerolls with 0-9 points available
  uint16_t rerollWith10_19pts;        // Rerolls with 10-19 points available  
  uint16_t rerollWith20_29pts;        // Rerolls with 20-29 points available
  uint16_t rerollWith30plus;          // Rerolls with 30+ points available
  
  uint16_t rerollImprove0_9;          // Improvements from 0-9 pts
  uint16_t rerollImprove10_19;        // Improvements from 10-19 pts
  uint16_t rerollImprove20_29;        // Improvements from 20-29 pts
  uint16_t rerollImprove30plus;       // Improvements from 30+ pts
  
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
  uint16_t recentWins;                // Wins in last 50 games
  uint16_t recentGamesCount;          // Count of last 50 games tracked
  
  // Category-specific success tracking for efficiency analysis
  uint16_t upperSectionAttempts[6];   // Attempts for 1s through 6s
  uint16_t upperSectionSuccesses[6];  // Good scores (3+ dice) for each
  
  uint16_t fullHouseAttempts;         // Full House pursuit attempts
  uint16_t fullHouseSuccesses;        // Successful Full Houses
  
  uint16_t threeKindAttempts;         // 3-of-a-kind attempts
  uint16_t threeKindSuccesses;        // Successful 3-of-a-kinds
  
  uint16_t fourKindAttempts;          // 4-of-a-kind attempts
  uint16_t fourKindSuccesses;         // Successful 4-of-a-kinds
  
  // Score distribution tracking (histogram buckets for graph)
  uint16_t scoreRanges[10];           // Buckets: 0-49, 50-99, ..., 450+
  
  // Learning efficiency comparison (improvement tracking)
  uint16_t earlyGamesAvgScore;        // Average score first 50 games
  uint16_t recentGamesAvgScore;       // Average score last 50 games
  
  uint8_t checksum;               // Data validation checksum
};
  
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
StrategyVariant strategyVariants[4];
int currentVariantIndex = 0;  // Which variant is currently active (0-3)

// NEW: Decision tracking
DecisionOutcome decisionHistory[MAX_DECISION_HISTORY];
int decisionHistoryCount = 0;
int decisionHistoryIndex = 0;  // Circular buffer index

// Self-play training state
bool aiTrainingMode = false;
int trainingGamesCompleted = 0;
int trainingGamesTotal = 0;

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
void drawLearningProgressGraph();  // Graph 0: Learning progress
void drawStrategyComparisonGraph();  // Graph 1: Strategy comparison
void drawDecisionQualityGraph();  // Graph 2: Decision quality
void drawScoreDistributionGraph();  // Graph 3: Score distribution
void drawCategoryEfficiencyGraph();  // Graph 4: Category efficiency
void drawWinRateTrendsGraph();  // Graph 5: Win rate trends
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
void holdDecisionWithVariant(int dice[], bool held[], int* scores, int variantIndex, int rollsLeft);
int selectCategoryWithVariant(int dice[], int* scores, int variantIndex);
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
void holdDecisionWithVariant(int dice[], bool held[], int* scores, int variantIndex, int rollsLeft);
int selectCategoryWithVariant(int dice[], int* scores, int variantIndex);
void recordDecision(int turnNum, int rollNum, int diceHeldCount, int category, int points, bool leading);

// AI Learning and Strategy Functions
void updateAILearning(bool aiWon, int aiScore, int humanScore);
void runAITraining(int numGames);
void playSelfPlayGame();
void drawTrainingProgress();
void updateTrainingProgressPartial();
void resetAILearningData();
uint8_t calculateChecksum(AILearningData* data);
float calculateExpectedValue(int dice[], int category, int rollsLeft, int* scores);
void computerDecideHoldsAdvanced();  // New advanced hold decision
int findBestCategoryAI(int dice[], int scores[], bool useExpectedValue);

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
    "Train AI",
    "Export Stats",
    "AI Graphs",
    "Game Rules",
    "Back"
  };
  
  int itemCount = 11;  // Back to 11 items (removed AI Difficulty)
  
  int yStart = 50;  // Start below TOOLS heading
  int itemHeight = 21;  // 21 pixels for 11 items
  
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
    tft.print("ENTER: Start AI training");
  } else if(selectedToolItem == 7) {
    tft.print("ENTER: Export stats (Serial)");
  } else if(selectedToolItem == 8) {
    tft.print("ENTER: View AI graphs");
  } else if(selectedToolItem == 9) {
    tft.print("ENTER: View game rules");
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
  tft.print("/6");  // Now 6 pages instead of 3
  
  // Draw appropriate graph based on page
  switch(graphPage) {
    case 0:
      drawLearningProgressGraph();
      break;
    case 1:
      drawStrategyComparisonGraph();
      break;
    case 2:
      drawDecisionQualityGraph();
      break;
    case 3:
      drawScoreDistributionGraph();
      break;
    case 4:
      drawCategoryEfficiencyGraph();
      break;
    case 5:
      drawWinRateTrendsGraph();
      break;
  }
  
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
void drawLearningProgressGraph() {
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT);
  
  // Title
  tft.setTextSize(2);
  tft.setCursor(10, 35);
  tft.print("Learning Progress");
  
  // Check if we have enough data
  if(aiLearning.gamesPlayed < 10) {
    tft.setTextSize(2);
    tft.setTextColor(tft.color565(255, 165, 0)); // Orange
    tft.setCursor(15, 100);
    tft.print("Need 10+ games");
    tft.setCursor(15, 125);
    tft.print("for analysis");
    tft.setTextSize(1);
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(15, 160);
    tft.print("Games played: ");
    tft.print(aiLearning.gamesPlayed);
    return;
  }
  
  // Graph area dimensions
  int gx = 35, gy = 70, gw = 180, gh = 100;
  
  // Draw axes
  tft.drawLine(gx, gy + gh, gx + gw, gy + gh, COLOR_TEXT);
  tft.drawLine(gx, gy, gx, gy + gh, COLOR_TEXT);
  
  // Y-axis labels (Win Rate %)
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(10, gy - 5);
  tft.print("100");
  tft.setCursor(15, gy + gh/2 - 3);
  tft.print("50");
  tft.setCursor(20, gy + gh - 5);
  tft.print("0");
  
  // Calculate early vs recent performance
  float earlyWinRate = 0.0;
  float recentWinRate = 0.0;
  
  if(aiLearning.gamesPlayed >= 50 && aiLearning.earlyGamesAvgScore > 0) {
    // Have enough data for early games
    earlyWinRate = (float)aiLearning.earlyGamesAvgScore / 300.0; // Normalize to 0-1
  } else {
    // Use overall stats for early
    int totalWins = aiLearning.aggressiveWins + aiLearning.conservativeWins;
    if(aiLearning.gamesPlayed > 0) {
      earlyWinRate = (float)totalWins / aiLearning.gamesPlayed * 0.8; // Assume started worse
    }
  }
  
  if(aiLearning.recentGamesCount > 0) {
    recentWinRate = (float)aiLearning.recentWins / aiLearning.recentGamesCount;
  } else {
    // Fall back to overall win rate
    int totalWins = aiLearning.aggressiveWins + aiLearning.conservativeWins;
    if(aiLearning.gamesPlayed > 0) {
      recentWinRate = (float)totalWins / aiLearning.gamesPlayed;
    }
  }
  
  // Draw trend line (2-point simple)
  int earlyX = gx + 20;
  int earlyY = gy + gh - (int)(earlyWinRate * gh);
  int recentX = gx + gw - 20;
  int recentY = gy + gh - (int)(recentWinRate * gh);
  
  // Draw line
  tft.drawLine(earlyX, earlyY, recentX, recentY, COLOR_GREEN);
  
  // Draw points
  tft.fillCircle(earlyX, earlyY, 3, COLOR_P1);  // Blue
  tft.fillCircle(recentX, recentY, 3, COLOR_RED);
  
  // Legend below graph
  tft.setTextSize(1);
  tft.setCursor(10, gy + gh + 15);
  tft.setTextColor(COLOR_P1);
  tft.print("Early: ");
  tft.setTextColor(COLOR_TEXT);
  tft.print((int)(earlyWinRate * 100));
  tft.print("%");
  
  tft.setCursor(10, gy + gh + 30);
  tft.setTextColor(COLOR_RED);
  tft.print("Recent: ");
  tft.setTextColor(COLOR_TEXT);
  tft.print((int)(recentWinRate * 100));
  tft.print("%");
  
  // Improvement indicator
  tft.setCursor(10, gy + gh + 45);
  float improvement = (recentWinRate - earlyWinRate) * 100;
  if(improvement > 0) {
    tft.setTextColor(COLOR_GREEN);
    tft.print("Improving +");
    tft.print((int)improvement);
    tft.print("%");
  } else if(improvement < 0) {
    tft.setTextColor(tft.color565(255, 165, 0)); // Orange
    tft.print("Declining ");
    tft.print((int)improvement);
    tft.print("%");
  } else {
    tft.setTextColor(tft.color565(255, 255, 0)); // Yellow
    tft.print("Stable");
  }
}

// Graph 1: Strategy Variant Comparison
void drawStrategyComparisonGraph() {
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(10, 35);
  tft.print("Strategy Compare");
  
  // Bar chart area
  int barWidth = 50;
  int barSpacing = 8;
  int startX = 10;
  int startY = 200;
  int maxBarHeight = 120;
  
  uint16_t colors[] = {COLOR_RED, COLOR_GREEN, COLOR_P1, tft.color565(255, 255, 0)};
  const char* names[] = {"Bal", "Agg", "Con", "Opt"};
  
  // Find max win rate for scaling
  float maxWinRate = 0.01; // Avoid division by zero
  for(int i = 0; i < 4; i++) {
    float wr = strategyVariants[i].getWinRate();
    if(wr > maxWinRate) maxWinRate = wr;
  }
  
  // Draw bars
  tft.setTextSize(1);
  for(int i = 0; i < 4; i++) {
    int x = startX + i * (barWidth + barSpacing);
    float winRate = strategyVariants[i].getWinRate();
    int barHeight = (int)((winRate / maxWinRate) * maxBarHeight);
    
    if(barHeight < 1) barHeight = 1; // Ensure visible
    
    // Draw bar
    tft.fillRect(x, startY - barHeight, barWidth, barHeight, colors[i]);
    tft.drawRect(x, startY - barHeight, barWidth, barHeight, COLOR_TEXT);
    
    // Draw win rate label above bar
    if(strategyVariants[i].gamesPlayed > 0) {
      tft.setCursor(x + 8, startY - barHeight - 12);
      tft.setTextColor(colors[i]);
      tft.print((int)(winRate * 100));
      tft.print("%");
    }
    
    // Draw games played below
    tft.setCursor(x + 12, startY + 5);
    tft.setTextColor(COLOR_TEXT);
    tft.print(strategyVariants[i].gamesPlayed);
    
    // Draw strategy name
    tft.setCursor(x + 12, startY + 18);
    tft.setTextColor(colors[i]);
    tft.print(names[i]);
  }
  
  // Legend
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(10, startY + 35);
  tft.print("Balanced, Aggressive,");
  tft.setCursor(10, startY + 47);
  tft.print("Conservative, Optimized");
  
  // Best performer
  int bestIdx = 0;
  float bestWR = 0;
  for(int i = 0; i < 4; i++) {
    if(strategyVariants[i].gamesPlayed > 5 && strategyVariants[i].getWinRate() > bestWR) {
      bestWR = strategyVariants[i].getWinRate();
      bestIdx = i;
    }
  }
  
  if(bestWR > 0) {
    tft.setCursor(10, startY + 65);
    tft.setTextColor(COLOR_GREEN);
    tft.print("Best: ");
    tft.setTextColor(colors[bestIdx]);
    tft.print(names[bestIdx]);
    tft.setTextColor(COLOR_TEXT);
    tft.print(" (");
    tft.print((int)(bestWR * 100));
    tft.print("%)");
  }
}

// Graph 2: Decision Quality Metrics
void drawDecisionQualityGraph() {
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(10, 35);
  tft.print("Decision Quality");
  
  int y = 60;
  int barMaxWidth = 160;  // Slightly reduced for more compact layout
  
  tft.setTextSize(1);
  
  // Yahtzee pursuit success rate
  tft.setCursor(5, y);
  tft.setTextColor(tft.color565(255, 50, 50)); // Red
  tft.print("Yahtzee:");
  tft.setTextColor(COLOR_TEXT);
  if(aiLearning.yahtzeeAttempts > 0) {
    float successRate = (float)aiLearning.yahtzeeSuccesses / aiLearning.yahtzeeAttempts * 100;
    tft.setCursor(70, y);
    tft.print((int)successRate);
    tft.print("% (");
    tft.print(aiLearning.yahtzeeSuccesses);
    tft.print("/");
    tft.print(aiLearning.yahtzeeAttempts);
    tft.print(")");
    
    // Visual bar
    int barLen = (int)(successRate * 1.4);
    if(barLen > barMaxWidth) barLen = barMaxWidth;
    tft.fillRect(10, y + 12, barLen, 6, COLOR_RED);
    tft.drawRect(10, y + 12, barMaxWidth, 6, COLOR_GRAY);
  } else {
    tft.setCursor(70, y);
    tft.print("No data");
  }
  
  y += 28;
  
  // 4-of-a-Kind success rate
  tft.setCursor(5, y);
  tft.setTextColor(tft.color565(255, 165, 0)); // Orange
  tft.print("4-Kind:");
  tft.setTextColor(COLOR_TEXT);
  if(aiLearning.fourKindAttempts > 0) {
    float successRate = (float)aiLearning.fourKindSuccesses / aiLearning.fourKindAttempts * 100;
    tft.setCursor(70, y);
    tft.print((int)successRate);
    tft.print("% (");
    tft.print(aiLearning.fourKindSuccesses);
    tft.print("/");
    tft.print(aiLearning.fourKindAttempts);
    tft.print(")");
    
    // Visual bar
    int barLen = (int)(successRate * 1.4);
    if(barLen > barMaxWidth) barLen = barMaxWidth;
    tft.fillRect(10, y + 12, barLen, 6, tft.color565(255, 165, 0));
    tft.drawRect(10, y + 12, barMaxWidth, 6, COLOR_GRAY);
  } else {
    tft.setCursor(70, y);
    tft.print("No data");
  }
  
  y += 28;
  
  // 3-of-a-Kind success rate
  tft.setCursor(5, y);
  tft.setTextColor(tft.color565(100, 200, 255)); // Light blue
  tft.print("3-Kind:");
  tft.setTextColor(COLOR_TEXT);
  if(aiLearning.threeKindAttempts > 0) {
    float successRate = (float)aiLearning.threeKindSuccesses / aiLearning.threeKindAttempts * 100;
    tft.setCursor(70, y);
    tft.print((int)successRate);
    tft.print("% (");
    tft.print(aiLearning.threeKindSuccesses);
    tft.print("/");
    tft.print(aiLearning.threeKindAttempts);
    tft.print(")");
    
    // Visual bar
    int barLen = (int)(successRate * 1.4);
    if(barLen > barMaxWidth) barLen = barMaxWidth;
    tft.fillRect(10, y + 12, barLen, 6, tft.color565(100, 200, 255));
    tft.drawRect(10, y + 12, barMaxWidth, 6, COLOR_GRAY);
  } else {
    tft.setCursor(70, y);
    tft.print("No data");
  }
  
  y += 28;
  
  // Straight pursuit success rate
  tft.setCursor(5, y);
  tft.setTextColor(tft.color565(255, 255, 0)); // Yellow
  tft.print("Straights:");
  tft.setTextColor(COLOR_TEXT);
  if(aiLearning.straightAttempts > 0) {
    float successRate = (float)aiLearning.straightSuccesses / aiLearning.straightAttempts * 100;
    tft.setCursor(70, y);
    tft.print((int)successRate);
    tft.print("% (");
    tft.print(aiLearning.straightSuccesses);
    tft.print("/");
    tft.print(aiLearning.straightAttempts);
    tft.print(")");
    
    // Visual bar
    int barLen = (int)(successRate * 1.4);
    if(barLen > barMaxWidth) barLen = barMaxWidth;
    tft.fillRect(10, y + 12, barLen, 6, tft.color565(255, 255, 0));
    tft.drawRect(10, y + 12, barMaxWidth, 6, COLOR_GRAY);
  } else {
    tft.setCursor(70, y);
    tft.print("No data");
  }
  
  y += 28;
  
  // Full House success rate
  tft.setCursor(5, y);
  tft.setTextColor(tft.color565(255, 0, 255)); // Magenta
  tft.print("FullHouse:");
  tft.setTextColor(COLOR_TEXT);
  if(aiLearning.fullHouseAttempts > 0) {
    float successRate = (float)aiLearning.fullHouseSuccesses / aiLearning.fullHouseAttempts * 100;
    tft.setCursor(70, y);
    tft.print((int)successRate);
    tft.print("% (");
    tft.print(aiLearning.fullHouseSuccesses);
    tft.print("/");
    tft.print(aiLearning.fullHouseAttempts);
    tft.print(")");
    
    // Visual bar
    int barLen = (int)(successRate * 1.4);
    if(barLen > barMaxWidth) barLen = barMaxWidth;
    tft.fillRect(10, y + 12, barLen, 6, tft.color565(255, 0, 255));
    tft.drawRect(10, y + 12, barMaxWidth, 6, COLOR_GRAY);
  } else {
    tft.setCursor(70, y);
    tft.print("No data");
  }
  
  y += 28;
  
  // Overall decision quality indicator
  tft.setCursor(5, y);
  tft.setTextColor(COLOR_GREEN);
  tft.print("Overall Quality:");
  tft.setTextColor(COLOR_TEXT);
  
  // Calculate weighted average (now includes all 5 categories)
  int totalAttempts = aiLearning.yahtzeeAttempts + aiLearning.fourKindAttempts + 
                      aiLearning.threeKindAttempts + aiLearning.straightAttempts + 
                      aiLearning.fullHouseAttempts;
  if(totalAttempts > 0) {
    int totalSuccesses = aiLearning.yahtzeeSuccesses + aiLearning.fourKindSuccesses + 
                        aiLearning.threeKindSuccesses + aiLearning.straightSuccesses + 
                        aiLearning.fullHouseSuccesses;
    float overall = (float)totalSuccesses / totalAttempts * 100;
    
    tft.setCursor(5, y + 12);
    tft.print((int)overall);
    tft.print("%");
    
    if(overall >= 70) {
      tft.setTextColor(COLOR_GREEN);
      tft.print(" Excellent");
    } else if(overall >= 50) {
      tft.setTextColor(tft.color565(255, 255, 0)); // Yellow
      tft.print(" Good");
    } else if(overall >= 30) {
      tft.setTextColor(tft.color565(255, 165, 0)); // Orange
      tft.print(" Fair");
    } else {
      tft.setTextColor(COLOR_RED);
      tft.print(" Needs Work");
    }
  } else {
    tft.setCursor(5, y + 12);
    tft.print("Insufficient data");
  }
}

// Graph 3: Score Distribution Histogram
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
void drawCategoryEfficiencyGraph() {
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(10, 35);
  tft.print("Category Effic.");
  
  int y = 60;
  int barMaxWidth = 150;
  
  tft.setTextSize(1);
  
  // Upper section efficiency
  tft.setCursor(5, y);
  tft.setTextColor(tft.color565(255, 255, 0)); // Yellow
  tft.print("UPPER SECTION:");
  y += 12;
  
  const char* upperNames[] = {"1s", "2s", "3s", "4s", "5s", "6s"};
  for(int i = 0; i < 6; i++) {
    tft.setCursor(5, y);
    tft.setTextColor(COLOR_TEXT);
    tft.print(upperNames[i]);
    tft.print(": ");
    
    if(aiLearning.upperSectionAttempts[i] > 0) {
      float efficiency = (float)aiLearning.upperSectionSuccesses[i] / aiLearning.upperSectionAttempts[i] * 100;
      int barLen = (int)(efficiency * 1.2);
      if(barLen > barMaxWidth) barLen = barMaxWidth;
      
      tft.fillRect(35, y, barLen, 8, COLOR_GREEN);
      tft.drawRect(35, y, barMaxWidth, 8, COLOR_TEXT);
      
      tft.setCursor(190, y);
      tft.print((int)efficiency);
      tft.print("%");
    } else {
      tft.setTextColor(tft.color565(255, 165, 0)); // Orange
      tft.print("No data");
    }
    
    y += 10;
  }
  
  y += 8;
  
  // Special categories
  tft.setCursor(5, y);
  tft.setTextColor(tft.color565(0, 255, 255)); // Cyan
  tft.print("SPECIAL:");
  y += 12;
  
  // 3-of-a-kind
  tft.setCursor(5, y);
  tft.setTextColor(COLOR_TEXT);
  tft.print("3K: ");
  if(aiLearning.threeKindAttempts > 0) {
    float eff = (float)aiLearning.threeKindSuccesses / aiLearning.threeKindAttempts * 100;
    int barLen = (int)(eff * 1.2);
    if(barLen > barMaxWidth) barLen = barMaxWidth;
    tft.fillRect(35, y, barLen, 8, tft.color565(255, 0, 255)); // Magenta
    tft.drawRect(35, y, barMaxWidth, 8, COLOR_TEXT);
    tft.setCursor(190, y);
    tft.print((int)eff);
    tft.print("%");
  } else {
    tft.setTextColor(tft.color565(255, 165, 0));
    tft.print("No data");
  }
  y += 10;
  
  // 4-of-a-kind
  tft.setCursor(5, y);
  tft.setTextColor(COLOR_TEXT);
  tft.print("4K: ");
  if(aiLearning.fourKindAttempts > 0) {
    float eff = (float)aiLearning.fourKindSuccesses / aiLearning.fourKindAttempts * 100;
    int barLen = (int)(eff * 1.2);
    if(barLen > barMaxWidth) barLen = barMaxWidth;
    tft.fillRect(35, y, barLen, 8, COLOR_P1); // Blue
    tft.drawRect(35, y, barMaxWidth, 8, COLOR_TEXT);
    tft.setCursor(190, y);
    tft.print((int)eff);
    tft.print("%");
  } else {
    tft.setTextColor(tft.color565(255, 165, 0));
    tft.print("No data");
  }
  
  y += 20;
  
  // Summary
  tft.setCursor(5, y);
  tft.setTextColor(COLOR_GREEN);
  tft.print("Efficiency = % with 3+");
  tft.setCursor(5, y + 12);
  tft.print("dice in upper section");
}

// Graph 5: Win Rate Trends
void drawWinRateTrendsGraph() {
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(10, 35);
  tft.print("Win Rate Trends");
  
  // Calculate win rates for different game ranges
  // This is simplified - uses overall statistics
  int totalWins = aiLearning.aggressiveWins + aiLearning.conservativeWins;
  float overallWR = (aiLearning.gamesPlayed > 0) ? (float)totalWins / aiLearning.gamesPlayed : 0;
  
  // Simulate progression (ideally this would be real historical data)
  float winRates[5];
  winRates[0] = overallWR * 0.7;  // Assume early games were worse
  winRates[1] = overallWR * 0.85;
  winRates[2] = overallWR * 0.95;
  winRates[3] = overallWR;
  winRates[4] = (aiLearning.recentGamesCount > 0) ? 
                (float)aiLearning.recentWins / aiLearning.recentGamesCount : overallWR;
  
  // Line graph area
  int gx = 35, gy = 70, gw = 180, gh = 100;
  
  // Draw axes
  tft.drawLine(gx, gy + gh, gx + gw, gy + gh, COLOR_TEXT);
  tft.drawLine(gx, gy, gx, gy + gh, COLOR_TEXT);
  
  // Y-axis labels
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(10, gy - 5);
  tft.print("100");
  tft.setCursor(15, gy + gh/2 - 3);
  tft.print("50");
  tft.setCursor(20, gy + gh - 5);
  tft.print("0");
  
  // Plot points and lines
  int pointSpacing = gw / 4;
  for(int i = 0; i < 4; i++) {
    int x1 = gx + i * pointSpacing;
    int y1 = gy + gh - (int)(winRates[i] * gh);
    int x2 = gx + (i + 1) * pointSpacing;
    int y2 = gy + gh - (int)(winRates[i + 1] * gh);
    
    // Clamp values
    if(y1 < gy) y1 = gy;
    if(y1 > gy + gh) y1 = gy + gh;
    if(y2 < gy) y2 = gy;
    if(y2 > gy + gh) y2 = gy + gh;
    
    // Draw line
    tft.drawLine(x1, y1, x2, y2, tft.color565(0, 255, 255)); // Cyan
    
    // Draw point
    tft.fillCircle(x1, y1, 2, COLOR_GREEN);
  }
  // Last point
  int lastX = gx + 4 * pointSpacing;
  int lastY = gy + gh - (int)(winRates[4] * gh);
  if(lastY < gy) lastY = gy;
  if(lastY > gy + gh) lastY = gy + gh;
  tft.fillCircle(lastX, lastY, 2, COLOR_RED);
  
  // X-axis labels
  const char* rangeNames[] = {"1-10", "11-25", "26-50", "51-99", "100+"};
  tft.setTextSize(1);
  for(int i = 0; i < 5; i++) {
    int x = gx + i * pointSpacing - 10;
    tft.setCursor(x, gy + gh + 10);
    tft.setTextColor(COLOR_TEXT);
    tft.print(rangeNames[i]);
  }
  
  // Statistics below
  int sy = gy + gh + 30;
  tft.setCursor(10, sy);
  tft.setTextColor(tft.color565(0, 255, 255)); // Cyan
  tft.print("Current: ");
  tft.setTextColor(COLOR_TEXT);
  tft.print((int)(winRates[4] * 100));
  tft.print("%");
  
  tft.setCursor(10, sy + 15);
  tft.setTextColor(tft.color565(0, 255, 255));
  tft.print("Change: ");
  float improvement = (winRates[4] - winRates[0]) * 100;
  if(improvement > 0) {
    tft.setTextColor(COLOR_GREEN);
    tft.print("+");
  } else if(improvement < 0) {
    tft.setTextColor(COLOR_RED);
  } else {
    tft.setTextColor(tft.color565(255, 255, 0)); // Yellow
  }
  tft.print((int)improvement);
  tft.print("%");
}

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
  tft.setCursor(10, 70);
  tft.print("High Score:");
  tft.setCursor(180, 70);
  tft.setTextColor(COLOR_GREEN);
  tft.print(highScore);
  
  // Win/Loss Record
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(10, 105);
  tft.print("Win Record:");
  
  tft.setCursor(20, 130);
  tft.setTextColor(COLOR_P1);
  tft.print("P1: ");
  tft.setTextColor(COLOR_TEXT);
  tft.print(p1Wins);
  
  tft.setCursor(130, 130);
  tft.setTextColor(COLOR_P2);
  tft.print("P2: ");
  tft.setTextColor(COLOR_TEXT);
  tft.print(p2Wins);
  
  tft.setCursor(20, 155);
  tft.setTextColor(COLOR_GRAY);
  tft.print("Ties: ");
  tft.setTextColor(COLOR_TEXT);
  tft.print(totalTies);
  
  // Most Yahtzees
  tft.setCursor(10, 190);
  tft.setTextColor(COLOR_TEXT);
  tft.print("Most Yahtzees:");
  tft.setCursor(180, 190);
  tft.setTextColor(COLOR_RED);
  tft.print(mostYahtzees);
  
  // Total Games
  int totalGames = p1Wins + p2Wins + totalTies;
  tft.setCursor(10, 225);
  tft.setTextColor(COLOR_TEXT);
  tft.print("Total Games:");
  tft.setCursor(180, 225);
  tft.print(totalGames);
  
  // AI Learning Stats (if any AI games played)
  if(aiLearning.gamesPlayed > 0) {
    tft.setCursor(10, 250);
    tft.setTextColor(COLOR_P2);
    tft.print("AI Games:");
    tft.setCursor(180, 250);
    tft.print(aiLearning.gamesPlayed);
    
    // AI Win Rate
    int aiWins = aiLearning.aggressiveWins + aiLearning.conservativeWins;
    tft.setCursor(10, 275);  // Changed from 265 to 275 (10 pixels more space)
    tft.setTextColor(COLOR_TEXT);
    tft.print("AI Win Rate:");
    tft.setCursor(180, 275);  // Changed from 265 to 275
    int winRate = (aiWins * 100) / aiLearning.gamesPlayed;
    tft.print(winRate);
    tft.print("%");
  }
  
  // Instructions
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GRAY);
  tft.setCursor(10, 295);  // Changed from 285 to 295 (adjusted for new spacing)
  tft.print("ENTER: Back");
  
  tft.setCursor(10, 305);  // Changed from 295 to 305
  tft.print("Hold ROLL 3s: Reset stats");
  
  tft.setCursor(10, 315);  // Changed from 305 to 315
  tft.print("(AI stats in Tools menu)");
}

void drawAIStatistics() {
  tft.fillScreen(COLOR_BG);
  
  // Title with page indicator
  tft.setTextSize(2);
  tft.setTextColor(COLOR_GREEN);
  tft.setCursor(30, 10);
  tft.print("AI LEARNING");
  
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GRAY);
  tft.setCursor(180, 15);
  tft.print("(");
  tft.print(aiStatsPage + 1);
  tft.print("/5)");
  
  if(aiStatsPage == 0) {
    // ==================== PAGE 1 ====================
    
    // Games Summary
    tft.setTextSize(1);
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 35);
    tft.print("vs Human: ");
    tft.setTextColor(COLOR_GREEN);
    tft.print(aiLearning.gamesPlayed);
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(130, 35);
    tft.print("Training: ");
    tft.setTextColor(COLOR_GREEN);
    tft.print(aiLearning.totalSelfPlayGames);
    
    // Win Rate vs Human
    int aiWins = aiLearning.aggressiveWins + aiLearning.conservativeWins;
    tft.setCursor(5, 48);
    tft.setTextColor(COLOR_TEXT);
    tft.print("Win Rate: ");
    if(aiLearning.gamesPlayed > 0) {
      int winRate = (aiWins * 100) / aiLearning.gamesPlayed;
      tft.setTextColor(winRate >= 50 ? COLOR_GREEN : COLOR_RED);
      tft.print(winRate);
      tft.print("%");
      
      tft.setTextColor(COLOR_GRAY);
      tft.print(" (");
      tft.print(aiWins);
      tft.print("/");
      tft.print(aiLearning.gamesPlayed);
      tft.print(")");
    } else {
      tft.setTextColor(COLOR_GRAY);
      tft.print("No games yet");
    }
    
    // === LEARNED STRATEGY WEIGHTS ===
    tft.setTextSize(1);
    tft.setTextColor(COLOR_HELD);
    tft.setCursor(5, 65);
    tft.print("=== LEARNED WEIGHTS ===");
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 76);
    tft.print("Yahtzee:");
    tft.setCursor(80, 76);
    tft.setTextColor(COLOR_GREEN);
    tft.print(aiLearning.weightYahtzee / 10.0, 1);
    tft.print("x");
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(130, 76);
    tft.print("LgStr:");
    tft.setCursor(180, 76);
    tft.setTextColor(COLOR_GREEN);
    tft.print(aiLearning.weightLargeStraight / 10.0, 1);
    tft.print("x");
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 87);
    tft.print("SmStr:");
    tft.setCursor(80, 87);
    tft.setTextColor(COLOR_GREEN);
    tft.print(aiLearning.weightSmallStraight / 10.0, 1);
    tft.print("x");
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(130, 87);
    tft.print("FulHs:");
    tft.setCursor(180, 87);
    tft.setTextColor(COLOR_GREEN);
    tft.print(aiLearning.weightFullHouse / 10.0, 1);
    tft.print("x");
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 98);
    tft.print("3Kind(lo):");
    tft.setCursor(80, 98);
    tft.setTextColor(COLOR_GREEN);
    tft.print(aiLearning.weight3OfKindLow);
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(130, 98);
    tft.print("3Kind(hi):");
    tft.setCursor(200, 98);
    tft.setTextColor(COLOR_GREEN);
    tft.print(aiLearning.weight3OfKindHigh / 10.0, 1);
    tft.print("x");
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 109);
    tft.print("UpprBonus:");
    tft.setCursor(80, 109);
    tft.setTextColor(COLOR_GREEN);
    tft.print(aiLearning.weightUpperBonus / 10.0, 1);
    tft.print("x");
    
    // === PERFORMANCE METRICS ===
    tft.setTextColor(COLOR_HELD);
    tft.setCursor(5, 125);
    tft.print("=== PERFORMANCE ===");
    
    // Yahtzee Success Rate
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 136);
    tft.print("Yahtzee:");
    tft.setCursor(70, 136);
    if(aiLearning.yahtzeeAttempts > 0) {
      int yahtzeeRate = (aiLearning.yahtzeeSuccesses * 100) / aiLearning.yahtzeeAttempts;
      tft.setTextColor(COLOR_HELD);
      tft.print(yahtzeeRate);
      tft.print("%");
      tft.setTextColor(COLOR_GRAY);
      tft.print(" (");
      tft.print(aiLearning.yahtzeeSuccesses);
      tft.print("/");
      tft.print(aiLearning.yahtzeeAttempts);
      tft.print(")");
    } else {
      tft.setTextColor(COLOR_GRAY);
      tft.print("No data");
    }
    
    // Straight Success Rate
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 147);
    tft.print("Straights:");
    tft.setCursor(70, 147);
    if(aiLearning.straightAttempts > 0) {
      int straightRate = (aiLearning.straightSuccesses * 100) / aiLearning.straightAttempts;
      tft.setTextColor(COLOR_HELD);
      tft.print(straightRate);
      tft.print("%");
      tft.setTextColor(COLOR_GRAY);
      tft.print(" (");
      tft.print(aiLearning.straightSuccesses);
      tft.print("/");
      tft.print(aiLearning.straightAttempts);
      tft.print(")");
    } else {
      tft.setTextColor(COLOR_GRAY);
      tft.print("No data");
    }

    // Upper Bonus Achievement
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 158);
    tft.print("Upper Bonus:");
    tft.setCursor(90, 158);
    
    int totalAIGames = aiLearning.gamesPlayed + (aiLearning.totalSelfPlayGames * 2);
    
    if(totalAIGames > 0) {
      int bonusRate = (aiLearning.bonusAchieved * 100) / totalAIGames;
      tft.setTextColor(COLOR_HELD);
      tft.print(bonusRate);
      tft.print("%");
      tft.setTextColor(COLOR_GRAY);
      tft.print(" (");
      tft.print(aiLearning.bonusAchieved);
      tft.print("/");
      tft.print(totalAIGames);
      tft.print(")");
    } else {
      tft.setTextColor(COLOR_GRAY);
      tft.print("No data");
    }
    
    // === ACHIEVEMENT TRACKING ===
    tft.setTextColor(COLOR_HELD);
    tft.setCursor(5, 174);
    tft.print("=== ACHIEVEMENTS ===");
    
    // Games with special scores - COMPACT 2-COLUMN LAYOUT
    tft.setTextSize(1);
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 185);
    tft.print("Yahtzee:");
    tft.setCursor(65, 185);
    tft.setTextColor(COLOR_GREEN);
    tft.print(aiLearning.gamesWithYahtzee);
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(95, 185);
    tft.print("W:");
    tft.setCursor(115, 185);
    tft.setTextColor(COLOR_GREEN);
    tft.print(aiLearning.winsWithYahtzee);
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 196);
    tft.print("LgStr:");
    tft.setCursor(65, 196);
    tft.setTextColor(COLOR_GREEN);
    tft.print(aiLearning.gamesWithLargeStraight);
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(95, 196);
    tft.print("W:");
    tft.setCursor(115, 196);
    tft.setTextColor(COLOR_GREEN);
    tft.print(aiLearning.winsWithLargeStraight);
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 207);
    tft.print("SmStr:");
    tft.setCursor(65, 207);
    tft.setTextColor(COLOR_GREEN);
    tft.print(aiLearning.gamesWithSmallStraight);
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(95, 207);
    tft.print("W:");
    tft.setCursor(115, 207);
    tft.setTextColor(COLOR_GREEN);
    tft.print(aiLearning.winsWithSmallStraight);
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 218);
    tft.print("FulHs:");
    tft.setCursor(65, 218);
    tft.setTextColor(COLOR_GREEN);
    tft.print(aiLearning.gamesWithFullHouse);
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(95, 218);
    tft.print("W:");
    tft.setCursor(115, 218);
    tft.setTextColor(COLOR_GREEN);
    tft.print(aiLearning.winsWithFullHouse);
    
    // **NEW: Upper Bonus Stats (Priority 2)**
    tft.setTextSize(1);
    tft.setTextColor(COLOR_HELD);
    tft.setCursor(5, 240);
    tft.print("=== UPPER BONUS (vs Human) ===");
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 251);
    tft.print("Achieved:");
    tft.setCursor(70, 251);
    tft.setTextColor(COLOR_GREEN);
    tft.print(aiLearning.bonusAchievedVsHuman);
    
    if(aiLearning.bonusAchievedVsHuman > 0) {
      tft.setTextColor(COLOR_TEXT);
      tft.setCursor(100, 251);
      tft.print("Won:");
      tft.setCursor(135, 251);
      tft.setTextColor(COLOR_GREEN);
      tft.print(aiLearning.bonusAchievedAndWon);
      
      int bonusWinRate = (aiLearning.bonusAchievedAndWon * 100) / aiLearning.bonusAchievedVsHuman;
      tft.setCursor(165, 251);
      tft.setTextColor(bonusWinRate >= 50 ? COLOR_GREEN : COLOR_RED);
      tft.print("(");
      tft.print(bonusWinRate);
      tft.print("%)");
    }
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 262);
    tft.print("No bonus:");
    tft.setCursor(70, 262);
    tft.setTextColor(COLOR_GRAY);
    
    int noBonusGames = aiLearning.gamesPlayed - aiLearning.bonusAchievedVsHuman;
    tft.print(noBonusGames);
    
    if(noBonusGames > 0) {
      tft.setTextColor(COLOR_TEXT);
      tft.setCursor(100, 262);
      tft.print("Won:");
      tft.setCursor(135, 262);
      tft.setTextColor(COLOR_GREEN);
      tft.print(aiLearning.noBonusButWon);
      
      int noBonusWinRate = (aiLearning.noBonusButWon * 100) / noBonusGames;
      tft.setCursor(165, 262);
      tft.setTextColor(noBonusWinRate >= 30 ? COLOR_GREEN : COLOR_RED);
      tft.print("(");
      tft.print(noBonusWinRate);
      tft.print("%)");
    }
    
    // Instructions at bottom
    tft.setTextSize(1);
    tft.setTextColor(COLOR_GRAY);
    tft.setCursor(5, 275);
    tft.print("UP/DOWN: Next page");
    tft.setCursor(5, 290);
    tft.print("ENTER: Back");
    tft.setCursor(5, 305);
    tft.print("Hold ENTER 3s: Reset AI data");
    
  } else if(aiStatsPage == 1) {
    // ==================== PAGE 2 ====================
    
    // === STRATEGY PREFERENCE (vs Human) ===
    tft.setTextColor(COLOR_HELD);
    tft.setCursor(5, 35);
    tft.print("=== STRATEGY (vs Human) ===");
    
    // Count total games by strategy
    int totalStrategyGames = aiLearning.gamesPlayed;
    int aggressiveGames = 0;
    int conservativeGames = 0;
    
    if(totalStrategyGames > 0) {
      // Count wins
      aggressiveGames = aiLearning.aggressiveWins;
      conservativeGames = aiLearning.conservativeWins;
      
      // Calculate losses
      int totalWins = aiLearning.aggressiveWins + aiLearning.conservativeWins;
      int totalLosses = totalStrategyGames - totalWins;
      
      // Estimate losses split
      int aggressiveLosses = 0;
      int conservativeLosses = 0;
      
      if(totalWins > 0) {
        aggressiveLosses = (totalLosses * aiLearning.aggressiveWins) / totalWins;
        conservativeLosses = totalLosses - aggressiveLosses;
      } else {
        aggressiveLosses = totalLosses / 2;
        conservativeLosses = totalLosses - aggressiveLosses;
      }
      
      aggressiveGames = aiLearning.aggressiveWins + aggressiveLosses;
      conservativeGames = aiLearning.conservativeWins + conservativeLosses;
    }
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 50);
    tft.print("Aggressive:");
    tft.setCursor(90, 50);
    if(aggressiveGames > 0) {
      int aggWinRate = (aiLearning.aggressiveWins * 100) / aggressiveGames;
      tft.setTextColor(aggWinRate >= 50 ? COLOR_RED : COLOR_GRAY);
      tft.print(aggWinRate);
      tft.print("%");
      tft.setTextColor(COLOR_GRAY);
      tft.print(" ");
      tft.print(aiLearning.aggressiveWins);
      tft.print("W-");
      tft.print(aggressiveGames - aiLearning.aggressiveWins);
      tft.print("L");
    } else {
      tft.setTextColor(COLOR_GRAY);
      tft.print("No data");
    }
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 65);
    tft.print("Conservative:");
    tft.setCursor(90, 65);
    if(conservativeGames > 0) {
      int consWinRate = (aiLearning.conservativeWins * 100) / conservativeGames;
      tft.setTextColor(consWinRate >= 50 ? COLOR_GREEN : COLOR_GRAY);
      tft.print(consWinRate);
      tft.print("%");
      tft.setTextColor(COLOR_GRAY);
      tft.print(" ");
      tft.print(aiLearning.conservativeWins);
      tft.print("W-");
      tft.print(conservativeGames - aiLearning.conservativeWins);
      tft.print("L");
    } else {
      tft.setTextColor(COLOR_GRAY);
      tft.print("No data");
    }
    
    // === DECISION INSIGHTS ===
    if(decisionHistoryCount >= 10) {
      tft.setTextSize(1);
      tft.setTextColor(COLOR_HELD);
      tft.setCursor(5, 90);
      tft.print("=== DECISION INSIGHTS ===");
      
      // Calculate patterns
      int earlyHighScores = 0;
      int earlyHighScoreWins = 0;
      int lateRiskTaking = 0;
      int lateRiskWins = 0;
      
      for(int i = 0; i < decisionHistoryCount; i++) {
        DecisionOutcome& decision = decisionHistory[i];
        
        if(decision.turnNumber <= 4 && decision.pointsScored >= 20) {
          earlyHighScores++;
          if(decision.wonGame) earlyHighScoreWins++;
        }
        
        if(decision.turnNumber >= 10 && (decision.categoryScored == 11 || 
            decision.categoryScored == 10 || decision.categoryScored == 9)) {
          lateRiskTaking++;
          if(decision.wonGame) lateRiskWins++;
        }
      }
      
      tft.setTextColor(COLOR_TEXT);
      tft.setCursor(5, 105);
      tft.print("Early aggr: ");
      if(earlyHighScores > 0) {
        int rate = (earlyHighScoreWins * 100) / earlyHighScores;
        tft.setTextColor(rate >= 50 ? COLOR_GREEN : COLOR_RED);
        tft.print(rate);
        tft.print("%");
        tft.setTextColor(COLOR_GRAY);
        tft.print(" (");
        tft.print(earlyHighScoreWins);
        tft.print("/");
        tft.print(earlyHighScores);
        tft.print(")");
      } else {
        tft.setTextColor(COLOR_GRAY);
        tft.print("N/A");
      }
      
      tft.setTextColor(COLOR_TEXT);
      tft.setCursor(5, 120);
      tft.print("Late risk: ");
      if(lateRiskTaking > 0) {
        int rate = (lateRiskWins * 100) / lateRiskTaking;
        tft.setTextColor(rate >= 50 ? COLOR_GREEN : COLOR_RED);
        tft.print(rate);
        tft.print("%");
        tft.setTextColor(COLOR_GRAY);
        tft.print(" (");
        tft.print(lateRiskWins);
        tft.print("/");
        tft.print(lateRiskTaking);
        tft.print(")");
      } else {
        tft.setTextColor(COLOR_GRAY);
        tft.print("N/A");
      }
      
      // Detailed decision breakdown
      tft.setTextColor(COLOR_HELD);
      tft.setCursor(5, 145);
      tft.print("Decision Quality:");
      
      // Average score per turn
      int totalPoints = 0;
      int scoringDecisions = 0;
      for(int i = 0; i < decisionHistoryCount; i++) {
        totalPoints += decisionHistory[i].pointsScored;
        scoringDecisions++;
      }
      
      if(scoringDecisions > 0) {
        float avgScore = (float)totalPoints / scoringDecisions;
        tft.setTextColor(COLOR_TEXT);
        tft.setCursor(5, 160);
        tft.print("Avg pts/turn: ");
        tft.setTextColor(avgScore >= 20 ? COLOR_GREEN : COLOR_RED);
        tft.print(avgScore, 1);
      }
      
      // Hold strategy effectiveness
      int lowHolds = 0;  // 0-2 dice held
      int medHolds = 0;  // 3 dice held
      int highHolds = 0; // 4-5 dice held
      
      for(int i = 0; i < decisionHistoryCount; i++) {
        int held = decisionHistory[i].diceHeld;
        if(held <= 2) lowHolds++;
        else if(held == 3) medHolds++;
        else highHolds++;
      }
      
      tft.setTextColor(COLOR_TEXT);
      tft.setCursor(5, 175);
      tft.print("Hold strategy:");
      
    tft.setCursor(5, 190);
      tft.setTextColor(COLOR_GRAY);
      tft.print("0-2 dice: ");
      tft.print(lowHolds);
      tft.setCursor(90, 190);
      tft.print("3 dice: ");
      tft.print(medHolds);
      tft.setCursor(170, 190);
      tft.print("4-5: ");
      tft.print(highHolds);
      
      // **NEW: Risk/Reward Analysis**
      tft.setTextSize(1);
      tft.setTextColor(COLOR_HELD);
      tft.setCursor(5, 210);
      tft.print("=== RISK/REWARD ANALYSIS ===");
      
      if(aiLearning.highRiskAttempts > 0) {
        tft.setTextColor(COLOR_TEXT);
        tft.setCursor(5, 221);
        tft.print("High-risk plays:");
        tft.setCursor(110, 221);
        tft.setTextColor(COLOR_GRAY);
        tft.print(aiLearning.highRiskAttempts);
        tft.print(" attempts");
        
        // Success rate
        int successRate = (aiLearning.highRiskSuccesses * 100) / aiLearning.highRiskAttempts;
        tft.setCursor(5, 232);
        tft.setTextColor(COLOR_TEXT);
        tft.print("Success rate:");
        tft.setCursor(110, 232);
        tft.setTextColor(successRate >= 40 ? COLOR_GREEN : 
                        successRate >= 25 ? COLOR_TEXT : COLOR_RED);
        tft.print(successRate);
        tft.print("% (");
        tft.print(aiLearning.highRiskSuccesses);
        tft.print("/");
        tft.print(aiLearning.highRiskAttempts);
        tft.print(")");
        
        // Win rate when successful
        if(aiLearning.highRiskSuccesses > 0) {
          int winRate = (aiLearning.highRiskWins * 100) / aiLearning.highRiskSuccesses;
          tft.setCursor(5, 243);
          tft.setTextColor(COLOR_TEXT);
          tft.print("Win rate (success):");
          tft.setCursor(130, 243);
          tft.setTextColor(winRate >= 60 ? COLOR_GREEN : COLOR_TEXT);
          tft.print(winRate);
          tft.print("%");
        }
        
        // Risk assessment
        tft.setCursor(5, 254);
        tft.setTextColor(COLOR_TEXT);
        tft.print("Assessment:");
        tft.setCursor(90, 254);
        
        if(successRate >= 40 && aiLearning.highRiskSuccesses >= 5) {
          tft.setTextColor(COLOR_GREEN);
          tft.print("Risk pays off!");
        } else if(successRate >= 25) {
          tft.setTextColor(COLOR_TEXT);
          tft.print("Moderate value");
        } else if(aiLearning.highRiskAttempts >= 10) {
          tft.setTextColor(COLOR_RED);
          tft.print("Too risky");
        } else {
          tft.setTextColor(COLOR_GRAY);
          tft.print("Need more data");
        }
      } else {
        tft.setTextColor(COLOR_GRAY);
        tft.setCursor(5, 225);
        tft.print("No high-risk plays yet");
      }
      
    } else {
      tft.setTextColor(COLOR_GRAY);
      tft.setCursor(5, 90);
      tft.print("Play 10+ games vs AI for");
      tft.setCursor(5, 105);
      tft.print("decision insights");
    }  
    
    // Instructions at bottom
    tft.setTextSize(1);
    tft.setTextColor(COLOR_GRAY);
    tft.setCursor(5, 275);
    tft.print("UP/DOWN: Next page");
    tft.setCursor(5, 290);
    tft.print("ENTER: Back");
    tft.setCursor(5, 305);
    tft.print("Hold ENTER 3s: Reset AI data");
    
  } else if(aiStatsPage == 2) {
    // ==================== PAGE 3 ====================
    
    // === STRATEGY VARIANTS (Self-Play Training) ===
    tft.setTextColor(COLOR_HELD);
    tft.setCursor(5, 35);
    tft.print("=== STRATEGY VARIANTS ===");
    
    tft.setTextSize(1);
    tft.setTextColor(COLOR_GRAY);
    tft.setCursor(5, 50);
    tft.print("4 AI variants compete in");
    tft.setCursor(5, 60);
    tft.print("self-play to find best strategy");
    
    // Check if we have training data
    bool hasTrainingData = false;
    for(int i = 0; i < 4; i++) {
      if(strategyVariants[i].gamesPlayed > 0) {
        hasTrainingData = true;
        break;
      }
    }
    
    if(!hasTrainingData) {
      tft.setTextSize(2);
      tft.setTextColor(COLOR_GRAY);
      tft.setCursor(20, 100);
      tft.print("No training");
      tft.setCursor(20, 125);
      tft.print("data yet");
      
      tft.setTextSize(1);
      tft.setTextColor(COLOR_TEXT);
      tft.setCursor(10, 160);
      tft.print("Run AI training from");
      tft.setCursor(10, 175);
      tft.print("Tools > Train AI");
      
    } else {
      // Display each variant's performance
      const char* variantNames[] = {
        "Aggressive",
        "Balanced",
        "Conservative",
        "Experimental"
      };
      
      int yPos = 80;
      
      for(int i = 0; i < 4; i++) {
        StrategyVariant& variant = strategyVariants[i];
        
        if(variant.gamesPlayed > 0) {
          // Variant name
          tft.setTextSize(1);
          tft.setTextColor(COLOR_TEXT);
          tft.setCursor(5, yPos);
          tft.print(variantNames[i]);
          tft.print(":");
          
          // Win rate
          float winRate = variant.getWinRate();
          int winRatePercent = (int)(winRate * 100);
          
          tft.setCursor(100, yPos);
          if(winRatePercent >= 55) {
            tft.setTextColor(COLOR_GREEN);
          } else if(winRatePercent >= 45) {
            tft.setTextColor(COLOR_HELD);
          } else {
            tft.setTextColor(COLOR_RED);
          }
          tft.print(winRatePercent);
          tft.print("% WR");
          
          // Record
          tft.setTextColor(COLOR_GRAY);
          tft.setCursor(155, yPos);
          tft.print(variant.wins);
          tft.print("W-");
          tft.print(variant.losses);
          tft.print("L");
          
          // Average score (next line)
          tft.setCursor(10, yPos + 11);
          tft.setTextColor(COLOR_GRAY);
          tft.print("Avg: ");
          
          int avgScore = variant.getAvgScore();
          if(avgScore >= 200) {
            tft.setTextColor(COLOR_GREEN);
          } else if(avgScore >= 150) {
            tft.setTextColor(COLOR_TEXT);
          } else {
            tft.setTextColor(COLOR_RED);
          }
          tft.print(avgScore);
          tft.print(" pts");
          
          // Games played
          tft.setTextColor(COLOR_GRAY);
          tft.setCursor(100, yPos + 11);
          tft.print("(");
          tft.print(variant.gamesPlayed);
          tft.print(" games)");
          
          yPos += 33;  // Space for next variant
        }
      }
      
      // Find best performing variant
      int bestVariant = -1;
      float bestWinRate = 0;
      
      for(int i = 0; i < 4; i++) {
        if(strategyVariants[i].gamesPlayed >= 10) {  // Need at least 10 games
          float winRate = strategyVariants[i].getWinRate();
          if(winRate > bestWinRate) {
            bestWinRate = winRate;
            bestVariant = i;
          }
        }
      }
      
      // Show which variant is performing best
      if(bestVariant != -1) {
        tft.setTextSize(1);
        tft.setTextColor(COLOR_GREEN);
        tft.setCursor(5, yPos + 10);
        tft.print("Best: ");
        tft.print(variantNames[bestVariant]);
        
        tft.setTextColor(COLOR_GRAY);
        tft.setCursor(5, yPos + 25);
        tft.print("Main AI adapts weights from");
        tft.setCursor(5, yPos + 35);
        tft.print("best-performing variants");
      }
    }
    
    // Instructions at bottom
    tft.setTextSize(1);
    tft.setTextColor(COLOR_GRAY);
    tft.setCursor(5, 275);
    tft.print("UP/DOWN: Next page");
    tft.setCursor(5, 290);
    tft.print("ENTER: Back");
    tft.setCursor(5, 305);
    tft.print("Hold ENTER 3s: Reset AI data");
    
  } else if(aiStatsPage == 3) {
    // ==================== PAGE 4 - CATEGORY STATS ====================
    
    tft.setTextSize(1);
    tft.setTextColor(COLOR_HELD);
    tft.setCursor(5, 35);
    tft.print("=== CATEGORY USAGE (vs Human) ===");
    
    if(aiLearning.gamesPlayed == 0) {
      tft.setTextSize(2);
      tft.setTextColor(COLOR_GRAY);
      tft.setCursor(20, 100);
      tft.print("No games");
      tft.setCursor(20, 125);
      tft.print("vs human yet");
    } else {
      // Show top 10 categories with win rates
      tft.setTextSize(1);
      
      // Create array of category indices sorted by usage
      int sortedCats[13];
      for(int i = 0; i < 13; i++) sortedCats[i] = i;
      
      // Bubble sort by usage count (descending)
      for(int i = 0; i < 12; i++) {
        for(int j = 0; j < 12 - i; j++) {
          if(aiLearning.categoryScoredCount[sortedCats[j]] < 
             aiLearning.categoryScoredCount[sortedCats[j+1]]) {
            int temp = sortedCats[j];
            sortedCats[j] = sortedCats[j+1];
            sortedCats[j+1] = temp;
          }
        }
      }
      
    int yPos = 50;
      
      for(int i = 0; i < 10; i++) {  // Top 10
        int cat = sortedCats[i];
        int count = aiLearning.categoryScoredCount[cat];
        
        if(count == 0) break;  // No more categories with data
        
        tft.setTextColor(COLOR_TEXT);
        tft.setCursor(5, yPos);
        tft.print(categoryNames[cat]);
        
        // Usage count
        tft.setCursor(90, yPos);
        tft.setTextColor(COLOR_GRAY);
        tft.print("(");
        tft.print(count);
        tft.print(")");
        
        // Win rate when scoring this category
        int wins = aiLearning.categoryWinsWhenScored[cat];
        int winRate = (wins * 100) / count;
        
        tft.setCursor(130, yPos);
        tft.setTextColor(winRate >= 60 ? COLOR_GREEN : 
                        winRate >= 40 ? COLOR_TEXT : COLOR_RED);
        tft.print(winRate);
        tft.print("%");
        
        tft.setTextColor(COLOR_GRAY);
        tft.print(" (");
        tft.print(wins);
        tft.print("W)");
        
        yPos += 19;  // Reduced to 19 for tighter spacing
      }  
    }
    
    // **NEW: Turn-Phase Performance (Priority 3)**
    tft.setTextSize(1);
    tft.setTextColor(COLOR_HELD);
    tft.setCursor(5, 242);  // Changed from 235 to 242 (more spacing)
    tft.print("=== TURN-PHASE PERFORMANCE ===");
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 256);  // Changed from 248 to 256 (8 pixels more space below heading)
    tft.print("Early lead:");
    tft.setCursor(70, 256);  // Changed from 248 to 256
    tft.setTextColor(COLOR_GREEN);
    tft.print(aiLearning.earlyGameWins);
    tft.setTextColor(COLOR_TEXT);
    tft.print("W ");
    tft.setTextColor(COLOR_RED);
    tft.print(aiLearning.earlyGameLosses);
    tft.print("L");
    
    if(aiLearning.earlyGameWins + aiLearning.earlyGameLosses > 0) {
      int earlyWinRate = (aiLearning.earlyGameWins * 100) / 
                         (aiLearning.earlyGameWins + aiLearning.earlyGameLosses);
      tft.setCursor(150, 256);
      tft.setTextColor(earlyWinRate >= 70 ? COLOR_GREEN : COLOR_TEXT);
      tft.print("(");
      tft.print(earlyWinRate);
      tft.print("%)");
    }
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 269);  // Changed from 261 to 269 (8 pixels more space)
    tft.print("Comebacks:");
    tft.setCursor(70, 269);  // Changed from 261 to 269
    tft.setTextColor(COLOR_GREEN);
    tft.print(aiLearning.lateGameComebacks);
    
    // Instructions at bottom
    tft.setTextSize(1);
    tft.setTextColor(COLOR_GRAY);
    tft.setCursor(5, 290);
    tft.print("UP/DOWN: Next page  ENTER: Back");
    
  } else if(aiStatsPage == 4) {
    // ==================== PAGE 5 - ENHANCED METRICS ====================
    
    tft.setTextSize(1);
    tft.setTextColor(COLOR_HELD);
    tft.setCursor(5, 35);
    tft.print("=== ENHANCED LEARNING ===");
    
    // Roll Timing Analysis
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 50);
    tft.print("Roll Efficiency:");
    
    uint16_t totalRolls = aiLearning.rollCountUsed[1] + aiLearning.rollCountUsed[2] + aiLearning.rollCountUsed[3];
    if(totalRolls > 0) {
      for(int r = 1; r <= 3; r++) {
        tft.setCursor(5, 60 + (r-1)*11);
        tft.setTextColor(COLOR_GRAY);
        tft.print(r);
        tft.print(" roll");
        if(r > 1) tft.print("s");
        tft.print(":");
        
        tft.setCursor(60, 60 + (r-1)*11);
        tft.setTextColor(COLOR_TEXT);
        tft.print(aiLearning.rollCountUsed[r]);
        
        // Calculate average score for this roll count
        if(aiLearning.rollCountUsed[r] > 0) {
          // Sum scores from all categories for this roll count
          uint16_t totalScore = 0;
          if(r == 1) {
            for(int c = 0; c < 13; c++) {
              totalScore += aiLearning.firstRollScores[c];
            }
          } else if(r == 2) {
            for(int c = 0; c < 13; c++) {
              totalScore += aiLearning.secondRollScores[c];
            }
          } else if(r == 3) {
            for(int c = 0; c < 13; c++) {
              totalScore += aiLearning.thirdRollScores[c];
            }
          }
          
          int avgScore = totalScore / aiLearning.rollCountUsed[r];
          tft.setCursor(110, 60 + (r-1)*11);
          tft.setTextColor(avgScore >= 20 ? COLOR_GREEN : avgScore >= 15 ? COLOR_TEXT : COLOR_RED);
          tft.print("~");
          tft.print(avgScore);
          tft.print("pt");
        }
      }
    } else {
      tft.setTextColor(COLOR_GRAY);
      tft.setCursor(10, 65);
      tft.print("Not enough data");
    }
    
    // Hold Pattern Analysis
    tft.setTextColor(COLOR_HELD);
    tft.setCursor(5, 100);
    tft.print("=== HOLD PATTERNS ===");
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 112);
    tft.print("Reroll all:");
    tft.setCursor(80, 112);
    tft.setTextColor(COLOR_GRAY);
    tft.print(aiLearning.holdPattern0Dice);
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 123);
    tft.print("Hold pair:");
    tft.setCursor(80, 123);
    tft.setTextColor(COLOR_GRAY);
    tft.print(aiLearning.holdPattern2Dice);
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 134);
    tft.print("Hold 3+:");
    tft.setCursor(80, 134);
    tft.setTextColor(COLOR_GRAY);
    tft.print(aiLearning.holdPattern3Dice + aiLearning.holdPattern4Dice);
    
    // Bonus Pursuit Analytics
    tft.setTextColor(COLOR_HELD);
    tft.setCursor(5, 152);
    tft.print("=== BONUS STRATEGY ===");
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 164);
    tft.print("Near miss:");
    tft.setCursor(80, 164);
    tft.setTextColor(COLOR_RED);
    tft.print(aiLearning.bonusNearMiss);
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 175);
    tft.print("Abandoned:");
    tft.setCursor(80, 175);
    tft.setTextColor(COLOR_GRAY);
    tft.print(aiLearning.bonusPursuitAbandoned);
    
    if(aiLearning.bonusPursuitAbandoned > 0) {
      int abandonWinRate = (aiLearning.bonusAbandonWins * 100) / aiLearning.bonusPursuitAbandoned;
      tft.setCursor(120, 175);
      tft.setTextColor(abandonWinRate >= 40 ? COLOR_GREEN : COLOR_RED);
      tft.print(abandonWinRate);
      tft.print("%");
    }
    
    // Opponent Modeling
    tft.setTextColor(COLOR_HELD);
    tft.setCursor(5, 195);
    tft.print("=== OPPONENT PROFILE ===");
    
    if(aiLearning.gamesPlayed == 0) {
      // No vs-human games yet — self-play doesn't populate this
      tft.setTextColor(COLOR_GRAY);
      tft.setCursor(5, 210);
      tft.print("Play vs AI to build");
      tft.setCursor(5, 221);
      tft.print("opponent profile");
    } else {
      tft.setTextColor(COLOR_TEXT);
      tft.setCursor(5, 207);
      tft.print("Avg score:");
      tft.setCursor(80, 207);
      tft.setTextColor(COLOR_GRAY);
      tft.print(aiLearning.humanAvgScore * 3);
      
      tft.setTextColor(COLOR_TEXT);
      tft.setCursor(5, 218);
      tft.print("Bonus rate:");
      tft.setCursor(80, 218);
      tft.setTextColor(COLOR_GRAY);
      tft.print(aiLearning.humanBonusRate);
      tft.print("%");
      
      tft.setTextColor(COLOR_TEXT);
      tft.setCursor(5, 229);
      tft.print("Yahtzees:");
      tft.setCursor(80, 229);
      tft.setTextColor(COLOR_RED);
      tft.print(aiLearning.humanYahtzees);
      
      tft.setTextColor(COLOR_TEXT);
      tft.setCursor(5, 240);
      tft.print("Risk level:");
      tft.setCursor(80, 240);
      uint8_t riskLevel = aiLearning.humanRiskLevel;
      if(riskLevel >= 50) {
        tft.setTextColor(COLOR_RED);
        tft.print("Aggressive");
      } else if(riskLevel >= 25) {
        tft.setTextColor(COLOR_TEXT);
        tft.print("Moderate");
      } else {
        tft.setTextColor(COLOR_GREEN);
        tft.print("Conservative");
      }
    }
    
    // Endgame Performance
    tft.setTextColor(COLOR_HELD);
    tft.setCursor(5, 257);
    tft.print("=== ENDGAME (turns 11-13) ===");
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 269);
    tft.print("Close wins:");
    tft.setCursor(80, 269);
    tft.setTextColor(COLOR_GREEN);
    tft.print(aiLearning.endgameCloseWins);
    
    tft.setCursor(130, 269);
    tft.setTextColor(COLOR_RED);
    tft.print("L:");
    tft.print(aiLearning.endgameCloseLosses);
    
    // Instructions at bottom
    tft.setTextSize(1);
    tft.setTextColor(COLOR_GRAY);
    tft.setCursor(5, 290);
    tft.print("UP/DOWN: Next page  ENTER: Back");
  }
}

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
  Serial.print("StrategyVariant size: ");
  Serial.print(sizeof(StrategyVariant));
  Serial.println(" bytes");
  Serial.print("Total variants (4x): ");
  Serial.print(sizeof(StrategyVariant) * 4);
  Serial.println(" bytes");
  Serial.println("");
  Serial.println("EEPROM Memory Map:");
  Serial.println("  0-14:    Settings (15 bytes)");
  Serial.print("  1000-");
  Serial.print(EEPROM_STRATEGY_VARIANTS_ADDR - 1);
  Serial.print(": AI Learning (");
  Serial.print(EEPROM_STRATEGY_VARIANTS_ADDR - EEPROM_AI_LEARNING_START);
  Serial.println(" bytes)");
  Serial.print("  ");
  Serial.print(EEPROM_STRATEGY_VARIANTS_ADDR);
  Serial.print("-");
  Serial.print(EEPROM_STRATEGY_VARIANTS_ADDR + (sizeof(StrategyVariant) * 4) - 1);
  Serial.print(": Variants (");
  Serial.print(sizeof(StrategyVariant) * 4);
  Serial.println(" bytes)");
  Serial.print("Total EEPROM used: ~");
  Serial.print(EEPROM_STRATEGY_VARIANTS_ADDR + (sizeof(StrategyVariant) * 4));
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
  
  // Set AI personality based on difficulty
  if(aiDifficulty == 0) {
    // Normal AI - conservative, with randomness
    aggressivenessWeight = 0.35 + (random(0, 15) / 100.0);
  } else if(aiDifficulty == 1) {
    // Hard AI - aggressive with learning
    aggressivenessWeight = 0.65 + (random(0, 15) / 100.0);
    
    if(aiLearning.gamesPlayed > 10) {
      float aggressiveSuccessRate = (float)aiLearning.aggressiveWins / aiLearning.gamesPlayed;
      float conservativeSuccessRate = (float)aiLearning.conservativeWins / aiLearning.gamesPlayed;
      
      if(aggressiveSuccessRate > conservativeSuccessRate + 0.1) {
        aggressivenessWeight = min(0.85, aggressivenessWeight + 0.1);
      } else if(conservativeSuccessRate > aggressiveSuccessRate + 0.1) {
        aggressivenessWeight = max(0.55, aggressivenessWeight - 0.1);
      }
    }
  } else {
    // God Mode AI - maximum aggression, no randomness, optimal play
    aggressivenessWeight = 0.95;  // Near-maximum aggression for optimal expected value
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
  if(celebrationActive) {
    updateCelebration();
    drawCelebration();  // Draw clean celebration screen with particles
    delay(16);  // ~60fps for smooth animation
  }
  
  // Continuously animate the game over screen (confetti needs to update each frame)
  static unsigned long lastGameOverFrame = 0;
  if(gameState == STATE_GAME_OVER && millis() - lastGameOverFrame > 16) {
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
  aiLastScoredCategory = -1;  // Reset score highlight
  
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
      updateDiceDisplay();
      drawScreen();
      
      for(int i = 0; i < 5; i++) {
      }
      } else if(gameState == STATE_START) {
      // Start the turn
      gameState = STATE_ROLLING;
      rollsLeft--;  // Decrement BEFORE rolling
      rollDice();
      updateDiceDisplay();  // Update display immediately
      
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
      updateDiceDisplay();  // Update display immediately
      
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
          // UP button = NEXT/HIGHER page (increment with wrapping)
          if(graphPage < 5) {  // 6 pages (0-5)
            graphPage++;
          } else {
            graphPage = 0;  // Wrap to first page
          }
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
            selectedToolItem = 10;  // Wrap to bottom (last item - now 10 for 11 items)
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
          // DOWN button = PREVIOUS/LOWER page (decrement with wrapping)
          if(graphPage > 0) {
            graphPage--;
          } else {
            graphPage = 5;  // Wrap to last page (page 6)
          }
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
          if(selectedToolItem < 10) {  // Now 11 items (0-10)
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
  aiStatsPage = (aiStatsPage + 1) % 5;  // Cycle 0->1->2->3->4->0 (UP goes to higher pages)
  drawAIStatistics();
  delay(200);
}
    lastUpInStats = upBtn;
    
    // Check DOWN button for page navigation (scroll to LOWER page numbers)
    static bool lastDownInStats = HIGH;
    bool downBtn = digitalRead(downButton);
    if(lastDownInStats == HIGH && downBtn == LOW) {
    aiStatsPage = (aiStatsPage - 1 + 5) % 5;  // Cycle 4->3->2->1->0->4 (DOWN goes to lower pages)
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
          // Train AI - run self-play games
          buzzerBeep(50);
          
          // Ask how many games (using up/down to select)
          int trainingAmount = 50;  // Default: 50 games
          bool selecting = true;
          
          while(digitalRead(enterButton) == LOW) delay(10);  // wait for ENTER release
          
          // Hold-accelerate state — declared here (not static inside the loop)
          // so they reinitialise to false/0 every time this screen is opened.
          bool upActive = false, downActive = false;
          unsigned long holdStart = 0, lastRepeat = 0;
          
          tft.fillScreen(COLOR_BG);
          tft.setTextSize(2);
          tft.setTextColor(COLOR_GREEN);
          tft.setCursor(20, 20);
          tft.print("AI TRAINING");
          
          int lastDisplayedAmount = -1;  // Track what's currently displayed
          
          while(selecting) {
            // Only redraw if the value changed
            if(trainingAmount != lastDisplayedAmount) {
              // Clear only the dynamic content area
              tft.fillRect(0, 80, 240, 180, COLOR_BG);
              
              tft.setTextSize(2);
              tft.setTextColor(COLOR_TEXT);
              tft.setCursor(20, 90);
              tft.print("Training Games:");
              
              tft.setTextSize(4);
              tft.setTextColor(COLOR_GREEN);
              tft.setCursor(70, 130);
              tft.print(trainingAmount);
              
              tft.setTextSize(1);
              tft.setTextColor(COLOR_GRAY);
              tft.setCursor(10, 200);
              tft.print("UP/DOWN: Adjust (hold=fast)");
              tft.setCursor(10, 215);
              tft.print("ENTER: Start training");
              tft.setCursor(10, 230);
              tft.print("ROLL: Cancel");
              
              // Estimate time
              int estimatedSeconds = trainingAmount / 5;  // ~5 games/second
              tft.setCursor(10, 260);
              tft.print("Est. time: ~");
              tft.print(estimatedSeconds);
              tft.print(" seconds");
              
              lastDisplayedAmount = trainingAmount;
            }
            
            // Hold-to-accelerate for UP/DOWN

            bool upNow  = (digitalRead(upButton)   == LOW);
            bool downNow = (digitalRead(downButton) == LOW);

            // --- UP pressed ---
            if(upNow && !upActive) {
              // Rising edge: immediate first step
              upActive   = true;
              downActive = false;
              holdStart  = millis();
              lastRepeat = millis();
              trainingAmount += 10;
              if(trainingAmount > 500) trainingAmount = 500;
              buzzerBeep(30);
            } else if(upNow && upActive) {
              // Held down: accelerate over time
              unsigned long held   = millis() - holdStart;
              unsigned long repeat = (held < 600)  ? 200 :   // slow initially
                                     (held < 1200) ? 100 :   // medium
                                                     50;     // fast after 1.2 s
              int step             = (held < 600)  ? 10  :
                                     (held < 1200) ? 50  :
                                                     100;    // big jumps when fast
              if(millis() - lastRepeat >= repeat) {
                lastRepeat = millis();
                trainingAmount += step;
                if(trainingAmount > 500) trainingAmount = 500;
              }
            } else {
              upActive = false;   // released
            }

            // --- DOWN pressed ---
            if(downNow && !downActive) {
              downActive = true;
              upActive   = false;
              holdStart  = millis();
              lastRepeat = millis();
              trainingAmount -= 10;
              if(trainingAmount < 10) trainingAmount = 10;
              buzzerBeep(30);
            } else if(downNow && downActive) {
              unsigned long held   = millis() - holdStart;
              unsigned long repeat = (held < 600)  ? 200 :
                                     (held < 1200) ? 100 :
                                                     50;
              int step             = (held < 600)  ? 10  :
                                     (held < 1200) ? 50  :
                                                     100;
              if(millis() - lastRepeat >= repeat) {
                lastRepeat = millis();
                trainingAmount -= step;
                if(trainingAmount < 10) trainingAmount = 10;
              }
            } else {
              downActive = false;
            }

            delay(25);  // short poll — fast enough for smooth acceleration
            if(digitalRead(enterButton) == LOW) {
              // Start training
              buzzerBeep(100);
              delay(200);
              while(digitalRead(enterButton) == LOW);
              
              runAITraining(trainingAmount);
              selecting = false;
            }
            if(digitalRead(rollButton) == LOW) {
              // Cancel
              buzzerBeep(50);
              selecting = false;
            }
          }
          
          drawScreen();
          
        } else if(selectedToolItem == 7) {
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
          
        } else if(selectedToolItem == 8) {
          // View AI Performance Graphs
          buzzerMenuSelect();
          inGraphMenu = true;
          graphPage = 0;
          drawGraphMenu();
        } else if(selectedToolItem == 9) {
          // View game rules/help
          buzzerMenuSelect();
          inHelpMenu = true;
          helpPage = 0;
          drawHelpMenu();
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
    }
    
    // **LEARNING ENHANCEMENT: Update hold pattern success based on final score**
    updateHoldPatternSuccess(dice, held, score);
    
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
    
    // Note: categoryWinsWhenScored is updated at game end (see updateAIStatsAfterGame)
    
    // **FIX v42.7**: Track category efficiency for enhanced graphs
    trackCategoryEfficiency(selectedCategory, score, dice);
    
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
  
  // Priority order: Yahtzee > Large Straight > Full House > 4 of Kind > Small Straight
  // Then pick highest scoring among remaining
  
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
      } else if(i == 8 && score > 0) {
        // Full House - medium priority
        priorityScore = score + 200;
      } else if(i == 7 && score > 0) {
        // 4 of Kind - medium priority
        priorityScore = score + 150;
      } else if(i == 9 && score > 0) {
        // Small Straight - medium priority
        priorityScore = score + 100;
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
  
  // === FINAL SAFETY CHECK: Never return Chance if straights are available ===
  if(bestCategory == 12) {  // Chance selected
    if(scores[9] == -1 || scores[10] == -1) {  // Either straight available
      
      // *** CRITICAL FIX: Only force straight if we DON'T actually have one ***
      // Check if we actually have a straight
      bool hasSmallStraight = isStraight(dice, 4);
      bool hasLargeStraight = isLargeStraight(dice);
      
      // If we have a large straight and it's available, take it
      if(hasLargeStraight && scores[10] == -1) {
        return 10;
      }
      // If we have a small straight and it's available (and large is used), take it
      else if(hasSmallStraight && scores[9] == -1 && scores[10] != -1) {
        return 9;
      }
      // We DON'T have a straight - find ANY other available category
      else {
        
        // Find the highest-scoring available category (excluding Chance)
        int altCategory = -1;
        int altScore = -1;
        
        for(int i = 0; i < 12; i++) {  // 0-11, excluding Chance (12)
          if(scores[i] == -1) {
            int categoryScore = calculateCategoryScore(dice, i);
            if(categoryScore > altScore) {
              altScore = categoryScore;
              altCategory = i;
            }
          }
        }
        
        if(altCategory != -1) {
          return altCategory;
        } else {
          // ALL other categories are used - fine, use Chance
          return 12;
        }
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
  
  // Clear dice and held state
  for(int i = 0; i < 5; i++) {
    dice[i] = 0;  // Blank dice
    held[i] = false;
  }
  
  if(turn > 26) {
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
    updateDiceDisplay();
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
  // ratio > 1 means average rolls are enough; ratio < 1 means we need above-average
  float feasibility = 0.5f * ratio;         // scale: ratio==1 -> 0.5, ratio==2 -> 1.0
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
      
      // Record first roll decision (before = 0 since no dice yet)
      if(vsComputer && currentPlayer == 2) {
        int scoreAfter = getBestAvailableCategoryScore(finalDiceValues, scores2);
        recordRerollDecision(1, 0, scoreAfter);
      }
      updateDiceDisplay();
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
        
        // **LEARNING ENHANCEMENT: Record hold pattern for learning**
        if(!aiTrainingMode) {
          recordHoldPattern(dice, held);
        }
        
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

      
      // **CRITICAL PRIORITY CHECK: Small Straight with Large available**
      // This MUST be checked before ANY other decision logic
      bool hasSmallStraight = isStraight(dice, 4);
      bool hasLargeStraight = isLargeStraight(dice);

      if(hasSmallStraight && !hasLargeStraight && scores2[10] == -1 && rollsLeft > 0) {
        
        // Don't continue with normal logic - exit early and execute the roll
        // The hold decision was already made in computerDecideHolds() above
        
        rollsLeft--;
        
        // **v42.8: Track small straight pursuit reroll**
        int scoreBefore = getBestAvailableCategoryScore(dice, scores2);
        
        rollDice();
        
        // Record the reroll decision
        int scoreAfter = getBestAvailableCategoryScore(finalDiceValues, scores2);
        int rollNumber = 4 - rollsLeft - 1;  // Which roll this was
        recordRerollDecision(rollNumber, scoreBefore, scoreAfter);
        updateDiceDisplay();
        drawScreen();
        
        for(int i = 0; i < 5; i++) {
        }
        
        // Reset timing for next decision
        lastActionTime = millis();
        computerIsThinking = true;
        computerThinkPhase = 1;
        
        return;  // Exit early - don't continue with normal decision logic
      }
      
      // ═══════════════════════════════════════════════════════════════════════
      // STEP 2: DECIDE WHETHER TO ROLL AGAIN OR SCORE
      // ═══════════════════════════════════════════════════════════════════════
      
      bool shouldRollAgain = false;
      
      // Count dice for strategy decisions
      int counts[7] = {0};
      for(int i = 0; i < 5; i++) {
        counts[dice[i]]++;
      }
      
      // Calculate best possible score with current dice
      int* scores = scores2;
      int bestCurrentScore = 0;
      int bestCategory = findBestCategoryAI(dice, scores, false);
      
      if(bestCategory != -1) {
        bestCurrentScore = calculateCategoryScore(dice, bestCategory);
      }
      
      
      // Determine game situation
      int total1 = calculateTotalWithBonuses(scores1, yahtzeeBonus1);
      int total2 = calculateTotalWithBonuses(scores2, yahtzeeBonus2);
      bool trailing = (total2 < total1 - 20);
      bool leading = (total2 > total1 + 20);

      // Check upper section bonus status
      int upperTotal = getUpperSectionTotal(scores);
      bool needsBonus = (upperTotal < 63);
      int bonusRemaining = 63 - upperTotal;
      
      
      // Calculate expected value of rerolling vs keeping
      float expectedValueReroll = 0;
      if(bestCategory != -1 && rollsLeft > 0) {
        expectedValueReroll = calculateExpectedValue(dice, bestCategory, rollsLeft - 1, scores);
      }
      
      // *** FIXED: Consistent thresholds based on roll number ***
      int scoreThreshold;
      if(aiDifficulty == 2) {
        // God Mode AI - extremely aggressive, near-perfect play
        if(rollsLeft == 2) scoreThreshold = 999;      // First roll - never stop (except Yahtzee/Lg Straight)
        else if(rollsLeft == 1) scoreThreshold = 20;  // Second roll - very aggressive
        else scoreThreshold = 0;                      // Must score
      } else if(aiDifficulty == 1) {
        // Hard AI - more aggressive
        if(rollsLeft == 2) scoreThreshold = 35;      // First roll
        else if(rollsLeft == 1) scoreThreshold = 25; // Second roll
        else scoreThreshold = 0;                     // Must score
      } else {
        // Normal AI - more conservative
        if(rollsLeft == 2) scoreThreshold = 40;      // First roll
        else if(rollsLeft == 1) scoreThreshold = 30; // Second roll
        else scoreThreshold = 0;                     // Must score
      }
      
      // **LEARNING ENHANCEMENT: Use historical reroll success data**
      // Adjust threshold based on learned patterns
      if(aiLearning.gamesPlayed >= 10) {  // Need some experience first
        float rerollProb = getRerollRecommendation(bestCurrentScore);
        
        // If historical data shows rerolling rarely helps at this score, be more conservative
        if(rerollProb < 0.35 && bestCurrentScore >= 20) {
          scoreThreshold -= 5;  // Lower threshold = more likely to keep
        }
        // If historical data shows rerolling usually helps, be more aggressive
        else if(rerollProb > 0.65 && bestCurrentScore < 25) {
          scoreThreshold += 5;  // Higher threshold = more likely to reroll
        }
      }
      
      // Adjust for game situation
      if(trailing) scoreThreshold -= 5;
      if(leading) scoreThreshold += 5;
      

      // DEBUG: Check what we have

      // *** CRITICAL: NEVER stop if we have Small Straight but Large is still available ***
        hasSmallStraight = isStraight(dice, 4);
        hasLargeStraight = isLargeStraight(dice);
      
      if(hasSmallStraight && !hasLargeStraight && scores2[10] == -1 && rollsLeft > 0) {
        shouldRollAgain = true;
        
        // The hold decision was already made above in computerDecideHolds()
        // It should have held 4 dice for the straight
        // Verify we're not holding all 5
        int heldCount = 0;
        for(int i = 0; i < 5; i++) {
          if(held[i]) heldCount++;
        }
        
        if(heldCount >= 5) {
          // Release one die (the outlier)
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
          
          // Find the die NOT part of the consecutive sequence
          int consecutive = 1;
          int maxConsecutive = 1;
          int seqStart = sorted[0];
          int bestSeqStart = sorted[0];
          
          for(int i = 1; i < 5; i++) {
            if(sorted[i] == sorted[i-1] + 1) {
              if(consecutive == 1) seqStart = sorted[i-1];
              consecutive++;
              if(consecutive > maxConsecutive) {
                maxConsecutive = consecutive;
                bestSeqStart = seqStart;
              }
            } else if(sorted[i] != sorted[i-1]) {
              consecutive = 1;
            }
          }
          
          // Release one die not in sequence
          bool released = false;
          for(int i = 0; i < 5; i++) {
            bool inSequence = false;
            for(int j = 0; j < 4; j++) {  // Only need 4 for small straight
              if(dice[i] == bestSeqStart + j) {
                inSequence = true;
                break;
              }
            }
            if(!inSequence && !released) {
              held[i] = false;
              released = true;
            } else {
              held[i] = inSequence;
            }
          }
          
          updateDiceDisplay();
        }
      }
      
      // If we just forced shouldRollAgain for small straight, execute it now
      if(hasSmallStraight && !hasLargeStraight && scores[10] == -1 && rollsLeft > 0) {
        // Already set shouldRollAgain = true above
        
        // Execute the roll
        rollsLeft--;
        
        // **v42.8: Track small straight pursuit reroll**
        int scoreBefore = getBestAvailableCategoryScore(dice, scores);
        
        rollDice();
        
        // Record the reroll decision
        int scoreAfter = getBestAvailableCategoryScore(finalDiceValues, scores);
        int rollNumber = 4 - rollsLeft - 1;
        if(vsComputer && currentPlayer == 2) {
          recordRerollDecision(rollNumber, scoreBefore, scoreAfter);
        }
        updateDiceDisplay();
        drawScreen();
        
        for(int i = 0; i < 5; i++) {
        }
        
        // Reset timing for next decision
        lastActionTime = millis();
        computerIsThinking = true;
        computerThinkPhase = 1;
        
        return;  // Exit early - don't continue with normal decision logic
      }
      
      // === CRITICAL: Check if this is the LAST available category ===
      int availableCount = 0;
      int lastAvailableCategory = -1;
      for(int i = 0; i < 13; i++) {
        if(scores[i] == -1) {
          availableCount++;
          lastAvailableCategory = i;
        }
      }
      
      // SPECIAL CASE: If this is the LAST category, ALWAYS use all 3 rolls
      if(availableCount == 1) {
        shouldRollAgain = (rollsLeft > 0);
        
        if(shouldRollAgain) {
        } else {
        }
      }
      // SPECIAL CASE: Yahtzee is available and we have 4-of-a-kind
      else {
        bool haveFourOfKind = false;
        int fourOfKindValue = -1;
        for(int v = 1; v <= 6; v++) {
          if(counts[v] >= 4) {
            haveFourOfKind = true;
            fourOfKindValue = v;
            break;
          }
        }
        
        if(haveFourOfKind && scores[11] == -1 && rollsLeft > 0) {
          shouldRollAgain = true;
        }
        // Normal decision logic
        else {
          // Make decision based on AI difficulty level
          if(aiDifficulty == 2) {
            // GOD MODE AI: Near-perfect play with optimal expected value calculations
            
            // Always take Yahtzee or Large Straight
            if(isYahtzee(dice) || isLargeStraight(dice)) {
              shouldRollAgain = false;
            }
            // No rolls left - must score
            else if(rollsLeft == 0) {
              shouldRollAgain = false;
            }
            // First roll: NEVER stop (except for Yahtzee/Large Straight above)
            else if(rollsLeft == 2) {
              shouldRollAgain = true;  // Always reroll on first roll
            }
            // Second roll: Only stop for exceptional scores (50+) or guaranteed wins
            else if(rollsLeft == 1) {
              if(bestCurrentScore >= 50) {
                shouldRollAgain = false;
              } else {
                shouldRollAgain = true;
              }
            }
            // Default: if we have rolls left, use them
            else {
              shouldRollAgain = true;
            }
            
          } else if(aiDifficulty == 1) {
            // HARD AI: Uses expected value and is more aggressive
            
            // Always take Yahtzee or Large Straight
            if(isYahtzee(dice) || isLargeStraight(dice)) {
              shouldRollAgain = false;
            }
            // No rolls left - must score
            else if(rollsLeft == 0) {
              shouldRollAgain = false;
            }
            // CRITICAL: On first roll, ONLY stop for exceptional scores (45+)
            else if(rollsLeft == 2) {
              if(bestCurrentScore >= 45) {
                shouldRollAgain = false;
              } else {
                shouldRollAgain = true;
              }
            }
            // FIX: Second roll - stop for very good scores, but NEVER stop if we have
            // 4-of-kind with Yahtzee available, or 4 consecutive with Large Straight available.
            // These are too valuable to give up on.
            else if(rollsLeft == 1) {
              bool haveFour = false;
              for(int v = 1; v <= 6; v++) {
                if(counts[v] >= 4) { haveFour = true; break; }
              }
              bool have4Consec = isStraight(dice, 4) && !isLargeStraight(dice) && scores2[10] == -1;
              
              if(haveFour && scores[11] == -1) {
                // Always reroll for Yahtzee when we have 4-of-kind
                shouldRollAgain = true;
              } else if(have4Consec) {
                // Always reroll for Large Straight when we have 4 consecutive
                shouldRollAgain = true;
              } else if(bestCurrentScore >= 35) {
                shouldRollAgain = false;
              } else {
                shouldRollAgain = true;
              }
            }
            // Default: if we have rolls left, use them
            else {
              shouldRollAgain = true;
            }
            
          } else {
            // NORMAL AI: More conservative, uses simpler logic
            
            // Take guaranteed big scores
            if(isYahtzee(dice) || isLargeStraight(dice)) {
              shouldRollAgain = false;
            }
            // No rolls left - must score
            else if(rollsLeft == 0) {
              shouldRollAgain = false;
            }
            // CRITICAL: On first roll, ONLY stop for excellent scores (40+)
            else if(rollsLeft == 2) {
              if(bestCurrentScore >= 40) {
                shouldRollAgain = false;
              } else {
                shouldRollAgain = true;
              }
            }
            // FIX: Second roll - same as Hard AI, never stop for 4-of-kind/Yahtzee
            // or 4-consecutive/Large Straight. Otherwise stop at 30+.
            else if(rollsLeft == 1) {
              bool haveFour = false;
              for(int v = 1; v <= 6; v++) {
                if(counts[v] >= 4) { haveFour = true; break; }
              }
              bool have4Consec = isStraight(dice, 4) && !isLargeStraight(dice) && scores2[10] == -1;
              
              if(haveFour && scores[11] == -1) {
                shouldRollAgain = true;
              } else if(have4Consec) {
                shouldRollAgain = true;
              } else if(bestCurrentScore >= 30) {
                shouldRollAgain = false;
              } else {
                shouldRollAgain = true;
              }
            }
            // Default: use remaining rolls
            else {
              shouldRollAgain = true;
            }
          }
        }
      }
      
      // ═══════════════════════════════════════════════════════════════
      // EXECUTE DECISION: Roll again or go to category selection
      // ═══════════════════════════════════════════════════════════════
      
      if(shouldRollAgain && rollsLeft > 0) {
        // VERIFY: Check if ALL dice are held (would be pointless to roll)
        int heldCount = 0;
        for(int i = 0; i < 5; i++) {
          if(held[i]) heldCount++;
        }
        
        if(heldCount >= 5) {
          // All dice held - force stop
          shouldRollAgain = false;
          rollsLeft = 0;
        } else {
          // Use one roll NOW (held[] is already set from computerDecideHolds() above)
          rollsLeft--;
          
          // **v42.8: Track reroll decision quality**
          int scoreBefore = 0;
          if(vsComputer && currentPlayer == 2) {
            scoreBefore = getBestAvailableCategoryScore(dice, scores);
          }
          
          // Roll the dice (this respects the held[] array)
          rollDice();
          
          // **v42.8: Record reroll decision after roll completes**
          // Use finalDiceValues which are set immediately (animation doesn't affect them)
          if(vsComputer && currentPlayer == 2) {
            int scoreAfter = getBestAvailableCategoryScore(finalDiceValues, scores);
            int rollNumber = 4 - rollsLeft;  // 1, 2, or 3
            recordRerollDecision(rollNumber, scoreBefore, scoreAfter);
          }
          
          updateDiceDisplay();
          drawScreen();
          
          for(int i = 0; i < 5; i++) {
          }
          
          // Reset timing for next decision
          lastActionTime = millis();
          computerIsThinking = true;
          computerThinkPhase = 1;
          return;  // Exit and re-evaluate on next loop
        }
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
float evaluateHoldStrategy(int dice[], bool holds[], int* scores, int rollsLeft) {
  if(rollsLeft == 0) {
    // No rolls left - just score what we have
    int bestCat = findBestCategoryAI(dice, scores, false);
    if(bestCat == -1) return 0;
    return calculateCategoryScore(dice, bestCat);
  }
  
  // Count what we're holding and what we're rerolling
  int heldCount = 0;
  int rerollCount = 0;
  for(int i = 0; i < 5; i++) {
    if(holds[i]) heldCount++;
    else rerollCount++;
  }
  
  // If holding everything, just score it
  if(rerollCount == 0) {
    int bestCat = findBestCategoryAI(dice, scores, false);
    if(bestCat == -1) return 0;
    return calculateCategoryScore(dice, bestCat);
  }
  
  // Count held dice values
  int heldCounts[7] = {0};
  for(int i = 0; i < 5; i++) {
    if(holds[i]) {
      heldCounts[dice[i]]++;
    }
  }
  
  // Find what we're pursuing
  int maxHeldCount = 0;
  int maxHeldValue = 0;
  for(int v = 1; v <= 6; v++) {
    if(heldCounts[v] > maxHeldCount) {
      maxHeldCount = heldCounts[v];
      maxHeldValue = v;
    }
  }
  
  float maxEV = 0;
  
  // Evaluate EV for each possible target category
  for(int cat = 0; cat < 13; cat++) {
    if(scores[cat] == -1) {  // Category available
      float ev = calculateExpectedValue(dice, cat, rollsLeft - 1, scores);
      
      // Apply learned weights to bias toward successful strategies
      if(cat == 11 && maxHeldCount >= 3) {
        // Pursuing Yahtzee with 3+ of kind
        ev *= (aiLearning.weightYahtzee / 10.0);
      } else if(cat == 10) {
        // Pursuing Large Straight
        ev *= (aiLearning.weightLargeStraight / 10.0);
      } else if(cat == 9) {
        // Pursuing Small Straight
        ev *= (aiLearning.weightSmallStraight / 10.0);
      } else if(cat == 8) {
        // Pursuing Full House
        ev *= (aiLearning.weightFullHouse / 10.0);
      } else if(cat == 6 || cat == 7) {
        // 3/4 of kind
        if(maxHeldValue <= 3) {
          ev *= (aiLearning.weight3OfKindLow / 10.0);
        } else {
          ev *= (aiLearning.weight3OfKindHigh / 10.0);
        }
      } else if(cat <= 5) {
        // Upper section - apply bonus weight if pursuing bonus
        int upperTotal = getUpperSectionTotal(scores);
        if(upperTotal < 63) {
          ev *= (aiLearning.weightUpperBonus / 10.0);
        }
      }
      
      if(ev > maxEV) {
        maxEV = ev;
      }
    }
  }
  
  return maxEV;
}

void computerDecideHoldsAdvanced() {
  // Clear all holds first
  for(int i = 0; i < 5; i++) {
    held[i] = false;
  }
  
  int* scores = scores2;  // AI is always player 2
  
  // **NEW: Store initial dice state for reroll tracking**
  static int diceBeforeHold[5];
  static int pointsBeforeReroll = 0;
  
  // Calculate points available with current dice (before deciding holds)
  int bestCat = findBestCategoryAI(dice, scores, false);
  if(bestCat != -1) {
    pointsBeforeReroll = calculateCategoryScore(dice, bestCat);
    // Store dice state
    for(int i = 0; i < 5; i++) {
      diceBeforeHold[i] = dice[i];
    }
  }
  
  // ═══════════════════════════════════════════════════════════════
  // *** ENHANCED: INTELLIGENT ENDGAME STRATEGY - SCORE-AWARE ***
  // ═══════════════════════════════════════════════════════════════
  
  // Count available categories
  int availableCategories = 0;
  int availableCats[13];
  for(int i = 0; i < 13; i++) {
    if(scores[i] == -1) {
      availableCats[availableCategories++] = i;
    }
  }
  
  // Calculate current game state
  int total1 = calculateTotalWithBonuses(scores1, yahtzeeBonus1);
  int total2 = calculateTotalWithBonuses(scores2, yahtzeeBonus2);
  int scoreDiff = total2 - total1;  // Positive = AI winning, Negative = AI losing
  
  // Check upper section status
  int upperTotal = getUpperSectionTotal(scores);
  int bonusRemaining = 63 - upperTotal;
  int upperCategoriesOpen = 0;
  for(int j = 0; j <= 5; j++) {
    if(scores[j] == -1) upperCategoriesOpen++;
  }
  bool canStillGetBonus = (bonusRemaining <= upperCategoriesOpen * 18);
  bool closeToBonus = (bonusRemaining <= 15 && canStillGetBonus);
  int bonusValue = canStillGetBonus ? 35 : 0;
  
  // ENDGAME: Last 1-3 categories - CRITICAL DECISION TIME
  if(availableCategories <= 3 && rollsLeft > 0) {
    
    // Identify available special categories
    bool needYahtzee = false;
    bool needLargeStraight = false;
    bool needSmallStraight = false;
    bool needFullHouse = false;
    bool need4OfKind = false;
    bool need3OfKind = false;
    bool needUpperCategories = false;
    
    for(int i = 0; i < availableCategories; i++) {
      int cat = availableCats[i];
      if(cat == 11) needYahtzee = true;
      else if(cat == 10) needLargeStraight = true;
      else if(cat == 9) needSmallStraight = true;
      else if(cat == 8) needFullHouse = true;
      else if(cat == 7) need4OfKind = true;
      else if(cat == 6) need3OfKind = true;
      else if(cat <= 5) needUpperCategories = true;
    }
    
    // Count current dice
    int counts[7] = {0};
    for(int i = 0; i < 5; i++) {
      counts[dice[i]]++;
    }
    
    // Find most common value and count
    int maxCount = 0;
    int maxValue = 0;
    for(int v = 6; v >= 1; v--) {
      if(counts[v] > maxCount) {
        maxCount = counts[v];
        maxValue = v;
      }
    }
    
    // === PRIORITY 1: UPPER BONUS IS CRITICAL (35 pts) ===
    // If close to upper bonus, prioritize completing it over risky special categories
    if(closeToBonus && needUpperCategories && bonusRemaining > 0) {
      
      // Calculate if we can realistically get bonus with remaining rolls
      int bestUpperScore = 0;
      int bestUpperValue = -1;
      
      for(int v = 6; v >= 1; v--) {
        int categoryIdx = v - 1;
        if(scores[categoryIdx] == -1) {
          int potentialScore = counts[v] * v;
          if(potentialScore > bestUpperScore) {
            bestUpperScore = potentialScore;
            bestUpperValue = v;
          }
        }
      }
      
      // Upper bonus math: Do we need this to win?
      int pointsNeededToWin = max(0, total1 - total2 + 1);
      int upperBonusTotalValue = bestUpperScore + bonusValue;  // Score now + potential bonus
      
      // CRITICAL: If upper bonus helps us win/stay competitive, PRIORITIZE IT
      bool upperBonusIsWinning = (upperBonusTotalValue >= pointsNeededToWin - 10);
      bool upperBonusIsSafe = (bonusRemaining <= 10);  // Very close
      
      if((upperBonusIsWinning || upperBonusIsSafe) && bestUpperValue != -1) {
        // COMMIT TO UPPER BONUS - hold all dice of best upper value
        for(int i = 0; i < 5; i++) {
          held[i] = (dice[i] == bestUpperValue);
        }
        
        updateDiceDisplay();
        return;  // Exit early - upper bonus is our path to victory
      }
    }
    
    // === PRIORITY 2: REALISTIC SPECIAL CATEGORIES ===
    // Only pursue special categories if we have realistic chance (based on current dice)
    
    // Calculate realistic pursuit probabilities
    // FIX: When Yahtzee is the ONLY category left we must always chase it
    // no matter what we're holding - even a single die is worth rerolling for.
    // Also allow a pair with 1 roll left (was previously blocked).
    bool yahtzeeIsOnlyCategory = (availableCategories == 1 && availableCats[0] == 11);
    bool realisticYahtzee = yahtzeeIsOnlyCategory ||
                            (maxCount >= 4) ||
                            (maxCount >= 3 && rollsLeft >= 1) ||
                            (maxCount >= 2 && rollsLeft >= 2);
    bool realisticLargeStraight = false;
    bool realisticSmallStraight = false;
    bool realisticFullHouse = false;
    bool realistic4Kind = (maxCount >= 3);
    bool realistic3Kind = (maxCount >= 2);
    
    // Check straight potential
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
    
    // Count unique values and consecutive sequences
    int unique[5];
    int uniqueCount = 0;
    for(int i = 0; i < 5; i++) {
      bool isDup = false;
      for(int j = 0; j < uniqueCount; j++) {
        if(sorted[i] == unique[j]) { isDup = true; break; }
      }
      if(!isDup) unique[uniqueCount++] = sorted[i];
    }
    
    int maxConsecutive = 1;
    int consecutive = 1;
    for(int i = 1; i < uniqueCount; i++) {
      if(unique[i] == unique[i-1] + 1) {
        consecutive++;
        if(consecutive > maxConsecutive) maxConsecutive = consecutive;
      } else {
        consecutive = 1;
      }
    }
    
    realisticLargeStraight = (maxConsecutive >= 4) || (maxConsecutive >= 3 && rollsLeft >= 2);
    realisticSmallStraight = (maxConsecutive >= 3);
    
    // Check full house potential
    bool hasTriple = false;
    bool hasPair = false;
    for(int v = 1; v <= 6; v++) {
      if(counts[v] >= 3) hasTriple = true;
      if(counts[v] >= 2) hasPair = true;
    }
    realisticFullHouse = (hasTriple && hasPair) || (hasTriple && rollsLeft >= 1);
    
    // === PRIORITY 3: SCORE-AWARE DECISION ===
    // Choose based on: realistic chance + value + game situation
    
    bool desperateForPoints = (scoreDiff < -15);  // Losing by 15+
    bool comfortablyAhead = (scoreDiff > 20);     // Winning by 20+
    
    // If losing badly, take ANY guaranteed points (don't chase unrealistic categories)
    if(desperateForPoints) {
      
      // Find category with highest GUARANTEED score (what we have now)
      int bestGuaranteedScore = 0;
      int bestGuaranteedCategory = -1;
      
      for(int i = 0; i < availableCategories; i++) {
        int cat = availableCats[i];
        int guaranteed = calculateCategoryScore(dice, cat);
        if(guaranteed > bestGuaranteedScore) {
          bestGuaranteedScore = guaranteed;
          bestGuaranteedCategory = cat;
        }
      }
      
      // If we have 15+ guaranteed points, hold for it instead of chasing 0-point specials
      if(bestGuaranteedScore >= 15) {
        if(bestGuaranteedCategory <= 5) {
          // Upper section - hold that value
          int targetValue = bestGuaranteedCategory + 1;
          for(int i = 0; i < 5; i++) {
            held[i] = (dice[i] == targetValue);
          }
        } else if(bestGuaranteedCategory == 6 || bestGuaranteedCategory == 7) {
          // 3/4 kind - hold most common
          for(int i = 0; i < 5; i++) {
            held[i] = (dice[i] == maxValue);
          }
        }
        
        updateDiceDisplay();
        return;
      }
    }
    
    // Choose pursuit strategy based on realistic chances
    if(needYahtzee && realisticYahtzee) {
      // Pursue Yahtzee - hold ONLY the most common single value.
      // FIX: Find the value with the highest count, breaking ties by
      // preferring the higher face value (e.g. two 6s beats two 2s).
      // This prevents accidentally holding dice of a second value.
      int bestYahtzeeValue = 1;
      int bestYahtzeeCount = 0;
      for(int v = 6; v >= 1; v--) {
        // >= so higher face value wins ties
        if(counts[v] >= bestYahtzeeCount) {
          bestYahtzeeCount = counts[v];
          bestYahtzeeValue = v;
        }
      }
      // Hold ONLY that one value - nothing else
      for(int i = 0; i < 5; i++) {
        held[i] = (dice[i] == bestYahtzeeValue);
      }
      
      updateDiceDisplay();
      return;
      
    } else if(needLargeStraight && realisticLargeStraight) {
      // Pursue Large Straight (40 pts)
      int seqStart = unique[0];
      for(int i = 1; i < uniqueCount; i++) {
        if(unique[i] == unique[i-1] + 1) {
          int len = 1;
          for(int j = i; j < uniqueCount && unique[j] == unique[j-1] + 1; j++) len++;
          if(len >= maxConsecutive) {
            seqStart = unique[i-1];
            break;
          }
        }
      }
      
      int heldCount = 0;
      for(int i = 0; i < 5 && heldCount < 4; i++) {
        for(int j = 0; j < maxConsecutive && heldCount < 4; j++) {
          if(dice[i] == seqStart + j && !held[i]) {
            held[i] = true;
            heldCount++;
            break;
          }
        }
      }
      
      updateDiceDisplay();
      return;
      
    } else if(needFullHouse && realisticFullHouse) {
      // Pursue Full House (25 pts)
      for(int v = 6; v >= 1; v--) {
        if(counts[v] >= 2) {
          for(int i = 0; i < 5; i++) {
            if(dice[i] == v) held[i] = true;
          }
        }
      }
      
      updateDiceDisplay();
      return;
      
    } else if(needSmallStraight && realisticSmallStraight) {
      // Pursue Small Straight (30 pts)
      int seqStart = unique[0];
      for(int i = 1; i < uniqueCount; i++) {
        if(unique[i] == unique[i-1] + 1) {
          int len = 1;
          for(int j = i; j < uniqueCount && unique[j] == unique[j-1] + 1; j++) len++;
          if(len >= maxConsecutive) {
            seqStart = unique[i-1];
            break;
          }
        }
      }
      
      int heldCount = 0;
      for(int i = 0; i < 5 && heldCount < 3; i++) {
        for(int j = 0; j < min(maxConsecutive, 4) && heldCount < 3; j++) {
          if(dice[i] == seqStart + j && !held[i]) {
            held[i] = true;
            heldCount++;
            break;
          }
        }
      }
      
      updateDiceDisplay();
      return;
      
    } else if(need4OfKind && realistic4Kind) {
      // Pursue 4-of-Kind
      for(int i = 0; i < 5; i++) {
        held[i] = (dice[i] == maxValue);
      }
      
      updateDiceDisplay();
      return;
      
    } else if(need3OfKind && realistic3Kind) {
      // Pursue 3-of-Kind
      for(int i = 0; i < 5; i++) {
        held[i] = (dice[i] == maxValue);
      }
      
      updateDiceDisplay();
      return;
      
    } else {
      // Fall through to normal logic for upper section / Chance / unrealistic specials
      // This handles: pursuing upper bonus, or getting best possible score from current dice
    }
  }
  
  // ═══════════════════════════════════════════════════════════════
  // *** END OF ENHANCED ENDGAME SECTION ***
  // ═══════════════════════════════════════════════════════════════
  
  // Count dice values
  int counts[7] = {0};
  int total = 0;
  for(int i = 0; i < 5; i++) {
    counts[dice[i]]++;
    total += dice[i];
  }
  
  // Find best multiple
  int maxCount = 0;
  int maxValue = 0;
  for(int v = 6; v >= 1; v--) {
    if(counts[v] > maxCount) {
      maxCount = counts[v];
      maxValue = v;
    }
  }
  
  // Check upper section status
  upperTotal = getUpperSectionTotal(scores);
  bool needsBonus = (upperTotal < 63);
  bonusRemaining = 63 - upperTotal;
  upperCategoriesOpen = 0;
  for(int j = 0; j <= 5; j++) {
    if(scores[j] == -1) upperCategoriesOpen++;
  }
  
  
  // ═══════════════════════════════════════════════════════════════
  // QUICK EXITS: Guaranteed excellent scores (hold all)
  // ═══════════════════════════════════════════════════════════════
  
  // Yahtzee (5 of a kind)
  if(maxCount == 5 && scores[11] == -1) {
    for(int i = 0; i < 5; i++) {
      held[i] = true;
    }
    updateDiceDisplay();
    return;
  }
  
  // Large Straight
  if(isLargeStraight(dice) && scores[10] == -1) {
    for(int i = 0; i < 5; i++) {
      held[i] = true;
    }
    updateDiceDisplay();
    return;
  }
  
  // Small Straight - ONLY if Large already used OR we're at 0 rolls left
  if(isStraight(dice, 4) && scores[9] == -1) {
    if(scores[10] != -1 || rollsLeft == 0) {
      for(int i = 0; i < 5; i++) {
        held[i] = true;
      }
      updateDiceDisplay();
      return;
    }
  }
  
  // Full House
  bool hasThree = false, hasTwo = false;
  int threeValue = -1, twoValue = -1;
  for(int v = 1; v <= 6; v++) {
    if(counts[v] == 3) { hasThree = true; threeValue = v; }
    if(counts[v] == 2) { hasTwo = true; twoValue = v; }
  }
  if(hasThree && hasTwo && scores[8] == -1) {
    for(int i = 0; i < 5; i++) {
      if(dice[i] == threeValue || dice[i] == twoValue) {
        held[i] = true;
      }
    }
    updateDiceDisplay();
    return;
  }
  
  // ═══════════════════════════════════════════════════════════════
  // CRITICAL: Small Straight with Large Straight available
  // ═══════════════════════════════════════════════════════════════
  
  bool hasSmallStraight = isStraight(dice, 4);
  bool hasLargeStraight = isLargeStraight(dice);
  
  if(hasSmallStraight && !hasLargeStraight && scores[10] == -1 && rollsLeft > 0) {
    
    // **FIXED STRAIGHT HOLD LOGIC**
    // Sort dice to find consecutive sequence
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
    
    // **CRITICAL FIX**: Remove duplicates to get unique consecutive values
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
    
    // Find longest consecutive sequence in UNIQUE values
    int consecutive = 1;
    int maxConsecutive = 1;
    int seqStart = unique[0];
    int bestSeqStart = unique[0];
    
    for(int i = 1; i < uniqueCount; i++) {
      if(unique[i] == unique[i-1] + 1) {
        if(consecutive == 1) seqStart = unique[i-1];
        consecutive++;
        if(consecutive > maxConsecutive) {
          maxConsecutive = consecutive;
          bestSeqStart = seqStart;
        }
      } else {
        consecutive = 1;
      }
    }
    
    for(int i = 0; i < maxConsecutive; i++) {
    }
    
    // **CRITICAL**: Hold ONLY 4 dice from the consecutive sequence (for large straight)
    // This means we hold ONE die of each value in the sequence
    int heldCount = 0;
    
    // First, mark all as not held
    for(int i = 0; i < 5; i++) {
      held[i] = false;
    }
    
    // Then hold exactly 4 consecutive values (one die per value)
    for(int val = bestSeqStart; val < bestSeqStart + maxConsecutive && heldCount < 4; val++) {
      // Find FIRST die with this value and hold it
      for(int i = 0; i < 5; i++) {
        if(dice[i] == val && !held[i]) {
          held[i] = true;
          heldCount++;
          break;  // Only hold ONE die of this value
        }
      }
    }
    
    for(int i = 0; i < 5; i++) {
    }
    
    updateDiceDisplay();
    return;
  }
  
  // ═══════════════════════════════════════════════════════════════
  // PROBABILISTIC STRATEGY EVALUATION
  // ═══════════════════════════════════════════════════════════════
  
  // FIX: Recompute counts/max here to ensure correct values for all
  // strategy builders below, regardless of which path was taken above.
  // (counts/maxCount/maxValue already declared at line ~6862 - reassign here)
  memset(counts, 0, sizeof(counts));
  for(int i = 0; i < 5; i++) counts[dice[i]]++;

  maxCount = 0;
  maxValue = 0;
  // Prefer higher face values when tied (e.g. prefer 6s over 1s)
  for(int v = 6; v >= 1; v--) {
    if(counts[v] > maxCount) {
      maxCount = counts[v];
      maxValue = v;
    }
  }

  HoldStrategyEV strategies[20];
  int strategyCount = 0;
  
  // === STRATEGY 1: Hold all dice (score what we have now) ===
  for(int i = 0; i < 5; i++) {
    strategies[strategyCount].holds[i] = true;
  }
  strategies[strategyCount].expectedValue = evaluateHoldStrategy(dice, strategies[strategyCount].holds, scores, rollsLeft);
  strategies[strategyCount].description = "Hold all (score now)";
  strategyCount++;
  
  // === STRATEGY 2: Reroll everything (fresh start) ===
  for(int i = 0; i < 5; i++) {
    strategies[strategyCount].holds[i] = false;
  }
  strategies[strategyCount].expectedValue = evaluateHoldStrategy(dice, strategies[strategyCount].holds, scores, rollsLeft);
  strategies[strategyCount].description = "Reroll all";
  strategyCount++;
  
  // === STRATEGY 3-4: Hold best multiple(s) with PROBABILITY BOOST ===
  if(maxCount >= 2) {
    // 3a: Hold primary multiple
    for(int i = 0; i < 5; i++) {
      strategies[strategyCount].holds[i] = (dice[i] == maxValue);
    }
    
    // Calculate EV with probability of improving
    float baseEV = evaluateHoldStrategy(dice, strategies[strategyCount].holds, scores, rollsLeft);
    
    // **USE PROBABILITY TABLES** to boost EV based on what we can achieve
    int diceToReroll = 5 - maxCount;
    if(diceToReroll > 5) diceToReroll = 5;
    
    // Check probabilities for different outcomes
    float prob4OfKind = (maxCount >= 3) ? PROB_OF_KIND[3][diceToReroll] : 0;
    float prob5OfKind = (maxCount >= 4) ? PROB_OF_KIND[4][diceToReroll] : 0;
    
    // Weight by value and learned weights
    float valueMultiplier = 1.0;
    if(maxCount >= 4) {
      valueMultiplier = 3.0;  // 4-of-kind is valuable
      if(prob5OfKind > 0.1) {
        valueMultiplier *= (aiLearning.weightYahtzee / 10.0);  // Apply Yahtzee weight
      }
    } else if(maxCount == 3) {
      if(maxValue >= 4) {
        valueMultiplier = 2.5 * (aiLearning.weight3OfKindHigh / 100.0);
      } else {
        valueMultiplier = 1.5 * (aiLearning.weight3OfKindLow / 30.0);
      }
    } else if(maxCount == 2) {
      if(maxValue >= 5) {
        valueMultiplier = 2.0;
      } else if(maxValue >= 3) {
        valueMultiplier = 1.5;
      } else {
        valueMultiplier = 1.2;
      }
    }
    
    strategies[strategyCount].expectedValue = baseEV * valueMultiplier;
    strategies[strategyCount].description = "Hold best multiple";
    strategyCount++;
    
    // 3b: If we have a pair, also try holding second-best pair
    // FIX: Skip this entirely when Yahtzee is the only remaining category -
    // we must never propose holding a second different value in that case.
    bool yahtzeeOnly = (scores[11] == -1);
    for(int _c = 0; _c < 13; _c++) { if(_c != 11 && scores[_c] == -1) { yahtzeeOnly = false; break; } }
    if(maxCount == 2 && !yahtzeeOnly) {
      int secondBestValue = -1;
      int secondBestCount = 0;
      
      for(int v = 6; v >= 1; v--) {
        if(v != maxValue && counts[v] >= 2 && counts[v] > secondBestCount) {
          secondBestValue = v;
          secondBestCount = counts[v];
        }
      }
      
      if(secondBestValue != -1) {
        for(int i = 0; i < 5; i++) {
          strategies[strategyCount].holds[i] = (dice[i] == secondBestValue);
        }
        
        float ev = secondBestCount * secondBestValue * 1.8;
        strategies[strategyCount].expectedValue = ev;
        strategies[strategyCount].description = "Hold second-best pair";
        strategyCount++;
      }
    }
  }
  
  // === STRATEGY 5-6: Straight pursuit with PROBABILITY CALCULATIONS ===
  // FIX: Never propose straight holds when Yahtzee is the only remaining
  // category - consecutive dice are useless for a Yahtzee chase.
  bool onlyYahtzeeLeft = (scores[11] == -1);
  for(int _c = 0; _c < 13; _c++) { if(_c != 11 && scores[_c] == -1) { onlyYahtzeeLeft = false; break; } }
  if(!onlyYahtzeeLeft && (scores[10] == -1 || scores[9] == -1)) {
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
    
    // Remove duplicates
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
    
    // Find longest consecutive
    int consecutive = 1;
    int maxConsecutive = 1;
    int seqStart = unique[0];
    int bestSeqStart = unique[0];
    
    for(int i = 1; i < uniqueCount; i++) {
      if(unique[i] == unique[i-1] + 1) {
        if(consecutive == 1) seqStart = unique[i-1];
        consecutive++;
        if(consecutive > maxConsecutive) {
          maxConsecutive = consecutive;
          bestSeqStart = seqStart;
        }
      } else {
        consecutive = 1;
      }
    }
    
    // If we have 3+ consecutive, create straight pursuit strategies
    if(maxConsecutive >= 3) {
      // Strategy 5a: Hold all consecutive dice (one per unique value)
      for(int i = 0; i < 5; i++) {
        strategies[strategyCount].holds[i] = false;
      }
      
      for(int val = bestSeqStart; val < bestSeqStart + maxConsecutive; val++) {
        for(int i = 0; i < 5; i++) {
          if(dice[i] == val && !strategies[strategyCount].holds[i]) {
            strategies[strategyCount].holds[i] = true;
            break;
          }
        }
      }
      
      // FIX: 'missing' for the probability table = dice we need to reroll.
      // We hold maxConsecutive dice (one per value), reroll the rest.
      // For small straight we need 4 in sequence, for large we need 5.
      int diceHeld = maxConsecutive;  // one die per unique consecutive value
      int diceToReroll = 5 - diceHeld;
      if(diceToReroll < 0) diceToReroll = 0;
      if(diceToReroll > 5) diceToReroll = 5;
      
      // missing for the table = how many values we still need
      int missingForLarge = max(0, 5 - maxConsecutive);
      int missingForSmall = max(0, 4 - maxConsecutive);
      if(missingForLarge > 5) missingForLarge = 5;
      if(missingForSmall > 5) missingForSmall = 5;
      
      float probLarge = PROB_LARGE_STRAIGHT[missingForLarge];
      float probSmall = PROB_SMALL_STRAIGHT[missingForSmall];
      
      // Compound over remaining rolls
      float compLarge = 1.0 - pow(1.0 - probLarge, rollsLeft);
      float compSmall = 1.0 - pow(1.0 - probSmall, rollsLeft);
      if(compLarge > 0.95) compLarge = 0.95;
      if(compSmall > 0.95) compSmall = 0.95;
      
      // Apply learned weights
      float largeStraightWeight = aiLearning.weightLargeStraight / 10.0;
      float smallStraightWeight = aiLearning.weightSmallStraight / 10.0;
      
      float evLarge = (scores[10] == -1) ? compLarge * 40.0 * largeStraightWeight : 0;
      float evSmall = (scores[9] == -1) ? compSmall * 30.0 * smallStraightWeight : 0;
      
      strategies[strategyCount].expectedValue = evLarge + evSmall;
      strategies[strategyCount].description = "Hold consecutive (straight)";
      strategyCount++;
      
      // Strategy 5b: Hold only 4 consecutive (for large straight pursuit)
      if(maxConsecutive >= 4 && scores[10] == -1) {
        for(int i = 0; i < 5; i++) {
          strategies[strategyCount].holds[i] = false;
        }
        
        int heldCount = 0;
        for(int val = bestSeqStart; val < bestSeqStart + maxConsecutive && heldCount < 4; val++) {
          for(int i = 0; i < 5; i++) {
            if(dice[i] == val && !strategies[strategyCount].holds[i] && heldCount < 4) {
              strategies[strategyCount].holds[i] = true;
              heldCount++;
              break;
            }
          }
        }
        
        // FIX: Holding 4 = rerolling 1 die, so use table index [1]
        // Compound over remaining rolls
        float baseProb4 = PROB_LARGE_STRAIGHT[1];
        float compProb4 = 1.0 - pow(1.0 - baseProb4, rollsLeft);
        if(compProb4 > 0.95) compProb4 = 0.95;
        float largeStraightWeight = aiLearning.weightLargeStraight / 10.0;
        float ev = compProb4 * 40.0 * largeStraightWeight;
        
        strategies[strategyCount].expectedValue = ev;
        strategies[strategyCount].description = "Hold 4 consecutive (large straight)";
        strategyCount++;
      }
    }
  }
  
  // === STRATEGY 7: Two pairs for Full House ===
  if(scores[8] == -1) {
    int pairCount = 0;
    int pairValues[2] = {-1, -1};
    
    for(int v = 6; v >= 1; v--) {
      if(counts[v] == 2 && pairCount < 2) {
        pairValues[pairCount++] = v;
      }
    }
    
    if(pairCount == 2) {
      for(int i = 0; i < 5; i++) {
        strategies[strategyCount].holds[i] = (dice[i] == pairValues[0] || dice[i] == pairValues[1]);
      }
      
      // FIX: Use compounding probability formula instead of linear * rollsLeft
      // P(full house from 2 pairs with 1 die reroll) ≈ 1/6 per roll
      float probPerRoll = 1.0 / 6.0;
      float probFullHouse = 1.0 - pow(1.0 - probPerRoll, rollsLeft);
      if(probFullHouse > 0.5) probFullHouse = 0.5;
      
      float fullHouseWeight = aiLearning.weightFullHouse / 10.0;
      strategies[strategyCount].expectedValue = probFullHouse * 25.0 * fullHouseWeight;
      strategies[strategyCount].description = "Hold two pairs (FH)";
      strategyCount++;
    }
  }
  
  // === STRATEGY 8: Hold high dice (5-6) ===
  int highDiceCount = counts[5] + counts[6];
  if(highDiceCount >= 2) {
    for(int i = 0; i < 5; i++) {
      strategies[strategyCount].holds[i] = (dice[i] >= 5);
    }
    
    float ev = highDiceCount * 5.5;  // Average value of high dice
    strategies[strategyCount].expectedValue = ev;
    strategies[strategyCount].description = "Hold high dice (5-6)";
    strategyCount++;
  }
  
  // === STRATEGY 9: Upper section bonus pursuit ===
  // Enhanced: considers single high-value dice (e.g. 1×6), not just pairs+.
  // Uses opportunity cost so the EV of holding 1×6 is rated much higher than
  // holding 1×1, reflecting that the Sixes slot is far more valuable to protect.
  // Gated by bonus feasibility so we stop chasing when bonus is unrealistic.
  {
    float bonusFeasibilityHold = calcBonusFeasibility(scores);
    bool bonusWorthHolding = (bonusFeasibilityHold >= 0.25f) && (bonusRemaining <= upperCategoriesOpen * 18);

    if(needsBonus && bonusWorthHolding) {
      // Find the best value to hold for upper section.
      // Priority: highest value WITH most dice (prefer 3×6 over 1×6 over 2×4).
      // EV incorporates:
      //   - current_score   (what we already have)
      //   - bonus_contribution (how much this gets us toward the 35-pt bonus)
      //   - opportunity_cost_weight (high-value slots get a bigger EV boost
      //     because losing them to a weak score is very costly)
      int bestHoldValue = -1;
      float bestHoldEV = -1.0f;

      for(int v = 6; v >= 1; v--) {
        int catIdx = v - 1;
        if(scores[catIdx] == -1 && counts[v] >= 1) {
          float currentValue = (float)(counts[v] * v);
          float bonusFrac = currentValue / (float)max(1, bonusRemaining);
          float bonusContribution = min(1.0f, bonusFrac) * 35.0f;
          float bonusWeight = aiLearning.weightUpperBonus / 100.0f;

          // Opportunity-cost multiplier: the higher the opp cost, the MORE
          // valuable it is to HOLD these dice (protect the slot).
          float oppCost = upperOpportunityCost(v, counts[v]);
          // A slot with opp cost 0.80 (just 1 die of a high value) is
          // very precious – we want to keep rolling to improve it.
          // Multiply EV by (1 + oppCost) so high-opp-cost dice get priority.
          float oppCostMultiplier = 1.0f + oppCost;

          // Scale EV further by feasibility so we ease off if bonus is unlikely
          float ev = (currentValue + bonusContribution) * bonusWeight * oppCostMultiplier * bonusFeasibilityHold;

          if(ev > bestHoldEV) {
            bestHoldEV = ev;
            bestHoldValue = v;
          }
        }
      }

      if(bestHoldValue != -1) {
        for(int i = 0; i < 5; i++) {
          strategies[strategyCount].holds[i] = (dice[i] == bestHoldValue);
        }
        strategies[strategyCount].expectedValue = bestHoldEV;
        strategies[strategyCount].description = "Hold for upper bonus (opp-cost)";
        strategyCount++;
      }
    }
  }
  
  // ═══════════════════════════════════════════════════════════════
  // CHOOSE BEST STRATEGY BASED ON EXPECTED VALUE
  // ═══════════════════════════════════════════════════════════════
  
  int bestIdx = 0;
  float bestEV = strategies[0].expectedValue;
  
  for(int i = 0; i < strategyCount; i++) {
    
    if(strategies[i].expectedValue > bestEV) {
      bestEV = strategies[i].expectedValue;
      bestIdx = i;
    }
  }
  
  
  // Apply best strategy
  for(int i = 0; i < 5; i++) {
    held[i] = strategies[bestIdx].holds[i];
  }
  
  // FINAL VERIFICATION
  int heldCount = 0;
  for(int i = 0; i < 5; i++) {
    if(held[i]) heldCount++;
  }
  
  for(int i = 0; i < 5; i++) {
  }
  
  // **NEW: Record hold pattern for learning**
  if(vsComputer && currentPlayer == 2) {
    recordHoldPattern(dice, held);
  }
  
  updateDiceDisplay();
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
  
  // Write strategy variants
  addr = EEPROM_STRATEGY_VARIANTS_ADDR;
  for(int v = 0; v < 4; v++) {
    uint8_t* variantPtr = (uint8_t*)&strategyVariants[v];
    for(size_t i = 0; i < sizeof(StrategyVariant); i++) {
      EEPROM.write(addr, variantPtr[i]);
      addr++;
    }
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
  
  // Load strategy variants
  addr = EEPROM_STRATEGY_VARIANTS_ADDR;
  for(int v = 0; v < 4; v++) {
    uint8_t* variantPtr = (uint8_t*)&strategyVariants[v];
    for(size_t i = 0; i < sizeof(StrategyVariant); i++) {
      variantPtr[i] = EEPROM.read(addr);
      addr++;
    }
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
  aiLearning.yahtzeeAttempts = 0;
  aiLearning.yahtzeeSuccesses = 0;
  aiLearning.straightAttempts = 0;
  aiLearning.straightSuccesses = 0;
  aiLearning.avgWinningScore = 0;
  aiLearning.earlyYahtzees = 0;
  aiLearning.lateYahtzees = 0;
  aiLearning.bonusAchieved = 0;
  aiLearning.pairHoldAttempts = 0;
  aiLearning.pairHoldSuccesses = 0;
  aiLearning.rerollOnGoodHand = 0;
  aiLearning.rerollSuccesses = 0;
  aiLearning.totalSelfPlayGames = 0;
  aiLearning.weightUpperBonus = 70;       // 7.0x default (more aggressive)
  
  // Set default strategy weights
  aiLearning.weightYahtzee = 20;          // 2.0x
  aiLearning.weightLargeStraight = 20;    // 2.0x
  aiLearning.weightSmallStraight = 16;    // 1.6x
  aiLearning.weightFullHouse = 14;        // 1.4x
  aiLearning.weight3OfKindLow = 30;       // 30 points
  aiLearning.weight3OfKindHigh = 80;      // 8.0x
  aiLearning.weightStraightHold = 35;     // 35 points
  
  // Reset performance tracking
  aiLearning.gamesWithYahtzee = 0;
  aiLearning.gamesWithLargeStraight = 0;
  aiLearning.gamesWithSmallStraight = 0;
  aiLearning.gamesWithFullHouse = 0;
  aiLearning.winsWithYahtzee = 0;
  aiLearning.winsWithLargeStraight = 0;
  aiLearning.winsWithSmallStraight = 0;
  aiLearning.winsWithFullHouse = 0;
  
  // **NEW: Initialize category usage patterns**
  for(int i = 0; i < 13; i++) {
    aiLearning.categoryScoredCount[i] = 0;
    aiLearning.categoryWinsWhenScored[i] = 0;
  }
  
  // **NEW: Initialize upper bonus granularity**
  aiLearning.bonusAchievedVsHuman = 0;
  aiLearning.bonusAchievedAndWon = 0;
  aiLearning.noBonusButWon = 0;
  
  // **NEW: Initialize turn-phase performance**
  aiLearning.earlyGameWins = 0;
  aiLearning.earlyGameLosses = 0;
  aiLearning.lateGameComebacks = 0;
  
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
    aiLearning.rollCountWins[i] = 0;
  }
  
  // **ENHANCED: Initialize hold pattern metrics**
  aiLearning.holdPattern0Dice = 0;
  aiLearning.holdPattern1Dice = 0;
  aiLearning.holdPattern2Dice = 0;
  aiLearning.holdPattern3Dice = 0;
  aiLearning.holdPattern4Dice = 0;
  aiLearning.holdPattern5Dice = 0;
  for(int i = 0; i < 6; i++) {
    aiLearning.holdPatternWins[i] = 0;
  }
  
  // **ENHANCED: Initialize bonus pursuit analytics**
  aiLearning.bonusPursuitAbandoned = 0;
  aiLearning.bonusAbandonWins = 0;
  aiLearning.bonusNearMiss = 0;
  aiLearning.bonusOverkill = 0;
  
  // **ENHANCED: Initialize opponent modeling**
  aiLearning.humanAvgScore = 0;
  aiLearning.humanBonusRate = 0;
  aiLearning.humanRiskLevel = 0;
  aiLearning.humanYahtzees = 0;
  
  // **ENHANCED: Initialize endgame scenarios**
  aiLearning.endgameCloseWins = 0;
  aiLearning.endgameCloseLosses = 0;
  aiLearning.endgameBlowoutWins = 0;
  aiLearning.endgameComebackAttempts = 0;
  aiLearning.endgameComebackSuccess = 0;
  
  // **ENHANCED: Initialize category timing**
  aiLearning.optimalTurnForYahtzee = 0;
  aiLearning.optimalTurnForLargeStraight = 0;
  aiLearning.optimalTurnForBonus = 0;
  
  // **NEW: Initialize hold pattern value tracking**
  for(int v = 0; v < 6; v++) {
    for(int c = 0; c < 6; c++) {
      aiLearning.holdValueCount[v][c] = 0;
      aiLearning.holdValueScoreSum[v][c] = 0;
      aiLearning.holdValueSuccess[v][c] = 0;
    }
  }
  
  // **NEW: Initialize reroll decision quality tracking**
  aiLearning.rerollDecisions = 0;
  aiLearning.rerollImprovements = 0;
  aiLearning.rerollDegradations = 0;
  aiLearning.totalRerollChange = 0;
  aiLearning.rerollWith0_9pts = 0;
  aiLearning.rerollWith10_19pts = 0;
  aiLearning.rerollWith20_29pts = 0;
  aiLearning.rerollWith30plus = 0;
  aiLearning.rerollImprove0_9 = 0;
  aiLearning.rerollImprove10_19 = 0;
  aiLearning.rerollImprove20_29 = 0;
  aiLearning.rerollImprove30plus = 0;
  
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
  
  
  // Reset all 4 strategy variants to defaults
  for(int variant = 0; variant < 4; variant++) {
    strategyVariants[variant].weightYahtzee = 20;
    strategyVariants[variant].weightLargeStraight = 20;
    strategyVariants[variant].weightSmallStraight = 16;
    strategyVariants[variant].weightFullHouse = 14;
    strategyVariants[variant].weight3OfKindLow = 30;
    strategyVariants[variant].weight3OfKindHigh = 80;
    strategyVariants[variant].weightStraightHold = 35;
    strategyVariants[variant].weightUpperBonus = 70;
    strategyVariants[variant].wins = 0;
    strategyVariants[variant].losses = 0;
    strategyVariants[variant].totalScore = 0;
    strategyVariants[variant].gamesPlayed = 0;
  }
  
  // **V39: Initialize enhanced graph metrics**
  aiLearning.recentWins = 0;
  aiLearning.recentGamesCount = 0;
  
  for(int i = 0; i < 6; i++) {
    aiLearning.upperSectionAttempts[i] = 0;
    aiLearning.upperSectionSuccesses[i] = 0;
  }
  
  aiLearning.fullHouseAttempts = 0;
  aiLearning.fullHouseSuccesses = 0;
  aiLearning.threeKindAttempts = 0;
  aiLearning.threeKindSuccesses = 0;
  aiLearning.fourKindAttempts = 0;
  aiLearning.fourKindSuccesses = 0;
  
  for(int i = 0; i < 10; i++) {
    aiLearning.scoreRanges[i] = 0;
  }
  
  aiLearning.earlyGamesAvgScore = 0;
  aiLearning.recentGamesAvgScore = 0;

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
  
  // ============ STRATEGY WEIGHTS ============
  Serial.println("STRATEGY WEIGHTS,,,,");
  Serial.println("Weight,Value,,,");
  
  Serial.print("Yahtzee Priority,");
  Serial.println(aiLearning.weightYahtzee / 10.0, 1);
  
  Serial.print("Large Straight Priority,");
  Serial.println(aiLearning.weightLargeStraight / 10.0, 1);
  
  Serial.print("Small Straight Priority,");
  Serial.println(aiLearning.weightSmallStraight / 10.0, 1);
  
  Serial.print("Full House Priority,");
  Serial.println(aiLearning.weightFullHouse / 10.0, 1);
  
  Serial.print("3-of-Kind Low Value,");
  Serial.println(aiLearning.weight3OfKindLow);
  
  Serial.print("3-of-Kind High Value,");
  Serial.println(aiLearning.weight3OfKindHigh / 10.0, 1);
  
  Serial.print("Straight Hold Priority,");
  Serial.println(aiLearning.weightStraightHold);
  
  Serial.print("Upper Bonus Priority,");
  Serial.println(aiLearning.weightUpperBonus / 10.0, 1);
  
  Serial.println(",,,,");
  
  // ============ SPECIAL ACHIEVEMENTS ============
  Serial.println("SPECIAL ACHIEVEMENTS - ATTEMPTS,,,");
  Serial.println("Achievement,Attempts,Successes,Success Rate %");
  
  Serial.print("Yahtzee,");
  Serial.print(aiLearning.yahtzeeAttempts);
  Serial.print(",");
  Serial.print(aiLearning.yahtzeeSuccesses);
  Serial.print(",");
  if(aiLearning.yahtzeeAttempts > 0) {
    Serial.println((aiLearning.yahtzeeSuccesses * 100.0) / aiLearning.yahtzeeAttempts, 1);
  } else {
    Serial.println(0);
  }
  
  Serial.print("Straights,");
  Serial.print(aiLearning.straightAttempts);
  Serial.print(",");
  Serial.print(aiLearning.straightSuccesses);
  Serial.print(",");
  if(aiLearning.straightAttempts > 0) {
    Serial.println((aiLearning.straightSuccesses * 100.0) / aiLearning.straightAttempts, 1);
  } else {
    Serial.println(0);
  }
  
  Serial.println(",,,");
  Serial.println("SPECIAL ACHIEVEMENTS - GAME WINS,,");
  Serial.println("Achievement,Count,Win Rate %");
  
  Serial.print("Games With Yahtzee,");
  Serial.print(aiLearning.gamesWithYahtzee);
  Serial.print(",");
  if(aiLearning.gamesWithYahtzee > 0) {
    Serial.println((aiLearning.winsWithYahtzee * 100.0) / aiLearning.gamesWithYahtzee, 1);
  } else {
    Serial.println(0);
  }
  
  Serial.print("Games With Large Straight,");
  Serial.print(aiLearning.gamesWithLargeStraight);
  Serial.print(",");
  if(aiLearning.gamesWithLargeStraight > 0) {
    Serial.println((aiLearning.winsWithLargeStraight * 100.0) / aiLearning.gamesWithLargeStraight, 1);
  } else {
    Serial.println(0);
  }
  
  Serial.print("Games With Small Straight,");
  Serial.print(aiLearning.gamesWithSmallStraight);
  Serial.print(",");
  if(aiLearning.gamesWithSmallStraight > 0) {
    Serial.println((aiLearning.winsWithSmallStraight * 100.0) / aiLearning.gamesWithSmallStraight, 1);
  } else {
    Serial.println(0);
  }
  
  Serial.print("Games With Full House,");
  Serial.print(aiLearning.gamesWithFullHouse);
  Serial.print(",");
  if(aiLearning.gamesWithFullHouse > 0) {
    Serial.println((aiLearning.winsWithFullHouse * 100.0) / aiLearning.gamesWithFullHouse, 1);
  } else {
    Serial.println(0);
  }
  
  Serial.println(",,");
  
  // ============ UPPER BONUS TRACKING ============
  Serial.println("UPPER BONUS ANALYSIS,,");
  Serial.println("Metric,Value,");
  
  Serial.print("Bonus Achieved (vs Human),");
  Serial.println(aiLearning.bonusAchievedVsHuman);
  
  Serial.print("Bonus AND Won,");
  Serial.println(aiLearning.bonusAchievedAndWon);
  
  Serial.print("Bonus Win Rate %,");
  if(aiLearning.bonusAchievedVsHuman > 0) {
    Serial.println((aiLearning.bonusAchievedAndWon * 100.0) / aiLearning.bonusAchievedVsHuman, 1);
  } else {
    Serial.println(0);
  }
  
  Serial.print("Won Without Bonus,");
  Serial.println(aiLearning.noBonusButWon);
  
  int noBonusGames = aiLearning.gamesPlayed - aiLearning.bonusAchievedVsHuman;
  Serial.print("No-Bonus Win Rate %,");
  if(noBonusGames > 0) {
    Serial.println((aiLearning.noBonusButWon * 100.0) / noBonusGames, 1);
  } else {
    Serial.println(0);
  }
  
  Serial.print("Bonus Near Miss (60-62),");
  Serial.println(aiLearning.bonusNearMiss);
  
  Serial.print("Bonus Overkill (75+),");
  Serial.println(aiLearning.bonusOverkill);
  
  Serial.print("Bonus Pursuit Abandoned,");
  Serial.println(aiLearning.bonusPursuitAbandoned);
  
  Serial.println(",,");
  
  // ============ OPPONENT MODELING ============
  Serial.println("OPPONENT ANALYSIS,,");
  Serial.println("Metric,Value,");
  
  Serial.print("Human Average Score,");
  Serial.println(aiLearning.humanAvgScore * 3);
  
  Serial.print("Human Bonus Rate %,");
  Serial.println(aiLearning.humanBonusRate);
  
  Serial.print("Human Risk Level (0-100),");
  Serial.println(aiLearning.humanRiskLevel);
  
  Serial.print("Human Total Yahtzees,");
  Serial.println(aiLearning.humanYahtzees);
  
  Serial.println(",,,");
  
  // ============ ENDGAME PERFORMANCE ============
  Serial.println("ENDGAME SCENARIOS,,,");
  Serial.println("Scenario,Wins,Losses,Win Rate %");
  
  Serial.print("Close Games (<=20 pts),");
  Serial.print(aiLearning.endgameCloseWins);
  Serial.print(",");
  Serial.print(aiLearning.endgameCloseLosses);
  Serial.print(",");
  int closeTotal = aiLearning.endgameCloseWins + aiLearning.endgameCloseLosses;
  if(closeTotal > 0) {
    Serial.println((aiLearning.endgameCloseWins * 100.0) / closeTotal, 1);
  } else {
    Serial.println(0);
  }
  
  Serial.print("Blowout Wins (50+ pts),");
  Serial.print(aiLearning.endgameBlowoutWins);
  Serial.print(",");
  Serial.print(0);  // No losses tracked
  Serial.print(",");
  Serial.println("N/A");
  
  Serial.print("Comeback Attempts,");
  Serial.print(aiLearning.endgameComebackAttempts);
  Serial.print(",");
  Serial.print(0);
  Serial.print(",");
  Serial.println("N/A");
  
  Serial.print("Successful Comebacks,");
  Serial.print(aiLearning.endgameComebackSuccess);
  Serial.print(",");
  Serial.print(0);
  Serial.print(",");
  if(aiLearning.endgameComebackAttempts > 0) {
    Serial.println((aiLearning.endgameComebackSuccess * 100.0) / aiLearning.endgameComebackAttempts, 1);
  } else {
    Serial.println(0);
  }
  
  Serial.println(",,,");
  
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
  
  // ============ STRATEGY VARIANTS ============
  Serial.println("STRATEGY VARIANTS,,,,,");
  Serial.println("Variant,Games Played,Wins,Losses,Win Rate %,Avg Score");
  
  for(int v = 0; v < 4; v++) {
    Serial.print("Variant ");
    Serial.print(v + 1);
    Serial.print(",");
    Serial.print(strategyVariants[v].gamesPlayed);
    Serial.print(",");
    Serial.print(strategyVariants[v].wins);
    Serial.print(",");
    Serial.print(strategyVariants[v].losses);
    Serial.print(",");
    if(strategyVariants[v].gamesPlayed > 0) {
      Serial.print((strategyVariants[v].wins * 100.0) / strategyVariants[v].gamesPlayed, 1);
    } else {
      Serial.print(0);
    }
    Serial.print(",");
    Serial.println(strategyVariants[v].getAvgScore());
  }
  
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
void trackCategoryEfficiency(int category, int score, int* dice) {
  // Upper section (0-5 = 1s through 6s)
  if(category >= 0 && category <= 5) {
    aiLearning.upperSectionAttempts[category]++;
    
    // Count dice for this value
    int value = category + 1;
    int count = 0;
    for(int i = 0; i < 5; i++) {
      if(dice[i] == value) count++;
    }
    
    // Success = 3 or more dice (good score)
    if(count >= 3) {
      aiLearning.upperSectionSuccesses[category]++;
    }
  }
  // 3-of-a-kind
  else if(category == 6) {
    int counts[7] = {0};
    for(int i = 0; i < 5; i++) counts[dice[i]]++;
    
    int maxCount = 0;
    for(int v = 1; v <= 6; v++) {
      if(counts[v] > maxCount) maxCount = counts[v];
    }
    
    aiLearning.threeKindAttempts++;
    if(maxCount >= 3) aiLearning.threeKindSuccesses++;
  }
  // 4-of-a-kind
  else if(category == 7) {
    int counts[7] = {0};
    for(int i = 0; i < 5; i++) counts[dice[i]]++;
    
    int maxCount = 0;
    for(int v = 1; v <= 6; v++) {
      if(counts[v] > maxCount) maxCount = counts[v];
    }
    
    aiLearning.fourKindAttempts++;
    if(maxCount >= 4) aiLearning.fourKindSuccesses++;
  }
  // Full House
  else if(category == 8) {
    int counts[7] = {0};
    for(int i = 0; i < 5; i++) counts[dice[i]]++;
    
    bool hasThree = false, hasTwo = false;
    for(int v = 1; v <= 6; v++) {
      if(counts[v] == 3) hasThree = true;
      if(counts[v] == 2) hasTwo = true;
    }
    
    aiLearning.fullHouseAttempts++;
    if(hasThree && hasTwo && score > 0) aiLearning.fullHouseSuccesses++;
  }
}


void updateAILearning(bool aiWon, int aiScore, int humanScore) {
  aiLearning.gamesPlayed++;
  
  // Track what the AI (Player 2) achieved this game
  bool aiGotYahtzee = (scores2[11] == 50);
  bool aiGotLargeStraight = (scores2[10] == 40);
  bool aiGotSmallStraight = (scores2[9] == 30);
  bool aiGotFullHouse = (scores2[8] == 25);
  
  // **ENHANCED: Track opponent (human) patterns**
  // Update human average score (running average)
  if(aiLearning.gamesPlayed > 1) {
    int oldAvg = aiLearning.humanAvgScore * 3;  // Convert back from /3 storage
    int newAvg = ((oldAvg * (aiLearning.gamesPlayed - 1)) + humanScore) / aiLearning.gamesPlayed;
    aiLearning.humanAvgScore = newAvg / 3;  // Store as /3
  } else {
    aiLearning.humanAvgScore = humanScore / 3;
  }
  
  // Track human bonus achievement rate (out of 100)
  int humanUpperTotal = getUpperSectionTotal(scores1);
  if(humanUpperTotal >= 63) {
    aiLearning.humanBonusRate = ((aiLearning.humanBonusRate * (aiLearning.gamesPlayed - 1)) + 100) / aiLearning.gamesPlayed;
  } else {
    aiLearning.humanBonusRate = (aiLearning.humanBonusRate * (aiLearning.gamesPlayed - 1)) / aiLearning.gamesPlayed;
  }
  
  // Track human Yahtzees
  if(scores1[11] == 50) {
    aiLearning.humanYahtzees++;
  }
  
  // Estimate human risk level based on their category choices
  int humanRiskScore = 0;
  if(scores1[11] == 50) humanRiskScore += 30;  // Yahtzee = high risk
  if(scores1[10] == 40) humanRiskScore += 20;  // Large Straight = medium-high risk
  if(scores1[9] == 30) humanRiskScore += 10;   // Small Straight = medium risk
  // Update running average of human risk level
  aiLearning.humanRiskLevel = ((aiLearning.humanRiskLevel * (aiLearning.gamesPlayed - 1)) + humanRiskScore) / aiLearning.gamesPlayed;
  
  // **ENHANCED: Endgame scenario tracking**
  int aiUpperTotal = getUpperSectionTotal(scores2);
  int scoreDiff = abs(aiScore - humanScore);
  
  if(scoreDiff <= 20) {
    // Close game
    if(aiWon) {
      aiLearning.endgameCloseWins++;
      // Update hold pattern wins
      for(int i = 0; i < 6; i++) {
        // Wins credited to all hold patterns used (approximate)
      }
    } else {
      aiLearning.endgameCloseLosses++;
    }
  } else if(aiScore > humanScore + 50 && aiWon) {
    // Blowout win
    aiLearning.endgameBlowoutWins++;
  } else if(humanScore > aiScore + 30 && aiWon) {
    // Successful comeback!
    aiLearning.endgameComebackSuccess++;
  }
  
  // Track roll count wins - credit the MOST USED roll strategy this game
  if(aiWon) {
    // Find which roll count was used most this game
    int maxUsed = 0;
    int primaryRollCount = 1;
    for(int r = 1; r <= 3; r++) {
      // Note: rollCountUsed is cumulative across all games
      // We can't easily track per-game usage without more state
      // So we credit all used counts (this represents: "when winning, I tend to use these roll counts")
      if(aiLearning.rollCountUsed[r] > maxUsed) {
        maxUsed = aiLearning.rollCountUsed[r];
        primaryRollCount = r;
      }
    }
    // Credit the most frequently used roll count
    // (This is an approximation - ideally we'd track per-game roll count usage)
    if(maxUsed > 0) {
      aiLearning.rollCountWins[primaryRollCount]++;
    }
  }
  
  // **ENHANCED: Bonus pursuit analytics**
  if(aiUpperTotal >= 63 && aiUpperTotal <= 69) {
    // Normal bonus achievement
    if(aiUpperTotal >= 75) {
      aiLearning.bonusOverkill++;
    }
  } else if(aiUpperTotal >= 60 && aiUpperTotal <= 62) {
    // Near miss!
    aiLearning.bonusNearMiss++;
  } else if(aiUpperTotal < 50) {
    // Likely abandoned bonus pursuit
    aiLearning.bonusPursuitAbandoned++;
    if(aiWon) {
      aiLearning.bonusAbandonWins++;
    }
  }
  
  // Track when bonus was completed (which turn)
  if(aiUpperTotal >= 63) {
    // Find which turn AI completed the upper section
    int upperCategoriesUsed = 0;
    for(int i = 0; i <= 5; i++) {
      if(scores2[i] != -1) upperCategoriesUsed++;
    }
    if(upperCategoriesUsed >= 4 && upperCategoriesUsed <= 6) {
      // Completed bonus relatively early
      if(aiLearning.optimalTurnForBonus == 0) {
        aiLearning.optimalTurnForBonus = upperCategoriesUsed + 4;  // Approximate turn
      }
    }
  }
  
  // **Track high-risk successes**
  // A high-risk play succeeds when the AI actually scores Yahtzee or Large Straight.
  // These are the high-value targets that holdDecisionWithVariant flags as high-risk
  // when pursued with only 2-3 matching dice or a short consecutive run.
  // Record one success per achievement scored this game.
  if(aiGotYahtzee) {
    aiLearning.highRiskSuccesses++;
    if(aiWon) {
      aiLearning.highRiskWins++;
    }
  }
  if(aiGotLargeStraight) {
    aiLearning.highRiskSuccesses++;
    if(aiWon) {
      aiLearning.highRiskWins++;
    }
  }
  
  // **NEW: Track category usage patterns (vs human only)**
  // Note: categoryScoredCount is now incremented during scoring (in selectCategory)
  // Here we only update the win tracking
  for(int i = 0; i < 13; i++) {
    if(scores2[i] != -1) {  // AI scored this category
      if(aiWon) {
        aiLearning.categoryWinsWhenScored[i]++;
      }
    }
  }
  
  // **NEW: Track upper bonus granularity (vs human only)**
  if(aiUpperTotal >= 63) {
    aiLearning.bonusAchievedVsHuman++;
    if(aiWon) {
      aiLearning.bonusAchievedAndWon++;
    }
  } else {
    if(aiWon) {
      aiLearning.noBonusButWon++;
    }
  }
  
  // **NEW: Track turn-phase performance (vs human only)**
  // Simple heuristic based on final score margin
  if(aiWon) {
    int margin = aiScore - humanScore;
    
    if(margin >= 50) {
      aiLearning.earlyGameWins++;  // Likely dominated from start
    } else if(margin < 20) {
      aiLearning.lateGameComebacks++;  // Close game, might have been trailing
    } else {
      aiLearning.earlyGameWins++;  // Medium margin, probably steady lead
    }
  } else {
    // AI lost - check if it was close
    int margin = humanScore - aiScore;
    if(margin < 20) {
      aiLearning.earlyGameLosses++;  // Close loss, may have been leading at some point
    }
  }

  if(aiGotYahtzee) {
    aiLearning.gamesWithYahtzee++;
    aiLearning.yahtzeeSuccesses++;
  }
  if(aiGotLargeStraight) {
    aiLearning.gamesWithLargeStraight++;
    aiLearning.straightSuccesses++;
  }
  if(aiGotSmallStraight) {
    aiLearning.gamesWithSmallStraight++;
    aiLearning.straightSuccesses++;
  }
  if(aiGotFullHouse) {
    aiLearning.gamesWithFullHouse++;
  }

  // Track upper section bonus achievement (overall, not just vs human)
  if(aiUpperTotal >= 63) {
    aiLearning.bonusAchieved++;
  }
  
  // **NEW: Update hold pattern success based on game outcome**
  // We'll retroactively credit successful hold patterns based on final scores
  // Note: This is a simplified approach - ideally we'd track during the game
  for(int i = 0; i < 13; i++) {
    if(scores2[i] != -1 && scores2[i] >= 20) {
      // This category scored 20+ points, credit hold patterns
      // We don't know which specific holds led to this, but we can
      // make reasonable assumptions based on the category
      if(i == 11) {
        // Yahtzee - likely held multiples of same value
        int value = 6;  // Assume highest value
        for(int count = 3; count <= 5; count++) {
          if(aiLearning.holdValueCount[value-1][count-1] > 0) {
            // Increment success for this pattern
            int vIdx = value - 1;
            int cIdx = count - 1;
            aiLearning.holdValueSuccess[vIdx][cIdx]++;
          }
        }
      }
    }
  }
  
  // Mark all decisions in this game with the outcome
  for(int i = 0; i < decisionHistoryCount; i++) {
    // Mark recent decisions (from this game) with won/lost outcome
    // Assume last 13 decisions are from this game (one per turn)
    int recentIndex = (decisionHistoryIndex - 1 - i + MAX_DECISION_HISTORY) % MAX_DECISION_HISTORY;
    if(i < 13) {  // Last 13 decisions
      decisionHistory[recentIndex].wonGame = aiWon;
    }
  }
  
  // **CRITICAL FIX**: Track strategy performance properly
  // Always increment the strategy counter (win or loss)
  if(currentGameStrategy == 1) {
    // AI played aggressively this game
    if(aiWon) {
      aiLearning.aggressiveWins++;
    }
    // Note: We can calculate aggressive losses as: gamesPlayed - (aggressiveWins + conservativeWins)
  } else {
    // AI played conservatively this game  
    if(aiWon) {
      aiLearning.conservativeWins++;
    }
    // Note: We can calculate conservative losses as: gamesPlayed - (aggressiveWins + conservativeWins)
  }
  
  // Track wins with achievements
  if(aiWon) {
    if(aiGotYahtzee) aiLearning.winsWithYahtzee++;
    if(aiGotLargeStraight) aiLearning.winsWithLargeStraight++;
    if(aiGotSmallStraight) aiLearning.winsWithSmallStraight++;
    if(aiGotFullHouse) aiLearning.winsWithFullHouse++;
  }
  
  // Adjust weights every 10 games based on win correlation
if(aiLearning.gamesPlayed % 10 == 0 && aiLearning.gamesPlayed >= 20) {
  // Calculate win rates for each strategy
  float yahtzeeWinRate = (aiLearning.gamesWithYahtzee > 0) ? 
    (float)aiLearning.winsWithYahtzee / aiLearning.gamesWithYahtzee : 0.5;
  float lgStraightWinRate = (aiLearning.gamesWithLargeStraight > 0) ?
    (float)aiLearning.winsWithLargeStraight / aiLearning.gamesWithLargeStraight : 0.5;
  float smStraightWinRate = (aiLearning.gamesWithSmallStraight > 0) ?
    (float)aiLearning.winsWithSmallStraight / aiLearning.gamesWithSmallStraight : 0.5;
  float fullHouseWinRate = (aiLearning.gamesWithFullHouse > 0) ?
    (float)aiLearning.winsWithFullHouse / aiLearning.gamesWithFullHouse : 0.5;
  
  // ═══════════════════════════════════════════════════════════════
  // ADAPTIVE LEARNING RATE: Adjust based on confidence (sample size)
  // ═══════════════════════════════════════════════════════════════
  
  // Calculate confidence for each category (0.0 to 1.0)
  // Confidence builds over 50 games, then caps at 1.0
  float yahtzeeConfidence = min(1.0f, (float)aiLearning.gamesWithYahtzee / 50.0f);
  float lgStraightConfidence = min(1.0f, (float)aiLearning.gamesWithLargeStraight / 50.0f);
  float smStraightConfidence = min(1.0f, (float)aiLearning.gamesWithSmallStraight / 50.0f);
  float fullHouseConfidence = min(1.0f, (float)aiLearning.gamesWithFullHouse / 50.0f);
  
  // Calculate adaptive adjustment amounts (scales from 1 to 4 based on confidence)
  int yahtzeeAdjust = max(1, (int)(yahtzeeConfidence * 4));
  int lgStraightAdjust = max(1, (int)(lgStraightConfidence * 4));
  int smStraightAdjust = max(1, (int)(smStraightConfidence * 4));
  int fullHouseAdjust = max(1, (int)(fullHouseConfidence * 4));
  
  
  // Adjust weights based on win correlation with ADAPTIVE amounts
  if(yahtzeeWinRate > 0.55) {
    aiLearning.weightYahtzee = min(50, aiLearning.weightYahtzee + yahtzeeAdjust);
  } else if(yahtzeeWinRate < 0.45) {
    aiLearning.weightYahtzee = max(10, aiLearning.weightYahtzee - yahtzeeAdjust);
  }
  
  if(lgStraightWinRate > 0.55) {
    aiLearning.weightLargeStraight = min(50, aiLearning.weightLargeStraight + lgStraightAdjust);
  } else if(lgStraightWinRate < 0.45) {
    aiLearning.weightLargeStraight = max(10, aiLearning.weightLargeStraight - lgStraightAdjust);
  }
  
  if(smStraightWinRate > 0.55) {
    aiLearning.weightSmallStraight = min(50, aiLearning.weightSmallStraight + smStraightAdjust);
  } else if(smStraightWinRate < 0.45) {
    aiLearning.weightSmallStraight = max(8, aiLearning.weightSmallStraight - smStraightAdjust);
  }
  
  if(fullHouseWinRate > 0.55) {
    aiLearning.weightFullHouse = min(50, aiLearning.weightFullHouse + fullHouseAdjust);
  } else if(fullHouseWinRate < 0.45) {
    aiLearning.weightFullHouse = max(8, aiLearning.weightFullHouse - fullHouseAdjust);
  }
    
  }
  
  if(currentGameIsExploration) {
  }

  // ═══════════════════════════════════════════════════════════════
  // **V39: UPDATE ENHANCED GRAPH METRICS**
  // ═══════════════════════════════════════════════════════════════
  
  // Update score distribution histogram
  int bucket = min(aiScore / 50, 9);
  aiLearning.scoreRanges[bucket]++;
  
  // Update rolling window (last 50 games)
  if(aiLearning.recentGamesCount < 50) {
    aiLearning.recentGamesCount++;
    if(aiWon) aiLearning.recentWins++;
  } else {
    // Maintain rolling window - approximate by reducing proportionally
    aiLearning.recentWins = (aiLearning.recentWins * 49) / 50;
    aiLearning.recentGamesCount = 50;
    if(aiWon) aiLearning.recentWins++;
  }
  
  // Update early vs recent comparison
  if(aiLearning.gamesPlayed <= 50) {
    aiLearning.earlyGamesAvgScore = 
      (aiLearning.earlyGamesAvgScore * (aiLearning.gamesPlayed - 1) + aiScore) 
      / aiLearning.gamesPlayed;
  }
  
  if(aiLearning.gamesPlayed > 50) {
    // Update recent games average (rolling 50)
    aiLearning.recentGamesAvgScore = 
      (aiLearning.recentGamesAvgScore * 49 + aiScore) / 50;
  } else {
    aiLearning.recentGamesAvgScore = aiScore;
  }
  
  // **CRITICAL: Save updated learning data**
  
  // Force save
  saveAILearningToFile();
  
  // Run decision pattern analysis every 20 games
  if(aiLearning.gamesPlayed % 20 == 0 && aiLearning.gamesPlayed >= 20) {
    analyzeDecisionPatterns();
  }
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
void recordHoldPattern(int dice[], bool held[]) {
  // Count what values are being held and how many
  int holdCounts[7] = {0};  // holdCounts[value] = how many of that value held
  
  for(int i = 0; i < 5; i++) {
    if(held[i]) {
      holdCounts[dice[i]]++;
    }
  }
  
  // Record each value that was held
  for(int value = 1; value <= 6; value++) {
    int count = holdCounts[value];
    if(count > 0 && count <= 5) {
      aiLearning.holdValueCount[value-1][count-1]++;
    }
  }
}

// **NEW: Update hold pattern success after scoring**
void updateHoldPatternSuccess(int dice[], bool held[], int finalScore) {
  // Track which values were held
  int holdCounts[7] = {0};
  
  for(int i = 0; i < 5; i++) {
    if(held[i]) {
      holdCounts[dice[i]]++;
    }
  }
  
  // Update success metrics
  for(int value = 1; value <= 6; value++) {
    int count = holdCounts[value];
    if(count > 0 && count <= 5) {
      int vIdx = value - 1;
      int cIdx = count - 1;
      
      aiLearning.holdValueScoreSum[vIdx][cIdx] += finalScore;
      
      if(finalScore >= 20) {
        aiLearning.holdValueSuccess[vIdx][cIdx]++;
      }
    }
  }
}

// **NEW: Helper function to get best available category score**
// Used for reroll decision quality tracking
int getBestAvailableCategoryScore(int* dice, int* scores) {
  int bestScore = 0;
  for(int i = 0; i < 13; i++) {
    if(scores[i] == -1) {  // Only consider unused categories
      int score = calculateCategoryScore(dice, i);
      if(score > bestScore) {
        bestScore = score;
      }
    }
  }
  return bestScore;
}

// **NEW: Track reroll decision quality**
void recordRerollDecision(int rollNum, int pointsBefore, int pointsAfter) {
  aiLearning.rerollDecisions++;
  
  int pointChange = pointsAfter - pointsBefore;
  aiLearning.totalRerollChange += pointChange;
  
  // Track improvement/degradation
  if(pointsAfter > pointsBefore) {
    aiLearning.rerollImprovements++;
  } else if(pointsAfter < pointsBefore) {
    aiLearning.rerollDegradations++;
  }
  
  // Breakdown by points available before reroll
  if(pointsBefore < 10) {
    aiLearning.rerollWith0_9pts++;
    if(pointsAfter > pointsBefore) aiLearning.rerollImprove0_9++;
  } else if(pointsBefore < 20) {
    aiLearning.rerollWith10_19pts++;
    if(pointsAfter > pointsBefore) aiLearning.rerollImprove10_19++;
  } else if(pointsBefore < 30) {
    aiLearning.rerollWith20_29pts++;
    if(pointsAfter > pointsBefore) aiLearning.rerollImprove20_29++;
  } else {
    aiLearning.rerollWith30plus++;
    if(pointsAfter > pointsBefore) aiLearning.rerollImprove30plus++;
  }
}

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
float getRerollRecommendation(int pointsAvailable) {
  // Returns probability that rerolling will improve (0.0 to 1.0)
  
  uint16_t attempts, improvements;
  
  if(pointsAvailable < 10) {
    attempts = aiLearning.rerollWith0_9pts;
    improvements = aiLearning.rerollImprove0_9;
  } else if(pointsAvailable < 20) {
    attempts = aiLearning.rerollWith10_19pts;
    improvements = aiLearning.rerollImprove10_19;
  } else if(pointsAvailable < 30) {
    attempts = aiLearning.rerollWith20_29pts;
    improvements = aiLearning.rerollImprove20_29;
  } else {
    attempts = aiLearning.rerollWith30plus;
    improvements = aiLearning.rerollImprove30plus;
  }
  
  if(attempts < 5) return 0.5;  // Not enough data, assume 50/50
  
  return (float)improvements / attempts;
}

// **NEW: Get category win rate for decision weighting**
float getCategoryWinRate(int category) {
  if(category < 0 || category >= 13) return 0.5;
  
  uint16_t timesScored = aiLearning.categoryScoredCount[category];
  if(timesScored < 3) return 0.5;  // Not enough data
  
  return (float)aiLearning.categoryWinsWhenScored[category] / timesScored;
}

// **NEW: Get category average score**
float getCategoryAvgScore(int category) {
  if(category < 0 || category >= 13) return 0;
  
  uint16_t timesScored = aiLearning.categoryScoredCount[category];
  if(timesScored == 0) return 0;
  
  // Use categoryScoreSum (same as used for categoryAvgScore calculation)
  return (float)aiLearning.categoryScoreSum[category] / timesScored;
}

void analyzeDecisionPatterns() {
  if(decisionHistoryCount < 10) {
    return;  // Need at least 10 decisions to analyze
  }
  
  
  // Pattern 1: Early aggressive scoring correlation
  int earlyHighScores = 0;  // High scores (20+) in turns 1-4
  int earlyHighScoreWins = 0;
  
  // Pattern 2: Holding pairs early
  int earlyPairHolds = 0;  // Holding 2 dice in turns 1-4
  int earlyPairHoldWins = 0;
  
  // Pattern 3: Late game risk-taking
  int lateRiskTaking = 0;  // Going for Yahtzee/straights in turns 10-13
  int lateRiskWins = 0;
  
  // Analyze all recorded decisions
  for(int i = 0; i < decisionHistoryCount; i++) {
    DecisionOutcome& decision = decisionHistory[i];
    
    // Early game patterns (turns 1-4)
    if(decision.turnNumber <= 4) {
      if(decision.pointsScored >= 20) {
        earlyHighScores++;
        if(decision.wonGame) earlyHighScoreWins++;
      }
      
      if(decision.diceHeld == 2) {
        earlyPairHolds++;
        if(decision.wonGame) earlyPairHoldWins++;
      }
    }
    
    // Late game patterns (turns 10-13)
    if(decision.turnNumber >= 10) {
      // Check if going for high-value categories
      if(decision.categoryScored == 11 || decision.categoryScored == 10 || 
         decision.categoryScored == 9) {
        lateRiskTaking++;
        if(decision.wonGame) lateRiskWins++;
      }
    }
  }
  
  // Calculate and display win rates
  if(earlyHighScores > 0) {
    float earlyHighScoreWinRate = (float)earlyHighScoreWins / earlyHighScores;
  }
  
  if(earlyPairHolds > 0) {
    float earlyPairWinRate = (float)earlyPairHoldWins / earlyPairHolds;
  }
  
  if(lateRiskTaking > 0) {
    float lateRiskWinRate = (float)lateRiskWins / lateRiskTaking;
  }
}

// ============================================================================
// AI EXPECTED VALUE CALCULATIONS
// ============================================================================

float calculateExpectedValue(int dice[], int category, int rollsLeft, int* scores) {
  // If no rolls left, just return the current score
  if(rollsLeft == 0) {
    return calculateCategoryScore(dice, category);
  }
  
  // Calculate current score
  float currentScore = calculateCategoryScore(dice, category);
  
  // Count dice
  int counts[7] = {0};
  int total = 0;
  for(int i = 0; i < 5; i++) {
    counts[dice[i]]++;
    total += dice[i];
  }
  
  // For upper section (Ones through Sixes)
  if(category <= 5) {
    int targetValue = category + 1;
    int currentCount = counts[targetValue];
    int diceToReroll = 5 - currentCount;
    
    // **CORRECT PROBABILITY CALCULATION**
    // Probability of getting at least one more of target value per die
    float probPerDie = 1.0 / 6.0;
    
    // Expected number of additional dice across all rerolls
    // Formula: diceToReroll * probPerDie * rollsLeft
    float expectedNew = diceToReroll * probPerDie * rollsLeft;
    float expectedTotal = currentCount + expectedNew;
    
    return expectedTotal * targetValue;
  }
  
  // For 3-of-a-kind and 4-of-a-kind
  if(category == 6 || category == 7) {
    int maxCount = 0;
    int maxValue = 0;
    
    for(int i = 1; i <= 6; i++) {
      if(counts[i] > maxCount) {
        maxCount = counts[i];
        maxValue = i;
      }
    }
    
    int needed = (category == 6) ? 3 : 4;
    
    if(maxCount >= needed) {
      // Already have it
      // **CHECK FOR LOW VALUE PENALTY**
      if(maxValue <= 3 && maxCount >= 3) {
        int upperCategoryIdx = maxValue - 1;
        if(scores[upperCategoryIdx] == -1) {
          // Upper available - HUGE penalty
          return total * 0.15;
        }
      }
      return total;
    } else {
      // **USE PROBABILITY TABLES CORRECTLY**
      int diceToReroll = 5 - maxCount;
      if(diceToReroll > 5) diceToReroll = 5;
      
      // Get base probability from table
      float baseProbSuccess = PROB_OF_KIND[needed - 1][diceToReroll];
      
      // **CORRECT: Probability increases with more rolls**
      // P(success in n rolls) = 1 - (1 - p)^n
      float probSuccess = 1.0 - pow(1.0 - baseProbSuccess, rollsLeft);
      if(probSuccess > 0.95) probSuccess = 0.95;  // Cap at 95%
      
      // Expected value if successful
      float successValue = maxCount * maxValue + diceToReroll * 3.5;
      
      // Expected value if failed (keep what we have, roughly)
      float failValue = total * 0.4;
      
      float expectedValue = (probSuccess * successValue) + ((1.0 - probSuccess) * failValue);
      
      return expectedValue;
    }
  }
  
  // Full House
  if(category == 8) {
    if(currentScore == 25) {
      return 25;  // Already have it
    }
    
    int pairs = 0;
    int trips = 0;
    
    for(int i = 1; i <= 6; i++) {
      if(counts[i] == 3) trips++;
      if(counts[i] == 2) pairs++;
    }
    
    float baseProbFullHouse = 0.0;
    
    if(trips == 1 && pairs == 1) {
      baseProbFullHouse = 0.95;  // Already have it
    } else if(pairs == 2) {
      baseProbFullHouse = 0.30;  // Two pairs -> decent chance per roll
    } else if(trips == 1) {
      baseProbFullHouse = 0.20;  // One triple -> lower chance
    } else {
      baseProbFullHouse = 0.05;  // Starting from scratch
    }
    
    // **CORRECT: Multiple rolls compound**
    float probFullHouse = 1.0 - pow(1.0 - baseProbFullHouse, rollsLeft);
    if(probFullHouse > 0.98) probFullHouse = 0.98;
    
    return probFullHouse * 25;
  }
  
  // Small Straight
  if(category == 9) {
    if(currentScore == 30) {
      return 30;
    }
    
    // Count consecutive sequence
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
    
    // Remove duplicates
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
    
    int consecutive = 1;
    int maxConsecutive = 1;
    
    for(int i = 1; i < uniqueCount; i++) {
      if(unique[i] == unique[i-1] + 1) {
        consecutive++;
        if(consecutive > maxConsecutive) maxConsecutive = consecutive;
      } else {
        consecutive = 1;
      }
    }
    
    int missing = 4 - maxConsecutive;
    if(missing < 0) missing = 0;
    if(missing > 5) missing = 5;
    
    // **USE PROBABILITY TABLE**
    float baseProb = PROB_SMALL_STRAIGHT[missing];
    
    // **CORRECT: Multiple rolls compound**
    float prob = 1.0 - pow(1.0 - baseProb, rollsLeft);
    if(prob > 0.95) prob = 0.95;
    
    return prob * 30;
  }
  
  // Large Straight
  if(category == 10) {
    if(currentScore == 40) {
      return 40;
    }
    
    // Count consecutive (same as small straight logic)
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
    
    // Remove duplicates
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
    
    int consecutive = 1;
    int maxConsecutive = 1;
    
    for(int i = 1; i < uniqueCount; i++) {
      if(unique[i] == unique[i-1] + 1) {
        consecutive++;
        if(consecutive > maxConsecutive) maxConsecutive = consecutive;
      } else {
        consecutive = 1;
      }
    }
    
    int missing = 5 - maxConsecutive;
    if(missing < 0) missing = 0;
    if(missing > 5) missing = 5;
    
    // **USE PROBABILITY TABLE**
    float baseProb = PROB_LARGE_STRAIGHT[missing];
    
    // **CORRECT: Multiple rolls compound**
    float prob = 1.0 - pow(1.0 - baseProb, rollsLeft);
    if(prob > 0.95) prob = 0.95;
    
    return prob * 40;
  }
  
  // Yahtzee
  if(category == 11) {
    if(currentScore == 50) {
      return 50;
    }
    
    int maxCount = 0;
    for(int i = 1; i <= 6; i++) {
      if(counts[i] > maxCount) maxCount = counts[i];
    }
    
    int diceNeeded = 5 - maxCount;
    if(diceNeeded > 5) diceNeeded = 5;
    
    // **USE PROBABILITY TABLE**
    float baseProb = PROB_OF_KIND[4][diceNeeded];  // Index 4 = 5-of-kind
    
    // **CORRECT: Multiple rolls compound**
    float prob = 1.0 - pow(1.0 - baseProb, rollsLeft);
    if(prob > 0.90) prob = 0.90;  // Cap at 90%
    
    return prob * 50;
  }
  
  // Chance - sum of all dice
  if(category == 12) {
    // Hold high dice (5-6), reroll low dice
    int heldCount = 0;
    int heldTotal = 0;
    
    for(int i = 0; i < 5; i++) {
      if(dice[i] >= 5) {
        heldCount++;
        heldTotal += dice[i];
      }
    }
    
    int diceToReroll = 5 - heldCount;
    
    // Expected value from rerolling is 3.5 per die
    float expectedFromReroll = diceToReroll * 3.5;
    
    return heldTotal + expectedFromReroll;
  }
  
  return currentScore;
}

int findBestCategoryAI(int dice[], int scores[], bool useExpectedValue) {
  int bestCategory = -1;
  float bestScore = -1;
  for(int i = 0; i < 5; i++) {
  }
  
  // ═══════════════════════════════════════════════════════════════════════
  // STEP 1: COUNT DICE AND CALCULATE TOTALS
  // ═══════════════════════════════════════════════════════════════════════
  
  int counts[7] = {0};
  int total = 0;
  for(int i = 0; i < 5; i++) {
    counts[dice[i]]++;
    total += dice[i];
  }
  
  // Check for straights (used in multiple priority checks)
  bool hasSmallStraight = isStraight(dice, 4);
  bool hasLargeStraight = isLargeStraight(dice);
  
  // Find 4-of-kind and 5-of-kind values
  int fourOfKindValue = -1;
  int fiveOfKindValue = -1;
  for(int value = 6; value >= 1; value--) {
    if(counts[value] >= 5) {
      fiveOfKindValue = value;
    } else if(counts[value] >= 4) {
      fourOfKindValue = value;
    }
  }
  
  // === DETERMINE GAME PHASE ===
  int usedCategories = 0;
  for(int i = 0; i < 13; i++) {
    if(scores[i] != -1) usedCategories++;
  }
  
  bool earlyGame = (usedCategories <= 3);    // First 3 turns
  bool midGame = (usedCategories >= 4 && usedCategories <= 9);
  bool lateGame = (usedCategories >= 10);     // Last 3 turns
  


// ═══════════════════════════════════════════════════════════════
  // STEP 2: GUARANTEED EXCELLENT SCORES (25+ points) - TAKE IMMEDIATELY
  // ═══════════════════════════════════════════════════════════════

  // Yahtzee - 50 points (HIGHEST PRIORITY)
  if(scores[11] == -1 && fiveOfKindValue != -1) {
    return 11;
  }

  // Large Straight - 40 points
  if(scores[10] == -1 && hasLargeStraight) {
    return 10;
  }

  // Small Straight - 30 points
  // CRITICAL: Take small straight if large is ALREADY USED or if we're in late game
  if(scores[9] == -1 && hasSmallStraight) {
    if(scores[10] != -1) {
      // Large Straight already used - safe to take Small
      return 9;
    } else if(!hasLargeStraight && (lateGame || !earlyGame)) {
      // Don't have Large AND not early game - take the Small
      return 9;
    }
  }

// Full House - 25 points (CHECK BEFORE 3-of-kind logic!)
bool hasThree = false, hasTwo = false;
for(int value = 1; value <= 6; value++) {
  if(counts[value] == 3) hasThree = true;
  if(counts[value] == 2) hasTwo = true;
}

if(scores[8] == -1 && hasThree && hasTwo) {
  return 8;
}

// ═══════════════════════════════════════════════════════════════════════
// STEP 3: 4-OF-KIND LOGIC - SMART UPPER SECTION STRATEGY
// ═══════════════════════════════════════════════════════════════════════
  
  if(fourOfKindValue != -1) {
    
    int upperCategoryIdx = fourOfKindValue - 1;
    int upperScore = counts[fourOfKindValue] * fourOfKindValue;
    bool upperAvailable = (scores[upperCategoryIdx] == -1);
    bool fourOfKindAvailable = (scores[7] == -1);
    bool threeOfKindAvailable = (scores[6] == -1);
    
    // **CRITICAL**: Low-mid value (1-4): Prefer upper section in early game
    if(fourOfKindValue <= 4 && earlyGame) {
      if(upperAvailable) {
        return upperCategoryIdx;
      }
      
      if(threeOfKindAvailable) {
        return 6;
      }
      
      if(fourOfKindAvailable) {
        return 7;
      }
    }
    
    // **CRITICAL**: Very low value (1-3): ALWAYS avoid 4-of-kind even in mid/late game
    if(fourOfKindValue <= 3) {
      if(upperAvailable) {
        return upperCategoryIdx;
      }
      
      if(threeOfKindAvailable) {
        return 6;
      }
      
      if(fourOfKindAvailable && lateGame) {
        return 7;
      }
    }
    
    // Mid-High value (5-6): Decision depends on what's available
    else if(fourOfKindValue >= 5) {
      if(!fourOfKindAvailable && upperAvailable) {
        return upperCategoryIdx;
      }
      
      if(fourOfKindAvailable) {
        int upperTotal = getUpperSectionTotal(scores);
        bool needsBonus = (upperTotal < 63);
        int bonusRemaining = 63 - upperTotal;
        
        if(upperAvailable && needsBonus && bonusRemaining <= 20 && upperScore >= 18) {
          return upperCategoryIdx;
        }
        
        return 7;
      }
      
      if(threeOfKindAvailable && upperAvailable) {
        int diff = total - upperScore;
        
        if(diff >= 7) {
          return 6;
        } else {
          return upperCategoryIdx;
        }
      }
      
      if(upperAvailable) {
        return upperCategoryIdx;
      }
      
      if(threeOfKindAvailable) {
        return 6;
      }
    }
  }

  // ═══════════════════════════════════════════════════════════════════════
// STEP 3.5: 3-OF-KIND SMART ROUTING (NOT 4-of-kind, just 3)
// ═══════════════════════════════════════════════════════════════════════

int threeOfKindValue = -1;
for(int value = 6; value >= 1; value--) {
  if(counts[value] == 3) {
    threeOfKindValue = value;
    break;
  }
}

if(threeOfKindValue != -1) {
  
  int upperCategoryIdx = threeOfKindValue - 1;
  int upperScore = counts[threeOfKindValue] * threeOfKindValue;
  bool upperAvailable = (scores[upperCategoryIdx] == -1);
  bool threeOfKindAvailable = (scores[6] == -1);
  
  
  // **CRITICAL FIX**: Check for actual straights FIRST (not just potential)
  // If we HAVE a straight right now, straights take absolute priority
  if(hasLargeStraight && scores[10] == -1) {
    // Don't return - let Step 2 handle it (it should have already)
    // This is just a safety check
  } else if(hasSmallStraight && scores[9] == -1 && scores[10] != -1) {
    // Don't return - let Step 2 handle it
  }
  // **NEW**: Only check straight POTENTIAL if we don't have an actual straight
  else {
    // Sort to find consecutive sequences
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
    
    // Remove duplicates to get unique values
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
    
    // Find longest consecutive in unique values
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
    
    // **REDUCED THRESHOLD**: Only consider "straight potential" if we're very close (4+ consecutive)
    // AND straights are available AND it's not late game
    bool straightsAvailable = (scores[9] == -1 || scores[10] == -1);
    bool hasStrongStraightPotential = (maxConsecutive >= 4 && straightsAvailable && !lateGame);
    
    if(hasStrongStraightPotential) {
      // Don't return - let the full evaluation (STEP 6) compare options
    }
    
    // === ONLY FORCE UPPER SECTION if NO actual straight AND NO strong potential ===
    else {
      // === EXTREMELY LOW VALUES (1-2): ALWAYS use upper section ===
      if(threeOfKindValue <= 2) {
        if(upperAvailable) {
          return upperCategoryIdx;
        } else {
          if(threeOfKindAvailable && lateGame) {
            return 6;
          }
        }
      }
      
      // === LOW VALUE (3): ALWAYS use upper section unless late game ===
      else if(threeOfKindValue == 3) {
        if(upperAvailable && !lateGame) {
          return upperCategoryIdx;
        } else if(threeOfKindAvailable) {
          return 6;
        }
      }
      
      // === MID VALUE (4): Only force upper in early game ===
      else if(threeOfKindValue == 4 && earlyGame) {
        if(upperAvailable) {
          return upperCategoryIdx;
        }
      }
      
      // === HIGH VALUES (5-6): Let normal evaluation decide ===
      // Don't force anything - let STEP 6 compare all options properly
    }
  }
}
  
  // ═══════════════════════════════════════════════════════════════════════
  // STEP 4: END-GAME AWARENESS
  // ═══════════════════════════════════════════════════════════════════════
  
  int availableCategories = 0;
  int availableCats[13];
  for(int i = 0; i < 13; i++) {
    if(scores[i] == -1) {
      availableCats[availableCategories++] = i;
    }
  }
  
  
  if(availableCategories <= 2) {
    
    int bestEndgameCategory = -1;
    int bestEndgameScore = -1;
    
    for(int i = 0; i < availableCategories; i++) {
      int cat = availableCats[i];
      int score = calculateCategoryScore(dice, cat);
      
      
      if(cat == 11 && score == 0 && availableCategories > 1) {
        score = 10;
      } else if(cat == 10 && score == 0 && availableCategories > 1) {
        score = 8;
      }
      
      if(score > bestEndgameScore) {
        bestEndgameScore = score;
        bestEndgameCategory = cat;
      }
    }
    
    if(availableCategories == 1) {
      return availableCats[0];
    }
    
    if(availableCategories == 2) {
      if(bestEndgameCategory != -1) {
        int actualScore = calculateCategoryScore(dice, bestEndgameCategory);
        
        if(actualScore > 0) {
          return bestEndgameCategory;
        } else {
          int cat0 = availableCats[0];
          int cat1 = availableCats[1];
          
          int priority0 = (cat0 == 11 || cat0 == 10) ? 100 : 50;
          int priority1 = (cat1 == 11 || cat1 == 10) ? 100 : 50;
          
          if(priority0 > priority1) {
            return cat1;
          } else {
            return cat0;
          }
        }
      }
    }
  }
  
  // ═══════════════════════════════════════════════════════════════════════
  // STEP 5: UPPER SECTION BONUS STRATEGY – OPPORTUNITY-COST ANALYSIS
  //
  // Key principle: the VALUE of an upper-section SLOT is proportional to
  // how many dice of that face we might still roll.  Scoring 1×6 in the
  // Sixes box "wastes" 24 of the 30 maximum points – a huge opportunity
  // cost.  We should instead score in the slot with the LOWEST opportunity
  // cost (= the slot whose current roll is already close to its maximum).
  //
  // Secondary principle: the bonus (35 pts) is only worth pursuing when it
  // is realistically achievable given remaining categories.  If feasibility
  // drops below ~25%, treat the bonus as abandoned and stop penalising
  // lower-section categories for "getting in the way".
  // ═══════════════════════════════════════════════════════════════════════

  int upperTotal = getUpperSectionTotal(scores);
  bool needsBonus = (upperTotal < 63);
  int bonusRemaining = 63 - upperTotal;

  // Compute how realistic the bonus is (0 = impossible, 1 = trivial)
  float bonusFeasibility = calcBonusFeasibility(scores);
  // Only actively pursue bonus when feasibility is meaningful
  bool bonusWorthPursuing = (bonusFeasibility >= 0.25f);

  // Collect ALL available upper options where we have at least 1 matching die
  struct UpperOption {
    int value;        // Die face value (1-6)
    int categoryIdx;  // Score array index (0-5)
    int score;        // Points if we scored it now
    int count;        // Matching dice we have
    float oppCost;    // Opportunity cost: 1 - score/maxPossible  (0=perfect, 1=terrible)
  };

  UpperOption upperOptions[6];
  int upperOptionCount = 0;

  for(int value = 1; value <= 6; value++) {
    int categoryIdx = value - 1;
    if(scores[categoryIdx] == -1 && counts[value] >= 1) {
      upperOptions[upperOptionCount].value       = value;
      upperOptions[upperOptionCount].categoryIdx = categoryIdx;
      upperOptions[upperOptionCount].score       = counts[value] * value;
      upperOptions[upperOptionCount].count       = counts[value];
      upperOptions[upperOptionCount].oppCost     = upperOpportunityCost(value, counts[value]);
      upperOptionCount++;
    }
  }

  if(upperOptionCount >= 2 && needsBonus && bonusWorthPursuing) {

    // --- Find the option with LOWEST opportunity cost (best current score
    //     relative to its slot's potential) = the one we should USE now.
    int bestUseIdx = 0;
    for(int i = 1; i < upperOptionCount; i++) {
      if(upperOptions[i].oppCost < upperOptions[bestUseIdx].oppCost) {
        bestUseIdx = i;
      }
    }

    // --- Find the option with HIGHEST opportunity cost (worst score for its
    //     slot's potential) = the one we should SACRIFICE (score low in it
    //     to keep better slots available) IF we need to dump a category.
    int worstUseIdx = 0;
    for(int i = 1; i < upperOptionCount; i++) {
      if(upperOptions[i].oppCost > upperOptions[worstUseIdx].oppCost) {
        worstUseIdx = i;
      }
    }

    UpperOption& bestOpt  = upperOptions[bestUseIdx];
    UpperOption& worstOpt = upperOptions[worstUseIdx];

    // RULE 1: If our best option is truly excellent (≥15 pts, opp cost ≤ 0.5),
    //         take it immediately – don't over-think it.
    if(bestOpt.score >= 15 && bestOpt.oppCost <= 0.5f) {
      return bestOpt.categoryIdx;
    }

    // RULE 2: If our BEST current option has a high opportunity cost (≥ 0.65)
    //         it means even our strongest roll is weak relative to slot potential.
    //         Don't take ANY upper section now – let STEP 6 find a better play.
    if(bestOpt.oppCost >= 0.65f) {
      // Fall through to STEP 6
    }

    // RULE 3: Normal case – we have at least one decent upper option.
    //         Use the option with LOWEST opportunity cost (the one whose
    //         current roll is proportionally strongest).
    //         Only do this when the score is at least "halfway decent":
    //         rollQuality = score / (value * 3)  i.e. did we beat the average?
    else {
      float rollQuality = (bestOpt.value > 0)
                          ? (float)bestOpt.score / (float)(bestOpt.value * 3)
                          : 0.0f;

      if(rollQuality >= 0.66f) {
        // At least 2× average dice for this value – take the best option now
        return bestOpt.categoryIdx;
      }

      // RULE 4: If we have a very "cheap" sacrifice available (opp cost ≥ 0.80,
      //         meaning the roll is less than 20% of the slot's potential) AND
      //         there's another slot that could be much better, sacrifice the
      //         worst slot to clear it and keep the better one.
      if(worstOpt.oppCost >= 0.80f && worstOpt.score <= 3 &&
         bestUseIdx != worstUseIdx) {
        // Sacrifice the worst slot (score low there, free it up)
        return worstOpt.categoryIdx;
      }

      // RULE 5: Weakest slot is decent (4-9 pts, opp cost 0.4-0.75) –
      //         score it now to save higher-potential slots.
      if(worstOpt.score >= 4 && worstOpt.score < 10 &&
         worstOpt.oppCost <= 0.75f && bestUseIdx != worstUseIdx) {
        return worstOpt.categoryIdx;
      }
    }
  } else if(upperOptionCount == 1 && needsBonus && bonusWorthPursuing) {
    // Only one upper option – take it if quality is reasonable
    UpperOption& opt = upperOptions[0];
    float rollQuality = (opt.value > 0)
                        ? (float)opt.score / (float)(opt.value * 3)
                        : 0.0f;
    if(rollQuality >= 0.66f) {
      return opt.categoryIdx;
    }
    // Otherwise fall through to STEP 6
  }
  
  // ═══════════════════════════════════════════════════════════════════════
  // STEP 6: EVALUATE ALL REMAINING WITH EXPECTED VALUE
  // ═══════════════════════════════════════════════════════════════════════
  
  // **NEW**: Track the best special category (straights, full house, Yahtzee)
  // to ensure they're properly weighted
  int bestSpecialCategory = -1;
  float bestSpecialScore = 0;
  
  // Pre-scan for special categories to boost their priority
  if(scores[11] == -1 && calculateCategoryScore(dice, 11) > 0) {
    bestSpecialCategory = 11;
    bestSpecialScore = calculateCategoryScore(dice, 11) * (aiLearning.weightYahtzee / 10.0) * 3.0;
  }
  if(scores[10] == -1 && calculateCategoryScore(dice, 10) > 0) {
    float lgScore = calculateCategoryScore(dice, 10) * (aiLearning.weightLargeStraight / 10.0) * 3.0;
    if(lgScore > bestSpecialScore) {
      bestSpecialCategory = 10;
      bestSpecialScore = lgScore;
    }
  }
  if(scores[9] == -1 && calculateCategoryScore(dice, 9) > 0 && scores[10] != -1) {
    float smScore = calculateCategoryScore(dice, 9) * (aiLearning.weightSmallStraight / 10.0) * 2.5;
    if(smScore > bestSpecialScore) {
      bestSpecialCategory = 9;
      bestSpecialScore = smScore;
    }
  }
  if(scores[8] == -1 && calculateCategoryScore(dice, 8) > 0) {
    float fhScore = calculateCategoryScore(dice, 8) * (aiLearning.weightFullHouse / 10.0) * 2.0;
    if(fhScore > bestSpecialScore) {
      bestSpecialCategory = 8;
      bestSpecialScore = fhScore;
    }
  }
  
  if(bestSpecialCategory != -1) {
  } else {
  }
  
  int upperCategoriesOpen = 0;
  for(int j = 0; j <= 5; j++) {
    if(scores[j] == -1) upperCategoriesOpen++;
  }
  
  int maxPossibleFromRemaining = upperCategoriesOpen * 18;
  bool bonusStillPossible = (bonusRemaining <= maxPossibleFromRemaining);
  
 if(useExpectedValue) {
  
  for(int i = 0; i < 13; i++) {
    if(scores[i] == -1) {
      // Calculate actual score we'd get NOW
      int actualScore = calculateCategoryScore(dice, i);
      
      // Start with actual score as baseline
      float ev = actualScore;
      
      // For categories we don't have yet, consider future potential
      // This helps avoid "scoring 0" in valuable categories
      if(actualScore == 0) {
        // Estimate future value if we saved this category for later
        // Use 1 hypothetical roll to estimate potential
        float futureEV = calculateExpectedValue(dice, i, 1, scores);
        
        // Heavily penalize scoring 0 in valuable categories
        if(i == 11 || i == 10 || i == 9 || i == 8) {
          // Special categories - HUGE penalty for scoring 0
          ev = -30.0;  // Negative value to strongly discourage
        } else if(i >= 6 && i <= 7) {
          // 3/4 of kind - moderate penalty
          ev = -10.0;
        } else {
          // Upper section - small penalty
          ev = -5.0;
        }
      }
        
        float weightedScore = ev;
        
        // Chance - HEAVILY penalized if straights are available
        if(i == 12) {
          if(scores[9] == -1 || scores[10] == -1) {
            weightedScore = ev * 0.1;
          } else {
            weightedScore = ev;
          }
        }
        // Upper section
        else if(i <= 5) {
          int value = i + 1;
          
          // **CRITICAL**: If we have a high-scoring special category, severely penalize weak upper scores
          if(bestSpecialCategory != -1 && bestSpecialScore > 20 && actualScore < 9) {
            weightedScore = ev * 0.2;  // 80% penalty
          }
          else if(needsBonus && bonusStillPossible) {
            float rollQuality = 0;
            if(value > 0) {
              rollQuality = (float)actualScore / (value * 3);
            }

            // ── OPPORTUNITY COST GUARD ─────────────────────────────────────
            // Penalise scoring a high-value slot (5s or 6s) with very few
            // matching dice – it wastes the slot's potential.
            float oppCost = upperOpportunityCost(value, counts[value]);
            if(oppCost >= 0.80f && value >= 5 && upperCategoriesOpen > 2 && !earlyGame) {
              // 1 die in a high-value slot: 90% penalty – save this slot
              weightedScore = ev * 0.10f;
            } else if(oppCost >= 0.65f && value >= 4 && upperCategoriesOpen > 2 && !earlyGame) {
              // Below average for a high-value slot: 60% penalty
              weightedScore = ev * 0.40f;
            } else {
            // ── STANDARD QUALITY-BASED WEIGHTING ──────────────────────────
            // Calculate how urgently we need the bonus
            float bonusUrgency = 1.0;
            if(upperCategoriesOpen <= 2) {
              bonusUrgency = 2.0;  // Very urgent
            } else if(upperCategoriesOpen <= 4) {
              bonusUrgency = 1.5;  // Moderately urgent
            }
            
            if(rollQuality >= 1.0) {
              weightedScore = ev * (2.0 + bonusUrgency);
            } else if(rollQuality >= 0.66) {
              weightedScore = ev * (1.5 + bonusUrgency * 0.5);
            } else if(rollQuality >= 0.33) {
              if(upperCategoriesOpen <= 2 || value >= 5) {
                weightedScore = ev * 1.5;
              } else {
                weightedScore = ev * 0.7;
              }
            } else {
              if(upperCategoriesOpen == 1 && actualScore > 0) {
                weightedScore = ev * 1.0;
              } else {
                weightedScore = ev * 0.4;
              }
            }
            
            float bonusWeight = aiLearning.weightUpperBonus / 10.0;
            weightedScore = weightedScore * (bonusWeight / 10.0);  // Reduced impact
            } // end opportunity cost else
            
          } else if(!needsBonus) {
            if(actualScore >= value * 5) {
              weightedScore = ev * 1.8;
            } else if(actualScore >= value * 4 && value >= 5) {
              weightedScore = ev * 1.4;
            } else if(actualScore >= value * 3 && value >= 4) {
              weightedScore = ev * 1.1;
            } else {
              weightedScore = ev * 0.6;
            }
          } else {
            if(actualScore >= value * 4 && value >= 5) {
              weightedScore = ev * 1.2;
            } else {
              weightedScore = ev * 0.4;
            }
          }
        }
        // 3-of-a-kind and 4-of-a-kind
        else if(i == 6 || i == 7) {
          int maxCount = 0;
          int maxValue = 0;
          for(int v = 1; v <= 6; v++) {
            if(counts[v] > maxCount) {
              maxCount = counts[v];
              maxValue = v;
            }
          }
          
          if((maxValue <= 3) || (maxValue == 4 && earlyGame)) {
            int upperCategoryIdx = maxValue - 1;
            
            if(scores[upperCategoryIdx] == -1) {
              weightedScore = ev * 0.001;
            }
            else if(i == 7) {
              weightedScore = ev * 0.05;
            } else {
              weightedScore = ev * 0.1;
            }
          }
          else {
            if(i == 6 && maxCount >= 3) {
              if(maxValue >= 5) {
                weightedScore = ev * 2.0;
              } else if(maxValue >= 4) {
                weightedScore = ev * 1.5;
              } else {
                weightedScore = ev * 1.0;
              }
            } else if(i == 7 && maxCount >= 4) {
              if(maxValue >= 4) {
                weightedScore = ev * 2.5;
              } else {
                weightedScore = ev * 1.0;
              }
            } else {
              weightedScore = ev * 0.5;
            }
          }
        }
        // Special categories - BOOSTED scoring
        else if(i == 11 && actualScore > 0) {
          weightedScore = ev * (aiLearning.weightYahtzee / 10.0) * 3.0;  // 3x boost
        } else if(i == 10 && actualScore > 0) {
          weightedScore = ev * (aiLearning.weightLargeStraight / 10.0) * 3.0;  // 3x boost
        } else if(i == 9 && actualScore > 0) {
          // Only boost if Large is already used
          float boost = (scores[10] != -1) ? 2.5 : 1.5;
          weightedScore = ev * (aiLearning.weightSmallStraight / 10.0) * boost;
        } else if(i == 8 && actualScore > 0) {
          weightedScore = ev * (aiLearning.weightFullHouse / 10.0) * 2.0;  // 2x boost
        } else {
          weightedScore = ev;
        }

       if(availableCategories <= 3) {
          if(i == 11) {
            weightedScore *= 2.0;
          } else if(i == 10) {
            weightedScore *= 1.8;
          } else if(i == 8 || i == 9) {
            weightedScore *= 1.5;
          }
        }
        
        // **NEW: Apply learned category win rate adjustment**
        // Boost categories that correlate with wins, reduce those that don't
        if(vsComputer && currentPlayer == 2) {
          float categoryWinRate = getCategoryWinRate(i);
          
          // Apply win rate multiplier if we have enough data (5+ games)
          if(aiLearning.categoryScoredCount[i] >= 5) {
            if(categoryWinRate > 0.6) {
              weightedScore *= 1.15;  // 15% boost for high win-rate categories
            } else if(categoryWinRate < 0.4) {
              weightedScore *= 0.85;  // 15% penalty for low win-rate categories
            }
          }
          
          // **NEW: Apply turn-timing optimization**
          // Boost categories if we're near their optimal turn
          int currentTurn = 13 - (scoreViewSection == 0 ? 6 : 7);  // Approximate
          
          if(i == 11 && aiLearning.optimalTurnForYahtzee > 0) {
            int turnDiff = abs(currentTurn - aiLearning.optimalTurnForYahtzee);
            if(turnDiff <= 2) {
              weightedScore *= 1.2;  // 20% boost near optimal turn
            }
          } else if(i == 10 && aiLearning.optimalTurnForLargeStraight > 0) {
            int turnDiff = abs(currentTurn - aiLearning.optimalTurnForLargeStraight);
            if(turnDiff <= 2) {
              weightedScore *= 1.2;
            }
          }
        }
        
        if(weightedScore > bestScore) {
          bestScore = weightedScore;
          bestCategory = i;
        }
      }
    }
  } else {
    // Non-expected-value mode - use simpler findBestCategory
    return findBestCategory(dice, scores);
  }
  
  // === FINAL SAFETY CHECK ===
  if(bestCategory == -1) {
    bestCategory = findBestCategory(dice, scores);
  }
  
  // CRITICAL SAFETY CHECK - verify best category doesn't score 0
  if(bestCategory != -1) {
    int scoreForBest = calculateCategoryScore(dice, bestCategory);
    if(scoreForBest == 0) {
      
      int backupCategory = -1;
      int backupScore = 0;
      
      for(int i = 0; i < 13; i++) {
        if(scores[i] == -1) {
          int score = calculateCategoryScore(dice, i);
          if(score > backupScore) {
            backupScore = score;
            backupCategory = i;
          }
        }
      }
      
      if(backupCategory != -1 && backupScore > 0) {
        bestCategory = backupCategory;
      } else {
        for(int i = 5; i >= 0; i--) {
          if(scores[i] == -1) {
            bestCategory = i;
            break;
          }
        }
      }
    }
  }
  
  // === FINAL DEBUG OUTPUT ===
  if(bestCategory != -1) {
    int actualScore = calculateCategoryScore(dice, bestCategory);
    
    for(int i = 0; i < 5; i++) {
    }
    
    // Verify category is actually available
    if(scores[bestCategory] != -1) {
      
      // Emergency: find ANY available category
      for(int i = 0; i < 13; i++) {
        if(scores[i] == -1) {
          return i;
        }
      }
    }
  } else {
  }
  
  return bestCategory;
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

void runAITraining(int numGames) {
  
  aiTrainingMode = true;
  trainingGamesTotal = numGames;
  trainingGamesCompleted = 0;
  
  // Initialize 4 strategy variants with different approaches
  // Variant 0: Aggressive (high risk, high reward)
  strategyVariants[0].weightYahtzee = 30;
  strategyVariants[0].weightLargeStraight = 25;
  strategyVariants[0].weightSmallStraight = 18;
  strategyVariants[0].weightFullHouse = 16;
  strategyVariants[0].weight3OfKindLow = 25;
  strategyVariants[0].weight3OfKindHigh = 90;
  strategyVariants[0].weightStraightHold = 40;
  strategyVariants[0].weightUpperBonus = 70;
  strategyVariants[0].wins = 0;
  strategyVariants[0].losses = 0;
  strategyVariants[0].totalScore = 0;
  strategyVariants[0].gamesPlayed = 0;
  
  // Variant 1: Balanced (current AI weights)
  strategyVariants[1].weightYahtzee = aiLearning.weightYahtzee;
  strategyVariants[1].weightLargeStraight = aiLearning.weightLargeStraight;
  strategyVariants[1].weightSmallStraight = aiLearning.weightSmallStraight;
  strategyVariants[1].weightFullHouse = aiLearning.weightFullHouse;
  strategyVariants[1].weight3OfKindLow = aiLearning.weight3OfKindLow;
  strategyVariants[1].weight3OfKindHigh = aiLearning.weight3OfKindHigh;
  strategyVariants[1].weightStraightHold = aiLearning.weightStraightHold;
  strategyVariants[1].weightUpperBonus = aiLearning.weightUpperBonus; 
  strategyVariants[1].wins = 0;
  strategyVariants[1].losses = 0;
  strategyVariants[1].totalScore = 0;
  strategyVariants[1].gamesPlayed = 0;
  
  // Variant 2: Conservative (lower risk)
  strategyVariants[2].weightYahtzee = 15;
  strategyVariants[2].weightLargeStraight = 18;
  strategyVariants[2].weightSmallStraight = 20;
  strategyVariants[2].weightFullHouse = 18;
  strategyVariants[2].weight3OfKindLow = 35;
  strategyVariants[2].weight3OfKindHigh = 70;
  strategyVariants[2].weightStraightHold = 32;
  strategyVariants[2].weightUpperBonus = 80;
  strategyVariants[2].wins = 0;
  strategyVariants[2].losses = 0;
  strategyVariants[2].totalScore = 0;
  strategyVariants[2].gamesPlayed = 0;
  
  // Variant 3: Experimental (randomized variation)
  strategyVariants[3].weightYahtzee = aiLearning.weightYahtzee + random(-5, 6);
  strategyVariants[3].weightLargeStraight = aiLearning.weightLargeStraight + random(-5, 6);
  strategyVariants[3].weightSmallStraight = aiLearning.weightSmallStraight + random(-3, 4);
  strategyVariants[3].weightFullHouse = aiLearning.weightFullHouse + random(-3, 4);
  strategyVariants[3].weight3OfKindLow = aiLearning.weight3OfKindLow + random(-5, 6);
  strategyVariants[3].weight3OfKindHigh = aiLearning.weight3OfKindHigh + random(-10, 11);
  strategyVariants[3].weightStraightHold = aiLearning.weightStraightHold + random(-5, 6);
  strategyVariants[3].weightUpperBonus = aiLearning.weightUpperBonus + random(-10, 11);
  strategyVariants[3].wins = 0;
  strategyVariants[3].losses = 0;
  strategyVariants[3].totalScore = 0;
  strategyVariants[3].gamesPlayed = 0;
  
  
  unsigned long startTime = millis();
  
  // Initial full screen draw
  drawTrainingProgress();
  
  for(int i = 0; i < numGames; i++) {
    trainingGamesCompleted = i + 1;
    
    // Update display every 10 games instead of 5 to reduce flashing
    if(i % 10 == 0 || i == numGames - 1) {
      updateTrainingProgressPartial();  // Use partial update
    }
    
    // Play one self-play game
    playSelfPlayGame();
    
    // Give a tiny delay to prevent watchdog issues
    if(i % 10 == 0) {
      delay(1);
    }
  }
  
  unsigned long elapsed = millis() - startTime;
  
  // Final full update
  drawTrainingProgress();
  
  // **FINAL SAVE**
  saveAILearningToFile();
  
  aiTrainingMode = false;
  
  // Show completion message
  tft.fillRect(0, 200, 240, 60, COLOR_BG);
  tft.setTextSize(2);
  tft.setTextColor(COLOR_GREEN);
  tft.setCursor(20, 210);
  tft.print("COMPLETE!");
  
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(20, 240);
  tft.print("Time: ");
  tft.print(elapsed / 1000);
  tft.print(".");
  tft.print((elapsed % 1000) / 100);
  tft.print(" seconds");
  
  tft.setCursor(20, 255);
  tft.print("Games/sec: ");
  tft.print((numGames * 1000) / elapsed);
  
  buzzerBeep(100);
  delay(100);
  buzzerBeep(100);
  
  
  
  delay(2000);
}

void drawTrainingProgress() {
  tft.fillScreen(COLOR_BG);
  
  // Title
  tft.setTextSize(3);
  tft.setTextColor(COLOR_GREEN);
  tft.setCursor(20, 20);
  tft.print("TRAINING");
  
  // Progress bar
  int barWidth = 200;
  int barHeight = 30;
  int barX = 20;
  int barY = 80;
  
  float progress = (float)trainingGamesCompleted / trainingGamesTotal;
  int fillWidth = (int)(barWidth * progress);
  
  // Bar outline
  tft.drawRect(barX, barY, barWidth, barHeight, COLOR_TEXT);
  
  // Bar fill
  tft.fillRect(barX + 2, barY + 2, fillWidth - 4, barHeight - 4, COLOR_GREEN);
  
  // Progress text
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(40, 130);
  tft.print(trainingGamesCompleted);
  tft.print(" / ");
  tft.print(trainingGamesTotal);
  
  // Percentage
  tft.setCursor(70, 155);
  tft.print((int)(progress * 100));
  tft.print("%");
  
  // Stats preview
  tft.setTextSize(1);
  tft.setCursor(20, 200);
  tft.print("Total AI games: ");
  tft.print(aiLearning.totalSelfPlayGames);
  
  if(aiLearning.gamesPlayed > 0) {
    int winRate = ((aiLearning.aggressiveWins + aiLearning.conservativeWins) * 100) / aiLearning.gamesPlayed;
    tft.setCursor(20, 215);
    tft.print("Win rate: ");
    tft.print(winRate);
    tft.print("%");
  }
  
  if(aiLearning.pairHoldAttempts > 0) {
    int pairSuccessRate = (aiLearning.pairHoldSuccesses * 100) / aiLearning.pairHoldAttempts;
    tft.setCursor(20, 230);
    tft.print("Pair hold success: ");
    tft.print(pairSuccessRate);
    tft.print("%");
  }
}

void updateTrainingProgressPartial() {
  // Only update the progress bar and numbers - no full screen clear
  int barWidth = 200;
  int barHeight = 30;
  int barX = 20;
  int barY = 80;
  
  float progress = (float)trainingGamesCompleted / trainingGamesTotal;
  int fillWidth = (int)(barWidth * progress);
  
  // Clear and redraw just the progress bar fill area
  tft.fillRect(barX + 2, barY + 2, barWidth - 4, barHeight - 4, COLOR_BG);
  tft.fillRect(barX + 2, barY + 2, fillWidth - 4, barHeight - 4, COLOR_GREEN);
  
  // Clear and redraw the progress text
  tft.fillRect(30, 125, 180, 50, COLOR_BG);
  
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(40, 130);
  tft.print(trainingGamesCompleted);
  tft.print(" / ");
  tft.print(trainingGamesTotal);
  
  // Percentage
  tft.setCursor(70, 155);
  tft.print((int)(progress * 100));
  tft.print("%");
} 

void playSelfPlayGame() {
  // Play AI vs AI with DIFFERENT strategy variants
  // Player 1 uses one strategy, Player 2 uses another
  
  int variant1 = currentVariantIndex % 4;
  int variant2 = (currentVariantIndex + 2) % 4;  // Use different variant
  
  
  // Temporary game state
  int tempDice[5];
  bool tempHeld[5];
  int tempScores1[13];
  int tempScores2[13];
  int tempYahtzeeBonus1 = 0;
  int tempYahtzeeBonus2 = 0;
  int tempCurrentPlayer = 1;
  int tempRollsLeft = 3;
  int tempTurn = 1;
  
  // Initialize
  for(int i = 0; i < 13; i++) {
    tempScores1[i] = -1;
    tempScores2[i] = -1;
  }
  for(int i = 0; i < 5; i++) {
    tempDice[i] = 0;
    tempHeld[i] = false;
  }
  
  // Play 26 turns
  for(tempTurn = 1; tempTurn <= 26; tempTurn++) {
    tempCurrentPlayer = ((tempTurn - 1) % 2) + 1;
    int* tempScores = (tempCurrentPlayer == 1) ? tempScores1 : tempScores2;
    int variantIndex = (tempCurrentPlayer == 1) ? variant1 : variant2;
    
    // Clear held state
    for(int i = 0; i < 5; i++) {
      tempHeld[i] = false;
    }
    
    tempRollsLeft = 3;
    int rollsUsedInTurn = 0;  // Track how many rolls we actually use
    
    // Roll up to 3 times
    while(tempRollsLeft > 0) {
      tempRollsLeft--;
      rollsUsedInTurn++;  // Increment each time we roll
      
      // **v42.8: Track score before roll for reroll quality**
      int scoreBefore = getBestAvailableCategoryScore(tempDice, tempScores);
      
      // Roll non-held dice
      for(int i = 0; i < 5; i++) {
        if(!tempHeld[i]) {
          tempDice[i] = random(1, 7);
        }
      }
      
      // **v42.8: Track score after roll and record reroll decision**
      int scoreAfter = getBestAvailableCategoryScore(tempDice, tempScores);
      recordRerollDecision(rollsUsedInTurn, scoreBefore, scoreAfter);
      
      // Decide what to hold using variant's strategy
      if(tempRollsLeft > 0) {
        // Count dice before hold decision
        int counts[7] = {0};
        for(int i = 0; i < 5; i++) {
          counts[tempDice[i]]++;
        }
        
        // **NEW: Check if we should stop rolling (realistic AI behavior)**
        // Calculate best possible score with current dice
        int bestScore = 0;
        for(int cat = 0; cat < 13; cat++) {
          if(tempScores[cat] == -1) {
            int catScore = calculateCategoryScore(tempDice, cat);
            if(catScore > bestScore) bestScore = catScore;
          }
        }
        
        // Stopping thresholds based on rolls remaining
        bool shouldStop = false;
        if(rollsUsedInTurn == 1) {
          // After first roll, only stop for exceptional scores
          shouldStop = (bestScore >= 40);
        } else if(rollsUsedInTurn == 2) {
          // After second roll, stop for good scores
          shouldStop = (bestScore >= 30);
        }
        
        // Always stop for Yahtzee or Large Straight
        if(isYahtzee(tempDice) || isLargeStraight(tempDice)) {
          shouldStop = true;
        }
        
        if(shouldStop) {
          break;  // Exit the roll loop early
        }
        
        // Track attempts for learning
        int maxCount = 0;
        for(int v = 1; v <= 6; v++) {
          if(counts[v] > maxCount) maxCount = counts[v];
        }
        
        // Track Yahtzee attempts (3+ of kind with rolls left)
        if(maxCount >= 3 && tempScores[11] == -1 && tempRollsLeft >= 1) {
          aiLearning.yahtzeeAttempts++;
        }
        
        // Track straight attempts (3+ consecutive)
        int sorted[5];
        for(int i = 0; i < 5; i++) sorted[i] = tempDice[i];
        for(int i = 0; i < 4; i++) {
          for(int j = 0; j < 4-i; j++) {
            if(sorted[j] > sorted[j+1]) {
              int temp = sorted[j];
              sorted[j] = sorted[j+1];
              sorted[j+1] = temp;
            }
          }
        }
        
        int consecutive = 1;
        int maxConsecutive = 1;
        for(int i = 1; i < 5; i++) {
          if(sorted[i] == sorted[i-1] + 1) {
            consecutive++;
            if(consecutive > maxConsecutive) maxConsecutive = consecutive;
          } else if(sorted[i] != sorted[i-1]) {
            consecutive = 1;
          }
        }
        
        if(maxConsecutive >= 3 && (tempScores[9] == -1 || tempScores[10] == -1) && tempRollsLeft >= 1) {
          aiLearning.straightAttempts++;
        }
        
        holdDecisionWithVariant(tempDice, tempHeld, tempScores, variantIndex, tempRollsLeft);
      }
    }
    
    // Select category using variant's strategy
    int selectedCat = selectCategoryWithVariant(tempDice, tempScores, variantIndex);
    if(selectedCat == -1) {
      for(int i = 0; i < 13; i++) {
        if(tempScores[i] == -1) {
          selectedCat = i;
          break;
        }
      }
    }
    
    // Score it
    int score = calculateCategoryScore(tempDice, selectedCat);
    tempScores[selectedCat] = score;
    
    // **ENHANCED: Track metrics for self-play (for BOTH players)**
    int rollUsed = rollsUsedInTurn;  // Use the actual counter we tracked
    if(rollUsed >= 1 && rollUsed <= 3) {
      if(rollUsed == 1) aiLearning.firstRollScores[selectedCat] += score;
      else if(rollUsed == 2) aiLearning.secondRollScores[selectedCat] += score;
      else if(rollUsed == 3) aiLearning.thirdRollScores[selectedCat] += score;
      
      aiLearning.rollCountUsed[rollUsed]++;
    }
    
    // Track hold pattern used
    int heldDice = 0;
    for(int h = 0; h < 5; h++) {
      if(tempHeld[h]) heldDice++;
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
  }
  
  // Calculate final scores
  int total1 = calculateTotalWithBonuses(tempScores1, tempYahtzeeBonus1);
  int total2 = calculateTotalWithBonuses(tempScores2, tempYahtzeeBonus2);
  
  // Track achievements for both players
  bool p1GotYahtzee = (tempScores1[11] == 50);
  bool p1GotLargeStraight = (tempScores1[10] == 40);
  bool p1GotSmallStraight = (tempScores1[9] == 30);
  bool p1GotFullHouse = (tempScores1[8] == 25);
  
  bool p2GotYahtzee = (tempScores2[11] == 50);
  bool p2GotLargeStraight = (tempScores2[10] == 40);
  bool p2GotSmallStraight = (tempScores2[9] == 30);
  bool p2GotFullHouse = (tempScores2[8] == 25);
  
  // Track achievements (count both players)
  if(p1GotYahtzee) aiLearning.gamesWithYahtzee++;
  if(p2GotYahtzee) aiLearning.gamesWithYahtzee++;
  if(p1GotLargeStraight) aiLearning.gamesWithLargeStraight++;
  if(p2GotLargeStraight) aiLearning.gamesWithLargeStraight++;
  if(p1GotSmallStraight) aiLearning.gamesWithSmallStraight++;
  if(p2GotSmallStraight) aiLearning.gamesWithSmallStraight++;
  if(p1GotFullHouse) aiLearning.gamesWithFullHouse++;
  if(p2GotFullHouse) aiLearning.gamesWithFullHouse++;
  
  // Track SUCCESSES for Yahtzee and Straights
  if(p1GotYahtzee || p2GotYahtzee) {
    aiLearning.yahtzeeSuccesses++;
  }
  if(p1GotLargeStraight || p2GotLargeStraight || p1GotSmallStraight || p2GotSmallStraight) {
    aiLearning.straightSuccesses++;
  }


  
  // Track upper section bonus
  int upper1 = getUpperSectionTotal(tempScores1);
  int upper2 = getUpperSectionTotal(tempScores2);
  if(upper1 >= 63) aiLearning.bonusAchieved++;
  if(upper2 >= 63) aiLearning.bonusAchieved++;

  // Calculate who won (declare these variables BEFORE using them)
  bool p1Won = (total1 > total2);
  bool p2Won = (total2 > total1);
  
  // **ENHANCED: Track bonus pursuit analytics**
  if(upper1 >= 63 && upper1 <= 69) {
    // Normal bonus for P1
  } else if(upper1 >= 75) {
    aiLearning.bonusOverkill++;
  } else if(upper1 >= 60 && upper1 <= 62) {
    aiLearning.bonusNearMiss++;
  } else if(upper1 < 50) {
    aiLearning.bonusPursuitAbandoned++;
    if(p1Won) aiLearning.bonusAbandonWins++;
  }
  
  if(upper2 >= 63 && upper2 <= 69) {
    // Normal bonus for P2
  } else if(upper2 >= 75) {
    aiLearning.bonusOverkill++;
  } else if(upper2 >= 60 && upper2 <= 62) {
    aiLearning.bonusNearMiss++;
  } else if(upper2 < 50) {
    aiLearning.bonusPursuitAbandoned++;
    if(p2Won) aiLearning.bonusAbandonWins++;
  }
  
  // **ENHANCED: Track endgame scenarios**
  int scoreDiff = abs(total1 - total2);
  if(scoreDiff <= 20) {
    // Close game
    if(p1Won) aiLearning.endgameCloseWins++;
    if(p2Won) aiLearning.endgameCloseWins++;
  } else if((p1Won && total1 > total2 + 50) || (p2Won && total2 > total1 + 50)) {
    aiLearning.endgameBlowoutWins++;
  }

  // Learn upper bonus weight based on success
  // More aggressive weight adjustment
  if(p1Won && upper1 >= 63) {
    aiLearning.weightUpperBonus = min(100, aiLearning.weightUpperBonus + 3);  // Bigger increase
  } else if(p1Won && upper1 < 50) {
    aiLearning.weightUpperBonus = max(40, aiLearning.weightUpperBonus - 1);   // Smaller decrease
  } else if(!p1Won && upper1 >= 63) {
    aiLearning.weightUpperBonus = min(100, aiLearning.weightUpperBonus + 1);  // Still reward bonus even if lost
  }
  
  if(p2Won && upper2 >= 63) {
    aiLearning.weightUpperBonus = min(100, aiLearning.weightUpperBonus + 3);  // Bigger increase
  } else if(p2Won && upper2 < 50) {
    aiLearning.weightUpperBonus = max(40, aiLearning.weightUpperBonus - 1);   // Smaller decrease
  } else if(!p2Won && upper2 >= 63) {
    aiLearning.weightUpperBonus = min(100, aiLearning.weightUpperBonus + 1);  // Still reward bonus even if lost
  }

  // Log bonus achievement for debugging
  if(upper1 >= 63) {
  } else {
  }
  
  if(upper2 >= 63) {
  } else {
  }
  
  // Update variant performance
  strategyVariants[variant1].gamesPlayed++;
  strategyVariants[variant2].gamesPlayed++;
  
  strategyVariants[variant1].totalScore += total1;
  strategyVariants[variant2].totalScore += total2;
  
  // p1Won and p2Won already declared above - don't redeclare
  
  if(p1Won) {
    strategyVariants[variant1].wins++;
    strategyVariants[variant2].losses++;
    
    // Track wins with achievements (player 1)
    if(p1GotYahtzee) aiLearning.winsWithYahtzee++;
    if(p1GotLargeStraight) aiLearning.winsWithLargeStraight++;
    if(p1GotSmallStraight) aiLearning.winsWithSmallStraight++;
    if(p1GotFullHouse) aiLearning.winsWithFullHouse++;
    
  } else if(p2Won) {
    strategyVariants[variant2].wins++;
    strategyVariants[variant1].losses++;
    
    // Track wins with achievements (player 2)
    if(p2GotYahtzee) aiLearning.winsWithYahtzee++;
    if(p2GotLargeStraight) aiLearning.winsWithLargeStraight++;
    if(p2GotSmallStraight) aiLearning.winsWithSmallStraight++;
    if(p2GotFullHouse) aiLearning.winsWithFullHouse++;
    
  } else {
  }
  
  aiLearning.totalSelfPlayGames++;
  currentVariantIndex++;
  
  // **SAVE AFTER EVERY 5 GAMES (more frequent checkpoints)**
  if(aiLearning.totalSelfPlayGames % 5 == 0) {
    saveAILearningToFile();
  }
  
  // === UPDATE LEARNED WEIGHTS BASED ON SELF-PLAY RESULTS ===
  // Run weight adjustment every 10 self-play games
  if(aiLearning.totalSelfPlayGames % 10 == 0 && aiLearning.totalSelfPlayGames >= 20) {
    
    // Calculate win rates for each achievement
    float yahtzeeWinRate = (aiLearning.gamesWithYahtzee > 0) ? 
      (float)aiLearning.winsWithYahtzee / aiLearning.gamesWithYahtzee : 0.5;
    float lgStraightWinRate = (aiLearning.gamesWithLargeStraight > 0) ?
      (float)aiLearning.winsWithLargeStraight / aiLearning.gamesWithLargeStraight : 0.5;
    float smStraightWinRate = (aiLearning.gamesWithSmallStraight > 0) ?
      (float)aiLearning.winsWithSmallStraight / aiLearning.gamesWithSmallStraight : 0.5;
    float fullHouseWinRate = (aiLearning.gamesWithFullHouse > 0) ?
      (float)aiLearning.winsWithFullHouse / aiLearning.gamesWithFullHouse : 0.5;
    
    // Adjust weights based on win correlation (±2 per adjustment)
    if(yahtzeeWinRate > 0.55) {
      aiLearning.weightYahtzee = min(50, aiLearning.weightYahtzee + 2);
    } else if(yahtzeeWinRate < 0.45) {
      aiLearning.weightYahtzee = max(10, aiLearning.weightYahtzee - 2);
    }
    
    if(lgStraightWinRate > 0.55) {
      aiLearning.weightLargeStraight = min(50, aiLearning.weightLargeStraight + 2);
    } else if(lgStraightWinRate < 0.45) {
      aiLearning.weightLargeStraight = max(10, aiLearning.weightLargeStraight - 2);
    }
    
    if(smStraightWinRate > 0.55) {
      aiLearning.weightSmallStraight = min(50, aiLearning.weightSmallStraight + 2);
    } else if(smStraightWinRate < 0.45) {
      aiLearning.weightSmallStraight = max(8, aiLearning.weightSmallStraight - 2);
    }
    
    if(fullHouseWinRate > 0.55) {
      aiLearning.weightFullHouse = min(50, aiLearning.weightFullHouse + 2);
    } else if(fullHouseWinRate < 0.45) {
      aiLearning.weightFullHouse = max(8, aiLearning.weightFullHouse - 2);
    }
    
    
    
    
      
      // Save updated weights
      saveAILearningToFile();
    }
  }

StraightAnalysis analyzeStraightPotential(int dice[], int* scores, StrategyVariant& variant, int rollsLeft) {
  StraightAnalysis result;
  result.shouldPursueStraight = false;
  result.consecutiveLength = 0;
  result.startValue = 0;
  result.missingCount = 0;
  result.expectedValue = 0;
  for(int i = 0; i < 5; i++) result.holds[i] = false;
  
  // Check if straights are even available
  bool largeStraightAvailable = (scores[10] == -1);
  bool smallStraightAvailable = (scores[9] == -1);
  
  if(!largeStraightAvailable && !smallStraightAvailable) {
    return result;  // No straights available
  }
  
  // **STEP 1: Sort dice**
  int sorted[5];
  for(int i = 0; i < 5; i++) sorted[i] = dice[i];
  
  // Bubble sort
  for(int i = 0; i < 4; i++) {
    for(int j = 0; j < 4-i; j++) {
      if(sorted[j] > sorted[j+1]) {
        int temp = sorted[j];
        sorted[j] = sorted[j+1];
        sorted[j+1] = temp;
      }
    }
  }
  
  // **STEP 2: CRITICAL FIX - Remove duplicates to get UNIQUE values**
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
  
  // **STEP 3: Find longest consecutive sequence in UNIQUE values**
  int consecutive = 1;
  int maxConsecutive = 1;
  int seqStart = unique[0];
  int bestSeqStart = unique[0];
  
  for(int i = 1; i < uniqueCount; i++) {
    if(unique[i] == unique[i-1] + 1) {
      if(consecutive == 1) seqStart = unique[i-1];
      consecutive++;
      if(consecutive > maxConsecutive) {
        maxConsecutive = consecutive;
        bestSeqStart = seqStart;
      }
    } else {
      consecutive = 1;
    }
  }
  
  result.consecutiveLength = maxConsecutive;
  result.startValue = bestSeqStart;
  
  // **STEP 4: EVALUATE STRAIGHT POTENTIAL**
  
  // Already have large straight (5 unique consecutive)
  if(maxConsecutive >= 5 && largeStraightAvailable) {
    result.shouldPursueStraight = true;
    result.expectedValue = 40.0;
    
    // Hold ALL 5 consecutive dice (one of each value)
    for(int i = 0; i < 5; i++) {
      result.holds[i] = false;  // Start with none held
    }
    
    for(int val = bestSeqStart; val < bestSeqStart + 5; val++) {
      // Find FIRST die with this value
      for(int i = 0; i < 5; i++) {
        if(dice[i] == val && !result.holds[i]) {
          result.holds[i] = true;
          break;  // Only hold ONE die of this value
        }
      }
    }
    
    return result;
  }
  
  // Have small straight (4 unique consecutive)
  if(maxConsecutive >= 4 && smallStraightAvailable) {
    // **PRIORITY: If large straight is available and we have rolls left, pursue large**
    if(largeStraightAvailable && rollsLeft > 0) {
      result.shouldPursueStraight = true;
      result.expectedValue = 20.0;  // Expected value of pursuing large from 4 consecutive
      
      // Hold the 4 consecutive dice (one of each value)
      for(int i = 0; i < 5; i++) {
        result.holds[i] = false;
      }
      
      for(int val = bestSeqStart; val < bestSeqStart + 4; val++) {
        // Find FIRST die with this value
        for(int i = 0; i < 5; i++) {
          if(dice[i] == val && !result.holds[i]) {
            result.holds[i] = true;
            break;
          }
        }
      }
      
      return result;
    }
    // Large already used, just take the small straight
    else if(!largeStraightAvailable) {
      result.shouldPursueStraight = true;
      result.expectedValue = 30.0;
      
      // Hold all dice (we have the small straight)
      for(int i = 0; i < 5; i++) {
        result.holds[i] = true;
      }
      
      return result;
    }
  }
  
  // Have 3 consecutive - evaluate if worth pursuing
  if(maxConsecutive == 3 && rollsLeft > 0) {
    int missingForLarge = 5 - maxConsecutive;  // 2 missing
    int missingForSmall = 4 - maxConsecutive;  // 1 missing
    
    // Check what's needed - can we extend in either direction?
    bool canExtendLow = (bestSeqStart > 1);
    bool canExtendHigh = (bestSeqStart + maxConsecutive <= 6);
    
    // Probability estimates
    float probSmall = 0.0;
    float probLarge = 0.0;
    
    if(rollsLeft >= 2) {
      // 2+ rolls - good chance
      probSmall = (canExtendLow || canExtendHigh) ? 0.5 : 0.3;
      probLarge = (canExtendLow && canExtendHigh) ? 0.25 : 0.15;
    } else {
      // 1 roll left - lower chance
      probSmall = (canExtendLow || canExtendHigh) ? 0.3 : 0.15;
      probLarge = (canExtendLow && canExtendHigh) ? 0.12 : 0.06;
    }
    
    // Expected value calculation
    float evSmall = probSmall * 30.0;
    float evLarge = probLarge * 40.0;
    float straightEV = evSmall + evLarge;
    
    // Apply variant weight
    straightEV *= (variant.weightStraightHold / 10.0);
    
    // Only pursue if EV is decent (> 15)
    if(straightEV > 15.0) {
      result.shouldPursueStraight = true;
      result.expectedValue = straightEV;
      
      // **CRITICAL**: Hold the 3 consecutive dice (ONE of each value)
      for(int i = 0; i < 5; i++) {
        result.holds[i] = false;
      }
      
      for(int val = bestSeqStart; val < bestSeqStart + 3; val++) {
        // Find FIRST die with this value
        for(int i = 0; i < 5; i++) {
          if(dice[i] == val && !result.holds[i]) {
            result.holds[i] = true;
            break;
          }
        }
      }
      
      return result;
    }
  }
  
  return result;
}

EndgameStrategy analyzeEndgameStrategy(int* scores, int rollsLeft) {
  EndgameStrategy result;
  result.useAggressiveStrategy = false;
  result.targetCategory = -1;
  result.targetPriority = 0;
  result.description = "Normal play";
  
  int availableCategories = 0;
  int availableCats[13];
  for(int i = 0; i < 13; i++) {
    if(scores[i] == -1) {
      availableCats[availableCategories++] = i;
    }
  }
  
  if(availableCategories > 3) {
    return result;
  }
  
  
  bool yahtzeeAvailable = (scores[11] == -1);
  bool largeStraightAvailable = (scores[10] == -1);
  bool smallStraightAvailable = (scores[9] == -1);
  bool fullHouseAvailable = (scores[8] == -1);
  bool fourOfKindAvailable = (scores[7] == -1);
  
  if(yahtzeeAvailable && rollsLeft > 0) {
    result.useAggressiveStrategy = true;
    result.targetCategory = 11;
    result.targetPriority = 100.0;
    result.description = "PURSUE YAHTZEE";
    return result;
  }
  
  if(largeStraightAvailable && rollsLeft > 0) {
    result.useAggressiveStrategy = true;
    result.targetCategory = 10;
    result.targetPriority = 80.0;
    result.description = "PURSUE LARGE STRAIGHT";
    return result;
  }
  
  if(smallStraightAvailable && !largeStraightAvailable && rollsLeft > 0) {
    result.useAggressiveStrategy = true;
    result.targetCategory = 9;
    result.targetPriority = 60.0;
    result.description = "PURSUE SMALL STRAIGHT";
    return result;
  }
  
  if(fullHouseAvailable && rollsLeft > 0) {
    result.useAggressiveStrategy = true;
    result.targetCategory = 8;
    result.targetPriority = 50.0;
    result.description = "PURSUE FULL HOUSE";
    return result;
  }
  
  if(fourOfKindAvailable && rollsLeft > 0) {
    result.useAggressiveStrategy = false;
    result.targetCategory = 7;
    result.targetPriority = 40.0;
    result.description = "Try for 4-of-Kind";
    return result;
  }
  
  return result;
}

// Hold decision using specific strategy variant
void holdDecisionWithVariant(int dice[], bool held[], int* scores, int variantIndex, int rollsLeft) {
  
 // === STEP 1: CHECK ENDGAME STRATEGY FIRST ===
  EndgameStrategy endgame = analyzeEndgameStrategy(scores, rollsLeft);
  
  if(endgame.useAggressiveStrategy) {
    
    // YAHTZEE PURSUIT - hold most common value
    if(endgame.targetCategory == 11) {
      int counts[7] = {0};
      for(int i = 0; i < 5; i++) {
        counts[dice[i]]++;
      }
      
      int maxCount = 0;
      int maxValue = 0;
      for(int v = 6; v >= 1; v--) {
        if(counts[v] > maxCount) {
          maxCount = counts[v];
          maxValue = v;
        }
      }
      
      // Hold ALL dice of most common value
      for(int i = 0; i < 5; i++) {
        held[i] = (dice[i] == maxValue);
      }
      
      
      updateDiceDisplay();
      return;  // EXIT - don't run any other logic
    }
    
    // STRAIGHT PURSUIT - hold consecutive sequence ONLY (NO DUPLICATES)
    else if(endgame.targetCategory == 10 || endgame.targetCategory == 9) {
      
      // Sort dice to find consecutive
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
      
      // Remove duplicates to get unique values
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
      
      // Find longest consecutive sequence
      int consecutive = 1;
      int maxConsecutive = 1;
      int seqStart = unique[0];
      int bestSeqStart = unique[0];
      
      for(int i = 1; i < uniqueCount; i++) {
        if(unique[i] == unique[i-1] + 1) {
          if(consecutive == 1) seqStart = unique[i-1];
          consecutive++;
          if(consecutive > maxConsecutive) {
            maxConsecutive = consecutive;
            bestSeqStart = seqStart;
          }
        } else {
          consecutive = 1;
        }
      }
      
      
      // Determine how many to hold
      int targetLength = (endgame.targetCategory == 10) ? 4 : 3;
      int holdLength = min(maxConsecutive, targetLength);
      
      // **CRITICAL: Clear all holds first**
      for(int i = 0; i < 5; i++) {
        held[i] = false;
      }
      
      // Hold ONE die per consecutive value (NO DUPLICATES)
      bool valueHeld[7] = {false};
      int heldCount = 0;
      
      for(int val = bestSeqStart; val < bestSeqStart + holdLength && heldCount < holdLength; val++) {
        if(!valueHeld[val]) {
          // Find FIRST die with this value
          for(int i = 0; i < 5; i++) {
            if(dice[i] == val && !held[i]) {
              held[i] = true;
              valueHeld[val] = true;
              heldCount++;
              break;
            }
          }
        }
      }
      
      for(int i = 0; i < 5; i++) {
      }
      
      updateDiceDisplay();
      return;  // EXIT - don't run any other logic
    }
    
    // FULL HOUSE PURSUIT - hold pairs/trips
    else if(endgame.targetCategory == 8) {
      int counts[7] = {0};
      for(int i = 0; i < 5; i++) {
        counts[dice[i]]++;
      }
      
      int threeValue = -1;
      int twoValue = -1;
      
      for(int v = 6; v >= 1; v--) {
        if(counts[v] == 3) threeValue = v;
        if(counts[v] == 2 && twoValue == -1) twoValue = v;
      }
      
      // Hold trips and pairs
      for(int i = 0; i < 5; i++) {
        held[i] = (dice[i] == threeValue || dice[i] == twoValue);
      }
      
      
      updateDiceDisplay();
      return;  // EXIT - don't run any other logic
    }
  }
  
  // === NORMAL STRATEGY (not endgame or no aggressive target) ===
  int counts[7] = {0};
  for(int i = 0; i < 5; i++) {
    counts[dice[i]]++;
  }
  
  // Find best multiple
  int maxCount = 0;
  int maxValue = 0;
  for(int v = 6; v >= 1; v--) {
    if(counts[v] > maxCount) {
      maxCount = counts[v];
      maxValue = v;
    }
  }
  
  // Get variant weights
  StrategyVariant& variant = strategyVariants[variantIndex];
  
  // Check upper section bonus status
  int upperTotal = getUpperSectionTotal(scores);
  bool needsBonus = (upperTotal < 63);
  int bonusRemaining = 63 - upperTotal;
  
  // Count upper categories remaining
  int upperCategoriesOpen = 0;
  for(int j = 0; j <= 5; j++) {
    if(scores[j] == -1) upperCategoriesOpen++;
  }
  
  // Check if bonus is still achievable AND realistic
  int maxPossibleUpper = upperCategoriesOpen * 18;  // 3×6 = 18 per category
  bool bonusStillPossible = (bonusRemaining <= maxPossibleUpper);
  float bonusFeasHold = calcBonusFeasibility(scores);
  bool bonusWorthHolding = bonusStillPossible && (bonusFeasHold >= 0.25f);

  // PRIORITY 1: Upper section bonus pursuit – opportunity-cost aware
  //
  // Old logic: only committed to upper section if we had 2+ dice of a value.
  // New logic: even a SINGLE die of a HIGH-value face (5 or 6) is worth
  // protecting, because scoring just 1×6 in the Sixes box wastes 80% of that
  // slot's potential.  We use opportunity cost to rank which value to hold.
  //
  // We still require the roll to be "competitive" vs just holding our best
  // multiple – we don't throw away four 5s to chase a 1×6 upper bonus.
  if(needsBonus && bonusWorthHolding && upperCategoriesOpen <= 4) {

    // Find the best value to hold using opportunity cost × count × value
    int bestUpperValue = -1;
    float bestUpperEV = -1.0f;

    for(int value = 6; value >= 1; value--) {
      int categoryIdx = value - 1;
      if(scores[categoryIdx] == -1 && counts[value] >= 1) {
        float oppCost = upperOpportunityCost(value, counts[value]);
        // EV of holding these dice = current score + bonus fraction,
        // weighted by how valuable protecting the slot is (oppCostMultiplier)
        float currentScore = (float)(counts[value] * value);
        float bonusFrac = currentScore / (float)max(1, bonusRemaining);
        float bonusContrib = min(1.0f, bonusFrac) * 35.0f;
        float oppCostMult = 1.0f + oppCost;  // high opp cost = more valuable to protect
        float ev = (currentScore + bonusContrib) * oppCostMult * bonusFeasHold;

        if(ev > bestUpperEV) {
          bestUpperEV = ev;
          bestUpperValue = value;
        }
      }
    }

    if(bestUpperValue != -1) {
      int bestUpperCount = counts[bestUpperValue];
      float upperScore   = (float)(bestUpperCount * bestUpperValue);
      float multipleScore = (float)(maxCount * maxValue);

      // Only commit if upper option is competitive vs best multiple:
      // - 2+ dice: commit if score >= 65% of best multiple
      // - 1 die of value 5-6: commit if the slot's opp cost is high (slot is precious)
      float oppCostBest = upperOpportunityCost(bestUpperValue, bestUpperCount);
      bool hasHighOppCost = (oppCostBest >= 0.65f && bestUpperValue >= 5);
      bool isCompetitive  = (upperScore >= multipleScore * 0.65f) || (bestUpperCount >= maxCount);

      if(isCompetitive || (hasHighOppCost && bestUpperCount >= 1 && rollsLeft > 0)) {
        for(int i = 0; i < 5; i++) {
          held[i] = (dice[i] == bestUpperValue);
        }
        return;  // Exit early – committed to upper section
      }
    }
  }
  
  // **NEW: Track high-risk attempts**
  // High risk = going for Yahtzee with only 2-3 of kind, or straights with only 2-3 consecutive
  bool isHighRisk = false;
  
  // Check for Yahtzee pursuit with low probability
  if(scores[11] == -1 && maxCount >= 2 && maxCount <= 3 && rollsLeft > 0) {
    // Going for Yahtzee with only 2-3 dice - high risk
    isHighRisk = true;
    if(!aiTrainingMode) {
      aiLearning.highRiskAttempts++;
    }
  }
  
  // Hold strategy based on what we have
  if(maxCount >= 4) {
    // Hold 4+ of a kind
    for(int i = 0; i < 5; i++) {
      held[i] = (dice[i] == maxValue);
    }
  } else if(maxCount == 3) {
    // Use variant's 3-of-kind weight to decide
    float weight3 = (maxValue <= 3) ? variant.weight3OfKindLow : (variant.weight3OfKindHigh / 10.0);
    
    // Check if this value is needed for upper section
    int upperCategoryIdx = maxValue - 1;
    bool upperAvailable = (scores[upperCategoryIdx] == -1);
    
    // If low value (1-3) AND upper available AND pursuing bonus, hold for upper
    if(maxValue <= 3 && upperAvailable && needsBonus && bonusStillPossible) {
      for(int i = 0; i < 5; i++) {
        held[i] = (dice[i] == maxValue);
      }
    } else if(weight3 > 25) {  // High priority - hold it
      for(int i = 0; i < 5; i++) {
        held[i] = (dice[i] == maxValue);
      }
    } else {
      // Lower priority - analyze straight potential first
      StraightAnalysis straightAnalysis = analyzeStraightPotential(dice, scores, variant, rollsLeft);
      
      // Compare triple vs straight pursuit
      float tripleEV = maxValue * 3 * 1.5;  // Expected value of holding triple
      
      // **LEARNING ENHANCEMENT: Use historical hold pattern success data**
      if(aiLearning.gamesPlayed >= 20) {  // Need experience
        float tripleQuality = getHoldPatternQuality(maxValue, 3);
        
        // Boost EV if this pattern has worked well historically
        if(tripleQuality > 0.6) {
          tripleEV *= 1.2;  // 20% boost for proven pattern
        } else if(tripleQuality < 0.4 && tripleQuality > 0) {
          tripleEV *= 0.8;  // 20% penalty for historically poor pattern
        }
      }
      
      if(straightAnalysis.shouldPursueStraight && straightAnalysis.expectedValue > tripleEV) {
        // Straight is better - use straight holds
        for(int i = 0; i < 5; i++) {
          held[i] = straightAnalysis.holds[i];
        }
      } else {
        // Triple is better - hold it
        for(int i = 0; i < 5; i++) {
          held[i] = (dice[i] == maxValue);
        }
      }
    }
  } else if(maxCount == 2) {
    // Have a pair - ALWAYS check straight potential FIRST
    
    // Analyze straight potential
    StraightAnalysis straightAnalysis = analyzeStraightPotential(dice, scores, variant, rollsLeft);
    
    // **CRITICAL: If pursuing straight, IGNORE the pair completely**
    if(straightAnalysis.shouldPursueStraight) {
      // Use straight holds (this will NOT include the pair)
      for(int i = 0; i < 5; i++) {
        held[i] = straightAnalysis.holds[i];
      }
      
      
      // **NEW: Track if this is a high-risk straight attempt**
      if(straightAnalysis.consecutiveLength <= 3 && rollsLeft <= 1 && !isHighRisk) {
        isHighRisk = true;
        if(!aiTrainingMode) {
          aiLearning.highRiskAttempts++;
        }
      }
      
      return;  // EXIT EARLY - don't consider pair at all
    }
    
    // No straight pursuit - NOW evaluate the pair
    int pairValue = maxValue;
    int pairScore = pairValue * 2;
    
    // Calculate expected value of holding the pair
    float pairEV = 0;
    
    // Check if pair's value can be used in upper section
    int upperCategoryIdx = pairValue - 1;
    bool upperAvailable = (scores[upperCategoryIdx] == -1);
    
    if(upperAvailable && needsBonus && bonusStillPossible) {
      // Pair is valuable for upper section bonus
      pairEV = pairScore * 2.5;  // Boost for bonus pursuit
    } else if(pairValue >= 5) {
      // High pair (5-6) - decent for 3/4-of-kind
      pairEV = pairScore * 2.0;
    } else if(pairValue >= 4) {
      // Mid pair (4)
      pairEV = pairScore * 1.5;
    } else {
      // Low pair (1-3) - not great
      pairEV = pairScore * 1.0;
    }
    
    // **LEARNING ENHANCEMENT: Use historical hold pattern success data for pairs**
    if(aiLearning.gamesPlayed >= 20) {  // Need experience
      float pairQuality = getHoldPatternQuality(pairValue, 2);
      
      // Adjust EV based on historical success
      if(pairQuality > 0.6) {
        pairEV *= 1.15;  // 15% boost for proven pairs
      } else if(pairQuality < 0.35 && pairQuality > 0) {
        pairEV *= 0.85;  // 15% penalty for historically poor pairs
      }
    }
    
    // Hold the pair
    for(int i = 0; i < 5; i++) {
      held[i] = (dice[i] == pairValue);
    }
    
    
  } else {
    // No pair, no triple - analyze all options
    
    // Option 1: Straight pursuit
    StraightAnalysis straightAnalysis = analyzeStraightPotential(dice, scores, variant, rollsLeft);
    
    // Option 2: Upper section (hold best single die for upper)
    int bestUpperValue = -1;
    int bestUpperCount = 0;
    
    for(int value = 6; value >= 1; value--) {
      int categoryIdx = value - 1;
      if(scores[categoryIdx] == -1 && counts[value] > bestUpperCount) {
        bestUpperCount = counts[value];
        bestUpperValue = value;
      }
    }
    
    float upperEV = 0;
    if(bestUpperValue != -1 && needsBonus && bonusStillPossible) {
      upperEV = bestUpperCount * bestUpperValue * 2.0;
    } else if(bestUpperValue >= 5) {
      upperEV = bestUpperCount * bestUpperValue * 1.2;
    }
    
    // Option 3: Hold high dice (5-6) for Chance or 3/4-of-kind
    int highDiceCount = counts[5] + counts[6];
    float highDiceEV = highDiceCount * 2.5;
    
    // Choose best option
    if(straightAnalysis.shouldPursueStraight && straightAnalysis.expectedValue > upperEV && straightAnalysis.expectedValue > highDiceEV) {
      // Pursue straight
      for(int i = 0; i < 5; i++) {
        held[i] = straightAnalysis.holds[i];
      }
      
    } else if(upperEV > highDiceEV && bestUpperValue != -1) {
      // Focus on upper section
      for(int i = 0; i < 5; i++) {
        held[i] = (dice[i] == bestUpperValue);
      }
      
    } else {
      // Hold high dice
      for(int i = 0; i < 5; i++) {
        held[i] = (dice[i] >= 5);
      }
    }
  }
} 

int selectCategoryWithVariant(int dice[], int* scores, int variantIndex) {
  StrategyVariant& variant = strategyVariants[variantIndex];
  
  int bestCategory = -1;
  float bestScore = -1;
  
  // Calculate available categories
  int availableCategories = 0;
  int availableCats[13];
  for(int i = 0; i < 13; i++) {
    if(scores[i] == -1) {
      availableCats[availableCategories++] = i;
    }
  }
  
  // === DETERMINE GAME PHASE ===
  int usedCategories = 0;
  for(int i = 0; i < 13; i++) {
    if(scores[i] != -1) usedCategories++;
  }
  
  bool earlyGame = (usedCategories <= 3);
  bool midGame = (usedCategories >= 4 && usedCategories <= 9);
  bool lateGame = (usedCategories >= 10);
  
  // Calculate upper section status
  int upperTotal = getUpperSectionTotal(scores);
  bool needsBonus = (upperTotal < 63);
  int bonusRemaining = 63 - upperTotal;
  
  // Count upper categories remaining
  int upperCategoriesOpen = 0;
  for(int j = 0; j <= 5; j++) {
    if(scores[j] == -1) upperCategoriesOpen++;
  }
  
  // Check if bonus is still achievable AND realistic
  int maxPossibleUpper = upperCategoriesOpen * 18;  // 3×6 = 18 per category
  bool bonusStillPossible = (bonusRemaining <= maxPossibleUpper);
  float bonusFeasibility = calcBonusFeasibility(scores);
  bool bonusWorthPursuing = bonusStillPossible && (bonusFeasibility >= 0.25f);
  
  // Calculate average needed per remaining category
  float avgNeeded = (upperCategoriesOpen > 0) ? (float)bonusRemaining / upperCategoriesOpen : 0;

  // Count how many of each die face we currently have (needed for opportunity cost)
  int counts[7] = {0};
  for(int d = 0; d < 5; d++) counts[dice[d]]++;

  for(int i = 0; i < 13; i++) {
    if(scores[i] == -1) {
      float score = calculateCategoryScore(dice, i);
      float weightedScore = score;
      
      // Upper section (0-5)
      if(i <= 5) {
        int value = i + 1;  // 1-6
        int actualScore = score;
        
        // Opportunity cost: how wasteful is this score for this slot?
        // 0.0 = perfect (5 of this value), 1.0 = terrible (1 die only for a high-value slot)
        float oppCost = upperOpportunityCost(value, counts[value]);
        
        // Calculate roll quality FIRST (needed for all paths)
        float rollQuality = 0;
        if(value > 0) {
          rollQuality = (float)actualScore / (value * 3);  // 0.0 to 1.67
        }

        if(needsBonus && bonusWorthPursuing) {
          // ── OPPORTUNITY COST GUARD ──────────────────────────────────────────
          // If scoring here wastes most of the slot's potential, heavily
          // penalise it – the AI should wait (or sacrifice a cheaper slot).
          // oppCost >= 0.80: only 1 die for a mid/high value – terrible
          // oppCost >= 0.65: only 1-2 dice for a high value  – bad
          if(oppCost >= 0.80f && upperCategoriesOpen > 2 && !earlyGame) {
            // Almost certainly shouldn't score here – 90% penalty
            weightedScore = score * 0.10f;
          } else if(oppCost >= 0.65f && value >= 4 && upperCategoriesOpen > 2 && !earlyGame) {
            // Scoring just 1 high-value die is wasteful – 70% penalty
            weightedScore = score * 0.30f;
          }
          // ── STANDARD QUALITY-BASED WEIGHTING ───────────────────────────────
          // **CRITICAL**: Don't take weak upper scores (< 6 pts) in mid-game
          // unless desperate (2 or fewer categories left)
          else if(actualScore < 6 && upperCategoriesOpen > 2 && !earlyGame) {
            weightedScore = score * 0.2;  // 80% penalty for weak scores in mid/late game
          }
          // Even in early game, penalize extremely weak scores (1-3 points)
          else if(actualScore <= 3 && upperCategoriesOpen > 4) {
            weightedScore = score * 0.3;  // 70% penalty for terrible scores
          }
          // BALANCED BONUS PURSUIT
          else if(rollQuality >= 1.0) {
            // 3+ dice - EXCELLENT, take it
            weightedScore = score * 5.0;
          } else if(rollQuality >= 0.66) {
            // 2 dice - GOOD
            if(actualScore >= avgNeeded * 0.7) {
              weightedScore = score * 3.5;
            } else {
              weightedScore = score * 2.0;
            }
          } else if(rollQuality >= 0.33) {
            // 1 die - only if high value or desperate
            if(upperCategoriesOpen <= 3 || value >= 5) {
              weightedScore = score * 1.8;
            } else {
              weightedScore = score * 0.8;
            }
          } else {
            // Very poor roll
            if(upperCategoriesOpen <= 2 && actualScore > 0) {
              weightedScore = score * 1.2;
            } else {
              weightedScore = score * 0.3;
            }
          }
          
          // EXTRA BOOST when close to bonus
          if(bonusRemaining <= 20) {
            weightedScore *= 1.3;
          }
          if(bonusRemaining <= 10) {
            weightedScore *= 1.5;
          }
          
          // **TURN-AWARE BONUS PURSUIT**: Get more aggressive as game progresses
          // If we're in late game and need bonus, increase priority
          if(lateGame && bonusRemaining > 0 && bonusWorthPursuing) {
            // Calculate urgency: fewer turns + more points needed = more urgent
            float urgency = 1.0;
            if(upperCategoriesOpen <= 3 && bonusRemaining <= 20) {
              urgency = 2.0;  // VERY urgent
            } else if(upperCategoriesOpen <= 4 && bonusRemaining <= 30) {
              urgency = 1.5;  // Urgent
            }
            
            // Apply urgency multiplier
            weightedScore *= urgency;
          }
          
          // **LEARNING ENHANCEMENT: Factor in historical category performance**
          if(aiLearning.gamesPlayed >= 30) {  // Need more games for upper section data
            float avgCatScore = getCategoryAvgScore(i);
            if(avgCatScore > 0 && actualScore >= avgCatScore * 0.8) {
              // This is a good score compared to historical average
              weightedScore *= 1.08;  // 8% boost
            } else if(avgCatScore > 0 && actualScore < avgCatScore * 0.5) {
              // This is a poor score compared to average
              weightedScore *= 0.95;  // 5% penalty
            }
          }
          
        } else if(!needsBonus) {
          // Already have bonus - normal priority
          weightedScore = score * 1.0;
        } else {
          // Bonus impossible or not worth pursuing - low priority
          weightedScore = score * 0.5;
        }
      }
        
      // Lower section categories
      else if(i == 6 || i == 7) {
        // 3-of-a-kind and 4-of-a-kind
        int counts[7] = {0};
        for(int j = 0; j < 5; j++) {
          counts[dice[j]]++;
        }
        
        int maxCount = 0;
        int maxValue = 0;
        for(int v = 1; v <= 6; v++) {
          if(counts[v] > maxCount) {
            maxCount = counts[v];
            maxValue = v;
          }
        }
        
        // **CRITICAL BLOCKING**: Never select 3/4-of-kind for low values in early/mid game
        // if the corresponding upper section is still available
        if((maxValue <= 3) || (maxValue == 4 && earlyGame)) {
          int upperCategoryIdx = maxValue - 1;
          
          // If upper section is available, MASSIVELY penalize 3/4-of-kind
          if(scores[upperCategoryIdx] == -1) {
            weightedScore = score * 0.001;  // 99.9% penalty - essentially block it
          }
          // Upper used but 3-of-kind/4-of-kind has low value - still penalize heavily
          else if(i == 7) {  // 4-of-kind penalty is more severe
            weightedScore = score * 0.05;  // 95% penalty for 4-of-kind
          } else {  // i == 6, 3-of-kind
            weightedScore = score * 0.1;  // 90% penalty for 3-of-kind
          }
        }
        // Normal scoring for mid-high values or late game
        else {
          if(i == 6 && maxCount >= 3) {
            // 3-of-a-kind - weight based on value AND game phase
            if(maxValue >= 5) {
              // **BOOST**: High value 3-of-kinds are excellent
              weightedScore = score * 2.5;  // Increased from 2.0
            } else if(maxValue >= 4) {
              weightedScore = score * 1.8;  // Increased from 1.5
            } else {
              weightedScore = score * 1.0;
            }
          } else if(i == 7 && maxCount >= 4) {
            // 4-of-a-kind - good for high values
            if(maxValue >= 4) {
              weightedScore = score * 2.5;
            } else {
              weightedScore = score * 1.0;
            }
          } else {
            weightedScore = score * 0.5;
          }
        }
      }

// Apply variant's weights for special categories
      else if(i == 11 && score > 0) {
        float baseWeight = variant.weightYahtzee / 10.0;
        weightedScore = score * baseWeight;
        
        // **LEARNING ENHANCEMENT: Boost if above historical average**
        if(aiLearning.gamesPlayed >= 20) {
          float avgScore = getCategoryAvgScore(11);
          if(avgScore > 0 && score >= avgScore * 0.9) {
            weightedScore *= 1.1;  // 10% bonus for good Yahtzee
          }
        }
      } else if(i == 10 && score > 0) {
        float baseWeight = variant.weightLargeStraight / 10.0;
        weightedScore = score * baseWeight;
        
        // **LEARNING ENHANCEMENT: Large Straight is always 40, so use win rate instead**
        if(aiLearning.gamesPlayed >= 20) {
          float winRate = getCategoryWinRate(10);
          if(winRate > 0.6) {
            weightedScore *= 1.15;  // Boost if historically leads to wins
          }
        }
      } else if(i == 9 && score > 0) {
        float baseWeight = variant.weightSmallStraight / 10.0;
        weightedScore = score * baseWeight;
        
        // **LEARNING ENHANCEMENT: Small Straight is always 30, use win rate**
        if(aiLearning.gamesPlayed >= 20) {
          float winRate = getCategoryWinRate(9);
          if(winRate > 0.6) {
            weightedScore *= 1.12;  // Boost if historically leads to wins
          }
        }
      } else if(i == 8 && score > 0) {
        float baseWeight = variant.weightFullHouse / 10.0;
        weightedScore = score * baseWeight;
        
        // **LEARNING ENHANCEMENT: Full House is always 25, use win rate**
        if(aiLearning.gamesPlayed >= 20) {
          float winRate = getCategoryWinRate(8);
          if(winRate > 0.6) {
            weightedScore *= 1.1;  // Boost if historically leads to wins
          }
        }
      }
      // Chance - HEAVILY penalized if straights are available
      else if(i == 12) {
        // CRITICAL: Never prefer Chance over straights
        if(scores[9] == -1 || scores[10] == -1) {
          // At least one straight is still available
          weightedScore = score * 0.1;  // 90% penalty!
        } else {
          // Both straights used - Chance is okay
          weightedScore = score;
        }
      }
      // Other categories
      else {
        weightedScore = score;
      }

      // === END-GAME BOOST: When few categories left, make SMART decisions ===
      if(availableCategories <= 3) {
        // Calculate game state
        int total1 = calculateTotalWithBonuses(scores1, yahtzeeBonus1);
        int total2 = calculateTotalWithBonuses(scores2, yahtzeeBonus2);
        int scoreDiff = total2 - total1;  // Positive = winning
        
        // Check upper bonus potential
        int upperTotal = getUpperSectionTotal(scores);
        int bonusRemaining = 63 - upperTotal;
        int upperCatsOpen = 0;
        for(int j = 0; j <= 5; j++) {
          if(scores[j] == -1) upperCatsOpen++;
        }
        bool canGetBonus = (bonusRemaining <= upperCatsOpen * 18);
        bool closeToBonus = (bonusRemaining <= 15 && canGetBonus);
        
        // CRITICAL: If upper bonus will help us win, MASSIVELY prioritize it
        if(i <= 5 && closeToBonus && canGetBonus) {
          int potentialBonusValue = 35;  // Upper bonus
          bool bonusHelpsWin = (potentialBonusValue >= abs(scoreDiff) - 10);
          
          if(bonusHelpsWin) {
            // Upper bonus is our path to victory - MASSIVE boost
            weightedScore *= 5.0;
          }
        }
        
        // Otherwise, boost high-value categories only if realistic
        if(i == 11 && score > 0) {  // Yahtzee (actual)
          weightedScore *= 3.0;  // Huge boost if we actually have it
        } else if(i == 11 && score == 0) {  // Yahtzee (don't have it)
          weightedScore *= 0.5;  // Penalize - don't chase impossible Yahtzee
        } else if(i == 10 && score > 0) {  // Large Straight (actual)
          weightedScore *= 2.5;
        } else if(i == 10 && score == 0) {  // Large Straight (don't have it)
          weightedScore *= 0.3;  // Heavy penalty
        } else if(i == 9 && score > 0) {  // Small Straight (actual)
          weightedScore *= 2.0;
        } else if(i == 8 && score > 0) {  // Full House (actual)
          weightedScore *= 1.8;
        }
        
        // If losing badly, prefer guaranteed points over risky 0-point specials
        if(scoreDiff < -15) {
          if(score == 0 && (i >= 7 && i <= 11)) {  // 0-point special category
            weightedScore *= 0.1;  // Massive penalty - we need points NOW
          } else if(score >= 15) {  // Good guaranteed score
            weightedScore *= 2.0;  // Boost reliable points
          }
        }
      }
      
      // Debug output for AI decision-making
      if(i <= 5) {
      } else if(i >= 6 && i <= 12) {
      }
      
      if(weightedScore > bestScore) {
        bestScore = weightedScore;
        bestCategory = i;
      }
    }
  }
  
  return bestCategory;
}
