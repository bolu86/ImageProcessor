#include "pch.h"

// Class to be tested
#include "image\ImageIO.h" 

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// ----------------------------------------------------------------------------
// ImageIOTests
// ----------------------------------------------------------------------------
TEST_CLASS(ImageIOTests)
{
private:
public:
    //TEST_CLASS_INITIALIZE(ClassSetup) {}
    //TEST_CLASS_CLEANUP(ClassTeardown) {}
    //TEST_METHOD_INITIALIZE(MethodSetup) {}
    //TEST_METHOD_CLEANUP(MethodTeardown) {}

    TEST_METHOD(LoadThenSaveRoundTripPreservesDimensions)
    {
        // Create a temporary folder for the test.
        TemporaryTestFolder temp_folder;

        auto checkAndGet = [](std::variant<Image, LoadError> im) {
            Assert::IsTrue(std::holds_alternative<Image>(im));
            return std::get<Image>(im);
        };

        // Test loading a valid image.
        auto loaded = ImageIO::load(globalTestFile("image0.png").string());
        Image loaded_image = checkAndGet(loaded);

        // Test saving the loaded image.
        auto saved_path = temp_folder.path() / "saved.png";
        bool saved = ImageIO::savePng(loaded_image, saved_path.string());
        Assert::IsTrue(saved);

        // Test reloading the saved image.
        auto reloaded = ImageIO::load(saved_path.string());
        Image reloaded_image = checkAndGet(reloaded);

        // Check the reloaded image by comparing with the initial loaded image.
        Assert::AreEqual(loaded_image.width, reloaded_image.width);
        Assert::AreEqual(loaded_image.height, reloaded_image.height);
    }

    TEST_METHOD(LoadNonexistentFileReturnsNullopt)
    {
        std::string path = "this_file_does_not_exist.png";

        // Test loading a non-existant file.
        auto loaded = ImageIO::load(path);
        Assert::IsTrue(std::holds_alternative<LoadError>(loaded));

        // Check the LoadError object.
        LoadError err = std::get<LoadError>(loaded);
        Assert::AreEqual(path, err.path);
        Assert::AreEqual(std::string("can't fopen"), err.reason);
    }
};