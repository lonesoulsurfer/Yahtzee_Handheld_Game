YAHTZEE - HANDHELD GAME WITH MACHINE LEARING

My favourite types of games are ones that require little bit of skill, and a little bit of luck and Yahtzee is definitely one of those games.


So in my continuing journey in all things microcontrollers, I decided to build my own electronic Yahtzee handheld game. In this build I used a Raspberry Pi Zero which is an ultra-compact version of the Raspberry Pi Pico.
The game can be played in either 2 player mode or player 1 vs the computer. Initially this seemed relatively straight forward. However, the more I worked on the code, the more I fell down a rabbit hole of trying to make the computer (or AI as I have been calling it!) actually learn and become a better player the more it plays!


I worked with Claude, and AI assistant developed by Anthropic to develop the code and the AI learning. If you are new to coding and want a place to start, then give Claude a go.


Now, the code is HUGE! Like 13K 14K 15K lines huge. I wasn’t planning to make it such a monster but trying to get the AI to learn was (and is still) a journey.


Initially the AI was rule based but these weren’t subtle enough when it came to making the right decisions. I thought well Yahtzee is just probabilities, I’ll get it to use those to determine best strategy. Using this strategy along with adding some weighted values helped to make the AI a better player. However, it was still ultimately rules based. I needed a way for the AI to learn which is why I implemented the ability for the AI to play itself and work out best strategies and adjust the weights according to how well certain types of games went. It also examines the way player 1 is playing and adapts!



<img width="1800" height="2400" alt="EKBK0852" src="https://github.com/user-attachments/assets/8ec25d56-e4e9-4fbc-844d-4c83ffc45c60" />

<img width="1800" height="2400" alt="MQDH6629" src="https://github.com/user-attachments/assets/2868cfb5-dff3-4b64-b821-424fb2c662f9" />

<img width="1512" height="2016" alt="JPOU3428" src="https://github.com/user-attachments/assets/17aec8ca-5ae5-4455-88a7-85767c115892" />

<img width="1512" height="1512" alt="RZYY5395" src="https://github.com/user-attachments/assets/09395b5d-c96f-4142-bf45-931f55bf02c9" />





