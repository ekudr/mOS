
#include <common.h>

class TestClass
{
private:
    int x[100];
public:
    TestClass(/* args */);
    ~TestClass();
};

TestClass::TestClass(/* args */)
{
}

TestClass::~TestClass()
{
}

void test_mem();

int main()
{
    debug("Cpp Test App\n");

    test_mem();
//     auto t = new TestClass();
// //    debug("_______Allocated class 0x%X bytes at 0x%lX\n", sizeof(t), t);
//     auto t1 = new TestClass();
// //    debug("_______Allocated class 0x%X bytes at 0x%lX\n", sizeof(t1), t1);
//     auto t2 = new TestClass();
// //    debug("_______Allocated class 0x%X bytes at 0x%lX\n", sizeof(t2), t2);

//     delete t;
//     delete t1;
//     delete t2;
}