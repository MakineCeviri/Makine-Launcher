# Makine-Launcher Test Fixtures

This directory contains sample game files for integration testing.

## Directory Structure

```
fixtures/
├── sample_renpy_game/        # Minimal Ren'Py game
│   ├── game/
│   │   └── script.rpy
│   └── renpy/
│       └── __init__.py
│
├── sample_rpgmaker_game/     # Minimal RPG Maker MV game
│   ├── package.json
│   └── www/data/
│       ├── System.json
│       ├── Map001.json
│       └── Actors.json
│
├── sample_unity_game/        # Minimal Unity structure (markers only)
│   ├── TestGame.exe          # Placeholder
│   ├── UnityPlayer.dll       # Marker file
│   └── TestGame_Data/
│       ├── Managed/
│       │   └── Assembly-CSharp.dll
│       └── globalgamemanagers
│
└── sample_gamemaker_game/    # Minimal GameMaker structure
    └── data.win              # Minimal IFF/FORM file
```

## Usage

These fixtures are used by integration tests to verify:

1. **Engine Detection** - Can we correctly identify each game engine?
2. **String Extraction** - Can we extract translatable strings?
3. **Translation Application** - Can we apply translations correctly?
4. **Backup/Restore** - Does the backup system work?

## Important Notes

- These are **minimal** fixtures for testing, not real games
- Binary files (DLLs, data.win) contain only marker signatures
- Real game testing requires actual game files (not included)

## Creating Real Test Fixtures

For comprehensive testing with real games:

1. Create a `fixtures_real/` directory (gitignored)
2. Copy actual game folders there
3. Set `MAKINE_TEST_FIXTURES_DIR` environment variable
4. Run integration tests

```bash
# Example
set MAKINE_TEST_FIXTURES_DIR=C:\path\to\fixtures_real
ctest -R integration
```

## Adding New Fixtures

When adding a new engine fixture:

1. Create minimal directory structure
2. Include signature/marker files
3. Add sample translatable content
4. Update this README
5. Add corresponding integration test

## License

Test fixtures are created for testing purposes only.
They do not contain any copyrighted game content.
