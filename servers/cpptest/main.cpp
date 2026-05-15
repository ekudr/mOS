
class TestClass
{
private:
    int x;
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


int main()
{
    auto t = new TestClass();
}