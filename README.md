# Yahtzee - Handheld Game with an Optimal AI Brain

YouTube - https://youtu.be/DMBQGAXjRF4

Instructables - https://www.instructables.com/Yahtzee-Raspberry-Pi-Handheld-Game/



This is a handheld Yahtzee game with a computer opponent. It uses a Waveshare RP2040-Zero and an ST7789 colour screen. The firmware also includes a Blackjack game.

<img src="https://github.com/user-attachments/assets/8ec25d56-e4e9-4fbc-844d-4c83ffc45c60" width="300" alt="EKBK0852" />
<img src="https://github.com/user-attachments/assets/09395b5d-c96f-4142-bf45-931f55bf02c9" width="300" alt="RZYY5395" />
<img src="https://github.com/user-attachments/assets/17aec8ca-5ae5-4455-88a7-85767c115892" width="300" alt="JPOU3428" />


---

## What you need

- All parts are in the [parts list](Parts%20List%20(1).pdf).
- The schematic is [Yahtzee - Schematic.png](Yahtzee%20-%20Schematic.png).
- The Gerber files for the PCB and the panels are in [Gerber_Files](Gerber_Files/). You can send them to a PCB maker.

## Upload the firmware

1. Download this repository. Click **Code → Download ZIP**, then extract the ZIP file.
2. In the Arduino IDE, install the board package **Raspberry Pi Pico/RP2040** by Earle Philhower.
3. In the **Library Manager**, install **Adafruit GFX Library** and **Adafruit ST7735 and ST7789 Library**.
4. Open `Yahtzee_v2/Yahtzee_v2.ino` (recommended), or `Yahtzee_v1/Yahtzee_v1.ino`.
5. Connect the RP2040-Zero to your computer with a USB cable.
6. In the **Tools** menu, set **Board** to **Waveshare RP2040 Zero**, and set **Port**.
7. Click **Upload**.

For V2, the file `vtable.h` must be in the same folder as the sketch. It is already there.

More detail about the computer opponent is in [YAHTZEE_AI.md](YAHTZEE_AI.md).

---

## Two Versions of the Firmware

There are two versions of the game engine's "brain", and both are available to download and try from this repo:

- **V1, the Machine Learning brain.** The original approach: hand-built heuristics, probability tables, and a set of weighted priorities that adjust over time through self-play, stored in EEPROM. See the [V1 Overview](#v1-overview-the-machine-learning-brain) below.
- **V2, the Optimal "V-Brain".** The current version, and the one the rest of this README focuses on. It replaces the learned weights with an exact, mathematically solved decision table. See [How V2 Works](#how-v2-works-the-optimal-v-brain) below.

Both firmware versions are fully playable and included in this repo (`Yahtzee_v1.ino` / `Yahtzee_v2.ino`) if you want to compare them, or if you just prefer the adaptive feel of V1.

---

## V1 Overview: The Machine Learning Brain

V1 started out rule based, then grew into a probability and weights system. It uses hardcoded probability tables (for straights and of-a-kinds) to work out expected value, then adjusts its priorities, things like Yahtzee chasing, upper bonus feasibility, and endgame aggression, using a set of learned weights. Those weights live in EEPROM and shift over time based on self-play (the "Train AI" tool runs four personality variants against each other) and on how the human opponent plays.

It's a genuinely adaptive system. It gets tougher the more you play it, and it reads your playing style to adjust its own. But it's still fundamentally an approximation: the weights are a best guess at what matters in a given situation, tuned by trial and error rather than worked out from the actual maths of the game, and it needs a run-up of games before its learned behaviour is any good.

---

## How V2 Works: The Optimal V-Brain

V2 throws out the heuristics and weights entirely and replaces them with an exact solution, worked out once using dynamic programming and baked into a lookup table (`vtable.h`) that ships with the firmware. Rather than estimating the best move, V2 looks up the true expected value of every possible decision for the exact game state it's in, and simply picks the best one.

### The Value Table

`vtable.h` contains a solved value for every combination of:
- Which of the 13 scoring categories are still open (the "scorecard mask")
- The current upper section running total (capped at 63, since anything beyond that doesn't matter to the bonus)
- Whether a Yahtzee has already been scored (which changes what future Yahtzees are worth, thanks to the 100 point bonus)

For any of those states, the table gives the true expected final score from that point onward, assuming optimal play. This was worked out offline by starting from the end of the game and working backwards (a classic dynamic programming approach), so every value in the table is provably correct, not estimated.

### Making a Decision

On every roll, the V-brain does the following:
1. Sorts the current dice and looks up which of the 252 possible dice states it's in.
2. For each way it could hold some subset of the dice, works out the expected value of rerolling the rest, by weighting every possible reroll outcome by its true probability and looking up the resulting value in the table.
3. Compares that against simply stopping and scoring the best available category right now.
4. Picks whichever option has the higher expected value. No weights, no priority list, just the actual numbers.

This same lookup approach handles category selection too. Once the game engine stops rolling, it checks the value of scoring in every open category and takes the one with the highest true expected value, correctly accounting for the upper section bonus and the cost of spending a good category on a mediocre roll.

### Why It's Better

V1's weights can only ever approximate optimal play, and they need games (and EEPROM wear) to converge toward something good. V2's table already is the optimal policy. There's nothing to learn or converge toward, so it plays at maximum strength from the very first roll of the very first game, every single game. The trade-off is that V2 no longer profiles or adapts to a specific human opponent the way V1's learning system did. It isn't trying to be adaptive, it's trying to be correct.

### Still Carried Over from V1

A few things didn't need reinventing, so V2 keeps them as is:
- **Timeout protection.** If the game engine takes more than 30 seconds to decide, a watchdog forces it to pick the best available category and resets its state, so the game can never hang.
- **Stats tracking.** Win/loss history, high scores, and Yahtzee counts are still tracked and stored in EEPROM for the stats screens, even though none of it feeds back into how the V-brain plays.

---

## A Note on God Mode / Difficulty

V1's Normal / Hard / God Mode difficulty levels changed how aggressively the game engine would reroll. In V2, difficulty no longer changes how the AI plays. The V-brain always plays the mathematically optimal move, since there's no such thing as a "more optimal than optimal" setting. It is still beatable, since Yahtzee has a meaningful luck component, but you're playing against the best possible strategy on every roll.

---

## Support

Do you like this project? You can support my work on [GitHub Sponsors](https://github.com/sponsors/lonesoulsurfer).

## Licence

- **Code:** [MIT License](LICENSE). You can use, change and sell it.
- **Hardware** (PCB, panels and Gerber files): [CERN Open Hardware Licence v2, Strongly Reciprocal](LICENSE-HARDWARE). You can make, change and sell boards. If you share or sell a changed design, you must also share the changed design files under the same licence.

Copyright (c) 2026 Marcus (lonesoulsurfer).
