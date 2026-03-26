MakineAI Test Fixture - GameMaker Game

This is a minimal GameMaker game structure for testing.

Structure:
- data.win - GameMaker data file (IFF/FORM format)

The data.win file uses GameMaker's IFF format:
- FORM header (magic + size)
- GEN8 chunk (game metadata)
- STRG chunk (string table)
- Other chunks (TXTR, SPRT, etc. - not included in minimal fixture)

Note: The integration tests create data.win files programmatically
with controlled content for precise testing of:
- FORM header validation
- Chunk parsing
- String extraction
- Binary patching

For testing with real games:
- Undertale (Windows) has a well-documented data.win
- DELTARUNE Chapter 1 is also a good test candidate

The test creates minimal IFF files with sample strings like:
- "Hello World"
- "Game Over"
- "Press Start"
- etc.
