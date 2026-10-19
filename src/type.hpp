#ifndef TYPE_H
#define TYPE_H

class Type {
public:
    int width;
    Type(int w): width(w) {}
    virtual ~Type() {}
};

class IntType : public Type {
public:
    IntType(): Type(4) {}
};

class FloatType : public Type {
public:
    FloatType(): Type(8) {}
};

class ArrayType : public Type {
public:
    Type *elem;
    int size;
    ArrayType(int size, Type *elem): size(size), elem(elem), Type(size * elem->width) {} 
};

#endif // TYPE_H