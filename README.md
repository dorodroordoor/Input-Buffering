# Input Buffer (Geode mod)

For every click AND release (any gamemode):
1. Tick 0 = the tick your input would normally land on.
   Simulate it there and look N ticks ahead (default 2). Survive -> click normally.
2. If you'd die, try the same input on tick 1, then tick 2 (max delay, default 2),
   each time looking N ticks ahead. First one that survives = input waits until that tick.
3. If none survive, click on tick 0 like normal.

Raise "Check ticks after input" for a more cautious check (useful for cube/ball/spider).
Build: install Geode SDK + CLI, set GEODE_SDK, run `geode build`.
