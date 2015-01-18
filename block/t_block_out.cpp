#include <block/block_out.h>

#include <stdio.h>

struct UserType1
{
    int d_i;
    double d_d;
};

class UserType2
{
    int d_i;
    double d_d;
public:
    UserType2() {}
    ~UserType2() {}

};

class Source
{
public:
    block::out<void()> out0;
    block::out<void(const char*)> out1;
    block::out<void(const char*, int)> out2;
    block::out<void(const char*, int, char)> out3;
    block::out<void(const char*, int, char, float)> out4;
    block::out<void(const char*, int, char, float, double)> out5;
    block::out<void(const char*, int, char, float, double, void *)> out6;
    block::out<void(const char*, int, char, float, double, void *, UserType1)> out7;
    block::out<void(const char*, int, char, float, double, void *, UserType1, UserType2)> out8;

    // not connected
    block::out<void(int, double)> out100;
    block::out<void(const int &, double)> out101;
    block::out<void(int, const double&)> out102;
    block::out<int(int, double)> out103;
    block::out<void(int&, double)> out104;
    block::out<void(int, double&)> out105;

    // connected to mulitple inports

    block::out<void(int, double)> out200;
    block::out<void(const int &, double)> out201;
    block::out<void(int, const double&)> out202;
    block::out<int(int, double)> out203;
    block::out<void(int&, double)> out204;
    block::out<void(int, double&)> out205;

    void Run()
    {
        out0();
        out1("Hello World");
        out2("Hello World", 1);
        out3("Hello World", 1, '2');
        out4("Hello World", 1, '2', 3.0f);
        out5("Hello World", 1, '2', 3.0f, 4.0);
        out6("Hello World", 1, '2', 3.0f, 4.0, 0);
        out7("Hello World", 1, '2', 3.0f, 4.0, 0, UserType1());
        out8("Hello World", 1, '2', 3.0f, 4.0, 0, UserType1(), UserType2());

        int i = 1;
        double d = 2.0;
        out100(1, 2.0);
        out101(1, 2.0);
        out102(1, 2.0);
        //out103(1, 2.0);
        out104(i, d);
        out105(i, d);

        out200(1, 2.0);
        out201(1, 2.0);
        out202(1, 2.0);
        out203(1, 2.0);
        out204(i, d);
        out205(i, d);
    }
};

struct Dummy 
{
    int d_i;
    int d_double;
};

class Sink : public Dummy
{
private:
    mutable int d_numCalls;
public:

    Sink()
        : d_numCalls(0)
    {
    }

    int numCalls() const { return d_numCalls; }

    void in0()
    {
            ++d_numCalls;
    }
    void in1(const char*) const
    {
        ++d_numCalls;
    }
    void in2(const char*, int)
    {
        ++d_numCalls;
    }
    void in3(const char*, int, char) const
    {
        ++d_numCalls;
    }
    void in4(const char*, int, char, float)
    {
        ++d_numCalls;
    }
    void in5(const char*, int, char, float, double) const
    {
        ++d_numCalls;
    }
    void in6(const char*, int, char, float, double, void *)
    {
        ++d_numCalls;
    }
    void in7(const char*, int, char, float, double, void *, UserType1) const
    {
        ++d_numCalls;
    }
    void in8(const char*, int, char, float, double, void *, UserType1, UserType2)
    {
        ++d_numCalls;
    }

    void in200(int, double)
    {
        ++d_numCalls;
    }
    void in201(const int&, double)
    {
        ++d_numCalls;
    }
    void in202(int, const double&)
    {
        ++d_numCalls;
    }
    int  in203(int, double)
    {
        ++d_numCalls;
        return d_numCalls;
    }
    void in204(int&, double)
    {
        ++d_numCalls;
    }
    void in205(int, double&)
    {
        ++d_numCalls;
    }
};

int main(int argc, char *argv[])
{
    {
        Source source;
        Sink sink;

        source.out0.connect(&sink, &Sink::in0);
        source.out1.connect(&sink, &Sink::in1);
        source.out2.connect(&sink, &Sink::in2);
        source.out3.connect(&sink, &Sink::in3);
        source.out4.connect(&sink, &Sink::in4);
        source.out5.connect(&sink, &Sink::in5);
        source.out6.connect(&sink, &Sink::in6);
        source.out7.connect(&sink, &Sink::in7);
        source.out8.connect(&sink, &Sink::in8);

        source.out200.connect(&sink, &Sink::in200);
        source.out200.connect(&sink, &Sink::in200);
        source.out201.connect(&sink, &Sink::in201);
        source.out201.connect(&sink, &Sink::in201);
        source.out202.connect(&sink, &Sink::in202);
        source.out202.connect(&sink, &Sink::in202);
        source.out203.connect(&sink, &Sink::in203);
        source.out203.connect(&sink, &Sink::in203);
        source.out204.connect(&sink, &Sink::in204);
        source.out204.connect(&sink, &Sink::in204);
        source.out205.connect(&sink, &Sink::in205);
        source.out205.connect(&sink, &Sink::in205);

        source.Run();

        printf("Member functions of Sink have been called %d times\n", sink.numCalls());
    }

    return 0;
}
