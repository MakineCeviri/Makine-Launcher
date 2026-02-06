# MakineAI Test Fixture - Ren'Py Script
# This is a minimal Ren'Py script for testing string extraction

define e = Character("Eileen", color="#c8ffc8")
define m = Character("Me", color="#ffffff")

label start:
    scene black

    "Welcome to the test game!"

    show eileen happy

    e "Hello there! My name is Eileen."
    e "I'm here to help you test the translation system."

    menu:
        "What would you like to do?"

        "Learn about the game":
            e "This is a test fixture for MakineAI."
            e "It contains sample dialogue for testing."
            jump learn_more

        "Start the adventure":
            e "Great! Let's begin our journey."
            jump adventure

        "Exit":
            e "Goodbye! See you next time."
            return

label learn_more:
    e "The MakineAI system can extract strings from Ren'Py games."
    e "It supports both .rpy and compiled .rpyc files."

    "You've learned about the system."

    jump start

label adventure:
    scene forest

    "You find yourself in a mysterious forest."

    e "Be careful! There might be dangers ahead."

    m "I'll do my best!"

    menu:
        "Which path do you take?"

        "The bright path":
            "You chose the safe route."
            e "Good choice! Safety first."

        "The dark path":
            "You chose the dangerous route."
            e "Brave, but risky!"

    "The adventure continues..."

    e "That's all for this test. Thank you for playing!"

    return
