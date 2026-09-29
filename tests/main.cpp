#include <juce_core/juce_core.h>
#include <iostream>

namespace
{
    struct ConsoleRunner : juce::UnitTestRunner
    {
        void logMessage (const juce::String& message) override { std::cout << message << std::endl; }
    };
}

// Usage: AkwardFreQTests [name-filter]      e.g. AkwardFreQTests LoopSnapper
// Exit code: 0 all passed, 1 a check failed, 2 nothing ran (which must never count as a pass).
int main (int argc, char* argv[])
{
    const juce::String filter = argc > 1 ? juce::String (argv[1]) : juce::String();

    juce::Array<juce::UnitTest*> chosen;
    for (auto* test : juce::UnitTest::getAllTests())
        if (filter.isEmpty() || test->getName().containsIgnoreCase (filter))
            chosen.add (test);

    if (chosen.isEmpty())
    {
        std::cerr << "No tests matched \"" << filter << "\"" << std::endl;
        return 2;
    }

    ConsoleRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTests (chosen);

    int passes = 0, failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        passes += runner.getResult (i)->passes;
        failures += runner.getResult (i)->failures;
    }

    std::cout << "\n" << chosen.size() << " test groups: " << passes << " checks passed, " << failures << " failed" << std::endl;

    if (passes + failures == 0) return 2;
    return failures > 0 ? 1 : 0;
}
