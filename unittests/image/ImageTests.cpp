#include "pch.h"

// Class to be tested
#include "image\Image.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// ----------------------------------------------------------------------------
// ImageTests
// ----------------------------------------------------------------------------
TEST_CLASS(ImageTests)
{
private:
public:
    //TEST_CLASS_INITIALIZE(ClassSetup) {}
    //TEST_CLASS_CLEANUP(ClassTeardown) {}
    //TEST_METHOD_INITIALIZE(MethodSetup) {}
    //TEST_METHOD_CLEANUP(MethodTeardown) {}
    TEST_METHOD(MakeImageFromRawBuffer) 
    {
        size_t w{ 1 }, h{ 2 }, n_ch{ 3 };
        std::filesystem::path path{ "some_path" };
        size_t byte_count = w * h * n_ch;
        std::vector<unsigned char> ref_vec(byte_count, 'a');
        const unsigned char* test_data = ref_vec.data();
        Image result = makeImageFromRawBuffer(w, h, n_ch, path, test_data);

        // Check wiring.
        Assert::AreEqual(static_cast<std::size_t>(1), result.width);
        Assert::AreEqual(static_cast<std::size_t>(2), result.height);
        Assert::AreEqual(static_cast<std::size_t>(3), result.channels);
        Assert::AreEqual(std::string("some_path"), result.source_path.string());
        Assert::IsTrue(ref_vec == result.pixels);
    }
};