# Skive

Skive is an esolang (esoteric programming language) that looks like someone duct-taped Brainfuck, Befunge, and a CAD tool together. It operates on an **infinite 2D grid of 8-bit cells**, where a pointer wanders around incrementing, decrementing, cloning, teleporting, and generally making a mess.  

Its design goals?  
- Be ridiculous.  
- Be powerful enough that people regret using it.  
- Give you *way* too many ways to mess with memory at once.  

---

## Memory Model
- The "tape" is a sparse 2D grid indexed by `(row, column)`.  
- Each cell holds an 8-bit integer (0–255).  
- Any uninitialized cell is **0** by default.  
- The pointer starts at `(0,0)` and moves around via `>` `<` `^` `v`.  

---

## Core Commands
The minimal Skive experience looks like Brainfuck on a caffeine bender:  
- `>` `<` `^` `v`: Move right, left, up, down.  
- `+` `-`: Increment/decrement current cell. Wraps at 255→0 and 0→255.  
- `|` `_`: Increment entire column/row. (*Why? Because chaos.*)  
- `~`: Randomize current cell with a random 8-bit value.  

---

## I/O and Loops
- `.` prints the current cell’s value as an ASCII character.  
  - Bonus form: `.(++++)` prints the same character multiple times.  
- `?` reads a single character from input.  
- `??` slurps the entire input into adjacent cells.  
- `[` `]`: Classic Brainfuck-style loop. `[ ... ]` repeats until the current cell is 0.  

---

## Labels and Teleportation
- `label{name}` marks the current cell with a name.  
- `\tp{name}` teleports the pointer to a labeled cell.  
- `\tp(x,y)` teleports *relative* to the current position (x right, y up).  

---

## Extended Commands (backslash syntax)
Skive leans hard into meta-powers:  
- `\clone{name}`: Copy current cell value into the labeled cell.  
- `\swap{name}`: Swap values with a labeled cell.  
- `\save{name}` and `\restore{name}`: Snapshot the entire memory grid and restore it later.  
- `\fill{src,corner1,corner2}`: Fill a rectangle with the value from the `src` label.  
- `\math{name,+,N}`: Do arithmetic on a labeled cell. Operations: `+ - * /`.  
- `\vset{name}`: Create a named vector.  
- `\pb(vec){label}`: Push the value of a labeled cell into a vector.  
- `\mod{name,N}`: Apply modulus to a labeled cell’s value.  

---

## Macros
Skive lets you define text macros in the program file:  
(foo)={++>++}
foofoofoo

expands into `++>++ ++>++ ++>++` before execution. Macros can expand up to 50 times (to prevent recursive doom spirals).  

---

## Comments
Comments are wrapped in triple backticks:
this is ignored

---

## Snapshots and Vectors
Because one memory grid is never enough:  
- Snapshots let you rewind your entire program state to an earlier point (useful for self-replicators and games).  
- Vectors are just Skive’s way of letting you shove a sequence of integers into a bucket for later.  

---

## Output Guide
Typing `!guide!` mid-program will dump the Skive cheat-sheet.  
(Yes, the interpreter literally has a built-in help screen because even it knows this language is nonsense.)  

---

## Why would you do this?
Skive is designed for:  
- People who like **puzzles**  
- People who want a hybrid of Brainfuck and Befunge but with an extra layer of chaos  
- People who just want to watch the world burn
