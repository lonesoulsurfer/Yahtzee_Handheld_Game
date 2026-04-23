
How the Yahtzee AI Works
When I first went on the journey to add a Player 1 vs the Computer mode, I didn't think it would take me down such a deep rabbit hole. The game of Yahtzee is a game of subtle nuances that revealed themselves to me when I tried (and still trying) to make an AI God Mode — an unbeatable opponent that always knows the right rolls and holds, and could use probability, strategy and learned weights to never lose.

The below is an outline of where I am currently.

In a Nutshell
The AI holds dice based on what it's closest to completing, always thinking about whether the potential reward is worth the risk of rerolling. It respects special categories like Yahtzee and Large Straight enough to keep chasing them even late in a roll. It gets smarter the more you play against it, gradually learning what tends to win and adjusting its priorities accordingly. And at higher difficulties, it's simply more willing to gamble on a better outcome rather than accepting a safe but mediocre score.

The Big Picture
The AI isn't just picking randomly or following a simple checklist. It's constantly asking itself two questions on every turn:

"Which dice should I keep?" "Is what I have now worth stopping for, or should I roll again?"

Everything it does flows from trying to answer those two questions as smartly as possible.

How It Decides Which Dice to Hold
Think of the AI like a card player who's always looking at their hand and asking "what am I closest to completing?" It looks at the dice and runs through a mental checklist, roughly in this order:

Is the game almost over? If there are only 2–3 scoring boxes left to fill, it stops being flexible and commits completely to whatever it still needs — chasing a Yahtzee, holding a straight sequence, or locking in pairs for a Full House. Crucially, it uses the actual remaining categories to decide what to hold — it won't hold two different numbers if neither remaining category can use them.

Are we close to the upper section bonus? The +35 bonus for scoring 63+ points in the top half of the scorecard is huge. If the AI is getting close, it'll often prioritise holding dice that help fill those upper boxes (Ones through Sixes), even if it means passing on something flashy.

What's the best hand it's sitting on? It looks at whether it has four-of-a-kind, three-of-a-kind, a pair, or a run of consecutive numbers, and figures out which is worth developing. It doesn't just hold the "most dice" — it asks which hand is actually worth the most if completed. Four 1s sounds like a lot to hold, but if the Ones box is already filled and Yahtzee is still available, it'll go for Yahtzee instead.

Key rule: Pairs beat nothing, but a straight run beats a pair. If you have 2-3-4-5 showing, the AI will completely ignore any pair and hold the run, because straights are worth a flat 30 or 40 points — more predictable and often more valuable than building three-of-a-kind from a low pair.

Probability Tables and Expected Value
Rather than using gut feel, the AI uses hardcoded probability tables to calculate the exact odds of completing any hand from its current dice. These are precomputed two-dimensional tables:

Table	Description
Of-a-Kind	Probability of hitting 2-, 3-, 4-, or 5-of-a-kind when rerolling a specific number of dice (e.g. holding 3-of-a-kind and rerolling 2 dice has ~7.4% chance of hitting 5-of-a-kind in one roll)
Large Straight	Probability of completing a Large Straight based on how many values are still missing and how many rolls remain
Small Straight	Same as above for Small Straight
For each possible way to hold the dice, the AI computes an Expected Value (EV) — the average score it can realistically expect if it plays that way. It then picks whichever hold pattern produces the highest EV, adjusted by the learned weights described below. This EV approach is the core engine; everything else either feeds into it or overrides it when warranted.

Locked Decisions: When EV Doesn't Apply
Some situations are so mathematically clear that the AI locks the decision before any EV comparison or opponent-model adjustment is allowed to interfere:

4-of-a-kind with Yahtzee open → Always reroll the 5th die
3-of-a-kind with Yahtzee open (or high value) → Always hold the triple and chase the 4th (and potentially 5th) match
Yahtzee or Large Straight already in hand → Always stop
Small Straight in hand with Large Straight already used → Always stop and bank the guaranteed 30 points
These decisions set a decision made flag that prevents any downstream logic — conservative overrides, endgame risk adjustments, trailing/leading adjustments — from cancelling them.

How It Decides Whether to Roll Again
After the first roll, it almost always rerolls. Unless it rolled a Yahtzee or Large Straight right out of the gate, it's rolling again. There's also a minimum score floor: on the first roll, the AI won't stop unless it has 35+ points in hand (40+ on God Mode).

After the second roll, it gets more selective. A confirmed Full House or solid upper box score is enough to stop — but four-of-a-kind with Yahtzee available, or four in a row with Large Straight available, always earns that third roll.

It also reads the score. If the AI is losing by a decent amount, it plays more aggressively. If it's comfortably ahead, it locks in reliable points.

Difficulty Margins
Difficulty	Reroll Condition
Normal	EV of rerolling must exceed current score by at least 3 points
Hard	EV must match or exceed current score (break-even is enough)
God Mode	Will reroll even if EV is slightly lower — always chasing the maximum
The Three Difficulty Levels
Level	Style	Base Aggression
Normal	Solid but forgiving. Stops if it has a decent score, won't chase long shots.	0.35
Hard	More aggressive. Willing to reroll a decent hand for something better. Uses learned data to adjust aggressiveness dynamically.	0.65
God Mode	Almost never stops after one roll. Squeezes every last roll out of every turn. Very tough to beat consistently.	0.95
Exploration vs Exploitation
About 10% of games, the AI deliberately plays differently from its learned "best" strategy — this is an exploration game. It picks a random strategy (conservative or aggressive) with a randomised aggressiveness weight between 0.3 and 0.9.

The other 90% of games it sticks to what's been working best — this is exploitation.

This balance ensures the AI doesn't get permanently stuck doing the same thing if something slightly different would actually work better.

How It Learns Over Time
This is where it gets interesting — the AI actually remembers how past games went and slowly adjusts its behaviour. All learning data is saved to the device's EEPROM (non-volatile flash memory), so it persists even after you turn it off. A checksum is stored alongside the data so the device can detect and recover from any corruption.

Here's what it tracks and learns from:

What scores tend to lead to wins — If games where it scores a Large Straight tend to go well, it prioritises Large Straights more. These preferences are stored as weight values per category type.

Confidence scaling — Each weight adjustment is scaled by how many data points back it up. A weight based on 50 games carries four times more authority than one based on 5 games.

Whether rerolling was worth it — by score bracket — It tracks reroll outcomes in four buckets (0–9, 10–19, 20–29, 30+) and builds a picture of when rerolling pays off in each bracket.

How each specific hold pattern performs — For every combination of dice value and count held (e.g. "held two 5s", "held three 6s"), it records how often that pattern led to a score of 20 or more. Tracked in a 6×6 grid (values 1–6 × counts 1–5).

When chasing Yahtzee or straights historically pays off — Separate success rate counters track every attempt and completion. If Yahtzee success rate falls below 12%, it tightens the entry threshold (only chases with 3+ matching dice). If it climbs above 28%, it gets more aggressive (will chase from just a pair with 2 rolls left).

Which hold count works best (0–5 dice held) — Tracked and displayed in the stats screen; informs the big picture of how the AI is playing.

Endgame close-game outcomes — From turn 11 onwards, if the game is within 20 points, the AI records whether it won or lost. If its close-game win rate falls below 45%, it becomes more aggressive in the final turns.

Chance category timing — Chance is most valuable early when all other slots are open. The AI tracks when it scores Chance (turns 1–3, 4–6, 7–9, or 10–13) and how often each timing leads to a win.

Score position at turn 10 — Tracked as a running average of (AI score minus human score) at the halfway point. If the AI is consistently 20+ points behind at turn 10, it raises aggression on early rerolls.

Category timing — For each of the 13 scoring categories, the AI tracks the average turn it tends to score that category and uses this to inform future category priority.

Your playing style — The AI quietly tracks how you play:

How often you hit the upper bonus
How many Yahtzees you roll
How aggressive you are (reroll rate and chase attempts)
Your average score
Against a strong player (avg. score over ~250) it plays harder, rerolling on scores it might otherwise accept. Against a conservative player it locks in moderate scores more readily. If you frequently score Yahtzees, the AI prioritises chasing its own Yahtzee more urgently.

The Four Strategies It Tests Against Itself
When you use the Train AI option in the Tools menu, the AI plays against itself using four different personalities:

Strategy	Style
Aggressive	Goes hard for Yahtzees and big straights, willing to sacrifice safe points
Balanced	Uses whatever it's currently learned as its best approach
Conservative	Prioritises guaranteed points and the upper bonus over risky special hands
Experimental	A slightly randomised version, trying things a bit differently each time
After each batch of self-play games, it looks at which personality won the most and nudges its main strategy in that direction. It's a slow process — you won't see it transform overnight — but over dozens of games it genuinely does shift toward what's been working.

Each strategy variant stores its own set of 8 weight values alongside its performance record (wins, games played, total score). It mostly picks the variant with the best recent win rate, but occasionally gives the others a shot so they can keep accumulating data.

Category Selection
Once the AI has decided to stop rolling, it chooses which scoring category to use:

It calculates an expected value for every open category given the current dice.
Upper section categories are weighted by a bonus feasibility score — if the 35-point bonus is still realistically achievable, upper slots get a boost proportional to how much they contribute toward hitting 63.
It considers opportunity cost — using a high-value slot (like Sixes) for a weak result is penalised more than using a low-value slot (like Ones) for the same score.
In the final 1–2 turns, category selection becomes critical: it won't squander a Yahtzee chase on a 3-of-a-kind slot, and it won't take Chance if a more targeted category is still reachable.
What Gets Displayed in the Stats Screens
The AI Statistics screen (accessible from the Tools menu) shows six pages of data including:

Overall win rate and score history
Reroll improvement rates per score bracket
Hold pattern frequency
Endgame win rates in close games and blowouts
The four strategy variant performances
Chance category timing
Turn-10 score position history
Current live values of all 8 learned weights
The graphs also track early-game vs recent-game average score so you can see whether the AI is improving over time.

Timeout Protection
If the AI ever gets into an unexpected state and takes more than 30 seconds to make a decision, a watchdog timer forces it to pick the best available category immediately and resets its state. This prevents the game from ever hanging.

A Note on God Mode
God Mode is the closest the AI gets to theoretically optimal Yahtzee play. It uses maximum aggression, the full probability tables, all learned weights, and never applies conservative overrides.

It is beatable — Yahtzee has a meaningful luck component — but it will consistently make the highest expected value decision available to it on every single roll.
