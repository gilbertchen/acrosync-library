#ifndef INCLUDED_BLOCK_OUT_H
#define INCLUDED_BLOCK_OUT_H

namespace block {

namespace impl {

class NullType;

template <class Prototype>
class PrototypeTrait
{
};

class InportBase;

template <class RT>
struct PrototypeTrait<RT()>
{
    typedef RT ReturnType;
    typedef NullType T1;
    typedef NullType T2;
    typedef NullType T3;
    typedef NullType T4;
    typedef NullType T5;
    typedef NullType T6;
    typedef NullType T7;
    typedef NullType T8;
    typedef RT InvokerPrototype(const InportBase*);
};

template <class RT, class A1>
struct PrototypeTrait<RT(A1)>
{
    typedef RT ReturnType;
    typedef A1 T1;
    typedef NullType T2;
    typedef NullType T3;
    typedef NullType T4;
    typedef NullType T5;
    typedef NullType T6;
    typedef NullType T7;
    typedef NullType T8;
    typedef RT InvokerPrototype(const InportBase*, A1);
};

template <class RT, class A1, class A2>
struct PrototypeTrait<RT(A1, A2)>
{
    typedef RT ReturnType;
    typedef A1 T1;
    typedef A2 T2;
    typedef NullType T3;
    typedef NullType T4;
    typedef NullType T5;
    typedef NullType T6;
    typedef NullType T7;
    typedef NullType T8;
    typedef RT InvokerPrototype(const InportBase*, A1, A2);
};

template <class RT, class A1, class A2, class A3>
struct PrototypeTrait<RT(A1, A2, A3)>
{
    typedef RT ReturnType;
    typedef A1 T1;
    typedef A2 T2;
    typedef A3 T3;
    typedef NullType T4;
    typedef NullType T5;
    typedef NullType T6;
    typedef NullType T7;
    typedef NullType T8;
    typedef RT InvokerPrototype(const InportBase*, A1, A2, A3);
};

template <class RT, class A1, class A2, class A3, class A4>
struct PrototypeTrait<RT(A1, A2, A3, A4)>
{
    typedef RT ReturnType;
    typedef A1 T1;
    typedef A2 T2;
    typedef A3 T3;
    typedef A4 T4;
    typedef NullType T5;
    typedef NullType T6;
    typedef NullType T7;
    typedef NullType T8;
    typedef RT InvokerPrototype(const InportBase*, A1, A2, A3, A4);
};

template <class RT, class A1, class A2, class A3, class A4, class A5>
struct PrototypeTrait<RT(A1, A2, A3, A4, A5)>
{
    typedef RT ReturnType;
    typedef A1 T1;
    typedef A2 T2;
    typedef A3 T3;
    typedef A4 T4;
    typedef A5 T5;
    typedef NullType T6;
    typedef NullType T7;
    typedef NullType T8;
    typedef RT InvokerPrototype(const InportBase*, A1, A2, A3, A4, A5);
};

template <class RT, class A1, class A2, class A3, class A4, class A5, class A6>
struct PrototypeTrait<RT(A1, A2, A3, A4, A5, A6)>
{
    typedef RT ReturnType;
    typedef A1 T1;
    typedef A2 T2;
    typedef A3 T3;
    typedef A4 T4;
    typedef A5 T5;
    typedef A6 T6;
    typedef NullType T7;
    typedef NullType T8;
    typedef RT InvokerPrototype(const InportBase*, A1, A2, A3, A4, A5, A6);
};

template <class RT, class A1, class A2, class A3, class A4, class A5, class A6, class A7>
struct PrototypeTrait<RT(A1, A2, A3, A4, A5, A6, A7)>
{
    typedef RT ReturnType;
    typedef A1 T1;
    typedef A2 T2;
    typedef A3 T3;
    typedef A4 T4;
    typedef A5 T5;
    typedef A6 T6;
    typedef A7 T7;
    typedef NullType T8;
    typedef RT InvokerPrototype(const InportBase*, A1, A2, A3, A4, A5, A6, A7);
};

template <class RT, class A1, class A2, class A3, class A4, class A5, class A6, class A7, class A8>
struct PrototypeTrait<RT(A1, A2, A3, A4, A5, A6, A7, A8)>
{
    typedef RT ReturnType;
    typedef A1 T1;
    typedef A2 T2;
    typedef A3 T3;
    typedef A4 T4;
    typedef A5 T5;
    typedef A6 T6;
    typedef A7 T7;
    typedef A8 T8;
    typedef RT InvokerPrototype(const InportBase*, A1, A2, A3, A4, A5, A6, A7, A8);
};

template <class ObjectType, class Prototype>
class MemberFunctionTrait
{
};

template <class ObjectType, class RT>
struct MemberFunctionTrait<ObjectType, RT()>
{
    typedef RT (ObjectType::*MemberFunction)();
    typedef RT (ObjectType::*ConstMemberFunction)() const;
};

template <class ObjectType, class RT, class A1>
struct MemberFunctionTrait<ObjectType, RT(A1)>
{
    typedef RT (ObjectType::*MemberFunction)(A1);
    typedef RT (ObjectType::*ConstMemberFunction)(A1) const;
};

template <class ObjectType, class RT, class A1, class A2>
struct MemberFunctionTrait<ObjectType, RT(A1, A2)>
{
    typedef RT (ObjectType::*MemberFunction)(A1, A2);
    typedef RT (ObjectType::*ConstMemberFunction)(A1, A2) const;
};

template <class ObjectType, class RT, class A1, class A2, class A3>
struct MemberFunctionTrait<ObjectType, RT(A1, A2, A3)>
{
    typedef RT (ObjectType::*MemberFunction)(A1, A2, A3);
    typedef RT (ObjectType::*ConstMemberFunction)(A1, A2, A3) const;
};

template <class ObjectType, class RT, class A1, class A2, class A3, class A4>
struct MemberFunctionTrait<ObjectType, RT(A1, A2, A3, A4)>
{
    typedef RT (ObjectType::*MemberFunction)(A1, A2, A3, A4);
    typedef RT (ObjectType::*ConstMemberFunction)(A1, A2, A3, A4) const;
};

template <class ObjectType, class RT, class A1, class A2, class A3, class A4, class A5>
struct MemberFunctionTrait<ObjectType, RT(A1, A2, A3, A4, A5)>
{
    typedef RT (ObjectType::*MemberFunction)(A1, A2, A3, A4, A5);
    typedef RT (ObjectType::*ConstMemberFunction)(A1, A2, A3, A4, A5) const;
};

template <class ObjectType, class RT, class A1, class A2, class A3, class A4, class A5, class A6>
struct MemberFunctionTrait<ObjectType, RT(A1, A2, A3, A4, A5, A6)>
{
    typedef RT (ObjectType::*MemberFunction)(A1, A2, A3, A4, A5, A6);
    typedef RT (ObjectType::*ConstMemberFunction)(A1, A2, A3, A4, A5, A6) const;
};

template <class ObjectType, class RT, class A1, class A2, class A3, class A4, class A5, class A6, class A7>
struct MemberFunctionTrait<ObjectType, RT(A1, A2, A3, A4, A5, A6, A7)>
{
    typedef RT (ObjectType::*MemberFunction)(A1, A2, A3, A4, A5, A6, A7);
    typedef RT (ObjectType::*ConstMemberFunction)(A1, A2, A3, A4, A5, A6, A7) const;
};

template <class ObjectType, class RT, class A1, class A2, class A3, class A4, class A5, class A6, class A7, class A8>
struct MemberFunctionTrait<ObjectType, RT(A1, A2, A3, A4, A5, A6, A7, A8)>
{
    typedef RT (ObjectType::*MemberFunction)(A1, A2, A3, A4, A5, A6, A7, A8);
    typedef RT (ObjectType::*ConstMemberFunction)(A1, A2, A3, A4, A5, A6, A7, A8) const;
};

// The following specialization allows extracting the prototype from a given member function.
template <class ObjectType, class RT>
struct MemberFunctionTrait<ObjectType, RT (ObjectType::*)()>
{
    typedef RT Prototype();
};

template <class ObjectType, class RT, class A1>
struct MemberFunctionTrait<ObjectType, RT (ObjectType::*)(A1)>
{
    typedef RT Prototype(A1);
};

template <class ObjectType, class RT, class A1, class A2>
struct MemberFunctionTrait<ObjectType, RT (ObjectType::*)(A1, A2)>
{
    typedef RT Prototype(A1, A2);
};

template <class ObjectType, class RT, class A1, class A2, class A3>
struct MemberFunctionTrait<ObjectType, RT (ObjectType::*)(A1, A2, A3)>
{
    typedef RT Prototype(A1, A2, A3);
};

template <class ObjectType, class RT, class A1, class A2, class A3, class A4>
struct MemberFunctionTrait<ObjectType, RT (ObjectType::*)(A1, A2, A3, A4)>
{
    typedef RT Prototype(A1, A2, A3, A4);
};

template <class ObjectType, class RT, class A1, class A2, class A3, class A4, class A5>
struct MemberFunctionTrait<ObjectType, RT (ObjectType::*)(A1, A2, A3, A4, A5)>
{
    typedef RT Prototype(A1, A2, A3, A4, A5);
};

template <class ObjectType, class RT, class A1, class A2, class A3, class A4, class A5, class A6>
struct MemberFunctionTrait<ObjectType, RT (ObjectType::*)(A1, A2, A3, A4, A5, A6)>
{
    typedef RT Prototype(A1, A2, A3, A4, A5, A6);
};

template <class ObjectType, class RT, class A1, class A2, class A3, class A4, class A5, class A6, class A7>
struct MemberFunctionTrait<ObjectType, RT (ObjectType::*)(A1, A2, A3, A4, A5, A6, A7)>
{
    typedef RT Prototype(A1, A2, A3, A4, A5, A6, A7);
};

template <class ObjectType, class RT, class A1, class A2, class A3, class A4, class A5, class A6, class A7, class A8>
struct MemberFunctionTrait<ObjectType, RT (ObjectType::*)(A1, A2, A3, A4, A5, A6, A7, A8)>
{
    typedef RT Prototype(A1, A2, A3, A4, A5, A6, A7, A8);
};

// Same as above, except for const member functions.
template <class ObjectType, class RT>
struct MemberFunctionTrait<ObjectType, RT (ObjectType::*)() const>
{
    typedef RT Prototype();
};

template <class ObjectType, class RT, class A1>
struct MemberFunctionTrait<ObjectType, RT (ObjectType::*)(A1) const>
{
    typedef RT Prototype(A1);
};

template <class ObjectType, class RT, class A1, class A2>
struct MemberFunctionTrait<ObjectType, RT (ObjectType::*)(A1, A2) const>
{
    typedef RT Prototype(A1, A2);
};

template <class ObjectType, class RT, class A1, class A2, class A3>
struct MemberFunctionTrait<ObjectType, RT (ObjectType::*)(A1, A2, A3) const>
{
    typedef RT Prototype(A1, A2, A3);
};

template <class ObjectType, class RT, class A1, class A2, class A3, class A4>
struct MemberFunctionTrait<ObjectType, RT (ObjectType::*)(A1, A2, A3, A4) const>
{
    typedef RT Prototype(A1, A2, A3, A4);
};

template <class ObjectType, class RT, class A1, class A2, class A3, class A4, class A5>
struct MemberFunctionTrait<ObjectType, RT (ObjectType::*)(A1, A2, A3, A4, A5) const>
{
    typedef RT Prototype(A1, A2, A3, A4, A5);
};

template <class ObjectType, class RT, class A1, class A2, class A3, class A4, class A5, class A6>
struct MemberFunctionTrait<ObjectType, RT (ObjectType::*)(A1, A2, A3, A4, A5, A6) const>
{
    typedef RT Prototype(A1, A2, A3, A4, A5, A6);
};

template <class ObjectType, class RT, class A1, class A2, class A3, class A4, class A5, class A6, class A7>
struct MemberFunctionTrait<ObjectType, RT (ObjectType::*)(A1, A2, A3, A4, A5, A6, A7) const>
{
    typedef RT Prototype(A1, A2, A3, A4, A5, A6, A7);
};

template <class ObjectType, class RT, class A1, class A2, class A3, class A4, class A5, class A6, class A7, class A8>
struct MemberFunctionTrait<ObjectType, RT (ObjectType::*)(A1, A2, A3, A4, A5, A6, A7, A8) const>
{
    typedef RT Prototype(A1, A2, A3, A4, A5, A6, A7, A8);
};

template <class ObjectType, class Prototype, bool IsConst>
struct ConstMemberFunctionTrait
{
    typedef typename MemberFunctionTrait<ObjectType, Prototype>::MemberFunction MemberFunction;
    typedef ObjectType * ObjectPointer;
};

template <class ObjectType, class Prototype>
struct ConstMemberFunctionTrait<ObjectType, Prototype, true>
{
    typedef typename MemberFunctionTrait<ObjectType, Prototype>::ConstMemberFunction MemberFunction;
    typedef const ObjectType * ObjectPointer;
};

union UniversalMemberFunction
{
    class VirtualBase
    {
    public:
        virtual int function1() { return 1; };
    };

    class NonVirtualBase
    {
    public: 
        int function2() { return 2; };
    };

    class SingleDerived: public VirtualBase
    {
    public: 
        int function3() { return 3; };
    };

    class MultipleDerived: public VirtualBase, public NonVirtualBase
    {
     public: 
        int function4() { return 4; };
    };

    class ForwardDeclared;

    int (VirtualBase::*d_memberFunction1)();
    int (NonVirtualBase::*d_memberFunction2)();
    int (SingleDerived::*d_memberFunction3)();
    int (MultipleDerived::*d_memberFunction4)();
    int (ForwardDeclared::*d_memberFunction5)();
    int *d_dataPointer;
    int (*d_functionPointer)();
    int d_int;
};

template <class Prototype, class ReturnType>
class OutportInvoker;


// A base class for all inports; it has a virtual destructor so that derived
// inport objects can be properly deleted with a base pointer.
class InportBase
{
public:
    InportBase() {}
    virtual ~InportBase() {}
};

template <class Prototype>
class Inport : public InportBase
{
    typedef typename PrototypeTrait<Prototype>::ReturnType       ReturnType;
    typedef typename PrototypeTrait<Prototype>::T1               T1;
    typedef typename PrototypeTrait<Prototype>::T2               T2;
    typedef typename PrototypeTrait<Prototype>::T3               T3;
    typedef typename PrototypeTrait<Prototype>::T4               T4;
    typedef typename PrototypeTrait<Prototype>::T5               T5;
    typedef typename PrototypeTrait<Prototype>::T6               T6;
    typedef typename PrototypeTrait<Prototype>::T7               T7;
    typedef typename PrototypeTrait<Prototype>::T8               T8;
    typedef typename PrototypeTrait<Prototype>::InvokerPrototype InvokerPrototype;

    template <class Prototype2, class ReturnType2 > friend class OutportInvoker;

    void                    *d_object;
    UniversalMemberFunction  d_memberFunction;
    InvokerPrototype        *d_invoker;

    // NOT IMPLEMENTED
    Inport(const Inport&);
    Inport &operator=(const Inport&);
public:
    template <class ObjectType>
    Inport(ObjectType *object, typename MemberFunctionTrait<ObjectType, Prototype>::MemberFunction memberFunction)
    {
        typedef typename MemberFunctionTrait<ObjectType, Prototype>::MemberFunction MemberFunction;
        d_object = object;
        *reinterpret_cast<MemberFunction*>(&d_memberFunction) = memberFunction;
        d_invoker = &Inport::template invoker<ObjectType, false>;
    }

    template <class ObjectType>
    Inport(const ObjectType *object, typename MemberFunctionTrait<ObjectType, Prototype>::ConstMemberFunction memberFunction)
    {
        typedef typename MemberFunctionTrait<ObjectType, Prototype>::ConstMemberFunction MemberFunction;
        d_object = const_cast<ObjectType*>(object);
        *reinterpret_cast<MemberFunction*>(&d_memberFunction) = memberFunction;
        d_invoker = &Inport::template invoker<ObjectType, true>;
    }

    ~Inport()
    {
    }

private:
    ReturnType operator()() const
    {
        return (*d_invoker)(this);
    }

    ReturnType operator()(T1 a1) const
    {
        return (*d_invoker)(this, a1);
    }

    ReturnType operator()(T1 a1, T2 a2) const
    {
        return (*d_invoker)(this, a1, a2);
    }

    ReturnType operator()(T1 a1, T2 a2, T3 a3) const
    {
        return (*d_invoker)(this, a1, a2, a3);
    }

    ReturnType operator()(T1 a1, T2 a2, T3 a3, T4 a4) const
    {
        return (*d_invoker)(this, a1, a2, a3, a4);
    }

    ReturnType operator()(T1 a1, T2 a2, T3 a3, T4 a4, T5 a5) const
    {
        return (*d_invoker)(this, a1, a2, a3, a4, a5);
    }

    ReturnType operator()(T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6) const
    {
        return (*d_invoker)(this, a1, a2, a3, a4, a5, a6);
    }

    ReturnType operator()(T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6, T7 a7) const
    {
        return (*d_invoker)(this, a1, a2, a3, a4, a5, a6, a7);
    }

    ReturnType operator()(T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6, T7 a7, T8 a8) const
    {
        return (*d_invoker)(this, a1, a2, a3, a4, a5, a6, a7, a8);
    }

    template <class ObjectType, bool IsConst>
    static ReturnType invoker(const InportBase *inportBase)
    {
        const Inport *inport = dynamic_cast<const Inport*>(inportBase);
        typedef typename ConstMemberFunctionTrait<ObjectType, Prototype, IsConst>::MemberFunction MemberFunction;
        typedef typename ConstMemberFunctionTrait<ObjectType, Prototype, IsConst>::ObjectPointer ObjectPointer;
        ObjectPointer object = static_cast<ObjectPointer>(inport->d_object);
        MemberFunction memberFunction = *reinterpret_cast<const MemberFunction*>(&inport->d_memberFunction);
	    return (object->*memberFunction)();
    }
    template <class ObjectType, bool IsConst>
    static ReturnType invoker(const InportBase *inportBase, T1 a1)
    {
        const Inport *inport = dynamic_cast<const Inport*>(inportBase);
        typedef typename ConstMemberFunctionTrait<ObjectType, Prototype, IsConst>::MemberFunction MemberFunction;
        typedef typename ConstMemberFunctionTrait<ObjectType, Prototype, IsConst>::ObjectPointer ObjectPointer;
        ObjectPointer object = static_cast<ObjectPointer>(inport->d_object);
        MemberFunction memberFunction = *reinterpret_cast<const MemberFunction*>(&inport->d_memberFunction);
	    return (object->*memberFunction)(a1);
    }
    template <class ObjectType, bool IsConst>
    static ReturnType invoker(const InportBase *inportBase, T1 a1, T2 a2)
    {
        const Inport *inport = dynamic_cast<const Inport*>(inportBase);
        typedef typename ConstMemberFunctionTrait<ObjectType, Prototype, IsConst>::MemberFunction MemberFunction;
        typedef typename ConstMemberFunctionTrait<ObjectType, Prototype, IsConst>::ObjectPointer ObjectPointer;
        ObjectPointer object = static_cast<ObjectPointer>(inport->d_object);
        MemberFunction memberFunction = *reinterpret_cast<const MemberFunction*>(&inport->d_memberFunction);
	    return (object->*memberFunction)(a1, a2);
    }
    template <class ObjectType, bool IsConst>
    static ReturnType invoker(const InportBase *inportBase, T1 a1, T2 a2, T3 a3)
    {
        const Inport *inport = dynamic_cast<const Inport*>(inportBase);
        typedef typename ConstMemberFunctionTrait<ObjectType, Prototype, IsConst>::MemberFunction MemberFunction;
        typedef typename ConstMemberFunctionTrait<ObjectType, Prototype, IsConst>::ObjectPointer ObjectPointer;
        ObjectPointer object = static_cast<ObjectPointer>(inport->d_object);
        MemberFunction memberFunction = *reinterpret_cast<const MemberFunction*>(&inport->d_memberFunction);
	    return (object->*memberFunction)(a1, a2, a3);
    }
    template <class ObjectType, bool IsConst>
    static ReturnType invoker(const InportBase *inportBase, T1 a1, T2 a2, T3 a3, T4 a4)
    {
        const Inport *inport = dynamic_cast<const Inport*>(inportBase);
        typedef typename ConstMemberFunctionTrait<ObjectType, Prototype, IsConst>::MemberFunction MemberFunction;
        typedef typename ConstMemberFunctionTrait<ObjectType, Prototype, IsConst>::ObjectPointer ObjectPointer;
        ObjectPointer object = static_cast<ObjectPointer>(inport->d_object);
        MemberFunction memberFunction = *reinterpret_cast<const MemberFunction*>(&inport->d_memberFunction);
	    return (object->*memberFunction)(a1, a2, a3, a4);
    }
    template <class ObjectType, bool IsConst>
    static ReturnType invoker(const InportBase *inportBase, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5)
    {
        const Inport *inport = dynamic_cast<const Inport*>(inportBase);
        typedef typename ConstMemberFunctionTrait<ObjectType, Prototype, IsConst>::MemberFunction MemberFunction;
        typedef typename ConstMemberFunctionTrait<ObjectType, Prototype, IsConst>::ObjectPointer ObjectPointer;
        ObjectPointer object = static_cast<ObjectPointer>(inport->d_object);
        MemberFunction memberFunction = *reinterpret_cast<const MemberFunction*>(&inport->d_memberFunction);
	    return (object->*memberFunction)(a1, a2, a3, a4, a5);
    }
    template <class ObjectType, bool IsConst>
    static ReturnType invoker(const InportBase *inportBase, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6)
    {
        const Inport *inport = dynamic_cast<const Inport*>(inportBase);
        typedef typename ConstMemberFunctionTrait<ObjectType, Prototype, IsConst>::MemberFunction MemberFunction;
        typedef typename ConstMemberFunctionTrait<ObjectType, Prototype, IsConst>::ObjectPointer ObjectPointer;
        ObjectPointer object = static_cast<ObjectPointer>(inport->d_object);
        MemberFunction memberFunction = *reinterpret_cast<const MemberFunction*>(&inport->d_memberFunction);
	    return (object->*memberFunction)(a1, a2, a3, a4, a5, a6);
    }
    template <class ObjectType, bool IsConst>
    static ReturnType invoker(const InportBase *inportBase, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6, T7 a7)
    {
        const Inport *inport = dynamic_cast<const Inport*>(inportBase);
        typedef typename ConstMemberFunctionTrait<ObjectType, Prototype, IsConst>::MemberFunction MemberFunction;
        typedef typename ConstMemberFunctionTrait<ObjectType, Prototype, IsConst>::ObjectPointer ObjectPointer;
        ObjectPointer object = static_cast<ObjectPointer>(inport->d_object);
        MemberFunction memberFunction = *reinterpret_cast<const MemberFunction*>(&inport->d_memberFunction);
	    return (object->*memberFunction)(a1, a2, a3, a4, a5, a6, a7);
    }
    template <class ObjectType, bool IsConst>
    static ReturnType invoker(const InportBase *inportBase, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6, T7 a7, T8 a8)
    {
        const Inport *inport = dynamic_cast<const Inport*>(inportBase);
        typedef typename ConstMemberFunctionTrait<ObjectType, Prototype, IsConst>::MemberFunction MemberFunction;
        typedef typename ConstMemberFunctionTrait<ObjectType, Prototype, IsConst>::ObjectPointer ObjectPointer;
        ObjectPointer object = static_cast<ObjectPointer>(inport->d_object);
        MemberFunction memberFunction = *reinterpret_cast<const MemberFunction*>(&inport->d_memberFunction);
	    return (object->*memberFunction)(a1, a2, a3, a4, a5, a6, a7, a8);
    }

};

struct outport_not_connected {};

template <class Prototype>
class InportDelegate : public InportBase
{
    typedef typename PrototypeTrait<Prototype>::InvokerPrototype InvokerPrototype;

    struct Element
    {
        const InportBase *d_inport;
        InvokerPrototype *d_invoker;
    };
    
    int      d_size;
    int      d_capacity;
    Element *d_elements;

    // NOT IMPLEMENTED
    InportDelegate(const InportDelegate&);
    InportDelegate& operator=(const InportDelegate&);

public:
    InportDelegate();

    ~InportDelegate();

    void push_back(const InportBase *inport, InvokerPrototype *invoker);

    const InportBase *getInport(int i) const { return d_elements[i].d_inport; }

    InvokerPrototype *getInvoker(int i) const { return d_elements[i].d_invoker; }

    int size() const { return d_size; }
};

template <class Prototype>
InportDelegate<Prototype>::InportDelegate()
    : d_size(0)
    , d_capacity(4)
    , d_elements(new Element[d_capacity])
{
}

template <class Prototype>
InportDelegate<Prototype>::~InportDelegate()
{
    delete [] d_elements;
}

template <class Prototype>
void InportDelegate<Prototype>::push_back(const InportBase *inport, InvokerPrototype *invoker)
{
    if (d_size >= d_capacity) {
        Element * elements = new Element[d_capacity * 2];
        for (int i = 0; i < d_capacity; ++i) {
            elements[i] = d_elements[i];
        }
        delete [] d_elements;
        d_elements = elements;
        d_capacity *= 2;
    }
    d_elements[d_size].d_inport = inport;
    d_elements[d_size].d_invoker = invoker;
    ++d_size;
}

template <class Prototype, class ReturnType>
class OutportInvoker
{
    typedef typename PrototypeTrait<Prototype>::T1               T1;
    typedef typename PrototypeTrait<Prototype>::T2               T2;
    typedef typename PrototypeTrait<Prototype>::T3               T3;
    typedef typename PrototypeTrait<Prototype>::T4               T4;
    typedef typename PrototypeTrait<Prototype>::T5               T5;
    typedef typename PrototypeTrait<Prototype>::T6               T6;
    typedef typename PrototypeTrait<Prototype>::T7               T7;
    typedef typename PrototypeTrait<Prototype>::T8               T8;
    typedef typename PrototypeTrait<Prototype>::InvokerPrototype InvokerPrototype;

    // NOT IMPLEMENTED
    OutportInvoker();
    OutportInvoker(const OutportInvoker&);
    ~OutportInvoker();
    OutportInvoker& operator=(const OutportInvoker&);
public:
    template <class Prototype2>
    static ReturnType singleInvoker(const InportBase *inport)
    {
        return (*static_cast<const Inport<Prototype2>*>(inport))();
    }
    template <class Prototype2>
    static ReturnType singleInvoker(const InportBase *inport, T1 a1)
    {
        return (*static_cast<const Inport<Prototype2>*>(inport))(a1);
    }
    template <class Prototype2>
    static ReturnType singleInvoker(const InportBase *inport, T1 a1, T2 a2)
    {
        return (*static_cast<const Inport<Prototype2>*>(inport))(a1, a2);
    }
    template <class Prototype2>
    static ReturnType singleInvoker(const InportBase *inport, T1 a1, T2 a2, T3 a3)
    {
        return (*static_cast<const Inport<Prototype2>*>(inport))(a1, a2, a3);
    }
    template <class Prototype2>
    static ReturnType singleInvoker(const InportBase *inport, T1 a1, T2 a2, T3 a3, T4 a4)
    {
        return (*static_cast<const Inport<Prototype2>*>(inport))(a1, a2, a3, a4);
    }
    template <class Prototype2>
    static ReturnType singleInvoker(const InportBase *inport, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5)
    {
        return (*static_cast<const Inport<Prototype2>*>(inport))(a1, a2, a3, a4, a5);
    }
    template <class Prototype2>
    static ReturnType singleInvoker(const InportBase *inport, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6)
    {
        return (*static_cast<const Inport<Prototype2>*>(inport))(a1, a2, a3, a4, a5, a6);
    }
    template <class Prototype2>
    static ReturnType singleInvoker(const InportBase *inport, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6, T7 a7)
    {
        return (*static_cast<const Inport<Prototype2>*>(inport))(a1, a2, a3, a4, a5, a6, a7);
    }
    template <class Prototype2>
    static ReturnType singleInvoker(const InportBase *inport, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6, T7 a7, T8 a8)
    {
        return (*static_cast<const Inport<Prototype2>*>(inport))(a1, a2, a3, a4, a5, a6, a7, a8);
    }

    static ReturnType defaultInvoker(const InportBase *)
    {
        throw outport_not_connected();
    }
    static ReturnType defaultInvoker(const InportBase *, T1)
    {
        throw outport_not_connected();
    }
    static ReturnType defaultInvoker(const InportBase *, T1, T2)
    {
        throw outport_not_connected();
    }
    static ReturnType defaultInvoker(const InportBase *, T1, T2, T3)
    {
        throw outport_not_connected();
    }
    static ReturnType defaultInvoker(const InportBase *, T1, T2, T3, T4)
    {
        throw outport_not_connected();
    }
    static ReturnType defaultInvoker(const InportBase *, T1, T2, T3, T4, T5)
    {
        throw outport_not_connected();
    }
    static ReturnType defaultInvoker(const InportBase *, T1, T2, T3, T4, T5, T6)
    {
        throw outport_not_connected();
    }
    static ReturnType defaultInvoker(const InportBase *, T1, T2, T3, T4, T5, T6, T7)
    {
        throw outport_not_connected();
    }
    static ReturnType defaultInvoker(const InportBase *, T1, T2, T3, T4, T5, T6, T7, T8)
    {
        throw outport_not_connected();
    }

    static ReturnType multiInvoker(const InportBase * inport)
    {
        const InportDelegate<Prototype> *inportDelegate =
            static_cast<const InportDelegate<Prototype>*>(inport);
        ReturnType rv;
        
        for (int i = 0; i < inportDelegate->size(); ++i) {
            InvokerPrototype *f = inportDelegate->getInvoker(i);
            rv = f(inportDelegate->getInport(i));
        }
        return rv;
    }
    static ReturnType multiInvoker(const InportBase * inport, T1 a1)
    {
        const InportDelegate<Prototype> *inportDelegate =
            static_cast<const InportDelegate<Prototype>*>(inport);
        ReturnType rv;
        
        for (int i = 0; i < inportDelegate->size(); ++i) {
            InvokerPrototype *f = inportDelegate->getInvoker(i);
            rv = f(inportDelegate->getInport(i), a1);
        }
        return rv;
    }
    static ReturnType multiInvoker(const InportBase * inport, T1 a1, T2 a2)
    {
        const InportDelegate<Prototype> *inportDelegate = 
            static_cast<const InportDelegate<Prototype>*>(inport);
        ReturnType rv;
        
        for (int i = 0; i < inportDelegate->size(); ++i) {
            InvokerPrototype *f = inportDelegate->getInvoker(i);
            rv = f(inportDelegate->getInport(i), a1, a2);
        }
        return rv;
    }
    static ReturnType multiInvoker(const InportBase * inport, T1 a1, T2 a2, T3 a3)
    {
        const InportDelegate<Prototype> *inportDelegate = 
            static_cast<const InportDelegate<Prototype>*>(inport);
        ReturnType rv;
        
        for (int i = 0; i < inportDelegate->size(); ++i) {
            InvokerPrototype *f = inportDelegate->getInvoker(i);
            rv = f(inportDelegate->getInport(i), a1, a2, a3);
        }
        return rv;
    }    
    static ReturnType multiInvoker(const InportBase * inport, T1 a1, T2 a2, T3 a3, T4 a4)
    {
        const InportDelegate<Prototype> *inportDelegate = 
            static_cast<const InportDelegate<Prototype>*>(inport);
        ReturnType rv;
        
        for (int i = 0; i < inportDelegate->size(); ++i) {
            InvokerPrototype *f = inportDelegate->getInvoker(i);
            rv = f(inportDelegate->getInport(i), a1, a2, a3, a4);
        }
        return rv;
    }
    static ReturnType multiInvoker(const InportBase * inport, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5)
    {
        const InportDelegate<Prototype> *inportDelegate = 
            static_cast<const InportDelegate<Prototype>*>(inport);
        ReturnType rv;
        
        for (int i = 0; i < inportDelegate->size(); ++i) {
            InvokerPrototype *f = inportDelegate->getInvoker(i);
            rv = f(inportDelegate->getInport(i), a1, a2, a3, a4, a5);
        }
        return rv;
    }
    static ReturnType multiInvoker(const InportBase * inport, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6)
    {
        const InportDelegate<Prototype> *inportDelegate = 
            static_cast<const InportDelegate<Prototype>*>(inport);
        ReturnType rv;
        
        for (int i = 0; i < inportDelegate->size(); ++i) {
            InvokerPrototype *f = inportDelegate->getInvoker(i);
            rv = f(inportDelegate->getInport(i), a1, a2, a3, a4, a5, a6);
        }
        return rv;
    }
    static ReturnType multiInvoker(const InportBase * inport, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6, T7 a7)
    {
        const InportDelegate<Prototype> *inportDelegate = 
            static_cast<const InportDelegate<Prototype>*>(inport);
        ReturnType rv;
        
        for (int i = 0; i < inportDelegate->size(); ++i) {
            InvokerPrototype *f = inportDelegate->getInvoker(i);
            rv = f(inportDelegate->getInport(i), a1, a2, a3, a4, a5, a6, a7);
        }
        return rv;
    }
    static ReturnType multiInvoker(const InportBase * inport, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6, T7 a7, T8 a8)
    {
        const InportDelegate<Prototype> *inportDelegate = 
            static_cast<const InportDelegate<Prototype>*>(inport);
        ReturnType rv;
        
        for (int i = 0; i < inportDelegate->size(); ++i) {
            InvokerPrototype *f = inportDelegate->getInvoker(i);
            rv = f(inportDelegate->getInport(i), a1, a2, a3, a4, a5, a6, a7, a8);
        }
        return rv;
    }
};

template <class Prototype>
class OutportInvoker<Prototype, void>
{
    typedef typename PrototypeTrait<Prototype>::T1               T1;
    typedef typename PrototypeTrait<Prototype>::T2               T2;
    typedef typename PrototypeTrait<Prototype>::T3               T3;
    typedef typename PrototypeTrait<Prototype>::T4               T4;
    typedef typename PrototypeTrait<Prototype>::T5               T5;
    typedef typename PrototypeTrait<Prototype>::T6               T6;
    typedef typename PrototypeTrait<Prototype>::T7               T7;
    typedef typename PrototypeTrait<Prototype>::T8               T8;
    typedef typename PrototypeTrait<Prototype>::InvokerPrototype InvokerPrototype;

public:
    template <class Prototype2>
    static void singleInvoker(const InportBase *inport)
    {
        (*static_cast<const Inport<Prototype2>*>(inport))();
    }
    template <class Prototype2>
    static void singleInvoker(const InportBase *inport, T1 a1)
    {
        (*static_cast<const Inport<Prototype2>*>(inport))(a1);
    }
    template <class Prototype2>
    static void singleInvoker(const InportBase *inport, T1 a1, T2 a2)
    {
        (*static_cast<const Inport<Prototype2>*>(inport))(a1, a2);
    }
    template <class Prototype2>
    static void singleInvoker(const InportBase *inport, T1 a1, T2 a2, T3 a3)
    {
        (*static_cast<const Inport<Prototype2>*>(inport))(a1, a2, a3);
    }
    template <class Prototype2>
    static void singleInvoker(const InportBase *inport, T1 a1, T2 a2, T3 a3, T4 a4)
    {
        (*static_cast<const Inport<Prototype2>*>(inport))(a1, a2, a3, a4);
    }
    template <class Prototype2>
    static void singleInvoker(const InportBase *inport, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5)
    {
        (*static_cast<const Inport<Prototype2>*>(inport))(a1, a2, a3, a4, a5);
    }
    template <class Prototype2>
    static void singleInvoker(const InportBase *inport, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6)
    {
        (*static_cast<const Inport<Prototype2>*>(inport))(a1, a2, a3, a4, a5, a6);
    }
    template <class Prototype2>
    static void singleInvoker(const InportBase *inport, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6, T7 a7)
    {
        (*static_cast<const Inport<Prototype2>*>(inport))(a1, a2, a3, a4, a5, a6, a7);
    }
    template <class Prototype2>
    static void singleInvoker(const InportBase *inport, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6, T7 a7, T8 a8)
    {
        (*static_cast<const Inport<Prototype2>*>(inport))(a1, a2, a3, a4, a5, a6, a7, a8);
    }

    static void defaultInvoker(const InportBase *)
    {
    }
    static void defaultInvoker(const InportBase *, T1)
    {
    }
    static void defaultInvoker(const InportBase *, T1, T2)
    {
    }
    static void defaultInvoker(const InportBase *, T1, T2, T3)
    {
    }
    static void defaultInvoker(const InportBase *, T1, T2, T3, T4)
    {
    }
    static void defaultInvoker(const InportBase *, T1, T2, T3, T4, T5)
    {
    }
    static void defaultInvoker(const InportBase *, T1, T2, T3, T4, T5, T6)
    {
    }
    static void defaultInvoker(const InportBase *, T1, T2, T3, T4, T5, T6, T7)
    {
    }
    static void defaultInvoker(const InportBase *, T1, T2, T3, T4, T5, T6, T7, T8)
    {
    }

    static void multiInvoker(const InportBase * inport)
    {
        const InportDelegate<Prototype> *inportDelegate = 
            static_cast<const InportDelegate<Prototype>*>(inport);
        
        for (int i = 0; i < inportDelegate->size(); ++i) {
            InvokerPrototype *f = inportDelegate->getInvoker(i);
            f(inportDelegate->getInport(i));
        }
    }
    static void multiInvoker(const InportBase * inport, T1 a1)
    {
        const InportDelegate<Prototype> *inportDelegate = 
            static_cast<const InportDelegate<Prototype>*>(inport);
        
        for (int i = 0; i < inportDelegate->size(); ++i) {
            InvokerPrototype *f = inportDelegate->getInvoker(i);
            f(inportDelegate->getInport(i), a1);
        }
    }
    static void multiInvoker(const InportBase * inport, T1 a1, T2 a2)
    {
        const InportDelegate<Prototype> *inportDelegate = 
            static_cast<const InportDelegate<Prototype>*>(inport);
        
        for (int i = 0; i < inportDelegate->size(); ++i) {
            InvokerPrototype *f = inportDelegate->getInvoker(i);
            f(inportDelegate->getInport(i), a1, a2);
        }
    }
    static void multiInvoker(const InportBase * inport, T1 a1, T2 a2, T3 a3)
    {
        const InportDelegate<Prototype> *inportDelegate = 
            static_cast<const InportDelegate<Prototype>*>(inport);
        
        for (int i = 0; i < inportDelegate->size(); ++i) {
            InvokerPrototype *f = inportDelegate->getInvoker(i);
            f(inportDelegate->getInport(i), a1, a2, a3);
        }
    }
    static void multiInvoker(const InportBase * inport, T1 a1, T2 a2, T3 a3, T4 a4)
    {
        const InportDelegate<Prototype> *inportDelegate = 
            static_cast<const InportDelegate<Prototype>*>(inport);
        
        for (int i = 0; i < inportDelegate->size(); ++i) {
            InvokerPrototype *f = inportDelegate->getInvoker(i);
            f(inportDelegate->getInport(i), a1, a2, a3, a4);
        }
    }
    static void multiInvoker(const InportBase * inport, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5)
    {
        const InportDelegate<Prototype> *inportDelegate = 
            static_cast<const InportDelegate<Prototype>*>(inport);
        
        for (int i = 0; i < inportDelegate->size(); ++i) {
            InvokerPrototype *f = inportDelegate->getInvoker(i);
            f(inportDelegate->getInport(i), a1, a2, a3, a4, a5);
        }
    }
    static void multiInvoker(const InportBase * inport, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6)
    {
        const InportDelegate<Prototype> *inportDelegate = 
            static_cast<const InportDelegate<Prototype>*>(inport);
        
        for (int i = 0; i < inportDelegate->size(); ++i) {
            InvokerPrototype *f = inportDelegate->getInvoker(i);
            f(inportDelegate->getInport(i), a1, a2, a3, a4, a5, a6);
        }
    }
    static void multiInvoker(const InportBase * inport, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6, T7 a7)
    {
        const InportDelegate<Prototype> *inportDelegate = 
            static_cast<const InportDelegate<Prototype>*>(inport);
        
        for (int i = 0; i < inportDelegate->size(); ++i) {
            InvokerPrototype *f = inportDelegate->getInvoker(i);
            f(inportDelegate->getInport(i), a1, a2, a3, a4, a5, a6, a7);
        }
    }
    static void multiInvoker(const InportBase * inport, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6, T7 a7, T8 a8)
    {
        const InportDelegate<Prototype> *inportDelegate = 
            static_cast<const InportDelegate<Prototype>*>(inport);
        
        for (int i = 0; i < inportDelegate->size(); ++i) {
            InvokerPrototype *f = inportDelegate->getInvoker(i);
            f(inportDelegate->getInport(i), a1, a2, a3, a4, a5, a6, a7, a8);
        }
    }
};

template <class Prototype>
class Outport
{
    typedef typename PrototypeTrait<Prototype>::ReturnType       ReturnType;
    typedef typename PrototypeTrait<Prototype>::T1               T1;
    typedef typename PrototypeTrait<Prototype>::T2               T2;
    typedef typename PrototypeTrait<Prototype>::T3               T3;
    typedef typename PrototypeTrait<Prototype>::T4               T4;
    typedef typename PrototypeTrait<Prototype>::T5               T5;
    typedef typename PrototypeTrait<Prototype>::T6               T6;
    typedef typename PrototypeTrait<Prototype>::T7               T7;
    typedef typename PrototypeTrait<Prototype>::T8               T8;
    typedef typename PrototypeTrait<Prototype>::InvokerPrototype InvokerPrototype;

    InportBase * d_inport;
    InvokerPrototype *d_invoker;

public:
    Outport();

    ~Outport();

	void disconnect()
	{
		delete d_inport;
		d_inport = 0;
	}

    bool isConnected() const
    {
        return d_inport != 0;
    }

    template <class ObjectType, class MemberFunction>
    void connect(ObjectType *object, MemberFunction memberFunction);

    ReturnType operator()() const
    {
        return (*d_invoker)(d_inport);
    }
    ReturnType operator()(T1 a1) const
    {
        return (*d_invoker)(d_inport, a1);
    }
    ReturnType operator()(T1 a1, T2 a2) const
    {
        return (*d_invoker)(d_inport, a1, a2);
    }
    ReturnType operator()(T1 a1, T2 a2, T3 a3) const
    {
        return (*d_invoker)(d_inport, a1, a2, a3);
    }
    ReturnType operator()(T1 a1, T2 a2, T3 a3, T4 a4) const
    {
        return (*d_invoker)(d_inport, a1, a2, a3, a4);
    }
    ReturnType operator()(T1 a1, T2 a2, T3 a3, T4 a4, T5 a5) const
    {
        return (*d_invoker)(d_inport, a1, a2, a3, a4, a5);
    }
    ReturnType operator()(T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6) const
    {
        return (*d_invoker)(d_inport, a1, a2, a3, a4, a5, a6);
    }
    ReturnType operator()(T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6, T7 a7) const
    {
        return (*d_invoker)(d_inport, a1, a2, a3, a4, a5, a6, a7);
    }
    ReturnType operator()(T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6, T7 a7, T8 a8) const
    {
        return (*d_invoker)(d_inport, a1, a2, a3, a4, a5, a6, a7, a8);
    }
};

template <class Prototype>
Outport<Prototype>::Outport()
    : d_invoker(&OutportInvoker<Prototype, ReturnType>::defaultInvoker)
{
    d_inport = 0;
}

template <class Prototype>
Outport<Prototype>::~Outport()
{
    delete d_inport;
}

template <class Prototype>
template <class ObjectType, class MemberFunction>
void Outport<Prototype>::connect(ObjectType *object, MemberFunction memberFunction)
{
    typedef typename MemberFunctionTrait<ObjectType, MemberFunction>::Prototype Prototype2;
	Inport<Prototype2> *inport = new Inport<Prototype2>(object, memberFunction);
    InvokerPrototype * multiInvoker = &OutportInvoker<Prototype, ReturnType>::multiInvoker;
    if (!d_inport) {
        // This outport hasn't been connected.
        d_inport = inport;
        d_invoker = &OutportInvoker<Prototype, ReturnType>::template singleInvoker<Prototype2>;
        return;
    } else if (d_invoker != multiInvoker) {
        // This outport has been connected only once.
        InportDelegate<Prototype> *inportDelegate = new InportDelegate<Prototype>;
        inportDelegate->push_back(d_inport, d_invoker);
        InvokerPrototype *f = &OutportInvoker<Prototype, ReturnType>::template singleInvoker<Prototype2>;
        inportDelegate->push_back(inport, f);
        
        d_inport = inportDelegate;
        d_invoker = &OutportInvoker<Prototype, ReturnType>::multiInvoker;
    } else {
        // This outport has been connected more than once.
        InvokerPrototype *f = &OutportInvoker<Prototype, ReturnType>::template singleInvoker<Prototype2>;
        InportDelegate<Prototype> *inportDelegate = dynamic_cast<InportDelegate<Prototype>*>(d_inport);
        inportDelegate->push_back(inport, f);
    }
}

} // close namespace impl

using impl::outport_not_connected;

template <class Prototype>
class out : public impl::Outport<Prototype>
{
public:
    out() {}
    ~out() {}
private:
    // NOT IMPLEMENTED
    out(const out&);
    out &operator=(const out&);
};

} // close namespace block

#endif // INCLUDED_BLOCK_OUT_H
