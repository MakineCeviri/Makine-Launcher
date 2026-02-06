MakineAI Test Fixture - Unity Game

This is a minimal Unity game structure for testing.
Binary files (DLLs) contain only marker signatures, not actual code.

Structure:
- TestGame.exe - Placeholder executable
- UnityPlayer.dll - Unity engine marker (MZ header)
- mono.dll - Mono runtime marker (indicates Mono build)
- TestGame_Data/
  - globalgamemanagers - Unity data marker
  - Managed/
    - Assembly-CSharp.dll - Game code marker (MZ header)

Note: For testing IL2CPP games, create a separate fixture with:
- GameAssembly.dll instead of Managed/
- TestGame_Data/il2cpp_data/Metadata/global-metadata.dat

The integration tests create these files programmatically for more
precise control over the binary content.
